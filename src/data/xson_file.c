#include "../internal/xrt_xson.h"



#if defined(XRT_FEATURE_XSON_FILE)

/* 使用高级配置限额读取并解析 XSON 文件。 */
XRT_API xvalue* xrtXsonReadFile(
	cstr sPath,
	const xxsonreadconfig* pConfig
)
{
	bytes pData;
	size_t iSize;
	xvalue* pValue;

	if ( (sPath == NULL) || !__xrtXsonReadConfigValid(pConfig) ) {
		if ( sPath == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pData = __xrtTextValueFileReadAll(
		sPath,
		pConfig->MaxInputBytes,
		&iSize,
		"xrt.xson",
		XXSON_ERROR_IO,
		"failed to read XSON file"
	);
	if ( pData == NULL ) {
		return NULL;
	}
	pValue = xrtXsonRead((xstrview){ (cstr)pData, iSize }, pConfig);
	xrtFree(pData);
	return pValue;
}



/* 使用默认严格配置读取并解析 XSON 文件。 */
XRT_API xvalue* xrtXsonParseFile(cstr sPath)
{
	xxsonreadconfig Config;

	xrtXsonReadConfigInit(&Config);
	return xrtXsonReadFile(sPath, &Config);
}



/* 使用高级配置完整序列化后原子替换 XSON 文件。 */
XRT_API bool xrtXsonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xxsonwriteconfig* pConfig
)
{
	xxsonwriter* pWriter;
	str sText;
	size_t iSize;
	bool bResult;

	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWriter = xrtXsonWriterCreate(pConfig);
	if ( pWriter == NULL ) {
		return false;
	}
	if (
		!xrtXsonWriterValue(pWriter, pValue) ||
		!xrtXsonWriterFinish(pWriter)
	) {
		xrtXsonWriterFree(pWriter);
		return false;
	}
	sText = xrtXsonWriterTake(pWriter, &iSize);
	xrtXsonWriterFree(pWriter);
	if ( sText == NULL ) {
		return false;
	}
	bResult = __xrtTextValueFileWriteAll(
		sPath,
		(xbytesview){ (cbytes)sText, iSize },
		"xrt.xson",
		XXSON_ERROR_IO,
		"failed to write XSON file"
	);
	xrtFree(sText);
	return bResult;
}



/* 紧凑或美化地序列化并原子替换 XSON 文件。 */
XRT_API bool xrtXsonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
)
{
	xxsonwriteconfig Config;

	xrtXsonWriteConfigInit(&Config);
	if ( bPretty ) {
		Config.Flags |= XXSON_WRITE_PRETTY;
	}
	return xrtXsonWriteFile(sPath, pValue, &Config);
}

#endif
