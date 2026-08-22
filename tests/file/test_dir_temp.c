#include "../test.h"



/* 临时目录必须排他创建、保持私有权限并返回拥有路径。 */
static void testDirTempCreate(void)
{
	str sFirst = xrtDirTemp(NULL, NULL, NULL);
	str sParent;
	str sSecond;
	str sName;
	xfileinfo Info;

	testRequire((sFirst != NULL) && xrtDirExists(sFirst),
		"default temporary directory creation failed");
	sParent = xrtPathParent(sFirst);
	testRequire(sParent != NULL, "temporary directory parent query failed");
	sSecond = xrtDirTemp(sParent, "xrt-dir-temp-", ".work");
	testRequire((sSecond != NULL) && xrtDirExists(sSecond) &&
		(strcmp(sFirst, sSecond) != 0),
		"custom temporary directory creation failed");
	sName = xrtPathName(sSecond);
	testRequire((sName != NULL) &&
		(strncmp(sName, "xrt-dir-temp-", 13u) == 0) &&
		(strlen(sName) == (13u + 16u + 5u)) &&
		(strcmp(sName + strlen(sName) - 5u, ".work") == 0),
		"temporary directory name has the wrong shape");
	testRequire(xrtPathStat(sSecond, false, &Info) &&
		(Info.Type == XFILE_TYPE_DIRECTORY),
		"temporary directory metadata is incorrect");
	#if !defined(_WIN32) && !defined(_WIN64)
		testRequire((Info.Available & XFILE_INFO_MODE) != 0u,
			"temporary directory mode is unavailable");
		testRequire((Info.Mode & 0077u) == 0u,
			"temporary directory grants group or other permissions");
	#endif
	testRequire(xrtDirRemove(sSecond) && xrtDirRemove(sFirst),
		"temporary directory cleanup failed");
	xrtFree(sName);
	xrtFree(sSecond);
	xrtFree(sParent);
	xrtFree(sFirst);
}



/* 临时目录必须拒绝路径片段注入和含糊的空目录。 */
static void testDirTempArguments(void)
{
	testRequire(xrtDirTemp(NULL, "bad/name", NULL) == NULL,
		"temporary directory accepted a path separator");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid temporary directory prefix reported the wrong error");
	xrtClearError();

	testRequire(xrtDirTemp("", "name-", NULL) == NULL,
		"temporary directory accepted an empty parent");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"empty temporary parent reported the wrong error");
	xrtClearError();
}



/* 临时目录回归入口。 */
int main(void)
{
	testDirTempCreate();
	testDirTempArguments();
	return 0;
}
