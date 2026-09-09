# Your First xllm Program

> Status: Chinese draft reviewed; English translation generated.

This tutorial helps you write a minimal xllm program. The goal is not to learn every feature at once, but to run the most important call sequence first: create a runtime, register an adapter/profile, build a turn, send a request, read the response, and release resources.

## The 4 Objects to Understand First

| Object | How to Think About It | Lifetime |
| --- | --- | --- |
| `xllm_runtime` | The global xllm runtime environment. | Create after program startup, destroy before exit. |
| `xllm_adapter` | Protocol implementation for a provider family. | Registered into the runtime. |
| `xllm_profile` | Concrete provider configuration. | Registered into the runtime and selected by ID. |
| `xllm` | Lightweight chat object bound to a profile. | Call `xllm_destroy` when done. |

The minimal call order is:

```text
xrtInit
  -> xllm_runtime_create
  -> xllm_register_*_adapter
  -> xllm_register_profile
  -> xllm_create / xllm_bind_profile
  -> xllm_turn_add_user_text
  -> xllm_send_ex
  -> xllm_response_get_text
  -> free/reset/destroy
```

## Step 1: Choose a Provider

Real calls need a provider endpoint, model name, and secret. To keep the structure clear, this tutorial uses GLM native as the example.

Prepare an environment variable:

```bat
set GLM_API_KEY=your-secret
```

The key configuration in the example is:

```c
tProfile.sId = "glm-native";
tProfile.sProvider = "zhipu";
tProfile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
tProfile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
tProfile.tAuth.sSecret = getenv("GLM_API_KEY");
tProfile.tModels.tText.sModelId = "glm-5-turbo";
```

If you use OpenAI-compatible, Qwen, Gemini, Ollama, or another provider, the overall order stays the same. Only the adapter registration function and profile fields change.

## Step 2: Write the Minimal Program

The code below uses `xllm-session.h` because it provides `xllm_turn_add_user_text` and `xllm_send_ex`, which are easier for beginners.

```c
#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    xllm_runtime *runtime = NULL;
    xllm *llm = NULL;
    xllm_response *response = NULL;
    xllm_profile profile;
    xllm_turn turn;
    xllm_error error;
    int status;

    xrtInit();

    xllm_profile_init(&profile);
    xllm_turn_init(&turn);
    xllm_error_init(&error);

    if (xllm_runtime_create(NULL, &runtime) != XRT_NET_OK) {
        fprintf(stderr, "failed to create runtime\n");
        return 1;
    }

    if (xllm_register_glm_native_adapter(runtime) != XRT_NET_OK) {
        fprintf(stderr, "failed to register adapter\n");
        xllm_runtime_destroy(runtime);
        return 2;
    }

    profile.sId = "glm-native";
    profile.sProvider = "zhipu";
    profile.sAdapter = XLLM_ADAPTER_GLM_NATIVE;
    profile.sBaseUrl = "https://open.bigmodel.cn/api/paas/v4";
    profile.tAuth.eKind = XLLM_AUTH_BEARER;
    profile.tAuth.sSecret = getenv("GLM_API_KEY");
    profile.tModels.tText.sModelId = "glm-5-turbo";
    profile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;

    if (xllm_register_profile(runtime, &profile) != XRT_NET_OK) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(runtime);
        return 3;
    }

    llm = xllm_create(runtime, NULL);
    if (!llm || xllm_bind_profile(llm, "glm-native") != XRT_NET_OK) {
        fprintf(stderr, "failed to create llm or bind profile\n");
        xllm_destroy(llm);
        xllm_runtime_destroy(runtime);
        return 4;
    }

    xllm_set_system_prompt(llm, "You are a concise assistant.");
    xllm_turn_add_user_text(&turn, "Introduce xllm in one sentence.");

    status = xllm_send_ex(llm, &turn, NULL, &response, &error);
    if (status != XRT_NET_OK || !response) {
        fprintf(stderr, "request failed: %s\n",
            error.sMessage ? error.sMessage : "(no message)");
        xllm_error_reset(&error);
        xllm_turn_reset(&turn);
        xllm_destroy(llm);
        xllm_runtime_destroy(runtime);
        return 5;
    }

    printf("%s\n", xllm_response_get_text(response));

    xllm_response_free(response);
    xllm_error_reset(&error);
    xllm_turn_reset(&turn);
    xllm_destroy(llm);
    xllm_runtime_destroy(runtime);
    return 0;
}
```

## Step 3: Compile

Examples in the repository usually include their own `build.bat`. For learning, start from an existing provider example:

```bat
cd D:\git\xllm\examples\glm
build.bat
```

If you create your own single-file program, make sure:

- The compiler can find `xllm-session.h`, `xllm.h`, and `lib/xrt.h`.
- Implementation files are included in the build. Single-header mode is usually handled through `XLLM_IMPLEMENTATION` or the example build scripts.
- On Windows, you have a C compiler such as MinGW-w64 `gcc`.

## What You Just Completed

Although the program is short, it has completed the real call path:

1. `xllm_runtime_create` creates the runtime.
2. `xllm_register_glm_native_adapter` registers the protocol adapter.
3. `xllm_register_profile` registers concrete provider configuration.
4. `xllm_create` creates a lightweight chat object.
5. `xllm_bind_profile` selects the profile.
6. `xllm_turn_add_user_text` adds user input.
7. `xllm_send_ex` makes the call.
8. `xllm_response_get_text` reads text output.
9. `xllm_response_free`, `xllm_turn_reset`, `xllm_destroy`, and `xllm_runtime_destroy` release resources.

## Common Questions

### Why start with `xllm-session.h`?

`xllm.h` provides low-level request/response APIs for full control over messages, parts, and context blocks. `xllm-session.h` adds turn helpers, which make resource management easier when learning.

### Why register both adapter and profile?

The adapter answers "how to communicate with this provider family." The profile answers "which endpoint, secret, model, and capability declaration are used for this call." One adapter can serve multiple profiles.

### Should I free strings returned by the response?

Do not separately free the string returned by `xllm_response_get_text`. It belongs to `xllm_response`; call `xllm_response_free(response)` at the end.

## Next Steps

- Continue with [Provider and Profile Introduction](provider-profile-intro.en.md).
- For function details, read [Core API](../api/api-core.en.md) and [Session API](../api/api-session.en.md).

[Back to Tutorials](README.en.md)
