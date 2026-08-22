#if !defined(_WIN32) && !defined(_WIN64)
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
#endif

#include "../internal/xrt_file.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <sys/stat.h>
	#include <sys/types.h>
#endif



#if defined(XRT_FEATURE_FILE_FIFO)

/* 设置 FIFO 模块结构化错误。 */
static void __xrtFifoError(xerrkind Kind, cstr sMessage, int iSystemCode)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.fifo";
	Desc.Code = XFIFO_ERROR_CREATE;
	Desc.SystemCode = iSystemCode;
	Desc.Operation = "create";
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 创建 POSIX FIFO。 */
XRT_API bool xrtFifoCreate(cstr sPath, uint32 iMode)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') || ((iMode & ~07777u) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		(void)iMode;
		__xrtFifoError(XERR_UNSUPPORTED,
			"POSIX FIFOs are not available on Windows", 0);
		return false;
	#else
		int iResult;

		do {
			iResult = mkfifo(sPath, (mode_t)iMode);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			int iCode = errno;

			__xrtFifoError(__xrtSystemErrorKind(iCode),
				"failed to create the FIFO", iCode);
			return false;
		}
		return true;
	#endif
}

#endif
