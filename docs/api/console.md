# Console

`console` 模块提供跨平台 UTF-8 标准输出。它只管理进程已有的
`stdout` 和 `stderr`，不会创建、接管或关闭原生控制台。

## 裁剪与依赖

| 项目 | 值 |
| --- | --- |
| 裁剪宏 | `XRT_FEATURE_CONSOLE` |
| 直接依赖 | `core` |
| 头文件 | `<xrt/console.h>` 或 `<xrt.h>` |

## 类型与常量

### `xconsolestream`

标准输出流名称跨平台稳定，不直接暴露 FILE 或原生句柄。

```c
typedef enum xconsolestream {
	XCONSOLE_STDOUT = 1,
	XCONSOLE_STDERR
} xconsolestream;
```

| 值 | 语义 |
|---|---|
| `XCONSOLE_STDOUT` | XCONSOLE标准输出 |
| `XCONSOLE_STDERR` | 标准错误流 |


### `xconsoleerror`

Console 错误代码在 xrt.console 域内稳定。

```c
typedef enum xconsoleerror {
	XCONSOLE_ERROR_STREAM = 1,
	XCONSOLE_ERROR_UTF8,
	XCONSOLE_ERROR_WRITE,
	XCONSOLE_ERROR_FLUSH,
	XCONSOLE_ERROR_READ,
	XCONSOLE_ERROR_LIMIT,
	XCONSOLE_ERROR_STATE,
	XCONSOLE_ERROR_TERMINAL
} xconsoleerror;
```

| 值 | 语义 |
|---|---|
| `XCONSOLE_ERROR_STREAM` | 失败 |
| `XCONSOLE_ERROR_UTF8` | 失败 |
| `XCONSOLE_ERROR_WRITE` | 写方向 |
| `XCONSOLE_ERROR_FLUSH` | 刷新失败 |
| `XCONSOLE_ERROR_READ` | 读取失败 |
| `XCONSOLE_ERROR_LIMIT` | 输入超过限制 |
| `XCONSOLE_ERROR_STATE` | 控制台状态非法 |
| `XCONSOLE_ERROR_TERMINAL` | 终端操作失败 |


## 选择模块

```c
#define XRT_MODULE_CONSOLE
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

模块选择宏只启用 Console 和不可裁剪的核心错误能力。

## 标准流

| 常量 | 含义 |
| --- | --- |
| `XCONSOLE_STDOUT` | 标准输出 |
| `XCONSOLE_STDERR` | 标准错误 |

文本契约是 UTF-8。真实 Windows 控制台会严格验证并转换为 UTF-16；
标准流重定向到文件或管道时，XRT 原样保留 UTF-8 字节。二进制输出应使用
文件或 IO API，不属于 Console 文本接口。

Windows 以 `GetStdHandle` 返回的进程标准句柄为权威，因此运行期间的
`SetStdHandle` 会立即生效。若代码通过 `_dup2` 等 CRT 接口重定向标准流，
还应同步调用 `SetStdHandle`；只有进程标准句柄不存在时才回退到 CRT 句柄。

## 输出

### `xrtConsoleWrite`

写入给定 UTF-8 视图，不追加换行。一次调用不会与另一线程的一次 Console 调用交错。

```c
bool xrtConsoleWrite(xconsolestream Stream, xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Stream` | 输入 | 枚举值 | `XCONSOLE_STDOUT` / `XCONSOLE_STDERR` |
| `Text` | 输入 | 借用、UTF-8 | 要写入的文本视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入标准流 | — |
| `false` | 流枚举非法、UTF-8 非法或写入失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XCONSOLE_ERROR_STREAM` — 流枚举无效
- `XCONSOLE_ERROR_UTF8` — 真实 Windows 控制台收到非法 UTF-8
- `XCONSOLE_ERROR_WRITE` — 标准流写入失败（保留原生系统错误码）

#### 范例

[console/variants · 基础形态](../../examples/console/variants/main.c) · 不加换行的裸写入

```c
bool bOk = xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("write-ok"));
```


### `xrtConsoleWriteLine`

在同一个标准流锁内写入文本并追加一个换行符。

```c
bool xrtConsoleWriteLine(xconsolestream Stream, xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Stream` | 输入 | 枚举值 | 目标标准流 |
| `Text` | 输入 | 借用、UTF-8 | 要写入的文本（换行由函数追加） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 文本与换行已在同一锁内写入 | — |
| `false` | 同 `xrtConsoleWrite` 的失败条件 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtConsoleWrite`（`STREAM` / `UTF8` / `WRITE`）

#### 范例

[console/output · 双流](../../examples/console/output/main.c) · 正常日志走 stdout、诊断走 stderr

```c
if (
	!xrtConsoleWriteLine(
		XCONSOLE_STDOUT,
		XRT_STR_LITERAL("service started")
	) ||
	!xrtConsoleWriteLine(
		XCONSOLE_STDERR,
		XRT_STR_LITERAL("example diagnostic")
	)
) {
	return 1;
```


## 刷新和终端判断

### `xrtConsoleFlush`

冲刷指定标准流的缓冲。

```c
bool xrtConsoleFlush(xconsolestream Stream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Stream` | 输入 | 枚举值 | 目标标准流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 缓冲已落地 | — |
| `false` | 流枚举非法或刷新失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XCONSOLE_ERROR_STREAM` — 流枚举无效
- `XCONSOLE_ERROR_FLUSH` — 刷新失败（保留原生系统错误码）

#### 范例

[console/output · 收尾](../../examples/console/output/main.c) · 退出前保证诊断落地

```c
return xrtConsoleFlush(XCONSOLE_STDERR) ? 0 : 2;
```


### `xrtConsoleIsTerminal`

判断流当前是否交互终端。返回 `false` 可以表示普通文件或管道，不是错误。

```c
bool xrtConsoleIsTerminal(xconsolestream Stream);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Stream` | 输入 | 枚举值 | 目标标准流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 流连接到交互终端 | — |
| `false` | 文件/管道重定向（正常结果）或流枚举非法 | 重定向不设错；非法流设置错误 |

#### 错误

- `XCONSOLE_ERROR_STREAM` — 流枚举无效

#### 范例

[console/variants · 终端判断](../../examples/console/variants/main.c) · 重定向下返回 0 是正常结果

```c
printf("is-terminal=%d\n",
	xrtConsoleIsTerminal(XCONSOLE_STDOUT) ? 1 : 0);
```


## 错误

错误域固定为 `xrt.console`：

| 错误码 | 含义 |
| --- | --- |
| `XCONSOLE_ERROR_STREAM` | 标准流枚举无效 |
| `XCONSOLE_ERROR_UTF8` | 文本输出或输入的 UTF-8/Unicode scalar 无效 |
| `XCONSOLE_ERROR_WRITE` | 标准流写入失败 |
| `XCONSOLE_ERROR_FLUSH` | 标准流刷新失败 |

系统调用失败会保留原生系统错误码。调用成功不清除线程中已有的错误。

## 输入、屏幕与终端会话

基础 `console` 输出始终先验证完整 UTF-8（包括重定向），然后按实际
字节长度写出，内嵌 NUL 不截断；二进制内容使用 io_standard。

| 模块 | 直接依赖 | 能力 |
|---|---|---|
| `console_input` | console、buffer、io_standard | 严格 Unicode 字符/行输入 |
| `console_screen` | console | 尺寸/颜色查询和 styled output，不带 Session/IO/thread |
| `console_terminal` | console_screen、io_standard、buffer、thread | 交互模式、批量屏幕输出与事件 |

### `xrtConsoleReadChar`

```c
int xrtConsoleReadChar(uint32* pCodepoint);
```

Strict Unicode scalar input: 1 value, 0 EOF, -1 error. No read-ahead.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pCodepoint` | `uint32*` | 成功读取的 Unicode 标量值，不接受代理项。 |

#### 返回值

1：读取一个 Unicode 标量；0：正常 EOF；-1：读取或编码错误。

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleReadLine`

```c
xbuffer* xrtConsoleReadLine(size_t iMaxBytes);
```

EOF before a character is a normal NULL result; an empty line owns an empty buffer.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iMaxBytes` | `size_t` | 一行输入最大 UTF-8 字节数。 |

#### 返回值

拥有型行缓冲；正常 EOF 且未读取字符返回 NULL。空行仍返回拥有型空缓冲。

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSize`

```c
bool xrtConsoleSize(xconsolestream Stream, uint32* pColumns, uint32* pRows);
```

Queries never change terminal modes. Nonterminal size is 0,0.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Stream` | `xconsolestream` | 标准流选择：stdout 或 stderr。 |
| `pColumns` | `uint32*` | 返回终端列数；非终端返回零。 |
| `pRows` | `uint32*` | 返回终端行数；非终端返回零。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionOpen`

```c
xconsolesession* xrtConsoleSessionOpen(uint32 Flags);
```

flags: raw=1, mouse=2, bracketed-paste=4, alternate-screen=8. Thread-affine, exclusive stdin.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Flags` | `uint32` | 会话选项位：raw=1、mouse=2、bracketed paste=4、alternate screen=8。 |

#### 返回值

成功交付结果指针，拥有或借用规则见上述契约；拒绝或失败为 NULL。

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionWrite`

```c
bool xrtConsoleSessionWrite(xconsolesession* pSession, xstrview Text);
```

把精确长度的 UTF-8 文本写入当前会话。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### 屏幕操作

- `xrtConsoleSessionMove(Session,uint32 X,uint32 Y)`：零起点，最大值拒绝以防 +1 溢出。
- `xrtConsoleSessionClear(Session,int Mode)`：0 清屏并回原点，1 清整行，2 到屏末。
- `xrtConsoleSessionCursor(Session,bool Visible)`：队列中显示/隐藏光标。
- `xrtConsoleSessionStyle(Session,int32 Fg,int32 Bg,uint32 Attributes)`：样式同 WriteStyled。

### `xrtConsoleSessionRead`

```c
xconsoleevent* xrtConsoleSessionRead(xconsolesession* pSession, int TimeoutMs);
```

NULL without error is timeout; CLOSED is emitted once. Returned event is owned.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |
| `TimeoutMs` | `int` | 相对毫秒等待预算；超时属于普通空结果。 |

#### 返回值

拥有型事件，由 xrtConsoleEventDestroy 释放；超时为无错误 NULL，CLOSED 事件只交付一次。

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleColorMode`

```c
int xrtConsoleColorMode(xconsolestream Stream);
```

查询输出流颜色支持能力；普通管道不被当成可控制的终端。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Stream` | `xconsolestream` | 标准流选择：stdout 或 stderr。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleEventDestroy`

```c
void xrtConsoleEventDestroy(xconsoleevent* pEvent);
```

释放事件及其拥有的文本、粘贴数据；NULL 安全。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pEvent` | `xconsoleevent*` | 读取所得拥有型事件，使用完通过本函数释放。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionClear`

```c
bool xrtConsoleSessionClear(xconsolesession* pSession, int Mode);
```

清除当前会话显示区域，不关闭会话。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |
| `Mode` | `int` | 清除模式，按终端清屏约定选择当前位置或整个屏幕。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionClose`

```c
bool xrtConsoleSessionClose(xconsolesession* pSession);
```

幂等结束会话并恢复创建时保存的终端状态。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionClosed`

```c
bool xrtConsoleSessionClosed(const xconsolesession* pSession);
```

查询会话是否已关闭，不执行读取或关闭操作。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `const xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionCursor`

```c
bool xrtConsoleSessionCursor(xconsolesession* pSession, bool Visible);
```

设置光标可见性。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |
| `Visible` | `bool` | 是否显示光标。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionDestroy`

```c
void xrtConsoleSessionDestroy(xconsolesession* pSession);
```

关闭会话并释放其资源；会话须在创建线程上销毁。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionFlush`

```c
bool xrtConsoleSessionFlush(xconsolesession* pSession);
```

把已接受的输出刷新至底层终端。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionMove`

```c
bool xrtConsoleSessionMove(xconsolesession* pSession, uint32 X, uint32 Y);
```

把光标移动到给定的零起点行列。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |
| `X` | `uint32` | 光标目标列（以零为起点）。 |
| `Y` | `uint32` | 光标目标行（以零为起点）。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionPasteSupported`

```c
bool xrtConsoleSessionPasteSupported(const xconsolesession* pSession);
```

查询 bracketed paste 是否被当前会话支持。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `const xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleSessionStyle`

```c
bool xrtConsoleSessionStyle(xconsolesession* pSession, int32 Foreground, int32 Background, uint32 Attributes);
```

设置当前会话后续输出的颜色及属性。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pSession` | `xconsolesession*` | 创建线程拥有的控制台会话；独占标准输入，在该线程上使用。 |
| `Foreground` | `int32` | 前景色：-1 默认，0..255 调色板，0x1000000 OR RGB 真彩色。 |
| `Background` | `int32` | 背景色，编码与前景色一致。 |
| `Attributes` | `uint32` | 样式位：粗体、弱化、斜体、下划线、反色。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。



### `xrtConsoleWriteStyled`

```c
bool xrtConsoleWriteStyled(xconsolestream Stream, xstrview Text, int32 Foreground, int32 Background, uint32 Attributes);
```

-1 default, 0..255 palette, 0x1000000|RGB true color; attributes bits 1,2,4,8,16.

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Stream` | `xconsolestream` | 标准流选择：stdout 或 stderr。 |
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |
| `Foreground` | `int32` | 前景色：-1 默认，0..255 调色板，0x1000000 OR RGB 真彩色。 |
| `Background` | `int32` | 背景色，编码与前景色一致。 |
| `Attributes` | `uint32` | 样式位：粗体、弱化、斜体、下划线、反色。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

参数、UTF-8、系统终端或会话线程/关闭状态错误会设错。ReadLine 的未读字符 EOF 和 SessionRead 的超时是正常空结果，应检查线程错误区分。

#### 范例

参见已注册的 [examples/console/output/main.c](../../examples/console/output/main.c)，结合本节参数和生存期规则使用。




### `xconsoleevent`

读取交付的拥有型事件，包含输入、按键、鼠标、粘贴或关闭信息；通过 EventDestroy 释放其中的数据。

```c
typedef struct xconsoleevent {
    xconsoleeventkind Kind;
    uint32 Key, Modifiers, Repeat;
    int32 X, Y, Wheel;
    uint32 Columns, Rows;
    bool Down;
    xbuffer* Text; /* owned UTF-8; may contain NUL */
} xconsoleevent;
```


### `xconsoleeventkind`

事件类型枚举；关闭事件只发出一次。

```c
typedef enum xconsoleeventkind {
    XCONSOLE_EVENT_TEXT = 1, XCONSOLE_EVENT_KEY, XCONSOLE_EVENT_RESIZE,
    XCONSOLE_EVENT_PASTE, XCONSOLE_EVENT_MOUSE, XCONSOLE_EVENT_CLOSED
} xconsoleeventkind;
```


### `xconsolekey`

终端输入的稳定键编号；与 Unicode 字符输入分别表达。

```c
typedef enum xconsolekey {
    XCONSOLE_KEY_ESCAPE = 0x110000, XCONSOLE_KEY_ENTER, XCONSOLE_KEY_TAB,
    XCONSOLE_KEY_BACKSPACE, XCONSOLE_KEY_UP, XCONSOLE_KEY_DOWN,
    XCONSOLE_KEY_LEFT, XCONSOLE_KEY_RIGHT, XCONSOLE_KEY_HOME,
    XCONSOLE_KEY_END, XCONSOLE_KEY_INSERT, XCONSOLE_KEY_DELETE,
    XCONSOLE_KEY_PAGE_UP, XCONSOLE_KEY_PAGE_DOWN, XCONSOLE_KEY_F1
} xconsolekey;
```


### `xconsolesession`

创建线程亲和的控制台会话，独占 stdin，Close 恢复终端状态，Destroy 释放资源。

```c
typedef struct xconsolesession xconsolesession;
```
