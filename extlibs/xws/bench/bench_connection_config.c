#include "../../../dev/bench/bench_common.h"
#include <xws.h>
int main(int argc, char** argv) {
 size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 10000;
 xwsconnconfig Config; xrtWsConnConfigInit(&Config);
 uint64_t Start = xbenchNowNs();
 for (size_t i = 0; i < Count; i++) { if (!xrtWsConnConfigValid(&Config)) return 1; }
 double Seconds = (double)(xbenchNowNs() - Start) / 1e9;
 printf("connection_config_per_sec=%.3f\n", (double)Count / (Seconds > 0 ? Seconds : 1e-9));
 return 0;
}
