#include <xredis.h>
#include "../../tests/support/pool_asking_peer.h"
int main(void)
{
    redisaskingpeer* P=redisSharedAskingPeerStart(AK_NORMAL,false,XREDIS_INIT_RESP3,NULL,NULL);
    xbytesview A[]={XRT_BYTES_LITERAL("APPLY"),XRT_BYTES_LITERAL("a\0x")};xfuture* F=NULL;
    if(xredisSharedPoolAskingCall(P->Pool,A,2,5000,&F)!=XREDIS_ADMISSION_ACCEPTED)return 1;
    const xredisaskingresult* R=xrtFutureValue(F);
    if(!R || !R->ControlReceived || R->Control.Status!=XREDIS_REQUEST_OK || R->Query.Status!=XREDIS_REQUEST_OK)return 2;
    redisAskingPeerStop(P);xredisreplyvalue V;
    if(!xredisReplyGet(R->Query.Reply,0,&V) || V.Data.Size!=3 || memcmp(V.Data.Data,"a\0x",3))return 3;
    xrtFutureDestroy(F);puts("One native shared ASKING/query Future; separate immutable outcomes survive engine shutdown");return 0;
}
