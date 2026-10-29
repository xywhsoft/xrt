#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <xredis.h>
#include "../tests/support/pool_connect_peer.h"
#include <errno.h>
#include <limits.h>
#include <time.h>
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif
static bool pcBenchAffinity(const char* Argument)
{
    unsigned long long Mask = 0; char* End = NULL;
    if (Argument) {
        errno = 0; Mask = strtoull(Argument, &End, 0);
        if (errno || !Argument[0] || Argument[0] == '-' || !End || *End || !Mask) return false;
    }
#if defined(_WIN32)
    DWORD_PTR Process = 0, System = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &Process, &System)) return false;
    if (Argument && ((unsigned long long)(DWORD_PTR)Mask != Mask || (Mask & ~(unsigned long long)Process) ||
        !SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)Mask))) return false;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &Process, &System)) return false;
    if (Argument && Process != (DWORD_PTR)Mask) return false;
    printf("affinity: process mask=0x%llx; requested=%s; per-thread pinning=false\n", (unsigned long long)Process, Argument ? Argument : "unchanged");
#elif defined(__linux__)
    cpu_set_t Set; if (sched_getaffinity(0, sizeof(Set), &Set)) return false;
    if (Argument) {
        cpu_set_t Desired; CPU_ZERO(&Desired);
        for (unsigned Index = 0; Index < sizeof(Mask) * CHAR_BIT; ++Index) if (Mask & (1ull << Index)) {
            if (!CPU_ISSET(Index, &Set)) return false;
            CPU_SET(Index, &Desired);
        }
        if (sched_setaffinity(0, sizeof(Desired), &Desired) || sched_getaffinity(0, sizeof(Set), &Set)) return false;
        if (!CPU_EQUAL(&Set, &Desired)) return false;
    }
    printf("affinity: process cpus=");
    bool First = true;
    for (unsigned Index = 0; Index < CPU_SETSIZE; ++Index) if (CPU_ISSET(Index, &Set)) {
        printf("%s%u", First ? "" : ",", Index); First = false;
    }
    printf("; requested=%s; per-thread pinning=false\n", Argument ? Argument : "unchanged");
#else
    if (Argument) return false;
    puts("affinity: unchanged; process pinning unsupported by this benchmark on this platform");
#endif
    return true;
}
static double pcBenchCPU(void)
{
#if defined(_WIN32)
    FILETIME Created, Exited, Kernel, User; ULARGE_INTEGER K, U;
    redisPeerRequire(GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User) != 0, "benchmark CPU time");
    K.LowPart = Kernel.dwLowDateTime; K.HighPart = Kernel.dwHighDateTime; U.LowPart = User.dwLowDateTime; U.HighPart = User.dwHighDateTime;
    return (double)(K.QuadPart + U.QuadPart) * .1;
#elif defined(__linux__)
    struct timespec Value; redisPeerRequire(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &Value) == 0, "benchmark CPU clock");
    return (double)Value.tv_sec * 1000000.0 + (double)Value.tv_nsec * .001;
#else
    return (double)clock() * 1000000.0 / (double)CLOCKS_PER_SEC;
#endif
}
static int pcBenchCompare(const void* Left, const void* Right)
{ double A = *(const double*)Left, B = *(const double*)Right; return (A > B) - (A < B); }
static xfuture* pcBenchAcquire(xredispool* Pool)
{
    xfuture* Future = NULL; redisPeerRequire(xredisPoolAcquire(Pool, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredisleaseresult*)xrtFutureValue(Future))->Lease, "benchmark automatic acquire"); return Future;
}
static void pcBenchReturn(xfuture* Future, bool Abort)
{
    xredislease* Lease = ((const xredisleaseresult*)xrtFutureValue(Future))->Lease; xfuture* Closed = xredisLeaseClosed(Lease);
    if (Abort) xredisLeaseAbort(Lease); else xredisLeaseReturn(Lease);
    redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Closed))->Reusable != Abort, "benchmark physical boundary");
    xrtFutureDestroy(Closed); xrtFutureDestroy(Future);
}
int main(int argc, char** argv)
{
    size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 1000;
    unsigned Mode = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 0; bool Repair = Mode == 1, Fixed = Mode == 2;
    if (!Count || Count > 1000000 || Mode > 2 || argc > 4 || !pcBenchAffinity(argc > 3 ? argv[3] : NULL)) return 2;
    double* Samples = xrtCalloc(Count, sizeof(double)); if (!Samples) return 1;
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Config.RetryMin = Config.RetryMax = 1; xredispool* Pool = NULL;
    if (Fixed) {
        xredisclient* Client = NULL;
        redisPeerRequire(redisConnectFactory(Peer.Engine, 0, 1, 5000, &Factory, &Client), "benchmark matched fixed member");
        xfuture* Ready = xredisClientReady(Client);
        redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK &&
            xredisPoolCreate(Peer.Engine, 0, &Client, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "benchmark matched fixed pool");
        xrtFutureDestroy(Ready); xredisClientRelease(Client);
    } else redisPeerRequire(xredisPoolConnect(Peer.Engine, 0, 1, NULL, &Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "benchmark automatic pool");
    pcBenchReturn(pcBenchAcquire(Pool), false); xbytesview Ping = XRT_BYTES_LITERAL("PING");
    double CPU = pcBenchCPU(), Started = xrtTimer();
    for (size_t Index = 0; Index < Count; ++Index) {
        double Began = xrtTimer(); xfuture* Acquired = pcBenchAcquire(Pool); xfuture* Call = NULL;
        xredislease* Lease = ((const xredisleaseresult*)xrtFutureValue(Acquired))->Lease;
        redisPeerRequire(xredisLeaseCall(Lease, &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredisrequestresult*)xrtFutureValue(Call))->Status == XREDIS_REQUEST_OK, "benchmark native PING");
        xrtFutureDestroy(Call); pcBenchReturn(Acquired, Repair);
        if (Repair) pcBenchReturn(pcBenchAcquire(Pool), false); /* Includes retry, TCP/setup/readiness and safe reuse. */
        Samples[Index] = (xrtTimer() - Began) * 1000000.0;
    }
    double Seconds = xrtTimer() - Started; CPU = pcBenchCPU() - CPU; qsort(Samples, Count, sizeof(double), pcBenchCompare);
    if (!Fixed) {
        xredispoolconnectinfo Info = {0}; Info.Size = sizeof(Info); xredisPoolConnectInfo(Pool, &Info);
        redisPeerRequire(Info.Connected == (Repair ? Count + 1u : 1u), "benchmark generation count");
    }
    printf("cycles_per_sec: %.9f\ncycle_p50_us: %.9f\ncycle_p99_us: %.9f\ncpu_us_per_cycle: %.9f\ncompleted: %zu\n", (double)Count / Seconds,
        Samples[(Count * 50u + 99u) / 100u - 1u], Samples[(Count * 99u + 99u) / 100u - 1u], CPU / (double)Count, Count);
    puts(Repair ? "workload: automatic pool; acquire/PING/Abort/physical close/1ms retry/TCP HELLO/reacquire/Return; two-worker local C peer; startup excluded" :
        Fixed ? "workload: matched fixed pool with connector compiled; warm acquire/PING/physical Return/observer release; identical peer, input and workers; startup excluded" :
        "workload: automatic pool; warm acquire/PING/physical Return/observer release; two-worker local C peer; startup excluded");
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "benchmark pool close");
    if (Fixed) redisConnectFactoryDrop(&Factory);
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisConnectPeerStop(&Peer); xrtFree(Samples); return 0;
}
