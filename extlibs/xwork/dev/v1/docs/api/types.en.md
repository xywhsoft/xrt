# xwork basic type API

> xwork’s version constants, status codes and public resource ownership conventions. After you understand this page, it will be easier to look at the runtime, workspace, run, tool, and persistence APIs.

[Back to API Index](README.md) | [Runtime API](api-runtime.md) | [Run API](api-run.md)

---

## Table of contents

- [Module Positioning](#Module Positioning)
- [Version Constants](#Version Constants)
- [Status Codes](#Status Codes)
- [Ownership Rules](#Ownership Rules)
- [Version and Status Functions](#Version and Status Functions)
  - [xwork_version](#xwork_version)
  - [xwork_status_cstr](#xwork_status_cstr)
- [Common Usage](#Common Usage)
- [Common Mistakes](#Common Mistakes)
- [Related Examples](#Related Examples)

---

## Module positioning

This page describes the basic contract shared by all xwork modules:

- Version constants and format versions.
- `xwork_status` status code semantics.
- Resource ownership rules for `init` / `reset` / `destroy` / borrowed / copied / owned.
- Two basic functions for querying version and status code text.

This page does not expand the structure fields of modules such as runtime, workspace, tool, and run; these contents are explained function by function in the corresponding module pages.

---

## Version constant

| Constant | Current value | Description | When to use |
| --- | --- | --- | --- |
| `XWORK_VERSION_MAJOR` | `0` | Major version number. The `0.x` stage indicates that the API is still converging. | release gate, compatibility judgment, diagnostic output. |
| `XWORK_VERSION_MINOR` | `1` | Minor version number. New abilities usually increase this value. | release gate, capability matrix. |
| `XWORK_VERSION_PATCH` | `0` | Revision number. Repair changes usually increase this value. | Diagnostics, build records. |
| `XWORK_PERSISTENCE_FORMAT_VERSION` | `14` | file persistence format version. | Read/write persistence store, migrations and compatibility checks. |
| `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` | `1` | remote worker/control plane protocol version. | Worker registration, control plane message decoding, cross-version communication. |

**Additional Note:**

- Use `xwork_version()` when a human-readable version of the string is required.
- Use `XWORK_PERSISTENCE_FORMAT_VERSION` when you need to determine persistence compatibility, and do not parse the version string.
- Use `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` when you need to determine remote worker protocol compatibility.

---

## Status code

### `xwork_status`

All APIs that return `xwork_status` follow the status code semantics on top of `xwork.h`.

| Value | Description | Common processing methods |
| --- | --- | --- |
| `XWORK_OK` | Operation completed. | Continue reading output parameters or proceed to the next step. |
| `XWORK_ERROR_INVALID_ARGUMENT` | Invalid required pointer, ID, enumeration value, or option combination. | Correct caller input; the same input should generally not be retried. |
| `XWORK_ERROR_NO_MEMORY` | Memory allocation failed. | Release resources, demote or terminate the current task. |
| `XWORK_ERROR_ALREADY_EXISTS` | The object corresponding to the stable ID already exists. | Change ID, query existing objects, or skip repeated creation. |
| `XWORK_ERROR_NOT_FOUND` | The object or persistent record does not exist. | Check ID, store root, run id, artifact id. |
| `XWORK_ERROR_INVALID_STATE` | The object exists, but the lifecycle state does not allow the operation. | Check run/graph/plane status to avoid repeating start/execute/destroy. |
| `XWORK_ERROR_EXTERNAL_FAILURE` | xrt/xllm, provider, host service, file system, process, persistence, or callback failed. | View event, artifact, host result, or provider diagnostics. |
| `XWORK_ERROR_UNSUPPORTED` | The capability is valid but is not implemented, not enabled, or is not supported by the version. | Check profile, host tool, remote protocol, persistence format. |
| `XWORK_ERROR_CANCELLED` | Collaborative cancellation observed. | Treat a run or task as canceled, not failed. |
| `XWORK_ERROR_PAUSED` | Execution stopped at recoverable boundary. | Get the approval/pending tool status and continue after explicit resume. |

---

## General rules for resource ownership

| Rules | Meaning |
| --- | --- |
| |
| |
| `*_destroy()` | Destroy opaque owned objects, such as runtime, run, agent pool, task graph, control plane, replay engine. |
| copied | Deep copy input string, array or structure contents during API call. The caller can release the original input after the call returns. |
| borrowed | xwork only saves or temporarily uses the caller pointer, and the caller must ensure that the life cycle covers the document requirements. |
| owned | xwork returns a new object or deep copy result, and the caller must use the corresponding reset/destroy to release it. |

**Additional Note:**

- The `const char *` or `const xwork_tool_def *` returned by the getter is treated as a borrowed pointer by default.
- runtime destroy will release workspace, tool registry and run that are still hanging on the runtime.
- Host service, persistence backend, callback user data, external xllm runtime/memory/cancel token are usually borrowed.

---

## Version and status functions

### xwork_version

Get the xwork runtime version string.

**Function:**

You can record this version in the log, diagnostic report, release gate, artifact metadata, or host UI to facilitate subsequent problem location.

**Function prototype:**

```c
XWORK_API const char *xwork_version(void);
```

**parameter:**

none.

**Return value:**

- Returns a static string, for example the current implementation returns `"0.1.0"`.
- The caller cannot release the return value.

**Resource ownership:**

The return value is a borrowed pointer, and the life cycle is the process life cycle.

**Additional Note:**

- If you need the numeric version, use `XWORK_VERSION_MAJOR`, `XWORK_VERSION_MINOR`, `XWORK_VERSION_PATCH`.
- Version strings are intended for human reading, and it is not recommended to use string comparison to determine compatibility.
- Persistence format and remote protocol compatibility use `XWORK_PERSISTENCE_FORMAT_VERSION` and `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` respectively.

**Example code:**

```c
#include "xwork.h"
#include <stdio.h>

int main(void)
{
    printf("xwork version: %s\n", xwork_version());
    return 0;
}
```

**Related API:**

- `XWORK_VERSION_MAJOR`
- `XWORK_VERSION_MINOR`
- `XWORK_VERSION_PATCH`
- `XWORK_PERSISTENCE_FORMAT_VERSION`
- `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`

---

### xwork_status_cstr

Convert `xwork_status` to a stable string.

**Function:**

You can display status code names in logs, assertion failures, test output, diagnostic artifacts, or the host UI instead of just showing numbers.

**Function prototype:**

```c
XWORK_API const char *xwork_status_cstr(xwork_status eStatus);
```

**parameter:**

| Parameters | Direction | Whether it can be `NULL` | Description |
| --- | --- | --- | --- |
| `eStatus` | Input | No | The status code to convert. Can be any `xwork_status` enumeration value; unknown values ​​will return a cryptic string. |

**Return value:**

- Return the static string corresponding to the status code, such as `"XWORK_OK"`.
- Unknown value returned `"XWORK_STATUS_UNKNOWN"`.
- The caller cannot release the return value.

**Resource ownership:**

The return value is a borrowed pointer, and the life cycle is the process life cycle.

**Additional Note:**

- This function does not allocate resources and will not fail.
- The returned string is suitable for logging and diagnosis, and is not recommended as a basis for judging program compatibility.
- Program logic should directly compare the `xwork_status` enumeration value.

**Example code:**

```c
#include "xwork.h"
#include <stdio.h>

int main(void)
{
    xwork_status status = XWORK_ERROR_INVALID_ARGUMENT;
    printf("status: %s\n", xwork_status_cstr(status));
    return 0;
}
```

**Related API:**

- `xwork_status`
- `XWORK_OK`
- `XWORK_ERROR_INVALID_ARGUMENT`

---

## Shared Helper Function

### xwork_memory_context_init

Initialize memory context.

**Function:**

Prepare an `xwork_memory_context` for the agent/run shared context reference.

**Function prototype:**

```c
XWORK_API void xwork_memory_context_init(xwork_memory_context *pContext);
```

**parameter:**

- `pContext`: context to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

A memory context usually holds persistent references and does not directly own an external memory store.

**Example code:**

```c
xwork_memory_context context;
xwork_memory_context_init(&context);
```

**Related API:**

- `xwork_memory_context_reset`

---

### xwork_memory_context_reset

Release memory context.

**Function:**

Release memory context internal deep copy of strings and lists.

**Function prototype:**

```c
XWORK_API void xwork_memory_context_reset(xwork_memory_context *pContext);
```

**parameter:**

- `pContext`: context to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases internal resources but does not release the structure itself.

**Additional Note:**

After the call, the context can be reused or safely discarded.

**Example code:**

```c
xwork_memory_context_reset(&context);
```

**Related API:**

- `xwork_memory_context_init`

---

### xwork_session_policy_init

Initialize session policy.

**Function:**

Prepare the policy configuration used by run/session.

**Function prototype:**

```c
XWORK_API void xwork_session_policy_init(xwork_session_policy *pPolicy);
```

**parameter:**

- `pPolicy`: The policy to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

Session policy will enter the execution path with run options and is used to control approval, tools, and automation behaviors.

**Example code:**

```c
xwork_session_policy policy;
xwork_session_policy_init(&policy);
```

**Related API:**

- `xwork_run_create`

---

### xwork_string_list_init

Initialize a list of strings.

**Function:**

Prepare a generic `xwork_string_list`.

**Function prototype:**

```c
XWORK_API void xwork_string_list_init(xwork_string_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

Used for string collection fields in shared objects.

**Example code:**

```c
xwork_string_list list;
xwork_string_list_init(&list);
```

**Related API:**

- `xwork_string_list_reset`

---

### xwork_string_list_reset

Free the string list.

**Function:**

Frees each string and list array in the string list.

**Function prototype:**

```c
XWORK_API void xwork_string_list_reset(xwork_string_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Frees the deep-copy string owned by the list.

**Additional Note:**

The list returns to an empty state after the call.

**Example code:**

```c
xwork_string_list_reset(&list);
```

**Related API:**

- `xwork_string_list_init`

---

## Common usage

### Record version and persistence format

```c
printf(
    "xwork=%s persistence=%d remote=%d\n",
    xwork_version(),
    XWORK_PERSISTENCE_FORMAT_VERSION,
    XWORK_REMOTE_PROTOCOL_VERSION_CURRENT
);
```

### Output status code

```c
xwork_status status = xwork_runtime_create(&tOptions, &pRuntime);
if (status != XWORK_OK) {
    fprintf(stderr, "create runtime failed: %s\n", xwork_status_cstr(status));
}
```

## Common mistakes

| Problem | Cause | Solution |
| --- | --- | --- |
| Use `strcmp(xwork_version(), "...")` to determine compatibility | The version string is intended for human reading, not a compatibility protocol. | Use numeric macros or module-specific version constants. |
| Release `xwork_version()` Return value | The return value is a static borrowed pointer. | Do not release. |
| Treat `XWORK_ERROR_PAUSED` as a failure | paused is the recoverable boundary. | Query the approval/pending tool status and resume explicitly. |
| Treat `XWORK_ERROR_CANCELLED` as an external failure | canceled means cooperative cancellation. | Mark run/task as canceled. |

## Related examples

- `examples\first_xwork_program.c`
- `tests\xwork_core_smoke.c`
