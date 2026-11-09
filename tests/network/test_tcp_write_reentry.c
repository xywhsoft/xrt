#include "../test.h"

#ifndef TEST_TCP_WRITE_REENTRY_BACKEND
#define TEST_TCP_WRITE_REENTRY_BACKEND XNET_PORT_SELECT
#endif

/* One native vector write can retire A and B together. The first releaser
 * appends C while B remains in WriteBuffer. Driving there used to resend B and
 * underflow byte credit. Check actual peer bytes as well as local retirement. */
typedef struct writeReentry writeReentry;
typedef struct writeSlot {
    writeReentry* Test;
    unsigned Index;
} writeSlot;

enum {
    WR_COPY, WR_REFS, WR_SINGLE, WR_CLOSE, WR_ABORT, WR_HIGH, WR_LOW, WR_CASES
};

struct writeReentry {
    xnetengine* Engine;
    xnetlistener* Listener;
    xnetstream* Client; /* Worker-only; the main thread holds a separate handle. */
    xatomicptr Server;
    xatomic32 Received, Drops, Closed, ListenerClosed, Drains, Low, High, Added;
    unsigned Mode;
    writeSlot Slots[4];
    unsigned char Payload[64], Bytes[128];
};

static void wrWait(xatomic32* Value, uint32 Expected, cstr Message)
{
    double Deadline = xrtTimer() + 5;
    while (xrtAtomic32Load(Value, XMEMORY_ACQUIRE) < Expected) {
        testRequire(xrtTimer() < Deadline, Message);
        xrtSleep(1);
    }
}

static void wrRelease(ptr Data, cbytes Bytes, size_t Size)
{
    writeSlot* Slot = Data;
    writeReentry* T = Slot->Test;
    testRequire(Size == 16 && Bytes == T->Payload + Slot->Index * 16,
        "exact buffer release");
    if (!Slot->Index && T->Mode < WR_HIGH) {
        if (T->Mode == WR_CLOSE)
            testRequire(xrtNetStreamClose(T->Client), "close from releaser");
        if (T->Mode == WR_ABORT)
            testRequire(xrtNetStreamAbort(T->Client), "abort from releaser");
        if (T->Mode == WR_REFS) {
            xnetref Refs[] = {
                {T->Payload + 32, 16, wrRelease, T->Slots + 2},
                {T->Payload + 48, 16, wrRelease, T->Slots + 3}
            };
            testRequire(xrtNetStreamSendRefs(T->Client, Refs, 2) == XNET_RESULT_OK,
                "batch append from releaser");
        } else {
            xnetresult Result = xrtNetStreamSend(T->Client, T->Payload + 32, 16);
            testRequire(Result == (T->Mode >= WR_CLOSE ? XNET_RESULT_CLOSED : XNET_RESULT_OK),
                "copy append from releaser");
        }
        xrtAtomic32Store(&T->Added, 1, XMEMORY_RELEASE);
    }
    xrtAtomic32FetchAdd(&T->Drops, 1, XMEMORY_RELEASE);
}

static void wrWaterAppend(xnetstream* Stream, writeReentry* T)
{
    if (!xrtAtomic32Exchange(&T->Added, 1, XMEMORY_ACQ_REL))
        testRequire(xrtNetStreamSend(Stream, T->Payload + 32, 16) == XNET_RESULT_OK,
            "append from water callback");
}

static void wrHigh(xnetstream* Stream, size_t Size, ptr Data)
{
    (void)Size;
    writeReentry* T = Data;
    xrtAtomic32FetchAdd(&T->High, 1, XMEMORY_RELEASE);
    if (T->Mode == WR_HIGH) wrWaterAppend(Stream, T);
}

static void wrLow(xnetstream* Stream, size_t Size, ptr Data)
{
    (void)Size;
    writeReentry* T = Data;
    xrtAtomic32FetchAdd(&T->Low, 1, XMEMORY_RELEASE);
    if (T->Mode == WR_LOW) wrWaterAppend(Stream, T);
}

static void wrDrain(xnetstream* Stream, ptr Data)
{
    (void)Stream;
    xrtAtomic32FetchAdd(&((writeReentry*)Data)->Drains, 1, XMEMORY_RELEASE);
}

static void wrOpen(xnetstream* Stream, ptr Data)
{
    writeReentry* T = Data;
    T->Client = Stream;
    if (T->Mode == WR_SINGLE) {
        testRequire(xrtNetStreamSendRef(Stream, T->Payload, 16, wrRelease,
            T->Slots) == XNET_RESULT_OK, "initial single reference");
    } else {
        xnetref Refs[] = {
            {T->Payload, 16, wrRelease, T->Slots},
            {T->Payload + 16, 16, wrRelease, T->Slots + 1}
        };
        testRequire(xrtNetStreamSendRefs(Stream, Refs, 2) == XNET_RESULT_OK,
            "initial reference batch");
    }
}

static void wrRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
    (void)Stream;
    writeReentry* T = Data;
    size_t Have = xrtAtomic32Load(&T->Received, XMEMORY_RELAXED);
    size_t Size = xrtNetBufSize(Buffer);
    testRequire(Size <= sizeof(T->Bytes) - Have, "wire bound");
    Size = xrtNetBufRead(Buffer, T->Bytes + Have, Size);
    xrtAtomic32Store(&T->Received, (uint32)(Have + Size), XMEMORY_RELEASE);
}

static void wrEnd(xnetstream* Stream, ptr Data)
{
    (void)Data;
    testRequire(xrtNetStreamAbort(Stream), "peer EOF abort");
}

static void wrClosed(xnetstream* Stream, xnetresult Result, const xerror* Cause, ptr Data)
{
    (void)Stream; (void)Result; (void)Cause;
    xrtAtomic32FetchAdd(&((writeReentry*)Data)->Closed, 1, XMEMORY_RELEASE);
}

static bool wrAccept(xnetlistener* Listener, xnetstream* Stream, ptr Data)
{
    (void)Listener;
    writeReentry* T = Data;
    static const xnetstreamevents Events = {NULL, wrRead, wrEnd, NULL, NULL, NULL, wrClosed};
    if (!xrtNetStreamSetEvents(Stream, &Events, T)) return false;
    xrtAtomicPtrStore(&T->Server, Stream, XMEMORY_RELEASE);
    return true;
}

static void wrListenerClosed(xnetlistener* Listener, ptr Data)
{
    (void)Listener;
    xrtAtomic32Store(&((writeReentry*)Data)->ListenerClosed, 1, XMEMORY_RELEASE);
}

static void wrRun(unsigned Mode, xnetportbackend Backend)
{
    writeReentry T = {0};
    T.Mode = Mode;
    xrtAtomicPtrInit(&T.Server, NULL);
    for (unsigned I = 0; I < 4; ++I) {
        T.Slots[I] = (writeSlot){&T, I};
        memset(T.Payload + 16 * I, 'A' + I, 16);
    }
    xnetengineconfig EngineConfig;
    xrtNetEngineConfigInit(&EngineConfig);
    EngineConfig.Workers = 1;
    EngineConfig.Backend = Backend;
    T.Engine = xrtNetEngineCreate(&EngineConfig);
    testRequire(T.Engine && xrtNetEngineStart(T.Engine), "reentry engine");

    xnetlistenconfig ListenConfig;
    xrtNetListenConfigInit(&ListenConfig);
    xrtNetAddrLoopback(&ListenConfig.Address, XNET_FAMILY_IPV4, 0);
    ListenConfig.Distribution = XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents ListenerEvents = {wrAccept, NULL, wrListenerClosed};
    T.Listener = xrtNetListen(T.Engine, &ListenConfig, &ListenerEvents, NULL, &T);
    xnetaddr Address;
    testRequire(T.Listener && xrtNetListenerLocal(T.Listener, &Address), "reentry listener");

    xnetstreamconfig StreamConfig;
    xrtNetStreamConfigInit(&StreamConfig);
    StreamConfig.WriteHighWater = 16;
    StreamConfig.WriteLowWater = 0;
    static const xnetstreamevents ClientEvents = {
        wrOpen, NULL, wrEnd, wrHigh, wrLow, wrDrain, wrClosed
    };
    xnetstream* Client = xrtNetStreamConnect(T.Engine, &Address, 0,
        &StreamConfig, &ClientEvents, &T);
    testRequire(Client != NULL, "reentry client");
    unsigned Drops = Mode == WR_REFS ? 4 : Mode == WR_SINGLE ? 1 : 2;
    unsigned Bytes = Mode == WR_REFS ? 64 : Mode == WR_SINGLE ? 32 : Mode == WR_CLOSE ? 32 : 48;
    wrWait(&T.Drops, Drops, "reference release timeout");
    if (Mode != WR_ABORT) {
        wrWait(&T.Received, Bytes, "peer receive timeout");
        if (Mode != WR_CLOSE) wrWait(&T.Drains, 1, "drain timeout");
    }
    testRequire(xrtNetStreamClose(Client), "idempotent final close");
    wrWait(&T.Closed, 2, "stream close timeout");
    testRequire(xrtNetStreamWaitRetired(Client, 5000) == XWAIT_OK, "physical client retirement");
    xnetstream* Server = xrtAtomicPtrLoad(&T.Server, XMEMORY_ACQUIRE);
    testRequire(Server && xrtNetStreamWaitRetired(Server, 5000) == XWAIT_OK,
        "physical peer retirement");
    testRequire(!xrtNetStreamPending(Client), "byte credit returns to zero");
    testRequire(xrtAtomic32Load(&T.Drops, XMEMORY_ACQUIRE) == Drops,
        "each native reference released exactly once");
    if (Mode != WR_ABORT) {
        testRequire(xrtAtomic32Load(&T.Received, XMEMORY_ACQUIRE) == Bytes,
            "final wire length has no duplicated or missing bytes");
        testRequire(!memcmp(T.Bytes, T.Payload, 16), "first frame exact");
        if (Mode == WR_SINGLE) {
            testRequire(!memcmp(T.Bytes + 16, T.Payload + 32, 16), "single-reference tail exact");
        } else {
            testRequire(!memcmp(T.Bytes + 16, T.Payload + 16, 16), "second frame exact");
            if (Bytes > 32)
                testRequire(!memcmp(T.Bytes + 32, T.Payload + 32, Bytes - 32), "reentrant tail exact");
        }
    }
    if (Mode == WR_HIGH)
        testRequire(xrtAtomic32Load(&T.High, XMEMORY_ACQUIRE) > 0, "high-water append executed");
    if (Mode == WR_LOW)
        testRequire(xrtAtomic32Load(&T.Low, XMEMORY_ACQUIRE) > 0, "low-water append executed");

    testRequire(xrtNetListenerClose(T.Listener), "listener close");
    wrWait(&T.ListenerClosed, 1, "listener close timeout");
    xrtNetStreamDestroy(Client);
    xrtNetStreamDestroy(Server);
    xrtNetListenerDestroy(T.Listener);
    double Deadline = xrtTimer() + 5;
    while (!xrtNetEngineStop(T.Engine)) {
        testRequire(xrtTimer() < Deadline, "listener runtime retirement");
        xrtClearError();
        xrtThreadYield();
    }
    testRequire(xrtNetEngineDestroy(T.Engine), "engine retirement");
}

int main(void)
{
    for (unsigned I = 0; I < WR_CASES; ++I) wrRun(I, TEST_TCP_WRITE_REENTRY_BACKEND);
#ifndef TEST_TCP_WRITE_REENTRY_NATIVE
    for (unsigned I = 0; I < WR_CASES; ++I) wrRun(I, XNET_PORT_AUTO);
#endif
    puts("[PASS] TCP reentrant release/water sends preserve exact FIFO bytes, credits and physical retirement");
    return 0;
}
