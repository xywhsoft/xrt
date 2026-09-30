#ifdef VFS_MEMORY_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/vfs.h>
#endif

#include "../test.h"



static void testVfsMemoryRead(xfile File, cstr sExpected)
{
	char sBuffer[64];
	size_t iRead = 0u;
	size_t iExpected = strlen(sExpected);

	memset(sBuffer, 0, sizeof(sBuffer));
	testRequire(xrtRead(File, sBuffer, sizeof(sBuffer), &iRead),
		"memory provider read failed");
	testRequire((iRead == iExpected) &&
		(memcmp(sBuffer, sExpected, iExpected) == 0),
		"memory provider returned unexpected data");
}



static void testVfsMemoryFilesAndDirectories(void)
{
	char sCopied[] = "hello";
	bytes pOwned = (bytes)xrtMemDup("owned", 5u);
	xvfsmemory Memory = xrtVfsMemoryCreate();
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount;
	xfile File;
	xfileinfo Info;
	xdir Dir;
	xdirentry Entry;
	const cstr Expected[] = { "empty.bin", "hello.txt", "owned.bin" };
	size_t iEntry = 0u;
	char sTail[3] = { 0 };
	size_t iRead = 0u;
	xfileoptions Write;

	testRequire((Memory != NULL) && (Vfs != NULL) && (pOwned != NULL),
		"memory provider fixture allocation failed");
	testRequire(xrtVfsMemoryPutCopy(Memory,
		"assets/hello.txt", sCopied, 5u), "copy put failed");
	memset(sCopied, 'x', 5u);
	testRequire(xrtVfsMemoryPutOwned(Memory,
		"assets/owned.bin", pOwned, 5u), "owned put failed");
	pOwned = NULL;
	testRequire(xrtVfsMemoryPutCopy(Memory,
		"assets/empty.bin", NULL, 0u), "empty file put failed");
	testRequire(xrtVfsMemoryPutCopy(Memory,
		"nested/deep/value.txt", "deep", 4u), "nested put failed");
	testRequire(xrtVfsMemorySeal(Memory) && xrtVfsMemorySeal(Memory),
		"memory provider seal is not idempotent");
	Mount = xrtVfsMemoryMount(Vfs, "/App", 10,
		XVFS_CASE_ASCII_INSENSITIVE, Memory, 0u);
	testRequire(Mount != NULL, "sealed memory provider mount failed");
	xrtVfsMemoryDestroy(Memory);
	Memory = NULL;

	File = xrtVfsOpen(Vfs, "/app/ASSETS/HELLO.TXT", NULL);
	testRequire(File != NULL, "case-insensitive memory open failed");
	testVfsMemoryRead(File, "hello");
	testRequire(xrtSeek(File, -2, XSEEK_END, NULL),
		"memory provider seek failed");
	testRequire(xrtRead(File, sTail, 2u, &iRead) &&
		(iRead == 2u) && (memcmp(sTail, "lo", 2u) == 0),
		"memory provider seek/read returned wrong data");
	memset(sTail, 0, sizeof(sTail));
	testRequire(xrtReadAt(File, 1u, sTail, 2u, &iRead) &&
		(iRead == 2u) && (memcmp(sTail, "el", 2u) == 0),
		"memory provider read-at failed");
	testRequire(xrtFileStat(File, &Info) &&
		(Info.Type == XFILE_TYPE_FILE) &&
		((Info.Available & XFILE_INFO_SIZE) != 0u) && (Info.Size == 5u),
		"memory provider opened-file stat failed");

	testRequire(xrtVfsStat(Vfs, "/APP", false, &Info) &&
		(Info.Type == XFILE_TYPE_DIRECTORY),
		"memory provider root stat failed");
	testRequire(xrtVfsStat(Vfs, "/app/NESTED/DEEP", false, &Info) &&
		(Info.Type == XFILE_TYPE_DIRECTORY),
		"memory provider synthesized directory stat failed");
	{
		xfile Empty = xrtVfsOpen(Vfs, "/app/assets/empty.bin", NULL);
		xfile Owned = xrtVfsOpen(Vfs, "/app/assets/owned.bin", NULL);

		testRequire((Empty != NULL) && (Owned != NULL),
			"memory provider did not open empty or owned data");
		testVfsMemoryRead(Empty, "");
		testVfsMemoryRead(Owned, "owned");
		testRequire(xrtClose(Empty) && xrtClose(Owned),
			"memory provider empty or owned file close failed");
	}
	Dir = xrtVfsDirOpen(Vfs, "/APP/assets", 0u);
	testRequire(Dir != NULL, "memory provider directory open failed");
	while ( xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM ) {
		testRequire((iEntry < 3u) &&
			xrtStrEqual(Entry.Name, xrtStrView(Expected[iEntry])),
			"memory provider directory order is wrong");
		testRequire((Entry.Flags & XDIR_ENTRY_UTF8) != 0u,
			"memory provider directory entry lost UTF-8 validation");
		iEntry++;
	}
	testRequire(iEntry == 3u, "memory provider directory count is wrong");
	testRequire(xrtDirClose(Dir), "memory provider directory close failed");

	xrtFileOptionsInit(&Write);
	Write.Flags = XFILE_WRITE;
	testRequire(xrtVfsOpen(Vfs, "/app/assets/hello.txt", &Write) == NULL,
		"read-only memory provider accepted write access");
	xrtClearError();
	testRequire(xrtVfsOpen(Vfs, "/app/missing.txt", &Write) == NULL,
		"memory provider unexpectedly opened a missing write target");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
		"missing memory write target did not remain a provider MISS");
	xrtClearError();
	testRequire(xrtVfsUnmount(Mount), "memory provider unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	testVfsMemoryRead(File, "");
	testRequire(xrtSeek(File, 0, XSEEK_START, NULL),
		"opened memory file did not survive provider destruction");
	testVfsMemoryRead(File, "hello");
	testRequire(xrtClose(File), "surviving memory file close failed");
}



static void testVfsMemorySnapshotReplacement(void)
{
	xvfsmemory Old = xrtVfsMemoryCreate();
	xvfsmemory New = xrtVfsMemoryCreate();
	xvfs Vfs = xrtVfsCreate();
	xvfsmount OldMount;
	xvfsmount NewMount;
	xfile OldFile;
	xfile NewFile;

	testRequire((Old != NULL) && (New != NULL) && (Vfs != NULL),
		"memory snapshot fixture allocation failed");
	testRequire(xrtVfsMemoryPutCopy(Old, "value", "old", 3u) &&
		xrtVfsMemoryPutCopy(New, "value", "new", 3u) &&
		xrtVfsMemorySeal(Old) && xrtVfsMemorySeal(New),
		"memory snapshot fixture build failed");
	OldMount = xrtVfsMemoryMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Old, 0u);
	testRequire(OldMount != NULL, "old memory snapshot mount failed");
	OldFile = xrtVfsOpen(Vfs, "/value", NULL);
	testRequire(OldFile != NULL, "old memory snapshot open failed");
	NewMount = xrtVfsMemoryMount(Vfs, "/", 10,
		XVFS_CASE_SENSITIVE, New, 0u);
	testRequire(NewMount != NULL, "new memory snapshot mount failed");
	NewFile = xrtVfsOpen(Vfs, "/value", NULL);
	testRequire(NewFile != NULL, "new memory snapshot open failed");
	testVfsMemoryRead(OldFile, "old");
	testVfsMemoryRead(NewFile, "new");
	testRequire(xrtClose(OldFile) && xrtClose(NewFile),
		"memory snapshot file close failed");
	testRequire(xrtVfsUnmount(NewMount) && xrtVfsUnmount(OldMount),
		"memory snapshot unmount failed");
	xrtVfsMountDestroy(NewMount);
	xrtVfsMountDestroy(OldMount);
	xrtVfsDestroy(Vfs);
	xrtVfsMemoryDestroy(New);
	xrtVfsMemoryDestroy(Old);
}



static void testVfsMemoryValidation(void)
{
	static const cstr Invalid[] = {
		"", "/absolute", "a/", "a//b", "a/./b", "a/../b", "a\\b"
	};
	xvfsmemory Memory = xrtVfsMemoryCreate();
	xvfs Vfs = xrtVfsCreate();
	bytes pOwned;

	testRequire((Memory != NULL) && (Vfs != NULL),
		"memory validation fixture allocation failed");
	testRequire(xrtVfsMemoryMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Memory, 0u) == NULL,
		"unsealed memory provider was mounted");
	xrtClearError();
	for ( size_t i = 0u; i < sizeof(Invalid) / sizeof(Invalid[0]); i++ ) {
		testRequire(!xrtVfsMemoryPutCopy(Memory, Invalid[i], "x", 1u),
			"memory provider accepted an invalid relative path");
		xrtClearError();
	}
	testRequire(xrtVfsMemoryPutCopy(Memory, "same", "first", 5u),
		"memory duplicate fixture put failed");
	testRequire(!xrtVfsMemoryPutCopy(Memory, "same", "second", 6u),
		"memory provider accepted a duplicate path");
	xrtClearError();
	pOwned = (bytes)xrtMemDup("owned", 5u);
	testRequire(pOwned != NULL, "memory owned failure fixture allocation failed");
	testRequire(!xrtVfsMemoryPutOwned(Memory, "same", pOwned, 5u),
		"memory owned put accepted a duplicate path");
	xrtFree(pOwned);
	xrtClearError();
	testRequire(xrtVfsMemorySeal(Memory), "valid memory provider seal failed");
	testRequire(!xrtVfsMemoryPutCopy(Memory, "later", "x", 1u),
		"sealed memory provider accepted mutation");
	xrtClearError();
	xrtVfsMemoryDestroy(Memory);
	xrtVfsDestroy(Vfs);

	Memory = xrtVfsMemoryCreate();
	testRequire((Memory != NULL) &&
		xrtVfsMemoryPutCopy(Memory, "a", "file", 4u) &&
		xrtVfsMemoryPutCopy(Memory, "a/b", "child", 5u),
		"file-directory conflict fixture build failed");
	testRequire(!xrtVfsMemorySeal(Memory),
		"memory provider accepted a file-directory conflict");
	xrtClearError();
	xrtVfsMemoryDestroy(Memory);
}



static void testVfsMemoryCaseCollision(void)
{
	xvfsmemory Memory = xrtVfsMemoryCreate();
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount;

	testRequire((Memory != NULL) && (Vfs != NULL) &&
		xrtVfsMemoryPutCopy(Memory, "A.txt", "A", 1u) &&
		xrtVfsMemoryPutCopy(Memory, "a.txt", "a", 1u) &&
		xrtVfsMemorySeal(Memory), "case-collision fixture build failed");
	testRequire(xrtVfsMemoryMount(Vfs, "/", 0,
		XVFS_CASE_ASCII_INSENSITIVE, Memory, 0u) == NULL,
		"ASCII-insensitive mount accepted colliding memory paths");
	xrtClearError();
	Mount = xrtVfsMemoryMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Memory, 0u);
	testRequire(Mount != NULL,
		"case-sensitive mount rejected distinct memory paths");
	testRequire(xrtVfsUnmount(Mount), "case-collision fixture unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsMemoryDestroy(Memory);
}



int main(void)
{
	testVfsMemoryFilesAndDirectories();
	testVfsMemorySnapshotReplacement();
	testVfsMemoryValidation();
	testVfsMemoryCaseCollision();
	return 0;
}
