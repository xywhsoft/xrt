# The first xwork program

>Status: First draft in Chinese, awaiting manual review.

This tutorial shows a minimal `runtime -> workspace -> run` call chain. It does not connect to the real model and does not execute tools. The goal is to understand the basic object life cycle of xwork.

## Objects to be understood

| Object | Function |
| --- | --- |
| |
| `xwork_workspace` | An operational workspace, usually corresponding to a project root directory. |
| `xwork_run` | An Agent task runs, carrying instruction, status, event, checkpoint and artifact. |

## Minimum calling order

```text
xwork_runtime_options_init
xwork_runtime_create
xwork_workspace_options_init
xwork_runtime_add_workspace
xwork_run_options_init
xwork_run_create
xwork_run_start
xwork_run_complete
xwork_runtime_destroy
```

## Sample code

The complete compilable file is located at [`examples/first_xwork_program.c`](../../examples/first_xwork_program.c).

```c
#include "xwork.h"

int main(void)
{
    xwork_runtime_options tRuntime;
    xwork_workspace_options tWorkspace;
    xwork_run_options tRun;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace *pWorkspace = NULL;
    xwork_run *pRun = NULL;
    const char *psWorkspaces[] = { "main" };

    xwork_runtime_options_init(&tRuntime);
    xwork_workspace_options_init(&tWorkspace);
    xwork_run_options_init(&tRun);

    tWorkspace.sWorkspaceId = "main";
    tWorkspace.sRootPath = "D:/git/project";

    tRun.sRunId = "run-1";
    tRun.sInstruction = "Inspect the workspace and summarize the task.";
    tRun.psWorkspaceIds = psWorkspaces;
    tRun.iWorkspaceCount = 1;

    if (xwork_runtime_create(&tRuntime, &pRuntime) != XWORK_OK) {
        return 1;
    }

    if (xwork_runtime_add_workspace(pRuntime, &tWorkspace, &pWorkspace) != XWORK_OK) {
        xwork_runtime_destroy(pRuntime);
        return 2;
    }

    if (xwork_run_create(pRuntime, &tRun, &pRun) != XWORK_OK) {
        xwork_runtime_destroy(pRuntime);
        return 3;
    }

    if (xwork_run_start(pRun) != XWORK_OK) {
        xwork_runtime_destroy(pRuntime);
        return 4;
    }

    xwork_run_complete(pRun);
    xwork_runtime_destroy(pRuntime);
    return 0;
}
```

## Compilation check

Execute from the repository root directory:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
```

Compile and run the corresponding example for this tutorial:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\first_xwork_program.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\first_xwork_program.exe -lws2_32 -liphlpapi
examples\first_xwork_program.exe
```

If you want to run the in-warehouse example, refer to [examples/README.md](../../examples/README.md).

## Next step

Continue reading [xllm Orchestration and Tool Loop](xllm-orchestrator-intro.md) to learn how xwork drives model rounds, handles tool calls, and approval holds.
