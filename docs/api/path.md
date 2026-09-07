# Path API

Path 提供跨 POSIX/Windows 的路径词法操作：拼接、清理、相对化、分解与可移植性检查；纯词法，不访问文件系统。

## 类型与常量

### `xpathstyle`

路径风格决定根、分隔符和绝对路径语义，不读取目标文件系统。

```c
typedef enum xpathstyle {
	XPATH_NATIVE = 0,
	XPATH_POSIX,
	XPATH_WINDOWS
} xpathstyle;
```

| 值 | 语义 |
|---|---|
| `XPATH_NATIVE` | 本机风格 |
| `XPATH_POSIX` | POSIX 风格 |
| `XPATH_WINDOWS` | Windows 风格 |

### `xpathroot`

根类型明确区分 Windows 驱动器相对路径、根相对路径和完整绝对路径。

```c
typedef enum xpathroot {
	XPATH_ROOT_NONE = 0,
	XPATH_ROOT_POSIX,
	XPATH_ROOT_WINDOWS,
	XPATH_ROOT_DRIVE_RELATIVE,
	XPATH_ROOT_DRIVE,
	XPATH_ROOT_UNC,
	XPATH_ROOT_DEVICE
} xpathroot;
```

| 值 | 语义 |
|---|---|
| `XPATH_ROOT_NONE` | 无 |
| `XPATH_ROOT_POSIX` | POSIX 根 / |
| `XPATH_ROOT_WINDOWS` | 设备命名空间根 \\. |
| `XPATH_ROOT_DRIVE_RELATIVE` | 驱动器相对（如 C:foo） |
| `XPATH_ROOT_DRIVE` | 驱动器根（如 C:\） |
| `XPATH_ROOT_UNC` | UNC 根（\\server\share） |
| `XPATH_ROOT_DEVICE` | 设备命名空间根 |

### `xpathflag`

路径分解标志。

```c
typedef enum xpathflag {
	XPATH_FLAG_ROOTED = 0x01,
	XPATH_FLAG_ABSOLUTE = 0x02,
	XPATH_FLAG_TRAILING_SEPARATOR = 0x04
} xpathflag;
```

| 值 | 语义 |
|---|---|
| `XPATH_FLAG_ROOTED` | ROOTED |
| `XPATH_FLAG_ABSOLUTE` | ABSOLUTE |
| `XPATH_FLAG_TRAILING_SEPARATOR` | 保留尾分隔符 |

### `xpathparts`

全部字段都借用输入路径；Ext 包含前导点，隐藏文件名本身不算扩展名。

```c
typedef struct xpathparts {
	xstrview Root;
	xstrview Parent;
	xstrview Name;
	xstrview Stem;
	xstrview Ext;
	xpathroot RootKind;
	uint32 Flags;
} xpathparts;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Root` | `xstrview` | Root |
| `Parent` | `xstrview` | Parent |
| `Name` | `xstrview` | 名称 |
| `Stem` | `xstrview` | Stem |
| `Ext` | `xstrview` | Ext |
| `RootKind` | `xpathroot` | RootKind |
| `Flags` | `uint32` | 标志位 |

### `xpathcomponentkind`

路径组件类型；根、点、双点和普通名称保持明确语义。

```c
typedef enum xpathcomponentkind {
	XPATH_COMPONENT_ROOT = 1,
	XPATH_COMPONENT_CURRENT,
	XPATH_COMPONENT_PARENT,
	XPATH_COMPONENT_NORMAL
} xpathcomponentkind;
```

| 值 | 语义 |
|---|---|
| `XPATH_COMPONENT_ROOT` | ROOT |
| `XPATH_COMPONENT_CURRENT` | CURRENT |
| `XPATH_COMPONENT_PARENT` | PARENT |
| `XPATH_COMPONENT_NORMAL` | 常规段 |

### `xpathcomponent`

路径组件借用输入文本。

```c
typedef struct xpathcomponent {
	xstrview Text;
	xpathcomponentkind Kind;
} xpathcomponent;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Text` | `xstrview` | 文本视图 |
| `Kind` | `xpathcomponentkind` | 错误种类 |

### `xpathiter`

零分配路径组件迭代器；字段仅由路径 API 维护。

```c
typedef struct xpathiter {
	xstrview Path;
	size_t Position;
	size_t RootSize;
	xpathstyle Style;
	uint32 State;
} xpathiter;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Path` | `xstrview` | 路径 |
| `Position` | `size_t` | 位置 |
| `RootSize` | `size_t` | RootSize |
| `Style` | `xpathstyle` | 样式 |
| `State` | `uint32` | 状态 |

### `xpatherror`

路径模块稳定错误代码。

```c
typedef enum xpatherror {
	XPATH_ERROR_FORMAT = 1,
	XPATH_ERROR_OVERFLOW,
	XPATH_ERROR_ROOT,
	XPATH_ERROR_SYSTEM
} xpatherror;
```

| 值 | 语义 |
|---|---|
| `XPATH_ERROR_FORMAT` | 格式非法 |
| `XPATH_ERROR_OVERFLOW` | 溢出 |
| `XPATH_ERROR_ROOT` | 失败 |
| `XPATH_ERROR_SYSTEM` | 系统调用失败 |

### `xpathsafesegment`

固定存储只允许通过 Path Safe Segment API 访问。

```c
typedef union xpathsafesegment {
	uint64 Alignment;
	uint8 Storage[XPATH_SAFE_SEGMENT_STORAGE_SIZE];
} xpathsafesegment;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Alignment` | `uint64` | 对齐（二次幂） |

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XPATH_SAFE_SEGMENT_STORAGE_SIZE` | `40u` | 流式可移植路径段检查器使用固定存储，不分配内存。 |

## 设计契约

路径体系分成三个可独立裁剪的层次：

- `XRT_FEATURE_PATH`：纯词法解析、分解、拼接、清理和相对路径，不访问文件系统。
- `XRT_FEATURE_PATH_SYSTEM`：工作目录、绝对路径、用户目录、临时目录和程序位置。
- `XRT_FEATURE_PATH_SAFE`：归档条目和静态资源名称的跨平台 UTF-8 词法校验。

`path_system` 和 `path_safe` 都依赖 `path` 与 `unicode`。纯词法层只依赖字符串模块，因此 URL、归档、构建工具和跨平台协议可以明确选择 POSIX 或 Windows 语义，而不引入系统调用。

全部返回 `str` 的函数都返回拥有字符串，调用方使用 `xrtFree` 释放。`xpathparts` 中的视图全部借用输入，输入失效后不可继续使用。失败返回 `NULL` 或 `false` 并保留结构化错误；谓词返回 `false` 时，如果需要区分“不是目标类型”和“参数错误”，应检查 `xrtGetError()`。

路径文本优先使用 UTF-8。词法层只禁止嵌入零字节，不强制路径必须是有效 UTF-8，以便操作本机可表达但非 Unicode 的 POSIX 文件名；Windows 系统层严格转换 UTF-8/UTF-16；POSIX 系统查询返回操作系统原始路径字节，调用方需要知道当前文件名编码；安全条目层要求严格有效的 UTF-8。

## 路径风格

```c
typedef enum xpathstyle {
	XPATH_NATIVE = 0,
	XPATH_POSIX,
	XPATH_WINDOWS
} xpathstyle;
```

- `XPATH_NATIVE` 在 Windows 上等价于 `XPATH_WINDOWS`，其他平台等价于 `XPATH_POSIX`。
- `XPATH_POSIX` 只把 `/` 视为分隔符，根为 `/`。
- `XPATH_WINDOWS` 同时接受 `/` 与 `\`，输出统一使用 `\`。

显式风格只决定词法规则，不假装目标操作系统存在对应驱动器、UNC 共享或文件。需要操作系统解释路径时使用 `path_system`。

```c
char xrtPathSep(void);
char xrtPathListSep(void);
```

`xrtPathSep` 返回本机目录分隔符。`xrtPathListSep` 返回环境路径列表分隔符，Windows 为 `;`，POSIX 为 `:`。

## 根和分解

```c
typedef enum xpathroot {
	XPATH_ROOT_NONE = 0,
	XPATH_ROOT_POSIX,
	XPATH_ROOT_WINDOWS,
	XPATH_ROOT_DRIVE_RELATIVE,
	XPATH_ROOT_DRIVE,
	XPATH_ROOT_UNC,
	XPATH_ROOT_DEVICE
} xpathroot;

typedef enum xpathflag {
	XPATH_FLAG_ROOTED = 0x01,
	XPATH_FLAG_ABSOLUTE = 0x02,
	XPATH_FLAG_TRAILING_SEPARATOR = 0x04
} xpathflag;
```

Windows 根必须明确区分：

| 输入 | 根类型 | 带根 | 完整绝对 |
| --- | --- | --- | --- |
| `name` | `NONE` | 否 | 否 |
| `C:name` | `DRIVE_RELATIVE` | 是 | 否 |
| `\name` | `WINDOWS` | 是 | 否 |
| `C:\name` | `DRIVE` | 是 | 是 |
| `\\server\share\name` | `UNC` | 是 | 是 |
| `\\?\C:\name` | `DEVICE` | 是 | 是 |

`C:name` 依赖驱动器 C 的当前目录，`\name` 依赖当前驱动器，所以二者都不能被 `xrtPathIsAbs` 误判成完整绝对路径。

```c
typedef struct xpathparts {
	xstrview Root;
	xstrview Parent;
	xstrview Name;
	xstrview Stem;
	xstrview Ext;
	xpathroot RootKind;
	uint32 Flags;
} xpathparts;

bool xrtPathParse(xstrview Path, xpathstyle Style, xpathparts* pParts);
```

`xrtPathParse` 零分配地分解路径。失败不修改 `*pParts`。`Ext` 包含前导点；只按最后一个点分割扩展名；`.gitignore` 的 `Stem` 是完整名称，`Ext` 为空。尾部分隔符表示目录语法，不产生虚假的空名称。

```c
xpathparts Parts;

if ( xrtPathParse(XRT_STR_LITERAL("archive/data.tar.gz"),
	XPATH_POSIX, &Parts) ) {
	/* Name=data.tar.gz, Stem=data.tar, Ext=.gz */
}
```

需要逐段处理路径时，不必重新扫描或分配字符串：

```c
typedef enum xpathcomponentkind {
	XPATH_COMPONENT_ROOT = 1,
	XPATH_COMPONENT_CURRENT,
	XPATH_COMPONENT_PARENT,
	XPATH_COMPONENT_NORMAL
} xpathcomponentkind;

typedef struct xpathcomponent {
	xstrview Text;
	xpathcomponentkind Kind;
} xpathcomponent;

typedef struct xpathiter {
	xstrview Path;
	size_t Position;
	size_t RootSize;
	xpathstyle Style;
	uint32 State;
} xpathiter;

bool xrtPathIterInit(xpathiter* pIterator,
	xstrview Path, xpathstyle Style);
bool xrtPathNext(xpathiter* pIterator, xpathcomponent* pComponent);
```

迭代器先返回根，再依次返回 `.`、`..` 和普通名称；重复分隔符不会制造空组件。组件视图借用原输入，`xpathiter` 的公开字段只用于栈上保存状态，不由调用方修改。初始化失败不修改迭代器；`xrtPathNext` 返回 `false` 表示遍历结束，迭代器或输出参数无效时会设置统一参数/状态错误。

### `xrtPathParse`

按指定风格零分配地分解路径，成功后所有视图都借用输入。

```c
bool xrtPathParse(xstrview Path, xpathstyle Style, xpathparts* pParts)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Path` | 输入 | 借用 | 待分解路径 |
| `Style` | 输入 | — | 路径风格 |
| `pParts` | 输出 | 非空 | 接收分解结果，视图借用输入 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已分解 | — |
| `false` | 格式或参数非法 | `XERR_ARGUMENT` / `xrt.path` 错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法

#### 范例

[tour](../../examples/path/tour/main.c) · 零分配分解

```c
	if ( !xrtPathParse(SV("C:\\dir\\file.txt"), XPATH_WINDOWS,
			&Parts) ||
		(Parts.RootKind != XPATH_ROOT_DRIVE) ||
		(Parts.Root.Size != 3u) ||
		(memcmp(Parts.Root.Data, "C:\\", 3u) != 0) ||
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) == 0u) ||
		((Parts.Flags & XPATH_FLAG_ROOTED) == 0u) ||
		(Parts.Name.Size != 8u) ||
		(memcmp(Parts.Name.Data, "file.txt", 8u) != 0) ||
		(Parts.Stem.Size != 4u) ||
		(memcmp(Parts.Stem.Data, "file", 4u) != 0) ||
```

### `xrtPathIterInit`

初始化零分配路径组件迭代器，成功后迭代器借用输入。

```c
bool xrtPathIterInit(xpathiter* pIterator,
	xstrview Path, xpathstyle Style)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输出 | 非空 | 接收迭代器 |
| `Path` | 输入 | 借用 | 待遍历路径 |
| `Style` | 输入 | — | 路径风格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 格式或参数非法 | `XERR_ARGUMENT` / `xrt.path` 错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法

#### 范例

[basic](../../examples/path/basic/main.c) · 初始化迭代

```c
	if ( !xrtPathIterInit(&Iterator, xrtStrView(sJoined), XPATH_NATIVE) ) {
```

### `xrtPathNext`

返回下一个借用组件，遍历结束时返回 `false`。

```c
bool xrtPathNext(xpathiter* pIterator, xpathcomponent* pComponent)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIterator` | 输入/输出 | 已初始化 | 目标迭代器 |
| `pComponent` | 输出 | 非空 | 接收组件视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已产出组件 | — |
| `false` | 遍历结束 | 不设错误 |

#### 错误

- 无错误 — 遍历结束返回 `false` 且不设置错误；空句柄设置 `XERR_ARGUMENT`

#### 范例

[basic](../../examples/path/basic/main.c) · 逐段迭代

```c
	while ( xrtPathNext(&Iterator, &Component) ) {
```

### `xrtPathIsAbs`

判断本机路径是否完整绝对；Windows 驱动器相对和根相对路径返回 `false`。

```c
bool xrtPathIsAbs(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 本机路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否完整绝对 | 空句柄返回 `false` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/path/tour/main.c) · 完整绝对

```c
	if ( !xrtPathIsAbs("C:\\a") ||
		xrtPathIsAbs("a\\b") ||
		!xrtPathIsRoot("C:\\") ||
		xrtPathIsRoot("C:\\a") ||
		!xrtPathIsRooted("\\a") ||
		xrtPathIsRooted("a") ) {
```

### `xrtPathIsRoot`

判断本机路径词法上是否恰好为一个完整文件系统根。

```c
bool xrtPathIsRoot(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 本机路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否为根 | 空句柄返回 `false` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/path/tour/main.c) · 恰好为根

```c
		!xrtPathIsRoot("C:\\") ||
```

### `xrtPathIsRooted`

判断本机路径是否带根；Windows 的 `C:foo` 和 `\foo` 也属于带根路径。

```c
bool xrtPathIsRooted(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 本机路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否带根 | 空句柄返回 `false` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[tour](../../examples/path/tour/main.c) · 带根判断

```c
		!xrtPathIsRooted("\\a") ||
```

### `xrtPathIsLocal`

判断路径是否能被安全拼入任意基目录；只做词法检查，不解析符号链接。

```c
bool xrtPathIsLocal(xstrview Path, xpathstyle Style)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Path` | 输入 | 借用 | 待检查路径 |
| `Style` | 输入 | — | 路径风格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否相对且可拼接 | 格式非法时 `false` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法

#### 范例

[basic](../../examples/path/basic/main.c) · 可拼接判断

```c
		xrtPathIsLocal(xrtStrView(sJoined), XPATH_NATIVE) ? 1 : 0);
```

## 常用分解函数

```c
str xrtPathName(cstr sPath);
str xrtPathStem(cstr sPath);
str xrtPathExt(cstr sPath);
str xrtPathParent(cstr sPath);
bool xrtPathIsAbs(cstr sPath);
bool xrtPathIsRoot(cstr sPath);
bool xrtPathIsRooted(cstr sPath);
bool xrtPathIsLocal(xstrview Path, xpathstyle Style);
```

四个字符串函数按本机风格解析并返回拥有副本。没有对应字段时返回已分配的空字符串，不用静态空串制造所有权例外。`xrtPathIsAbs` 只接受完整绝对路径；`xrtPathIsRoot` 只接受 `/`、`C:\`、完整 UNC 根等完整文件系统根；`xrtPathIsRooted` 还接受 Windows 驱动器相对和根相对路径。路径谓词都是纯词法判断，不查询路径是否存在。

`xrtPathIsLocal` 判断路径能否安全拼入任意基目录：路径必须非空、不能带根，清理过程中的 `..` 不能越过起点。Windows 风格还拒绝驱动器语法、备用数据流和设备保留名。它适合先检查归档子路径、静态资源键和用户提供的相对名称；它只提供词法包含保证，目录中的符号链接或挂载点仍可能改变最终位置。

```c
str sName = xrtPathName("logs/server.log");
str sExt = xrtPathExt("logs/server.log");

xrtFree(sName);
xrtFree(sExt);
```

### `xrtPathName`

复制本机路径的末级名称，包含扩展名。

```c
str xrtPathName(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 本机路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[basic](../../examples/path/basic/main.c) · 名称分解

```c
	sName = xrtPathName(sJoined);
```

### `xrtPathStem`

复制本机路径的末级名称，不包含最后一个扩展名。

```c
str xrtPathStem(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 本机路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[basic](../../examples/path/basic/main.c) · 主干分解

```c
	sStem = xrtPathStem(sJoined);
```

### `xrtPathExt`

复制本机路径的最后一个扩展名，结果包含前导点。

```c
str xrtPathExt(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 本机路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[basic](../../examples/path/basic/main.c) · 扩展名分解

```c
	sExt = xrtPathExt(sJoined);
```

### `xrtPathParent`

复制本机路径的父路径；没有父路径时返回已分配的空字符串。

```c
str xrtPathParent(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 本机路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[tour](../../examples/path/tour/main.c) · 父路径

```c
	sParent = xrtPathParent("C:\\dir\\file.txt");
```

## 清理和拼接

```c
str xrtPathClean(xstrview Path, xpathstyle Style);
str xrtPathBuild(const xstrview* arrParts, size_t iCount, xpathstyle Style);
str xrtPathJoin(cstr sLeft, cstr sRight);
```

`xrtPathClean` 是纯词法操作：

- 合并重复分隔符。
- 删除 `.` 段。
- 在不越过根的前提下折叠 `..`。
- 保留相对路径开头无法折叠的 `..`。
- 空路径清理为 `.`。
- Windows 设备命名空间原样复制，不对其特殊语义做破坏性清理。

它不访问文件系统，不解析符号链接，不判断目标是否存在，也不改变大小写。

`xrtPathBuild` 拼接任意数量的路径并清理结果。后续带根项会替换之前内容；Windows 根相对项（`\name`）会保留左侧已有的驱动器或 UNC 卷前缀。驱动器相对前缀 `C:` 与普通名称拼接后仍是 `C:name`，不会被错误升级为 `C:\name`。零项结果为 `.`。`xrtPathJoin` 是按本机风格拼接两个零结尾字符串的常用 Helper。三者都使用动态容量，没有旧版 4094 字节限制；构建结果会原地清理，常见拼接只需要一块动态缓冲。

```c
xstrview arrParts[] = {
	XRT_STR_LITERAL("var"),
	XRT_STR_LITERAL("cache"),
	XRT_STR_LITERAL("../run/app.pid")
};
str sPath = xrtPathBuild(arrParts, 3, XPATH_POSIX);

/* sPath == "var/run/app.pid" */
xrtFree(sPath);
```

### `xrtPathJoin`

按本机风格拼接两个路径；Windows 根相对右项保留已有卷前缀。

```c
str xrtPathJoin(cstr sLeft, cstr sRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sLeft` | 输入 | 非空、零结尾 | 左项 |
| `sRight` | 输入 | 非空、零结尾 | 右项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[basic](../../examples/path/basic/main.c) · 两段拼接

```c
	sJoined = xrtPathJoin("project", "src/../include/xrt.h");
```

### `xrtPathBuild`

按指定风格拼接并清理；Windows 根相对项保留已有卷前缀。

```c
str xrtPathBuild(const xstrview* arrParts, size_t iCount, xpathstyle Style)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `arrParts` | 输入 | 非空数组 | 路径分段视图 |
| `iCount` | 输入 | > 0 | 分段数量 |
| `Style` | 输入 | — | 目标路径风格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法
- `XERR_MEMORY` — 结果分配失败
- `XERR_RANGE` — 结果长度引起尺寸溢出

#### 范例

[tour](../../examples/path/tour/main.c) · 多段拼接

```c
	sBuilt = xrtPathBuild(arrParts, 3u, XPATH_WINDOWS);
```

### `xrtPathClean`

纯词法清理分隔符、点和双点段，不访问文件系统或解析符号链接。

```c
str xrtPathClean(xstrview Path, xpathstyle Style)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Path` | 输入 | 借用 | 待清理路径 |
| `Style` | 输入 | — | 路径风格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[tour](../../examples/path/tour/main.c) · 词法清理

```c
	sClean = xrtPathClean(SV("C:\\a\\..\\b\\\\c\\"), XPATH_WINDOWS);
```

### `xrtPathSep`

返回本机路径分隔符。

```c
char xrtPathSep(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 分隔字符 | Windows 为 `\\`，POSIX 为 `/` | — |

#### 错误

- 无 — 纯常量查询

#### 范例

[tour](../../examples/path/tour/main.c) · 分隔符

```c
	printf("path: sep=%c list=%c", xrtPathSep(),
		xrtPathListSep());
```

### `xrtPathListSep`

返回本机路径列表分隔符，Windows 为分号，POSIX 为冒号。

```c
char xrtPathListSep(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 分隔字符 | Windows 为 `;`，POSIX 为 `:` | — |

#### 错误

- 无 — 纯常量查询

#### 范例

[tour](../../examples/path/tour/main.c) · 列表分隔符

```c
		xrtPathListSep());
```

## 相对路径

```c
str xrtPathRelative(xstrview Base, xstrview Target, xpathstyle Style);
str xrtPathRel(cstr sBase, cstr sTarget);
```

`xrtPathRelative` 把 `Base` 当作目录，纯词法计算到 `Target` 的相对路径。两端先清理；根类型或根值不同时返回 `XPATH_ERROR_ROOT`。相同路径返回 `.`。

Windows 风格的根和路径段按 ASCII 大小写不敏感比较，POSIX 风格逐字节比较。该规则只表达常规 Windows 词法语义，不查询单个目录是否启用了大小写敏感属性。

相对输入开头无法折叠的 `..` 表示未知父目录。若 `Base` 比 `Target` 退得更深，例如从 `../../a` 到 `../b`，纯词法信息不足以写出正确结果，函数返回 `XPATH_ERROR_ROOT`，而不是生成一个无法满足重新拼接逆关系的字符串。Windows 设备命名空间会关闭普通点段解析，因此也不参与普通相对路径计算。

`xrtPathRel` 属于系统层，先按操作系统规则把两端转成绝对路径，再调用纯词法原语。它同样不解析符号链接，因此比较的是路径表达，不是文件身份。

```c
str sRel = xrtPathRelative(XRT_STR_LITERAL("/srv/app"),
	XRT_STR_LITERAL("/srv/data/file"), XPATH_POSIX);

/* sRel == "../data/file" */
xrtFree(sRel);
```

### `xrtPathRelative`

纯词法计算从目录 `Base` 到 `Target` 的相对路径；根不同时报错。

```c
str xrtPathRelative(xstrview Base, xstrview Target, xpathstyle Style)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Base` | 输入 | 借用 | 基目录 |
| `Target` | 输入 | 借用 | 目标路径 |
| `Style` | 输入 | — | 路径风格 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法
- `XERR_MEMORY` — 结果分配失败
- `xrt.path` / `XPATH_ERROR_ROOT`（`XERR_VALUE`） — 两路径根不同
- `xrt.path` / `XPATH_ERROR_ROOT`（`XERR_UNSUPPORTED`） — Windows 跨卷相对化不可表达

#### 范例

[tour](../../examples/path/tour/main.c) · 词法相对化

```c
	sRel = xrtPathRelative(SV("C:\\a\\b"), SV("C:\\a\\c\\d"),
		XPATH_WINDOWS);
```

### `xrtPathRel`

把两个路径转为绝对路径后计算从 `Base` 到 `Target` 的相对路径。

```c
str xrtPathRel(cstr sBase, cstr sTarget)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sBase` | 输入 | 非空、零结尾 | 基目录 |
| `sTarget` | 输入 | 非空、零结尾 | 目标路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[tour](../../examples/path/tour/main.c) · 绝对化相对化

```c
		str sRelSys = xrtPathRel("C:\\a\\b", "C:\\a\\c\\d");
```

## 修改名称

```c
str xrtPathWithName(cstr sPath, cstr sName);
str xrtPathWithExt(cstr sPath, cstr sExtension);
```

`xrtPathWithName` 替换末级名称，并允许新名称包含相对路径段；新名称带根时按 `xrtPathBuild` 规则替换旧父路径。`xrtPathWithExt` 只替换最后一个扩展名，非空扩展名可省略前导点，空扩展名删除扩展名；扩展名不得包含路径分隔符。

```c
str sPath = xrtPathWithExt("archive/data.tar.gz", "zip");

/* sPath == "archive/data.tar.zip" */
xrtFree(sPath);
```

### `xrtPathWithName`

替换本机路径的末级名称；新名称可以包含相对路径段。

```c
str xrtPathWithName(cstr sPath, cstr sName)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 原路径 |
| `sName` | 输入 | 非空、零结尾 | 新末级名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法
- `XERR_MEMORY` — 结果分配失败

#### 范例

[basic](../../examples/path/basic/main.c) · 替换名称

```c
	sRenamed = xrtPathWithName(sJoined, "runtime.h");
```

### `xrtPathWithExt`

替换最后一个扩展名；空扩展名删除扩展名，非空值可省略前导点。

```c
str xrtPathWithExt(cstr sPath, cstr sExtension)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 原路径 |
| `sExtension` | 输入 | 非空或空串 | 新扩展名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的结果 | — |
| `NULL` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_FORMAT`（`XERR_VALUE`） — 路径格式非法
- `XERR_MEMORY` — 结果分配失败
- `XERR_RANGE` — 结果长度引起尺寸溢出

#### 范例

[tour](../../examples/path/tour/main.c) · 替换扩展名

```c
	sExt = xrtPathWithExt("C:\\dir\\file", ".md");
```

## 系统路径

启用 `XRT_FEATURE_PATH_SYSTEM` 后提供：

```c
str xrtPathCwd(void);
bool xrtPathSetCwd(cstr sPath);
str xrtPathAbs(cstr sPath);
str xrtPathReal(cstr sPath);
str xrtPathRel(cstr sBase, cstr sTarget);
str xrtPathHome(void);
str xrtPathTemp(void);
str xrtPathExecutable(void);
str xrtPathAppDir(void);
```

- `xrtPathCwd` 动态读取当前工作目录，不依赖 `MAX_PATH` 或 `PATH_MAX`。
- `xrtPathSetCwd` 修改进程级当前目录，会影响其他线程，库代码不应把它当作局部状态。
- `xrtPathAbs` 使用操作系统规则解释驱动器相对等本机路径，但不要求目标存在；空路径表示当前工作目录。
- `xrtPathReal` 要求目标存在，跟随符号链接和 Windows 重解析点，返回操作系统确认的物理绝对路径。Windows 结果可能保留 `\\?\` 设备前缀；POSIX 结果由 `realpath` 解析。
- `xrtPathHome` 返回当前用户目录，不用当前目录伪装缺失结果。
- `xrtPathTemp` 返回系统临时目录；它不是一个已经安全创建的临时对象。
- `xrtPathExecutable` 返回当前可执行文件的绝对 UTF-8 路径。
- `xrtPathAppDir` 返回可执行文件所在目录，不等同于当前工作目录。

Windows 系统调用使用宽字符 API 和严格 UTF-8 转换。动态查询不使用固定长度缓冲。系统查询失败会在错误中保留原始 Win32 或 `errno` 代码。

`xrtPathReal` 适合比较已存在对象的物理位置、诊断链接和做目录树操作的前置校验，但它本身不是安全沙箱：查询完成后路径仍可能被其他线程或进程替换。需要抵抗恶意并发替换时，应使用后续文件层的目录句柄相对操作和禁止跟随链接选项。

### `xrtPathCwd`

返回当前工作目录的绝对 UTF-8 路径。

```c
str xrtPathCwd(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败

#### 范例

[system](../../examples/path/system/main.c) · 工作目录

```c
	sCwd = xrtPathCwd();
```

### `xrtPathSetCwd`

修改进程当前工作目录；该操作影响进程内其他线程。

```c
bool xrtPathSetCwd(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 新工作目录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已切换 | — |
| `false` | 失败 | `xrt.path` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败

#### 范例

[tour](../../examples/path/tour/main.c) · 切换目录

```c
	if ( !xrtPathSetCwd(sCwd) ) {  /* 切到当前目录（等价还原） */
```

### `xrtPathAbs`

使用操作系统规则返回绝对路径；空路径表示当前工作目录。

```c
str xrtPathAbs(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 允许空串 | 待绝对化路径，空 = 当前目录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败
- `XERR_MEMORY` — 结果分配失败

#### 范例

[tour](../../examples/path/tour/main.c) · 绝对化

```c
	sAbs = xrtPathAbs(".");
```

### `xrtPathReal`

返回已存在路径跟随符号链接后的物理绝对路径。

```c
str xrtPathReal(cstr sPath)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、零结尾 | 已存在的路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败
- `XERR_MEMORY` — 结果分配失败

#### 范例

[system](../../examples/path/system/main.c) · 物理路径

```c
	sReal = xrtPathReal(".");
```

### `xrtPathHome`

返回当前用户主目录。

```c
str xrtPathHome(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败

#### 范例

[system](../../examples/path/system/main.c) · 主目录

```c
	sHome = xrtPathHome();
```

### `xrtPathTemp`

返回操作系统临时目录。

```c
str xrtPathTemp(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败

#### 范例

[system](../../examples/path/system/main.c) · 临时目录

```c
	sTemp = xrtPathTemp();
```

### `xrtPathExecutable`

返回当前可执行文件的绝对 UTF-8 路径。

```c
str xrtPathExecutable(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败

#### 范例

[tour](../../examples/path/tour/main.c) · 可执行文件

```c
	sExe = xrtPathExecutable();
```

### `xrtPathAppDir`

返回当前可执行文件所在目录。

```c
str xrtPathAppDir(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 由 `xrtFree` 释放的路径 | — |
| `NULL` | 失败 | `xrt.path` 域错误 |

#### 错误

- `xrt.path` / `XPATH_ERROR_SYSTEM` — 平台调用失败，kind 由系统错误码映射并保留系统码
- `XERR_NOT_FOUND`（`XPATH_ERROR_SYSTEM`） — 主目录环境变量缺失等查询失败

#### 范例

[system](../../examples/path/system/main.c) · 应用目录

```c
	sApp = xrtPathAppDir();
```

## 安全条目

需要在 percent 解码、归档流或其他分块输入中逐段检查时，可以直接使用同一套
可移植段状态机：

```c
#define XPATH_SAFE_SEGMENT_STORAGE_SIZE 40u

typedef union xpathsafesegment {
	uint64 Alignment;
	uint8 Storage[XPATH_SAFE_SEGMENT_STORAGE_SIZE];
} xpathsafesegment;

void xrtPathSafeSegmentInit(xpathsafesegment* pState);
bool xrtPathSafeSegmentFeed(xpathsafesegment* pState, uint8 iValue);
bool xrtPathSafeSegmentFinish(const xpathsafesegment* pState);
```

状态机不分配内存，`Feed` 接收已经解码的字节，`Finish` 统一检查空段、点段、
尾随点或空格和 Windows 设备保留名。UTF-8 完整性和路径分隔符仍由外层流解析器
处理；因此它是段级原语，不替代完整入口检查。

```c
bool xrtPathIsSafeEntry(xstrview Path, bool bDirectory);
```

该函数检查一个归档、包或静态资源条目是否为跨 Windows/POSIX 可移植的相对 UTF-8 路径。它拒绝：

- 空路径、绝对路径、驱动器路径、反斜杠和空段。
- `.`、`..` 与父目录穿越。
- 嵌入零字节、无效 UTF-8、控制字符和 Windows 禁用字符。
- 尾随点或空格。
- `CON`、`NUL`、`COM1`、`LPT1` 等 Windows 设备保留名及其扩展名形式。

`bDirectory` 为 `true` 时允许末尾 `/`，但不要求必须带末尾 `/`；为 `false` 时末尾 `/` 被拒绝。

```c
bool bImage = xrtPathIsSafeEntry(
	XRT_STR_LITERAL("assets/icons/app.png"), false);
bool bTraversal = xrtPathIsSafeEntry(
	XRT_STR_LITERAL("../secret.txt"), false);
```

这是词法入口检查，不是完整文件系统沙箱。攻击者仍可能借助目标目录内已有符号链接或挂载点逃逸。安全解包和静态文件服务还必须使用后续文件模块提供的“相对可信目录句柄打开、禁止跟随符号链接、验证最终对象”能力。

### `xrtPathSafeSegmentInit`

初始化一个可跨任意输入分块复用的可移植路径段检查器。

```c
void xrtPathSafeSegmentInit(xpathsafesegment* pState)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输出 | 非空 | 接收检查器状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[tour](../../examples/path/tour/main.c) · 初始化检查器

```c
	xrtPathSafeSegmentInit(&Safe);
```

### `xrtPathSafeSegmentFeed`

加入一个已解码字节；一旦确定非法便返回 `false`。

```c
bool xrtPathSafeSegmentFeed(
	xpathsafesegment* pState,
	uint8 iValue
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入/输出 | 已初始化 | 检查器状态 |
| `iValue` | 输入 | — | 已解码 UTF-8 字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 字节仍合法，可继续 | — |
| `false` | 段已确定非法 | 不设错误 |

#### 错误

- 无 — 返回 `false` 表示检查不通过，不设置错误

#### 范例

[tour](../../examples/path/tour/main.c) · 逐字节检查

```c
			if ( !xrtPathSafeSegmentFeed(&Safe,
					(uint8)sCheck[i]) ) {
```

### `xrtPathSafeSegmentFinish`

完成空段、点段、尾部规则和 Windows 设备保留名检查。

```c
bool xrtPathSafeSegmentFinish(
	const xpathsafesegment* pState
)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入 | 已初始化且已喂入完整段 | 检查器状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 段可移植安全 | — |
| `false` | 段不安全 | 不设错误 |

#### 错误

- 无 — 返回 `false` 表示检查不通过，不设置错误

#### 范例

[tour](../../examples/path/tour/main.c) · 完成检查

```c
		if ( !xrtPathSafeSegmentFinish(&Safe) ) {
```

### `xrtPathIsSafeEntry`

检查归档条目是否为跨 Windows/POSIX 可移植的 UTF-8 相对路径。

```c
bool xrtPathIsSafeEntry(xstrview Path, bool bDirectory)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Path` | 输入 | 借用 | 条目路径 |
| `bDirectory` | 输入 | — | 是否目录条目（允许尾分隔符） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 条目安全 | — |
| `false` | 绝对路径、空、非 UTF-8 或含不安全段 | 不设错误 |

#### 错误

- 无 — 返回 `false` 表示检查不通过，不设置错误

#### 范例

[safe](../../examples/path/safe/main.c) · 条目检查

```c
			xrtPathIsSafeEntry(xrtStrView(arrEntries[i]), false) ?
```

## 错误

```c
typedef enum xpatherror {
	XPATH_ERROR_FORMAT = 1,
	XPATH_ERROR_OVERFLOW,
	XPATH_ERROR_ROOT,
	XPATH_ERROR_SYSTEM
} xpatherror;
```

路径错误域为 `xrt.path`：

- `FORMAT`：嵌入零字节、无末级名称或非法扩展名等格式错误。
- `OVERFLOW`：路径长度或容量计算不能表达。
- `ROOT`：相对路径两端根不兼容。
- `SYSTEM`：系统查询失败，`SystemCode` 保存平台错误码。

内存分配失败继续使用统一的 `XERR_MEMORY`，参数错误使用统一参数错误，不重复制造路径私有错误类别。

## 旧版迁移

| 旧版能力 | 新 API | 处理 |
| --- | --- | --- |
| `xrtPathParse` | `xrtPathParse` | 保留名称，改为零分配视图和明确根类型 |
| `xrtPathJoin` | `xrtPathJoin` / `xrtPathBuild` | 移除固定缓冲，增加多段与显式风格 |
| `xrtPathNormalize` | `xrtPathClean` | 明确为纯词法清理 |
| `xrtPathAbs` | `xrtPathAbs` | 动态系统查询，严格错误和 UTF 转换 |
| `xrtPathRelative` | `xrtPathRelative` / `xrtPathRel` | 拆分纯词法和系统绝对化两层 |
| `xrtPathGetNameExt` | `xrtPathName` | 使用简短统一命名 |
| `xrtPathGetName` | `xrtPathStem` | 避免“Name 是否含扩展名”的歧义 |
| `xrtPathGetExt` | `xrtPathExt` | 扩展名包含前导点 |
| `xrtPathGetDir` | `xrtPathParent` | 使用明确的父路径语义 |
| `xrtPathIsSafeArchive` | `xrtPathIsSafeEntry` | 补齐 UTF-8、设备名和可移植字符边界 |
| `xrtPathRandom` | 后续原子临时文件/目录 API | 删除只生成名称却承诺唯一性的 TOCTOU 设计 |

旧版 `xrtPathRandom` 不进入路径层。安全临时资源必须由文件层一次系统调用或带排他创建循环完成“选名并创建”，不能先猜一个不存在的路径再打开。

## 完整示例

- `examples/path/basic/main.c`：分解、修改名称和扩展名。
- `examples/path/system/main.c`：当前目录、用户目录、临时目录和程序目录。
- `examples/path/safe/main.c`：归档与静态资源入口校验。
