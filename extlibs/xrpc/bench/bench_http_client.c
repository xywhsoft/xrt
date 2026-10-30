#include "../examples/common/http_peer.h"
typedef struct rpcHttpBenchSlot {xfuture* F;double Scheduled;size_t Next;} rpcHttpBenchSlot;
typedef struct rpcHttpBenchRun {
    rpcHttpBenchSlot* Slots;double *Latency,*Lateness;size_t Head,Count,Completed,Rejected,Failed;
} rpcHttpBenchRun;
static void rpcHttpBenchCollect(rpcHttpBenchRun* B)
{
    size_t* Link=&B->Head;
    while(*Link!=SIZE_MAX){rpcHttpBenchSlot* S=&B->Slots[*Link];
        if(xrtFutureState(S->F)==XFUTURE_PENDING){Link=&S->Next;continue;}
        xfutureresult F;bool OK=xrtFutureResult(S->F,&F) && F.State==XFUTURE_RESOLVED && F.Value;
        if(OK){const xrpccallresult* R=F.Value;xrpcresponseinfo I={0};I.Size=sizeof(I);
            OK=R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED && xrpcResponsesInfo(R->Responses,&I) &&
                I.Count==1 && I.Messages[0].Result.Raw.Size==2 && !memcmp(I.Messages[0].Result.Raw.Data,"[]",2);}
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
    if(!Count || Count>1000000u || !(Rate>0) || Rate>1000000. || argc>3)return 1;
    rpcHttpBenchRun B={0};B.Head=SIZE_MAX;B.Slots=xrtCalloc(Count,sizeof(*B.Slots));B.Latency=xrtMalloc(Count*sizeof(double));B.Lateness=xrtMalloc(Count*sizeof(double));
    if(!B.Slots || !B.Latency || !B.Lateness)return 2;
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpconfig K;xrpcHttpConfigInit(&K);K.MaxRequests=128;
    xrpchttpclient* C=rpcHttpConnect(&P,&K);double Start=xrtTimer();size_t Accepted=0;
    for(size_t N=0;N<Count;++N){double Scheduled=Start+(double)N/Rate;
        while(xrtTimer()<Scheduled){rpcHttpBenchCollect(&B);xrtSleep(1);}
        B.Lateness[N]=(xrtTimer()-Scheduled)*1000000.;xfuture* F=NULL;
        xrpcadmission A=xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&F);
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
    puts("workload: fixed absolute arrivals, all offers/counts retained, queue-inclusive observation from planned arrival including late offers/refusals and Future polling; one persistent HTTP/TCP connection, local C fixture on another worker, construction/encode/send/parse/Future read/destruction included; no TLS or independent comparator");
    rpcHttpClose(&P,C);xrtFree(B.Slots);xrtFree(B.Latency);xrtFree(B.Lateness);return B.Failed?4:0;
}
