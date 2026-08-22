#include "../test.h"



/* 在系统临时目录构造链接测试路径。 */
static str testLinkPath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "link test path allocation failed");
	return sPath;
}



/* 使用基础文件 API 创建链接目标。 */
static void testLinkWrite(cstr sPath, cstr sText)
{
	xfile File = xrtOpen(sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);

	testRequire(File != NULL, "link target open failed");
	testRequire(xrtWriteFull(File, sText, strlen(sText), NULL),
		"link target write failed");
	testRequire(xrtClose(File), "link target close failed");
}



/* 硬链接必须共享对象身份和内容，但不是符号链接。 */
static void testHardLink(cstr sSource, cstr sHard)
{
	xfileinfo SourceInfo;
	xfileinfo HardInfo;
	xfile File;
	char arrData[7];

	if ( !xrtLinkHard(sSource, sHard) ) {
		#if defined(__ANDROID__)
			testRequire((xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
				"Android hard link failure lost its permission error");
			xrtClearError();
			return;
		#else
			testRequire(false, "hard link creation failed");
		#endif
	}
	testRequire(xrtPathStat(sSource, true, &SourceInfo) &&
		xrtPathStat(sHard, true, &HardInfo), "hard link metadata query failed");
	testRequire((SourceInfo.Device == HardInfo.Device) &&
		(SourceInfo.Identity == HardInfo.Identity) &&
		(SourceInfo.LinkCount >= 2u) && (HardInfo.LinkCount >= 2u),
		"hard link does not share file identity");
	testRequire(xrtLinkRead(sHard) == NULL,
		"hard link was accepted as a symbolic link");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"hard link read reported the wrong error");
	xrtClearError();

	File = xrtOpen(sHard, XFILE_READ);
	testRequire(File != NULL, "hard link read open failed");
	testRequire(xrtReadFull(File, arrData, sizeof(arrData), NULL) &&
		(memcmp(arrData, "content", sizeof(arrData)) == 0),
		"hard link content mismatch");
	testRequire(xrtClose(File), "hard link read close failed");
	testRequire(xrtFileDelete(sHard) && xrtFileExists(sSource),
		"hard link deletion affected the source name");
}



/* 符号链接必须保留存储目标并在删除时不跟随目标。 */
static void testSymbolicLink(cstr sSource, cstr sLink)
{
	str sName = xrtPathName(sSource);
	str sTarget;
	xfileinfo LinkInfo;
	xfileinfo TargetInfo;
	bool bCreated;

	testRequire(sName != NULL, "symbolic link target name allocation failed");
	bCreated = xrtLinkCreate(sName, sLink, false);
	if ( !bCreated ) {
		#if defined(_WIN32) || defined(_WIN64)
			testRequire((xrtGetError() != NULL) &&
				((xrtErrorKind(xrtGetError()) == XERR_PERMISSION) ||
				 (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED)),
				"symbolic link creation failed for an unexpected reason");
			xrtClearError();
			xrtFree(sName);
			return;
		#else
			testRequire(false, "symbolic link creation failed");
		#endif
	}
	sTarget = xrtLinkRead(sLink);
	testRequire((sTarget != NULL) && (strcmp(sTarget, sName) == 0),
		"symbolic link target text mismatch");
	testRequire(xrtPathStat(sLink, false, &LinkInfo) &&
		(LinkInfo.Type == XFILE_TYPE_LINK),
		"no-follow metadata lost symbolic link identity");
	testRequire(xrtPathStat(sLink, true, &LinkInfo) &&
		xrtPathStat(sSource, true, &TargetInfo) &&
		(LinkInfo.Device == TargetInfo.Device) &&
		(LinkInfo.Identity == TargetInfo.Identity),
		"follow metadata did not resolve the symbolic link target");
	testRequire(xrtLinkDelete(sLink) && !xrtPathExists(sLink) &&
		xrtFileExists(sSource), "symbolic link deletion affected its target");
	xrtFree(sTarget);
	xrtFree(sName);
}



/* 悬空链接仍必须可读取和删除，跟随查询则明确失败。 */
static void testDanglingLink(cstr sLink)
{
	static const char sMissing[] = "xrt-link-target-does-not-exist";
	str sTarget;
	bool bCreated = xrtLinkCreate(sMissing, sLink, false);

	if ( !bCreated ) {
		#if defined(_WIN32) || defined(_WIN64)
			xrtClearError();
			return;
		#else
			testRequire(false, "dangling link creation failed");
		#endif
	}
	sTarget = xrtLinkRead(sLink);
	testRequire((sTarget != NULL) && (strcmp(sTarget, sMissing) == 0),
		"dangling link target text mismatch");
	testRequire(!xrtFileExists(sLink),
		"dangling symbolic link was reported as a file");
	testRequire(xrtPathExists(sLink),
		"no-follow existence check lost a dangling symbolic link");
	testRequire(xrtLinkDelete(sLink), "dangling link deletion failed");
	xrtFree(sTarget);
}



/* 文件链接层回归入口。 */
int main(void)
{
	str sSource = testLinkPath("xrt-link-source.tmp");
	str sHard = testLinkPath("xrt-link-hard.tmp");
	str sSymbolic = testLinkPath("xrt-link-symbolic.tmp");

	(void)xrtLinkDelete(sSymbolic);
	(void)xrtFileDelete(sSymbolic);
	(void)xrtFileDelete(sHard);
	(void)xrtFileDelete(sSource);
	xrtClearError();
	testLinkWrite(sSource, "content");
	testHardLink(sSource, sHard);
	testSymbolicLink(sSource, sSymbolic);
	testDanglingLink(sSymbolic);
	testRequire(xrtFileDelete(sSource), "link fixture cleanup failed");
	xrtFree(sSymbolic);
	xrtFree(sHard);
	xrtFree(sSource);
	return 0;
}
