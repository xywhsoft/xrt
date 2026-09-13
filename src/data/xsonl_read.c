#include "../internal/xrt_xsonl.h"

#if defined(XRT_FEATURE_XSONL_READ)

/* 即使输入为空，也完整验证配置和底层记录配置。 */
bool __xrtXsonlReadConfigValid(const xxsonlreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pConfig->Flags & ~XXSONL_READ_REJECT_EMPTY_LINES) != 0) ||
		 (pConfig->MaxInputBytes == 0) || (pConfig->MaxRecords == 0) ||
		 (pConfig->MaxTotalValues == 0) ||
		 (pConfig->MaxTotalDecodedBytes == 0) ||
		 (pConfig->Reserved[0] != 0) || (pConfig->Reserved[1] != 0) ||
		 (pConfig->Reserved[2] != 0) || (pConfig->Reserved[3] != 0) ) {
		__xrtTextLinesError(&__xrtXsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"read", "invalid XSONL read configuration", NULL, false);
		return false;
	}
	if ( !__xrtXsonReadConfigValid(&pConfig->Record) ) {
		__xrtTextLinesError(&__xrtXsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"read", "invalid record read configuration", NULL, true);
		return false;
	}
	return true;
}

XRT_API void xrtXsonlReadConfigInit(xxsonlreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtXsonReadConfigInit(&pConfig->Record);
	pConfig->MaxInputBytes = XXSON_INPUT_DEFAULT;
	pConfig->MaxRecords = XXSON_CONTAINER_DEFAULT;
	pConfig->MaxTotalValues = XXSON_VALUES_DEFAULT;
	pConfig->MaxTotalDecodedBytes = XXSON_DECODED_DEFAULT;
}

static xvalue* __xrtXsonlRecordRead(
	xstrview Text, const void* pConfig, xtextvaluebudget* pBudget, bool bValidate
)
{
	return __xrtXsonReadBudget(Text, (const xxsonreadconfig*)pConfig, pBudget, bValidate);
}

static xvalue* __xrtXsonlRead(
	xstrview Text, const xxsonlreadconfig* pConfig, bool bValidate
)
{
	xxsonlreadconfig Snapshot;
	xtextlinesreadconfig Config;
	if ( !__xrtXsonlReadConfigValid(pConfig) ) {
		return NULL;
	}
	Snapshot = *pConfig;
	memset(&Config, 0, sizeof(Config));
	Config.RejectEmpty = (Snapshot.Flags & XXSONL_READ_REJECT_EMPTY_LINES) != 0;
	Config.MaxInputBytes = Snapshot.MaxInputBytes;
	Config.MaxLineBytes = Snapshot.Record.MaxInputBytes;
	Config.MaxRecords = Snapshot.MaxRecords;
	Config.MaxTotalValues = Snapshot.MaxTotalValues;
	Config.MaxTotalDecodedBytes = Snapshot.MaxTotalDecodedBytes;
	Config.Record = &Snapshot.Record;
	Config.Read = __xrtXsonlRecordRead;
	return __xrtTextLinesRead(Text, &Config, &__xrtXsonlFormat, bValidate);
}

XRT_API xvalue* xrtXsonlRead(xstrview Text, const xxsonlreadconfig* pConfig)
{
	return __xrtXsonlRead(Text, pConfig, false);
}

XRT_API xvalue* xrtXsonlParse(xstrview Text)
{
	xxsonlreadconfig Config;
	xrtXsonlReadConfigInit(&Config);
	return xrtXsonlRead(Text, &Config);
}

XRT_API bool xrtXsonlValid(xstrview Text)
{
	xxsonlreadconfig Config;
	xvalue* pResult;
	xrtXsonlReadConfigInit(&Config);
	pResult = __xrtXsonlRead(Text, &Config, true);
	xrtValueRelease(pResult);
	return pResult != NULL;
}
#endif

