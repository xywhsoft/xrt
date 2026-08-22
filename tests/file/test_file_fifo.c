#include "../test.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <sys/types.h>
	#include <sys/stat.h>
#endif



/* FIFO 创建在 POSIX 上形成对象，在 Windows 上明确报告不支持。 */
int main(void)
{
	static const char sPath[] = "xrt-file-fifo-test";

	testRequire(!xrtFifoCreate(NULL, 0600u) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FIFO creation accepted a null path");
	testRequire(!xrtFifoCreate("", 0600u) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FIFO creation accepted an empty path");
	testRequire(!xrtFifoCreate(sPath, 010000u) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"FIFO creation accepted mode bits outside 07777");
	xrtClearError();

	#if defined(_WIN32) || defined(_WIN64)
		testRequire(!xrtFifoCreate(sPath, 0600u),
			"Windows FIFO creation unexpectedly succeeded");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.fifo") == 0) &&
			(xrtErrorCode(xrtGetError()) == XFIFO_ERROR_CREATE) &&
			(strcmp(xrtErrorOperation(xrtGetError()), "create") == 0),
			"Windows FIFO creation reported the wrong error");
	#elif defined(__ANDROID__)
		(void)xrtFileDelete(sPath);
		xrtClearError();
		testRequire(!xrtFifoCreate(sPath, 0640u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION) &&
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.fifo") == 0) &&
			(xrtErrorCode(xrtGetError()) == XFIFO_ERROR_CREATE) &&
			(strcmp(xrtErrorOperation(xrtGetError()), "create") == 0),
			"Android FIFO failure lost its permission error");
	#else
		xfileinfo Info;
		mode_t iMask;
		bool bCreated;

		(void)xrtFileDelete(sPath);
		xrtClearError();
		iMask = umask(0);
		bCreated = xrtFifoCreate(sPath, 0640u);
		(void)umask(iMask);
		testRequire(bCreated, "FIFO creation failed");
		testRequire(xrtPathStat(sPath, false, &Info) &&
			(Info.Type == XFILE_TYPE_FIFO) &&
			((Info.Available & XFILE_INFO_MODE) != 0u) &&
			((Info.Mode & 07777u) == 0640u),
			"FIFO metadata type or permission mode is incorrect");
		testRequire(!xrtFifoCreate(sPath, 0600u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_EXISTS) &&
			(strcmp(xrtErrorDomain(xrtGetError()), "xrt.fifo") == 0) &&
			(xrtErrorCode(xrtGetError()) == XFIFO_ERROR_CREATE) &&
			(strcmp(xrtErrorOperation(xrtGetError()), "create") == 0),
			"duplicate FIFO creation reported the wrong error");
		xrtClearError();
		testRequire(xrtFileDelete(sPath), "FIFO cleanup failed");
	#endif
	return 0;
}
