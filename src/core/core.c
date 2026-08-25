#include "../internal/xrt_internal.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <unistd.h>
#endif

#if defined(__APPLE__)
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
		if ( sysctl(aMib, 2, &iCount, &iSize, NULL, 0) != 0 || iCount <= 0 )
			iCount = 1;
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
XRT_API int32 xrtRefRetain(volatile int32* pCount)
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
XRT_API int32 xrtRefRelease(volatile int32* pCount)
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
