#include "../test.h"



#if !defined(_WIN32) && !defined(_WIN64) && !defined(__ANDROID__)
	#include <sys/stat.h>
	#include <unistd.h>
#endif



#if defined(_WIN32) || defined(_WIN64)
	#define TEST_TREE_ATTRIBUTE_HIDDEN 0x02u
#endif



/* 在系统临时目录构造目录树测试路径。 */
static str testTreePath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "tree test path allocation failed");
	return sPath;
}



/* 使用整文件层写入目录树夹具。 */
static void testTreeWrite(cstr sPath, cstr sText)
{
	testRequire(xrtFileWriteAll(sPath,
		(xbytesview){ (const unsigned char*)sText, strlen(sText) }),
		"tree fixture write failed");
}



/* 检查文件内容与预期文本完全一致。 */
static void testTreeText(cstr sPath, cstr sText)
{
	size_t iSize;
	bytes pData = xrtFileReadAll(sPath, &iSize);

	testRequire((pData != NULL) && (iSize == strlen(sText)) &&
		(memcmp(pData, sText, iSize) == 0), "tree file content mismatch");
	xrtFree(pData);
}



/* 删除测试遗留对象，但不吞掉非不存在错误。 */
static void testTreeReset(cstr sPath)
{
	xfileinfo Info;

	if ( !xrtPathStat(sPath, false, &Info) ) {
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
			"tree reset metadata query failed");
		xrtClearError();
		return;
	}
	if ( Info.Type == XFILE_TYPE_DIRECTORY ) {
		testRequire(xrtDirRemoveAll(sPath), "tree reset directory removal failed");
	} else if ( Info.Type == XFILE_TYPE_LINK ) {
		testRequire(xrtLinkDelete(sPath), "tree reset link removal failed");
	} else {
		testRequire(xrtFileDelete(sPath), "tree reset file removal failed");
	}
}



/* 构造包含空目录、嵌套目录和冲突类型的源目录树。 */
static void testTreeFixture(cstr sSource)
{
	str sSub = xrtPathJoin(sSource, "sub");
	str sDirectory = xrtPathJoin(sSource, "dir-as-source");
	str sEmpty = xrtPathJoin(sSource, "empty");
	str sRootFile = xrtPathJoin(sSource, "root.txt");
	str sNested = xrtPathJoin(sSub, "nested.txt");
	str sDirectoryFile = xrtPathJoin(sDirectory, "item.txt");
	str sFile = xrtPathJoin(sSource, "file-as-source.txt");

	testRequire((sSub != NULL) && (sDirectory != NULL) && (sEmpty != NULL) &&
		(sRootFile != NULL) && (sNested != NULL) &&
		(sDirectoryFile != NULL) && (sFile != NULL),
		"tree fixture path build failed");
	testRequire(xrtDirCreateAll(sSub) && xrtDirCreate(sDirectory) &&
		xrtDirCreate(sEmpty), "tree fixture directory creation failed");
	testTreeWrite(sRootFile, "root");
	testTreeWrite(sNested, "nested");
	testTreeWrite(sDirectoryFile, "directory");
	testTreeWrite(sFile, "file");
	xrtFree(sFile);
	xrtFree(sDirectoryFile);
	xrtFree(sNested);
	xrtFree(sRootFile);
	xrtFree(sEmpty);
	xrtFree(sDirectory);
	xrtFree(sSub);
}



/* 默认复制要求目标不存在，并保留空目录、内容和符号链接。 */
static void testTreeCopyDefault(cstr sSource, cstr sTarget, bool bHasLink)
{
	str sRootFile = xrtPathJoin(sTarget, "root.txt");
	str sNested = xrtPathJoin(sTarget, "sub/nested.txt");
	str sEmpty = xrtPathJoin(sTarget, "empty");
	str sLink = xrtPathJoin(sTarget, "root-link");
	xwalkstats Stats;

	testRequire(xrtFileTreeCopy(sSource, sTarget, NULL, &Stats),
		"default tree copy failed");
	testRequire((Stats.Items == (bHasLink ? 9u : 8u)) &&
		(Stats.Directories == 4u) && (Stats.Files == 4u) &&
		(Stats.Links == (bHasLink ? 1u : 0u)) && (Stats.Bytes == 23u),
		"default tree copy statistics are incorrect");
	testTreeText(sRootFile, "root");
	testTreeText(sNested, "nested");
	testRequire(xrtDirExists(sEmpty), "tree copy lost an empty directory");
	if ( bHasLink ) {
		str sStored = xrtLinkRead(sLink);

		testRequire((sStored != NULL) && (strcmp(sStored, "root.txt") == 0),
			"tree copy did not preserve symbolic link text");
		xrtFree(sStored);
	}
	testRequire(!xrtDirCopy(sSource, sTarget, false),
		"default tree copy merged an existing target");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
		"existing tree target reported the wrong error");
	xrtClearError();
	xrtFree(sLink);
	xrtFree(sEmpty);
	xrtFree(sNested);
	xrtFree(sRootFile);
}



/* 合并替换必须保留无冲突项，并处理文件与目录互占名称。 */
static void testTreeCopyMerge(cstr sSource, cstr sTarget)
{
	str sRootFile = xrtPathJoin(sTarget, "root.txt");
	str sStale = xrtPathJoin(sTarget, "stale.txt");
	str sDirectory = xrtPathJoin(sTarget, "dir-as-source");
	str sDirectoryFile = xrtPathJoin(sDirectory, "item.txt");
	str sFile = xrtPathJoin(sTarget, "file-as-source.txt");
	str sOld = xrtPathJoin(sFile, "old.txt");

	testRequire((sRootFile != NULL) && (sStale != NULL) &&
		(sDirectory != NULL) && (sDirectoryFile != NULL) &&
		(sFile != NULL) && (sOld != NULL), "merge fixture path build failed");
	testRequire(xrtDirCreate(sTarget), "merge target creation failed");
	testTreeWrite(sRootFile, "old-root");
	testTreeWrite(sStale, "stale");
	testTreeWrite(sDirectory, "was-file");
	testRequire(xrtDirCreate(sFile), "merge collision directory creation failed");
	testTreeWrite(sOld, "old-child");
	testRequire(xrtDirCopy(sSource, sTarget, true), "merging tree copy failed");
	testTreeText(sRootFile, "root");
	testTreeText(sStale, "stale");
	testRequire(xrtDirExists(sDirectory),
		"source directory did not replace a target file");
	testTreeText(sDirectoryFile, "directory");
	testRequire(xrtFileExists(sFile),
		"source file did not replace a target directory");
	testTreeText(sFile, "file");
	xrtFree(sOld);
	xrtFree(sFile);
	xrtFree(sDirectoryFile);
	xrtFree(sDirectory);
	xrtFree(sStale);
	xrtFree(sRootFile);
}



/* 高级选项必须拒绝非法组合、冲突和目标位于源树内部。 */
static void testTreeCopyBoundaries(cstr sSource, cstr sTarget)
{
	str sConflict = xrtPathJoin(sTarget, "root.txt");
	str sDescendant = xrtPathJoin(sSource, "copy-inside-source");
	xtreecopyoptions Options;
	xwalkstats Stats;
	xwalkstats Saved;

	testRequire(xrtDirCreate(sTarget), "boundary target creation failed");
	testTreeWrite(sConflict, "keep");
	xrtTreeCopyOptionsInit(&Options);
	Options.Flags = XTREE_COPY_MERGE;
	testRequire(!xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"merge without replacement overwrote a conflict");
	testTreeText(sConflict, "keep");
	xrtClearError();

	Options.Flags = XTREE_COPY_REPLACE;
	testRequire(!xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"tree copy accepted replace without merge");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"invalid tree options reported the wrong error");
	xrtClearError();

	memset(&Stats, 0xA5, sizeof(Stats));
	Saved = Stats;
	xrtTreeCopyOptionsInit(&Options);
	testRequire(!xrtFileTreeCopy(sSource, sDescendant, &Options, &Stats),
		"tree copy accepted a descendant target");
	testRequire(memcmp(&Stats, &Saved, sizeof(Stats)) == 0,
		"failed descendant copy modified statistics output");
	testRequire(!xrtPathExists(sDescendant),
		"descendant copy created a partial target");
	xrtClearError();
	testRequire(!xrtFileTreeCopy(sSource, sSource, NULL, NULL),
		"tree copy accepted identical source and target paths");
	xrtClearError();
	xrtFree(sDescendant);
	xrtFree(sConflict);
}



#if !defined(_WIN32) && !defined(_WIN64) && !defined(__ANDROID__)

/* 跟随到特殊对象必须拒绝或按显式策略跳过，不能静默成功。 */
static void testTreeFollowedSpecial(cstr sSource)
{
	str sFifo = testTreePath("xrt-tree-followed-special.fifo");
	str sLink = xrtPathJoin(sSource, "special-link");
	str sTarget = testTreePath("xrt-tree-followed-special-target");
	str sTargetLink = xrtPathJoin(sTarget, "special-link");
	xtreecopyoptions Options;

	testRequire((sLink != NULL) && (sTargetLink != NULL),
		"followed-special fixture path allocation failed");
	testTreeReset(sTarget);
	(void)unlink(sFifo);
	testRequire(mkfifo(sFifo, 0600) == 0,
		"followed-special FIFO creation failed");
	testRequire(xrtLinkCreate(sFifo, sLink, false),
		"followed-special link creation failed");

	xrtTreeCopyOptionsInit(&Options);
	Options.Flags = XTREE_COPY_FOLLOW_LINKS;
	testRequire(!xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"followed special object was silently accepted");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tree") == 0) &&
		(xrtErrorCode(xrtGetError()) == XTREE_ERROR_SPECIAL),
		"followed special object reported the wrong tree error");
	xrtClearError();
	testRequire(!xrtPathExists(sTarget),
		"failed followed-special copy published a partial target");

	Options.Flags = XTREE_COPY_FOLLOW_LINKS | XTREE_COPY_SKIP_SPECIAL;
	testRequire(xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"followed special object was not skipped by policy");
	testRequire(!xrtPathExists(sTargetLink),
		"skipped followed-special link appeared in the target");

	testRequire(xrtDirRemoveAll(sTarget) && xrtLinkDelete(sLink) &&
		(unlink(sFifo) == 0), "followed-special cleanup failed");
	xrtFree(sTargetLink);
	xrtFree(sTarget);
	xrtFree(sLink);
	xrtFree(sFifo);
}

#endif



/* 链接策略必须支持保留、跳过、跟随根链接并拒绝目录环。 */
static bool testTreeLinks(cstr sSource)
{
	str sLink = xrtPathJoin(sSource, "root-link");
	str sSourceLink = testTreePath("xrt-tree-source-link");
	str sTarget = testTreePath("xrt-tree-link-target");
	str sTargetLink = xrtPathJoin(sTarget, "root-link");
	str sTargetRoot = xrtPathJoin(sTarget, "root.txt");
	str sCycle = xrtPathJoin(sSource, "sub/back");
	str sAlias = testTreePath("xrt-tree-source-alias");
	str sAliasTarget = xrtPathJoin(sAlias, "copy-inside-source");
	xtreecopyoptions Options;
	bool bCreated;

	testTreeReset(sSourceLink);
	testTreeReset(sTarget);
	testTreeReset(sAlias);
	bCreated = xrtLinkCreate("root.txt", sLink, false);
	if ( !bCreated ) {
		#if defined(_WIN32) || defined(_WIN64)
			testRequire((xrtGetError() != NULL) &&
				((xrtErrorKind(xrtGetError()) == XERR_PERMISSION) ||
				 (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED)),
				"tree symbolic link fixture failed unexpectedly");
			xrtClearError();
			xrtFree(sCycle);
			xrtFree(sTargetRoot);
			xrtFree(sTargetLink);
			xrtFree(sTarget);
			xrtFree(sSourceLink);
			xrtFree(sAliasTarget);
			xrtFree(sAlias);
			xrtFree(sLink);
			return false;
		#else
			testRequire(false, "tree symbolic link fixture creation failed");
		#endif
	}

	#if !defined(_WIN32) && !defined(_WIN64) && !defined(__ANDROID__)
		testTreeFollowedSpecial(sSource);
	#endif

	xrtTreeCopyOptionsInit(&Options);
	Options.Flags = XTREE_COPY_SKIP_LINKS;
	testRequire(xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"skip-link tree copy failed");
	testRequire(!xrtPathExists(sTargetLink) && xrtFileExists(sTargetRoot),
		"skip-link tree copy retained a symbolic link");
	testRequire(xrtDirRemoveAll(sTarget), "skip-link target cleanup failed");

	testRequire(xrtLinkCreate(sSource, sSourceLink, true),
		"source directory link creation failed");
	testRequire(!xrtFileTreeCopy(sSourceLink, sTarget, NULL, NULL),
		"tree copy followed a root link without permission");
	xrtClearError();
	xrtTreeCopyOptionsInit(&Options);
	Options.Flags = XTREE_COPY_FOLLOW_LINKS;
	testRequire(xrtFileTreeCopy(sSourceLink, sTarget, &Options, NULL),
		"tree copy did not follow an allowed root link");
	testRequire(xrtFileExists(sTargetRoot) && xrtFileExists(sTargetLink),
		"root-link tree copy lost source contents");
	testTreeText(sTargetLink, "root");
	testRequire(xrtDirRemoveAll(sTarget) && xrtLinkDelete(sSourceLink),
		"root-link tree copy cleanup failed");
	testRequire(xrtDirCreate(sTarget) && xrtDirCreate(sTargetLink),
		"followed-link replacement fixture creation failed");
	Options.Flags = XTREE_COPY_FOLLOW_LINKS |
		XTREE_COPY_MERGE | XTREE_COPY_REPLACE;
	testRequire(xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"followed file link did not replace a target directory");
	testRequire(xrtFileExists(sTargetLink),
		"followed file link replacement retained the target directory");
	testTreeText(sTargetLink, "root");
	testRequire(xrtDirRemoveAll(sTarget),
		"followed-link replacement cleanup failed");
	testTreeReset(sAlias);
	testRequire(xrtLinkCreate(sSource, sAlias, true),
		"source alias creation failed");
	testRequire(!xrtFileTreeCopy(sSource, sAliasTarget, NULL, NULL),
		"tree copy accepted a target redirected into the source by a link");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tree") == 0) &&
		(xrtErrorCode(xrtGetError()) == XTREE_ERROR_DESCENDANT),
		"linked descendant target reported the wrong tree error");
	xrtClearError();
	testRequire(!xrtPathExists(sAliasTarget),
		"linked descendant check created a partial target");
	testRequire(xrtLinkDelete(sAlias), "source alias cleanup failed");

	testRequire(xrtLinkCreate("..", sCycle, true),
		"directory cycle fixture creation failed");
	testRequire(!xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"follow-link tree copy accepted a directory cycle");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tree") == 0) &&
		(xrtErrorCode(xrtGetError()) == XTREE_ERROR_LINK_CYCLE),
		"directory cycle reported the wrong tree error");
	xrtClearError();
	testRequire(!xrtPathExists(sTarget),
		"failed cyclic tree copy left a partial new target");
	testRequire(xrtLinkDelete(sCycle), "directory cycle cleanup failed");
	xrtFree(sCycle);
	xrtFree(sTargetRoot);
	xrtFree(sTargetLink);
	xrtFree(sTarget);
	xrtFree(sSourceLink);
	xrtFree(sAliasTarget);
	xrtFree(sAlias);
	xrtFree(sLink);
	return true;
}



/* 可选元数据复制必须保留源文件时间和平台权限属性。 */
static void testTreeMetadata(cstr sSource)
{
	const xtime Accessed = (xtime)978307200000000LL;
	const xtime Modified = (xtime)978307201345678LL;
	str sTarget = testTreePath("xrt-tree-metadata-target");
	str sSourceFile = xrtPathJoin(sSource, "root.txt");
	str sTargetFile = xrtPathJoin(sTarget, "root.txt");
	xtreecopyoptions Options;
	xfileinfo SourceInfo;
	xfileinfo TargetInfo;

	testTreeReset(sTarget);
	testRequire(xrtPathSetTimes(sSourceFile, true, &Accessed, &Modified),
		"tree source metadata time setup failed");
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(xrtPathStat(sSourceFile, false, &SourceInfo) &&
			xrtPathSetAttributes(sSourceFile,
				SourceInfo.Attributes | TEST_TREE_ATTRIBUTE_HIDDEN),
			"tree source Windows attribute setup failed");
	#else
		testRequire(xrtPathSetMode(sSourceFile, true, 0600u),
			"tree source POSIX mode setup failed");
	#endif
	xrtTreeCopyOptionsInit(&Options);
	Options.Flags = XTREE_COPY_METADATA;
	testRequire(xrtFileTreeCopy(sSource, sTarget, &Options, NULL),
		"metadata-preserving tree copy failed");
	testRequire(xrtPathStat(sSourceFile, true, &SourceInfo) &&
		xrtPathStat(sTargetFile, true, &TargetInfo) &&
		(TargetInfo.Accessed == Accessed) &&
		(TargetInfo.Modified == Modified) &&
		(SourceInfo.Modified == Modified),
		"tree copy did not preserve file timestamps");
	#if defined(_WIN32) || defined(_WIN64)
		testRequire((TargetInfo.Attributes & TEST_TREE_ATTRIBUTE_HIDDEN) != 0u,
			"tree copy did not preserve Windows attributes");
	#else
		testRequire((TargetInfo.Mode & 0777u) == 0600u,
			"tree copy did not preserve POSIX mode");
	#endif
	testRequire(xrtDirRemoveAll(sTarget), "metadata tree target cleanup failed");
	xrtFree(sTargetFile);
	xrtFree(sSourceFile);
	xrtFree(sTarget);
}



/* 清空、递归删除和根保护必须形成明确且安全的删除契约。 */
static void testTreeRemove(cstr sTarget)
{
	str sTemporary = xrtPathTemp();
	str sAbsolute;
	str sRoot;
	xpathparts Parts;
	xwalkstats Stats;
	uint64 iRecursiveSize;
	uint64 iDirectSize;
	bool bEmpty;

	testRequire(xrtDirStats(sTarget, true, &Stats) &&
		(Stats.Directories >= 4u) && (Stats.Files >= 5u),
		"recursive directory statistics are incorrect");
	testRequire(xrtDirSize(sTarget, true, &iRecursiveSize) &&
		xrtDirSize(sTarget, false, &iDirectSize) &&
		(iRecursiveSize > iDirectSize), "directory size helpers are incorrect");
	testRequire(xrtDirClean(sTarget), "tree clean failed");
	testRequire(xrtDirExists(sTarget) && xrtDirEmpty(sTarget, &bEmpty) && bEmpty,
		"tree clean did not preserve one empty root directory");
	testRequire(xrtDirRemoveAll(sTarget) && !xrtPathExists(sTarget),
		"recursive tree removal failed");
	{
		str sNested = xrtPathJoin(sTarget, "new/child");

		testRequire((sNested != NULL) && xrtDirEnsureEmpty(sNested) &&
			xrtDirExists(sNested), "ensure-empty did not create a missing tree");
		testRequire(xrtDirEnsureEmpty(sTarget) &&
			xrtDirEmpty(sTarget, &bEmpty) && bEmpty,
			"ensure-empty did not clean an existing tree");
		testRequire(xrtDirRemoveAll(sTarget),
			"ensure-empty tree cleanup failed");
		xrtFree(sNested);
	}
	testRequire(sTemporary != NULL, "temporary path query for root guard failed");
	sAbsolute = xrtPathAbs(sTemporary);
	testRequire((sAbsolute != NULL) &&
		xrtPathParse(xrtStrView(sAbsolute), XPATH_NATIVE, &Parts),
		"filesystem root parse failed");
	sRoot = xrtStrDupView(Parts.Root);
	testRequire(sRoot != NULL, "filesystem root allocation failed");
	testRequire(!xrtDirRemoveAll(sRoot),
		"recursive removal accepted a filesystem root");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tree") == 0) &&
		(xrtErrorCode(xrtGetError()) == XTREE_ERROR_ROOT),
		"filesystem root protection reported the wrong error");
	xrtClearError();
	xrtFree(sRoot);
	xrtFree(sAbsolute);
	xrtFree(sTemporary);
}



/* 目录移动必须支持快速改名、冲突拒绝和显式合并替换。 */
static void testTreeMove(void)
{
	str sSource = testTreePath("xrt-tree-move-source");
	str sTarget = testTreePath("xrt-tree-move-target");
	str sSourceFile = xrtPathJoin(sSource, "item.txt");
	str sTargetFile = xrtPathJoin(sTarget, "item.txt");
	str sStale = xrtPathJoin(sTarget, "stale.txt");

	testTreeReset(sSource);
	testTreeReset(sTarget);
	testRequire(xrtDirCreate(sSource), "move source creation failed");
	testTreeWrite(sSourceFile, "first");
	testRequire(xrtDirMove(sSource, sTarget, false),
		"same-volume directory move failed");
	testRequire(!xrtPathExists(sSource), "directory move retained its source");
	testTreeText(sTargetFile, "first");

	testRequire(xrtDirCreate(sSource), "merge move source creation failed");
	testTreeWrite(sSourceFile, "second");
	testTreeWrite(sStale, "stale");
	testRequire(!xrtDirMove(sSource, sTarget, false),
		"non-replacing directory move accepted an existing target");
	testRequire(xrtDirExists(sSource),
		"failed non-replacing move removed its source");
	xrtClearError();
	testRequire(xrtDirMove(sSource, sTarget, true),
		"replacing directory move did not merge an existing target");
	testRequire(!xrtPathExists(sSource),
		"merged directory move retained its source");
	testTreeText(sTargetFile, "second");
	testTreeText(sStale, "stale");
	testRequire(xrtDirRemoveAll(sTarget), "move target cleanup failed");
	xrtFree(sStale);
	xrtFree(sTargetFile);
	xrtFree(sSourceFile);
	xrtFree(sTarget);
	xrtFree(sSource);
}



/* 目录树层回归入口。 */
int main(void)
{
	str sSource = testTreePath("xrt-tree-source");
	str sTarget = testTreePath("xrt-tree-target");
	str sBoundary = testTreePath("xrt-tree-boundary");
	bool bHasLink;

	testTreeReset(sSource);
	testTreeReset(sTarget);
	testTreeReset(sBoundary);
	testTreeFixture(sSource);
	bHasLink = testTreeLinks(sSource);
	testTreeMetadata(sSource);
	testTreeCopyDefault(sSource, sTarget, bHasLink);
	testRequire(xrtDirRemoveAll(sTarget), "default tree target cleanup failed");
	testTreeCopyMerge(sSource, sTarget);
	testTreeCopyBoundaries(sSource, sBoundary);
	testTreeRemove(sTarget);
	testRequire(xrtDirRemoveAll(sBoundary), "boundary target cleanup failed");
	testTreeMove();
	testRequire(xrtDirRemoveAll(sSource), "tree source cleanup failed");
	xrtFree(sBoundary);
	xrtFree(sTarget);
	xrtFree(sSource);
	return 0;
}
