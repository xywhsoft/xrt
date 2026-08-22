#include "../test.h"



/* JSON 文件测试使用工作区内固定临时名称并在前后清理。 */
static cstr testJsonFilePath(void)
{
	return ".xrt-json-file-test.json";
}



/* 构建文件往返使用的简单对象。 */
static xvalue* testJsonFileValue(void)
{
	xvalue* pRoot = xrtValueObject();

	if (
		(pRoot == NULL) ||
		!xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("xrt"))
		) ||
		!xrtValueObjectSetNew(
			pRoot,
			XRT_STR_LITERAL("version"),
			xrtValueInt(2)
		)
	) {
		xrtValueRelease(pRoot);
		return NULL;
	}
	return pRoot;
}



/* 验证默认文件快捷函数的原子写入和读取往返。 */
static void testJsonFileRoundtrip(void)
{
	cstr sPath = testJsonFilePath();
	xvalue* pRoot = testJsonFileValue();
	xvalue* pRead;
	xstrview Name;
	int64 iVersion;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	testRequire(pRoot != NULL, "JSON file fixture failed");
	testRequire(
		xrtJsonStringifyFile(sPath, pRoot, true),
		"JSON file write failed"
	);
	pRead = xrtJsonParseFile(sPath);
	testRequire(pRead != NULL, "JSON file read failed");
	testRequire(
		xrtValueGetString(
			xrtValueObjectGet(pRead, XRT_STR_LITERAL("name")),
			&Name
		) &&
		(Name.Size == 3u) && (memcmp(Name.Data, "xrt", 3u) == 0) &&
		xrtValueGetInt(
			xrtValueObjectGet(pRead, XRT_STR_LITERAL("version")),
			&iVersion
		) &&
		(iVersion == 2),
		"JSON file roundtrip value mismatch"
	);
	xrtValueRelease(pRead);
	xrtValueRelease(pRoot);
	testRequire(xrtFileDelete(sPath), "JSON file cleanup failed");
}



/* 验证解析失败不会改写文件，输入限额和 I/O 原因链保持清晰。 */
static void testJsonFileErrors(void)
{
	cstr sPath = testJsonFilePath();
	xjsonreadconfig ReadConfig;
	xjsonwriteconfig WriteConfig;
	xvalue* pBytes;
	bytes pData;
	size_t iSize;
	const unsigned char arrBytes[] = { 1, 2 };
	const xerror* pError;

	(void)xrtFileDelete(sPath);
	testRequire(
		xrtFileWriteAll(sPath, XRT_BYTES_LITERAL("{\"ok\":true}")),
		"JSON error fixture write failed"
	);
	pBytes = xrtValueBytes(
		(xbytesview){ arrBytes, sizeof(arrBytes) }
	);
	testRequire(pBytes != NULL, "JSON unsupported file fixture failed");
	xrtJsonWriteConfigInit(&WriteConfig);
	testRequire(
		!xrtJsonWriteFile(sPath, pBytes, &WriteConfig),
		"unsupported JSON file write should fail before replacement"
	);
	pData = xrtFileReadAll(sPath, &iSize);
	testRequire(
		(pData != NULL) && (iSize == 11u) &&
		(memcmp(pData, "{\"ok\":true}", 11u) == 0),
		"failed JSON serialization changed target file"
	);
	xrtFree(pData);
	xrtValueRelease(pBytes);

	xrtJsonReadConfigInit(&ReadConfig);
	ReadConfig.MaxInputBytes = 4u;
	xrtClearError();
	testRequire(
		xrtJsonReadFile(sPath, &ReadConfig) == NULL,
		"JSON file input limit was not enforced"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorDomain(pError) != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.json") == 0) &&
		(xrtErrorCode(pError) == XJSON_ERROR_IO) &&
		(xrtErrorCause(pError) != NULL),
		"JSON file input limit cause chain mismatch"
	);

	testRequire(
		xrtFileWriteAll(sPath, XRT_BYTES_LITERAL("{\"a\":}")),
		"invalid JSON file fixture write failed"
	);
	xrtClearError();
	testRequire(
		xrtJsonParseFile(sPath) == NULL,
		"invalid JSON file was accepted"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) == XJSON_ERROR_SYNTAX),
		"JSON file parse error was obscured"
	);
	testRequire(xrtFileDelete(sPath), "JSON error fixture cleanup failed");

	xrtClearError();
	testRequire(
		xrtJsonParseFile(".xrt-json-file-missing.json") == NULL,
		"missing JSON file should fail"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) == XJSON_ERROR_IO) &&
		(xrtErrorCause(pError) != NULL),
		"missing JSON file cause chain mismatch"
	);
}



/* 运行 JSON 文件快捷路径和错误链测试。 */
int main(void)
{
	testJsonFileRoundtrip();
	testJsonFileErrors();
	printf("[PASS] JSON file\n");
	return 0;
}
