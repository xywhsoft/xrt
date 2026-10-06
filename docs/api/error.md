# 错误 API

Error 是全库统一的结构化错误对象与线程错误报告机制；本文件同时收录 core 并集的公共函数，与 [memory.md](memory.md)、[core.md](core.md) 共享头文件。

## 精确诊断文本

`xerrordescview` 与 `xerrorlocationview` 是 `xerrordesc` / `xerrorlocation`
的长度明确版本。Domain、Operation、Message、Data、File 都是 `xstrview`，
接受内嵌 NUL；NULL/0 表示空文本，NULL/非零是参数错误。创建时借用输入，
在一次分配中复制全部内容并增加 Cause 引用，不保存调用方缓冲区或描述地址。
尺寸与终止符的总和在分配/复制前检查溢出；失败不取得任何输入的所有权。
错误保持不可变，视图访问不分配，静态 OOM 错误也有完整的长度。

### `xrtErrorBuildView`

`xerror* xrtErrorBuildView(const xerrordescview* pDesc)`：返回 owned 错误，
失败返回 NULL 并设置当前错误。数值字段和原因链语义与 `xrtErrorBuild` 相同。

### `xrtErrorBuildViewAt`

`xerror* xrtErrorBuildViewAt(const xerrordescview*, const xerrorlocationview*)`：
同时复制可选的精确文件名；行列不能为负。旧 C 字符串构造函数只测量一次，
随后调用这个共同实现。printf 错误格式化保留其返回的实际长度，包括 `%c` 的 NUL。

### `xrtErrorDomainView`

返回完整 Domain 的借用 `xstrview`；`xrtErrorFind` 的 C 字符串域必须与完整视图
等长且逐字节相等，不能只匹配内嵌 NUL 前缀。

### `xrtErrorOperationView`

返回完整 Operation 的借用 `xstrview`。

### `xrtErrorMessageView`

返回完整 Message 的借用 `xstrview`，包括 NUL 后缀。

### `xrtErrorDataView`

返回完整 Data 的借用 `xstrview`。

### `xrtErrorFileView`

返回完整 File 的借用 `xstrview`。五个访问器对 NULL 错误返回空视图；视图仅在
错误仍存活时有效。旧 `cstr` 访问器是显式 C 互操入口，按 C 规则只观察到首个
NUL 之前的前缀；完整内容必须使用 View。text/JSON 日志格式器使用精确视图，
输出相应的控制字符转义，不把任意诊断文本作为裸 JSON 或格式串执行。

## 类型与常量

### `xseek`

通用 IO 与文件游标共享的移动基准。

```c
typedef enum xseek {
	XSEEK_START = 0,
	XSEEK_CURRENT,
	XSEEK_END
} xseek;
```

| 值 | 语义 |
|---|---|
| `XSEEK_START` | 从文件起点 |
| `XSEEK_CURRENT` | 从当前位置 |
| `XSEEK_END` | 遍历结束 |


### `xrtresourcelimits`

资源限额结构：限制输入/输出/单项字节数、条目数、节点数、深度与压缩比；`iFlags` 启用符号链接/硬链接/设备文件/外部实体的放行位。结构带 `iSize`/`iVersion` 前向兼容字段，未指定字段保持零值。


```c
typedef struct xrtresourcelimits {
	uint32 iSize;
	uint32 iVersion;
	uint64 iMaxInputBytes;
	uint64 iMaxOutputBytes;
	uint64 iMaxItemBytes;
	uint64 iMaxEntries;
	uint64 iMaxNodes;
	uint32 iMaxDepth;
	uint32 iMaxCompressionRatio;
	uint32 iFlags;
	uint32 iReserved;
} xrtresourcelimits;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `iSize` | `uint32` | iSize |
| `iVersion` | `uint32` | iVersion |
| `iMaxInputBytes` | `uint64` | iMaxInputBytes |
| `iMaxOutputBytes` | `uint64` | iMaxOutputBytes |
| `iMaxItemBytes` | `uint64` | iMaxItemBytes |
| `iMaxEntries` | `uint64` | iMaxEntries |
| `iMaxNodes` | `uint64` | iMaxNodes |
| `iMaxDepth` | `uint32` | iMaxDepth |
| `iMaxCompressionRatio` | `uint32` | iMaxCompressionRatio |
| `iFlags` | `uint32` | iFlags |
| `iReserved` | `uint32` | iReserved |


### `xrtprogressflag`

进度事件标志：`TOTAL_KNOWN` 表示总输入量已知，`FINAL` 表示本次事件为最后一次。


```c
typedef enum xrtprogressflag {
	XRT_PROGRESS_TOTAL_KNOWN = 1u << 0,
	XRT_PROGRESS_FINAL = 1u << 1
} xrtprogressflag;
```

| 值 | 语义 |
|---|---|
| `XRT_PROGRESS_TOTAL_KNOWN` | XRTPROGRESSTOTALKNOWN |


### `xrtprogress`

长耗时流操作共用的进度事件（带 `iSize`/`iVersion` 前向兼容字段）；回调仅在发起操作的线程内同步调用。


```c
typedef struct xrtprogress {
	uint32 iSize;
	uint32 iVersion;
	uint32 iFlags;
	uint32 iReserved;
	uint64 iInputBytes;
	uint64 iTotalInputBytes;
	uint64 iOutputBytes;
} xrtprogress;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `iSize` | `uint32` | iSize |
| `iVersion` | `uint32` | iVersion |
| `iFlags` | `uint32` | iFlags |
| `iReserved` | `uint32` | iReserved |
| `iInputBytes` | `uint64` | iInputBytes |
| `iTotalInputBytes` | `uint64` | iTotalInputBytes |
| `iOutputBytes` | `uint64` | iOutputBytes |


### `xbytesview`

字节视图只借用内存，不拥有数据，也不要求末尾补零。

```c
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `cbytes` | 数据 |
| `Size` | `size_t` | 字节数 |


### `xstrview`

字符串视图只借用字节，不拥有数据，也不要求末尾补零。

```c
typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `cstr` | 数据 |
| `Size` | `size_t` | 字节数 |


### `xtime`

绝对时间使用 公元 UTC 毫秒；该标量也是 xlang time 类型的底层表示。

```c
typedef int64 xtime;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xrtprogressproc`

返回 false 请求取消。实现不得在回调返回后继续保存 pProgress 或 pUserData。

```c
typedef bool (*xrtprogressproc)(const xrtprogress* pProgress, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。


### `xerrkind`

跨模块稳定的错误类别。

```c
typedef enum xerrkind {
	XERR_NONE = 0,
	XERR_ARGUMENT,
	XERR_TYPE,
	XERR_VALUE,
	XERR_RANGE,
	XERR_STATE,
	XERR_MEMORY,
	XERR_IO,
	XERR_NOT_FOUND,
	XERR_EXISTS,
	XERR_PERMISSION,
	XERR_AGAIN,
	XERR_TIMEOUT,
	XERR_CANCELLED,
	XERR_CLOSED,
	XERR_PROTOCOL,
	XERR_UNSUPPORTED,
	XERR_INTERNAL
} xerrkind;
```

| 值 | 语义 |
|---|---|
| `XERR_NONE` | 无 |
| `XERR_ARGUMENT` | 参数非法 |
| `XERR_TYPE` | 类型 |
| `XERR_VALUE` | 值非法 |
| `XERR_RANGE` | 范围越界 |
| `XERR_STATE` | 状态非法 |
| `XERR_MEMORY` | 内存分配失败 |
| `XERR_IO` | 系统 IO 失败 |
| `XERR_NOT_FOUND` | 未找到 |
| `XERR_EXISTS` | 已存在 |
| `XERR_PERMISSION` | 权限不足 |
| `XERR_AGAIN` | 暂不可推进 |
| `XERR_TIMEOUT` | 超时 |
| `XERR_CANCELLED` | 已取消 |
| `XERR_CLOSED` | 已关闭 |
| `XERR_PROTOCOL` | 协议非法 |
| `XERR_UNSUPPORTED` | 不支持 |
| `XERR_INTERNAL` | 内部不变量破坏 |


### `xerrordesc`

描述一个完整错误，所有字符串在创建时复制。

```c
typedef struct xerrordesc {
	xerrkind Kind;
	int32 Code;
	int32 SystemCode;
	cstr Domain;
	cstr Operation;
	cstr Message;
	cstr Data;
	const xerror* Cause;
} xerrordesc;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Kind` | `xerrkind` | 错误种类 |
| `Code` | `int32` | 错误码 |
| `SystemCode` | `int32` | 平台错误码 |
| `Domain` | `cstr` | 错误域 |
| `Operation` | `cstr` | 失败操作名 |
| `Message` | `cstr` | 消息文本 |
| `Data` | `cstr` | 数据 |
| `Cause` | `const xerror*` | 原因链 |


### `xerrorlocation`

可选的源码位置；零值表示调用方没有提供对应信息。

```c
typedef struct xerrorlocation {
	cstr File;
	int32 Line;
	int32 Column;
} xerrorlocation;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `File` | `cstr` | 文件名 |
| `Line` | `int32` | 行号 |
| `Column` | `int32` | 列号 |


### `xerror`

错误对象由 XRT 管理，对外保持不可变。

```c
typedef struct xerror xerror;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。


### `xerrorhandler`

错误处理器只借用错误对象，保存时必须增加引用。

```c
typedef void (*xerrorhandler)(const xerror* pError, ptr pUserData);
```

回调类型；参数与返回语义见签名及各使用方 API 节。


### `xallocator`

XRT 所有动态内存最终使用同一个底层分配器。

```c
typedef struct xallocator {
	ptr Context;
	xallocproc Alloc;
	xreallocproc Realloc;
	xfreeproc Free;
} xallocator;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Context` | `ptr` | 回调上下文 |
| `Alloc` | `xallocproc` | Alloc |
| `Realloc` | `xreallocproc` | Realloc |
| `Free` | `xfreeproc` | 空闲量 |


### `xallocproc`

自定义底层分配器回调。

```c
typedef ptr (*xallocproc)(ptr pContext, size_t iSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。


### `xreallocproc`

底层分配器的重分配回调：把 `pMemory` 调整为 `iSize` 字节，失败返回 `NULL`；`pMemory == NULL` 等价分配，`iSize == 0` 等价释放。


```c
typedef ptr (*xreallocproc)(ptr pContext, ptr pMemory, size_t iSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。


### `xfreeproc`

底层分配器的释放回调：释放 `pMemory`；`pMemory == NULL` 应为空操作。


```c
typedef void (*xfreeproc)(ptr pContext, ptr pMemory);
```

回调类型；参数与返回语义见签名及各使用方 API 节。


### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_VERSION_MAJOR` | `2` | XRT 版本信息。 |
| `XRT_VERSION_MINOR` | `0` | VERSIONMINOR |
| `XRT_VERSION_PATCH` | `0` | VERSIONPATCH |
| `XRT_NPOS` | `SIZE_MAX` | 所有基于 size_t 的查找接口共用的未找到标记。 |
| `XRT_RESOURCE_LIMITS_VERSION` | `1u` | 解析器、压缩器与归档器共用的资源边界。零值表示不限制对应项目。 |
| `XRT_RESOURCE_ALLOW_SYMLINKS` | `0x00000001u` | RESOURCEALLOWSYMLINKS |
| `XRT_RESOURCE_ALLOW_HARDLINKS` | `0x00000002u` | RESOURCEALLOWHARDLINKS |
| `XRT_RESOURCE_ALLOW_DEVICE_FILES` | `0x00000004u` | RESOURCEALLOWDEVICEFILES |
| `XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES` | `0x00000008u` | RESOURCEALLOWEXTERNALENTITIES |
| `XRT_PROGRESS_VERSION` | `1u` | 长耗时流操作共用的进度事件。回调仅在发起操作的线程内同步调用。 |
| `XRT_BYTES_INIT` | `(sData) \` | INIT 用于聚合初始化器；LITERAL 用于赋值和函数实参表达式。 |
| `XRT_STR_INIT` | `(sText) { (sText), sizeof(sText) - 1u }` | STR初始化 |

## 设计契约

`xerror` 是不可变、可跨线程持有的结构化错误对象。错误由通用类别、稳定域、模块代码、系统代码、操作名、UTF-8 消息、可选数据和原因链组成。通用类别用于跨模块控制流，域与代码用于模块精确判断，消息只用于展示。

Core 错误 API 不依赖容器、字符串模块或 printf 运行时。需要结构化附加数据的模块应为
`Data` 定义稳定格式，或在自己的结果对象中保存详细字段，不能要求调用方解析展示消息。
动态消息格式化是独立的 `error_format` 便利模块，不增加最小 Core 的代码体积。

## 通用类别

- `XERR_ARGUMENT`、`XERR_TYPE`、`XERR_VALUE`、`XERR_RANGE`：输入不符合契约。
- `XERR_STATE`：对象或运行时状态不允许当前操作。
- `XERR_MEMORY`：内存不足。
- `XERR_IO`、`XERR_NOT_FOUND`、`XERR_EXISTS`、`XERR_PERMISSION`：系统资源错误。
- `XERR_AGAIN`：当前无法推进，但对象仍有效，调用方可等待后重试。
- `XERR_TIMEOUT`、`XERR_CANCELLED`、`XERR_CLOSED`：等待终止或资源关闭原因。
- `XERR_PROTOCOL`、`XERR_UNSUPPORTED`、`XERR_INTERNAL`：协议、能力或内部不变量错误。

`XERR_NONE` 只表示没有错误，不能用于创建错误对象。

## 字段与原因链

`xrtErrorKind`、`xrtErrorDomain`、`xrtErrorCode`、`xrtErrorSystemCode`、`xrtErrorOperation`、`xrtErrorMessage`、`xrtErrorData` 和 `xrtErrorCause` 返回不可变字段。空错误指针返回零值或空字符串。

`xrtErrorIs` 沿原因链查找通用类别。`xrtErrorFind` 沿原因链精确匹配域和代码。两者返回借用指针，未找到时返回 `NULL`。

## 当前错误

每个执行上下文拥有一个当前错误。普通线程默认使用线程上下文；协程和任务调度器切换自己的错误槽，因此迁移协程不会污染承载线程或其他任务。原生线程退出时仍留在默认错误槽中的对象会自动释放，C 扩展线程不需要为防止泄漏而强制清错。

- `xrtGetError` 借用当前错误。
- `xrtTakeError` 取走当前引用并清空错误槽。
- `xrtSetError` 增加传入对象的引用并替换当前错误，传入 `NULL` 等价于清除。
- `xrtClearError` 清除并释放当前引用。

常见失败不需要手工创建、设置再释放临时对象：

```c
xrtSetErrorInfo(XERR_ARGUMENT, "app.config", 1, "path is empty");
```

需要动态消息时选择 `XRT_MODULE_ERROR_FORMAT`：

```c
#define XRT_MODULE_ERROR_FORMAT
#include <xrt.h>

xrtSetErrorFormat(XERR_NOT_FOUND, "app.config", 2,
	"file does not exist: %s", path);
```

两个 Helper 都会创建不可变错误并直接移交给当前上下文。`xrtSetErrorFormat` 使用 printf
规则，支持任意长度消息，拒绝具有写入副作用的 `%n`，并把 `NULL` 格式报告为参数错误。
格式化测量与写入和字符串格式化模块复用同一内部实现，不重复维护解析器。构造失败时保留
无分配的 `XERR_MEMORY`，不会发布部分消息。

成功操作不隐式清除旧错误。调用方只能在函数通过返回值报告失败后读取当前错误；需要长期保留时使用 `xrtTakeError` 或 `xrtErrorRef`。

## API

### 错误对象构建

### `xrtErrorBuild`

从完整描述创建一个错误对象。

```c
xerror* xrtErrorBuild(const xerrordesc* pDesc);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDesc` | 输入 | 非空 | 类别/域/码/操作/消息/数据/原因完整描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有引用的错误对象 | — |
| `NULL` | 描述非法或 OOM | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 描述指针非法
- `XERR_MEMORY` — 对象或字段副本分配失败

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 六字段描述逐项核对

```c
pError = xrtErrorBuild(&Desc);
if ( (pError == NULL) ||
	(xrtErrorKind(pError) != XERR_ARGUMENT) ||
	(xrtErrorCode(pError) != 7) ||
	(strcmp(xrtErrorDomain(pError), "demo") != 0) ||
```


### `xrtErrorBuildAt`

从完整描述和可选源码位置创建一个错误对象。

```c
xerror* xrtErrorBuildAt(
	const xerrordesc* pDesc,
	const xerrorlocation* pLocation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDesc` | 输入 | 非空 | 完整描述 |
| `pLocation` | 输入 | 允许空 | File/Line/Column 源码位置；空 = 无位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 带位置的错误对象 | — |
| `NULL` | 参数非法或 OOM | 同 `xrtErrorBuild` |

#### 错误

- 同 `xrtErrorBuild`

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · 三定位器核对

```c
Location.File = "democ.c";
Location.Line = 10;
Location.Column = 2;
pInner = xrtErrorBuildAt(&Desc, &Location);
```


### `xrtErrorCreate`

创建一个常用错误对象。

```c
xerror* xrtErrorCreate(xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | 枚举值 | 错误类别 |
| `sDomain` | 输入 | 非空 | 稳定域字符串 |
| `iCode` | 输入 | — | 域内代码 |
| `sMessage` | 输入 | 非空 | 人类可读消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有引用的错误对象 | — |
| `NULL` | 参数非法或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` / `XERR_MEMORY`

#### 范例

[core/error · 原因链](../../examples/core/error/main.c) · 下层错误构建

```c
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "example.net", 1, "connect timeout");
```


### `xrtErrorWrap`

创建带有原因链的错误对象；内部增加原因的引用。

```c
xerror* xrtErrorWrap(const xerror* pCause, xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCause` | 输入 | 非空、借用 | 下层原因；Wrap 增加其引用 |
| `Kind` | 输入 | — | 上层类别 |
| `sDomain` | 输入 | — | 上层域 |
| `iCode` | 输入 | — | 上层代码 |
| `sMessage` | 输入 | — | 上层消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 上层错误，`Cause` 指向原因对象 | — |
| `NULL` | 参数非法或 OOM | 原因对象不受影响 |

#### 错误

- `XERR_ARGUMENT` / `XERR_MEMORY`

#### 范例

[core/error · 原因链](../../examples/core/error/main.c) · 包装后释放自己的原因引用

```c
pError = xrtErrorWrap(pCause, XERR_IO, "example.client", 2, "request failed");
xrtErrorFree(pCause);
```


### `xrtErrorRef`

增加错误对象引用并返回原指针。

```c
xerror* xrtErrorRef(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空 | 目标错误对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 原指针 | 引用 +1；须配一次 `ErrorFree` | — |
| `NULL` | 参数非法或引用耗尽 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — `pError` 为空

#### 范例

[error/tour · 共享引用](../../examples/error/tour/main.c) · Ref 与 Free 配对

```c
pRef = xrtErrorRef(pError);
if ( (pRef != pError) ) {
	goto Cleanup;
}
xrtErrorFree(pRef);  /* Ref 那份 */
```


### `xrtErrorFree`

释放错误对象引用。

```c
void xrtErrorFree(xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 要释放的引用；归零时连同原因链释放 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[core/error · 生命周期](../../examples/core/error/main.c) · 每份引用各自释放

```c
xrtSetError(pError);
xrtErrorFree(pError);
```


### 字段访问与原因链查询

### `xrtErrorKind`

返回错误类别。

```c
xerrkind xrtErrorKind(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `xerrkind` | 类别枚举；空指针返回 `XERR_NONE` |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 构建后核对类别

```c
(xrtErrorKind(pError) != XERR_ARGUMENT) ||
```


### `xrtErrorDomain`

返回错误的稳定域字符串。

```c
cstr xrtErrorDomain(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 域字符串借用；空指针返回空串 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 域核对

```c
(strcmp(xrtErrorDomain(pError), "demo") != 0) ||
```


### `xrtErrorCode`

返回域内错误代码。

```c
int32 xrtErrorCode(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 域内代码；空指针返回 0 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 代码核对

```c
(xrtErrorCode(pError) != 7) ||
```


### `xrtErrorSystemCode`

返回原生系统错误码（若错误由系统调用失败产生）。

```c
int32 xrtErrorSystemCode(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 平台原生错误码；无系统码时为 0 |

#### 范例

[process/open · 失败诊断](../../examples/process/open/main.c) · 打开失败时打印系统码

```c
pError != NULL ? xrtErrorSystemCode(pError) : 0
```


### `xrtErrorOperation`

返回产生错误的操作名。

```c
cstr xrtErrorOperation(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 操作名借用；未设置时为空串 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 操作名核对

```c
(strcmp(xrtErrorOperation(pError), "load") != 0) ||
```


### `xrtErrorMessage`

返回人类可读消息；永不返回 `NULL`。

```c
cstr xrtErrorMessage(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 消息借用；空错误返回 `"(no error)"` |

#### 范例

[core/error · 读取](../../examples/core/error/main.c) · 未设置错误时也有安全占位

```c
printf("error: %s\n", xrtErrorMessage(xrtGetError()));
```


### `xrtErrorData`

返回结构化附加数据字符串（如 `offset=12`）。

```c
cstr xrtErrorData(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 数据字符串借用；未设置时为空串 |

#### 范例

[error/tour · 构建](../../examples/error/tour/main.c) · 结构化数据核对

```c
(strcmp(xrtErrorData(pError), "ctx") != 0) ) {
```


### `xrtErrorFile`

返回错误记录的源文件名。

```c
cstr xrtErrorFile(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | 文件名借用；未记录时为空串 |

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · BuildAt 的定位器之一

```c
(strcmp(xrtErrorFile(pInner), "democ.c") != 0) ||
```


### `xrtErrorLine`

返回错误记录的源码行号。

```c
int32 xrtErrorLine(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 行号；未记录时为 0 |

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · 行号核对

```c
(xrtErrorLine(pInner) != 10) ||
```


### `xrtErrorColumn`

返回错误记录的源码列号。

```c
int32 xrtErrorColumn(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `int32` | 列号；未记录时为 0 |

#### 范例

[error/tour · 源码位置](../../examples/error/tour/main.c) · 列号核对

```c
(xrtErrorColumn(pInner) != 2) ) {
```


### `xrtErrorCause`

返回原因链中的下层错误（借用）。

```c
const xerror* xrtErrorCause(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 目标错误 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 下层原因；链尾返回 `NULL` |

#### 错误

- 无 — 链尾 `NULL` 是查询结果

#### 范例

[network/proxy_dial · 链遍历](../../examples/network/proxy_dial/main.c) · 沿链逐层打印

```c
pError = xrtErrorCause(pError);
```


### `xrtErrorIs`

沿原因链查找指定类别，命中返回借用的错误。

```c
const xerror* xrtErrorIs(const xerror* pError, xerrkind Kind);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 链起点 |
| `Kind` | 输入 | — | 要匹配的类别 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 链上第一个该类别的错误 |
| `NULL` | 链上无该类别（查询结果，不设错） |

#### 错误

- 无 — 未命中是查询结果

#### 范例

[core/error · 类别判定](../../examples/core/error/main.c) · 判断失败是否由超时引起

```c
xrtErrorIs(xrtGetError(), XERR_TIMEOUT) != NULL ? "yes" : "no");
```


### `xrtErrorFind`

沿原因链查找完全匹配的错误域和代码，返回借用的错误对象。

```c
const xerror* xrtErrorFind(const xerror* pError, cstr sDomain, int32 iCode);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 链起点 |
| `sDomain` | 输入 | 非空 | 要匹配的域 |
| `iCode` | 输入 | — | 要匹配的代码 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 链上第一个域 + 代码完全匹配的错误 |
| `NULL` | 未命中（查询结果，不设错） |

#### 错误

- 无 — 未命中是查询结果

#### 范例

[error/tour · 链查找](../../examples/error/tour/main.c) · 命中内层 / 未命中为空

```c
pFound = (xerror*)xrtErrorFind(pError, "inner", 42);
pMiss = xrtErrorFind(pError, "inner", 99);
```


### 当前执行上下文错误

### `xrtGetError`

返回当前执行上下文借用的错误对象。

```c
const xerror* xrtGetError(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 借用指针 | 线程错误槽中的对象；借用有效期到下一次本线程错误设置 |
| `NULL` | 无错误 |

#### 错误

- 无 — 空槽返回 `NULL` 是查询结果

#### 范例

[error/tour · 读取](../../examples/error/tour/main.c) · 设置后立即读取核对

```c
if ( (xrtGetError() == NULL) ||
	(xrtErrorKind(xrtGetError()) != XERR_TIMEOUT) ) {
```


### `xrtTakeError`

取走当前执行上下文的错误对象。

```c
xerror* xrtTakeError(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有引用；槽被清空，调用方负责 `ErrorFree` | — |
| `NULL` | 槽为空 | 纯取走，不设置错误 |

#### 错误

- 无 — 空槽返回 `NULL` 是查询结果

#### 范例

[data/json_tour · 取走](../../examples/data/json_tour/main.c) · 解析失败后取走向法定位

```c
((pError = xrtTakeError()) == NULL) ||
!xrtJsonErrorLocation(pError, &Location) ||
```


### `xrtSetError`

将错误对象设置到当前执行上下文，函数会增加引用。

```c
void xrtSetError(const xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 要设置的错误；空 = 清除 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 槽持有自己的引用；调用方仍须释放自己的引用（“SetError 不偷引用”） |

#### 范例

[core/error · 设置](../../examples/core/error/main.c) · 设置后立即释放自己的引用

```c
xrtSetError(pError);
xrtErrorFree(pError);
```


### `xrtSetErrorTake`

将错误对象所有权转移到当前执行上下文，调用后不得继续使用原引用。

```c
void xrtSetErrorTake(xerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许空 | 要转移的错误；空 = 清除 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 所有权转移；原指针此后无效 |

#### 范例

[error/tour · 所有权转移](../../examples/error/tour/main.c) · 转移后置空原指针

```c
xrtSetErrorTake(pTaken);
pTaken = NULL;  /* 所有权已转移，不得继续使用 */
```


### `xrtSetErrorInfo`

创建常用错误并直接设置到当前执行上下文。

```c
void xrtSetErrorInfo(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | — | 类别 |
| `sDomain` | 输入 | 非空 | 域 |
| `iCode` | 输入 | — | 代码 |
| `sMessage` | 输入 | 非空 | 消息 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 一步完成创建 + 设置；OOM 时槽设置内存错误 |

#### 范例

[error/tour · 便捷设置](../../examples/error/tour/main.c) · 四参数直设

```c
xrtSetErrorInfo(XERR_RANGE, "demo", 5, "out of range");
```


### `xrtSetErrorKind`

设置无分配的通用错误；`NONE` 清除错误，无效类别设置参数错误。

```c
void xrtSetErrorKind(xerrkind Kind);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | — | 通用类别 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 零分配路径，适合热路径 |

#### 范例

[error/tour · 通用错误](../../examples/error/tour/main.c) · 设置即触发全局 Handler

```c
xrtSetErrorKind(XERR_TIMEOUT);  /* 触发全局 Handler 一次 */
```


### `xrtClearError`

清除当前执行上下文的错误。

```c
void xrtClearError(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 释放槽持有的引用；后续 `GetError` 返回 `NULL` |

#### 范例

[error/tour · 清空](../../examples/error/tour/main.c) · 每段核对前清理基线

```c
xrtClearError();
xrtSetErrorInfo(XERR_RANGE, "demo", 5, "out of range");
```


### `xrtSetErrorHandler`

设置进程级错误通知处理器。

```c
void xrtSetErrorHandler(xerrorhandler pHandler, ptr pUserData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHandler` | 输入 | 允许空 | 每次任何线程设置错误时收到通知；空 = 注销 |
| `pUserData` | 输入 | 任意值 | 原样传给处理器 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 处理器在错误设置点同步执行；不得递归设置错误 |

#### 范例

[error/tour · 全局通知](../../examples/error/tour/main.c) · 计数通知后注销

```c
(void)xrtSetErrorHandler(exampleHandler, NULL);
xrtSetErrorKind(XERR_TIMEOUT);  /* 触发全局 Handler 一次 */
```


### `xrtSetErrorFormat`

使用 printf 规则创建常用错误并直接设置到当前执行上下文；需启用 `XRT_MODULE_ERROR_FORMAT`。

```c
void xrtSetErrorFormat(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sFormat,
	...
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Kind` | 输入 | — | 错误类别 |
| `sDomain` | 输入 | 非空 | 稳定域 |
| `iCode` | 输入 | — | 域内代码 |
| `sFormat` | 输入 | 非空、printf 规则 | 格式串；拒绝 `%n` |
| `...` | 输入 | — | 与格式串对应的可变实参 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 一步完成格式化构造与设置；构造失败时保留无分配的 `XERR_MEMORY`，不发布部分消息 |

#### 错误

- `XERR_ARGUMENT` — 格式串为空或含 `%n`
- `XERR_MEMORY` — 消息缓冲分配失败（无分配路径）

#### 范例

[core/error_format · 动态消息](../../examples/core/error_format/main.c) · printf 规则一步设置

```c
	xrtSetErrorFormat(
		XERR_NOT_FOUND,
		"example.config",
		1,
		"file does not exist: %s", "app.json"
	);
```



## 错误处理器

`xrtSetErrorHandler` 安装一个进程级观察处理器。设置错误时，XRT 先更新当前错误，再使用并发一致的“回调+用户数据”快照通知处理器。回调只借用错误对象，可以读取或增加引用；处理器内部再次设置错误不会递归调用自身。

处理器在隔离的错误边界内执行。处理器调用的 XRT API 即使失败，或者处理器主动设置、清除、取走当前错误，回调返回后仍会恢复正在通知的主错误。处理器需要保留主错误时仍应调用 `xrtErrorRef`；通过 `xrtTakeError` 取得的引用由处理器自行释放。

处理器用于日志、调试和语言运行时桥接，不能替代函数返回值。并发替换处理器时，已经开始的通知可以使用替换前的完整快照；调用方必须让旧处理器的用户数据存活到全部在途回调结束。

## OOM 保证

参数、状态、范围和内存不足等核心错误使用静态不可变对象。即使自定义分配器
已经失败，`xrtMalloc()` 仍能无分配地设置 `XERR_MEMORY`，错误构造失败也
不会用第二次分配覆盖这个原因。

## 范例

```c
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "app.net", 1, "connect timeout");
xerror* pError = xrtErrorWrap(pCause, XERR_IO, "app.client", 2, "request failed");

xrtErrorFree(pCause);
if ( pError == NULL ) {
	return 1;
}
xrtSetError(pError);
xrtErrorFree(pError);

if ( xrtErrorIs(xrtGetError(), XERR_TIMEOUT) != NULL ) {
	/* 根据原因链决定是否重试。 */
}
xrtClearError();
```

结构化原因链范例位于 `examples/core/error/main.c`，动态消息范例位于
`examples/core/error_format/main.c`。

## 旧版资产决策

旧版只保存线程局部 UTF-8 字符串，并通过 `bFree` 把消息所有权交给错误槽；
模块只能依赖消息文本，上层宿主也无法稳定区分错误类别。新版保留线程隔离、
设置/读取/清除的简短手感和进程级观察回调，替换为不可变引用对象、稳定域与
代码、系统代码、操作、机器数据和原因链。

旧 `test_base.h` 的字符串错误、UTF-16/UTF-32 转换入口不进入核心错误层；
字符转换由 Charset 完成，模块直接构造 UTF-8 结构化错误。当前回归额外覆盖
错误深复制、原因查询、并发线程隔离、线程退出析构、协程执行上下文和 OOM
无分配报告。

## 附录：Core 并集函数

本文件的门禁并集包含 core 模块全部四个头（features/core/error/memory），因此下列内存、引用与版本函数在本文件同样成节；完整模块语境见 [core.md](core.md) 与 [memory.md](memory.md)。

### `xrtVersion`

返回当前 XRT 版本字符串。

```c
cstr xrtVersion(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `cstr` | `XRT_VERSION_TEXT` 静态借用字符串，不得释放 |

#### 范例

[core/version_limits · 版本](../../examples/core/version_limits/main.c) · 编译期宏与运行期字符串一致

```c
printf("version=%s\n", xrtVersion());
```

### `xrtResourceLimitsInit`

把资源边界初始化为适合处理不受信任输入的保守默认值。

```c
void xrtResourceLimitsInit(xrtresourcelimits* pLimits);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLimits` | 输出 | 非空 | 接收清零后的保守默认边界 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 先清零整个结构再写默认值；零值字段表示不限制该项目 |

#### 范例

[core/version_limits · 资源边界](../../examples/core/version_limits/main.c) · 初始化后按需收紧

```c
printf("version=%s\n", xrtVersion());
xrtResourceLimitsInit(&Limits);
```

### `xrtRefRetain`

原子增加引用计数并返回新值。

```c
int32 xrtRefRetain(volatile int32* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCount` | 输入/输出 | 非空、正计数 | 对象内嵌的 `volatile int32` 计数字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 新计数值 | — |
| `-1` | 空指针、非正计数或达到 `INT32_MAX`（溢出保护） | 计数不变，不设置错误 |

#### 错误

- 无 — 原语按返回值报告；调用方把 `-1` 当失败处理

#### 范例

[core/reference · 共享对象](../../examples/core/reference/main.c) · 第二持有者接管前先 Retain

```c
if ( (pObject == NULL) || (xrtRefRetain(&pObject->RefCount) < 0) ) {
	return NULL;
}
```

### `xrtRefRelease`

原子减少引用计数并返回新值；返回零的线程负责析构。

```c
int32 xrtRefRelease(volatile int32* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCount` | 输入/输出 | 非空、正计数 | 对象内嵌计数字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 仍有其他持有者 | — |
| `0` | 调用方是最后持有者，执行析构 | — |
| `-1` | 空指针或非正计数（重复释放） | 计数不变，不设置错误 |

#### 错误

- 无 — 原语按返回值报告

#### 范例

[core/reference · 析构判定](../../examples/core/reference/main.c) · 归零者释放对象

```c
if ( (pObject != NULL) && (xrtRefRelease(&pObject->RefCount) == 0) ) {
```


### `xrtOwnershipRefRetain`

在 ownership mutation 域中原子增加一个独立、图可见的引用计数更新。

```c
int32 xrtOwnershipRefRetain(volatile int32* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCount` | 输入/输出 | 非空、正计数 | 图节点计数字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 新计数 | — |
| `-1` | 参数、边界或准入失败 | 计数不变 |

#### 错误

- 计数边界按 `-1` 报告；准入错误遵循 ownership scope 契约

#### 范例

参见已注册的 [examples/core/reference/main.c](../../examples/core/reference/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtOwnershipRefRetain(&iGraphCount)
```
### `xrtOwnershipRefRelease`

在 ownership mutation 域中原子减少一个独立、图可见的引用计数更新。

```c
int32 xrtOwnershipRefRelease(volatile int32* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCount` | 输入/输出 | 非空、正计数 | 图节点计数字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 仍有其他持有者 | — |
| `0` | 最后引用 | — |
| `-1` | 参数、边界或准入失败 | 计数不变 |

#### 错误

- 计数边界按 `-1` 报告；准入错误遵循 ownership scope 契约

#### 范例

参见已注册的 [examples/core/reference/main.c](../../examples/core/reference/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtOwnershipRefRelease(&iGraphCount)
```
### `xrtSetAllocator`

替换进程级分配器；必须在任何 XRT 分配发生前调用。

```c
bool xrtSetAllocator(const xallocator* pAllocator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAllocator` | 输入 | 非空、三个回调齐全 | 新分配器（Context + Alloc/Realloc/Free） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 分配器已替换 | — |
| `false` | 已有分配发生后再替换，或分配器非法 | 原分配器保持；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` — 进程内已存在 XRT 分配，无法安全替换
- `XERR_ARGUMENT` — 分配器结构或回调非法

#### 范例

[core/allocator_tour · 自定义分配器](../../examples/core/allocator_tour/main.c) · 计数分配器替换

```c
Custom.Context = (ptr)&Count;
Custom.Alloc = exampleAlloc;
Custom.Realloc = exampleRealloc;
Custom.Free = exampleFree;
if ( !xrtSetAllocator(&Custom) ) {
	goto Cleanup;
}
```

### `xrtGetAllocator`

读取当前进程分配器副本。

```c
void xrtGetAllocator(xallocator* pAllocator);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAllocator` | 输出 | 非空 | 接收当前分配器结构副本 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 副本可用于稍后恢复默认分配器 |

#### 范例

[core/allocator_tour · 恢复](../../examples/core/allocator_tour/main.c) · 先取默认，结束后还原

```c
xrtGetAllocator(&Default);
```

### `xrtMalloc`

分配指定字节的内存。

```c
ptr xrtMalloc(size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 未初始化内存 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` 已设置 |

#### 错误

- `XERR_MEMORY` — 分配失败（线程错误已由分配器路径设置）

#### 范例

[core/reference · 创建对象](../../examples/core/reference/main.c) · 统一收口入口

```c
example_object* pObject = (example_object*)xrtMalloc(sizeof(example_object));
```

### `xrtCalloc`

按元素数 × 大小分配并清零。

```c
ptr xrtCalloc(size_t iCount, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCount` | 输入 | — | 元素数 |
| `iSize` | 输入 | — | 每元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已清零内存 | — |
| `NULL` | 分配失败或乘法溢出 | `XERR_MEMORY` / `XERR_RANGE` |

#### 错误

- `XERR_MEMORY` — 分配失败
- `XERR_RANGE` — `iCount * iSize` 溢出

#### 范例

[core/memory · 分配](../../examples/core/memory/main.c) · 4 字节清零分配

```c
pValues = (unsigned char*)xrtCalloc(4, sizeof(unsigned char));
if ( pValues == NULL ) {
	return 1;
}
```

### `xrtRealloc`

扩容或缩容已有分配；失败时原块保持有效。

```c
ptr xrtRealloc(ptr pMemory, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | 原块；空指针等价 `Malloc` |
| `iSize` | 输入 | — | 新字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新块（内容前 min(旧,新) 字节保留） | — |
| `NULL` | 分配失败 | 原块仍有效，调用方继续持有 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[core/memory · 扩容](../../examples/core/memory/main.c) · 旧内容保留核对

```c
pValues = (unsigned char*)xrtRealloc(pValues, 16);
if ( pValues == NULL ) {
	return 2;
}
```

### `xrtFree`

释放分配；空指针是空操作。

```c
void xrtFree(ptr pMemory);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | `Malloc`/`Calloc`/`Realloc`/`MemDup` 族产物 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作，无需判空 |

#### 范例

[core/memory · 收尾](../../examples/core/memory/main.c) · 与持有顺序相反释放

```c
xrtFree(pCopy);
xrtFree(pValues);
return 0;
```

### `xrtMemDup`

复制一段内存为新的独立分配。

```c
ptr xrtMemDup(const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用、非空（`iSize > 0` 时） | 源字节 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 与源逐字节一致的独立副本 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[core/memory · 复制](../../examples/core/memory/main.c) · 分配 + memcpy 一步完成

```c
pCopy = (unsigned char*)xrtMemDup(Source, sizeof(Source));
if ( pCopy == NULL ) {
	xrtFree(pValues);   /* 失败路径也要释放已持有的资源 */
	return 3;
}
```

### `xrtSecureZero`

清零敏感内存；不会被编译器当作死存储删除。

```c
void xrtSecureZero(ptr pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用期间有效 | 密钥、令牌等敏感缓冲 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 普通 memset 可能被优化删除；本函数保证清零真实发生 |

#### 范例

[core/memory · 安全清零](../../examples/core/memory/main.c) · 释放敏感缓冲前擦除

```c
xrtSecureZero(pValues, 16);
xrtSecureZero(pCopy, sizeof(Source));
```

### `xrtMallocAt`

记录调用位置的 `Malloc`（`XRT_FEATURE_MEMORY_DEBUG`）；语义与 `xrtMalloc` 一致。

```c
ptr xrtMallocAt(size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSize` | 输入 | — | 字节数 |
| `sFile` | 输入 | 静态字符串 | 通常传 `__FILE__` |
| `iLine` | 输入 | — | 通常传 `__LINE__` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 未初始化内存（位置进入分配记录） | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 位置参数不影响堆行为

```c
pBlock = (uint8*)xrtMallocAt(4u, __FILE__, __LINE__);
```

### `xrtCallocAt`

记录调用位置的 `Calloc`。

```c
ptr xrtCallocAt(size_t iCount, size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iCount` | 输入 | — | 元素数 |
| `iSize` | 输入 | — | 每元素字节数 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已清零内存 | — |
| `NULL` | 失败或溢出 | 同 `xrtCalloc` |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 清零分配带位置

```c
uint8* pZero = (uint8*)xrtCallocAt(2u, 4u, __FILE__, __LINE__);
```

### `xrtReallocAt`

记录调用位置的 `Realloc`。

```c
ptr xrtReallocAt(ptr pMemory, size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | 原块 |
| `iSize` | 输入 | — | 新字节数 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新块 | — |
| `NULL` | 分配失败 | 原块仍有效 |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 扩容带位置

```c
((pBlock = (uint8*)xrtReallocAt(pBlock, 16u, __FILE__,
	__LINE__)) == NULL) ||
```

### `xrtFreeAt`

记录调用位置的 `Free`。

```c
void xrtFreeAt(ptr pMemory, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMemory` | 输入 | 允许空 | 要释放的块 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 配对释放带位置

```c
xrtFreeAt(pBlock, __FILE__, __LINE__);
```

### `xrtMemDupAt`

记录调用位置的 `MemDup`。

```c
ptr xrtMemDupAt(const void* pData, size_t iSize, cstr sFile, uint32 iLine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 源字节 |
| `iSize` | 输入 | — | 字节数 |
| `sFile` | 输入 | — | 源文件 |
| `iLine` | 输入 | — | 行号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 独立副本 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 范例

[core/allocator_tour · At 族](../../examples/core/allocator_tour/main.c) · 复制带位置

```c
((pDup = (uint8*)xrtMemDupAt("hi", 2u, __FILE__,
	__LINE__)) == NULL) ||
```


### `xrtErrorOwnership`

```c
xrtownershipref xrtErrorOwnership(const xerror* pError);
```

Borrowed physical ownership view. Immutable cause is one owning edge;
inline diagnostic text is not a separate reference-counted node. Static
immortal errors and NULL have an empty view. See xrtOwnershipInspect.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pError` | `const xerror*` | 借用的 const xerror* 对象或调用方结果槽，按上述操作契约使用。 |

#### 返回值

借用的物理视图；空视图不产生拥有引用，检查前仍需保证全图静止。

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtErrorOwnershipAdapterV1`

```c
const xrtownershipadapterv1* xrtErrorOwnershipAdapterV1(xrtownershipref Reference);
```

Exact resident immutable Error/Cause DAG, with real pins and no user code.
Clear keeps immutable cause owners until last Drop. Static errors are empty
references. Caller still needs whole-domain Freeze and child admission.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Reference` | `xrtownershipref` | 借用的物理所有权视图；查询前保证整个可达图静止及代码驻留。 |

#### 返回值

借用的常驻适配器；不满足完整准入协议返回 NULL。拒绝不等于空图。

#### 错误

NULL 表示准入拒绝或不识别；不调用未知策略回调，不授予生命周期或代码卸载权限。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtOwnershipFreezeTryBegin`

```c
bool xrtOwnershipFreezeTryBegin(xrtownershipscope* pScope);
```

Nonblocking exclusive admission. Busy returns false, leaves the zero scope
and ambient error unchanged, and never waits for a reader (including self).
Success freezes ONLY participating transitions until ScopeEnd. It is NOT
whole-graph quiescence unless EVERY reachable adapter and mutation path is
covered, nor is it a claim/commit operation or permission to unload code.
Value, Future/Promise and Cancel ownership transitions participate, including
complete waiter callbacks and combine/continuation assembly. Native blocking
waits and coroutine parking do not keep an enclosing mutation active.
User-owned callback payloads, native objects, task/transport state, other
direct atomic counters and generated storage still need outer participation.
The holder may nest balanced mutation scopes on the SAME native thread for
inspection cursors/controlled commit. Do not wait for other threads, suspend
fibers, or call arbitrary user callbacks while frozen. Keep code/data alive
independently; do not acquire a lock held by a blocked participant.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pScope` | `xrtownershipscope*` | 零初始化的作用域；只在进入它的原生线程上结束，不复制活动作用域。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

忙碌返回 false，零作用域与已有线程错误保持不变；无效作用域或状态才设错。

#### 范例

参见已注册的 [examples/value/discovery/main.c](../../examples/value/discovery/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtOwnershipFreezeTryBegin(&freeze)
```
### `xrtOwnershipInspect`

```c
bool xrtOwnershipInspect(const xrtownershipref* pAnchors, size_t iAnchorCount, const xrtownershipref* pInternalSlots, size_t iInternalSlotCount, xrtownershipresult* pResult);
```

Discover from borrowed anchors AND internal owning slots. Internal slots
are references held by an enclosing owner being retired (e.g. module
globals), so each occurrence subtracts one real strong reference. Anchors
only seed discovery and do NOT subtract references. Sharing is deduplicated
globally by physical identity; edges retain their multiplicity.
An external root has Count > internal incoming edges. Reachability from
these roots determines ReachableAnchorCount (unique anchor identities).
false leaves output and graph unchanged, including allocation/trace errors,
inconsistent descriptors and overcounted edges. No destructor is invoked.
This is NOT a concurrent collector, lifetime pin, or unload permission.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pAnchors` | `const xrtownershipref*` | 借用的发现起点数组；重复锚点不增加实际强引用。 |
| `iAnchorCount` | `size_t` | 锚点数组元素数，也是可达性输出数组的长度。 |
| `pInternalSlots` | `const xrtownershipref*` | 待退休所有者持有的真实强引用槽数组；每个槽保留独立的计数。 |
| `iInternalSlotCount` | `size_t` | 内部所有权槽数，重复引用不能合并。 |
| `pResult` | `xrtownershipresult*` | 调用方结果槽，按当前签名的类型交付结果。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtOwnershipInspectReachable`

```c
bool xrtOwnershipInspectReachable(const xrtownershipref* pAnchors, size_t iAnchorCount, const xrtownershipref* pInternalSlots, size_t iInternalSlotCount, bool* pReachable, xrtownershipresult* pResult, xrtownershiprootproc pRootPolicy, ptr pRootContext);
```

Same snapshot, additionally returning one reachability bit per input
anchor (NULL anchors are false; duplicate anchors have identical bits).
pReachable has iAnchorCount elements and aliases neither input nor result.
pRootPolicy may be NULL; otherwise it can force additional scope roots,
for example native objects outside the domain being collected.
On failure BOTH outputs remain unchanged. The caller still owns the
quiescent point and every code/data lifetime through any later commit.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pAnchors` | `const xrtownershipref*` | 借用的发现起点数组；重复锚点不增加实际强引用。 |
| `iAnchorCount` | `size_t` | 锚点数组元素数，也是可达性输出数组的长度。 |
| `pInternalSlots` | `const xrtownershipref*` | 待退休所有者持有的真实强引用槽数组；每个槽保留独立的计数。 |
| `iInternalSlotCount` | `size_t` | 内部所有权槽数，重复引用不能合并。 |
| `pReachable` | `bool*` | 调用方提供的可达性布尔数组，不与输入或统计结果重叠。 |
| `pResult` | `xrtownershipresult*` | 调用方结果槽，按当前签名的类型交付结果。 |
| `pRootPolicy` | `xrtownershiprootproc` | 可选的额外根判定回调；只在调用方保证的静止点使用。 |
| `pRootContext` | `ptr` | 根判定回调的借用上下文。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtOwnershipMutationBegin`

```c
bool xrtOwnershipMutationBegin(xrtownershipscope* pScope);
```

Enter BEFORE any lock protecting participating edges/counts, leave AFTER
the complete mutation. Concurrent and nested mutations are allowed. Only a
currently frozen domain can delay entry; collectors never queue an upgrade
behind an active mutator. Scope entry/end allocate no memory or TLS slots.
xrtOwnershipRefRetain/Release can participate for one standalone atomic
counter update. Generic xrtRefRetain/Release deliberately remain outside
this domain. Neither pair covers an enclosing field update, callback, or
destructor; graph adapters must guard each complete transition.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pScope` | `xrtownershipscope*` | 零初始化的作用域；只在进入它的原生线程上结束，不复制活动作用域。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtOwnershipScopeEnd`

```c
bool xrtOwnershipScopeEnd(xrtownershipscope* pScope);
```

End either kind; a frozen parent cannot end with nested scopes outstanding.
Invalid/copy/wrong-thread/double-end calls fail without releasing admission.
Invalid arguments/state set an error; success preserves the ambient error.
Closing a freeze is a release boundary for subsequent mutation admission.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pScope` | `xrtownershipscope*` | 零初始化的作用域；只在进入它的原生线程上结束，不复制活动作用域。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/value/discovery/main.c](../../examples/value/discovery/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtOwnershipScopeEnd(&freeze)
```
### `xrtOwnershipSnapshotCreate`

```c
bool xrtOwnershipSnapshotCreate(const xrtownershipref* pAnchors, size_t iAnchorCount, const xrtownershipref* pInternalSlots, size_t iInternalSlotCount, xrtownershipadmitproc pAdmit, ptr pAdmitContext, xrtownershipsnapshot** ppSnapshot);
```

在调用方提供的全图静止点发现并去重物理节点。记录强引用、内部引用和可达性，锚点仍为借用。快照不替代节点或代码的生存期保障。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pAnchors` | `const xrtownershipref*` | 借用的发现起点数组；重复锚点不增加实际强引用。 |
| `iAnchorCount` | `size_t` | 锚点数组元素数，也是可达性输出数组的长度。 |
| `pInternalSlots` | `const xrtownershipref*` | 待退休所有者持有的真实强引用槽数组；每个槽保留独立的计数。 |
| `iInternalSlotCount` | `size_t` | 内部所有权槽数，重复引用不能合并。 |
| `pAdmit` | `xrtownershipadmitproc` | 逐物理节点的准入回调；不能把未知原生资源假定为安全节点。 |
| `pAdmitContext` | `ptr` | 准入回调的借用上下文。 |
| `ppSnapshot` | `xrtownershipsnapshot**` | 成功时交付快照；快照不保活其中的节点或回调代码。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtOwnershipSnapshotDestroy`

```c
void xrtOwnershipSnapshotDestroy(xrtownershipsnapshot* pSnapshot);
```

Frees only snapshot bookkeeping; it neither releases nor touches nodes.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSnapshot` | `xrtownershipsnapshot*` | 有效快照；节点本体及其 Ops 的生存期仍由调用方保证。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtOwnershipSnapshotNode`

```c
bool xrtOwnershipSnapshotNode(const xrtownershipsnapshot* pSnapshot, size_t iIndex, xrtownershipnode* pNode);
```

The output is unchanged for NULL/invalid arguments or an out-of-range index.
A successful read allocates nothing and never calls an adapter again.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSnapshot` | `const xrtownershipsnapshot*` | 有效快照；节点本体及其 Ops 的生存期仍由调用方保证。 |
| `iIndex` | `size_t` | 以零为起点的元素序号。 |
| `pNode` | `xrtownershipnode*` | 成功时交付节点计数和可达性，失败保持原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtOwnershipSnapshotNodeCount`

```c
size_t xrtOwnershipSnapshotNodeCount(const xrtownershipsnapshot* pSnapshot);
```

返回快照中去重后的物理节点数。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSnapshot` | `const xrtownershipsnapshot*` | 有效快照；节点本体及其 Ops 的生存期仍由调用方保证。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

无效参数、生命周期状态或内存不足按当前模块错误模型报告。尚未接受的数据和未提交的拥有关系保持调用方所有；策略身份不匹配拒绝调用未知回调。详见上述逐接口契约。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtRuntimeRetireThreadStorage`

```c
bool xrtRuntimeRetireThreadStorage(void);
```

Terminal retirement of this runtime instance's internal Windows TLS/FLS.
The host must first stop admission, join all XRT work, release all exported
resources, clear dynamic thread keys on their owning threads, and unbind
execution contexts. Native threads/fibers may remain alive but must not
enter this instance again, including during retirement. Call outside
DllMain/loader lock, while this instance and its allocator are still loaded.
true permits unloading with respect to internal TLS/FLS callbacks only;
it is NOT an object/code lease or a general resource ownership check.
false keeps the code resident: retirement may be partial and may only be
retried, never resumed. Successful calls are idempotent. No error TLS is
accessed by this function. Unsupported platforms return false unchanged.

#### 参数

无参数。

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

失败可能已部分退休，只能保持代码驻留并重试；不访问错误 TLS。不支持的平台返回 false，不修改线程错误。

#### 范例

参见已注册的 [examples/core/error/main.c](../../examples/core/error/main.c)，结合本节参数和生存期规则使用。




### `xrtownershipadapterv1`

真实生命周期协议。Hold/Drop 必须对应实际引用；Claim、Restore、Clear 与 Finish 分别处理临时隔离、撤销、提交和冻结外清理。不能以 Trace 描述代替生命周期认证。

```c
typedef struct xrtownershipadapterv1 {
	size_t size;
	bool (*Hold)(const void* pData);
	void (*Drop)(const void* pData);
	bool (*Claim)(const void* pData, const void* pToken);
	void (*Restore)(const void* pData, const void* pToken);
	bool (*Finalize)(const void* pData, const void* pToken);
	void (*Clear)(const void* pData, const void* pToken);
	bool (*Finish)(const void* pData, const void* pToken);
} xrtownershipadapterv1;
```


### `xrtownershipadmitproc`

每个物理节点的显式准入回调。只认证调用方独立了解且驻留的操作表；不产生引用，也不授权卸载代码。

```c
typedef bool (*xrtownershipadmitproc)(xrtownershipref Reference, ptr pContext);
```


### `xrtownershipnode`

快照节点记录：Reference 为借用身份，StrongCount 为真实强计数，InternalCount 为内部拥有边计数，Reachable 为外部根可达性。

```c
typedef struct xrtownershipnode {
	xrtownershipref Reference;
	size_t StrongCount;
	size_t InternalCount;
	bool Reachable;
} xrtownershipnode;
```


### `xrtownershipops`

物理对象的计数、边枚举和诊断操作表。Count 与 Trace 必须在同一个全图静止点运行，借用字段不能伪装成拥有边。

```c
typedef struct xrtownershipops xrtownershipops;
```


### `xrtownershippreparationv1`

生命周期退休前的语义准备。准备在冻结外完成实际工作与回调；准备完成后必须重建并重新验证整个图。

```c
typedef struct xrtownershippreparationv1 {
	size_t size;
	const xrtownershipadapterv1* Adapter;
	bool (*Ready)(const void* pData);
	xrtownershipprepareresult (*Prepare)(const void* pData, const void* pToken);
} xrtownershippreparationv1;
```


### `xrtownershipprepareresult`

语义准备结果：区分完成、暂忙及失败；忙碌不授权跳过工作或强制清理。

```c
typedef enum xrtownershipprepareresult {
	XRT_OWNERSHIP_PREPARE_FAILED = -1,
	XRT_OWNERSHIP_PREPARE_BUSY = 0,
	XRT_OWNERSHIP_PREPARE_READY = 1
} xrtownershipprepareresult;
```


### `xrtownershipref`

借用物理身份，由 Data 和 Ops 共同定义。视图本身没有 Retain，不保活对象或代码。

```c
typedef struct xrtownershipref {
	const void* Data;
	const xrtownershipops* Ops;
} xrtownershipref;
```


### `xrtownershipresult`

发现和可达性统计。物理节点去重，拥有边保持 multiplicity；外部根不能通过猜测计数排除。

```c
typedef struct xrtownershipresult {
	size_t NodeCount;
	size_t EdgeCount;
	size_t ExternalRootCount;
	size_t ReachableAnchorCount;
} xrtownershipresult;
```


### `xrtownershiprootproc`

额外根策略回调，可以强制把域外原生资源作为根。它不授予节点的生命周期准入。

```c
typedef bool (*xrtownershiprootproc)(xrtownershipref Reference, ptr pContext);
```


### `xrtownershipscope`

零初始化后进入，在同一个原生线程上配对结束。活动作用域不可复制；冻结覆盖参与的完整变更，不能在冻结期间等待别的参与线程。

```c
typedef struct xrtownershipscope {
	struct xrtownershipscope* Self;
	struct xrtownershipscope* Parent;
	uint64 Thread;
	size_t Children;
	uint32 Mode;
} xrtownershipscope;
```


### `xrtownershipsnapshot`

拥有型快照账本；Destroy 只释放账本，不释放节点。快照里记录的引用、代码仍由调用方独立保活。

```c
typedef struct xrtownershipsnapshot xrtownershipsnapshot;
```


### `xrtownershiptrace`

枚举真实强引用槽的回调，保留重复槽，不能枚举只是借用的地址。

```c
typedef bool (*xrtownershiptrace)(const void* pData, xrtownershipvisitor pVisit, ptr pContext);
```


### `xrtownershipvisitor`

接收每个真实拥有槽的访问回调；它的失败必须沿 Trace 调用链传播。

```c
typedef bool (*xrtownershipvisitor)(xrtownershipref Reference, ptr pContext);
```
