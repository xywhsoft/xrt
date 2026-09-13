#include "../internal/xrt_xsonl.h"

#if defined(XRT_FEATURE_XSONL_FILE)

XRT_API xvalue* xrtXsonlReadFile(cstr sPath, const xxsonlreadconfig* pConfig)
{
	bytes pData;
	size_t iSize;
	xvalue* pValue;
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtXsonlReadConfigValid(pConfig) ) {
		return NULL;
	}
	pData = __xrtTextValueFileReadAll(sPath, pConfig->MaxInputBytes, &iSize,
		"xrt.xsonl", XXSONL_ERROR_IO, "failed to read XSONL file");
	if ( pData == NULL ) {
		return NULL;
	}
	pValue = xrtXsonlRead((xstrview){ (cstr)pData, iSize }, pConfig);
	xrtFree(pData);
	return pValue;
}

XRT_API xvalue* xrtXsonlParseFile(cstr sPath)
{
	xxsonlreadconfig Config;
	xrtXsonlReadConfigInit(&Config);
	return xrtXsonlReadFile(sPath, &Config);
}

XRT_API bool xrtXsonlWriteFile(
	cstr sPath, const xvalue* pArray, const xxsonlwriteconfig* pConfig
)
{
	str sText;
	size_t iSize;
	bool bResult;
	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	sText = __xrtXsonlStringify(pArray, pConfig, &iSize);
	if ( sText == NULL ) {
		return false;
	}
	bResult = __xrtTextValueFileWriteAll(sPath, (xbytesview){ (cbytes)sText, iSize },
		"xrt.xsonl", XXSONL_ERROR_IO, "failed to write XSONL file");
	xrtFree(sText);
	return bResult;
}

XRT_API bool xrtXsonlStringifyFile(cstr sPath, const xvalue* pArray)
{
	xxsonlwriteconfig Config;
	xrtXsonlWriteConfigInit(&Config);
	return xrtXsonlWriteFile(sPath, pArray, &Config);
}
#endif

