#include "../internal/xrt_xlonl.h"

#if defined(XRT_FEATURE_XLONL_READ)

/* 即使输入为空，也完整验证配置和底层记录配置。 */
bool __xrtXlonlReadConfigValid(const xxlonlreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pConfig->Flags & ~XXLONL_READ_REJECT_EMPTY_LINES) != 0) ||
		 (pConfig->MaxInputBytes == 0) || (pConfig->MaxRecords == 0) ||
		 (pConfig->MaxTotalValues == 0) ||
		 (pConfig->MaxTotalDecodedBytes == 0) ||
		 (pConfig->Reserved[0] != 0) || (pConfig->Reserved[1] != 0) ||
		 (pConfig->Reserved[2] != 0) || (pConfig->Reserved[3] != 0) ) {
		__xrtTextLinesError(&__xrtXlonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"read", "invalid XLONL read configuration", NULL, false);
		return false;
	}
	if ( !__xrtXlonReadConfigValid(&pConfig->Record) ) {
		__xrtTextLinesError(&__xrtXlonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"read", "invalid record read configuration", NULL, true);
		return false;
	}
	return true;
}

XRT_API void xrtXlonlReadConfigInit(xxlonlreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtXlonReadConfigInit(&pConfig->Record);
	pConfig->MaxInputBytes = XXLON_INPUT_DEFAULT;
	pConfig->MaxRecords = XXLON_CONTAINER_DEFAULT;
	pConfig->MaxTotalValues = XXLON_VALUES_DEFAULT;
	pConfig->MaxTotalDecodedBytes = XXLON_DECODED_DEFAULT;
}

static xvalue* __xrtXlonlRecordRead(
	xstrview Text, const void* pConfig, xtextvaluebudget* pBudget, bool bValidate
)
{
	return __xrtXlonReadBudget(Text, (const xxlonreadconfig*)pConfig, pBudget, bValidate);
}

static xvalue* __xrtXlonlRead(
	xstrview Text, const xxlonlreadconfig* pConfig, bool bValidate
)
{
	xxlonlreadconfig Snapshot;
	xtextlinesreadconfig Config;
	if ( !__xrtXlonlReadConfigValid(pConfig) ) {
		return NULL;
	}
	Snapshot = *pConfig;
	memset(&Config, 0, sizeof(Config));
	Config.RejectEmpty = (Snapshot.Flags & XXLONL_READ_REJECT_EMPTY_LINES) != 0;
	Config.MaxInputBytes = Snapshot.MaxInputBytes;
	Config.MaxLineBytes = Snapshot.Record.MaxInputBytes;
	Config.MaxRecords = Snapshot.MaxRecords;
	Config.MaxTotalValues = Snapshot.MaxTotalValues;
	Config.MaxTotalDecodedBytes = Snapshot.MaxTotalDecodedBytes;
	Config.Record = &Snapshot.Record;
	Config.Read = __xrtXlonlRecordRead;
	return __xrtTextLinesRead(Text, &Config, &__xrtXlonlFormat, bValidate);
}

XRT_API xvalue* xrtXlonlRead(xstrview Text, const xxlonlreadconfig* pConfig)
{
	return __xrtXlonlRead(Text, pConfig, false);
}

XRT_API xvalue* xrtXlonlParse(xstrview Text)
{
	xxlonlreadconfig Config;
	xrtXlonlReadConfigInit(&Config);
	return xrtXlonlRead(Text, &Config);
}

XRT_API bool xrtXlonlValid(xstrview Text)
{
	xxlonlreadconfig Config;
	xvalue* pResult;
	xrtXlonlReadConfigInit(&Config);
	pResult = __xrtXlonlRead(Text, &Config, true);
	xrtValueRelease(pResult);
	return pResult != NULL;
}
#endif
