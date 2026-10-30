#include "../tests/support/http_tls_service.h"
#include <xrt/memory.h>
typedef struct rpcHttpBenchSlot {xfuture* F;double Scheduled;size_t Next;} rpcHttpBenchSlot;
typedef struct rpcHttpBenchRun {
    rpcHttpBenchSlot* Slots;double *Latency,*Lateness;size_t Head,Count,Completed,Rejected,Failed,Mode;
} rpcHttpBenchRun;
static void rpcHttpBenchCollect(rpcHttpBenchRun* B)
{
    size_t* Link=&B->Head;
    while(*Link!=SIZE_MAX){rpcHttpBenchSlot* S=&B->Slots[*Link];
        if(xrtFutureState(S->F)==XFUTURE_PENDING){Link=&S->Next;continue;}
        xfutureresult F;bool OK=xrtFutureResult(S->F,&F) && F.State==XFUTURE_RESOLVED && F.Value;
        if(OK){const xrpccallresult* R=F.Value;xrpcresponseinfo I={0};I.Size=sizeof(I);
            if(B->Mode==1)OK=R->Status==XRPC_CALL_NOTIFY_DONE && R->Delivery==XRPC_MAY_HAVE_EXECUTED && !R->Responses;
            else{OK=R->Status==(B->Mode?XRPC_CALL_BATCH_DONE:XRPC_CALL_OK) && R->Delivery==XRPC_REPLIED && xrpcResponsesInfo(R->Responses,&I) && I.Count==(B->Mode?B->Mode:1);
                for(size_t K=0;OK && K<I.Count;++K){size_t Index=B->Mode?((const xrpcbatchcallresult*)R)->Indices[K]:0;
                    OK=Index<I.Count && I.Messages[Index].Kind==XRPC_MESSAGE_RESULT && I.Messages[Index].Result.Raw.Size==2 && !memcmp(I.Messages[Index].Result.Raw.Data,"[]",2);}}}
        if(!OK)++B->Failed;
        B->Latency[B->Count++]=(xrtTimer()-S->Scheduled)*1000000.;++B->Completed;
        xrtFutureDestroy(S->F);S->F=NULL;*Link=S->Next;
    }
}
static int rpcHttpBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
int main(int argc,char** argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):2000;double Rate=argc>2?strtod(argv[2],NULL):2000.;
    size_t Mode=argc>3?(size_t)strtoull(argv[3],NULL,10):0;
    if(!Count || Count>1000000u || !(Rate>0) || Rate>1000000. || Mode>256 || argc>5)return 1;
    bool Secure=argc>4?strtoul(argv[4],NULL,10)!=0:true;
    rpcHttpBenchRun B={0};B.Mode=Mode;B.Head=SIZE_MAX;B.Slots=xrtCalloc(Count,sizeof(*B.Slots));B.Latency=xrtMalloc(Count*sizeof(double));B.Lateness=xrtMalloc(Count*sizeof(double));
    if(!B.Slots || !B.Latency || !B.Lateness)return 2;
    rpcTlsService P;rpcTlsServiceStart(&P,NULL,NULL,Secure);xrpchttpconfig K;xrpcHttpConfigInit(&K);K.MaxRequests=128;
    xrpchttpitem Items[256];for(size_t N=0;N<Mode;++N)Items[N]=(xrpchttpitem){XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")};
    xrpchttpclient* C=Secure?rpcTlsServiceClient(&P,&K):rpcServiceClient(&P.Base,&K);
    xfuture* Ready=rpcServiceSubmit(C,"echo","[]",5000);rpcServiceRequire(rpcServiceResult(Ready)->Status==XRPC_CALL_OK,"matched benchmark readiness");xrtFutureDestroy(Ready);double Start=xrtTimer();size_t Accepted=0;
    for(size_t N=0;N<Count;++N){double Scheduled=Start+(double)N/Rate;
        while(xrtTimer()<Scheduled){rpcHttpBenchCollect(&B);xrtSleep(1);}
        B.Lateness[N]=(xrtTimer()-Scheduled)*1000000.;xfuture* F=NULL;
        xrpcadmission A=Mode==1?xrpcHttpNotifyAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&F):
            Mode?xrpcHttpBatchAsync(C,Items,Mode,5000,&F):xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&F);
        if(A==XRPC_ADMISSION_ACCEPTED){++Accepted;B.Slots[N]=(rpcHttpBenchSlot){F,Scheduled,B.Head};B.Head=N;}
        else{++B.Rejected;if(A!=XRPC_ADMISSION_OVERLOADED)++B.Failed;B.Latency[B.Count++]=(xrtTimer()-Scheduled)*1000000.;}
        rpcHttpBenchCollect(&B);
    }
    double End=xrtTimer()+15;while(B.Head!=SIZE_MAX && xrtTimer()<End){rpcHttpBenchCollect(&B);if(B.Head!=SIZE_MAX)xrtSleep(1);}
    double Seconds=xrtTimer()-Start;
    if(B.Head!=SIZE_MAX || B.Count!=Count || B.Completed!=Accepted || !Accepted || !(Seconds>0))return 3;
    qsort(B.Latency,Count,sizeof(double),rpcHttpBenchCompare);qsort(B.Lateness,Count,sizeof(double),rpcHttpBenchCompare);
    printf("offered: %zu\naccepted: %zu\nrejected: %zu\ncompleted: %zu\nfailed: %zu\nlatency_samples: %zu\ncompleted_per_sec: %.9f\nall_p50_us: %.9f\nall_p99_us: %.9f\noffer_lateness_p99_us: %.9f\n",
        Count,Accepted,B.Rejected,B.Completed,B.Failed,B.Count,(double)B.Completed/Seconds,B.Latency[(Count*50+99)/100-1],B.Latency[(Count*99+99)/100-1],B.Lateness[(Count*99+99)/100-1]);
    puts("workload: fixed absolute arrivals, all offers/counts retained, queue-inclusive observation from planned arrival including late offers/refusals and Future polling; matched plaintext/TLS 1.3 AES-128-GCM modes, one warmed persistent connection, actual production server on round-robin workers and separate bounded executor; same compiled closure, host engine, worker budgets, payload and arrival rate; client/server construction, certificate handshake, one readiness RPC and shutdown outside timing; encode/record/send/parse/Future read/destruction included; fixture identity/trust, no independent comparator or saturation claim");
    printf("transport: %s\n",Secure?"tls":"tcp");
    printf("mode: %s\nitems_per_request: %zu\n",Mode==1?"notification":Mode?"batch":"call",Mode?Mode:1);
    rpcServiceClientClose(C);rpcTlsServiceStop(&P,Secure);xrtFree(B.Slots);xrtFree(B.Latency);xrtFree(B.Lateness);return B.Failed?4:0;
}
