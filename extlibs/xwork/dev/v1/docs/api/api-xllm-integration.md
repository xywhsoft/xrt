# xllm Integration API

> 状态：中文逐函数参考，待人工审阅。

xwork 通过 `xllm_runtime`、profile/bootstrap options、session policy、workspace memory 和 cancel token 与 xllm 集成。xwork 不实现 provider 协议适配，而是把 xllm 的模型能力放入可审批、可恢复的 run。

## 模块定位

xllm integration API 负责描述 xwork 如何借用或创建 `xllm_runtime`，如何配置 provider profile/transport，以及如何把 workspace memory、model stream event 和取消边界接入 agent run。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 外部不透明对象 | `xllm_runtime`, `xllm_session`, `xllm_memory`, `xllm_cancel_token` |
| 结构体 | `xwork_xllm_profile_options`, `xwork_xllm_transport_options`, `xwork_xllm_bootstrap_options`, `xwork_model_event` |
| 函数 | `xwork_xllm_transport_options_init`, `xwork_xllm_profile_options_init`, `xwork_xllm_bootstrap_options_init`, `xwork_runtime_get_llm_runtime` |

## 集成模式

| 模式 | 说明 |
| --- | --- |
| Borrow existing runtime | 调用方创建 `xllm_runtime`，通过 `xwork_runtime_options::pLlmRuntime` 借给 xwork。 |
| Bootstrap runtime | 调用方提供 `xwork_runtime_options::pLlmBootstrap`，xwork 在创建 runtime 时创建并拥有 xllm runtime。 |

`pLlmRuntime` 和 `pLlmBootstrap` 不能同时使用。

## 结构体字段

### xwork_xllm_profile_options

| 字段 | 说明 |
| --- | --- |
| `sProfileId` | xllm profile id。 |
| `sDisplayName` | 展示名。 |
| `sProvider` | provider 名称。 |
| `sAdapter` | adapter id，例如 `XWORK_XLLM_ADAPTER_OPENAI_COMPAT`。 |
| `sBaseUrl` | provider base URL。 |
| `sModelId` | 模型 id。 |
| `sApiKey` | API key。调用者负责保密和生命周期。 |
| `sOpenAIOrganizationId` | OpenAI-compatible organization id。 |
| `sOpenAIProjectId` | OpenAI-compatible project id。 |
| `sAnthropicApiVersion` | Anthropic API version。 |
| `psAnthropicBetaHeaders` | Anthropic beta header 数组。 |
| `iAnthropicBetaHeaderCount` | beta header 数量。 |
| `iMaxOutputTokens` | 默认最大输出 token 数。`0` 表示不由 xwork 设置。 |

### xwork_xllm_transport_options

| 字段 | 说明 |
| --- | --- |
| `bSetConnectTimeoutMs` / `iConnectTimeoutMs` | 是否设置连接超时和超时毫秒数。 |
| `bSetReadTimeoutMs` / `iReadTimeoutMs` | 是否设置读取超时和超时毫秒数。 |
| `bSetVerifyPeer` / `bVerifyPeer` | 是否设置 TLS peer verification。 |
| `eProxyKind` | proxy 类型。默认 `XWORK_XLLM_PROXY_UNSPECIFIED`。 |
| `sProxyHost` | proxy host。 |
| `bSetProxyPort` / `iProxyPort` | 是否设置 proxy port 和端口值。 |
| `sProxyUser` / `sProxyPass` | proxy 凭据。 |
| `sCaBundlePath` | CA bundle 路径。 |
| `sClientCertPath` / `sClientKeyPath` | mTLS client certificate/key 路径。 |

### xwork_xllm_bootstrap_options

| 字段 | 说明 |
| --- | --- |
| `pProfiles` | 借用的 xllm profile options 数组。 |
| `iProfileCount` | profile 数量。 |
| `eDebugMode` | xllm debug 输出模式。 |
| `eRedactMode` | 日志脱敏模式。 |
| `tTransportDefaults` | transport 默认值。 |

## Adapter 常量

- `XWORK_XLLM_ADAPTER_OPENAI_COMPAT`
- `XWORK_XLLM_ADAPTER_GLM_NATIVE`
- `XWORK_XLLM_ADAPTER_MINIMAX_NATIVE`
- `XWORK_XLLM_ADAPTER_KIMI_NATIVE`
- `XWORK_XLLM_ADAPTER_GEMINI_NATIVE`
- `XWORK_XLLM_ADAPTER_VERTEX_GEMINI_NATIVE`
- `XWORK_XLLM_ADAPTER_QWEN_NATIVE`
- `XWORK_XLLM_ADAPTER_DOUBAO_NATIVE`
- `XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE`
- `XWORK_XLLM_ADAPTER_OLLAMA_NATIVE`

## 所有权规则

- `xwork_runtime_options::pLlmRuntime` 是借用指针，调用者拥有并销毁。
- `xwork_runtime_options::pLlmBootstrap` 在 `xwork_runtime_create` 时被读取；若 bootstrap 创建 xllm runtime，xwork runtime 拥有并在销毁时释放它。
- `xwork_xllm_bootstrap_options::pProfiles` 是借用数组。
- `xwork_workspace_options::pMemory` 是借用 `xllm_memory *`。
- `xwork_runtime_get_llm_runtime` 返回借用指针。

---

### xwork_xllm_transport_options_init

初始化 xllm transport options。

**功能：**

你可以在配置 provider transport 前调用该函数，获得稳定默认值，再按需设置 timeout、TLS、proxy 和证书字段。

**函数原型：**

```c
XWORK_API void xwork_xllm_transport_options_init(xwork_xllm_transport_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；为 `NULL` 时不执行任何操作。非 `NULL` 时会清零并设置默认 proxy kind。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有字符串字段均为调用方提供的借用指针。

**补充说明：**

- 默认 `eProxyKind` 为 `XWORK_XLLM_PROXY_UNSPECIFIED`。
- `bSet*` 字段用于区分“未指定”和“显式设置为 0/false”。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_xllm_transport_options options;
    xwork_xllm_transport_options_init(&options);
    options.bSetReadTimeoutMs = true;
    options.iReadTimeoutMs = 60000u;
    return 0;
}
```

**相关 API：**

- `xwork_xllm_bootstrap_options_init`
- `xwork_runtime_create`

---

### xwork_xllm_profile_options_init

初始化 xllm profile options。

**功能：**

你可以在定义 provider profile 前调用该函数，清空所有模型、adapter、endpoint、credential 和 token 限制字段。

**函数原型：**

```c
XWORK_API void xwork_xllm_profile_options_init(xwork_xllm_profile_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；为 `NULL` 时不执行任何操作。非 `NULL` 时会整体清零。

**返回值：**

无。

**资源归属：**

函数不分配资源。所有字符串和 beta header 数组都是借用数据。

**补充说明：**

- `sApiKey` 由调用方负责保密和生命周期。
- 可以先应用 `xwork_profile_apply_xllm_profile_options`，再覆盖 provider 细节。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_xllm_profile_options profile;
    xwork_xllm_profile_options_init(&profile);
    profile.sProfileId = "default";
    profile.sAdapter = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
    profile.sModelId = "gpt-example";
    return 0;
}
```

**相关 API：**

- `xwork_profile_apply_xllm_profile_options`
- `xwork_xllm_bootstrap_options_init`

---

### xwork_xllm_bootstrap_options_init

初始化 xllm bootstrap options。

**功能：**

你可以在让 xwork 创建并拥有 xllm runtime 前调用该函数，设置 profile 列表、debug/redaction 策略和 transport defaults。

**函数原型：**

```c
XWORK_API void xwork_xllm_bootstrap_options_init(xwork_xllm_bootstrap_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；为 `NULL` 时不执行任何操作。非 `NULL` 时清零并初始化默认 debug/redaction/transport。

**返回值：**

无。

**资源归属：**

函数不分配资源。`pProfiles` 是借用数组，调用者必须保证其在 runtime create 期间有效。

**补充说明：**

- 默认 `eDebugMode` 为 `XWORK_XLLM_DEBUG_NONE`。
- 默认 `eRedactMode` 为 `XWORK_XLLM_REDACT_DEFAULT`。
- 函数会调用 `xwork_xllm_transport_options_init` 初始化 `tTransportDefaults`。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_xllm_profile_options profile;
    xwork_xllm_bootstrap_options bootstrap;

    xwork_xllm_profile_options_init(&profile);
    profile.sProfileId = "default";

    xwork_xllm_bootstrap_options_init(&bootstrap);
    bootstrap.pProfiles = &profile;
    bootstrap.iProfileCount = 1u;
    return 0;
}
```

**相关 API：**

- `xwork_runtime_options_init`
- `xwork_runtime_create`
- `xwork_profile_apply_xllm_bootstrap_options`

---

### xwork_runtime_get_llm_runtime

获取 runtime 当前绑定的 `xllm_runtime`。

**功能：**

你可以用该函数在诊断、host integration 或高级产品逻辑中访问 xwork runtime 内部使用的 xllm runtime。

**函数原型：**

```c
XWORK_API xllm_runtime *xwork_runtime_get_llm_runtime(const xwork_runtime *pRuntime);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；为 `NULL` 时返回 `NULL`。

**返回值：**

- 返回借用的 `xllm_runtime *`。
- 如果 runtime 无效或没有绑定 xllm runtime，返回 `NULL`。

**资源归属：**

返回值是借用指针。不要通过该指针销毁 runtime；所有权仍由原始调用方或 xwork runtime 持有。

**补充说明：**

- 如果 runtime 通过 `pLlmRuntime` 创建，返回的是调用方借给 xwork 的对象。
- 如果 runtime 通过 `pLlmBootstrap` 创建，返回的是 xwork runtime 拥有的对象。
- 指针在 `xwork_runtime_destroy` 后失效。

**范例代码：**

```c
#include "xwork.h"

int has_llm_runtime(const xwork_runtime *runtime) {
    return xwork_runtime_get_llm_runtime(runtime) != NULL ? 1 : 0;
}
```

**相关 API：**

- `xwork_runtime_create`
- `xwork_runtime_destroy`
- `xwork_xllm_bootstrap_options_init`

## Model events

xwork 将 xllm stream event 转为 `xwork_model_event` 并转发给 callback。callback 返回 `false` 会取消当前 model turn，并通过 `XWORK_ERROR_CANCELLED` 传播。interrupt/cancel token 检查先于用户 callback。

## Workspace memory

`xwork_workspace_options::pMemory` 是 borrowed `xllm_memory *`。workspace memory sync 会调用 xllm memory 能力同步 workspace root 或单个文件。恢复 run 时，memory 对象不会从 snapshot 自动恢复；调用方应重新创建或加载兼容 memory，再注册 workspace。

## 错误处理

本页的 init/getter 函数不返回 `xwork_status`。实际 bootstrap 错误通过 `xwork_runtime_create` 返回，例如参数无效、bootstrap 失败或底层 xllm provider 初始化失败。

## 恢复边界

run snapshot 不保存 live `xllm_runtime`、`xllm_session`、`xllm_memory` 或 cancel token。恢复时必须重新建立兼容的 xllm runtime/profile/session/workspace memory，再加载 run 或 task 状态。

## 线程边界

xwork 不声明 `xllm_runtime` 或 `xllm_memory` 可被任意并发 mutation。对同一 runtime/session/memory 的写入、sync、model turn 和 cancellation 边界应由宿主或 orchestrator 串行化。

## 相关文档

- [Runtime API](api-runtime.md)
- [Workspace API](api-workspace.md)
- [Orchestrator API](api-orchestrator.md)
- [xllm 编排与工具循环](../guide/xllm-orchestrator-intro.md)
