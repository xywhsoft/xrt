# xllm Providers API

> Provider API 解释如何选择、注册和配置不同模型服务的 adapter 与 profile。

[返回 API 索引](README.md) | [Core Runtime API](api-core.md) | [基础类型](types.md)

---

## 目录

- [模块定位](#模块定位)
- [Adapter 与 Profile 的关系](#adapter-与-profile-的关系)
- [Provider 常量](#provider-常量)
- [Profile 配置类型](#profile-配置类型)
  - [xllm_profile_init](#xllm_profile_init)
  - [xllm_profile](#xllm_profile)
  - [xllm_auth](#xllm_auth)
  - [xllm_provider_options](#xllm_provider_options)
  - [xllm_transport_options](#xllm_transport_options)
  - [xllm_profile_models](#xllm_profile_models)
  - [xllm_model_caps](#xllm_model_caps)
- [内置 Provider Adapter](#内置-provider-adapter)
- [能力矩阵](#能力矩阵)
- [稳定不支持项](#稳定不支持项)
- [Profile 示例](#profile-示例)
- [常见错误](#常见错误)

---

## 模块定位

xllm 把 provider 接入拆成两层：

- **Adapter**：负责把 xllm 请求/响应模型转换成某个 provider 的 wire protocol。
- **Profile**：负责保存某个具体调用配置，包括 adapter、endpoint、认证、模型 ID、能力、默认生成参数和 provider 专有选项。

你通常先注册 adapter，再注册一个或多个 profile。调用时只在 request 里写 `sProfileId`，不需要每次都重复 endpoint、key、模型和能力配置。

---

## Adapter 与 Profile 的关系

```text
xllm_runtime
  adapter: openai_compat
  adapter: anthropic_native
  profile: openai-fast -> adapter=openai_compat, model=gpt-4.1-mini
  profile: local-ollama -> adapter=ollama_native, model=llama3.2
```

同一个 adapter 可以服务多个 profile。例如你可以用 `openai_compat` 同时配置 OpenAI、Azure OpenAI 或第三方兼容网关。

---

## Provider 常量

| 常量 | Adapter 名称 | 说明 |
| --- | --- | --- |
| `XLLM_ADAPTER_OPENAI_COMPAT` | `openai_compat` | OpenAI 兼容协议。 |
| `XLLM_ADAPTER_GLM_NATIVE` | `glm_native` | GLM 原生协议。 |
| `XLLM_ADAPTER_MINIMAX_NATIVE` | `minimax_native` | MiniMax 原生协议。 |
| `XLLM_ADAPTER_KIMI_NATIVE` | `kimi_native` | Kimi 原生协议。 |
| `XLLM_ADAPTER_GEMINI_NATIVE` | `gemini_native` | Gemini 原生协议。 |
| `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | `vertex_gemini_native` | Vertex Gemini 原生协议。 |
| `XLLM_ADAPTER_QWEN_NATIVE` | `qwen_native` | Qwen 原生协议。 |
| `XLLM_ADAPTER_DOUBAO_NATIVE` | `doubao_native` | Doubao 原生协议。 |
| `XLLM_ADAPTER_ANTHROPIC_NATIVE` | `anthropic_native` | Anthropic 原生协议。 |
| `XLLM_ADAPTER_OLLAMA_NATIVE` | `ollama_native` | Ollama 原生协议。 |

---

## Profile 配置类型

### xllm_profile_init

初始化 profile。

**功能：**

你在栈上创建 `xllm_profile` 后，先调用它获得安全默认值，再填写 ID、adapter、endpoint、认证和模型。

**函数原型：**

```c
XLLM_API void xllm_profile_init(xllm_profile *pProfile);
```

**参数：**

| 参数 | 方向 | 是否可为 `NULL` | 说明 |
| --- | --- | --- | --- |
| `pProfile` | 输出 | 可为 `NULL` | 要初始化的 profile。 |

**返回值：**

无。

**资源归属：**

不分配资源。`pProfile` 由调用者持有。

**补充说明：**

初始化后：

- `tAuth.eKind = XLLM_AUTH_NONE`
- `tModels.tText.eCapMode = XLLM_CAP_MODE_AUTO`
- `tModels.tMultimodal.eCapMode = XLLM_CAP_MODE_AUTO`

**范例代码：**

```c
xllm_profile profile;
xllm_profile_init(&profile);
profile.sId = "demo";
profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
```

**相关 API：**

- `xllm_register_profile`

---

### xllm_profile

profile 是 provider 调用配置的主结构。

| 字段 | 是否必填 | 说明 |
| --- | --- | --- |
| `sId` | 是 | profile 唯一 ID。请求通过它选择配置。 |
| `sName` | 否 | 展示名称。 |
| `sProvider` | 建议填写 | provider 名称，如 `openai`、`anthropic`、`ollama`。 |
| `sAdapter` | 是 | adapter 名称，通常使用 `XLLM_ADAPTER_*`。 |
| `sBaseUrl` | 多数 provider 必填 | endpoint base URL。 |
| `tAuth` | 视 provider 而定 | 认证配置。 |
| `pDefaultHeaders` / `iDefaultHeaderCount` | 可选 | 默认 HTTP header。 |
| `tProviderOptions` | 可选 | provider 专有选项。 |
| `tTransport` | 可选 | 超时、代理、TLS 等传输选项。 |
| `tModels` | 是 | 文本/多模态模型绑定。 |
| `tDefaults` | 可选 | 默认生成参数、reasoning、response format。 |
| `tVendorExtra` | 可选 | 扩展字段。 |

**资源归属：**

`xllm_register_profile` 会克隆 profile。注册成功后，原始 profile 可以离开作用域。

---

### xllm_auth

认证配置。

| 字段 | 说明 |
| --- | --- |
| `eKind` | 认证方式。 |
| `sSecret` | API key、token 或其他密钥。 |
| `sHeaderName` | API key header 名。 |
| `sScheme` | 自定义 Authorization scheme。 |

| `xllm_auth_kind` | 说明 |
| --- | --- |
| `XLLM_AUTH_NONE` | 不发送认证。 |
| `XLLM_AUTH_BEARER` | 发送 `Authorization: Bearer <secret>`。 |
| `XLLM_AUTH_API_KEY_HEADER` | 发送自定义 API key header。 |

**补充说明：**

- 不要把真实密钥写入仓库。
- 日志和 trace 应配合 `XLLM_REDACT_DEFAULT` 或 `XLLM_REDACT_STRICT`。

---

### xllm_provider_options

provider 专有选项。

| 字段 | 说明 |
| --- | --- |
| `sOpenAIOrganizationId` | OpenAI organization header。 |
| `sOpenAIProjectId` | OpenAI project header。 |
| `sAnthropicApiVersion` | Anthropic API version。 |
| `psAnthropicBetaHeaders` / `iAnthropicBetaHeaderCount` | Anthropic beta headers。 |
| `tVendorExtra` | 其他 provider 扩展。 |

**补充说明：**

这些字段只在对应 adapter 中有意义。对不相关 provider 设置这些字段通常不会产生效果。

---

### xllm_transport_options

传输配置。

| 字段 | 说明 |
| --- | --- |
| `tConnectTimeoutMs` / `tReadTimeoutMs` | 连接/读取超时，单位毫秒。 |
| `tVerifyPeer` | 是否验证 TLS peer。 |
| `eProxyKind` | 代理类型。 |
| `sProxyHost` / `tProxyPort` | 代理地址和端口。 |
| `sProxyUser` / `sProxyPass` | 代理认证。 |
| `sCaBundlePath` | CA bundle 路径。 |
| `sClientCertPath` / `sClientKeyPath` | 客户端证书和私钥。 |
| `tVendorExtra` | 扩展字段。 |

**建议：**

- 交互式请求可从 30-90 秒超时开始。
- 长代码生成或 reasoning 请求可使用 120-300 秒。
- 多模态上传请求通常需要更长超时。

---

### xllm_profile_models

模型绑定。

| 字段 | 说明 |
| --- | --- |
| `tText` | 文本模型绑定。 |
| `tMultimodal` | 多模态模型绑定。 |

`xllm_model_binding` 字段：

| 字段 | 说明 |
| --- | --- |
| `sModelId` | provider 使用的模型 ID。 |
| `sAliasOf` | 别名来源，可选。 |
| `eCapMode` | 能力声明模式。 |
| `tCaps` | 模型能力。 |
| `tVendorExtra` | 扩展字段。 |

---

### xllm_model_caps

模型能力。

| 字段 | 说明 |
| --- | --- |
| `uFlags` | `XLLM_CAP_*` 能力位。 |
| `psSupportedMimeTypes` / `iSupportedMimeTypeCount` | 支持 MIME 类型。 |
| `uMaxContextTokens` | 最大上下文。 |
| `uMaxInputTokens` / `uMaxOutputTokens` | 输入/输出 token 限制。 |
| `uRecommendedOutputReserve` | 推荐输出预留。 |
| `uMaxPartsPerMessage` | 单条消息最多 part 数。 |
| `uMaxImages` / `uMaxFiles` | 图片/文件数量限制。 |
| `uMaxPartBytes` | 单 part 字节限制。 |
| `sTokenizerId` | tokenizer 标识。 |
| 参数规则字段 | temperature、top_p、max output tokens 限制。 |

**补充说明：**

能力声明越准确，`xllm_validate_request` 越能在发起网络请求前发现错误。

---

## 内置 Provider Adapter

这些函数的详细注册语义见 [api-core.md](api-core.md#内置-adapter-注册函数)。

| 注册函数 | 常量 | 适用场景 |
| --- | --- | --- |
| `xllm_register_openai_compat_adapter` | `XLLM_ADAPTER_OPENAI_COMPAT` | OpenAI、Azure OpenAI、OpenAI-compatible 网关。 |
| `xllm_register_anthropic_native_adapter` | `XLLM_ADAPTER_ANTHROPIC_NATIVE` | Anthropic Messages API。 |
| `xllm_register_ollama_native_adapter` | `XLLM_ADAPTER_OLLAMA_NATIVE` | 本地或自托管 Ollama。 |
| `xllm_register_gemini_native_adapter` | `XLLM_ADAPTER_GEMINI_NATIVE` | Gemini API。 |
| `xllm_register_vertex_gemini_native_adapter` | `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | Vertex Gemini。 |
| `xllm_register_glm_native_adapter` | `XLLM_ADAPTER_GLM_NATIVE` | GLM 原生接口。 |
| `xllm_register_minimax_native_adapter` | `XLLM_ADAPTER_MINIMAX_NATIVE` | MiniMax 原生接口。 |
| `xllm_register_kimi_native_adapter` | `XLLM_ADAPTER_KIMI_NATIVE` | Kimi 原生接口。 |
| `xllm_register_qwen_native_adapter` | `XLLM_ADAPTER_QWEN_NATIVE` | Qwen 原生接口。 |
| `xllm_register_doubao_native_adapter` | `XLLM_ADAPTER_DOUBAO_NATIVE` | Doubao 原生接口。 |

---

## 能力矩阵

下表是当前已归档能力基线的学习版摘要。它表示 xllm adapter 层的实现和稳定测试基线，不保证上游每个模型、地区、账号或网关都支持同样能力。

| Adapter | 文本 | 流式 | Tool call | Tool result | 图片 URL | inline 图片 | 图片 file_id | 文件 URL | inline 文件 | 文件 file_id | JSON/schema | 主要注意事项 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `openai_compat` | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 真实能力取决于具体兼容端点和模型。 |
| `azure_openai` via `openai_compat` | 支持 | 支持 | 支持 | 支持 | 支持 | 端点敏感 | 支持 | 支持 | 支持 | 支持 | 支持 | 大 inline 图片可能受网关限制。 |
| `anthropic_native` | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 部分 | 部分 | 支持 | 部分 | strict JSON schema 是稳定不支持项。 |
| `ollama_native` | 支持 | 支持 | 支持 | 未稳定基线 | 支持 | 支持 | 不支持 | 不支持 | 不支持 | 不支持 | 部分 | `tool_choice=required`、文件输入和 image file_id 不支持。 |
| `glm_native` | 支持 | 未稳定基线 | 支持 | 未稳定基线 | 支持 | 端点敏感 | 不支持 | 不支持 | 不支持 | 不支持 | 未稳定基线 | 文件输入和 image file_id 不支持。 |
| `minimax_native` | 支持 | 未稳定基线 | 支持 | 未稳定基线 | 支持 | 端点敏感 | 不支持 | 不支持 | 不支持 | 不支持 | 未稳定基线 | 文件输入和 image file_id 不支持。 |
| `kimi_native` | 支持 | 未稳定基线 | 支持 | 未稳定基线 | 支持 | 端点敏感 | 支持 | 不支持 | 不支持 | 不支持 | 未稳定基线 | 文件输入不支持。 |
| `qwen_native` | 支持 | 未稳定基线 | 支持 | 支持 | 支持 | 端点敏感 | 不支持 | 不支持 | 不支持 | 不支持 | 未稳定基线 | thinking 下强制 named tool 不支持。 |
| `doubao_native` | 支持 | 未稳定基线 | 支持 | 支持 | 支持 | 端点敏感 | 不支持 | 不支持 | 不支持 | 不支持 | 未稳定基线 | 文件输入和 image file_id 不支持。 |
| `gemini_native` | 支持 | 未稳定基线 | 支持 | 部分 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | tool result 目前以 text/json 为主。 |
| `vertex_gemini_native` | 支持 | 未稳定基线 | 支持 | 部分 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 支持 | 与 Gemini tool result 限制类似。 |

---

## 稳定不支持项

以下限制属于回归测试表面。宿主不应假设这些能力“可能偷偷可用”。

| Adapter | 不支持项 | 典型错误 |
| --- | --- | --- |
| `anthropic_native` | strict `json_schema` 输出 | `XLLM_ERROR_UNSUPPORTED_CAPABILITY` |
| `ollama_native` | `tool_choice=required` | `XLLM_ERROR_UNSUPPORTED_CAPABILITY` |
| `ollama_native` | 文件输入、图片 `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `glm_native` | 文件输入、图片 `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `minimax_native` | 文件输入、图片 `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `kimi_native` | 文件输入 | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `qwen_native` | thinking 模式下强制 named tool、文件输入、图片 `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_CAPABILITY` 或 `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `doubao_native` | 文件输入、图片 `provider_file_id` | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |
| `gemini_native` / `vertex_gemini_native` | tool result 图片/文件 part | `XLLM_ERROR_UNSUPPORTED_INPUT_TYPE` |

---

## Profile 示例

### OpenAI-compatible 文本 profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

profile.sId = "openai-text";
profile.sProvider = "openai";
profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
profile.sBaseUrl = "https://api.openai.com/v1";
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("OPENAI_API_KEY");
profile.tModels.tText.sModelId = "gpt-4.1-mini";
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM |
    XLLM_CAP_JSON_OUT |
    XLLM_CAP_TOOL_CALL_OUT;

xllm_register_openai_compat_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

### Ollama 本地 profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

profile.sId = "ollama-local";
profile.sProvider = "ollama";
profile.sAdapter = XLLM_ADAPTER_OLLAMA_NATIVE;
profile.sBaseUrl = "http://127.0.0.1:11434";
profile.tAuth.eKind = XLLM_AUTH_NONE;
profile.tModels.tText.sModelId = "llama3.2";
profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

xllm_register_ollama_native_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

### Gemini 多模态 profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

profile.sId = "gemini-mm";
profile.sProvider = "gemini";
profile.sAdapter = XLLM_ADAPTER_GEMINI_NATIVE;
profile.sBaseUrl = "https://generativelanguage.googleapis.com";
profile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
profile.tAuth.sHeaderName = "x-goog-api-key";
profile.tAuth.sSecret = getenv("GEMINI_API_KEY");
profile.tModels.tText.sModelId = "gemini-2.0-flash";
profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
profile.tModels.tMultimodal.sModelId = "gemini-2.0-flash";
profile.tModels.tMultimodal.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_IMAGE_IN |
    XLLM_CAP_FILE_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_JSON_OUT;

xllm_register_gemini_native_adapter(runtime);
xllm_register_profile(runtime, &profile);
```

---

## 常见错误

| 问题 | 常见原因 | 处理方式 |
| --- | --- | --- |
| profile 注册成功但请求失败 | 没有先注册 adapter，或 `sAdapter` 拼写不一致。 | 使用 `XLLM_ADAPTER_*` 常量，先注册 adapter 再注册 profile。 |
| 多模态请求被拒绝 | 只配置了 `tText`，没有配置 `tMultimodal` 或能力位。 | 配置多模态模型和 `XLLM_CAP_IMAGE_IN` / `XLLM_CAP_FILE_IN`。 |
| provider 返回 401 | `tAuth` 配置错误或密钥为空。 | 检查 `eKind`、header 名和环境变量。 |
| OpenAI-compatible 网关不支持某字段 | 兼容协议不代表完整 OpenAI 能力。 | 用 capability flags 限制宿主 UI，必要时降级。 |
| 日志泄露密钥风险 | debug 开太高且关闭脱敏。 | 使用 `XLLM_REDACT_DEFAULT` 或 `XLLM_REDACT_STRICT`。 |

## 相关示例

- `examples\openai\azure_openai\azure_openai_stateless.c`
- `examples\gemini\gemini_multimodal.c`
- `examples\anthropic\glm\glm_stateless.c`
- `examples\qwen\qwen_tool_loop.c`
- `examples\smoke_openai_compat_adapter.c`
- `examples\smoke_ollama_native_adapter.c`
