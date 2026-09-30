# VFS namespace、provider 与文件生命周期合同

状态：已冻结，作为 `XRT-VFS-001` 的实现依据。本文中的 v1 名称、所有权、
路径语义和并发语义只能通过后续设计变更提交修改，不能在实现中静默改变。

## 目标与边界

VFS 为调用方显式创建的文件命名空间。它把 disk、memory、pack 和外部 provider
统一为 `xfile`、`xdir` 与 `xfileinfo`，但不接管进程中的原生文件 API：

- `xrtFileOpen`、`xrtPathStat` 和 `xrtDirOpen` 始终访问操作系统文件系统；
- `xrtVfsOpen`、`xrtVfsStat` 和 `xrtVfsDirOpen` 只访问传入的 VFS；
- `xrtVfsOpen` 返回普通 `xfile`，成功后使用现有 read/seek/stat/close API；
- namespace 之间完全隔离，不设置进程全局 current VFS；
- mount 是解析策略，provider 是资源实现，两者没有 xs、TCC 或 mdo 语义；
- pack provider 读取任意 `xfile + offset + length`，不定位当前可执行文件；
- v1 不实现虚拟符号链接、watch、事务和跨 provider rename。

显式 namespace 避免库代码受宿主安装顺序影响，也允许 xs 为 application 和 SDK
建立两个信任边界不同的 VFS。单文件程序可以只挂载 pack；便携目录可以用更高
优先级挂载 disk，而不改变消费方代码。

## 模块和 ABI

公开头为 `include/xrt/vfs.h`，特性宏为 `XRT_FEATURE_VFS`，依赖 file、dir、
mutex、atomic、string 和 path 的纯词法能力。memory、disk 和 pack 分别使用
独立特性宏，核心 namespace 不强依赖压缩算法或平台目录根实现。

所有外部回调表以 `Size` 和 `Version` 开头。xrt 只读取 `Size` 覆盖的已知字段；
v1 未知尾部被忽略，已知字段中的未知 capability 或 flag 被拒绝。回调表在 mount
时复制，provider 不需要让表结构永久存活。函数指针所指代码以及 provider context
必须活到最后一个 generation 引用释放。

公开类型采用以下名称：

```c
typedef struct xvfs_impl* xvfs;
typedef struct xvfs_mount_impl* xvfsmount;

typedef enum xvfslookup {
    XVFS_LOOKUP_ERROR = -1,
    XVFS_LOOKUP_MISS = 0,
    XVFS_LOOKUP_OPENED = 1
} xvfslookup;

typedef enum xvfscase {
    XVFS_CASE_SENSITIVE = 0,
    XVFS_CASE_ASCII_INSENSITIVE = 1
} xvfscase;
```

不增加第二套文件元数据类型。所有 VFS stat 都使用现有 `xfileinfo`。

## 路径合同

VFS API 只接受规范的 UTF-8 绝对虚拟路径：

- 根路径是 `/`；其他路径以 `/` 开头且不以 `/` 结尾；
- 分隔符只能是 `/`，反斜线被拒绝；
- 拒绝无效 UTF-8、空 segment、`.`、`..` 和控制字符 NUL；
- 不执行 URL percent decode、Unicode normalization 或平台路径展开；
- 最大字节数由 `XRT_VFS_PATH_MAX` 限制，v1 值为 32768；
- mount prefix 遵守相同规则；根 mount `/` 合法；
- provider 得到不带开头 `/` 的相对路径，mount 根本身对应空 view；
- provider 不得把相对路径再次解释为绝对路径。

`cstr` 不能表达内嵌 NUL，因此 API 扫描到首个 NUL 为止；带长度的内部入口仍必须
拒绝 view 中的 NUL。虚拟路径比较按 UTF-8 字节执行。ASCII 不区分大小写模式只折叠
`A-Z`，不声称实现语言相关的 Unicode case folding。

disk provider 必须逐 segment 解析相对路径。它不能简单拼接后交给平台，因为这会
重新引入 `..`、Windows drive/UNC、符号链接或 reparse point 逃逸。调用方选择的
case 模式也必须由 provider 主动验证，不能把宿主文件系统的偶然行为当作合同。

## mount 解析

每个成功 mount 得到单调递增的 `MountId`。一次 lookup 从一个不可变 snapshot
构造候选列表，排序键依次为：

1. 匹配 prefix 的字节长度，长 prefix 在前；
2. 调用方给出的 `Priority`，数值大者在前；
3. `MountId`，较早 mount 的在前。

prefix 只能匹配完整 segment：`/app` 匹配 `/app` 和 `/app/a`，不匹配
`/apple`。候选返回 `MISS` 后继续下一个候选，包括更短 prefix 的候选。`ERROR`
立即停止，`OPENED` 立即成功。同优先级采用较早 mount，避免添加新 provider 时
静默覆盖已有内容；需要覆盖时必须明确提高 priority。

provider 的 `MISS` 表示该路径在本 provider 中不存在，不得修改线程错误。
`ERROR` 表示命中 provider 但访问失败，并必须设置具体错误。若 provider 违反
ERROR 合同，VFS 合成 `XVFS_ERROR_PROVIDER`。所有候选都 MISS 时设置
`XVFS_ERROR_NOT_FOUND`。

VFS 不因权限、校验失败、损坏包或暂时 I/O 错误回退到低优先级资源。这条规则防止
损坏的外部配置悄悄退回内置默认值，也防止安全检查被 overlay 顺序绕过。

## namespace 与 mount 生命周期

`xvfs` 使用引用计数。`xrtVfsCreate` 返回一个 owned reference，`xrtVfsRef` 增加
引用，`xrtVfsDestroy` 释放一个引用。最后一个 namespace 引用会发布空 snapshot，
但旧 snapshot、进行中的 callback 和已打开对象继续保活其 generation。

mount 表使用不可变 snapshot：

1. mount/unmount 在注册表 mutex 内复制并构造新 snapshot；
2. 完整初始化后以 release 语义发布；
3. lookup 以 acquire 语义取得 snapshot 引用，然后释放注册表 mutex；
4. provider callback 只在注册表 mutex 外调用；
5. snapshot 持有每个 mount generation；
6. lookup 在调用 provider 前另持有候选 generation；
7. 打开的文件和目录持有实际命中的 generation；
8. unmount 只阻止新 lookup，最后一个 generation 引用释放 provider context。

`xvfsmount` 是引用计数控制句柄。`xrtVfsMount` 返回 owned reference；
`xrtVfsMountRef` 增加引用；`xrtVfsMountDestroy` 只释放控制句柄，不隐式 unmount；
`xrtVfsUnmount` 是线程安全且幂等的，成功后该句柄仍可查询 ID 或销毁。控制句柄
保留 namespace 控制块，避免 VFS 先销毁造成悬空指针，但不让已 unmount 的
generation 重新发布。

不使用延时释放、固定等待或全局 quiesce。任何等待 provider callback 结束的逻辑
都不得持有注册表 mutex。provider 的最后一次 ContextRelease 可以在任意完成
lookup 或 close 的线程执行，因此 provider 自己负责所需的线程亲和调度。

## 公开 namespace API

冻结的核心入口如下：

```c
XRT_API xvfs xrtVfsCreate(void);
XRT_API void xrtVfsRef(xvfs Vfs);
XRT_API void xrtVfsDestroy(xvfs Vfs);

XRT_API xvfsmount xrtVfsMount(
    xvfs Vfs,
    cstr sVirtualPrefix,
    int32 iPriority,
    xvfscase CaseMode,
    const struct xvfsprovider_v1* pProvider,
    void* pProviderContext,
    uint32 iFlags);

XRT_API void xrtVfsMountRef(xvfsmount Mount);
XRT_API void xrtVfsMountDestroy(xvfsmount Mount);
XRT_API bool xrtVfsUnmount(xvfsmount Mount);
XRT_API uint64 xrtVfsMountId(xvfsmount Mount);

XRT_API xfile xrtVfsOpen(
    xvfs Vfs,
    cstr sVirtualPath,
    const xfileoptions* pOptions);

XRT_API bool xrtVfsStat(
    xvfs Vfs,
    cstr sVirtualPath,
    bool bFollowLink,
    xfileinfo* pInfo);

XRT_API bytes xrtVfsReadAll(
    xvfs Vfs,
    cstr sVirtualPath,
    size_t* pSize);

XRT_API bytes xrtVfsReadAllLimit(
    xvfs Vfs,
    cstr sVirtualPath,
    size_t iLimit,
    size_t* pSize);

XRT_API xdir xrtVfsDirOpen(
    xvfs Vfs,
    cstr sVirtualPath,
    uint32 iFlags);
```

`xrtVfsDirOpen` 返回普通 `xdir`，随后使用 `xrtDirNext` 和 `xrtDirClose`。
`xrtDirPath` 对 VFS 目录返回规范虚拟路径，`xrtDirEntryPath` 也拼接虚拟路径。
原生 `xrtDirOpen` 的行为不变。

`iFlags` 在 v1 必须为零；该字段为后续只读、禁止枚举或审计策略保留。未知非零位
返回 invalid argument。`bFollowLink` 为 API 对称保留；v1 provider 不产生虚拟链接，
disk provider 可按其 mount 安全策略决定是否允许跟随 root 内链接。

## 文件后端合同

`xfile` 内部改为带 ops 的分派对象。native backend 保留当前系统行为；VFS 文件
使用 provider file ops。公共 VFS provider 不接触 `struct xfile_impl`，而是返回：

```c
typedef enum xvfsfilecapability {
    XVFS_FILE_READ = 0x00000001,
    XVFS_FILE_WRITE = 0x00000002,
    XVFS_FILE_READ_AT = 0x00000004,
    XVFS_FILE_WRITE_AT = 0x00000008,
    XVFS_FILE_SEEK = 0x00000010,
    XVFS_FILE_STAT = 0x00000020,
    XVFS_FILE_RESIZE = 0x00000040,
    XVFS_FILE_FLUSH = 0x00000080
} xvfsfilecapability;

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

typedef struct xvfsfile_v1 {
    const xvfsfileops_v1* Ops;
    void* State;
    uint32 Flags;
    uint32 Reserved;
} xvfsfile_v1;
```

VFS 在 provider 返回 OPENED 后复制已知 file ops，验证 capabilities、回调和实际
打开 flags，再构造 `xfile`。构造失败时由 VFS 调用一次 `Close(State)`。构造成功后
`xrtClose` 恰好调用一次 Close。OPENED 转移 State 所有权；MISS 和 ERROR 不转移，
并要求输出保持全零。

capability 与非空回调必须一致。Read 或 ReadAt 至少有一个，VFS wrapper 可以用
ReadAt 和受保护 cursor 实现 Read；Write 同理。wrapper 负责串行化依赖共享 cursor
的 Read/Write/Seek，显式 ReadAt/WriteAt 可并发进入 provider。Stat、Flush、Resize
按现有 xfile 并发合同执行。provider 自己保护其 State 中的其他共享数据。

`xrtFileNative`、file map、file lock 和现有 OS completion async 对非 native backend
返回稳定的 unsupported 错误。callback provider 的 VFS Open 拒绝 `XFILE_ASYNC`；
只有声明 `XVFS_PROVIDER_NATIVE_OPEN` 并转移真实原生 `xfile` 的 provider 可保留平台
async。后续若增加 provider 异步 ops，必须独立定义取消和完成所有权，并与线程池模拟
区分。不能用阻塞回调冒充 OS completion async。

## provider ABI

provider 表冻结为：

```c
typedef struct xvfsdirops_v1 xvfsdirops_v1;

typedef struct xvfsdir_v1 {
    const xvfsdirops_v1* Ops;
    void* State;
} xvfsdir_v1;

typedef struct xvfsprovider_v1 {
    uint32 Size;
    uint32 Version;
    uint64 Capabilities;

    void (*ContextRetain)(void* pContext);
    void (*ContextRelease)(void* pContext);

    xvfslookup (*Open)(
        void* pContext,
        xvfscase CaseMode,
        xstrview RelativePath,
        const xfileoptions* pOptions,
        xvfsfile_v1* pFile);

    xvfslookup (*Stat)(
        void* pContext,
        xvfscase CaseMode,
        xstrview RelativePath,
        bool bFollowLink,
        xfileinfo* pInfo);

    xvfslookup (*DirOpen)(
        void* pContext,
        xvfscase CaseMode,
        xstrview RelativePath,
        uint32 iFlags,
        xvfsdir_v1* pDir);

    xvfslookup (*OpenNative)(
        void* pContext,
        xvfscase CaseMode,
        xstrview RelativePath,
        const xfileoptions* pOptions,
        xfile* pFile);
} xvfsprovider_v1;
```

`OpenNative` 是保持 v1 版本号的尺寸扩展尾字段。初始 v1 表的最小合法 `Size` 为
`offsetof(xvfsprovider_v1, OpenNative)`；运行库先清零本地完整表，再只复制调用方
`Size` 覆盖的已知前缀。这样旧 provider 不读取或写入新字段，新 provider 面对不认识
该 capability 的旧运行库会在 mount 时被拒绝，而不会调用越界回调。

ContextRetain 和 ContextRelease 必须同时为空或同时非空。mount 在发布 generation
前调用一次 Retain，最后一个 generation 引用调用一次 Release。空回调表示 context
由静态存储期或其他外部所有者保证；标准 provider 一律使用显式引用。provider
callback 可并发、可重入，也可在持有自己的 VFS 引用时调用任意 VFS API。

`CaseMode` 是当前 generation 在 mount 时冻结的策略，Open、OpenNative、Stat 和
DirOpen 必须对相对路径执行同一策略；v1 的不区分大小写只折叠 ASCII A-Z。
Open 与 OpenNative 必须且只能提供一个；OpenNative 还必须声明
`XVFS_PROVIDER_NATIVE_OPEN`，并在 OPENED 时转移一个打开标志完全匹配的原生 xfile。
Stat 和 DirOpen 可由 capability 声明为不支持。VFS 不通过 Open
猜测 MISS 与 ERROR，也不通过读完整文件伪造 provider stat。provider 可在 callback
内部使用 xrt，但不能依赖调用线程已有错误作为输出。

## 目录 ABI 与合并

目录 provider ops 为：

```c
typedef struct xvfsdirops_v1 {
    uint32 Size;
    uint32 Version;
    bool (*Next)(void* pState, xdirentry* pEntry, bool* pEnd);
    void (*Close)(void* pState);
} xvfsdirops_v1;
```

Next 成功返回 true，并以 `pEnd` 区分 item 与正常结束；失败返回 false 并设置错误。
名称和可用元数据借用到下一次 Next 或 Close。VFS 在 callback 返回后立即复制，验证
名称是单个规范 UTF-8 segment，随后再调用 provider。

一次 `xrtVfsDirOpen` 对所有匹配候选调用 DirOpen：MISS 跳过，ERROR 使整个 open
失败。VFS 物化可见条目并按解析顺序去重，第一个同名条目获胜；比较遵守对应 mount
case 模式。最终结果按 UTF-8 字节升序返回，使不同 provider 的枚举顺序不影响输出。
若 case 模式不同但两个名称按任一候选规则冲突，仍由解析顺序较前者获胜。

物化过程有条目数、单名长度、总名称字节和总内存上限；超过上限返回 limit，不能
交付截断目录。provider 目录在成功、错误、OOM 和验证失败路径都恰好关闭一次。
打开后的普通 `xdir` 自持所有复制数据和命中的 generation，不借用 provider entry。

## 标准 memory provider

memory provider context 是引用计数不可变索引。构建器允许复制字节或接管 owned
buffer；发布 mount 后的更新通过新 snapshot 完成，已经打开的文件保留旧 blob。

v1 支持空文件、Read、ReadAt、Seek、Stat 和目录枚举。每个 blob 有独立引用；打开
文件持有 blob，不借用构建器或调用方 buffer。重复规范路径在构建时失败。目录索引
由文件路径生成，文件与目录同名冲突失败。

建议 API 为 `xrtVfsMemoryCreate`、`xrtVfsMemoryPutCopy`、
`xrtVfsMemoryPutOwned`、`xrtVfsMemorySeal` 和 `xrtVfsMemoryDestroy`；只有 sealed
context 能通过 `xrtVfsMemoryMount` mount。PutOwned 仅在成功时消费由 xrt allocator
分配的 buffer，失败时调用方仍拥有。seal 同时生成字节序和 ASCII 折叠序索引；
大小写不敏感 mount 会拒绝折叠冲突，避免同一相对路径得到不确定结果。

## 标准 disk provider

disk provider 持有 `xroot` 而非路径字符串。它把所有操作限定在已打开目录根中，
读写授权在 context 创建时冻结。只读 context 拒绝 CREATE、WRITE、TRUNCATE、
APPEND、EXCLUSIVE、SYNC 和任何后续写操作；没有 READ 权限的 context 拒绝读取、
stat 与目录枚举。

路径解析逐 segment 使用 root-relative 原语。默认拒绝符号链接和 Windows reparse
point；首版 disk provider 不提供放宽链接策略的选项。xrt 内部 root 策略会在任何
链接目标被重写前拒绝它，并通过锚定父目录枚举逐字节确认大小写敏感路径的每个
segment。创建新文件时还会探测宿主文件系统已存在的大小写别名，避免在 Windows、
macOS 等默认不区分大小写的文件系统上误开或覆盖另一个名称。检查后再按绝对路径
打开不构成证明。目录枚举不返回 `.` 和 `..`，且执行与 open 相同的 case 和链接策略。
ASCII 不区分大小写 lookup 必须为每个 segment 找到唯一折叠匹配；同一物理目录存在
折叠冲突时，lookup 和目录枚举都以不可回退错误失败。无效 UTF-8 名称不进入 VFS。

末级文件由 root policy 原子限制为普通文件。POSIX 打开临时加入 `O_NONBLOCK`，取得
句柄并 `fstat` 确认类型后恢复阻塞模式，防止 stat 与 open 之间被换成 FIFO；Windows
使用 `FILE_NON_DIRECTORY_FILE` 并对最终句柄复查类型。Windows 根目录枚举以空相对名
重新打开独立 file object，避免 `DuplicateHandle` 共享 `NtQueryDirectoryFile` 游标。

disk provider 可以返回 native backend，因而保留 map、lock、native handle 和平台
async 能力；VFS generation 作为附加 owner 挂到该 xfile，close 先关闭 native state，
再释放 generation。该附加 owner 是 xfile 内部能力，不向普通调用方公开。

## 标准 pack provider

pack provider 接收源 `xfile`、归档 offset 和 length。源必须以 READ 打开并支持
ReadAt；provider 不改变其 cursor。`xrtVfsPackCreate()` 成功时接管 Source，失败时
Source 仍归调用方；由此可以持有任意 callback backend，而不把“复制原生句柄”误当成
通用引用。provider、mount 和已打开文件以引用计数延长 Source 生命周期，最后一个
引用释放时恰好调用一次 `xrtClose()`。

Offset 和 Length 必须覆盖完整归档，从 header 首字节到 trailer 末字节。xs 只负责从
可执行文件末尾发现 trailer、计算候选范围并选择 AUTO/明确格式；header、index、entry
和 trailer 的全部验证均由 xrt 完成。pack provider 不认识“当前 exe”、PE 或 ELF。

### 格式选择与限制

`xvfspackoptions` 使用 `Size + Version`，并冻结 Format、MaxEntries、MaxIndexBytes、
MaxEntryBytes、MaxDecodedBytes 与 CacheBytes。非零解析限制由 Init 填入生产默认值，
调用方不能用零绕过硬上限；CacheBytes 为零明确禁用常驻缓存。AUTO 只按完整的 8 字节 header magic 选择格式，不在解析失败后尝试
另一格式。首版接受 `XVFS_PACK_XRT_V1` 和迁移格式 `XVFS_PACK_XSVPACK_V1`。

新格式全部使用 little-endian 无符号整数和范围内相对偏移：

```text
header (80 bytes)
  0   magic          8   "XRTPACK\0"
  8   major          2   1
  10  minor          2   0
  12  headerSize     4   80
  16  flags          4   0
  20  entryCount     4
  24  dataOffset     8   80
  32  dataSize       8
  40  indexOffset    8   dataOffset + dataSize
  48  indexSize      8
  56  archiveSize    8   等于 Create 的 Length
  64  decodedSize    8   所有 originalSize 之和
  72  indexCrc32     4   IEEE CRC-32
  76  reserved       4   0

index record (40 + pathSize bytes)
  0   pathSize       2
  2   codec          1   0=STORE, 1=LZMA1
  3   flags          1   0
  4   reserved0      4   0
  8   dataOffset     8   相对 header
  16  storedSize     8
  24  originalSize   8
  32  originalCrc32  4   IEEE CRC-32
  36  reserved1      4   0
  40  path           n   无 NUL 的规范 UTF-8 相对路径

trailer (32 bytes)
  0   magic          8   "XRTPEND\0"
  8   archiveSize    8   等于 header.archiveSize
  16  headerOffset   8   固定为 0
  24  metadataCrc32  4   CRC32(trailer[0..24) || header[0..80))
  28  flags          4   0
```

物理布局严格为 `header || data || index || trailer`。每个非空 data extent 必须位于
data 区、按 index 顺序连续且互不重叠；零字节 STORE 条目位于当前 data cursor。
index 必须被记录完全消费，路径按 UTF-8 字节严格递增。LZMA1 payload 为 5 字节 SDK
properties 加一条独立 LZMA1 stream；空文件必须使用 STORE。该约束排除未被索引描述的
隐藏区、别名 extent 和平台对齐差异，使相同输入可以生成逐字节相同的归档。

兼容格式严格实现既有 `XSVPACK` v1：64 字节 `XSVFHDR\0` header、变长旧 index 和
24 字节 `XSVPACK\0` trailer。trailer 的绝对 HeaderOffset 必须等于 Create 的 Offset，
header ArchiveSize 必须等于 Length，保留字段和 flags 必须为零。旧格式仍执行当前路径、
UTF-8、重复项、extent、预算和 checksum 验证；“兼容”不保留旧 reader 的宽松行为。

parser 在发布索引前验证：

- magic、版本、header 长度和整型加法/乘法无溢出；
- 索引、名称、压缩数据和 trailer 均在给定范围内且不非法重叠；
- 路径满足 VFS 相对路径合同，无重复文件或文件/目录冲突；
- codec、原始长度、压缩长度和 checksum 参数受支持；
- 单条目和归档总解压预算不超过配置；
- 不接受符号链接、设备节点或平台特殊路径。

条目缓存状态机为：

```text
UNLOADED -> LOADING -> READY
                    -> FAILED
```

状态转换在 entry mutex 下进行，解压和校验在锁外执行。并发首次读取中只有一个线程
成为 loader，其余线程在 condition 上等待。READY 只在完整校验后发布；FAILED 保存
可复制的结构化错误，所有等待者得到同一类别和代码。等待期间不自旋。

缓存按解压字节设置硬预算，并公开 hits、misses、loads、failures、resident bytes 和
evictions。LRU eviction 只移除 cache 对 blob 的引用；已打开文件持有自己的 blob
引用。单条目大于预算时允许无缓存读取，但仍只发布校验后的不可变 blob。CRC 只用于
损坏检测，签名和来源信任属于上层发布策略。

公开对象和操作固定为：

```c
typedef struct xvfs_pack_impl* xvfspack;

void xrtVfsPackOptionsInit(xvfspackoptions* options);
xvfspack xrtVfsPackCreate(xfile source, uint64 offset, uint64 length,
    const xvfspackoptions* options);
void xrtVfsPackRef(xvfspack pack);
void xrtVfsPackDestroy(xvfspack pack);
xvfspackformat xrtVfsPackFormat(xvfspack pack);
bool xrtVfsPackStats(xvfspack pack, xvfspackstats* stats);
xvfsmount xrtVfsPackMount(xvfs vfs, cstr prefix, int32 priority,
    xvfscase caseMode, xvfspack pack, uint32 flags);
```

Options 为 NULL 等价于 Init 默认值。Stats 是瞬时一致快照；计数单调递增，
ResidentBytes 是快照时仍由 cache 持有的解压字节。格式中的路径保持原大小写；敏感
mount 使用主索引，ASCII 不敏感 mount 仅在整个归档不存在折叠冲突时成立。

Open 只接受读语义。首次打开文件触发 load；成功后每个打开文件持有 blob reference
和独立 cursor。目录和 stat 只访问发布后的不可变索引，不触发内容解压。FAILED 保存
`xrtErrorRef()`，后续打开以及同一轮等待者都通过 `xrtSetError()` 得到等价错误；pack
销毁时释放该引用。

## 错误域

VFS 使用 `xrt.vfs`，稳定代码至少包括：

```c
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
```

参数错误和非法状态继续使用 xrt 通用错误。provider 返回的权限、I/O、OOM 和取消
错误保持原 domain 与 kind；VFS 只在 provider 未履行合同时合成 provider 错误。
遍历 MISS 不覆盖调用前错误，最终 NOT_FOUND 才发布新的线程错误。

## 可观测性与故障注入边界

核心 namespace 不在每次 lookup 热路径维护全局计数，也不引入进程级日志回调。
调用方用稳定 `MountId` 关联自己的 mount/unmount 与请求日志；具有内部缓存的标准
provider 通过自身 API 暴露必要统计，pack 使用 `xrtVfsPackStats()`。xs 和产品层知道
application generation、模块与请求身份，应在该层记录业务诊断，而不是把这些语义
下沉到 xrt。

xrt 不提供可在生产进程中误开启的全局“失败开关”。确定性故障测试使用公共 provider
ABI 构造短读、提前 EOF、I/O 错误和协议违例 source，并使用 memory-debug fail-after
覆盖分配失败。底层 source 的结构化错误必须保持 domain/code；VFS 只在 provider
违反回调合同时合成 `XVFS_ERROR_PROVIDER`。这种边界允许 xs 后续注入磁盘满或 TCC
读取错误，同时不让测试状态污染其他 namespace。

## 失败原子性

- Create 失败不产生可见 namespace；
- Mount 在 Retain、generation、snapshot 和控制句柄全部成功后一次发布；
- Mount 失败不消费调用方 context reference，也不改变旧 snapshot；
- Unmount 构造新 snapshot 失败时旧 mount 保持 active；
- provider OPENED 后任何包装失败都调用一次 file Close；
- DirOpen 在任一候选 ERROR/OOM 时关闭已打开的所有 provider 目录；
- memory seal 或 pack 索引构建失败不发布半成品；
- 所有 out 参数只在成功时修改，除文档明确的 size query 外。

OOM 测试必须覆盖每个分配点，并验证旧 snapshot、context 引用、文件 state、目录 state
和线程错误。不能用“进程即将退出”跳过释放证明。

## 重入和锁顺序

注册表 mutex 只保护 snapshot 发布和 mount 状态。它不包围 provider callback、
file/dir callback、context release、内存释放或错误构建。规范锁顺序是：

```text
namespace registry -> snapshot construction only
provider index      -> provider-local entry selection
pack entry          -> one entry state transition
file cursor         -> one opened file cursor
```

后三级都不得取得 namespace registry。不同 pack entry 不互锁。condition wait 必须在
循环中检查状态，取消或虚假唤醒不能发布半成品。Close 可与独立 ReadAt 的调用关系遵守
现有 xfile 合同：调用方必须保证对象引用有效；xrt 不尝试从已释放的裸指针补取引用。

## 验证矩阵

模块化和单头构建都必须覆盖：

- 规范路径、非法 UTF-8、dot segment、反斜线、长度与属性测试；
- prefix segment 边界、最长 prefix、priority、稳定 MountId 顺序；
- 跨较短 prefix 的 MISS 回退，以及 ERROR 不回退；
- mount/unmount/open 竞争和 VFS destroy 后旧文件继续读取；
- callback 内 open、mount、unmount 和 context release 重入；
- file backend 每项 capability、unsupported、close-once 与包装 OOM；
- 原生 file、map、lock、async、whole-file 和 dir 全量回归；
- memory 空文件、owned/copy buffer、seal snapshot 和目录冲突；
- disk root 逃逸、链接/reparse point、case、只读授权和 TOCTOU 压测；
- pack 截断、越界、重叠、溢出、重复路径、坏 UTF-8 和 checksum；
- 同一 pack 条目百线程首次读取、失败唤醒、预算和 eviction；
- 目录合并、同名遮蔽、混合 case、确定排序和 limit 原子失败；
- 每个分配点 OOM，以及 provider 故意违反协议的防御；
- ASan、UBSan、可用平台的 TSan 或 Windows ASan；
- pack parser fuzz target；
- native 文件重构前后基准和 VFS lookup 基准。

完成门还要求生成 `single/xrt.h`，禁止手工复制；公开 struct 布局和 feature 组合通过
ABI/编译矩阵；API 文档和最小示例与实现一致。

## 实施顺序

1. 先关闭 Future Watch 调用方裸指针竞态，VFS 不建立在未证明的异步生命周期上；
2. 把 native `xfile`、`xdir` 改造成内部 backend 分派，保持外部行为不变；
3. 加入测试 backend，证明 capability、错误、close-once 和 owner attachment；
4. 实现路径解析、snapshot、mount 和 provider 协议；
5. 实现 memory 和 disk provider，再实现目录合并；
6. 实现 pack parser、缓存和 fuzz target；
7. 生成单头、运行 sanitizer/平台矩阵和基准；
8. xs 仅在本阶段门全部通过后迁移 application/SDK 双 VFS。

本合同不允许通过修改 `xfileoptions` 塞入 VFS 指针，也不允许 provider 返回进程期
借用 buffer。所有可跨 callback、unmount 或 reload 的资源都必须有可审计的 owned
reference。
