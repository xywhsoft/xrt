#include "../internal/xrt_internal.h"
#include <xrt/logger.h>



#if defined(XRT_FEATURE_LOGGER_FILE_TEXT)

/* 文本文件适配器持有创建时复制的不可变格式配置。 */
typedef struct xlogfiletextstate {
	xlogtextconfig Config;
} xlogfiletextstate;



/* 把通用文件格式回调适配到文本流式格式器。 */
static bool __xrtLogFileTextFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	xlogfiletextstate* pState = (xlogfiletextstate*)pUserData;

	return xrtLogTextWrite(
		pRecord,
		&pState->Config,
		pWrite,
		pWriteData,
		NULL
	);
}



/* 释放文本文件适配器配置。 */
static void __xrtLogFileTextDrop(ptr pUserData)
{
	xrtFree(pUserData);
}



/* 使用复制的文本配置创建文件 Sink。 */
XRT_API xlogsink* xrtLogTextFile(
	const xlogfileoptions* pOptions,
	const xlogtextconfig* pText
)
{
	xlogtextconfig Default;
	xlogfiletextstate* pState;
	xlogfileconfig Config;
	xlogsink* pSink;

	if ( pOptions == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pText == NULL ) {
		if ( !xrtLogTextConfigInit(&Default, XLOG_TEXT_FULL) ) {
			return NULL;
		}
		pText = &Default;
	}
	if ( !xrtLogTextConfigValidate(pText) ) {
		return NULL;
	}
	pState = (xlogfiletextstate*)xrtMalloc(sizeof(xlogfiletextstate));
	if ( pState == NULL ) {
		return NULL;
	}
	pState->Config = *pText;
	memset(&Config, 0, sizeof(Config));
	Config.Options = *pOptions;
	Config.Format = __xrtLogFileTextFormat;
	Config.Drop = __xrtLogFileTextDrop;
	Config.UserData = pState;
	pSink = xrtLogFile(&Config);
	if ( pSink == NULL ) {
		xrtFree(pState);
	}
	return pSink;
}



/* 创建并附加文本文件 Sink。 */
XRT_API bool xrtLogAddTextFile(
	xlogger* pLogger,
	const xlogfileoptions* pOptions,
	const xlogtextconfig* pText
)
{
	xlogsink* pSink;
	bool bResult;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSink = xrtLogTextFile(pOptions, pText);
	if ( pSink == NULL ) {
		return false;
	}
	bResult = xrtLogAttach(pLogger, pSink);
	xrtLogSinkFree(pSink);
	return bResult;
}

#endif
