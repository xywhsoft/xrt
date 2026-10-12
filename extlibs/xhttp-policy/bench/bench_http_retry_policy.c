#include "../../../dev/bench/bench_common.h"
#include <xhttp-policy.h>
int main(int argc,char** argv) {
 uint32 Iterations=xbenchArgU32(argc,argv,1,1000000u); uint64 Checksum=0; xbenchtimer Timer;
 if (!Iterations) return 1;
 xbenchTimerStart(&Timer);
 for (uint32 i=0;i<Iterations;i++) { uint64 Delay; if (!xrtHttpRetryBackoff(250,30000,i%12,&Delay)) return 2; Checksum += Delay; }
 xbenchTimerStop(&Timer);
 xbenchPrintMetricDouble("retry_policy_ops_per_sec",xbenchSafeRate(Iterations,xbenchTimerElapsedNs(&Timer)));
 xbenchPrintMetricU64("checksum",Checksum); return 0;
}
