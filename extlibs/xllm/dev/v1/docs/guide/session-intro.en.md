# Session Introduction

A session stores the short-term context for one continuous conversation. Use `xllm_session` when you want the model to remember what was said in the previous few turns.

[Back to Tutorials](README.en.md) | [Session API](../api/api-session.en.md) | [Request and Response Introduction](request-response-intro.en.md)

## What You Will Learn

After reading this guide, you should be able to decide:

- When to use `xllm_send_ex`, and when to use `xllm_session_chat_ex`.
- How to create a session with a system prompt.
- How to send multi-turn conversations, where the second turn automatically carries the first turn's history.
- What automatic compacting solves, and what it does not solve.
- Where the boundary is between session and memory.

## When to Use a Session

If your application is "one question, one answer", and each request is independent, such as batch translation, rewriting one paragraph, or generating one JSON object, then `xllm_create` + `xllm_send_ex` or the lower-level `xllm_chat_ex` is usually enough.

If your application is a continuous conversation, such as a chat window, command-line assistant, or AI IDE panel, and you want the user's second message to refer to objects mentioned in the first message, use `xllm_session`. The session organizes previous turns as context for later requests, so you do not need to manually rebuild `xllm_request` with every historical message.

A simple rule of thumb:

| Requirement | Recommended API |
| --- | --- |
| Send one request only | `xllm_send_ex` or `xllm_chat_ex` |
| Multi-turn chat with short-term history | `xllm_session_chat_ex` |
| Multi-turn chat with automatic compression near the context limit | `xllm_session` + `bEnableAutoCompact` |
| Remember knowledge across processes, days, or sessions | `xllm_memory`, not session |

## Minimal Flow

Using a session is similar to using a normal `xllm` object. The main difference is that you create the object with `xllm_session_create`:

1. Initialize xrt with `xrtInit()`.
2. Create a runtime with `xllm_runtime_create`.
3. Register a provider adapter, such as `xllm_register_glm_native_adapter`.
4. Register a profile with `xllm_register_profile`.
5. Initialize `xllm_session_options`, then set `sProfileId` and `sSystemPrompt`.
6. Call `xllm_session_create`.
7. For each turn, create a new `xllm_turn`, add the user's content, then call `xllm_session_chat_ex`.
8. Free each response, reset each turn, and finally destroy the session and runtime.

## Creating a Session

The following snippet shows the session creation part. For a complete example, see `examples/glm/glm_session.c`.

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

There are two key points:

- `sProfileId` decides which provider, model, and authentication settings the session uses by default.
- `sSystemPrompt` becomes the default system prompt for the session, so you do not need to pass it again on every turn.

## Sending Multi-Turn Conversations

Before each request, initialize a new `xllm_turn` and only put the new user input into it. The session is responsible for adding the existing history to the model request.

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

The second turn still contains only the new question:

```c
xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "What is my project name?");

iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOptions, &pResponse, &tError);
```

If the first turn was successfully written into history, the model has a chance to answer with the project name from the previous turn.

## Streaming Output

Sessions support the same event callback mechanism as normal requests. After setting `xllm_call_options.pfnOnEvent`, you can print text in real time from `XLLM_EVENT_TEXT_DELTA`:

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

Returning `false` from the callback aborts the current request. In a chat UI, you usually append the delta text to the current message bubble here. In a command-line program, you can print it directly.

## Automatic Compacting

Multi-turn conversations keep growing and may eventually exceed the model context window. The session compact mechanism compresses or truncates older history so the conversation can continue.

Common fields:

| Field | Purpose |
| --- | --- |
| `bEnableAutoCompact` | Whether the session may compact automatically when needed |
| `fCompactTriggerRatio` | Trigger ratio when approaching the context limit |
| `uCompactTriggerTurns` | Auxiliary trigger based on number of turns |
| `uReserveOutputTokens` | Tokens reserved for model output |
| `uKeepRecentTurns` | Number of recent raw turns to keep when compacting |
| `eCompactStrategy` | Use truncation, summary, or a custom strategy |
| `sSummarizerProfileId` | Profile used for summary-based compacting |

You can also compact manually:

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

When learning, start with `XLLM_COMPACT_TRUNCATE`. After you understand the trigger points, try `XLLM_COMPACT_SUMMARIZE`. Summary compacting requires an available summarizer profile.

## Session Is Not Long-Term Memory

A session is suitable for short-term context in the current conversation. It should not be used for:

- Saving long-term user preferences.
- Saving a project knowledge base.
- Remembering tasks across days.
- Retrieving relevant code from workspace files.

Use `xllm_memory` for these requirements. A common pattern is: the session stores the current chat history, memory stores long-term knowledge, and before each turn you search memory and inject the retrieved result as context blocks.

## Cleaning Up Resources

Each object has an explicit cleanup function:

```c
if ( pResponse ) {
    xllm_response_free(pResponse);
}
xllm_turn_reset(&tTurn);
xllm_session_destroy(pSession);
xllm_runtime_destroy(pRuntime);
```

If you use `xllm_error` to receive error details, call `xllm_error_free` at the end. If you reuse the same error object in a loop, you can call `xllm_error_reset` after each failed operation, then call `xllm_error_free` once at the end.

## Common Mistakes

Do not keep appending user messages to the same `xllm_turn` and resend it repeatedly. Each new input turn should use a new turn object, and you should call `xllm_turn_reset` after sending.

Do not forget capability flags in the profile. The session validates requests based on model capabilities. Missing flags such as `XLLM_CAP_TEXT_IN`, `XLLM_CAP_TEXT_OUT`, or `XLLM_CAP_STREAM` may cause validation failure.

Do not write API keys into documentation or source code. The examples use `getenv("GLM_API_KEY")`; real applications should also read secrets from secure configuration sources.

Do not treat compacting as permanent memory. Compacting keeps the current context within the model window; it does not build a searchable knowledge base.

## Next Steps

- To understand the structure of turns, messages, and responses, continue with [Request and Response Introduction](request-response-intro.en.md).
- To let the model call host-side tools, continue with [Tool Loop Introduction](tool-loop-intro.en.md).
- To add historical knowledge or workspace files to the context, continue with [Memory RAG Introduction](memory-rag-intro.en.md).
