#include "../../tests/support/sentinel_events_peer.h"
int main(void)
{
    sefixture F;seStart(&F,false,false,false);
    xredispool* P=seCreate(&F,1);
    xfuture* First=spAcquire(P,5000);spPing(spLease(First));
    seListen(P,50);
    double End=xrtTimer()+5;
    while(seInfo(P).Session.State!=XREDIS_SESSION_ONLINE){
        redisPeerRequire(xrtTimer()<End,"listener setup");xrtSleep(1);
    }
    xrtAtomic32Store(&F.Pool.Current,1,XMEMORY_RELEASE);
    xfuture* Waiting=spAcquire(P,5000);
    seSend(&F,0,"cache 127.0.0.1 1234 127.0.0.1 5678",1);
    spPing(spLease(Waiting));
    redisPeerRequire(spInfo(P).Generation==2 && spInfo(P).Port==F.Pool.B.Base.Address.Port,"verified online switch");
    seStop(&F,P);
    xrtFutureDestroy(First);xrtFutureDestroy(Waiting);
    puts("Sentinel events trigger verified pool refresh");
    return 0;
}
