/* White-box OS failure injection; production has no test hook or bypass. */
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#ifndef FLS_OUT_OF_INDEXES
#define FLS_OUT_OF_INDEXES ((DWORD)0xffffffffu)
DWORD WINAPI FlsAlloc(VOID (WINAPI *)(PVOID));
BOOL WINAPI FlsFree(DWORD);
PVOID WINAPI FlsGetValue(DWORD);
BOOL WINAPI FlsSetValue(DWORD, PVOID);
#endif
static int failAlloc, failFree, frees, drops, reentered;
static DWORD WINAPI probeAlloc(VOID (WINAPI *callback)(PVOID))
{
    if (failAlloc) { failAlloc = 0; return FLS_OUT_OF_INDEXES; }
    return FlsAlloc(callback);
}
static BOOL WINAPI probeFree(DWORD index)
{
    ++frees;
    if (failFree && --failFree == 0) return FALSE;
    return FlsFree(index);
}
static BOOL WINAPI probeTlsFree(DWORD index)
{
    ++frees;
    if (failFree && --failFree == 0) return FALSE;
    return TlsFree(index);
}
#define FlsAlloc probeAlloc
#define FlsFree probeFree
#define TlsFree probeTlsFree
#endif
#define XRT_MODULE_CORE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
static xrt_local_slot payload, errorSlot, heapSlot, guard, borrowed, rejected;
static DWORD WINAPI concurrentRetire(void* unused)
{
    (void)unused;
    return xrtRuntimeRetireThreadStorage() ? 1 : 0;
}
static void WINAPI dropPayload(void* value)
{
    HANDLE thread;
    DWORD result;
    testRequire(value == (void*)1 && drops++ == 0, "payload first");
    testRequire(FlsGetValue(guard.Index) == (void*)4, "guard remains live");
    testRequire(!xrtRuntimeRetireThreadStorage(), "reentrant retire rejected");
    thread = CreateThread(NULL, 0, concurrentRetire, NULL, 0, NULL);
    testRequire(thread && WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0 &&
        GetExitCodeThread(thread, &result) && result == 0, "concurrent retire rejected without deadlock");
    CloseHandle(thread);
    testRequire(__xrtLocalSlotAlloc(&rejected, NULL, XRT_LOCAL_PAYLOAD, true)
        == FLS_OUT_OF_INDEXES, "new registration rejected while retiring");
    reentered++;
}
static void WINAPI dropError(void* value)
{
    testRequire(value == (void*)2 && drops++ == 1, "error after payload");
}
static void WINAPI dropHeap(void* value)
{
    testRequire(value == (void*)3 && drops++ == 2, "heap after error");
}
#endif
int main(int argc, char** argv)
{
#if defined(_WIN32) || defined(_WIN64)
    int before, fault = argc == 2 ? atoi(argv[1]) : 2;
    failAlloc = 1;
    testRequire(__xrtLocalSlotAlloc(&rejected, NULL, XRT_LOCAL_PAYLOAD, true)
        == FLS_OUT_OF_INDEXES && __xrtLocalSlots == NULL, "allocation rollback");
    testRequire(__xrtLocalSlotAlloc(&guard, NULL, XRT_LOCAL_BORROWED, true)
        != FLS_OUT_OF_INDEXES, "guard alloc");
    testRequire(__xrtLocalSlotAlloc(&payload, dropPayload, XRT_LOCAL_PAYLOAD, true)
        != FLS_OUT_OF_INDEXES, "payload alloc");
    testRequire(__xrtLocalSlotAlloc(&heapSlot, dropHeap, XRT_LOCAL_HEAP, true)
        != FLS_OUT_OF_INDEXES, "heap alloc");
    testRequire(__xrtLocalSlotAlloc(&errorSlot, dropError, XRT_LOCAL_ERROR, true)
        != FLS_OUT_OF_INDEXES, "error alloc");
    testRequire(__xrtLocalSlotAlloc(&borrowed, NULL, XRT_LOCAL_BORROWED, false)
        != TLS_OUT_OF_INDEXES, "borrowed TLS alloc");
    testRequire(FlsSetValue(payload.Index, (void*)1) && FlsSetValue(errorSlot.Index, (void*)2)
        && FlsSetValue(heapSlot.Index, (void*)3) && FlsSetValue(guard.Index, (void*)4), "set values");
    testRequire(fault >= 1 && fault <= 5, "fault position");
    failFree = fault;
    testRequire(!xrtRuntimeRetireThreadStorage() && drops == (fault <= 3 ? fault - 1 : 3),
        "partial free failure keeps remaining callbacks resident");
    testRequire(xrtRuntimeRetireThreadStorage(), "retry completes");
    testRequire(drops == 3 && reentered == 1 && __xrtLocalSlots == NULL, "ordered exactly once cleanup");
    before = frees;
    testRequire(xrtRuntimeRetireThreadStorage() && frees == before, "idempotent retire");
    testRequire(__xrtLocalSlotAlloc(&rejected, NULL, XRT_LOCAL_PAYLOAD, true)
        == FLS_OUT_OF_INDEXES, "retired instance cannot recreate slots");
#else
    (void)argc; (void)argv;
    testRequire(!xrtRuntimeRetireThreadStorage(), "unsupported platform is explicit");
#endif
    puts("runtime thread storage retirement passed");
    return 0;
}
