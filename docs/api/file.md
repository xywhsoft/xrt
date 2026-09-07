# File API

File 提供跨平台的同步文件 I/O：打开/读写/定位、整文件读写、元数据与目录遍历；异步完成式文件见 [file_async.md](file_async.md)。

## 类型与常量

### `xrooterror`

目录根模块稳定错误代码。

```c
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
```

| 值 | 语义 |
|---|---|
| `XROOT_ERROR_OPEN` | 失败 |
| `XROOT_ERROR_CLOSE` | 失败 |
| `XROOT_ERROR_RESOLVE` | 失败 |
| `XROOT_ERROR_ESCAPE` | 失败 |
| `XROOT_ERROR_LIMIT` | 超限 |
| `XROOT_ERROR_FILE` | 失败 |
| `XROOT_ERROR_STAT` | 失败 |
| `XROOT_ERROR_CREATE` | 创建 |
| `XROOT_ERROR_REMOVE` | 失败 |
| `XROOT_ERROR_LINK` | 链接解析失败 |

### `xfileflag`

打开标志允许组合；至少指定 READ 或 WRITE。

```c
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
```

| 值 | 语义 |
|---|---|
| `XFILE_READ` | 读方向 |
| `XFILE_WRITE` | 写方向 |
| `XFILE_CREATE` | 创建 |
| `XFILE_TRUNCATE` | 创建时截断 |
| `XFILE_APPEND` | 追加 |
| `XFILE_EXCLUSIVE` | 独占创建 |
| `XFILE_NOFOLLOW` | 不跟随符号链接 |
| `XFILE_SYNC` | 落盘方式 |
| `XFILE_ASYNC` | 异步标志 |

### `xfileshare`

Windows 共享策略；POSIX 接受这些字段但没有对应打开限制。

```c
typedef enum xfileshare {
	XFILE_SHARE_READ = 0x01,
	XFILE_SHARE_WRITE = 0x02,
	XFILE_SHARE_DELETE = 0x04,
	XFILE_SHARE_ALL = 0x07
} xfileshare;
```

| 值 | 语义 |
|---|---|
| `XFILE_SHARE_READ` | 读方向 |
| `XFILE_SHARE_WRITE` | 写方向 |
| `XFILE_SHARE_DELETE` | DELETE 方法 |
| `XFILE_SHARE_ALL` | 共享全部（读+写+删除） |

### `xfileoptions`

高级打开选项；Mode 只使用 POSIX 权限低 12 位。

```c
typedef struct xfileoptions {
	uint32 Flags;
	uint32 Mode;
	uint32 Share;
} xfileoptions;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `Mode` | `uint32` | 模式 |
| `Share` | `uint32` | Share |

### `xfiletype`

跨平台稳定的文件对象类别。

```c
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
```

| 值 | 语义 |
|---|---|
| `XFILE_TYPE_NONE` | 无 |
| `XFILE_TYPE_FILE` | 普通文件 |
| `XFILE_TYPE_DIRECTORY` | 目录 |
| `XFILE_TYPE_LINK` | 符号链接 |
| `XFILE_TYPE_FIFO` | FIFO 管道 |
| `XFILE_TYPE_SOCKET` | 套接字 |
| `XFILE_TYPE_DEVICE` | 设备文件 |
| `XFILE_TYPE_OTHER` | 其他（设备/管道等） |

### `xfileinfoflag`

元数据可用位避免用零伪装平台不提供的时间或身份。

```c
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
```

| 值 | 语义 |
|---|---|
| `XFILE_INFO_SIZE` | 尺寸 |
| `XFILE_INFO_MODE` | 权限与类型位 |
| `XFILE_INFO_ACCESS_TIME` | ACCESS时间 |
| `XFILE_INFO_MODIFY_TIME` | MODIFY时间 |
| `XFILE_INFO_CREATE_TIME` | 创建时间 |
| `XFILE_INFO_CHANGE_TIME` | CHANGE时间 |
| `XFILE_INFO_IDENTITY` | 文件标识（设备+inode） |
| `XFILE_INFO_LINK_COUNT` | 硬链接数 |

### `xfileinfo`

文件元数据时间统一使用 Unix Epoch 微秒。

```c
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
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xfiletype` | 类型 |
| `Available` | `uint32` | Available |
| `Mode` | `uint32` | 模式 |
| `Attributes` | `uint32` | Attributes |
| `Size` | `uint64` | 字节数 |
| `Device` | `uint64` | Device |
| `Identity` | `uint64` | Identity |
| `LinkCount` | `uint64` | LinkCount |
| `Accessed` | `xtime` | Accessed |
| `Modified` | `xtime` | Modified |
| `Created` | `xtime` | Created |
| `Changed` | `xtime` | Changed |

### `xfileerror`

文件模块稳定错误代码。

```c
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
```

| 值 | 语义 |
|---|---|
| `XFILE_ERROR_OPEN` | 失败 |
| `XFILE_ERROR_READ` | 读方向 |
| `XFILE_ERROR_WRITE` | 写方向 |
| `XFILE_ERROR_SEEK` | 失败 |
| `XFILE_ERROR_STAT` | 失败 |
| `XFILE_ERROR_RESIZE` | 失败 |
| `XFILE_ERROR_SYNC` | 失败 |
| `XFILE_ERROR_CLOSE` | 失败 |
| `XFILE_ERROR_EOF` | 失败 |
| `XFILE_ERROR_COPY` | 失败 |
| `XFILE_ERROR_MOVE` | 失败 |
| `XFILE_ERROR_DELETE` | Delete失败 |
| `XFILE_ERROR_TEMP` | 失败 |
| `XFILE_ERROR_METADATA` | 失败 |
| `XFILE_ERROR_TOUCH` | 失败 |
| `XFILE_ERROR_TEXT` | 文本 |
| `XFILE_ERROR_LIMIT` | 超限 |
| `XFILE_ERROR_LOCK` | 失败 |
| `XFILE_ERROR_MAP` | 内存映射失败 |

### `xfilelock`

文件锁支持共享读和排他写两种跨进程模式。

```c
typedef enum xfilelock {
	XFILE_LOCK_SHARED = 1,
	XFILE_LOCK_EXCLUSIVE
} xfilelock;
```

| 值 | 语义 |
|---|---|
| `XFILE_LOCK_SHARED` | 共享锁 |
| `XFILE_LOCK_EXCLUSIVE` | 独占创建 |

### `xfilemapflag`

映射始终可读，可选择共享写或私有写，两种写模式不能同时启用。

```c
typedef enum xfilemapflag {
	XFILE_MAP_READ = 0x01,
	XFILE_MAP_WRITE = 0x02,
	XFILE_MAP_COPY = 0x04
} xfilemapflag;
```

| 值 | 语义 |
|---|---|
| `XFILE_MAP_READ` | 读方向 |
| `XFILE_MAP_WRITE` | 写方向 |
| `XFILE_MAP_COPY` | 写时复制映射 |

### `xdirflag`

完整元数据可能增加每个条目一次系统查询，默认只返回枚举器已有信息。

```c
typedef enum xdirflag {
	XDIR_STAT = 0x01,
	XDIR_FOLLOW_LINKS = 0x02,
	XDIR_INCLUDE_DOTS = 0x04
} xdirflag;
```

| 值 | 语义 |
|---|---|
| `XDIR_STAT` | stat 信息 |
| `XDIR_FOLLOW_LINKS` | 跟随符号链接 |
| `XDIR_INCLUDE_DOTS` | 包含 . 与 .. 项 |

### `xdirnext`

目录迭代结果明确区分条目、正常结束和失败。

```c
typedef enum xdirnext {
	XDIR_NEXT_ERROR = -1,
	XDIR_NEXT_END = 0,
	XDIR_NEXT_ITEM = 1
} xdirnext;
```

| 值 | 语义 |
|---|---|
| `XDIR_NEXT_ERROR` | 失败 |
| `XDIR_NEXT_END` | END |
| `XDIR_NEXT_ITEM` | 已产出条目 |

### `xdirentryflag`

POSIX 文件名允许原始字节；该标志表示名称已经通过严格 UTF-8 检查。

```c
typedef enum xdirentryflag {
	XDIR_ENTRY_UTF8 = 0x01
} xdirentryflag;
```

| 值 | 语义 |
|---|---|
| `XDIR_ENTRY_UTF8` | 名称为 UTF-8 |

### `xdirentry`

名称和元数据借用到下一次迭代或关闭；Name 始终额外带零结尾。

```c
typedef struct xdirentry {
	xstrview Name;
	xfileinfo Info;
	uint32 Flags;
} xdirentry;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Name` | `xstrview` | 名称 |
| `Info` | `xfileinfo` | 信息输出 |
| `Flags` | `uint32` | 标志位 |

### `xdirroots`

系统根目录列表拥有每个字符串以及指针数组。

```c
typedef struct xdirroots {
	str* Items;
	size_t Count;
} xdirroots;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Items` | `str*` | 元素数组 |
| `Count` | `size_t` | 数量 |

### `xdirerror`

目录模块稳定错误代码。

```c
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
```

| 值 | 语义 |
|---|---|
| `XDIR_ERROR_OPEN` | 失败 |
| `XDIR_ERROR_NEXT` | 失败 |
| `XDIR_ERROR_CLOSE` | 失败 |
| `XDIR_ERROR_CREATE` | 创建 |
| `XDIR_ERROR_REMOVE` | 失败 |
| `XDIR_ERROR_ROOTS` | 失败 |
| `XDIR_ERROR_ENTRY` | 失败 |
| `XDIR_ERROR_TEMP` | 临时路径不可用 |

### `xtreecopyflag`

目录树复制默认要求目标不存在并保留符号链接。

```c
typedef enum xtreecopyflag {
	XTREE_COPY_MERGE = 0x01,
	XTREE_COPY_REPLACE = 0x02,
	XTREE_COPY_FOLLOW_LINKS = 0x04,
	XTREE_COPY_SKIP_LINKS = 0x08,
	XTREE_COPY_ONE_FILESYSTEM = 0x10,
	XTREE_COPY_SKIP_SPECIAL = 0x20,
	XTREE_COPY_METADATA = 0x40
} xtreecopyflag;
```

| 值 | 语义 |
|---|---|
| `XTREE_COPY_MERGE` | MERGE |
| `XTREE_COPY_REPLACE` | REPLACE |
| `XTREE_COPY_FOLLOW_LINKS` | FOLLOWLINKS |
| `XTREE_COPY_SKIP_LINKS` | 跳过LINKS |
| `XTREE_COPY_ONE_FILESYSTEM` | ONEFILESYSTEM |
| `XTREE_COPY_SKIP_SPECIAL` | 跳过SPECIAL |
| `XTREE_COPY_METADATA` | 复制元数据 |

### `xtreecopyoptions`

高级目录树复制选项。

```c
typedef struct xtreecopyoptions {
	uint32 Flags;
} xtreecopyoptions;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |

### `xtreeerror`

目录树模块稳定错误代码。

```c
typedef enum xtreeerror {
	XTREE_ERROR_OPTIONS = 1,
	XTREE_ERROR_SOURCE,
	XTREE_ERROR_TARGET,
	XTREE_ERROR_DESCENDANT,
	XTREE_ERROR_LINK_CYCLE,
	XTREE_ERROR_SPECIAL,
	XTREE_ERROR_ROOT
} xtreeerror;
```

| 值 | 语义 |
|---|---|
| `XTREE_ERROR_OPTIONS` | Options失败 |
| `XTREE_ERROR_SOURCE` | 源码位置 |
| `XTREE_ERROR_TARGET` | 失败 |
| `XTREE_ERROR_DESCENDANT` | 失败 |
| `XTREE_ERROR_LINK_CYCLE` | 失败 |
| `XTREE_ERROR_SPECIAL` | 失败 |
| `XTREE_ERROR_ROOT` | 根操作非法 |

### `xlinkerror`

链接模块稳定错误代码。

```c
typedef enum xlinkerror {
	XLINK_ERROR_CREATE = 1,
	XLINK_ERROR_READ,
	XLINK_ERROR_DELETE,
	XLINK_ERROR_FORMAT
} xlinkerror;
```

| 值 | 语义 |
|---|---|
| `XLINK_ERROR_CREATE` | 创建 |
| `XLINK_ERROR_READ` | 读方向 |
| `XLINK_ERROR_DELETE` | Delete失败 |
| `XLINK_ERROR_FORMAT` | 链接格式非法 |

### `xfifoerror`

FIFO 模块稳定错误代码。

```c
typedef enum xfifoerror {
	XFIFO_ERROR_CREATE = 1
} xfifoerror;
```

| 值 | 语义 |
|---|---|
| `XFIFO_ERROR_CREATE` | 管道创建失败 |

### `xwalkflag`

遍历默认不跟随链接，可选择限制在根文件系统内。

```c
typedef enum xwalkflag {
	XWALK_FOLLOW_LINKS = 0x01,
	XWALK_ONE_FILESYSTEM = 0x02
} xwalkflag;
```

| 值 | 语义 |
|---|---|
| `XWALK_FOLLOW_LINKS` | XWALKFOLLOWLINKS |
| `XWALK_ONE_FILESYSTEM` | 限制在根文件系统内 |

### `xwalkevent`

每个目录产生进入和离开事件，其他对象产生条目事件。

```c
typedef enum xwalkevent {
	XWALK_ENTER = 1,
	XWALK_ITEM,
	XWALK_LEAVE
} xwalkevent;
```

| 值 | 语义 |
|---|---|
| `XWALK_ENTER` | ENTER |
| `XWALK_ITEM` | ITEM |
| `XWALK_LEAVE` | 离开目录 |

### `xwalkentryflag`

条目标志保留物理链接身份以及不能继续下降的原因。

```c
typedef enum xwalkentryflag {
	XWALK_ENTRY_UTF8 = 0x01,
	XWALK_ENTRY_LINK = 0x02,
	XWALK_ENTRY_CYCLE = 0x04,
	XWALK_ENTRY_CROSS_FILESYSTEM = 0x08
} xwalkentryflag;
```

| 值 | 语义 |
|---|---|
| `XWALK_ENTRY_UTF8` | UTF-8 |
| `XWALK_ENTRY_LINK` | LINK |
| `XWALK_ENTRY_CYCLE` | CYCLE |
| `XWALK_ENTRY_CROSS_FILESYSTEM` | 进入新文件系统 |

### `xwalkcontrol`

回调可继续、跳过当前目录、成功停止或报告失败。

```c
typedef enum xwalkcontrol {
	XWALK_CONTINUE = 0,
	XWALK_SKIP,
	XWALK_STOP,
	XWALK_ERROR
} xwalkcontrol;
```

| 值 | 语义 |
|---|---|
| `XWALK_CONTINUE` | CONTINUE（100 继续） |
| `XWALK_SKIP` | 跳过 |
| `XWALK_STOP` | STOP |
| `XWALK_ERROR` | 失败 |

### `xwalkerroraction`

遍历系统错误可终止、跳过当前路径，或成功停止。

```c
typedef enum xwalkerroraction {
	XWALK_ERROR_ABORT = 0,
	XWALK_ERROR_SKIP,
	XWALK_ERROR_STOP
} xwalkerroraction;
```

| 值 | 语义 |
|---|---|
| `XWALK_ERROR_ABORT` | 失败 |
| `XWALK_ERROR_SKIP` | 跳过 |
| `XWALK_ERROR_STOP` | 停止 |

### `xwalkoptions`

遍历选项为空时等价于不跟随链接且深度无限。

```c
typedef struct xwalkoptions {
	uint32 Flags;
	size_t MaxDepth;
	xwalkerrorproc OnError;
} xwalkoptions;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Flags` | `uint32` | 标志位 |
| `MaxDepth` | `size_t` | MaxDepth |
| `OnError` | `xwalkerrorproc` | OnError |

### `xwalkentry`

Path、Parent 和 Name 只在回调期间借用，Info 在跟随链接时描述目标。

```c
typedef struct xwalkentry {
	cstr Path;
	xstrview Parent;
	xstrview Name;
	xfileinfo Info;
	xwalkevent Event;
	uint32 Flags;
	size_t Depth;
} xwalkentry;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Path` | `cstr` | 路径 |
| `Parent` | `xstrview` | Parent |
| `Name` | `xstrview` | 名称 |
| `Info` | `xfileinfo` | 信息输出 |
| `Event` | `xwalkevent` | Event |
| `Flags` | `uint32` | 标志位 |
| `Depth` | `size_t` | Depth |

### `xwalkstats`

遍历统计按对象计数，目录只在进入时计一次。

```c
typedef struct xwalkstats {
	uint64 Items;
	uint64 Files;
	uint64 Directories;
	uint64 Links;
	uint64 Others;
	uint64 Bytes;
	bool Stopped;
} xwalkstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Items` | `uint64` | 元素数组 |
| `Files` | `uint64` | Files |
| `Directories` | `uint64` | Directories |
| `Links` | `uint64` | Links |
| `Others` | `uint64` | Others |
| `Bytes` | `uint64` | Bytes |
| `Stopped` | `bool` | Stopped |

### `xwalkerror`

遍历模块稳定错误代码。

```c
typedef enum xwalkerror {
	XWALK_ERROR_OPTIONS = 1,
	XWALK_ERROR_CALLBACK,
	XWALK_ERROR_IDENTITY,
	XWALK_ERROR_OVERFLOW
} xwalkerror;
```

| 值 | 语义 |
|---|---|
| `XWALK_ERROR_OPTIONS` | Options失败 |
| `XWALK_ERROR_CALLBACK` | 回调失败 |
| `XWALK_ERROR_IDENTITY` | 失败 |
| `XWALK_ERROR_OVERFLOW` | 深度超限 |

### `xwalkerrorproc`

错误和路径只在回调期间借用；用户数据与条目回调共用。

```c
typedef xwalkerroraction (*xwalkerrorproc)(cstr sPath,
	const xerror* pError, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xwalkproc`

遍历回调不拥有条目，返回错误时应先设置结构化错误。

```c
typedef xwalkcontrol (*xwalkproc)(const xwalkentry* pEntry, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 设计契约

文件体系按使用层次拆成十三个可裁剪功能组。每一层只复用下层原语，不维护第二套文件实现：

| 功能宏 | 能力 | 直接依赖 |
| --- | --- | --- |
| `XRT_FEATURE_FILE` | 句柄、二进制 IO、定位、元数据和路径基础操作 | `path_system`、`time` |
| `XRT_FEATURE_FILE_LOCK` | 跨进程共享、排他和字节区间锁 | `file` |
| `XRT_FEATURE_FILE_MAP` | 只读、共享写和私有写内存映射 | `file` |
| `XRT_FEATURE_FILE_TEMP` | 安全临时文件和公共临时命名底座 | `file`、`random_secure` |
| `XRT_FEATURE_FILE_WHOLE` | 整文件、原子写、复制和移动 | `file_temp` |
| `XRT_FEATURE_FILE_TEXT` | 编码检测和 UTF-8 文本读写 | `file_whole`、`charset_detect` |
| `XRT_FEATURE_DIR` | 目录创建、枚举、根目录和空目录操作 | `file` |
| `XRT_FEATURE_DIR_TEMP` | 安全临时目录 | `dir`、`file_temp` |
| `XRT_FEATURE_FILE_WALK` | 可控深度优先遍历和统计 | `dir` |
| `XRT_FEATURE_FILE_LINK` | 符号链接和硬链接 | `file` |
| `XRT_FEATURE_FILE_ROOT` | 句柄锚定的根内路径解析和基础操作 | `file_link` |
| `XRT_FEATURE_FILE_TREE` | 目录树复制、移动、删除、清理和统计 | `file_walk`、`file_whole`、`file_link` |
| `XRT_FEATURE_FILE_FIFO` | POSIX FIFO 创建 | `file` |

路径统一使用 UTF-8。Windows 使用宽字符系统调用并严格转换 UTF-8；POSIX 路径按原始字节交给系统，目录条目通过标志说明名称是否为严格有效 UTF-8。

所有 `xfile`、`xdir` 和返回的拥有内存都由 XRT 分配器管理。`xfile` 与 `xdir` 是不透明对象，成功关闭后立即失效；即使系统关闭动作返回失败，对象也已经销毁，不能重试关闭或继续使用。

文件对象包含一个共享系统游标。多个线程可以并发使用绝对偏移读写；依赖共享游标的读写和定位仍应由调用方按一组逻辑操作同步。Windows 实现会在单次共享游标或绝对偏移操作内部使用轻量锁，保证绝对偏移操作保存并恢复游标；POSIX 直接使用 `pread`/`pwrite`。对象关闭和原生句柄访问不参与内部生命周期同步。独立打开的文件对象拥有独立游标。`xrtFileNative` 是明确的原生逃生接口，使用原生句柄后由调用方维护 XRT 契约。

Windows 追加文件对象内部持有同一次打开产生的原子追加数据句柄和控制句柄，后者只供文件锁等不能使用追加权限的系统操作。`xrtFileNative` 只返回数据句柄，不能借此绕过追加语义；`xrtClose` 统一释放两个句柄。普通文件仍只持有一个句柄。

## 文件打开

```c
typedef struct xfile_impl* xfile;

typedef enum xfileflag {
	XFILE_READ = 0x0001,
	XFILE_WRITE = 0x0002,
	XFILE_CREATE = 0x0004,
	XFILE_TRUNCATE = 0x0008,
	XFILE_APPEND = 0x0010,
	XFILE_EXCLUSIVE = 0x0020,
	XFILE_NOFOLLOW = 0x0040,
	XFILE_SYNC = 0x0080
} xfileflag;

typedef enum xfileshare {
	XFILE_SHARE_READ = 0x01,
	XFILE_SHARE_WRITE = 0x02,
	XFILE_SHARE_DELETE = 0x04,
	XFILE_SHARE_ALL = 0x07
} xfileshare;

typedef struct xfileoptions {
	uint32 Flags;
	uint32 Mode;
	uint32 Share;
} xfileoptions;
```

至少指定 `READ` 或 `WRITE`。创建、截断、追加、排他和同步标志要求写权限；`EXCLUSIVE` 还要求 `CREATE`。`APPEND` 使用系统追加语义，每次系统写都定位到当时文件末尾，不等价于打开后手动 `seek`。`APPEND | TRUNCATE` 在同一次打开中先清空文件，此后仍保持内核原子追加；Windows 从原打开句柄复制受限追加句柄，不进行第二次按路径打开。

`NOFOLLOW` 禁止跟随末级符号链接或 Windows 重解析点，适合检查和管理链接本身；它不阻止父目录中的链接。`SYNC` 请求系统同步写语义，但不能替代应用级事务。平台没有对应原语时返回 `XERR_UNSUPPORTED`，不会静默忽略。`Mode` 只接受 POSIX 权限低 12 位，创建时仍受进程 `umask` 影响；Windows 接受但忽略。`Share` 只控制 Windows 共享策略，POSIX 没有对应的打开限制。

```c
void xrtFileOptionsInit(xfileoptions* pOptions);
xfile xrtFileOpen(cstr sPath, const xfileoptions* pOptions);
xfile xrtOpen(cstr sPath, uint32 iFlags);
bool xrtClose(xfile File);
```

`xrtFileOptionsInit` 初始化为只读、`0666` 和 `XFILE_SHARE_ALL`。传入空选项的 `xrtFileOpen` 使用同一默认值。`xrtOpen` 是常用路径，只覆盖标志。文件打开拒绝目录对象。

新打开的 Windows 句柄和 POSIX 文件描述符默认不可由子进程继承。POSIX 优先在 `open` 时原子设置 `O_CLOEXEC`，旧平台回退到 `FD_CLOEXEC`；该策略是固定安全契约，不额外暴露容易误用的“允许继承”标志。

```c
xfile File = xrtOpen("data.bin",
	XFILE_READ | XFILE_WRITE | XFILE_CREATE);

if ( File != NULL ) {
	(void)xrtClose(File);
}
```

### `xrtFileOptionsInit`

初始化文件打开选项为零值（默认权限与共享策略）。

```c
void xrtFileOptionsInit(xfileoptions* pOptions);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输出 | 非空 | 接收默认选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化，不失败 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[file/io_tour · 选项打开](../../examples/file/io_tour/main.c) · 观察

```c
	(void)xrtFileOptionsInit(&Options);
```


### `xrtFileOpen`

使用完整选项（标志/权限/共享/临时语义）打开文件。

```c
xfile xrtFileOpen(cstr sPath, const xfileoptions* pOptions);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |
| `pOptions` | 输入 | 允许空 | 空 = 仅默认标志 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 打开的文件对象 | — |
| `NULL` | 打开失败 | 系统错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 选项打开](../../examples/file/io_tour/main.c) · 观察

```c
	File = xrtFileOpen(sPath, &Options);
```


### `xrtOpen`

使用默认权限和共享策略按标志打开文件的便捷入口。

```c
xfile xrtOpen(cstr sPath, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |
| `iFlags` | 输入 | `XFILE_*` 组合 | 打开标志（READ/WRITE/CREATE/TRUNCATE/APPEND 等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 打开的文件对象 | — |
| `NULL` | 打开失败 | 系统错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 一次创建](../../examples/file/basic/main.c) · 观察

```c
	xfile File = xrtOpen("xrt-file-example.tmp",
		XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
```


## 二进制 IO

```c
bool xrtRead(xfile File, ptr pBuffer, size_t iRequest, size_t* pRead);
bool xrtWrite(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten);
bool xrtReadFull(xfile File, ptr pBuffer,
	size_t iRequest, size_t* pRead);
bool xrtWriteFull(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten);
bool xrtReadAt(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead);
bool xrtWriteAt(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten);
bool xrtReadAtFull(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead);
bool xrtWriteAtFull(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten);
```

`xrtRead` 和 `xrtWrite` 各执行一次有效系统 IO，允许成功短读或短写。零长度请求允许缓冲为空。计数输出可为空；提供时会先清零，成功后写入实际数量。

EOF 不是 `xrtRead` 的错误：函数返回 `true` 且读取量为零。`xrtReadFull` 持续读取到填满缓冲，提前 EOF 返回 `false`、错误码为 `XFILE_ERROR_EOF`，并通过 `pRead` 保留已经读取的字节数。`xrtWriteFull` 持续写入；系统成功但不再产生写进度时也返回失败，避免死循环。

`xrtReadAt` 和 `xrtWriteAt` 从绝对字节偏移执行单次 IO，不读取也不修改共享游标；完整版本保持相同 EOF、短读、短写和部分计数契约。偏移与实际请求范围必须处于有符号 64 位文件范围。追加句柄拒绝绝对偏移写，避免 POSIX `O_APPEND` 与 Windows 追加访问产生平台分歧。零长度操作仍验证文件对象和访问权限，但不限制偏移值。

```c
unsigned char Header[16];
size_t iRead;

if ( !xrtReadFull(File, Header, sizeof(Header), &iRead) ) {
	/* iRead 保留失败前已经取得的数据量。 */
}
```

### `xrtClose`

关闭并销毁文件对象；即使系统关闭失败也不再允许使用该对象。

```c
bool xrtClose(xfile File);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 允许空 | 打开的文件对象；空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已关闭并释放 | — |
| `false` | 系统关闭失败（对象仍被销毁） | 系统错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 收尾](../../examples/file/basic/main.c) · 观察

```c
	if ( !xrtClose(File) ) {
		return 1;
	}
```


### `xrtRead`

单次读取，成功读取零字节表示 EOF。

```c
bool xrtRead(xfile File, ptr pBuffer, size_t iRequest, size_t* pRead);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `pBuffer` | 输出 | 非空 | 接收缓冲 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pRead` | 输出 | 可空 | 实际读取量（可为 0 = EOF） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已读取（可能短读） | — |
| `false` | 参数或系统错误 | `*pRead` 语义不定 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 单次读写](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtRead(File, Buffer, sizeof(Buffer), &iDone) || (iDone != 4u) ) {
```


### `xrtWrite`

单次写入，允许成功短写。

```c
bool xrtWrite(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `pBuffer` | 输入 | 借用 | 写入数据 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pWritten` | 输出 | 可空 | 实际写入量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写入（可能短写） | — |
| `false` | 参数或系统错误 | `*pWritten` 语义不定 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 单次读写](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtWrite(File, "abcd", 4u, &iDone) || (iDone != 4u) ) {
```


### `xrtReadFull`

持续读取到填满缓冲；提前 EOF 返回失败并保留实际读取量。

```c
bool xrtReadFull(xfile File, ptr pBuffer,
	size_t iRequest, size_t* pRead);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `pBuffer` | 输出 | 非空 | 接收缓冲 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pRead` | 输出 | 可空 | 实际读取量（失败时保留） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 缓冲已填满 | — |
| `false` | 提前 EOF 或系统错误 | `*pRead` 保留已读量 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 完整读取](../../examples/file/basic/main.c) · 观察

```c
		 !xrtReadFull(File, arrText, sizeof(arrText), NULL) ) {
```


### `xrtWriteFull`

持续写入到全部完成；失败时保留实际写入量。

```c
bool xrtWriteFull(xfile File, const void* pBuffer,
	size_t iRequest, size_t* pWritten);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `pBuffer` | 输入 | 借用 | 写入数据 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pWritten` | 输出 | 可空 | 实际写入量（失败时保留） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 全部写入完成 | — |
| `false` | 系统错误 | `*pWritten` 保留已写量 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 一次创建](../../examples/file/basic/main.c) · 观察

```c
	if ( !xrtWriteFull(File, sText, sizeof(sText), NULL) ||
```


### `xrtReadAt`

从绝对偏移单次读取，不改变共享文件游标。

```c
bool xrtReadAt(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `pBuffer` | 输出 | 非空 | 接收缓冲 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pRead` | 输出 | 可空 | 实际读取量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已读取（可能短读） | — |
| `false` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 定位读写](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtReadAt(File, 0u, Buffer, 4u, &iDone) || (iDone != 4u) ) {
```


### `xrtWriteAt`

向绝对偏移单次写入，不改变共享文件游标。

```c
bool xrtWriteAt(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `pBuffer` | 输入 | 借用 | 写入数据 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pWritten` | 输出 | 可空 | 实际写入量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写入（可能短写） | — |
| `false` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 定位读写](../../examples/file/io_tour/main.c) · 观察

```c
	(void)xrtWriteAt(File, 0u, "AB", 2u, NULL);
```


### `xrtReadAtFull`

从绝对偏移持续读取到填满缓冲；预读文件头不扰动流式游标。

```c
bool xrtReadAtFull(xfile File, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `pBuffer` | 输出 | 非空 | 接收缓冲 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pRead` | 输出 | 可空 | 实际读取量（失败时保留） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 缓冲已填满 | — |
| `false` | 提前 EOF 或系统错误 | `*pRead` 保留已读量 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 预读](../../examples/file/basic/main.c) · 观察

```c
		 !xrtReadAtFull(File, 0, arrText, sizeof(arrText), NULL) ||
```


### `xrtWriteAtFull`

向绝对偏移持续写入到全部完成。

```c
bool xrtWriteAtFull(xfile File, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `pBuffer` | 输入 | 借用 | 写入数据 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pWritten` | 输出 | 可空 | 实际写入量（失败时保留） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 全部写入完成 | — |
| `false` | 系统错误 | `*pWritten` 保留已写量 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 定位读写](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtWriteAtFull(File, 4u, "efgh", 4u, NULL) ) {
```


## 定位和大小

```c
typedef enum xseek {
	XSEEK_START = 0,
	XSEEK_CURRENT,
	XSEEK_END
} xseek;

bool xrtSeek(xfile File, int64 iOffset, xseek Origin, uint64* pPosition);
bool xrtTell(xfile File, uint64* pPosition);
bool xrtFileSize(xfile File, uint64* pSize);
bool xrtFileResize(xfile File, uint64 iSize);
bool xrtFileSetSize(cstr sPath, uint64 iSize);
bool xrtFlush(xfile File);
uint32 xrtFileFlags(xfile File);
intptr_t xrtFileNative(xfile File);
```

定位、文件大小和偏移均为 64 位。`xrtSeek` 的位置输出可为空，失败时不承诺输出；`xrtTell` 要求输出。`xrtFileResize` 修改已经以写权限打开的文件，扩大时产生平台定义的稀疏或零填充区域。追加句柄拒绝调整大小，避免 POSIX 可截断而 Windows 原子追加权限不能截断的跨平台分歧。`xrtFileSetSize` 是常用路径，要求普通文件已经存在，不隐式创建。

`xrtFlush` 把文件数据和系统要求的必要元数据提交到稳定存储。它只覆盖当前文件，不同步父目录项。`xrtFileFlags` 返回对象创建时经过验证的打开标志，供分层适配器在接管前检查读写能力。`xrtFileNative` 返回 Windows `HANDLE` 或 POSIX 文件描述符的整数表示，失败返回 `-1`；原生句柄由 `xfile` 持有，调用方不能自行关闭。

### `xrtSeek`

按 64 位偏移移动共享文件游标。

```c
bool xrtSeek(xfile File, int64 iOffset, xseek Origin, uint64* pPosition);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iOffset` | 输入 | 可负 | 偏移量 |
| `Origin` | 输入 | `XSEEK_START/CURRENT/END` | 偏移基准 |
| `pPosition` | 输出 | 可空 | 接收新游标位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 游标已移动 | — |
| `false` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 游标](../../examples/file/basic/main.c) · 观察

```c
		 !xrtSeek(File, 3, XSEEK_START, NULL) ||
```


### `xrtTell`

返回共享文件游标位置。

```c
bool xrtTell(xfile File, uint64* pPosition);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `pPosition` | 输出 | 非空 | 接收游标位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 位置已写出 | — |
| `false` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 游标](../../examples/file/basic/main.c) · 观察

```c
		 !xrtTell(File, &iPosition) || (iPosition != 3u) ||
```


### `xrtFlush`

把文件数据和必要元数据提交到稳定存储。

```c
bool xrtFlush(xfile File);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已提交稳定存储 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 单次读写](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtFlush(File) ) {
```


### `xrtFileSize`

返回打开对象当前大小。

```c
bool xrtFileSize(xfile File, uint64* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `pSize` | 输出 | 非空 | 接收字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 大小已写出 | — |
| `false` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 元数据](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtFileSize(File, &iSize) || (iSize != 8u) ) {
```


### `xrtFileResize`

修改打开文件的大小（截断或扩展，扩展区为零填充）。

```c
bool xrtFileResize(xfile File, uint64 iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iSize` | 输入 | — | 新大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 大小已修改 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 截断](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtFileResize(File, 4u) || !xrtFileSize(File, &iSize) ||
```


### `xrtFileSetSize`

打开路径并修改普通文件大小，文件必须已经存在。

```c
bool xrtFileSetSize(cstr sPath, uint64 iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 已存在的文件路径 |
| `iSize` | 输入 | — | 新大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 大小已修改 | — |
| `false` | 路径不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 截断](../../examples/file/io_tour/main.c) · 观察

```c
if ( !xrtFileSetSize(sPath, 2u) ) {
		goto cleanup;
	}
```


## 文件锁

```c
typedef enum xfilelock {
	XFILE_LOCK_SHARED = 1,
	XFILE_LOCK_EXCLUSIVE
} xfilelock;

bool xrtFileLockRange(xfile File, xfilelock Mode,
	uint64 iOffset, uint64 iSize, bool bWait);
bool xrtFileUnlockRange(xfile File,
	uint64 iOffset, uint64 iSize);
bool xrtFileLock(xfile File, xfilelock Mode, bool bWait);
bool xrtFileUnlock(xfile File);
```

共享锁要求读权限，排他锁要求写权限。区间使用半开范围；`iSize=0` 表示从偏移到文件末端，并在系统支持时覆盖后续增长。偏移和有限长度必须处于有符号 64 位文件范围。解除锁时应传入完全相同的范围；整文件函数是偏移和长度均为零的便捷封装。

`bWait=false` 只尝试一次，存在跨进程冲突时返回 `XERR_AGAIN` 和 `XFILE_ERROR_LOCK`。`bWait=true` 使用系统阻塞锁；当前同步入口没有超时或取消语义，不应放在必须保持响应的事件循环线程。以后可取消的锁等待会建立在任务和上下文层，不改变本组同步原语。

文件锁是协调协议，不是线程互斥量。POSIX 使用 `fcntl` 建议锁，同一进程内的线程不会彼此冲突，而且进程关闭指向同一文件的其他描述符也可能释放锁；Windows 锁与句柄关联，并会影响遵守系统锁语义的 IO。跨平台代码应在进程内另用同步原语、显式解锁，并避免在持锁期间关闭该文件的其他句柄。锁不会保护通过其他文件名、映射或不遵守建议锁的访问。

### `xrtFileLock`

锁定整个文件；`bWait` 决定冲突时阻塞或立即失败。

```c
bool xrtFileLock(xfile File, xfilelock Mode, bool bWait);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `Mode` | 输入 | `XFILE_LOCK_SHARED/EXCLUSIVE` | 锁模式 |
| `bWait` | 输入 | — | 冲突时是否阻塞等待 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已取得锁 | — |
| `false` | 冲突（非阻塞）或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_AGAIN` — 非阻塞模式遇冲突（如实现设置）

#### 范例

[file/lock · 全文件锁](../../examples/file/lock/main.c) · 观察

```c
	if ( !xrtFileLock(File, XFILE_LOCK_EXCLUSIVE, false) ) {
```


### `xrtFileUnlock`

解除整个文件锁。

```c
bool xrtFileUnlock(xfile File);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 已上锁的文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解锁 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/lock · 全文件锁](../../examples/file/lock/main.c) · 观察

```c
	if ( !xrtFileUnlock(File) || !xrtClose(File) ) {
```


### `xrtFileLockRange`

锁定字节区间；`iSize` 为零表示从偏移到文件末端及后续增长。

```c
bool xrtFileLockRange(xfile File, xfilelock Mode,
	uint64 iOffset, uint64 iSize, bool bWait);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `Mode` | 输入 | — | 共享或排他 |
| `iOffset` | 输入 | — | 区间起点 |
| `iSize` | 输入 | 零 = 到末端 | 区间长度 |
| `bWait` | 输入 | — | 冲突阻塞 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 区间锁已取得 | — |
| `false` | 冲突或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 区间锁](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtFileLockRange(File, XFILE_LOCK_SHARED, 0u, 4u, true) ) {
```


### `xrtFileUnlockRange`

解除完全相同的字节区间锁（参数必须与加锁时逐项一致）。

```c
bool xrtFileUnlockRange(xfile File,
	uint64 iOffset, uint64 iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iOffset` | 输入 | 与加锁一致 | 区间起点 |
| `iSize` | 输入 | 与加锁一致 | 区间长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解锁 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 区间锁](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtFileUnlockRange(File, 0u, 4u) ) {
```


## 文件映射

```c
typedef struct xfilemap_impl* xfilemap;

typedef enum xfilemapflag {
	XFILE_MAP_READ = 0x01,
	XFILE_MAP_WRITE = 0x02,
	XFILE_MAP_COPY = 0x04
} xfilemapflag;

xfilemap xrtFileMap(xfile File, uint64 iOffset,
	size_t iSize, uint32 iFlags);
ptr xrtFileMapData(xfilemap Map);
size_t xrtFileMapSize(xfilemap Map);
bool xrtFileMapFlush(xfilemap Map,
	size_t iOffset, size_t iSize);
bool xrtFileUnmap(xfilemap Map);
```

映射必须包含 `READ`。`READ | WRITE` 创建共享写映射并要求文件以非追加方式读写打开；追加句柄只允许只读或私有写映射，不能绕过原子追加契约修改任意偏移。`READ | COPY` 创建可修改但不回写文件的私有映射；两种写标志互斥。实现会在内部按系统分配粒度扩大并对齐视图，`xrtFileMapData` 仍精确指向调用方请求的偏移。

`xrtFileMap` 在创建时验证范围不越过当前文件末端。`iSize=0` 表示映射到当前末端；偏移恰好等于末端时返回有效空映射，其数据为空、大小为零，仍必须调用 `xrtFileUnmap`。映射创建后可以关闭原文件句柄，但调用方不得在映射存续期间截断或以其他方式缩小底层文件；这样做可能使后续访问产生系统异常。

映射页面不经过 XRT 分配器，返回地址只借用到解除映射。多个线程可访问同一映射，但数据竞争由调用方同步。`xrtFileMapFlush` 只接受共享写映射，大小为零表示刷新到映射末端；它把脏页提交给操作系统，不等价于断电持久化。需要稳定存储时，在刷新后继续对仍打开的文件调用 `xrtFlush`。私有映射不能刷新到文件。

### `xrtFileMap`

映射文件区间；`iSize` 为零表示映射到当前文件末端。

```c
xfilemap xrtFileMap(xfile File, uint64 iOffset,
	size_t iSize, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `iOffset` | 输入 | — | 映射起点 |
| `iSize` | 输入 | 零 = 到末端 | 映射长度 |
| `iFlags` | 输入 | `XFILE_MAP_READ/WRITE` | 访问模式 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 映射对象 | — |
| `NULL` | 系统错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/map · 只读映射](../../examples/file/map/main.c) · 观察

```c
	Map = xrtFileMap(File, 0u, 0u, XFILE_MAP_READ);
```


### `xrtFileMapData`

返回借用的映射数据；空映射返回空指针。

```c
ptr xrtFileMapData(xfilemap Map);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Map` | 输入 | 非空 | 映射对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 映射首字节（借用，随 Unmap 失效） | — |
| `NULL` | 空映射或参数非法 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file/map · 只读映射](../../examples/file/map/main.c) · 观察

```c
		printf("%.*s\n", (int)xrtFileMapSize(Map),
			(const char*)xrtFileMapData(Map));
```


### `xrtFileMapSize`

返回调用方可访问的映射字节数。

```c
size_t xrtFileMapSize(xfilemap Map);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Map` | 输入 | 非空 | 映射对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 可访问字节数 | — |
| `0` | 空映射或参数非法 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file/map · 只读映射](../../examples/file/map/main.c) · 观察

```c
		printf("%.*s\n", (int)xrtFileMapSize(Map),
```


### `xrtFileMapFlush`

把共享写映射的指定区间提交给操作系统。

```c
bool xrtFileMapFlush(xfilemap Map,
	size_t iOffset, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Map` | 输入 | 非空 | 共享写映射 |
| `iOffset` | 输入 | — | 区间起点 |
| `iSize` | 输入 | 零 = 全部 | 区间长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已提交 | — |
| `false` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 映射提交](../../examples/file/io_tour/main.c) · 观察

```c
	printf(" map-flush=%d\n", xrtFileMapFlush(Map, 0u, 0u) ? 1 : 0);
```


### `xrtFileUnmap`

解除映射并销毁映射对象。

```c
bool xrtFileUnmap(xfilemap Map);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Map` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解除并释放 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/map · 收尾](../../examples/file/map/main.c) · 观察

```c
		if ( xrtFileUnmap(Map) && xrtClose(File) &&
```


## 元数据

```c
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
```

只有 `Available` 中对应位有效的字段才能读取。时间统一为 Unix Epoch 微秒；原生时间精度高于微秒时向负无穷方向取整，因此纪元前不足一微秒的时间表示为 `-1`，不会错误折叠成 `0`。`Device` 与 `Identity` 只用于同一运行平台内判断对象身份，不是可持久化 ID。`Attributes` 保存 Windows 原生属性；`Mode` 保存 POSIX 模式。平台不提供的创建时间、变更时间或链接数不会用零伪装成有效值。

```c
bool xrtPathStat(cstr sPath, bool bFollowLink, xfileinfo* pInfo);
bool xrtFileStat(xfile File, xfileinfo* pInfo);
bool xrtPathSetTimes(cstr sPath, bool bFollowLink,
	const xtime* pAccessed, const xtime* pModified);
bool xrtPathSetMode(cstr sPath, bool bFollowLink, uint32 iMode);
bool xrtPathSetAttributes(cstr sPath, uint32 iAttributes);
bool xrtPathExists(cstr sPath);
bool xrtFileExists(cstr sPath);
bool xrtDirExists(cstr sPath);
```

`xrtPathStat` 通过 `bFollowLink` 明确选择末级链接或目标对象；成功才修改 `*pInfo`。`xrtFileStat` 查询打开对象。时间设置至少提供一个时间指针，空指针表示保留该项。

`xrtPathSetMode` 只在 POSIX 提供，Windows 返回 `XERR_UNSUPPORTED`。`xrtPathSetAttributes` 只在 Windows 提供，POSIX 返回 `XERR_UNSUPPORTED`。谓词把“查询失败”压缩成 `false`；需要区分不存在、权限失败和类型不符时必须使用 `xrtPathStat`。

### `xrtFileFlags`

返回打开文件经过验证的标志；失败返回 0。

```c
uint32 xrtFileFlags(xfile File);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XFILE_*` 组合 | 打开时的标志 | — |
| `0` | 对象非法 | 系统错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 自省](../../examples/file/io_tour/main.c) · 观察

```c
	iFlags = xrtFileFlags(File);
```


### `xrtFileNative`

返回 HANDLE 或文件描述符的整数表示；失败返回 -1。调用方不得关闭它。

```c
intptr_t xrtFileNative(xfile File);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 平台原生句柄（借用，随 Close 失效） | — |
| `-1` | 对象非法 | 系统错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 自省](../../examples/file/io_tour/main.c) · 观察

```c
		intptr_t Native = xrtFileNative(File);
```


### `xrtFileStat`

查询打开文件的元数据。

```c
bool xrtFileStat(xfile File, xfileinfo* pInfo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空 | 文件对象 |
| `pInfo` | 输出 | 非空 | 接收大小/时间/属性快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · 元数据](../../examples/file/io_tour/main.c) · 观察

```c
	if ( !xrtFileStat(File, &Info) || (Info.Size != 8u) ) {
```


### `xrtFileExists`

判断路径是否存在且为普通文件。

```c
bool xrtFileExists(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 存在普通文件 | — |
| `false` | 不存在或查询失败（区分二者用 `xrtPathStat`） | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/whole · 清理](../../examples/file/whole/main.c) · 观察

```c
	if ( xrtFileExists(sSource) && !xrtFileDelete(sSource) ) {
```


### `xrtFileTouch`

创建空文件；已存在文件刷新时间戳。

```c
bool xrtFileTouch(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已创建或时间戳已刷新 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/io_tour · Touch](../../examples/file/io_tour/main.c) · 观察

```c
	(void)xrtFileTouch(sPath);
```


### `xrtFileDelete`

删除普通文件或空目录占位的文件路径。

```c
bool xrtFileDelete(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已删除 | — |
| `false` | 不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/basic · 清理](../../examples/file/basic/main.c) · 观察

```c
	return xrtFileDelete("xrt-file-example.tmp") ? 0 : 1;
```


### `xrtPathRename`

重命名路径；`bReplace` 控制目标已存在时是否替换。

```c
bool xrtPathRename(cstr sSource, cstr sTarget, bool bReplace);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sSource` | 输入 | 非空、UTF-8 | 源路径 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换已存在目标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已重命名 | — |
| `false` | 目标存在且不替换，或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/link_tour · 重命名](../../examples/file/link_tour/main.c) · 观察

```c
	if ( !xrtPathRename(sOrigin, sRenamed, false) ) {
```


### `xrtFileCopy`

流式复制普通文件；不跟随末级链接，`bReplace` 控制是否替换普通文件目标。

```c
bool xrtFileCopy(cstr sSource, cstr sTarget, bool bReplace);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sSource` | 输入 | 非空 | 源文件 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换已存在的普通文件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已复制 | — |
| `false` | 目标存在且不替换，或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/whole · 复制移动](../../examples/file/whole/main.c) · 观察

```c
		 !xrtFileCopy(sSource, sCopy, false) ||
```


### `xrtFileMove`

移动普通文件；优先改名，跨卷时复制成功后删除源文件。

```c
bool xrtFileMove(cstr sSource, cstr sTarget, bool bReplace);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sSource` | 输入 | 非空 | 源文件 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换已存在的普通文件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已移动 | — |
| `false` | 目标存在且不替换，或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/whole · 复制移动](../../examples/file/whole/main.c) · 观察

```c
		 !xrtFileMove(sCopy, sMoved, false) ) {
```


### `xrtPathStat`

查询路径元数据；`bFollowLink` 决定是否跟随末级符号链接。

```c
bool xrtPathStat(cstr sPath, bool bFollowLink, xfileinfo* pInfo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |
| `bFollowLink` | 输入 | — | 跟随末级符号链接 |
| `pInfo` | 输出 | 非空 | 接收元数据快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 不存在或系统错误 | 区分二者靠错误码 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_NOT_FOUND` — 路径不存在

#### 范例

[file/link · 硬链接](../../examples/file/link/main.c) · 观察

```c
		 !xrtPathStat(sLink, true, &Info) ) {
```


### `xrtPathExists`

判断任意路径是否存在（文件/目录/链接均算存在）。

```c
bool xrtPathExists(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 存在某种对象 | — |
| `false` | 不存在或查询失败（区分用 `xrtPathStat`） | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/link_tour · 存在性](../../examples/file/link_tour/main.c) · 观察

```c
	printf("exists=%d", xrtPathExists(sOrigin) ? 1 : 0);
```


### `xrtPathSetTimes`

设置访问和修改时间；空指针表示保留对应时间，至少设置一项。

```c
bool xrtPathSetTimes(cstr sPath, bool bFollowLink,
	const xtime* pAccessed, const xtime* pModified);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `bFollowLink` | 输入 | — | 跟随末级链接 |
| `pAccessed` | 输入 | 可空 | 新访问时间 |
| `pModified` | 输入 | 可空 | 新修改时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 时间已更新 | — |
| `false` | 两项全空或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/link_tour · 时间戳](../../examples/file/link_tour/main.c) · 观察

```c
	printf("times=%d", xrtPathSetTimes(sRenamed, true, &iNow, &iNow) ? 1 : 0);
```


### `xrtPathSetMode`

设置 POSIX 权限模式；Windows 明确返回不支持。

```c
bool xrtPathSetMode(cstr sPath, bool bFollowLink, uint32 iMode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `bFollowLink` | 输入 | — | 跟随末级链接 |
| `iMode` | 输入 | 八进制位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 模式已设置 | — |
| `false` | Windows 上必然失败 | `XERR_UNSUPPORTED`（Windows） |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_UNSUPPORTED` — Windows 平台

#### 范例

[file/link_tour · 平台差异](../../examples/file/link_tour/main.c) · 观察

```c
	if ( xrtPathSetMode(sRenamed, true, 0600u) ) {
```


### `xrtPathSetAttributes`

设置 Windows 原生属性；POSIX 明确返回不支持。

```c
bool xrtPathSetAttributes(cstr sPath, uint32 iAttributes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `iAttributes` | 输入 | `FILE_ATTRIBUTE_*` | Windows 原生属性 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 属性已设置 | — |
| `false` | POSIX 上必然失败 | `XERR_UNSUPPORTED`（POSIX） |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_UNSUPPORTED` — POSIX 平台

#### 范例

[file/link_tour · 平台差异](../../examples/file/link_tour/main.c) · 观察

```c
	} else if ( xrtPathSetAttributes(sRenamed, 0u) ) {
```


## 基础路径操作

```c
bool xrtPathRename(cstr sSource, cstr sTarget, bool bReplace);
bool xrtFileDelete(cstr sPath);
bool xrtFileTouch(cstr sPath);
```

`xrtPathRename` 执行同卷改名，适用于文件、链接和目录。`bReplace=true` 使用系统原子替换。`bReplace=false` 要求目标不存在；Windows、Linux、macOS 和 FreeBSD 使用平台原子排他改名，其他 POSIX 平台只对可硬链接对象提供不覆盖回退，目录返回 `XERR_UNSUPPORTED`，绝不使用“先检查再替换”的竞态方案。跨卷返回系统错误，不隐式复制。

`xrtFileDelete` 删除非目录文件或链接自身，不递归。`xrtFileTouch` 排他地创建缺失空文件，或把已有文件的访问和修改时间更新为当前时刻，不截断内容。

## 临时文件和目录

```c
xfile xrtFileTemp(cstr sDirectory, cstr sPrefix,
	cstr sSuffix, str* pPath);
str xrtDirTemp(cstr sDirectory, cstr sPrefix, cstr sSuffix);
```

两项 API 都把安全随机命名和系统排他创建合成一个操作，不暴露“先生成不存在路径，再由调用方创建”的竞态窗口。每个候选名称包含 64 位安全随机值；只有系统明确报告名称已存在时才重试，其他权限、路径、内存或熵源错误立即原样返回。公开 API 的目录为空指针时使用系统临时目录，空字符串目录是参数错误。

前后缀为空指针时，临时文件默认使用 `.xrt-` 和 `.tmp`，临时目录默认使用 `.xrt-dir-` 和空后缀。显式空字符串表示不添加该片段。片段不能包含路径分隔符；Windows 还拒绝控制字符、保留名称字符、数据流分隔符以及会被系统折叠的末尾点或空格。它们只控制末级名称，不允许改变目标目录。

`xrtFileTemp` 成功返回读写文件，并通过 `pPath` 返回拥有路径；POSIX 以 `0600` 创建。调用方分别负责 `xrtClose`、`xrtFileDelete` 和 `xrtFree`。`xrtDirTemp` 成功返回拥有路径；POSIX 以 `0700` 创建，调用方负责 `xrtDirRemove` 和 `xrtFree`。Windows 文件和目录安全描述符继承父目录 ACL，因此应用仍应选择权限合适的父目录。两项 API 都不会自动删除对象，也不会把路径寿命绑定到句柄。

```c
str sPath = NULL;
xfile File = xrtFileTemp(NULL, "upload-", ".part", &sPath);

if ( File != NULL ) {
	/* 使用 File。 */
	(void)xrtClose(File);
	(void)xrtFileDelete(sPath);
	xrtFree(sPath);
}
```

### `xrtFileTemp`

排他创建临时文件并返回拥有路径；目录为空指针时使用系统临时目录。

```c
xfile xrtFileTemp(cstr sDirectory, cstr sPrefix,
	cstr sSuffix, str* pPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sDirectory` | 输入 | 可空 | 父目录；空 = 系统临时目录 |
| `sPrefix` | 输入 | 可空 | 文件名前缀 |
| `sSuffix` | 输入 | 可空 | 文件名后缀 |
| `pPath` | 输出 | 非空 | 接收拥有路径（`xrtFree` 释放） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 排他打开的临时文件 | — |
| `NULL` | 创建失败；`*pPath` 未定义 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_EXISTS` — 唯一名重试耗尽（罕见）

#### 范例

[file/temp · 临时文件](../../examples/file/temp/main.c) · 观察

```c
	xfile File = xrtFileTemp(NULL, "xrt-example-", ".tmp", &sPath);
```


### `xrtDirTemp`

排他创建临时目录并返回拥有路径；目录为空指针时使用系统临时目录。

```c
str xrtDirTemp(cstr sDirectory, cstr sPrefix, cstr sSuffix);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sDirectory` | 输入 | 可空 | 父目录；空 = 系统临时目录 |
| `sPrefix` | 输入 | 可空 | 目录名前缀 |
| `sSuffix` | 输入 | 可空 | 目录名后缀 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有路径（`xrtFree` 释放） | — |
| `NULL` | 创建失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_temp · 临时目录](../../examples/file/dir_temp/main.c) · 观察

```c
	str sPath = xrtDirTemp(NULL, "xrt-example-dir-", NULL);
```


## 整文件

```c
bytes xrtFileReadAll(cstr sPath, size_t* pSize);
bytes xrtFileReadAllLimit(cstr sPath,
	size_t iLimit, size_t* pSize);
bool xrtFileWriteAll(cstr sPath, xbytesview Data);
bool xrtFileAppend(cstr sPath, xbytesview Data);
bool xrtFileWriteAtomic(cstr sPath, xbytesview Data);
bool xrtFileCopy(cstr sSource, cstr sTarget, bool bReplace);
bool xrtFileMove(cstr sSource, cstr sTarget, bool bReplace);
```

`xrtFileReadAll` 按打开时取得的大小精确分配初始缓冲，并适应读取期间增长的文件；空文件只分配结尾字节，不再预留固定 4K。返回值是拥有缓冲，有效数据后总有一个额外零字节，但该字节不计入 `*pSize`，二进制内容仍必须按长度使用。失败时大小输出为零。

`xrtFileReadAllLimit` 在同一读取契约上增加硬字节上限。打开时已知大小超限会在分配前失败；读取期间增长越过上限也会在扩容前失败，错误码为 `XFILE_ERROR_LIMIT`。`iLimit=0` 只允许空文件。这是读取上传内容、配置或其他不可信文件时的首选入口。

`xrtFileWriteAll` 创建或截断并完整写入，失败可能留下部分目标。`xrtFileAppend` 使用系统追加打开语义。`xrtFileWriteAtomic` 在目标同目录排他创建临时文件，完整写入并 `xrtFlush` 后原子替换目标；失败会清理临时文件。新文件按 `0666 + umask` 创建；替换已有 POSIX 普通文件时保留其模式，Windows 使用 `ReplaceFileW` 保留原目标可合并的 ACL、属性和文件系统元数据。链接、目录和特殊对象不是原子写目标。它保证普通读者不会看到半份新内容，但不承诺所有 POSIX ACL、扩展属性或父目录项已经完成断电持久化。

`xrtFileCopy` 和 `xrtFileMove` 都只接受末级路径本身为普通文件的源与目标，不跟随末级符号链接或 Windows 重解析点；移动目录、链接等文件系统条目应使用 `xrtPathRename`，跨卷链接和目录使用对应模块构建策略。复制使用固定大小栈缓冲流式写入目标同目录临时文件，完成后一次发布，不会按源大小分配复制缓冲。新目标按 `0666 + umask` 创建，不再继承内部私有临时文件的 `0600`，但默认不复制时间、POSIX 模式、ACL 或扩展元数据。`xrtFileMove` 优先改名，只在系统明确报告跨卷时复制成功后删除源文件。删除源失败时目标已经完整存在，错误会明确保留该状态。

```c
size_t iSize;
bytes pData = xrtFileReadAll("input.bin", &iSize);

if ( pData != NULL ) {
	(void)xrtFileWriteAtomic("output.bin",
		(xbytesview){ pData, iSize });
	xrtFree(pData);
}
```

### `xrtFileReadAll`

读取完整文件；结果总有一个额外零字节，空文件也返回可释放缓冲。

```c
bytes xrtFileReadAll(cstr sPath, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |
| `pSize` | 输出 | 可空 | 接收字节数（不含零哨兵） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 完整内容 + 零哨兵，`xrtFree` 释放 | — |
| `NULL` | 系统错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_MEMORY` — 分配失败

#### 范例

[file/whole · 整读](../../examples/file/whole/main.c) · 观察

```c
	pData = xrtFileReadAll(sMoved, &iSize);
	if ( pData == NULL ) {
		goto cleanup;
	}
```


### `xrtFileReadAllLimit`

在硬上限内读取完整文件；增长越过上限时失败。

```c
bytes xrtFileReadAllLimit(cstr sPath,
	size_t iLimit, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `iLimit` | 输入 | — | 源文件字节硬上限 |
| `pSize` | 输出 | 可空 | 接收字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 上限内的完整内容 + 零哨兵 | — |
| `NULL` | 超上限或失败 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_RANGE` — 文件越过上限
- `XERR_MEMORY` — 分配失败

#### 范例

[file/io_tour · 带上限整读](../../examples/file/io_tour/main.c) · 观察

```c
bytes pAll = xrtFileReadAllLimit(sPath, 16u, &iDone);
```


### `xrtFileWriteAll`

创建或截断文件并完整写入全部字节。

```c
bool xrtFileWriteAll(cstr sPath, xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `Data` | 输入 | 借用 | 完整内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已完整写入 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/whole · 整写](../../examples/file/whole/main.c) · 观察

```c
	if ( !xrtFileWriteAll(sSource, XRT_BYTES_LITERAL("first")) ||
```


### `xrtFileAppend`

使用操作系统追加语义完整写入全部字节。

```c
bool xrtFileAppend(cstr sPath, xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径（不存在则创建） |
| `Data` | 输入 | 借用 | 追加内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已追加 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/whole · 追加](../../examples/file/whole/main.c) · 观察

```c
		 !xrtFileAppend(sSource, XRT_BYTES_LITERAL(" second")) ||
```


### `xrtFileWriteAtomic`

在同目录完整写入排他临时文件，再原子替换目标；读者永远见不到半个文件。

```c
bool xrtFileWriteAtomic(cstr sPath, xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目标路径 |
| `Data` | 输入 | 借用 | 完整内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已原子发布 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/whole · 原子替换](../../examples/file/whole/main.c) · 观察

```c
		 !xrtFileWriteAtomic(sSource, XRT_BYTES_LITERAL("published")) ||
```


## 文本文件

```c
str xrtFileReadText(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t* pSize);
str xrtFileReadTextLimit(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t iLimit, size_t* pSize);
bool xrtFileWriteText(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom);
bool xrtFileWriteTextAtomic(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom);
```

文本 API 的内存表示固定为 UTF-8，不把编码状态塞进文件句柄。读取 `XENCODING_UNKNOWN` 时先识别 BOM，再用严格检测选择 UTF-8、UTF-16 或 UTF-32；无法可靠确定时失败，不按本地代码页猜测。显式编码允许有或没有匹配 BOM，BOM 不进入返回文本。

`xrtFileReadTextLimit` 的 `iLimit` 限制源文件字节数，文件增长越过上限也会失败；`xrtFileReadText` 是不施加业务上限的便捷入口。`Policy` 使用字符集模块的严格、替换等策略。返回字符串由调用方释放，`pSize` 是 UTF-8 字节数。

合法 UTF-8 文件直接复用整文件读取所得的拥有缓冲，BOM 在原位移除；写入 UTF-8 且不要求 BOM 时直接使用调用方借用视图。其他编码和非法 UTF-8 的替换策略才进入转码器。`bWriteBom` 控制是否输出对应 BOM。普通写可能留下部分文件，原子版本复用 `xrtFileWriteAtomic`。

流式字符、行和格式化读写不进入 `xfile`。后续缓冲流和文本读取器会在同一二进制原语上提供这些能力，避免文件句柄同时维护隐藏编码、缓冲和游标状态。

### `xrtFileReadText`

读取并转换为 UTF-8；`UNKNOWN` 根据 BOM 和严格检测选择 Unicode 编码。

```c
str xrtFileReadText(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `Encoding` | 输入 | — | 源编码；`XENCODING_UNKNOWN` 自动探测 |
| `Policy` | 输入 | — | 严格或替换 |
| `pSize` | 输出 | 可空 | 接收 UTF-8 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | UTF-8 文本 + 零结尾，`xrtFree` 释放 | — |
| `NULL` | 编码错误或失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `xrt.charset` 域错误 — 非法序列（严格模式）
- `XERR_MEMORY`

#### 范例

[file/text · 读回](../../examples/file/text/main.c) · 观察

```c
	sText = xrtFileReadText(sPath, XENCODING_UNKNOWN, XUTF_STRICT, &iSize);
```


### `xrtFileReadTextLimit`

在源文件字节硬上限内读取并转换为 UTF-8。

```c
str xrtFileReadTextLimit(cstr sPath, xencoding Encoding,
	xutfpolicy Policy, size_t iLimit, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `Encoding` | 输入 | — | 源编码 |
| `Policy` | 输入 | — | 错误策略 |
| `iLimit` | 输入 | — | 源文件字节硬上限 |
| `pSize` | 输出 | 可空 | UTF-8 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 上限内的 UTF-8 文本 | — |
| `NULL` | 超上限或失败 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_RANGE` — 源文件越过上限
- `xrt.charset` 域错误 — 严格模式非法序列

#### 范例

[file/report · 限额读回](../../examples/file/report/main.c) · 观察

```c
	sReadBack = xrtFileReadTextLimit(
		sFilePath, XENCODING_UTF8, XUTF_STRICT, 4096u, &iReadSize
	);
```


### `xrtFileWriteText`

把 UTF-8 文本转换为目标编码后完整写入。

```c
bool xrtFileWriteText(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |
| `Text` | 输入 | 借用、UTF-8 | 文本 |
| `Encoding` | 输入 | — | 目标编码 |
| `Policy` | 输入 | — | 错误策略 |
| `bWriteBom` | 输入 | — | 是否写 BOM |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已转换并写入 | — |
| `false` | 编码错误或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `xrt.charset` 域错误 — UTF-8 源非法或目标不可表示

#### 范例

[file/text · 按编码写](../../examples/file/text/main.c) · 观察

```c
	if ( !xrtFileWriteText(sPath, XRT_STR_LITERAL("Hello, XRT"),
```


### `xrtFileWriteTextAtomic`

把 UTF-8 文本转换后原子替换目标。

```c
bool xrtFileWriteTextAtomic(cstr sPath, xstrview Text,
	xencoding Encoding, xutfpolicy Policy, bool bWriteBom);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目标路径 |
| `Text` | 输入 | 借用、UTF-8 | 文本 |
| `Encoding` | 输入 | — | 目标编码 |
| `Policy` | 输入 | — | 错误策略 |
| `bWriteBom` | 输入 | — | 是否写 BOM |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已原子发布 | — |
| `false` | 编码错误或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `xrt.charset` 域错误

#### 范例

[file/report · 原子发布](../../examples/file/report/main.c) · 观察

```c
		 !xrtFileWriteTextAtomic(
			sFilePath,
			(xstrview){ arrReport, (size_t)iReportSize },
			XENCODING_UTF8,
			XUTF_STRICT,
			false
		 ) ) {
```


## 目录枚举

```c
typedef struct xdir_impl* xdir;

typedef enum xdirflag {
	XDIR_STAT = 0x01,
	XDIR_FOLLOW_LINKS = 0x02,
	XDIR_INCLUDE_DOTS = 0x04
} xdirflag;

typedef enum xdirnext {
	XDIR_NEXT_ERROR = -1,
	XDIR_NEXT_END = 0,
	XDIR_NEXT_ITEM = 1
} xdirnext;

typedef enum xdirentryflag {
	XDIR_ENTRY_UTF8 = 0x01
} xdirentryflag;

typedef struct xdirentry {
	xstrview Name;
	xfileinfo Info;
	uint32 Flags;
} xdirentry;
```

`XDIR_STAT` 为每项补全元数据，可能增加一次系统查询；零标志优先使用枚举器已有信息。`XDIR_FOLLOW_LINKS` 只在补充元数据时跟随末级链接，并且必须与 `XDIR_STAT` 同时使用。目录迭代器的根路径本身会跟随末级链接。默认跳过 `.` 和 `..`，`XDIR_INCLUDE_DOTS` 可显式包含。

`Name`、名称后的零字节和条目元数据都借用迭代器，只在下一次 `xrtDirNext` 或关闭前有效。Windows 复用每个迭代器按需增长的 UTF-8 名称缓冲，不再为每个条目分配一次；POSIX 直接借用 `readdir` 名称。POSIX 原始文件名可能不是 UTF-8，只有 `XDIR_ENTRY_UTF8` 置位时才能交给 Unicode 文本 API。

```c
xdir xrtDirOpen(cstr sPath, uint32 iFlags);
xdirnext xrtDirNext(xdir Dir, xdirentry* pEntry);
bool xrtDirClose(xdir Dir);
cstr xrtDirPath(xdir Dir);
str xrtDirEntryPath(xdir Dir, const xdirentry* pEntry);
```

`xrtDirNext` 使用三态结果明确区分条目、正常结束和错误；失败不修改输出，发生错误后的迭代器只能关闭。迭代器不是并发对象，不允许多个线程同时读取或关闭；重新开始枚举应关闭后再次打开。`xrtDirPath` 返回迭代器借用的根路径。`xrtDirEntryPath` 拼接并返回拥有路径。

```c
xdir Dir = xrtDirOpen("assets", 0u);
xdirentry Entry;

while ( (Dir != NULL) &&
	(xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM) ) {
	str sPath = xrtDirEntryPath(Dir, &Entry);
	xrtFree(sPath);
}
if ( Dir != NULL ) {
	(void)xrtDirClose(Dir);
}
```

### `xrtDirOpen`

打开目录迭代器；根路径跟随末级链接，零标志提供无额外 stat 的枚举。

```c
xdir xrtDirOpen(cstr sPath, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 目录路径 |
| `iFlags` | 输入 | 零或 `XDIR_*` | 枚举选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 目录迭代器 | — |
| `NULL` | 打开失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/directory · 枚举](../../examples/file/directory/main.c) · 观察

```c
	xdir Dir = xrtDirOpen(".", 0u);
```


### `xrtDirNext`

读取下一条目录项；失败时不修改输出。

```c
xdirnext xrtDirNext(xdir Dir, xdirentry* pEntry);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Dir` | 输入 | 非空 | 迭代器 |
| `pEntry` | 输出 | 非空 | 接收条目（名称借用到下一次调用） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XDIR_NEXT_ITEM` | 取得一条 | — |
| `XDIR_NEXT_END` | 枚举完毕（正常结果） | 不设置错误 |
| `XDIR_NEXT_ERROR` | 系统错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/directory · 枚举](../../examples/file/directory/main.c) · 观察

```c
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
```


### `xrtDirClose`

关闭并销毁目录迭代器。

```c
bool xrtDirClose(xdir Dir);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Dir` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已关闭 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/directory · 枚举](../../examples/file/directory/main.c) · 观察

```c
	xrtDirClose(Dir);
```


### `xrtDirPath`

返回迭代器借用的目录路径。

```c
cstr xrtDirPath(xdir Dir);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Dir` | 输入 | 非空 | 迭代器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `cstr` | 借用路径（存活到 Close） | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file/dir_tour · 枚举辅助](../../examples/file/dir_tour/main.c) · 观察

```c
		printf("dir=%s", xrtDirPath(Dir));
```


### `xrtDirEntryPath`

把迭代器目录与条目名称拼成拥有路径。

```c
str xrtDirEntryPath(xdir Dir, const xdirentry* pEntry);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Dir` | 输入 | 非空 | 迭代器 |
| `pEntry` | 输入 | 非空 | 目录条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 拥有路径（`xrtFree` 释放） | — |
| `NULL` | 参数或系统错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 枚举辅助](../../examples/file/dir_tour/main.c) · 观察

```c
			str sFull = xrtDirEntryPath(Dir, &Entry);
```


## 目录基础操作

```c
typedef struct xdirroots {
	str* Items;
	size_t Count;
} xdirroots;

bool xrtDirCreate(cstr sPath);
bool xrtDirCreateMode(cstr sPath, uint32 iMode);
bool xrtDirCreateAll(cstr sPath);
bool xrtDirCreateAllMode(cstr sPath, uint32 iMode);
bool xrtDirRemove(cstr sPath);
bool xrtDirEmpty(cstr sPath, bool* pEmpty);
bool xrtDirRoots(xdirroots* pRoots);
void xrtDirRootsFree(xdirroots* pRoots);
```

单级创建要求目标不存在；递归创建允许已经存在的目录，但拒绝中途出现非目录对象。默认模式为 `0777` 并受 POSIX `umask` 影响，已有目录的模式不会被修改。递归创建不是事务：后续组件失败时，已经创建的前缀目录会保留；它也会经过已有的中间链接，处理不可信相对路径应使用目录根能力。`xrtDirRemove` 只删除空目录。`xrtDirEmpty` 成功才修改输出。

`xrtDirRoots` 在 Windows 返回当前可枚举驱动器根，在 POSIX 返回 `/`。成功前不修改输出；调用方应传入未持有旧列表的结构。结构、数组和每个字符串都由调用方通过 `xrtDirRootsFree` 一次释放；释放函数可接受零初始化结构并在完成后清零。

### `xrtDirExists`

判断路径是否存在且为目录。

```c
bool xrtDirExists(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 存在目录 | — |
| `false` | 非目录或查询失败 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/tree · 前置清理](../../examples/file/tree/main.c) · 观察

```c
	if ( xrtDirExists(sSource) ) {
```


### `xrtDirCreate`

使用平台默认模式创建一个目录。

```c
bool xrtDirCreate(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目标路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已创建 | — |
| `false` | 已存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_EXISTS` — 已存在

#### 范例

[file/tree · 浅创建](../../examples/file/tree/main.c) · 观察

```c
	if ( !xrtDirCreate(sSource) ) {
```


### `xrtDirCreateMode`

使用显式 POSIX 模式创建一个目录；Windows 接受但忽略模式。

```c
bool xrtDirCreateMode(cstr sPath, uint32 iMode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目标路径 |
| `iMode` | 输入 | 八进制位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已创建 | — |
| `false` | 已存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_EXISTS`

#### 范例

[file/dir_tour · 创建](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirCreateMode("xrt-dir-tour/single", 0700u) ) {
```


### `xrtDirCreateAll`

使用平台默认模式递归创建全部缺失目录。

```c
bool xrtDirCreateAll(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 多级路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 全链已存在或已创建 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 递归建链](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirCreateAll(sDeep) ) {
```


### `xrtDirCreateAllMode`

使用显式 POSIX 模式递归创建全部缺失目录；失败可能保留已创建前缀。

```c
bool xrtDirCreateAllMode(cstr sPath, uint32 iMode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 多级路径 |
| `iMode` | 输入 | 八进制位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 全链就绪 | — |
| `false` | 系统错误（已建前缀保留） | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 递归建链](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirCreateAllMode("xrt-dir-tour/mode/a/b", 0700u) ) {
```


### `xrtDirRemove`

删除一个空目录，不递归删除其内容。

```c
bool xrtDirRemove(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目标目录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已删除 | — |
| `false` | 非空/不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_STATE` — 目录非空（如实现设置）

#### 范例

[file/dir_temp · 收尾](../../examples/file/dir_temp/main.c) · 观察

```c
	bResult = xrtDirRemove(sPath);
```


### `xrtDirRemoveAll`

递归删除整棵目录树（含根目录）。

```c
bool xrtDirRemoveAll(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 树根路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 整树已删除 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/tree · 前置清理](../../examples/file/tree/main.c) · 观察

```c
		(void)xrtDirRemoveAll(sSource);
```


### `xrtDirEmpty`

查询目录是否为空；失败时不修改输出。

```c
bool xrtDirEmpty(cstr sPath, bool* pEmpty);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目录路径 |
| `pEmpty` | 输出 | 非空 | 接收空判定 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 判定已写出 | — |
| `false` | 不存在或系统错误 | `*pEmpty` 不变 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 观测](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirEmpty("xrt-dir-tour/single", &bEmpty) || !bEmpty ) {
```


### `xrtDirEnsureEmpty`

确保目录存在且为空：不存在则创建，存在则清空内容但保留目录。

```c
bool xrtDirEnsureEmpty(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 目录存在且为空 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 观测](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirEnsureEmpty("xrt-dir-tour/fresh") ) {
```


### `xrtDirClean`

删除目录全部内容但保留目录自身。

```c
bool xrtDirClean(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 内容已清空 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 清内容留目录](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirClean(sDeep) ) {
```


### `xrtDirMove`

移动目录；`bReplace` 控制目标已存在目录的替换语义。

```c
bool xrtDirMove(cstr sSource, cstr sTarget, bool bReplace);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sSource` | 输入 | 非空 | 源目录 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换已存在目标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已移动 | — |
| `false` | 目标存在且不替换，或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 移动](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirMove("xrt-dir-tour/single", "xrt-dir-tour/renamed", false) ) {
```


### `xrtDirCopy`

递归复制目录树；`bReplace` 控制目标冲突。

```c
bool xrtDirCopy(cstr sSource, cstr sTarget, bool bReplace);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sSource` | 输入 | 非空 | 源树 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换已存在对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 整树已复制 | — |
| `false` | 冲突或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/tree · 树复制](../../examples/file/tree/main.c) · 观察

```c
		xrtClose(File) && xrtDirCopy(sSource, sTarget, false) ) {
```


### `xrtDirStats`

统计目录内容：文件数/目录数/字节数；`bRecursive` 控制是否深入子目录。

```c
bool xrtDirStats(cstr sPath, bool bRecursive, xwalkstats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目录路径 |
| `bRecursive` | 输入 | — | 深入子目录 |
| `pStats` | 输出 | 非空 | 接收统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 统计已写出 | — |
| `false` | 不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 观测](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirStats(sDeep, false, &Stats) || (Stats.Files != 1u) ) {
```


### `xrtDirSize`

统计目录总字节数；`bRecursive` 控制是否深入子目录。

```c
bool xrtDirSize(cstr sPath, bool bRecursive, uint64* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 目录路径 |
| `bRecursive` | 输入 | — | 深入子目录 |
| `pSize` | 输出 | 非空 | 接收总字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 总量已写出 | — |
| `false` | 不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 观测](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirSize(sDeep, true, &iSize) || (iSize != 7u) ) {
```


### `xrtDirRoots`

查询当前系统可枚举的文件系统根目录。

```c
bool xrtDirRoots(xdirroots* pRoots);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRoots` | 输出 | 非空 | 接收根列表（`Items`/`Count`） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 列表已写出（用后 `DirRootsFree`） | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 根列表](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtDirRoots(&Roots) || (Roots.Count < 1u) ) {
```


### `xrtDirRootsFree`

释放系统根目录列表并清零。

```c
void xrtDirRootsFree(xdirroots* pRoots);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRoots` | 输入 | 允许空 | `DirRoots` 产物 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 列表已释放 | — |

#### 错误

- 无 — 释放不失败

#### 范例

[file/dir_tour · 根列表](../../examples/file/dir_tour/main.c) · 观察

```c
	xrtDirRootsFree(&Roots);
```


## 深度优先遍历

```c
typedef enum xwalkflag {
	XWALK_FOLLOW_LINKS = 0x01,
	XWALK_ONE_FILESYSTEM = 0x02
} xwalkflag;

typedef enum xwalkevent {
	XWALK_ENTER = 1,
	XWALK_ITEM,
	XWALK_LEAVE
} xwalkevent;

typedef enum xwalkentryflag {
	XWALK_ENTRY_UTF8 = 0x01,
	XWALK_ENTRY_LINK = 0x02,
	XWALK_ENTRY_CYCLE = 0x04,
	XWALK_ENTRY_CROSS_FILESYSTEM = 0x08
} xwalkentryflag;

typedef enum xwalkcontrol {
	XWALK_CONTINUE = 0,
	XWALK_SKIP,
	XWALK_STOP,
	XWALK_ERROR
} xwalkcontrol;

typedef enum xwalkerroraction {
	XWALK_ERROR_ABORT = 0,
	XWALK_ERROR_SKIP,
	XWALK_ERROR_STOP
} xwalkerroraction;

typedef xwalkerroraction (*xwalkerrorproc)(cstr sPath,
	const xerror* pError, ptr pUserData);

typedef struct xwalkoptions {
	uint32 Flags;
	size_t MaxDepth;
	xwalkerrorproc OnError;
} xwalkoptions;

typedef struct xwalkentry {
	cstr Path;
	xstrview Parent;
	xstrview Name;
	xfileinfo Info;
	xwalkevent Event;
	uint32 Flags;
	size_t Depth;
} xwalkentry;

typedef struct xwalkstats {
	uint64 Items;
	uint64 Files;
	uint64 Directories;
	uint64 Links;
	uint64 Others;
	uint64 Bytes;
	bool Stopped;
} xwalkstats;

typedef xwalkcontrol (*xwalkproc)(const xwalkentry* pEntry, ptr pUserData);
```

每个目录产生 `ENTER` 和 `LEAVE`，文件、未跟随链接和特殊对象产生 `ITEM`。目录只在 `ENTER` 时计入统计。`Path` 是完整路径，`Parent` 和 `Name` 是其中借用的父目录与末级名称视图，三者只在当前回调期间有效；跟随链接时 `Info` 描述有效目标，`XWALK_ENTRY_LINK` 保留物理入口原本是链接这一事实。

`XWALK_SKIP` 只允许从 `ENTER` 返回，跳过当前目录内容但仍产生 `LEAVE`。`STOP` 是成功早停并设置 `Stats.Stopped`；此时尚未产生的 `LEAVE` 不再补发。`ERROR` 要求回调先设置结构化错误；无错误失败会转换成遍历内部错误。无效控制值会被拒绝。

```c
void xrtWalkOptionsInit(xwalkoptions* pOptions);
bool xrtFileWalk(cstr sPath, const xwalkoptions* pOptions,
	xwalkproc pProc, ptr pUserData, xwalkstats* pStats);
```

默认不跟随链接、允许跨文件系统、深度无限并在首个系统错误处终止。`MaxDepth=0` 只访问根。回调为空时只统计。成功才写入 `pStats`；统计溢出返回错误，不静默回绕。

`OnError` 接收发生错误的准确路径和借用错误。`XWALK_ERROR_ABORT` 终止并保留原错误；回调也可以先设置另一结构化错误来替换它。`XWALK_ERROR_SKIP` 跳过失败条目，目录枚举失败时跳过该目录剩余内容；`XWALK_ERROR_STOP` 成功早停。跳过的未解析条目不计入统计。回调返回后路径和错误立即失效，错误动作非法会成为 `XWALK_ERROR_CALLBACK`。只有 I/O、缺失、类型变化、权限、暂时不可用和平台不支持等路径环境错误进入该回调；内存不足、溢出、参数、状态和内部错误始终终止，不能被误吞。

跟随链接时使用目录身份检测祖先环，环目录仍产生带 `CYCLE` 标志的 `ENTER/LEAVE`，但不会下降。`ONE_FILESYSTEM` 对跨设备目录做同样处理并设置 `CROSS_FILESYSTEM`。平台无法提供稳定目录身份时，这两种安全模式返回不支持，而不是在未知状态继续。遍历使用显式堆栈，不消耗与目录深度成正比的 C 调用栈；条目顺序保持系统枚举顺序，不为排序缓存整个目录。

### `xrtWalkOptionsInit`

初始化遍历选项：不跟随链接、允许跨文件系统、深度无限。

```c
void xrtWalkOptionsInit(xwalkoptions* pOptions);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输出 | 非空 | 接收默认选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化，不失败 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[file/walk · 限深遍历](../../examples/file/walk/main.c) · 观察

```c
	xrtWalkOptionsInit(&Options);
	Options.MaxDepth = 1u;
```


### `xrtFileWalk`

深度优先遍历一个文件系统对象；回调为空时只计算统计。

```c
bool xrtFileWalk(cstr sPath, const xwalkoptions* pOptions,
	xwalkproc pProc, ptr pUserData, xwalkstats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 遍历起点 |
| `pOptions` | 输入 | 允许空 | 空 = 默认选项 |
| `pProc` | 输入 | 可空 | 访问回调；返回假中止遍历 |
| `pUserData` | 输入 | 任意值 | 原样传给回调 |
| `pStats` | 输出 | 可空 | 接收统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 遍历完成（或被回调中止且算成功） | — |
| `false` | 起点不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_NOT_FOUND` — 起点不存在

#### 范例

[file/walk · 限深遍历](../../examples/file/walk/main.c) · 观察

```c
	if ( !xrtFileWalk(".", &Options, printEntry, NULL, &Stats) ) {
```


## 链接和 FIFO

```c
bool xrtLinkCreate(cstr sTarget, cstr sLink, bool bDirectory);
bool xrtLinkHard(cstr sExisting, cstr sLink);
str xrtLinkRead(cstr sLink);
bool xrtLinkDelete(cstr sLink);
bool xrtFifoCreate(cstr sPath, uint32 iMode);
```

符号链接目标可以不存在，并按传入文本存储；Windows 创建链接时必须通过 `bDirectory` 提供目标类别提示。`xrtLinkRead` 返回拥有的零结尾目标文本，不把相对目标改写成绝对路径。`xrtLinkDelete` 只删除链接自身。硬链接要求已有普通文件，链接关系受文件系统限制。

`xrtFifoCreate` 在 POSIX 调用 `mkfifo`，路径必须尚不存在，模式只允许低 12 位且仍受 `umask` 影响；Windows 明确返回 `XERR_UNSUPPORTED`。FIFO 的打开、阻塞和 IO 直接使用文件原语及系统语义，创建函数本身不打开 FIFO，也不隐藏可能阻塞的打开行为。

### `xrtLinkCreate`

创建符号链接；目标可以不存在，目录提示在 Windows 上是必需信息。

```c
bool xrtLinkCreate(cstr sTarget, cstr sLink, bool bDirectory);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sTarget` | 输入 | 非空 | 链接目标（按原文本保存） |
| `sLink` | 输入 | 非空 | 链接路径 |
| `bDirectory` | 输入 | — | 目标是否目录（Windows 必需） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 链接已创建 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/link_tour · 符号链接](../../examples/file/link_tour/main.c) · 观察

```c
	if ( !xrtLinkCreate(sRenamed, sLink, false) ) {
```


### `xrtLinkHard`

为已存在文件创建硬链接。

```c
bool xrtLinkHard(cstr sExisting, cstr sLink);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sExisting` | 输入 | 非空 | 已存在的普通文件 |
| `sLink` | 输入 | 非空 | 新链接路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 硬链接已创建 | — |
| `false` | 跨卷/目标存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/link · 硬链接](../../examples/file/link/main.c) · 观察

```c
		 !xrtClose(File) || !xrtLinkHard(sSource, sLink) ||
```


### `xrtLinkRead`

读取符号链接中存储的目标文本，返回拥有的零结尾路径字节。

```c
str xrtLinkRead(cstr sLink);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sLink` | 输入 | 非空 | 链接路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 目标文本（`xrtFree` 释放） | — |
| `NULL` | 非链接或系统错误 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_NOT_FOUND` — 链接不存在

#### 范例

[file/link_tour · 读目标](../../examples/file/link_tour/main.c) · 观察

```c
		str sTarget = xrtLinkRead(sLink);
```


### `xrtLinkDelete`

删除符号链接自身，不跟随也不删除目标。

```c
bool xrtLinkDelete(cstr sLink);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sLink` | 输入 | 非空 | 链接路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 链接已删除（目标不受影响） | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/link_tour · 删链接](../../examples/file/link_tour/main.c) · 观察

```c
	if ( !xrtLinkDelete(sLink) ) {
```


### `xrtFifoCreate`

创建不存在的 POSIX FIFO；模式只允许低 12 位且仍受 umask 影响，Windows 返回不支持。

```c
bool xrtFifoCreate(cstr sPath, uint32 iMode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | FIFO 路径 |
| `iMode` | 输入 | 低 12 位 | POSIX 权限（受 umask） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | FIFO 已创建 | — |
| `false` | Windows 必然失败 | `XERR_UNSUPPORTED`（Windows） |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_UNSUPPORTED` — Windows 平台
- `XERR_EXISTS` — 已存在

#### 范例

[file/fifo · 平台门控](../../examples/file/fifo/main.c) · 观察

```c
		if ( !xrtFifoCreate(sPath, 0600u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) ) {
```


## 目录能力

```c
typedef struct xroot_impl* xroot;

xroot xrtRootOpen(cstr sPath);
xroot xrtRootOpenIn(xroot Root, cstr sPath);
bool xrtRootClose(xroot Root);
cstr xrtRootPath(xroot Root);
intptr_t xrtRootNative(xroot Root);
```

`xroot` 持有已经打开的真实目录句柄。`xrtRootOpen` 允许初始根路径自身经过系统链接解析；成功后，后续操作都相对于已锚定句柄进行，不再用字符串拼接回到原路径。目录在外部被改名后，根对象仍指向同一个目录。`xrtRootOpenIn` 使用同一安全解析器创建更窄的子根；路径为 `.` 时仍返回拥有独立原生句柄的新对象。

`xrtRootPath` 返回创建对象时保存的只读诊断文本，生命周期借用到关闭，不参与安全判断，也不保证在目录改名后更新。`xrtRootNative` 返回借用的 Windows `HANDLE` 或 POSIX 文件描述符；所有权仍属于根对象。成功或失败的 `xrtRootClose` 都会销毁对象，不能重试或继续使用。普通根内操作可以从多个线程并发调用，但关闭、原生句柄操作和其他根操作不得并发。

```c
xfile xrtRootFileOpen(xroot Root, cstr sPath,
	const xfileoptions* pOptions);
bool xrtRootStat(xroot Root, cstr sPath,
	bool bFollowLink, xfileinfo* pInfo);
bool xrtRootDirCreate(xroot Root, cstr sPath, uint32 iMode);
bool xrtRootRemove(xroot Root, cstr sPath);
str xrtRootLinkRead(xroot Root, cstr sPath);
```

根内路径必须是非空相对 UTF-8 路径。重复分隔符和 `.` 会规范化；`a/../b` 可以回到根内上级，任何试图越过根的 `..` 都返回 `XERR_PERMISSION`。绝对路径、Windows 卷路径、UNC 路径、设备名、备用数据流、控制字符和不适合 Win32 文件名的末尾点或空格会在系统调用前拒绝。末尾分隔符保留“必须是目录”的约束，包括链接替换路径后。

中间目录始终以“不跟随链接”方式逐段打开。遇到符号链接时，解析器读取系统实际跟随的目标，把相对目标重新放入剩余路径，并从根句柄重新开始；绝对目标和越界目标拒绝。末级文件默认可以跟随安全的相对链接，`XFILE_NOFOLLOW` 禁止跟随。`CREATE | EXCLUSIVE` 遇到已有链接必须失败，不能通过链接创建或截断目标。链接解析最多八次、总解析步骤最多 255 次，循环或异常深度返回限制错误。

`xrtRootFileOpen` 与 `xrtFileOpen` 使用完全相同的选项和 `xfile` 所有权。`xrtRootStat` 的 `bFollowLink=false` 查询链接自身，`true` 只跟随仍在根内的目标，成功才修改输出。`xrtRootDirCreate` 只创建末级目录，不隐式创建父级；模式在 POSIX 受 `umask` 影响，Windows 接受但忽略。`xrtRootRemove` 删除非目录对象或空目录，绝不跟随末级链接，也拒绝删除根对象自身。`xrtRootLinkRead` 返回拥有的链接目标原文，调用方使用 `xrtFree` 释放。

```c
xroot Root = xrtRootOpen("public");
const char sData[] = "data";
xfileoptions Options;
xfile File;
xfileinfo Info;

xrtFileOptionsInit(&Options);
Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
File = xrtRootFileOpen(Root, "uploads/item.bin", &Options);

if ( File != NULL ) {
	(void)xrtWriteFull(File, sData, sizeof(sData) - 1u, NULL);
	(void)xrtClose(File);
}
if ( xrtRootStat(Root, "uploads/item.bin", true, &Info) ) {
	/* Info 描述锚定目录内的最终对象。 */
}
(void)xrtRootClose(Root);
```

Windows 使用根句柄加 `NtCreateFile` 的 `RootDirectory` 逐段解析，并拒绝未知重解析点；POSIX 使用 `openat`、`fstatat`、`mkdirat`、`unlinkat` 和 `readlinkat`，每个遍历步骤启用 `O_NOFOLLOW`。该契约防止普通路径和链接交换把操作带出根目录，但不承诺隔离拥有系统特权的对手，也不阻止管理员通过挂载、绑定挂载、卷管理或原生句柄直接改变可见文件系统。需要进程级安全边界时仍应结合操作系统沙箱、权限和独立身份。

### `xrtRootOpen`

打开并锚定一个真实目录；根路径自身允许包含链接。

```c
xroot xrtRootOpen(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 根目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 锚定的根对象 | — |
| `NULL` | 打开失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/root · 父根与子根](../../examples/file/root/main.c) · 观察

```c
	Parent = xrtRootOpen(".");
```


### `xrtRootOpenIn`

在已有根内打开并锚定一个子目录。

```c
xroot xrtRootOpenIn(xroot Root, cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 父根 |
| `sPath` | 输入 | 非空、根内相对 | 子目录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 子根（父根保持独立） | — |
| `NULL` | 越界或打开失败 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_ARGUMENT` — 相对路径越出根外

#### 范例

[file/root · 父根与子根](../../examples/file/root/main.c) · 观察

```c
	Root = xrtRootOpenIn(Parent, sDirectory);
```


### `xrtRootClose`

关闭原生目录句柄并销毁根对象；关闭不得与其他根操作并发。

```c
bool xrtRootClose(xroot Root);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已关闭并释放 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/root · 收尾](../../examples/file/root/main.c) · 观察

```c
		 !xrtRootClose(Root) ||
```


### `xrtRootPath`

返回创建根对象时保存的诊断路径，不参与任何安全判断。

```c
cstr xrtRootPath(xroot Root);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `cstr` | 借用诊断路径（存活到 Close） | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file/root_tour · 自省](../../examples/file/root_tour/main.c) · 观察

```c
		xrtRootPath(Root), xrtRootPath(Root),
```


### `xrtRootNative`

返回根目录 HANDLE 或文件描述符的整数表示，所有权仍属于根对象。

```c
intptr_t xrtRootNative(xroot Root);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `>= 0` | 原生句柄（借用） | — |
| `-1` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file/root_tour · 自省](../../examples/file/root_tour/main.c) · 观察

```c
		xrtRootNative(Root) != 0 ? 1 : 0);
```


### `xrtRootFileOpen`

在根内使用完整文件选项打开普通文件。

```c
xfile xrtRootFileOpen(xroot Root, cstr sPath,
	const xfileoptions* pOptions);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sPath` | 输入 | 非空、根内相对 | 文件路径 |
| `pOptions` | 输入 | 允许空 | 打开选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 打开的文件 | — |
| `NULL` | 越界或打开失败 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_ARGUMENT` — 越界路径

#### 范例

[file/root · 根内文件](../../examples/file/root/main.c) · 观察

```c
	File = xrtRootFileOpen(Root, sName, &Options);
```


### `xrtRootStat`

查询根内对象元数据；`bFollowLink` 决定是否解析末级链接。

```c
bool xrtRootStat(xroot Root, cstr sPath,
	bool bFollowLink, xfileinfo* pInfo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sPath` | 输入 | 非空、根内相对 | 对象路径 |
| `bFollowLink` | 输入 | — | 解析末级链接 |
| `pInfo` | 输出 | 非空 | 接收元数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 快照已写出 | — |
| `false` | 越界/不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_NOT_FOUND`

#### 范例

[file/root_tour · 元数据](../../examples/file/root_tour/main.c) · 观察

```c
	bOk = xrtRootStat(Root, "xrt-root-tour-target.tmp", true, &Info);
```


### `xrtRootDirCreate`

在根内创建一个目录；POSIX 使用显式模式，Windows 接受但忽略模式。

```c
bool xrtRootDirCreate(xroot Root, cstr sPath, uint32 iMode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sPath` | 输入 | 非空、根内相对 | 目录路径 |
| `iMode` | 输入 | 八进制位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已创建 | — |
| `false` | 越界/已存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_EXISTS`

#### 范例

[file/root · 父根与子根](../../examples/file/root/main.c) · 观察

```c
	if ( !xrtRootDirCreate(Parent, sDirectory, 0700u) ) {
```


### `xrtRootRemove`

删除根内一个非目录对象或空目录，不跟随末级链接。

```c
bool xrtRootRemove(xroot Root, cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sPath` | 输入 | 非空、根内相对 | 目标路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已删除 | — |
| `false` | 越界/不存在或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_NOT_FOUND`

#### 范例

[file/root · 收尾](../../examples/file/root/main.c) · 观察

```c
		 !xrtRootRemove(Root, sName) ||
```


### `xrtRootLinkRead`

读取根内末级符号链接或受支持重解析点保存的目标文本。

```c
str xrtRootLinkRead(xroot Root, cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sPath` | 输入 | 非空、根内相对 | 链接路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 目标文本（`xrtFree` 释放） | — |
| `NULL` | 非链接或失败 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_NOT_FOUND`

#### 范例

[file/root_tour · 链接族](../../examples/file/root_tour/main.c) · 观察

```c
		sTarget = xrtRootLinkRead(Root, "xrt-root-tour-link");
```


### `xrtRootLinkCreate`

在根内创建符号链接；链接目标按原文本保存，链接路径的父目录始终锚定。

```c
bool xrtRootLinkCreate(xroot Root, cstr sTarget,
	cstr sLink, bool bDirectory);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sTarget` | 输入 | 非空 | 目标文本（不经根解析） |
| `sLink` | 输入 | 非空、根内相对 | 链接路径 |
| `bDirectory` | 输入 | — | 目标目录提示（Windows） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 链接已创建 | — |
| `false` | 越界或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/root_tour · 链接族](../../examples/file/root_tour/main.c) · 观察

```c
	bOk = xrtRootLinkCreate(Root, "target.txt", "xrt-root-tour-link", false);
```


### `xrtRootLinkHard`

在同一根内为普通文件创建硬链接，源和目标路径都经过根解析。

```c
bool xrtRootLinkHard(xroot Root, cstr sExisting, cstr sLink);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sExisting` | 输入 | 非空、根内相对 | 已存在文件 |
| `sLink` | 输入 | 非空、根内相对 | 新链接 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 硬链接已创建 | — |
| `false` | 越界/跨卷或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/root_tour · 链接族](../../examples/file/root_tour/main.c) · 观察

```c
	bOk = xrtRootLinkHard(Root, "xrt-root-tour-target.tmp",
```


### `xrtRootFifoCreate`

在根内创建 POSIX FIFO；不支持 FIFO 的平台返回 `XERR_UNSUPPORTED`。

```c
bool xrtRootFifoCreate(xroot Root, cstr sPath, uint32 iMode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sPath` | 输入 | 非空、根内相对 | FIFO 路径 |
| `iMode` | 输入 | 低 12 位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | FIFO 已创建 | — |
| `false` | Windows 必然失败 | `XERR_UNSUPPORTED`（Windows） |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_UNSUPPORTED` — Windows

#### 范例

[file/root_tour · 平台门控](../../examples/file/root_tour/main.c) · 观察

```c
	bOk = xrtRootFifoCreate(Root, "xrt-root-tour-fifo", 0600u);
```


### `xrtRootSetMode`

在根内设置对象权限；跟随链接时仍由根解析器阻止越界。

```c
bool xrtRootSetMode(xroot Root, cstr sPath,
	bool bFollowLink, uint32 iMode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Root` | 输入 | 非空 | 根对象 |
| `sPath` | 输入 | 非空、根内相对 | 对象路径 |
| `bFollowLink` | 输入 | — | 解析末级链接 |
| `iMode` | 输入 | 八进制位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 模式已设置 | — |
| `false` | Windows 必然失败或越界 | `XERR_UNSUPPORTED`（Windows） |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_UNSUPPORTED` — Windows

#### 范例

[file/root_tour · 平台门控](../../examples/file/root_tour/main.c) · 观察

```c
	bOk = xrtRootSetMode(Root, "xrt-root-tour-target.tmp", true, 0600u);
```


## 目录树

```c
typedef enum xtreecopyflag {
	XTREE_COPY_MERGE = 0x01,
	XTREE_COPY_REPLACE = 0x02,
	XTREE_COPY_FOLLOW_LINKS = 0x04,
	XTREE_COPY_SKIP_LINKS = 0x08,
	XTREE_COPY_ONE_FILESYSTEM = 0x10,
	XTREE_COPY_SKIP_SPECIAL = 0x20,
	XTREE_COPY_METADATA = 0x40
} xtreecopyflag;

typedef struct xtreecopyoptions {
	uint32 Flags;
} xtreecopyoptions;
```

默认要求目标不存在、保留符号链接文本并拒绝特殊对象。`MERGE` 允许目标目录已经存在；`REPLACE` 只与 `MERGE` 一起使用，并替换冲突文件、目录或链接。未设置 `REPLACE` 时任何冲突立即失败。

`FOLLOW_LINKS` 与 `SKIP_LINKS` 互斥。跟随模式复制有效目标并使用遍历身份检测目录环；跳过模式不在目标创建对应条目。`ONE_FILESYSTEM` 不下降到其他设备。`SKIP_SPECIAL` 忽略 FIFO、socket 和设备等不能普通复制的对象。

`METADATA` 在复制内容后应用通用时间以及 POSIX 模式或部分 Windows 可写属性。它不保留 ACL、所有者、扩展属性、备用数据流、稀疏布局和硬链接关系。需要这些能力时，使用 `xrtFileWalk`、`xrtPathStat`、`xrtLinkHard` 与平台原生接口构建策略，不需要修改目录树核心。

```c
void xrtTreeCopyOptionsInit(xtreecopyoptions* pOptions);
bool xrtFileTreeCopy(cstr sSource, cstr sTarget,
	const xtreecopyoptions* pOptions, xwalkstats* pStats);
bool xrtDirCopy(cstr sSource, cstr sTarget, bool bReplace);
```

复制在创建目标前同时检查词法路径、对象身份和解析链接后的最近现存父级，拒绝目标等于源或物理上位于源目录内部。Windows 物理路径使用 UTF-16 序数、不区分大小写且按组件边界比较，不把 `\\?\` 设备路径错误地交给普通相对路径算法。源根本身是链接时，只有 `FOLLOW_LINKS` 才允许复制。

目标原本不存在时，复制在同一父目录排他创建私有兄弟暂存树，完整成功后用不替换改名一次性发布。并发创建最终目标的一方会使发布失败，但不会被清理逻辑误删；失败会尽力清理 XRT 自己的暂存树并保留原始错误，持续内存或系统故障仍可能留下名称以 `.xrt-tree-` 开头的私有暂存目录。合并到既有目录失败时不回滚已经成功发布的条目。只有复制和最终发布都成功才写入 `pStats`。

`xrtDirCopy` 是常用封装：`bReplace=false` 使用默认严格复制，`true` 使用 `MERGE | REPLACE`。

```c
bool xrtFileTreeRemove(cstr sPath, bool bKeepRoot, xwalkstats* pStats);
bool xrtDirRemoveAll(cstr sPath);
bool xrtDirClean(cstr sPath);
bool xrtDirMove(cstr sSource, cstr sTarget, bool bReplace);
bool xrtDirStats(cstr sPath, bool bRecursive, xwalkstats* pStats);
bool xrtDirSize(cstr sPath, bool bRecursive, uint64* pSize);
bool xrtDirEnsureEmpty(cstr sPath);
```

递归删除按后序执行且不跟随链接，显式拒绝完整文件系统根。`xrtDirRemoveAll` 删除根，`xrtDirClean` 保留根。删除中途失败不会恢复已经删除的对象。

`xrtDirMove` 优先同卷原子改名。目标已存在且允许替换时执行合并复制后删除源，因此不是原子目录替换；跨卷或平台缺少目录排他改名时也采用复制后删除。复制成功但删除源失败时两端可能同时存在。

`xrtDirStats` 非递归模式统计根和直接子项，递归模式统计完整树；`xrtDirSize` 只累计普通文件字节。`xrtDirEnsureEmpty` 创建缺失目录，或清空已有目录；同名非目录对象返回类型错误。

高级复制可以显式组合策略并取得完整统计：

```c
xtreecopyoptions Options;
xwalkstats Stats;

xrtTreeCopyOptionsInit(&Options);
Options.Flags = XTREE_COPY_MERGE | XTREE_COPY_REPLACE |
	XTREE_COPY_METADATA;
if ( !xrtFileTreeCopy("assets", "build/assets", &Options, &Stats) ) {
	return false;
}
printf("copied %llu files, %llu bytes\n",
	(unsigned long long)Stats.Files,
	(unsigned long long)Stats.Bytes);
```

常用目录管理不需要构造选项：

```c
xwalkstats Stats;
uint64 iSize;

if ( !xrtDirEnsureEmpty("build/work") ||
	 !xrtDirCopy("assets", "build/work/assets", false) ||
	 !xrtDirStats("build/work", true, &Stats) ||
	 !xrtDirSize("build/work", true, &iSize) ||
	 !xrtDirMove("build/work", "build/output", false) ) {
	return false;
}
if ( !xrtDirClean("build/output") ||
	 !xrtDirRemoveAll("build/output") ) {
	return false;
}
```

需要保留删除统计时直接使用 `xrtFileTreeRemove`：

```c
xwalkstats Stats;

if ( !xrtFileTreeRemove("cache", true, &Stats) ) {
	return false;
}
```

### `xrtTreeCopyOptionsInit`

初始化树复制选项：目标必须不存在、保留符号链接、拒绝特殊对象。

```c
void xrtTreeCopyOptionsInit(xtreecopyoptions* pOptions);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOptions` | 输出 | 非空 | 接收默认选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化，不失败 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[file/dir_tour · 高级树复制](../../examples/file/dir_tour/main.c) · 观察

```c
		xrtTreeCopyOptionsInit(&Options);
```


### `xrtFileTreeCopy`

使用高级选项复制目录树，统计成功时返回源树对象数量。

```c
bool xrtFileTreeCopy(cstr sSource, cstr sTarget,
	const xtreecopyoptions* pOptions, xwalkstats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sSource` | 输入 | 非空 | 源树根 |
| `sTarget` | 输入 | 非空 | 目标路径（默认须不存在） |
| `pOptions` | 输入 | 允许空 | 空 = 默认保守选项 |
| `pStats` | 输出 | 可空 | 接收复制统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 整树已复制 | — |
| `false` | 目标冲突或系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码
- `XERR_EXISTS` — 目标已存在且选项要求不存在

#### 范例

[file/dir_tour · 高级树复制](../../examples/file/dir_tour/main.c) · 观察

```c
		if ( !xrtFileTreeCopy(sRoot, "xrt-dir-tour-copy", &Options, &Stats) ||
```


### `xrtFileTreeRemove`

后序删除目录树；`bKeepRoot` 为真时只清空内容。

```c
bool xrtFileTreeRemove(cstr sPath, bool bKeepRoot,
	xwalkstats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 | 树根路径 |
| `bKeepRoot` | 输入 | — | 保留根目录（等价只清空） |
| `pStats` | 输出 | 可空 | 接收删除统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 整树（或内容）已删除 | — |
| `false` | 系统错误 | — |

#### 错误

- 系统错误（`xrt.io` 域）— 平台调用失败，保留原生错误码

#### 范例

[file/dir_tour · 后序删除](../../examples/file/dir_tour/main.c) · 观察

```c
	if ( !xrtFileTreeRemove(sRoot, true, &Stats) ) {
```


## 错误域

文件体系使用统一 `xerror`，同时按子模块稳定区分错误域：

```c
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

typedef enum xwalkerror {
	XWALK_ERROR_OPTIONS = 1,
	XWALK_ERROR_CALLBACK,
	XWALK_ERROR_IDENTITY,
	XWALK_ERROR_OVERFLOW
} xwalkerror;

typedef enum xlinkerror {
	XLINK_ERROR_CREATE = 1,
	XLINK_ERROR_READ,
	XLINK_ERROR_DELETE,
	XLINK_ERROR_FORMAT
} xlinkerror;

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

typedef enum xfifoerror {
	XFIFO_ERROR_CREATE = 1
} xfifoerror;

typedef enum xtreeerror {
	XTREE_ERROR_OPTIONS = 1,
	XTREE_ERROR_SOURCE,
	XTREE_ERROR_TARGET,
	XTREE_ERROR_DESCENDANT,
	XTREE_ERROR_LINK_CYCLE,
	XTREE_ERROR_SPECIAL,
	XTREE_ERROR_ROOT
} xtreeerror;
```

这些枚举依次属于 `xrt.file`、`xrt.dir`、`xrt.walk`、`xrt.link`、`xrt.root`、`xrt.fifo` 和 `xrt.tree`。每个常量表示失败的具体操作或阶段，不替代统一错误种类。

错误 `Kind` 表达跨模块类别，例如不存在、权限、参数、类型、IO、范围和不支持；域内代码表达具体操作阶段；`SystemCode` 保留 Win32 或 `errno`。清理失败默认不覆盖触发操作的首个错误，除非清理本身是唯一失败。

## 异步边界

`file_async` 在有界任务池上提供绝对偏移读写、大小、调整、刷新和可等待关闭。它没有给每个同步路径 Helper 机械增加一个版本；异步整文件、目录管理和遍历仍按独立裁剪层建设。同步与异步路径共享同一打开标志、64 位偏移、所有权和结构化错误语义。

## 旧版资产处理

新版保留了旧文件体系中已经验证的 64 位定位、二进制 IO、整文件操作、目录枚举、系统根目录枚举、复制移动、链接、FIFO 和目录树能力，并迁移旧测试中的根目录、枚举结束、输出边界和平台分支。旧版 `test_file_rootfs.h` 验证的是驱动器或 `/` 的系统根枚举，这一资产已经进入 `xrtDirRoots`；它不是句柄锚定的目录能力，因此没有被错误包装成新的 `xroot` 实现。旧版 `xrtPathRandom` 的前后缀使用手感进入临时对象 API，但“查询路径不存在后再返回”的竞态契约被排他创建替代；旧原子写和大小写改名中的临时路径调用点也统一复用这一份实现。

以下旧设计被有证据地退役：文件句柄内保存文本编码、公开句柄内部字段、静态空字符串失败哨兵、魔法 EOF 返回值、重复的文本/二进制函数族，以及内部失败后仍返回不可信计数。旧中英文文档存在签名和语义漂移，本文件与公共头文件共同作为新版本权威契约。

## 完整示例

- `examples/file/basic/main.c`：打开、完整读写、定位和元数据。
- `examples/file/lock/main.c`：非阻塞排他文件锁。
- `examples/file/map/main.c`：只读文件映射。
- `examples/file/temp/main.c`：安全临时文件创建、使用和清理。
- `examples/file/whole/main.c`：整文件、原子写、复制和移动。
- `examples/file/text/main.c`：编码检测、UTF-8 文本和 BOM。
- `examples/file/directory/main.c`：目录创建、枚举、空目录和系统根。
- `examples/file/dir_temp/main.c`：安全临时目录创建和清理。
- `examples/file/walk/main.c`：深度限制、回调控制和统计。
- `examples/file/link/main.c`：符号链接读取、删除和硬链接。
- `examples/file/root/main.c`：锚定目录、打开子根和处理不可信相对路径。
- `examples/file/tree/main.c`：目录树复制、统计和递归清理。
- `examples/file/fifo/main.c`：POSIX FIFO 创建及 Windows 不支持分支。
