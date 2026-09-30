#ifdef VFS_PACK_SOURCE_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/vfs.h>
#endif

#include "../test.h"
#include "vfs_pack_fixture.h"



typedef enum test_vfs_pack_source_mode {
	TEST_VFS_PACK_SOURCE_CHUNKED = 0,
	TEST_VFS_PACK_SOURCE_EOF,
	TEST_VFS_PACK_SOURCE_ERROR,
	TEST_VFS_PACK_SOURCE_NO_READ_AT
} test_vfs_pack_source_mode;

typedef struct test_vfs_pack_source {
	const uint8* Data;
	size_t Size;
	test_vfs_pack_source_mode Mode;
	uint32 Retains;
	uint32 Releases;
	uint32 Closes;
} test_vfs_pack_source;



static bool testVfsPackSourceRead(void* pState, void* pBuffer,
	size_t iRequest, size_t* pRead)
{
	(void)pState;
	(void)pBuffer;
	(void)iRequest;
	*pRead = 0u;
	return true;
}

static bool testVfsPackSourceReadAt(void* pState, uint64 iOffset,
	void* pBuffer, size_t iRequest, size_t* pRead)
{
	test_vfs_pack_source* pSource = (test_vfs_pack_source*)pState;
	size_t iAvailable;
	size_t iDone;

	if ( pSource->Mode == TEST_VFS_PACK_SOURCE_ERROR ) {
		xrtSetErrorInfo(XERR_IO, "test.vfs-pack-source", 77,
			"injected source read failure");
		return false;
	}
	if ( iOffset >= pSource->Size ) {
		*pRead = 0u;
		return true;
	}
	iAvailable = pSource->Size - (size_t)iOffset;
	if ( pSource->Mode == TEST_VFS_PACK_SOURCE_EOF ) {
		if ( iAvailable <= 1u ) {
			*pRead = 0u;
			return true;
		}
		iAvailable--;
	}
	iDone = iRequest < iAvailable ? iRequest : iAvailable;
	if ( (pSource->Mode == TEST_VFS_PACK_SOURCE_CHUNKED) &&
		(iDone > 3u) ) iDone = 3u;
	if ( iDone != 0u )
		memcpy(pBuffer, pSource->Data + (size_t)iOffset, iDone);
	*pRead = iDone;
	return true;
}

static bool testVfsPackSourceStat(void* pState, xfileinfo* pInfo)
{
	test_vfs_pack_source* pSource = (test_vfs_pack_source*)pState;

	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = pSource->Size;
	return true;
}

static void testVfsPackSourceClose(void* pState)
{
	((test_vfs_pack_source*)pState)->Closes++;
}

static const xvfsfileops_v1 testVfsPackSourceReadAtOps = {
	(uint32)sizeof(xvfsfileops_v1), XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ_AT | XVFS_FILE_STAT,
	NULL, NULL, testVfsPackSourceReadAt, NULL, NULL,
	testVfsPackSourceStat, NULL, NULL, testVfsPackSourceClose
};

static const xvfsfileops_v1 testVfsPackSourceReadOps = {
	(uint32)sizeof(xvfsfileops_v1), XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ | XVFS_FILE_STAT,
	testVfsPackSourceRead, NULL, NULL, NULL, NULL,
	testVfsPackSourceStat, NULL, NULL, testVfsPackSourceClose
};



static void testVfsPackSourceRetain(void* pContext)
{
	((test_vfs_pack_source*)pContext)->Retains++;
}

static void testVfsPackSourceRelease(void* pContext)
{
	((test_vfs_pack_source*)pContext)->Releases++;
}

static xvfslookup testVfsPackSourceOpen(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	const xfileoptions* pOptions, xvfsfile_v1* pFile)
{
	test_vfs_pack_source* pSource = (test_vfs_pack_source*)pContext;
	(void)CaseMode;

	if ( !xrtStrEqual(RelativePath, XRT_STR_LITERAL("archive")) )
		return XVFS_LOOKUP_MISS;
	pFile->Ops = pSource->Mode == TEST_VFS_PACK_SOURCE_NO_READ_AT ?
		&testVfsPackSourceReadOps : &testVfsPackSourceReadAtOps;
	pFile->State = pSource;
	pFile->Flags = pOptions->Flags;
	return XVFS_LOOKUP_OPENED;
}

static const xvfsprovider_v1 testVfsPackSourceProvider = {
	(uint32)sizeof(xvfsprovider_v1), XRT_VFS_PROVIDER_VERSION, 0u,
	testVfsPackSourceRetain, testVfsPackSourceRelease,
	testVfsPackSourceOpen, NULL, NULL, NULL
};



static xfile testVfsPackSourceFile(test_vfs_pack_source* pSource)
{
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount;
	xfile File;

	testRequire(Vfs != NULL, "pack source VFS creation failed");
	Mount = xrtVfsMount(Vfs, "/", 0, XVFS_CASE_SENSITIVE,
		&testVfsPackSourceProvider, pSource, 0u);
	testRequire(Mount != NULL, "pack source mount failed");
	File = xrtVfsOpen(Vfs, "/archive", NULL);
	testRequire(File != NULL, "pack source open failed");
	testRequire(xrtVfsUnmount(Mount), "pack source unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	return File;
}



static void testVfsPackSourceCase(const uint8* pArchive,
	size_t iArchiveSize, test_vfs_pack_source_mode Mode)
{
	test_vfs_pack_source SourceState;
	xfile Source;
	xvfspack Pack;

	memset(&SourceState, 0, sizeof(SourceState));
	SourceState.Data = pArchive;
	SourceState.Size = iArchiveSize;
	SourceState.Mode = Mode;
	Source = testVfsPackSourceFile(&SourceState);
	Pack = xrtVfsPackCreate(Source, 0u, iArchiveSize, NULL);
	if ( Mode == TEST_VFS_PACK_SOURCE_CHUNKED ) {
		testRequire((Pack != NULL) &&
			(xrtVfsPackFormat(Pack) == XVFS_PACK_XRT_V1),
			"pack parser did not tolerate legal short reads");
		xrtVfsPackDestroy(Pack);
	} else {
		testRequire(Pack == NULL,
			"pack parser accepted an injected source failure");
		if ( Mode == TEST_VFS_PACK_SOURCE_ERROR ) {
			testRequire(xrtErrorFind(xrtGetError(),
				"test.vfs-pack-source", 77) != NULL,
				"pack parser replaced the source I/O error");
		} else if ( Mode == TEST_VFS_PACK_SOURCE_NO_READ_AT ) {
			testRequire((xrtGetError() != NULL) &&
				(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
				"pack parser reported a wrong missing ReadAt error");
		} else {
			testRequire(xrtGetError() != NULL,
				"pack parser omitted the premature EOF error");
		}
		testRequire(xrtClose(Source),
			"failed pack Create did not preserve source ownership");
		xrtClearError();
	}
	testRequire((SourceState.Retains == 1u) &&
		(SourceState.Releases == 1u) && (SourceState.Closes == 1u),
		"pack source ownership was not released exactly once");
}



int main(void)
{
	static const uint8 Payload[] = "fault-injected source";
	bytes pArchive;
	size_t iArchiveSize;

	pArchive = testVfsPackFixtureBuild("payload",
		Payload, sizeof(Payload) - 1u, Payload, sizeof(Payload) - 1u,
		XVFS_PACK_STORE, false, &iArchiveSize);
	testVfsPackSourceCase(pArchive, iArchiveSize,
		TEST_VFS_PACK_SOURCE_CHUNKED);
	testVfsPackSourceCase(pArchive, iArchiveSize,
		TEST_VFS_PACK_SOURCE_EOF);
	testVfsPackSourceCase(pArchive, iArchiveSize,
		TEST_VFS_PACK_SOURCE_ERROR);
	testVfsPackSourceCase(pArchive, iArchiveSize,
		TEST_VFS_PACK_SOURCE_NO_READ_AT);
	xrtFree(pArchive);
	return 0;
}
