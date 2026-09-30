#include "../internal/xrt_xlonl.h"

#if defined(XRT_FEATURE_XLONL_FILE)

XRT_API xvalue* xrtXlonlReadFile(cstr sPath, const xxlonlreadconfig* pConfig)
{
	bytes pData;
	size_t iSize;
	xvalue* pValue;
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtXlonlReadConfigValid(pConfig) ) {
		return NULL;
	}
	pData = __xrtTextValueFileReadAll(sPath, pConfig->MaxInputBytes, &iSize,
		"xrt.xlonl", XXLONL_ERROR_IO, "failed to read XLONL file");
	if ( pData == NULL ) {
		return NULL;
	}
	pValue = xrtXlonlRead((xstrview){ (cstr)pData, iSize }, pConfig);
	xrtFree(pData);
	return pValue;
}

XRT_API xvalue* xrtXlonlParseFile(cstr sPath)
{
	xxlonlreadconfig Config;
	xrtXlonlReadConfigInit(&Config);
	return xrtXlonlReadFile(sPath, &Config);
}

XRT_API bool xrtXlonlWriteFile(
	cstr sPath, const xvalue* pArray, const xxlonlwriteconfig* pConfig
)
{
	str sText;
	size_t iSize;
	bool bResult;
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	sText = __xrtXlonlStringify(pArray, pConfig, &iSize);
	if ( sText == NULL ) {
		return false;
	}
	bResult = __xrtTextValueFileWriteAll(sPath, (xbytesview){ (cbytes)sText, iSize },
		"xrt.xlonl", XXLONL_ERROR_IO, "failed to write XLONL file");
	xrtFree(sText);
	return bResult;
}

XRT_API bool xrtXlonlStringifyFile(cstr sPath, const xvalue* pArray)
{
	xxlonlwriteconfig Config;
	xrtXlonlWriteConfigInit(&Config);
	return xrtXlonlWriteFile(sPath, pArray, &Config);
}
#endif
