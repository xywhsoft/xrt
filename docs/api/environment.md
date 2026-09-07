# 环境变量

`<xrt/environment.h>` 提供进程环境变量的 UTF-8 读取、设置和删除能力。该模块独立于进程创建体系；只读取环境变量的程序不需要携带线程、管道或子进程实现。

启用方式：

```c
#define XRT_MODULE_ENVIRONMENT
#include "xrt.h"
```

模块依赖 Unicode 基础转换，用于保证 Windows 宽字符环境与 POSIX 字节环境共享同一份严格 UTF-8 契约。

## 类型与常量

### `xenverror`

环境变量错误代码在 xrt.environment 域内稳定。

```c
typedef enum xenverror {
	XENV_ERROR_NAME = 1,
	XENV_ERROR_VALUE,
	XENV_ERROR_SYSTEM
} xenverror;
```

| 值 | 语义 |
|---|---|
| `XENV_ERROR_NAME` | 名称 |
| `XENV_ERROR_VALUE` | 值非法 |
| `XENV_ERROR_SYSTEM` | 系统调用失败 |

## 错误代码

`xenverror` 属于 `xrt.environment` 域：

- `XENV_ERROR_NAME`：名称为空、包含 `=` 或不是合法 UTF-8。
- `XENV_ERROR_VALUE`：值不是合法 UTF-8，或系统返回了无法转换的值。
- `XENV_ERROR_SYSTEM`：操作系统环境 API 失败；`xrtErrorSystemCode` 保留原生错误码。

内存不足继续使用统一的 `XERR_MEMORY`。全部错误都进入当前 XRT 错误执行上下文，因此上层宿主可以映射，C 调用方也可通过 `xrtGetError` 检查。

## API

### `xrtEnvLookup`

无歧义基础入口：查询过程成功返回 `true`，变量是否存在由输出指针区分。

```c
bool xrtEnvLookup(cstr sName, str* psValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sName` | 输入 | 非空、不含 `=`、严格 UTF-8 | 变量名 |
| `psValue` | 输出 | 非空 | 进入时先置 `NULL`；存在时为 `xrtFree` 释放的副本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 查询成功；`*psValue` 非空 = 值副本，`NULL` = 变量不存在 | — |
| `false` | 名称非法、系统失败或 OOM | `*psValue` 保持 `NULL`；错误经 `xrtGetError()` 报告 |

#### 错误

- `XENV_ERROR_NAME` — 名称非法
- `XENV_ERROR_VALUE` — 系统值无法转换为 UTF-8
- `XENV_ERROR_SYSTEM` — 环境 API 失败
- `XERR_MEMORY` — 副本分配失败

#### 范例

[environment/variants · 缺失语义](../../examples/environment/variants/main.c) · 不存在是成功状态（输出为空）

```c
/* 不存在的变量：成功返回 + 空输出。 */
if ( xrtEnvLookup("XRT_DEFINITELY_MISSING", &sValue) ) {
	printf("missing=ok\n");
}
xrtFree(sValue);
```

### `xrtEnvGet`

常见路径的一行便捷入口；需要区分缺失和失败的代码应使用 `xrtEnvLookup`。

```c
str xrtEnvGet(cstr sName);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sName` | 输入 | 非空、不含 `=`、严格 UTF-8 | 变量名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式 UTF-8 副本（空变量值返回独立空串） | — |
| `NULL` | 变量不存在或查询失败 | 失败时错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtEnvLookup`（`NAME` / `VALUE` / `SYSTEM` / `XERR_MEMORY`）

#### 范例

[system/environment · 拥有式读取](../../examples/system/environment/main.c) · 副本不受后续环境修改影响

```c
sValue = xrtEnvGet("XRT_ENVIRONMENT_EXAMPLE");
if ( sValue == NULL ) {
	return 2;
}
printf("value=%s\n", sValue);
```

### `xrtEnvSet`

设置或覆盖进程级变量。

```c
bool xrtEnvSet(cstr sName, cstr sValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sName` | 输入 | 非空、不含 `=`、严格 UTF-8 | 变量名 |
| `sValue` | 输入 | 非空、严格 UTF-8；允许空串 | 新值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置（子进程会继承） | — |
| `false` | 名称/值非法、系统失败或 OOM | 环境不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XENV_ERROR_NAME` / `XENV_ERROR_VALUE` — 名称或值非法
- `XENV_ERROR_SYSTEM` — 设置 API 失败（Windows 用宽字符 API，不经 ANSI 代码页）
- `XERR_MEMORY`

#### 范例

[system/environment · 全流程](../../examples/system/environment/main.c) · 真实进程环境生效

```c
if ( !xrtEnvSet("XRT_ENVIRONMENT_EXAMPLE", "hello") ) {
	return 1;
}
```

### `xrtEnvRemove`

幂等删除变量；变量原本不存在仍返回 `true`。

```c
bool xrtEnvRemove(cstr sName);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sName` | 输入 | 非空、不含 `=`、严格 UTF-8 | 变量名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已删除或本就不存在（幂等） | — |
| `false` | 名称非法或系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XENV_ERROR_NAME` — 名称非法
- `XENV_ERROR_SYSTEM` — 删除 API 失败

#### 范例

[system/environment · 收尾](../../examples/system/environment/main.c) · 范例不留下全局副作用

```c
return xrtEnvRemove("XRT_ENVIRONMENT_EXAMPLE") ? 0 : 3;
```

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

完整示例见 `examples/system/environment/main.c` 与 `examples/environment/variants/main.c`。
