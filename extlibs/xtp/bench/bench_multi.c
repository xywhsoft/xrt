/* Many real connections share one engine. Each bidirectional pair is pinned
 * to one worker; independent pairs execute concurrently on different workers. */
#define XTP_BENCH_MULTI
#define main native_single_main
#include "bench_native.c"
#undef main
static void multi_begin(xnetworker* worker,void* user)
{native_test* t=user;(void)worker;t->Begin=xrtTimer();for(size_t i=0;i<2;++i){native_peer* p=&t->Peers[i];
    p->Submitted=p->Completed=p->Messages=0;native_fill(p);drain(p);}}
#if defined(XTP_BENCH_PRODUCTION)
static void multi_inspect(xnetworker* worker,void* user)
{
    native_test* t=user;(void)worker;t->LiveBytes=t->LiveBlocks=t->ReservedPeak=0;
    for(size_t i=0;i<2;++i){native_peer* p=&t->Peers[i];xtpinfo info;assert(xtpInfo(p->Xtp,&info)==XTP_OK);
        assert(!info.Pending&&!info.IncomingRequests&&!info.TxFrames&&!info.TxBytes&&!info.Retained);
        assert(info.StorageBytes==p->AllocLive&&p->Completed==native_count&&p->Messages==native_count);
        t->LiveBytes+=p->AllocLive;t->LiveBlocks+=p->AllocBlocks;t->ReservedPeak+=info.PeakReservedBytes;}
    xrtAtomic32Store(&t->Inspected,1,XMEMORY_RELEASE);
}
#endif
int main(int argc,char** argv)
{
    size_t connections=argc>1?strtoul(argv[1],NULL,10):16,workers=argc>2?strtoul(argv[2],NULL,10):4;
    native_count=argc>3?strtoul(argv[3],NULL,10):10000;unsigned mode=argc>4?(unsigned)strtoul(argv[4],NULL,10):0;
    native_body=argc>5?strtoul(argv[5],NULL,10):64;native_window=32;native_rate=0;
    unsigned duration=argc>6?(unsigned)strtoul(argv[6],NULL,10):0;
#if defined(XTP_BENCH_PRODUCTION)
    if(!connections||connections>64||!workers||workers>16||!native_count||native_count>100000||mode>3||native_body>65536||duration>604800)return 1;
#else
    if(!connections||connections>64||!workers||workers>16||!native_count||native_count>100000||mode>2||native_body>65536||duration>86400)return 1;
#endif
    native_test* tests=calloc(connections,sizeof(*tests));xnetlistener** listeners=calloc(connections,sizeof(*listeners));
    xtlsstream** tls=calloc(connections,sizeof(*tls));assert(tests&&listeners&&tls);
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Workers=(uint32)workers;
    xnetengine* engine=xrtNetEngineCreate(&ec);assert(engine&&xrtNetEngineStart(engine));
    for(size_t k=0;k<connections;++k){native_test* t=&tests[k];t->Mode=mode;t->Engine=engine;
#if defined(XTP_BENCH_PRODUCTION)
        if(mode==3){t->Mode=1;t->Wss=true;}
        xrtAtomic32Init(&t->Inspected,0);
#endif
        xrtAtomic32Init(&t->Ready,0);xrtAtomic32Init(&t->Done,0);xrtAtomic32Init(&t->Failed,0);xrtAtomic32Init(&t->Closing,0);
        for(unsigned i=0;i<2;++i){native_peer* p=&t->Peers[i];p->Test=t;p->Index=i;p->Begin=calloc(native_count,sizeof(double));p->Latency=calloc(native_count,sizeof(double));assert(p->Begin&&p->Latency);}
        if(t->Mode==1)tls_setup(t);
        xnetlistenconfig c;xrtNetListenConfigInit(&c);c.Affinity=k;c.Distribution=XNET_ACCEPT_LOCAL;
        assert(xrtNetAddrParse(&c.Address,"127.0.0.1",0));xnetlistenerevents e={accept_peer,NULL,NULL};
        listeners[k]=xrtNetListen(engine,&c,&e,NULL,t);assert(listeners[k]);xnetaddr a;assert(xrtNetListenerLocal(listeners[k],&a));
        if(t->Mode==1){xtlsclientconfig cc;xrtTlsClientConfigInit(&cc);cc.Context=t->Context;cc.Verifier=t->Verifier;cc.ServerName=XRT_STR_LITERAL("xtp.fixture");
            static const xstrview protocols[]={XRT_STR_INIT("xtp/3")};cc.Protocols=protocols;cc.ProtocolCount=1;
#if defined(XTP_BENCH_PRODUCTION)
            static const xstrview http[]={XRT_STR_INIT("http/1.1")};if(t->Wss)cc.Protocols=http;
#endif
            tls[k]=xrtTlsStreamConnect(engine,&a,k,&c.Stream,&cc,&t->TlsConfig,&tls_events,&t->Peers[0]);assert(tls[k]);}
        else assert(xrtNetStreamConnect(engine,&a,k,&c.Stream,&tcp_events,&t->Peers[0]));}
    double deadline=xrtTimer()+60;
    for(size_t k=0;k<connections;++k)while(xrtAtomic32Load(&tests[k].Ready,XMEMORY_ACQUIRE)!=2){assert(xrtTimer()<deadline);xrtSleep(1);}
    double begin=xrtTimer(),cpu=xtp_bench_cpu();uint64_t rounds=0;
#if defined(XTP_BENCH_PRODUCTION)
    double next_report=0;
#endif
    do{for(size_t k=0;k<connections;++k){xrtAtomic32Store(&tests[k].Done,0,XMEMORY_RELEASE);assert(xrtNetEnginePost(engine,k,multi_begin,&tests[k]));}
        deadline=xrtTimer()+120;for(size_t k=0;k<connections;++k)while(!xrtAtomic32Load(&tests[k].Done,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtSleep(1);}
        ++rounds;
#if defined(XTP_BENCH_PRODUCTION)
        for(size_t k=0;k<connections;++k){xrtAtomic32Store(&tests[k].Inspected,0,XMEMORY_RELEASE);assert(xrtNetEnginePost(engine,k,multi_inspect,&tests[k]));}
        for(size_t k=0;k<connections;++k)while(!xrtAtomic32Load(&tests[k].Inspected,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtSleep(1);}
        double elapsed=xrtTimer()-begin;
        if(elapsed>=next_report){size_t live=0,blocks=0,reserved=0;for(size_t k=0;k<connections;++k){live+=tests[k].LiveBytes;blocks+=tests[k].LiveBlocks;reserved+=tests[k].ReservedPeak;}
            printf("{\"checkpoint\":true,\"seconds\":%.9f,\"rounds\":%llu,\"completed\":%llu,\"live_xtp_bytes\":%zu,\"live_xtp_blocks\":%zu,\"peak_xtp_bytes\":%zu,\"ledger_match\":true,\"quiescent\":true}\n",
                elapsed,(unsigned long long)rounds,(unsigned long long)(rounds*connections*2*native_count),live,blocks,reserved);fflush(stdout);next_report=elapsed+1;}
#endif
    }while(duration&&xrtTimer()-begin<duration);
    double seconds=xrtTimer()-begin,cpuseconds=xtp_bench_cpu()-cpu;
    for(size_t k=0;k<connections;++k)xrtAtomic32Store(&tests[k].Closing,1,XMEMORY_RELEASE);
    for(size_t k=0;k<connections;++k){native_test* t=&tests[k];for(size_t i=0;i<2;++i){native_peer* p=&t->Peers[i];
#if defined(XTP_BENCH_PRODUCTION)
        if(mode==3){(void)xrtWsConnAbort(p->Ws);xrtWsConnDestroy(p->Ws);continue;}
#endif
        if(mode==1){xtlsstream* stream=i?p->Tls:tls[k];(void)xrtTlsStreamAbort(stream);assert(xrtTlsStreamWaitRetired(stream,10000)==XWAIT_OK);xrtTlsStreamDestroy(stream);}
        else{if(p->Ws)(void)xrtWsConnAbort(p->Ws);else assert(xrtNetStreamClose(p->Tcp));assert(xrtNetStreamWaitRetired(p->Tcp,10000)==XWAIT_OK);
            if(p->Ws)xrtWsConnDestroy(p->Ws);
            xrtNetStreamDestroy(p->Tcp);}}
        assert(xrtNetListenerClose(listeners[k]));xrtNetListenerDestroy(listeners[k]);}
    deadline=xrtTimer()+10;xnetretireresult retired;
    while((retired=xrtNetEngineTryDestroy(engine))==XNET_RETIRE_BUSY){assert(xrtTimer()<deadline);xrtSleep(1);}assert(retired==XNET_RETIRE_READY);
    size_t peak=0;uint64_t writes=0,copied=0;double slowest=0,fastest=1e9;
    for(size_t k=0;k<connections;++k){native_test* t=&tests[k];if(t->Seconds>slowest)slowest=t->Seconds;if(t->Seconds<fastest)fastest=t->Seconds;
        for(size_t i=0;i<2;++i){native_peer* p=&t->Peers[i];xtpinfo info;assert(xtpInfo(p->Xtp,&info)==XTP_OK);peak+=info.PeakReservedBytes;copied+=info.BatchCopiedBytes;
#if defined(XTP_BENCH_PRODUCTION)
            writes+=p->TotalWrites;
#else
            writes+=p->WriteCalls;
#endif
            assert(p->Completed==native_count&&p->Messages==native_count);qsort(p->Latency,native_count,sizeof(double),native_compare);
            printf("{\"connection\":%zu,\"peer\":%zu,\"completed\":%zu,\"p99_us\":%.3f,\"p999_us\":%.3f,\"max_us\":%.3f}\n",k,i,p->Completed,p->Latency[(native_count-1)*99/100],p->Latency[(native_count-1)*999/1000],p->Latency[native_count-1]);
            assert(xtpClose(p->Xtp,clock_us())==XTP_CLOSED&&xtpDestroy(p->Xtp)==XTP_OK);
#if defined(XTP_BENCH_PRODUCTION)
            assert(!p->AllocLive&&!p->AllocBlocks);
#endif
            free(p->Begin);free(p->Latency);}
        if(t->Mode==1){xrtTlsVerifierRelease(t->Verifier);xrtTlsIdentityRelease(t->Identity);xrtTlsContextRelease(t->Context);}}
    printf("{\"summary\":true,\"mode\":%u,\"connections\":%zu,\"workers\":%zu,\"body\":%zu,\"rounds\":%llu,\"completed\":%llu,\"seconds\":%.9f,\"rpc_per_sec\":%.3f,\"cpu_seconds\":%.6f,\"fastest_pair_seconds\":%.9f,\"slowest_pair_seconds\":%.9f,\"peak_xtp_bytes\":%zu,\"writer_calls\":%llu,\"batch_copied_bytes\":%llu}\n",
        mode,connections,workers,native_body,(unsigned long long)rounds,(unsigned long long)(rounds*connections*2*native_count),seconds,rounds*connections*2*native_count/seconds,cpuseconds,fastest,slowest,peak,(unsigned long long)writes,(unsigned long long)copied);
    free(tls);free(listeners);free(tests);return 0;
}
