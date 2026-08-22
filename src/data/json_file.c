#include "../internal/xrt_json.h"



#if defined(XRT_FEATURE_JSON_FILE)

/* 使用高级配置限额读取并解析 JSON 文件。 */
XRT_API xvalue* xrtJsonReadFile(
	cstr sPath,
	const xjsonreadconfig* pConfig
)
{
	bytes pData;
	size_t iSize;
	xvalue* pValue;

	if ( (sPath == NULL) || !__xrtJsonReadConfigValid(pConfig) ) {
		if ( sPath == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pData = __xrtTextValueFileReadAll(
		sPath,
		pConfig->MaxInputBytes,
		&iSize,
		"xrt.json",
		XJSON_ERROR_IO,
		"failed to read JSON file"
	);
	if ( pData == NULL ) {
		return NULL;
	}
	pValue = xrtJsonRead((xstrview){ (cstr)pData, iSize }, pConfig);
	xrtFree(pData);
	return pValue;
}



/* 使用默认严格配置读取并解析 JSON 文件。 */
XRT_API xvalue* xrtJsonParseFile(cstr sPath)
{
	xjsonreadconfig Config;

	xrtJsonReadConfigInit(&Config);
	return xrtJsonReadFile(sPath, &Config);
}



/* 使用高级配置完整序列化后原子替换 JSON 文件。 */
XRT_API bool xrtJsonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xjsonwriteconfig* pConfig
)
{
	xjsonwriter* pWriter;
	str sText;
	size_t iSize;
	bool bResult;

	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWriter = xrtJsonWriterCreate(pConfig);
	if ( pWriter == NULL ) {
		return false;
	}
	if ( !xrtJsonWriterValue(pWriter, pValue) ||
		 !xrtJsonWriterFinish(pWriter) ) {
		xrtJsonWriterFree(pWriter);
		return false;
	}
	sText = xrtJsonWriterTake(pWriter, &iSize);
	xrtJsonWriterFree(pWriter);
	if ( sText == NULL ) {
		return false;
	}
	bResult = __xrtTextValueFileWriteAll(
		sPath,
		(xbytesview){ (cbytes)sText, iSize },
		"xrt.json",
		XJSON_ERROR_IO,
		"failed to write JSON file"
	);
	xrtFree(sText);
	return bResult;
}



/* 紧凑或美化地序列化并原子替换 JSON 文件。 */
XRT_API bool xrtJsonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
)
{
	xjsonwriteconfig Config;

	xrtJsonWriteConfigInit(&Config);
	if ( bPretty ) {
		Config.Flags |= XJSON_WRITE_PRETTY;
	}
	return xrtJsonWriteFile(sPath, pValue, &Config);
}

#endif
