/* Public-API retirement stress, shared by modular and native single builds.
 * A separate joining thread bounds regressions that would hang Destroy. */
#include <stdio.h>
#include <stdlib.h>
static void shutdownRequire(bool valid, const char* message)
{
    if (!valid) { fprintf(stderr, "[FAIL] executor shutdown: %s\n", message); exit(1); }
}
static void shutdownJob(ptr raw)
{ (void)xrtAtomic32FetchAdd((xatomic32*)raw, 1, XMEMORY_RELAXED); }
static int32 shutdownRounds(ptr raw)
{
    xatomic32* completed = raw;
    for (unsigned round = 0; round < 1024; ++round) {
        xexecutorconfig config = {round % 8 == 0 ? 2u : 1u, 4, 0};
        xatomic32 jobs; xrtAtomic32Init(&jobs, 0);
        xexecutor* executor = xrtExecutorCreate(&config);
        shutdownRequire(executor != NULL, "create");
        unsigned expected = round % 4;
        xexecutoritem items[3];
        for (unsigned i = 0; i < expected; ++i)
            items[i] = (xexecutoritem){shutdownJob, &jobs, NULL, NULL};
        if (expected)
            shutdownRequire(xrtExecutorSubmitBatch(executor, items, expected), "submit");
        if (round % 2) {
            shutdownRequire(xrtExecutorClose(executor), "close");
            shutdownRequire(xrtExecutorWaitFor(executor, 5000) == XWAIT_OK, "drain");
        }
        shutdownRequire(xrtExecutorDestroy(executor), "physical join");
        shutdownRequire(xrtAtomic32Load(&jobs, XMEMORY_ACQUIRE) == expected, "lost work");
        (void)xrtAtomic32FetchAdd(completed, 1, XMEMORY_RELEASE);
    }
    return 0;
}
static void testExecutorShutdown(void)
{
    xatomic32 completed; xrtAtomic32Init(&completed, 0);
    xthread* thread = xrtThreadCreate(shutdownRounds, &completed, 0);
    shutdownRequire(thread != NULL, "driver create");
    shutdownRequire(xrtThreadWaitFor(thread, 30000) == XWAIT_OK, "bounded retirement watchdog");
    xrtThreadDestroy(thread);
    shutdownRequire(xrtAtomic32Load(&completed, XMEMORY_ACQUIRE) == 1024, "incomplete rounds");
}
