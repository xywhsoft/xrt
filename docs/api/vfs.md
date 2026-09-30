# VFS API

`xrt/vfs.h` 提供显式、可并存的虚拟文件命名空间。它不改变
`xrtFileOpen()`、`xrtPathStat()` 或 `xrtDirOpen()` 的原生文件系统语义。

## 命名空间与挂载

```c
xvfs vfs = xrtVfsCreate();
xvfsmount mount = xrtVfsMount(
    vfs,
    "/app",
    100,
    XVFS_CASE_SENSITIVE,
    &provider,
    provider_context,
    0
);
```

命名空间和 mount 控制句柄都使用显式引用计数。`xrtVfsUnmount()` 幂等，
只阻止新的 lookup；旧快照、正在执行的 provider callback、已经打开的 `xfile`
和 `xdir` 会继续持有对应 generation。`xrtVfsMountDestroy()` 只释放控制句柄，
不会隐式 unmount。

路径必须是规范 UTF-8 绝对虚拟路径。根为 `/`，分隔符固定为 `/`；拒绝反斜线、
重复分隔符、`.`、`..`、尾部分隔符、无效 UTF-8 和超过
`XRT_VFS_PATH_MAX` 的输入。provider 收到不带开头 `/` 的相对路径。

lookup 顺序固定为：最长 prefix、最高 priority、最早 MountId。只有
`XVFS_LOOKUP_MISS` 会继续查询；`XVFS_LOOKUP_ERROR` 必须携带线程错误并立即终止。

### Overlay 用法

便携程序可以先把内置 memory 或 pack provider 挂到 `/app`，再把外部 disk provider
以更高 priority 挂到同一 prefix。消费方始终读取 `/app/...`：外部层存在同名资源时
覆盖内置层，不存在时以 MISS 回退。权限、校验和、损坏或 I/O 错误是 ERROR，会阻止
回退，避免损坏的外部配置被内置默认值掩盖。`examples/file/vfs/main.c` 用两个 memory
provider 演示覆盖、回退、卸载以及恢复内置值；替换为 disk/pack 时解析规则不变。

## Provider ABI

`xvfsprovider_v1`、`xvfsfileops_v1` 和 `xvfsdirops_v1` 都以 `Size + Version`
开头。mount 时会复制已知 provider 表，未知尾部被忽略，未知 capability 被拒绝。
函数代码和 context 必须存活到最后一个 generation 引用释放。

Open、Stat 和 DirOpen 每次都接收当前 mount 冻结的 `xvfscase`。provider 必须对
相对路径使用该策略；`XVFS_CASE_ASCII_INSENSITIVE` 只折叠 ASCII A-Z，不执行
Unicode case folding。

`ContextRetain` 与 `ContextRelease` 必须同时提供或同时为空。成功 mount 调用一次
Retain，generation 最终释放时调用一次 Release。所有 provider callback 都在
namespace 注册锁外执行，可以并发、重入，并可再次调用 VFS API。

`xvfsprovidercapability` 定义 namespace 级可选能力：
`XVFS_PROVIDER_STAT` 对应 `Stat`，`XVFS_PROVIDER_DIRECTORY` 对应 `DirOpen`，
`XVFS_PROVIDER_NATIVE_OPEN` 对应尾部 `OpenNative`；合法位掩码是
`XVFS_PROVIDER_CAPABILITIES`。文件级合法能力位掩码是
`XVFS_FILE_CAPABILITIES`。capability 位与对应回调必须严格一致，不能声明后留空，
也不能提供未声明的回调。

普通 Open 返回 `XVFS_LOOKUP_OPENED` 时把 file State 所有权转移给 VFS；包装失败和正常
`xrtClose()` 都恰好调用一次 Close。MISS 和 ERROR 不转移所有权，并要求输出保持全零。
capability 必须与同名回调严格一致。普通 callback provider 拒绝 `XFILE_ASYNC`，且不能
使用 `xrtFileNative()`、file map、file lock 或 OS completion async。

声明 `XVFS_PROVIDER_NATIVE_OPEN` 的 provider 必须只提供 `OpenNative`，该回调在
OPENED 时直接转移一个打开标志完全匹配的原生 `xfile`。VFS 不包装或降级该文件，
只把命中的 generation 作为内部附加 owner；所以 native handle、map、lock 和平台
async 能力保持不变。旧 provider 表可以把 `Size` 设为 `OpenNative` 的偏移，运行库
会补零已知尾部；未知尾部仍被忽略。

## 文件与目录

`xrtVfsOpen()` 返回普通 `xfile`，随后使用 `xrtRead()`、`xrtReadAt()`、
`xrtSeek()`、`xrtFileStat()` 和 `xrtClose()`。只有 ReadAt/WriteAt 的 provider
会获得由 VFS mutex 保护的逻辑 cursor，显式 offset 操作仍可并发进入 provider。

`xrtVfsDirOpen()` 会查询所有匹配 provider，复制并验证每个条目，按解析顺序去重，
再按 UTF-8 字节排序。混合 case 策略下，只要任一冲突项声明 ASCII 不区分大小写，
较后的条目就被遮蔽。返回值是普通 `xdir`；`xrtDirPath()` 与
`xrtDirEntryPath()` 始终使用虚拟 `/` 路径。

目录物化有硬上限：8192 个条目、单名 4096 字节、总名称 8 MiB。超过上限会以
`XVFS_ERROR_LIMIT` 原子失败，不返回截断目录。

## Memory provider

`xvfsmemory` 是先构建、再 seal 的不可变资源索引。`xrtVfsMemoryCreate()` 返回一个
owned reference；`xrtVfsMemoryRef()` 和 `xrtVfsMemoryDestroy()` 管理其生命周期。
构建阶段不是并发 API，seal 成功后可由任意数量的 mount 和线程并发读取。
公开句柄不暴露内部布局；`xvfs_memory_impl` 始终是运行库私有实现。

`xrtVfsMemoryPutCopy()` 复制调用方数据，`xrtVfsMemoryPutOwned()` 只在成功时接管
由 `xrtMalloc()` 系列分配、可由 `xrtFree()` 释放的缓冲。两者都支持零字节文件；
相对路径必须规范且不能为空。重复文件路径立即失败，文件/目录同名冲突在
`xrtVfsMemorySeal()` 原子构建目录索引时失败。分配失败不会改变构建器，可直接重试
seal；seal 成功后不再允许 Put。

`xrtVfsMemoryMount()` 只接受 sealed context。ASCII 不区分大小写的 mount 会拒绝
折叠后冲突的路径；大小写敏感 mount 保留原始名称。provider 支持 Read、ReadAt、
Seek、Stat 和目录枚举，拒绝写、创建、截断与追加。打开的文件独立持有 blob，
所以 unmount、销毁 namespace 或释放 provider context 都不会使旧文件失效。

## Disk provider

`xrtVfsDiskCreate()` 打开并锚定一个物理目录，返回拥有引用的 `xvfsdisk`。
`xrtVfsDiskRef()` 和 `xrtVfsDiskDestroy()` 管理生命周期。`XVFS_DISK_READ` 与
`XVFS_DISK_WRITE` 在创建时冻结；至少需要一项，未知位被拒绝。只读 context 拒绝
WRITE、CREATE、TRUNCATE、APPEND、EXCLUSIVE 和 SYNC，写-only context 拒绝读取、
stat 和目录枚举。

公开句柄与授权位定义为：

```c
typedef struct xvfs_disk_impl* xvfsdisk;

typedef enum xvfsdiskaccess {
    XVFS_DISK_READ = 0x01,
    XVFS_DISK_WRITE = 0x02
} xvfsdiskaccess;

#define XVFS_DISK_ACCESS 0x03u
```

`xvfs_disk_impl` 的布局始终是运行库私有实现；调用方只持有和传递 `xvfsdisk`。

`xrtVfsDiskMount()` 把锚定目录发布到虚拟 prefix。每个路径 segment 都从目录句柄
解析；大小写敏感模式逐字节确认实际名称，ASCII 不区分大小写模式选择唯一折叠匹配，
遇到折叠冲突即失败。首版始终拒绝符号链接和 Windows reparse point，包括中间路径、
末级文件和目录条目。文件打开只接受普通文件；末级对象会在取得原生句柄后再次验证，
因此并发替换成 FIFO 或其他特殊文件也不会阻塞或绕过限制。无效 UTF-8 的物理目录
名称不会进入 VFS。

成功打开返回原生 `xfile`，因此保留 native handle、映射、锁和平台异步 I/O 能力。
文件通过 VFS generation 保持 provider context；unmount、销毁 namespace 或释放调用方
持有的 `xvfsdisk` 后，已打开文件仍然有效。

## Pack provider

`xrtVfsPackCreate()` 从任意支持 ReadAt 的同步 `xfile` 读取一个完整归档范围。Offset
和 Length 从 header 首字节覆盖到 trailer 末字节；provider 不定位当前可执行文件，
也不改变 Source cursor。创建成功时 Source 所有权转移给 `xvfspack`，创建失败时 Source
仍归调用方。最后一个 pack、mount generation 或打开文件引用释放后，Source 恰好关闭
一次。

`xrtVfsPackOptionsInit()` 写入 `XRT_VFS_PACK_OPTIONS_VERSION` 对应的生产默认值。
Format 可以是 `XVFS_PACK_AUTO`、`XVFS_PACK_XRT_V1` 或迁移格式
`XVFS_PACK_XSVPACK_V1`；AUTO 只按完整 header magic
选择一次，不用另一 parser 掩盖损坏。MaxEntries、MaxIndexBytes、MaxEntryBytes 和
MaxDecodedBytes 是非零硬上限，CacheBytes 为零时禁用常驻 cache。公开的默认常量为：

```c
XRT_VFS_PACK_MAX_ENTRIES_DEFAULT
XRT_VFS_PACK_MAX_INDEX_BYTES_DEFAULT
XRT_VFS_PACK_MAX_ENTRY_BYTES_DEFAULT
XRT_VFS_PACK_MAX_DECODED_BYTES_DEFAULT
XRT_VFS_PACK_CACHE_BYTES_DEFAULT
```

新格式使用范围内相对偏移、little-endian 固定宽度整数、严格连续的 data/index/trailer
布局和 IEEE CRC-32。`xvfspackcodec` 定义 `XVFS_PACK_STORE` 与
`XVFS_PACK_LZMA1`；后者是 `5-byte properties + LZMA1 stream`。旧 XSVPACK v1
仍验证 trailer 中的绝对 HeaderOffset 必须等于 Create Offset。两种 parser 都拒绝截断、
溢出、未描述的数据区、重叠 extent、未知 flags/codec、无效 UTF-8、非规范路径、重复
路径、文件/目录冲突、错误 checksum 和超出预算的展开长度。完整字节布局见
`docs/design/vfs.md`。

`xrtVfsPackMount()` 发布只读 provider；写入、创建、截断、追加与同步写标志被拒绝。
stat 和目录枚举只访问不可变索引，不解压内容。ASCII 不区分大小写 mount 会在归档中
存在折叠冲突时失败。

首次打开条目时，一个 loader 在 entry mutex 外读取、解压和校验，其他线程在 condition
上等待。成功校验后才发布不可变 blob；失败保存结构化错误，后续调用得到相同类别和
代码且不重复解压。缓存按解压字节执行硬预算和 LRU 淘汰；淘汰只释放 cache 引用，已
打开 `xfile` 持有独立 blob 引用。大于预算的条目可以读取但不常驻。

`xrtVfsPackStats()` 返回 Hits、Misses、Loads、Failures、ResidentBytes 和 Evictions 的
线程安全快照。`xrtVfsPackFormat()` 返回 AUTO 最终选择的明确格式。pack、mount 与目录
句柄都用引用计数；打开文件在 unmount、namespace 销毁和 cache eviction 后继续有效。

## 诊断与测试注入

`xrtVfsMountId()` 提供稳定、单调的 namespace 内标识，宿主可用它关联挂载、卸载和
请求日志。核心 VFS 不设置全局日志回调，也不为每次 lookup 增加常驻计数；pack 的
缓存与加载数据由 `xrtVfsPackStats()` 提供。业务 generation、模块名和请求身份应由
xs 或应用层记录。

测试中的短读、提前 EOF、I/O 错误和协议违例通过普通 provider/source backend 注入，
OOM 使用 memory-debug fail-after。没有影响整个进程的生产故障开关。provider 已发布的
结构化错误保持原 domain/code，只有违反 ABI 合同时才由 VFS 合成 provider 错误。

## 错误

VFS 自有错误使用 `xrt.vfs` 域和 `xvfserror`。provider 已设置的 I/O、权限、内存、
取消等错误保持原 domain；provider 返回 ERROR 或 callback 失败却没有错误对象时，
VFS 合成 `XVFS_ERROR_PROVIDER`。全部候选 MISS 时返回 `XVFS_ERROR_NOT_FOUND`。

完整的路径、所有权、并发、失败原子性和标准 provider 合同见
`docs/design/vfs.md`。
