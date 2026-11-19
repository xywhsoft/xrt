#ifndef XRPC_BENCH_FIXED_ARRIVAL_H
#define XRPC_BENCH_FIXED_ARRIVAL_H

#include "http_measure.h"
#include "process_cpu.h"
#include <xrpc/call.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*rpcBenchSubmit)(ptr,xfuture**);
typedef bool (*rpcBenchValidate)(ptr,const xrpccallresult*);

typedef struct rpcBenchArrivalConfig {
    size_t Count;
    double Rate;
    bool Sleep;
    cstr Trace;
    int Accepted,Overloaded;
    rpcBenchSubmit Submit;
    rpcBenchValidate Validate;
    ptr Context;
} rpcBenchArrivalConfig;

typedef struct rpcBenchOffer {
    xfuture* Future;
    rpcBenchObservation Observation;
    double Scheduled,Submitted,Terminal,Observed;
    size_t Next;
    int Admission,Status,Delivery;
} rpcBenchOffer;

typedef struct rpcBenchArrivals {
    rpcBenchOffer* Offers;
    double *Latency,*Service,*ObserverLag,*Lateness;
    double LastTerminal;
    size_t Head,Samples,Accepted,Completed,Rejected,Failed,Ready;
} rpcBenchArrivals;

static void rpcBenchRecord(rpcBenchArrivals* Run,const rpcBenchOffer* Offer)
{
    Run->Latency[Run->Samples]=(Offer->Terminal-Offer->Scheduled)*1e6;
    Run->Service[Run->Samples++]=(Offer->Terminal-Offer->Submitted)*1e6;
    if(Offer->Terminal>Run->LastTerminal)Run->LastTerminal=Offer->Terminal;
}

static void rpcBenchCollect(rpcBenchArrivals* Run,const rpcBenchArrivalConfig* Config)
{
    size_t* Link=&Run->Head;
    while(*Link!=SIZE_MAX){
        rpcBenchOffer* Offer=&Run->Offers[*Link];
        if(!rpcBenchObserved(&Offer->Observation,&Offer->Terminal)){
            Link=&Offer->Next;
            continue;
        }
        Offer->Observed=xrtTimer();
        xfutureresult Future;
        const xrpccallresult* Result=NULL;
        if(xrtFutureResult(Offer->Future,&Future) && Future.State==XFUTURE_RESOLVED)
            Result=Future.Value;
        if(Result){
            Offer->Status=(int)Result->Status;
            Offer->Delivery=(int)Result->Delivery;
        }
        if(!Result || !Config->Validate(Config->Context,Result))++Run->Failed;
        rpcBenchRecord(Run,Offer);
        Run->ObserverLag[Run->Completed++]=(Offer->Observed-Offer->Terminal)*1e6;
        rpcBenchObservationUnit(&Offer->Observation);
        xrtFutureDestroy(Offer->Future);
        Offer->Future=NULL;
        *Link=Offer->Next;
    }
}

static bool rpcBenchOfferAt(rpcBenchArrivals* Run,const rpcBenchArrivalConfig* Config,
                            size_t Index,double Start)
{
    rpcBenchOffer* Offer=&Run->Offers[Index];
    Offer->Scheduled=Start+(double)Index/Config->Rate;
    Offer->Status=Offer->Delivery=-1;
    while(xrtTimer()<Offer->Scheduled){
        rpcBenchCollect(Run,Config);
        if(Config->Sleep)xrtSleep(1);
        else xrtThreadYield();
    }
    Offer->Submitted=xrtTimer();
    Run->Lateness[Index]=(Offer->Submitted-Offer->Scheduled)*1e6;
    Offer->Admission=Config->Submit(Config->Context,&Offer->Future);
    if(Offer->Admission==Config->Accepted){
        if(!Offer->Future || !rpcBenchObserve(&Offer->Observation,Offer->Future)){
            xrtFutureDestroy(Offer->Future);
            return false;
        }
        ++Run->Accepted;
        if(Offer->Observation.Ready)++Run->Ready;
        Offer->Next=Run->Head;
        Run->Head=Index;
    }else {
        ++Run->Rejected;
        if(Offer->Admission!=Config->Overloaded)++Run->Failed;
        Offer->Terminal=Offer->Observed=xrtTimer();
        rpcBenchRecord(Run,Offer);
    }
    rpcBenchCollect(Run,Config);
    return true;
}

static int rpcBenchArrivalCompare(const void* Left,const void* Right)
{
    double A=*(const double*)Left,B=*(const double*)Right;
    return (A>B)-(A<B);
}

static double rpcBenchArrivalRank(const double* Sorted,size_t Count,size_t Parts)
{return Count?Sorted[(Count*Parts+999u)/1000u-1u]:0.;}

static bool rpcBenchWriteTrace(const rpcBenchArrivals* Run,const rpcBenchArrivalConfig* Config,double Start)
{
    if(!Config->Trace || !*Config->Trace)return true;
    FILE* Stream=fopen(Config->Trace,"wb");
    if(!Stream)return false;
    bool OK=fputs("index,scheduled_us,submitted_us,terminal_us,observed_us,admission,status,delivery,watch_ready\n",Stream)>=0;
    for(size_t Index=0;OK && Index<Config->Count;++Index){
        const rpcBenchOffer* Offer=&Run->Offers[Index];
        int Ready=Offer->Admission==Config->Accepted?(int)Offer->Observation.Ready:-1;
        OK=fprintf(Stream,"%zu,%.9f,%.9f,%.9f,%.9f,%d,%d,%d,%d\n",Index,
            (Offer->Scheduled-Start)*1e6,(Offer->Submitted-Start)*1e6,
            (Offer->Terminal-Start)*1e6,(Offer->Observed-Start)*1e6,
            Offer->Admission,Offer->Status,Offer->Delivery,Ready)>0;
    }
    if(ferror(Stream))OK=false;
    if(fclose(Stream))OK=false;
    return OK;
}

static void rpcBenchReport(rpcBenchArrivals* Run,size_t Count,double Seconds,double Observed,double Cpu)
{
    qsort(Run->Latency,Count,sizeof(double),rpcBenchArrivalCompare);
    qsort(Run->Service,Count,sizeof(double),rpcBenchArrivalCompare);
    qsort(Run->Lateness,Count,sizeof(double),rpcBenchArrivalCompare);
    qsort(Run->ObserverLag,Run->Completed,sizeof(double),rpcBenchArrivalCompare);
    printf("offered: %zu\naccepted: %zu\nrejected: %zu\ncompleted: %zu\nfailed: %zu\nlatency_samples: %zu\n"
           "watch_pending: %zu\nwatch_ready: %zu\nobserver_samples: %zu\n",
        Count,Run->Accepted,Run->Rejected,Run->Completed,Run->Failed,Run->Samples,
        Run->Accepted-Run->Ready,Run->Ready,Run->Completed);
    printf("completed_per_sec: %.9f\nobserved_per_sec: %.9f\nprocess_cpu_us: %.9f\ncpu_us_per_offer: %.9f\n",
        (double)Run->Completed/Seconds,(double)Run->Completed/Observed,Cpu,Cpu/(double)Count);
    printf("all_p50_us: %.9f\nall_p95_us: %.9f\nall_p99_us: %.9f\nall_p999_us: %.9f\n",
        rpcBenchArrivalRank(Run->Latency,Count,500),rpcBenchArrivalRank(Run->Latency,Count,950),
        rpcBenchArrivalRank(Run->Latency,Count,990),rpcBenchArrivalRank(Run->Latency,Count,999));
    printf("submit_p50_us: %.9f\nsubmit_p99_us: %.9f\nsubmit_p999_us: %.9f\n",
        rpcBenchArrivalRank(Run->Service,Count,500),rpcBenchArrivalRank(Run->Service,Count,990),
        rpcBenchArrivalRank(Run->Service,Count,999));
    printf("observer_lag_p50_us: %.9f\nobserver_lag_p99_us: %.9f\nobserver_lag_p999_us: %.9f\n"
           "offer_lateness_p99_us: %.9f\n",
        rpcBenchArrivalRank(Run->ObserverLag,Run->Completed,500),rpcBenchArrivalRank(Run->ObserverLag,Run->Completed,990),
        rpcBenchArrivalRank(Run->ObserverLag,Run->Completed,999),rpcBenchArrivalRank(Run->Lateness,Count,990));
    printf("terminal_elapsed_us: %.9f\ncollector_elapsed_us: %.9f\n"
           "harness_slot_bytes: %zu\nharness_arrays_bytes: %zu\n",
        Seconds*1e6,Observed*1e6,sizeof(rpcBenchOffer),Count*(sizeof(rpcBenchOffer)+4u*sizeof(double)));
}

static int rpcBenchRunArrivals(const rpcBenchArrivalConfig* Config)
{
    const size_t Count=Config->Count;
    rpcBenchArrivals Run={.Head=SIZE_MAX};
    Run.Offers=xrtCalloc(Count,sizeof(*Run.Offers));
    double* Values=xrtMalloc(Count*4u*sizeof(double));
    int Status=2;
    if(!Run.Offers || !Values)goto Done;
    Run.Latency=Values;
    Run.Service=Values+Count;
    Run.ObserverLag=Values+2u*Count;
    Run.Lateness=Values+3u*Count;

    double CpuStart,CpuEnd;
    Status=3;
    if(!rpcBenchProcessCpu(&CpuStart))goto Done;
    double Start=xrtTimer();
    for(size_t Index=0;Index<Count;++Index)
        if(!rpcBenchOfferAt(&Run,Config,Index,Start))goto Done;
    double End=xrtTimer()+15.;
    while(Run.Head!=SIZE_MAX && xrtTimer()<End){
        rpcBenchCollect(&Run,Config);
        if(Run.Head!=SIZE_MAX){
            if(Config->Sleep)xrtSleep(1);
            else xrtThreadYield();
        }
    }
    double Observed=xrtTimer()-Start,Seconds=Run.LastTerminal-Start;
    if(!rpcBenchProcessCpu(&CpuEnd) || CpuEnd<CpuStart || Run.Head!=SIZE_MAX ||
       Run.Samples!=Count || Run.Completed!=Run.Accepted || !Run.Accepted ||
       !(Seconds>0.) || Observed<Seconds)goto Done;
    rpcBenchReport(&Run,Count,Seconds,Observed,CpuEnd-CpuStart);
    Status=!rpcBenchWriteTrace(&Run,Config,Start)?5:Run.Failed?4:0;
Done:
    for(size_t Link=Run.Head;Link!=SIZE_MAX;Link=Run.Offers[Link].Next){
        rpcBenchOffer* Offer=&Run.Offers[Link];
        rpcBenchObservationUnit(&Offer->Observation);
        xrtFutureDestroy(Offer->Future);
    }
    xrtFree(Values);
    xrtFree(Run.Offers);
    return Status;
}

#endif
