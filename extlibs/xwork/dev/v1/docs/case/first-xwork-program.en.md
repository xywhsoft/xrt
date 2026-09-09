# The first xwork program

>Status: First draft in Chinese, awaiting review.

This example corresponds to [`examples/first_xwork_program.c`](../../examples/first_xwork_program.c) and is used to verify whether the minimum runtime, workspace, and run life cycle are available.

## Problem solved

This program does not access the real model, does not register the host tool, and does not write to the persistence store. It just proves that the minimal shared object model works:

- Create `xwork_runtime`.
- Register a workspace.
- Create a run.
- Advance run to started/completed.
- Destroy the runtime and release the objects it owns.

## Build and run

See [`examples/README.md`](../../examples/README.md) for the build command. The minimum command is:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\first_xwork_program.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\first_xwork_program.exe -lws2_32 -liphlpapi
```

The exit code after running is `0`, indicating that the life cycle is advanced successfully.

## Execution process

```text
xwork_runtime_options_init
xwork_workspace_options_init
xwork_run_options_init
xwork_runtime_create
xwork_runtime_add_workspace
xwork_run_create
xwork_run_start
xwork_run_complete
xwork_runtime_destroy
```

## Key API

| API | Function |
| --- | --- |
| `xwork_runtime_create()` | Create runtime. |
| `xwork_runtime_add_workspace()` | Hang workspace to runtime. |
| `xwork_run_create()` | Create a run with instruction and workspace refs. |
| `xwork_run_start()` | Advance run to started. |
| `xwork_run_complete()` | Marks the run as complete. |
| `xwork_runtime_destroy()` | Releases the runtime and its owning workspace/run. |

## Extension direction

- Access `xwork_run_execute()` and let run enter the orchestrator tool loop.
- Add file persistence, verify run snapshot and event log.
- Register the host tool, and then access policy/approval.

## Related documents

- [First xwork Program Tutorial](../guide/first-xwork-program.md)
- [Runtime API](../api/api-runtime.md)
- [Workspace API](../api/api-workspace.md)
- [Run API](../api/api-run.md)
