/* Private integration artifact, never a release DLL. */
#define XRT_MODULE_CORE
#define XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_RANDOM_DEFAULT
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD_KEY
#define XRT_MODULE_COROUTINE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

__declspec(dllexport) int touch(void)
{
    void* memory = xrtMalloc(37);
    xerror* error;
    xthreadkey* key;
    if (!memory || !xrtTemp(128)) return 0;
    xrtFree(memory);
    (void)xrtRand64();
    if (!xrtMemDebugFailAfter(100000)) return 0;
    xrtMemDebugFailClear();
    error = xrtErrorCreate(XERR_VALUE, "retire.test", 1, "owned FLS error");
    if (!error) return 0;
    xrtSetErrorTake(error);
    /* Trigger borrowed-slot initialization without leaving user-owned state. */
    if (!__xrtFutureNotifyKeyEnsure() || !__xrtCoTlsEnsure()) return 0;
    key = xrtThreadKeyCreate(NULL);
    if (!key || !xrtThreadKeySet(key, (void*)1) ||
        !xrtThreadKeysClear() || !xrtThreadKeyDestroy(key)) return 0;
    return 1;
}
__declspec(dllexport) int retire(void)
{
    return xrtRuntimeRetireThreadStorage();
}
