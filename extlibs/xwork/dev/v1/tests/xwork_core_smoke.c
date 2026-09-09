#include <assert.h>
#include <stdio.h>
#include <string.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"
#include "../xwork.c"

int main(void)
{
    xwork_runtime *pRuntime = NULL;
    xwork_workspace *pWorkspace = NULL;
    xwork_run *pRun = NULL;
    xwork_runtime_options tRuntimeOptions;
    xwork_workspace_options tWorkspaceOptions;
    xwork_tool_def tToolDef;
    xwork_run_options tRunOptions;
    xwork_run_summary tSummary;
    const xwork_tool_def *pFoundTool;
    const char *asWorkspaceIds[1];

    assert(strcmp(xwork_version(), "0.1.0") == 0);
    assert(XWORK_VERSION_MAJOR == 0);
    assert(XWORK_VERSION_MINOR == 1);
    assert(XWORK_VERSION_PATCH == 0);

    xwork_runtime_options_init(&tRuntimeOptions);
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);
    assert(pRuntime != NULL);
    assert(xwork_runtime_get_workspace_count(pRuntime) == 0u);
    assert(xwork_runtime_get_tool_count(pRuntime) == 0u);
    assert(xwork_runtime_get_run_count(pRuntime) == 0u);

    xwork_workspace_options_init(&tWorkspaceOptions);
    tWorkspaceOptions.sWorkspaceId = "core";
    tWorkspaceOptions.sRootPath = ".";
    assert(xwork_runtime_add_workspace(pRuntime, &tWorkspaceOptions, &pWorkspace) == XWORK_OK);
    assert(pWorkspace != NULL);
    assert(strcmp(xwork_workspace_get_id(pWorkspace), "core") == 0);
    assert(strcmp(xwork_workspace_get_root_path(pWorkspace), ".") == 0);
    assert(xwork_runtime_get_workspace_count(pRuntime) == 1u);
    assert(xwork_runtime_find_workspace(pRuntime, "core") == pWorkspace);

    xwork_tool_def_init(&tToolDef);
    tToolDef.sToolId = "core.echo";
    tToolDef.sDisplayName = "Core Echo";
    tToolDef.sDescription = "Core smoke virtual tool.";
    tToolDef.eKind = XWORK_TOOL_VIRTUAL;
    tToolDef.eSideEffect = XWORK_SIDE_EFFECT_READ_ONLY;
    tToolDef.eApprovalMode = XWORK_APPROVAL_NEVER;
    assert(xwork_runtime_register_tool(pRuntime, &tToolDef) == XWORK_OK);
    assert(xwork_runtime_register_tool(pRuntime, &tToolDef) == XWORK_ERROR_ALREADY_EXISTS);
    assert(xwork_runtime_get_tool_count(pRuntime) == 1u);
    pFoundTool = xwork_runtime_find_tool(pRuntime, "core.echo");
    assert(pFoundTool != NULL);
    assert(strcmp(pFoundTool->sDisplayName, "Core Echo") == 0);

    asWorkspaceIds[0] = "core";
    xwork_run_options_init(&tRunOptions);
    tRunOptions.sRunId = "core-run";
    tRunOptions.sInstruction = "core smoke";
    tRunOptions.eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
    tRunOptions.psWorkspaceIds = asWorkspaceIds;
    tRunOptions.iWorkspaceCount = 1u;
    assert(xwork_run_create(pRuntime, &tRunOptions, &pRun) == XWORK_OK);
    assert(pRun != NULL);
    assert(xwork_runtime_get_run_count(pRuntime) == 1u);
    assert(xwork_run_get_state(pRun) == XWORK_RUN_CREATED);

    assert(xwork_run_start(pRun) == XWORK_OK);
    assert(xwork_run_get_state(pRun) == XWORK_RUN_RUNNING);
    assert(xwork_run_complete(pRun) == XWORK_OK);
    assert(xwork_run_get_state(pRun) == XWORK_RUN_COMPLETED);

    xwork_run_summary_init(&tSummary);
    assert(xwork_run_get_summary(pRun, &tSummary) == XWORK_OK);
    assert(strcmp(tSummary.sRunId, "core-run") == 0);
    assert(strcmp(tSummary.sInstruction, "core smoke") == 0);
    assert(tSummary.eState == XWORK_RUN_COMPLETED);
    assert(tSummary.iWorkspaceCount == 1u);
    xwork_run_summary_reset(&tSummary);

    xwork_run_destroy(pRun);
    xwork_runtime_destroy(pRuntime);
    puts("xwork core smoke passed");
    return 0;
}
