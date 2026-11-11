#include <xredis/subscriptions.h>
#include "../../tests/support/client_peer.h"
static bool subscriptionsExamplePeer(redispeer* P,xredisreply* R,ptr Data)
{
    (void)Data;xredisreplyvalue Root,Word;xredisReplyGet(R,0,&Root);xredisReplyGet(R,Root.FirstChild,&Word);
    if(redisPeerName(Word,"SUBSCRIBE")){static const char Frames[]="*3\r\n+subscribe\r\n$5\r\nex\0ch\r\n:1\r\n*3\r\n+message\r\n$5\r\nex\0ch\r\n$3\r\nv\0x\r\n";redisPeerSend(P,Frames,sizeof(Frames)-1);return true;}
    if(redisPeerName(Word,"UNSUBSCRIBE")){static const char Ack[]="*3\r\n+unsubscribe\r\n$5\r\nex\0ch\r\n:0\r\n";redisPeerSend(P,Ack,sizeof(Ack)-1);return true;}
    return false;
}
int main(void)
{
    xredissubscriptionsconfig Options;xredissubscriptions* Store=NULL;
    redisPeerRequire(xredisSubscriptionsConfigInit(&Options) && xredisSubscriptionsCreate(&Options,&Store) && xredisSubscriptionsRetain(Store),"desired state ownership");xredisSubscriptionsRelease(Store);
    xbytesview Name=XRT_BYTES_LITERAL("ex\0ch");uint64 Revision=0;
    redisPeerRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"commit local intent");
    xredissubscriptionsinfo Info={0};Info.Size=sizeof(Info);redisPeerRequire(xredisSubscriptionsInfo(Store,&Info) && Info.Entries==1 && Info.Revision==Revision,"local revision");
    xredissubscriptionsnapshot* Snapshot=NULL;redisPeerRequire(xredisSubscriptionsSnapshot(Store,&Snapshot) && xredisSubscriptionSnapshotRetain(Snapshot),"snapshot ownership");xredisSubscriptionSnapshotRelease(Snapshot);
    redisPeerRequire(xredisSubscriptionSnapshotInfo(Snapshot,&Info),"immutable snapshot info");xredissubscriptionview View;redisPeerRequire(xredisSubscriptionSnapshotAt(Snapshot,0,&View) && View.Name.Size==5,"borrowed snapshot view");
    redispeer P;redisPeerStart(&P);redispeerhook Hook={subscriptionsExamplePeer,NULL};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=NULL;
    redisPeerRequire(xredisClientConnect(P.Engine,&P.Address,0,NULL,&C),"replacement transport");xfuture* Ready=xredisClientReady(C);redisPeerRequire(xrtFutureWaitFor(Ready,5000)==XWAIT_OK,"replacement ready");xrtFutureDestroy(Ready);
    xredissubscriptionreplayconfig Config;redisPeerRequire(xredisSubscriptionReplayConfigInit(&Config),"replay work bounds");xredissubscriptionreplay* Replay=NULL;
    redisPeerRequire(xredisSubscriptionReplayCreate(Store,C,&Config,&Replay)==XREDIS_ADMISSION_ACCEPTED,"capture and claim replacement");
    for(;;){xredissubscriptionstep Step=xredisSubscriptionReplayStep(Replay);redisPeerRequire(Step!=XREDIS_SUBSCRIPTIONS_STEP_ERROR,"replay step");if(Step==XREDIS_SUBSCRIPTIONS_DONE)break;
        xfuture* Pending=xredisSubscriptionReplayPending(Replay);if(Pending){redisPeerRequire(xrtFutureWaitFor(Pending,5000)==XWAIT_OK,"native control observer");xrtFutureDestroy(Pending);}else xrtThreadYield();}
    xredissubscriptionreplayresult Result={0};Result.Size=sizeof(Result);redisPeerRequire(xredisSubscriptionReplayResult(Replay,&Result) && Result.Status==XREDIS_SUBSCRIPTIONS_OK && Result.Revision==Revision && Result.Acknowledged==1,"restored captured revision");
    redisPeerRequire(!xredisSubscriptionReplayCancel(Replay),"terminal result stays terminal");xredisSubscriptionReplayDestroy(Replay);xredisSubscriptionsRelease(Store);
    xredisreply* Message=NULL;double End=xrtTimer()+5;while(!xredisClientTakePush(C,&Message)){redisPeerRequire(xrtTimer()<End,"startup message");xrtThreadYield();}
    xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,5000)==XWAIT_OK,"physical close");xrtFutureDestroy(Closed);xredisClientRelease(C);redisPeerStop(&P);
    redisPeerRequire(xredisSubscriptionSnapshotAt(Snapshot,0,&View) && View.Name.Size==5 && !memcmp(View.Name.Data,"ex\0ch",5),"snapshot outlives all physical owners");
    xredispubsubevent E;redisPeerRequire(xredisPubSubEventRead(Message,&E)==XREDIS_PUBSUB_EVENT && E.Payload.Size==3,"owned startup message");printf("restored_revision=%llu entries=%zu payload_bytes=%zu\n",(unsigned long long)Revision,Info.Entries,E.Payload.Size);
    xredisReplyRelease(Message);xredisSubscriptionSnapshotRelease(Snapshot);return 0;
}
