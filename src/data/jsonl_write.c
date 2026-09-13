#include "../internal/xrt_jsonl.h"

#if defined(XRT_FEATURE_JSONL_WRITE)

/* 拒绝 PRETTY，保证一个元素只生成一个物理行。 */
static bool __xrtJsonlWriteConfigValid(const xjsonlwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pConfig->Record.Flags & XJSON_WRITE_PRETTY) != 0) ||
		 (pConfig->MaxOutputBytes == 0) || (pConfig->MaxRecords == 0) ||
		 (pConfig->Reserved[0] != 0) || (pConfig->Reserved[1] != 0) ||
		 (pConfig->Reserved[2] != 0) || (pConfig->Reserved[3] != 0) ) {
		__xrtTextLinesError(&__xrtJsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"write", "invalid JSONL write configuration", NULL, false);
		return false;
	}
	if ( !__xrtJsonWriteConfigValid(&pConfig->Record) ) {
		__xrtTextLinesError(&__xrtJsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"write", "invalid record write configuration", NULL, true);
		return false;
	}
	return true;
}

XRT_API void xrtJsonlWriteConfigInit(xjsonlwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtJsonWriteConfigInit(&pConfig->Record);
	pConfig->MaxOutputBytes = XJSON_INPUT_DEFAULT;
	pConfig->MaxRecords = XJSON_CONTAINER_DEFAULT;
}

static bool __xrtJsonlRecordWrite(
	const xvalue* pValue, const void* pConfig, xtextlineswriteproc pWrite, ptr pUserData
)
{
	return xrtJsonWrite(pValue, (const xjsonwriteconfig*)pConfig, pWrite, pUserData);
}

static xtextlineswriteconfig __xrtJsonlWriteConfig(const xjsonlwriteconfig* pConfig)
{
	xtextlineswriteconfig Config;
	Config.MaxOutputBytes = pConfig->MaxOutputBytes;
	Config.MaxRecords = pConfig->MaxRecords;
	Config.Record = &pConfig->Record;
	Config.Write = __xrtJsonlRecordWrite;
	return Config;
}

XRT_API bool xrtJsonlWrite(
	const xvalue* pArray, const xjsonlwriteconfig* pConfig,
	xjsonwriteproc pWrite, ptr pUserData
)
{
	xjsonlwriteconfig Snapshot;
	xtextlineswriteconfig Config;
	if ( !__xrtJsonlWriteConfigValid(pConfig) ) {
		return false;
	}
	Snapshot = *pConfig;
	Config = __xrtJsonlWriteConfig(&Snapshot);
	return __xrtTextLinesWrite(pArray, &Config, &__xrtJsonlFormat, pWrite, pUserData);
}

str __xrtJsonlStringify(
	const xvalue* pArray, const xjsonlwriteconfig* pConfig, size_t* pSize
)
{
	xjsonlwriteconfig Snapshot;
	xtextlineswriteconfig Config;
	if ( !__xrtJsonlWriteConfigValid(pConfig) ) {
		return NULL;
	}
	Snapshot = *pConfig;
	Config = __xrtJsonlWriteConfig(&Snapshot);
	return __xrtTextLinesStringify(pArray, &Config, &__xrtJsonlFormat, pSize);
}

XRT_API str xrtJsonlStringify(const xvalue* pArray, size_t* pSize)
{
	xjsonlwriteconfig Config;
	xrtJsonlWriteConfigInit(&Config);
	return __xrtJsonlStringify(pArray, &Config, pSize);
}
#endif

