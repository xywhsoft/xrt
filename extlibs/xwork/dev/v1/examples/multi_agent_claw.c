#include "../xwork.h"

#include <stdio.h>
#include <string.h>

static int xwork_example_check(xwork_status iStatus, const char *sStep)
{
    if ( iStatus == XWORK_OK ) {
        return 0;
    }
    fprintf(stderr, "%s failed: %d\n", sStep, (int)iStatus);
    return 1;
}

static xwork_status multi_agent_claw_execute(
    xwork_run *pRun,
    const xwork_task_node_summary *pNode,
    void *pUserData
)
{
    xwork_report_artifact_options tReportOptions;
    char sName[96];
    char sStorageRef[128];
    char sReportJson[512];

    (void)pUserData;
    if ( !pRun || !pNode || !pNode->sTaskId || !pNode->sAgentId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    snprintf(sName, sizeof(sName), "%s-report.json", pNode->sTaskId);
    snprintf(sStorageRef, sizeof(sStorageRef), "report://multi-agent-claw/%s", pNode->sTaskId);
    snprintf(
        sReportJson,
        sizeof(sReportJson),
        "{\"schema\":\"%s\",\"report_kind\":\"task\",\"status\":\"completed\","
        "\"subject_ref\":\"task://%s\",\"title\":\"%s completed\","
        "\"summary\":\"Agent %s completed task %s.\","
        "\"body_markdown\":\"# Task result\\n\\n- agent: %s\\n- task: %s\\n\"}",
        XWORK_REPORT_SCHEMA_V1,
        pNode->sTaskId,
        pNode->sTaskId,
        pNode->sAgentId,
        pNode->sTaskId,
        pNode->sAgentId,
        pNode->sTaskId
    );

    xwork_report_artifact_options_init(&tReportOptions);
    tReportOptions.sName = sName;
    tReportOptions.sMimeType = "application/json";
    tReportOptions.sStorageRef = sStorageRef;
    tReportOptions.sSummary = "multi-agent claw task report";
    tReportOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tReportOptions.sOutputRole = "report.task";
    tReportOptions.eReportClass = XWORK_ARTIFACT_REPORT_PROGRESS;
    tReportOptions.sReportSubjectRef = pNode->sTaskId;
    tReportOptions.sReportText = sReportJson;
    return xwork_run_emit_report_artifact(pRun, &tReportOptions, NULL);
}

static int add_agent(
    xwork_agent_pool *pPool,
    const char *sAgentId,
    const char *sDisplayName,
    xwork_agent_role eRole
)
{
    xwork_agent_options tAgentOptions;
    xwork_agent *pAgent = NULL;

    xwork_agent_options_init(&tAgentOptions);
    tAgentOptions.sAgentId = sAgentId;
    tAgentOptions.sDisplayName = sDisplayName;
    tAgentOptions.eRole = eRole;
    tAgentOptions.eAutonomy = XWORK_AUTONOMY_AUTO;
    return xwork_example_check(
        xwork_agent_pool_add_agent(pPool, &tAgentOptions, &pAgent),
        "add agent"
    );
}

static int add_task(
    xwork_task_graph *pGraph,
    const char *sTaskId,
    const char *sAgentId,
    const char *sInstruction,
    const char *sParentRunId,
    const char **psWorkspaceIds,
    size_t iWorkspaceCount
)
{
    xwork_task_node_options tTaskOptions;

    xwork_task_node_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = sTaskId;
    tTaskOptions.sAgentId = sAgentId;
    tTaskOptions.sParentRunId = sParentRunId;
    tTaskOptions.sInstruction = sInstruction;
    tTaskOptions.psWorkspaceIds = psWorkspaceIds;
    tTaskOptions.iWorkspaceCount = iWorkspaceCount;
    return xwork_example_check(
        xwork_task_graph_add_node(pGraph, &tTaskOptions),
        "add task"
    );
}

int main(void)
{
    xwork_profile tProfile;
    xwork_file_persistence tStore;
    xwork_file_persistence_options tStoreOptions;
    xwork_persistence_backend tPersistence;
    xwork_runtime_options tRuntimeOptions;
    xwork_workspace_options tWorkspaceOptions;
    xwork_agent_pool_options tPoolOptions;
    xwork_task_graph_options tGraphOptions;
    xwork_task_graph_options tRecoveryOptions;
    xwork_task_graph_result tResult;
    xwork_agent_pool_snapshot tPoolSnapshot;
    xwork_task_graph_snapshot tGraphSnapshot;
    xwork_task_node_summary tRecoveredSummary;
    xwork_run_index_query tRunIndexQuery;
    xwork_run_index_list tRunIndex;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace *pWorkspace = NULL;
    xwork_agent_pool *pPool = NULL;
    xwork_agent_pool *pRecoveredPool = NULL;
    xwork_task_graph *pGraph = NULL;
    xwork_task_graph *pRecoveredGraph = NULL;
    const char *asWorkspaceIds[1];
    int iExit = 1;

    xwork_profile_init(&tProfile);
    xwork_file_persistence_init(&tStore);
    xwork_file_persistence_options_init(&tStoreOptions);
    xwork_persistence_backend_init(&tPersistence);
    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_workspace_options_init(&tWorkspaceOptions);
    xwork_agent_pool_options_init(&tPoolOptions);
    xwork_task_graph_options_init(&tGraphOptions);
    xwork_task_graph_options_init(&tRecoveryOptions);
    xwork_task_graph_result_init(&tResult);
    xwork_agent_pool_snapshot_init(&tPoolSnapshot);
    xwork_task_graph_snapshot_init(&tGraphSnapshot);
    xwork_task_node_summary_init(&tRecoveredSummary);
    xwork_run_index_list_init(&tRunIndex);

    if ( xwork_example_check(
             xwork_profile_get_builtin(XWORK_PROFILE_XCLAW, &tProfile),
             "load xclaw profile"
         ) ) goto cleanup;

    tStoreOptions.sRootPath = "examples/.xwork_multi_agent_claw_store";
    if ( xwork_example_check(
             xwork_file_persistence_configure_backend(
                 &tStore,
                 &tStoreOptions,
                 &tPersistence
             ),
             "configure file persistence"
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_profile_apply_runtime_options(&tProfile, &tRuntimeOptions),
             "apply runtime profile"
         ) ) goto cleanup;
    tRuntimeOptions.pPersistenceBackend = &tPersistence;
    if ( xwork_example_check(
             xwork_runtime_create(&tRuntimeOptions, &pRuntime),
             "create runtime"
         ) ) goto cleanup;

    tWorkspaceOptions.sWorkspaceId = "workspace";
    tWorkspaceOptions.sRootPath = ".";
    if ( xwork_example_check(
             xwork_profile_apply_workspace_options(&tProfile, &tWorkspaceOptions),
             "apply workspace profile"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_runtime_add_workspace(pRuntime, &tWorkspaceOptions, &pWorkspace),
             "add workspace"
         ) ) goto cleanup;
    asWorkspaceIds[0] = "workspace";

    tPoolOptions.sPoolId = "example-multi-agent-claw-pool";
    tPoolOptions.pRuntime = pRuntime;
    if ( xwork_example_check(
             xwork_agent_pool_create(&tPoolOptions, &pPool),
             "create agent pool"
         ) ) goto cleanup;
    if ( add_agent(pPool, "planner", "Planner", XWORK_AGENT_ROLE_PLANNER) ) goto cleanup;
    if ( add_agent(pPool, "coder", "Coder", XWORK_AGENT_ROLE_CODER) ) goto cleanup;
    if ( add_agent(pPool, "tester", "Tester", XWORK_AGENT_ROLE_TESTER) ) goto cleanup;
    if ( add_agent(pPool, "reviewer", "Reviewer", XWORK_AGENT_ROLE_REVIEWER) ) goto cleanup;

    if ( xwork_example_check(
             xwork_agent_pool_get_snapshot(pPool, &tPoolSnapshot),
             "snapshot agent pool"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_file_persistence_store_agent_pool_snapshot(&tStore, &tPoolSnapshot),
             "store agent pool"
         ) ) goto cleanup;

    tGraphOptions.sGraphId = "example-multi-agent-claw";
    tGraphOptions.pAgentPool = pPool;
    tGraphOptions.iMaxConcurrency = 2u;
    tGraphOptions.eFailurePolicy = XWORK_TASK_FAILURE_REQUIRE_ALL;
    tGraphOptions.pfnExecute = multi_agent_claw_execute;
    if ( xwork_example_check(
             xwork_task_graph_create(&tGraphOptions, &pGraph),
             "create task graph"
         ) ) goto cleanup;

    if ( add_task(
             pGraph,
             "plan",
             "planner",
             "Create the autonomous work plan.",
             "example-multi-agent-claw-parent",
             asWorkspaceIds,
             1u
         ) ) goto cleanup;
    if ( add_task(
             pGraph,
             "implement",
             "coder",
             "Implement the planned change.",
             "example-multi-agent-claw-parent",
             asWorkspaceIds,
             1u
         ) ) goto cleanup;
    if ( add_task(
             pGraph,
             "validate",
             "tester",
             "Validate the implementation.",
             "example-multi-agent-claw-parent",
             asWorkspaceIds,
             1u
         ) ) goto cleanup;
    if ( add_task(
             pGraph,
             "report",
             "reviewer",
             "Review the agent outputs and produce a final handoff.",
             "example-multi-agent-claw-parent",
             asWorkspaceIds,
             1u
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_task_graph_add_dependency(pGraph, "plan", "implement"),
             "add dependency"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_task_graph_add_dependency(pGraph, "plan", "validate"),
             "add dependency"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_task_graph_add_dependency(pGraph, "implement", "report"),
             "add dependency"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_task_graph_add_dependency(pGraph, "validate", "report"),
             "add dependency"
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_task_graph_execute(pGraph, &tResult),
             "execute task graph"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_task_graph_get_snapshot(pGraph, &tGraphSnapshot),
             "snapshot task graph"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_file_persistence_store_task_graph_snapshot(&tStore, &tGraphSnapshot),
             "store task graph"
         ) ) goto cleanup;

    xwork_run_index_query_init(&tRunIndexQuery);
    tRunIndexQuery.sParentRunId = "example-multi-agent-claw-parent";
    tRunIndexQuery.sAgentId = "reviewer";
    tRunIndexQuery.sTaskId = "report";
    tRunIndexQuery.bRequireArtifacts = true;
    if ( xwork_example_check(
             xwork_file_persistence_query_run_index(
                 &tStore,
                 &tRunIndexQuery,
                 &tRunIndex
             ),
             "query child run index"
         ) ) goto cleanup;
    if ( tRunIndex.iCount == 0u ) {
        fprintf(stderr, "expected persisted reviewer report child run\n");
        goto cleanup;
    }
    xwork_run_index_list_reset(&tRunIndex);

    tRecoveryOptions.sGraphId = "example-multi-agent-claw-recovered";
    tRecoveryOptions.iMaxConcurrency = 2u;
    tRecoveryOptions.eFailurePolicy = XWORK_TASK_FAILURE_REQUIRE_ALL;
    tRecoveryOptions.pfnExecute = multi_agent_claw_execute;
    if ( xwork_example_check(
             xwork_file_persistence_recover_task_graph(
                 &tStore,
                 pRuntime,
                 "example-multi-agent-claw-pool",
                 "example-multi-agent-claw",
                 &tRecoveryOptions,
                 &pRecoveredPool,
                 &pRecoveredGraph
             ),
             "recover task graph"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_task_graph_get_node_summary(
                 pRecoveredGraph,
                 "report",
                 &tRecoveredSummary
             ),
             "inspect recovered task"
         ) ) goto cleanup;
    if ( tRecoveredSummary.eState != XWORK_TASK_COMPLETED ) {
        fprintf(stderr, "expected recovered report task to be completed\n");
        goto cleanup;
    }

    printf(
        "multi-agent claw completed %zu/%zu tasks and recovered report state %d.\n",
        tResult.iCompletedCount,
        tResult.iTotalCount,
        (int)tRecoveredSummary.eState
    );
    iExit = 0;

cleanup:
    xwork_task_node_summary_reset(&tRecoveredSummary);
    xwork_task_graph_snapshot_reset(&tGraphSnapshot);
    xwork_agent_pool_snapshot_reset(&tPoolSnapshot);
    xwork_run_index_list_reset(&tRunIndex);
    if ( pRecoveredGraph ) {
        xwork_task_graph_destroy(pRecoveredGraph);
    }
    if ( pRecoveredPool ) {
        xwork_agent_pool_destroy(pRecoveredPool);
    }
    if ( pGraph ) {
        xwork_task_graph_destroy(pGraph);
    }
    if ( pPool ) {
        xwork_agent_pool_destroy(pPool);
    }
    if ( pRuntime ) {
        xwork_runtime_destroy(pRuntime);
    }
    xwork_file_persistence_reset(&tStore);
    return iExit;
}
