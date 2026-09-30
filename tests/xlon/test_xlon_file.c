#include "../test.h"



/* XLON 文件测试使用工作区内固定临时名称并在前后清理。 */
static cstr testXlonFilePath(void)
{
	return ".xrt-xlon-file-test.xlon";
}



/* 构建包含二进制、时间和集合的文件往返值。 */
static xvalue* testXlonFileValue(void)
{
	static const uint8 arrBytes[] = { 0u, 1u, 255u };
	xvalue* pRoot = xrtValueObject();
	xvalue* pSet = xrtValueSet();
	xtime Time;
	bool bResult;

	if (
		(pRoot == NULL) || (pSet == NULL) ||
		!xrtTimeParseRFC3339(
			XRT_STR_LITERAL("2026-07-31T08:00:00Z"),
			&Time
		)
	) {
		xrtValueRelease(pSet);
		xrtValueRelease(pRoot);
		return NULL;
	}
	bResult =
		xrtValueSetAddNew(pSet, xrtValueInt(1)) &&
		xrtValueSetAddNew(
			pSet,
			xrtValueString(XRT_STR_LITERAL("x"))
		) &&
		xrtValueObjectSet(pRoot, XRT_STR_LITERAL("set"), pSet) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("blob"),
			xrtValueBytes((xbytesview){ arrBytes, sizeof(arrBytes) })
		) &&
		xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("time"),
			xrtValueTime(Time)
		);
	xrtValueRelease(pSet);
	if ( !bResult ) {
		xrtValueRelease(pRoot);
		return NULL;
	}
	return pRoot;
}



/* 验证默认文件快捷函数的原子写入和完整类型往返。 */
static void testXlonFileRoundtrip(void)
{
	cstr sPath = testXlonFilePath();
	xvalue* pRoot = testXlonFileValue();
	xvalue* pRead;
	xvalue* pSet;
	xvalue* pOne;
	xvalue* pText;
	xbytesview Data;
	xtime Time;
	xtime Expected;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	testRequire(pRoot != NULL, "XLON file fixture failed");
	testRequire(
		xrtXlonStringifyFile(sPath, pRoot, true),
		"XLON file write failed"
	);
	pRead = xrtXlonParseFile(sPath);
	testRequire(pRead != NULL, "XLON file read failed");
	pSet = xrtValueObjectGet(pRead, XRT_STR_LITERAL("set"));
	pOne = xrtValueInt(1);
	pText = xrtValueString(XRT_STR_LITERAL("x"));
	testRequire(
		(pSet != NULL) && (pOne != NULL) && (pText != NULL) &&
		xrtValueSetHas(pSet, pOne) &&
		xrtValueSetHas(pSet, pText) &&
		xrtValueGetBytes(
			xrtValueObjectGet(pRead, XRT_STR_LITERAL("blob")),
			&Data
		) &&
		(Data.Size == 3u) &&
		(Data.Data[0] == 0u) && (Data.Data[1] == 1u) &&
		(Data.Data[2] == 255u) &&
		xrtValueGetTime(
			xrtValueObjectGet(pRead, XRT_STR_LITERAL("time")),
			&Time
		) &&
		xrtTimeParseRFC3339(
			XRT_STR_LITERAL("2026-07-31T08:00:00Z"),
			&Expected
		) &&
		(Time == Expected),
		"XLON file roundtrip value mismatch"
	);
	xrtValueRelease(pText);
	xrtValueRelease(pOne);
	xrtValueRelease(pRead);
	xrtValueRelease(pRoot);
	testRequire(xrtFileDelete(sPath), "XLON file cleanup failed");
}



/* 验证序列化失败不替换文件，限额和 I/O 原因链保持清晰。 */
static void testXlonFileErrors(void)
{
	cstr sPath = testXlonFilePath();
	xxlonreadconfig ReadConfig;
	xxlonwriteconfig WriteConfig;
	int iTarget = 1;
	xvalue* pPointer;
	bytes pData;
	size_t iSize;
	const xerror* pError;

	(void)xrtFileDelete(sPath);
	testRequire(
		xrtFileWriteAll(sPath, XRT_BYTES_LITERAL("{\"ok\":true}")),
		"XLON error fixture write failed"
	);
	pPointer = xrtValuePointer(&iTarget);
	testRequire(pPointer != NULL, "XLON unsupported file fixture failed");
	xrtXlonWriteConfigInit(&WriteConfig);
	testRequire(
		!xrtXlonWriteFile(sPath, pPointer, &WriteConfig),
		"unsupported XLON file write should fail before replacement"
	);
	pData = xrtFileReadAll(sPath, &iSize);
	testRequire(
		(pData != NULL) && (iSize == 11u) &&
		(memcmp(pData, "{\"ok\":true}", 11u) == 0),
		"failed XLON serialization changed target file"
	);
	xrtFree(pData);
	xrtValueRelease(pPointer);

	xrtXlonReadConfigInit(&ReadConfig);
	ReadConfig.MaxInputBytes = 4u;
	xrtClearError();
	testRequire(
		xrtXlonReadFile(sPath, &ReadConfig) == NULL,
		"XLON file input limit was not enforced"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.xlon") == 0) &&
		(xrtErrorCode(pError) == XXLON_ERROR_IO) &&
		(xrtErrorCause(pError) != NULL),
		"XLON file input limit cause chain mismatch"
	);

	testRequire(
		xrtFileWriteAll(sPath, XRT_BYTES_LITERAL("set[1,]")),
		"invalid XLON file fixture write failed"
	);
	xrtClearError();
	testRequire(
		xrtXlonParseFile(sPath) == NULL,
		"invalid XLON file was accepted"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) == XXLON_ERROR_SYNTAX),
		"XLON file parse error was obscured"
	);
	testRequire(xrtFileDelete(sPath), "XLON error fixture cleanup failed");

	xrtClearError();
	testRequire(
		xrtXlonParseFile(".xrt-xlon-file-missing.xlon") == NULL,
		"missing XLON file should fail"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) == XXLON_ERROR_IO) &&
		(xrtErrorCause(pError) != NULL),
		"missing XLON file cause chain mismatch"
	);
}



/* 运行 XLON 文件快捷路径、原子替换和错误链测试。 */
int main(void)
{
	testXlonFileRoundtrip();
	testXlonFileErrors();
	printf("[PASS] XLON file\n");
	return 0;
}
