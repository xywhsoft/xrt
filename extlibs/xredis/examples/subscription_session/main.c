#include <xredis/subscription_session.h>
#include "../../tests/support/subscription_session_peer.h"
static xredissubscriptionsessioncurrent sessionExampleWait(xredissubscriptionsession* S,uint64 Revision)
{
    double End=xrtTimer()+10;for(;;){xredissubscriptionsessioncurrent C;xredisadmission A=xredisSubscriptionSessionCurrent(S,&C);
        if(A==XREDIS_ADMISSION_ACCEPTED){if(C.Revision>=Revision)return C;xredisClientRelease(C.Client);}else redisPeerRequire(A==XREDIS_ADMISSION_OVERLOADED,"pending current");
        redisPeerRequire(xrtTimer()<End,"example online deadline");xrtThreadYield();}
}
int main(void)
{
    sessionpeer Peer;sessionPeerStart(&Peer);sessionfactory Factory;xredissubscriptionsessionconfig Config;redisPeerRequire(xredisSubscriptionSessionConfigInit(&Config),"session defaults");sessionFactoryConfig(&Factory,&Peer,&Config);Factory.FailFirst=1;
    xredissubscriptions* Store=NULL;redisPeerRequire(xredisSubscriptionsCreate(NULL,&Store),"desired store");xbytesview Name=XRT_BYTES_LITERAL("stable");uint64 Revision=0;redisPeerRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"initial intent");
    xredissubscriptionsession* Session=NULL;redisPeerRequire(xredisSubscriptionSessionCreate(Peer.Engine,0,Store,&Config,&Session)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriptionSessionRetain(Session),"autonomous session ownership");xredisSubscriptionSessionRelease(Session);
    xredissubscriptionsessioncurrent First=sessionExampleWait(Session,Revision);uint64 Generation=First.Generation;xredisClientRelease(First.Client);
    Name=(xbytesview)XRT_BYTES_LITERAL("next\0name");redisPeerRequire(xredisSubscriptionSessionApply(Session,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"schedule live intent");xredissubscriptionsessioncurrent Updated=sessionExampleWait(Session,Revision);redisPeerRequire(Updated.Generation==Generation,"unchanged transport generation");xredisClientRelease(Updated.Client);
    xredissubscriptionsessioninfo Info={0};Info.Size=sizeof(Info);redisPeerRequire(xredisSubscriptionSessionInfo(Session,&Info),"state observation");xfuture* Closed=xredisSubscriptionSessionClosed(Session);redisPeerRequire(xredisSubscriptionSessionClose(Session) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK && xrtFutureValue(Closed),"physical close and context drop");
    xredisSubscriptionsRelease(Store);sessionPeerStop(&Peer);redisPeerRequire(xredisSubscriptionSessionInfo(Session,&Info) && Info.State==XREDIS_SESSION_CLOSED,"metadata after Engine");printf("attempts=%llu generations=%llu applied_revision=%llu\n",(unsigned long long)Info.Attempts,(unsigned long long)Info.Generations,(unsigned long long)Info.AppliedRevision);xredisSubscriptionSessionRelease(Session);xrtFutureDestroy(Closed);return 0;
}
