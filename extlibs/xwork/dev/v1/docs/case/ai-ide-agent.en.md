# AI IDE Agent Example

> Corresponding source code: `examples/ai_ide_agent.c`

This example shows the AI ​​IDE integration baseline under the `xcode` profile.

## Problem solved

AI IDE needs to allow the model to read the project, propose modifications, wait for user approval, and then precipitate the results into viewable artifacts. This example passes the minimum closed loop.

## process

```text
create runtime/workspace/local host
apply xcode profile
read README.md
emit file-content artifact
mock model requests filesystem.apply_patch
policy requires approval
submit approval
resume tool loop
emit patch artifact and final report
```

## Key points

- `xcode` profile is semi-automatic by default, suitable for human review workflow in IDE.
- patch uses dry-run semantics, which is suitable for UI display and manual confirmation.
- File contents, patch results, and final reports all enter the artifact stream.
- Approval suspension is a resumable state, not a temporary callback.

## Key API

| API | Function |
| --- | --- |
| `xwork_profile_get_builtin(XWORK_PROFILE_XCODE)` | Get AI IDE default profile. |
| `xwork_local_host_configure_services()` | Enable local filesystem host service. |
| `xwork_runtime_register_builtin_tool()` | Registers `filesystem.read_text` and `filesystem.apply_patch`. |
| `xwork_runtime_invoke_host_service()` | Read `README.md`. |
| `xwork_run_emit_output_artifact()` | Log file contents artifact. |
| `xwork_run_emit_patch_artifact()` | Log dry-run patch artifact. |
| `xwork_run_execute()` | Execute mock model turn + tool loop. |
| `xwork_run_get_last_approval_request()` | Read approval request. |
| `xwork_run_submit_approval()` | Submit the approval results. |
| `xwork_run_resume()` | Restore an approved run. |

## Approval life cycle

```text
model requests filesystem.apply_patch
policy requires approval
run enters WAITING_APPROVAL
UI reads xwork_approval_request
user approves
xwork_run_submit_approval
xwork_run_resume
orchestrator executes tool and completes
```

## Artifact List

| Artifact | Description |
| --- | --- |
| file-content output | Reading results of `README.md`. |
| patch artifact | dry-run patch with apply result and file summary JSON. |
| final report | `xwork.report.v1` final report. |

## Suitable for expansion

- Connect to the real editor buffer host service.
- Display approval request to IDE UI.
- Display artifact summary as file changes, diagnostics and reporting panels.
