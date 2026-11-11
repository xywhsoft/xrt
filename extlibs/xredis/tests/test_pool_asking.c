#include "test.h"
#include <xredis/pool_asking.h>
#include <xrt/memory.h>
#include "support/pool_asking_peer.h"
static const xredisaskingresult* akWait(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureValue(F),"ASKING typed Future");return xrtFutureValue(F);}
static xfuture* akSubmit(redisaskingpeer* P,int64 Timeout)
{unsigned char Bytes[]={'a',0,0xff};xbytesview A[]={XRT_BYTES_LITERAL("APPLY"),{Bytes,sizeof(Bytes)}};xfuture* F=NULL;testRequire(xredisLeaseAskingCallAsync(P->Lease,A,2,Timeout,&F)==XREDIS_ADMISSION_ACCEPTED,"ASKING admission");memset(Bytes,0xcc,sizeof(Bytes));return F;}
static void akReply(const xredisaskingresult* R)
{xredisreplyvalue V;testRequire(R->Query.Status==XREDIS_REQUEST_OK && R->Query.Delivery==XREDIS_REPLIED && R->ControlReceived && R->Control.Status==XREDIS_REQUEST_OK && R->Control.Delivery==XREDIS_REPLIED,"separate complete pair outcomes");testRequire(xredisReplyGet(R->Query.Reply,0,&V) && V.Data.Size==3 && !memcmp(V.Data.Data,"a\0\xff",3),"owned binary query");xredisreplyinfo I={0};I.Size=sizeof(I);testRequire(xredisReplyInfo(R->Control.Reply,&I) && xredisReplyGet(R->Control.Reply,I.Root,&V) && V.Type==XREDIS_REPLY_SIMPLE_STRING && V.Data.Size==2 && !memcmp(V.Data.Data,"OK",2),"owned attributed OK control");}
static xfuture* akReturn(redisaskingpeer* F,bool Reusable)
{xfuture* Closed=xredisLeaseClosed(F->Lease);testRequire(Closed && xredisLeaseReturn(F->Lease) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Closed))->Reusable==Reusable,"ASKING physical return/reuse boundary");return Closed;}
static void akBasic(bool Secure,xredisinitmode Mode)
{
    redisaskingpeer* P=redisAskingPeerStart(AK_NORMAL,Secure,Mode,NULL,NULL);xfuture* A=redisAskingAcquire(P);xfuture* F=akSubmit(P,5000);const xredisaskingresult* R=akWait(F);akReply(R);xredisreplyinfo RI={0};RI.Size=sizeof(RI);xredisreplyvalue Control;testRequire(xredisReplyInfo(R->Control.Reply,&RI) && xredisReplyGet(R->Control.Reply,RI.Root,&Control) && (Control.Attributes!=XREDIS_REPLY_NONE)==(Mode!=XREDIS_INIT_RESP2),"protocol-specific attributed control preserves root");
    xfuture* Closed=akReturn(P,true);xfuture* Again=redisAskingAcquire(P);xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* Ordinary=NULL;
    testRequire(xredisLeaseCall(P->Lease,&Ping,1,5000,&Ordinary)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(Ordinary))->Status==XREDIS_REQUEST_OK,"next lease has no leaked ASKING flag");xfuture* Last=akReturn(P,true);
    redisAskingPeerStop(P);akReply(R);xrtFutureDestroy(F);xrtFutureDestroy(A);xrtFutureDestroy(Again);xrtFutureDestroy(Closed);xrtFutureDestroy(Last);xrtFutureDestroy(Ordinary);
}
static void akErrors(bool Secure)
{
    const unsigned Modes[]={AK_CONTROL_ERROR,AK_QUERY_ERROR,AK_BAD_CONTROL,AK_BAD_WIRE,AK_DROP_PREFIX,AK_DROP_QUERY};
    for(size_t K=0;K<sizeof(Modes)/sizeof(*Modes);++K){unsigned Mode=Modes[K];redisaskingpeer* P=redisAskingPeerStart(Mode,Secure,XREDIS_INIT_RESP3,NULL,NULL);xfuture* A=redisAskingAcquire(P),*F=akSubmit(P,5000);const xredisaskingresult* R=akWait(F);
        if(Mode==AK_CONTROL_ERROR)testRequire(R->ControlReceived && R->Control.Status==XREDIS_REQUEST_SERVER_ERROR && R->Query.Status==XREDIS_REQUEST_OK && R->Query.Delivery==XREDIS_REPLIED,"control rejection cannot hide executed business query");
        else if(Mode==AK_QUERY_ERROR)testRequire(R->ControlReceived && R->Control.Status==XREDIS_REQUEST_OK && R->Query.Status==XREDIS_REQUEST_SERVER_ERROR && R->Query.Reply,"query redirection remains explicit");
        else if(Mode==AK_BAD_CONTROL)testRequire(R->ControlReceived && R->Control.Status==XREDIS_REQUEST_PROTOCOL && R->Query.Status==XREDIS_REQUEST_PROTOCOL && R->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED,"bad control closes uncertain group");
        else testRequire(R->Query.Status==(Mode==AK_BAD_WIRE?XREDIS_REQUEST_PROTOCOL:XREDIS_REQUEST_DISCONNECTED) && R->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED,"wire/disconnect uncertainty cannot authorize replay");
        xfuture* Closed=akReturn(P,Mode==AK_CONTROL_ERROR || Mode==AK_QUERY_ERROR);redisAskingPeerStop(P);testRequire(R->Query.Status!=XREDIS_REQUEST_OK || R->Query.Reply,"retained error result after engine destruction");xrtFutureDestroy(F);xrtFutureDestroy(A);xrtFutureDestroy(Closed);
    }
}
static void akPushBarrier(redisaskingpeer* P)
{double End=xrtTimer()+5;xredisreply* Push=NULL;while(!xredisClientTakePush(P->Client,&Push)){testRequire(xrtTimer()<End,"prefix reply receive barrier");xrtThreadYield();}xredisReplyRelease(Push);}
static void akStops(bool Secure)
{
    for(unsigned Known=0;Known<2;++Known)for(unsigned Cancel=0;Cancel<2;++Cancel){
        redisaskingpeer* P=redisAskingPeerStart(Known?AK_HOLD_QUERY:AK_HOLD_CONTROL,Secure,XREDIS_INIT_RESP3,NULL,NULL);xfuture* A=redisAskingAcquire(P),*F=akSubmit(P,Cancel?5000:300);
        redisPeerWait(&P->Queries,1);if(Known)akPushBarrier(P);if(Cancel)testRequire(xrtFutureCancel(F),"cancel sent ASKING group");const xredisaskingresult* R=akWait(F);xredisaskingresult Saved=*R;
        testRequire(R->Query.Status==(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT) && R->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED && R->ControlReceived==(Known!=0),"immutable prefix knowledge and conservative query delivery");
        xfuture* Returned=xredisLeaseClosed(P->Lease);testRequire(xredisLeaseReturn(P->Lease),"seal cancelled lease");testRequire(xrtFutureState(Returned)==XFUTURE_PENDING,"logical cancellation cannot release a physical pair");
        xfuture* Pending=NULL;testRequire(xredisPoolAcquireAsync(P->Pool,5000,&Pending)==XREDIS_ADMISSION_ACCEPTED && xrtFutureState(Pending)==XFUTURE_PENDING,"next lease queues behind pair drain");
        redisPeerReleaseBlocked(P->Peer);testRequire(xrtFutureWaitFor(Returned,10000)==XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable,"complete pair drain permits reuse");
        testRequire(xrtFutureWaitFor(Pending,10000)==XWAIT_OK && ((const xredisleaseresult*)xrtFutureValue(Pending))->Lease,"queued grant after physical drain");P->Lease=((const xredisleaseresult*)xrtFutureValue(Pending))->Lease;
        xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* PingF=NULL;testRequire(xredisLeaseCall(P->Lease,&Ping,1,5000,&PingF)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(PingF))->Status==XREDIS_REQUEST_OK,"next generation is synchronized");
        testRequire(!memcmp(&Saved,R,sizeof(Saved)),"late reply drain cannot mutate retained outcome");xfuture* Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Returned);xrtFutureDestroy(Pending);xrtFutureDestroy(PingF);xrtFutureDestroy(Last);
    }
}
static void akUnsent(void)
{
    xredisclientconfig C;xredisClientConfigInit(&C);C.MaxInFlight=1;redisaskingpeer* P=redisAskingPeerStart(AK_NORMAL,false,XREDIS_INIT_RESP3,&C,NULL);xfuture* A=redisAskingAcquire(P);xbytesview Hold=XRT_BYTES_LITERAL("HOLD");xfuture* Before=NULL;
    testRequire(xredisLeaseCallAsync(P->Lease,&Hold,1,5000,&Before)==XREDIS_ADMISSION_ACCEPTED,"preceding physical blocker");redisPeerWait(&P->Ordinary,1);
    for(unsigned Cancel=0;Cancel<2;++Cancel){xfuture* F=akSubmit(P,Cancel?5000:0);if(Cancel)testRequire(xrtFutureCancel(F),"cancel pending ASKING");const xredisaskingresult* R=akWait(F);testRequire(R->Query.Delivery==XREDIS_NOT_SENT && !R->ControlReceived && R->Query.Status==(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT),"unsent group has no one-shot state");xrtFutureDestroy(F);}
    testRequire(!xrtAtomic32Load(&P->Prefixes,XMEMORY_ACQUIRE),"no ASKING was sent while blocked");redisPeerReleaseBlocked(P->Peer);testRequire(((const xredisrequestresult*)akWait(Before))->Status==XREDIS_REQUEST_OK,"ordinary blocker finishes");xfuture* Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(Before);xrtFutureDestroy(Last);
}
static void akDrainExpiry(bool Secure)
{
    xredisclientconfig C;xredisClientConfigInit(&C);C.DrainTimeout=30;redisaskingpeer* P=redisAskingPeerStart(AK_HOLD_CONTROL,Secure,XREDIS_INIT_RESP3,&C,NULL);xfuture* A=redisAskingAcquire(P),*F=akSubmit(P,5000);redisPeerWait(&P->Queries,1);testRequire(xrtFutureCancel(F),"cancel before forced drain expiry");testRequire(akWait(F)->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED,"uncertain stopped pair");xfuture* Closed=akReturn(P,false);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Closed);
}
typedef struct akproducer {redisaskingpeer* P;} akproducer;
static int32 akProduce(ptr Data)
{redisaskingpeer* P=((akproducer*)Data)->P;for(unsigned K=0;K<24;++K){xfuture* F=akSubmit(P,5000);akReply(akWait(F));xrtFutureDestroy(F);xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* O=NULL;testRequire(xredisLeaseCall(P->Lease,&Ping,1,5000,&O)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(O))->Status==XREDIS_REQUEST_OK,"parallel ordinary request");xrtFutureDestroy(O);}return 0;}
static void akConcurrent(bool Secure)
{redisaskingpeer* P=redisAskingPeerStart(AK_NORMAL,Secure,XREDIS_INIT_RESP3,NULL,NULL);xfuture* A=redisAskingAcquire(P);akproducer Work={P};xthread* Threads[4];for(unsigned K=0;K<4;++K){Threads[K]=xrtThreadCreate(akProduce,&Work,0);testRequire(Threads[K]!=NULL,"ASKING producer");}for(unsigned K=0;K<4;++K){testRequire(xrtThreadWaitFor(Threads[K],10000)==XWAIT_OK && xrtThreadExitCode(Threads[K])==0,"producer join");xrtThreadDestroy(Threads[K]);}testRequire(xrtAtomic32Load(&P->Prefixes,XMEMORY_ACQUIRE)==96 && xrtAtomic32Load(&P->Queries,XMEMORY_ACQUIRE)==96,"all concurrent groups remain adjacent");xfuture* Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(Last);}
static void akReentry(void)
{redisaskingpeer* P=redisAskingPeerStart(AK_SYNC,false,XREDIS_INIT_RESP3,NULL,NULL);xfuture* A=redisAskingAcquire(P),*F=akSubmit(P,5000);akReply(akWait(F));redisPeerWait(&P->SyncChecked,1);testRequire(akWait(P->Reentry)->Query.Status==XREDIS_REQUEST_OK,"async reentry completes after original group");xfuture* Nested=P->Reentry;xfuture* Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(Nested);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Last);}
static void akLimitsOom(void)
{
    redisaskingpeer* P=redisAskingPeerStart(AK_NORMAL,false,XREDIS_INIT_RESP3,NULL,NULL);xfuture* A=redisAskingAcquire(P);xbytesview Args[]={XRT_BYTES_LITERAL("APPLY"),XRT_BYTES_LITERAL("x")};const cstr Bad[]={"ASKING","AUTH","HELLO","SELECT","QUIT","CLIENT","MULTI","WATCH","READONLY","READWRITE","RESET","SUBSCRIBE"};
    for(size_t K=0;K<sizeof(Bad)/sizeof(*Bad);++K){xbytesview V={(cbytes)Bad[K],strlen(Bad[K])};xfuture* Rejected=(void*)(uintptr_t)1;testRequire(xredisLeaseAskingCallAsync(P->Lease,&V,1,5000,&Rejected)==XREDIS_ADMISSION_ERROR && Rejected==(void*)(uintptr_t)1,"query mode controls rejected before prefix");xrtClearError();}
    unsigned Success=0,Failure=0;
    for(size_t Limit=1;Limit<=4096;Limit+=17){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"constructor OOM budget");xfuture* F=(void*)(uintptr_t)1;xredisadmission Status=xredisLeaseAskingCallAsync(P->Lease,Args,2,5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"leave constructor budget");xrtClearError();if(Status==XREDIS_ADMISSION_ACCEPTED){testRequire(akWait(F)->Query.Status==XREDIS_REQUEST_OK,"success after OOM sweep");xrtFutureDestroy(F);++Success;}else{testRequire(Status==XREDIS_ADMISSION_ERROR && F==(void*)(uintptr_t)1,"OOM failure is atomic");++Failure;}
        xallocationbudgetinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;do{testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<End,"budget owner physical retirement");if(I.LiveCount)xrtThreadYield();}while(I.LiveCount);testRequire(!I.LiveBytes,"OOM rollback returns all owned bytes");xrtAllocationBudgetFree(B);}
    testRequire(Success && Failure,"actual successful and failing allocation prefixes");xfuture* Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(Last);
    xredisclientconfig C;xredisClientConfigInit(&C);C.MaxCommandBytes=32;P=redisAskingPeerStart(AK_NORMAL,false,XREDIS_INIT_RESP3,&C,NULL);A=redisAskingAcquire(P);xfuture* F=(void*)(uintptr_t)1;testRequire(xredisLeaseAskingCallAsync(P->Lease,Args,2,5000,&F)==XREDIS_ADMISSION_OVERLOADED && F==(void*)(uintptr_t)1,"prefix plus state included in native wire cap");Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(Last);
}
static void akQuotaBoundaryAndLateProtocol(void)
{
    xbytesview Args[]={XRT_BYTES_LITERAL("APPLY"),XRT_BYTES_LITERAL("x")};size_t Wire=0;testRequire(xredisCommandSize(Args,2,&Wire),"measure query wire");Wire+=16;
    xredispoolconfig K;xredisPoolConfigInit(&K);K.MaxRequests=1;redisaskingpeer* P=redisAskingPeerStart(AK_HOLD_CONTROL,false,XREDIS_INIT_RESP3,NULL,&K);xfuture* A=redisAskingAcquire(P),*F=NULL;
    testRequire(xredisLeaseAskingCallAsync(P->Lease,Args,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"one request quota admits a whole two-reply group");redisPeerWait(&P->Queries,1);xredisclientinfo CI={0};CI.Size=sizeof(CI);xredispoolinfo PI={0};PI.Size=sizeof(PI);testRequire(xredisClientInfo(P->Client,&CI) && xredisPoolInfo(P->Pool,&PI) && CI.Requests==1 && PI.Requests==1 && CI.CommandBytes>Wire && PI.CommandBytes==CI.CommandBytes,"both quota layers include exact aligned pair state");size_t Charged=CI.CommandBytes;xfuture* Rejected=(void*)(uintptr_t)1;
    testRequire(xredisLeaseAskingCallAsync(P->Lease,Args,2,5000,&Rejected)==XREDIS_ADMISSION_OVERLOADED && Rejected==(void*)(uintptr_t)1,"one request quota has no hidden second reservation");testRequire(xrtFutureCancel(F),"cancel quota sample");akWait(F);redisPeerReleaseBlocked(P->Peer);xfuture* Closed=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Closed);
    for(unsigned Which=0;Which<3;++Which){xredisclientconfig C;xredisClientConfigInit(&C);xredisPoolConfigInit(&K);if(Which==0)C.MaxCommandBytes=Wire;else if(Which==1)K.MaxCommandBytes=Wire;else C.MaxCommandBytes=K.MaxCommandBytes=Charged;
        P=redisAskingPeerStart(AK_NORMAL,false,XREDIS_INIT_RESP3,&C,&K);A=redisAskingAcquire(P);F=(void*)(uintptr_t)1;xredisadmission Status=xredisLeaseAskingCallAsync(P->Lease,Args,2,5000,&F);
        if(Which<2)testRequire(Status==XREDIS_ADMISSION_OVERLOADED && F==(void*)(uintptr_t)1,"metadata alone exceeds exact wire-only cap");else{testRequire(Status==XREDIS_ADMISSION_ACCEPTED && akWait(F)->Query.Status==XREDIS_REQUEST_OK,"exact total charge accepted by both caps");xrtFutureDestroy(F);}
        Closed=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(Closed);
    }
    P=redisAskingPeerStart(AK_HOLD_BAD_CONTROL,false,XREDIS_INIT_RESP3,NULL,NULL);A=redisAskingAcquire(P);F=akSubmit(P,5000);redisPeerWait(&P->Queries,1);testRequire(xrtFutureCancel(F),"abandon before malformed control arrives");const xredisaskingresult* R=akWait(F);xredisaskingresult Saved=*R;testRequire(!R->ControlReceived && R->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED,"unknown immutable prefix knowledge");redisPeerReleaseBlocked(P->Peer);Closed=akReturn(P,false);testRequire(!memcmp(R,&Saved,sizeof(Saved)),"late malformed prefix closes without changing published result");redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Closed);
}
static void akForgottenAndClose(void)
{
    redisaskingpeer* P=redisAskingPeerStart(AK_NORMAL,false,XREDIS_INIT_RESP3,NULL,NULL);xfuture* A=redisAskingAcquire(P);
    for(unsigned K=0;K<96;++K)xrtFutureDestroy(akSubmit(P,5000));
    xredisclientinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;
    do{testRequire(xredisClientInfo(P->Client,&I) && xrtTimer()<End,"forgotten pair retirement");if(I.Requests)xrtThreadYield();}while(I.Requests);
    testRequire(I.Accepted==96 && I.Completed==96 && !I.CommandBytes,"observer loss releases all request/state owners");xfuture* Closed=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(Closed);
    P=redisAskingPeerStart(AK_HOLD_CONTROL,false,XREDIS_INIT_RESP3,NULL,NULL);A=redisAskingAcquire(P);xfuture* F=akSubmit(P,5000);redisPeerWait(&P->Queries,1);testRequire(xredisPoolClose(P->Pool),"close during outstanding pair");testRequire(akWait(F)->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED,"pool close preserves execution uncertainty");Closed=akReturn(P,false);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Closed);
}
#if defined(XREDIS_ASKING_WHITEBOX) && defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
typedef struct akgate {xnetpost Post;xmutex Gate;xatomic32 Entered,Left;} akgate;
static void akBlocked(xnetworker* W,ptr Data)
{(void)W;akgate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);xrtMutexLock(&G->Gate);xrtMutexUnlock(&G->Gate);xrtAtomic32Store(&G->Left,1,XMEMORY_RELEASE);}
static void akPostHold(redisaskingpeer* P,akgate* G)
{memset(G,0,sizeof(*G));testRequire(xrtMutexInit(&G->Gate) && xrtMutexLock(&G->Gate) && xrtNetPostInit(&G->Post) && xrtNetPost(xrtNetEngineWorker(P->Peer->Engine,0),&G->Post,akBlocked,G),"pause client wire turn");}
static void akHold(redisaskingpeer* P,akgate* G){akPostHold(P,G);redisPeerWait(&G->Entered,1);}
static void akRelease(akgate* G){xrtMutexUnlock(&G->Gate);redisPeerWait(&G->Left,1);xrtMutexDestroy(&G->Gate);}
static void akPartialTls(void)
{
    for(unsigned Stop=1;Stop<=2;++Stop){xredisclientconfig C;xredisClientConfigInit(&C);C.WorkRequests=1;C.DrainTimeout=10000;redisaskingpeer* P=redisAskingPeerStart(AK_NORMAL,true,XREDIS_INIT_RESP3,&C,NULL);xfuture* A=redisAskingAcquire(P);unsigned char* Bytes=xrtMalloc(65536);testRequire(Bytes!=NULL,"partial TLS fixture");memset(Bytes,'a',65536);Bytes[65535]='z';xbytesview Args[]={XRT_BYTES_LITERAL("APPLYLARGE"),{Bytes,65536}};xfuture* F=NULL;akgate Before,Inside;
        if(Stop==1)akHold(P,&Before);
        testRequire(xredisLeaseAskingCallAsync(P->Lease,Args,2,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"partial TLS admission");memset(Bytes,0xcc,65536);xrtFree(Bytes);
        if(Stop==1){akPostHold(P,&Inside);akRelease(&Before);redisPeerWait(&Inside.Entered,1);xredisclient* Native=P->Client;
            testRequire(Native->TlsWrite && xredisAskingPair(Native->TlsWrite) && Native->TlsWritten>0 && Native->TlsWritten<sizeof(xredisAskingWire)-1,"actual partial ASKING accepted before cancel");
            testRequire(!xrtAtomic32Load(&P->Prefixes,XMEMORY_ACQUIRE) && xrtFutureCancel(F),"stop inside prefix before server consumes it");akRelease(&Inside);
        }else{redisPeerWait(&P->Prefixes,1);bool Seen=false;
            for(unsigned Attempt=0;Attempt<128 && !Seen;++Attempt){akHold(P,&Inside);xredisclient* Native=P->Client;xredisrequest* R=Native->TlsWrite;
                Seen=R && xredisAskingPair(R) && xredisAskingState(R)->Seen==1 && Native->TlsWritten>=sizeof(xredisAskingWire)-1 && Native->TlsWritten<R->WireBytes;
                if(Seen){testRequire(xrtFutureCancel(F),"stop after prefix ACK during partial query");}
                akRelease(&Inside);if(!Seen)xrtThreadYield();}
            testRequire(Seen,"observed exact prefix ACK during partial query write");
        }
        const xredisaskingresult* R=akWait(F);xredisaskingresult Saved=*R;testRequire(R->Query.Status==XREDIS_REQUEST_CANCELLED && R->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED && R->ControlReceived==(Stop==2),"partial stop has exact immutable control knowledge");xfuture* Returned=akReturn(P,true);testRequire(!memcmp(R,&Saved,sizeof(Saved)),"late TLS drain does not mutate retained result");
        xfuture* Next=redisAskingAcquire(P);xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* O=NULL;testRequire(xredisLeaseCall(P->Lease,&Ping,1,5000,&O)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(O))->Status==XREDIS_REQUEST_OK,"partial TLS group drained before slot reuse");xfuture* Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Returned);xrtFutureDestroy(Next);xrtFutureDestroy(O);xrtFutureDestroy(Last);
    }
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
static void akLargeTls(void)
{redisaskingpeer* P=redisAskingPeerStart(AK_NORMAL,true,XREDIS_INIT_RESP3,NULL,NULL);xfuture* A=redisAskingAcquire(P);unsigned char* Data=xrtMalloc(65536);testRequire(Data!=NULL,"large query fixture");memset(Data,'a',65536);Data[65535]='z';xbytesview Args[]={XRT_BYTES_LITERAL("APPLYLARGE"),{Data,65536}};xfuture* F=NULL;testRequire(xredisLeaseAskingCallAsync(P->Lease,Args,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"fragmented TLS group");memset(Data,0xcc,65536);xrtFree(Data);testRequire(akWait(F)->Query.Status==XREDIS_REQUEST_OK,"TLS early control cannot retire a partial wire");xfuture* Last=akReturn(P,true);redisAskingPeerStop(P);xrtFutureDestroy(A);xrtFutureDestroy(F);xrtFutureDestroy(Last);}
#endif
int main(void)
{akBasic(false,XREDIS_INIT_RESP2);akBasic(false,XREDIS_INIT_RESP3);akErrors(false);akStops(false);akUnsent();akDrainExpiry(false);akConcurrent(false);akReentry();akLimitsOom();akQuotaBoundaryAndLateProtocol();akForgottenAndClose();
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
akBasic(true,XREDIS_INIT_RESP2);akBasic(true,XREDIS_INIT_RESP3);akErrors(true);akStops(true);akDrainExpiry(true);akConcurrent(true);akLargeTls();
#if defined(XREDIS_ASKING_WHITEBOX)
akPartialTls();
#endif
#endif
puts("[PASS] leased ASKING: adjacent groups, explicit execution/control errors, binary ownership, cancellation/deadline drain/reuse, OOM, worker reentry and producers");return 0;}
