#include <stddef.h>
#include <stdint.h>

#include <xrt/error.h>
#include <xrt/vfs.h>



#define XRT_VFS_PACK_FUZZ_MAX_INPUT ((size_t)1048576u)
#define XRT_VFS_PACK_FUZZ_MAX_ENTRIES 4096u
#define XRT_VFS_PACK_FUZZ_MAX_INDEX UINT64_C(1048576)
#define XRT_VFS_PACK_FUZZ_MAX_ENTRY UINT64_C(1048576)
#define XRT_VFS_PACK_FUZZ_MAX_DECODED UINT64_C(4194304)
#define XRT_VFS_PACK_FUZZ_CACHE UINT64_C(131072)



/* 用 memory provider 把任意字节包装为同步 ReadAt xfile。 */
static xfile __xrtVfsPackFuzzSource(const uint8* pData, size_t iSize)
{
	xvfsmemory Memory = NULL;
	xvfs Vfs = NULL;
	xvfsmount Mount = NULL;
	xfile Source = NULL;

	Memory = xrtVfsMemoryCreate();
	Vfs = xrtVfsCreate();
	if ( (Memory == NULL) || (Vfs == NULL) ) goto cleanup;
	if ( !xrtVfsMemoryPutCopy(Memory, "archive", pData, iSize) ||
		!xrtVfsMemorySeal(Memory) ) goto cleanup;
	Mount = xrtVfsMemoryMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Memory, 0u);
	if ( Mount == NULL ) goto cleanup;
	Source = xrtVfsOpen(Vfs, "/archive", NULL);

cleanup:
	if ( Mount != NULL ) {
		(void)xrtVfsUnmount(Mount);
		xrtVfsMountDestroy(Mount);
	}
	xrtVfsDestroy(Vfs);
	xrtVfsMemoryDestroy(Memory);
	return Source;
}



/* 成功解析的归档继续走挂载、目录和内容解码路径。 */
static void __xrtVfsPackFuzzProbe(xvfspack Pack)
{
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount = NULL;
	xfile File = NULL;
	xdir Dir = NULL;
	uint8 Buffer[4096];
	size_t iRead = 0u;
	xdirentry Entry;
	size_t iEntries = 0u;

	if ( Vfs == NULL ) goto cleanup;
	Mount = xrtVfsPackMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Pack, 0u);
	if ( Mount == NULL ) goto cleanup;
	File = xrtVfsOpen(Vfs, "/payload", NULL);
	if ( File != NULL ) {
		(void)xrtRead(File, Buffer, sizeof(Buffer), &iRead);
		(void)xrtClose(File);
		File = NULL;
	}
	xrtClearError();
	Dir = xrtVfsDirOpen(Vfs, "/", 0u);
	if ( Dir != NULL ) {
		while ( (iEntries < 64u) &&
			(xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM) ) iEntries++;
		(void)xrtDirClose(Dir);
		Dir = NULL;
	}

cleanup:
	if ( Dir != NULL ) (void)xrtDirClose(Dir);
	if ( File != NULL ) (void)xrtClose(File);
	if ( Mount != NULL ) {
		(void)xrtVfsUnmount(Mount);
		xrtVfsMountDestroy(Mount);
	}
	xrtVfsDestroy(Vfs);
}



/* 统一公开确定性回归和 libFuzzer 使用的 pack 输入入口。 */
int xrtVfsPackFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	xvfspackoptions Options;
	xfile Source;
	xvfspack Pack;

	if ( ((pData == NULL) && (iSize != 0u)) ||
		(iSize > XRT_VFS_PACK_FUZZ_MAX_INPUT) ) return 0;
	Source = __xrtVfsPackFuzzSource(pData, iSize);
	if ( Source == NULL ) {
		xrtClearError();
		return 0;
	}
	xrtVfsPackOptionsInit(&Options);
	Options.MaxEntries = XRT_VFS_PACK_FUZZ_MAX_ENTRIES;
	Options.MaxIndexBytes = XRT_VFS_PACK_FUZZ_MAX_INDEX;
	Options.MaxEntryBytes = XRT_VFS_PACK_FUZZ_MAX_ENTRY;
	Options.MaxDecodedBytes = XRT_VFS_PACK_FUZZ_MAX_DECODED;
	Options.CacheBytes = XRT_VFS_PACK_FUZZ_CACHE;
	Pack = xrtVfsPackCreate(Source, 0u, (uint64)iSize, &Options);
	if ( Pack == NULL ) {
		(void)xrtClose(Source);
		xrtClearError();
		return 0;
	}
	__xrtVfsPackFuzzProbe(Pack);
	xrtVfsPackDestroy(Pack);
	xrtClearError();
	return 0;
}



#if defined(XRT_VFS_PACK_FUZZ_LIBFUZZER)

/* 把独立 pack 入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtVfsPackFuzzerTestOneInput(pData, iSize);
}

#endif
