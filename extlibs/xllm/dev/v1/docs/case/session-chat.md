# 带短期历史的 Session Chat

本案例展示怎样用 `xllm_session` 保存短期历史，让模型在第二轮对话中引用第一轮内容。

[返回范例解析](README.md) | [Session 入门](../guide/session-intro.md) | [Session API](../api/api-session.md)

## 问题

最小聊天调用每次请求都是独立的。用户第一轮说“我的项目叫 xllm”，第二轮问“我的项目叫什么”，如果你没有把第一轮历史带上，模型通常无法可靠回答。

Session chat 解决这个问题：它自动保存当前对话历史，并在后续请求中带给模型。

## 架构

```text
应用程序
  -> xllm_runtime
    -> adapter + profile
  -> xllm_session
    -> turn 1: 用户告诉项目信息
    -> response 1: 模型确认
    -> turn 2: 用户追问
    -> response 2: 模型基于历史回答
```

Session 保存的是短期上下文。它适合当前聊天窗口，不适合长期知识库。长期知识应写入 `xllm_memory`。

## 步骤 1：准备 Runtime 和 Profile

Session 仍然需要 runtime、adapter 和 profile。以 GLM 为例：

```c
xllm_runtime *pRuntime = NULL;
xllm_profile tProfile;

xllm_runtime_create(NULL, &pRuntime);
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

xllm_register_profile(pRuntime, &tProfile);
```

## 步骤 2：创建 Session

```c
xllm_session *pSession = NULL;
xllm_session_options tSessionOptions;

xllm_session_options_init(&tSessionOptions);
tSessionOptions.sProfileId = "glm-native";
tSessionOptions.sSystemPrompt = "You are a concise assistant.";
tSessionOptions.bEnableAutoCompact = true;
tSessionOptions.uCompactTriggerTurns = 12u;

if ( xllm_session_create(pRuntime, &tSessionOptions, &pSession) != XRT_NET_OK ) {
    fprintf(stderr, "failed to create session\n");
    return 1;
}
```

关键字段：

| 字段 | 作用 |
| --- | --- |
| `sProfileId` | session 默认使用的 profile |
| `sSystemPrompt` | session 默认系统提示词 |
| `bEnableAutoCompact` | 是否允许历史接近上下文上限时自动 compact |
| `uCompactTriggerTurns` | 触发 compact 的轮次数提示 |

## 步骤 3：发送第一轮

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
    xllm_response_free(pResponse);
    pResponse = NULL;
}

xllm_turn_reset(&tTurn);
```

发送成功后，session 会把这轮用户输入和模型回答纳入历史。

## 步骤 4：发送第二轮

第二轮只需要放入新问题，不要手工拼接第一轮历史：

```c
xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "What is my project name?");

iStatus = xllm_session_chat_ex(
    pSession,
    &tTurn,
    &tCallOptions,
    &pResponse,
    &tError
);

if ( iStatus == XRT_NET_OK && pResponse ) {
    printf("visible text: %s\n", xllm_response_get_text(pResponse));
    xllm_response_free(pResponse);
}

xllm_turn_reset(&tTurn);
```

如果第一轮写入历史成功，第二轮模型就能看到“project name is xllm”。

## Compact：让长对话继续进行

随着轮次增加，历史会接近模型上下文上限。Session compact 用来裁剪或摘要旧历史：

```c
xllm_compact_options tCompactOptions;
xllm_compact_result tCompactResult;

xllm_compact_options_init(&tCompactOptions);
tCompactOptions.eMode = XLLM_COMPACT_TO_FIT_CURRENT_MODEL;
tCompactOptions.eStrategy = XLLM_COMPACT_TRUNCATE;

if ( xllm_session_compact(pSession, &tCompactOptions, &tCompactResult) == XRT_NET_OK ) {
    if ( tCompactResult.bCompacted ) {
        printf("compacted: %u -> %u\n",
               tCompactResult.uInputTokensBefore,
               tCompactResult.uInputTokensAfter);
    }
}
```

学习阶段可以先使用截断策略。生产聊天系统更常见的做法是保留最近几轮原文，把较旧内容摘要成 session summary。

## 导出和恢复 Session State

当你要把 session 暂存到磁盘、数据库或任务队列时，可以导出 state：

```c
xllm_session_state *pState = NULL;
xvalue tValue = NULL;

if ( xllm_session_export_state(pSession, &pState) == XRT_NET_OK ) {
    xllm_session_state_to_xvalue(pState, &tValue);
    /* 把 tValue 序列化到你的存储系统 */
}
```

恢复时：

```c
xllm_session_state *pState = NULL;
xllm_session *pRestored = NULL;

/* 从存储系统读回 xvalue，再构造 state */
xllm_session_state_from_xvalue(tValue, &pState);
xllm_session_import_state(pRuntime, pState, &pRestored);
```

State 保存的是会话状态，不是长期记忆治理系统。用户偏好、任务、事实仍建议写入 memory。

## 完整示例

完整可运行示例见：

```text
examples/glm/glm_session.c
```

它展示了：

- 注册 GLM native adapter。
- 创建带系统提示词的 session。
- 发送两轮对话。
- 使用 `XLLM_STREAM_PREFER` 打印流式文本。
- 处理 `xllm_error`。
- 清理 response、turn、session、runtime。

## 关键 API

| API | 作用 |
| --- | --- |
| `xllm_session_options_init` | 初始化 session 选项 |
| `xllm_session_create` | 创建 session |
| `xllm_session_chat_ex` | 发送一轮对话并保留历史 |
| `xllm_session_compact` | 手工压缩历史 |
| `xllm_session_clear_history` | 清空历史 |
| `xllm_session_export_state` | 导出 session state |
| `xllm_session_import_state` | 恢复 session |
| `xllm_session_destroy` | 销毁 session |

## 扩展点

你可以继续添加：

- Tool executor：让 session 中的模型调用宿主工具。
- Memory RAG：每轮发送前检索长期知识并注入 turn。
- Summary compact：把旧历史摘要成更短上下文。
- State persistence：把 session state 保存到数据库。

## 常见问题

不要在每轮 turn 中手工重复所有历史。Session 已经负责历史管理，turn 只放本轮新输入。

不要把 session 当作长期记忆。Session compact 后旧内容可能被裁剪或摘要，不适合保存用户长期偏好。

不要忘记释放每轮 response。Session 保存历史不代表 response 可以不释放。

如果第二轮没有记住第一轮，先确认第一轮请求成功、response 没有错误、session 没有被销毁或清空。
