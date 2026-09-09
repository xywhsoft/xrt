# xwork 基础类型 API

> xwork 的版本常量、状态码和公共资源归属约定。你读懂本页后，再看 runtime、workspace、run、tool、persistence API 会更容易。

[返回 API 索引](README.md) | [Runtime API](api-runtime.md) | [Run API](api-run.md)

---

## 目录

- [模块定位](#模块定位)
- [版本常量](#版本常量)
- [状态码](#状态码)
- [资源归属总则](#资源归属总则)
- [版本与状态函数](#版本与状态函数)
  - [xwork_version](#xwork_version)
  - [xwork_status_cstr](#xwork_status_cstr)
- [常见用法](#常见用法)
- [常见错误](#常见错误)
- [相关示例](#相关示例)

---

## 模块定位

本页说明所有 xwork 模块共享的基础契约：

- 版本常量和格式版本。
- `xwork_status` 状态码语义。
- `init` / `reset` / `destroy` / borrowed / copied / owned 的资源归属规则。
- 查询版本和状态码文本的两个基础函数。

本页不展开 runtime、workspace、tool、run 等模块的结构体字段；这些内容在对应模块页中逐函数说明。

---

## 版本常量

| 常量 | 当前值 | 说明 | 什么时候使用 |
| --- | --- | --- | --- |
| `XWORK_VERSION_MAJOR` | `0` | 主版本号。`0.x` 阶段代表 API 仍在收敛。 | release gate、兼容性判断、诊断输出。 |
| `XWORK_VERSION_MINOR` | `1` | 次版本号。新增能力通常提升该值。 | release gate、能力矩阵。 |
| `XWORK_VERSION_PATCH` | `0` | 修订版本号。修复类变更通常提升该值。 | 诊断、构建记录。 |
| `XWORK_PERSISTENCE_FORMAT_VERSION` | `14` | file persistence 格式版本。 | 读取/写入持久化 store、迁移和兼容性检查。 |
| `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` | `1` | remote worker/control plane 协议版本。 | worker 注册、control plane 消息解码、跨版本通信。 |

**补充说明：**

- 需要人类可读版本字符串时使用 `xwork_version()`。
- 需要判断持久化兼容性时使用 `XWORK_PERSISTENCE_FORMAT_VERSION`，不要解析版本字符串。
- 需要判断 remote worker 协议兼容性时使用 `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`。

---

## 状态码

### `xwork_status`

所有返回 `xwork_status` 的 API 都遵循 `xwork.h` 顶部的状态码语义。

| 值 | 说明 | 常见处理方式 |
| --- | --- | --- |
| `XWORK_OK` | 操作完成。 | 继续读取输出参数或进入下一步。 |
| `XWORK_ERROR_INVALID_ARGUMENT` | 必填指针、ID、枚举值或 option 组合无效。 | 修正调用方输入；通常不应重试同一输入。 |
| `XWORK_ERROR_NO_MEMORY` | 内存分配失败。 | 释放资源、降级或终止当前任务。 |
| `XWORK_ERROR_ALREADY_EXISTS` | 稳定 ID 对应的对象已存在。 | 换 ID、查询已有对象，或跳过重复创建。 |
| `XWORK_ERROR_NOT_FOUND` | 对象或持久化记录不存在。 | 检查 ID、store root、run id、artifact id。 |
| `XWORK_ERROR_INVALID_STATE` | 对象存在，但生命周期状态不允许该操作。 | 检查 run/graph/plane 状态，避免重复 start/execute/destroy。 |
| `XWORK_ERROR_EXTERNAL_FAILURE` | xrt/xllm、provider、host service、文件系统、进程、持久化或 callback 失败。 | 查看 event、artifact、host result 或 provider 诊断。 |
| `XWORK_ERROR_UNSUPPORTED` | 能力有效但未实现、未启用或版本不支持。 | 检查 profile、host tool、remote protocol、persistence format。 |
| `XWORK_ERROR_CANCELLED` | 协作式取消被观察到。 | 将 run 或任务视为 cancelled，而不是 failed。 |
| `XWORK_ERROR_PAUSED` | 执行停在可恢复边界。 | 获取 approval/pending tool 状态，显式 resume 后继续。 |

---

## 资源归属总则

| 规则 | 含义 |
| --- | --- |
| `*_init()` | 初始化调用者提供的结构体，通常不分配长期资源，可用于栈上对象。 |
| `*_reset()` | 释放结构体内部持有的深拷贝资源，让对象可复用或安全丢弃。 |
| `*_destroy()` | 销毁 opaque owned object，例如 runtime、run、agent pool、task graph、control plane、replay engine。 |
| copied | API 调用期间深拷贝输入字符串、数组或结构体内容。调用返回后调用方可释放原始输入。 |
| borrowed | xwork 只保存或临时使用调用方指针，调用方必须保证生命周期覆盖文档要求。 |
| owned | xwork 返回新对象或深拷贝结果，调用方必须用对应 reset/destroy 释放。 |

**补充说明：**

- getter 返回的 `const char *` 或 `const xwork_tool_def *` 默认视为 borrowed pointer。
- runtime destroy 会释放仍挂在 runtime 上的 workspace、tool registry 和 run。
- host service、persistence backend、callback user data、外部 xllm runtime/memory/cancel token 通常是 borrowed。

---

## 版本与状态函数

### xwork_version

获取 xwork 运行时版本字符串。

**功能：**

你可以在日志、诊断报告、release gate、artifact metadata 或宿主 UI 中记录该版本，方便后续定位问题。

**函数原型：**

```c
XWORK_API const char *xwork_version(void);
```

**参数：**

无。

**返回值：**

- 返回静态字符串，例如当前实现返回 `"0.1.0"`。
- 调用者不能释放返回值。

**资源归属：**

返回值是 borrowed pointer，生命周期为进程生命周期。

**补充说明：**

- 如果你需要数值版本，可使用 `XWORK_VERSION_MAJOR`、`XWORK_VERSION_MINOR`、`XWORK_VERSION_PATCH`。
- 版本字符串用于人类阅读，不建议用字符串比较判断兼容性。
- persistence format 和 remote protocol 兼容性分别使用 `XWORK_PERSISTENCE_FORMAT_VERSION` 和 `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`。

**范例代码：**

```c
#include "xwork.h"
#include <stdio.h>

int main(void)
{
    printf("xwork version: %s\n", xwork_version());
    return 0;
}
```

**相关 API：**

- `XWORK_VERSION_MAJOR`
- `XWORK_VERSION_MINOR`
- `XWORK_VERSION_PATCH`
- `XWORK_PERSISTENCE_FORMAT_VERSION`
- `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`

---

### xwork_status_cstr

把 `xwork_status` 转换为稳定字符串。

**功能：**

你可以在日志、断言失败、测试输出、诊断 artifact 或宿主 UI 中展示状态码名称，避免只显示数字。

**函数原型：**

```c
XWORK_API const char *xwork_status_cstr(xwork_status eStatus);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `eStatus` | 输入 | 否 | 要转换的状态码。可以是任意 `xwork_status` 枚举值；未知数值会返回兜底字符串。 |

**返回值：**

- 返回状态码对应的静态字符串，例如 `"XWORK_OK"`。
- 未知数值返回 `"XWORK_STATUS_UNKNOWN"`。
- 调用者不能释放返回值。

**资源归属：**

返回值是 borrowed pointer，生命周期为进程生命周期。

**补充说明：**

- 该函数不分配资源，不会失败。
- 返回字符串适合日志和诊断，不建议作为程序兼容性判断依据。
- 程序逻辑应直接比较 `xwork_status` 枚举值。

**范例代码：**

```c
#include "xwork.h"
#include <stdio.h>

int main(void)
{
    xwork_status status = XWORK_ERROR_INVALID_ARGUMENT;
    printf("status: %s\n", xwork_status_cstr(status));
    return 0;
}
```

**相关 API：**

- `xwork_status`
- `XWORK_OK`
- `XWORK_ERROR_INVALID_ARGUMENT`

---

## Shared Helper 函数

### xwork_memory_context_init

初始化 memory context。

**功能：**

准备一个用于 agent/run 共享上下文引用的 `xwork_memory_context`。

**函数原型：**

```c
XWORK_API void xwork_memory_context_init(xwork_memory_context *pContext);
```

**参数：**

- `pContext`：要初始化的 context；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

memory context 通常保存可持久化引用，不直接拥有外部 memory store。

**范例代码：**

```c
xwork_memory_context context;
xwork_memory_context_init(&context);
```

**相关 API：**

- `xwork_memory_context_reset`

---

### xwork_memory_context_reset

释放 memory context。

**功能：**

释放 memory context 内部深拷贝字符串和列表。

**函数原型：**

```c
XWORK_API void xwork_memory_context_reset(xwork_memory_context *pContext);
```

**参数：**

- `pContext`：要释放的 context；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部资源，不释放结构体本身。

**补充说明：**

调用后 context 可复用或安全丢弃。

**范例代码：**

```c
xwork_memory_context_reset(&context);
```

**相关 API：**

- `xwork_memory_context_init`

---

### xwork_session_policy_init

初始化 session policy。

**功能：**

准备 run/session 使用的策略配置。

**函数原型：**

```c
XWORK_API void xwork_session_policy_init(xwork_session_policy *pPolicy);
```

**参数：**

- `pPolicy`：要初始化的 policy；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

session policy 会随 run options 进入执行路径，用于控制审批、工具和自动化行为。

**范例代码：**

```c
xwork_session_policy policy;
xwork_session_policy_init(&policy);
```

**相关 API：**

- `xwork_run_create`

---

### xwork_string_list_init

初始化字符串列表。

**功能：**

准备一个通用 `xwork_string_list`。

**函数原型：**

```c
XWORK_API void xwork_string_list_init(xwork_string_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

用于共享对象中的字符串集合字段。

**范例代码：**

```c
xwork_string_list list;
xwork_string_list_init(&list);
```

**相关 API：**

- `xwork_string_list_reset`

---

### xwork_string_list_reset

释放字符串列表。

**功能：**

释放字符串列表中的每个字符串和列表数组。

**函数原型：**

```c
XWORK_API void xwork_string_list_reset(xwork_string_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的 deep-copy 字符串。

**补充说明：**

调用后列表回到空状态。

**范例代码：**

```c
xwork_string_list_reset(&list);
```

**相关 API：**

- `xwork_string_list_init`

---

## 常见用法

### 记录版本和持久化格式

```c
printf(
    "xwork=%s persistence=%d remote=%d\n",
    xwork_version(),
    XWORK_PERSISTENCE_FORMAT_VERSION,
    XWORK_REMOTE_PROTOCOL_VERSION_CURRENT
);
```

### 输出状态码

```c
xwork_status status = xwork_runtime_create(&tOptions, &pRuntime);
if (status != XWORK_OK) {
    fprintf(stderr, "create runtime failed: %s\n", xwork_status_cstr(status));
}
```

## 常见错误

| 问题 | 原因 | 处理方式 |
| --- | --- | --- |
| 用 `strcmp(xwork_version(), "...")` 判断兼容性 | 版本字符串面向人类阅读，不是兼容性协议。 | 使用数值宏或模块专用版本常量。 |
| 释放 `xwork_version()` 返回值 | 返回值是静态 borrowed pointer。 | 不要释放。 |
| 把 `XWORK_ERROR_PAUSED` 当作失败 | paused 是可恢复边界。 | 查询 approval/pending tool 状态并显式 resume。 |
| 把 `XWORK_ERROR_CANCELLED` 当作外部失败 | cancelled 表示协作式取消。 | 将 run/task 标记为 cancelled。 |

## 相关示例

- `examples\first_xwork_program.c`
- `tests\xwork_core_smoke.c`
