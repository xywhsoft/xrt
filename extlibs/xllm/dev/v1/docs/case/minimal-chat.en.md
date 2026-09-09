# Minimal Chat Call

This case shows a minimal xllm text chat program: create a runtime, register a provider, register a profile, send one user message, and read the model answer.

[Back to Case Studies](README.en.md) | [First xllm Program](../guide/first-xllm-program.en.md) | [Core API](../api/api-core.en.md)

## Problem

You want to complete one chat request with the smallest practical set of xllm API calls. This scenario does not need long-term history, tools, or memory. It only sends user text to the model and receives an answer.

This case is useful for:

- Verifying whether the provider API key works.
- Verifying whether xllm compiles and links correctly.
- Learning the minimal relationship between runtime, profile, turn, and response.
- Building a foundation for more complex session, tool, and memory scenarios.

## Architecture

The minimal call chain is:

```text
application
  -> xllm_runtime
    -> provider adapter
    -> provider profile
  -> xllm
    -> xllm_turn
    -> xllm_send_ex
  <- xllm_response
```

You can think of `xllm_runtime` as the global runtime environment, `xllm_profile` as the connection configuration for one provider/model, and `xllm_turn` as the new input for this turn.

## Step 1: Initialize Runtime

```c
xllm_runtime *pRuntime = NULL;

xrtInit();

if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
    fprintf(stderr, "failed to create runtime\n");
    return 1;
}
```

`xrtInit()` comes from xrt. xllm is built on top of xrt, so example programs should initialize xrt first.

## Step 2: Register a Provider Adapter

Using the GLM native adapter as an example:

```c
if ( xllm_register_glm_native_adapter(pRuntime) != XRT_NET_OK ) {
    fprintf(stderr, "failed to register glm native adapter\n");
    xllm_runtime_destroy(pRuntime);
    return 2;
}
```

The adapter converts xllm's common request structure into the provider's HTTP request, then parses the provider response back into `xllm_response`.

## Step 3: Register a Profile

A profile describes which provider, base URL, authentication method, model, and capabilities this call uses.

```c
xllm_profile tProfile;

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
    fprintf(stderr, "failed to register profile\n");
    xllm_runtime_destroy(pRuntime);
    return 3;
}
```

The field learners most often miss is `tCaps.uFlags`. It tells xllm that the current model supports text input, text output, streaming output, and so on. Tool, JSON, image, file, and other capabilities are also declared here.

## Step 4: Create an xllm Object and Bind the Profile

```c
xllm *pLlm = xllm_create(pRuntime, NULL);

if ( !pLlm || xllm_bind_profile(pLlm, "glm-native") != XRT_NET_OK ) {
    fprintf(stderr, "failed to create llm or bind profile\n");
    xllm_runtime_destroy(pRuntime);
    return 4;
}

xllm_set_system_prompt(pLlm, "You are a concise assistant.");
```

An `xllm` object is suitable for one or a few stateless requests. If you need continuous chat, use `xllm_session`; see [Session Chat with Short-Term History](session-chat.en.md).

## Step 5: Build a Turn

```c
xllm_turn tTurn;

xllm_turn_init(&tTurn);
xllm_turn_add_user_text(&tTurn, "Use one sentence to introduce xllm.");
```

A turn is "new input for this round". The minimal chat contains only one user text part. Later, multimodal input, tool definitions, and JSON output formats can also be placed into a turn.

## Step 6: Send the Request and Read the Response

```c
xllm_call_options tCallOptions;
xllm_error tError;
xllm_response *pResponse = NULL;
int iStatus;

xllm_call_options_init(&tCallOptions);
tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
tCallOptions.uTimeoutMs = 120000u;

xllm_error_init(&tError);

iStatus = xllm_send_ex(pLlm, &tTurn, &tCallOptions, &pResponse, &tError);
if ( iStatus != XRT_NET_OK || !pResponse ) {
    fprintf(stderr, "request failed: code=%d http=%d msg=%s\n",
            (int)tError.eCode,
            (int)tError.iHttpStatus,
            tError.sMessage ? tError.sMessage : "(null)");
    return 5;
}

printf("%s\n", xllm_response_get_text(pResponse));
```

`xllm_response_get_text` returns visible text and is suitable for normal chat UIs. If you need to read tool calls, JSON output, or multiple output parts, use the response API.

## Step 7: Clean Up Resources

```c
xllm_response_free(pResponse);
xllm_error_free(&tError);
xllm_turn_reset(&tTurn);
xllm_destroy(pLlm);
xllm_runtime_destroy(pRuntime);
```

Cleanup order does not have to be exactly the reverse of creation order, but make sure:

- `xllm_response_free` releases the response.
- `xllm_turn_reset` releases messages and parts copied inside the turn.
- `xllm_destroy` releases the `xllm` object.
- `xllm_runtime_destroy` releases the runtime last.

## Complete Example

The complete runnable example is:

```text
examples/glm/glm_stateless.c
```

Before running it, set the environment variable:

```powershell
$env:GLM_API_KEY="your GLM API key"
```

Then run the GLM example build script from the repository root:

```bat
cmd /c .\examples\glm\build.bat
```

The script generates `build\glm_native_stateless.exe` and also builds the session, reasoning, JSON schema, tool loop, multimodal, and stream examples. Set `GLM_API_KEY` before running the stateless example. The example sends one sentence request and prints both streaming deltas and final visible text.

## Key APIs

| API | Purpose |
| --- | --- |
| `xllm_runtime_create` | Create the runtime environment |
| `xllm_register_glm_native_adapter` | Register the GLM native adapter |
| `xllm_profile_init` | Initialize a profile |
| `xllm_register_profile` | Register a profile into the runtime |
| `xllm_create` | Create a stateless `xllm` object |
| `xllm_bind_profile` | Bind the default profile |
| `xllm_turn_add_user_text` | Add user text |
| `xllm_send_ex` | Send a request and receive error details |
| `xllm_response_get_text` | Read answer text |
| `xllm_response_free` | Free the response |

## Extension Points

You can continue from this minimal case by adding:

- Streaming event callback: display text deltas in real time.
- JSON schema output: make the model return structured data.
- Provider switching: use the same request with OpenAI compatible, Qwen, Kimi, or other profiles.
- Session: save multi-turn chat history.
- Memory: inject retrieval results into the turn.

## Common Questions

If you get an authentication error, first check whether `GLM_API_KEY` is set, whether it has leading/trailing spaces, and whether the provider base URL is correct.

If you get an unsupported-capability error, check whether `tProfile.tModels.tText.tCaps.uFlags` contains the capabilities required by the request.

If there is no streaming output, confirm that the profile declares `XLLM_CAP_STREAM` and that `tCallOptions.eStreamMode` is not `XLLM_STREAM_OFF`.

If the response is empty, print `xllm_error` first, then enable diagnostics trace to inspect the raw provider response.
