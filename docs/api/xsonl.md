# XSONL

XSONL 将逐行记录序列与 xvalue Array 相互转换，每个元素对应一条记录；单条值沿用 XSON 的类型和编解码规则。适用于日志、批量导入和文件交换。

## 裁剪与依赖

| 公开选择宏 | 实现宏 | 直接依赖 |
|---|---|---|
| `XRT_MODULE_XSONL_CORE` | `XRT_FEATURE_XSONL_CORE` | core |
| `XRT_MODULE_XSONL_READ` | `XRT_FEATURE_XSONL_READ` | xsonl_core、xson_read |
| `XRT_MODULE_XSONL_WRITE` | `XRT_FEATURE_XSONL_WRITE` | xsonl_core、xson_write |
| `XRT_MODULE_XSONL_FILE` | `XRT_FEATURE_XSONL_FILE` | xsonl_read、xsonl_write、file_whole |
| `XRT_MODULE_XSONL` | `XRT_FEATURE_XSONL` | xsonl_file |

头文件为 `<xrt/xsonl.h>`，也可经总头 `<xrt.h>` 引入。单独选择读取或写出不引入文件模块。

## 稳定契约

- 默认忽略空白行：空行，以及仅含 ASCII 空格、Tab、CR 的行。接受 LF、CRLF 及混用；单独 CR 不作为记录分隔符。Unicode 空白不会自动变为空行。
- 空输入、全部为空白行都返回空 Array。严格空行模式仍接受零字节输入与最后一条记录的单个终止换行；额外空行报错。
- 一行必须恰好包含一个完整值；允许最后一条完整记录没有终止换行。跨行值、同一行多个根值、BOM、非法 UTF-8、残缺尾记录均失败。
- 字符串中转义的换行不分割记录。数组记录保留嵌套，例如文本 `[]\n` 得到包含一个空数组的 Array。
- 反序列化只在全部记录成功后移交拥有的 Array；失败返回 C NULL，释放全部部分结果。
- 序列化根值必须是 `XVALUE_ARRAY`；null、空字符串、空容器都是实际记录。SKIP 策略不能跳过整条记录，只作用于单条根值内部的成员。
- 输出紧凑，每条记录追加 LF，空 Array 输出零字节。PRETTY 配置被拒绝。配置按值快照，回调修改原配置不影响进行中的调用。
- `Valid` 沿用 XSON 的无 DOM 语法验证：忽略空白行，检查默认累计预算和内建标签，不执行重复键 DOM 策略。
- 文件写入先完成内存序列化，再原子替换；序列化失败不修改目标。同步回调写出允许失败前已提交部分记录，其字节不能撤回。
- 单条注释和尾随逗号仅在 Record 中显式开启，属于兼容扩展，不得跨物理行延续。
- XSONL 支持 bytes、time、set、intmap、非有限 float 标签以及显式自定义编解码回调；自定义回调创建的对象和副作用由调用方负责，累计值预算计输入语法值，不遍历回调创建的对象；累计解码预算只计内建 bytes 标签。

线程与所有权：API 不带隐式锁，各次调用拥有独立工作区；同一可变输入由调用方同步。写出通过外层 Array 的 backing 快照迭代，回调期间仍须遵守 Value 子树的所有权约定。输入文本/配置借用到调用返回，结果 Array 使用 `xrtValueRelease` 释放，结果字符串使用 `xrtFree` 释放。输出回调在返回前消费字节。

## 常量

| 常量 | 值 | 语义 |
|---|---|---|
| `XXSONL_READ_REJECT_EMPTY_LINES` | `UINT32_C(0x00000001)` | 空白行报错，默认关闭。 |

配置必须经 Init 初始化；所有预算非零。Record 的默认值由现有 XSON 初始化函数提供。整体输入/输出默认 64 MiB，记录数与语法值数默认 1000000。内建 bytes 累计解码预算默认 64 MiB。

## 类型

### `xxsonlerror`

```c
typedef enum xxsonlerror {
	XXSONL_ERROR_CONFIG = 1801,
	XXSONL_ERROR_SYNTAX,
	XXSONL_ERROR_LIMIT,
	XXSONL_ERROR_RECORD,
	XXSONL_ERROR_TYPE,
	XXSONL_ERROR_OUTPUT,
	XXSONL_ERROR_IO,
	XXSONL_ERROR_STATE
} xxsonlerror;
```

错误经 `xrtGetError()` 取得，格式域为 `xrt.xsonl`；参数与底层 OOM 也可直接返回 Core 错误。RECORD/OUTPUT/IO 包装保留 Cause 的 Kind。若包装自身分配失败，保留原始原因错误，不承诺定位数据。

| 值 | 语义 |
|---|---|
| `XXSONL_ERROR_CONFIG` | 配置未初始化、保留位非零、预算为零或启用 PRETTY。 |
| `XXSONL_ERROR_SYNTAX` | 严格空行模式下遇到空白行。 |
| `XXSONL_ERROR_LIMIT` | 整体或单条预算耗尽。 |
| `XXSONL_ERROR_RECORD` | 单条值编解码或加入 Array 失败；Cause 保留具体原因。 |
| `XXSONL_ERROR_TYPE` | 写出根值不是 Array。 |
| `XXSONL_ERROR_OUTPUT` | 同步输出回调失败。 |
| `XXSONL_ERROR_IO` | 限额文件读取或原子文件替换失败。 |
| `XXSONL_ERROR_STATE` | 逐行处理状态错误的保留代码。 |

### `xxsonllocation`

```c
typedef struct xxsonllocation {
	size_t Offset;
	size_t Line;
	size_t Column;
	size_t RecordIndex;
} xxsonllocation;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `size_t` | 原始输入中的零基字节偏移。 |
| `Line` | `size_t` | 按 LF 分隔的一基物理行号，CRLF 计一行，忽略的空白行仍计数。 |
| `Column` | `size_t` | 当前物理行内一基 UTF-8 字节列，不按 Unicode 字符计数。 |
| `RecordIndex` | `size_t` | 零基记录下标；空白行不递增，空行错误指向下一待接收记录下标。 |

### `xxsonlreadflag`

```c
typedef enum xxsonlreadflag {
	XXSONL_READ_REJECT_EMPTY_LINES = UINT32_C(0x00000001)
} xxsonlreadflag;
```

| 值 | 语义 |
|---|---|
| `XXSONL_READ_REJECT_EMPTY_LINES` | 将默认忽略的空白行改为语法错误。 |

### `xxsonlreadconfig`

```c
typedef struct xxsonlreadconfig {
	xxsonreadconfig Record;
	uint32 Flags;
	size_t MaxInputBytes;
	size_t MaxRecords;
	size_t MaxTotalValues;
	size_t MaxTotalDecodedBytes;
	uint32 Reserved[4];
} xxsonlreadconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Record` | `xxsonreadconfig` | 单条配置；MaxInputBytes/MaxOutputBytes 不包含 CRLF/LF 分隔符，深度从每条根值开始计算。 |
| `Flags` | `uint32` | 默认 0；XXSONL_READ_REJECT_EMPTY_LINES 启用空白行报错。 |
| `MaxInputBytes` | `size_t` | 整段原始输入上限，包含被忽略的空白行和所有分隔符；默认 64 MiB。 |
| `MaxRecords` | `size_t` | 非空记录数量上限，等于结果 Array 最大元素数；默认 1000000。 |
| `MaxTotalValues` | `size_t` | 所有记录的语法值累计上限，包含被重复键策略丢弃的值，不计合成 Array；默认 1000000。 |
| `MaxTotalDecodedBytes` | `size_t` | 内建 bytes 标签累计解码字节上限，检查在缓冲分配前；默认 64 MiB。 |
| `Reserved` | `uint32[4]` | 保留空间，必须全部为零。 |

### `xxsonlwriteconfig`

```c
typedef struct xxsonlwriteconfig {
	xxsonwriteconfig Record;
	size_t MaxOutputBytes;
	size_t MaxRecords;
	uint32 Reserved[4];
} xxsonlwriteconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Record` | `xxsonwriteconfig` | 单条配置；MaxInputBytes/MaxOutputBytes 不包含 CRLF/LF 分隔符，深度从每条根值开始计算。 |
| `MaxOutputBytes` | `size_t` | 全部输出字节上限，包含每条 LF，不包含结果末尾 NUL；默认 64 MiB。 |
| `MaxRecords` | `size_t` | 非空记录数量上限，等于结果 Array 最大元素数；默认 1000000。 |
| `Reserved` | `uint32[4]` | 保留空间，必须全部为零。 |

## 文本、配置与文件接口

### `xrtXsonlErrorLocation`

读取全局字节偏移、一基物理行列及零基记录下标；无位置时保持输出不变。

```c
bool xrtXsonlErrorLocation(
	const xerror* pError,
	xxsonllocation* pLocation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 允许为空；只读借用 | 从本格式错误中读取位置；无位置或其他错误域返回 false。 |
| `pLocation` | 输出 | 非空 | 成功填入全局位置；失败保持内容不变。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| true | 已读取位置。 | — |
| false | 无位置或参数非法。 | 位置输出不变；无位置本身不设置错误。 |

#### 错误

- `XERR_ARGUMENT` — 位置输出为空。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlErrorLocation 调用，失败转入统一清理。

```c
if ( !xrtXsonlErrorLocation(xrtGetError(), &Location) ) goto done;
```

### `xrtXsonlReadConfigInit`

初始化默认忽略空白行、严格单条语法和有限累计预算。

```c
void xrtXsonlReadConfigInit(
	xxsonlreadconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 设置默认值并清零保留字段。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| void | 完成初始化。 | 空指针时设置参数错误。 |

#### 错误

- `XERR_ARGUMENT` — 配置输出为空。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlReadConfigInit 调用，失败转入统一清理。

```c
xrtXsonlReadConfigInit(&Read);
```

### `xrtXsonlParse`

使用默认配置解析记录序列；成功返回拥有的 Array，空输入返回空 Array。

```c
xvalue* xrtXsonlParse(
	xstrview Text
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 显式字节长度；仅零长度可使用 NULL Data | 完整文本借用到调用结束，非 NUL 结尾文本也可输入。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有的 Array，调用方释放。 | — |
| NULL | 参数、预算、语法或资源错误。 | 不返回部分结果；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_PROTOCOL` — 非空行不是完整合法值，或严格模式遇到空白行；单条错误保留底层原因链。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlParse 调用，失败转入统一清理。

```c
pArray = xrtXsonlParse(XRT_STR_LITERAL("{\"id\":1}\n\n[2,3]\r\nnull\n"));
if ( pArray == NULL ) goto done;
```

### `xrtXsonlRead`

按配置解析全部记录；失败释放部分结果并返回 NULL，结果由 xrtValueRelease 释放。

```c
xvalue* xrtXsonlRead(
	xstrview Text,
	const xxsonlreadconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 显式字节长度；仅零长度可使用 NULL Data | 完整文本借用到调用结束，非 NUL 结尾文本也可输入。 |
| `pConfig` | 输入 | 非空；必须已初始化 | 单条配置和累计预算；本次调用不修改来源。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有的 Array，调用方释放。 | — |
| NULL | 参数、预算、语法或资源错误。 | 不返回部分结果；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_PROTOCOL` — 非空行不是完整合法值，或严格模式遇到空白行；单条错误保留底层原因链。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlRead 调用，失败转入统一清理。

```c
pRead = xrtXsonlRead((xstrview){ Text, Size }, &Read);
if ( pRead == NULL ) goto done;
```

### `xrtXsonlValid`

默认忽略空白行，验证逐行语法和累计预算，不构造 Value DOM；重复键策略不参与验证。

```c
bool xrtXsonlValid(
	xstrview Text
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 显式字节长度；仅零长度可使用 NULL Data | 完整文本借用到调用结束，非 NUL 结尾文本也可输入。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| true | 全部非空行通过默认语法与预算检查。 | — |
| false | 操作失败。 | 不返回部分结果；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_PROTOCOL` — 非空行不是完整合法值，或严格模式遇到空白行；单条错误保留底层原因链。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlValid 调用，失败转入统一清理。

```c
if ( !xrtXsonlValid((xstrview){ Text, Size }) ) goto done;
```

### `xrtXsonlWriteConfigInit`

初始化紧凑单行输出、LF 分隔及有限累计预算；PRETTY 配置非法。

```c
void xrtXsonlWriteConfigInit(
	xxsonlwriteconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 设置默认值并清零保留字段。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| void | 完成初始化。 | 空指针时设置参数错误。 |

#### 错误

- `XERR_ARGUMENT` — 配置输出为空。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlWriteConfigInit 调用，失败转入统一清理。

```c
xrtXsonlWriteConfigInit(&Write);
```

### `xrtXsonlStringify`

每个 Array 元素写成一行；返回 xrtFree 释放的 NUL 结尾文本，失败不修改可空的 pSize。

```c
str xrtXsonlStringify(
	const xvalue* pArray,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且类型为 XVALUE_ARRAY | 序列化各元素；调用不消费调用方拥有引用。 |
| `pSize` | 输出 | 允许为空 | 成功写入字节数，含 LF、不含 NUL；失败不修改。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有的 NUL 结尾文本，空输出仍返回可释放字符串。 | — |
| NULL | 配置、类型、编码或资源错误。 | pSize 不变；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_TYPE` — 序列化根值不是 Array。
- `XERR_UNSUPPORTED` — 单条值不能由所选格式编码。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlStringify 调用，失败转入统一清理。

```c
Text = xrtXsonlStringify(pArray, &Size);
if ( Text == NULL ) goto done;
```

### `xrtXsonlWrite`

同步分块输出各条记录及 LF；回调借用字节仅在调用期间有效，失败不能撤回已提交字节。

```c
bool xrtXsonlWrite(
	const xvalue* pArray,
	const xxsonlwriteconfig* pConfig,
	xxsonwriteproc pWrite,
	ptr pUserData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pArray` | 输入 | 非空且类型为 XVALUE_ARRAY | 序列化各元素；调用不消费调用方拥有引用。 |
| `pConfig` | 输入 | 非空；必须已初始化 | 单条配置和累计预算；本次调用不修改来源。 |
| `pWrite` | 输入 | 非空同步回调 | 分块消费借用字节，返回 false 终止输出。 |
| `pUserData` | 输入 | 允许为空 | 原样传递给同步输出回调。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| true | 全部输出完成。 | — |
| false | 操作失败。 | 输入引用仍归调用方；已提交字节无法撤回，错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_TYPE` — 序列化根值不是 Array。
- `XERR_UNSUPPORTED` — 单条值不能由所选格式编码。
- `XERR_IO` — 文件操作或未提供具体错误的输出回调失败；底层原因的其他 Kind 原样保留。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlWrite 调用，失败转入统一清理。

```c
if ( !xrtXsonlWrite(pArray, &Write, discard, NULL) ) goto done;
```

### `xrtXsonlParseFile`

按默认配置限额读取文件并返回拥有的 Array。

```c
xvalue* xrtXsonlParseFile(
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 NUL 结尾路径 | 读取源文件或原子替换的目标文件。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有的 Array，调用方释放。 | — |
| NULL | 参数、预算、语法或资源错误。 | 不返回部分结果；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_PROTOCOL` — 非空行不是完整合法值，或严格模式遇到空白行；单条错误保留底层原因链。
- `XERR_IO` — 文件操作或未提供具体错误的输出回调失败；底层原因的其他 Kind 原样保留。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlParseFile 调用，失败转入统一清理。

```c
pRead = xrtXsonlParseFile(Path);
if ( pRead == NULL ) goto done;
```

### `xrtXsonlReadFile`

按整体输入上限读取文件，逐行解析；失败不返回部分 Array。

```c
xvalue* xrtXsonlReadFile(
	cstr sPath,
	const xxsonlreadconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 NUL 结尾路径 | 读取源文件或原子替换的目标文件。 |
| `pConfig` | 输入 | 非空；必须已初始化 | 单条配置和累计预算；本次调用不修改来源。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有的 Array，调用方释放。 | — |
| NULL | 参数、预算、语法或资源错误。 | 不返回部分结果；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_PROTOCOL` — 非空行不是完整合法值，或严格模式遇到空白行；单条错误保留底层原因链。
- `XERR_IO` — 文件操作或未提供具体错误的输出回调失败；底层原因的其他 Kind 原样保留。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlReadFile 调用，失败转入统一清理。

```c
pRead = xrtXsonlReadFile(Path, &Read);
if ( pRead == NULL ) goto done;
```

### `xrtXsonlStringifyFile`

按默认配置完整序列化 Array 后原子替换文件。

```c
bool xrtXsonlStringifyFile(
	cstr sPath,
	const xvalue* pArray
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 NUL 结尾路径 | 读取源文件或原子替换的目标文件。 |
| `pArray` | 输入 | 非空且类型为 XVALUE_ARRAY | 序列化各元素；调用不消费调用方拥有引用。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| true | 全部输出完成。 | — |
| false | 操作失败。 | 不返回部分结果；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_TYPE` — 序列化根值不是 Array。
- `XERR_UNSUPPORTED` — 单条值不能由所选格式编码。
- `XERR_IO` — 文件操作或未提供具体错误的输出回调失败；底层原因的其他 Kind 原样保留。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlStringifyFile 调用，失败转入统一清理。

```c
if ( !xrtXsonlStringifyFile(Path, pArray) ) goto done;
```

### `xrtXsonlWriteFile`

按高级配置完整序列化后原子替换文件；序列化失败保留原文件。

```c
bool xrtXsonlWriteFile(
	cstr sPath,
	const xvalue* pArray,
	const xxsonlwriteconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空 NUL 结尾路径 | 读取源文件或原子替换的目标文件。 |
| `pArray` | 输入 | 非空且类型为 XVALUE_ARRAY | 序列化各元素；调用不消费调用方拥有引用。 |
| `pConfig` | 输入 | 非空；必须已初始化 | 单条配置和累计预算；本次调用不修改来源。 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| true | 全部输出完成。 | — |
| false | 操作失败。 | 不返回部分结果；错误经 xrtGetError() 取得。 |

#### 错误

- `XERR_ARGUMENT` — 必要指针为空、配置字段或保留位非法。
- `XERR_MEMORY` — 工作区、结果或结构化错误分配失败。
- `XERR_RANGE` — 单条或累计资源预算耗尽。
- `XERR_TYPE` — 序列化根值不是 Array。
- `XERR_UNSUPPORTED` — 单条值不能由所选格式编码。
- `XERR_IO` — 文件操作或未提供具体错误的输出回调失败；底层原因的其他 Kind 原样保留。
其他单条编解码错误沿用 Cause 的具体 Kind 与代码。

#### 范例

[xsonl](../../examples/data/xsonl/main.c) · 完整程序中的 xrtXsonlWriteFile 调用，失败转入统一清理。

```c
if ( !xrtXsonlWriteFile(Path, pArray, &Write) ) goto done;
```

