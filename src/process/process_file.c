#include "../internal/xrt_process.h"



#if defined(XRT_FEATURE_PROCESS_FILE)

/* 把借用文件映射为 Spawn 复制的原生标准流句柄。 */
XRT_API xprocessio xrtProcessFile(xfile File)
{
	xprocessio Io;

	Io.Mode = XPROCESS_IO_HANDLE;
	Io.Handle = xrtFileNative(File);
	if ( Io.Handle == -1 ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"file",
			"process file is invalid",
			0
		);
	}
	return Io;
}

#endif
