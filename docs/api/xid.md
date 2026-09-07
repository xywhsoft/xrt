# XID

XID 模块生成 192 位分布式标识。它保留旧版 XRT 的 24 字节二进制和 32 字符紧凑文本优势，但使用跨平台固定布局、系统安全随机源和值类型 API，删除对本机 IP、结构体端序和逐对象堆分配的依赖。

## 模块

`XRT_MODULE_XID` 启用 `XRT_FEATURE_XID`，并精确依赖 `time`、`random_secure` 和 `codec_base64`。模块不依赖网络、线程、任务或容器。

## 公开类型与常量

| 名称 | 含义 |
| --- | --- |
| `xid` | 固定 24 字节的 XID 值类型 |
| `xiderror` | `xrt.xid` 错误域的稳定错误代码类型 |
| `XID_ERROR_FORMAT` | 文本长度、字母表或规范编码错误 |
| `XID_BINARY_SIZE` | 二进制值长度，固定为 24 |
| `XID_TEXT_SIZE` | 文本长度，固定为 32 |
| `XID_TEXT_CAPACITY` | 包含末尾零字节的写入容量，固定为 33 |
| `XID_ZERO` | 仅用于对象定义的静态全零初始化器 |

## 布局

`xid` 始终是 `XID_BINARY_SIZE`，即 24 字节：

| 字节 | 内容 |
| --- | --- |
| `0..7` | 经符号偏置后按大端保存的 Unix 微秒 |
| `8..23` | 128 位操作系统安全随机数 |

符号偏置让完整 `xtime` 范围按无符号字节顺序排列。不同生成时间的 XID 可以直接按 24 字节比较；同一微秒内的顺序由随机后缀决定。

`XID_ZERO` 初始化全零值，`xrtXidIsZero` 检查全零。全零值是合法的可存储值，但不会由正常生成器产生。

## 生成

`xrtXidMake` 把一个新 ID 写入调用方提供的 `xid`，不分配内存。随机源失败时输出保持全零，并原样传播 `xrt.random` 错误；不会退化到可预测伪随机数。

`xrtXidMakeMany` 批量生成连续数组。它一次获取整批安全随机字节，再为每项写入时间前缀，适合日志批次、对象导入和高吞吐服务。`iCount == 0` 时允许 `pXids == NULL`；大小溢出和非空空指针会失败。随机源失败时整批清零。

`xrtXidMakeString` 是常见路径 helper，直接返回新生成的 32 字符文本；调用方使用 `xrtFree` 释放。

XID 用于唯一标识，不是身份验证令牌。虽然随机后缀来自安全随机源，文本仍公开生成时间，不应代替 session secret、CSRF token 或访问凭据。

### `xrtXidMake`

生成一个使用当前 Unix 微秒和 128 位系统安全随机数的 XID。

```c
bool xrtXidMake(xid* pXid)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pXid` | 输出 | 非空 | 接收 XID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已生成 | — |
| `false` | 系统随机源失败 | `XERR_IO` |

#### 错误

- `XERR_ARGUMENT` — 输出为空
- `XERR_IO` — 系统安全随机源读取失败

#### 范例

[xid](../../examples/id/xid/main.c) · 生成

```c
	if ( !xrtXidMake(&Value) ||
		 !xrtXidWrite(&Value, arrText, sizeof(arrText)) ||
		 !xrtXidParse((xstrview){ arrText, XID_TEXT_SIZE }, &Parsed) ||
		 !xrtXidTime(&Parsed, &iTime) ) {
```

### `xrtXidMakeMany`

批量生成 XID，一次取得整批安全随机字节以降低系统调用成本。

```c
bool xrtXidMakeMany(xid* pXids, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pXids` | 输出 | 非空数组 | 接收 XID 数组 |
| `iCount` | 输入 | > 0 | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 全部生成 | — |
| `false` | 失败，输出内容不确定 | `XERR_IO` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或数量为零
- `XERR_IO` — 系统安全随机源读取失败

#### 范例

[xid_batch](../../examples/id/xid_batch/main.c) · 批量生成

```c
	if ( !xrtXidMakeMany(Values, 4u) || xrtXidIsZero(&Values[0]) ) {
```

## 文本

`XID_TEXT_SIZE` 固定为 32，`XID_TEXT_CAPACITY` 是包含末尾零字节所需的 33 字节容量。字母表为：

```text
-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz
```

字母表按 ASCII 递增并且 URL-safe。固定长度编码因此同时保持二进制顺序和普通字符串字典序，XID 文本可以直接作为数据库有序键、文件名或 URL 路径段。

`xrtXidWrite` 写入调用方缓冲，不分配内存；容量不足时把可用输出首字节清零并返回范围错误。`xrtXidFormat` 返回由 `xrtFree` 释放的文本。

`xrtXidParse` 只接受完整 32 字节规范文本，不接受空白、填充、别名字母、截断或附加数据。解析失败不修改输出 `xid`。错误域为 `xrt.xid`，代码是 `XID_ERROR_FORMAT`；`xrtXidErrorOffset` 返回第一个非法字节、文本末尾或第一个多余字节的位置。

### `xrtXidMakeString`

生成一个由 `xrtFree` 释放的 32 字符 XID。

```c
str xrtXidMakeString(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾 XID 文本，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_IO` / `XERR_MEMORY` |

#### 错误

- `XERR_IO` — 系统安全随机源失败
- `XERR_MEMORY` — 分配失败

#### 范例

[xid_batch](../../examples/id/xid_batch/main.c) · 生成并格式化

```c
	sGenerated = xrtXidMakeString();
```

### `xrtXidWrite`

把 XID 写为 32 字符有序 URL-safe 文本，并在末尾补零。

```c
bool xrtXidWrite(const xid* pXid, char* sOutput, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pXid` | 输入 | 非空 | 源 XID |
| `sOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | >= 33 | 容量，须含末尾零 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 容量不足或参数非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量小于 33 字节，不写半个结果

#### 范例

[xid](../../examples/id/xid/main.c) · 写入缓冲

```c
		 !xrtXidWrite(&Value, arrText, sizeof(arrText)) ||
```

### `xrtXidFormat`

创建由 `xrtFree` 释放的 XID 文本。

```c
str xrtXidFormat(const xid* pXid)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pXid` | 输入 | 非空 | 源 XID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾 XID 文本，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[xid_batch](../../examples/id/xid_batch/main.c) · 格式化

```c
	sFormatted = xrtXidFormat(&Values[0]);
```

### `xrtXidParse`

严格解析完整的 32 字符 XID；失败时不修改输出值。

```c
bool xrtXidParse(xstrview Text, xid* pXid)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 32 字符文本 |
| `pXid` | 输出 | 非空 | 接收 XID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解析 | — |
| `false` | 文本非法 | `XERR_ARGUMENT` / `xrt.xid` 域错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空
- `xrt.xid` 域错误 — 长度不是 32 或含非法字符；字节位置可由 `xrtXidErrorOffset` 读取

#### 范例

[xid](../../examples/id/xid/main.c) · 解析

```c
		 !xrtXidParse((xstrview){ arrText, XID_TEXT_SIZE }, &Parsed) ||
```

### `xrtXidErrorOffset`

从 `xrt.xid` 格式错误的机器数据中读取文本字节位置。

```c
bool xrtXidErrorOffset(const xerror* pError, size_t* pOffset)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pError` | 输入 | 非空、`xrt.xid` 域 | 解析错误 |
| `pOffset` | 输出 | 非空 | 接收文本字节偏移 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出偏移 | — |
| `false` | 错误不含位置数据 | 不设错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- 无位置数据返回 `false` 且不设置错误

#### 范例

[xid_batch](../../examples/id/xid_batch/main.c) · 错误定位

```c
		 !xrtXidErrorOffset(xrtGetError(), &iOffset) ||
```

## 时间与比较

`xrtXidTime` 从固定前缀恢复 Unix 微秒，不访问系统时钟。任意 24 字节值都能按同一布局解释，因此二进制反序列化不需要额外“有效”标记。

`xrtXidCompare` 返回负数、零或正数的规范三态结果，先比较时间前缀，再比较随机后缀。`xrtXidEqual` 比较全部 24 字节。空指针是参数错误；XID 是小型值类型，通常应直接嵌入结构或数组，而不是以可空堆对象表示。

### `xrtXidTime`

提取生成时间；任意 24 字节值都可以按稳定布局解释。

```c
bool xrtXidTime(const xid* pXid, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pXid` | 输入 | 非空 | 源 XID |
| `pTime` | 输出 | 非空 | 接收生成时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 时间已写出 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[xid](../../examples/id/xid/main.c) · 提取时间

```c
		 !xrtXidTime(&Parsed, &iTime) ) {
```

### `xrtXidCompare`

按时间前缀和随机后缀执行三态字典序比较。

```c
int xrtXidCompare(const xid* pLeft, const xid* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | 左 XID |
| `pRight` | 输入 | 非空 | 右 XID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `< 0` / `0` / `> 0` | 比较结果 | — |

#### 错误

- 无 — 纯比较，不设置错误

#### 范例

[xid_batch](../../examples/id/xid_batch/main.c) · 三态比较

```c
	printf("batch order: %d\n", xrtXidCompare(&Values[0], &Values[1]));
```

### `xrtXidEqual`

判断两个 XID 的全部 24 字节是否相同。

```c
bool xrtXidEqual(const xid* pLeft, const xid* pRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | 左 XID |
| `pRight` | 输入 | 非空 | 右 XID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否相同 | — |

#### 错误

- 无 — 纯比较，不设置错误

#### 范例

[xid_batch](../../examples/id/xid_batch/main.c) · 相等判断

```c
		 ) || !xrtXidEqual(&Values[0], &Parsed) ||
```

### `xrtXidIsZero`

判断 XID 是否为全零值。

```c
bool xrtXidIsZero(const xid* pXid)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pXid` | 输入 | 非空 | 源 XID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否全零 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[xid_batch](../../examples/id/xid_batch/main.c) · 全零判断

```c
	if ( !xrtXidMakeMany(Values, 4u) || xrtXidIsZero(&Values[0]) ) {
```

## 示例

`examples/id/xid/main.c` 展示无分配生成、固定缓冲写入、解析和时间提取。`examples/id/xid_batch/main.c` 展示批量生成、拥有型文本、三态比较、相等/全零判断和结构化错误位置。并发无碰撞、OOM、完整有符号时间范围和单头文件路径由模块测试覆盖。
