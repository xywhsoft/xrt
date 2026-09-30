#include "../test.h"



static void testVfsDiskName(char* sBuffer, size_t iCapacity, cstr sLabel)
{
	static uint32 iSequence = 0u;
	int iSize = snprintf(sBuffer, iCapacity, ".xrt-%s-%lld-%u",
		sLabel, (long long)xrtNow(), (unsigned int)++iSequence);

	testRequire((iSize > 0) && ((size_t)iSize < iCapacity),
		"disk provider fixture name formatting failed");
}



static xroot testVfsDiskRootCreate(cstr sName, xroot* pParent)
{
	xroot Parent = xrtRootOpen(".");
	xroot Root;

	testRequire(Parent != NULL, "disk provider parent root open failed");
	if ( !xrtRootRemove(Parent, sName) ) xrtClearError();
	testRequire(xrtRootDirCreate(Parent, sName, 0700u),
		"disk provider fixture directory creation failed");
	Root = xrtRootOpenIn(Parent, sName);
	testRequire(Root != NULL, "disk provider fixture root open failed");
	*pParent = Parent;
	return Root;
}



static void testVfsDiskWrite(xroot Root, cstr sPath, cstr sText)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, sPath, &Options);
	testRequire(File != NULL, "disk provider fixture file open failed");
	testRequire(xrtWriteFull(File, sText, strlen(sText), NULL),
		"disk provider fixture file write failed");
	testRequire(xrtClose(File), "disk provider fixture file close failed");
}



static void testVfsDiskRead(xfile File, cstr sExpected)
{
	char sBuffer[96] = { 0 };
	size_t iSize = strlen(sExpected);
	size_t iRead = 0u;

	testRequire((iSize < sizeof(sBuffer)) &&
		xrtRead(File, sBuffer, sizeof(sBuffer), &iRead) &&
		(iRead == iSize) && (memcmp(sBuffer, sExpected, iSize) == 0),
		"disk provider file content is incorrect");
}



/* Disk provider 必须保留原生能力、目录元数据和已打开文件生命周期。 */
static void testVfsDiskBasics(void)
{
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	xvfsdisk Disk;
	xvfs Vfs;
	xvfsmount Mount;
	xfile File;
	xfileoptions Options;
	xfileinfo Info;
	xdir Dir;
	xdirentry Entry;
	xdirnext Next;
	bool bValue = false;
	bool bNew = false;

	testVfsDiskName(sDirectory, sizeof(sDirectory), "vfs-disk-basic");
	Root = testVfsDiskRootCreate(sDirectory, &Parent);
	testRequire(xrtRootDirCreate(Root, "Data", 0700u),
		"disk provider child directory creation failed");
	testVfsDiskWrite(Root, "Data/Value.txt", "native-payload");
	Disk = xrtVfsDiskCreate(sDirectory,
		XVFS_DISK_READ | XVFS_DISK_WRITE);
	Vfs = xrtVfsCreate();
	testRequire((Disk != NULL) && (Vfs != NULL),
		"disk provider basic objects creation failed");
	Mount = xrtVfsDiskMount(Vfs, "/disk", 10,
		XVFS_CASE_SENSITIVE, Disk, 0u);
	testRequire(Mount != NULL, "disk provider basic mount failed");
	File = xrtVfsOpen(Vfs, "/disk/Data/Value.txt", NULL);
	testRequire((File != NULL) &&
		(xrtFileNative(File) != (intptr_t)-1),
		"disk provider did not return a native file");
	testVfsDiskRead(File, "native-payload");
	testRequire(xrtVfsStat(Vfs, "/disk/Data/Value.txt", true, &Info) &&
		(Info.Type == XFILE_TYPE_FILE) && (Info.Size == 14u),
		"disk provider stat metadata is incorrect");

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	{
		xfile NewFile = xrtVfsOpen(Vfs, "/disk/Data/New.txt", &Options);

		testRequire(NewFile != NULL, "writable disk provider create failed");
		testRequire(xrtWriteFull(NewFile, "new", 3u, NULL) &&
			xrtClose(NewFile), "writable disk provider write failed");
	}
	Dir = xrtVfsDirOpen(Vfs, "/disk/Data", 0u);
	testRequire(Dir != NULL, "disk provider directory open failed");
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		if ( xrtStrEqual(Entry.Name, xrtStrView("Value.txt")) ) {
			bValue = Entry.Info.Type == XFILE_TYPE_FILE;
		} else if ( xrtStrEqual(Entry.Name, xrtStrView("New.txt")) ) {
			bNew = Entry.Info.Type == XFILE_TYPE_FILE;
		} else {
			testRequire(false, "disk provider returned an unexpected entry");
		}
	}
	testRequire((Next == XDIR_NEXT_END) && bValue && bNew &&
		xrtDirClose(Dir), "disk provider directory contents are incorrect");

	testRequire(xrtVfsUnmount(Mount), "disk provider basic unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsDiskDestroy(Disk);
	testRequire(xrtSeek(File, 0, XSEEK_START, NULL),
		"native disk file did not survive provider destruction");
	testVfsDiskRead(File, "native-payload");
	testRequire(xrtClose(File), "surviving native disk file close failed");

	testRequire(xrtRootRemove(Root, "Data/New.txt") &&
		xrtRootRemove(Root, "Data/Value.txt") &&
		xrtRootRemove(Root, "Data") &&
		xrtRootClose(Root) && xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent), "disk provider basic fixture cleanup failed");
}



/* 读写权限必须在进入物理 lookup 前生效，并阻止 overlay 回退。 */
static void testVfsDiskAccess(void)
{
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	xvfsdisk ReadOnly;
	xvfsdisk WriteOnly;
	xvfs Vfs;
	xvfsmount Mount;
	xfileoptions Options;
	xfile File;
	xfileinfo Info;

	testVfsDiskName(sDirectory, sizeof(sDirectory), "vfs-disk-access");
	Root = testVfsDiskRootCreate(sDirectory, &Parent);
	testVfsDiskWrite(Root, "value.txt", "value");
	Vfs = xrtVfsCreate();
	ReadOnly = xrtVfsDiskCreate(sDirectory, XVFS_DISK_READ);
	testRequire((Vfs != NULL) && (ReadOnly != NULL),
		"read-only disk provider creation failed");
	Mount = xrtVfsDiskMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, ReadOnly, 0u);
	testRequire(Mount != NULL, "read-only disk provider mount failed");
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE | XFILE_SYNC;
	testRequire(xrtVfsOpen(Vfs, "/blocked.txt", &Options) == NULL,
		"read-only disk provider accepted a write open");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
		"read-only disk provider reported the wrong error");
	xrtClearError();
	testRequire(xrtVfsUnmount(Mount), "read-only disk unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDiskDestroy(ReadOnly);

	WriteOnly = xrtVfsDiskCreate(sDirectory, XVFS_DISK_WRITE);
	testRequire(WriteOnly != NULL, "write-only disk provider creation failed");
	Mount = xrtVfsDiskMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, WriteOnly, 0u);
	testRequire(Mount != NULL, "write-only disk provider mount failed");
	testRequire(xrtVfsOpen(Vfs, "/value.txt", NULL) == NULL,
		"write-only disk provider accepted a read open");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
		"write-only read rejection reported the wrong error");
	xrtClearError();
	testRequire(!xrtVfsStat(Vfs, "/value.txt", true, &Info) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
		"write-only disk provider accepted stat");
	xrtClearError();
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtVfsOpen(Vfs, "/created.txt", &Options);
	testRequire((File != NULL) &&
		xrtWriteFull(File, "created", 7u, NULL) && xrtClose(File),
		"write-only disk provider could not create a file");
	testRequire(xrtVfsUnmount(Mount), "write-only disk unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDiskDestroy(WriteOnly);
	xrtVfsDestroy(Vfs);

	testRequire(xrtRootRemove(Root, "created.txt") &&
		xrtRootRemove(Root, "value.txt") && xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) && xrtRootClose(Parent),
		"disk access fixture cleanup failed");
}



/* Mount case 策略必须独立于宿主文件系统，并拒绝折叠歧义。 */
static void testVfsDiskCase(void)
{
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	xvfsdisk Disk;
	xvfs Vfs;
	xvfsmount Mount;
	xfile File;
	xfileoptions Options;

	testVfsDiskName(sDirectory, sizeof(sDirectory), "vfs-disk-case");
	Root = testVfsDiskRootCreate(sDirectory, &Parent);
	testRequire(xrtRootDirCreate(Root, "Folder", 0700u),
		"disk case fixture directory creation failed");
	testVfsDiskWrite(Root, "Folder/Value.TXT", "case");
	Disk = xrtVfsDiskCreate(sDirectory, XVFS_DISK_READ);
	Vfs = xrtVfsCreate();
	testRequire((Disk != NULL) && (Vfs != NULL),
		"disk case objects creation failed");
	Mount = xrtVfsDiskMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Disk, 0u);
	testRequire(Mount != NULL, "case-sensitive disk mount failed");
	testRequire(xrtVfsOpen(Vfs, "/folder/value.txt", NULL) == NULL,
		"case-sensitive disk mount accepted substituted case");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
		"case-sensitive miss reported the wrong error");
	xrtClearError();
	testRequire(xrtVfsUnmount(Mount), "case-sensitive disk unmount failed");
	xrtVfsMountDestroy(Mount);
	Mount = xrtVfsDiskMount(Vfs, "/", 0,
		XVFS_CASE_ASCII_INSENSITIVE, Disk, 0u);
	testRequire(Mount != NULL, "case-insensitive disk mount failed");
	File = xrtVfsOpen(Vfs, "/folder/value.txt", NULL);
	testRequire(File != NULL, "case-insensitive disk lookup failed");
	testVfsDiskRead(File, "case");
	testRequire(xrtClose(File), "case-insensitive disk file close failed");

	/* 在支持大小写不同名的宿主上验证折叠冲突拒绝。 */
	testVfsDiskWrite(Root, "Fold.txt", "upper");
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE;
	File = xrtRootFileOpen(Root, "fold.txt", &Options);
	if ( File != NULL ) {
		testRequire(xrtClose(File), "case collision fixture close failed");
		testRequire(xrtVfsOpen(Vfs, "/FOLD.TXT", NULL) == NULL &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
			"case-insensitive lookup accepted a folded collision");
		xrtClearError();
		testRequire(xrtVfsDirOpen(Vfs, "/", 0u) == NULL &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
			"case-insensitive directory accepted a folded collision");
		xrtClearError();
		testRequire(xrtRootRemove(Root, "fold.txt"),
			"case collision lowercase cleanup failed");
	} else {
		/* 默认不区分大小写的宿主无法构造两个物理名称。 */
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_EXISTS),
			"case collision fixture failed for an unexpected reason");
		xrtClearError();
	}
	testRequire(xrtRootRemove(Root, "Fold.txt"),
		"case collision uppercase cleanup failed");
	testRequire(xrtVfsUnmount(Mount), "case-insensitive disk unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsDiskDestroy(Disk);

	testRequire(xrtRootRemove(Root, "Folder/Value.TXT") &&
		xrtRootRemove(Root, "Folder") && xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) && xrtRootClose(Parent),
		"disk case fixture cleanup failed");
}



/* Provider 必须始终从根句柄解析，物理目录改名后仍保持同一锚点。 */
static void testVfsDiskRenameAnchor(void)
{
	char sDirectory[96];
	char sMoved[96];
	xroot Parent;
	xroot Root;
	xvfsdisk Disk;
	xvfs Vfs;
	xvfsmount Mount;
	xfile File;

	testVfsDiskName(sDirectory, sizeof(sDirectory), "vfs-disk-anchor");
	testVfsDiskName(sMoved, sizeof(sMoved), "vfs-disk-anchor-moved");
	Root = testVfsDiskRootCreate(sDirectory, &Parent);
	if ( !xrtRootRemove(Parent, sMoved) ) xrtClearError();
	testVfsDiskWrite(Root, "value.txt", "anchored");
	Disk = xrtVfsDiskCreate(sDirectory, XVFS_DISK_READ);
	Vfs = xrtVfsCreate();
	testRequire((Disk != NULL) && (Vfs != NULL),
		"disk anchor objects creation failed");
	Mount = xrtVfsDiskMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Disk, 0u);
	testRequire(Mount != NULL, "disk anchor mount failed");
	testRequire(xrtPathRename(sDirectory, sMoved, false),
		"disk anchor physical directory rename failed");
	File = xrtVfsOpen(Vfs, "/value.txt", NULL);
	testRequire(File != NULL,
		"disk provider lost its anchor after physical directory rename");
	testVfsDiskRead(File, "anchored");
	testRequire(xrtClose(File), "disk anchor file close failed");
	testRequire(xrtVfsUnmount(Mount), "disk anchor unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsDiskDestroy(Disk);
	testRequire(xrtRootRemove(Root, "value.txt") && xrtRootClose(Root) &&
		xrtRootRemove(Parent, sMoved) && xrtRootClose(Parent),
		"disk anchor fixture cleanup failed");
}



/* 链接作为不可回退权限错误处理，目录枚举也不能暴露链接。 */
static void testVfsDiskLinks(void)
{
	char sDirectory[96];
	str sLink;
	xroot Parent;
	xroot Root;
	xvfsdisk Disk;
	xvfs Vfs;
	xvfsmount Mount;

	testVfsDiskName(sDirectory, sizeof(sDirectory), "vfs-disk-link");
	Root = testVfsDiskRootCreate(sDirectory, &Parent);
	testVfsDiskWrite(Root, "target.txt", "target");
	sLink = xrtPathJoin(sDirectory, "linked.txt");
	testRequire(sLink != NULL, "disk link fixture path construction failed");
	if ( xrtLinkCreate("target.txt", sLink, false) ) {
		Disk = xrtVfsDiskCreate(sDirectory, XVFS_DISK_READ);
		Vfs = xrtVfsCreate();
		testRequire((Disk != NULL) && (Vfs != NULL),
			"disk link objects creation failed");
		Mount = xrtVfsDiskMount(Vfs, "/", 0,
			XVFS_CASE_SENSITIVE, Disk, 0u);
		testRequire(Mount != NULL, "disk link mount failed");
		testRequire(xrtVfsOpen(Vfs, "/linked.txt", NULL) == NULL,
			"disk provider followed a symbolic link");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"disk link rejection reported the wrong error");
		xrtClearError();
		testRequire(xrtVfsDirOpen(Vfs, "/", 0u) == NULL,
			"disk directory exposed a symbolic link");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"disk directory link rejection reported the wrong error");
		xrtClearError();
		testRequire(xrtVfsUnmount(Mount), "disk link unmount failed");
		xrtVfsMountDestroy(Mount);
		xrtVfsDestroy(Vfs);
		xrtVfsDiskDestroy(Disk);
		testRequire(xrtRootRemove(Root, "linked.txt"),
			"disk link fixture cleanup failed");
	} else {
		/* Windows without symbolic-link privilege still exercises other contracts. */
		xrtClearError();
	}
	xrtFree(sLink);
	testRequire(xrtRootRemove(Root, "target.txt") && xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) && xrtRootClose(Parent),
		"disk link fixture directory cleanup failed");
}



/* 特殊文件不得通过 native open 绕过普通文件合同。 */
static void testVfsDiskSpecialFiles(void)
{
	char sDirectory[96];
	xroot Parent;
	xroot Root;

	testVfsDiskName(sDirectory, sizeof(sDirectory), "vfs-disk-special");
	Root = testVfsDiskRootCreate(sDirectory, &Parent);
	if ( xrtRootFifoCreate(Root, "pipe", 0600u) ) {
		xvfsdisk Disk = xrtVfsDiskCreate(sDirectory, XVFS_DISK_READ);
		xvfs Vfs = xrtVfsCreate();
		xvfsmount Mount;

		testRequire((Disk != NULL) && (Vfs != NULL),
			"disk special-file objects creation failed");
		Mount = xrtVfsDiskMount(Vfs, "/", 0,
			XVFS_CASE_SENSITIVE, Disk, 0u);
		testRequire(Mount != NULL, "disk special-file mount failed");
		testRequire(xrtVfsOpen(Vfs, "/pipe", NULL) == NULL &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
			"disk provider opened a non-regular file");
		xrtClearError();
		testRequire(xrtVfsUnmount(Mount),
			"disk special-file unmount failed");
		xrtVfsMountDestroy(Mount);
		xrtVfsDestroy(Vfs);
		xrtVfsDiskDestroy(Disk);
		testRequire(xrtRootRemove(Root, "pipe"),
			"disk special-file cleanup failed");
	} else {
		/* Windows 等不提供文件系统 FIFO 的平台跳过此夹具。 */
		xrtClearError();
	}
	testRequire(xrtRootClose(Root) && xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent), "disk special-file directory cleanup failed");
}



static void testVfsDiskValidation(void)
{
	testRequire(xrtVfsDiskCreate(".", 0u) == NULL,
		"disk provider accepted empty access");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"empty disk access reported the wrong error");
	xrtClearError();
	testRequire(xrtVfsDiskCreate(".", 0x80000000u) == NULL,
		"disk provider accepted unknown access bits");
	xrtClearError();
}



int main(void)
{
	testVfsDiskBasics();
	testVfsDiskAccess();
	testVfsDiskCase();
	testVfsDiskRenameAnchor();
	testVfsDiskLinks();
	testVfsDiskSpecialFiles();
	testVfsDiskValidation();
	return 0;
}
