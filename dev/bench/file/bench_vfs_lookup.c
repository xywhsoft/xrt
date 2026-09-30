#include "../bench_common.h"

#define XRT_MODULE_VFS
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



#define BENCH_VFS_MISS_MOUNTS 8u



typedef struct bench_vfs_context {
	cstr Match;
	unsigned char Value;
} bench_vfs_context;



static volatile uint64 benchVfsSink;



static bool benchVfsReadAt(void* pState, uint64 iOffset,
	void* pBuffer, size_t iRequest, size_t* pRead)
{
	bench_vfs_context* pContext = (bench_vfs_context*)pState;

	if ( (iOffset != 0u) || (iRequest == 0u) ) {
		*pRead = 0u;
		return true;
	}
	((unsigned char*)pBuffer)[0] = pContext->Value;
	*pRead = 1u;
	return true;
}

static bool benchVfsFileStat(void* pState, xfileinfo* pInfo)
{
	(void)pState;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = 1u;
	return true;
}

static void benchVfsFileClose(void* pState)
{
	(void)pState;
}

static const xvfsfileops_v1 benchVfsFileOps = {
	(uint32)sizeof(xvfsfileops_v1),
	XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ_AT | XVFS_FILE_STAT,
	NULL, NULL,
	benchVfsReadAt, NULL,
	NULL, benchVfsFileStat,
	NULL, NULL,
	benchVfsFileClose
};

static xvfslookup benchVfsOpen(void* pContext, xvfscase CaseMode,
	xstrview Relative,
	const xfileoptions* pOptions, xvfsfile_v1* pFile)
{
	bench_vfs_context* pProvider = (bench_vfs_context*)pContext;
	(void)CaseMode;

	if ( !xrtStrEqual(Relative, xrtStrView(pProvider->Match)) )
		return XVFS_LOOKUP_MISS;
	pFile->Ops = &benchVfsFileOps;
	pFile->State = pProvider;
	pFile->Flags = pOptions->Flags;
	return XVFS_LOOKUP_OPENED;
}

static xvfslookup benchVfsStat(void* pContext, xvfscase CaseMode,
	xstrview Relative,
	bool bFollowLink, xfileinfo* pInfo)
{
	bench_vfs_context* pProvider = (bench_vfs_context*)pContext;

	(void)CaseMode;
	(void)bFollowLink;
	if ( !xrtStrEqual(Relative, xrtStrView(pProvider->Match)) )
		return XVFS_LOOKUP_MISS;
	(void)benchVfsFileStat(pProvider, pInfo);
	return XVFS_LOOKUP_OPENED;
}

static const xvfsprovider_v1 benchVfsProvider = {
	(uint32)sizeof(xvfsprovider_v1),
	XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_STAT,
	NULL, NULL,
	benchVfsOpen,
	benchVfsStat,
	NULL,
	NULL
};



static bool benchVfsStatLoop(xvfs Vfs, uint64 iOperations,
	uint64* pElapsed)
{
	xbenchtimer Timer;
	uint64 iSum = 0u;

	xbenchTimerStart(&Timer);
	for ( uint64 i = 0u; i < iOperations; i++ ) {
		xfileinfo Info;

		if ( !xrtVfsStat(Vfs, "/asset", false, &Info) ||
			(Info.Size != 1u) ) return false;
		iSum += Info.Size;
	}
	xbenchTimerStop(&Timer);
	*pElapsed = xbenchTimerElapsedNs(&Timer);
	benchVfsSink += iSum;
	return true;
}

static bool benchVfsOpenLoop(xvfs Vfs, uint64 iOperations,
	uint64* pElapsed)
{
	xbenchtimer Timer;
	uint64 iSum = 0u;

	xbenchTimerStart(&Timer);
	for ( uint64 i = 0u; i < iOperations; i++ ) {
		xfile File = xrtVfsOpen(Vfs, "/asset", NULL);
		unsigned char iValue = 0u;
		size_t iRead = 0u;

		if ( (File == NULL) ||
			 !xrtRead(File, &iValue, 1u, &iRead) ||
			 (iRead != 1u) || !xrtClose(File) ) {
			if ( File != NULL ) (void)xrtClose(File);
			return false;
		}
		iSum += iValue;
	}
	xbenchTimerStop(&Timer);
	*pElapsed = xbenchTimerElapsedNs(&Timer);
	benchVfsSink += iSum;
	return true;
}



int main(int argc, char** argv)
{
	uint64 iStatOperations = xbenchArgU64(argc, argv, 1, 1000000u);
	uint64 iOpenOperations = xbenchArgU64(argc, argv, 2, 250000u);
	bench_vfs_context Hit = { "asset", UINT8_C(0x5a) };
	bench_vfs_context Miss = { "other", 0u };
	xvfsmount arrMount[BENCH_VFS_MISS_MOUNTS + 1u];
	xvfs Vfs;
	uint64 iStatElapsed = 0u;
	uint64 iOpenElapsed = 0u;
	bool bCleanup = true;

	if ( (iStatOperations == 0u) || (iOpenOperations == 0u) ) return 1;
	Vfs = xrtVfsCreate();
	if ( Vfs == NULL ) return 1;
	arrMount[0] = xrtVfsMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, &benchVfsProvider, &Hit, 0u);
	if ( arrMount[0] == NULL ) {
		xrtVfsDestroy(Vfs);
		return 1;
	}
	for ( size_t i = 0u; i < BENCH_VFS_MISS_MOUNTS; i++ ) {
		arrMount[i + 1u] = xrtVfsMount(Vfs, "/", (int32)(i + 1u),
			XVFS_CASE_SENSITIVE, &benchVfsProvider, &Miss, 0u);
		if ( arrMount[i + 1u] == NULL ) {
			for ( size_t j = 0u; j <= i; j++ ) {
				(void)xrtVfsUnmount(arrMount[j]);
				xrtVfsMountDestroy(arrMount[j]);
			}
			xrtVfsDestroy(Vfs);
			return 1;
		}
	}
	if ( !benchVfsStatLoop(Vfs, iStatOperations, &iStatElapsed) ||
		 !benchVfsOpenLoop(Vfs, iOpenOperations, &iOpenElapsed) ) {
		bCleanup = false;
	}
	for ( size_t i = 0u; i < BENCH_VFS_MISS_MOUNTS + 1u; i++ ) {
		bCleanup = xrtVfsUnmount(arrMount[i]) && bCleanup;
		xrtVfsMountDestroy(arrMount[i]);
	}
	xrtVfsDestroy(Vfs);
	if ( !bCleanup ) return 1;
	printf("vfs_stat_lookup_ops_per_sec=%.6f\n",
		xbenchSafeRate(iStatOperations, iStatElapsed));
	printf("vfs_stat_lookup_ns_per_op=%.6f\n",
		(double)iStatElapsed / (double)iStatOperations);
	printf("vfs_open_read_close_ops_per_sec=%.6f\n",
		xbenchSafeRate(iOpenOperations, iOpenElapsed));
	printf("vfs_open_read_close_ns_per_op=%.6f\n",
		(double)iOpenElapsed / (double)iOpenOperations);
	return benchVfsSink == 0u ? 1 : 0;
}
