# Session 入门

Session 用来保存一段连续对话的短期上下文。你希望模型记住前几轮刚说过的内容时，就使用 `xllm_session`。

[返回教程入口](README.md) | [Session API](../api/api-session.md) | [请求与响应入门](request-response-intro.md)

## 你会学到什么

读完本文，你应该能判断：

- 什么时候使用 `xllm_send_ex`，什么时候使用 `xllm_session_chat_ex`。
- 怎样创建一个带系统提示词的 session。
- 怎样发送多轮对话，并让第二轮自动带上第一轮历史。
- 自动 compact 解决什么问题，不能解决什么问题。
- session 和 memory 的边界在哪里。

## 什么时候使用 Session

如果你的应用是“一问一答”，每次请求都独立，例如批量翻译、改写一段文本、生成一次 JSON，通常直接使用 `xllm_create` + `xllm_send_ex` 或底层 `xllm_chat_ex` 就够了。

如果你的应用是“持续对话”，例如聊天窗口、命令行助手、AI IDE 面板，你希望用户第二句话能引用第一句话里的对象，就应该使用 `xllm_session`。Session 会把历史轮次组织成后续请求的上下文，你不需要手工把每轮消息重新拼进 `xllm_request`。

一个简单判断：

| 需求 | 推荐接口 |
| --- | --- |
| 只发一次请求 | `xllm_send_ex` 或 `xllm_chat_ex` |
| 多轮聊天，保留短期历史 | `xllm_session_chat_ex` |
| 多轮聊天，并希望接近上下文上限时自动压缩 | `xllm_session` + `bEnableAutoCompact` |
| 跨进程、跨天、跨会话记住知识 | `xllm_memory`，不是 session |

## 最小流程

使用 session 的流程和普通 `xllm` 对象很像，只是创建对象时换成 `xllm_session_create`：

1. 初始化 xrt：`xrtInit()`。
2. 创建运行时：`xllm_runtime_create`。
3. 注册 provider adapter：例如 `xllm_register_glm_native_adapter`。
4. 注册 profile：`xllm_register_profile`。
5. 初始化 `xllm_session_options`，设置 `sProfileId` 和 `sSystemPrompt`。
6. 调用 `xllm_session_create`。
7. 每一轮都创建一个新的 `xllm_turn`，添加用户内容，然后调用 `xllm_session_chat_ex`。
8. 释放每轮响应，重置 turn，最后销毁 session 和 runtime。

## 创建 Session

下面的代码片段展示 session 创建部分。完整示例可以参考 `examples/glm/glm_session.c`。

```c
xllm_runtime *pRuntime = NULL;
xllm_session *pSession = NULL;
xllm_profile tProfile;
xllm_session_options tSessionOptions;

xrtInit();

if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK ) {
    return 1;
}

xllm_register_glm_native_adapter(pRuntime);

xllm_profile_init(&tProfile);
tProfile.sId = "glm-native";
tProfile.sProvider = "zhipu";
tProfile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
tProfile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
tProfile.tAuth.sSecret = getenv("GLM_API_KEY");
tProfile.tModels.tText.sModelId = "glm-5-turbo";
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
    return 1;
}

xllm_session_options_init(&tSessionOptions);
tSessionOptions.sProfileId = "glm-native";
tSessionOptions.sSystemPrompt = "You are a concise assistant.";
tSessionOptions.bEnableAutoCompact = true;
tSessionOptions.uCompactTriggerTurns = 12u;

if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK ) {
    return 1;
}
```

这里有两个关键点：

- `sProfileId` 决定 session 默认使用哪个 provider、model 和认证信息。
- `sSystemPrompt` 会作为 session 的默认系统提示词，后续每轮不需要重复传入。

## 发送多轮对话

每次发送前都初始化一个新的 `xllm_turn`，只放入这一轮用户新输入即可。Session 会负责把已有历史加入模型请求。

```c
xllm_turn tTurn;
xllm_call_options tCallOptions;
xllm_error tError;
xllm_response *pResponse = NULL;
int iStatus;

xllm_call_options_init(&tCallOptions);
tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
tCallOptions.uTimeoutMs = 120000u;

xllm_error_init(&tError);

xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "My project name is xllm. Please remember it.");

iStatus = xllm_session_chat_ex(
    pSession,
    &tTurn,
    &tCallOptions,
    &pResponse,
    &tError
);

if ( iStatus == XRT_NET_OK && pResponse ) {
    printf("%s\n", xllm_response_get_text(pResponse));
    xllm_response_free(pResponse);
}

xllm_turn_reset(&tTurn);
xllm_error_free(&tError);
```

第二轮仍然只写新问题：

```c
xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "What is my project name?");

iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
```

如果第一轮成功写入历史，模型在第二轮就有机会回答出刚才的项目名。

## 流式输出

Session 支持和普通请求一样的事件回调。你设置 `xllm_call_options.pfnOnEvent` 后，可以在 `XLLM_EVENT_TEXT_DELTA` 中实时打印文本：

```c
static bool on_event(const xllm_event *pEvent, void *pUserData)
{
    (void)pUserData;
    if ( pEvent && pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("%s", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
        fflush(stdout);
    }
    return true;
}

xllm_call_options_init(&tCallOptions);
tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
tCallOptions.pfnOnEvent = on_event;
```

回调返回 `false` 表示中止本次请求。写聊天 UI 时，你通常在这里把增量文本追加到当前消息气泡；命令行程序可以直接打印。

## 自动 Compact

多轮对话会越来越长，最终可能超过模型上下文窗口。Session 提供 compact 机制，把旧历史压缩或裁剪后继续对话。

常用字段：

| 字段 | 作用 |
| --- | --- |
| `bEnableAutoCompact` | 是否允许 session 在需要时自动 compact |
| `fCompactTriggerRatio` | 接近上下文上限的触发比例 |
| `uCompactTriggerTurns` | 经过多少轮后触发 compact 的辅助条件 |
| `uReserveOutputTokens` | 为模型输出预留的 token |
| `uKeepRecentTurns` | compact 时尽量保留最近多少轮原始对话 |
| `eCompactStrategy` | 使用截断、摘要或自定义策略 |
| `sSummarizerProfileId` | 摘要 compact 时使用的 profile |

你也可以手工调用：

```c
xllm_compact_options tOptions;
xllm_compact_result tResult;

xllm_compact_options_init(&tOptions);
tOptions.eMode = XLLM_COMPACT_TO_FIT_CURRENT_MODEL;
tOptions.eStrategy = XLLM_COMPACT_TRUNCATE;

if ( xllm_session_compact(pSession, &tOptions, &tResult) == XRT_NET_OK ) {
    if ( tResult.bCompacted ) {
        printf("input tokens: %u -> %u\n",
               tResult.uInputTokensBefore,
               tResult.uInputTokensAfter);
    }
}
```

学习时可以先使用 `XLLM_COMPACT_TRUNCATE`，理解 compact 触发点后，再尝试 `XLLM_COMPACT_SUMMARIZE`。摘要 compact 需要一个可用的 summarizer profile。

## Session 不是长期记忆

Session 只适合保存当前对话的短期上下文。它不应该承担这些工作：

- 保存用户长期偏好。
- 保存项目知识库。
- 记住跨天任务。
- 从工作区文件中检索相关代码。

这些需求应该使用 `xllm_memory`。一个常见组合是：session 保存当前聊天历史，memory 保存长期知识；每一轮发送前先从 memory 检索，再把检索结果作为 context block 注入当前 turn。

## 清理资源

每个对象都有明确释放方式：

```c
if ( pResponse ) {
    xllm_response_free(pResponse);
}
xllm_turn_reset(&tTurn);
xllm_session_destroy(pSession);
xllm_runtime_destroy(pRuntime);
```

如果使用 `xllm_error` 接收错误详情，最后调用 `xllm_error_free`。如果在循环里重复使用同一个错误对象，可以在每次失败处理后调用 `xllm_error_reset`，最后再 `xllm_error_free`。

## 常见错误

不要把同一个 `xllm_turn` 不断追加用户消息后反复发送。每轮新输入应使用新的 turn，发送后调用 `xllm_turn_reset`。

不要忘记给 profile 写能力标记。Session 会根据模型能力验证请求，缺少 `XLLM_CAP_TEXT_IN`、`XLLM_CAP_TEXT_OUT` 或 `XLLM_CAP_STREAM` 等标记时，可能在验证阶段失败。

不要把 API key 写进文档或源码。示例使用 `getenv("GLM_API_KEY")`，实际应用也应该从安全配置来源读取。

不要把 compact 当作永久记忆。Compact 是为了把当前上下文压到模型窗口内，不是为了建立可检索知识库。

## 下一步

- 想了解 turn、message、response 的结构，继续读 [请求与响应入门](request-response-intro.md)。
- 想让模型调用宿主工具，继续读 [Tool Loop 入门](tool-loop-intro.md)。
- 想把历史知识或工作区文件加入上下文，继续读 [Memory RAG 入门](memory-rag-intro.md)。
