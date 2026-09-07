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
	XCONSOLE_ERROR_FLUSH
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
|---|---|---|---|
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
|---|---|---|---|
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
}
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
|---|---|---|---|
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
|---|---|---|---|
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
| `XCONSOLE_ERROR_UTF8` | 真实 Windows 控制台收到非法 UTF-8 |
| `XCONSOLE_ERROR_WRITE` | 标准流写入失败 |
| `XCONSOLE_ERROR_FLUSH` | 标准流刷新失败 |

系统调用失败会保留原生系统错误码。调用成功不清除线程中已有的错误。
