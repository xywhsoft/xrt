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

`int xrtConsoleReadChar(uint32* Codepoint)`：1 为 Unicode scalar，0 为
正常 EOF，-1 为错误；Codepoint 非空。Windows 原生输入严格组合 UTF-16
代理对，重定向和 POSIX 输入严格验证 UTF-8。无效输入是 XERR_IO，
不预读下一字符。调用期间独占标准输入，活动 raw Reader/Session
造成 XERR_STATE；错误时已经消费的输入不回滚。

### `xrtConsoleReadLine`

`xbuffer* xrtConsoleReadLine(size_t MaxBytes)`：去掉 LF/CRLF，裸 CR
仍是内容，UTF-8 实际字节上限不计终止符。EOF 前没有字符返回正常
NULL，空行返回独立空 Buffer；失败 NULL 并设错，调用者 Destroy
返回值。NUL 原样保留；零上限允许空行，不允许非空内容。

### `xrtConsoleSize` / `xrtConsoleColorMode` / `xrtConsoleWriteStyled`

Size 接受 stdout/stderr 和非空的 uint32 列/行输出；非终端返回 true、
0/0，查询不改模式。ColorMode 返回 0/1/2/3（none/basic/256/truecolor），
尊重 NO_COLOR 和 TERM=dumb，非法 stream 返回 -1 并设错。
WriteStyled 接受 xstrview、foreground/background/attributes，颜色 -1
默认、0..255 色板、0x1000000|RGB 真实色；属性 bits1/2/4/8/16 为
bold/dim/italic/underline/reverse。先验证样式和全文，重定向不插入
ANSI；交互输出临时设置样式并恢复，Windows 同时恢复输出模式。

### `xrtConsoleSessionOpen` / `xrtConsoleSessionClose` / `xrtConsoleSessionDestroy`

Open flags：raw1/mouse2/paste4/alternate-screen8，非法位拒绝；要求
交互 stdin/stdout，同一进程只允许一个标准输入消费者。Open 返回
拥有的 Session，普通操作限定创建线程。Close 幂等，输出失败仍继续
恢复，释放租约；失败应由显式 Close 观察。Destroy 负责最后资源
清理，允许最后拥有者在另一个线程执行，但禁止并发操作/析构。
`xrtConsoleSessionClosed` 查询关闭状态，NULL 视为关闭。

Windows 精确恢复 GetConsoleMode 和光标状态。POSIX 精确恢复 termios，
关闭本 Session 启用的 ANSI 模式；若隐藏了光标则显示（无法查询此前
可见状态）。不承诺与其他竞争终端库共存或强杀后的恢复。

### `xrtConsoleSessionWrite` / `xrtConsoleSessionFlush`

Write 将严格 UTF-8 xstrview 加入队列（含 NUL），16 MiB 硬上限；
Flush 写出一批并刷新。失败可能部分写出，批次被丢弃而不重放；不
是“出错后恢复屏幕”的事务。Close 也提交剩余批次。模式切换只发生
在 Open/Close，不为每个 write 反复设置模式。

### 屏幕操作

- `xrtConsoleSessionMove(Session,uint32 X,uint32 Y)`：零起点，最大值拒绝以防 +1 溢出。
- `xrtConsoleSessionClear(Session,int Mode)`：0 清屏并回原点，1 清整行，2 到屏末。
- `xrtConsoleSessionCursor(Session,bool Visible)`：队列中显示/隐藏光标。
- `xrtConsoleSessionStyle(Session,int32 Fg,int32 Bg,uint32 Attributes)`：样式同 WriteStyled。

### `xrtConsoleSessionRead` / `xrtConsoleEventDestroy`

Read timeoutMs 为 -1 永久等待或非负毫秒；NULL 无错是 timeout。
返回拥有的 Event，Destroy 释放独立 Text Buffer。Text/Key/Resize/
Paste/Mouse/Closed kind 为 1..6；Closed 在 EOF 仅发一次。事件内容
1 MiB 上限。Key 专用值从0x110000起，Text key 为 Unicode scalar；
modifiers Shift/Ctrl/Alt/Meta=1/2/4/8，repeat/down 保留平台信息。
Mouse key 按钮位左1/右2/中4，x/y 为零起点，wheel 带符号。未对应
当前 kind 的字段无意义，不能当作跨事件状态。

POSIX 普通按键没有 key-up，down=true；独立 Escape 最多约30 ms
消歧义，resize 最多50 ms轮询。分段 UTF-8/转义保留到完整事件，未知
序列保留字节回退；数字以受检十进制解析，禁止 scanf 溢出。支持
bracketed paste（NUL 保留）和 SGR mouse。
`xrtConsoleSessionPasteSupported` 只在活动 POSIX Session 开启 paste
时为 true；Windows 原生输入不能区分粘贴与打字，返回 false，字符
仍以 Text 提供，不伪造 Paste。

验收入口：console/input/screen/terminal 模块与单头测试；Windows
terminal_windows 在 CREATE_NO_WINDOW 私有控制台注入事件并检查
精确恢复。`python3 tools/check_console_terminal_pty.py` 在私有 PTY
覆盖 Unicode、分段转义、数字边界、修饰键、NUL Paste、鼠标、resize、
timeout、输入租约与恢复，绝不改变调用者终端。
