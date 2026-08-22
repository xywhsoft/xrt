#include <stdio.h>

#include <xrt.h>



/* 展示可裁剪的 POSIX FIFO 创建能力。 */
int main(void)
{
	static const char sPath[] = "xrt-fifo-example";

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtFifoCreate(sPath, 0600u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) ) {
			puts("FIFO is not supported on Windows");
			return 0;
		}
		return 1;
	#else
		(void)xrtFileDelete(sPath);
		xrtClearError();
		if ( !xrtFifoCreate(sPath, 0600u) ) {
			return 1;
		}
		puts("FIFO created");
		return xrtFileDelete(sPath) ? 0 : 1;
	#endif
}
