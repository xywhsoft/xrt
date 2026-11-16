#define XTP_BENCH_NATIVE
#include <stdlib.h>
#include <time.h>
#include "bench_clock.h"
struct native_peer;
static void native_report(unsigned,struct native_peer*,double,size_t);
#include "../examples/support/native_loopback.h"
static int native_compare(const void* a,const void* b)
{double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y?1:0;}
static void native_report(unsigned mode,native_peer* p,double seconds,size_t peak)
{
    qsort(p->Latency,p->Completed,sizeof(*p->Latency),native_compare);
    size_t n=p->Completed;
    printf("{\"mode\":%u,\"peer\":%u,\"offered\":%zu,\"count\":%zu,\"rejected\":%zu,\"rate\":%zu,\"body\":%zu,\"window\":%zu,\"seconds\":%.9f,\"rpc_per_sec\":%.3f,\"p50_us\":%.3f,\"p95_us\":%.3f,\"p99_us\":%.3f,\"p999_us\":%.3f,\"max_us\":%.3f,\"generator_lag_us\":%.3f,\"generator_ticks\":%zu,\"xtp_allocations\":%zu,\"peak_reserved_bytes\":%zu,\"plaintext_bytes\":%zu,\"writer_calls\":%zu,\"native_sent_bytes\":%llu,\"native_received_bytes\":%llu,\"read_events\":%llu,\"write_events\":%llu}\n",
        mode,p->Index,native_count,n,p->Rejected,native_rate,native_body,native_window,seconds,n/seconds,n?p->Latency[n/2]:0,
        n?p->Latency[(n-1)*95/100]:0,n?p->Latency[(n-1)*99/100]:0,n?p->Latency[(n-1)*999/1000]:0,n?p->Latency[n-1]:0,
        p->GeneratorLagUs,p->GeneratorTicks,p->AllocationCalls,peak,p->PlaintextBytes,p->WriteCalls,
        (unsigned long long)p->NativeSent,(unsigned long long)p->NativeReceived,(unsigned long long)p->ReadEvents,(unsigned long long)p->WriteEvents);
}
int main(int argc,char** argv)
{
    unsigned mode=argc>1?(unsigned)strtoul(argv[1],NULL,10):0;
    native_count=argc>2?strtoul(argv[2],NULL,10):10000;native_body=argc>3?strtoul(argv[3],NULL,10):64;
    native_window=argc>4?strtoul(argv[4],NULL,10):32;
    native_rate=argc>5?strtoul(argv[5],NULL,10):0;
    if(mode>2||!native_count||native_count>1000000||native_body>65536||!native_window||native_window>32||native_rate>1000000)return 1;
    double cpu=xtp_bench_cpu();run(mode,false);printf("cpu_seconds: %.6f\n",xtp_bench_cpu()-cpu);return 0;
}
