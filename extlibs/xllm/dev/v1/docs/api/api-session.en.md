# xllm Session API

> Session API manages short-term conversation history, system prompts, automatic compacting, tool loops, and session state import/export.

[Back to API Index](README.en.md) | [Tools API](api-tools.en.md) | [Request / Response API](api-request-response.en.md)

---

## Table of Contents

- [Module Role](#module-role)
- [Two Conversation Entry Points](#two-conversation-entry-points)
- [Turn Construction APIs](#turn-construction-apis)
- [Lightweight xllm Object](#lightweight-xllm-object)
- [Session Lifecycle](#session-lifecycle)
- [Session Chat](#session-chat)
- [Compact](#compact)
- [Session State](#session-state)
- [Common Usage](#common-usage)
- [Common Mistakes](#common-mistakes)
- [Related Examples](#related-examples)

---

## Module Role

Session solves the problem of keeping short-term multi-turn context within the same task. It fits:

- AI IDE tasks that involve several conversation turns.
- Organizing user input, assistant output, tool calls, and tool results into history.
- Compacting or summarizing when history becomes too long.
- Exporting session state and restoring a task later.

Session is not long-term memory. Long-term knowledge, project indexes, user facts, and preferences should use Memory API.

---

## Two Conversation Entry Points

| Entry | Type | Best For |
| --- | --- | --- |
| Lightweight object | `xllm *` | Reusing profile, system prompt, default call options, and using `xllm_send_ex` to build a request from a turn. It does not maintain long-term history. |
| Session object | `xllm_session *` | Short-term history, compacting, state export/import, and automatic turn/response commit. |

New integrations usually prefer `xllm_session`. `xllm *` is more of a convenience wrapper.

---

## Turn Construction APIs

### xllm_turn_init

Initializes `xllm_turn`.

**Prototype:**

```c
XLLM_API void xllm_turn_init(xllm_turn *pTurn);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pTurn` | output | yes | Turn to initialize. |

**Return Value:**

None.

**Notes:**

Defaults include `XLLM_SLOT_AUTO`, `XLLM_SYSTEM_INHERIT`, `XLLM_TOOL_CHOICE_AUTO`, `XLLM_RESPONSE_TEXT`, and `XLLM_REASONING_DEFAULT`.

**Example Code:**

```c
xllm_turn turn;
xllm_turn_init(&turn);
```

---

### xllm_turn_reset

Releases internal turn resources and restores defaults.

**Prototype:**

```c
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
```

**Resource Ownership:**

Releases internal resources added by turn helpers, including text, images, files, tools, stop sequences, and schemas.

---

### xllm_turn_clone

Clones a turn.

**Prototype:**

```c
XLLM_API int xllm_turn_clone(xllm_turn *pOut, const xllm_turn *pIn);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pOut` | output | no | Clone target. |
| `pIn` | input | no | Turn to clone. |

**Return Value:**

- `XRT_NET_OK`: cloned successfully.
- `XRT_NET_ERROR`: invalid parameter or allocation failure.

**Resource Ownership:**

After success, `pOut` owns independent resources and must eventually be cleaned with `xllm_turn_reset`.

---

### xllm_turn_set_system_prompt

Sets the system prompt for this turn.

**Prototype:**

```c
XLLM_API int xllm_turn_set_system_prompt(xllm_turn *pTurn, const char *sText);
```

**Description:**

The function copies `sText`. Passing `NULL` clears this turn's system prompt.

---

### xllm_turn_set_system_mode

Sets how this turn's system prompt applies to the session.

**Prototype:**

```c
XLLM_API int xllm_turn_set_system_mode(xllm_turn *pTurn, xllm_system_mode eMode);
```

| Value | Description |
| --- | --- |
| `XLLM_SYSTEM_INHERIT` | Use the session default system prompt. |
| `XLLM_SYSTEM_REPLACE` | Replace the system prompt for this turn. |
| `XLLM_SYSTEM_APPEND` | Append this turn's system prompt. |

---

### xllm_turn_set_stop_sequences

Sets stop sequences.

**Prototype:**

```c
XLLM_API int xllm_turn_set_stop_sequences(xllm_turn *pTurn, const char **psStop, size_t iStopCount);
```

The function copies stop sequence strings. Use `iStopCount=0` to clear them.

---

### xllm_turn_set_json_schema_response

Requests JSON schema output for this turn.

**Prototype:**

```c
XLLM_API int xllm_turn_set_json_schema_response(
    xllm_turn *pTurn,
    const char *sSchemaName,
    xvalue tJsonSchema,
    xvalue tVendorExtra
);
```

**Notes:**

- `sSchemaName` is copied.
- `tJsonSchema` and `tVendorExtra` are retained according to xvalue rules.
- If the provider does not support strict schema, the call may return `XLLM_ERROR_UNSUPPORTED_CAPABILITY`.

---

### Input Add Functions

These functions add user input parts to a turn.

```c
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);
XLLM_API int xllm_turn_add_image_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType);
XLLM_API int xllm_turn_add_image_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType);
XLLM_API int xllm_turn_add_image_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType);
XLLM_API int xllm_turn_add_file_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType);
XLLM_API int xllm_turn_add_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType);
XLLM_API int xllm_turn_add_file_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType);
```

**Return Value:**

- `XRT_NET_OK`: added successfully.
- `XRT_NET_ERROR`: invalid parameter or allocation failure.

**Resource Ownership:**

The functions copy input strings, which are released by `xllm_turn_reset`.

**Example Code:**

```c
xllm_turn_add_user_text(&turn, "Explain this error.");
xllm_turn_add_image_url(&turn, "https://example.com/a.png", "image/png");
```

Tool-related `xllm_turn_add_tool` and `xllm_turn_set_tool_choice` are documented in [api-tools.en.md](api-tools.en.md).

---

## Lightweight xllm Object

### xllm_create / xllm_destroy

Creates and destroys a lightweight `xllm` object.

```c
XLLM_API xllm *xllm_create(xllm_runtime *pRuntime, const xllm_create_options *pOptions);
XLLM_API void xllm_destroy(xllm *pLlm);
```

**Description:**

- `xllm_create` requires an existing runtime.
- If `pOptions->sInitialProfileId` does not exist, creation fails and returns `NULL`.
- `xllm_destroy` accepts `NULL`.

### xllm_bind_profile

Binds the default profile.

```c
XLLM_API int xllm_bind_profile(xllm *pLlm, const char *sProfileId);
```

After success, `xllm_send_ex` uses this profile to build requests.

### xllm_set_system_prompt / xllm_get_system_prompt

Sets and reads the lightweight object's system prompt.

```c
XLLM_API int xllm_set_system_prompt(xllm *pLlm, const char *sText);
XLLM_API const char *xllm_get_system_prompt(const xllm *pLlm);
```

The returned system prompt pointer belongs to `pLlm`; do not free it.

### xllm_send / xllm_send_ex

Builds one request from a turn and calls the model.

```c
XLLM_API int xllm_send(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_send_ex(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);
```

`xllm_send_ex` uses the default profile and system prompt, but does not maintain session history.

### xllm_send_async_*

```c
XLLM_API xfuture *xllm_send_async_thread(xllm *pLlm, const xllm_turn *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_send_async_engine(xllm *pLlm, xnetengine *pEngine, uint32 uAffinityKey, const xllm_turn *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_send_async_co(xllm *pLlm, xcosched *pSched, const xllm_turn *pTurn, const xllm_call_options *pOptions, size_t iStackSize);
```

Reading and freeing async futures follows xrt `xfuture` rules.

---

## Session Lifecycle

### xllm_session_options_init

Initializes session options.

```c
XLLM_API void xllm_session_options_init(xllm_session_options *pOptions);
```

Defaults:

- `bEnableAutoCompact = true`
- `fCompactTriggerRatio = 0.85`
- `uCompactTriggerTurns = 16`
- `uKeepRecentTurns = 8`
- `bKeepActiveToolChain = true`
- `eCompactStrategy = XLLM_COMPACT_SUMMARIZE`

### xllm_session_create / xllm_session_destroy

Creates and destroys a session.

```c
XLLM_API int xllm_session_create(
    xllm_runtime *pRuntime,
    const xllm_session_options *pOptions,
    xllm_session **ppSession
);

XLLM_API void xllm_session_destroy(xllm_session *pSession);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRuntime` | input | no | Runtime. |
| `pOptions` | input | yes | Session configuration. |
| `ppSession` | output | no | Receives session on success. |

**Resource Ownership:**

On success, the session is owned by the caller and released with `xllm_session_destroy`.

### xllm_session_set_system_prompt

Sets the session default system prompt.

```c
XLLM_API int xllm_session_set_system_prompt(xllm_session *pSession, const char *sText);
```

`sText` is copied. Passing `NULL` clears it.

### xllm_session_clear_history

Clears session history and summary.

```c
XLLM_API int xllm_session_clear_history(xllm_session *pSession);
```

The session object, profile, system prompt, and options are kept. Only committed history is cleared.

Tool executor setup is documented in [api-tools.en.md](api-tools.en.md).

---

## Session Chat

### xllm_session_chat / xllm_session_chat_ex

Calls the model with session history plus the current turn.

```c
XLLM_API int xllm_session_chat(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_session_chat_ex(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);
```

**Purpose:**

The function creates a request snapshot from current session history, calls the model, and on success commits this turn, possible tool-chain supplement messages, and the final response into session history.

**Return Value:**

- `XRT_NET_OK`: call succeeded.
- Non-`XRT_NET_OK`: invalid parameter, snapshot construction failure, provider call failure, or similar error.

**Resource Ownership:**

On success, `*ppResponse` is freed by the caller with `xllm_response_free`. The session keeps its own history copy.

### xllm_session_chat_async_*

```c
XLLM_API xfuture *xllm_session_chat_async_thread(xllm_session *pSession, const xllm_turn_request *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_session_chat_async_engine(xllm_session *pSession, xnetengine *pEngine, uint32 uAffinityKey, const xllm_turn_request *pTurn, const xllm_call_options *pOptions);
XLLM_API xfuture *xllm_session_chat_async_co(xllm_session *pSession, xcosched *pSched, const xllm_turn_request *pTurn, const xllm_call_options *pOptions, size_t iStackSize);
```

The session, runtime, engine/scheduler must remain valid until the future completes.

---

## Compact

### xllm_compact_options_init

Initializes compact options.

```c
XLLM_API void xllm_compact_options_init(xllm_compact_options *pOptions);
```

Defaults:

- `eMode = XLLM_COMPACT_TO_FIT_CURRENT_MODEL`
- `eStrategy = XLLM_COMPACT_SUMMARIZE`

### xllm_session_compact

Manually compacts session history.

```c
XLLM_API int xllm_session_compact(
    xllm_session *pSession,
    const xllm_compact_options *pOptions,
    xllm_compact_result *pResult
);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pSession` | input/output | no | Session. |
| `pOptions` | input | yes | Compact options. Passing `NULL` uses session default strategy. |
| `pResult` | output | no | Compact result. |

**Return Value:**

- `XRT_NET_OK`: compact flow completed; check `pResult->bCompacted` to see whether history actually changed.
- `XRT_NET_ERROR`: invalid parameter or internal failure.

**Notes:**

- The summarize strategy first tries model summarization, and may use local rolling summary if that fails.
- If token count does not decrease after summarization, it may fall back to truncate or skip applying the candidate.
- Automatic compacting is triggered after successful session-chat commit.

---

## Session State

### xllm_session_export_state

Exports session state.

```c
XLLM_API int xllm_session_export_state(
    xllm_session *pSession,
    xllm_session_state **ppState
);
```

On success, `*ppState` is freed by the caller with `xllm_session_state_free`.

### xllm_session_import_state

Restores a session from state.

```c
XLLM_API int xllm_session_import_state(
    xllm_runtime *pRuntime,
    const xllm_session_state *pState,
    xllm_session **ppSession
);
```

The runtime must already have the profile required by the state registered.

### xllm_session_state_to_xvalue

Converts state to `xvalue`.

```c
XLLM_API int xllm_session_state_to_xvalue(
    const xllm_session_state *pState,
    xvalue *ptValue
);
```

The current export format writes `type = "xllm_session_state"` and `version = 2`, and includes profile, system prompt, summary, compact options, and history.

### xllm_session_state_from_xvalue

Parses state from `xvalue`.

```c
XLLM_API int xllm_session_state_from_xvalue(
    xvalue tValue,
    xllm_session_state **ppState
);
```

On success, free `*ppState` with `xllm_session_state_free`.

### xllm_session_state_free

Frees state.

```c
XLLM_API void xllm_session_state_free(xllm_session_state *pState);
```

Accepts `NULL`.

---

## Common Usage

### Minimal Session Chat

```c
xllm_session_options opt;
xllm_session *session = NULL;
xllm_turn turn;
xllm_response *response = NULL;
xllm_error err;

xllm_session_options_init(&opt);
opt.sProfileId = "demo";
opt.sSystemPrompt = "You are a concise assistant.";

xllm_session_create(runtime, &opt, &session);
xllm_turn_init(&turn);
xllm_error_init(&err);

xllm_turn_add_user_text(&turn, "Explain what xllm session is for.");
if (xllm_session_chat_ex(session, &turn, NULL, &response, &err) == XRT_NET_OK) {
    printf("%s\n", xllm_response_get_text(response));
}

xllm_response_free(response);
xllm_error_free(&err);
xllm_turn_reset(&turn);
xllm_session_destroy(session);
```

### Export and Restore

```c
xllm_session_state *state = NULL;
xllm_session *restored = NULL;

if (xllm_session_export_state(session, &state) == XRT_NET_OK) {
    xllm_session_import_state(runtime, state, &restored);
}

xllm_session_destroy(restored);
xllm_session_state_free(state);
```

---

## Common Mistakes

| Problem | Common Cause | Fix |
| --- | --- | --- |
| `session chat arguments are invalid` | Session has no profile, or turn/response output parameter is null. | Set `sProfileId` when creating session, or make sure the profile exists after importing state. |
| History keeps growing | Compacting is disabled or keep-recent-turns is too large. | Use default auto compacting, or call `xllm_session_compact` manually. |
| Session is treated as long-term memory | Session only stores short-term task context. | Use Memory API for long-term knowledge. |
| State restore fails | Runtime has not registered the profile required by the state. | Register adapter/profile before importing state. |
| Async session crashes | Session/runtime was destroyed before future completion. | Wait for the future to complete before releasing objects. |

## Related Examples

- `examples\agent_loop\agent_loop.c`
- `examples\glm\glm_session.c`
- `examples\gemini\gemini_session.c`
- `examples\smoke_session_history.c`
- `examples\smoke_session_compact.c`
- `examples\smoke_session_state_options.c`
