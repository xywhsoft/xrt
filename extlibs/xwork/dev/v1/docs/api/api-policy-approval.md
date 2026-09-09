# Policy / Approval API

> 状态：中文逐函数参考，待人工审阅。

Policy / Approval API 定义 xwork 在执行文件、进程、网络、远程 worker 和 replay 副作用之前的统一安全边界。

## 模块定位

Policy 决定“是否允许、是否需要审批、是否可自动审批”。Approval request 是 run 暂停后暴露给产品 UI、CLI 或自动策略的审计对象。本模块不实现 UI、账号权限、worker 认证或 socket 访问控制；这些仍由宿主产品负责。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 结构体 | `xwork_policy_options`, `xwork_approval_eval_input`, `xwork_approval_decision`, `xwork_network_policy_eval_input`, `xwork_network_policy_decision`, `xwork_approval_request` |
| 函数 | `xwork_policy_options_init`, `xwork_approval_eval_input_init`, `xwork_approval_decision_init`, `xwork_network_policy_eval_input_init`, `xwork_network_policy_decision_init`, `xwork_approval_request_init`, `xwork_approval_request_reset`, `xwork_policy_evaluate_approval`, `xwork_policy_evaluate_network_access` |

## 核心策略

| 字段/对象 | 说明 |
| --- | --- |
| `eAutoApproveRiskLimit` | 自动审批允许的最高风险级别。 |
| `psNetworkAllowHostPatterns` | 网络 host allowlist。数组和字符串均为 borrowed。 |
| `psNetworkDenyHostPatterns` | 网络 host denylist，优先于 allowlist。 |
| `bDenyNetworkByDefault` | 无 allowlist 命中时是否默认拒绝网络访问。 |
| `xwork_approval_decision` | 评估后的 allow/require approval/risk/scope/reason。 |
| `xwork_approval_request` | run 暂停后给产品层展示和提交的审批对象。 |

## 所有权规则

- policy options 会按值复制到 runtime/control-plane 等 options 中，但 allow/deny pattern 数组及字符串是 borrowed。
- eval input 中的 tool id、scope、reason、URL、host 等字符串都是 borrowed。
- decision 中的字符串指向静态文本或 input-owned override 文本；如果需要长期保存，调用方应复制。
- approval request init 不分配资源；查询/加载得到的 request 拥有 deep-copy 字段，使用后必须 reset。

---

### xwork_policy_options_init

初始化 policy options。

**功能：**

用于创建 runtime、control plane 或独立评估前设置默认安全策略。

**函数原型：**

```c
XWORK_API void xwork_policy_options_init(xwork_policy_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；非 `NULL` 时写入默认 policy。

**返回值：**

无。

**资源归属：**

函数不分配资源。allow/deny 数组字段默认 `NULL`，由调用方后续提供并保持生命周期。

**补充说明：**

- 建议先 init，再应用 profile，再按产品安全要求覆盖。
- 默认策略适合开发期基线，不应替代产品最终安全策略。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_policy_options policy;
    xwork_policy_options_init(&policy);
    policy.bDenyNetworkByDefault = true;
    return 0;
}
```

**相关 API：**

- `xwork_policy_evaluate_approval`
- `xwork_policy_evaluate_network_access`

---

### xwork_approval_eval_input_init

初始化审批评估输入。

**功能：**

用于描述一次工具、远程任务或 side effect 的审批上下文。

**函数原型：**

```c
XWORK_API void xwork_approval_eval_input_init(xwork_approval_eval_input *pInput);
```

**参数：**

- `pInput`：输出参数。可为 `NULL`；非 `NULL` 时写入默认 autonomy、approval mode、side effect 和 risk。

**返回值：**

无。

**资源归属：**

函数不分配资源。输入中的字符串字段由调用方借用提供。

**补充说明：**

- 可通过 risk override 字段提供比默认 side-effect 映射更准确的风险说明。

**范例代码：**

```c
xwork_approval_eval_input input;
xwork_approval_eval_input_init(&input);
input.eSideEffect = XWORK_SIDE_EFFECT_PROCESS_EXEC;
```

**相关 API：**

- `xwork_policy_evaluate_approval`

---

### xwork_approval_decision_init

初始化审批评估结果。

**功能：**

准备接收 `xwork_policy_evaluate_approval` 的输出。

**函数原型：**

```c
XWORK_API void xwork_approval_decision_init(xwork_approval_decision *pDecision);
```

**参数：**

- `pDecision`：输出参数。可为 `NULL`；非 `NULL` 时清零并写入默认风险级别。

**返回值：**

无。

**资源归属：**

函数不分配资源。decision 字符串为 borrowed。

**补充说明：**

- decision 不需要 reset。

**范例代码：**

```c
xwork_approval_decision decision;
xwork_approval_decision_init(&decision);
```

**相关 API：**

- `xwork_policy_evaluate_approval`

---

### xwork_network_policy_eval_input_init

初始化网络策略评估输入。

**功能：**

用于描述一次网络访问请求的 URL/host 上下文。

**函数原型：**

```c
XWORK_API void xwork_network_policy_eval_input_init(
    xwork_network_policy_eval_input *pInput
);
```

**参数：**

- `pInput`：输出参数。可为 `NULL`；非 `NULL` 时清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。URL 和 host 字符串由调用方借用提供。

**补充说明：**

- 未请求网络访问时，评估通常允许通过。

**范例代码：**

```c
xwork_network_policy_eval_input input;
xwork_network_policy_eval_input_init(&input);
input.bNetworkAccessRequested = true;
input.sHost = "api.example.com";
```

**相关 API：**

- `xwork_policy_evaluate_network_access`

---

### xwork_network_policy_decision_init

初始化网络策略评估结果。

**功能：**

准备接收 `xwork_policy_evaluate_network_access` 的输出。

**函数原型：**

```c
XWORK_API void xwork_network_policy_decision_init(
    xwork_network_policy_decision *pDecision
);
```

**参数：**

- `pDecision`：输出参数。可为 `NULL`；非 `NULL` 时清零并写入默认风险级别。

**返回值：**

无。

**资源归属：**

函数不分配资源。decision 字符串为 borrowed。

**补充说明：**

- decision 不需要 reset。

**范例代码：**

```c
xwork_network_policy_decision decision;
xwork_network_policy_decision_init(&decision);
```

**相关 API：**

- `xwork_policy_evaluate_network_access`

---

### xwork_approval_request_init

初始化 approval request。

**功能：**

准备接收 run 或 persistence 返回的审批请求。

**函数原型：**

```c
XWORK_API void xwork_approval_request_init(xwork_approval_request *pRequest);
```

**参数：**

- `pRequest`：输出参数。可为 `NULL`；非 `NULL` 时清零并设置默认风险/状态。

**返回值：**

无。

**资源归属：**

函数不分配资源。填充后的 request 拥有 deep-copy 字段，使用后调用 `xwork_approval_request_reset`。

**补充说明：**

- request 是审计对象，通常展示给 UI/CLI 后由产品调用 `xwork_run_submit_approval`。

**范例代码：**

```c
xwork_approval_request request;
xwork_approval_request_init(&request);
xwork_approval_request_reset(&request);
```

**相关 API：**

- `xwork_run_get_last_approval_request`
- `xwork_approval_request_reset`

---

### xwork_approval_request_reset

释放并重置 approval request。

**功能：**

释放 request 中的 id、run id、tool id、reason、scope、action summary 等 deep-copy 字段。

**函数原型：**

```c
XWORK_API void xwork_approval_request_reset(xwork_approval_request *pRequest);
```

**参数：**

- `pRequest`：输入/输出参数。可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 request 拥有的字符串副本。

**补充说明：**

- reset 后 request 回到 init 状态。

**范例代码：**

```c
xwork_approval_request_reset(&request);
```

**相关 API：**

- `xwork_approval_request_init`

---

### xwork_policy_evaluate_approval

评估一次 side effect 是否允许或需要审批。

**功能：**

根据 autonomy、approval mode、side effect、risk override 和 auto-approve limit 生成统一审批决策。

**函数原型：**

```c
XWORK_API xwork_status xwork_policy_evaluate_approval(
    const xwork_policy_options *pPolicy,
    const xwork_approval_eval_input *pInput,
    xwork_approval_decision *pDecision
);
```

**参数：**

- `pPolicy`：输入参数。可为 `NULL`；为 `NULL` 时使用默认 policy。
- `pInput`：输入参数。必须非 `NULL`。
- `pDecision`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：评估成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：input 或 decision 为空，或枚举值无效。

**资源归属：**

decision 字符串是 borrowed；函数不分配需要调用者释放的资源。

**补充说明：**

- `XWORK_APPROVAL_ALWAYS` 会强制需要审批。
- `XWORK_APPROVAL_NEVER` 会跳过审批，但产品层仍可额外加 gate。
- `XWORK_APPROVAL_DEFAULT` 会结合 autonomy、side effect 和风险阈值判断。

**范例代码：**

```c
xwork_policy_options policy;
xwork_approval_eval_input input;
xwork_approval_decision decision;

xwork_policy_options_init(&policy);
xwork_approval_eval_input_init(&input);
xwork_approval_decision_init(&decision);
input.eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
input.eSideEffect = XWORK_SIDE_EFFECT_WORKSPACE_WRITE;

(void)xwork_policy_evaluate_approval(&policy, &input, &decision);
```

**相关 API：**

- `xwork_run_submit_approval`
- `xwork_policy_evaluate_network_access`

---

### xwork_policy_evaluate_network_access

评估一次网络访问是否允许。

**功能：**

根据 network requested 标志、URL/host、denylist、allowlist 和默认拒绝策略生成网络访问决策。

**函数原型：**

```c
XWORK_API xwork_status xwork_policy_evaluate_network_access(
    const xwork_policy_options *pPolicy,
    const xwork_network_policy_eval_input *pInput,
    xwork_network_policy_decision *pDecision
);
```

**参数：**

- `pPolicy`：输入参数。可为 `NULL`；为 `NULL` 时使用默认 policy。
- `pInput`：输入参数。必须非 `NULL`。
- `pDecision`：输出参数。必须非 `NULL`。

**返回值：**

- `XWORK_OK`：评估成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：input 或 decision 为空。

**资源归属：**

decision 字符串是 borrowed；函数不分配需要调用者释放的资源。

**补充说明：**

- deny patterns 优先于 allow patterns。
- 配置 allow patterns 后，host 必须命中 allowlist。
- 未配置 allow patterns 时取决于 `bDenyNetworkByDefault`。

**范例代码：**

```c
xwork_network_policy_eval_input input;
xwork_network_policy_decision decision;

xwork_network_policy_eval_input_init(&input);
xwork_network_policy_decision_init(&decision);
input.bNetworkAccessRequested = true;
input.sHost = "api.example.com";

(void)xwork_policy_evaluate_network_access(NULL, &input, &decision);
```

**相关 API：**

- `xwork_policy_options_init`
- `xwork_policy_evaluate_approval`

## 错误处理

- `XWORK_ERROR_INVALID_ARGUMENT`：policy/input/decision 指针无效或 enum 无效。
- `XWORK_ERROR_INVALID_STATE`：run 当前状态不能提交审批。
- `XWORK_ERROR_NOT_FOUND`：没有可提交的审批请求。

## 恢复边界

approval request 可以通过 run snapshot/persistence 恢复为审计对象。policy callback、UI 状态、用户会话和账号权限不属于 xwork 恢复范围。

## 线程边界

policy evaluate 函数不修改全局状态；并发安全取决于调用方是否并发修改传入的 policy/input storage。

## 相关文档

- [Tool API](api-tools.md)
- [Run API](api-run.md)
- [Orchestrator API](api-orchestrator.md)
- [工具、审批与 artifact](../guide/tool-approval-artifact-intro.md)
- [内部 policy contract](../../dev/docs/POLICY_APPROVAL.md)
