/* -std=c11 的严格 ISO 模式下 Darwin 系统头不定义 u_int、隐藏
   _SC_NPROCESSORS_ONLN；必须在任何系统头之前声明 BSD 特性宏。 */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
	#define _DARWIN_C_SOURCE 1
#endif

#include "../internal/xrt_internal.h"

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
static SRWLOCK __xrtLocalSlotLock = SRWLOCK_INIT;
static xrt_local_slot* __xrtLocalSlots;
/* 0 active, 1 retiring, 2 retired, 3 failed terminal retirement. */
static unsigned __xrtLocalSlotPhase;

DWORD __xrtLocalSlotAlloc(xrt_local_slot* pSlot,
	VOID (WINAPI *pDestroy)(PVOID), unsigned iOrder, bool bFiber)
{
	DWORD iIndex;
	AcquireSRWLockExclusive(&__xrtLocalSlotLock);
	if ( __xrtLocalSlotPhase != 0 ) {
		ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
		SetLastError(ERROR_INVALID_PARAMETER);
		return TLS_OUT_OF_INDEXES;
	}
	iIndex = bFiber ? FlsAlloc(pDestroy) : TlsAlloc();
	if ( iIndex != TLS_OUT_OF_INDEXES ) {
		pSlot->Index = iIndex;
		pSlot->Order = iOrder;
		pSlot->Fiber = bFiber;
		pSlot->Next = __xrtLocalSlots;
		__xrtLocalSlots = pSlot;
	}
	ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
	return iIndex;
}

/* Called by a failed lazy initializer, before any value is published. */
bool __xrtLocalSlotFree(xrt_local_slot* pSlot)
{
	xrt_local_slot** ppSlot;
	bool bFreed;
	AcquireSRWLockExclusive(&__xrtLocalSlotLock);
	ppSlot = &__xrtLocalSlots;
	while ( *ppSlot != NULL && *ppSlot != pSlot ) ppSlot = &(*ppSlot)->Next;
	if ( *ppSlot == NULL ) {
		ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
		return false;
	}
	/* Initializer rollback has no callback payload and cannot re-enter XRT. */
	bFreed = (pSlot->Fiber ? FlsFree(pSlot->Index) : TlsFree(pSlot->Index)) != 0;
	if ( bFreed ) *ppSlot = pSlot->Next;
	ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
	return bFreed;
}
#endif

XRT_API bool xrtRuntimeRetireThreadStorage(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		unsigned iOrder;
		AcquireSRWLockExclusive(&__xrtLocalSlotLock);
		if ( __xrtLocalSlotPhase == 1 || __xrtLocalSlotPhase == 2 ) {
			bool bDone = __xrtLocalSlotPhase == 2;
			ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
			return bDone;
		}
		__xrtLocalSlotPhase = 1;
		ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
		for ( iOrder = XRT_LOCAL_PAYLOAD; iOrder <= XRT_LOCAL_BORROWED; ++iOrder ) {
			for ( ;; ) {
				xrt_local_slot** ppSlot;
				xrt_local_slot* pSlot;
				bool bFreed;
				AcquireSRWLockExclusive(&__xrtLocalSlotLock);
				ppSlot = &__xrtLocalSlots;
				while ( *ppSlot != NULL && (*ppSlot)->Order != iOrder ) ppSlot = &(*ppSlot)->Next;
				pSlot = *ppSlot;
				ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
				if ( pSlot == NULL ) break;
				/* FlsFree invokes every nonempty fiber's destructor synchronously.
				 * Never hold our lock across it; nested retirement must fail rather
				 * than deadlock. Existing lower-level slots remain usable by drops. */
				bFreed = (pSlot->Fiber ? FlsFree(pSlot->Index) : TlsFree(pSlot->Index)) != 0;
				AcquireSRWLockExclusive(&__xrtLocalSlotLock);
				if ( !bFreed ) {
					__xrtLocalSlotPhase = 3;
					ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
					return false;
				}
				*ppSlot = pSlot->Next;
				ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
			}
		}
		AcquireSRWLockExclusive(&__xrtLocalSlotLock);
		__xrtLocalSlotPhase = 2;
		ReleaseSRWLockExclusive(&__xrtLocalSlotLock);
		return true;
	#else
		return false;
	#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
	#include <unistd.h>
#endif

#if defined(__APPLE__)
	#include <sys/types.h>
	#include <sys/sysctl.h>
#endif



#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
/* TinyCC POSIX 没有可靠的原子内建，引用边界检查与更新共用短锁。 */
static pthread_mutex_t __xrtPublicRefLock = PTHREAD_MUTEX_INITIALIZER;
#endif



/* 把当前平台系统错误代码映射为跨模块稳定类别。 */
xerrkind __xrtSystemErrorKind(int iCode)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( (iCode == ERROR_FILE_NOT_FOUND) || (iCode == ERROR_PATH_NOT_FOUND) ||
			 (iCode == ERROR_INVALID_DRIVE) || (iCode == ERROR_INVALID_NAME) ) {
			return XERR_NOT_FOUND;
		}
		if ( (iCode == ERROR_FILE_EXISTS) || (iCode == ERROR_ALREADY_EXISTS) ) {
			return XERR_EXISTS;
		}
		if ( (iCode == ERROR_ACCESS_DENIED) || (iCode == ERROR_SHARING_VIOLATION) ||
			 (iCode == ERROR_LOCK_VIOLATION) || (iCode == ERROR_PRIVILEGE_NOT_HELD) ||
			 (iCode == ERROR_WRITE_PROTECT) ) {
			return XERR_PERMISSION;
		}
		if ( (iCode == ERROR_NOT_ENOUGH_MEMORY) || (iCode == ERROR_OUTOFMEMORY) ) {
			return XERR_MEMORY;
		}
		if ( (iCode == ERROR_BUSY) || (iCode == ERROR_RETRY) ||
			 (iCode == ERROR_IO_PENDING) ) {
			return XERR_AGAIN;
		}
		if ( (iCode == ERROR_TIMEOUT) || (iCode == WAIT_TIMEOUT) ) {
			return XERR_TIMEOUT;
		}
		if ( iCode == ERROR_OPERATION_ABORTED ) {
			return XERR_CANCELLED;
		}
		if ( (iCode == ERROR_NOT_SUPPORTED) ||
			 (iCode == ERROR_CALL_NOT_IMPLEMENTED) ) {
			return XERR_UNSUPPORTED;
		}
		if ( (iCode == ERROR_INVALID_PARAMETER) || (iCode == ERROR_BAD_ARGUMENTS) ) {
			return XERR_ARGUMENT;
		}
	#else
		if ( (iCode == ENOENT) || (iCode == ENOTDIR) ) {
			return XERR_NOT_FOUND;
		}
		if ( iCode == EEXIST ) {
			return XERR_EXISTS;
		}
		if ( (iCode == EACCES) || (iCode == EPERM) || (iCode == EROFS) ) {
			return XERR_PERMISSION;
		}
		if ( iCode == ENOMEM ) {
			return XERR_MEMORY;
		}
		if ( (iCode == EAGAIN) || (iCode == EWOULDBLOCK) || (iCode == EBUSY) ) {
			return XERR_AGAIN;
		}
		#if defined(ETIMEDOUT)
			if ( iCode == ETIMEDOUT ) {
				return XERR_TIMEOUT;
			}
		#endif
		#if defined(ECANCELED)
			if ( iCode == ECANCELED ) {
				return XERR_CANCELLED;
			}
		#endif
		if ( iCode == EINVAL ) {
			return XERR_ARGUMENT;
		}
		#if defined(EOVERFLOW)
			if ( iCode == EOVERFLOW ) {
				return XERR_RANGE;
			}
		#endif
		#if defined(ENOSYS)
			if ( iCode == ENOSYS ) {
				return XERR_UNSUPPORTED;
			}
		#endif
		#if defined(ENOTSUP)
			if ( iCode == ENOTSUP ) {
				return XERR_UNSUPPORTED;
			}
		#endif
		#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (EOPNOTSUPP != ENOTSUP))
			if ( iCode == EOPNOTSUPP ) {
				return XERR_UNSUPPORTED;
			}
		#endif
	#endif
	return XERR_IO;
}



/* 返回当前系统可用于调度的逻辑处理器数量。 */
uint32 __xrtProcessorCount(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		SYSTEM_INFO tInfo;

		GetSystemInfo(&tInfo);
		return tInfo.dwNumberOfProcessors != 0 ?
			(uint32)tInfo.dwNumberOfProcessors : 1u;
	#elif defined(__APPLE__)
		int aMib[2] = {CTL_HW, HW_AVAILCPU};
		int iCount = 0;
		size_t iSize = sizeof(iCount);

		/* -std=c11 的严格 ISO 模式会隐藏 _SC_NPROCESSORS_ONLN，Apple 走 sysctl */
		if ( (sysctl(aMib, 2, &iCount, &iSize, NULL, 0) != 0) ||
			(iCount <= 0) ) {
			iCount = 1;
		}
		return (uint32)iCount;
	#elif defined(_SC_NPROCESSORS_ONLN)
		long iCount = sysconf(_SC_NPROCESSORS_ONLN);

		return (iCount > 0) && ((uint64)iCount <= UINT32_MAX) ?
			(uint32)iCount : 1u;
	#else
		return 1u;
	#endif
}



/* 返回当前 XRT 版本字符串。 */
XRT_API cstr xrtVersion(void)
{
	return XRT_VERSION_TEXT;
}



/* 初始化一组适合处理不受信任输入的通用资源边界。 */
XRT_API void xrtResourceLimitsInit(xrtresourcelimits* pLimits)
{
	if ( pLimits == NULL ) {
		return;
	}
	memset(pLimits, 0, sizeof(*pLimits));
	pLimits->iSize = (uint32)sizeof(*pLimits);
	pLimits->iVersion = XRT_RESOURCE_LIMITS_VERSION;
	pLimits->iMaxInputBytes = 256u * 1024u * 1024u;
	pLimits->iMaxOutputBytes = 512u * 1024u * 1024u;
	pLimits->iMaxItemBytes = 256u * 1024u * 1024u;
	pLimits->iMaxEntries = 100000u;
	pLimits->iMaxNodes = 1000000u;
	pLimits->iMaxDepth = 128u;
	pLimits->iMaxCompressionRatio = 1000u;
}



/* 原子增加有效引用计数，失败时返回 -1。 */
static int32 __xrtRefRetainUnfenced(volatile int32* pCount)
{
	int32 iOld;
	int32 iNext;

	if ( pCount == NULL ) {
		return -1;
	}
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_mutex_lock(&__xrtPublicRefLock);
		iOld = *pCount;
		if ( (iOld <= 0) || (iOld == INT32_MAX) ) {
			iNext = -1;
		} else {
			iNext = iOld + 1;
			*pCount = iNext;
		}
		(void)pthread_mutex_unlock(&__xrtPublicRefLock);
		return iNext;
	#else
		iOld = __xrtAtomicRefLoad(pCount);
		for ( ;; ) {
			if ( (iOld <= 0) || (iOld == INT32_MAX) ) {
				return -1;
			}
			iNext = iOld + 1;
			if ( __xrtAtomicRefCompareExchange(pCount, iNext, iOld) == iOld ) {
				return iNext;
			}
			iOld = __xrtAtomicRefLoad(pCount);
		}
	#endif
}



/* 原子减少有效引用计数，失败时返回 -1。 */
static int32 __xrtRefReleaseUnfenced(volatile int32* pCount)
{
	int32 iOld;
	int32 iNext;

	if ( pCount == NULL ) {
		return -1;
	}
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_mutex_lock(&__xrtPublicRefLock);
		iOld = *pCount;
		if ( iOld <= 0 ) {
			iNext = -1;
		} else {
			iNext = iOld - 1;
			*pCount = iNext;
		}
		(void)pthread_mutex_unlock(&__xrtPublicRefLock);
		return iNext;
	#else
		iOld = __xrtAtomicRefLoad(pCount);
		for ( ;; ) {
			if ( iOld <= 0 ) {
				return -1;
			}
			iNext = iOld - 1;
			if ( __xrtAtomicRefCompareExchange(pCount, iNext, iOld) == iOld ) {
				return iNext;
			}
			iOld = __xrtAtomicRefLoad(pCount);
		}
	#endif
}

/* The existing CAS/count semantics are unchanged. This short participation
 * makes native strong retain/release and Value weak promotion linearize on
 * the same side of an admitted ownership freeze. Enclosing edge mutations
 * still need an outer scope; a single RC update is not a graph transaction. */
XRT_API int32 xrtRefRetain(volatile int32* pCount)
{
	xrtownershipscope Scope = {0};
	int32 iResult;
	if (pCount == NULL) return -1;
	if (!xrtOwnershipMutationBegin(&Scope)) return -1;
	iResult = __xrtRefRetainUnfenced(pCount);
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	return iResult;
}

XRT_API int32 xrtRefRelease(volatile int32* pCount)
{
	xrtownershipscope Scope = {0};
	int32 iResult;
	if (pCount == NULL) return -1;
	if (!xrtOwnershipMutationBegin(&Scope)) return -1;
	iResult = __xrtRefReleaseUnfenced(pCount);
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	return iResult;
}
