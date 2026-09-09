# Session Chat with Short-Term History

This case shows how to use `xllm_session` to keep short-term history, so the model can refer to first-turn content in the second turn.

[Back to Case Studies](README.en.md) | [Session Introduction](../guide/session-intro.en.md) | [Session API](../api/api-session.en.md)

## Problem

Minimal chat calls are independent each time. If the user says "my project is named xllm" in the first turn, then asks "what is my project called" in the second turn, the model usually cannot answer reliably unless you include the first turn in the context.

Session chat solves this problem. It automatically stores the current conversation history and sends it to the model in later requests.

## Architecture

```text
application
  -> xllm_runtime
    -> adapter + profile
  -> xllm_session
    -> turn 1: user provides project information
    -> response 1: model confirms
    -> turn 2: user asks a follow-up
    -> response 2: model answers from history
```

A session stores short-term context. It is suitable for the current chat window, not for a long-term knowledge base. Long-term knowledge should be written into `xllm_memory`.

## Step 1: Prepare Runtime and Profile

A session still needs a runtime, adapter, and profile. Using GLM as an example:

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

## Step 2: Create the Session

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

Key fields:

| Field | Purpose |
| --- | --- |
| `sProfileId` | Default profile used by the session |
| `sSystemPrompt` | Default system prompt for the session |
| `bEnableAutoCompact` | Whether history may compact automatically near the context limit |
| `uCompactTriggerTurns` | Turn-count hint for triggering compact |

## Step 3: Send the First Turn

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

After the request succeeds, the session includes this user input and model response in history.

## Step 4: Send the Second Turn

The second turn only needs the new question. Do not manually concatenate first-turn history:

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

If the first turn was successfully written into history, the model can see "project name is xllm" in the second turn.

## Compact: Let Long Conversations Continue

As turn count grows, history approaches the model context limit. Session compacting trims or summarizes older history:

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

When learning, start with truncation. Production chat systems more commonly keep the most recent raw turns and summarize older content into a session summary.

## Exporting and Restoring Session State

When you need to temporarily store a session in a file, database, or task queue, export its state:

```c
xllm_session_state *pState = NULL;
xvalue tValue = NULL;

if ( xllm_session_export_state(pSession, &pState) == XRT_NET_OK ) {
    xllm_session_state_to_xvalue(pState, &tValue);
    /* Serialize tValue into your storage system. */
}
```

Restore it later:

```c
xllm_session_state *pState = NULL;
xllm_session *pRestored = NULL;

/* Read xvalue from your storage system, then build state. */
xllm_session_state_from_xvalue(tValue, &pState);
xllm_session_import_state(pRuntime, pState, &pRestored);
```

State stores session state, not a long-term memory governance system. User preferences, tasks, and facts should still be written into memory.

## Complete Example

The complete runnable example is:

```text
examples/glm/glm_session.c
```

It demonstrates:

- Registering the GLM native adapter.
- Creating a session with a system prompt.
- Sending two turns.
- Printing streaming text with `XLLM_STREAM_PREFER`.
- Handling `xllm_error`.
- Cleaning up response, turn, session, and runtime.

## Key APIs

| API | Purpose |
| --- | --- |
| `xllm_session_options_init` | Initialize session options |
| `xllm_session_create` | Create a session |
| `xllm_session_chat_ex` | Send one conversation turn and preserve history |
| `xllm_session_compact` | Manually compact history |
| `xllm_session_clear_history` | Clear history |
| `xllm_session_export_state` | Export session state |
| `xllm_session_import_state` | Restore a session |
| `xllm_session_destroy` | Destroy a session |

## Extension Points

You can continue by adding:

- Tool executor: let the model call host-side tools inside the session.
- Memory RAG: retrieve long-term knowledge before each turn and inject it.
- Summary compact: summarize old history into shorter context.
- State persistence: save session state into a database.

## Common Questions

Do not manually repeat all history in every turn. The session manages history; the turn should contain only the new input for this round.

Do not treat session as long-term memory. After compacting, old content may be trimmed or summarized, so it is not suitable for long-term user preferences.

Do not forget to free every response. The session keeping history does not mean response objects can be left allocated.

If the second turn does not remember the first, first confirm that the first request succeeded, the response had no error, and the session was not destroyed or cleared.
