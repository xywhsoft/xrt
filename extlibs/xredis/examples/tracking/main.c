#include "../../tests/support/client_peer.h"
#include <xredis/tracking.h>
static bool exampleRead(redispeer* P,xredisreply* R,ptr User)
{
    (void)R;(void)User;
    static const char Wire[]=">2\r\n+invalidate\r\n*1\r\n$3\r\nkey\r\n+value\r\n";
    redisPeerSend(P,Wire,sizeof(Wire)-1);return true;
}
int main(void)
{
    redispeer P;redisPeerStart(&P);redispeerhook Hook={exampleRead,NULL};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=NULL;redisPeerRequire(xredisClientConnect(P.Engine,&P.Address,0,NULL,&C) && xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"observed client");
    xbytesview Read=XRT_BYTES_LITERAL("READ");xfuture* F=NULL;redisPeerRequire(xredisClientTrackingCall(C,&Read,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"read call");
    const xredistrackingresult* R=xrtFutureValue(F);xredistrackingpush Push;
    redisPeerRequire(R && R->Request.Status==XREDIS_REQUEST_OK && xredisClientTrackingTake(C,&Push) && Push.Sequence==1 && R->Sequence==2,"reply collected before earlier invalidation");
    xredistrackingevent Event;redisPeerRequire(xredisTrackingEventRead(Push.Reply,&Event)==XREDIS_TRACKING_EVENT && Event.Keys==1,"invalidation view");
    xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"physical close");xredisClientRelease(C);xrtFutureDestroy(Closed);redisPeerStop(&P);
    redisPeerRequire(xredisTrackingEventRead(Push.Reply,&Event)==XREDIS_TRACKING_EVENT && R->Sequence==2,"retained outputs outlive all owners");
    printf("reply_sequence=%llu invalidation_sequence=%llu keys=%zu\n",(unsigned long long)R->Sequence,(unsigned long long)Push.Sequence,Event.Keys);
    xredisReplyRelease(Push.Reply);xrtFutureDestroy(F);return 0;
}
