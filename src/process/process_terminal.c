#include "../internal/xrt_process.h"



#if defined(XRT_FEATURE_PROCESS_TERMINAL)

/* 查询当前平台运行时是否具备伪终端支持。 */
XRT_API bool xrtProcessTerminalSupported(void)
{
	return __xrtProcessTerminalSupportedPlatform();
}



/* 校验终端对象和尺寸后交给平台调整窗口。 */
XRT_API bool xrtProcessResize(
	xprocess* pProcess,
	uint32 iColumns,
	uint32 iRows
)
{
	if ( pProcess == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"terminal.resize",
			"process is null",
			0
		);
		return false;
	}
	if ( !pProcess->Terminal ) {
		__xrtProcessErrorSet(
			XERR_STATE,
			XPROCESS_ERROR_TERMINAL,
			"terminal.resize",
			"process is not attached to a terminal",
			0
		);
		return false;
	}
	if ( (iColumns == 0u) || (iColumns > 32767u) ||
		(iRows == 0u) || (iRows > 32767u) ) {
		__xrtProcessErrorSet(
			XERR_RANGE,
			XPROCESS_ERROR_TERMINAL,
			"terminal.resize",
			"terminal dimensions must be between 1 and 32767",
			0
		);
		return false;
	}
	return __xrtProcessTerminalResizePlatform(
		pProcess,
		iColumns,
		iRows
	);
}

#endif
