# 环境变量

`<xrt/environment.h>` 提供进程环境变量的 UTF-8 读取、设置和删除能力。该模块独立于进程创建体系；只读取环境变量的程序不需要携带线程、管道或子进程实现。

启用方式：

```c
#define XRT_MODULE_ENVIRONMENT
#include "xrt.h"
```

模块依赖 Unicode 基础转换，用于保证 Windows 宽字符环境与 POSIX 字节环境共享同一份严格 UTF-8 契约。

## 错误代码

`xenverror` 属于 `xrt.environment` 域：

- `XENV_ERROR_NAME`：名称为空、包含 `=` 或不是合法 UTF-8。
- `XENV_ERROR_VALUE`：值不是合法 UTF-8，或系统返回了无法转换的值。
- `XENV_ERROR_SYSTEM`：操作系统环境 API 失败；`xrtErrorSystemCode` 保留原生错误码。

内存不足继续使用统一的 `XERR_MEMORY`。全部错误都进入当前 XRT 错误执行上下文，因此上层宿主可以映射，C 调用方也可通过 `xrtGetError` 检查。

## `xrtEnvLookup`

```c
bool xrtEnvLookup(cstr sName, str* psValue);
```

这是无歧义基础入口。返回 `true` 表示查询过程成功：变量存在时 `*psValue` 是由 `xrtFree` 释放的 UTF-8 副本，不存在时 `*psValue == NULL`。空变量值会返回独立的空字符串，因此不会与变量不存在混淆。

函数进入后先把有效输出参数设为 `NULL`。名称必须非空、不能包含 `=`，并且必须是严格 UTF-8。

## `xrtEnvGet`

```c
str xrtEnvGet(cstr sName);
```

常见路径的一行便捷入口。成功时返回拥有副本；变量不存在或查询失败时返回 `NULL`。需要区分缺失和失败的代码应使用 `xrtEnvLookup`。

## `xrtEnvSet`

```c
bool xrtEnvSet(cstr sName, cstr sValue);
```

设置或覆盖进程级变量。值不能为空指针，但允许为空字符串。名称和值都按严格 UTF-8 验证；Windows 使用宽字符系统 API，不经过当前 ANSI 代码页。

## `xrtEnvRemove`

```c
bool xrtEnvRemove(cstr sName);
```

幂等删除变量。变量原本不存在仍返回 `true`。

## 并发与外部修改

环境是进程级共享状态。XRT 自身的 POSIX 读取、设置和删除入口使用同一把内部短锁，读取返回独立副本，不会借用 `getenv` 的易失指针。直接调用 C 运行库或第三方库修改环境时，调用方仍应与这些外部操作自行同步。

## 示例

```c
str sValue;

if ( !xrtEnvSet("APP_MODE", "test") ) {
	return false;
}
if ( !xrtEnvLookup("APP_MODE", &sValue) ) {
	return false;
}
if ( sValue != NULL ) {
	printf("%s\n", sValue);
	xrtFree(sValue);
}
xrtEnvRemove("APP_MODE");
```

完整示例见 `examples/system/environment/main.c`。
