# Request / Response 入门

> 状态：中文初稿已生成，待审阅。

这篇教程解释 xllm 的一次调用由什么组成：你怎样构造输入，怎样控制输出格式，怎样读取文本、JSON 和 tool call，怎样释放 response。

## 两条使用路径

xllm 有两种构造请求的方式：

| 路径 | 适合谁 | 代表 API |
| --- | --- | --- |
| Turn helper | 初学者、普通聊天、session、agent | `xllm_turn_add_user_text`、`xllm_send_ex` |
| Raw request | 需要完全控制 message/part/context/tool | `xllm_request`、`xllm_chat_ex` |

如果你刚开始学，先用 turn helper。等你需要同时控制多条 message、多模态 part 或底层 context block 时，再使用 raw request。

## 一次输入由什么组成

底层结构是：

```text
xllm_request
  -> messages[]
    -> parts[]
```

一个 message 有角色，例如 `USER`、`ASSISTANT`、`SYSTEM`、`TOOL`。  
一个 part 是具体内容，例如文本、图片、文件、JSON。

turn helper 会帮你构造这些结构：

```c
xllm_turn turn;
xllm_turn_init(&turn);
xllm_turn_add_user_text(&turn, "解释一下 RAG 是什么。");
```

## 最小文本请求

```c
xllm_turn turn;
xllm_response *response = NULL;
xllm_error error;

xllm_turn_init(&turn);
xllm_error_init(&error);

xllm_turn_add_user_text(&turn, "用一句话解释 xllm。");

if (xllm_send_ex(llm, &turn, NULL, &response, &error) == XRT_NET_OK) {
    printf("%s\n", xllm_response_get_text(response));
}

xllm_response_free(response);
xllm_error_reset(&error);
xllm_turn_reset(&turn);
```

## 读取文本输出

最常用的读取方式是：

```c
const char *text = xllm_response_get_text(response);
```

这个函数返回 response 中已经整理好的可见文本。它适合普通聊天、摘要、问答、解释类输出。

注意：不要释放 `text`。它属于 `response`。

## 读取输出条目

当你需要更细地处理输出时，可以遍历 output：

```c
size_t count = xllm_response_get_output_count(response);
for (size_t i = 0; i < count; ++i) {
    const xllm_output_item *item = xllm_response_get_output(response, i);
    if (!item) {
        continue;
    }
    printf("output kind=%d\n", (int)item->eKind);
}
```

这适合判断模型是否返回了 message、tool call、thinking 或其他结构化输出。

## 请求 JSON 输出

如果 provider 和模型支持 JSON 输出，可以设置 response format。使用 turn helper 时，常见做法是设置 JSON schema：

```c
xllm_turn_set_json_schema_response(
    &turn,
    "answer_schema",
    schema_value,
    0);
```

调用完成后读取 JSON：

```c
size_t output_index = 0;
size_t part_index = 0;
const xvalue *json = xllm_response_get_first_json(
    response,
    &output_index,
    &part_index);
```

如果 `json == NULL`，表示没有拿到 JSON part。你应该检查模型能力是否包含 `XLLM_CAP_JSON_OUT`，以及 provider 是否支持当前 schema 模式。

## 读取 tool call

如果你给 turn 或 request 提供了工具定义，模型可能返回 tool call：

```c
size_t count = xllm_response_get_tool_call_count(response);
for (size_t i = 0; i < count; ++i) {
    const xllm_output_tool_call *call =
        xllm_response_get_tool_call(response, i);
    if (!call) {
        continue;
    }

    printf("tool=%s args=%s\n",
        call->sToolName ? call->sToolName : "",
        call->sArgumentsJson ? call->sArgumentsJson : "{}");
}
```

如果你只是想让 xllm 自动执行工具循环，后续阅读 [Tool Loop 入门](tool-loop-intro.md)。

## call options 控制什么

`xllm_call_options` 控制一次调用的运行方式，而不是 provider 身份。

常见字段包括：

| 能力 | 你会用它做什么 |
| --- | --- |
| stream mode | 请求流式输出或禁止流式。 |
| event callback | 接收 streaming delta、start/end、error。 |
| timeout | 控制调用超时。 |
| cancel token | 从外部取消调用。 |
| artifact policy | 控制大文件/产物输出处理。 |

流式输出的基本形态：

```c
static bool on_event(const xllm_event *event, void *ctx)
{
    (void)ctx;
    if (event && event->eType == XLLM_EVENT_TEXT_DELTA) {
        printf("%s", event->as.tTextDelta.sText);
    }
    return true;
}

xllm_call_options options;
xllm_call_options_init(&options);
options.eStreamMode = XLLM_STREAM_PREFER;
options.pfnOnEvent = on_event;
```

## response 的释放规则

只要 API 通过 `xllm_response **ppResponse` 返回了 response，最终都用：

```c
xllm_response_free(response);
```

不要单独释放 response 里的字符串、数组、JSON 指针或 tool call 指针。

## error 的使用方式

推荐调试时使用 `_ex` 版本：

```c
xllm_error error;
xllm_error_init(&error);

if (xllm_send_ex(llm, &turn, &options, &response, &error) != XRT_NET_OK) {
    fprintf(stderr, "code=%d http=%d message=%s\n",
        (int)error.eCode,
        (int)error.iHttpStatus,
        error.sMessage ? error.sMessage : "");
}

xllm_error_reset(&error);
```

`eCode` 是 xllm 归一化错误码，`iHttpStatus` 是 provider HTTP 状态码，`sProviderCode` 和 `sProviderMessage` 是上游错误信息。

## 什么时候使用 raw request

当你需要下面能力时，raw request 更合适：

- 一次请求里放多条历史 message。
- 自己构造 context block。
- 同时放文本、图片、文件、JSON part。
- 精确控制 tools、tool policy、generation params。
- 在不使用 `xllm` 轻量对象的情况下直接调用 `xllm_chat_ex`。

raw request 的入口是：

```c
xllm_request request;
memset(&request, 0, sizeof(request));
request.sProfileId = "glm-native";
```

但 raw request 的结构体里包含数组指针，资源管理更容易写错。初学阶段可以先把 turn helper 用熟。

## 常见错误

### 忘记释放 response

`xllm_response_get_text` 返回的字符串不用释放，但 `response` 本身必须释放。

### 没有检查 JSON 是否为空

不是所有模型都会按预期返回 JSON。读取 JSON 后一定检查返回指针。

### 请求能力和模型能力不匹配

如果你请求流式、JSON、工具或多模态，profile 的 model caps 也应该声明相应能力。

## 下一步

- 想保持多轮历史，继续读 [Session 入门](session-intro.md)。
- 想自动执行工具，继续读 [Tool Loop 入门](tool-loop-intro.md)。
- API 细节见 [Request/Response API](../api/api-request-response.md)。

[返回教程入口](README.md)
