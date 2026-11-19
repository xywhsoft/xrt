#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "../tests/support/sentinel_events_peer.h"
#include <math.h>

enum { SE_BENCH_WARMUP = 8, SE_BENCH_MAX_SLOTS = 8 };

static int seBenchCompare(const void* Left, const void* Right)
{
    double A=*(const double*)Left, B=*(const double*)Right;
    return (A>B)-(A<B);
}

static void seBenchLatency(cstr Label, double* Samples, size_t Count)
{
    qsort(Samples,Count,sizeof(*Samples),seBenchCompare);
    static const unsigned Percentiles[]={500,950,990,999};
    static const char* Names[]={"p50","p95","p99","p999"};
    for(size_t I=0;I<sizeof(Percentiles)/sizeof(Percentiles[0]);++I){
        size_t Rank=(Count*Percentiles[I]+999)/1000;
        printf("%s_%s_us: %.3f\n",Label,Names[I],Samples[Rank-1]);
    }
    printf("%s_max_us: %.3f\n",Label,Samples[Count-1]);
}

static void seBenchReady(sefixture* Fixture, xredispool* Pool)
{
    uint32 Before=xrtAtomic32Load(&Fixture->Pool.Roles,XMEMORY_ACQUIRE);
    seListen(Pool,1);
    double End=xrtTimer()+10;
    for(;;){
        xredissentineleventinfo Info=seInfo(Pool);
        if(Info.Session.State==XREDIS_SESSION_ONLINE && Info.Hints==1 &&
           xrtAtomic32Load(&Fixture->Pool.Roles,XMEMORY_ACQUIRE)>Before)return;
        redisPeerRequire(xrtTimer()<End,"warm listener and initial topology");
        xrtSleep(1);
    }
}

int main(int argc,char** argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):1024;
    size_t Slots=argc>2?(size_t)strtoull(argv[2],NULL,10):1;
    unsigned Mode=argc>3?(unsigned)strtoul(argv[3],NULL,10):1;
    unsigned Replica=argc>4?(unsigned)strtoul(argv[4],NULL,10):0;
    if(argc>5 || !Count || Count>8192 || !Slots || Slots>SE_BENCH_MAX_SLOTS || Mode>1 || Replica)return 2;
    double* Samples=malloc(2*Count*sizeof(*Samples));
    redisPeerRequire(Samples!=NULL,"latency samples");
    double* Complete=Samples+Count;

    sefixture Fixture;
    seStart(&Fixture,false,false,false);
    Fixture.Pool.ReapClosed=true;
    xrtAtomic32Store(&Fixture.Pool.Seed.Base.SkipLive,1,XMEMORY_RELEASE);
    xrtAtomic32Store(&Fixture.Pool.A.Base.SkipLive,1,XMEMORY_RELEASE);
    xrtAtomic32Store(&Fixture.Pool.B.Base.SkipLive,1,XMEMORY_RELEASE);
    xredispool* Pool=seCreate(&Fixture,Slots);
    xfuture* Held[SE_BENCH_MAX_SLOTS];
    for(size_t I=0;I<Slots;++I){
        Held[I]=spAcquire(Pool,10000);
        spPing(spLease(Held[I]));
    }
    seBenchReady(&Fixture,Pool);

    double Active=0, NextTrial=xrtTimer()+.002;
    for(size_t Trial=0;Trial<Count+SE_BENCH_WARMUP;++Trial){
        xfuture *Retiring[SE_BENCH_MAX_SLOTS],*Waiting[SE_BENCH_MAX_SLOTS];
        for(size_t I=0;I<Slots;++I){
            Retiring[I]=xredisLeaseClosed(spLease(Held[I]));
            Waiting[I]=spAcquire(Pool,10000);
            redisPeerRequire(Retiring[I] && !xrtFutureDone(Retiring[I]) && !xrtFutureDone(Waiting[I]),
                "all old leases live and all replacements waiting");
        }
        /* Keep the configured 1ms event throttle out of the measured interval. */
        while(xrtTimer()<NextTrial)xrtThreadYield();
        bool Alternate=(Trial%2)==0;
        xrtAtomic32Store(&Fixture.Pool.Current,Alternate,XMEMORY_RELEASE);
        sewrite Event={.Fixture=&Fixture,.Payload="cache 127.0.0.1 1234 127.0.0.1 5678",.Count=1};
        double Started=xrtTimer();
        if(Mode)seQueue(&Event);
        else redisPeerRequire(xredisSentinelPoolRefresh(Pool)==XREDIS_ADMISSION_ACCEPTED,"explicit trigger");
        spLease(Waiting[0]);
        double First=xrtTimer();
        for(size_t I=1;I<Slots;++I)spLease(Waiting[I]);
        double Last=Slots==1?First:xrtTimer();
        if(Mode)redisPeerWait(&Event.Done,1);

        redisconnectpeer* Target=Alternate?&Fixture.Pool.B.Base:&Fixture.Pool.A.Base;
        xredissentinelpoolinfo Topology=spInfo(Pool);
        redisPeerRequire(Topology.Generation==Trial+2 && Topology.Changes==Trial+1 &&
            Topology.Port==Target->Address.Port && Topology.Phase==XREDIS_SENTINEL_POOL_ONLINE &&
            Topology.LastStatus==XREDIS_SENTINEL_OK && Topology.Connect.Connecting==0,"one verified generation for each trial");
        for(size_t I=0;I<Slots;++I){
            redisPeerRequire(xrtFutureState(Retiring[I])==XFUTURE_RESOLVED,"complete retirement before replacement");
            xrtFutureDestroy(Retiring[I]);
            xrtFutureDestroy(Held[I]);
            Held[I]=Waiting[I];
        }
        uint32 Commands=xrtAtomic32Load(&Target->Commands,XMEMORY_ACQUIRE);
        spPing(spLease(Held[Slots-1]));
        redisPeerRequire(xrtAtomic32Load(&Target->Commands,XMEMORY_ACQUIRE)==Commands+1,
            "replacement is usable on the verified endpoint");
        if(Trial>=SE_BENCH_WARMUP){
            size_t At=Trial-SE_BENCH_WARMUP;
            Samples[At]=(First-Started)*1000000;
            Complete[At]=(Last-Started)*1000000;
            Active+=Last-Started;
        }
        NextTrial=Last+.002;
    }
    xredissentineleventinfo Info=seInfo(Pool);
    uint64 Expected=Mode?Count+SE_BENCH_WARMUP:0;
    redisPeerRequire(Info.Session.Generations==1 && Info.Matched==Expected && !Info.Ignored &&
        Info.Hints==Expected+1,"one subscriber and exact hint accounting");
    redisPeerRequire(isfinite(Active) && Active>0,"monotonic recovery intervals");
    printf("samples: %zu\nslots: %zu\nrecovery_batches_per_active_sec: %.3f\n",
        Count,Slots,Count/Active);
    seBenchLatency("first_lease",Samples,Count);
    seBenchLatency("all_leases",Complete,Count);
    free(Samples);
    seStop(&Fixture,Pool);
    for(size_t I=0;I<Slots;++I)xrtFutureDestroy(Held[I]);
    return 0;
}
