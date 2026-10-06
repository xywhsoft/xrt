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

### `xrtVfsCreate`

```c
xvfs xrtVfsCreate(void);
```

创建一个初始 mount snapshot 为空的隔离命名空间。

#### 参数

无参数。

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsCreate();
```
### `xrtVfsDestroy`

```c
void xrtVfsDestroy(xvfs Vfs);
```

释放一个命名空间 reference。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsDestroy(Vfs);
```
### `xrtVfsDirOpen`

```c
xdir xrtVfsDirOpen(xvfs Vfs, cstr sVirtualPath, uint32 iFlags);
```

物化并合并所有可见 provider 条目，返回普通 xdir。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPath` | `cstr` | 规范绝对虚拟路径，不能含逃逸根的路径分量。 |
| `iFlags` | `uint32` | 接口选项位；未知或不支持的位被拒绝。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsDiskCreate`

```c
xvfsdisk xrtVfsDiskCreate(cstr sPhysicalRoot, uint32 iAccess);
```

打开并锚定物理目录；Access 至少包含一个已知权限位。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `sPhysicalRoot` | `cstr` | 要打开并锚定的物理目录路径。 |
| `iAccess` | `uint32` | 磁盘权限，至少一个已知 READ/WRITE 位。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsDiskDestroy`

```c
void xrtVfsDiskDestroy(xvfsdisk Disk);
```

释放一份磁盘 provider 引用，最后一份关闭锚定根句柄。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Disk` | `xvfsdisk` | 已锚定物理目录 provider。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsDiskMount`

```c
xvfsmount xrtVfsDiskMount(xvfs Vfs, cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode, xvfsdisk Disk, uint32 iFlags);
```

挂载锚定目录；首版始终拒绝符号链接和 reparse point。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPrefix` | `cstr` | 规范挂载前缀；lookup 按路径分量匹配。 |
| `iPriority` | `int32` | mount 优先级；高优先级先查找。 |
| `CaseMode` | `xvfscase` | 此 mount 的精确或 ASCII 大小写折叠策略。 |
| `Disk` | `xvfsdisk` | 已锚定物理目录 provider。 |
| `iFlags` | `uint32` | 接口选项位；未知或不支持的位被拒绝。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsDiskRef`

```c
void xrtVfsDiskRef(xvfsdisk Disk);
```

增加或释放一个 disk provider reference。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Disk` | `xvfsdisk` | 已锚定物理目录 provider。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsMemoryCreate`

```c
xvfsmemory xrtVfsMemoryCreate(void);
```

创建可变的 memory provider 构建器。

#### 参数

无参数。

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsMemoryCreate();
```
### `xrtVfsMemoryDestroy`

```c
void xrtVfsMemoryDestroy(xvfsmemory Memory);
```

释放一份内存 provider 引用，最后一份清理索引与拥有型文件数据。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Memory` | `xvfsmemory` | 内存 provider 构建器；Seal 后不可变。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsMemoryDestroy(Builtin);
```
### `xrtVfsMemoryMount`

```c
xvfsmount xrtVfsMemoryMount(xvfs Vfs, cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode, xvfsmemory Memory, uint32 iFlags);
```

只允许挂载已经 seal 的 memory provider。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPrefix` | `cstr` | 规范挂载前缀；lookup 按路径分量匹配。 |
| `iPriority` | `int32` | mount 优先级；高优先级先查找。 |
| `CaseMode` | `xvfscase` | 此 mount 的精确或 ASCII 大小写折叠策略。 |
| `Memory` | `xvfsmemory` | 内存 provider 构建器；Seal 后不可变。 |
| `iFlags` | `uint32` | 接口选项位；未知或不支持的位被拒绝。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsMemoryMount(Vfs, "/app", 0,
		XVFS_CASE_SENSITIVE, Builtin, 0u);
```
### `xrtVfsMemoryPutCopy`

```c
bool xrtVfsMemoryPutCopy(xvfsmemory Memory, cstr sRelativePath, const void* pData, size_t iSize);
```

复制文件数据；相对路径必须规范且不能为空。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Memory` | `xvfsmemory` | 内存 provider 构建器；Seal 后不可变。 |
| `sRelativePath` | `cstr` | 非空规范相对路径，不允许越过 provider 根。 |
| `pData` | `const void*` | 借用的 const void* 对象或调用方结果槽，按上述操作契约使用。 |
| `iSize` | `size_t` | 数据字节数。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsMemoryPutCopy(Builtin, "config/settings.json",
		sBuiltinSettings, sizeof(sBuiltinSettings) - 1u)
```
### `xrtVfsMemoryPutOwned`

```c
bool xrtVfsMemoryPutOwned(xvfsmemory Memory, cstr sRelativePath, bytes pData, size_t iSize);
```

成功时接管由 xrt 分配的缓冲，失败时调用方仍持有它。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Memory` | `xvfsmemory` | 内存 provider 构建器；Seal 后不可变。 |
| `sRelativePath` | `cstr` | 非空规范相对路径，不允许越过 provider 根。 |
| `pData` | `bytes` | 借用的 bytes 对象或调用方结果槽，按上述操作契约使用。 |
| `iSize` | `size_t` | 数据字节数。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsMemoryRef`

```c
void xrtVfsMemoryRef(xvfsmemory Memory);
```

增加或释放一个 memory provider reference。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Memory` | `xvfsmemory` | 内存 provider 构建器；Seal 后不可变。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsMemorySeal`

```c
bool xrtVfsMemorySeal(xvfsmemory Memory);
```

原子构建不可变目录索引；重复调用成功，分配失败可直接重试。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Memory` | `xvfsmemory` | 内存 provider 构建器；Seal 后不可变。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsMemorySeal(Builtin)
```
### `xrtVfsMount`

```c
xvfsmount xrtVfsMount(xvfs Vfs, cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode, const xvfsprovider_v1* pProvider, void* pProviderContext, uint32 iFlags);
```

在规范虚拟 prefix 上发布一个 provider generation。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPrefix` | `cstr` | 规范挂载前缀；lookup 按路径分量匹配。 |
| `iPriority` | `int32` | mount 优先级；高优先级先查找。 |
| `CaseMode` | `xvfscase` | 此 mount 的精确或 ASCII 大小写折叠策略。 |
| `pProvider` | `const xvfsprovider_v1*` | 版本化 provider 操作表，其回调代码必须覆盖 mount 与已打开资源。 |
| `pProviderContext` | `void*` | provider 上下文；引用由挂载协议管理。 |
| `iFlags` | `uint32` | 接口选项位；未知或不支持的位被拒绝。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsMountDestroy`

```c
void xrtVfsMountDestroy(xvfsmount Mount);
```

释放控制句柄引用；卸载命名空间映射使用 Unmount，不能以释放句柄代替。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Mount` | `xvfsmount` | 已发布 mount generation 的控制句柄。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsMountDestroy(OverrideMount);
```
### `xrtVfsMountId`

```c
uint64 xrtVfsMountId(xvfsmount Mount);
```

返回稳定、单调分配的 mount 标识。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Mount` | `xvfsmount` | 已发布 mount generation 的控制句柄。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsMountRef`

```c
void xrtVfsMountRef(xvfsmount Mount);
```

增加或释放一个 mount 控制句柄 reference。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Mount` | `xvfsmount` | 已发布 mount generation 的控制句柄。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsOpen`

```c
xfile xrtVfsOpen(xvfs Vfs, cstr sVirtualPath, const xfileoptions* pOptions);
```

把一个规范绝对虚拟路径打开为普通 xfile。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPath` | `cstr` | 规范绝对虚拟路径，不能含逃逸根的路径分量。 |
| `pOptions` | `const xfileoptions*` | 版本化选项；先使用对应 Init，再调整字段。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsPackCreate`

```c
xvfspack xrtVfsPackCreate(xfile Source, uint64 iOffset, uint64 iLength, const xvfspackoptions* pOptions);
```

成功时接管 Source，失败时调用方仍持有；范围必须包含完整归档。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Source` | `xfile` | 覆盖完整归档的文件；Create 成功转移，失败仍归调用方。 |
| `iOffset` | `uint64` | 归档在 Source 中的起始字节偏移。 |
| `iLength` | `uint64` | 归档完整字节长度。 |
| `pOptions` | `const xvfspackoptions*` | 版本化选项；先使用对应 Init，再调整字段。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsPackDestroy`

```c
void xrtVfsPackDestroy(xvfspack Pack);
```

释放一份归档 provider 引用；最后一份关闭其拥有的 Source。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Pack` | `xvfspack` | 已经完整验证索引的只读归档 provider。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsPackFormat`

```c
xvfspackformat xrtVfsPackFormat(xvfspack Pack);
```

返回 Create 最终选择的明确格式。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Pack` | `xvfspack` | 已经完整验证索引的只读归档 provider。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsPackMount`

```c
xvfsmount xrtVfsPackMount(xvfs Vfs, cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode, xvfspack Pack, uint32 iFlags);
```

挂载已经完整解析并验证索引的只读 pack。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPrefix` | `cstr` | 规范挂载前缀；lookup 按路径分量匹配。 |
| `iPriority` | `int32` | mount 优先级；高优先级先查找。 |
| `CaseMode` | `xvfscase` | 此 mount 的精确或 ASCII 大小写折叠策略。 |
| `Pack` | `xvfspack` | 已经完整验证索引的只读归档 provider。 |
| `iFlags` | `uint32` | 接口选项位；未知或不支持的位被拒绝。 |

#### 返回值

成功交付调用方拥有的句柄；失败为空句柄。按对应 Destroy/Close 释放，mount 的卸载须另调 Unmount。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsPackOptionsInit`

```c
void xrtVfsPackOptionsInit(xvfspackoptions* pOptions);
```

写入生产默认限制和 AUTO 格式。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pOptions` | `xvfspackoptions*` | 版本化选项；先使用对应 Init，再调整字段。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsPackRef`

```c
void xrtVfsPackRef(xvfspack Pack);
```

增加或释放 pack provider reference；最后一个引用关闭 Source。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Pack` | `xvfspack` | 已经完整验证索引的只读归档 provider。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsPackStats`

```c
bool xrtVfsPackStats(xvfspack Pack, xvfspackstats* pStats);
```

取得线程安全的缓存统计快照。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Pack` | `xvfspack` | 已经完整验证索引的只读归档 provider。 |
| `pStats` | `xvfspackstats*` | 输出一致的缓存统计快照。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsReadAll`

```c
bytes xrtVfsReadAll(xvfs Vfs, cstr sVirtualPath, size_t* pSize);
```

读取完整虚拟文件，并在 pSize 之外追加一个零字节。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPath` | `cstr` | 规范绝对虚拟路径，不能含逃逸根的路径分量。 |
| `pSize` | `size_t*` | 输出文件数据字节数，不包含额外终止零字节。 |

#### 返回值

成功为拥有型缓冲，使用 xrtFree 释放；失败为 NULL。精确长度及输出槽规则见上述契约。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsReadAll(Vfs, sPath, &iSize);
```
### `xrtVfsReadAllLimit`

```c
bytes xrtVfsReadAllLimit(xvfs Vfs, cstr sVirtualPath, size_t iLimit, size_t* pSize);
```

在硬字节上限内读取完整虚拟文件，额外追加一个零字节；输出长度不包含该字节。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPath` | `cstr` | 规范绝对虚拟路径，不能含逃逸根的路径分量。 |
| `iLimit` | `size_t` | 硬上限；不会为了判断 EOF 再读取超出上限的一个字节。 |
| `pSize` | `size_t*` | 输出文件数据字节数，不包含额外终止零字节。 |

#### 返回值

成功为拥有型缓冲，使用 xrtFree 释放；失败为 NULL。精确长度及输出槽规则见上述契约。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsRef`

```c
void xrtVfsRef(xvfs Vfs);
```

增加一个命名空间 owned reference。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsStat`

```c
bool xrtVfsStat(xvfs Vfs, cstr sVirtualPath, bool bFollowLink, xfileinfo* pInfo);
```

查询一个规范绝对虚拟路径且不打开文件。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Vfs` | `xvfs` | 有效虚拟命名空间引用。 |
| `sVirtualPath` | `cstr` | 规范绝对虚拟路径，不能含逃逸根的路径分量。 |
| `bFollowLink` | `bool` | 是否请求跟随链接；具体 provider 必须具有对应能力。 |
| `pInfo` | `xfileinfo*` | 返回文件元数据。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)，结合本节参数和生存期规则使用。



### `xrtVfsUnmount`

```c
bool xrtVfsUnmount(xvfsmount Mount);
```

幂等地阻止新 lookup 选择这个 generation。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Mount` | `xvfsmount` | 已发布 mount generation 的控制句柄。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

xrt.vfs 域区分路径、挂载、provider、未找到、不支持、限制、归档损坏与校验和失败。只有 MISS 可以回退到其他 mount；ERROR 保留失败，不能降级为未找到。

#### 范例

参见已注册的 [examples/file/vfs/main.c](../../examples/file/vfs/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtVfsUnmount(OverrideMount);
```
### `xvfscase`

每个 mount 冻结的大小写策略；首版只折叠 ASCII A-Z，不做 Unicode case folding。

```c
typedef enum xvfscase {
	XVFS_CASE_SENSITIVE = 0,
	XVFS_CASE_ASCII_INSENSITIVE = 1
} xvfscase;
```


| 值 | 语义 |
|---|---|
| `XVFS_CASE_SENSITIVE` | 按 UTF-8 字节精确匹配。 |
| `XVFS_CASE_ASCII_INSENSITIVE` | 仅折叠 ASCII A-Z。 |

### `xvfsdir_v1`

provider 打开的目录状态，OPENED 转移 State；MISS/ERROR 不转移且保持全零。

```c
typedef struct xvfsdir_v1 {
	const xvfsdirops_v1* Ops;
	void* State;
} xvfsdir_v1;
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Ops` | `const xvfsdirops_v1*` | 同一物理 Data 节点的计数和真实边枚举操作表。 |
| `State` | `void*` | 此对象的当前状态快照。 |

### `xvfsdirops_v1`

Size/Version 开头的目录操作表，Next 返回条目和 End，Close 回收恰好一次 State。

```c
typedef struct xvfsdirops_v1 {
	uint32 Size;
	uint32 Version;
	bool (*Next)(void* pState, xdirentry* pEntry, bool* pEnd);
	void (*Close)(void* pState);
} xvfsdirops_v1;
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Size` | `uint32` | 结构字节大小，先由对应 Init 初始化。 |
| `Version` | `uint32` | 公共 ABI 版本，不以未知尾字段猜测能力。 |

### `xvfsdiskaccess`

创建 disk context 时冻结的 READ/WRITE 权限，至少一个已知权限位。

```c
typedef enum xvfsdiskaccess {
	XVFS_DISK_READ = 0x01,
	XVFS_DISK_WRITE = 0x02
} xvfsdiskaccess;
```


| 值 | 语义 |
|---|---|
| `XVFS_DISK_READ` | 顺序读取能力位，对应回调或创建权限。 |
| `XVFS_DISK_WRITE` | 顺序写入能力位，对应回调或创建权限。 |

### `xvfserror`

xrt.vfs 稳定域错误码；损坏、校验失败、权限失败都不能被视作 MISS。

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


| 值 | 语义 |
|---|---|
| `XVFS_ERROR_CREATE` | 创建失败 |
| `XVFS_ERROR_PATH` | 虚拟路径无效 |
| `XVFS_ERROR_MOUNT` | 挂载失败 |
| `XVFS_ERROR_UNMOUNT` | 卸载失败 |
| `XVFS_ERROR_NOT_FOUND` | 资源未找到 |
| `XVFS_ERROR_PROVIDER` | provider 协议错误 |
| `XVFS_ERROR_OPEN` | 打开失败 |
| `XVFS_ERROR_STAT` | 查询元数据 |
| `XVFS_ERROR_DIRECTORY` | 枚举目录 |
| `XVFS_ERROR_UNSUPPORTED` | 所需能力不支持 |
| `XVFS_ERROR_LIMIT` | 达到资源硬限制 |
| `XVFS_ERROR_CORRUPT` | 归档损坏 |
| `XVFS_ERROR_CHECKSUM` | 校验和不匹配 |

### `xvfsfile_v1`

provider 打开的文件状态。OPENED 时 Ops/State 被接管，MISS/ERROR 要求输出全零，不转移状态。

```c
typedef struct xvfsfile_v1 {
	const xvfsfileops_v1* Ops;
	void* State;
	uint32 Flags;
	uint32 Reserved;
} xvfsfile_v1;
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Ops` | `const xvfsfileops_v1*` | 同一物理 Data 节点的计数和真实边枚举操作表。 |
| `State` | `void*` | 此对象的当前状态快照。 |
| `Flags` | `uint32` | 当前对象的选项/状态位。 |
| `Reserved` | `uint32` | 保留字段，初始化为零。 |

### `xvfsfilecapability`

文件回调能力位，位与相应函数指针必须严格一致。

```c
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
```


| 值 | 语义 |
|---|---|
| `XVFS_FILE_READ` | 顺序读取能力位，对应回调或创建权限。 |
| `XVFS_FILE_WRITE` | 顺序写入能力位，对应回调或创建权限。 |
| `XVFS_FILE_READ_AT` | 定位读取能力位，对应回调或创建权限。 |
| `XVFS_FILE_WRITE_AT` | 定位写入能力位，对应回调或创建权限。 |
| `XVFS_FILE_SEEK` | 调整逻辑位置能力位，对应回调或创建权限。 |
| `XVFS_FILE_STAT` | 查询元数据能力位，对应回调或创建权限。 |
| `XVFS_FILE_RESIZE` | 修改文件大小能力位，对应回调或创建权限。 |
| `XVFS_FILE_FLUSH` | 刷新输出能力位，对应回调或创建权限。 |

### `xvfsfileops_v1`

Size/Version 开头的文件操作表。State 在 OPENED 时转移，包装失败及普通 Close 都恰好清理一次；未知能力被拒绝。

```c
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
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Size` | `uint32` | 结构字节大小，先由对应 Init 初始化。 |
| `Version` | `uint32` | 公共 ABI 版本，不以未知尾字段猜测能力。 |
| `Capabilities` | `uint64` | 已声明的能力位，必须与对应函数指针一致。 |

### `xvfslookup`

ERROR 不允许回退，MISS 才查下一层，OPENED 转移输出状态的拥有权。

```c
typedef enum xvfslookup {
	XVFS_LOOKUP_ERROR = -1,
	XVFS_LOOKUP_MISS = 0,
	XVFS_LOOKUP_OPENED = 1
} xvfslookup;
```


| 值 | 语义 |
|---|---|
| `XVFS_LOOKUP_ERROR` | 实际访问失败，停止 lookup，禁止回退。 |
| `XVFS_LOOKUP_MISS` | 本 provider 中不存在，继续下一层。 |
| `XVFS_LOOKUP_OPENED` | 成功打开，转移输出状态所有权。 |

### `xvfspackcodec`

逐条 codec：STORE 原样保存，LZMA1 解码须通过大小和校验边界。

```c
typedef enum xvfspackcodec {
	XVFS_PACK_STORE = 0,
	XVFS_PACK_LZMA1 = 1
} xvfspackcodec;
```


| 值 | 语义 |
|---|---|
| `XVFS_PACK_STORE` | 条目原样保存。 |
| `XVFS_PACK_LZMA1` | 条目使用 LZMA1 编解码。 |

### `xvfspackformat`

AUTO 仅按完整 magic 选择格式；解析失败不降级为另一格式。

```c
typedef enum xvfspackformat {
	XVFS_PACK_AUTO = 0,
	XVFS_PACK_XRT_V1 = 1,
	XVFS_PACK_XSVPACK_V1 = 2
} xvfspackformat;
```


| 值 | 语义 |
|---|---|
| `XVFS_PACK_AUTO` | 仅按完整 magic 选择格式。 |
| `XVFS_PACK_XRT_V1` | XRT v1 归档。 |
| `XVFS_PACK_XSVPACK_V1` | 旧 XSVPACK v1 归档。 |

### `xvfspackoptions`

带 Size/Version 的限制集合。非零解析限制是硬上限，CacheBytes 零表示不保留解码缓存引用，先使用 OptionsInit 填入默认值。

```c
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
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Size` | `uint32` | 结构字节大小，先由对应 Init 初始化。 |
| `Version` | `uint32` | 公共 ABI 版本，不以未知尾字段猜测能力。 |
| `Format` | `xvfspackformat` | AUTO 或明确归档格式；读取成功后报告明确格式。 |
| `MaxEntries` | `uint32` | 最大目录项数硬上限。 |
| `MaxIndexBytes` | `uint64` | 最大索引字节硬上限。 |
| `MaxEntryBytes` | `uint64` | 最大单项解码字节硬上限。 |
| `MaxDecodedBytes` | `uint64` | 累计解码字节硬上限。 |
| `CacheBytes` | `uint64` | 允许缓存持有的解码字节，零表示不持有缓存引用。 |

### `xvfspackstats`

线程安全统计快照，命中、未命中、加载、失败与驱逐计数单调递增，ResidentBytes 是当前缓存持有的解码字节。

```c
typedef struct xvfspackstats {
	uint64 Hits;
	uint64 Misses;
	uint64 Loads;
	uint64 Failures;
	uint64 ResidentBytes;
	uint64 Evictions;
} xvfspackstats;
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Hits` | `uint64` | 缓存命中累计数。 |
| `Misses` | `uint64` | 缓存未命中累计数。 |
| `Loads` | `uint64` | 实际加载累计数。 |
| `Failures` | `uint64` | 失败累计数。 |
| `ResidentBytes` | `uint64` | 当前缓存持有的解码字节。 |
| `Evictions` | `uint64` | 缓存驱逐累计数。 |

### `xvfsprovider_v1`

版本化 provider 操作表。ContextRetain/Release 成对提供，挂载复制已知表字段，回调在 namespace 锁外运行。NativeOpen 直接转移原生 xfile，不能同时提供普通 Open。

```c
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
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Size` | `uint32` | 结构字节大小，先由对应 Init 初始化。 |
| `Version` | `uint32` | 公共 ABI 版本，不以未知尾字段猜测能力。 |
| `Capabilities` | `uint64` | 已声明的能力位，必须与对应函数指针一致。 |

### `xvfsprovidercapability`

provider 可选 Stat、Directory、NativeOpen 能力，必须与对应回调一致。

```c
typedef enum xvfsprovidercapability {
	XVFS_PROVIDER_STAT = UINT64_C(0x00000001),
	XVFS_PROVIDER_DIRECTORY = UINT64_C(0x00000002),
	XVFS_PROVIDER_NATIVE_OPEN = UINT64_C(0x00000004)
} xvfsprovidercapability;
```

`XRT_VFS_DIR_OPS_VERSION`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_DIR_OPS_VERSION 1u
```

`XRT_VFS_FILE_OPS_VERSION`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_FILE_OPS_VERSION 1u
```

`XRT_VFS_PACK_CACHE_BYTES_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_PACK_CACHE_BYTES_DEFAULT UINT64_C(67108864)
```

`XRT_VFS_PACK_MAX_DECODED_BYTES_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_PACK_MAX_DECODED_BYTES_DEFAULT UINT64_C(8589934592)
```

`XRT_VFS_PACK_MAX_ENTRIES_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_PACK_MAX_ENTRIES_DEFAULT 100000u
```

`XRT_VFS_PACK_MAX_ENTRY_BYTES_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_PACK_MAX_ENTRY_BYTES_DEFAULT UINT64_C(1073741824)
```

`XRT_VFS_PACK_MAX_INDEX_BYTES_DEFAULT`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_PACK_MAX_INDEX_BYTES_DEFAULT UINT64_C(67108864)
```

`XRT_VFS_PROVIDER_VERSION`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XRT_VFS_PROVIDER_VERSION 1u
```

`XVFS_DISK_ACCESS`：常量值见定义；用于对应接口的版本、容量、默认配置或能力约束。

```c
#define XVFS_DISK_ACCESS 0x03u
```

| 值 | 语义 |
|---|---|
| `XVFS_PROVIDER_STAT` | 查询元数据能力位，对应回调或创建权限。 |
| `XVFS_PROVIDER_DIRECTORY` | 枚举目录能力位，对应回调或创建权限。 |
| `XVFS_PROVIDER_NATIVE_OPEN` | 转移原生文件句柄能力位，对应回调或创建权限。 |
