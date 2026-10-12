#include "../../../dev/bench/bench_common.h"
#include <xws-group.h>
int main(int argc, char** argv) {
 size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 10000;
 xwsgroup* pGroup = xrtWsGroupCreate(8); if (pGroup == NULL) return 1;
 uint64_t Start = xbenchNowNs();
 for (size_t i = 0; i < Count; i++) { xwsgroupsnapshot* pSnapshot = xrtWsGroupSnapshotCreate(pGroup); if (pSnapshot == NULL || xrtWsGroupSnapshotCount(pSnapshot) != 0) return 1; xrtWsGroupSnapshotDestroy(pSnapshot); }
 double Seconds = (double)(xbenchNowNs() - Start) / 1e9;
 printf("group_snapshot_per_sec=%.3f\n", (double)Count / (Seconds > 0 ? Seconds : 1e-9));
 xrtWsGroupDestroy(pGroup);
 return 0;
}
