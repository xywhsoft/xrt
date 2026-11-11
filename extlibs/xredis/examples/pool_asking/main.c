#include <xredis.h>
#include "../../tests/support/pool_asking_peer.h"
int main(void)
{
    redisaskingpeer* F=redisAskingPeerStart(AK_NORMAL,false,XREDIS_INIT_RESP3,NULL,NULL);xfuture* Acquired=redisAskingAcquire(F);
    unsigned char Payload[]={'a',0,'b'};xbytesview Query[]={XRT_BYTES_LITERAL("APPLY"),{Payload,sizeof(Payload)}};xfuture* Result=NULL;
    if(xredisLeaseAskingCall(F->Lease,Query,2,5000,&Result)!=XREDIS_ADMISSION_ACCEPTED)return 1;
    const xredisaskingresult* R=xrtFutureValue(Result);
    if(!R || R->Query.Status!=XREDIS_REQUEST_OK || !R->ControlReceived || R->Control.Status!=XREDIS_REQUEST_OK)return 2;
    xfuture* Closed=xredisLeaseClosed(F->Lease);if(!xredisLeaseReturn(F->Lease) || xrtFutureWaitFor(Closed,10000)!=XWAIT_OK)return 3;
    redisAskingPeerStop(F);xredisreplyvalue V;
    if(!xredisReplyGet(R->Query.Reply,0,&V) || V.Data.Size!=3 || memcmp(V.Data.Data,"a\0b",3))return 4;
    puts("ASKING+binary query: explicit control/query outcomes survive pool and engine shutdown");
    xrtFutureDestroy(Acquired);xrtFutureDestroy(Result);xrtFutureDestroy(Closed);return 0;
}
