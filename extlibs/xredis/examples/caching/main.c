#include "../../tests/support/client_peer.h"
#include <xredis/tracking.h>
static bool cachingExamplePeer(redispeer* P,xredisreply* R,ptr User)
{
    (void)User;xredisreplyvalue Root,Name;xredisReplyGet(R,0,&Root);xredisReplyGet(R,Root.FirstChild,&Name);
    if(redisPeerName(Name,"HELLO")){
        static const char Hello[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P,Hello,sizeof(Hello)-1);
    }else if(redisPeerName(Name,"CLIENT"))redisPeerSend(P,"+OK\r\n",5);
    else redisPeerSend(P,"$5\r\nvalue\r\n",11);
    return true;
}
int main(void)
{
    redispeer P;redisPeerStart(&P);redispeerhook Hook={cachingExamplePeer,NULL};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=NULL;redisPeerRequire(xredisClientConnectInit(P.Engine,&P.Address,0,NULL,NULL,&C),"managed Client");
    xfuture* Ready=xredisClientReady(C);redisPeerRequire(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"setup");xrtFutureDestroy(Ready);
    redisPeerRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"receive order");
    xredistrackingconfig K;xredisTrackingConfigInit(&K);K.Flags=XREDIS_TRACKING_OPTIN;xfuture* Setup=NULL;
    redisPeerRequire(xredisClientTrackingConfigureAsync(C,&K,5000,&Setup)==XREDIS_ADMISSION_ACCEPTED && xrtFutureWaitFor(Setup,5000)==XWAIT_OK && ((const xredistrackingresult*)xrtFutureValue(Setup))->Request.Status==XREDIS_REQUEST_OK,"tracking OPTIN");xrtFutureDestroy(Setup);
    xbytesview Query[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("binary\0key")};xfuture* Future=NULL;
    redisPeerRequire(xredisClientCachingCall(C,true,Query,2,5000,&Future)==XREDIS_ADMISSION_ACCEPTED,"adjacent CACHING/query group");
    const xrediscachingresult* R=xrtFutureValue(Future);
    redisPeerRequire(R && R->ControlReceived && R->Control.Request.Status==XREDIS_REQUEST_OK && R->Query.Request.Status==XREDIS_REQUEST_OK,"both independent outcomes");
    xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"physical close");xredisClientRelease(C);xrtFutureDestroy(Closed);redisPeerStop(&P);
    xredisreplyvalue V;redisPeerRequire(xredisReplyGet(R->Query.Request.Reply,0,&V) && V.Data.Size==5 && !memcmp(V.Data.Data,"value",5),"both results outlive physical owners");
    printf("control_sequence=%llu query_sequence=%llu value=%.*s\n",(unsigned long long)R->Control.Sequence,(unsigned long long)R->Query.Sequence,(int)V.Data.Size,V.Data.Data);
    xrtFutureDestroy(Future);return 0;
}
