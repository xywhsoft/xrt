#ifdef VFS_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/vfs.h>
#endif

#include "../test.h"

#include <stddef.h>



typedef struct test_vfs_context {
	cstr FilePath;
	cstr Data;
	const cstr* Names;
	size_t NameCount;
	volatile int32 References;
	uint32 Retains;
	uint32 Releases;
	uint32 Opens;
	uint32 Closes;
	xvfscase LastOpenCase;
	xvfscase LastStatCase;
	xvfscase LastDirCase;
	bool FailOpen;
	bool BreakMiss;
} test_vfs_context;

typedef struct test_vfs_file {
	test_vfs_context* Context;
	size_t Cursor;
} test_vfs_file;

typedef struct test_vfs_dir {
	test_vfs_context* Context;
	size_t Position;
} test_vfs_dir;

typedef struct test_vfs_native_context {
	cstr Path;
	volatile int32 References;
	uint32 Retains;
	uint32 Releases;
} test_vfs_native_context;

/* v1 初始发布布局，用真实短对象验证 Size 尾扩展不会越界读取。 */
typedef struct test_vfs_legacy_provider {
	uint32 Size;
	uint32 Version;
	uint64 Capabilities;
	void (*ContextRetain)(void* pContext);
	void (*ContextRelease)(void* pContext);
	xvfslookup (*Open)(void* pContext, xvfscase CaseMode,
		xstrview RelativePath,
		const xfileoptions* pOptions, xvfsfile_v1* pFile);
	xvfslookup (*Stat)(void* pContext, xvfscase CaseMode,
		xstrview RelativePath,
		bool bFollowLink, xfileinfo* pInfo);
	xvfslookup (*DirOpen)(void* pContext, xvfscase CaseMode,
		xstrview RelativePath,
		uint32 iFlags, xvfsdir_v1* pDir);
} test_vfs_legacy_provider;



static bool testVfsReadAt(void* pState, uint64 iOffset,
	void* pBuffer, size_t iRequest, size_t* pRead)
{
	test_vfs_file* pFile = (test_vfs_file*)pState;
	size_t iSize = strlen(pFile->Context->Data);
	size_t iStart = (iOffset > SIZE_MAX) ? SIZE_MAX : (size_t)iOffset;
	size_t iDone = (iStart < iSize) ? (iSize - iStart) : 0u;

	if ( iDone > iRequest ) iDone = iRequest;
	if ( iDone != 0u ) memcpy(pBuffer,
		pFile->Context->Data + iStart, iDone);
	*pRead = iDone;
	return true;
}

static bool testVfsFileStat(void* pState, xfileinfo* pInfo)
{
	test_vfs_file* pFile = (test_vfs_file*)pState;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = strlen(pFile->Context->Data);
	return true;
}

static void testVfsFileClose(void* pState)
{
	test_vfs_file* pFile = (test_vfs_file*)pState;
	pFile->Context->Closes++;
	xrtFree(pFile);
}

static const xvfsfileops_v1 testVfsFileOps = {
	(uint32)sizeof(xvfsfileops_v1),
	XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ_AT | XVFS_FILE_STAT,
	NULL, NULL,
	testVfsReadAt, NULL,
	NULL, testVfsFileStat,
	NULL, NULL,
	testVfsFileClose
};



static void testVfsContextRetain(void* pContext)
{
	test_vfs_context* pVfs = (test_vfs_context*)pContext;
	pVfs->Retains++;
	(void)xrtRefRetain(&pVfs->References);
}

static void testVfsContextRelease(void* pContext)
{
	test_vfs_context* pVfs = (test_vfs_context*)pContext;
	pVfs->Releases++;
	(void)xrtRefRelease(&pVfs->References);
}

static xvfslookup testVfsOpen(void* pContext, xvfscase CaseMode,
	xstrview RelativePath,
	const xfileoptions* pOptions, xvfsfile_v1* pFile)
{
	test_vfs_context* pVfs = (test_vfs_context*)pContext;
	xstrview Expected = xrtStrView(pVfs->FilePath);
	test_vfs_file* pState;

	pVfs->Opens++;
	pVfs->LastOpenCase = CaseMode;
	if ( pVfs->FailOpen ) return XVFS_LOOKUP_ERROR;
	if ( !((CaseMode == XVFS_CASE_ASCII_INSENSITIVE) ?
		xrtStrCaseEqual(RelativePath, Expected) :
		xrtStrEqual(RelativePath, Expected)) ) {
		if ( pVfs->BreakMiss ) {
			pFile->State = pVfs;
		}
		return XVFS_LOOKUP_MISS;
	}
	pState = (test_vfs_file*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) return XVFS_LOOKUP_ERROR;
	pState->Context = pVfs;
	pFile->Ops = &testVfsFileOps;
	pFile->State = pState;
	pFile->Flags = pOptions->Flags;
	return XVFS_LOOKUP_OPENED;
}

static xvfslookup testVfsStat(void* pContext, xvfscase CaseMode,
	xstrview RelativePath,
	bool bFollowLink, xfileinfo* pInfo)
{
	test_vfs_context* pVfs = (test_vfs_context*)pContext;
	(void)bFollowLink;
	pVfs->LastStatCase = CaseMode;
	if ( !((CaseMode == XVFS_CASE_ASCII_INSENSITIVE) ?
		xrtStrCaseEqual(RelativePath, xrtStrView(pVfs->FilePath)) :
		xrtStrEqual(RelativePath, xrtStrView(pVfs->FilePath))) )
		return XVFS_LOOKUP_MISS;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = strlen(pVfs->Data);
	return XVFS_LOOKUP_OPENED;
}

static bool testVfsDirNext(void* pState, xdirentry* pEntry, bool* pEnd)
{
	test_vfs_dir* pDir = (test_vfs_dir*)pState;
	if ( pDir->Position == pDir->Context->NameCount ) {
		*pEnd = true;
		return true;
	}
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Name = xrtStrView(pDir->Context->Names[pDir->Position++]);
	pEntry->Info.Type = XFILE_TYPE_FILE;
	return true;
}

static void testVfsDirClose(void* pState)
{
	xrtFree(pState);
}

static const xvfsdirops_v1 testVfsDirOps = {
	(uint32)sizeof(xvfsdirops_v1),
	XRT_VFS_DIR_OPS_VERSION,
	testVfsDirNext,
	testVfsDirClose
};

static xvfslookup testVfsDirOpen(void* pContext, xvfscase CaseMode,
	xstrview RelativePath,
	uint32 iFlags, xvfsdir_v1* pDir)
{
	test_vfs_context* pVfs = (test_vfs_context*)pContext;
	test_vfs_dir* pState;
	(void)iFlags;
	pVfs->LastDirCase = CaseMode;
	if ( RelativePath.Size != 0u ) return XVFS_LOOKUP_MISS;
	pState = (test_vfs_dir*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) return XVFS_LOOKUP_ERROR;
	pState->Context = pVfs;
	pDir->Ops = &testVfsDirOps;
	pDir->State = pState;
	return XVFS_LOOKUP_OPENED;
}

static const xvfsprovider_v1 testVfsProvider = {
	(uint32)sizeof(xvfsprovider_v1),
	XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_STAT | XVFS_PROVIDER_DIRECTORY,
	testVfsContextRetain,
	testVfsContextRelease,
	testVfsOpen,
	testVfsStat,
	testVfsDirOpen,
	NULL
};



static void testVfsNativeRetain(void* pContext)
{
	test_vfs_native_context* pNative = (test_vfs_native_context*)pContext;
	pNative->Retains++;
	(void)xrtRefRetain(&pNative->References);
}

static void testVfsNativeRelease(void* pContext)
{
	test_vfs_native_context* pNative = (test_vfs_native_context*)pContext;
	pNative->Releases++;
	(void)xrtRefRelease(&pNative->References);
}

static xvfslookup testVfsNativeOpen(void* pContext, xvfscase CaseMode,
	xstrview RelativePath,
	const xfileoptions* pOptions, xfile* pFile)
{
	test_vfs_native_context* pNative =
		(test_vfs_native_context*)pContext;
	(void)CaseMode;
	if ( !xrtStrEqual(RelativePath, xrtStrView("file")) ) {
		return XVFS_LOOKUP_MISS;
	}
	*pFile = xrtFileOpen(pNative->Path, pOptions);
	return (*pFile != NULL) ? XVFS_LOOKUP_OPENED : XVFS_LOOKUP_ERROR;
}

static const xvfsprovider_v1 testVfsNativeProvider = {
	(uint32)sizeof(xvfsprovider_v1),
	XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_NATIVE_OPEN,
	testVfsNativeRetain,
	testVfsNativeRelease,
	NULL,
	NULL,
	NULL,
	testVfsNativeOpen
};



static test_vfs_context testVfsContext(cstr sPath, cstr sData,
	const cstr* pNames, size_t iNameCount)
{
	test_vfs_context Context;
	memset(&Context, 0, sizeof(Context));
	Context.FilePath = sPath;
	Context.Data = sData;
	Context.Names = pNames;
	Context.NameCount = iNameCount;
	Context.References = 1;
	return Context;
}

static void testVfsReadText(xfile File, cstr sExpected)
{
	char sBuffer[64];
	size_t iRead = 0u;

	memset(sBuffer, 0, sizeof(sBuffer));
	testRequire(xrtRead(File, sBuffer, sizeof(sBuffer) - 1u, &iRead),
		"VFS file read failed");
	testRequire((iRead == strlen(sExpected)) &&
		(memcmp(sBuffer, sExpected, iRead) == 0),
		"VFS file returned unexpected bytes");
}



static void testVfsLookupAndLifetime(void)
{
	test_vfs_context Root = testVfsContext("app/fallback.txt", "root", NULL, 0u);
	test_vfs_context High = testVfsContext("same.txt", "high", NULL, 0u);
	xvfs Vfs = xrtVfsCreate();
	xvfsmount RootMount;
	xvfsmount HighMount;
	xfile File;
	xfileinfo Info;
	uint64 iRootId;
	uint64 iHighId;

	testRequire(Vfs != NULL, "VFS creation failed");
	RootMount = xrtVfsMount(Vfs, "/", 0, XVFS_CASE_SENSITIVE,
		&testVfsProvider, &Root, 0u);
	HighMount = xrtVfsMount(Vfs, "/app", 10, XVFS_CASE_SENSITIVE,
		&testVfsProvider, &High, 0u);
	testRequire((RootMount != NULL) && (HighMount != NULL),
		"VFS mount failed");
	iRootId = xrtVfsMountId(RootMount);
	iHighId = xrtVfsMountId(HighMount);
	testRequire((iRootId != 0u) && (iHighId == iRootId + 1u),
		"mount identifiers are not monotonic");
	File = xrtVfsOpen(Vfs, "/app/same.txt", NULL);
	testRequire(File != NULL, "long-prefix lookup failed");
	testVfsReadText(File, "high");
	testRequire(xrtSeek(File, 0, XSEEK_START, NULL),
		"ReadAt cursor fallback did not expose seek");
	testVfsReadText(File, "high");
	testRequire(xrtFileStat(File, &Info) && (Info.Size == 4u),
		"provider file stat failed");
	testRequire(xrtVfsUnmount(HighMount) && xrtVfsUnmount(HighMount),
		"unmount is not idempotent");
	testVfsReadText(File, "");
	testRequire(xrtClose(File), "VFS file close failed");
	File = xrtVfsOpen(Vfs, "/app/fallback.txt", NULL);
	testRequire(File != NULL, "MISS did not fall back to a shorter prefix");
	testVfsReadText(File, "root");
	testRequire(xrtClose(File), "fallback file close failed");
	testRequire(xrtVfsStat(Vfs, "/app/fallback.txt", false, &Info) &&
		(Info.Size == 4u), "VFS stat fallback failed");
	xrtVfsMountDestroy(HighMount);
	testRequire((High.Retains == 1u) && (High.Releases == 1u),
		"unmounted generation did not release its context exactly once");
	testRequire(xrtVfsUnmount(RootMount), "root unmount failed");
	xrtVfsDestroy(Vfs);
	xrtVfsMountDestroy(RootMount);
	testRequire((Root.Retains == 1u) && (Root.Releases == 1u),
		"namespace/control lifetime did not release root context");
}



static void testVfsMountOrder(void)
{
	test_vfs_context First = testVfsContext("file", "first", NULL, 0u);
	test_vfs_context Second = testVfsContext("file", "second", NULL, 0u);
	test_vfs_context High = testVfsContext("file", "high", NULL, 0u);
	xvfs Vfs = xrtVfsCreate();
	xvfsmount FirstMount;
	xvfsmount SecondMount;
	xvfsmount HighMount;
	xfile File;

	testRequire(Vfs != NULL, "mount-order VFS creation failed");
	FirstMount = xrtVfsMount(Vfs, "/same", 0,
		XVFS_CASE_SENSITIVE, &testVfsProvider, &First, 0u);
	SecondMount = xrtVfsMount(Vfs, "/same", 0,
		XVFS_CASE_SENSITIVE, &testVfsProvider, &Second, 0u);
	HighMount = xrtVfsMount(Vfs, "/same", 5,
		XVFS_CASE_SENSITIVE, &testVfsProvider, &High, 0u);
	testRequire((FirstMount != NULL) && (SecondMount != NULL) &&
		(HighMount != NULL), "mount-order fixture setup failed");

	File = xrtVfsOpen(Vfs, "/same/file", NULL);
	testRequire(File != NULL, "priority lookup failed");
	testVfsReadText(File, "high");
	testRequire(xrtClose(File), "priority file close failed");
	testRequire(xrtVfsUnmount(HighMount), "high-priority unmount failed");

	File = xrtVfsOpen(Vfs, "/same/file", NULL);
	testRequire(File != NULL, "stable MountId lookup failed");
	testVfsReadText(File, "first");
	testRequire(xrtClose(File), "first MountId file close failed");
	testRequire(xrtVfsUnmount(FirstMount), "first MountId unmount failed");

	File = xrtVfsOpen(Vfs, "/same/file", NULL);
	testRequire(File != NULL, "second MountId fallback failed");
	testVfsReadText(File, "second");
	testRequire(xrtClose(File), "second MountId file close failed");
	testRequire(xrtVfsUnmount(SecondMount), "second MountId unmount failed");

	xrtVfsMountDestroy(HighMount);
	xrtVfsMountDestroy(FirstMount);
	xrtVfsMountDestroy(SecondMount);
	xrtVfsDestroy(Vfs);
}



static void testVfsCasePolicyPropagation(void)
{
	const cstr Names[] = { "Icon.PNG" };
	test_vfs_context Context = testVfsContext(
		"Icon.PNG", "case", Names, 1u);
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount;
	xfile File;
	xfileinfo Info;
	xdir Dir;

	testRequire(Vfs != NULL, "case-policy VFS creation failed");
	Mount = xrtVfsMount(Vfs, "/Assets", 0,
		XVFS_CASE_ASCII_INSENSITIVE, &testVfsProvider, &Context, 0u);
	testRequire(Mount != NULL, "case-policy mount failed");
	File = xrtVfsOpen(Vfs, "/assets/icon.png", NULL);
	testRequire(File != NULL,
		"provider did not receive the mount case policy for open");
	testVfsReadText(File, "case");
	testRequire(xrtClose(File), "case-policy file close failed");
	testRequire(xrtVfsStat(Vfs, "/ASSETS/ICON.PNG", false, &Info) &&
		(Info.Size == 4u),
		"provider did not receive the mount case policy for stat");
	Dir = xrtVfsDirOpen(Vfs, "/aSsEtS", 0u);
	testRequire(Dir != NULL,
		"provider did not receive the mount case policy for directory open");
	testRequire(xrtDirClose(Dir), "case-policy directory close failed");
	testRequire((Context.LastOpenCase == XVFS_CASE_ASCII_INSENSITIVE) &&
		(Context.LastStatCase == XVFS_CASE_ASCII_INSENSITIVE) &&
		(Context.LastDirCase == XVFS_CASE_ASCII_INSENSITIVE),
		"provider callbacks observed an inconsistent case policy");
	testRequire(xrtVfsUnmount(Mount), "case-policy unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
}



static void testVfsOpenFileOutlivesNamespace(void)
{
	test_vfs_context Context = testVfsContext("file", "retained", NULL, 0u);
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount;
	xfile File;

	testRequire(Vfs != NULL, "retained-file VFS creation failed");
	Mount = xrtVfsMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, &testVfsProvider, &Context, 0u);
	testRequire(Mount != NULL, "retained-file mount failed");
	File = xrtVfsOpen(Vfs, "/file", NULL);
	testRequire(File != NULL, "retained-file open failed");
	testRequire(xrtVfsUnmount(Mount), "retained-file unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	testRequire(Context.Releases == 0u,
		"provider context was released while an opened file still used it");
	testVfsReadText(File, "retained");
	testRequire(xrtClose(File), "retained-file close failed");
	testRequire((Context.Retains == 1u) && (Context.Releases == 1u),
		"opened file did not retain its provider generation");
}



static void testVfsNativeOpenAndLegacySize(void)
{
	const cstr sPath = "test-vfs-native.tmp";
	test_vfs_native_context Context = { sPath, 1, 0u, 0u };
	test_vfs_context LegacyContext =
		testVfsContext("file", "legacy", NULL, 0u);
	test_vfs_legacy_provider LegacyProvider;
	xfileoptions WriteOptions = {
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE,
		0600u,
		XFILE_SHARE_ALL
	};
	xvfs Vfs;
	xvfsmount NativeMount;
	xvfsmount LegacyMount;
	xfile File;
	char sBuffer[16] = { 0 };
	size_t iDone = 0u;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtFileOpen(sPath, &WriteOptions);
	testRequire(File != NULL, "native-provider fixture open failed");
	testRequire(xrtWriteFull(File, "native", 6u, NULL),
		"native-provider fixture write failed");
	testRequire(xrtClose(File), "native-provider fixture close failed");

	Vfs = xrtVfsCreate();
	testRequire(Vfs != NULL, "native-provider VFS creation failed");
	NativeMount = xrtVfsMount(Vfs, "/native", 0,
		XVFS_CASE_SENSITIVE, &testVfsNativeProvider, &Context, 0u);
	testRequire(NativeMount != NULL, "native provider mount failed");
	File = xrtVfsOpen(Vfs, "/native/file", NULL);
	testRequire((File != NULL) &&
		(xrtFileNative(File) != (intptr_t)-1),
		"native provider lost the native file capability");
	testRequire(xrtVfsUnmount(NativeMount), "native provider unmount failed");
	xrtVfsMountDestroy(NativeMount);
	xrtVfsDestroy(Vfs);
	testRequire(Context.Releases == 0u,
		"native file did not retain its provider generation");
	testRequire(xrtRead(File, sBuffer, sizeof(sBuffer), &iDone) &&
		(iDone == 6u) && (memcmp(sBuffer, "native", 6u) == 0),
		"native provider file failed after namespace destruction");
	testRequire(xrtClose(File), "native provider file close failed");
	testRequire((Context.Retains == 1u) && (Context.Releases == 1u),
		"native file generation was not released exactly once");

	memset(&LegacyProvider, 0, sizeof(LegacyProvider));
	LegacyProvider.Size = (uint32)sizeof(LegacyProvider);
	LegacyProvider.Version = XRT_VFS_PROVIDER_VERSION;
	LegacyProvider.Capabilities =
		XVFS_PROVIDER_STAT | XVFS_PROVIDER_DIRECTORY;
	LegacyProvider.ContextRetain = testVfsContextRetain;
	LegacyProvider.ContextRelease = testVfsContextRelease;
	LegacyProvider.Open = testVfsOpen;
	LegacyProvider.Stat = testVfsStat;
	LegacyProvider.DirOpen = testVfsDirOpen;
	testRequire(sizeof(LegacyProvider) ==
		offsetof(xvfsprovider_v1, OpenNative),
		"legacy provider fixture does not match the original v1 prefix");
	Vfs = xrtVfsCreate();
	LegacyMount = xrtVfsMount(Vfs, "/legacy", 0,
		XVFS_CASE_SENSITIVE,
		(const xvfsprovider_v1*)(const void*)&LegacyProvider,
		&LegacyContext, 0u);
	testRequire(LegacyMount != NULL,
		"VFS rejected a provider using the original v1 table size");
	File = xrtVfsOpen(Vfs, "/legacy/file", NULL);
	testRequire(File != NULL, "legacy-size provider lookup failed");
	testVfsReadText(File, "legacy");
	testRequire(xrtClose(File), "legacy-size provider close failed");
	testRequire(xrtVfsUnmount(LegacyMount),
		"legacy-size provider unmount failed");
	xrtVfsMountDestroy(LegacyMount);
	xrtVfsDestroy(Vfs);
	testRequire(xrtFileDelete(sPath), "native-provider fixture cleanup failed");
}



static void testVfsDirectory(void)
{
	const cstr HighNames[] = { "b.txt", "A.txt" };
	const cstr LowNames[] = { "a.txt", "c.txt" };
	test_vfs_context High = testVfsContext("none", "", HighNames, 2u);
	test_vfs_context Low = testVfsContext("none", "", LowNames, 2u);
	xvfs Vfs = xrtVfsCreate();
	xvfsmount HighMount = xrtVfsMount(Vfs, "/app", 10,
		XVFS_CASE_ASCII_INSENSITIVE, &testVfsProvider, &High, 0u);
	xvfsmount LowMount = xrtVfsMount(Vfs, "/app", 0,
		XVFS_CASE_SENSITIVE, &testVfsProvider, &Low, 0u);
	xdir Dir;
	xdirentry Entry;
	const cstr Expected[] = { "A.txt", "b.txt", "c.txt" };
	size_t i = 0u;

	testRequire((Vfs != NULL) && (HighMount != NULL) && (LowMount != NULL),
		"directory fixture mount failed");
	Dir = xrtVfsDirOpen(Vfs, "/app", 0u);
	testRequire(Dir != NULL, "merged VFS directory open failed");
	testRequire(strcmp(xrtDirPath(Dir), "/app") == 0,
		"VFS directory lost its virtual path");
	while ( xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM ) {
		str sPath;
		testRequire((i < 3u) &&
			xrtStrEqual(Entry.Name, xrtStrView(Expected[i])),
			"merged VFS directory order or shadowing is wrong");
		sPath = xrtDirEntryPath(Dir, &Entry);
		testRequire((sPath != NULL) && (strchr(sPath, '\\') == NULL),
			"VFS directory entry path used a platform separator");
		xrtFree(sPath);
		i++;
	}
	testRequire(i == 3u, "merged VFS directory entry count is wrong");
	testRequire(xrtDirClose(Dir), "merged VFS directory close failed");
	testRequire(xrtVfsUnmount(HighMount) && xrtVfsUnmount(LowMount),
		"directory fixture unmount failed");
	xrtVfsMountDestroy(HighMount);
	xrtVfsMountDestroy(LowMount);
	xrtVfsDestroy(Vfs);
}



static void testVfsErrors(void)
{
	test_vfs_context Context = testVfsContext("file", "value", NULL, 0u);
	static const cstr arrInvalid[] = {
		"relative", "/a/../b", "/a/./b", "/a//b", "/a/", "/a\\b"
	};
	char sInvalidUtf8[] = { '/', (char)0xc0, (char)0xaf, 0 };
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount = xrtVfsMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, &testVfsProvider, &Context, 0u);
	xvfsmount BoundaryMount;
	xvfsprovider_v1 InvalidProvider;
	xfileoptions AsyncOptions = {
		XFILE_READ | XFILE_ASYNC, 0u, XFILE_SHARE_ALL
	};
	bytes pData;
	str sBoundary;
	size_t iSize = 99u;

	testRequire((Vfs != NULL) && (Mount != NULL), "error fixture setup failed");
	for ( size_t i = 0u; i < sizeof(arrInvalid) / sizeof(arrInvalid[0]); i++ ) {
		testRequire(xrtVfsOpen(Vfs, arrInvalid[i], NULL) == NULL,
			"VFS accepted a non-canonical path");
		xrtClearError();
	}
	testRequire(xrtVfsOpen(Vfs, sInvalidUtf8, NULL) == NULL,
		"VFS accepted invalid UTF-8");
	xrtClearError();
	sBoundary = (str)xrtMalloc(XRT_VFS_PATH_MAX + 2u);
	testRequire(sBoundary != NULL, "VFS path-boundary allocation failed");
	sBoundary[0] = '/';
	memset(sBoundary + 1u, 'a', XRT_VFS_PATH_MAX - 1u);
	sBoundary[XRT_VFS_PATH_MAX] = 0;
	BoundaryMount = xrtVfsMount(Vfs, sBoundary, 0,
		XVFS_CASE_SENSITIVE, &testVfsProvider, &Context, 0u);
	testRequire(BoundaryMount != NULL,
		"VFS rejected a canonical path at XRT_VFS_PATH_MAX");
	testRequire(xrtVfsUnmount(BoundaryMount),
		"VFS maximum-length prefix unmount failed");
	xrtVfsMountDestroy(BoundaryMount);
	sBoundary[XRT_VFS_PATH_MAX] = 'a';
	sBoundary[XRT_VFS_PATH_MAX + 1u] = 0;
	testRequire(xrtVfsOpen(Vfs, sBoundary, NULL) == NULL,
		"VFS accepted a path longer than XRT_VFS_PATH_MAX");
	xrtFree(sBoundary);
	xrtClearError();
	pData = xrtVfsReadAllLimit(Vfs, "/file", 3u, &iSize);
	testRequire((pData == NULL) && (iSize == 99u),
		"read-all limit modified output or accepted excess data");
	xrtClearError();
	Context.BreakMiss = true;
	testRequire(xrtVfsOpen(Vfs, "/missing", NULL) == NULL,
		"VFS accepted provider output on MISS");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"provider protocol violation reported the wrong error");
	xrtClearError();
	Context.BreakMiss = false;
	Context.FailOpen = true;
	testRequire(xrtVfsOpen(Vfs, "/file", NULL) == NULL,
		"VFS accepted provider ERROR without an error object");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL),
		"missing provider error was not synthesized");
	xrtClearError();
	Context.FailOpen = false;
	{
		uint32 iCloses = Context.Closes;
		testRequire(xrtVfsOpen(Vfs, "/file", &AsyncOptions) == NULL,
			"callback provider accepted native async I/O");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
			(Context.Closes == iCloses + 1u),
			"callback async rejection did not close transferred state");
		xrtClearError();
	}
	InvalidProvider = testVfsNativeProvider;
	InvalidProvider.Open = testVfsOpen;
	testRequire(xrtVfsMount(Vfs, "/invalid-native", 0,
		XVFS_CASE_SENSITIVE, &InvalidProvider, &Context, 0u) == NULL,
		"VFS accepted both callback and native open contracts");
	xrtClearError();
	InvalidProvider = testVfsNativeProvider;
	InvalidProvider.OpenNative = NULL;
	testRequire(xrtVfsMount(Vfs, "/missing-native", 0,
		XVFS_CASE_SENSITIVE, &InvalidProvider, &Context, 0u) == NULL,
		"VFS accepted a missing native open callback");
	xrtClearError();
	testRequire(xrtVfsUnmount(Mount), "error fixture unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
}



int main(void)
{
	testVfsLookupAndLifetime();
	testVfsMountOrder();
	testVfsCasePolicyPropagation();
	testVfsOpenFileOutlivesNamespace();
	testVfsNativeOpenAndLegacySize();
	testVfsDirectory();
	testVfsErrors();
	return 0;
}
