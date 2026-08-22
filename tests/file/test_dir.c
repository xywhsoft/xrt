#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../test.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <unistd.h>
#endif



/* 在系统临时目录构造目录测试路径。 */
static str testDirPath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "directory test path allocation failed");
	return sPath;
}



/* 使用基础文件 API 创建一个小型目录条目。 */
static void testDirWrite(cstr sPath, cstr sText)
{
	xfile File = xrtOpen(sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);

	testRequire(File != NULL, "directory fixture file open failed");
	testRequire(xrtWriteFull(File, sText, strlen(sText), NULL),
		"directory fixture file write failed");
	testRequire(xrtClose(File), "directory fixture file close failed");
}



/* 创建多级目录必须容忍已有目录并拒绝文件占据中间组件。 */
static void testDirCreate(cstr sRoot, cstr sNested,
	cstr sBlocked, cstr sBlockedChild)
{
	testRequire(xrtDirCreateAll(sNested), "recursive directory create failed");
	testRequire(xrtDirExists(sRoot) && xrtDirExists(sNested),
		"recursive directory create omitted a component");
	testRequire(xrtDirCreateAll(sNested),
		"recursive directory create rejected an existing tree");
	testRequire(!xrtDirCreate(sNested),
		"single directory create accepted an existing directory");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
		"existing directory create reported the wrong error");
	xrtClearError();

	testDirWrite(sBlocked, "file");
	testRequire(!xrtDirCreateAll(sBlockedChild),
		"recursive create crossed a file component");
	testRequire(xrtFileExists(sBlocked) && !xrtDirExists(sBlockedChild),
		"failed recursive create damaged the file component");
	xrtClearError();
}



/* 高性能迭代必须跳过点条目并返回可直接拼接的借用名称。 */
static void testDirIterate(cstr sRoot, cstr sFile, cstr sNested)
{
	xdir Dir = xrtDirOpen(sRoot, 0u);
	xdirentry Entry;
	xdirentry Saved;
	xdirnext Next;
	bool bFile = false;
	bool bNested = false;
	size_t iCount = 0;

	testRequire(Dir != NULL, "directory iterator open failed");
	testRequire(strcmp(xrtDirPath(Dir), sRoot) == 0,
		"directory iterator lost its root path");
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		str sEntryPath;

		testRequire(!xrtStrEqual(Entry.Name, XRT_STR_LITERAL(".")) &&
			!xrtStrEqual(Entry.Name, XRT_STR_LITERAL("..")),
			"default directory iteration exposed dot entries");
		testRequire((Entry.Flags & XDIR_ENTRY_UTF8) != 0u,
			"UTF-8 fixture name was not marked as UTF-8");
		sEntryPath = xrtDirEntryPath(Dir, &Entry);
		testRequire(sEntryPath != NULL, "directory entry path build failed");
		if ( strcmp(sEntryPath, sFile) == 0 ) {
			bFile = true;
		}
		if ( strcmp(sEntryPath, sNested) == 0 ) {
			bNested = true;
		}
		xrtFree(sEntryPath);
		iCount++;
	}
	testRequire((Next == XDIR_NEXT_END) && (iCount == 4u) && bFile && bNested,
		"directory iterator returned the wrong entries");
	memset(&Entry, 0xA5, sizeof(Entry));
	Saved = Entry;
	testRequire(xrtDirNext(Dir, &Entry) == XDIR_NEXT_END,
		"completed directory iterator did not stay at end");
	testRequire(memcmp(&Entry, &Saved, sizeof(Entry)) == 0,
		"directory end modified the entry output");
	testRequire(xrtDirClose(Dir), "directory iterator close failed");
}



/* 完整元数据模式必须报告稳定类型、大小和时间可用位。 */
static void testDirStat(cstr sRoot)
{
	xdir Dir = xrtDirOpen(sRoot, XDIR_STAT);
	xdirentry Entry;
	xdirnext Next;
	bool bSawFile = false;
	bool bSawDirectory = false;

	testRequire(Dir != NULL, "stat directory iterator open failed");
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		if ( Entry.Info.Type == XFILE_TYPE_FILE ) {
			bSawFile = true;
			testRequire(((Entry.Info.Available & XFILE_INFO_SIZE) != 0u) &&
				(Entry.Info.Size != 0u), "file entry metadata is incomplete");
		}
		if ( Entry.Info.Type == XFILE_TYPE_DIRECTORY ) {
			bSawDirectory = true;
		}
	}
	testRequire((Next == XDIR_NEXT_END) && bSawFile && bSawDirectory,
		"stat directory iteration lost entry types");
	testRequire(xrtDirClose(Dir), "stat directory iterator close failed");
}



#if !defined(_WIN32) && !defined(_WIN64)

/* POSIX 原始字节名称和链接元数据必须保持可表达。 */
static void testDirPosixEntries(cstr sRoot)
{
	static const char arrRawName[] = {
		'b', 'a', 'd', '-', (char)0xFF, '\0'
	};
	str sRawPath = xrtPathJoin(sRoot, arrRawName);
	str sLinkPath = xrtPathJoin(sRoot, "nested-link");
	xdir Dir;
	xdirentry Entry;
	xdirnext Next;
	bool bRaw = false;
	bool bLink = false;

	testRequire((sRawPath != NULL) && (sLinkPath != NULL),
		"POSIX directory fixture path build failed");
	testDirWrite(sRawPath, "raw");
	testRequire(symlink("nested", sLinkPath) == 0,
		"POSIX directory symlink fixture create failed");

	Dir = xrtDirOpen(sRoot, XDIR_STAT);
	testRequire(Dir != NULL, "POSIX no-follow directory iterator open failed");
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		if ( (Entry.Name.Size == (sizeof(arrRawName) - 1u)) &&
			 (memcmp(Entry.Name.Data, arrRawName,
			 sizeof(arrRawName) - 1u) == 0) ) {
			bRaw = true;
			testRequire((Entry.Flags & XDIR_ENTRY_UTF8) == 0u,
				"invalid POSIX filename was marked as UTF-8");
		}
		if ( xrtStrEqual(Entry.Name, XRT_STR_LITERAL("nested-link")) ) {
			bLink = true;
			testRequire(Entry.Info.Type == XFILE_TYPE_LINK,
				"no-follow directory stat lost the link type");
		}
	}
	testRequire((Next == XDIR_NEXT_END) && bRaw && bLink,
		"POSIX directory iterator lost a raw name or link");
	testRequire(xrtDirClose(Dir), "POSIX no-follow iterator close failed");

	Dir = xrtDirOpen(sRoot, XDIR_STAT | XDIR_FOLLOW_LINKS);
	testRequire(Dir != NULL, "POSIX follow directory iterator open failed");
	bLink = false;
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		if ( xrtStrEqual(Entry.Name, XRT_STR_LITERAL("nested-link")) ) {
			bLink = true;
			testRequire(Entry.Info.Type == XFILE_TYPE_DIRECTORY,
				"follow directory stat did not expose the link target");
		}
	}
	testRequire((Next == XDIR_NEXT_END) && bLink,
		"follow directory iterator lost the link entry");
	testRequire(xrtDirClose(Dir), "POSIX follow iterator close failed");
	testRequire(xrtFileDelete(sLinkPath) && xrtFileDelete(sRawPath),
		"POSIX directory fixture cleanup failed");
	xrtFree(sLinkPath);
	xrtFree(sRawPath);
}

#endif



/* 参数和失败路径必须给出稳定错误且保持输出不变。 */
static void testDirErrors(cstr sRoot, cstr sFile, cstr sMissing)
{
	xdirentry Entry;
	xdirentry Saved;
	bool bEmpty = true;

	testRequire(xrtDirOpen(sRoot, XDIR_FOLLOW_LINKS) == NULL,
		"directory iterator accepted follow without stat");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid directory flags reported the wrong error");
	xrtClearError();
	testRequire(xrtDirOpen(sRoot, 0x80000000u) == NULL,
		"directory iterator accepted unknown flags");
	xrtClearError();
	testRequire(xrtDirOpen(sFile, 0u) == NULL,
		"directory iterator accepted a regular file");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"regular-file directory open reported the wrong error");
	xrtClearError();

	memset(&Entry, 0xA5, sizeof(Entry));
	Saved = Entry;
	testRequire(xrtDirNext(NULL, &Entry) == XDIR_NEXT_ERROR,
		"null directory iterator did not fail");
	testRequire(memcmp(&Entry, &Saved, sizeof(Entry)) == 0,
		"failed directory next modified its output");
	xrtClearError();
	testRequire(!xrtDirEmpty(sMissing, &bEmpty) && bEmpty,
		"failed empty query modified its output");
	xrtClearError();
	testRequire(!xrtDirRoots(NULL), "null root-list output was accepted");
	xrtClearError();
}



/* 可选点条目、空目录查询和空目录删除必须保持明确语义。 */
static void testDirDotsAndEmpty(cstr sNested)
{
	xdir Dir;
	xdirentry Entry;
	xdirnext Next;
	bool bDot = false;
	bool bDotDot = false;
	bool bEmpty = false;

	testRequire(xrtDirEmpty(sNested, &bEmpty) && bEmpty,
		"new nested directory is not empty");
	Dir = xrtDirOpen(sNested, XDIR_INCLUDE_DOTS);
	testRequire(Dir != NULL, "dot-entry iterator open failed");
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		bDot = bDot || xrtStrEqual(Entry.Name, XRT_STR_LITERAL("."));
		bDotDot = bDotDot || xrtStrEqual(Entry.Name, XRT_STR_LITERAL(".."));
	}
	testRequire((Next == XDIR_NEXT_END) && bDot && bDotDot,
		"dot-entry iteration did not expose both dot entries");
	testRequire(xrtDirClose(Dir), "dot-entry iterator close failed");
	testRequire(xrtDirRemove(sNested), "empty directory removal failed");
	testRequire(!xrtDirExists(sNested), "removed directory still exists");
}



/* 系统根目录查询必须返回拥有的绝对根路径。 */
static void testDirRootList(void)
{
	xdirroots Roots;
	size_t i;

	memset(&Roots, 0, sizeof(Roots));
	testRequire(xrtDirRoots(&Roots) && (Roots.Count != 0u),
		"system root list is empty");
	for ( i = 0; i < Roots.Count; i++ ) {
		testRequire((Roots.Items[i] != NULL) && xrtPathIsAbs(Roots.Items[i]),
			"system root is not an owned absolute path");
	}
	xrtDirRootsFree(&Roots);
	testRequire((Roots.Items == NULL) && (Roots.Count == 0u),
		"system root list free did not clear the object");
}



/* 目录基础层回归入口。 */
int main(void)
{
	str sRoot = testDirPath("xrt-dir-base");
	str sNested = xrtPathJoin(sRoot, "nested");
	str sFile = xrtPathJoin(sRoot, "alpha.txt");
	str sUtf8 = xrtPathJoin(sRoot, "\xE8\xBE\xB9\xE7\x95\x8C.txt");
	str sBlocked = xrtPathJoin(sRoot, "blocked");
	str sBlockedChild = xrtPathJoin(sBlocked, "child");
	str sMissing = xrtPathJoin(sRoot, "missing");

	testRequire((sNested != NULL) && (sFile != NULL) && (sUtf8 != NULL) &&
		(sBlocked != NULL) && (sBlockedChild != NULL) && (sMissing != NULL),
		"directory fixture path build failed");
	(void)xrtFileDelete(sFile);
	(void)xrtFileDelete(sUtf8);
	(void)xrtFileDelete(sBlocked);
	(void)xrtDirRemove(sNested);
	(void)xrtDirRemove(sRoot);
	xrtClearError();
	testDirCreate(sRoot, sNested, sBlocked, sBlockedChild);
	testDirWrite(sFile, "alpha");
	testDirWrite(sUtf8, "utf8");
	testDirIterate(sRoot, sFile, sNested);
	testDirStat(sRoot);
	testDirErrors(sRoot, sFile, sMissing);
	#if !defined(_WIN32) && !defined(_WIN64)
		testDirPosixEntries(sRoot);
	#endif
	testDirDotsAndEmpty(sNested);
	testDirRootList();
	testRequire(!xrtDirRemove(sRoot),
		"non-empty directory removal unexpectedly succeeded");
	xrtClearError();
	testRequire(xrtFileDelete(sFile) && xrtFileDelete(sUtf8) &&
		xrtFileDelete(sBlocked) && xrtDirRemove(sRoot),
		"directory fixture cleanup failed");
	xrtFree(sBlockedChild);
	xrtFree(sBlocked);
	xrtFree(sMissing);
	xrtFree(sUtf8);
	xrtFree(sFile);
	xrtFree(sNested);
	xrtFree(sRoot);
	return 0;
}
