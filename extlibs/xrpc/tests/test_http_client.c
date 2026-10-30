#include "test.h"
#include "../examples/common/http_peer.h"
static void testHttpBasic(void)
{
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpconfig K;xrpcHttpConfigInit(&K);K.ReadBytes=3;K.WorkRequests=2;
    xrpchttpclient* C=rpcHttpConnect(&P,&K);xfuture* F[64];
    for(unsigned I=0;I<64;++I){char Params[32];snprintf(Params,sizeof(Params),"[%u]",I);F[I]=rpcHttpSubmit(C,"echo",Params,5000);memset(Params,'x',sizeof(Params));}
    for(unsigned I=0;I<64;++I){const xrpccallresult* R=rpcHttpResult(F[I]);testRequire(R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED,"HTTP ordinary result");
        xrpcresponseinfo V={0};V.Size=sizeof(V);testRequire(xrpcResponsesInfo(R->Responses,&V),"HTTP retained snapshot");char Expected[32];int N=snprintf(Expected,sizeof(Expected),"[%u]",I);
        testRequire(V.Messages[0].Result.Raw.Size==(size_t)N && !memcmp(V.Messages[0].Result.Raw.Data,Expected,(size_t)N),"HTTP queued correlation/copied params");if(I)xrtFutureDestroy(F[I]);
    }
    xfuture* Chunk=rpcHttpSubmit(C,"chunk",NULL,5000);testRequire(rpcHttpResult(Chunk)->Status==XRPC_CALL_OK,"HTTP chunk/informational/trailer");xrtFutureDestroy(Chunk);
    xfuture* Error=rpcHttpSubmit(C,"error",NULL,5000);testRequire(rpcHttpResult(Error)->Status==XRPC_CALL_REMOTE_ERROR,"HTTP error body at status 500");xrtFutureDestroy(Error);
    rpcHttpClose(&P,C);testRequire(rpcHttpResult(F[0])->Status==XRPC_CALL_OK,"HTTP result retained engine/client");xrtFutureDestroy(F[0]);
}
static void testHttpStops(void)
{
    for(unsigned Mode=0;Mode<2;++Mode){rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpclient* C=rpcHttpConnect(&P,NULL);
        xfuture* Hold=rpcHttpSubmit(C,"hold",NULL,Mode?5000:1000);rpcHttpWait(&P.Commands,1);
        xfuture* Cancel=rpcHttpSubmit(C,"echo","[]",5000);testRequire(xrtFutureCancel(Cancel),"HTTP queued cancel");
        const xrpccallresult* R=rpcHttpResult(Cancel);testRequire(R->Status==XRPC_CALL_CANCELLED && R->Delivery==XRPC_NOT_SENT,"HTTP queued cancel delivery");
        xfuture* Due=rpcHttpSubmit(C,"echo","[]",0);R=rpcHttpResult(Due);testRequire(R->Status==XRPC_CALL_TIMEOUT && R->Delivery==XRPC_NOT_SENT,"HTTP immediate deadline sent bytes");
        if(Mode)testRequire(xrtFutureCancel(Hold),"HTTP sent cancel");
        R=rpcHttpResult(Hold);
        testRequire(R->Status==(Mode?XRPC_CALL_CANCELLED:XRPC_CALL_TIMEOUT) && R->Delivery==XRPC_MAY_HAVE_EXECUTED,"HTTP sent stop certainty");
        xrtFutureDestroy(Hold);xrtFutureDestroy(Cancel);xrtFutureDestroy(Due);rpcHttpClose(&P,C);
        testRequire(xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE)==1,"HTTP cancelled work replayed/sent");
    }
}
static void testHttpFailureAndCloseBody(void)
{
    cstr Methods[]={"badid","drop","close","broken","large","trunc","encoding"};
    xrpccallstatus Expected[]={XRPC_CALL_PROTOCOL,XRPC_CALL_TRANSPORT,XRPC_CALL_OK,XRPC_CALL_PROTOCOL,XRPC_CALL_LIMIT,XRPC_CALL_PROTOCOL,XRPC_CALL_PROTOCOL};
    for(unsigned I=0;I<7;++I){rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpclient* C=rpcHttpConnect(&P,NULL);xfuture* F=rpcHttpSubmit(C,Methods[I],NULL,5000);
        const xrpccallresult* R=rpcHttpResult(F);testRequire(R->Status==Expected[I],"HTTP ID/transport/EOF/framing/resource outcome");
        rpcHttpClose(&P,C);xrtFutureDestroy(F);
    }
    for(unsigned Mode=0;Mode<5;++Mode){rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpconfig K;xrpcHttpConfigInit(&K);
        if(Mode==0)K.Head.MaxHead=128;else if(Mode==1)K.Body.MaxTrailer=8;else if(Mode==2)K.Responses.MaxOwnedBytes=1;else if(Mode==3)K.Responses.Decode.MaxInputBytes=8;else K.MaxResponseWireBytes=100;
        xrpchttpclient* C=rpcHttpConnect(&P,&K);xfuture* F=rpcHttpSubmit(C,Mode==0?"bighead":(Mode==1 || Mode==4)?"chunk":"echo",NULL,5000);
        testRequire(rpcHttpResult(F)->Status==XRPC_CALL_LIMIT,"HTTP head/trailer/snapshot/JSON cap misclassified");rpcHttpClose(&P,C);xrtFutureDestroy(F);
    }
}
static void rpcHttpIdle(xrpchttpclient* C)
{double End=xrtTimer()+10;for(;;){xrpchttpinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpInfo(C,&I),"HTTP idle info");if(!I.Requests)return;testRequire(xrtTimer()<End,"HTTP physical request tail did not retire");xrtSleep(1);}}
static void testHttpAdmissionAndOOM(void)
{
    for(unsigned Bytes=0;Bytes<2;++Bytes){rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpconfig K;xrpcHttpConfigInit(&K);
        if(Bytes){K.MaxRequestBytes=256;K.MaxRequestBytesTotal=256;}else K.MaxRequests=1;
        xrpchttpclient* C=rpcHttpConnect(&P,&K);xfuture* Hold=rpcHttpSubmit(C,"hold",NULL,5000);rpcHttpWait(&P.Commands,1);
        xfuture* Output=(xfuture*)(uintptr_t)1;xrtSetErrorInfo(XERR_VALUE,"xrt.test",321,"caller error");const xerror* Ambient=xrtGetError();
        testRequire(xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&Output)==XRPC_ADMISSION_OVERLOADED &&
            Output==(xfuture*)(uintptr_t)1 && xrtGetError()==Ambient,"HTTP count/byte refusal changed output/ambient");xrtClearError();
        testRequire(xrtFutureCancel(Hold),"HTTP admission stop");(void)rpcHttpResult(Hold);xrtFutureDestroy(Hold);rpcHttpClose(&P,C);
    }
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpclient* C=rpcHttpConnect(&P,NULL);unsigned Success=0,Failure=0;
    for(size_t Limit=1;Limit<=4096;++Limit){
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"HTTP quota enter");
        xfuture* F=NULL;xrpcadmission A=xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&F);
        testRequire(xrtAllocationBudgetLeave(&Scope),"HTTP quota leave");
        if(A==XRPC_ADMISSION_ACCEPTED){++Success;testRequire(rpcHttpResult(F)->Status==XRPC_CALL_OK,"HTTP admitted quota result");xrtFutureDestroy(F);rpcHttpIdle(C);}
        else{++Failure;testRequire(A==XRPC_ADMISSION_ERROR && !F,"HTTP failed quota published output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"HTTP construction quota leaked");xrtAllocationBudgetFree(B);
    }
    testRequire(Success && Failure,"HTTP quota sweep missed allocation boundary");
    xfuture* Output=(xfuture*)(uintptr_t)1;
    testRequire(xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("["),5000,&Output)==XRPC_ADMISSION_ERROR && Output==(xfuture*)(uintptr_t)1,"HTTP malformed local params publication");xrtClearError();
    testRequire(xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,(xfuture**)C)==XRPC_ADMISSION_ERROR,"HTTP output/client alias");xrtClearError();
    rpcHttpClose(&P,C);
}
typedef struct rpcHttpSubmitter {xrpchttpclient* C;xfuture* F[32];unsigned Index;} rpcHttpSubmitter;
static int32 rpcHttpSubmitThread(ptr Data)
{rpcHttpSubmitter* S=Data;for(unsigned I=0;I<32;++I){char Param[32];snprintf(Param,sizeof(Param),"[%u]",S->Index*32+I);if(S->Index<2)S->F[I]=rpcHttpSubmit(S->C,"echo",Param,5000);
 else if(S->Index==2){xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),{Param,strlen(Param)}},{XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),{0}}};testRequire(xrpcHttpBatchAsync(S->C,Items,2,5000,&S->F[I])==XRPC_ADMISSION_ACCEPTED,"concurrent batch submit");}
 else testRequire(xrpcHttpNotifyAsync(S->C,XRT_STR_LITERAL("echo"),(xstrview){Param,strlen(Param)},5000,&S->F[I])==XRPC_ADMISSION_ACCEPTED,"concurrent notify submit");}return 0;}
static void testHttpConcurrentSubmit(void)
{
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpclient* C=rpcHttpConnect(&P,NULL);rpcHttpSubmitter S[4];xthread* T[4];
    for(unsigned I=0;I<4;++I){S[I]=(rpcHttpSubmitter){0};S[I].C=C;S[I].Index=I;T[I]=xrtThreadCreate(rpcHttpSubmitThread,&S[I],0);testRequire(T[I]!=NULL,"HTTP producer thread");}
    for(unsigned I=0;I<4;++I){testRequire(xrtThreadWait(T[I])==XWAIT_OK,"HTTP producer join");xrtThreadDestroy(T[I]);
        for(unsigned J=0;J<32;++J){const xrpccallresult* R=rpcHttpResult(S[I].F[J]);testRequire(R->Status==(I==3?XRPC_CALL_NOTIFY_DONE:I==2?XRPC_CALL_BATCH_DONE:XRPC_CALL_OK),"HTTP concurrent completion");
            if(I==3){xrtFutureDestroy(S[I].F[J]);continue;}
            xrpcresponseinfo V={0};V.Size=sizeof(V);testRequire(xrpcResponsesInfo(R->Responses,&V),"HTTP concurrent snapshot");char Expected[32];int N=snprintf(Expected,sizeof(Expected),"[%u]",I*32+J);
            testRequire(V.Messages[0].Result.Raw.Size==(size_t)N && !memcmp(V.Messages[0].Result.Raw.Data,Expected,(size_t)N),"HTTP concurrent ID mismatch");xrtFutureDestroy(S[I].F[J]);}
    }
    rpcHttpClose(&P,C);
}
typedef struct rpcHttpReentry {xrpchttpclient* C;unsigned Called;} rpcHttpReentry;
static void rpcHttpCompletionClose(ptr Data)
{rpcHttpReentry* R=Data;++R->Called;testRequire(xrpcHttpClose(R->C),"HTTP reentrant close");xrpcHttpRelease(R->C);R->C=NULL;}
static void testHttpReentry(void)
{
    rpcHttpPeer P;rpcHttpPeerStart(&P);rpcHttpReentry R={rpcHttpConnect(&P,NULL),0};xfuture* Closed=xrpcHttpClosed(R.C);
    xfuture* F=rpcHttpSubmit(R.C,"hold",NULL,5000);rpcHttpWait(&P.Commands,1);xfuturewatch Watch;
    testRequire(xrtFutureWatchInit(&Watch,rpcHttpCompletionClose,NULL,&R) && xrtFutureWatchAdd(F,&Watch)==XFUTURE_WATCH_PENDING && xrtFutureCancel(F),"HTTP reentry watch/cancel");
    testRequire(rpcHttpResult(F)->Status==XRPC_CALL_CANCELLED,"HTTP reentrant outcome");xrtFutureWatchRemove(F,&Watch);
    testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && R.Called==1 && !R.C,"HTTP reentrant physical close");
    xrtFutureDestroy(Closed);xrtFutureDestroy(F);rpcHttpPeerStop(&P);
}
static void testHttpConstruction(void)
{
    const size_t Limits[]={1,64,4096,65536,131072,262144,1048576};unsigned Success=0,Failure=0;
    for(size_t N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){rpcHttpPeer P;rpcHttpPeerStart(&P);xallocationbudget* B=NULL;xallocationscope S={0};
        testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&S),"HTTP factory quota enter");
        xrpchttpclient* C=NULL;bool OK=xrpcHttpConnect(P.Engine,&P.Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,&C);
        testRequire(xrtAllocationBudgetLeave(&S),"HTTP factory quota leave");
        if(OK){++Success;rpcHttpClose(&P,C);}else{++Failure;testRequire(!C,"HTTP factory failure output");rpcHttpPeerStop(&P);}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"HTTP factory retained budget after engine join");xrtAllocationBudgetFree(B);
    }
    testRequire(Success && Failure,"HTTP factory quota boundaries");
    rpcHttpPeer P;rpcHttpPeerStart(&P);
    for(unsigned Mode=0;Mode<7;++Mode){xrpchttpconfig K;xrpcHttpConfigInit(&K);
        if(Mode==0)K.Size=0;else if(Mode==1)K.Responses.Decode.Size=0;else if(Mode==2)K.Head.MaxFields=0;
        else if(Mode==3)K.Body.MaxBody=0;else if(Mode==4)K.Responses.Decode.MaxIdBytes=1;else if(Mode==5)K.MaxRequestBytesTotal=0;else K.MaxResponseWireBytes=0;
        xrpchttpclient* C=(xrpchttpclient*)(uintptr_t)1;
        testRequire(!xrpcHttpConnect(P.Engine,&P.Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),&K,&C) && C==(xrpchttpclient*)(uintptr_t)1,"HTTP bad factory configuration publication");xrtClearError();
    }
    rpcHttpPeerStop(&P);
}
static xfuture* rpcHttpBatch(xrpchttpclient* C,const xrpchttpitem* Items,size_t Count,int64 Timeout)
{xfuture* F=NULL;testRequire(xrpcHttpBatchAsync(C,Items,Count,Timeout,&F)==XRPC_ADMISSION_ACCEPTED,"HTTP batch admission");return F;}
static void testHttpNotificationsAndBatches(void)
{
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpclient* C=rpcHttpConnect(&P,NULL);xfuture* F=NULL;
    testRequire(xrpcHttpNotifyAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&F)==XRPC_ADMISSION_ACCEPTED,"HTTP notification admission");
    const xrpccallresult* R=rpcHttpResult(F);testRequire(R->Status==XRPC_CALL_NOTIFY_DONE && R->Delivery==XRPC_MAY_HAVE_EXECUTED && !R->Responses,"HTTP notification confirmation");xrtFutureDestroy(F);
    char Params[]="[7]";xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("escaped"),{Params,3}},
        {XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("error"),{0}}};
    F=rpcHttpBatch(C,Items,3,5000);memset(Params,'x',3);
    const xrpcbatchcallresult* B=(const xrpcbatchcallresult*)rpcHttpResult(F);xrpcresponseinfo I={0};I.Size=sizeof(I);
    testRequire(B->Call.Status==XRPC_CALL_BATCH_DONE && B->Call.Delivery==XRPC_REPLIED && B->Count==3 && B->Indices[0]==1 && B->Indices[1]==SIZE_MAX && B->Indices[2]==0 &&
        xrpcResponsesInfo(B->Call.Responses,&I) && I.Batch && I.Count==2 && I.Messages[0].Kind==XRPC_MESSAGE_ERROR && I.Messages[1].Result.Raw.Size==3 && !memcmp(I.Messages[1].Result.Raw.Data,"[7]",3),"HTTP reverse/escaped-ID/error/copied batch");
    xfuture* One=rpcHttpBatch(C,&Items[2],1,5000);testRequire(rpcHttpResult(One)->Status==XRPC_CALL_BATCH_DONE,"single-item batch lost array shape");xrtFutureDestroy(One);
    xrpchttpitem Notes[2]={Items[1],Items[1]};One=rpcHttpBatch(C,Notes,2,5000);B=(const xrpcbatchcallresult*)rpcHttpResult(One);
    testRequire(B->Call.Status==XRPC_CALL_NOTIFY_DONE && B->Call.Delivery==XRPC_MAY_HAVE_EXECUTED && B->Count==2 && B->Indices[0]==SIZE_MAX && B->Indices[1]==SIZE_MAX,"HTTP all-notification batch");xrtFutureDestroy(One);
    rpcHttpClose(&P,C);B=(const xrpcbatchcallresult*)rpcHttpResult(F);testRequire(xrpcResponsesInfo(B->Call.Responses,&I) && B->Indices[0]==1,"batch retained engine/client");xrtFutureDestroy(F);
    cstr Bad[]={"duplicate","missing","unknown","notifyid","shape","tail"};
    for(size_t K=0;K<6;++K){rpcHttpPeerStart(&P);C=rpcHttpConnect(&P,NULL);Items[0].Method=(xstrview){Bad[K],strlen(Bad[K])};Items[0].Params=(xstrview){0};
        F=rpcHttpBatch(C,Items,3,5000);B=(const xrpcbatchcallresult*)rpcHttpResult(F);
        testRequire(B->Call.Status==XRPC_CALL_PROTOCOL && !B->Call.Responses && B->Count==3 && B->Indices[0]==SIZE_MAX && B->Indices[1]==SIZE_MAX && B->Indices[2]==SIZE_MAX,"malformed batch partial publication");rpcHttpClose(&P,C);xrtFutureDestroy(F);
    }
    for(unsigned Mode=0;Mode<2;++Mode){rpcHttpPeerStart(&P);C=rpcHttpConnect(&P,NULL);
        testRequire(xrpcHttpNotifyAsync(C,Mode?XRT_STR_LITERAL("notifyerror"):XRT_STR_LITERAL("notifybody"),(xstrview){0},5000,&F)==XRPC_ADMISSION_ACCEPTED,"bad notification admission");R=rpcHttpResult(F);
        testRequire(R->Status==(Mode?XRPC_CALL_HTTP_ERROR:XRPC_CALL_PROTOCOL) && !R->Responses && R->Delivery==XRPC_MAY_HAVE_EXECUTED,"notification HTTP failure policy");
        if(Mode)testRequire(R->Cause && xrtErrorCode(R->Cause)==503,"notification HTTP diagnostic");
        rpcHttpClose(&P,C);xrtFutureDestroy(F);
    }
}
static void testHttpBatchStopsLimitsOOM(void)
{
    xrpchttpitem Items[]={{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("hold"),{0}},{XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),{0}},{XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")}};
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpconfig K;xrpcHttpConfigInit(&K);K.MaxBatchBytesTotal=1;xrpchttpclient* C=rpcHttpConnect(&P,&K);xfuture* F=(xfuture*)(uintptr_t)1;
    testRequire(xrpcHttpBatchAsync(C,Items,3,5000,&F)==XRPC_ADMISSION_OVERLOADED && F==(xfuture*)(uintptr_t)1,"batch byte admission output");
    xrpchttpitem Oversized[257]={0};
    testRequire(xrpcHttpBatchAsync(C,Oversized,257,5000,&F)==XRPC_ADMISSION_OVERLOADED,"batch count admission");rpcHttpClose(&P,C);
    rpcHttpPeerStart(&P);C=rpcHttpConnect(&P,NULL);F=rpcHttpBatch(C,Items,3,5000);rpcHttpWait(&P.Commands,1);
    xfuture* Queued=rpcHttpBatch(C,Items,3,5000);testRequire(xrtFutureCancel(Queued),"queued batch cancel");
    const xrpcbatchcallresult* B=(const xrpcbatchcallresult*)rpcHttpResult(Queued);testRequire(B->Call.Status==XRPC_CALL_CANCELLED && B->Call.Delivery==XRPC_NOT_SENT && B->Count==3 && B->Indices[0]==SIZE_MAX,"queued batch stop");
    xfuture* Due=rpcHttpBatch(C,Items,3,0);B=(const xrpcbatchcallresult*)rpcHttpResult(Due);testRequire(B->Call.Status==XRPC_CALL_TIMEOUT && B->Call.Delivery==XRPC_NOT_SENT,"batch entry deadline");
    testRequire(xrtFutureCancel(F),"sent batch cancel");B=(const xrpcbatchcallresult*)rpcHttpResult(F);testRequire(B->Call.Status==XRPC_CALL_CANCELLED && B->Call.Delivery==XRPC_MAY_HAVE_EXECUTED && !B->Call.Responses && B->Indices[2]==SIZE_MAX,"sent batch stop");
    xrtFutureDestroy(F);xrtFutureDestroy(Queued);xrtFutureDestroy(Due);rpcHttpClose(&P,C);testRequire(xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE)==1,"batch replay");
    rpcHttpPeerStart(&P);C=rpcHttpConnect(&P,NULL);Items[0].Method=XRT_STR_LITERAL("echo");unsigned Passed=0,Failed=0;
    for(size_t Limit=1;Limit<=4096;++Limit){xallocationbudget* Budget=NULL;xallocationscope Scope={0};F=NULL;
        testRequire(xrtAllocationBudgetCreate(Limit,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"batch request prefix enter");
        xrpcadmission A=xrpcHttpBatchAsync(C,Items,3,5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"batch prefix leave");
        if(A==XRPC_ADMISSION_ACCEPTED){++Passed;testRequire(rpcHttpResult(F)->Status==XRPC_CALL_BATCH_DONE,"admitted batch quota result");xrtFutureDestroy(F);rpcHttpIdle(C);}
        else{++Failed;testRequire(A==XRPC_ADMISSION_ERROR && !F,"batch failed allocation output");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes,"batch construction quota leaked");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Passed && Failed,"batch quota boundaries");rpcHttpClose(&P,C);
}
int main(void)
{testHttpBasic();testHttpStops();testHttpFailureAndCloseBody();testHttpAdmissionAndOOM();testHttpConcurrentSubmit();testHttpReentry();testHttpConstruction();testHttpNotificationsAndBatches();testHttpBatchStopsLimitsOOM();puts("[PASS] xrpc HTTP client: ordinary/notification/batch/reverse-ID/escaped-ID/atomic-map/error/limits/cancel/deadline/OOM/concurrency/reentry/physical-close");return 0;}
