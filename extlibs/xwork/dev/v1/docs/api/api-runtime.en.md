# Runtime API

> Status: Chinese function-by-function reference, waiting for manual review.

`xwork_runtime` is the top-level object of xwork and is responsible for holding the workspace, tool registry, run, policy, host services, persistence backend, optional xllm runtime and replay engine.

## Module positioning

Runtime is responsible for organizing the internal shared state of xwork. It is not responsible for the UI, web service lifecycle, concrete model provider protocols, or product-level planner strategies. See [xllm Integration API](api-xllm-integration.md) for xllm-related getter details, [Tool API](api-tools.md) for tool/host service call details, and the persistence facade will be included in the persistence document later.

## This page covers the statement

| Category | Statement |
| --- | --- |
| Opaque objects | `xwork_runtime` |
| Structure | `xwork_runtime_options` |
| Function | `xwork_runtime_options_init`, `xwork_runtime_create`, `xwork_runtime_destroy`,

## xwork_runtime_options field

| Field | Description |
| --- | --- |
| `pLlmRuntime` | Borrowed `xllm_runtime *`. Cannot be set at the same time as `pLlmBootstrap`. |
| `pLlmBootstrap` | xllm bootstrap configuration. Read when create; if the creation is successful, xwork runtime has the generated `xllm_runtime`. |
| `pReplayEngine` | Borrowed replay engine. Must remain valid during host service record/replay. |
| `pHostServices` | host service table. The structure is copied by value, and callback is borrowed from user data. |
| `pPersistenceBackend` | persistence backend table. The structure is copied by value, and callback is borrowed from user data. |
| `tPolicy` | runtime default policy. |
| `pUserData` | Runtime-level user data pointer, xwork does not interpret and does not release it. |

## Core Ownership

- `xwork_runtime_create` returns owned runtime, use `xwork_runtime_destroy` to release.
- runtime destroy will release workspace, tool and run that are still attached.
- When using `pLlmRuntime`, xwork only borrows the existing xllm runtime.
- When using `pLlmBootstrap`, xwork creates and owns the xllm runtime.
- The structures of `pHostServices` / `pPersistenceBackend` are copied by value, but the callback function and `pUserData` are borrowed.
- `pReplayEngine` is borrowed and not destroyed by the runtime.

## Common calling sequence

```text
xwork_runtime_options_init
Fill host/persistence/xllm/policy options
xwork_runtime_create
xwork_runtime_add_workspace
xwork_runtime_register_builtin_tool
xwork_run_create
xwork_runtime_destroy
```

---

### xwork_runtime_options_init

Initialize runtime options.

**Function:**

You can call this function before creating the runtime to set the xllm, host service, persistence, policy, and user data fields to stable default values.

**Function prototype:**

```c
XWORK_API void xwork_runtime_options_init(xwork_runtime_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; `NULL` does nothing. When not `NULL`, the nested policy is cleared and initialized.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All pointer fields are initialized to `NULL`.

**Additional Note:**

- `pLlmRuntime` and `pLlmBootstrap` are optional and cannot be set at the same time.
- When calling the profile apply function, it is recommended to init first, then apply profile, and then do product layer coverage.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime_options options;
    xwork_runtime_options_init(&options);
    return 0;
}
```

**Related API:**

- `xwork_runtime_create`
- `xwork_profile_apply_runtime_options`

---

### xwork_runtime_create

Create runtime.

**Function:**

You can use this function to create the xwork top-level object and connect the xllm runtime, host services, persistence backend, replay engine and default policy to subsequent agent runs.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_create(
    const xwork_runtime_options *pOptions,
    xwork_runtime **ppRuntime
);
```

**parameter:**

- `pOptions`: input parameters. Can be `NULL`; `NULL` uses the default configuration.
- `ppRuntime`: Output parameter. Must not be `NULL`. Receives owned runtime on success; persists as `NULL` on failure.

**Return value:**

- `XWORK_OK`: Created successfully.
- `XWORK_ERROR_INVALID_ARGUMENT`: The output pointer is empty, or both `pLlmRuntime` and `pLlmBootstrap` are set.
- `XWORK_ERROR_NO_MEMORY`: runtime allocation failed.
- Others `xwork_status`: The status returned by transparently transmitting the bootstrap path when xllm bootstrap fails.

**Resource ownership:**

- Upon success `*ppRuntime` is owned by the caller and must be released with `xwork_runtime_destroy`.
- If xllm runtime is created via `pLlmBootstrap`, xwork runtime owns it.
- If `pLlmRuntime` is passed in, the xwork runtime only borrows it.
- The host services and persistence backend structures are copied by value, but their internal callback/user data are borrowed.

**Additional Note:**

- Creating a runtime does not automatically register workspace, tool or run.
- Creating a runtime does not automatically restore state from persistence.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime_options options;
    xwork_runtime *runtime = NULL;

    xwork_runtime_options_init(&options);
    if (xwork_runtime_create(&options, &runtime) != XWORK_OK) {
        return 1;
    }

    xwork_runtime_destroy(runtime);
    return 0;
}
```

**Related API:**

- `xwork_runtime_options_init`
- `xwork_runtime_destroy`
- `xwork_runtime_add_workspace`

---

### xwork_runtime_destroy

Destroy the runtime.

**Function:**

You can use this function to release the runtime and the run, workspace, and tool records still attached to the runtime.

**Function prototype:**

```c
XWORK_API void xwork_runtime_destroy(xwork_runtime *pRuntime);
```

**parameter:**

- `pRuntime`: input/destroy parameters. Can be `NULL`; `NULL` does nothing.

**Return value:**

none.

**Resource ownership:**

- The function releases the runtime itself.
- The function releases the run, workspace, and tool records that are still attached.
- If the runtime owns the xllm runtime created by bootstrap, the function destroys it.
- The function does not destroy borrowed `pLlmRuntime`, `pReplayEngine`, host service user data, or persistence backend user data.

**Additional Note:**

- Do not destroy the runtime while the asynchronous run is still executing unless the product layer has completed cancellation and wait.
- After destruction, all borrowed pointers obtained through runtime are invalid.

**Example code:**

```c
#include "xwork.h"

void close_runtime(xwork_runtime *runtime) {
    xwork_runtime_destroy(runtime);
}
```

**Related API:**

- `xwork_runtime_create`
- `xwork_run_async_cancel`
- `xwork_run_async_wait`

---

### xwork_runtime_get_host_services

Get the runtime current host service table.

**Function:**

You can use this function to check the host service callback of the runtime configuration for diagnostics, bridging, or advanced hosting integration.

**Function prototype:**

```c
XWORK_API const xwork_host_services *xwork_runtime_get_host_services(const xwork_runtime *pRuntime);
```

**parameter:**

- `pRuntime`: input parameters. Can be `NULL`; returns `NULL` if `NULL`.

**Return value:**

- Returns the host service table borrow pointer.
- If `pRuntime` is `NULL`, return `NULL`.

**Resource ownership:**

The return value is owned by the runtime and cannot be released by the caller or continued to be used after the runtime is destroyed.

**Additional Note:**

- What is returned is the host service table copied internally by the runtime.
- The life cycle of callback and user data is still guaranteed by the caller.

**Example code:**

```c
#include "xwork.h"

int has_host_services(const xwork_runtime *runtime) {
    return xwork_runtime_get_host_services(runtime) != NULL ? 1 : 0;
}
```

**Related API:**

- `xwork_host_services_init`
- `xwork_runtime_invoke_host_service`

---

### xwork_runtime_get_persistence_backend

Get the current persistence backend of the runtime.

**Function:**

You can use this function to check whether the runtime is configured with a persistence backend, or to display the current persistence capability boundaries in the diagnostic UI.

**Function prototype:**

```c
XWORK_API const xwork_persistence_backend *xwork_runtime_get_persistence_backend(
    const xwork_runtime *pRuntime
);
```

**parameter:**

- `pRuntime`: input parameters. Can be `NULL`; returns `NULL` if `NULL`.

**Return value:**

- Returns the persistence backend borrow pointer.
- If `pRuntime` is `NULL`, return `NULL`.

**Resource ownership:**

The return value is owned by the runtime and cannot be released by the caller. The callback/user data inside the backend is still managed by the caller.

**Additional Note:**

- The presence of a return pointer does not mean that all persistence callbacks have been configured.
- Specific storage/loading capabilities should be determined according to the backend contract.

**Example code:**

```c
#include "xwork.h"

int has_persistence_backend(const xwork_runtime *runtime) {
    return xwork_runtime_get_persistence_backend(runtime) != NULL ? 1 : 0;
}
```

**Related API:**

- `xwork_persistence_backend_init`
- `xwork_file_persistence_configure_backend`

---

### xwork_runtime_get_policy_options

Copy the runtime's current policy options.

**Function:**

You can use this function to read the runtime default policy for UI display, diagnostic reports, or dynamic decision-making.

**Function prototype:**

```c
XWORK_API xwork_status xwork_runtime_get_policy_options(
    const xwork_runtime *pRuntime,
    xwork_policy_options *pOptions
);
```

**parameter:**

- `pRuntime`: input parameters. Must not be `NULL`.
- `pOptions`: Output parameter. Must not be `NULL`. Receives a by-value copy of policy options on success.

**Return value:**

- `XWORK_OK`: Copy successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: runtime or output pointer is null.

**Resource ownership:**

The function copies policy options by value and does not allocate heap resources. If the policy contains a borrowed string, the copy still maintains borrowed semantics.

**Additional Note:**

- Modifying the returned copy will not modify the runtime internal policy.
- If you need to change the runtime policy, the current public API does not provide runtime mutations and should be configured before creating the runtime.

**Example code:**

```c
#include "xwork.h"

int read_policy(const xwork_runtime *runtime) {
    xwork_policy_options policy;
    return xwork_runtime_get_policy_options(runtime, &policy) == XWORK_OK ? 0 : 1;
}
```

**Related API:**

- `xwork_policy_options_init`
- `xwork_profile_apply_runtime_options`

---

### xwork_runtime_get_workspace_count

Get the number of registered workspaces in the runtime.

**Function:**

You can use this function to display the number of workspaces currently managed by the runtime in diagnostics, testing, or UI.

**Function prototype:**

```c
XWORK_API size_t xwork_runtime_get_workspace_count(const xwork_runtime *pRuntime);
```

**parameter:**

- `pRuntime`: input parameters. Can be `NULL`; returns `0` if `NULL`.

**Return value:**

Returns the number of workspaces. Returns `0` if the parameter is invalid.

**Resource ownership:**

This function does not return a pointer and does not transfer ownership.

**Additional Note:**

- The statistics are based on the workspace currently attached to the runtime linked list.
- When registering/destroying workspaces concurrently, the caller should serialize itself.

**Example code:**

```c
#include "xwork.h"

size_t workspace_count(const xwork_runtime *runtime) {
    return xwork_runtime_get_workspace_count(runtime);
}
```

**Related API:**

- `xwork_runtime_add_workspace`
- `xwork_runtime_find_workspace`

---

### xwork_runtime_get_tool_count

Get the number of registered tools in the runtime.

**Function:**

You can use this function to check whether the current runtime has completed tool registration, or to assert the number of built-in tools in a test.

**Function prototype:**

```c
XWORK_API size_t xwork_runtime_get_tool_count(const xwork_runtime *pRuntime);
```

**parameter:**

- `pRuntime`: input parameters. Can be `NULL`; returns `0` if `NULL`.

**Return value:**

Returns the number of registered tools. Returns `0` if the parameter is invalid.

**Resource ownership:**

This function does not return a pointer and does not transfer ownership.

**Additional Note:**

- The statistics are based on the records in the current tool registry of the runtime.
- When registering tools concurrently, the caller should serialize itself.

**Example code:**

```c
#include "xwork.h"

size_t tool_count(const xwork_runtime *runtime) {
    return xwork_runtime_get_tool_count(runtime);
}
```

**Related API:**

- `xwork_runtime_register_tool`
- `xwork_runtime_register_builtin_tool`

---

### xwork_runtime_get_run_count

Gets the number of runs still attached to the runtime.

**Function:**

You can use this function in diagnostic, testing, or resource management logic to observe the number of runs currently held by the runtime.

**Function prototype:**

```c
XWORK_API size_t xwork_runtime_get_run_count(const xwork_runtime *pRuntime);
```

**parameter:**

- `pRuntime`: input parameters. Can be `NULL`; returns `0` if `NULL`.

**Return value:**

Returns the current run number of runtime. Returns `0` if the parameter is invalid.

**Resource ownership:**

This function does not return a pointer and does not transfer ownership.

**Additional Note:**

- The statistics are for live runs that are still attached to the runtime, not historical runs in persistence.
- If you need to query the persistence run, please use the persistence API.

**Example code:**

```c
#include "xwork.h"

size_t live_run_count(const xwork_runtime *runtime) {
    return xwork_runtime_get_run_count(runtime);
}
```

**Related API:**

- `xwork_run_create`
- `xwork_run_destroy`

## Error handling

- `XWORK_ERROR_INVALID_ARGUMENT`: The output pointer is empty, the runtime is empty, or the mutually exclusive options are set at the same time.
- `XWORK_ERROR_NO_MEMORY`: runtime allocation failed.
- `XWORK_ERROR_EXTERNAL_FAILURE`: May be returned by lower layer when bootstrap or external dependency fails.

## Restore boundaries

The runtime itself does not automatically restore full state from disk. The caller needs to configure the persistence backend and call the recovery API explicitly. Host services, xllm runtime, replay engine and callback user data need to be re-provided by the caller.

## Thread boundaries

The runtime is not a universal concurrent container. When registering a workspace, tool, run, or destroying a runtime, it should be serialized by the caller. Asynchronous run only synchronizes the async handle's own state, which does not mean that any mutation in the runtime is thread-safe.

## Related documents

- [Workspace API](api-workspace.md)
- [Tool API](api-tools.md)
- [Run API](api-run.md)
- [xllm Integration API](api-xllm-integration.md)
