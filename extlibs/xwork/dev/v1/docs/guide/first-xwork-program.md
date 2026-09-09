# 第一个 xwork 程序

> 状态：中文初稿，待人工审阅。

本教程展示最小 `runtime -> workspace -> run` 调用链。它不接入真实模型，也不执行工具，目标是理解 xwork 的基础对象生命周期。

## 需要理解的对象

| 对象 | 作用 |
| --- | --- |
| `xwork_runtime` | xwork 的顶层对象，持有 workspace、tool registry、run 和可选 xllm runtime。 |
| `xwork_workspace` | 一个可操作工作区，通常对应一个项目根目录。 |
| `xwork_run` | 一次 Agent 任务运行，承载 instruction、状态、event、checkpoint 和 artifact。 |

## 最小调用顺序

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

## 示例代码

完整可编译文件位于 [`examples/first_xwork_program.c`](../../examples/first_xwork_program.c)。

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

## 编译检查

从仓库根目录执行：

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
```

编译并运行本教程对应示例：

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\first_xwork_program.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\first_xwork_program.exe -lws2_32 -liphlpapi
examples\first_xwork_program.exe
```

如果要运行仓库内示例，参考 [examples/README.md](../../examples/README.md)。

## 下一步

继续阅读 [xllm 编排与工具循环](xllm-orchestrator-intro.md)，了解 xwork 如何驱动模型回合、处理工具调用和审批暂停。
