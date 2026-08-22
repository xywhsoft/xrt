#ifndef XRT_FILE_H
#define XRT_FILE_H

#include <xrt/error.h>
#include <xrt/path.h>
#include <xrt/time.h>

#if defined(XRT_FEATURE_FILE_TEXT)
	#include <xrt/charset.h>
#endif



#if defined(XRT_FEATURE_FILE) && \
	(!defined(XRT_FEATURE_PATH_SYSTEM) || !defined(XRT_FEATURE_TIME))
	#error "XRT file support requires path-system and time support"
#endif

#if defined(XRT_FEATURE_FILE_TEMP) && \
	(!defined(XRT_FEATURE_FILE) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT temporary files require file and secure-random support"
#endif

#if defined(XRT_FEATURE_FILE_WHOLE) && !defined(XRT_FEATURE_FILE_TEMP)
	#error "XRT whole-file support requires XRT_FEATURE_FILE_TEMP"
#endif

#if defined(XRT_FEATURE_FILE_TEXT) && !defined(XRT_FEATURE_FILE_WHOLE)
	#error "XRT file text requires XRT_FEATURE_FILE_WHOLE"
#endif

#if defined(XRT_FEATURE_FILE_TEXT) && !defined(XRT_FEATURE_CHARSET)
	#error "XRT file text requires XRT_FEATURE_CHARSET"
#endif

#if defined(XRT_FEATURE_FILE_TEXT) && !defined(XRT_FEATURE_CHARSET_DETECT)
	#error "XRT file text requires XRT_FEATURE_CHARSET_DETECT"
#endif

#if defined(XRT_FEATURE_DIR) && !defined(XRT_FEATURE_FILE)
	#error "XRT directory support requires XRT_FEATURE_FILE"
#endif

#if defined(XRT_FEATURE_DIR_TEMP) && \
	(!defined(XRT_FEATURE_DIR) || !defined(XRT_FEATURE_FILE_TEMP))
	#error "XRT temporary directories require directory and temporary-file support"
#endif

#if defined(XRT_FEATURE_FILE_WALK) && !defined(XRT_FEATURE_DIR)
	#error "XRT file walking requires XRT_FEATURE_DIR"
#endif

#if defined(XRT_FEATURE_FILE_LINK) && !defined(XRT_FEATURE_FILE)
	#error "XRT file links require XRT_FEATURE_FILE"
#endif

#if defined(XRT_FEATURE_FILE_ROOT) && \
	(!defined(XRT_FEATURE_FILE) || !defined(XRT_FEATURE_FILE_LINK))
	#error "XRT file roots require file and file-link support"
#endif

#if defined(XRT_FEATURE_FILE_FIFO) && !defined(XRT_FEATURE_FILE)
	#error "XRT FIFO support requires XRT_FEATURE_FILE"
#endif

#if defined(XRT_FEATURE_FILE_LOCK) && !defined(XRT_FEATURE_FILE)
	#error "XRT file locking requires XRT_FEATURE_FILE"
#endif

#if defined(XRT_FEATURE_FILE_MAP) && !defined(XRT_FEATURE_FILE)
	#error "XRT file mapping requires XRT_FEATURE_FILE"
#endif

#if defined(XRT_FEATURE_FILE_TREE) && \
	(!defined(XRT_FEATURE_FILE_WALK) || !defined(XRT_FEATURE_FILE_WHOLE) || \
	 !defined(XRT_FEATURE_FILE_LINK))
	#error "XRT file tree requires file walk, whole-file, and file-link support"
#endif



/* 文件句柄保持不透明，平台句柄只能通过显式逃生接口取得。 */
typedef struct xfile_impl* xfile;



#if defined(XRT_FEATURE_FILE_MAP)

/* 文件映射对象保持不透明，映射地址只借用到解除映射。 */
typedef struct xfilemap_impl* xfilemap;

#endif



#if defined(XRT_FEATURE_FILE_ROOT)

/* 目录根对象持有真实目录句柄，根内路径不能越过该目录。 */
typedef struct xroot_impl* xroot;



/* 目录根模块稳定错误代码。 */
typedef enum xrooterror {
	XROOT_ERROR_OPEN = 1,
	XROOT_ERROR_CLOSE,
	XROOT_ERROR_RESOLVE,
	XROOT_ERROR_ESCAPE,
	XROOT_ERROR_LIMIT,
	XROOT_ERROR_FILE,
	XROOT_ERROR_STAT,
	XROOT_ERROR_CREATE,
	XROOT_ERROR_REMOVE,
	XROOT_ERROR_LINK
} xrooterror;

#endif



/* 打开标志允许组合；至少指定 READ 或 WRITE。 */
typedef enum xfileflag {
	XFILE_READ = 0x0001,
	XFILE_WRITE = 0x0002,
	XFILE_CREATE = 0x0004,
	XFILE_TRUNCATE = 0x0008,
	XFILE_APPEND = 0x0010,
	XFILE_EXCLUSIVE = 0x0020,
	XFILE_NOFOLLOW = 0x0040,
	XFILE_SYNC = 0x0080,
	/* 为完成式异步 I/O 打开；普通 Read/Write API 不接受该对象。 */
	XFILE_ASYNC = 0x0100
} xfileflag;



/* Windows 共享策略；POSIX 接受这些字段但没有对应打开限制。 */
typedef enum xfileshare {
	XFILE_SHARE_READ = 0x01,
	XFILE_SHARE_WRITE = 0x02,
	XFILE_SHARE_DELETE = 0x04,
	XFILE_SHARE_ALL = 0x07
} xfileshare;



/* 高级打开选项；Mode 只使用 POSIX 权限低 12 位。 */
typedef struct xfileoptions {
	uint32 Flags;
	uint32 Mode;
	uint32 Share;
} xfileoptions;



/* 跨平台稳定的文件对象类别。 */
typedef enum xfiletype {
	XFILE_TYPE_NONE = 0,
	XFILE_TYPE_FILE,
	XFILE_TYPE_DIRECTORY,
	XFILE_TYPE_LINK,
	XFILE_TYPE_FIFO,
	XFILE_TYPE_SOCKET,
	XFILE_TYPE_DEVICE,
	XFILE_TYPE_OTHER
} xfiletype;



/* 元数据可用位避免用零伪装平台不提供的时间或身份。 */
typedef enum xfileinfoflag {
	XFILE_INFO_SIZE = 0x0001,
	XFILE_INFO_MODE = 0x0002,
	XFILE_INFO_ACCESS_TIME = 0x0004,
	XFILE_INFO_MODIFY_TIME = 0x0008,
	XFILE_INFO_CREATE_TIME = 0x0010,
	XFILE_INFO_CHANGE_TIME = 0x0020,
	XFILE_INFO_IDENTITY = 0x0040,
	XFILE_INFO_LINK_COUNT = 0x0080
} xfileinfoflag;



/* 文件元数据时间统一使用 Unix Epoch 微秒。 */
typedef struct xfileinfo {
	xfiletype Type;
	uint32 Available;
	uint32 Mode;
	uint32 Attributes;
	uint64 Size;
	uint64 Device;
	uint64 Identity;
	uint64 LinkCount;
	xtime Accessed;
	xtime Modified;
	xtime Created;
	xtime Changed;
} xfileinfo;



/* 文件模块稳定错误代码。 */
typedef enum xfileerror {
	XFILE_ERROR_OPEN = 1,
	XFILE_ERROR_READ,
	XFILE_ERROR_WRITE,
	XFILE_ERROR_SEEK,
	XFILE_ERROR_STAT,
	XFILE_ERROR_RESIZE,
	XFILE_ERROR_SYNC,
	XFILE_ERROR_CLOSE,
	XFILE_ERROR_EOF,
	XFILE_ERROR_COPY,
	XFILE_ERROR_MOVE,
	XFILE_ERROR_DELETE,
	XFILE_ERROR_TEMP,
	XFILE_ERROR_METADATA,
	XFILE_ERROR_TOUCH,
	XFILE_ERROR_TEXT,
	XFILE_ERROR_LIMIT,
	XFILE_ERROR_LOCK,
	XFILE_ERROR_MAP
} xfileerror;



#if defined(XRT_FEATURE_FILE_LOCK)

/* 文件锁支持共享读和排他写两种跨进程模式。 */
typedef enum xfilelock {
	XFILE_LOCK_SHARED = 1,
	XFILE_LOCK_EXCLUSIVE
} xfilelock;

#endif



#if defined(XRT_FEATURE_FILE_MAP)

/* 映射始终可读，可选择共享写或私有写，两种写模式不能同时启用。 */
typedef enum xfilemapflag {
	XFILE_MAP_READ = 0x01,
	XFILE_MAP_WRITE = 0x02,
	XFILE_MAP_COPY = 0x04
} xfilemapflag;

#endif



#if defined(XRT_FEATURE_DIR)

/* 目录迭代器拥有系统枚举句柄，对外保持不透明。 */
typedef struct xdir_impl* xdir;



/* 完整元数据可能增加每个条目一次系统查询，默认只返回枚举器已有信息。 */
typedef enum xdirflag {
	XDIR_STAT = 0x01,
	XDIR_FOLLOW_LINKS = 0x02,
	XDIR_INCLUDE_DOTS = 0x04
} xdirflag;



/* 目录迭代结果明确区分条目、正常结束和失败。 */
typedef enum xdirnext {
	XDIR_NEXT_ERROR = -1,
	XDIR_NEXT_END = 0,
	XDIR_NEXT_ITEM = 1
} xdirnext;



/* POSIX 文件名允许原始字节；该标志表示名称已经通过严格 UTF-8 检查。 */
typedef enum xdirentryflag {
	XDIR_ENTRY_UTF8 = 0x01
} xdirentryflag;



/* 名称和元数据借用到下一次迭代或关闭；Name 始终额外带零结尾。 */
typedef struct xdirentry {
	xstrview Name;
	xfileinfo Info;
	uint32 Flags;
} xdirentry;



/* 系统根目录列表拥有每个字符串以及指针数组。 */
typedef struct xdirroots {
	str* Items;
	size_t Count;
} xdirroots;



/* 目录模块稳定错误代码。 */
typedef enum xdirerror {
	XDIR_ERROR_OPEN = 1,
	XDIR_ERROR_NEXT,
	XDIR_ERROR_CLOSE,
	XDIR_ERROR_CREATE,
	XDIR_ERROR_REMOVE,
	XDIR_ERROR_ROOTS,
	XDIR_ERROR_ENTRY,
	XDIR_ERROR_TEMP
} xdirerror;

#endif



#if defined(XRT_FEATURE_FILE_TREE)

/* 目录树复制默认要求目标不存在并保留符号链接。 */
typedef enum xtreecopyflag {
	XTREE_COPY_MERGE = 0x01,
	XTREE_COPY_REPLACE = 0x02,
	XTREE_COPY_FOLLOW_LINKS = 0x04,
	XTREE_COPY_SKIP_LINKS = 0x08,
	XTREE_COPY_ONE_FILESYSTEM = 0x10,
	XTREE_COPY_SKIP_SPECIAL = 0x20,
	XTREE_COPY_METADATA = 0x40
} xtreecopyflag;



/* 高级目录树复制选项。 */
typedef struct xtreecopyoptions {
	uint32 Flags;
} xtreecopyoptions;



/* 目录树模块稳定错误代码。 */
typedef enum xtreeerror {
	XTREE_ERROR_OPTIONS = 1,
	XTREE_ERROR_SOURCE,
	XTREE_ERROR_TARGET,
	XTREE_ERROR_DESCENDANT,
	XTREE_ERROR_LINK_CYCLE,
	XTREE_ERROR_SPECIAL,
	XTREE_ERROR_ROOT
} xtreeerror;

#endif



#if defined(XRT_FEATURE_FILE_LINK)

/* 链接模块稳定错误代码。 */
typedef enum xlinkerror {
	XLINK_ERROR_CREATE = 1,
	XLINK_ERROR_READ,
	XLINK_ERROR_DELETE,
	XLINK_ERROR_FORMAT
} xlinkerror;

#endif



#if defined(XRT_FEATURE_FILE_FIFO)

/* FIFO 模块稳定错误代码。 */
typedef enum xfifoerror {
	XFIFO_ERROR_CREATE = 1
} xfifoerror;

#endif



#if defined(XRT_FEATURE_FILE_WALK)

/* 遍历默认不跟随链接，可选择限制在根文件系统内。 */
typedef enum xwalkflag {
	XWALK_FOLLOW_LINKS = 0x01,
	XWALK_ONE_FILESYSTEM = 0x02
} xwalkflag;



/* 每个目录产生进入和离开事件，其他对象产生条目事件。 */
typedef enum xwalkevent {
	XWALK_ENTER = 1,
	XWALK_ITEM,
	XWALK_LEAVE
} xwalkevent;



/* 条目标志保留物理链接身份以及不能继续下降的原因。 */
typedef enum xwalkentryflag {
	XWALK_ENTRY_UTF8 = 0x01,
	XWALK_ENTRY_LINK = 0x02,
	XWALK_ENTRY_CYCLE = 0x04,
	XWALK_ENTRY_CROSS_FILESYSTEM = 0x08
} xwalkentryflag;



/* 回调可继续、跳过当前目录、成功停止或报告失败。 */
typedef enum xwalkcontrol {
	XWALK_CONTINUE = 0,
	XWALK_SKIP,
	XWALK_STOP,
	XWALK_ERROR
} xwalkcontrol;



/* 遍历系统错误可终止、跳过当前路径，或成功停止。 */
typedef enum xwalkerroraction {
	XWALK_ERROR_ABORT = 0,
	XWALK_ERROR_SKIP,
	XWALK_ERROR_STOP
} xwalkerroraction;



/* 错误和路径只在回调期间借用；用户数据与条目回调共用。 */
typedef xwalkerroraction (*xwalkerrorproc)(cstr sPath,
	const xerror* pError, ptr pUserData);



/* 遍历选项为空时等价于不跟随链接且深度无限。 */
typedef struct xwalkoptions {
	uint32 Flags;
	size_t MaxDepth;
	xwalkerrorproc OnError;
} xwalkoptions;



/* Path、Parent 和 Name 只在回调期间借用，Info 在跟随链接时描述目标。 */
typedef struct xwalkentry {
	cstr Path;
	xstrview Parent;
	xstrview Name;
	xfileinfo Info;
	xwalkevent Event;
	uint32 Flags;
	size_t Depth;
} xwalkentry;



/* 遍历统计按对象计数，目录只在进入时计一次。 */
typedef struct xwalkstats {
	uint64 Items;
	uint64 Files;
	uint64 Directories;
	uint64 Links;
	uint64 Others;
	uint64 Bytes;
	bool Stopped;
} xwalkstats;



/* 遍历回调不拥有条目，返回错误时应先设置结构化错误。 */
typedef xwalkcontrol (*xwalkproc)(const xwalkentry* pEntry, ptr pUserData);



/* 遍历模块稳定错误代码。 */
typedef enum xwalkerror {
	XWALK_ERROR_OPTIONS = 1,
	XWALK_ERROR_CALLBACK,
	XWALK_ERROR_IDENTITY,
	XWALK_ERROR_OVERFLOW
} xwalkerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_FILE)

/* 初始化高级打开选项为只读、0666 权限和允许全部 Windows 共享。 */
XRT_API void xrtFileOptionsInit(xfileoptions* pOptions);



/* 使用完整选项打开文件。 */
XRT_API xfile xrtFileOpen(cstr sPath, const xfileoptions* pOptions);



/* 使用默认权限和共享策略按标志打开文件。 */
XRT_API xfile xrtOpen(cstr sPath, uint32 iFlags);



/* 关闭并销毁文件对象；即使系统关闭失败也不再允许使用该对象。 */
XRT_API bool xrtClose(xfile File);



/* 单次读取，成功读取零字节表示 EOF。 */
XRT_API bool xrtRead(xfile File, ptr pBuffer, size_t iRequest, size_t* pRead);



/* 单次写入，允许成功短写。 */
XRT_API bool xrtWrite(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten);



/* 持续读取到填满缓冲；提前 EOF 返回失败并保留实际读取量。 */
XRT_API bool xrtReadFull(xfile File, ptr pBuffer,
	size_t iRequest, size_t* pRead);



/* 持续写入到全部完成；失败时保留实际写入量。 */
XRT_API bool xrtWriteFull(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten);



/* 从绝对偏移单次读取，不改变共享文件游标。 */
XRT_API bool xrtReadAt(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead);



/* 向绝对偏移单次写入，不改变共享文件游标。 */
XRT_API bool xrtWriteAt(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten);



/* 从绝对偏移持续读取到填满缓冲。 */
XRT_API bool xrtReadAtFull(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead);



/* 向绝对偏移持续写入到全部完成。 */
XRT_API bool xrtWriteAtFull(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten);



/* 按 64 位偏移移动共享文件游标。 */
XRT_API bool xrtSeek(xfile File, int64 iOffset, xseek Origin, uint64* pPosition);



/* 返回共享文件游标位置。 */
XRT_API bool xrtTell(xfile File, uint64* pPosition);



/* 返回打开对象当前大小。 */
XRT_API bool xrtFileSize(xfile File, uint64* pSize);



/* 修改打开文件的大小。 */
XRT_API bool xrtFileResize(xfile File, uint64 iSize);



/* 打开路径并修改普通文件大小，文件必须已经存在。 */
XRT_API bool xrtFileSetSize(cstr sPath, uint64 iSize);



/* 把文件数据和必要元数据提交到稳定存储。 */
XRT_API bool xrtFlush(xfile File);



/* 返回打开文件经过验证的标志；失败返回 0。 */
XRT_API uint32 xrtFileFlags(xfile File);



/* 返回 HANDLE 或文件描述符的整数表示；失败返回 -1。 */
XRT_API intptr_t xrtFileNative(xfile File);



/* 查询路径元数据；bFollowLink 决定是否跟随末级符号链接。 */
XRT_API bool xrtPathStat(cstr sPath, bool bFollowLink, xfileinfo* pInfo);



/* 查询打开文件的元数据。 */
XRT_API bool xrtFileStat(xfile File, xfileinfo* pInfo);



/* 设置访问和修改时间；空指针表示保留对应时间，至少设置一项。 */
XRT_API bool xrtPathSetTimes(cstr sPath, bool bFollowLink,
	const xtime* pAccessed, const xtime* pModified);



/* 设置 POSIX 权限模式；Windows 明确返回不支持。 */
XRT_API bool xrtPathSetMode(cstr sPath, bool bFollowLink, uint32 iMode);



/* 设置 Windows 原生属性；POSIX 明确返回不支持。 */
XRT_API bool xrtPathSetAttributes(cstr sPath, uint32 iAttributes);



/* 判断路径是否存在；需要区分缺失和查询错误时使用 xrtPathStat。 */
XRT_API bool xrtPathExists(cstr sPath);



/* 判断路径是否为普通文件。 */
XRT_API bool xrtFileExists(cstr sPath);



/* 判断路径是否为目录。 */
XRT_API bool xrtDirExists(cstr sPath);



/* 同卷重命名文件、链接或目录；bReplace 控制是否替换已存在目标。 */
XRT_API bool xrtPathRename(cstr sSource, cstr sTarget, bool bReplace);



/* 删除一个非目录文件或链接。 */
XRT_API bool xrtFileDelete(cstr sPath);



/* 创建不存在的空文件，或把已有对象的访问和修改时间更新为当前时刻。 */
XRT_API bool xrtFileTouch(cstr sPath);

#endif



#if defined(XRT_FEATURE_FILE_LOCK)

/* 锁定字节区间；iSize 为零表示从偏移到文件末端及后续增长。 */
XRT_API bool xrtFileLockRange(xfile File, xfilelock Mode,
	uint64 iOffset, uint64 iSize, bool bWait);



/* 解除完全相同的字节区间锁。 */
XRT_API bool xrtFileUnlockRange(xfile File,
	uint64 iOffset, uint64 iSize);



/* 锁定整个文件。 */
XRT_API bool xrtFileLock(xfile File, xfilelock Mode, bool bWait);



/* 解除整个文件锁。 */
XRT_API bool xrtFileUnlock(xfile File);

#endif



#if defined(XRT_FEATURE_FILE_MAP)

/* 映射文件区间；iSize 为零表示映射到当前文件末端。 */
XRT_API xfilemap xrtFileMap(xfile File, uint64 iOffset,
	size_t iSize, uint32 iFlags);



/* 返回借用的映射数据；空映射返回空指针。 */
XRT_API ptr xrtFileMapData(xfilemap Map);



/* 返回调用方可访问的映射字节数。 */
XRT_API size_t xrtFileMapSize(xfilemap Map);



/* 把共享写映射的指定区间提交给操作系统。 */
XRT_API bool xrtFileMapFlush(xfilemap Map,
	size_t iOffset, size_t iSize);



/* 解除映射并销毁映射对象。 */
XRT_API bool xrtFileUnmap(xfilemap Map);

#endif



#if defined(XRT_FEATURE_FILE_ROOT)

/* 打开并锚定一个真实目录；根路径自身允许包含链接。 */
XRT_API xroot xrtRootOpen(cstr sPath);



/* 在已有根内打开并锚定一个子目录。 */
XRT_API xroot xrtRootOpenIn(xroot Root, cstr sPath);



/* 关闭原生目录句柄并销毁根对象；关闭不得与其他根操作并发。 */
XRT_API bool xrtRootClose(xroot Root);



/* 返回创建根对象时保存的诊断路径，不参与任何安全判断。 */
XRT_API cstr xrtRootPath(xroot Root);



/* 返回根目录 HANDLE 或文件描述符的整数表示，所有权仍属于根对象。 */
XRT_API intptr_t xrtRootNative(xroot Root);



/* 在根内使用完整文件选项打开普通文件。 */
XRT_API xfile xrtRootFileOpen(xroot Root, cstr sPath,
	const xfileoptions* pOptions);



/* 查询根内对象元数据；bFollowLink 决定是否解析末级链接。 */
XRT_API bool xrtRootStat(xroot Root, cstr sPath,
	bool bFollowLink, xfileinfo* pInfo);



/* 在根内创建一个目录；POSIX 使用显式模式，Windows 接受但忽略模式。 */
XRT_API bool xrtRootDirCreate(xroot Root, cstr sPath, uint32 iMode);



/* 删除根内一个非目录对象或空目录，不跟随末级链接。 */
XRT_API bool xrtRootRemove(xroot Root, cstr sPath);



/* 读取根内末级符号链接或受支持重解析点保存的目标文本。 */
XRT_API str xrtRootLinkRead(xroot Root, cstr sPath);



/* 在根内创建符号链接；链接目标按原文本保存，链接路径的父目录始终锚定。 */
XRT_API bool xrtRootLinkCreate(xroot Root, cstr sTarget,
	cstr sLink, bool bDirectory);



/* 在同一根内为普通文件创建硬链接，源和目标路径都经过根解析。 */
XRT_API bool xrtRootLinkHard(xroot Root, cstr sExisting, cstr sLink);



/* 在根内创建 POSIX FIFO；不支持 FIFO 的平台返回 XERR_UNSUPPORTED。 */
XRT_API bool xrtRootFifoCreate(xroot Root, cstr sPath, uint32 iMode);



/* 在根内设置对象权限；跟随链接时仍由根解析器阻止越界。 */
XRT_API bool xrtRootSetMode(xroot Root, cstr sPath,
	bool bFollowLink, uint32 iMode);

#endif



#if defined(XRT_FEATURE_FILE_TEMP)

/* 排他创建临时文件并返回拥有路径；目录为空指针时使用系统临时目录。 */
XRT_API xfile xrtFileTemp(cstr sDirectory, cstr sPrefix,
	cstr sSuffix, str* pPath);

#endif



#if defined(XRT_FEATURE_FILE_WHOLE)

/* 读取完整文件；结果总有一个额外零字节，空文件也返回可释放缓冲。 */
XRT_API bytes xrtFileReadAll(cstr sPath, size_t* pSize);



/* 在硬上限内读取完整文件；增长越过上限时失败。 */
XRT_API bytes xrtFileReadAllLimit(cstr sPath,
	size_t iLimit, size_t* pSize);



/* 创建或截断文件并完整写入全部字节。 */
XRT_API bool xrtFileWriteAll(cstr sPath, xbytesview Data);



/* 使用操作系统追加语义完整写入全部字节。 */
XRT_API bool xrtFileAppend(cstr sPath, xbytesview Data);



/* 在同目录完整写入排他临时文件，再原子替换目标。 */
XRT_API bool xrtFileWriteAtomic(cstr sPath, xbytesview Data);



/* 流式复制普通文件；不跟随末级链接，bReplace 控制是否替换普通文件目标。 */
XRT_API bool xrtFileCopy(cstr sSource, cstr sTarget, bool bReplace);



/* 移动普通文件；不跟随末级链接，优先改名，跨卷时复制成功后删除源文件。 */
XRT_API bool xrtFileMove(cstr sSource, cstr sTarget, bool bReplace);

#endif



#if defined(XRT_FEATURE_FILE_TEXT)

/* 读取并转换为 UTF-8；UNKNOWN 根据 BOM 和严格检测选择 Unicode 编码。 */
XRT_API str xrtFileReadText(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t* pSize);



/* 在源文件字节硬上限内读取并转换为 UTF-8。 */
XRT_API str xrtFileReadTextLimit(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t iLimit, size_t* pSize);



/* 把 UTF-8 文本转换为目标编码后完整写入。 */
XRT_API bool xrtFileWriteText(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom);



/* 把 UTF-8 文本转换后原子替换目标。 */
XRT_API bool xrtFileWriteTextAtomic(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom);

#endif



#if defined(XRT_FEATURE_DIR)

/* 打开目录迭代器；根路径跟随末级链接，零标志提供无额外 stat 的枚举。 */
XRT_API xdir xrtDirOpen(cstr sPath, uint32 iFlags);



/* 读取下一条目录项；失败时不修改输出。 */
XRT_API xdirnext xrtDirNext(xdir Dir, xdirentry* pEntry);



/* 关闭并销毁目录迭代器。 */
XRT_API bool xrtDirClose(xdir Dir);



/* 返回迭代器借用的目录路径。 */
XRT_API cstr xrtDirPath(xdir Dir);



/* 把迭代器目录与条目名称拼成拥有路径。 */
XRT_API str xrtDirEntryPath(xdir Dir, const xdirentry* pEntry);



/* 使用平台默认模式创建一个目录。 */
XRT_API bool xrtDirCreate(cstr sPath);



/* 使用显式 POSIX 模式创建一个目录；Windows 接受但忽略模式。 */
XRT_API bool xrtDirCreateMode(cstr sPath, uint32 iMode);



/* 使用平台默认模式递归创建全部缺失目录。 */
XRT_API bool xrtDirCreateAll(cstr sPath);



/* 使用显式 POSIX 模式递归创建全部缺失目录；失败可能保留已创建前缀。 */
XRT_API bool xrtDirCreateAllMode(cstr sPath, uint32 iMode);



/* 删除一个空目录，不递归删除其内容。 */
XRT_API bool xrtDirRemove(cstr sPath);



/* 查询目录是否为空；失败时不修改输出。 */
XRT_API bool xrtDirEmpty(cstr sPath, bool* pEmpty);



/* 查询当前系统可枚举的文件系统根目录。 */
XRT_API bool xrtDirRoots(xdirroots* pRoots);



/* 释放系统根目录列表并清零。 */
XRT_API void xrtDirRootsFree(xdirroots* pRoots);

#endif



#if defined(XRT_FEATURE_DIR_TEMP)

/* 排他创建临时目录并返回拥有路径；目录为空指针时使用系统临时目录。 */
XRT_API str xrtDirTemp(cstr sDirectory, cstr sPrefix, cstr sSuffix);

#endif



#if defined(XRT_FEATURE_FILE_WALK)

/* 初始化为不跟随链接、允许跨文件系统且深度无限。 */
XRT_API void xrtWalkOptionsInit(xwalkoptions* pOptions);



/* 深度优先遍历一个文件系统对象；回调为空时只计算统计。 */
XRT_API bool xrtFileWalk(cstr sPath, const xwalkoptions* pOptions,
	xwalkproc pProc, ptr pUserData, xwalkstats* pStats);

#endif



#if defined(XRT_FEATURE_FILE_LINK)

/* 创建符号链接；目标可以不存在，目录提示在 Windows 上是必需信息。 */
XRT_API bool xrtLinkCreate(cstr sTarget, cstr sLink, bool bDirectory);



/* 为已存在文件创建硬链接。 */
XRT_API bool xrtLinkHard(cstr sExisting, cstr sLink);



/* 读取符号链接中存储的目标文本，返回拥有的零结尾路径字节。 */
XRT_API str xrtLinkRead(cstr sLink);



/* 删除符号链接自身，不跟随也不删除目标。 */
XRT_API bool xrtLinkDelete(cstr sLink);

#endif



#if defined(XRT_FEATURE_FILE_FIFO)

/* 创建不存在的 POSIX FIFO；模式只允许低 12 位且仍受 umask 影响，Windows 返回不支持。 */
XRT_API bool xrtFifoCreate(cstr sPath, uint32 iMode);

#endif



#if defined(XRT_FEATURE_FILE_TREE)

/* 初始化为目标必须不存在、保留符号链接和拒绝特殊对象。 */
XRT_API void xrtTreeCopyOptionsInit(xtreecopyoptions* pOptions);



/* 使用高级选项复制目录树，统计成功时返回源树对象数量。 */
XRT_API bool xrtFileTreeCopy(cstr sSource, cstr sTarget,
	const xtreecopyoptions* pOptions, xwalkstats* pStats);



/* 常用目录复制；允许替换时采用合并目录并替换冲突对象。 */
XRT_API bool xrtDirCopy(cstr sSource, cstr sTarget, bool bReplace);



/* 后序删除目录树；bKeepRoot 为真时只清空内容。 */
XRT_API bool xrtFileTreeRemove(cstr sPath, bool bKeepRoot,
	xwalkstats* pStats);



/* 递归删除一个目录及全部内容。 */
XRT_API bool xrtDirRemoveAll(cstr sPath);



/* 删除目录全部内容但保留目录自身。 */
XRT_API bool xrtDirClean(cstr sPath);



/* 优先同卷改名，跨卷时复制成功后删除源目录树。 */
XRT_API bool xrtDirMove(cstr sSource, cstr sTarget, bool bReplace);



/* 统计目录树；非递归模式只统计根和直接子项。 */
XRT_API bool xrtDirStats(cstr sPath, bool bRecursive, xwalkstats* pStats);



/* 返回目录树中普通文件的总字节数。 */
XRT_API bool xrtDirSize(cstr sPath, bool bRecursive, uint64* pSize);



/* 创建缺失目录，或清空已有目录并保留根。 */
XRT_API bool xrtDirEnsureEmpty(cstr sPath);

#endif



XRT_EXTERN_C_END

#endif
