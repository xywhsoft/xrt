#include "../../../dev/bench/bench_common.h"
#include <xhttp-server.h>
int main(int argc,char** argv) {
 uint32 Iterations=xbenchArgU32(argc,argv,1,1000000u); uint64 Checksum=0; xbenchtimer Timer;
 if (!Iterations) return 1;
 xbenchTimerStart(&Timer);
 for (uint32 i=0;i<Iterations;i++) { xhttprouteparam Params[2]; size_t Count; if (xrtHttpRouteMatch(XRT_STR_LITERAL("/users/{id}/files/{name}"), XRT_STR_LITERAL("/users/42/files/readme"), Params,2,&Count) != XHTTP_ROUTE_MATCH) return 2; Checksum += Count + Params[0].Value.Size; }
 xbenchTimerStop(&Timer);
 xbenchPrintMetricDouble("route_match_ops_per_sec",xbenchSafeRate(Iterations,xbenchTimerElapsedNs(&Timer)));
 xbenchPrintMetricU64("checksum",Checksum); return 0;
}
