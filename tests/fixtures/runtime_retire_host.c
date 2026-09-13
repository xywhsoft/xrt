/* Only native synchronization and scalar ABI; no second XRT instance. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int (*Entry)(void);
typedef struct Worker { Entry Touch; HANDLE Ready, Done; int Ok; } Worker;
typedef struct Fiber { Entry Touch; void* Main; int Ok; } Fiber;
static DWORD WINAPI worker(void* data)
{
    Worker* state = (Worker*)data;
    state->Ok = state->Touch();
    SetEvent(state->Ready);
    WaitForSingleObject(state->Done, INFINITE);
    return 0; /* Exit only after DLL code has been unmapped. */
}
static void WINAPI fiber(void* data)
{
    Fiber* state = (Fiber*)data;
    state->Ok = state->Touch();
    SwitchToFiber(state->Main);
}
int main(int argc, char** argv)
{
    int iteration;
    void* mainFiber;
    if (argc != 2) return 2;
    mainFiber = ConvertThreadToFiber(NULL);
    if (!mainFiber) return 3;
    for (iteration = 0; iteration < 96; ++iteration) {
        HMODULE dll = LoadLibraryA(argv[1]);
        FARPROC symbol;
        Entry touch, retire;
        Worker workers[4]; HANDLE threads[4];
        Fiber fibers[3]; void* fiberHandles[3];
        MEMORY_BASIC_INFORMATION region;
        int i;
        if (!dll) return 4;
        symbol = GetProcAddress(dll, "touch"); memcpy(&touch, &symbol, sizeof(touch));
        symbol = GetProcAddress(dll, "retire"); memcpy(&retire, &symbol, sizeof(retire));
        if (!touch || !retire || !touch()) return 5;
        for (i = 0; i < 4; ++i) {
            workers[i].Touch = touch; workers[i].Ok = 0;
            workers[i].Ready = CreateEventA(NULL, TRUE, FALSE, NULL);
            workers[i].Done = CreateEventA(NULL, TRUE, FALSE, NULL);
            if (!workers[i].Ready || !workers[i].Done) return 6;
            threads[i] = CreateThread(NULL, 0, worker, &workers[i], 0, NULL);
            if (!threads[i] || WaitForSingleObject(workers[i].Ready, 10000) != WAIT_OBJECT_0 || !workers[i].Ok) return 7;
        }
        for (i = 0; i < 3; ++i) {
            fibers[i].Touch = touch; fibers[i].Main = mainFiber; fibers[i].Ok = 0;
            fiberHandles[i] = CreateFiber(0, fiber, &fibers[i]);
            if (!fiberHandles[i]) return 8;
            SwitchToFiber(fiberHandles[i]);
            if (!fibers[i].Ok) return 9;
        }
        if (!retire() || !retire() || !FreeLibrary(dll)) return 10;
        if (!VirtualQuery((const void*)(uintptr_t)touch, &region, sizeof(region)) || region.State != MEM_FREE) return 11;
        for (i = 0; i < 3; ++i) DeleteFiber(fiberHandles[i]);
        for (i = 0; i < 4; ++i) {
            SetEvent(workers[i].Done);
            if (WaitForSingleObject(threads[i], 10000) != WAIT_OBJECT_0) return 12;
            CloseHandle(threads[i]); CloseHandle(workers[i].Ready); CloseHandle(workers[i].Done);
        }
    }
    if (!ConvertFiberToThread()) return 13;
    puts("retire unload passed: 96 loads, 384 parked thread exits, 288 parked fiber deletions, MEM_FREE confirmed");
    return 0;
}
