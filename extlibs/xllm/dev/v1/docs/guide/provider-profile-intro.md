# Provider 与 Profile 入门

> 状态：中文初稿已生成，待审阅。

学习 xllm 时，最容易混淆的是 provider、adapter、profile、model 这几个词。本篇把它们拆开讲清楚，并告诉你什么时候该新增 profile、什么时候该换 adapter。

## 一句话理解

- **provider**：服务提供方，例如 OpenAI、GLM、Gemini、Ollama。
- **adapter**：xllm 里负责对接某种协议的代码。
- **profile**：你注册到 runtime 的一份具体调用配置。
- **model binding**：profile 里声明“文本、视觉、多模态”等槽位使用哪个模型。

可以这样记：

```text
adapter 负责“怎么发请求”
profile 负责“发到哪里、用什么密钥、用哪个模型”
```

## adapter 和 profile 的关系

一个 adapter 可以对应多个 profile。例如你可以用同一个 `openai_compat` adapter 注册：

- 一个 OpenAI profile。
- 一个 Azure OpenAI profile。
- 一个本地 OpenAI-compatible gateway profile。

每个 profile 都有自己的 `sId`。调用时你不是直接选择 adapter，而是选择 profile。

## 常见内置 adapter

| Adapter 常量 | 适合场景 |
| --- | --- |
| `XLLM_ADAPTER_OPENAI_COMPAT` | OpenAI-compatible 接口。 |
| `XLLM_ADAPTER_GLM_NATIVE` | GLM 原生接口。 |
| `XLLM_ADAPTER_MINIMAX_NATIVE` | MiniMax 原生接口。 |
| `XLLM_ADAPTER_KIMI_NATIVE` | Kimi 原生接口。 |
| `XLLM_ADAPTER_GEMINI_NATIVE` | Gemini 原生接口。 |
| `XLLM_ADAPTER_VERTEX_GEMINI_NATIVE` | Vertex Gemini。 |
| `XLLM_ADAPTER_QWEN_NATIVE` | Qwen 原生接口。 |
| `XLLM_ADAPTER_DOUBAO_NATIVE` | Doubao 原生接口。 |
| `XLLM_ADAPTER_ANTHROPIC_NATIVE` | Anthropic 原生接口。 |
| `XLLM_ADAPTER_OLLAMA_NATIVE` | Ollama 本地或远程服务。 |

对应注册函数形如：

```c
xllm_register_openai_compat_adapter(runtime);
xllm_register_glm_native_adapter(runtime);
xllm_register_gemini_native_adapter(runtime);
```

## profile 里最重要的字段

```c
typedef struct {
    const char *sId;
    const char *sName;
    const char *sProvider;
    const char *sAdapter;
    const char *sBaseUrl;
    xllm_auth tAuth;
    xllm_header *pDefaultHeaders;
    size_t iDefaultHeaderCount;
    xllm_provider_options tProviderOptions;
    xllm_transport_options tTransport;
    xllm_profile_models tModels;
    xllm_profile_defaults tDefaults;
    xvalue tVendorExtra;
} xllm_profile;
```

初学阶段先关注这些字段：

| 字段 | 什么时候要填 |
| --- | --- |
| `sId` | 必填。后续绑定 profile 时使用。 |
| `sProvider` | 建议填写，便于日志和诊断。 |
| `sAdapter` | 必填。必须和已注册 adapter 匹配。 |
| `sBaseUrl` | 真实 provider 通常必填。 |
| `tAuth` | 需要鉴权时填写。 |
| `tModels.tText.sModelId` | 文本调用使用的模型。 |
| `tModels.tText.tCaps.uFlags` | 声明模型能力。 |

## 鉴权怎么配置

Bearer token：

```c
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("GLM_API_KEY");
```

API key header：

```c
profile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
profile.tAuth.sHeaderName = "api-key";
profile.tAuth.sSecret = getenv("AZURE_OPENAI_API_KEY");
```

无鉴权：

```c
profile.tAuth.eKind = XLLM_AUTH_NONE;
```

本地 Ollama 这类服务常常可以使用无鉴权。

## 模型能力为什么要声明

xllm 会根据能力声明做请求校验和路由判断。例如：

```c
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM;
```

这表示该文本模型支持文本输入、文本输出和流式输出。  
如果你向只声明文本能力的模型发送图片，`xllm_validate_request` 或调用过程就能更早发现问题。

## 什么时候新增 profile

遇到下面情况，通常新增 profile：

- 同一个 provider 下要切换不同模型。
- 同一个 adapter 下要连接不同 endpoint。
- 同一个 endpoint 有不同的密钥、组织、项目或租户。
- 你想为“文本模型”和“多模态模型”分别设置默认参数。

## 什么时候换 adapter

遇到下面情况，通常换 adapter：

- provider 的协议不是 OpenAI-compatible。
- 你需要使用 provider 原生能力，例如原生多模态、原生工具、特殊 header。
- 你要连接本地 Ollama，而不是远程 HTTP API。

## 范例：注册一个 GLM profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

xllm_register_glm_native_adapter(runtime);

profile.sId = "glm-native";
profile.sProvider = "zhipu";
profile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
profile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("GLM_API_KEY");
profile.tModels.tText.sModelId = "glm-5-turbo";
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM;

xllm_register_profile(runtime, &profile);
```

## 范例：注册一个 OpenAI-compatible profile

```c
xllm_profile profile;
xllm_profile_init(&profile);

xllm_register_openai_compat_adapter(runtime);

profile.sId = "openai";
profile.sProvider = "openai";
profile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
profile.sBaseUrl = "https://api.openai.com/v1";
profile.tAuth.eKind = XLLM_AUTH_BEARER;
profile.tAuth.sSecret = getenv("OPENAI_API_KEY");
profile.tModels.tText.sModelId = "gpt-4.1-mini";
profile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_JSON_OUT |
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_STREAM;

xllm_register_profile(runtime, &profile);
```

## 选择 profile 的两种方式

轻量 `xllm` 对象：

```c
xllm *llm = xllm_create(runtime, NULL);
xllm_bind_profile(llm, "glm-native");
```

底层 request：

```c
xllm_request request;
memset(&request, 0, sizeof(request));
request.sProfileId = "glm-native";
```

初学时推荐使用 `xllm_create` + `xllm_bind_profile`，因为它能让每个 turn 少写一些重复配置。

## 常见错误

### profile 的 `sAdapter` 没有注册

如果你填写了 `XLLM_ADAPTER_GLM_NATIVE`，但没有先调用 `xllm_register_glm_native_adapter(runtime)`，profile 注册或后续调用会失败。

### 密钥为空

`getenv("GLM_API_KEY")` 返回 `NULL` 时，请求会因为鉴权失败而失败。调试时先打印环境变量是否存在，不要把密钥写进文档或代码仓库。

### 能力声明过少

如果你要使用 JSON、工具、多模态或流式输出，需要在 caps 中声明相应能力。否则 xllm 会认为当前模型不支持这些请求。

## 下一步

- 继续阅读 [Request / Response 入门](request-response-intro.md)。
- 函数字段细节见 [Provider API](../api/api-providers.md) 和 [Core API](../api/api-core.md)。

[返回教程入口](README.md)
