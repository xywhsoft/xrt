#include "../../tests/support/sentinel_pool_peer.h"
static void spExample(bool Replicas)
{
    spfixture F;spStart(&F,0,false,false);F.Replicas=Replicas;xredissentinelpoolconfig K;spConfig(&F,&K);xredispool* P=spCreate(&F,1,NULL,&K);
    xfuture* First=spAcquire(P,5000);spPing(spLease(First));xrtAtomic32Store(&F.Current,1,XMEMORY_RELEASE);spReturn(spLease(First),true);
    xfuture* Next=spAcquire(P,5000);spPing(spLease(Next));redisPeerRequire(spInfo(P).Generation==2,"automatic generation replacement");spReturn(spLease(Next),false);
    spClose(&F,P);spStop(&F);xredisPoolRelease(P);xrtFutureDestroy(First);xrtFutureDestroy(Next);printf("Native pool rediscovered and replaced its %s generation\n",Replicas?"replica":"master");
}
int main(void)
{spExample(false);spExample(true);return 0;}
