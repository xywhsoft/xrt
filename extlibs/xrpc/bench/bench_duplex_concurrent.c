#include "../examples/duplex/app_dispatch.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct rpcbench {
    xrpcduplex* Client;
    xevent *Start, *Wake;
    xatomic32 Failed;
} rpcbench;

typedef struct rpcproducer {
    rpcbench* Bench;
    double* Samples;
    size_t Calls;
} rpcproducer;

static void wakeOwner(ptr Data)
{
    (void)xrtEventSet(Data);
}

static bool echoed(xfuture* Future)
{
    if (xrtFutureWaitFor(Future, 6000) != XWAIT_OK) return false;
    const xrpccallresult* Result = xrtFutureValue(Future);
    xrpcresponseinfo Info = {.Size = sizeof(Info)};
    if (!Result || Result->Status != XRPC_CALL_OK || Result->Delivery != XRPC_REPLIED ||
        !xrpcResponsesInfo(Result->Responses, &Info) || Info.Count != 1 || Result->Index != 0)
        return false;
    xstrview Value = Info.Messages[0].Result.Raw;
    return Value.Size == 4 && !memcmp(Value.Data, "[42]", 4);
}

static int32 produce(ptr Data)
{
    rpcproducer* Producer = Data;
    rpcbench* Bench = Producer->Bench;
    if (xrtEventWait(Bench->Start) != XWAIT_OK) return 1;
    for (size_t I = 0; I < Producer->Calls; ++I) {
        if (xrtAtomic32Load(&Bench->Failed, XMEMORY_ACQUIRE)) return 1;
        double Began = xrtTimer();
        xfuture* Future = NULL;
        bool OK = xrpcDuplexCall(Bench->Client, XRT_STR_LITERAL("echo"), XRT_STR_LITERAL("[42]"),
            5000, &Future) == XRPC_DUPLEX_ACCEPTED && echoed(Future);
        Producer->Samples[I] = (xrtTimer() - Began) * 1e6;
        if (Future) xrtFutureDestroy(Future);
        if (!OK) {
            xrtAtomic32Store(&Bench->Failed, 1, XMEMORY_RELEASE);
            wakeOwner(Bench->Wake);
            return 1;
        }
    }
    return 0;
}

static int transfer(xrpcduplex* From, xrpcduplex* To)
{
    xrpcduplexoutput* Output = NULL;
    uint64 Token;
    int Pending = xrpcDuplexNext(From, &Output, &Token);
    if (Pending != 1) return Pending;
    bool OK = xrpcDuplexStarted(From, Token) && xrpcDuplexSent(From, Token) &&
        xrpcDuplexReceive(To, xrpcDuplexOutputWire(Output), (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED;
    xrpcDuplexOutputRelease(Output);
    return OK ? 1 : -1;
}

static bool drive(rpcbench* Bench, xrpcduplex* Server, apprequest* Job, size_t Count)
{
    for (size_t Done = 0; Done < Count;) {
        if (xrtAtomic32Load(&Bench->Failed, XMEMORY_ACQUIRE) || !xrpcDuplexStep(Bench->Client))
            return false;
        int Progress = transfer(Bench->Client, Server);
        if (Progress < 0) return false;
        if (Progress == 0) {
            if (xrtEventWaitFor(Bench->Wake, 6000) != XWAIT_OK) return false;
            continue;
        }
        if (!appFinish(Server, Job) || Job->Token || transfer(Server, Bench->Client) != 1)
            return false;
        ++Done;
    }
    return true;
}

static int byLatency(const void* A, const void* B)
{
    double L = *(const double*)A, R = *(const double*)B;
    return (L > R) - (L < R);
}

int main(int Argc, char** Argv)
{
    enum { MAX_PRODUCERS = 8 };
    size_t Producers = Argc > 1 ? strtoul(Argv[1], NULL, 10) : 4;
    size_t Calls = Argc > 2 ? strtoul(Argv[2], NULL, 10) : 20000;
    if (!Producers || Producers > MAX_PRODUCERS || !Calls || Calls > 100000 || Argc > 3)
        return 1;
    size_t Count = Producers * Calls;
    double* Samples = malloc(Count * sizeof(*Samples));
    rpcbench Bench = {.Start = xrtEventCreate(true, false), .Wake = xrtEventCreate(false, false)};
    xrtAtomic32Init(&Bench.Failed, 0);
    xrpcduplex* Server = NULL;
    apprequest Job = {0};
    xrpcduplexconfig Config;
    if (!Samples || !Bench.Start || !Bench.Wake || !xrpcDuplexConfigInit(&Config)) return 2;
    Config.Wake = wakeOwner;
    Config.User = Bench.Wake;
    if (!xrpcDuplexCreate(&Config, &Bench.Client)) return 2;
    Config.Wake = NULL;
    Config.User = &Job;
    Config.OnRequest = appTakeRequest;
    if (!xrpcDuplexCreate(&Config, &Server)) return 2;

    rpcproducer Work[MAX_PRODUCERS];
    xthread* Threads[MAX_PRODUCERS];
    size_t Started = 0;
    for (; Started < Producers; ++Started) {
        Work[Started] = (rpcproducer){&Bench, Samples + Started * Calls, Calls};
        Threads[Started] = xrtThreadCreate(produce, &Work[Started], 0);
        if (!Threads[Started]) break;
    }
    if (Started < Producers) xrtAtomic32Store(&Bench.Failed, 1, XMEMORY_RELEASE);

    double Began = xrtTimer();
    bool OK = xrtEventSet(Bench.Start) && Started == Producers && drive(&Bench, Server, &Job, Count);
    if (!OK && (!xrpcDuplexClose(Bench.Client, XRPC_CALL_CLOSED) || !xrpcDuplexStep(Bench.Client)))
        return 3;
    for (size_t I = 0; I < Started; ++I) {
        if (xrtThreadWaitFor(Threads[I], 8000) != XWAIT_OK) return 3;
        if (xrtThreadExitCode(Threads[I])) OK = false;
        xrtThreadDestroy(Threads[I]);
    }
    double Seconds = xrtTimer() - Began;
    if (!xrpcDuplexClose(Bench.Client, XRPC_CALL_CLOSED) || !xrpcDuplexStep(Bench.Client) ||
        !xrpcDuplexDestroy(Bench.Client) || !xrpcDuplexClose(Server, XRPC_CALL_CLOSED) ||
        !xrpcDuplexStep(Server) || !xrpcDuplexDestroy(Server) ||
        !xrtEventDestroy(Bench.Wake) || !xrtEventDestroy(Bench.Start)) OK = false;
    if (!OK || Seconds <= 0) {
        free(Samples);
        return 4;
    }

    qsort(Samples, Count, sizeof(*Samples), byLatency);
    printf("calls_per_second: %.3f\n", Count / Seconds);
    printf("latency_p50_us: %.3f\n", Samples[(Count * 50 + 99) / 100 - 1]);
    printf("latency_p95_us: %.3f\n", Samples[(Count * 95 + 99) / 100 - 1]);
    printf("latency_p99_us: %.3f\n", Samples[(Count * 99 + 99) / 100 - 1]);
    printf("latency_p999_us: %.3f\n", Samples[(Count * 999 + 999) / 1000 - 1]);
    printf("calls: %zu\n", Count);
    free(Samples);
    return 0;
}
