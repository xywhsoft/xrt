# Profiles API

> 状态：中文逐函数参考，待人工审阅。

Profiles API 提供面向产品场景的默认 runtime、workspace、run、orchestrator、xllm 和 policy 配置。当前内置 profile 包括 `xcode` 和 `xclaw`。

## 模块定位

Profile 是默认策略集合，不是产品完整策略。产品层可以以 profile 为基线，再覆盖更严格的安全、模型、memory、planner 或工具策略。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 常量 | `XWORK_PROFILE_XCODE`, `XWORK_PROFILE_XCLAW` |
| 结构体 | `xwork_profile` |
| 函数 | `xwork_profile_init`, `xwork_profile_get_builtin`, `xwork_profile_apply_runtime_options`, `xwork_profile_apply_xllm_profile_options`, `xwork_profile_apply_xllm_bootstrap_options`, `xwork_profile_apply_workspace_options`, `xwork_profile_apply_run_options`, `xwork_profile_apply_orchestrator_options` |

## Profile 字段

| 字段 | 说明 |
| --- | --- |
| `sProfileId` | 稳定 profile id。 |
| `sDisplayName` | 展示名。 |
| `sDescription` | profile 说明。 |
| `sDefaultLlmProfileId` | 默认 xllm profile id。 |
| `sDefaultSessionProfileId` | 默认 session profile id。 |
| `eAutonomy` | 默认自主性。 |
| `tPolicy` | 默认 policy。 |
| `tSessionPolicy` | 默认 session policy。 |
| `iDefaultMaxTurns` | 默认最大模型回合数。 |
| `bDefaultAutoApprove` | 是否默认自动批准可自动化步骤。 |
| `bEnableWorkspaceMemory` | 是否默认启用 workspace memory。 |
| `ePlannerMode` | planner 边界默认模式。 |

## 内置 Profile

| Profile | 默认倾向 |
| --- | --- |
| `XWORK_PROFILE_XCODE` / `"xcode"` | 面向 AI IDE，半自动，低风险自动审批，默认不启用 workspace memory，planner boundary 关闭，网络默认拒绝。 |
| `XWORK_PROFILE_XCLAW` / `"xclaw"` | 面向自主 Agent，自动化程度更高，可启用 workspace memory 和 planner boundary，网络仍默认拒绝，除非调用方配置 allowlist。 |

## 推荐覆盖顺序

```text
options_init
xwork_profile_get_builtin
xwork_profile_apply_*_options
产品层覆盖更严格策略
create runtime / workspace / run / orchestrator
```

这样可以避免 profile 覆盖产品层显式配置。

---

### xwork_profile_init

初始化 profile 结构。

**功能：**

你可以在手动构造 profile 或接收内置 profile 前调用该函数，使字段进入稳定默认状态。

**函数原型：**

```c
XWORK_API void xwork_profile_init(xwork_profile *pProfile);
```

**参数：**

- `pProfile`：输出参数。可为 `NULL`；为 `NULL` 时不执行任何操作。非 `NULL` 时会清零并写入默认策略。

**返回值：**

无。

**资源归属：**

函数不分配堆内存。profile 中的字符串字段保持借用语义。

**补充说明：**

- 默认自主性为 `XWORK_AUTONOMY_SEMI_AUTO`。
- 默认 max turns 为 `4`。
- 默认 auto approve 为 `true`。
- 默认 planner mode 为 `XWORK_PLANNER_OFF`。
- 该函数会初始化嵌套的 policy 和 session policy。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_profile profile;
    xwork_profile_init(&profile);
    return profile.iDefaultMaxTurns == 4u ? 0 : 1;
}
```

**相关 API：**

- `xwork_profile_get_builtin`
- `xwork_profile_apply_run_options`

---

### xwork_profile_get_builtin

获取内置 profile。

**功能：**

你可以用该函数加载 `xcode` 或 `xclaw` 的默认配置，作为产品初始化选项的基线。

**函数原型：**

```c
XWORK_API xwork_status xwork_profile_get_builtin(
    const char *sProfileId,
    xwork_profile *pProfile
);
```

**参数：**

- `sProfileId`：输入参数。必须是内置 profile id，例如 `XWORK_PROFILE_XCODE` 或 `XWORK_PROFILE_XCLAW`。
- `pProfile`：输出参数。必须非 `NULL`。函数会先初始化，再写入内置 profile。

**返回值：**

- `XWORK_OK`：获取成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：输出指针无效。
- `XWORK_ERROR_NOT_FOUND`：profile id 为空或不是内置 profile。

**资源归属：**

返回的 profile 结构由调用者拥有；其中字符串指向静态内置数据，调用者不能释放。

**补充说明：**

- 获取 profile 不会修改 runtime 或 options，必须继续调用 apply 函数。
- 内置 profile 是稳定默认值，但产品层仍应显式覆盖安全边界。

**范例代码：**

```c
#include "xwork.h"

int load_xclaw_profile(void) {
    xwork_profile profile;
    return xwork_profile_get_builtin(XWORK_PROFILE_XCLAW, &profile) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_profile_init`
- `xwork_profile_apply_runtime_options`

---

### xwork_profile_apply_runtime_options

把 profile 的 runtime 级策略写入 runtime options。

**功能：**

你可以用该函数把 profile 中的 policy 默认值应用到 `xwork_runtime_options`。

**函数原型：**

```c
XWORK_API xwork_status xwork_profile_apply_runtime_options(
    const xwork_profile *pProfile,
    xwork_runtime_options *pOptions
);
```

**参数：**

- `pProfile`：输入参数。必须非 `NULL`。
- `pOptions`：输入/输出参数。必须非 `NULL`。函数会写入 `tPolicy`。

**返回值：**

- `XWORK_OK`：应用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：profile 或 options 为空。

**资源归属：**

函数不分配资源，不接管 `pOptions` 内其他借用指针的所有权。

**补充说明：**

- 该函数只覆盖 runtime policy，不修改 xllm runtime、host services 或 persistence backend。
- 如果产品需要更严格策略，应在调用该函数后再覆盖。

**范例代码：**

```c
#include "xwork.h"

int apply_runtime_profile(const xwork_profile *profile) {
    xwork_runtime_options options;
    xwork_runtime_options_init(&options);
    return xwork_profile_apply_runtime_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_runtime_options_init`
- `xwork_runtime_create`

---

### xwork_profile_apply_xllm_profile_options

把 profile 的默认模型 profile id 写入 xllm profile options。

**功能：**

你可以用该函数让 `xwork_xllm_profile_options` 默认使用 profile 预设的 xllm profile id 和展示名。

**函数原型：**

```c
XWORK_API xwork_status xwork_profile_apply_xllm_profile_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pOptions
);
```

**参数：**

- `pProfile`：输入参数。必须非 `NULL`。
- `pOptions`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：应用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：profile 或 options 为空。

**资源归属：**

函数不复制字符串。写入的 profile id 和 display name 是借用指针，通常来自静态内置 profile。

**补充说明：**

- 如果 `pOptions->sProfileId` 已经有非空值，函数不会覆盖它。
- 如果 `pOptions->sDisplayName` 已经有非空值，函数不会覆盖它。

**范例代码：**

```c
#include "xwork.h"

int apply_llm_profile(const xwork_profile *profile) {
    xwork_xllm_profile_options options;
    xwork_xllm_profile_options_init(&options);
    return xwork_profile_apply_xllm_profile_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_xllm_profile_options_init`
- `xwork_profile_apply_xllm_bootstrap_options`

---

### xwork_profile_apply_xllm_bootstrap_options

把 profile 的 xllm profile 默认值接入 bootstrap options。

**功能：**

你可以用该函数同时准备 `xwork_xllm_profile_options` 和 `xwork_xllm_bootstrap_options`，使 runtime create 时能 bootstrap 一个默认 xllm runtime。

**函数原型：**

```c
XWORK_API xwork_status xwork_profile_apply_xllm_bootstrap_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pProfileOptions,
    xwork_xllm_bootstrap_options *pBootstrapOptions
);
```

**参数：**

- `pProfile`：输入参数。必须非 `NULL`。
- `pProfileOptions`：输入/输出参数。必须非 `NULL`。函数会应用默认 xllm profile 字段。
- `pBootstrapOptions`：输入/输出参数。必须非 `NULL`。如果未设置 profile 列表，函数会指向 `pProfileOptions` 并设置 count 为 `1`。

**返回值：**

- `XWORK_OK`：应用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：任一参数为空。

**资源归属：**

函数不复制 `pProfileOptions`。如果把它写入 `pBootstrapOptions->pProfiles`，调用者必须保证该结构在 `xwork_runtime_create` 使用期间保持有效。

**补充说明：**

- 如果 `pBootstrapOptions->pProfiles` 已经非 `NULL`，函数不会覆盖它。
- 如果 `iProfileCount` 为 `0`，函数会设置为 `1`。

**范例代码：**

```c
#include "xwork.h"

int apply_bootstrap_profile(const xwork_profile *profile) {
    xwork_xllm_profile_options llm_profile;
    xwork_xllm_bootstrap_options bootstrap;
    xwork_xllm_profile_options_init(&llm_profile);
    xwork_xllm_bootstrap_options_init(&bootstrap);
    return xwork_profile_apply_xllm_bootstrap_options(
        profile,
        &llm_profile,
        &bootstrap
    ) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_xllm_bootstrap_options_init`
- `xwork_runtime_create`

---

### xwork_profile_apply_workspace_options

把 profile 的 workspace 默认值写入 workspace options。

**功能：**

你可以用该函数根据 profile 决定 workspace 是否默认启用 memory。

**函数原型：**

```c
XWORK_API xwork_status xwork_profile_apply_workspace_options(
    const xwork_profile *pProfile,
    xwork_workspace_options *pOptions
);
```

**参数：**

- `pProfile`：输入参数。必须非 `NULL`。
- `pOptions`：输入/输出参数。必须非 `NULL`。函数会写入 `bEnableMemory`。

**返回值：**

- `XWORK_OK`：应用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：profile 或 options 为空。

**资源归属：**

函数不分配资源，不设置或接管 `pOptions->pMemory`。

**补充说明：**

- 启用 memory 只设置布尔开关；调用方仍必须提供有效 `xllm_memory *`。
- 产品可在调用后继续覆盖 include/exclude 策略。

**范例代码：**

```c
#include "xwork.h"

int apply_workspace_profile(const xwork_profile *profile) {
    xwork_workspace_options options;
    xwork_workspace_options_init(&options);
    return xwork_profile_apply_workspace_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_workspace_options_init`
- `xwork_runtime_add_workspace`

---

### xwork_profile_apply_run_options

把 profile 的 run 默认值写入 run options。

**功能：**

你可以用该函数让 run 使用 profile 的默认模型 profile、session profile、自主性和 session policy。

**函数原型：**

```c
XWORK_API xwork_status xwork_profile_apply_run_options(
    const xwork_profile *pProfile,
    xwork_run_options *pOptions
);
```

**参数：**

- `pProfile`：输入参数。必须非 `NULL`。
- `pOptions`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：应用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：profile 或 options 为空。

**资源归属：**

函数不复制字符串。写入的 `sLlmProfileId` 和 `sSessionProfileId` 是借用指针。

**补充说明：**

- 如果 run options 已经设置非空 profile id，函数不会覆盖。
- 函数会覆盖 `eAutonomy` 和 `tSessionPolicy`。

**范例代码：**

```c
#include "xwork.h"

int apply_run_profile(const xwork_profile *profile) {
    xwork_run_options options;
    xwork_run_options_init(&options);
    return xwork_profile_apply_run_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_run_options_init`
- `xwork_run_create`

---

### xwork_profile_apply_orchestrator_options

把 profile 的 orchestrator 默认值写入 orchestrator options。

**功能：**

你可以用该函数让 orchestrator 使用 profile 的默认 max turns、planner mode 和 auto approve 设置。

**函数原型：**

```c
XWORK_API xwork_status xwork_profile_apply_orchestrator_options(
    const xwork_profile *pProfile,
    xwork_orchestrator_options *pOptions
);
```

**参数：**

- `pProfile`：输入参数。必须非 `NULL`。
- `pOptions`：输入/输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：应用成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：profile 或 options 为空。

**资源归属：**

函数不分配资源，不接管 options 中任何外部指针所有权。

**补充说明：**

- 只有 `iDefaultMaxTurns > 0` 时才会覆盖 `pOptions->iMaxTurns`。
- 函数会设置 `ePlannerMode` 和 `bAutoApprove`。

**范例代码：**

```c
#include "xwork.h"

int apply_orchestrator_profile(const xwork_profile *profile) {
    xwork_orchestrator_options options;
    xwork_orchestrator_options_init(&options);
    return xwork_profile_apply_orchestrator_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**相关 API：**

- `xwork_orchestrator_options_init`
- `xwork_run_execute`

## 错误处理

- `XWORK_ERROR_INVALID_ARGUMENT`：profile 或目标 options 指针为空。
- `XWORK_ERROR_NOT_FOUND`：内置 profile id 不存在。

## 恢复边界

profile 是配置数据，不承载 live 状态。恢复 run 时，调用方应重新应用与原 run 兼容的 profile/options，再加载 snapshot 或 persistence 数据。

## 线程边界

profile apply 函数只写入调用方传入的 options，不访问全局可变状态。并发安全取决于调用方是否并发修改同一个 options 结构。

## 相关文档

- [Runtime API](api-runtime.md)
- [Workspace API](api-workspace.md)
- [Run API](api-run.md)
- [AI IDE Agent 范例](../case/ai-ide-agent.md)
- [claw 自主 Agent 范例](../case/claw-autonomous-agent.md)
