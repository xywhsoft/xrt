#include "../internal/xrt_internal.h"
#include <xrt/logger.h>



#if defined(XRT_FEATURE_LOGGER_PRINTF)

/* 格式化显式长度消息并同步提交，回调返回前构建器始终有效。 */
XRT_API xlogresult xrtLogSourcePrintfV(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	xstrview File,
	xstrview Function,
	uint32 iLine,
	uint64 iThreadId,
	cstr sFormat,
	va_list Args
)
{
	xstrbuf Message;
	xlogresult Result;

	xrtStrBufInit(&Message);
	if ( !xrtStrBufAppendFormatV(&Message, sFormat, Args) ) {
		xrtStrBufFree(&Message);
		return XLOG_RESULT_ERROR;
	}
	Result = xrtLogSource(
		pLogger,
		Level,
		xrtStrBufView(&Message),
		pFields,
		iFieldCount,
		File,
		Function,
		iLine,
		iThreadId
	);
	xrtStrBufFree(&Message);
	return Result;
}



/* 格式化并提交完整源码元数据。 */
XRT_API xlogresult xrtLogSourcePrintf(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	xstrview File,
	xstrview Function,
	uint32 iLine,
	uint64 iThreadId,
	cstr sFormat,
	...
)
{
	va_list Args;
	xlogresult Result;

	va_start(Args, sFormat);
	Result = xrtLogSourcePrintfV(
		pLogger,
		Level,
		pFields,
		iFieldCount,
		File,
		Function,
		iLine,
		iThreadId,
		sFormat,
		Args
	);
	va_end(Args);
	return Result;
}



/* 格式化并提交结构化字段。 */
XRT_API xlogresult xrtLogFieldsPrintfV(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	cstr sFormat,
	va_list Args
)
{
	return xrtLogSourcePrintfV(
		pLogger,
		Level,
		pFields,
		iFieldCount,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		0,
		0,
		sFormat,
		Args
	);
}



/* 格式化并提交结构化字段。 */
XRT_API xlogresult xrtLogFieldsPrintf(
	xlogger* pLogger,
	xloglevel Level,
	const xlogfield* pFields,
	size_t iFieldCount,
	cstr sFormat,
	...
)
{
	va_list Args;
	xlogresult Result;

	va_start(Args, sFormat);
	Result = xrtLogFieldsPrintfV(
		pLogger,
		Level,
		pFields,
		iFieldCount,
		sFormat,
		Args
	);
	va_end(Args);
	return Result;
}



/* 格式化并提交常用日志。 */
XRT_API xlogresult xrtLogPrintfV(
	xlogger* pLogger,
	xloglevel Level,
	cstr sFormat,
	va_list Args
)
{
	return xrtLogFieldsPrintfV(
		pLogger,
		Level,
		NULL,
		0,
		sFormat,
		Args
	);
}



/* 格式化并提交常用日志。 */
XRT_API xlogresult xrtLogPrintf(
	xlogger* pLogger,
	xloglevel Level,
	cstr sFormat,
	...
)
{
	va_list Args;
	xlogresult Result;

	va_start(Args, sFormat);
	Result = xrtLogPrintfV(pLogger, Level, sFormat, Args);
	va_end(Args);
	return Result;
}

#endif
