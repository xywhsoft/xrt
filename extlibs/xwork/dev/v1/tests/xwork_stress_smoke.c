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

#define XWORK_STRESS_RUN_COUNT 24u
#define XWORK_STRESS_ARTIFACTS_PER_RUN 12u

static void xwork_stress_terminal_long_output(void)
{
    xwork_runtime *pRuntime = NULL;
    xwork_runtime_options tRuntimeOptions;
    xwork_local_host tHost;
    xwork_local_host_options tHostOptions;
    xwork_host_services tHostServices;
    xwork_tool_result tResult;
    xwork_status iStatus;
    const char *sTerminalRequest;
    const char *sPipeRequest;

    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_local_host_init(&tHost);
    xwork_local_host_options_init(&tHostOptions);
    xwork_host_services_init(&tHostServices);
    xwork_tool_result_init(&tResult);

    tHostOptions.sDefaultWorkingDirectory = ".";
    tHostOptions.bEnforceFilesystemRoot = true;
    tHostOptions.iMaxProcessOutputBytes = 65536u;
    iStatus = xwork_local_host_configure_services(&tHost, &tHostOptions, &tHostServices);
    assert(iStatus == XWORK_OK);

    tRuntimeOptions.pHostServices = &tHostServices;
    iStatus = xwork_runtime_create(&tRuntimeOptions, &pRuntime);
    assert(iStatus == XWORK_OK);
    assert(pRuntime != NULL);

#if defined(_WIN32)
    sTerminalRequest =
        "{\"command\":\"for /L %i in (1,1,200) do @echo xwork-terminal-long-output-%i\","
        "\"use_terminal\":true,\"include_events\":true,\"max_output_bytes\":65536}";
    sPipeRequest =
        "{\"command\":\"for /L %i in (1,1,200) do @echo xwork-terminal-long-output-%i\","
        "\"include_events\":true,\"max_output_bytes\":65536}";
#else
    sTerminalRequest =
        "{\"command\":\"for i in $(seq 1 200); do echo xwork-terminal-long-output-$i; done\","
        "\"use_terminal\":true,\"include_events\":true,\"max_output_bytes\":65536}";
    sPipeRequest =
        "{\"command\":\"for i in $(seq 1 200); do echo xwork-terminal-long-output-$i; done\","
        "\"include_events\":true,\"max_output_bytes\":65536}";
#endif

    iStatus = xwork_runtime_invoke_host_service(
        pRuntime,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_EXEC,
        sTerminalRequest,
        &tResult
    );
    assert(iStatus == XWORK_OK);
    assert(tResult.sOutputText != NULL);
    assert(strstr(tResult.sOutputText, "\"ok\":true") != NULL);
    assert(strstr(tResult.sOutputText, "\"use_terminal\":true") != NULL);
    assert(strstr(tResult.sOutputText, "\"terminal_output_captured\":") != NULL);
    assert(strstr(tResult.sOutputText, "\"include_events\":true") != NULL);
    assert(strstr(tResult.sOutputText, "\"events\":[") != NULL);
    if ( strstr(tResult.sOutputText, "\"terminal_output_captured\":true") != NULL ) {
        assert(strstr(tResult.sOutputText, "xwork-terminal-long-output-200") != NULL);
    }
    assert(strstr(tResult.sOutputText, "\"truncated\":true") == NULL);

    iStatus = xwork_runtime_invoke_host_service(
        pRuntime,
        XWORK_HOST_PROCESS,
        XWORK_HOST_PROCESS_EXEC,
        sPipeRequest,
        &tResult
    );
    assert(iStatus == XWORK_OK);
    assert(tResult.sOutputText != NULL);
    assert(strstr(tResult.sOutputText, "\"ok\":true") != NULL);
    assert(strstr(tResult.sOutputText, "\"include_events\":true") != NULL);
    assert(strstr(tResult.sOutputText, "\"events\":[") != NULL);
    assert(strstr(tResult.sOutputText, "xwork-terminal-long-output-200") != NULL);
    assert(strstr(tResult.sOutputText, "\"truncated\":true") == NULL);

    xwork_runtime_destroy(pRuntime);
    xwork_local_host_reset(&tHost);
}

static void xwork_stress_emit_artifacts_for_run(xwork_runtime *pRuntime, unsigned int iRun)
{
    xwork_run *pRun = NULL;
    xwork_run_options tRunOptions;
    xwork_output_artifact_options tArtifactOptions;
    xwork_artifact tArtifact;
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;
    char sRunId[64];
    char sName[96];
    char sStorageRef[96];
    char sSummary[96];
    char sOutputText[160];
    unsigned int iArtifact;

    snprintf(sRunId, sizeof(sRunId), "stress-run-%03u", iRun);

    xwork_run_options_init(&tRunOptions);
    xwork_run_snapshot_init(&tSnapshot);
    tRunOptions.sRunId = sRunId;
    tRunOptions.sInstruction = "stress persistence query";
    tRunOptions.eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;

    iStatus = xwork_run_create(pRuntime, &tRunOptions, &pRun);
    assert(iStatus == XWORK_OK);
    assert(pRun != NULL);

    iStatus = xwork_run_start(pRun);
    assert(iStatus == XWORK_OK);

    for ( iArtifact = 0u; iArtifact < XWORK_STRESS_ARTIFACTS_PER_RUN; ++iArtifact ) {
        snprintf(sName, sizeof(sName), "stress-output-%03u-%03u.json", iRun, iArtifact);
        snprintf(sStorageRef, sizeof(sStorageRef), "stress://run/%03u/artifact/%03u", iRun, iArtifact);
        snprintf(sSummary, sizeof(sSummary), "stress output %03u/%03u", iRun, iArtifact);
        snprintf(
            sOutputText,
            sizeof(sOutputText),
            "{\"schema\":\"xwork.stress.v1\",\"run\":%u,\"artifact\":%u}",
            iRun,
            iArtifact
        );

        xwork_output_artifact_options_init(&tArtifactOptions);
        xwork_artifact_init(&tArtifact);
        tArtifactOptions.sName = sName;
        tArtifactOptions.sMimeType = "application/json";
        tArtifactOptions.sStorageRef = sStorageRef;
        tArtifactOptions.sSummary = sSummary;
        tArtifactOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
        tArtifactOptions.sOutputRole = "stress.output";
        tArtifactOptions.sOutputText = sOutputText;
        iStatus = xwork_run_emit_output_artifact(pRun, &tArtifactOptions, &tArtifact);
        assert(iStatus == XWORK_OK);
        xwork_artifact_reset(&tArtifact);
    }

    iStatus = xwork_run_complete(pRun);
    assert(iStatus == XWORK_OK);
    iStatus = xwork_run_get_snapshot(pRun, &tSnapshot);
    assert(iStatus == XWORK_OK);
    iStatus = xwork__runtime_store_run_snapshot(pRuntime, &tSnapshot);
    assert(iStatus == XWORK_OK);
    xwork_run_snapshot_reset(&tSnapshot);
    xwork_run_destroy(pRun);
}

static void xwork_stress_persistence_many_runs_artifacts(void)
{
    xwork_runtime *pRuntime = NULL;
    xwork_runtime_options tRuntimeOptions;
    xwork_file_persistence tStore;
    xwork_file_persistence_options tStoreOptions;
    xwork_persistence_backend tBackend;
    xwork_run_index_query tRunQuery;
    xwork_run_index_list tRunPage;
    xwork_run_index_list tRunPage2;
    xwork_artifact_summary_query tArtifactQuery;
    xwork_artifact_summary_list tArtifactPage;
    xwork_artifact_summary_list tArtifactPage2;
    xwork_status iStatus;
    unsigned int iRun;

    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_file_persistence_init(&tStore);
    xwork_file_persistence_options_init(&tStoreOptions);
    xwork_persistence_backend_init(&tBackend);
    xwork_run_index_list_init(&tRunPage);
    xwork_run_index_list_init(&tRunPage2);
    xwork_artifact_summary_list_init(&tArtifactPage);
    xwork_artifact_summary_list_init(&tArtifactPage2);

    tStoreOptions.sRootPath = "tests/persistence_store_stress";
    iStatus = xwork_file_persistence_configure_backend(&tStore, &tStoreOptions, &tBackend);
    assert(iStatus == XWORK_OK);

    tRuntimeOptions.pPersistenceBackend = &tBackend;
    iStatus = xwork_runtime_create(&tRuntimeOptions, &pRuntime);
    assert(iStatus == XWORK_OK);
    assert(pRuntime != NULL);

    for ( iRun = 0u; iRun < XWORK_STRESS_RUN_COUNT; ++iRun ) {
        xwork_stress_emit_artifacts_for_run(pRuntime, iRun);
    }

    xwork_run_index_query_init(&tRunQuery);
    tRunQuery.bFilterState = true;
    tRunQuery.eState = XWORK_RUN_COMPLETED;
    tRunQuery.bRequireArtifacts = true;
    tRunQuery.bFilterMinArtifactCount = true;
    tRunQuery.iMinArtifactCount = XWORK_STRESS_ARTIFACTS_PER_RUN;
    tRunQuery.iLimit = 8u;
    tRunQuery.eSort = XWORK_RUN_INDEX_SORT_ARTIFACT_COUNT_DESC;
    iStatus = xwork_file_persistence_query_run_index(&tStore, &tRunQuery, &tRunPage);
    if ( iStatus != XWORK_OK ) {
        fprintf(stderr, "query_run_index status=%d\n", (int)iStatus);
    }
    assert(iStatus == XWORK_OK);
    assert(tRunPage.iCount > 0u);
    assert(tRunPage.iCount <= 8u);
    assert(tRunPage.pItems[0].iArtifactCount >= XWORK_STRESS_ARTIFACTS_PER_RUN);
    assert(tRunPage.bHasMore);
    assert(tRunPage.sNextAfterRunId != NULL);

    tRunQuery.sAfterRunId = tRunPage.sNextAfterRunId;
    iStatus = xwork_file_persistence_query_run_index(&tStore, &tRunQuery, &tRunPage2);
    assert(iStatus == XWORK_OK);
    assert(tRunPage2.iCount > 0u);
    assert(strcmp(tRunPage.pItems[0].tSummary.sRunId, tRunPage2.pItems[0].tSummary.sRunId) != 0);

    xwork_artifact_summary_query_init(&tArtifactQuery);
    tArtifactQuery.bHasKind = true;
    tArtifactQuery.eKind = XWORK_ARTIFACT_OUTPUT;
    tArtifactQuery.bHasOutputClass = true;
    tArtifactQuery.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tArtifactQuery.sOutputRole = "stress.output";
    tArtifactQuery.sNamePrefix = "stress-output-000-";
    tArtifactQuery.iLimit = 5u;
    iStatus = xwork_file_persistence_query_artifact_summaries(
        &tStore,
        "stress-run-000",
        &tArtifactQuery,
        &tArtifactPage
    );
    assert(iStatus == XWORK_OK);
    assert(tArtifactPage.iCount == 5u);
    assert(tArtifactPage.bHasMore);
    assert(tArtifactPage.iNextAfterSequence > 0u);
    assert(tArtifactPage.pItems[0].iSequence < tArtifactPage.iNextAfterSequence);

    tArtifactQuery.bHasAfterSequence = true;
    tArtifactQuery.iAfterSequence = tArtifactPage.iNextAfterSequence;
    iStatus = xwork_file_persistence_query_artifact_summaries(
        &tStore,
        "stress-run-000",
        &tArtifactQuery,
        &tArtifactPage2
    );
    assert(iStatus == XWORK_OK);
    assert(tArtifactPage2.iCount > 0u);
    assert(tArtifactPage2.pItems[0].iSequence > tArtifactPage.iNextAfterSequence);

    xwork_artifact_summary_list_reset(&tArtifactPage2);
    xwork_artifact_summary_list_reset(&tArtifactPage);
    xwork_run_index_list_reset(&tRunPage2);
    xwork_run_index_list_reset(&tRunPage);
    xwork_runtime_destroy(pRuntime);
    xwork_file_persistence_reset(&tStore);
}

int main(void)
{
    xwork_stress_terminal_long_output();
    xwork_stress_persistence_many_runs_artifacts();
    puts("xwork stress smoke passed");
    return 0;
}
