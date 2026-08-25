#include "../internal/xrt_coroutine.h"

#include <errno.h>



#if defined(XRT_FEATURE_COROUTINE)

#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))

#define XRT_CO_FLOAT_CONTROL_MASK 0x030F031Fu

unsigned int _controlfp(unsigned int iNew, unsigned int iMask);



/* TinyCC Windows 通过 CRT 控制字捕获浮点执行环境。 */
static bool __xrtCoFloatCapture(xrt_co_float_environment* pEnvironment)
{
	*pEnvironment = (uint32)_controlfp(0, 0);
	return true;
}



/* TinyCC Windows 通过 CRT 控制字恢复浮点执行环境。 */
static bool __xrtCoFloatRestore(
	const xrt_co_float_environment* pEnvironment
)
{
	(void)_controlfp(*pEnvironment, XRT_CO_FLOAT_CONTROL_MASK);
	return true;
}

#else

/* 常规 CRT 捕获完整浮点执行环境。 */
static bool __xrtCoFloatCapture(xrt_co_float_environment* pEnvironment)
{
	return fegetenv(pEnvironment) == 0;
}



/* 常规 CRT 恢复完整浮点执行环境。 */
static bool __xrtCoFloatRestore(
	const xrt_co_float_environment* pEnvironment
)
{
	return fesetenv(pEnvironment) == 0;
}

#endif

/* 将平台错误写入协程错误域。 */
static void __xrtCoSetSystemError(cstr sOperation, int iCode, cstr sMessage)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = __xrtSystemErrorKind(iCode);
	tDesc.Code = 1;
	tDesc.SystemCode = iCode;
	tDesc.Domain = "xrt.coroutine";
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



#if defined(_WIN32) || defined(_WIN64)

static DWORD __xrtCoTlsIndex = TLS_OUT_OF_INDEXES;
static volatile LONG __xrtCoTlsState;
static DWORD __xrtCoTlsError = ERROR_NOT_ENOUGH_MEMORY;



/* 创建原生线程共享的 TLS 槽，不能使用会随 Fiber 切换的 FLS。 */
static bool __xrtCoTlsEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtCoTlsState, 1, 0);

	if ( iState == 0 ) {
		__xrtCoTlsIndex = TlsAlloc();
		if ( __xrtCoTlsIndex == TLS_OUT_OF_INDEXES ) {
			__xrtCoTlsError = GetLastError();
		}
		InterlockedExchange(
			&__xrtCoTlsState,
			__xrtCoTlsIndex != TLS_OUT_OF_INDEXES ? 2 : 3
		);
		if ( __xrtCoTlsIndex == TLS_OUT_OF_INDEXES ) {
			__xrtCoSetSystemError(
				"runtime",
				(int)__xrtCoTlsError,
				"coroutine runtime TLS allocation failed"
			);
			return false;
		}
		return true;
	}
	while ( (iState = InterlockedCompareExchange(&__xrtCoTlsState, 0, 0)) == 1 ) {
		Sleep(0);
	}
	if ( iState != 2 ) {
		__xrtCoSetSystemError(
			"runtime",
			(int)__xrtCoTlsError,
			"coroutine runtime TLS allocation failed"
		);
		return false;
	}
	return true;
}



/* 获取或创建当前原生线程的协程运行时。 */
xrt_co_runtime* __xrtCoRuntimeGet(bool bCreate)
{
	xrt_co_runtime* pRuntime;

	if ( (__xrtCoTlsState != 2) && !bCreate ) {
		return NULL;
	}
	if ( !__xrtCoTlsEnsure() ) {
		return NULL;
	}
	pRuntime = (xrt_co_runtime*)TlsGetValue(__xrtCoTlsIndex);
	if ( (pRuntime == NULL) && bCreate ) {
		pRuntime = (xrt_co_runtime*)xrtCalloc(1, sizeof(xrt_co_runtime));
		if ( pRuntime == NULL ) {
			return NULL;
		}
		pRuntime->OwnerThreadId = xrtThreadCurrentId();
		if ( !TlsSetValue(__xrtCoTlsIndex, pRuntime) ) {
			int iCode = (int)GetLastError();

			xrtFree(pRuntime);
			__xrtCoSetSystemError(
				"runtime",
				iCode,
				"coroutine runtime TLS update failed"
			);
			return NULL;
		}
	}
	return pRuntime;
}



/* 确保本次恢复路径拥有可返回的宿主 Fiber。 */
static bool __xrtCoPrepareMainFiber(xrt_co_runtime* pRuntime)
{
	if ( pRuntime->MainFiber == NULL ) {
		pRuntime->MainFiber = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
		if ( pRuntime->MainFiber != NULL ) {
			pRuntime->Converted = true;
		} else if ( GetLastError() == ERROR_ALREADY_FIBER ) {
			pRuntime->MainFiber = GetCurrentFiber();
		} else {
			__xrtCoSetSystemError(
				"attach",
				(int)GetLastError(),
				"current thread could not enter coroutine runtime"
			);
			return false;
		}
	}
	pRuntime->CallerFiber = GetCurrentFiber();
	return true;
}



/* 释放当前 Windows 线程的协程运行时并恢复普通线程形态。 */
bool __xrtCoRuntimeDetach(void)
{
	xrt_co_runtime* pRuntime = __xrtCoRuntimeGet(false);

	if ( pRuntime == NULL ) {
		return true;
	}
	if ( (pRuntime->Current != NULL) || (pRuntime->LiveCount != 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pRuntime->Converted ) {
		if ( GetCurrentFiber() != pRuntime->MainFiber ) {
			__xrtErrorSetInvalidState();
			return false;
		}
		if ( !ConvertFiberToThread() ) {
			__xrtCoSetSystemError(
				"detach",
				(int)GetLastError(),
				"coroutine host fiber could not return to a thread"
			);
			return false;
		}
		pRuntime->Converted = false;
		pRuntime->MainFiber = NULL;
		pRuntime->CallerFiber = NULL;
	}
	if ( !TlsSetValue(__xrtCoTlsIndex, NULL) ) {
		__xrtCoSetSystemError(
			"detach",
			(int)GetLastError(),
			"coroutine runtime TLS clear failed"
		);
		return false;
	}
	xrtFree(pRuntime);
	return true;
}



/* Fiber 入口执行统一协程包装；协程完成后永远不会再次恢复。 */
static VOID CALLBACK __xrtCoFiberEntry(LPVOID pData)
{
	xcoro* pCo = (xcoro*)pData;
	xrt_co_runtime* pRuntime = pCo->Runtime;

	(void)__xrtTempContextSwap(&pCo->Temp);
	if ( (xrtTempCurrent() != &pCo->Temp) ||
		 !__xrtCoFloatRestore(&pCo->FloatEnvironment) ) {
		__xrtCoFinish(pCo, XCORO_TERM_ERROR);
		__xrtCoBackendYield(pRuntime, pCo);
		abort();
	}
	__xrtCoEntry(pCo);
	abort();
}



/* 使用可增长 Fiber 栈创建 Windows 协程。 */
bool __xrtCoBackendCreate(xcoro* pCo, size_t iStackSize)
{
	size_t iCommitSize = 16u * 1024u;

	if ( !__xrtCoFloatCapture(&pCo->FloatEnvironment) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( iCommitSize > iStackSize ) {
		iCommitSize = iStackSize;
	}
	pCo->Fiber = CreateFiberEx(
		iCommitSize,
		iStackSize,
		FIBER_FLAG_FLOAT_SWITCH,
		__xrtCoFiberEntry,
		pCo
	);
	if ( pCo->Fiber == NULL ) {
		__xrtCoSetSystemError(
			"create",
			(int)GetLastError(),
			"coroutine fiber creation failed"
		);
		return false;
	}
	return true;
}



/* 释放 Windows Fiber 及其按需提交的栈。 */
void __xrtCoBackendDestroy(xcoro* pCo)
{
	if ( pCo->Fiber != NULL ) {
		DeleteFiber(pCo->Fiber);
		pCo->Fiber = NULL;
	}
}



/* 从当前宿主 Fiber 恢复协程。 */
bool __xrtCoBackendResume(xrt_co_runtime* pRuntime, xcoro* pCo)
{
	if ( !__xrtCoPrepareMainFiber(pRuntime) ) {
		return false;
	}
	SwitchToFiber(pCo->Fiber);
	return true;
}



/* 从协程返回本次调用它的宿主 Fiber。 */
void __xrtCoBackendYield(xrt_co_runtime* pRuntime, xcoro* pCo)
{
	(void)pCo;
	SwitchToFiber(pRuntime->CallerFiber);
}



#else

#include <pthread.h>
/* -std=c11 严格 ISO 模式下 Darwin 的 mman.h 隐藏 MAP_ANONYMOUS。 */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
	#define _DARWIN_C_SOURCE 1
#endif
#include <sys/mman.h>
#include <unistd.h>

#if defined(XRT_CO_SHADOW_STACK)
	#include <sys/syscall.h>

	#ifndef __NR_arch_prctl
		#define __NR_arch_prctl 158
	#endif
	#ifndef __NR_map_shadow_stack
		#define __NR_map_shadow_stack 453
	#endif
	#ifndef ARCH_SHSTK_STATUS
		#define ARCH_SHSTK_STATUS 0x5005
	#endif
	#ifndef ARCH_SHSTK_SHSTK
		#define ARCH_SHSTK_SHSTK 0x1u
	#endif
	#ifndef SHADOW_STACK_SET_TOKEN
		#define SHADOW_STACK_SET_TOKEN 0x1u
	#endif
#endif



#if defined(XRT_CO_ADDRESS_SANITIZER)
void __sanitizer_start_switch_fiber(
	void** ppFakeStackSave,
	const void* pStackBottom,
	size_t iStackSize
);
void __sanitizer_finish_switch_fiber(
	void* pFakeStackSave,
	const void** ppPreviousStackBottom,
	size_t* pPreviousStackSize
);
#endif

#if defined(XRT_CO_THREAD_SANITIZER)
void* __tsan_get_current_fiber(void);
void* __tsan_create_fiber(unsigned int iFlags);
void __tsan_destroy_fiber(void* pFiber);
void __tsan_switch_to_fiber(void* pFiber, unsigned int iFlags);
#endif

#if defined(XRT_CO_MEMORY_SANITIZER)
void __msan_start_switch_fiber(const void* pStackBottom, size_t iStackSize);
void __msan_finish_switch_fiber(
	const void** ppPreviousStackBottom,
	size_t* pPreviousStackSize
);
#endif



#ifndef MAP_ANONYMOUS
	#ifdef MAP_ANON
		#define MAP_ANONYMOUS MAP_ANON
	#endif
#endif



static pthread_key_t __xrtCoTlsKey;
static pthread_once_t __xrtCoTlsOnce = PTHREAD_ONCE_INIT;
static bool __xrtCoTlsReady;
static int __xrtCoTlsError;



/* 在线程退出时释放不再可达的协程运行时。 */
static void __xrtCoTlsDestroy(void* pData)
{
	xrt_co_runtime* pRuntime = (xrt_co_runtime*)pData;

	xrtFree(pRuntime);
}



/* 创建 POSIX 线程协程运行时键。 */
static void __xrtCoTlsInit(void)
{
	__xrtCoTlsError = pthread_key_create(&__xrtCoTlsKey, __xrtCoTlsDestroy);
	__xrtCoTlsReady = __xrtCoTlsError == 0;
}



/* 获取或创建当前 POSIX 线程的协程运行时。 */
xrt_co_runtime* __xrtCoRuntimeGet(bool bCreate)
{
	xrt_co_runtime* pRuntime;
	int iCode;

	iCode = pthread_once(&__xrtCoTlsOnce, __xrtCoTlsInit);
	if ( iCode != 0 ) {
		if ( bCreate ) {
			__xrtCoSetSystemError(
				"runtime",
				iCode,
				"coroutine runtime TLS initialization failed"
			);
		}
		return NULL;
	}
	if ( !__xrtCoTlsReady ) {
		if ( bCreate ) {
			__xrtCoSetSystemError(
				"runtime",
				__xrtCoTlsError,
				"coroutine runtime TLS allocation failed"
			);
		}
		return NULL;
	}
	pRuntime = (xrt_co_runtime*)pthread_getspecific(__xrtCoTlsKey);
	if ( (pRuntime == NULL) && bCreate ) {
		pRuntime = (xrt_co_runtime*)xrtCalloc(1, sizeof(xrt_co_runtime));
		if ( pRuntime == NULL ) {
			return NULL;
		}
		pRuntime->OwnerThreadId = xrtThreadCurrentId();
		iCode = pthread_setspecific(__xrtCoTlsKey, pRuntime);
		if ( iCode != 0 ) {
			xrtFree(pRuntime);
			__xrtCoSetSystemError(
				"runtime",
				iCode,
				"coroutine runtime TLS update failed"
			);
			return NULL;
		}
	}
	return pRuntime;
}



/* 释放当前 POSIX 线程的协程运行时。 */
bool __xrtCoRuntimeDetach(void)
{
	xrt_co_runtime* pRuntime = __xrtCoRuntimeGet(false);
	int iCode;

	if ( pRuntime == NULL ) {
		return true;
	}
	if ( (pRuntime->Current != NULL) || (pRuntime->LiveCount != 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iCode = pthread_setspecific(__xrtCoTlsKey, NULL);
	if ( iCode != 0 ) {
		__xrtCoSetSystemError(
			"detach",
			iCode,
			"coroutine runtime TLS clear failed"
		);
		return false;
	}
	xrtFree(pRuntime);
	return true;
}



/* 汇编后端入口从保留上下文槽直接接收当前协程。 */
static XRT_CO_NO_ADDRESS_SANITIZER void __xrtCoAsmEntry(xcoro* pCo)
{
	xrt_co_runtime* pRuntime;

	if ( pCo == NULL ) {
		abort();
	}
	pRuntime = pCo->Runtime;
	if ( (pRuntime == NULL) || (pRuntime->Current != pCo) ) {
		abort();
	}
	#if defined(XRT_CO_ADDRESS_SANITIZER)
		__sanitizer_finish_switch_fiber(
			pCo->Context.SanitizerFakeStack,
			&pRuntime->MainContext.SanitizerStackBottom,
			&pRuntime->MainContext.SanitizerStackSize
		);
		pCo->Context.SanitizerFakeStack = NULL;
	#endif
	#if defined(XRT_CO_MEMORY_SANITIZER)
		__msan_finish_switch_fiber(
			&pRuntime->MainContext.SanitizerStackBottom,
			&pRuntime->MainContext.SanitizerStackSize
		);
	#endif
	__xrtCoEntry(pCo);
	abort();
}



/* 返回平台页面大小。 */
static size_t __xrtCoPageSize(void)
{
	long iPageSize = sysconf(_SC_PAGESIZE);

	return iPageSize > 0 ? (size_t)iPageSize : 4096u;
}



#if defined(XRT_CO_SHADOW_STACK)
/* 为启用 CET 的线程创建独立 shadow stack，并生成首个恢复令牌。 */
static bool __xrtCoShadowStackCreate(
	xcoro* pCo,
	size_t iStackSize,
	size_t iPageSize
)
{
	unsigned long iStatus = 0;
	long iMapResult;
	size_t iShadowSize;
	uintptr_t iOldShadow;
	uintptr_t iNewShadow;
	uintptr_t iShadowTop;

	if ( syscall(__NR_arch_prctl, ARCH_SHSTK_STATUS, &iStatus) != 0 ) {
		if ( (errno == EINVAL) || (errno == ENOSYS) ) {
			return true;
		}
		__xrtCoSetSystemError(
			"create",
			errno,
			"coroutine shadow stack status query failed"
		);
		return false;
	}
	if ( (iStatus & ARCH_SHSTK_SHSTK) == 0 ) {
		return true;
	}
	iShadowSize = ((iStackSize + iPageSize - 1u) / iPageSize) * iPageSize;
	iMapResult = syscall(
		__NR_map_shadow_stack,
		0,
		iShadowSize,
		SHADOW_STACK_SET_TOKEN
	);
	if ( iMapResult == -1 ) {
		__xrtCoSetSystemError(
			"create",
			errno,
			"coroutine shadow stack mapping failed"
		);
		return false;
	}
	pCo->ShadowStackMap = (ptr)(uintptr_t)iMapResult;
	pCo->ShadowStackMapSize = iShadowSize;
	iShadowTop = (uintptr_t)pCo->ShadowStackMap + iShadowSize;
	__asm__ volatile (
		"rdsspq %0\n\t"
		"rstorssp -0x8(%2)\n\t"
		"saveprevssp\n\t"
		"rdsspq %1\n\t"
		"rstorssp -0x8(%0)\n\t"
		"saveprevssp\n\t"
		: "=&r"(iOldShadow), "=&r"(iNewShadow)
		: "r"(iShadowTop)
		: "memory", "cc"
	);
	if ( iNewShadow == 0 ) {
		(void)munmap(pCo->ShadowStackMap, pCo->ShadowStackMapSize);
		pCo->ShadowStackMap = NULL;
		pCo->ShadowStackMapSize = 0;
		__xrtErrorSetInvalidState();
		return false;
	}
	pCo->Context.Registers[38] = (ptr)iNewShadow;
	return true;
}
#endif



#if defined(XRT_CO_SHADOW_CALL_STACK)
/* 为 ARM64 ShadowCallStack 创建带双侧保护页的独立返回地址栈。 */
static bool __xrtCoShadowCallStackCreate(
	xcoro* pCo,
	size_t iStackSize,
	size_t iPageSize
)
{
	size_t iShadowSize;
	size_t iMapSize;
	uint8* pShadow;

	if ( iStackSize > (SIZE_MAX - (iPageSize - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iShadowSize =
		((iStackSize + iPageSize - 1u) / iPageSize) * iPageSize;
	if ( iShadowSize > (SIZE_MAX - (2u * iPageSize)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iMapSize = iShadowSize + (2u * iPageSize);
	pShadow = (uint8*)mmap(
		NULL,
		iMapSize,
		PROT_NONE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0
	);
	if ( pShadow == MAP_FAILED ) {
		__xrtCoSetSystemError(
			"create",
			errno,
			"coroutine shadow call stack mapping failed"
		);
		return false;
	}
	if ( mprotect(pShadow + iPageSize, iShadowSize, PROT_READ | PROT_WRITE) != 0 ) {
		int iCode = errno;

		(void)munmap(pShadow, iMapSize);
		__xrtCoSetSystemError(
			"create",
			iCode,
			"coroutine shadow call stack protection failed"
		);
		return false;
	}
	pCo->ShadowStackMap = pShadow;
	pCo->ShadowStackMapSize = iMapSize;
	pCo->Context.Registers[30] = pShadow + iPageSize;
	return true;
}
#endif



/* 使用惰性物理页面和栈底保护页创建 POSIX 协程栈。 */
bool __xrtCoBackendCreate(xcoro* pCo, size_t iStackSize)
{
	size_t iPageSize = __xrtCoPageSize();
	size_t iMapSize;
	uint8* pStackTop;

	if ( iStackSize > (SIZE_MAX - iPageSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iMapSize = iStackSize + iPageSize;
	pCo->StackMap = mmap(
		NULL,
		iMapSize,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS,
		-1,
		0
	);
	if ( pCo->StackMap == MAP_FAILED ) {
		pCo->StackMap = NULL;
		__xrtCoSetSystemError("create", errno, "coroutine stack mapping failed");
		return false;
	}
	if ( mprotect(pCo->StackMap, iPageSize, PROT_NONE) != 0 ) {
		int iCode = errno;

		(void)munmap(pCo->StackMap, iMapSize);
		pCo->StackMap = NULL;
		__xrtCoSetSystemError("create", iCode, "coroutine guard page protection failed");
		return false;
	}
	pCo->StackMapSize = iMapSize;
	pStackTop = (uint8*)pCo->StackMap + iMapSize;
	pStackTop = (uint8*)((uintptr_t)pStackTop & ~(uintptr_t)0x0Fu);
	#if defined(__x86_64__) || defined(_M_X64)
		pStackTop -= sizeof(ptr);
	#endif
	memset(&pCo->Context, 0, sizeof(pCo->Context));
	if ( !__xrtCoFloatCapture(&pCo->Context.FloatEnvironment) ) {
		(void)munmap(pCo->StackMap, pCo->StackMapSize);
		pCo->StackMap = NULL;
		pCo->StackMapSize = 0;
		__xrtErrorSetInvalidState();
		return false;
	}
	#if defined(XRT_CO_SHADOW_STACK)
		if ( !__xrtCoShadowStackCreate(pCo, iStackSize, iPageSize) ) {
			(void)munmap(pCo->StackMap, pCo->StackMapSize);
			pCo->StackMap = NULL;
			pCo->StackMapSize = 0;
			return false;
		}
	#endif
	#if defined(XRT_CO_SHADOW_CALL_STACK)
		if ( !__xrtCoShadowCallStackCreate(pCo, iStackSize, iPageSize) ) {
			(void)munmap(pCo->StackMap, pCo->StackMapSize);
			pCo->StackMap = NULL;
			pCo->StackMapSize = 0;
			return false;
		}
	#endif
	#if defined(XRT_CO_ADDRESS_SANITIZER) || \
		defined(XRT_CO_MEMORY_SANITIZER)
		pCo->Context.SanitizerStackBottom =
			(uint8*)pCo->StackMap + iPageSize;
		pCo->Context.SanitizerStackSize = iMapSize - iPageSize;
	#endif
	#if defined(XRT_CO_THREAD_SANITIZER)
		pCo->Context.ThreadSanitizerFiber = __tsan_create_fiber(0);
		if ( pCo->Context.ThreadSanitizerFiber == NULL ) {
			__xrtCoBackendDestroy(pCo);
			__xrtErrorSetInvalidState();
			return false;
		}
	#endif
	pCo->Context.Registers[0] = (ptr)__xrtCoAsmEntry;
	pCo->Context.Registers[1] = pStackTop;
	pCo->Context.Registers[39] = pCo;
	return true;
}



/* 释放 POSIX 协程栈映射。 */
void __xrtCoBackendDestroy(xcoro* pCo)
{
	#if defined(XRT_CO_THREAD_SANITIZER)
		if ( pCo->Context.ThreadSanitizerFiber != NULL ) {
			__tsan_destroy_fiber(pCo->Context.ThreadSanitizerFiber);
			pCo->Context.ThreadSanitizerFiber = NULL;
		}
	#endif
	#if defined(XRT_CO_SHADOW_STACK) || \
		defined(XRT_CO_SHADOW_CALL_STACK)
		if ( pCo->ShadowStackMap != NULL ) {
			(void)munmap(
				pCo->ShadowStackMap,
				pCo->ShadowStackMapSize
			);
			pCo->ShadowStackMap = NULL;
			pCo->ShadowStackMapSize = 0;
		}
	#endif
	if ( pCo->StackMap != NULL ) {
		(void)munmap(pCo->StackMap, pCo->StackMapSize);
		pCo->StackMap = NULL;
		pCo->StackMapSize = 0;
	}
}



/* 从宿主栈恢复 POSIX 协程上下文。 */
bool __xrtCoBackendResume(xrt_co_runtime* pRuntime, xcoro* pCo)
{
	if ( !__xrtCoFloatCapture(&pRuntime->MainContext.FloatEnvironment) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !__xrtCoFloatRestore(&pCo->Context.FloatEnvironment) ) {
		(void)__xrtCoFloatRestore(&pRuntime->MainContext.FloatEnvironment);
		__xrtErrorSetInvalidState();
		return false;
	}
	#if defined(XRT_CO_THREAD_SANITIZER)
		pRuntime->MainContext.ThreadSanitizerFiber =
			__tsan_get_current_fiber();
		if ( pRuntime->MainContext.ThreadSanitizerFiber == NULL ) {
			__xrtErrorSetInvalidState();
			return false;
		}
		__tsan_switch_to_fiber(pCo->Context.ThreadSanitizerFiber, 0);
	#endif
	#if defined(XRT_CO_MEMORY_SANITIZER)
		__msan_start_switch_fiber(
			pCo->Context.SanitizerStackBottom,
			pCo->Context.SanitizerStackSize
		);
	#endif
	#if defined(XRT_CO_ADDRESS_SANITIZER)
		__sanitizer_start_switch_fiber(
			&pRuntime->MainContext.SanitizerFakeStack,
			pCo->Context.SanitizerStackBottom,
			pCo->Context.SanitizerStackSize
		);
	#endif
	__xrtCoContextSwap(&pRuntime->MainContext, &pCo->Context);
	#if defined(XRT_CO_ADDRESS_SANITIZER)
		__sanitizer_finish_switch_fiber(
			pRuntime->MainContext.SanitizerFakeStack,
			&pCo->Context.SanitizerStackBottom,
			&pCo->Context.SanitizerStackSize
		);
		pRuntime->MainContext.SanitizerFakeStack = NULL;
	#endif
	#if defined(XRT_CO_MEMORY_SANITIZER)
		__msan_finish_switch_fiber(
			&pCo->Context.SanitizerStackBottom,
			&pCo->Context.SanitizerStackSize
		);
	#endif
	return true;
}



/* 从 POSIX 协程返回宿主栈。 */
void __xrtCoBackendYield(xrt_co_runtime* pRuntime, xcoro* pCo)
{
	bool bSaved = __xrtCoFloatCapture(&pCo->Context.FloatEnvironment);
	bool bRestored = __xrtCoFloatRestore(
		&pRuntime->MainContext.FloatEnvironment
	);

	if ( !bSaved || !bRestored ) {
		__xrtErrorSetInvalidState();
	}
	#if defined(XRT_CO_THREAD_SANITIZER)
		__tsan_switch_to_fiber(
			pRuntime->MainContext.ThreadSanitizerFiber,
			0
		);
	#endif
	#if defined(XRT_CO_MEMORY_SANITIZER)
		__msan_start_switch_fiber(
			pRuntime->MainContext.SanitizerStackBottom,
			pRuntime->MainContext.SanitizerStackSize
		);
	#endif
	#if defined(XRT_CO_ADDRESS_SANITIZER)
		__sanitizer_start_switch_fiber(
			pCo->Term == XCORO_TERM_NONE ?
				&pCo->Context.SanitizerFakeStack : NULL,
			pRuntime->MainContext.SanitizerStackBottom,
			pRuntime->MainContext.SanitizerStackSize
		);
	#endif
	__xrtCoContextSwap(&pCo->Context, &pRuntime->MainContext);
	#if defined(XRT_CO_ADDRESS_SANITIZER)
		__sanitizer_finish_switch_fiber(
			pCo->Context.SanitizerFakeStack,
			&pRuntime->MainContext.SanitizerStackBottom,
			&pRuntime->MainContext.SanitizerStackSize
		);
		pCo->Context.SanitizerFakeStack = NULL;
	#endif
	#if defined(XRT_CO_MEMORY_SANITIZER)
		__msan_finish_switch_fiber(
			&pRuntime->MainContext.SanitizerStackBottom,
			&pRuntime->MainContext.SanitizerStackSize
		);
	#endif
}

#endif

#endif
