#include "../test.h"



/* 临时文件必须在指定目录排他创建并返回完整拥有路径。 */
static void testFileTempCreate(void)
{
	str sDirectory = xrtPathTemp();
	str sPath = NULL;
	str sParent;
	str sName;
	xfileinfo Info;
	xfile File;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	File = xrtFileTemp(sDirectory, "xrt-file-temp-", ".dat", &sPath);
	testRequire((File != NULL) && (sPath != NULL) && xrtFileExists(sPath),
		"exclusive temporary file creation failed");
	sParent = xrtPathParent(sPath);
	sName = xrtPathName(sPath);
	testRequire((sParent != NULL) && (strcmp(sParent, sDirectory) == 0),
		"temporary file ignored its requested directory");
	testRequire((sName != NULL) &&
		(strncmp(sName, "xrt-file-temp-", 14u) == 0) &&
		(strlen(sName) == (14u + 16u + 4u)) &&
		(strcmp(sName + strlen(sName) - 4u, ".dat") == 0),
		"temporary file name has the wrong prefix, entropy, or suffix");
	testRequire(xrtFileStat(File, &Info) &&
		(Info.Type == XFILE_TYPE_FILE),
		"temporary file metadata is incorrect");
	#if !defined(_WIN32) && !defined(_WIN64)
		testRequire((Info.Available & XFILE_INFO_MODE) != 0u,
			"temporary file mode is unavailable");
		testRequire((Info.Mode & 0077u) == 0u,
			"temporary file grants group or other permissions");
	#endif
	testRequire(xrtWriteFull(File, "temp", 4u, NULL),
		"temporary file write failed");
	testRequire(xrtClose(File), "temporary file close failed");
	testRequire(xrtFileDelete(sPath), "temporary file cleanup failed");
	xrtFree(sName);
	xrtFree(sParent);
	xrtFree(sPath);
	xrtFree(sDirectory);
}



/* 临时文件必须拒绝路径注入、空目录和缺失路径输出。 */
static void testFileTempArguments(void)
{
	str sPath = (str)(uintptr_t)1u;

	testRequire(xrtFileTemp(NULL,
		"bad/name", ".tmp", &sPath) == NULL,
		"temporary file accepted a path separator in its prefix");
	testRequire((sPath == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid temporary prefix reported the wrong result");
	xrtClearError();

	sPath = (str)(uintptr_t)1u;
	testRequire(xrtFileTemp("", "name-", ".tmp", &sPath) == NULL,
		"temporary file accepted an empty directory");
	testRequire((sPath == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"empty temporary directory reported the wrong result");
	xrtClearError();

	testRequire(xrtFileTemp(NULL, NULL, NULL, NULL) == NULL,
		"temporary file accepted a missing path output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"missing temporary path output reported the wrong error");
	xrtClearError();

	#if defined(_WIN32) || defined(_WIN64)
		sPath = (str)(uintptr_t)1u;
		testRequire(xrtFileTemp(NULL,
			"stream:", ".tmp", &sPath) == NULL,
			"temporary file accepted a Windows stream separator");
		testRequire((sPath == NULL) && (xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"Windows temporary-name injection reported the wrong result");
		xrtClearError();
	#endif
}



/* 临时文件回归入口。 */
int main(void)
{
	testFileTempCreate();
	testFileTempArguments();
	return 0;
}
