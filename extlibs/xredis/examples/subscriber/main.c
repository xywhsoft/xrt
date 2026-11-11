#include <xredis/subscriber.h>
#include "../../tests/support/client_peer.h"
static bool subscriberExamplePeer(redispeer* P,xredisreply* R,ptr Data)
{
    (void)Data;xredisreplyvalue Root,Word;xredisReplyGet(R,0,&Root);xredisReplyGet(R,Root.FirstChild,&Word);
    if(redisPeerName(Word,"SUBSCRIBE")){static const char Frames[]="*3\r\n+subscribe\r\n$5\r\nex\0ch\r\n:1\r\n*3\r\n+message\r\n$5\r\nex\0ch\r\n$3\r\nv\0x\r\n";redisPeerSend(P,Frames,sizeof(Frames)-1);return true;}
    if(redisPeerName(Word,"UNSUBSCRIBE")){static const char Ack[]="*3\r\n+unsubscribe\r\n$5\r\nex\0ch\r\n:0\r\n";redisPeerSend(P,Ack,sizeof(Ack)-1);return true;}
    return false;
}
int main(void)
{
    redispeer P;redisPeerStart(&P);redispeerhook Hook={subscriberExamplePeer,NULL};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=NULL;redisPeerRequire(xredisClientConnect(P.Engine,&P.Address,0,NULL,&C),"subscriber example connect");xfuture* Ready=xredisClientReady(C);redisPeerRequire(xrtFutureWaitFor(Ready,5000)==XWAIT_OK,"ready");xrtFutureDestroy(Ready);
    xredissubscriberconfig Options;redisPeerRequire(xredisSubscriberConfigInit(&Options) && xredisSubscriberStart(C,&Options)==XREDIS_ADMISSION_ACCEPTED,"dedicate idle connection");xbytesview Name=XRT_BYTES_LITERAL("ex\0ch");xfuture* F=NULL;
    redisPeerRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED && xrtFutureWaitFor(F,5000)==XWAIT_OK,"subscribe");xfutureresult V;redisPeerRequire(xrtFutureResult(F,&V) && ((xredissubscriberresult*)V.Value)->Request.Status==XREDIS_REQUEST_OK,"subscription ACK");xrtFutureDestroy(F);
    xredissubscriberinfo Info={0};Info.Size=sizeof(Info);redisPeerRequire(xredisSubscriberInfo(C,&Info) && Info.Subscriptions==1,"confirmed registry snapshot");
    xredisreply* Message=NULL;double End=xrtTimer()+5;while(!xredisClientTakePush(C,&Message)){redisPeerRequire(xrtTimer()<End,"message deadline");xrtThreadYield();}
    F=NULL;redisPeerRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,NULL,0,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"unsubscribe all");redisPeerRequire(xrtFutureResult(F,&V) && ((xredissubscriberresult*)V.Value)->Request.Status==XREDIS_REQUEST_OK,"unsubscribe ACK");xrtFutureDestroy(F);
    xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,5000)==XWAIT_OK,"physical close");xrtFutureDestroy(Closed);xredisClientRelease(C);redisPeerStop(&P);
    xredispubsubevent E;redisPeerRequire(xredisPubSubEventRead(Message,&E)==XREDIS_PUBSUB_EVENT && E.Channel.Size==5 && E.Payload.Size==3,"message outlives connection and Engine");printf("channel_bytes=%zu payload_bytes=%zu (owned after close)\n",E.Channel.Size,E.Payload.Size);xredisReplyRelease(Message);return 0;
}
