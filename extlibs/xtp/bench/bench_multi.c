/* Many real connections share one engine. Each bidirectional pair is pinned
 * to one worker; independent pairs execute concurrently on different workers. */
#define XTP_BENCH_MULTI
#define main native_single_main
#include "bench_native.c"
#undef main
static void multi_begin(xnetworker* worker,void* user)
{native_test* t=user;(void)worker;t->Begin=xrtTimer();for(size_t i=0;i<2;++i){native_peer* p=&t->Peers[i];
    p->Submitted=p->Completed=p->Messages=0;native_fill(p);drain(p);}}
int main(int argc,char** argv)
{
    size_t connections=argc>1?strtoul(argv[1],NULL,10):16,workers=argc>2?strtoul(argv[2],NULL,10):4;
    native_count=argc>3?strtoul(argv[3],NULL,10):10000;unsigned mode=argc>4?(unsigned)strtoul(argv[4],NULL,10):0;
    native_body=argc>5?strtoul(argv[5],NULL,10):64;native_window=32;native_rate=0;
    unsigned duration=argc>6?(unsigned)strtoul(argv[6],NULL,10):0;
    if(!connections||connections>64||!workers||workers>16||!native_count||native_count>100000||mode>2||native_body>65536||duration>86400)return 1;
    native_test* tests=calloc(connections,sizeof(*tests));xnetlistener** listeners=calloc(connections,sizeof(*listeners));
    xtlsstream** tls=calloc(connections,sizeof(*tls));assert(tests&&listeners&&tls);
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Workers=(uint32)workers;
    xnetengine* engine=xrtNetEngineCreate(&ec);assert(engine&&xrtNetEngineStart(engine));
    for(size_t k=0;k<connections;++k){native_test* t=&tests[k];t->Mode=mode;t->Engine=engine;
        xrtAtomic32Init(&t->Ready,0);xrtAtomic32Init(&t->Done,0);xrtAtomic32Init(&t->Failed,0);xrtAtomic32Init(&t->Closing,0);
        for(unsigned i=0;i<2;++i){native_peer* p=&t->Peers[i];p->Test=t;p->Index=i;p->Begin=calloc(native_count,sizeof(double));p->Latency=calloc(native_count,sizeof(double));assert(p->Begin&&p->Latency);}
        if(mode==1)tls_setup(t);
        xnetlistenconfig c;xrtNetListenConfigInit(&c);c.Affinity=k;c.Distribution=XNET_ACCEPT_LOCAL;
        assert(xrtNetAddrParse(&c.Address,"127.0.0.1",0));xnetlistenerevents e={accept_peer,NULL,NULL};
        listeners[k]=xrtNetListen(engine,&c,&e,NULL,t);assert(listeners[k]);xnetaddr a;assert(xrtNetListenerLocal(listeners[k],&a));
        if(mode==1){xtlsclientconfig cc;xrtTlsClientConfigInit(&cc);cc.Context=t->Context;cc.Verifier=t->Verifier;cc.ServerName=XRT_STR_LITERAL("xtp.fixture");
            static const xstrview protocols[]={XRT_STR_INIT("xtp/3")};cc.Protocols=protocols;cc.ProtocolCount=1;
            tls[k]=xrtTlsStreamConnect(engine,&a,k,&c.Stream,&cc,&t->TlsConfig,&tls_events,&t->Peers[0]);assert(tls[k]);}
        else assert(xrtNetStreamConnect(engine,&a,k,&c.Stream,&tcp_events,&t->Peers[0]));}
    double deadline=xrtTimer()+60;
    for(size_t k=0;k<connections;++k)while(xrtAtomic32Load(&tests[k].Ready,XMEMORY_ACQUIRE)!=2){assert(xrtTimer()<deadline);xrtSleep(1);}
    double begin=xrtTimer(),cpu=xtp_bench_cpu();size_t rounds=0;
    do{for(size_t k=0;k<connections;++k){xrtAtomic32Store(&tests[k].Done,0,XMEMORY_RELEASE);assert(xrtNetEnginePost(engine,k,multi_begin,&tests[k]));}
        deadline=xrtTimer()+120;for(size_t k=0;k<connections;++k)while(!xrtAtomic32Load(&tests[k].Done,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtSleep(1);}
        ++rounds;
    }while(duration&&xrtTimer()-begin<duration);
    double seconds=xrtTimer()-begin,cpuseconds=xtp_bench_cpu()-cpu;
    for(size_t k=0;k<connections;++k)xrtAtomic32Store(&tests[k].Closing,1,XMEMORY_RELEASE);
    for(size_t k=0;k<connections;++k){native_test* t=&tests[k];for(size_t i=0;i<2;++i){native_peer* p=&t->Peers[i];
        if(mode==1){xtlsstream* stream=i?p->Tls:tls[k];(void)xrtTlsStreamAbort(stream);assert(xrtTlsStreamWaitRetired(stream,10000)==XWAIT_OK);xrtTlsStreamDestroy(stream);}
        else{if(p->Ws)(void)xrtWsConnAbort(p->Ws);else assert(xrtNetStreamClose(p->Tcp));assert(xrtNetStreamWaitRetired(p->Tcp,10000)==XWAIT_OK);
            if(p->Ws)xrtWsConnDestroy(p->Ws);
            xrtNetStreamDestroy(p->Tcp);}}
        assert(xrtNetListenerClose(listeners[k]));xrtNetListenerDestroy(listeners[k]);}
    deadline=xrtTimer()+10;xnetretireresult retired;
    while((retired=xrtNetEngineTryDestroy(engine))==XNET_RETIRE_BUSY){assert(xrtTimer()<deadline);xrtSleep(1);}assert(retired==XNET_RETIRE_READY);
    size_t peak=0,writes=0;uint64_t copied=0;double slowest=0,fastest=1e9;
    for(size_t k=0;k<connections;++k){native_test* t=&tests[k];if(t->Seconds>slowest)slowest=t->Seconds;if(t->Seconds<fastest)fastest=t->Seconds;
        for(size_t i=0;i<2;++i){native_peer* p=&t->Peers[i];xtpinfo info;assert(xtpInfo(p->Xtp,&info)==XTP_OK);peak+=info.PeakReservedBytes;copied+=info.BatchCopiedBytes;writes+=p->WriteCalls;
            assert(p->Completed==native_count&&p->Messages==native_count);qsort(p->Latency,native_count,sizeof(double),native_compare);
            printf("{\"connection\":%zu,\"peer\":%zu,\"completed\":%zu,\"p99_us\":%.3f,\"p999_us\":%.3f,\"max_us\":%.3f}\n",k,i,p->Completed,p->Latency[(native_count-1)*99/100],p->Latency[(native_count-1)*999/1000],p->Latency[native_count-1]);
            assert(xtpClose(p->Xtp,clock_us())==XTP_CLOSED&&xtpDestroy(p->Xtp)==XTP_OK);free(p->Begin);free(p->Latency);}
        if(mode==1){xrtTlsVerifierRelease(t->Verifier);xrtTlsIdentityRelease(t->Identity);xrtTlsContextRelease(t->Context);}}
    printf("{\"summary\":true,\"mode\":%u,\"connections\":%zu,\"workers\":%zu,\"body\":%zu,\"rounds\":%zu,\"completed\":%zu,\"seconds\":%.9f,\"rpc_per_sec\":%.3f,\"cpu_seconds\":%.6f,\"fastest_pair_seconds\":%.9f,\"slowest_pair_seconds\":%.9f,\"peak_xtp_bytes\":%zu,\"writer_calls\":%zu,\"batch_copied_bytes\":%llu}\n",
        mode,connections,workers,native_body,rounds,rounds*connections*2*native_count,seconds,rounds*connections*2*native_count/seconds,cpuseconds,fastest,slowest,peak,writes,(unsigned long long)copied);
    free(tls);free(listeners);free(tests);return 0;
}
