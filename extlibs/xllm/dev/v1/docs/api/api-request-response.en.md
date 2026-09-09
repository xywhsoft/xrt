# xllm Request / Response API

> This page explains how to build a core request, read model responses, handle JSON/tool-call output, and release related resources correctly.

[Back to API Index](README.en.md) | [Basic Types](types.en.md) | [Core Runtime API](api-core.en.md) | [Tools API](api-tools.en.md)

---

## Table of Contents

- [Module Role](#module-role)
- [Core Objects](#core-objects)
- [Request Lifecycle](#request-lifecycle)
  - [xllm_request_init](#xllm_request_init)
  - [xllm_request_reset](#xllm_request_reset)
  - [xllm_call_options_init](#xllm_call_options_init)
- [Error Object Lifecycle](#error-object-lifecycle)
  - [xllm_error_init](#xllm_error_init)
  - [xllm_error_reset](#xllm_error_reset)
  - [xllm_error_free](#xllm_error_free)
- [Reading Responses](#reading-responses)
  - [xllm_response_get_text](#xllm_response_get_text)
  - [xllm_response_get_output_count](#xllm_response_get_output_count)
  - [xllm_response_get_output](#xllm_response_get_output)
  - [xllm_response_get_tool_call_count](#xllm_response_get_tool_call_count)
  - [xllm_response_get_tool_call](#xllm_response_get_tool_call)
  - [xllm_response_get_json](#xllm_response_get_json)
  - [xllm_response_get_first_json](#xllm_response_get_first_json)
  - [xllm_response_free](#xllm_response_free)
- [Freeing Tool Execution Results](#freeing-tool-execution-results)
  - [xllm_tool_exec_result_free](#xllm_tool_exec_result_free)
- [Common Usage](#common-usage)
- [Common Mistakes](#common-mistakes)
- [Related Examples](#related-examples)

---

## Module Role

Request / Response API is the "data plane" of xllm:

- `xllm_request` describes the content, profile, tools, generation parameters, and output format you send to the model.
- `xllm_call_options` describes how this call runs, such as streaming, timeout, cancellation, artifacts, and retries.
- `xllm_response` describes model-returned text, JSON, tool calls, thinking, refusals, usage, and raw extension data.
- `xllm_error` describes failure reasons and is suitable for logs, UI messages, and automatic fallback.

If you use `xllm_session_chat`, you usually build `xllm_turn` instead of `xllm_request`; response reading and error handling still follow the rules on this page.

---

## Core Objects

### `xllm_request`

| Field | Description |
| --- | --- |
| `sProfileId` | Profile ID used by this request. It must match a registered profile. |
| `eSlot` | Model slot: `XLLM_SLOT_AUTO`, `XLLM_SLOT_TEXT`, or `XLLM_SLOT_MULTIMODAL`. |
| `pMessages` / `iMessageCount` | Message array. |
| `pContextBlocks` / `iContextBlockCount` | Additional context blocks, often injected by memory/context packing. |
| `pTools` / `iToolCount` | Available tool definitions. |
| `tToolPolicy` | Tool selection policy. |
| `tGeneration` | Generation parameters such as temperature, top_p, max tokens, and stop. |
| `tResponseFormat` | Text, JSON, or JSON schema output requirement. |
| `tReasoning` | Reasoning-related parameters. |
| `tVendorExtra` | Provider or host extension fields. |

**Ownership:**

When manually constructing a low-level `xllm_request`, if you put strings, arrays, or `xvalue` values into the request and expect `xllm_request_reset()` to free them, use xllm/xrt-compatible allocation and follow internal ownership rules. The safest learning path is to follow repository examples or use session-layer helpers such as `xllm_turn_add_user_text` to reduce manual memory management.

### `xllm_call_options`

| Field | Description |
| --- | --- |
| `eStreamMode` | Whether to use streaming. Default is `XLLM_STREAM_AUTO`. |
| `uTimeoutMs` | Total call timeout. `0` means use the default policy. |
| `pCancelToken` | Optional cancel token. |
| `pfnOnEvent` / `pUserData` | Streaming event callback. |
| `eArtifactPolicy` / `pArtifactSink` | Artifact handling policy. |
| retry fields such as `uMaxRetries` | Provider/network retry policy. |
| `bBestEffortStructuredOutput` | Whether best-effort structured output is allowed. |
| `eLocalFilePolicy` | Local file handling policy. |
| `tVendorExtra` | Extension fields. |

### `xllm_response`

`xllm_response` is allocated by call APIs. Callers should not create it on the stack. Read it, then release it with `xllm_response_free()`.

| Field | Description |
| --- | --- |
| `sVisibleText` | Text suitable for direct display. |
| `pOutputs` / `iOutputCount` | Structured output array. |
| `tUsage` | Token usage. |
| `tRefusal` / `tSafety` | Refusal and safety information. |
| `tEffectiveParams` | Effective parameters. |
| `bHasError` / `tError` | Error inside the response. |
| `tRaw` / `tVendorExtra` | Raw or extension information. |

---

## Request Lifecycle

### xllm_request_init

Initializes `xllm_request`.

**Purpose:**

Call this first after creating an `xllm_request` on the stack or heap to get safe defaults.

**Prototype:**

```c
XLLM_API void xllm_request_init(xllm_request *pRequest);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRequest` | output | yes | Request object to initialize. If `NULL`, the function returns immediately. |

**Return Value:**

None.

**Resource Ownership:**

Does not allocate resources. `pRequest` is caller-owned.

**Notes:**

After initialization:

- `eSlot = XLLM_SLOT_AUTO`
- `tToolPolicy.eMode = XLLM_TOOL_CHOICE_AUTO`
- `tResponseFormat.eKind = XLLM_RESPONSE_TEXT`
- `tReasoning.eLevel = XLLM_REASONING_DEFAULT`

**Example Code:**

```c
xllm_request req;
xllm_request_init(&req);
req.sProfileId = "demo";
```

**Related APIs:**

- `xllm_request_reset`
- `xllm_chat_ex`

---

### xllm_request_reset

Releases internal request resources and restores defaults.

**Purpose:**

After a request finishes, call it to release internal strings, arrays, content parts, context blocks, tools, `xvalue`, and similar resources, then reinitialize the object.

**Prototype:**

```c
XLLM_API void xllm_request_reset(xllm_request *pRequest);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pRequest` | input/output | yes | Request to clean. |

**Return Value:**

None.

**Resource Ownership:**

This function releases internal resources that xllm considers owned by the request. The request can be reused after the call.

**Notes:**

- Do not put string literals or memory not allocated by xllm/xrt into fields that reset will free, unless a construction API explicitly says it copied the data.
- If you only borrow external memory temporarily, make sure reset will not free that field, or use helper APIs instead.

**Example Code:**

```c
xllm_request req;
xllm_request_init(&req);

/* Build and call. */

xllm_request_reset(&req);
```

**Related APIs:**

- `xllm_request_init`

---

### xllm_call_options_init

Initializes `xllm_call_options`.

**Purpose:**

Use this structure when you need streaming, timeout, cancellation, retry, or artifact handling. Whenever you need non-default behavior, initialize first and then modify fields.

**Prototype:**

```c
XLLM_API void xllm_call_options_init(xllm_call_options *pOptions);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pOptions` | output | yes | Call options to initialize. |

**Return Value:**

None.

**Resource Ownership:**

Does not allocate resources. `pOptions` is caller-owned.

**Notes:**

After initialization:

- `eStreamMode = XLLM_STREAM_AUTO`
- `eArtifactPolicy = XLLM_ARTIFACT_INLINE_SMALL`
- `eLocalFilePolicy = XLLM_LOCAL_FILE_AUTO`

**Example Code:**

```c
xllm_call_options opt;
xllm_call_options_init(&opt);
opt.eStreamMode = XLLM_STREAM_PREFER;
opt.uTimeoutMs = 90000;
```

**Related APIs:**

- `xllm_chat_ex`
- `xllm_cancel_token_create`

---

## Error Object Lifecycle

### xllm_error_init

Initializes an error object.

**Prototype:**

```c
XLLM_API void xllm_error_init(xllm_error *pError);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pError` | output | yes | Error object to initialize. |

**Return Value:**

None. After initialization, `eCode = XLLM_ERROR_NONE`.

**Resource Ownership:**

Does not allocate resources.

**Example Code:**

```c
xllm_error err;
xllm_error_init(&err);
```

**Related APIs:**

- `xllm_error_reset`
- `xllm_error_free`

---

### xllm_error_reset

Releases internal strings and extension data in the error object, then restores it to "no error".

**Prototype:**

```c
XLLM_API void xllm_error_reset(xllm_error *pError);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pError` | input/output | yes | Error object to reset. |

**Return Value:**

None.

**Resource Ownership:**

Releases `sMessage`, provider error fields, request id, selected model, MIME type, and `tVendorExtra`.

**Example Code:**

```c
xllm_error_reset(&err);
```

**Related APIs:**

- `xllm_chat_ex`

---

### xllm_error_free

Releases internal resources in an error object.

**Prototype:**

```c
XLLM_API void xllm_error_free(xllm_error *pError);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pError` | input/output | yes | Error object whose internals should be released. |

**Return Value:**

None.

**Resource Ownership:**

This function does not free the outer `pError` structure itself. It only releases internal resources. A stack object can simply leave scope after this call.

**Example Code:**

```c
xllm_error err;
xllm_error_init(&err);
/* Call API. */
xllm_error_free(&err);
```

**Related APIs:**

- `xllm_error_init`

---

## Reading Responses

### xllm_response_get_text

Gets the text best suited for direct display.

**Purpose:**

Use this first when you only want to display model output to the user. It returns `sVisibleText`, refusal text, refusal text in output items, or finally the first text message part in that order.

**Prototype:**

```c
XLLM_API const char *xllm_response_get_text(const xllm_response *pResponse);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response object. |

**Return Value:**

- Returns a borrowed text pointer.
- Returns `NULL` if there is no displayable text or `pResponse=NULL`.

**Resource Ownership:**

The returned pointer belongs to `pResponse`; do not free it. Copy the string if you need to keep it long term.

**Example Code:**

```c
const char *text = xllm_response_get_text(response);
printf("%s\n", text ? text : "");
```

**Related APIs:**

- `xllm_response_get_output`
- `xllm_response_free`

---

### xllm_response_get_output_count

Gets the number of output items in a response.

**Prototype:**

```c
XLLM_API size_t xllm_response_get_output_count(const xllm_response *pResponse);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response object. |

**Return Value:**

- Number of output items.
- Returns `0` if `pResponse=NULL`.

**Resource Ownership:**

Does not allocate resources.

**Example Code:**

```c
for (size_t i = 0; i < xllm_response_get_output_count(response); ++i) {
    const xllm_output_item *out = xllm_response_get_output(response, i);
}
```

**Related APIs:**

- `xllm_response_get_output`

---

### xllm_response_get_output

Reads an output item by index.

**Prototype:**

```c
XLLM_API const xllm_output_item *xllm_response_get_output(const xllm_response *pResponse, size_t iIndex);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response object. |
| `iIndex` | input | no | Output item index, starting from `0`. |

**Return Value:**

- Returns a borrowed output item pointer on success.
- Returns `NULL` if `pResponse=NULL` or the index is out of range.

**Resource Ownership:**

The returned pointer belongs to the response. Do not free or modify it.

**Example Code:**

```c
const xllm_output_item *out = xllm_response_get_output(response, 0);
if (out && out->eKind == XLLM_OUTPUT_MESSAGE) {
    /* Read out->as.tMessage. */
}
```

**Related APIs:**

- `xllm_response_get_output_count`

---

### xllm_response_get_tool_call_count

Counts tool calls in the response.

**Prototype:**

```c
XLLM_API size_t xllm_response_get_tool_call_count(const xllm_response *pResponse);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response object. |

**Return Value:**

- Number of tool call output items.
- Returns `0` if `pResponse=NULL`.

**Resource Ownership:**

Does not allocate resources.

**Example Code:**

```c
size_t n = xllm_response_get_tool_call_count(response);
```

**Related APIs:**

- `xllm_response_get_tool_call`

---

### xllm_response_get_tool_call

Reads a tool call by tool-call index.

**Prototype:**

```c
XLLM_API const xllm_output_tool_call *xllm_response_get_tool_call(const xllm_response *pResponse, size_t iIndex);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response object. |
| `iIndex` | input | no | Which tool call to read. This is not the raw `pOutputs` index. |

**Return Value:**

- Returns a borrowed tool-call pointer on success.
- Returns `NULL` if it does not exist.

**Resource Ownership:**

The returned pointer belongs to the response. Strings such as `sArgumentsJson` also belong to the response.

**Notes:**

This function skips non-`XLLM_OUTPUT_TOOL_CALL` output items, so `iIndex=0` means the first tool call, not the first output item.

**Example Code:**

```c
for (size_t i = 0; i < xllm_response_get_tool_call_count(response); ++i) {
    const xllm_output_tool_call *call = xllm_response_get_tool_call(response, i);
    printf("tool: %s args: %s\n", call->sToolName, call->sArgumentsJson);
}
```

**Related APIs:**

- `xllm_tool_exec_result_free`
- [api-tools.en.md](api-tools.en.md)

---

### xllm_response_get_json

Reads a JSON value from a specific output item and part.

**Prototype:**

```c
XLLM_API const xvalue *xllm_response_get_json(const xllm_response *pResponse, size_t iOutputIndex, size_t iPartIndex);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response object. |
| `iOutputIndex` | input | no | Output item index. |
| `iPartIndex` | input | no | Message part index. |

**Return Value:**

- If that position is `XLLM_PART_JSON`, returns a borrowed `xvalue` pointer.
- Returns `NULL` if the position is invalid or is not a JSON part.

**Resource Ownership:**

The returned `xvalue` belongs to the response. Copy it according to xvalue rules if you need to keep it long term.

**Example Code:**

```c
const xvalue *json = xllm_response_get_json(response, 0, 0);
if (json) {
    /* Read xvalue. */
}
```

**Related APIs:**

- `xllm_response_get_first_json`

---

### xllm_response_get_first_json

Finds the first JSON part in the response.

**Prototype:**

```c
XLLM_API const xvalue *xllm_response_get_first_json(const xllm_response *pResponse, size_t *piOutputIndex, size_t *piPartIndex);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response object. |
| `piOutputIndex` | output | yes | Receives the output index containing JSON; receives `(size_t)-1` if not found. |
| `piPartIndex` | output | yes | Receives the part index containing JSON; receives `(size_t)-1` if not found. |

**Return Value:**

- Returns the first JSON `xvalue` borrowed pointer if found.
- Returns `NULL` if not found or `pResponse=NULL`.

**Resource Ownership:**

The returned `xvalue` belongs to the response.

**Example Code:**

```c
size_t output_index;
size_t part_index;
const xvalue *json = xllm_response_get_first_json(response, &output_index, &part_index);
if (json) {
    printf("json at output=%zu part=%zu\n", output_index, part_index);
}
```

**Related APIs:**

- `xllm_response_get_json`

---

### xllm_response_free

Frees a response.

**Prototype:**

```c
XLLM_API void xllm_response_free(xllm_response *pResponse);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResponse` | input | yes | Response to free. |

**Return Value:**

None.

**Resource Ownership:**

Releases the outer response object and its internal strings, outputs, usage/refusal/safety/effective params/error/raw/vendor extra.

**Notes:**

- Only free responses returned by xllm.
- All pointers obtained from response getters become invalid after this call.

**Example Code:**

```c
xllm_response_free(response);
response = NULL;
```

**Related APIs:**

- `xllm_chat_ex`

---

## Freeing Tool Execution Results

### xllm_tool_exec_result_free

Frees internal resources of a tool execution result.

**Prototype:**

```c
XLLM_API void xllm_tool_exec_result_free(xllm_tool_exec_result *pResult);
```

**Parameters:**

| Parameter | Direction | Can Be `NULL` | Description |
| --- | --- | --- | --- |
| `pResult` | input/output | yes | Tool execution result. |

**Return Value:**

None.

**Resource Ownership:**

Releases content parts and extension data inside `pResult`, but does not free the outer `pResult` structure itself.

**Notes:**

If you implement `xllm_tool_execute_fn` and allocate returned content in `pResult`, clean it with this function after the call chain is done.

**Example Code:**

```c
xllm_tool_exec_result result;
memset(&result, 0, sizeof(result));
/* Fill result. */
xllm_tool_exec_result_free(&result);
```

**Related APIs:**

- [api-tools.en.md](api-tools.en.md)

---

## Common Usage

### Read Plain Text

```c
xllm_response *response = NULL;
xllm_error err;
xllm_error_init(&err);

if (xllm_chat_ex(runtime, &request, NULL, &response, &err) == XRT_NET_OK) {
    const char *text = xllm_response_get_text(response);
    printf("%s\n", text ? text : "");
}

xllm_response_free(response);
xllm_error_free(&err);
```

### Read JSON Output

```c
const xvalue *json = xllm_response_get_first_json(response, NULL, NULL);
if (!json) {
    fprintf(stderr, "response does not contain JSON\n");
}
```

### Read Tool Calls

```c
size_t count = xllm_response_get_tool_call_count(response);
for (size_t i = 0; i < count; ++i) {
    const xllm_output_tool_call *call = xllm_response_get_tool_call(response, i);
    printf("call %s: %s\n", call->sToolName, call->sArgumentsJson);
}
```

---

## Common Mistakes

| Problem | Common Cause | Fix |
| --- | --- | --- |
| `xllm_response_get_text` returns `NULL` | Response only contains JSON, tool calls, artifacts, or the call failed. | Check `eStatus` and output types first. |
| Tool-call index confused with output index | `xllm_response_get_tool_call` uses "which tool call". | Use `xllm_response_get_output` when you need raw output items. |
| Text pointer used after free | Getters return borrowed pointers. | Copy strings you need before `xllm_response_free`. |
| `xllm_request_reset` crashes | Request contains external memory that xllm should not free. | Use helpers, or follow examples with xrt allocation and clear ownership. |
| Error object leaks | `xllm_error_free` was not called after use. | Pair every `xllm_error_init` with `xllm_error_free`. |

## Related Examples

- `examples\doubao\doubao_json_schema.c`
- `examples\gemini\gemini_json_schema.c`
- `examples\glm\glm_tool_loop.c`
- `examples\smoke_chat_ex_low_level.c`
- `build.bat smoke -Filter "chat_ex_low_level"`
