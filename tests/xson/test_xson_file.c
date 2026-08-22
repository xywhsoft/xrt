#include "../test.h"



/* XSON 文件测试使用工作区内固定临时名称并在前后清理。 */
static cstr testXsonFilePath(void)
{
	return ".xrt-xson-file-test.xson";
}



/* 构建包含二进制、时间和集合的文件往返值。 */
static xvalue* testXsonFileValue(void)
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
static void testXsonFileRoundtrip(void)
{
	cstr sPath = testXsonFilePath();
	xvalue* pRoot = testXsonFileValue();
	xvalue* pRead;
	xvalue* pSet;
	xvalue* pOne;
	xvalue* pText;
	xbytesview Data;
	xtime Time;
	xtime Expected;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	testRequire(pRoot != NULL, "XSON file fixture failed");
	testRequire(
		xrtXsonStringifyFile(sPath, pRoot, true),
		"XSON file write failed"
	);
	pRead = xrtXsonParseFile(sPath);
	testRequire(pRead != NULL, "XSON file read failed");
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
		"XSON file roundtrip value mismatch"
	);
	xrtValueRelease(pText);
	xrtValueRelease(pOne);
	xrtValueRelease(pRead);
	xrtValueRelease(pRoot);
	testRequire(xrtFileDelete(sPath), "XSON file cleanup failed");
}



/* 验证序列化失败不替换文件，限额和 I/O 原因链保持清晰。 */
static void testXsonFileErrors(void)
{
	cstr sPath = testXsonFilePath();
	xxsonreadconfig ReadConfig;
	xxsonwriteconfig WriteConfig;
	int iTarget = 1;
	xvalue* pPointer;
	bytes pData;
	size_t iSize;
	const xerror* pError;

	(void)xrtFileDelete(sPath);
	testRequire(
		xrtFileWriteAll(sPath, XRT_BYTES_LITERAL("{\"ok\":true}")),
		"XSON error fixture write failed"
	);
	pPointer = xrtValuePointer(&iTarget);
	testRequire(pPointer != NULL, "XSON unsupported file fixture failed");
	xrtXsonWriteConfigInit(&WriteConfig);
	testRequire(
		!xrtXsonWriteFile(sPath, pPointer, &WriteConfig),
		"unsupported XSON file write should fail before replacement"
	);
	pData = xrtFileReadAll(sPath, &iSize);
	testRequire(
		(pData != NULL) && (iSize == 11u) &&
		(memcmp(pData, "{\"ok\":true}", 11u) == 0),
		"failed XSON serialization changed target file"
	);
	xrtFree(pData);
	xrtValueRelease(pPointer);

	xrtXsonReadConfigInit(&ReadConfig);
	ReadConfig.MaxInputBytes = 4u;
	xrtClearError();
	testRequire(
		xrtXsonReadFile(sPath, &ReadConfig) == NULL,
		"XSON file input limit was not enforced"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.xson") == 0) &&
		(xrtErrorCode(pError) == XXSON_ERROR_IO) &&
		(xrtErrorCause(pError) != NULL),
		"XSON file input limit cause chain mismatch"
	);

	testRequire(
		xrtFileWriteAll(sPath, XRT_BYTES_LITERAL("set[1,]")),
		"invalid XSON file fixture write failed"
	);
	xrtClearError();
	testRequire(
		xrtXsonParseFile(sPath) == NULL,
		"invalid XSON file was accepted"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) == XXSON_ERROR_SYNTAX),
		"XSON file parse error was obscured"
	);
	testRequire(xrtFileDelete(sPath), "XSON error fixture cleanup failed");

	xrtClearError();
	testRequire(
		xrtXsonParseFile(".xrt-xson-file-missing.xson") == NULL,
		"missing XSON file should fail"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) == XXSON_ERROR_IO) &&
		(xrtErrorCause(pError) != NULL),
		"missing XSON file cause chain mismatch"
	);
}



/* 运行 XSON 文件快捷路径、原子替换和错误链测试。 */
int main(void)
{
	testXsonFileRoundtrip();
	testXsonFileErrors();
	printf("[PASS] XSON file\n");
	return 0;
}
