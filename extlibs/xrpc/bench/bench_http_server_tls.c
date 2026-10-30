#include "../tests/support/http_tls_service.h"
#include "http_measure.h"
#include "process_cpu.h"
#include <xrt/memory.h>
#include <math.h>

typedef struct rpcHttpBenchSlot {
    xfuture* F;rpcBenchObservation Observation;
    double Scheduled,Submitted,Terminal,Observed;
    size_t Next;int Admission,Status,Delivery;
} rpcHttpBenchSlot;
typedef struct rpcHttpBenchRun {
    rpcHttpBenchSlot* Slots;
    double *Latency,*Service,*ObserverLag,*Lateness;
    double LastTerminal;
    size_t Head,Count,Completed,Rejected,Failed,Mode,Ready;
} rpcHttpBenchRun;
static void rpcHttpBenchCollect(rpcHttpBenchRun* B)
{
    size_t* Link=&B->Head;
    while(*Link!=SIZE_MAX){rpcHttpBenchSlot* S=&B->Slots[*Link];double At=0;
        if(!rpcBenchObserved(&S->Observation,&At)){Link=&S->Next;continue;}
        S->Terminal=At;S->Observed=xrtTimer();
        xfutureresult F;bool OK=xrtFutureResult(S->F,&F) && F.State==XFUTURE_RESOLVED && F.Value;
        if(OK){const xrpccallresult* R=F.Value;xrpcresponseinfo I={0};I.Size=sizeof(I);S->Status=(int)R->Status;S->Delivery=(int)R->Delivery;
            if(B->Mode==1)OK=R->Status==XRPC_CALL_NOTIFY_DONE && R->Delivery==XRPC_MAY_HAVE_EXECUTED && !R->Responses;
            else{OK=R->Status==(B->Mode?XRPC_CALL_BATCH_DONE:XRPC_CALL_OK) && R->Delivery==XRPC_REPLIED && xrpcResponsesInfo(R->Responses,&I) && I.Count==(B->Mode?B->Mode:1);
                for(size_t K=0;OK && K<I.Count;++K){size_t Index=B->Mode?((const xrpcbatchcallresult*)R)->Indices[K]:0;
                    OK=Index<I.Count && I.Messages[Index].Kind==XRPC_MESSAGE_RESULT && I.Messages[Index].Result.Raw.Size==2 && !memcmp(I.Messages[Index].Result.Raw.Data,"[]",2);}}}
        if(!OK)++B->Failed;
        B->Latency[B->Count]=(S->Terminal-S->Scheduled)*1000000.;
        B->Service[B->Count++]=(S->Terminal-S->Submitted)*1000000.;
        B->ObserverLag[B->Completed++]=(S->Observed-S->Terminal)*1000000.;
        if(S->Terminal>B->LastTerminal)B->LastTerminal=S->Terminal;
        rpcBenchObservationUnit(&S->Observation);xrtFutureDestroy(S->F);S->F=NULL;*Link=S->Next;
    }
}
static int rpcHttpBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
static double rpcHttpBenchPercentile(const double* Values,size_t Count,size_t Parts)
{return Count?Values[(Count*Parts+999u)/1000u-1u]:0.;}
static bool rpcHttpBenchTrace(cstr Path,const rpcHttpBenchRun* B,size_t Count,double Start)
{
    if(!Path)return true;
    FILE* F=fopen(Path,"wb");if(!F)return false;
    bool OK=fputs("index,scheduled_us,submitted_us,terminal_us,observed_us,admission,status,delivery,watch_ready\n",F)>=0;
    for(size_t N=0;OK && N<Count;++N){const rpcHttpBenchSlot* S=&B->Slots[N];
        OK=fprintf(F,"%zu,%.9f,%.9f,%.9f,%.9f,%d,%d,%d,%d\n",N,(S->Scheduled-Start)*1000000.,(S->Submitted-Start)*1000000.,
            (S->Terminal-Start)*1000000.,(S->Observed-Start)*1000000.,S->Admission,S->Status,S->Delivery,S->Admission==XRPC_ADMISSION_ACCEPTED?(int)S->Observation.Ready:-1)>0;}
    if(ferror(F))OK=false;
    if(fclose(F))OK=false;
    return OK;
}
int main(int argc,char** argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):1000;double Rate=argc>2?strtod(argv[2],NULL):1000.;
    size_t Mode=argc>3?(size_t)strtoull(argv[3],NULL,10):0;
    if(!Count || Count>1000000u || !(Rate>0) || !isfinite(Rate) || Rate>1000000. || Mode>256 || argc>7)return 1;
    bool Secure=argc>4?strtoul(argv[4],NULL,10)!=0:true;bool Sleep=argc>5 && !strcmp(argv[5],"sleep");
    if(argc>5 && !Sleep && strcmp(argv[5],"yield"))return 1;
    rpcHttpBenchRun B={0};B.Mode=Mode;B.Head=SIZE_MAX;B.Slots=xrtCalloc(Count,sizeof(*B.Slots));
    B.Latency=xrtMalloc(Count*sizeof(double));B.Service=xrtMalloc(Count*sizeof(double));B.ObserverLag=xrtMalloc(Count*sizeof(double));B.Lateness=xrtMalloc(Count*sizeof(double));
    if(!B.Slots || !B.Latency || !B.Service || !B.ObserverLag || !B.Lateness)return 2;
    rpcTlsService P;rpcTlsServiceStart(&P,NULL,NULL,Secure);xrpchttpconfig K;xrpcHttpConfigInit(&K);K.MaxRequests=128;
    xrpchttpitem Items[256];for(size_t N=0;N<Mode;++N)Items[N]=(xrpchttpitem){XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")};
    xrpchttpclient* C=Secure?rpcTlsServiceClient(&P,&K):rpcServiceClient(&P.Base,&K);
    xfuture* Ready=rpcServiceSubmit(C,"echo","[]",5000);rpcServiceRequire(rpcServiceResult(Ready)->Status==XRPC_CALL_OK,"matched benchmark readiness");xrtFutureDestroy(Ready);
    double CpuStart=0,CpuEnd=0;rpcServiceRequire(rpcBenchProcessCpu(&CpuStart),"initial process CPU");double Start=xrtTimer();size_t Accepted=0;
    for(size_t N=0;N<Count;++N){rpcHttpBenchSlot* S=&B.Slots[N];S->Scheduled=Start+(double)N/Rate;S->Status=S->Delivery=-1;
        while(xrtTimer()<S->Scheduled){rpcHttpBenchCollect(&B);if(Sleep)xrtSleep(1);else xrtThreadYield();}
        S->Submitted=xrtTimer();B.Lateness[N]=(S->Submitted-S->Scheduled)*1000000.;
        xrpcadmission A=Mode==1?xrpcHttpNotifyAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&S->F):
            Mode?xrpcHttpBatchAsync(C,Items,Mode,5000,&S->F):xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&S->F);S->Admission=(int)A;
        if(A==XRPC_ADMISSION_ACCEPTED){++Accepted;rpcServiceRequire(rpcBenchObserve(&S->Observation,S->F),"benchmark completion watch");if(S->Observation.Ready)++B.Ready;S->Next=B.Head;B.Head=N;}
        else{++B.Rejected;if(A!=XRPC_ADMISSION_OVERLOADED)++B.Failed;S->Terminal=S->Observed=xrtTimer();
            B.Latency[B.Count]=(S->Terminal-S->Scheduled)*1000000.;B.Service[B.Count++]=(S->Terminal-S->Submitted)*1000000.;if(S->Terminal>B.LastTerminal)B.LastTerminal=S->Terminal;}
        rpcHttpBenchCollect(&B);
    }
    double End=xrtTimer()+15;while(B.Head!=SIZE_MAX && xrtTimer()<End){rpcHttpBenchCollect(&B);if(B.Head!=SIZE_MAX){if(Sleep)xrtSleep(1);else xrtThreadYield();}}
    double ObservedSeconds=xrtTimer()-Start,Seconds=B.LastTerminal-Start;rpcServiceRequire(rpcBenchProcessCpu(&CpuEnd) && CpuEnd>=CpuStart,"final process CPU");
    if(B.Head!=SIZE_MAX || B.Count!=Count || B.Completed!=Accepted || !Accepted || !(Seconds>0) || ObservedSeconds<Seconds)return 3;
    qsort(B.Latency,Count,sizeof(double),rpcHttpBenchCompare);qsort(B.Service,Count,sizeof(double),rpcHttpBenchCompare);
    qsort(B.Lateness,Count,sizeof(double),rpcHttpBenchCompare);qsort(B.ObserverLag,Accepted,sizeof(double),rpcHttpBenchCompare);
    printf("offered: %zu\naccepted: %zu\nrejected: %zu\ncompleted: %zu\nfailed: %zu\nlatency_samples: %zu\nwatch_pending: %zu\nwatch_ready: %zu\nobserver_samples: %zu\n",Count,Accepted,B.Rejected,B.Completed,B.Failed,B.Count,Accepted-B.Ready,B.Ready,Accepted);
    printf("completed_per_sec: %.9f\nobserved_per_sec: %.9f\nprocess_cpu_us: %.9f\ncpu_us_per_offer: %.9f\n",(double)B.Completed/Seconds,(double)B.Completed/ObservedSeconds,CpuEnd-CpuStart,(CpuEnd-CpuStart)/(double)Count);
    printf("all_p50_us: %.9f\nall_p95_us: %.9f\nall_p99_us: %.9f\nall_p999_us: %.9f\n",rpcHttpBenchPercentile(B.Latency,Count,500),rpcHttpBenchPercentile(B.Latency,Count,950),rpcHttpBenchPercentile(B.Latency,Count,990),rpcHttpBenchPercentile(B.Latency,Count,999));
    printf("submit_p50_us: %.9f\nsubmit_p99_us: %.9f\nsubmit_p999_us: %.9f\n",rpcHttpBenchPercentile(B.Service,Count,500),rpcHttpBenchPercentile(B.Service,Count,990),rpcHttpBenchPercentile(B.Service,Count,999));
    printf("observer_lag_p50_us: %.9f\nobserver_lag_p99_us: %.9f\nobserver_lag_p999_us: %.9f\noffer_lateness_p99_us: %.9f\n",rpcHttpBenchPercentile(B.ObserverLag,Accepted,500),rpcHttpBenchPercentile(B.ObserverLag,Accepted,990),rpcHttpBenchPercentile(B.ObserverLag,Accepted,999),rpcHttpBenchPercentile(B.Lateness,Count,990));
    printf("terminal_elapsed_us: %.9f\ncollector_elapsed_us: %.9f\n",Seconds*1000000.,ObservedSeconds*1000000.);
    printf("harness_slot_bytes: %zu\nharness_arrays_bytes: %zu\n",sizeof(rpcHttpBenchSlot),Count*(sizeof(rpcHttpBenchSlot)+4u*sizeof(double)));
    puts("workload: absolute planned arrivals; every offer includes queue/arrival delay; terminal WATCH notification timestamp (READY registration is a separately counted upper bound, not historical Core publication); submit-to-notification and collector lag reported separately; refusals included at admission return; matched TCP/TLS 1.3 AES-128-GCM, same closure/workers/payload/rate and warmed persistent production server with bounded executor; setup/handshake/readiness/shutdown excluded; process CPU includes all threads, watch/timer/atomic/collection/validation/destruction and yielding load generator; array allocation/sort/report/optional trace excluded; no comparator, saturation or isolated native/TLS latency claim");
    printf("arrival_wait: %s\ntransport: %s\nmode: %s\nitems_per_request: %zu\n",Sleep?"sleep1":"yield",Secure?"tls":"tcp",Mode==1?"notification":Mode?"batch":"call",Mode?Mode:1);
    bool Traced=rpcHttpBenchTrace(argc>6?argv[6]:NULL,&B,Count,Start);
    rpcServiceClientClose(C);rpcTlsServiceStop(&P,Secure);xrtFree(B.Slots);xrtFree(B.Latency);xrtFree(B.Service);xrtFree(B.ObserverLag);xrtFree(B.Lateness);return !Traced?5:B.Failed?4:0;
}
