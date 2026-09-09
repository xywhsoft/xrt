# Profiles API

> Status: Chinese function-by-function reference, waiting for manual review.

The Profiles API provides default runtime, workspace, run, orchestrator, xllm, and policy configurations for product scenarios. Current built-in profiles include `xcode` and `xclaw`.

## Module positioning

Profile is a default policy set, not a complete product policy. The product layer can use the profile as a baseline and then cover more stringent security, model, memory, planner or tool policies.

## This page covers the statement

| Category | Statement |
| --- | --- |
| Constant | `XWORK_PROFILE_XCODE`, `XWORK_PROFILE_XCLAW` |
| Structure | `xwork_profile` |
| Function | `xwork_profile_init`, `xwork_profile_get_builtin`, `xwork_profile_apply_runtime_options`,

## Profile field

| Field | Description |
| --- | --- |
| `sProfileId` | Stable profile id. |
| `sDisplayName` | Display name. |
| `sDescription` | profile description. |
| `sDefaultLlmProfileId` | Default xllm profile id. |
| `sDefaultSessionProfileId` | Default session profile id. |
| `eAutonomy` | Default autonomy. |
| `tPolicy` | Default policy. |
| `tSessionPolicy` | Default session policy. |
| `iDefaultMaxTurns` | The default maximum number of model rounds. |
| `bDefaultAutoApprove` | Whether to automatically approve steps by default. |
| `bEnableWorkspaceMemory` | Whether to enable workspace memory by default. |
| `ePlannerMode` | planner boundary default mode. |

## Built-in Profile

| Profile | Default tendency |
| --- | --- |
| `XWORK_PROFILE_XCODE` / |
| `XWORK_PROFILE_XCLAW` / |

## Recommended coverage order

```text
options_init
xwork_profile_get_builtin
xwork_profile_apply_*_options
The product layer overrides stricter policies
create runtime / workspace / run / orchestrator
```

This prevents the profile from overriding the product-level explicit configuration.

---

### xwork_profile_init

Initialize the profile structure.

**Function:**

You can call this function before manually constructing a profile or receiving a built-in profile to put fields into a stable default state.

**Function prototype:**

```c
XWORK_API void xwork_profile_init(xwork_profile *pProfile);
```

**parameter:**

- `pProfile`: Output parameter. Can be `NULL`; `NULL` does nothing. If not `NULL`, it will be cleared and the default policy will be written.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate heap memory. String fields in profiles maintain borrow semantics.

**Additional Note:**

- Default autonomy is `XWORK_AUTONOMY_SEMI_AUTO`.
- Default max turns is `4`.
- Default auto approve is `true`.
- The default planner mode is `XWORK_PLANNER_OFF`.
- This function initializes nested policy and session policy.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_profile profile;
    xwork_profile_init(&profile);
    return profile.iDefaultMaxTurns == 4u ? 0 : 1;
}
```

**Related API:**

- `xwork_profile_get_builtin`
- `xwork_profile_apply_run_options`

---

### xwork_profile_get_builtin

Get the built-in profile.

**Function:**

You can use this function to load the default configuration of `xcode` or `xclaw` as a baseline for product initialization options.

**Function prototype:**

```c
XWORK_API xwork_status xwork_profile_get_builtin(
    const char *sProfileId,
    xwork_profile *pProfile
);
```

**parameter:**

- `sProfileId`: input parameters. Must be a built-in profile id, such as `XWORK_PROFILE_XCODE` or `XWORK_PROFILE_XCLAW`.
- `pProfile`: Output parameter. Must not be `NULL`. The function will be initialized first and then written to the built-in profile.

**Return value:**

- `XWORK_OK`: Obtained successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid output pointer.
- `XWORK_ERROR_NOT_FOUND`: profile id is empty or not a built-in profile.

**Resource ownership:**

The returned profile structure is owned by the caller; the strings point to static built-in data that cannot be released by the caller.

**Additional Note:**

- Obtaining the profile will not modify the runtime or options, and you must continue to call the apply function.
- Built-in profiles are stable defaults, but product layers should still explicitly override security boundaries.

**Example code:**

```c
#include "xwork.h"

int load_xclaw_profile(void) {
    xwork_profile profile;
    return xwork_profile_get_builtin(XWORK_PROFILE_XCLAW, &profile) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_profile_init`
- `xwork_profile_apply_runtime_options`

---

### xwork_profile_apply_runtime_options

Write the profile's runtime-level policy into runtime options.

**Function:**

You can use this function to apply the policy default value in the profile to `xwork_runtime_options`.

**Function prototype:**

```c
XWORK_API xwork_status xwork_profile_apply_runtime_options(
    const xwork_profile *pProfile,
    xwork_runtime_options *pOptions
);
```

**parameter:**

- `pProfile`: input parameters. Must not be `NULL`.
- `pOptions`: input/output parameters. Must not be `NULL`. The function writes `tPolicy`.

**Return value:**

- `XWORK_OK`: applied successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: profile or options are empty.

**Resource ownership:**

The function does not allocate resources and does not take over ownership of other borrowed pointers within `pOptions`.

**Additional Note:**

- This function only overrides the runtime policy and does not modify the xllm runtime, host services or persistence backend.
- If the product requires a stricter policy, it should be overridden after calling this function.

**Example code:**

```c
#include "xwork.h"

int apply_runtime_profile(const xwork_profile *profile) {
    xwork_runtime_options options;
    xwork_runtime_options_init(&options);
    return xwork_profile_apply_runtime_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_runtime_options_init`
- `xwork_runtime_create`

---

### xwork_profile_apply_xllm_profile_options

Write the profile's default model profile id into xllm profile options.

**Function:**

You can use this function to make `xwork_xllm_profile_options` use the profile's default xllm profile id and display name by default.

**Function prototype:**

```c
XWORK_API xwork_status xwork_profile_apply_xllm_profile_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pOptions
);
```

**parameter:**

- `pProfile`: input parameters. Must not be `NULL`.
- `pOptions`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: applied successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: profile or options are empty.

**Resource ownership:**

The function does not copy strings. The profile id and display name written are borrowed pointers, usually from static built-in profiles.

**Additional Note:**

- If `pOptions->sProfileId` already has a non-null value, the function does not overwrite it.
- If `pOptions->sDisplayName` already has a non-null value, the function does not overwrite it.

**Example code:**

```c
#include "xwork.h"

int apply_llm_profile(const xwork_profile *profile) {
    xwork_xllm_profile_options options;
    xwork_xllm_profile_options_init(&options);
    return xwork_profile_apply_xllm_profile_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_xllm_profile_options_init`
- `xwork_profile_apply_xllm_bootstrap_options`

---

### xwork_profile_apply_xllm_bootstrap_options

Connect the xllm profile default value of profile to bootstrap options.

**Function:**

You can use this function to prepare `xwork_xllm_profile_options` and `xwork_xllm_bootstrap_options` at the same time, so that a default xllm runtime can be bootstrapped during runtime create.

**Function prototype:**

```c
XWORK_API xwork_status xwork_profile_apply_xllm_bootstrap_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pProfileOptions,
    xwork_xllm_bootstrap_options *pBootstrapOptions
);
```

**parameter:**

- `pProfile`: input parameters. Must not be `NULL`.
- `pProfileOptions`: input/output parameters. Must not be `NULL`. The function applies the default xllm profile fields.
- `pBootstrapOptions`: input/output parameters. Must not be `NULL`. If the profile list is not set, the function points to `pProfileOptions` and sets count to `1`.

**Return value:**

- `XWORK_OK`: applied successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: Either parameter is empty.

**Resource ownership:**

Function does not copy `pProfileOptions`. If it is written to `pBootstrapOptions->pProfiles`, the caller must ensure that the structure remains valid for the duration of `xwork_runtime_create`'s use.

**Additional Note:**

- If `pBootstrapOptions->pProfiles` is no longer `NULL`, the function does not overwrite it.
- If `iProfileCount` is `0`, the function is set to `1`.

**Example code:**

```c
#include "xwork.h"

int apply_bootstrap_profile(const xwork_profile *profile) {
    xwork_xllm_profile_options llm_profile;
    xwork_xllm_bootstrap_options bootstrap;
    xwork_xllm_profile_options_init(&llm_profile);
    xwork_xllm_bootstrap_options_init(&bootstrap);
    return xwork_profile_apply_xllm_bootstrap_options(
        profile,
        &llm_profile,
        &bootstrap
    ) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_xllm_bootstrap_options_init`
- `xwork_runtime_create`

---

### xwork_profile_apply_workspace_options

Write the profile's workspace default value into workspace options.

**Function:**

You can use this function to determine whether the workspace has memory enabled by default based on the profile.

**Function prototype:**

```c
XWORK_API xwork_status xwork_profile_apply_workspace_options(
    const xwork_profile *pProfile,
    xwork_workspace_options *pOptions
);
```

**parameter:**

- `pProfile`: input parameters. Must not be `NULL`.
- `pOptions`: input/output parameters. Must not be `NULL`. The function writes `bEnableMemory`.

**Return value:**

- `XWORK_OK`: applied successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: profile or options are empty.

**Resource ownership:**

The function does not allocate resources and does not set or take over `pOptions->pMemory`.

**Additional Note:**

- Enabling memory only sets the boolean switch; the caller must still provide a valid `xllm_memory *`.
- Products can continue to override include/exclude policies after they are called.

**Example code:**

```c
#include "xwork.h"

int apply_workspace_profile(const xwork_profile *profile) {
    xwork_workspace_options options;
    xwork_workspace_options_init(&options);
    return xwork_profile_apply_workspace_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_workspace_options_init`
- `xwork_runtime_add_workspace`

---

### xwork_profile_apply_run_options

Write the profile's run default value into run options.

**Function:**

You can use this function to tell run to use the profile's default model profile, session profile, autonomy, and session policy.

**Function prototype:**

```c
XWORK_API xwork_status xwork_profile_apply_run_options(
    const xwork_profile *pProfile,
    xwork_run_options *pOptions
);
```

**parameter:**

- `pProfile`: input parameters. Must not be `NULL`.
- `pOptions`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: applied successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: profile or options are empty.

**Resource ownership:**

The function does not copy strings. The `sLlmProfileId` and `sSessionProfileId` written are borrow pointers.

**Additional Note:**

- If run options already sets a non-empty profile id, the function will not override it.
- Function overrides `eAutonomy` and `tSessionPolicy`.

**Example code:**

```c
#include "xwork.h"

int apply_run_profile(const xwork_profile *profile) {
    xwork_run_options options;
    xwork_run_options_init(&options);
    return xwork_profile_apply_run_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_run_options_init`
- `xwork_run_create`

---

### xwork_profile_apply_orchestrator_options

Write the profile's orchestrator default values ​​into orchestrator options.

**Function:**

You can use this function to tell the orchestrator to use the profile's default max turns, planner mode, and auto approve settings.

**Function prototype:**

```c
XWORK_API xwork_status xwork_profile_apply_orchestrator_options(
    const xwork_profile *pProfile,
    xwork_orchestrator_options *pOptions
);
```

**parameter:**

- `pProfile`: input parameters. Must not be `NULL`.
- `pOptions`: input/output parameters. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: applied successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: profile or options are empty.

**Resource ownership:**

The function does not allocate resources and does not take over ownership of any external pointers in options.

**Additional Note:**

- `pOptions->iMaxTurns` is only overwritten if it is `iDefaultMaxTurns > 0`.
- Function sets `ePlannerMode` and `bAutoApprove`.

**Example code:**

```c
#include "xwork.h"

int apply_orchestrator_profile(const xwork_profile *profile) {
    xwork_orchestrator_options options;
    xwork_orchestrator_options_init(&options);
    return xwork_profile_apply_orchestrator_options(profile, &options) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_orchestrator_options_init`
- `xwork_run_execute`

## Error handling

- `XWORK_ERROR_INVALID_ARGUMENT`: profile or target options pointer is null.
- `XWORK_ERROR_NOT_FOUND`: Built-in profile id does not exist.

## Restore boundaries

Profile is configuration data and does not carry live status. When resuming a run, the caller should reapply the profile/options compatible with the original run, and then load the snapshot or persistence data.

## Thread boundaries

The profile apply function only writes the options passed in by the caller and does not access the global mutable state. Concurrency safety depends on whether callers modify the same options structure concurrently.

## Related documents

- [Runtime API](api-runtime.md)
- [Workspace API](api-workspace.md)
- [Run API](api-run.md)
- [AI IDE Agent Example](../case/ai-ide-agent.md)
- [claw Autonomous Agent Example](../case/claw-autonomous-agent.md)
