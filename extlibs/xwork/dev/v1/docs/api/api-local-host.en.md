# Local Host API

>Status: First draft in Chinese, awaiting review.

Local Host API is a local host service helper provided by xwork, which is used to link filesystem, process, terminal, vcs and editor-buffer capabilities to `xwork_host_services` of runtime.

## Related Statements

- `xwork_local_host_options`
- `xwork_local_host`
- `xwork_local_host_options_init()`
- `xwork_local_host_init()`
- `xwork_local_host_configure_services()`
- `xwork_local_host_reset()`
- `xwork_host_services`
- `xwork_host_services_init()`

## Module positioning

local host is a local in-process helper. It is suitable for examples, smoke, AI IDE/claw local execution baseline. Production products can still implement their own host services as long as they adhere to the same JSON contract, policy, and artifact semantics.

## Configuration fields

| Field | Description |
| --- | --- |
| `sDefaultWorkingDirectory` | Default working directory. |
| `bEnforceFilesystemRoot` | Whether to force path to stay under root. |
| `psFilesystemAllowPathPrefixes` | File path allow prefix. |
| `psFilesystemDenyPathPrefixes` | File path deny prefix. |
| `psCommandAllowPatterns` | Command allow pattern. |
| `psCommandDenyPatterns` | Command deny pattern. |
| `bDenyDestructiveCommands` | Reject destructive commands before spawning. |
| `iMaxReadBytes` | File reading limit. |
| `iMaxProcessInputBytes` | stdin_text upper limit. |
| `iMaxProcessEnvEntries` | env entry limit. |
| `iMaxProcessOutputBytes` | process output upper limit. |

Capability switches include filesystem read/write, process exec, VCS status/diff/log/branch and editor buffers.

## Minimal configuration

```c
xwork_local_host_options tOptions;
xwork_local_host tHost;
xwork_host_services tServices;

xwork_local_host_options_init(&tOptions);
xwork_local_host_init(&tHost);
xwork_host_services_init(&tServices);

tOptions.sDefaultWorkingDirectory = "D:/git/project";
tOptions.bEnforceFilesystemRoot = true;
tOptions.bEnableFilesystemReadText = true;
tOptions.bEnableFilesystemWriteText = true;
tOptions.bEnableProcessExec = true;
tOptions.bEnableVcsStatus = true;

status = xwork_local_host_configure_services(&tHost, &tOptions, &tServices);
```

`xwork_runtime_options::pHostServices` will copy `tServices` by value, but `tHost` is the actual owner of the callback user data and must live until the runtime no longer calls the host service.

## Function-by-function description

### xwork_host_service_init

Initialize a single host service.

**Function:**

Clear `xwork_host_service` and prepare to populate operation callback, user data and capability description.

**Function prototype:**

```c
XWORK_API void xwork_host_service_init(xwork_host_service *pService);
```

**parameter:**

- `pService`: The service to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; both callback and user data are managed by the caller.

**Additional Note:**

The production host can directly fill in its own `xwork_host_service` without local host.

**Example code:**

```c
xwork_host_service service;
xwork_host_service_init(&service);
```

**Related API:**

- `xwork_host_services_init`

---

### xwork_host_services_init

Initialize the host services collection.

**Function:**

Initialize all host service slots in `xwork_host_services` to empty.

**Function prototype:**

```c
XWORK_API void xwork_host_services_init(xwork_host_services *pServices);
```

**parameter:**

- `pServices`: services to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the services structure is owned by the caller.

**Additional Note:**

After passing to runtime options, runtime copies services by value; the life cycle of callback user data is still guaranteed by the caller.

**Example code:**

```c
xwork_host_services services;
xwork_host_services_init(&services);
```

**Related API:**

- `xwork_local_host_configure_services`

---

### xwork_local_host_options_init

Initialize local host options.

**Function:**

Set the default security and capabilities configuration of the local host service helper.

**Function prototype:**

```c
XWORK_API void xwork_local_host_options_init(xwork_local_host_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The filesystem, process, terminal, vcs or editor-buffer capabilities that need to be exposed should be explicitly enabled before calling `xwork_local_host_configure_services`.

**Example code:**

```c
xwork_local_host_options opts;
xwork_local_host_options_init(&opts);
opts.bEnableProcessExec = true;
```

**Related API:**

- `xwork_local_host_configure_services`

---

### xwork_local_host_init

Initialize local host helper.

**Function:**

Prepare an `xwork_local_host` held by the caller to carry host service callback user data.

**Function prototype:**

```c
XWORK_API void xwork_local_host_init(xwork_local_host *pHost);
```

**parameter:**

- `pHost`: The host to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No long-term resources are allocated; the host structure is owned by the caller.

**Additional Note:**

After being configured to the runtime, `pHost` must survive until the runtime no longer calls the host service.

**Example code:**

```c
xwork_local_host host;
xwork_local_host_init(&host);
```

**Related API:**

- `xwork_local_host_reset`

---

### xwork_local_host_reset

Release local host helper.

**Function:**

Release the path, allow/deny list, terminal/session status and other resources copied within the local host.

**Function prototype:**

```c
XWORK_API void xwork_local_host_reset(xwork_local_host *pHost);
```

**parameter:**

- `pHost`: The host to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases internal resources but does not release the structure itself.

**Additional Note:**

Do not reset when the runtime may still call the host service.

**Example code:**

```c
xwork_local_host_reset(&host);
```

**Related API:**

- `xwork_local_host_init`

---

### xwork_local_host_configure_services

Configure local host services.

**Function:**

Initialize the host helper according to local host options and hook the filesystem/process/terminal/vcs/editor-buffer callback to `xwork_host_services`.

**Function prototype:**

```c
XWORK_API xwork_status xwork_local_host_configure_services(
    xwork_local_host *pHost,
    const xwork_local_host_options *pOptions,
    xwork_host_services *pServices
);
```

**parameter:**

- `pHost`: local host helper owned by the caller.
- `pOptions`: Configuration parameters.
- `pServices`: Output services; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

`pHost` saves a copy of the configuration as callback user data; `pServices` does not own `pHost`.

**Additional Note:**

local `process.exec` go xrt subprocess. The terminal session is a live local resource and will not be automatically restored by snapshot.

**Example code:**

```c
xwork_local_host_configure_services(&host, &opts, &services);
```

**Related API:**

- `xwork_runtime_create`

---

### xwork_host_invoke_context_should_cancel

Check if the host service call should be canceled.

**Function:**

Have long-running host services check for cancel token or runtime interruption status at stage boundaries.

**Function prototype:**

```c
XWORK_API bool xwork_host_invoke_context_should_cancel(
    const xwork_host_invoke_context *pContext,
    const char *sPhase
);
```

**parameter:**

- `pContext`: host invoke context; can be `NULL`.
- `sPhase`: Optional stage name, used for diagnostics.

**Return value:**

Returns `true` if cancellation is expected; otherwise returns `false`.

**Resource ownership:**

Do not allocate resources; do not take over `sPhase`.

**Additional Note:**

The host service should be actively checked before spawning, during I/O loops, waiting for child processes, and uploading chunks.

**Example code:**

```c
if (xwork_host_invoke_context_should_cancel(ctx, "before-spawn")) {
    return XWORK_ERROR_CANCELLED;
}
```

**Related API:**

- `xwork_tool_exec_context_should_cancel`

---

## process.exec

local `process.exec` takes the xrt subprocess path and does not depend on shell `popen`. It supports:

- cwd override
- stdin_text
- env list
- timeout_ms
- timeout_stop：interrupt / terminate / kill / kill_tree
- allow_nonzero_exit
- merge_stderr
- include_events
- max_output_bytes
- terminal mode

Results include stdout/stderr, exit code, truncation flags, I/O statistics, timeout/cancel/stop reason, and optional ordered events.

## interactive terminal

local host supports:

- `process.start_terminal`
- `process.list_terminals`
- `process.terminal_read`
- `process.terminal_write`
- `process.terminal_resize`
- `process.terminal_stop`

The terminal session is a live local resource and will not be automatically restored across process restarts. The persistence artifact only saves transcript/state/inventory records.

## Security recommendations

- `bEnforceFilesystemRoot` is enabled by default.
- Use dry-run or approve for write, delete, move, apply_patch.
- Configure allow/deny pattern on process.exec.
- Turn on `bDenyDestructiveCommands` for destructive command.
- Network capabilities are deny-by-default by default.

## Related documents

- [Host Tools API](api-host-tools.md)
- [Policy / Approval API](api-policy-approval.md)
- [claw Autonomous Agent Example](../case/claw-autonomous-agent.md)
- [Internal host tool contract](../../dev/docs/HOST_TOOL_CONTRACTS.md)
