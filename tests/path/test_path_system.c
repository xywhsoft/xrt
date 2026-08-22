#include "../test.h"



/* 系统路径查询必须返回可组合的绝对 UTF-8 路径。 */
int main(void)
{
	str sCwd = xrtPathCwd();
	str sAbs;
	str sEmptyAbs;
	str sReal;
	str sChild;
	str sRel;
	str sHome;
	str sTemp;
	str sExecutable;
	str sAppDir;
	str sExecutableParent;

	testRequire((sCwd != NULL) && xrtPathIsAbs(sCwd),
		"current directory is not absolute");
	testRequire(xrtPathSetCwd(sCwd),
		"setting the current directory to itself failed");
	sAbs = xrtPathAbs(".");
	testRequire((sAbs != NULL) && xrtPathIsAbs(sAbs),
		"absolute path resolution failed");
	sEmptyAbs = xrtPathAbs("");
	testRequire((sEmptyAbs != NULL) && (strcmp(sEmptyAbs, sCwd) == 0),
		"empty absolute path did not resolve to the current directory");
	sReal = xrtPathReal(".");
	testRequire((sReal != NULL) && xrtPathIsAbs(sReal),
		"physical path resolution failed");
	testRequire(xrtPathReal("xrt-path-system-missing-object") == NULL,
		"physical path resolution accepted a missing object");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
		"missing physical path reported the wrong error");
	xrtClearError();
	sChild = xrtPathJoin(sCwd, "child.txt");
	testRequire(sChild != NULL, "system path child join failed");
	sRel = xrtPathRel(sCwd, sChild);
	testRequire((sRel != NULL) && (strcmp(sRel, "child.txt") == 0),
		"system relative path failed");

	sHome = xrtPathHome();
	sTemp = xrtPathTemp();
	sExecutable = xrtPathExecutable();
	sAppDir = xrtPathAppDir();
	testRequire((sHome != NULL) && (sHome[0] != 0),
		"home directory query failed");
	testRequire((sTemp != NULL) && (sTemp[0] != 0),
		"temporary directory query failed");
	testRequire((sExecutable != NULL) && xrtPathIsAbs(sExecutable),
		"executable path query failed");
	testRequire((sAppDir != NULL) && xrtPathIsAbs(sAppDir),
		"application directory query failed");
	sExecutableParent = xrtPathParent(sExecutable);
	testRequire((sExecutableParent != NULL) &&
		(strcmp(sExecutableParent, sAppDir) == 0),
		"application directory does not match the executable parent");

	/* 必需路径参数必须统一拒绝空指针和无对象的物理路径。 */
	xrtClearError();
	testRequire(!xrtPathSetCwd(NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null current directory argument was accepted");
	xrtClearError();
	testRequire((xrtPathAbs(NULL) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null absolute path argument was accepted");
	xrtClearError();
	testRequire((xrtPathReal("") == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"empty physical path argument was accepted");

	xrtFree(sCwd);
	xrtFree(sAbs);
	xrtFree(sEmptyAbs);
	xrtFree(sReal);
	xrtFree(sChild);
	xrtFree(sRel);
	xrtFree(sHome);
	xrtFree(sTemp);
	xrtFree(sExecutable);
	xrtFree(sAppDir);
	xrtFree(sExecutableParent);
	return 0;
}
