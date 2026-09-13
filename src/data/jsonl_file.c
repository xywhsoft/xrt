#include "../internal/xrt_jsonl.h"

#if defined(XRT_FEATURE_JSONL_FILE)

XRT_API xvalue* xrtJsonlReadFile(cstr sPath, const xjsonlreadconfig* pConfig)
{
	bytes pData;
	size_t iSize;
	xvalue* pValue;
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtJsonlReadConfigValid(pConfig) ) {
		return NULL;
	}
	pData = __xrtTextValueFileReadAll(sPath, pConfig->MaxInputBytes, &iSize,
		"xrt.jsonl", XJSONL_ERROR_IO, "failed to read JSONL file");
	if ( pData == NULL ) {
		return NULL;
	}
	pValue = xrtJsonlRead((xstrview){ (cstr)pData, iSize }, pConfig);
	xrtFree(pData);
	return pValue;
}

XRT_API xvalue* xrtJsonlParseFile(cstr sPath)
{
	xjsonlreadconfig Config;
	xrtJsonlReadConfigInit(&Config);
	return xrtJsonlReadFile(sPath, &Config);
}

XRT_API bool xrtJsonlWriteFile(
	cstr sPath, const xvalue* pArray, const xjsonlwriteconfig* pConfig
)
{
	str sText;
	size_t iSize;
	bool bResult;
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	sText = __xrtJsonlStringify(pArray, pConfig, &iSize);
	if ( sText == NULL ) {
		return false;
	}
	bResult = __xrtTextValueFileWriteAll(sPath, (xbytesview){ (cbytes)sText, iSize },
		"xrt.jsonl", XJSONL_ERROR_IO, "failed to write JSONL file");
	xrtFree(sText);
	return bResult;
}

XRT_API bool xrtJsonlStringifyFile(cstr sPath, const xvalue* pArray)
{
	xjsonlwriteconfig Config;
	xrtJsonlWriteConfigInit(&Config);
	return xrtJsonlWriteFile(sPath, pArray, &Config);
}
#endif

