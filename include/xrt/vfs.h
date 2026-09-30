#ifndef XRT_VFS_H
#define XRT_VFS_H

#include <xrt/file.h>



#if defined(XRT_FEATURE_VFS) && \
	(!defined(XRT_FEATURE_DIR) || !defined(XRT_FEATURE_MUTEX) || \
	 !defined(XRT_FEATURE_UNICODE))
	#error "XRT VFS requires directory, mutex, and Unicode support"
#endif

#if defined(XRT_FEATURE_VFS_MEMORY) && !defined(XRT_FEATURE_VFS)
	#error "XRT memory VFS provider requires VFS support"
#endif

#if defined(XRT_FEATURE_VFS_DISK) && \
	(!defined(XRT_FEATURE_VFS) || !defined(XRT_FEATURE_FILE_ROOT))
	#error "XRT disk VFS provider requires VFS and file-root support"
#endif

#if defined(XRT_FEATURE_VFS_PACK) && \
	(!defined(XRT_FEATURE_VFS) || !defined(XRT_FEATURE_COND))
	#error "XRT pack VFS provider requires VFS and condition-variable support"
#endif



#define XRT_VFS_PATH_MAX 32768u
#define XRT_VFS_PROVIDER_VERSION 1u
#define XRT_VFS_FILE_OPS_VERSION 1u
#define XRT_VFS_DIR_OPS_VERSION 1u
#define XRT_VFS_PACK_OPTIONS_VERSION 1u

#define XRT_VFS_PACK_MAX_ENTRIES_DEFAULT 100000u
#define XRT_VFS_PACK_MAX_INDEX_BYTES_DEFAULT UINT64_C(67108864)
#define XRT_VFS_PACK_MAX_ENTRY_BYTES_DEFAULT UINT64_C(1073741824)
#define XRT_VFS_PACK_MAX_DECODED_BYTES_DEFAULT UINT64_C(8589934592)
#define XRT_VFS_PACK_CACHE_BYTES_DEFAULT UINT64_C(67108864)



#if defined(XRT_FEATURE_VFS)

typedef struct xvfs_impl* xvfs;
typedef struct xvfs_mount_impl* xvfsmount;

#if defined(XRT_FEATURE_VFS_MEMORY)
typedef struct xvfs_memory_impl* xvfsmemory;
#endif

#if defined(XRT_FEATURE_VFS_DISK)
typedef struct xvfs_disk_impl* xvfsdisk;
#endif

#if defined(XRT_FEATURE_VFS_PACK)
typedef struct xvfs_pack_impl* xvfspack;
#endif



/* Provider lookup 明确区分资源不存在和不可回退的访问失败。 */
typedef enum xvfslookup {
	XVFS_LOOKUP_ERROR = -1,
	XVFS_LOOKUP_MISS = 0,
	XVFS_LOOKUP_OPENED = 1
} xvfslookup;



/* 每个 mount 显式声明大小写策略；v1 只折叠 ASCII A-Z。 */
typedef enum xvfscase {
	XVFS_CASE_SENSITIVE = 0,
	XVFS_CASE_ASCII_INSENSITIVE = 1
} xvfscase;



#if defined(XRT_FEATURE_VFS_DISK)

/* Disk provider 权限在 context 创建时冻结。 */
typedef enum xvfsdiskaccess {
	XVFS_DISK_READ = 0x01,
	XVFS_DISK_WRITE = 0x02
} xvfsdiskaccess;

#define XVFS_DISK_ACCESS 0x03u

#endif



#if defined(XRT_FEATURE_VFS_PACK)

/* AUTO 只按完整 header magic 选择格式，不把解析失败降级到另一格式。 */
typedef enum xvfspackformat {
	XVFS_PACK_AUTO = 0,
	XVFS_PACK_XRT_V1 = 1,
	XVFS_PACK_XSVPACK_V1 = 2
} xvfspackformat;



/* 新格式和旧 XSVPACK v1 使用相同的逐条 codec 编号。 */
typedef enum xvfspackcodec {
	XVFS_PACK_STORE = 0,
	XVFS_PACK_LZMA1 = 1
} xvfspackcodec;



/* 非零解析限制是硬上限；CacheBytes 为零表示验证后不保留 cache 引用。 */
typedef struct xvfspackoptions {
	uint32 Size;
	uint32 Version;
	xvfspackformat Format;
	uint32 MaxEntries;
	uint64 MaxIndexBytes;
	uint64 MaxEntryBytes;
	uint64 MaxDecodedBytes;
	uint64 CacheBytes;
} xvfspackoptions;



/* 计数单调递增；ResidentBytes 是取样时 cache 持有的解压字节。 */
typedef struct xvfspackstats {
	uint64 Hits;
	uint64 Misses;
	uint64 Loads;
	uint64 Failures;
	uint64 ResidentBytes;
	uint64 Evictions;
} xvfspackstats;

#endif



/* xrt.vfs 错误域中的稳定错误代码。 */
typedef enum xvfserror {
	XVFS_ERROR_CREATE = 1,
	XVFS_ERROR_PATH,
	XVFS_ERROR_MOUNT,
	XVFS_ERROR_UNMOUNT,
	XVFS_ERROR_NOT_FOUND,
	XVFS_ERROR_PROVIDER,
	XVFS_ERROR_OPEN,
	XVFS_ERROR_STAT,
	XVFS_ERROR_DIRECTORY,
	XVFS_ERROR_UNSUPPORTED,
	XVFS_ERROR_LIMIT,
	XVFS_ERROR_CORRUPT,
	XVFS_ERROR_CHECKSUM
} xvfserror;



/* 文件能力位必须与对应回调严格一致。 */
typedef enum xvfsfilecapability {
	XVFS_FILE_READ = UINT64_C(0x00000001),
	XVFS_FILE_WRITE = UINT64_C(0x00000002),
	XVFS_FILE_READ_AT = UINT64_C(0x00000004),
	XVFS_FILE_WRITE_AT = UINT64_C(0x00000008),
	XVFS_FILE_SEEK = UINT64_C(0x00000010),
	XVFS_FILE_STAT = UINT64_C(0x00000020),
	XVFS_FILE_RESIZE = UINT64_C(0x00000040),
	XVFS_FILE_FLUSH = UINT64_C(0x00000080)
} xvfsfilecapability;

#define XVFS_FILE_CAPABILITIES UINT64_C(0x000000ff)



typedef struct xvfsfileops_v1 {
	uint32 Size;
	uint32 Version;
	uint64 Capabilities;
	bool (*Read)(void* pState, void* pBuffer,
		size_t iRequest, size_t* pRead);
	bool (*Write)(void* pState, const void* pBuffer,
		size_t iRequest, size_t* pWritten);
	bool (*ReadAt)(void* pState, uint64 iOffset, void* pBuffer,
		size_t iRequest, size_t* pRead);
	bool (*WriteAt)(void* pState, uint64 iOffset, const void* pBuffer,
		size_t iRequest, size_t* pWritten);
	bool (*Seek)(void* pState, int64 iOffset,
		xseek Origin, uint64* pPosition);
	bool (*Stat)(void* pState, xfileinfo* pInfo);
	bool (*Resize)(void* pState, uint64 iSize);
	bool (*Flush)(void* pState);
	void (*Close)(void* pState);
} xvfsfileops_v1;



/* OPENED 把 State 所有权转移给 VFS；MISS 和 ERROR 必须保持全零。 */
typedef struct xvfsfile_v1 {
	const xvfsfileops_v1* Ops;
	void* State;
	uint32 Flags;
	uint32 Reserved;
} xvfsfile_v1;



typedef struct xvfsdirops_v1 {
	uint32 Size;
	uint32 Version;
	bool (*Next)(void* pState, xdirentry* pEntry, bool* pEnd);
	void (*Close)(void* pState);
} xvfsdirops_v1;



/* OPENED 把 State 所有权转移给 VFS；MISS 和 ERROR 必须保持全零。 */
typedef struct xvfsdir_v1 {
	const xvfsdirops_v1* Ops;
	void* State;
} xvfsdir_v1;



typedef enum xvfsprovidercapability {
	XVFS_PROVIDER_STAT = UINT64_C(0x00000001),
	XVFS_PROVIDER_DIRECTORY = UINT64_C(0x00000002),
	XVFS_PROVIDER_NATIVE_OPEN = UINT64_C(0x00000004)
} xvfsprovidercapability;

#define XVFS_PROVIDER_CAPABILITIES UINT64_C(0x00000007)



typedef struct xvfsprovider_v1 {
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
	/* 可选尾字段；仅在声明 NATIVE_OPEN 时使用并直接转移 xfile 所有权。 */
	xvfslookup (*OpenNative)(void* pContext, xvfscase CaseMode,
		xstrview RelativePath,
		const xfileoptions* pOptions, xfile* pFile);
} xvfsprovider_v1;



XRT_EXTERN_C_BEGIN

/* 创建一个初始 mount snapshot 为空的隔离命名空间。 */
XRT_API xvfs xrtVfsCreate(void);

/* 增加一个命名空间 owned reference。 */
XRT_API void xrtVfsRef(xvfs Vfs);

/* 释放一个命名空间 reference。 */
XRT_API void xrtVfsDestroy(xvfs Vfs);

/* 在规范虚拟 prefix 上发布一个 provider generation。 */
XRT_API xvfsmount xrtVfsMount(xvfs Vfs, cstr sVirtualPrefix,
	int32 iPriority, xvfscase CaseMode,
	const xvfsprovider_v1* pProvider, void* pProviderContext,
	uint32 iFlags);

/* 增加或释放一个 mount 控制句柄 reference。 */
XRT_API void xrtVfsMountRef(xvfsmount Mount);
XRT_API void xrtVfsMountDestroy(xvfsmount Mount);

/* 幂等地阻止新 lookup 选择这个 generation。 */
XRT_API bool xrtVfsUnmount(xvfsmount Mount);

/* 返回稳定、单调分配的 mount 标识。 */
XRT_API uint64 xrtVfsMountId(xvfsmount Mount);

/* 把一个规范绝对虚拟路径打开为普通 xfile。 */
XRT_API xfile xrtVfsOpen(xvfs Vfs, cstr sVirtualPath,
	const xfileoptions* pOptions);

/* 查询一个规范绝对虚拟路径且不打开文件。 */
XRT_API bool xrtVfsStat(xvfs Vfs, cstr sVirtualPath,
	bool bFollowLink, xfileinfo* pInfo);

/* 读取完整虚拟文件，并在 pSize 之外追加一个零字节。 */
XRT_API bytes xrtVfsReadAll(xvfs Vfs, cstr sVirtualPath,
	size_t* pSize);
XRT_API bytes xrtVfsReadAllLimit(xvfs Vfs, cstr sVirtualPath,
	size_t iLimit, size_t* pSize);

/* 物化并合并所有可见 provider 条目，返回普通 xdir。 */
XRT_API xdir xrtVfsDirOpen(xvfs Vfs, cstr sVirtualPath,
	uint32 iFlags);

#if defined(XRT_FEATURE_VFS_MEMORY)

/* 创建可变的 memory provider 构建器。 */
XRT_API xvfsmemory xrtVfsMemoryCreate(void);

/* 增加或释放一个 memory provider reference。 */
XRT_API void xrtVfsMemoryRef(xvfsmemory Memory);
XRT_API void xrtVfsMemoryDestroy(xvfsmemory Memory);

/* 复制文件数据；相对路径必须规范且不能为空。 */
XRT_API bool xrtVfsMemoryPutCopy(xvfsmemory Memory,
	cstr sRelativePath, const void* pData, size_t iSize);

/* 成功时接管由 xrt 分配的缓冲，失败时调用方仍持有它。 */
XRT_API bool xrtVfsMemoryPutOwned(xvfsmemory Memory,
	cstr sRelativePath, bytes pData, size_t iSize);

/* 原子构建不可变目录索引；重复调用成功，分配失败可直接重试。 */
XRT_API bool xrtVfsMemorySeal(xvfsmemory Memory);

/* 只允许挂载已经 seal 的 memory provider。 */
XRT_API xvfsmount xrtVfsMemoryMount(xvfs Vfs,
	cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode,
	xvfsmemory Memory, uint32 iFlags);

#endif

#if defined(XRT_FEATURE_VFS_DISK)

/* 打开并锚定物理目录；Access 至少包含一个已知权限位。 */
XRT_API xvfsdisk xrtVfsDiskCreate(cstr sPhysicalRoot, uint32 iAccess);

/* 增加或释放一个 disk provider reference。 */
XRT_API void xrtVfsDiskRef(xvfsdisk Disk);
XRT_API void xrtVfsDiskDestroy(xvfsdisk Disk);

/* 挂载锚定目录；首版始终拒绝符号链接和 reparse point。 */
XRT_API xvfsmount xrtVfsDiskMount(xvfs Vfs,
	cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode,
	xvfsdisk Disk, uint32 iFlags);

#endif

#if defined(XRT_FEATURE_VFS_PACK)

/* 写入生产默认限制和 AUTO 格式。 */
XRT_API void xrtVfsPackOptionsInit(xvfspackoptions* pOptions);

/* 成功时接管 Source，失败时调用方仍持有；范围必须包含完整归档。 */
XRT_API xvfspack xrtVfsPackCreate(xfile Source,
	uint64 iOffset, uint64 iLength, const xvfspackoptions* pOptions);

/* 增加或释放 pack provider reference；最后一个引用关闭 Source。 */
XRT_API void xrtVfsPackRef(xvfspack Pack);
XRT_API void xrtVfsPackDestroy(xvfspack Pack);

/* 返回 Create 最终选择的明确格式。 */
XRT_API xvfspackformat xrtVfsPackFormat(xvfspack Pack);

/* 取得线程安全的缓存统计快照。 */
XRT_API bool xrtVfsPackStats(xvfspack Pack, xvfspackstats* pStats);

/* 挂载已经完整解析并验证索引的只读 pack。 */
XRT_API xvfsmount xrtVfsPackMount(xvfs Vfs,
	cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode,
	xvfspack Pack, uint32 iFlags);

#endif

XRT_EXTERN_C_END

#endif

#endif
