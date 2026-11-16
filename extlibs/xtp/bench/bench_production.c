/* Persistent connection acceptance, with checked allocator/ledger accounting.
 * Instrumentation is deliberately absent from normal capacity benchmarks. */
#if defined(NDEBUG)
#error "Production acceptance requires enabled runtime assertions"
#endif
#define XTP_BENCH_PRODUCTION
#define XTP_TEST_WSS
#include "bench_multi.c"
