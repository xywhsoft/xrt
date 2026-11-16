#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdbool.h>
static bool FailNativeSet;
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
static BOOL WINAPI testSwapNativeSet(DWORD Key,PVOID Value)
{if(FailNativeSet)return FALSE;return FlsSetValue(Key,Value);}
#define FlsSetValue testSwapNativeSet
#else
#include <pthread.h>
#include <errno.h>
static int testSwapNativeSet(pthread_key_t Key,const void* Value)
{if(FailNativeSet)return ENOMEM;return pthread_setspecific(Key,Value);}
#define pthread_setspecific testSwapNativeSet
#endif
#define XRT_MODULE_CORE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#if defined(_WIN32) || defined(_WIN64)
#undef FlsSetValue
#else
#undef pthread_setspecific
#endif
#include "../test.h"
int main(void)
{
    xerror* A=xrtErrorCreate(XERR_STATE,"swap.native",1,"existing slot owner");xerror* B=xrtErrorCreate(XERR_STATE,"swap.native",2,"incoming caller owner");testRequire(A&&B,"native failure owners");xerror* Old=NULL;
    testRequire(xrtTrySwapErrorTake(A,&Old)&&!Old,"native failure baseline slot");Old=(xerror*)(uintptr_t)1;FailNativeSet=true;bool OK=xrtTrySwapErrorTake(B,&Old);FailNativeSet=false;
    testRequire(!OK&&Old==(xerror*)(uintptr_t)1&&xrtGetError()==A&&xrtErrorCode(B)==2,"native TLS write failure preserves slot, output and incoming owner");
    FailNativeSet=true;OK=xrtTrySwapErrorTake(NULL,&Old);FailNativeSet=false;testRequire(!OK&&Old==(xerror*)(uintptr_t)1&&xrtGetError()==A,"failed take does not lose the existing slot reference");
    testRequire(xrtTrySwapErrorTake(B,&Old)&&Old==A,"retry after native failure");xrtErrorFree(Old);testRequire(xrtTrySwapErrorTake(NULL,&Old)&&Old==B,"final owning take");xrtErrorFree(Old);puts("[PASS] native TLS setter failure retains every error owner and output");return 0;
}
