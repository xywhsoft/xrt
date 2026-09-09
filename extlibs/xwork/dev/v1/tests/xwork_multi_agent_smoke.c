#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"
#include "../xwork.c"

typedef struct {
    xmutex_struct tLock;
    size_t iActiveCount;
    size_t iPeakActiveCount;
    size_t iFlakyAttemptCount;
    xwork_task_graph *pCancelGraph;
    xwork_task_graph *pPauseGraph;
} multi_agent_smoke_state;

static xwork_status smoke_task_execute(
    xwork_run *pRun,
    const xwork_task_node_summary *pNode,
    void *pUserData
)
{
    multi_agent_smoke_state *pState = (multi_agent_smoke_state *)pUserData;

    assert(pRun != NULL);
    assert(pNode != NULL);
    assert(pNode->sTaskId != NULL);

    xrtMutexLock(&pState->tLock);
    ++pState->iActiveCount;
    if ( pState->iActiveCount > pState->iPeakActiveCount ) {
        pState->iPeakActiveCount = pState->iActiveCount;
    }
    xrtMutexUnlock(&pState->tLock);

    xrtSleep(50u);

    if ( strcmp(pNode->sTaskId, "flaky") == 0 ) {
        xrtMutexLock(&pState->tLock);
        ++pState->iFlakyAttemptCount;
        --pState->iActiveCount;
        xrtMutexUnlock(&pState->tLock);
        return pNode->iAttemptCount == 1u ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK;
    }
    if ( strcmp(pNode->sTaskId, "cancel") == 0 ) {
        assert(xwork_task_graph_cancel(pState->pCancelGraph, "smoke cancel") == XWORK_OK);
        xrtMutexLock(&pState->tLock);
        --pState->iActiveCount;
        xrtMutexUnlock(&pState->tLock);
        return XWORK_ERROR_CANCELLED;
    }
    if ( strcmp(pNode->sTaskId, "pause-first") == 0 ) {
        assert(xwork_task_graph_pause(pState->pPauseGraph, "smoke pause") == XWORK_OK);
    }

    xrtMutexLock(&pState->tLock);
    --pState->iActiveCount;
    xrtMutexUnlock(&pState->tLock);

    if ( strcmp(pNode->sTaskId, "fail") == 0 ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    return XWORK_OK;
}

static void add_agent(
    xwork_agent_pool *pPool,
    const char *sAgentId,
    xwork_agent_role eRole
)
{
    xwork_agent_options tAgentOptions;
    xwork_agent *pAgent = NULL;

    xwork_agent_options_init(&tAgentOptions);
    tAgentOptions.sAgentId = sAgentId;
    tAgentOptions.eRole = eRole;
    tAgentOptions.eAutonomy = XWORK_AUTONOMY_AUTO;
    tAgentOptions.iMaxTurns = strcmp(sAgentId, "tester") == 0 ? 3u : 2u;
    tAgentOptions.iTimeoutMs = strcmp(sAgentId, "tester") == 0 ? 2500u : 1500u;
    tAgentOptions.iMaxRetries = strcmp(sAgentId, "coder") == 0 ? 1u : 0u;
    assert(xwork_agent_pool_add_agent(pPool, &tAgentOptions, &pAgent) == XWORK_OK);
    assert(pAgent != NULL);
    assert(strcmp(xwork_agent_get_id(pAgent), sAgentId) == 0);
    assert(xwork_agent_get_role(pAgent) == eRole);
}

static void add_task(
    xwork_task_graph *pGraph,
    const char *sTaskId,
    const char *sAgentId,
    const char *sParentRunId
)
{
    xwork_task_node_options tNodeOptions;

    xwork_task_node_options_init(&tNodeOptions);
    tNodeOptions.sTaskId = sTaskId;
    tNodeOptions.sAgentId = sAgentId;
    tNodeOptions.sParentRunId = sParentRunId;
    tNodeOptions.sInstruction = "mock task";
    assert(xwork_task_graph_add_node(pGraph, &tNodeOptions) == XWORK_OK);
}

static void assert_agent_budget(
    const xwork_agent_pool_snapshot *pSnapshot,
    const char *sAgentId,
    size_t iMaxTurns,
    size_t iTimeoutMs
)
{
    size_t i;

    assert(pSnapshot != NULL);
    assert(sAgentId != NULL);
    for ( i = 0u; i < pSnapshot->tAgents.iCount; ++i ) {
        if ( strcmp(pSnapshot->tAgents.pItems[i].sAgentId, sAgentId) == 0 ) {
            assert(pSnapshot->tAgents.pItems[i].iMaxTurns == iMaxTurns);
            assert(pSnapshot->tAgents.pItems[i].iTimeoutMs == iTimeoutMs);
            return;
        }
    }
    assert(false);
}

int main(void)
{
    xwork_runtime_options tRuntimeOptions;
    xwork_agent_pool_options tPoolOptions;
    xwork_task_graph_options tGraphOptions;
    xwork_task_graph_result tResult;
    xwork_task_node_summary tSummary;
    xwork_task_node_summary_list tList;
    xwork_agent_pool_snapshot tPoolSnapshot;
    xwork_agent_pool_snapshot tLoadedPoolSnapshot;
    xwork_task_graph_snapshot tSnapshot;
    xwork_task_graph_snapshot tLoadedSnapshot;
    xwork_handoff_request_options tHandoffRequest;
    xwork_handoff_result_options tHandoffResult;
    xwork_handoff_summary tHandoffSummary;
    xwork_handoff_summary_list tHandoffList;
    xwork_run_options tAggregateRunOptions;
    xwork_run_summary tRunSummary;
    xwork_run_step tStep;
    xwork_run_index_query tRunIndexQuery;
    xwork_run_index_list tRunIndex;
    xwork_file_persistence_options tPersistenceOptions;
    xwork_file_persistence tPersistence;
    xwork_persistence_backend tPersistenceBackend;
    xwork_event tEvent;
    xwork_artifact tArtifact;
    xwork_runtime *pRuntime = NULL;
    xwork_run *pAggregateRun = NULL;
    xwork_run *pJoinRun = NULL;
    xwork_agent_pool *pPool = NULL;
    xwork_agent_pool *pImportedPool = NULL;
    xwork_agent_pool *pRecoveredPool = NULL;
    xwork_task_graph *pGraph = NULL;
    xwork_task_graph *pImportedGraph = NULL;
    xwork_task_graph *pRecoveredGraph = NULL;
    xwork_task_graph *pFailureGraph = NULL;
    xwork_task_graph *pRetryGraph = NULL;
    xwork_task_graph *pCancelGraph = NULL;
    xwork_task_graph *pPauseGraph = NULL;
    multi_agent_smoke_state tState;
    const char *sTempRoot;
    char sPersistenceRoot[512];
    bool bFoundJoinRun = false;
    bool bFoundBlockedEvent = false;
    bool bFoundUnblockedEvent = false;
    size_t i;

    memset(&tState, 0, sizeof(tState));
    xrtMutexInit(&tState.tLock);
    xwork_task_graph_result_init(&tResult);
    xwork_task_node_summary_init(&tSummary);
    xwork_task_node_summary_list_init(&tList);
    xwork_agent_pool_snapshot_init(&tPoolSnapshot);
    xwork_agent_pool_snapshot_init(&tLoadedPoolSnapshot);
    xwork_task_graph_snapshot_init(&tSnapshot);
    xwork_task_graph_snapshot_init(&tLoadedSnapshot);
    xwork_handoff_request_options_init(&tHandoffRequest);
    xwork_handoff_result_options_init(&tHandoffResult);
    xwork_handoff_summary_init(&tHandoffSummary);
    xwork_handoff_summary_list_init(&tHandoffList);
    xwork_run_options_init(&tAggregateRunOptions);
    xwork_run_summary_init(&tRunSummary);
    xwork_run_step_init(&tStep);
    xwork_run_index_list_init(&tRunIndex);
    xwork_file_persistence_init(&tPersistence);
    xwork_event_init(&tEvent);
    xwork_artifact_init(&tArtifact);

    sTempRoot = getenv("TEMP");
    if ( !sTempRoot || !sTempRoot[0] ) {
        sTempRoot = ".";
    }
    snprintf(
        sPersistenceRoot,
        sizeof(sPersistenceRoot),
        "%s/xwork_multi_agent_persistence_%ld_%ld",
        sTempRoot,
        (long)time(NULL),
        (long)clock()
    );
    xwork_file_persistence_options_init(&tPersistenceOptions);
    tPersistenceOptions.sRootPath = sPersistenceRoot;
    assert(xwork_file_persistence_configure_backend(
        &tPersistence,
        &tPersistenceOptions,
        &tPersistenceBackend
    ) == XWORK_OK);

    xwork_runtime_options_init(&tRuntimeOptions);
    tRuntimeOptions.pPersistenceBackend = &tPersistenceBackend;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);

    xwork_agent_pool_options_init(&tPoolOptions);
    tPoolOptions.sPoolId = "smoke-pool";
    tPoolOptions.pRuntime = pRuntime;
    assert(xwork_agent_pool_create(&tPoolOptions, &pPool) == XWORK_OK);
    add_agent(pPool, "coder", XWORK_AGENT_ROLE_CODER);
    add_agent(pPool, "tester", XWORK_AGENT_ROLE_TESTER);
    assert(xwork_agent_pool_get_agent_count(pPool) == 2u);
    assert(xwork_agent_pool_find_agent(pPool, "coder") != NULL);
    assert(xwork_agent_pool_get_snapshot(pPool, &tPoolSnapshot) == XWORK_OK);
    assert(strcmp(tPoolSnapshot.sPoolId, "smoke-pool") == 0);
    assert(tPoolSnapshot.tAgents.iCount == 2u);
    assert_agent_budget(&tPoolSnapshot, "coder", 2u, 1500u);
    assert_agent_budget(&tPoolSnapshot, "tester", 3u, 2500u);
    assert(xwork_file_persistence_store_agent_pool_snapshot(
        &tPersistence,
        &tPoolSnapshot
    ) == XWORK_OK);
    assert(xwork_file_persistence_load_agent_pool_snapshot(
        &tPersistence,
        "smoke-pool",
        &tLoadedPoolSnapshot
    ) == XWORK_OK);
    assert(tLoadedPoolSnapshot.tAgents.iCount == 2u);
    assert_agent_budget(&tLoadedPoolSnapshot, "coder", 2u, 1500u);
    assert_agent_budget(&tLoadedPoolSnapshot, "tester", 3u, 2500u);
    assert(xwork_agent_pool_create_from_snapshot(
        pRuntime,
        &tLoadedPoolSnapshot,
        &pImportedPool
    ) == XWORK_OK);
    assert(xwork_agent_pool_get_agent_count(pImportedPool) == 2u);
    assert(xwork_agent_pool_find_agent(pImportedPool, "coder") != NULL);
    assert(xwork_agent_pool_find_agent(pImportedPool, "tester") != NULL);

    xwork_task_graph_options_init(&tGraphOptions);
    tGraphOptions.sGraphId = "parallel";
    tGraphOptions.pAgentPool = pPool;
    tGraphOptions.iMaxConcurrency = 3u;
    tGraphOptions.eFailurePolicy = XWORK_TASK_FAILURE_REQUIRE_ALL;
    tGraphOptions.pfnExecute = smoke_task_execute;
    tGraphOptions.pUserData = &tState;
    assert(xwork_task_graph_create(&tGraphOptions, &pGraph) == XWORK_OK);

    add_task(pGraph, "code", "coder", "parent-run");
    add_task(pGraph, "test", "tester", "parent-run");
    add_task(pGraph, "join", "tester", "parent-run");
    assert(xwork_task_graph_add_dependency(pGraph, "code", "join") == XWORK_OK);
    assert(xwork_task_graph_add_dependency(pGraph, "test", "join") == XWORK_OK);
    assert(xwork_task_graph_get_node_count(pGraph) == 3u);
    assert(xwork_task_graph_get_snapshot(pGraph, &tSnapshot) == XWORK_OK);
    assert(tSnapshot.tNodes.iCount == 3u);
    assert(tSnapshot.tNodes.pItems[2].iDependencyCount == 2u);
    assert(tSnapshot.tNodes.pItems[2].iMaxTurns == 3u);
    assert(tSnapshot.tNodes.pItems[2].iTimeoutMs == 2500u);
    assert(strcmp(tSnapshot.tNodes.pItems[2].psDependencyTaskIds[0], "code") == 0);
    assert(strcmp(tSnapshot.tNodes.pItems[2].psDependencyTaskIds[1], "test") == 0);
    assert(xwork_file_persistence_store_task_graph_snapshot(
        &tPersistence,
        &tSnapshot
    ) == XWORK_OK);
    assert(xwork_file_persistence_load_task_graph_snapshot(
        &tPersistence,
        "parallel",
        &tLoadedSnapshot
    ) == XWORK_OK);
    assert(strcmp(tLoadedSnapshot.sGraphId, "parallel") == 0);
    assert(tLoadedSnapshot.tNodes.iCount == 3u);
    assert(tLoadedSnapshot.tNodes.pItems[2].iDependencyCount == 2u);
    assert(tLoadedSnapshot.tNodes.pItems[2].iMaxTurns == 3u);
    assert(tLoadedSnapshot.tNodes.pItems[2].iTimeoutMs == 2500u);
    xwork_task_graph_snapshot_reset(&tLoadedSnapshot);
    tGraphOptions.sGraphId = "parallel-imported";
    tGraphOptions.pAgentPool = pImportedPool;
    assert(xwork_task_graph_create_from_snapshot(
        &tGraphOptions,
        &tSnapshot,
        &pImportedGraph
    ) == XWORK_OK);
    assert(xwork_task_graph_get_node_count(pImportedGraph) == 3u);
    assert(xwork_task_graph_get_node_summary(pImportedGraph, "join", &tSummary) == XWORK_OK);
    assert(tSummary.iDependencyCount == 2u);
    assert(tSummary.iMaxTurns == 3u);
    assert(tSummary.iTimeoutMs == 2500u);
    xwork_task_node_summary_reset(&tSummary);
    xwork_task_graph_destroy(pImportedGraph);
    pImportedGraph = NULL;
    tGraphOptions.sGraphId = "parallel-recovered";
    tGraphOptions.pAgentPool = NULL;
    assert(xwork_file_persistence_recover_task_graph(
        &tPersistence,
        pRuntime,
        "smoke-pool",
        "parallel",
        &tGraphOptions,
        &pRecoveredPool,
        &pRecoveredGraph
    ) == XWORK_OK);
    assert(xwork_agent_pool_get_agent_count(pRecoveredPool) == 2u);
    assert(xwork_task_graph_get_node_count(pRecoveredGraph) == 3u);
    assert(xwork_task_graph_execute(pRecoveredGraph, &tResult) == XWORK_OK);
    assert(tResult.iCompletedCount == 3u);
    assert(xwork_task_graph_get_node_summary(pRecoveredGraph, "join", &tSummary) == XWORK_OK);
    assert(tSummary.eState == XWORK_TASK_COMPLETED);
    xwork_task_node_summary_reset(&tSummary);
    xwork_task_graph_destroy(pRecoveredGraph);
    pRecoveredGraph = NULL;
    xwork_agent_pool_destroy(pRecoveredPool);
    pRecoveredPool = NULL;
    xwork_task_graph_snapshot_reset(&tSnapshot);
    tGraphOptions.sGraphId = "parallel";
    tGraphOptions.pAgentPool = pPool;

    assert(xwork_task_graph_execute(pGraph, &tResult) == XWORK_OK);
    assert(tResult.iTotalCount == 3u);
    assert(tResult.iCompletedCount == 3u);
    assert(tResult.iFailedCount == 0u);
    assert(tResult.iSkippedCount == 0u);
    assert(tState.iPeakActiveCount >= 2u);

    assert(xwork_task_graph_get_node_summary(pGraph, "join", &tSummary) == XWORK_OK);
    assert(tSummary.eState == XWORK_TASK_COMPLETED);
    assert(strcmp(tSummary.sParentRunId, "parent-run") == 0);
    assert(tSummary.iMaxTurns == 3u);
    assert(tSummary.iTimeoutMs == 2500u);
    xwork_task_node_summary_reset(&tSummary);
    pJoinRun = xwork_task_graph_get_node_run(pGraph, "join");
    assert(pJoinRun != NULL);
    assert(xwork_run_get_event_count(pJoinRun) >= 8u);
    assert(xwork_run_get_last_event(pJoinRun, &tEvent) == XWORK_OK);
    assert(tEvent.eKind == XWORK_EVENT_TASK_JOINED);
    xwork_event_reset(&tEvent);
    for ( i = 0u; i < xwork_run_get_step_count(pJoinRun); ++i ) {
        assert(xwork_run_get_step(pJoinRun, i, &tStep) == XWORK_OK);
        if ( tStep.eEventKind == XWORK_EVENT_TASK_BLOCKED ) {
            bFoundBlockedEvent = true;
        } else if ( tStep.eEventKind == XWORK_EVENT_TASK_UNBLOCKED ) {
            bFoundUnblockedEvent = true;
        }
        xwork_run_step_reset(&tStep);
    }
    assert(bFoundBlockedEvent);
    assert(bFoundUnblockedEvent);
    assert(xwork_run_get_summary(pJoinRun, &tRunSummary) == XWORK_OK);
    assert(strcmp(tRunSummary.sParentRunId, "parent-run") == 0);
    assert(strcmp(tRunSummary.sAgentId, "tester") == 0);
    assert(strcmp(tRunSummary.sTaskId, "join") == 0);
    xwork_run_summary_reset(&tRunSummary);

    {
        const char *psArtifactRefs[] = { "artifact://parallel/code.patch" };
        const char *psMemoryRefs[] = { "memory://session/planning" };
        const char *psSharedWorkspaces[] = { "workspace://repo" };

        tHandoffRequest.sHandoffId = "handoff-code-to-join";
        tHandoffRequest.sFromTaskId = "code";
        tHandoffRequest.sToTaskId = "join";
        tHandoffRequest.sReason = "share implementation context";
        tHandoffRequest.psArtifactRefs = psArtifactRefs;
        tHandoffRequest.iArtifactRefCount = 1u;
        tHandoffRequest.psMemoryContextRefs = psMemoryRefs;
        tHandoffRequest.iMemoryContextRefCount = 1u;
        tHandoffRequest.psSharedWorkspaceIds = psSharedWorkspaces;
        tHandoffRequest.iSharedWorkspaceCount = 1u;
        tHandoffRequest.bReadOnlySharedContext = true;
        tHandoffRequest.bWritableWorkspace = true;
        assert(xwork_task_graph_request_handoff(
            pGraph,
            &tHandoffRequest,
            &tHandoffSummary
        ) == XWORK_OK);
        assert(strcmp(tHandoffSummary.sHandoffId, "handoff-code-to-join") == 0);
        assert(strcmp(tHandoffSummary.sFromTaskId, "code") == 0);
        assert(strcmp(tHandoffSummary.sToTaskId, "join") == 0);
        assert(tHandoffSummary.eState == XWORK_HANDOFF_PENDING);
        assert(tHandoffSummary.iArtifactRefCount == 1u);
        assert(strcmp(tHandoffSummary.psArtifactRefs[0], "artifact://parallel/code.patch") == 0);
        assert(tHandoffSummary.iMemoryContextRefCount == 1u);
        assert(strcmp(tHandoffSummary.psMemoryContextRefs[0], "memory://session/planning") == 0);
        assert(tHandoffSummary.iSharedWorkspaceCount == 1u);
        assert(strcmp(tHandoffSummary.psSharedWorkspaceIds[0], "workspace://repo") == 0);
        assert(tHandoffSummary.bReadOnlySharedContext);
        assert(tHandoffSummary.bWritableWorkspace);
        xwork_handoff_summary_reset(&tHandoffSummary);

        assert(xwork_task_graph_list_handoffs(pGraph, &tHandoffList) == XWORK_OK);
        assert(tHandoffList.iCount == 1u);
        assert(strcmp(tHandoffList.pItems[0].sHandoffId, "handoff-code-to-join") == 0);
        xwork_handoff_summary_list_reset(&tHandoffList);

        xwork_handoff_result_options_init(&tHandoffResult);
        tHandoffResult.sHandoffId = "handoff-code-to-join";
        tHandoffResult.eState = XWORK_HANDOFF_ACCEPTED;
        tHandoffResult.iStatus = XWORK_OK;
        tHandoffResult.sMessage = "accepted by tester";
        assert(xwork_task_graph_resolve_handoff(
            pGraph,
            &tHandoffResult,
            &tHandoffSummary
        ) == XWORK_OK);
        assert(tHandoffSummary.eState == XWORK_HANDOFF_ACCEPTED);
        assert(strcmp(tHandoffSummary.sMessage, "accepted by tester") == 0);
        xwork_handoff_summary_reset(&tHandoffSummary);

        tHandoffResult.eState = XWORK_HANDOFF_COMPLETED;
        tHandoffResult.sMessage = "context consumed";
        assert(xwork_task_graph_resolve_handoff(
            pGraph,
            &tHandoffResult,
            &tHandoffSummary
        ) == XWORK_OK);
        assert(tHandoffSummary.eState == XWORK_HANDOFF_COMPLETED);
        xwork_handoff_summary_reset(&tHandoffSummary);
        assert(xwork_run_get_last_event(pJoinRun, &tEvent) == XWORK_OK);
        assert(tEvent.eKind == XWORK_EVENT_HANDOFF_COMPLETED);
        xwork_event_reset(&tEvent);
    }

    xwork_run_index_query_init(&tRunIndexQuery);
    tRunIndexQuery.sParentRunId = "parent-run";
    tRunIndexQuery.sAgentId = "tester";
    tRunIndexQuery.sTaskId = "join";
    assert(xwork_file_persistence_query_run_index(
        &tPersistence,
        &tRunIndexQuery,
        &tRunIndex
    ) == XWORK_OK);
    assert(tRunIndex.iCount >= 1u);
    for ( i = 0u; i < tRunIndex.iCount; ++i ) {
        if ( strcmp(tRunIndex.pItems[i].tSummary.sRunId, "parallel:join") == 0 ) {
            bFoundJoinRun = true;
            assert(strcmp(tRunIndex.pItems[i].tSummary.sAgentId, "tester") == 0);
            assert(strcmp(tRunIndex.pItems[i].tSummary.sTaskId, "join") == 0);
        }
    }
    assert(bFoundJoinRun);
    xwork_run_index_list_reset(&tRunIndex);

    assert(xwork_task_graph_emit_agent_result_report(
        pGraph,
        "join",
        "join-agent-result",
        &tArtifact
    ) == XWORK_OK);
    assert(strcmp(tArtifact.sArtifactId, "join-agent-result") == 0);
    assert(tArtifact.eKind == XWORK_ARTIFACT_REPORT);
    assert(tArtifact.eOutputClass == XWORK_ARTIFACT_OUTPUT_JSON);
    assert(strcmp(tArtifact.sOutputRole, "agent.result.report") == 0);
    assert(tArtifact.eReportClass == XWORK_ARTIFACT_REPORT_SUMMARY);
    assert(strstr(tArtifact.sContentText, "\"report_kind\":\"agent_result\"") != NULL);
    assert(strstr(tArtifact.sContentText, "\"task_id\":\"join\"") != NULL);
    xwork_artifact_reset(&tArtifact);

    xwork_run_options_init(&tAggregateRunOptions);
    tAggregateRunOptions.sRunId = "parallel-parent-report";
    tAggregateRunOptions.sInstruction = "multi-agent parent report";
    assert(xwork_run_create(pRuntime, &tAggregateRunOptions, &pAggregateRun) == XWORK_OK);
    assert(xwork_task_graph_emit_aggregate_report(
        pGraph,
        pAggregateRun,
        "parallel-aggregate-report",
        &tArtifact
    ) == XWORK_OK);
    assert(strcmp(tArtifact.sArtifactId, "parallel-aggregate-report") == 0);
    assert(tArtifact.eKind == XWORK_ARTIFACT_REPORT);
    assert(tArtifact.eOutputClass == XWORK_ARTIFACT_OUTPUT_JSON);
    assert(strcmp(tArtifact.sOutputRole, "agent.aggregate.report") == 0);
    assert(tArtifact.eReportClass == XWORK_ARTIFACT_REPORT_FINAL);
    assert(strstr(tArtifact.sContentText, "\"report_kind\":\"multi_agent_aggregate\"") != NULL);
    assert(strstr(tArtifact.sContentText, "\"completed_count\":3") != NULL);
    xwork_artifact_reset(&tArtifact);

    assert(xwork_task_graph_list_node_summaries(pGraph, &tList) == XWORK_OK);
    assert(tList.iCount == 3u);
    xwork_task_node_summary_list_reset(&tList);
    assert(xwork_task_graph_get_snapshot(pGraph, &tSnapshot) == XWORK_OK);
    assert(strcmp(tSnapshot.sGraphId, "parallel") == 0);
    assert(tSnapshot.iMaxConcurrency == 3u);
    assert(tSnapshot.tResult.iCompletedCount == 3u);
    assert(tSnapshot.tNodes.iCount == 3u);
    assert(tSnapshot.tNodes.pItems[2].iMaxTurns == 3u);
    assert(tSnapshot.tNodes.pItems[2].iTimeoutMs == 2500u);
    assert(tSnapshot.tHandoffs.iCount == 1u);
    assert(strcmp(tSnapshot.tHandoffs.pItems[0].sHandoffId, "handoff-code-to-join") == 0);
    assert(tSnapshot.tHandoffs.pItems[0].eState == XWORK_HANDOFF_COMPLETED);
    assert(tSnapshot.tHandoffs.pItems[0].iArtifactRefCount == 1u);
    assert(xwork_file_persistence_store_task_graph_snapshot(
        &tPersistence,
        &tSnapshot
    ) == XWORK_OK);
    assert(xwork_file_persistence_load_task_graph_snapshot(
        &tPersistence,
        "parallel",
        &tLoadedSnapshot
    ) == XWORK_OK);
    assert(tLoadedSnapshot.tHandoffs.iCount == 1u);
    assert(tLoadedSnapshot.tNodes.pItems[2].iMaxTurns == 3u);
    assert(tLoadedSnapshot.tNodes.pItems[2].iTimeoutMs == 2500u);
    assert(strcmp(tLoadedSnapshot.tHandoffs.pItems[0].sHandoffId, "handoff-code-to-join") == 0);
    assert(tLoadedSnapshot.tHandoffs.pItems[0].eState == XWORK_HANDOFF_COMPLETED);
    assert(tLoadedSnapshot.tHandoffs.pItems[0].iMemoryContextRefCount == 1u);
    xwork_task_graph_snapshot_reset(&tLoadedSnapshot);
    tGraphOptions.sGraphId = "parallel-handoff-imported";
    tGraphOptions.pAgentPool = pPool;
    assert(xwork_task_graph_create_from_snapshot(
        &tGraphOptions,
        &tSnapshot,
        &pImportedGraph
    ) == XWORK_OK);
    assert(xwork_task_graph_list_handoffs(pImportedGraph, &tHandoffList) == XWORK_OK);
    assert(tHandoffList.iCount == 1u);
    assert(tHandoffList.pItems[0].eState == XWORK_HANDOFF_COMPLETED);
    xwork_handoff_summary_list_reset(&tHandoffList);
    xwork_task_graph_destroy(pImportedGraph);
    pImportedGraph = NULL;
    tGraphOptions.sGraphId = "parallel-handoff-recovered";
    tGraphOptions.pAgentPool = NULL;
    assert(xwork_file_persistence_recover_task_graph(
        &tPersistence,
        pRuntime,
        "smoke-pool",
        "parallel",
        &tGraphOptions,
        &pRecoveredPool,
        &pRecoveredGraph
    ) == XWORK_OK);
    assert(xwork_task_graph_list_handoffs(pRecoveredGraph, &tHandoffList) == XWORK_OK);
    assert(tHandoffList.iCount == 1u);
    assert(tHandoffList.pItems[0].eState == XWORK_HANDOFF_COMPLETED);
    xwork_handoff_summary_list_reset(&tHandoffList);
    xwork_task_graph_destroy(pRecoveredGraph);
    pRecoveredGraph = NULL;
    xwork_agent_pool_destroy(pRecoveredPool);
    pRecoveredPool = NULL;
    xwork_task_graph_snapshot_reset(&tSnapshot);

    xwork_task_graph_options_init(&tGraphOptions);
    tGraphOptions.sGraphId = "failure";
    tGraphOptions.pAgentPool = pPool;
    tGraphOptions.iMaxConcurrency = 2u;
    tGraphOptions.eFailurePolicy = XWORK_TASK_FAILURE_REQUIRE_ALL;
    tGraphOptions.pfnExecute = smoke_task_execute;
    tGraphOptions.pUserData = &tState;
    assert(xwork_task_graph_create(&tGraphOptions, &pFailureGraph) == XWORK_OK);
    add_task(pFailureGraph, "fail", "coder", NULL);
    add_task(pFailureGraph, "dependent", "tester", NULL);
    assert(xwork_task_graph_add_dependency(pFailureGraph, "fail", "dependent") == XWORK_OK);
    assert(xwork_task_graph_execute(pFailureGraph, &tResult) == XWORK_ERROR_EXTERNAL_FAILURE);
    assert(tResult.iTotalCount == 2u);
    assert(tResult.iFailedCount == 1u);
    assert(tResult.iSkippedCount == 1u);
    assert(xwork_task_graph_get_node_summary(pFailureGraph, "dependent", &tSummary) == XWORK_OK);
    assert(tSummary.eState == XWORK_TASK_SKIPPED);
    xwork_task_node_summary_reset(&tSummary);

    xwork_task_graph_options_init(&tGraphOptions);
    tGraphOptions.sGraphId = "retry";
    tGraphOptions.pAgentPool = pPool;
    tGraphOptions.iMaxConcurrency = 1u;
    tGraphOptions.eFailurePolicy = XWORK_TASK_FAILURE_REQUIRE_ALL;
    tGraphOptions.pfnExecute = smoke_task_execute;
    tGraphOptions.pUserData = &tState;
    assert(xwork_task_graph_create(&tGraphOptions, &pRetryGraph) == XWORK_OK);
    add_task(pRetryGraph, "flaky", "coder", NULL);
    assert(xwork_task_graph_execute(pRetryGraph, &tResult) == XWORK_OK);
    assert(tResult.iCompletedCount == 1u);
    assert(tState.iFlakyAttemptCount == 2u);
    assert(xwork_task_graph_get_node_summary(pRetryGraph, "flaky", &tSummary) == XWORK_OK);
    assert(tSummary.eState == XWORK_TASK_COMPLETED);
    assert(tSummary.iAttemptCount == 2u);
    assert(tSummary.iMaxTurns == 2u);
    assert(tSummary.iTimeoutMs == 1500u);
    assert(tSummary.iMaxRetries == 1u);
    xwork_task_node_summary_reset(&tSummary);

    xwork_task_graph_options_init(&tGraphOptions);
    tGraphOptions.sGraphId = "cancel";
    tGraphOptions.pAgentPool = pPool;
    tGraphOptions.iMaxConcurrency = 1u;
    tGraphOptions.eFailurePolicy = XWORK_TASK_FAILURE_REQUIRE_ALL;
    tGraphOptions.pfnExecute = smoke_task_execute;
    tGraphOptions.pUserData = &tState;
    assert(xwork_task_graph_create(&tGraphOptions, &pCancelGraph) == XWORK_OK);
    tState.pCancelGraph = pCancelGraph;
    add_task(pCancelGraph, "cancel", "coder", NULL);
    add_task(pCancelGraph, "after-cancel", "tester", NULL);
    assert(xwork_task_graph_add_dependency(pCancelGraph, "cancel", "after-cancel") == XWORK_OK);
    assert(xwork_task_graph_execute(pCancelGraph, &tResult) == XWORK_ERROR_CANCELLED);
    assert(xwork_task_graph_is_cancelled(pCancelGraph));
    assert(tResult.iCancelledCount == 2u);
    assert(xwork_task_graph_get_node_summary(pCancelGraph, "after-cancel", &tSummary) == XWORK_OK);
    assert(tSummary.eState == XWORK_TASK_CANCELLED);
    xwork_task_node_summary_reset(&tSummary);
    assert(xwork_task_graph_get_snapshot(pCancelGraph, &tSnapshot) == XWORK_OK);
    assert(tSnapshot.bCancelRequested);
    assert(strcmp(tSnapshot.sCancelReason, "smoke cancel") == 0);
    assert(tSnapshot.tResult.iCancelledCount == 2u);
    xwork_task_graph_snapshot_reset(&tSnapshot);

    xwork_task_graph_options_init(&tGraphOptions);
    tGraphOptions.sGraphId = "pause";
    tGraphOptions.pAgentPool = pPool;
    tGraphOptions.iMaxConcurrency = 1u;
    tGraphOptions.eFailurePolicy = XWORK_TASK_FAILURE_REQUIRE_ALL;
    tGraphOptions.pfnExecute = smoke_task_execute;
    tGraphOptions.pUserData = &tState;
    assert(xwork_task_graph_create(&tGraphOptions, &pPauseGraph) == XWORK_OK);
    tState.pPauseGraph = pPauseGraph;
    add_task(pPauseGraph, "pause-first", "coder", NULL);
    add_task(pPauseGraph, "pause-second", "tester", NULL);
    assert(xwork_task_graph_add_dependency(pPauseGraph, "pause-first", "pause-second") == XWORK_OK);
    assert(xwork_task_graph_execute(pPauseGraph, &tResult) == XWORK_ERROR_PAUSED);
    assert(xwork_task_graph_is_paused(pPauseGraph));
    assert(tResult.iCompletedCount == 1u);
    assert(xwork_task_graph_get_node_summary(pPauseGraph, "pause-second", &tSummary) == XWORK_OK);
    assert(tSummary.eState == XWORK_TASK_PENDING);
    xwork_task_node_summary_reset(&tSummary);
    assert(xwork_task_graph_get_snapshot(pPauseGraph, &tSnapshot) == XWORK_OK);
    assert(tSnapshot.bPauseRequested);
    assert(strcmp(tSnapshot.sPauseReason, "smoke pause") == 0);
    assert(xwork_file_persistence_store_task_graph_snapshot(
        &tPersistence,
        &tSnapshot
    ) == XWORK_OK);
    xwork_task_graph_snapshot_reset(&tSnapshot);
    assert(xwork_file_persistence_load_task_graph_snapshot(
        &tPersistence,
        "pause",
        &tLoadedSnapshot
    ) == XWORK_OK);
    assert(tLoadedSnapshot.bPauseRequested);
    assert(strcmp(tLoadedSnapshot.sPauseReason, "smoke pause") == 0);
    xwork_task_graph_snapshot_reset(&tLoadedSnapshot);
    assert(xwork_task_graph_resume(pPauseGraph) == XWORK_OK);
    assert(!xwork_task_graph_is_paused(pPauseGraph));
    assert(xwork_task_graph_execute(pPauseGraph, &tResult) == XWORK_OK);
    assert(tResult.iCompletedCount == 2u);

    xwork_task_graph_destroy(pPauseGraph);
    xwork_task_graph_destroy(pCancelGraph);
    xwork_task_graph_destroy(pRetryGraph);
    xwork_task_graph_destroy(pFailureGraph);
    xwork_task_graph_destroy(pRecoveredGraph);
    xwork_task_graph_destroy(pGraph);
    xwork_run_destroy(pAggregateRun);
    xwork_agent_pool_destroy(pRecoveredPool);
    xwork_agent_pool_destroy(pImportedPool);
    xwork_agent_pool_destroy(pPool);
    xwork_runtime_destroy(pRuntime);
    xwork_task_graph_snapshot_reset(&tSnapshot);
    xwork_task_graph_snapshot_reset(&tLoadedSnapshot);
    xwork_handoff_summary_reset(&tHandoffSummary);
    xwork_handoff_summary_list_reset(&tHandoffList);
    xwork_agent_pool_snapshot_reset(&tPoolSnapshot);
    xwork_agent_pool_snapshot_reset(&tLoadedPoolSnapshot);
    xwork_run_summary_reset(&tRunSummary);
    xwork_run_index_list_reset(&tRunIndex);
    xwork_file_persistence_reset(&tPersistence);
    xwork_event_reset(&tEvent);
    xwork_run_step_reset(&tStep);
    xwork_artifact_reset(&tArtifact);
    xrtMutexUnit(&tState.tLock);

    printf("xwork multi-agent smoke passed\n");
    return 0;
}
