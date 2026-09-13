#include "../internal/xrt_jsonl.h"

#if defined(XRT_FEATURE_JSONL_READ)

/* 即使输入为空，也完整验证配置和底层记录配置。 */
bool __xrtJsonlReadConfigValid(const xjsonlreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pConfig->Flags & ~XJSONL_READ_REJECT_EMPTY_LINES) != 0) ||
		 (pConfig->MaxInputBytes == 0) || (pConfig->MaxRecords == 0) ||
		 (pConfig->MaxTotalValues == 0) ||
		 (pConfig->Reserved[0] != 0) || (pConfig->Reserved[1] != 0) ||
		 (pConfig->Reserved[2] != 0) || (pConfig->Reserved[3] != 0) ) {
		__xrtTextLinesError(&__xrtJsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"read", "invalid JSONL read configuration", NULL, false);
		return false;
	}
	if ( !__xrtJsonReadConfigValid(&pConfig->Record) ) {
		__xrtTextLinesError(&__xrtJsonlFormat, XTEXT_LINES_CONFIG, XERR_ARGUMENT,
			"read", "invalid record read configuration", NULL, true);
		return false;
	}
	return true;
}

XRT_API void xrtJsonlReadConfigInit(xjsonlreadconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtJsonReadConfigInit(&pConfig->Record);
	pConfig->MaxInputBytes = XJSON_INPUT_DEFAULT;
	pConfig->MaxRecords = XJSON_CONTAINER_DEFAULT;
	pConfig->MaxTotalValues = XJSON_VALUES_DEFAULT;
}

static xvalue* __xrtJsonlRecordRead(
	xstrview Text, const void* pConfig, xtextvaluebudget* pBudget, bool bValidate
)
{
	return __xrtJsonReadBudget(Text, (const xjsonreadconfig*)pConfig, pBudget, bValidate);
}

static xvalue* __xrtJsonlRead(
	xstrview Text, const xjsonlreadconfig* pConfig, bool bValidate
)
{
	xjsonlreadconfig Snapshot;
	xtextlinesreadconfig Config;
	if ( !__xrtJsonlReadConfigValid(pConfig) ) {
		return NULL;
	}
	Snapshot = *pConfig;
	memset(&Config, 0, sizeof(Config));
	Config.RejectEmpty = (Snapshot.Flags & XJSONL_READ_REJECT_EMPTY_LINES) != 0;
	Config.MaxInputBytes = Snapshot.MaxInputBytes;
	Config.MaxLineBytes = Snapshot.Record.MaxInputBytes;
	Config.MaxRecords = Snapshot.MaxRecords;
	Config.MaxTotalValues = Snapshot.MaxTotalValues;
	Config.MaxTotalDecodedBytes = SIZE_MAX;
	Config.Record = &Snapshot.Record;
	Config.Read = __xrtJsonlRecordRead;
	return __xrtTextLinesRead(Text, &Config, &__xrtJsonlFormat, bValidate);
}

XRT_API xvalue* xrtJsonlRead(xstrview Text, const xjsonlreadconfig* pConfig)
{
	return __xrtJsonlRead(Text, pConfig, false);
}

XRT_API xvalue* xrtJsonlParse(xstrview Text)
{
	xjsonlreadconfig Config;
	xrtJsonlReadConfigInit(&Config);
	return xrtJsonlRead(Text, &Config);
}

XRT_API bool xrtJsonlValid(xstrview Text)
{
	xjsonlreadconfig Config;
	xvalue* pResult;
	xrtJsonlReadConfigInit(&Config);
	pResult = __xrtJsonlRead(Text, &Config, true);
	xrtValueRelease(pResult);
	return pResult != NULL;
}
#endif

