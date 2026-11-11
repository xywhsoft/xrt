#include "../../tests/support/role_client_peer.h"
#include <stdio.h>
int main(void)
{
    const redisinitpeerstep Steps[]={{{XRT_BYTES_LITERAL("HELLO"),XRT_BYTES_LITERAL("3")},2,XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY),false,false},{{XRT_BYTES_LITERAL("ROLE")},1,XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),false,false}};
    roleclientpeer P;roleClientPeerStart(&P,false,false,Steps,2);xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Flags=XREDIS_INIT_REQUIRE_MASTER;xredisclient* C=NULL;
    if(!roleClientPeerConnect(&P,0,NULL,&Init,&C)){roleClientPeerStop(&P);return 1;}
    xfuture* Ready=xredisClientReady(C);xfuture* Ping=roleClientPing(C,5000);const xredisreadyresult* R=roleClientWait(Ready);bool OK=R->Status==XREDIS_READY_OK && R->Step==XREDIS_INIT_DONE && R->Hello!=NULL;const xredisrequestresult* B=roleClientWait(Ping);OK=OK && B->Status==XREDIS_REQUEST_OK && B->Delivery==XREDIS_REPLIED;xrtFutureDestroy(Ping);roleClientClose(&P,C);R=roleClientWait(Ready);OK=OK && R->Status==XREDIS_READY_OK;xrtFutureDestroy(Ready);if(!OK)return 1;
    puts("Role-verified readiness completed before queued PING; retained result survives close");return 0;
}
