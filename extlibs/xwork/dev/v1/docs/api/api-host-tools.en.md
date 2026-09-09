# Host Tools API

>Status: First draft in Chinese, awaiting review.

Host Tools API describes the boundary between xwork built-in tools and local host services. What the model sees is the tool id. When xwork is executed, it calls the file, process, terminal, VCS or editor capabilities provided by the host through the host service.

## Related Statements

- `xwork_host_service_kind`
- `xwork_host_service`
- `xwork_host_services`
- `xwork_host_invoke_context`
- `xwork_local_host_options`
- `xwork_local_host`
- `xwork_host_service_init()`
- `xwork_host_services_init()`
- `xwork_local_host_options_init()`
- `xwork_local_host_init()`
- `xwork_local_host_configure_services()`
- `xwork_local_host_reset()`
- `xwork_runtime_invoke_host_service()`
- `xwork_runtime_invoke_host_service_ex()`

## Module positioning

Host service is the side effect boundary between xwork and the host system. xwork defines contracts, policy gates, events, and artifacts; the host is responsible for actually accessing the file system, starting processes, managing terminals, and connecting to remote systems or editors.

## Host service kind

| Kind | Description |
| --- | --- |
| `XWORK_HOST_FILESYSTEM` | File reading and writing, list, stat, glob, mkdir, move, delete, apply_patch. |
| `XWORK_HOST_PROCESS` | `process.exec` and an interactive terminal session. |
| `XWORK_HOST_VCS` | status, diff, log, branch. |
| `XWORK_HOST_NETWORK` | External network access boundary. |
| `XWORK_HOST_DIAGNOSTICS` | Generate diagnostics from sources such as process output. |
| `XWORK_HOST_EDITOR` | Editor buffer bridge. |

## local host helper

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
tOptions.bDenyDestructiveCommands = true;

status = xwork_local_host_configure_services(&tHost, &tOptions, &tServices);
```

After putting `tServices` into `xwork_runtime_options::pHostServices`, the runtime will copy the service table by value; `tHost` itself must survive until the runtime no longer calls these services.

## General JSON rules

- The path defaults to workspace-relative unless the host configuration specifies otherwise.
- After turning on `bEnforceFilesystemRoot`, the filesystem/editor path must remain under `sDefaultWorkingDirectory`.
- Failure response contains `ok:false`, `error_kind`, and optionally `error`.
- Bounded output may return `truncated:true` or paging metadata.
- When the runtime has `pReplayEngine`, the host service can enter the record/strict/audit replay path.
- The orchestrator can automatically synthesize artifacts from successful built-in tool results.

## Filesystem Tools

Built-in filesystem tools:

- `filesystem.read_text`
- `filesystem.write_text`
- `filesystem.list`
- `filesystem.stat`
- `filesystem.glob`
- `filesystem.mkdir`
- `filesystem.move`
- `filesystem.delete`
- `filesystem.apply_patch`

The v1 built-in implementation of `filesystem.apply_patch` is a single-file exact text replacement, using `old_text` / `new_text` / `dry_run`.

## Process and terminal

`process.exec` supports:

- `command`
- `cwd`
- `stdin_text`
- `timeout_ms`
- `timeout_stop`
- `env`
- `max_output_bytes`
- `merge_stderr`
- `include_events`
- `use_terminal`
- `terminal_cols` / `terminal_rows`

Results include stdout/stderr, exit code, truncation flag, I/O byte count, timeout/cancel/stop reason, and can return xrt subprocess events.

Interactive terminal session tools:

- `process.start_terminal`
- `process.terminal_read`
- `process.terminal_write`
- `process.terminal_resize`
- `process.terminal_stop`
- `process.list_terminals`

Use `xwork.terminal_state.v1` for terminal state and `xwork.terminal_inventory.v1` for inventory.

## VCS and editor

The VCS tool currently targets read-only queries:

- `vcs.status`
- `vcs.diff`
- `vcs.log`
- `vcs.branch`

The editor tool is used in the product-side buffer bridge:

- `editor.open_buffer`
- `editor.apply_edit`

## Security Boundary

- filesystem root enforcement should not be bypassed by the product layer.
- process allow/deny patterns and destructive command interception should be performed before spawning.
- network default recommendation deny-by-default.
- Remote worker execution also needs to go through policy, workspace root and capability allowlist.

## Related documents

- [Tool API](api-tools.md)
- [Policy / Approval API](api-policy-approval.md)
- [Tools, Approval, and Artifacts](../guide/tool-approval-artifact-intro.md)
- [Internal host tool contract](../../dev/docs/HOST_TOOL_CONTRACTS.md)
- [Internal host tool examples](../../dev/docs/HOST_TOOL_EXAMPLES.md)
