#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"
#include "../xwork.c"

static void xwork_persistence_smoke_write_future_snapshot(const char *sPath)
{
    FILE *pFile = fopen(sPath, "wb");
    assert(pFile != NULL);
    assert(
        xwork__file_write_bytes(
            pFile,
            xwork__snapshot_magic,
            sizeof(xwork__snapshot_magic)
        ) == XWORK_OK
    );
    assert(
        xwork__file_write_u64(
            pFile,
            (uint64_t)XWORK_PERSISTENCE_FORMAT_VERSION + 1u
        ) == XWORK_OK
    );
    assert(fclose(pFile) == 0);
}

int main(void)
{
    xwork_runtime *pRuntime = NULL;
    xwork_run *pRun = NULL;
    xwork_file_persistence tStore;
    xwork_file_persistence_options tStoreOptions;
    xwork_persistence_backend tBackend;
    xwork_runtime_options tRuntimeOptions;
    xwork_run_options tRunOptions;
    xwork_output_artifact_options tOutputOptions;
    xwork_artifact tArtifact;
    xwork_run_snapshot tSnapshot;
    xwork_run_summary tSummary;
    xwork_event tEvent;
    xwork_artifact tLoadedArtifact;
    xwork_run_index_query tRunQuery;
    xwork_run_index_list tRunIndex;
    xwork_artifact_summary_list tArtifactSummaries;
    xwork_status iStatus;
    char *sFutureRunDir = NULL;
    char *sFutureSnapshotPath = NULL;
    char sStoreRoot[128];

    assert(XWORK_PERSISTENCE_FORMAT_VERSION == 14u);
    xwork_file_persistence_init(&tStore);
    xwork_file_persistence_options_init(&tStoreOptions);
    xwork_persistence_backend_init(&tBackend);
    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_run_options_init(&tRunOptions);
    xwork_output_artifact_options_init(&tOutputOptions);
    xwork_artifact_init(&tArtifact);
    xwork_run_snapshot_init(&tSnapshot);
    xwork_run_summary_init(&tSummary);
    xwork_event_init(&tEvent);
    xwork_artifact_init(&tLoadedArtifact);
    xwork_run_index_list_init(&tRunIndex);
    xwork_artifact_summary_list_init(&tArtifactSummaries);

    snprintf(
        sStoreRoot,
        sizeof(sStoreRoot),
        "tests/persistence_store_split_%ld_%ld",
        (long)time(NULL),
        (long)clock()
    );
    tStoreOptions.sRootPath = sStoreRoot;
    assert(xwork_file_persistence_configure_backend(&tStore, &tStoreOptions, &tBackend) == XWORK_OK);

    tRuntimeOptions.pPersistenceBackend = &tBackend;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);

    tRunOptions.sRunId = "persistence-split-run";
    tRunOptions.sParentRunId = "persistence-parent";
    tRunOptions.sAgentId = "persistence-agent";
    tRunOptions.sTaskId = "persistence-task";
    tRunOptions.sInstruction = "persistence split smoke";
    tRunOptions.eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
    assert(xwork_run_create(pRuntime, &tRunOptions, &pRun) == XWORK_OK);
    assert(xwork_run_start(pRun) == XWORK_OK);

    tOutputOptions.sName = "persistence-output.json";
    tOutputOptions.sMimeType = "application/json";
    tOutputOptions.sStorageRef = "memory://persistence-output";
    tOutputOptions.sSummary = "persistence output";
    tOutputOptions.eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON;
    tOutputOptions.sOutputRole = "persistence.output";
    tOutputOptions.sOutputText = "{\"ok\":true,\"source\":\"persistence-smoke\"}";
    assert(xwork_run_emit_output_artifact(pRun, &tOutputOptions, &tArtifact) == XWORK_OK);
    assert(tArtifact.sArtifactId != NULL);

    assert(xwork_run_complete(pRun) == XWORK_OK);
    assert(xwork_run_get_snapshot(pRun, &tSnapshot) == XWORK_OK);
    assert(xwork__runtime_store_run_snapshot(pRuntime, &tSnapshot) == XWORK_OK);

    assert(
        xwork_file_persistence_load_run_summary(
            &tStore,
            "persistence-split-run",
            &tSummary
        ) == XWORK_OK
    );
    assert(tSummary.eState == XWORK_RUN_COMPLETED);
    assert(strcmp(tSummary.sParentRunId, "persistence-parent") == 0);
    assert(strcmp(tSummary.sAgentId, "persistence-agent") == 0);
    assert(strcmp(tSummary.sTaskId, "persistence-task") == 0);
    assert(strcmp(tSummary.sInstruction, "persistence split smoke") == 0);

    xwork_run_snapshot_reset(&tSnapshot);
    assert(
        xwork_file_persistence_load_run_snapshot(
            &tStore,
            "persistence-split-run",
            &tSnapshot
        ) == XWORK_OK
    );
    assert(tSnapshot.eState == XWORK_RUN_COMPLETED);
    assert(strcmp(tSnapshot.sAgentId, "persistence-agent") == 0);
    assert(strcmp(tSnapshot.sTaskId, "persistence-task") == 0);
    assert(tSnapshot.iArtifactCount >= 1u);

    assert(
        xwork_file_persistence_load_last_event(
            &tStore,
            "persistence-split-run",
            &tEvent
        ) == XWORK_OK
    );
    assert(tEvent.eKind == XWORK_EVENT_RUN_COMPLETED);

    assert(
        xwork_file_persistence_load_last_artifact(
            &tStore,
            "persistence-split-run",
            &tLoadedArtifact
        ) == XWORK_OK
    );
    assert(strcmp(tLoadedArtifact.sName, "persistence-output.json") == 0);
    assert(strstr(tLoadedArtifact.sContentText, "persistence-smoke") != NULL);

    xwork_run_index_query_init(&tRunQuery);
    tRunQuery.bFilterState = true;
    tRunQuery.eState = XWORK_RUN_COMPLETED;
    tRunQuery.sParentRunId = "persistence-parent";
    tRunQuery.sAgentId = "persistence-agent";
    tRunQuery.sTaskId = "persistence-task";
    tRunQuery.bRequireArtifacts = true;
    iStatus = xwork_file_persistence_query_run_index(&tStore, &tRunQuery, &tRunIndex);
    assert(iStatus == XWORK_OK);
    assert(tRunIndex.iCount >= 1u);

    assert(
        xwork_file_persistence_list_artifact_summaries(
            &tStore,
            "persistence-split-run",
            &tArtifactSummaries
        ) == XWORK_OK
    );
    assert(tArtifactSummaries.iCount >= 1u);
    assert(strcmp(tArtifactSummaries.pItems[0].sName, "persistence-output.json") == 0);

    sFutureRunDir = xwork__build_run_dir_path(&tStore, "persistence-split-newer");
    assert(sFutureRunDir != NULL);
    assert(xwork__ensure_directory_tree(sFutureRunDir) == XWORK_OK);
    sFutureSnapshotPath = xwork__dup_printf("%s/latest.snapshot", sFutureRunDir);
    assert(sFutureSnapshotPath != NULL);
    xwork_persistence_smoke_write_future_snapshot(sFutureSnapshotPath);
    xwork_run_snapshot_reset(&tSnapshot);
    assert(
        xwork_file_persistence_load_run_snapshot(
            &tStore,
            "persistence-split-newer",
            &tSnapshot
        ) == XWORK_ERROR_UNSUPPORTED
    );

    free(sFutureSnapshotPath);
    free(sFutureRunDir);
    xwork_artifact_summary_list_reset(&tArtifactSummaries);
    xwork_run_index_list_reset(&tRunIndex);
    xwork_artifact_reset(&tLoadedArtifact);
    xwork_event_reset(&tEvent);
    xwork_run_summary_reset(&tSummary);
    xwork_run_snapshot_reset(&tSnapshot);
    xwork_artifact_reset(&tArtifact);
    xwork_run_destroy(pRun);
    xwork_runtime_destroy(pRuntime);
    xwork_file_persistence_reset(&tStore);
    puts("xwork persistence smoke passed");
    return 0;
}
