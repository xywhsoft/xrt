# 按模型能力选择 Provider 路径

本案例展示宿主程序怎样根据模型能力选择文本、流式、工具、图片、文件和 JSON/schema 调用路径。

[返回范例解析](README.md) | [Provider/Profile 入门](../guide/provider-profile-intro.md) | [Providers API](../api/api-providers.md)

## 问题

不同 provider、不同模型支持的能力不一样。有的模型只支持文本，有的支持流式输出，有的支持工具调用，有的支持图片或文件输入，有的支持 JSON/schema 输出。

如果应用不检查能力，就可能在运行时遇到：

- 请求构造成功，但 provider 拒绝。
- 模型不支持 tool call。
- 请求里带了图片，但 profile 没有 multimodal model。
- 要求 JSON schema，但模型只支持普通文本。
- 开启 stream，但 provider 或模型不支持。

这个案例的目标是：在发送前用 profile 能力选择合适路径，并在不支持时给用户明确反馈。

## 架构

```text
用户任务
  -> 识别需要的能力
  -> 从 profile/model caps 选择 slot
  -> 构造 turn/request
  -> xllm_validate_request
  -> xllm_send_ex 或 xllm_session_chat_ex
```

能力选择不应该散落在 UI 代码里。建议封装成一个小函数：输入任务需求，输出 profile ID、slot、call options 和是否允许降级。

## 常见能力标记

| 能力 | 常量 | 典型需求 |
| --- | --- | --- |
| 文本输入 | `XLLM_CAP_TEXT_IN` | 用户输入文本 |
| 图片输入 | `XLLM_CAP_IMAGE_IN` | 看图、OCR、截图理解 |
| 文件输入 | `XLLM_CAP_FILE_IN` | 上传 PDF、文档、代码文件 |
| 工具结果输入 | `XLLM_CAP_TOOL_RESULT_IN` | 自动 tool loop |
| 文本输出 | `XLLM_CAP_TEXT_OUT` | 普通聊天 |
| JSON 输出 | `XLLM_CAP_JSON_OUT` | 结构化结果 |
| 工具调用输出 | `XLLM_CAP_TOOL_CALL_OUT` | 模型调用工具 |
| 流式输出 | `XLLM_CAP_STREAM` | 打字机效果、实时响应 |
| 推理控制 | `XLLM_CAP_REASONING_CONTROL` | 控制 reasoning level 或预算 |
| 并行工具 | `XLLM_CAP_PARALLEL_TOOL_CALL` | 一轮多个 tool call |

Profile 中通常这样声明文本模型：

```c
tProfile.tModels.tText.sModelId = "glm-5-turbo";
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM;
```

如果同一个 provider 有多模态模型，可以再声明：

```c
tProfile.tModels.tMultimodal.sModelId = "vision-model";
tProfile.tModels.tMultimodal.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_IMAGE_IN |
    XLLM_CAP_TEXT_OUT;
```

## 任务需求建模

宿主程序可以把用户任务整理成一组需要的能力：

```c
typedef struct {
    bool bNeedsTextIn;
    bool bNeedsImageIn;
    bool bNeedsFileIn;
    bool bNeedsJsonOut;
    bool bNeedsToolCalls;
    bool bPreferStream;
} app_llm_requirements;
```

然后计算 capability flags：

```c
static xllm_capability_flags app_required_caps(const app_llm_requirements *pReq)
{
    xllm_capability_flags uCaps = 0;

    if ( pReq->bNeedsTextIn ) {
        uCaps |= XLLM_CAP_TEXT_IN;
    }
    if ( pReq->bNeedsImageIn ) {
        uCaps |= XLLM_CAP_IMAGE_IN;
    }
    if ( pReq->bNeedsFileIn ) {
        uCaps |= XLLM_CAP_FILE_IN;
    }
    if ( pReq->bNeedsJsonOut ) {
        uCaps |= XLLM_CAP_JSON_OUT;
    } else {
        uCaps |= XLLM_CAP_TEXT_OUT;
    }
    if ( pReq->bNeedsToolCalls ) {
        uCaps |= XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_TOOL_RESULT_IN;
    }

    return uCaps;
}
```

流式输出通常是偏好而不是硬要求。除非你的产品必须实时返回，否则可以在不支持 stream 时降级到非流式。

## 选择 Text Slot 或 Multimodal Slot

`xllm_slot` 用来选择模型槽：

| Slot | 用法 |
| --- | --- |
| `XLLM_SLOT_AUTO` | 让 xllm 根据输入自动选择 |
| `XLLM_SLOT_TEXT` | 强制使用文本模型 |
| `XLLM_SLOT_MULTIMODAL` | 强制使用多模态模型 |

如果用户上传图片，通常应该选择 multimodal：

```c
xllm_turn tTurn;
xllm_turn_init(&tTurn);

tTurn.eSlot = XLLM_SLOT_MULTIMODAL;
xllm_turn_add_user_text(&tTurn, "请描述这张图片。");
xllm_turn_add_image_file(&tTurn, "screenshot.png", "image/png");
```

如果只是文本任务，使用 `XLLM_SLOT_TEXT` 或 `XLLM_SLOT_AUTO` 都可以。初学时可先使用 `AUTO`，当你要明确避免误选模型时再指定 slot。

## 发送前验证请求

在复杂应用里，发送前先验证请求可以更早发现问题：

```c
xllm_error tError;
xllm_request tRequest;
xllm_call_options tCallOptions;

xllm_error_init(&tError);
xllm_request_init(&tRequest);
xllm_call_options_init(&tCallOptions);

/* 构造 tRequest，包括 profile、slot、messages、tools、response format 等 */

if ( xllm_validate_request(pRuntime, &tRequest, &tCallOptions, &tError) != XRT_NET_OK ) {
    fprintf(stderr, "request unsupported: code=%d required=0x%llx model=%s\n",
            (int)tError.eCode,
            (unsigned long long)tError.uRequiredCapability,
            tError.sSelectedModel ? tError.sSelectedModel : "(null)");
}
```

如果验证失败，`uRequiredCapability` 和 `sSelectedModel` 可以帮助你告诉用户“当前模型不支持图片输入”或“当前模型不支持工具调用”。

## JSON 输出路径

如果任务需要结构化结果，优先使用 response format，而不是在 prompt 里只写“请返回 JSON”。

```c
xllm_turn_set_json_schema_response(
    &tTurn,
    "extract_result",
    tJsonSchema,
    NULL
);
```

同时 profile 应声明：

```c
tProfile.tModels.tText.tCaps.uFlags |= XLLM_CAP_JSON_OUT;
```

如果模型不支持 JSON/schema，应用可以降级为普通文本，但应明确告诉调用者结果不再有强结构保证。

## 工具调用路径

工具调用需要双向能力：

```c
tProfile.tModels.tText.tCaps.uFlags |=
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_TOOL_RESULT_IN;
```

然后在 turn 里添加工具，并设置 executor：

```c
xllm_turn_add_tool(&tTurn, &tTool);
xllm_set_tool_executor(pLlm, &tExecutor);
```

如果 provider 不支持工具调用，你可以选择：

- 禁用工具按钮。
- 改用普通文本问答。
- 在宿主侧先执行固定检索，再把结果作为 context block 注入。

不要在模型不支持 tool call 时伪造工具协议给 provider。那会让行为不可预测。

## 流式降级策略

流式输出适合 UI 体验，但不一定是硬需求：

```c
xllm_call_options_init(&tCallOptions);

if ( bModelSupportsStream ) {
    tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
    tCallOptions.pfnOnEvent = on_event;
} else {
    tCallOptions.eStreamMode = XLLM_STREAM_OFF;
}
```

如果你的产品必须实时输出，可以使用 `XLLM_STREAM_REQUIRE`。这时模型不支持 stream 应直接失败，而不是静默降级。

## 多 Provider 配置建议

一个应用通常会注册多个 profile：

| Profile | 用途 |
| --- | --- |
| `fast-text` | 低延迟普通聊天 |
| `strong-text` | 复杂推理 |
| `vision` | 图片输入 |
| `tool-agent` | 工具调用 |
| `json-extract` | JSON/schema 输出 |

选择策略可以从简单规则开始：

1. 有图片或文件，选择 multimodal profile。
2. 需要工具，选择 tool-capable profile。
3. 需要 JSON/schema，选择 structured-output profile。
4. 只要普通文本，选择 fast-text。
5. 用户要求高质量或复杂推理，再切 strong-text。

## 关键 API

| API/类型 | 作用 |
| --- | --- |
| `xllm_profile` | 定义 provider、adapter、model、能力 |
| `xllm_model_caps` | 描述模型能力和限制 |
| `xllm_capability_flags` | 表达能力集合 |
| `xllm_slot` | 选择 text 或 multimodal 模型槽 |
| `xllm_validate_request` | 发送前验证请求是否支持 |
| `xllm_turn_add_image_file` | 添加图片输入 |
| `xllm_turn_add_tool` | 添加工具定义 |
| `xllm_turn_set_json_schema_response` | 要求 JSON schema 输出 |

## 完整示例来源

可参考这些示例理解不同路径：

```text
examples/glm/glm_stateless.c
examples/glm/glm_session.c
examples/glm/glm_tool_loop.c
examples/openai/azure_openai/azure_openai_tool_loop.c
examples/gemini/gemini_session.c
examples/qwen/qwen_tool_loop.c
```

真实 provider 能力会随模型变化。更新 provider 或模型时，应重新运行对应 smoke 或 provider probe。

## 常见问题

不要只根据 provider 名称判断能力。同一个 provider 下不同模型能力可能不同。

不要把 stream 当作默认必需。许多应用可以优雅降级到非流式。

不要在未声明 `XLLM_CAP_IMAGE_IN` 的 profile 上发送图片。

不要在没有 `XLLM_CAP_TOOL_RESULT_IN` 的模型上启用自动 tool loop。

不要把 JSON/schema 能力等同于“prompt 里要求返回 JSON”。真正的结构化输出应走 response format。
