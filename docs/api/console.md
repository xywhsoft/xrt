# Console

`console` 模块提供跨平台 UTF-8 标准输出。它只管理进程已有的
`stdout` 和 `stderr`，不会创建、接管或关闭原生控制台。

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

## 输出

```c
xrtConsoleWrite(
	XCONSOLE_STDOUT,
	XRT_STR_LITERAL("loading")
);
xrtConsoleWriteLine(
	XCONSOLE_STDOUT,
	XRT_STR_LITERAL(" complete")
);
```

`xrtConsoleWrite` 写入给定视图，`xrtConsoleWriteLine` 在同一个标准流锁
内追加一个换行符。一次调用不会与另一线程的一次 Console 调用交错。

文本契约是 UTF-8。真实 Windows 控制台会严格验证并转换为 UTF-16；
标准流重定向到文件或管道时，XRT 原样保留 UTF-8 字节。二进制输出应使用
文件或 IO API，不属于 Console 文本接口。

Windows 以 `GetStdHandle` 返回的进程标准句柄为权威，因此运行期间的
`SetStdHandle` 会立即生效。若代码通过 `_dup2` 等 CRT 接口重定向标准流，
还应同步调用 `SetStdHandle`；只有进程标准句柄不存在时才回退到 CRT 句柄。

## 刷新和终端判断

```c
bool interactive = xrtConsoleIsTerminal(XCONSOLE_STDOUT);
bool flushed = xrtConsoleFlush(XCONSOLE_STDOUT);
```

`xrtConsoleIsTerminal` 返回 `false` 可以表示普通文件或管道，不是错误。
传入非法流时返回 `false` 并设置 `xrt.console` 错误。

## 错误

错误域固定为 `xrt.console`：

| 错误码 | 含义 |
| --- | --- |
| `XCONSOLE_ERROR_STREAM` | 标准流枚举无效 |
| `XCONSOLE_ERROR_UTF8` | 真实 Windows 控制台收到非法 UTF-8 |
| `XCONSOLE_ERROR_WRITE` | 标准流写入失败 |
| `XCONSOLE_ERROR_FLUSH` | 标准流刷新失败 |

系统调用失败会保留原生系统错误码。调用成功不清除线程中已有的错误。
