#include "../internal/xrt_xlonl.h"

#if defined(XRT_FEATURE_XLONL_WRITE)

/* 拒绝 PRETTY，保证一个元素只生成一个物理行。 */
static bool __xrtXlonlWriteConfigValid(const xxlonlwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pConfig->Record.Flags & XXLON_WRITE_PRETTY) != 0) ||
		 (pConfig->MaxOutputBytes == 0) || (pConfig->MaxRecords == 0) ||
		 (pConfig->Reserved[0] != 0) || (pConfig->Reserved[1] != 0) ||
		 (pConfig->Reserved[2] != 0) || (pConfig->Reserved[3] != 0) ) {
		__xrtTextLinesError(&__xrtXlonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"write", "invalid XLONL write configuration", NULL, false);
		return false;
	}
	if ( !__xrtXlonWriteConfigValid(&pConfig->Record) ) {
		__xrtTextLinesError(&__xrtXlonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"write", "invalid record write configuration", NULL, true);
		return false;
	}
	return true;
}

XRT_API void xrtXlonlWriteConfigInit(xxlonlwriteconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtXlonWriteConfigInit(&pConfig->Record);
	pConfig->MaxOutputBytes = XXLON_INPUT_DEFAULT;
	pConfig->MaxRecords = XXLON_CONTAINER_DEFAULT;
}

static bool __xrtXlonlRecordWrite(
	const xvalue* pValue, const void* pConfig, xtextlineswriteproc pWrite, ptr pUserData
)
{
	return xrtXlonWrite(pValue, (const xxlonwriteconfig*)pConfig, pWrite, pUserData);
}

static xtextlineswriteconfig __xrtXlonlWriteConfig(const xxlonlwriteconfig* pConfig)
{
	xtextlineswriteconfig Config;
	Config.MaxOutputBytes = pConfig->MaxOutputBytes;
	Config.MaxRecords = pConfig->MaxRecords;
	Config.Record = &pConfig->Record;
	Config.Write = __xrtXlonlRecordWrite;
	return Config;
}

XRT_API bool xrtXlonlWrite(
	const xvalue* pArray, const xxlonlwriteconfig* pConfig,
	xxlonwriteproc pWrite, ptr pUserData
)
{
	xxlonlwriteconfig Snapshot;
	xtextlineswriteconfig Config;
	if ( !__xrtXlonlWriteConfigValid(pConfig) ) {
		return false;
	}
	Snapshot = *pConfig;
	Config = __xrtXlonlWriteConfig(&Snapshot);
	return __xrtTextLinesWrite(pArray, &Config, &__xrtXlonlFormat, pWrite, pUserData);
}

str __xrtXlonlStringify(
	const xvalue* pArray, const xxlonlwriteconfig* pConfig, size_t* pSize
)
{
	xxlonlwriteconfig Snapshot;
	xtextlineswriteconfig Config;
	if ( !__xrtXlonlWriteConfigValid(pConfig) ) {
		return NULL;
	}
	Snapshot = *pConfig;
	Config = __xrtXlonlWriteConfig(&Snapshot);
	return __xrtTextLinesStringify(pArray, &Config, &__xrtXlonlFormat, pSize);
}

XRT_API str xrtXlonlStringify(const xvalue* pArray, size_t* pSize)
{
	xxlonlwriteconfig Config;
	xrtXlonlWriteConfigInit(&Config);
	return __xrtXlonlStringify(pArray, &Config, pSize);
}
#endif
