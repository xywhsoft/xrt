# Request / Response Introduction

> Status: Chinese draft reviewed; English translation generated.

This tutorial explains what one xllm call consists of: how to build input, control output format, read text, JSON, and tool calls, and release the response.

## Two Usage Paths

xllm has two ways to construct requests:

| Path | Best For | Representative APIs |
| --- | --- | --- |
| Turn helper | Beginners, normal chat, sessions, agents | `xllm_turn_add_user_text`, `xllm_send_ex` |
| Raw request | Full control over messages, parts, context, tools | `xllm_request`, `xllm_chat_ex` |

If you are just starting, use turn helpers first. Switch to raw requests when you need to control multiple messages, multimodal parts, or low-level context blocks.

## What an Input Contains

The low-level structure is:

```text
xllm_request
  -> messages[]
    -> parts[]
```

A message has a role, such as `USER`, `ASSISTANT`, `SYSTEM`, or `TOOL`. A part is concrete content, such as text, image, file, or JSON.

Turn helpers build these structures for you:

```c
xllm_turn turn;
xllm_turn_init(&turn);
xllm_turn_add_user_text(&turn, "Explain what RAG is.");
```

## Minimal Text Request

```c
xllm_turn turn;
xllm_response *response = NULL;
xllm_error error;

xllm_turn_init(&turn);
xllm_error_init(&error);

xllm_turn_add_user_text(&turn, "Explain xllm in one sentence.");

if (xllm_send_ex(llm, &turn, NULL, &response, &error) == XRT_NET_OK) {
    printf("%s\n", xllm_response_get_text(response));
}

xllm_response_free(response);
xllm_error_reset(&error);
xllm_turn_reset(&turn);
```

## Reading Text Output

The most common read method is:

```c
const char *text = xllm_response_get_text(response);
```

This returns the visible text already collected from the response. It is suitable for normal chat, summaries, Q&A, and explanations.

Do not free `text`; it belongs to `response`.

## Reading Output Items

When you need finer output handling, iterate output items:

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

This is useful for checking whether the model returned a message, tool call, thinking item, or another structured output.

## Requesting JSON Output

If the provider and model support JSON output, set response format. With turn helpers, the common approach is JSON schema:

```c
xllm_turn_set_json_schema_response(
    &turn,
    "answer_schema",
    schema_value,
    0);
```

After the call, read JSON:

```c
size_t output_index = 0;
size_t part_index = 0;
const xvalue *json = xllm_response_get_first_json(
    response,
    &output_index,
    &part_index);
```

If `json == NULL`, there was no JSON part. Check that the model capability includes `XLLM_CAP_JSON_OUT` and that the provider supports the current schema mode.

## Reading Tool Calls

If you provide tool definitions in the turn or request, the model may return tool calls:

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

If you want xllm to execute the tool loop automatically, continue with [Tool Loop Introduction](tool-loop-intro.en.md).

## What Call Options Control

`xllm_call_options` controls how one call runs, not which provider identity is used.

Common fields include:

| Capability | What You Use It For |
| --- | --- |
| stream mode | Request or disable streaming output. |
| event callback | Receive streaming deltas, start/end, and errors. |
| timeout | Control call timeout. |
| cancel token | Cancel a call externally. |
| artifact policy | Control large output/artifact handling. |

Basic streaming form:

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

## Response Cleanup Rules

Whenever an API returns a response through `xllm_response **ppResponse`, release it with:

```c
xllm_response_free(response);
```

Do not separately free strings, arrays, JSON pointers, or tool call pointers inside the response.

## Using Error Objects

Use `_ex` variants when debugging:

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

`eCode` is the normalized xllm error code, `iHttpStatus` is the provider HTTP status, and `sProviderCode` / `sProviderMessage` are upstream error details.

## When to Use Raw Request

Raw requests are better when you need:

- Multiple history messages in one request.
- Manually constructed context blocks.
- Text, image, file, and JSON parts together.
- Precise control over tools, tool policy, and generation params.
- Direct `xllm_chat_ex` calls without a lightweight `xllm` object.

Raw request entry:

```c
xllm_request request;
memset(&request, 0, sizeof(request));
request.sProfileId = "glm-native";
```

But raw request structs include array pointers, which makes resource management easier to get wrong. Beginners should get comfortable with turn helpers first.

## Common Mistakes

### Forgetting to free response

The string returned by `xllm_response_get_text` does not need to be freed, but the response itself must be freed.

### Not checking whether JSON is null

Not every model returns JSON as expected. Always check the returned pointer after reading JSON.

### Request capability does not match model capability

If you request streaming, JSON, tools, or multimodal input, the profile's model caps should declare the corresponding capability.

## Next Steps

- To keep multi-turn history, continue with [Session Introduction](session-intro.en.md).
- To execute tools automatically, continue with [Tool Loop Introduction](tool-loop-intro.en.md).
- API details: [Request/Response API](../api/api-request-response.en.md).

[Back to Tutorials](README.en.md)
