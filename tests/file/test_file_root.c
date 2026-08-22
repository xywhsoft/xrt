#include "../test.h"



/* 为每个测试生成低冲突且无需额外分配的本地名称。 */
static void testRootName(char* sBuffer, size_t iCapacity, cstr sLabel)
{
	static uint32 iSequence = 0u;
	int iSize = snprintf(sBuffer, iCapacity, ".xrt-%s-%lld-%u",
		sLabel, (long long)xrtNow(), (unsigned int)++iSequence);

	testRequire((iSize > 0) && ((size_t)iSize < iCapacity),
		"test root name formatting failed");
}



/* 使用根内文件 API 创建并写入一份短文件。 */
static void testRootWrite(xroot Root, cstr sPath, cstr sText)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, sPath, &Options);
	testRequire(File != NULL, "root-relative file open failed");
	testRequire(xrtWriteFull(File, sText, strlen(sText), NULL),
		"root-relative file write failed");
	testRequire(xrtClose(File), "root-relative file close failed");
}



/* 使用根内文件 API 读取并核对一份短文件。 */
static void testRootRead(xroot Root, cstr sPath, cstr sExpected)
{
	char sBuffer[64];
	size_t iSize = strlen(sExpected);
	xfile File = xrtRootFileOpen(Root, sPath, NULL);

	testRequire(File != NULL, "root-relative read open failed");
	memset(sBuffer, 0, sizeof(sBuffer));
	testRequire((iSize < sizeof(sBuffer)) &&
		xrtReadFull(File, sBuffer, iSize, NULL) &&
		(memcmp(sBuffer, sExpected, iSize) == 0),
		"root-relative file content is incorrect");
	testRequire(xrtClose(File), "root-relative read close failed");
}



/* 在当前目录创建一个只供当前测试使用的根和父根。 */
static xroot testRootCreate(cstr sName, xroot* pParent)
{
	xroot Parent = xrtRootOpen(".");
	xroot Root;

	testRequire(Parent != NULL, "test parent root open failed");
	if ( !xrtRootRemove(Parent, sName) ) {
		xrtClearError();
	}
	testRequire(xrtRootDirCreate(Parent, sName, 0700u),
		"test root directory creation failed");
	Root = xrtRootOpenIn(Parent, sName);
	testRequire(Root != NULL, "test child root open failed");
	*pParent = Parent;
	return Root;
}



/* 关闭测试根并从父根删除已经清空的目录。 */
static void testRootDestroy(xroot Parent, xroot Root, cstr sName)
{
	testRequire(xrtRootClose(Root), "test child root close failed");
	testRequire(xrtRootRemove(Parent, sName),
		"test root directory cleanup failed");
	testRequire(xrtRootClose(Parent), "test parent root close failed");
}



/* 根对象必须覆盖文件、子根、元数据、创建和删除基础操作。 */
static void testRootBasics(void)
{
	char sDirectory[96];
	xfileoptions Options;
	xfile File;
	xfileinfo Info;
	uint64 iSize;
	xroot Parent;
	xroot Child;
	xroot Root;

	testRootName(sDirectory, sizeof(sDirectory), "root-basic");
	Root = testRootCreate(sDirectory, &Parent);
	testRequire((Root != NULL) &&
		(xrtRootPath(Root) != NULL) &&
		(xrtRootNative(Root) != (intptr_t)-1),
		"root object did not retain its directory capability");

	testRootWrite(Root, "data.bin", "root-data");
	testRootRead(Root, "data.bin", "root-data");
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_APPEND | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, "data.bin", &Options);
	testRequire(File != NULL,
		"root append and truncate open failed");
	testRequire(xrtFileSize(File, &iSize) && (iSize == 0u),
		"root append and truncate did not clear the file");
	testRequire(xrtWriteFull(File, "root-data", 9u, NULL),
		"root append and truncate write failed");
	testRequire(xrtClose(File),
		"root append and truncate close failed");
	testRootRead(Root, "data.bin", "root-data");
	testRequire(xrtRootStat(Root, "data.bin", true, &Info) &&
		(Info.Type == XFILE_TYPE_FILE) && (Info.Size == 9u),
		"root-relative file metadata is incorrect");
	testRequire(xrtRootStat(Root, ".", true, &Info) &&
		(Info.Type == XFILE_TYPE_DIRECTORY),
		"root directory metadata is incorrect");
	testRequire(xrtRootFileOpen(Root, ".", NULL) == NULL,
		"root file open accepted its directory");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"root directory file-open reported the wrong error");
	xrtClearError();
	testRequire(xrtRootDirCreate(Root, "nested", 0700u),
		"root-relative directory creation failed");
	Child = xrtRootOpenIn(Root, "nested");
	testRequire(Child != NULL, "root-relative child capability failed");
	testRootWrite(Child, "child.txt", "child");
	testRootRead(Root, "nested/child.txt", "child");
	testRootRead(Root, "nested/../data.bin", "root-data");
	testRequire(!xrtRootRemove(Root, "data.bin/"),
		"root removed a file through a directory path");
	xrtClearError();
	testRootRead(Root, "data.bin", "root-data");
	testRequire(xrtRootDirCreate(Root, "trailing/", 0700u),
		"root failed to create a directory path with a trailing separator");
	testRequire(xrtRootRemove(Root, "trailing/"),
		"root failed to remove a directory path with a trailing separator");

	testRequire(xrtRootRemove(Child, "child.txt"),
		"child-root file removal failed");
	testRequire(xrtRootClose(Child), "child root close failed");
	testRequire(xrtRootRemove(Root, "nested"),
		"root-relative directory removal failed");
	testRequire(xrtRootRemove(Root, "data.bin"),
		"root-relative file removal failed");
	testRootDestroy(Parent, Root, sDirectory);
}



/* 绝对路径、越界父目录和 Windows 特殊名称必须在系统调用前被拒绝。 */
static void testRootEscape(void)
{
	char sDirectory[96];
	char sOutsideName[96];
	str sAbsolute;
	str sEscape;
	xroot Parent;
	xroot Root;

	testRootName(sDirectory, sizeof(sDirectory), "root-escape");
	testRootName(sOutsideName, sizeof(sOutsideName), "root-outside");
	Root = testRootCreate(sDirectory, &Parent);
	if ( !xrtRootRemove(Parent, sOutsideName) ) {
		xrtClearError();
	}
	testRootWrite(Parent, sOutsideName, "outside");
	sEscape = xrtPathJoin("..", sOutsideName);
	testRequire(sEscape != NULL, "escape test path construction failed");
	sAbsolute = xrtPathAbs(sOutsideName);
	testRequire(sAbsolute != NULL, "absolute escape path construction failed");

	testRequire(xrtRootFileOpen(Root, sEscape, NULL) == NULL,
		"root accepted a parent-directory escape");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
		"parent-directory escape reported the wrong error");
	xrtClearError();

	testRequire(xrtRootFileOpen(Root, sAbsolute, NULL) == NULL,
		"root accepted an absolute path");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
		"absolute path escape reported the wrong error");
	xrtClearError();

	testRequire(!xrtRootRemove(Root, "."),
		"root allowed removal of the capability directory");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
		"root self-removal reported the wrong error");
	xrtClearError();

	#if defined(_WIN32) || defined(_WIN64)
		testRequire(xrtRootFileOpen(Root, "NUL", NULL) == NULL,
			"root accepted a Windows device name");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"Windows device name reported the wrong error");
		xrtClearError();

		testRequire(xrtRootFileOpen(Root, "CONOUT$", NULL) == NULL,
			"root accepted the longest Windows device name");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"longest Windows device name reported the wrong error");
		xrtClearError();

		testRequire(xrtRootFileOpen(Root,
			"COM\xC2\xB9", NULL) == NULL,
			"root accepted a superscript Windows device name");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"superscript Windows device name reported the wrong error");
		xrtClearError();

		testRequire(xrtRootFileOpen(Root, "data:stream", NULL) == NULL,
			"root accepted a Windows alternate data stream");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"Windows stream name reported the wrong error");
		xrtClearError();
	#endif

	testRequire(xrtRootRemove(Parent, sOutsideName),
		"outside test file cleanup failed");
	testRootDestroy(Parent, Root, sDirectory);
	xrtFree(sAbsolute);
	xrtFree(sEscape);
}



/* 相对链接可以留在根内，但绝对链接和越界链接不得被跟随。 */
static void testRootLinks(void)
{
	char sDirectory[96];
	str sTargetPath;
	str sAbsoluteTarget;
	str sInsideLink;
	str sEscapeLink;
	str sLoopLeft;
	str sLoopRight;
	xfileinfo Info;
	xroot Parent;
	xroot Root;

	testRootName(sDirectory, sizeof(sDirectory), "root-link");
	Root = testRootCreate(sDirectory, &Parent);
	testRootWrite(Root, "target.txt", "inside");
	sTargetPath = xrtPathJoin(sDirectory, "target.txt");
	testRequire(sTargetPath != NULL,
		"link target path construction failed");
	sAbsoluteTarget = xrtPathAbs(sTargetPath);
	sInsideLink = xrtPathJoin(sDirectory, "inside-link");
	sEscapeLink = xrtPathJoin(sDirectory, "escape-link");
	sLoopLeft = xrtPathJoin(sDirectory, "loop-left");
	sLoopRight = xrtPathJoin(sDirectory, "loop-right");
	testRequire((sAbsoluteTarget != NULL) && (sInsideLink != NULL) &&
		(sEscapeLink != NULL) && (sLoopLeft != NULL) &&
		(sLoopRight != NULL), "link test path construction failed");

	if ( xrtLinkCreate("target.txt", sInsideLink, false) ) {
		xfileoptions Options;
		str sStored = xrtRootLinkRead(Root, "inside-link");

		testRequire((sStored != NULL) &&
			(strcmp(sStored, "target.txt") == 0),
			"root-relative link target text is incorrect");
		xrtFree(sStored);
		testRequire(xrtRootStat(Root, "inside-link", false, &Info) &&
			(Info.Type == XFILE_TYPE_LINK),
			"root lstat did not preserve link identity");
		testRequire(xrtRootStat(Root, "inside-link", true, &Info) &&
			(Info.Type == XFILE_TYPE_FILE),
			"root stat did not follow a safe relative link");

		xrtFileOptionsInit(&Options);
		Options.Flags = XFILE_READ | XFILE_NOFOLLOW;
		testRequire(xrtRootFileOpen(Root,
			"inside-link", &Options) == NULL,
			"root nofollow open accepted a symbolic link");
		xrtClearError();

		xrtFileOptionsInit(&Options);
		Options.Flags = XFILE_WRITE | XFILE_CREATE |
			XFILE_TRUNCATE | XFILE_EXCLUSIVE;
		testRequire(xrtRootFileOpen(Root,
			"inside-link", &Options) == NULL,
			"root exclusive create accepted an existing link");
		xrtClearError();
		testRootRead(Root, "inside-link", "inside");
		testRequire(xrtRootRemove(Root, "inside-link"),
			"root-relative link removal failed");

		testRequire(xrtLinkCreate(sAbsoluteTarget,
			sInsideLink, false), "absolute link creation failed");
		testRequire(xrtRootFileOpen(Root, "inside-link", NULL) == NULL,
			"root followed an absolute link target");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"absolute link target reported the wrong error");
		xrtClearError();
		testRequire(xrtRootRemove(Root, "inside-link"),
			"absolute link cleanup failed");

		testRequire(xrtLinkCreate("../outside.txt",
			sEscapeLink, false), "escape link creation failed");
		testRequire(xrtRootFileOpen(Root, "escape-link", NULL) == NULL,
			"root followed a link above its directory");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"escaping link reported the wrong error");
		xrtClearError();
		testRequire(xrtRootRemove(Root, "escape-link"),
			"escaping link cleanup failed");

		testRequire(xrtLinkCreate("../",
			sEscapeLink, true), "trailing escape link creation failed");
		testRequire(xrtRootOpenIn(Root, "escape-link") == NULL,
			"root followed a trailing link above its directory");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"trailing escaping link reported the wrong error");
		xrtClearError();
		testRequire(xrtRootRemove(Root, "escape-link"),
			"trailing escape link cleanup failed");

		testRequire(xrtLinkCreate("loop-right",
			sLoopLeft, false), "left loop link creation failed");
		testRequire(xrtLinkCreate("loop-left",
			sLoopRight, false), "right loop link creation failed");
		testRequire(xrtRootFileOpen(Root, "loop-left", NULL) == NULL,
			"root did not stop a symbolic link cycle");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE),
			"symbolic link cycle reported the wrong error");
		xrtClearError();
		testRequire(xrtRootRemove(Root, "loop-left") &&
			xrtRootRemove(Root, "loop-right"),
			"symbolic link cycle cleanup failed");
	} else {
		/* Windows without symlink privilege still exercises all non-link contracts. */
		xrtClearError();
	}

	testRequire(xrtRootRemove(Root, "target.txt"),
		"link target cleanup failed");
	testRootDestroy(Parent, Root, sDirectory);
	xrtFree(sLoopRight);
	xrtFree(sLoopLeft);
	xrtFree(sEscapeLink);
	xrtFree(sInsideLink);
	xrtFree(sAbsoluteTarget);
	xrtFree(sTargetPath);
}





/* 根对象创建类操作必须使用父目录句柄，不能退回显示路径。 */
static void testRootCreateObjects(void)
{
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	bool bHardLink;
	#if !defined(_WIN32) && !defined(_WIN64)
		xfileinfo Info;
	#endif

	testRootName(sDirectory, sizeof(sDirectory), "root-create");
	Root = testRootCreate(sDirectory, &Parent);
	testRootWrite(Root, "source.txt", "linked");
	bHardLink = xrtRootLinkHard(Root, "source.txt", "hard.txt");
	if ( !bHardLink ) {
		const xerror* pError = xrtGetError();

		#if defined(__ANDROID__)
			testRequire((pError != NULL) &&
				(xrtErrorKind(pError) == XERR_PERMISSION),
				"Android root hard-link failure lost its permission error");
			xrtClearError();
		#else
			fprintf(stderr, "root hard-link error: %s (system=%d)\n",
				pError != NULL ? xrtErrorMessage(pError) : "missing error",
				pError != NULL ? (int)xrtErrorSystemCode(pError) : 0);
			testRequire(false, "root-relative hard-link creation failed");
		#endif
	} else {
		testRootRead(Root, "hard.txt", "linked");
	}
	testRequire(xrtRootSetMode(Root,
		"source.txt", true, 0640u),
		"root-relative mode update failed");

	#if defined(_WIN32) || defined(_WIN64)
		testRequire(!xrtRootLinkCreate(Root,
			"source.txt", "symbolic.txt", false),
			"Windows root unexpectedly created an unsupported symbolic link");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
			"unsupported Windows root link reported the wrong error");
		xrtClearError();
		testRequire(!xrtRootFifoCreate(Root, "pipe", 0600u),
			"Windows root unexpectedly created a POSIX FIFO");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
			"unsupported Windows root FIFO reported the wrong error");
		xrtClearError();
	#else
		testRequire(xrtRootStat(Root, "source.txt", true, &Info) &&
			((Info.Available & XFILE_INFO_MODE) != 0u) &&
			((Info.Mode & 0777u) == 0640u),
			"root-relative mode update was not applied");
		testRequire(xrtRootLinkCreate(Root,
			"source.txt", "symbolic.txt", false),
			"root-relative symbolic-link creation failed");
		testRootRead(Root, "symbolic.txt", "linked");
		#if defined(__ANDROID__)
			testRequire(!xrtRootFifoCreate(Root, "pipe", 0600u) &&
				(xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
				"Android root FIFO failure lost its permission error");
			xrtClearError();
			testRequire(xrtRootRemove(Root, "symbolic.txt"),
				"root-relative symbolic-link cleanup failed");
		#else
			testRequire(xrtRootFifoCreate(Root, "pipe", 0600u) &&
				xrtRootStat(Root, "pipe", false, &Info) &&
				(Info.Type == XFILE_TYPE_FIFO),
				"root-relative FIFO creation failed");
			testRequire(xrtRootRemove(Root, "pipe") &&
				xrtRootRemove(Root, "symbolic.txt"),
				"root-relative POSIX object cleanup failed");
		#endif
	#endif

	testRequire((!bHardLink || xrtRootRemove(Root, "hard.txt")) &&
		xrtRootRemove(Root, "source.txt"),
		"root-relative hard-link cleanup failed");
	testRootDestroy(Parent, Root, sDirectory);
}



/* 根句柄必须在目录改名后继续引用原来的目录对象。 */
static void testRootRenameAnchor(void)
{
	char sDirectory[96];
	char sMoved[96];
	str sMovedFile;
	xroot Parent;
	xroot Root;

	testRootName(sDirectory, sizeof(sDirectory), "root-anchor");
	testRootName(sMoved, sizeof(sMoved), "root-moved");
	Root = testRootCreate(sDirectory, &Parent);
	if ( !xrtRootRemove(Parent, sMoved) ) {
		xrtClearError();
	}
	testRequire(xrtPathRename(sDirectory, sMoved, false),
		"root directory rename failed");
	testRootWrite(Root, "after-rename.txt", "anchored");
	sMovedFile = xrtPathJoin(sMoved, "after-rename.txt");
	testRequire((sMovedFile != NULL) && xrtFileExists(sMovedFile),
		"root handle did not follow the renamed directory object");
	testRequire(xrtRootRemove(Root, "after-rename.txt"),
		"anchored file cleanup failed");
	testRequire(xrtRootClose(Root), "anchor test root close failed");
	testRequire(xrtRootRemove(Parent, sMoved),
		"renamed root directory cleanup failed");
	testRequire(xrtRootClose(Parent), "anchor parent root close failed");
	xrtFree(sMovedFile);
}



/* 目录根回归入口。 */
int main(void)
{
	testRootBasics();
	testRootEscape();
	testRootLinks();
	testRootCreateObjects();
	testRootRenameAnchor();
	return 0;
}
