#include "../internal/xrt_internal.h"
#include <xrt/logger.h>



#if defined(XRT_FEATURE_LOGGER_FILE_JSON)

/* JSON 文件适配器持有创建时复制的不可变格式配置。 */
typedef struct xlogfilejsonstate {
	xlogjsonconfig Config;
} xlogfilejsonstate;



/* 把通用文件格式回调适配到 JSON 流式格式器。 */
static bool __xrtLogFileJsonFormat(
	const xlogrecord* pRecord,
	xlogwriteproc pWrite,
	ptr pWriteData,
	ptr pUserData
)
{
	xlogfilejsonstate* pState = (xlogfilejsonstate*)pUserData;

	return xrtLogJsonWrite(
		pRecord,
		&pState->Config,
		pWrite,
		pWriteData,
		NULL
	);
}



/* 释放 JSON 文件适配器配置。 */
static void __xrtLogFileJsonDrop(ptr pUserData)
{
	xrtFree(pUserData);
}



/* 使用复制的 JSON 配置创建文件 Sink。 */
XRT_API xlogsink* xrtLogJsonFile(
	const xlogfileoptions* pOptions,
	const xlogjsonconfig* pJson
)
{
	xlogjsonconfig Default;
	xlogfilejsonstate* pState;
	xlogfileconfig Config;
	xlogsink* pSink;

	if ( pOptions == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pJson == NULL ) {
		if ( !xrtLogJsonConfigInit(&Default) ) {
			return NULL;
		}
		pJson = &Default;
	}
	if ( !xrtLogJsonConfigValidate(pJson) ) {
		return NULL;
	}
	pState = (xlogfilejsonstate*)xrtMalloc(sizeof(xlogfilejsonstate));
	if ( pState == NULL ) {
		return NULL;
	}
	pState->Config = *pJson;
	memset(&Config, 0, sizeof(Config));
	Config.Options = *pOptions;
	Config.Format = __xrtLogFileJsonFormat;
	Config.Drop = __xrtLogFileJsonDrop;
	Config.UserData = pState;
	pSink = xrtLogFile(&Config);
	if ( pSink == NULL ) {
		xrtFree(pState);
	}
	return pSink;
}



/* 创建并附加 JSON 文件 Sink。 */
XRT_API bool xrtLogAddJsonFile(
	xlogger* pLogger,
	const xlogfileoptions* pOptions,
	const xlogjsonconfig* pJson
)
{
	xlogsink* pSink;
	bool bResult;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSink = xrtLogJsonFile(pOptions, pJson);
	if ( pSink == NULL ) {
		return false;
	}
	bResult = xrtLogAttach(pLogger, pSink);
	xrtLogSinkFree(pSink);
	return bResult;
}

#endif
