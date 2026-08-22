#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../internal/xrt_internal.h"

#include <errno.h>
#include <limits.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <fcntl.h>
	#include <unistd.h>
	#if defined(__linux__)
		#if defined(__ANDROID__)
			#include <sys/syscall.h>
		#else
			#include <sys/random.h>
		#endif
	#endif
#endif



#if defined(XRT_FEATURE_RANDOM_SECURE)

/* 设置带平台诊断码的密码安全随机源错误。 */
static void __xrtSecureRandomError(xerrkind Kind,
	int iSystemCode, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.random";
	Desc.Code = XRANDOM_ERROR_SYSTEM;
	Desc.SystemCode = (int32)iSystemCode;
	Desc.Operation = "secure-random";
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 清除可能已经写入的部分随机数据并报告失败。 */
static bool __xrtSecureRandomFail(ptr pData, size_t iSize,
	xerrkind Kind, int iSystemCode, cstr sMessage)
{
	xrtSecureZero(pData, iSize);
	__xrtSecureRandomError(Kind, iSystemCode, sMessage);
	return false;
}



#if defined(_WIN32) || defined(_WIN64)

#define XRT_BCRYPT_SYSTEM_RANDOM 0x00000002u
#define XRT_SECURE_RANDOM_UNINITIALIZED 0
#define XRT_SECURE_RANDOM_LOADING 1
#define XRT_SECURE_RANDOM_READY 2
#define XRT_SECURE_RANDOM_FAILED 3

typedef LONG (WINAPI *__xrt_bcrypt_random_proc)(PVOID pAlgorithm,
	unsigned char* pData, ULONG iSize, ULONG iFlags);



static volatile LONG __xrtSecureRandomState = XRT_SECURE_RANDOM_UNINITIALIZED;
static HMODULE __xrtSecureRandomLibrary = NULL;
static __xrt_bcrypt_random_proc __xrtSecureRandomProvider = NULL;
static int __xrtSecureRandomLoadError = 0;



/* 一次性解析系统 CNG；成功后保留模块句柄，避免随机热路径反复进入加载器。 */
static __xrt_bcrypt_random_proc __xrtSecureRandomWindowsProvider(
	int* pSystemCode)
{
	LONG iState = InterlockedCompareExchange(
		&__xrtSecureRandomState,
		XRT_SECURE_RANDOM_UNINITIALIZED,
		XRT_SECURE_RANDOM_UNINITIALIZED
	);

	if ( iState == XRT_SECURE_RANDOM_UNINITIALIZED ) {
		iState = InterlockedCompareExchange(
			&__xrtSecureRandomState,
			XRT_SECURE_RANDOM_LOADING,
			XRT_SECURE_RANDOM_UNINITIALIZED
		);
		if ( iState == XRT_SECURE_RANDOM_UNINITIALIZED ) {
			FARPROC pAddress;

			__xrtSecureRandomLibrary = LoadLibraryW(L"bcrypt.dll");
			if ( __xrtSecureRandomLibrary != NULL ) {
				pAddress = GetProcAddress(__xrtSecureRandomLibrary,
					"BCryptGenRandom");
				if ( pAddress != NULL ) {
					memcpy(&__xrtSecureRandomProvider, &pAddress,
						sizeof(__xrtSecureRandomProvider));
				}
			}
			if ( __xrtSecureRandomProvider != NULL ) {
				(void)InterlockedExchange(&__xrtSecureRandomState,
					XRT_SECURE_RANDOM_READY);
			} else {
				__xrtSecureRandomLoadError = (int)GetLastError();
				if ( __xrtSecureRandomLoadError == 0 ) {
					__xrtSecureRandomLoadError = ERROR_PROC_NOT_FOUND;
				}
				if ( __xrtSecureRandomLibrary != NULL ) {
					(void)FreeLibrary(__xrtSecureRandomLibrary);
					__xrtSecureRandomLibrary = NULL;
				}
				(void)InterlockedExchange(&__xrtSecureRandomState,
					XRT_SECURE_RANDOM_FAILED);
			}
		}
	}
	while ( InterlockedCompareExchange(
		&__xrtSecureRandomState,
		XRT_SECURE_RANDOM_UNINITIALIZED,
		XRT_SECURE_RANDOM_UNINITIALIZED
	) == XRT_SECURE_RANDOM_LOADING ) {
		(void)SwitchToThread();
	}
	if ( InterlockedCompareExchange(
		&__xrtSecureRandomState,
		XRT_SECURE_RANDOM_UNINITIALIZED,
		XRT_SECURE_RANDOM_UNINITIALIZED
	) == XRT_SECURE_RANDOM_READY ) {
		return __xrtSecureRandomProvider;
	}
	if ( pSystemCode != NULL ) {
		*pSystemCode = __xrtSecureRandomLoadError;
	}
	return NULL;
}



/* 调用系统 CNG 填满缓冲，避免单头文件使用者额外链接 bcrypt 导入库。 */
static bool __xrtSecureRandomWindows(ptr pData, size_t iSize)
{
	__xrt_bcrypt_random_proc pRandom;
	uint8* pWrite = (uint8*)pData;
	size_t iRemain = iSize;
	int iLoadError = 0;

	pRandom = __xrtSecureRandomWindowsProvider(&iLoadError);
	if ( pRandom == NULL ) {
		return __xrtSecureRandomFail(pData, iSize,
			__xrtSystemErrorKind(iLoadError), iLoadError,
			"loading the Windows cryptographic provider failed");
	}
	while ( iRemain != 0 ) {
		ULONG iChunk = iRemain > (size_t)ULONG_MAX ?
			ULONG_MAX : (ULONG)iRemain;
		LONG iStatus = pRandom(
			NULL, pWrite, iChunk, XRT_BCRYPT_SYSTEM_RANDOM);

		if ( iStatus < 0 ) {
			return __xrtSecureRandomFail(pData, iSize, XERR_IO,
				(int)iStatus, "Windows secure random generation failed");
		}
		pWrite += iChunk;
		iRemain -= iChunk;
	}
	return true;
}



#undef XRT_BCRYPT_SYSTEM_RANDOM
#undef XRT_SECURE_RANDOM_UNINITIALIZED
#undef XRT_SECURE_RANDOM_LOADING
#undef XRT_SECURE_RANDOM_READY
#undef XRT_SECURE_RANDOM_FAILED

#else

/* 把超大缓冲拆成 POSIX read/getrandom 可表达的有符号长度。 */
static size_t __xrtSecureRandomChunk(size_t iRemain)
{
	#if defined(SSIZE_MAX)
		return iRemain > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : iRemain;
	#else
		return iRemain;
	#endif
}



/* Android 低 API 直接调用稳定的内核接口，普通 Linux 使用 libc 包装。 */
#if defined(__linux__)
static ssize_t __xrtSecureRandomGet(void* pData, size_t iSize)
{
	#if defined(__ANDROID__)
		#if defined(__NR_getrandom)
			return (ssize_t)syscall(__NR_getrandom, pData, iSize, 0);
		#else
			errno = ENOSYS;
			return -1;
		#endif
	#else
		return getrandom(pData, iSize, 0);
	#endif
}
#endif



/* 打开不继承的系统随机设备，并完整处理可中断的系统调用。 */
static int __xrtSecureRandomDeviceOpen(void)
{
	int hFile;

	do {
		#if defined(O_CLOEXEC)
			hFile = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
		#else
			hFile = open("/dev/urandom", O_RDONLY);
		#endif
	} while ( (hFile < 0) && (errno == EINTR) );
	if ( hFile < 0 ) {
		return -1;
	}
	#if !defined(O_CLOEXEC)
		{
			int iFlags;

			do {
				iFlags = fcntl(hFile, F_GETFD);
			} while ( (iFlags < 0) && (errno == EINTR) );
			if ( iFlags >= 0 ) {
				int iResult;

				do {
					iResult = fcntl(hFile, F_SETFD, iFlags | FD_CLOEXEC);
				} while ( (iResult < 0) && (errno == EINTR) );
				if ( iResult == 0 ) {
					return hFile;
				}
			}
			{
				int iCode = errno;

				(void)close(hFile);
				errno = iCode;
				return -1;
			}
		}
	#else
		return hFile;
	#endif
}



/* 从 /dev/urandom 填满剩余区间，不把短读误判为成功。 */
static bool __xrtSecureRandomDevice(
	ptr pData, size_t iSize, size_t iOffset)
{
	uint8* pWrite = (uint8*)pData;
	int hFile = __xrtSecureRandomDeviceOpen();

	if ( hFile < 0 ) {
		int iCode = errno;

		return __xrtSecureRandomFail(pData, iSize,
			__xrtSystemErrorKind(iCode), iCode,
			"opening the operating-system random source failed");
	}
	while ( iOffset < iSize ) {
		size_t iChunk = __xrtSecureRandomChunk(iSize - iOffset);
		ssize_t iRead = read(hFile, pWrite + iOffset, iChunk);

		if ( iRead > 0 ) {
			iOffset += (size_t)iRead;
			continue;
		}
		if ( (iRead < 0) && (errno == EINTR) ) {
			continue;
		}
		{
			int iCode = iRead == 0 ? EIO : errno;

			(void)close(hFile);
			return __xrtSecureRandomFail(pData, iSize,
				__xrtSystemErrorKind(iCode), iCode,
				"reading the operating-system random source failed");
		}
	}
	(void)close(hFile);
	return true;
}



/* Linux 优先使用无文件描述符的 getrandom，旧内核才回退到系统设备。 */
static bool __xrtSecureRandomPosix(ptr pData, size_t iSize)
{
	size_t iOffset = 0;

	#if defined(__linux__)
		while ( iOffset < iSize ) {
			size_t iChunk = __xrtSecureRandomChunk(iSize - iOffset);
			ssize_t iRead = __xrtSecureRandomGet(
				(uint8*)pData + iOffset, iChunk);

			if ( iRead > 0 ) {
				iOffset += (size_t)iRead;
				continue;
			}
			if ( (iRead < 0) && (errno == EINTR) ) {
				continue;
			}
			if ( (iRead < 0) && (errno == ENOSYS) ) {
				break;
			}
			{
				int iCode = iRead == 0 ? EIO : errno;

				return __xrtSecureRandomFail(pData, iSize,
					__xrtSystemErrorKind(iCode), iCode,
					"operating-system secure random generation failed");
			}
		}
		if ( iOffset == iSize ) {
			return true;
		}
	#endif
	return __xrtSecureRandomDevice(pData, iSize, iOffset);
}

#endif



/* 生成密码安全随机字节；任何平台失败都不会退化为可预测伪随机数。 */
XRT_API bool xrtSecureRandom(ptr pData, size_t iSize)
{
	if ( iSize == 0 ) {
		return true;
	}
	if ( pData == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return __xrtSecureRandomWindows(pData, iSize);
	#else
		return __xrtSecureRandomPosix(pData, iSize);
	#endif
}

#endif
