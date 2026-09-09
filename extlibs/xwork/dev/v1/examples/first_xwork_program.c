#include "../xwork.h"

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
    tWorkspace.sRootPath = ".";

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
