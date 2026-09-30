#include "../internal/xrt_xlon.h"



#if defined(XRT_FEATURE_XLON_FILE)

/* 使用高级配置限额读取并解析 XLON 文件。 */
XRT_API xvalue* xrtXlonReadFile(
	cstr sPath,
	const xxlonreadconfig* pConfig
)
{
	bytes pData;
	size_t iSize;
	xvalue* pValue;

	if ( (sPath == NULL) || !__xrtXlonReadConfigValid(pConfig) ) {
		if ( sPath == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pData = __xrtTextValueFileReadAll(
		sPath,
		pConfig->MaxInputBytes,
		&iSize,
		"xrt.xlon",
		XXLON_ERROR_IO,
		"failed to read XLON file"
	);
	if ( pData == NULL ) {
		return NULL;
	}
	pValue = xrtXlonRead((xstrview){ (cstr)pData, iSize }, pConfig);
	xrtFree(pData);
	return pValue;
}



/* 使用默认严格配置读取并解析 XLON 文件。 */
XRT_API xvalue* xrtXlonParseFile(cstr sPath)
{
	xxlonreadconfig Config;

	xrtXlonReadConfigInit(&Config);
	return xrtXlonReadFile(sPath, &Config);
}



/* 使用高级配置完整序列化后原子替换 XLON 文件。 */
XRT_API bool xrtXlonWriteFile(
	cstr sPath,
	const xvalue* pValue,
	const xxlonwriteconfig* pConfig
)
{
	xxlonwriter* pWriter;
	str sText;
	size_t iSize;
	bool bResult;

	if ( sPath == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWriter = xrtXlonWriterCreate(pConfig);
	if ( pWriter == NULL ) {
		return false;
	}
	if (
		!xrtXlonWriterValue(pWriter, pValue) ||
		!xrtXlonWriterFinish(pWriter)
	) {
		xrtXlonWriterFree(pWriter);
		return false;
	}
	sText = xrtXlonWriterTake(pWriter, &iSize);
	xrtXlonWriterFree(pWriter);
	if ( sText == NULL ) {
		return false;
	}
	bResult = __xrtTextValueFileWriteAll(
		sPath,
		(xbytesview){ (cbytes)sText, iSize },
		"xrt.xlon",
		XXLON_ERROR_IO,
		"failed to write XLON file"
	);
	xrtFree(sText);
	return bResult;
}



/* 紧凑或美化地序列化并原子替换 XLON 文件。 */
XRT_API bool xrtXlonStringifyFile(
	cstr sPath,
	const xvalue* pValue,
	bool bPretty
)
{
	xxlonwriteconfig Config;

	xrtXlonWriteConfigInit(&Config);
	if ( bPretty ) {
		Config.Flags |= XXLON_WRITE_PRETTY;
	}
	return xrtXlonWriteFile(sPath, pValue, &Config);
}

#endif
