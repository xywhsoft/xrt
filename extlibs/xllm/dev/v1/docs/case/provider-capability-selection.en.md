# Choosing Provider Paths by Model Capability

This case shows how a host program can choose text, streaming, tool, image, file, and JSON/schema call paths based on model capabilities.

[Back to Case Studies](README.en.md) | [Provider/Profile Introduction](../guide/provider-profile-intro.en.md) | [Providers API](../api/api-providers.en.md)

## Problem

Different providers and models support different capabilities. Some models only support text, some support streaming output, some support tool calls, some support image or file input, and some support JSON/schema output.

If an application does not check capabilities, it may encounter runtime problems:

- The request is constructed successfully, but the provider rejects it.
- The model does not support tool calls.
- The request contains an image, but the profile has no multimodal model.
- JSON schema is required, but the model only supports plain text.
- Streaming is enabled, but the provider or model does not support it.

The goal of this case is to choose the right path from profile capabilities before sending, and give the user clear feedback when a capability is unsupported.

## Architecture

```text
user task
  -> identify required capabilities
  -> choose slot from profile/model caps
  -> build turn/request
  -> xllm_validate_request
  -> xllm_send_ex or xllm_session_chat_ex
```

Capability selection should not be scattered across UI code. Wrap it in a small function: input task requirements, output profile ID, slot, call options, and whether fallback is allowed.

## Common Capability Flags

| Capability | Constant | Typical Requirement |
| --- | --- | --- |
| Text input | `XLLM_CAP_TEXT_IN` | User text input |
| Image input | `XLLM_CAP_IMAGE_IN` | Image understanding, OCR, screenshot understanding |
| File input | `XLLM_CAP_FILE_IN` | Uploaded PDFs, documents, code files |
| Tool result input | `XLLM_CAP_TOOL_RESULT_IN` | Automatic tool loop |
| Text output | `XLLM_CAP_TEXT_OUT` | Normal chat |
| JSON output | `XLLM_CAP_JSON_OUT` | Structured result |
| Tool call output | `XLLM_CAP_TOOL_CALL_OUT` | Model calls tools |
| Streaming output | `XLLM_CAP_STREAM` | Typewriter effect, real-time response |
| Reasoning control | `XLLM_CAP_REASONING_CONTROL` | Control reasoning level or budget |
| Parallel tools | `XLLM_CAP_PARALLEL_TOOL_CALL` | Multiple tool calls in one turn |

A text model is usually declared like this in a profile:

```c
tProfile.tModels.tText.sModelId = "glm-5-turbo";
tProfile.tModels.tText.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_TEXT_OUT |
    XLLM_CAP_STREAM;
```

If the same provider has a multimodal model, declare it too:

```c
tProfile.tModels.tMultimodal.sModelId = "vision-model";
tProfile.tModels.tMultimodal.tCaps.uFlags =
    XLLM_CAP_TEXT_IN |
    XLLM_CAP_IMAGE_IN |
    XLLM_CAP_TEXT_OUT;
```

## Modeling Task Requirements

The host program can organize a user task into required capabilities:

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

Then compute capability flags:

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

Streaming output is usually a preference, not a hard requirement. Unless your product must respond in real time, you can fall back to non-streaming when streaming is unsupported.

## Choosing Text or Multimodal Slot

`xllm_slot` selects a model slot:

| Slot | Usage |
| --- | --- |
| `XLLM_SLOT_AUTO` | Let xllm choose based on input |
| `XLLM_SLOT_TEXT` | Force the text model |
| `XLLM_SLOT_MULTIMODAL` | Force the multimodal model |

If the user uploads an image, usually choose multimodal:

```c
xllm_turn tTurn;
xllm_turn_init(&tTurn);

tTurn.eSlot = XLLM_SLOT_MULTIMODAL;
xllm_turn_add_user_text(&tTurn, "Please describe this image.");
xllm_turn_add_image_file(&tTurn, "screenshot.png", "image/png");
```

For text-only tasks, `XLLM_SLOT_TEXT` and `XLLM_SLOT_AUTO` can both work. Start with `AUTO` while learning; specify a slot when you need to avoid an unintended model choice.

## Validating Before Sending

In complex applications, validate the request before sending so problems surface earlier:

```c
xllm_error tError;
xllm_request tRequest;
xllm_call_options tCallOptions;

xllm_error_init(&tError);
xllm_request_init(&tRequest);
xllm_call_options_init(&tCallOptions);

/* Build tRequest, including profile, slot, messages, tools, response format, etc. */

if ( xllm_validate_request(pRuntime, &tRequest, &tCallOptions, &tError) != XRT_NET_OK ) {
    fprintf(stderr, "request unsupported: code=%d required=0x%llx model=%s\n",
            (int)tError.eCode,
            (unsigned long long)tError.uRequiredCapability,
            tError.sSelectedModel ? tError.sSelectedModel : "(null)");
}
```

If validation fails, `uRequiredCapability` and `sSelectedModel` can help you tell the user "the current model does not support image input" or "the current model does not support tool calls".

## JSON Output Path

If a task needs structured output, prefer response format instead of only writing "please return JSON" in the prompt.

```c
xllm_turn_set_json_schema_response(
    &tTurn,
    "extract_result",
    tJsonSchema,
    NULL
);
```

The profile should also declare:

```c
tProfile.tModels.tText.tCaps.uFlags |= XLLM_CAP_JSON_OUT;
```

If the model does not support JSON/schema, the application can fall back to plain text, but should clearly tell the caller that the result no longer has strong structure guarantees.

## Tool Call Path

Tool calling needs bidirectional capabilities:

```c
tProfile.tModels.tText.tCaps.uFlags |=
    XLLM_CAP_TOOL_CALL_OUT |
    XLLM_CAP_TOOL_RESULT_IN;
```

Then add tools to the turn and set an executor:

```c
xllm_turn_add_tool(&tTurn, &tTool);
xllm_set_tool_executor(pLlm, &tExecutor);
```

If the provider does not support tool calls, you can:

- Disable the tool button.
- Fall back to ordinary text Q&A.
- Execute fixed retrieval on the host side first, then inject results as context blocks.

Do not fabricate a tool protocol for the provider when the model does not support tool calls. That makes behavior unpredictable.

## Streaming Fallback Strategy

Streaming output is good for UI experience, but it is not always a hard requirement:

```c
xllm_call_options_init(&tCallOptions);

if ( bModelSupportsStream ) {
    tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
    tCallOptions.pfnOnEvent = on_event;
} else {
    tCallOptions.eStreamMode = XLLM_STREAM_OFF;
}
```

If your product must output in real time, use `XLLM_STREAM_REQUIRE`. In that case, unsupported streaming should fail directly instead of silently falling back.

## Multi-Provider Configuration Advice

An application usually registers multiple profiles:

| Profile | Purpose |
| --- | --- |
| `fast-text` | Low-latency normal chat |
| `strong-text` | Complex reasoning |
| `vision` | Image input |
| `tool-agent` | Tool calls |
| `json-extract` | JSON/schema output |

A simple selection strategy:

1. If there is an image or file, choose a multimodal profile.
2. If tools are needed, choose a tool-capable profile.
3. If JSON/schema is needed, choose a structured-output profile.
4. For ordinary text, choose fast-text.
5. If the user asks for higher quality or complex reasoning, switch to strong-text.

## Key APIs

| API/Type | Purpose |
| --- | --- |
| `xllm_profile` | Define provider, adapter, model, and capabilities |
| `xllm_model_caps` | Describe model capabilities and limits |
| `xllm_capability_flags` | Express a capability set |
| `xllm_slot` | Select text or multimodal model slot |
| `xllm_validate_request` | Validate whether a request is supported before sending |
| `xllm_turn_add_image_file` | Add image input |
| `xllm_turn_add_tool` | Add tool definitions |
| `xllm_turn_set_json_schema_response` | Request JSON schema output |

## Complete Example Sources

Refer to these examples for different paths:

```text
examples/glm/glm_stateless.c
examples/glm/glm_session.c
examples/glm/glm_tool_loop.c
examples/openai/azure_openai/azure_openai_tool_loop.c
examples/gemini/gemini_session.c
examples/qwen/qwen_tool_loop.c
```

Real provider capabilities change with models. When updating a provider or model, rerun the matching smoke tests or provider probe.

## Common Questions

Do not infer capabilities only from the provider name. Different models under the same provider may have different capabilities.

Do not treat streaming as required by default. Many applications can fall back gracefully to non-streaming.

Do not send images through a profile that does not declare `XLLM_CAP_IMAGE_IN`.

Do not enable automatic tool loops on a model without `XLLM_CAP_TOOL_RESULT_IN`.

Do not treat JSON/schema capability as the same thing as "asking for JSON in the prompt". Real structured output should use response format.
