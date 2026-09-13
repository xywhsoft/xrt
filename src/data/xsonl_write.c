#include "../internal/xrt_xsonl.h"

#if defined(XRT_FEATURE_XSONL_WRITE)

/* 拒绝 PRETTY，保证一个元素只生成一个物理行。 */
static bool __xrtXsonlWriteConfigValid(const xxsonlwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pConfig->Record.Flags & XXSON_WRITE_PRETTY) != 0) ||
		 (pConfig->MaxOutputBytes == 0) || (pConfig->MaxRecords == 0) ||
		 (pConfig->Reserved[0] != 0) || (pConfig->Reserved[1] != 0) ||
		 (pConfig->Reserved[2] != 0) || (pConfig->Reserved[3] != 0) ) {
		__xrtTextLinesError(&__xrtXsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"write", "invalid XSONL write configuration", NULL, false);
		return false;
	}
	if ( !__xrtXsonWriteConfigValid(&pConfig->Record) ) {
		__xrtTextLinesError(&__xrtXsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"write", "invalid record write configuration", NULL, true);
		return false;
	}
	return true;
}

XRT_API void xrtXsonlWriteConfigInit(xxsonlwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtXsonWriteConfigInit(&pConfig->Record);
	pConfig->MaxOutputBytes = XXSON_INPUT_DEFAULT;
	pConfig->MaxRecords = XXSON_CONTAINER_DEFAULT;
}

static bool __xrtXsonlRecordWrite(
	const xvalue* pValue, const void* pConfig, xtextlineswriteproc pWrite, ptr pUserData
)
{
	return xrtXsonWrite(pValue, (const xxsonwriteconfig*)pConfig, pWrite, pUserData);
}

static xtextlineswriteconfig __xrtXsonlWriteConfig(const xxsonlwriteconfig* pConfig)
{
	xtextlineswriteconfig Config;
	Config.MaxOutputBytes = pConfig->MaxOutputBytes;
	Config.MaxRecords = pConfig->MaxRecords;
	Config.Record = &pConfig->Record;
	Config.Write = __xrtXsonlRecordWrite;
	return Config;
}

XRT_API bool xrtXsonlWrite(
	const xvalue* pArray, const xxsonlwriteconfig* pConfig,
	xxsonwriteproc pWrite, ptr pUserData
)
{
	xxsonlwriteconfig Snapshot;
	xtextlineswriteconfig Config;
	if ( !__xrtXsonlWriteConfigValid(pConfig) ) {
		return false;
	}
	Snapshot = *pConfig;
	Config = __xrtXsonlWriteConfig(&Snapshot);
	return __xrtTextLinesWrite(pArray, &Config, &__xrtXsonlFormat, pWrite, pUserData);
}

str __xrtXsonlStringify(
	const xvalue* pArray, const xxsonlwriteconfig* pConfig, size_t* pSize
)
{
	xxsonlwriteconfig Snapshot;
	xtextlineswriteconfig Config;
	if ( !__xrtXsonlWriteConfigValid(pConfig) ) {
		return NULL;
	}
	Snapshot = *pConfig;
	Config = __xrtXsonlWriteConfig(&Snapshot);
	return __xrtTextLinesStringify(pArray, &Config, &__xrtXsonlFormat, pSize);
}

XRT_API str xrtXsonlStringify(const xvalue* pArray, size_t* pSize)
{
	xxsonlwriteconfig Config;
	xrtXsonlWriteConfigInit(&Config);
	return __xrtXsonlStringify(pArray, &Config, pSize);
}
#endif

