# claw Autonomous Agent Example

> Corresponding source code: `examples/claw_autonomous_agent.c`

This example shows the autonomous Agent baseline under the `xclaw` profile.

## Problem solved

Claw-like products require greater autonomy and can automatically perform low-risk or authorized actions while retaining artifacts, snapshots, and recovery capabilities.

## process

```text
create runtime/workspace/local host/file persistence
apply xclaw profile
execute process.exec
emit command artifact
emit final report
persist run snapshot
recover run from file persistence
```

## Key points

- `process.exec` is based on xrt subprocess path and supports stdout/stderr, timeout, stdin, env and event output.
- The command results generate command artifacts and necessary diagnostics reports.
- running snapshot restores lifecycle state, but does not restore the live process handle.
- The `xclaw` profile can enable higher automation policies, but the network should still deny-by-default.

## process.exec request/response

Request example:

```json
{"command":"cmd /c echo xwork-claw-example","timeout_ms":10000,"include_events":true}
```

A successful response contains:

```json
{"ok":true,"stdout":"xwork-claw-example\r\n","stderr":"","exit_code":0,"timed_out":false,"cancelled":false}
```

The actual output text varies between platform shells and newlines, and callers should rely on structured fields rather than hardcoding the full stdout.

## Key API

| API | Function |
| --- | --- |
| `xwork_profile_get_builtin(XWORK_PROFILE_XCLAW)` | Get the autonomous Agent default profile. |
| `xwork_file_persistence_configure_backend()` | Configure `.xwork_claw_store`. |
| `xwork_local_host_configure_services()` | Enable local process host service. |
| `xwork_runtime_register_builtin_tool()` | Register `process.exec`. |
| `xwork_runtime_invoke_host_service()` | Execute the command. |
| `xwork_run_emit_command_artifact()` | Save command artifact. |
| `xwork_run_emit_report_artifact()` | Save final report. |
| `xwork_run_get_snapshot()` | Get run snapshot. |
| `xwork_runtime_recover_run_from_persistence()` | Resume run from file persistence. |

## Restore boundaries

Can restore run status, summary, event, artifact and latest snapshot. A live process handle that has ended or is still running externally cannot be resumed. If the product needs to continue command execution after recovery, it should reschedule a new host tool call.

## Suitable for expansion

- Connect to real model provider.
- Add stricter command allow/deny policy.
- Convert command artifacts into task reports or diagnostic lists.
