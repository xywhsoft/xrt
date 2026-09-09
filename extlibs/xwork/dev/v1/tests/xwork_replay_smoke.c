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

static void xwork_replay_smoke_write_future_replay(const char *sPath)
{
    FILE *pFile = fopen(sPath, "wb");
    assert(pFile != NULL);
    assert(xwork__file_write_bytes(
        pFile,
        xwork__replay_magic,
        sizeof(xwork__replay_magic)
    ) == XWORK_OK);
    assert(xwork__file_write_u64(
        pFile,
        (uint64_t)XWORK_PERSISTENCE_FORMAT_VERSION + 1u
    ) == XWORK_OK);
    assert(fclose(pFile) == 0);
}

static xwork_status xwork_replay_smoke_host_invoke(
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult,
    void *pUserData
)
{
    int *piInvokeCount = (int *)pUserData;

    assert(sOperationId != NULL);
    assert(strcmp(sOperationId, "filesystem.read_text") == 0);
    assert(sRequestJson != NULL);
    assert(strcmp(sRequestJson, "{\"path\":\"README.md\"}") == 0);
    assert(pResult != NULL);
    assert(piInvokeCount != NULL);
    ++(*piInvokeCount);

    xwork_tool_result_init(pResult);
    pResult->sOutputText = "{\"ok\":true,\"text\":\"recorded host output\"}";
    pResult->sVisibleSummary = "host invoke ok";
    pResult->bRetryable = false;
    return XWORK_OK;
}

int main(void)
{
    xwork_replay_engine *pRecord = NULL;
    xwork_replay_engine *pStrict = NULL;
    xwork_replay_engine *pAudit = NULL;
    xwork_replay_engine *pLoadedReplay = NULL;
    xwork_replay_engine *pHostRecord = NULL;
    xwork_replay_engine *pHostReplay = NULL;
    xwork_replay_options tOptions;
    xwork_replay_entry_options tEntry;
    xwork_replay_entry_summary tActual;
    xwork_replay_entry_summary_list tEntries;
    xwork_replay_filesystem_ref_options tFsRef;
    xwork_replay_filesystem_ref_summary tFsActual;
    xwork_replay_filesystem_ref_summary_list tFsRefs;
    xwork_replay_event_options tReplayEvent;
    xwork_replay_event_summary tActualEvent;
    xwork_replay_event_summary_list tEvents;
    xwork_replay_manifest tManifest;
    xwork_replay_result tResult;
    xwork_replay_divergence tDivergence;
    xwork_model_event tModelEvent;
    xwork_file_persistence tStore;
    xwork_file_persistence_options tStoreOptions;
    xwork_persistence_backend tBackend;
    xwork_string_list tReplayIds;
    xwork_host_services tHostServices;
    xwork_tool_result tHostResult;
    xwork_runtime *pRuntime = NULL;
    xwork_run *pRun = NULL;
    xwork_runtime_options tRuntimeOptions;
    xwork_run_options tRunOptions;
    xwork_artifact tReportArtifact;
    char sStoreRoot[128];
    char *sFutureReplaysDir = NULL;
    char *sFutureReplayPath = NULL;
    char sHashA[17];
    char sHashB[17];
    int iHostInvokeCount = 0;

    xwork_replay_options_init(&tOptions);
    xwork_replay_entry_options_init(&tEntry);
    xwork_replay_entry_summary_init(&tActual);
    xwork_replay_entry_summary_list_init(&tEntries);
    xwork_replay_filesystem_ref_options_init(&tFsRef);
    xwork_replay_filesystem_ref_summary_init(&tFsActual);
    xwork_replay_filesystem_ref_summary_list_init(&tFsRefs);
    xwork_replay_event_options_init(&tReplayEvent);
    xwork_replay_event_summary_init(&tActualEvent);
    xwork_replay_event_summary_list_init(&tEvents);
    xwork_replay_manifest_init(&tManifest);
    xwork_replay_result_init(&tResult);
    xwork_replay_divergence_init(&tDivergence);
    memset(&tModelEvent, 0, sizeof(tModelEvent));
    xwork_file_persistence_init(&tStore);
    xwork_file_persistence_options_init(&tStoreOptions);
    xwork_persistence_backend_init(&tBackend);
    xwork_string_list_init(&tReplayIds);
    xwork_host_services_init(&tHostServices);
    xwork_tool_result_init(&tHostResult);
    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_run_options_init(&tRunOptions);
    xwork_artifact_init(&tReportArtifact);

    assert(xwork_replay_hash_text("{\"a\":1}", sHashA, sizeof(sHashA)) == XWORK_OK);
    assert(xwork_replay_hash_text("{\"a\":1}", sHashB, sizeof(sHashB)) == XWORK_OK);
    assert(strcmp(sHashA, sHashB) == 0);
    assert(xwork_replay_hash_json("{\"b\":2,\"a\":1}", sHashA, sizeof(sHashA)) == XWORK_OK);
    assert(xwork_replay_hash_json("{ \"a\" : 1, \"b\" : 2 }", sHashB, sizeof(sHashB)) == XWORK_OK);
    assert(strcmp(sHashA, sHashB) == 0);
    assert(xwork_replay_hash_json("not-json", sHashA, sizeof(sHashA)) == XWORK_ERROR_INVALID_ARGUMENT);

    tOptions.sReplayId = "replay-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_RECORD;
    tOptions.bBlockSideEffects = false;
    assert(xwork_replay_engine_create(&tOptions, &pRecord) == XWORK_OK);

    tEntry.eKind = XWORK_REPLAY_ENTRY_MODEL;
    tEntry.sKey = "model:1";
    tEntry.sOperationId = "chat";
    tEntry.sRequestJson = "{\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";
    tEntry.sResponseJson = "{\"content\":\"hello\"}";
    assert(xwork_replay_engine_record_entry(pRecord, &tEntry) == XWORK_OK);

    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_CHECKPOINT;
    tEntry.sKey = "checkpoint:1";
    tEntry.sOperationId = "after-model";
    tEntry.sContentHash = "checkpoint-hash-1";
    assert(xwork_replay_engine_record_entry(pRecord, &tEntry) == XWORK_OK);

    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = "tool:1";
    tEntry.sOperationId = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tEntry.sArgumentsJson = "{\"path\":\"README.md\"}";
    tEntry.sResultJson = "{\"ok\":true}";
    assert(xwork_replay_engine_record_entry(pRecord, &tEntry) == XWORK_OK);

    xwork_replay_filesystem_ref_options_init(&tFsRef);
    tFsRef.sRefId = "fsref:readme";
    tFsRef.sPath = "README.md";
    tFsRef.sMetadataJson = "{\"kind\":\"file\",\"size\":1234}";
    tFsRef.sContentHash = "readme-hash-1";
    assert(xwork_replay_engine_record_filesystem_ref(pRecord, &tFsRef) == XWORK_OK);

    tModelEvent.eType = 1001;
    tModelEvent.sResponseId = "response:1";
    tModelEvent.sText = "hello";
    xwork_replay_event_options_from_model_event(&tModelEvent, &tReplayEvent);
    assert(tReplayEvent.eKind == XWORK_REPLAY_EVENT_MODEL_STREAM);
    assert(strcmp(tReplayEvent.sKey, "response:1") == 0);
    assert(xwork_replay_engine_record_event(pRecord, &tReplayEvent) == XWORK_OK);
    xwork_replay_event_options_init(&tReplayEvent);
    tReplayEvent.eKind = XWORK_REPLAY_EVENT_TERMINAL_INTERACTION;
    tReplayEvent.sKey = "terminal:1";
    tReplayEvent.sName = XWORK_TOOL_PROCESS_TERMINAL_READ;
    tReplayEvent.iType = 1;
    tReplayEvent.sPayloadJson = "{\"session_id\":\"terminal:1\",\"offset\":0}";
    tReplayEvent.sContentText = "prompt>";
    assert(xwork_replay_engine_record_event(pRecord, &tReplayEvent) == XWORK_OK);

    assert(xwork_replay_engine_get_manifest(pRecord, &tManifest) == XWORK_OK);
    assert(strcmp(tManifest.sReplayId, "replay-smoke") == 0);
    assert(tManifest.iEntryCount == 4u);
    assert(strcmp(tManifest.sContentHashAlgorithm, "fnv1a64") == 0);

    assert(xwork_replay_engine_list_entries(pRecord, &tEntries) == XWORK_OK);
    assert(tEntries.iCount == 4u);
    assert(tEntries.pItems[0].iSequence == 1u);
    assert(tEntries.pItems[0].eKind == XWORK_REPLAY_ENTRY_MODEL);
    assert(tEntries.pItems[0].sRequestHash != NULL);
    assert(xwork_replay_hash_json(
        "{ \"messages\" : [ { \"content\" : \"hi\", \"role\" : \"user\" } ] }",
        sHashA,
        sizeof(sHashA)
    ) == XWORK_OK);
    assert(strcmp(tEntries.pItems[0].sRequestHash, sHashA) == 0);
    assert(tEntries.pItems[1].eKind == XWORK_REPLAY_ENTRY_CHECKPOINT);
    assert(strcmp(tEntries.pItems[1].sContentHash, "checkpoint-hash-1") == 0);
    assert(tEntries.pItems[2].eKind == XWORK_REPLAY_ENTRY_TOOL);
    assert(tEntries.pItems[2].sArgumentsHash != NULL);
    assert(tEntries.pItems[3].eKind == XWORK_REPLAY_ENTRY_FILESYSTEM);
    assert(strcmp(tEntries.pItems[3].sOperationId, XWORK_REPLAY_FILESYSTEM_SNAPSHOT_REF) == 0);
    assert(strcmp(tEntries.pItems[3].sKey, "fsref:readme") == 0);
    assert(strcmp(tEntries.pItems[3].sRequestJson, "README.md") == 0);
    assert(strcmp(tEntries.pItems[3].sContentHash, "readme-hash-1") == 0);
    assert(xwork_replay_engine_list_filesystem_refs(pRecord, &tFsRefs) == XWORK_OK);
    assert(tFsRefs.iCount == 1u);
    assert(strcmp(tFsRefs.pItems[0].sRefId, "fsref:readme") == 0);
    assert(strcmp(tFsRefs.pItems[0].sPath, "README.md") == 0);
    assert(strcmp(tFsRefs.pItems[0].sMetadataJson, "{\"kind\":\"file\",\"size\":1234}") == 0);
    assert(strcmp(tFsRefs.pItems[0].sContentHash, "readme-hash-1") == 0);
    assert(xwork_replay_engine_list_events(pRecord, &tEvents) == XWORK_OK);
    assert(tEvents.iCount == 2u);
    assert(tEvents.pItems[0].eKind == XWORK_REPLAY_EVENT_MODEL_STREAM);
    assert(tEvents.pItems[0].sContentHash != NULL);
    assert(tEvents.pItems[1].eKind == XWORK_REPLAY_EVENT_TERMINAL_INTERACTION);
    assert(tEvents.pItems[1].sPayloadHash != NULL);

    tOptions.sReplayId = "replay-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_STRICT;
    assert(xwork_replay_engine_create(&tOptions, &pStrict) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_MODEL;
    tEntry.sKey = "model:1";
    tEntry.sOperationId = "chat";
    tEntry.sRequestJson = "{\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";
    tEntry.sResponseJson = "{\"content\":\"hello\"}";
    assert(xwork_replay_engine_record_entry(pStrict, &tEntry) == XWORK_ERROR_INVALID_STATE);
    assert(xwork_replay_engine_load_entry(pStrict, &tEntry) == XWORK_OK);
    assert(xwork_replay_engine_replay_entry(
        pStrict,
        &tEntry,
        &tActual
    ) == XWORK_OK);
    assert(tActual.eKind == XWORK_REPLAY_ENTRY_MODEL);
    assert(strcmp(tActual.sKey, "model:1") == 0);
    assert(xwork_replay_engine_get_result(pStrict, &tResult) == XWORK_OK);
    assert(tResult.iStatus == XWORK_OK);
    assert(tResult.iReplayedCount == 1u);
    xwork_replay_result_reset(&tResult);
    xwork_replay_entry_summary_reset(&tActual);
    xwork_replay_engine_destroy(pStrict);
    pStrict = NULL;

    tOptions.sReplayId = "replay-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_STRICT;
    assert(xwork_replay_engine_create(&tOptions, &pStrict) == XWORK_OK);
    assert(xwork_replay_engine_load_filesystem_ref(pStrict, &tFsRef) == XWORK_OK);
    assert(xwork_replay_engine_replay_filesystem_ref(
        pStrict,
        &tFsRef,
        &tFsActual
    ) == XWORK_OK);
    assert(strcmp(tFsActual.sRefId, "fsref:readme") == 0);
    assert(strcmp(tFsActual.sPath, "README.md") == 0);
    assert(strcmp(tFsActual.sContentHash, "readme-hash-1") == 0);
    xwork_replay_filesystem_ref_summary_reset(&tFsActual);
    xwork_replay_engine_destroy(pStrict);
    pStrict = NULL;

    tOptions.sReplayId = "replay-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_STRICT;
    assert(xwork_replay_engine_create(&tOptions, &pStrict) == XWORK_OK);
    assert(xwork_replay_engine_list_entries(pRecord, &tEntries) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_MODEL;
    tEntry.sKey = tEntries.pItems[0].sKey;
    tEntry.sOperationId = tEntries.pItems[0].sOperationId;
    tEntry.sRequestHash = tEntries.pItems[0].sRequestHash;
    tEntry.sResponseHash = tEntries.pItems[0].sResponseHash;
    assert(xwork_replay_engine_load_entry(pStrict, &tEntry) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_CHECKPOINT;
    tEntry.sKey = tEntries.pItems[1].sKey;
    tEntry.sOperationId = tEntries.pItems[1].sOperationId;
    tEntry.sContentHash = tEntries.pItems[1].sContentHash;
    assert(xwork_replay_engine_load_entry(pStrict, &tEntry) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = tEntries.pItems[2].sKey;
    tEntry.sOperationId = tEntries.pItems[2].sOperationId;
    tEntry.sArgumentsHash = tEntries.pItems[2].sArgumentsHash;
    tEntry.sResultHash = tEntries.pItems[2].sResultHash;
    assert(xwork_replay_engine_load_entry(pStrict, &tEntry) == XWORK_OK);
    assert(xwork_replay_engine_seek_checkpoint(
        pStrict,
        "missing-checkpoint"
    ) == XWORK_ERROR_NOT_FOUND);
    assert(xwork_replay_engine_seek_checkpoint(
        pStrict,
        "checkpoint:1"
    ) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = "tool:1";
    tEntry.sOperationId = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tEntry.sArgumentsJson = "{\"path\":\"README.md\"}";
    tEntry.sResultJson = "{\"ok\":true}";
    assert(xwork_replay_engine_replay_entry(
        pStrict,
        &tEntry,
        &tActual
    ) == XWORK_OK);
    assert(strcmp(tActual.sKey, "tool:1") == 0);
    xwork_replay_entry_summary_reset(&tActual);

    xwork_replay_event_options_init(&tReplayEvent);
    tReplayEvent.eKind = XWORK_REPLAY_EVENT_MODEL_STREAM;
    tReplayEvent.sKey = tEvents.pItems[0].sKey;
    tReplayEvent.sName = tEvents.pItems[0].sName;
    tReplayEvent.iType = tEvents.pItems[0].iType;
    tReplayEvent.sContentHash = tEvents.pItems[0].sContentHash;
    assert(xwork_replay_engine_load_event(pStrict, &tReplayEvent) == XWORK_OK);
    assert(xwork_replay_engine_replay_event(
        pStrict,
        &tReplayEvent,
        &tActualEvent
    ) == XWORK_OK);
    assert(tActualEvent.eKind == XWORK_REPLAY_EVENT_MODEL_STREAM);
    assert(strcmp(tActualEvent.sKey, "response:1") == 0);
    xwork_replay_event_summary_reset(&tActualEvent);
    xwork_replay_entry_summary_list_reset(&tEntries);

    tOptions.sReplayId = "replay-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_RECORD;
    tOptions.bBlockSideEffects = true;
    assert(xwork_replay_engine_create(&tOptions, &pAudit) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_PROCESS;
    tEntry.sKey = "process:blocked";
    tEntry.sArgumentsJson = "{\"command\":\"echo blocked\"}";
    assert(xwork_replay_engine_record_entry(pAudit, &tEntry) == XWORK_ERROR_PAUSED);
    assert(xwork_replay_engine_cancel(pAudit, "stop replay") == XWORK_OK);
    assert(xwork_replay_engine_record_entry(pAudit, &tEntry) == XWORK_ERROR_CANCELLED);
    assert(xwork_replay_engine_get_result(pAudit, &tResult) == XWORK_OK);
    assert(tResult.iStatus == XWORK_ERROR_CANCELLED);
    xwork_replay_engine_destroy(pAudit);
    pAudit = NULL;

    tOptions.sReplayId = "replay-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_AUDIT;
    tOptions.bBlockSideEffects = false;
    assert(xwork_replay_engine_create(&tOptions, &pAudit) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = "tool:1";
    tEntry.sOperationId = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tEntry.sArgumentsJson = "{\"path\":\"README.md\"}";
    tEntry.sResultJson = "{\"ok\":true}";
    assert(xwork_replay_engine_load_entry(pAudit, &tEntry) == XWORK_OK);
    tEntry.sArgumentsJson = "{\"path\":\"DEVELOPMENT_SPEC.md\"}";
    assert(xwork_replay_engine_replay_entry(
        pAudit,
        &tEntry,
        &tActual
    ) == XWORK_OK);
    assert(xwork_replay_engine_get_first_divergence(
        pAudit,
        &tDivergence
    ) == XWORK_OK);
    assert(tDivergence.eKind == XWORK_REPLAY_DIVERGENCE_REQUEST_MISMATCH);
    assert(strcmp(tDivergence.sExpectedKey, "tool:1") == 0);
    assert(strcmp(tDivergence.sActualKey, "tool:1") == 0);
    assert(xwork_replay_engine_get_result(pAudit, &tResult) == XWORK_OK);
    assert(tResult.bDiverged);
    assert(tResult.iDivergenceCount == 1u);
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);
    tRunOptions.sRunId = "replay-report-run";
    tRunOptions.sInstruction = "replay report smoke";
    assert(xwork_run_create(pRuntime, &tRunOptions, &pRun) == XWORK_OK);
    assert(xwork_run_start(pRun) == XWORK_OK);
    assert(xwork_replay_engine_emit_report_artifact(
        pAudit,
        pRun,
        "replay-divergence-report",
        &tReportArtifact
    ) == XWORK_OK);
    assert(xwork_run_get_artifact_count(pRun) == 1u);
    assert(strcmp(tReportArtifact.sArtifactId, "replay-divergence-report") == 0);
    assert(tReportArtifact.eKind == XWORK_ARTIFACT_REPORT);
    assert(tReportArtifact.eOutputClass == XWORK_ARTIFACT_OUTPUT_TEXT);
    assert(tReportArtifact.eReportClass == XWORK_ARTIFACT_REPORT_DIAGNOSTICS);
    assert(strcmp(tReportArtifact.sReportSubjectRef, "replay-smoke") == 0);
    assert(tReportArtifact.sContentText != NULL);
    assert(strstr(tReportArtifact.sContentText, "request_mismatch") != NULL);
    assert(strstr(tReportArtifact.sContentText, "tool:1") != NULL);
    xwork_artifact_reset(&tReportArtifact);
    xwork_run_destroy(pRun);
    pRun = NULL;
    xwork_runtime_destroy(pRuntime);
    pRuntime = NULL;
    xwork_replay_engine_destroy(pAudit);
    pAudit = NULL;
    xwork_replay_divergence_reset(&tDivergence);
    xwork_replay_result_reset(&tResult);
    xwork_replay_entry_summary_reset(&tActual);

    tOptions.eMode = XWORK_REPLAY_MODE_AUDIT;
    assert(xwork_replay_engine_create(&tOptions, &pAudit) == XWORK_OK);
    xwork_replay_event_options_init(&tReplayEvent);
    tReplayEvent.eKind = XWORK_REPLAY_EVENT_TERMINAL_INTERACTION;
    tReplayEvent.sKey = "terminal:1";
    tReplayEvent.sName = XWORK_TOOL_PROCESS_TERMINAL_READ;
    tReplayEvent.iType = 1;
    tReplayEvent.sPayloadJson = "{\"session_id\":\"terminal:1\",\"offset\":0}";
    tReplayEvent.sContentText = "prompt>";
    assert(xwork_replay_engine_load_event(pAudit, &tReplayEvent) == XWORK_OK);
    tReplayEvent.sContentText = "changed>";
    assert(xwork_replay_engine_replay_event(
        pAudit,
        &tReplayEvent,
        &tActualEvent
    ) == XWORK_OK);
    assert(xwork_replay_engine_get_first_divergence(
        pAudit,
        &tDivergence
    ) == XWORK_OK);
    assert(tDivergence.eKind == XWORK_REPLAY_DIVERGENCE_CONTENT_MISMATCH);
    xwork_replay_engine_destroy(pAudit);
    pAudit = NULL;
    xwork_replay_divergence_reset(&tDivergence);
    xwork_replay_event_summary_reset(&tActualEvent);

    assert(xwork_replay_engine_get_result(pRecord, &tResult) == XWORK_OK);
    assert(tResult.iStatus == XWORK_OK);
    assert(tResult.iRecordedCount == 4u);
    assert(tResult.iDivergenceCount == 0u);
    assert(xwork_replay_engine_get_first_divergence(
        pRecord,
        &tDivergence
    ) == XWORK_ERROR_NOT_FOUND);

    tOptions.sReplayId = "host-auto-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_RECORD;
    tOptions.bBlockSideEffects = false;
    assert(xwork_replay_engine_create(&tOptions, &pHostRecord) == XWORK_OK);
    tHostServices.tFilesystem.pfnInvoke = xwork_replay_smoke_host_invoke;
    tHostServices.tFilesystem.pUserData = &iHostInvokeCount;
    xwork_runtime_options_init(&tRuntimeOptions);
    tRuntimeOptions.pReplayEngine = pHostRecord;
    tRuntimeOptions.pHostServices = &tHostServices;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);
    assert(xwork_runtime_invoke_host_service(
        pRuntime,
        XWORK_HOST_FILESYSTEM,
        "filesystem.read_text",
        "{\"path\":\"README.md\"}",
        &tHostResult
    ) == XWORK_OK);
    assert(iHostInvokeCount == 1);
    assert(strcmp(tHostResult.sOutputText, "{\"ok\":true,\"text\":\"recorded host output\"}") == 0);
    xwork_runtime_destroy(pRuntime);
    pRuntime = NULL;
    xwork_replay_entry_summary_list_reset(&tEntries);
    assert(xwork_replay_engine_list_entries(pHostRecord, &tEntries) == XWORK_OK);
    assert(tEntries.iCount == 1u);
    assert(tEntries.pItems[0].iStatus == XWORK_OK);
    assert(tEntries.pItems[0].eKind == XWORK_REPLAY_ENTRY_FILESYSTEM);
    assert(strcmp(tEntries.pItems[0].sArgumentsJson, "{\"path\":\"README.md\"}") == 0);
    assert(strcmp(tEntries.pItems[0].sResultJson, "{\"ok\":true,\"text\":\"recorded host output\"}") == 0);
    xwork_replay_entry_summary_list_reset(&tEntries);

    snprintf(
        sStoreRoot,
        sizeof(sStoreRoot),
        "tests/replay_store_%ld_%ld",
        (long)time(NULL),
        (long)clock()
    );
    tStoreOptions.sRootPath = sStoreRoot;
    assert(xwork_file_persistence_configure_backend(
        &tStore,
        &tStoreOptions,
        &tBackend
    ) == XWORK_OK);
    assert(xwork_file_persistence_store_replay(&tStore, pRecord) == XWORK_OK);
    assert(xwork_file_persistence_store_replay(&tStore, pHostRecord) == XWORK_OK);
    assert(xwork_file_persistence_list_replays(&tStore, &tReplayIds) == XWORK_OK);
    assert(tReplayIds.iCount == 2u);

    sFutureReplaysDir = xwork__dup_printf("%s/replays", sStoreRoot);
    assert(sFutureReplaysDir != NULL);
    assert(xwork__ensure_directory_tree(sFutureReplaysDir) == XWORK_OK);
    sFutureReplayPath = xwork__build_replay_file_path(
        sFutureReplaysDir,
        "replay-newer"
    );
    assert(sFutureReplayPath != NULL);
    xwork_replay_smoke_write_future_replay(sFutureReplayPath);
    xwork_replay_manifest_reset(&tManifest);
    assert(xwork_file_persistence_load_replay_manifest(
        &tStore,
        "replay-newer",
        &tManifest
    ) == XWORK_ERROR_UNSUPPORTED);
    free(sFutureReplayPath);
    sFutureReplayPath = NULL;
    free(sFutureReplaysDir);
    sFutureReplaysDir = NULL;

    xwork_replay_manifest_reset(&tManifest);
    assert(xwork_file_persistence_load_replay_manifest(
        &tStore,
        "replay-smoke",
        &tManifest
    ) == XWORK_OK);
    assert(strcmp(tManifest.sReplayId, "replay-smoke") == 0);
    assert(tManifest.iEntryCount == 4u);

    xwork_replay_entry_summary_list_reset(&tEntries);
    assert(xwork_file_persistence_load_replay_entries(
        &tStore,
        "replay-smoke",
        &tEntries
    ) == XWORK_OK);
    assert(tEntries.iCount == 4u);
    assert(tEntries.pItems[0].sRequestHash != NULL);
    assert(tEntries.pItems[1].eKind == XWORK_REPLAY_ENTRY_CHECKPOINT);
    assert(tEntries.pItems[2].sArgumentsHash != NULL);
    assert(strcmp(tEntries.pItems[3].sOperationId, XWORK_REPLAY_FILESYSTEM_SNAPSHOT_REF) == 0);
    assert(strcmp(tEntries.pItems[3].sRequestJson, "README.md") == 0);
    assert(strcmp(tEntries.pItems[3].sContentHash, "readme-hash-1") == 0);

    xwork_replay_result_reset(&tResult);
    assert(xwork_file_persistence_load_replay_result(
        &tStore,
        "replay-smoke",
        &tResult
    ) == XWORK_OK);
    assert(tResult.iStatus == XWORK_OK);
    assert(tResult.iRecordedCount == 4u);

    tOptions.eMode = XWORK_REPLAY_MODE_STRICT;
    assert(xwork_file_persistence_load_replay_engine(
        &tStore,
        "replay-smoke",
        &tOptions,
        &pLoadedReplay
    ) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_MODEL;
    tEntry.sKey = "model:1";
    tEntry.sOperationId = "chat";
    tEntry.sRequestJson = "{\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";
    tEntry.sResponseJson = "{\"content\":\"hello\"}";
    assert(xwork_replay_engine_replay_entry(
        pLoadedReplay,
        &tEntry,
        &tActual
    ) == XWORK_OK);
    xwork_replay_entry_summary_reset(&tActual);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_CHECKPOINT;
    tEntry.sKey = "checkpoint:1";
    tEntry.sOperationId = "after-model";
    tEntry.sContentHash = "checkpoint-hash-1";
    assert(xwork_replay_engine_replay_entry(
        pLoadedReplay,
        &tEntry,
        &tActual
    ) == XWORK_OK);
    xwork_replay_entry_summary_reset(&tActual);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = "tool:1";
    tEntry.sOperationId = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tEntry.sArgumentsJson = "{\"path\":\"README.md\"}";
    tEntry.sResultJson = "{\"ok\":true}";
    assert(xwork_replay_engine_replay_entry(
        pLoadedReplay,
        &tEntry,
        &tActual
    ) == XWORK_OK);
    xwork_replay_filesystem_ref_summary_list_reset(&tFsRefs);
    assert(xwork_replay_engine_list_filesystem_refs(pLoadedReplay, &tFsRefs) == XWORK_OK);
    assert(tFsRefs.iCount == 1u);
    assert(strcmp(tFsRefs.pItems[0].sRefId, "fsref:readme") == 0);
    xwork_replay_result_reset(&tResult);
    assert(xwork_replay_engine_get_result(pLoadedReplay, &tResult) == XWORK_OK);
    assert(tResult.iReplayedCount == 3u);
    xwork_replay_engine_destroy(pLoadedReplay);
    pLoadedReplay = NULL;

    assert(xwork_file_persistence_load_replay_engine(
        &tStore,
        "replay-smoke",
        &tOptions,
        &pLoadedReplay
    ) == XWORK_OK);
    assert(xwork_replay_engine_seek_checkpoint(
        pLoadedReplay,
        "checkpoint:1"
    ) == XWORK_OK);
    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = "tool:1";
    tEntry.sOperationId = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tEntry.sArgumentsJson = "{\"path\":\"README.md\"}";
    tEntry.sResultJson = "{\"ok\":true}";
    assert(xwork_replay_engine_replay_entry(
        pLoadedReplay,
        &tEntry,
        &tActual
    ) == XWORK_OK);
    xwork_replay_result_reset(&tResult);
    assert(xwork_replay_engine_get_result(pLoadedReplay, &tResult) == XWORK_OK);
    assert(tResult.iReplayedCount == 1u);

    tOptions.sReplayId = "host-auto-smoke";
    tOptions.eMode = XWORK_REPLAY_MODE_STRICT;
    assert(xwork_file_persistence_load_replay_engine(
        &tStore,
        "host-auto-smoke",
        &tOptions,
        &pHostReplay
    ) == XWORK_OK);
    xwork_replay_entry_summary_list_reset(&tEntries);
    assert(xwork_replay_engine_list_entries(pHostReplay, &tEntries) == XWORK_OK);
    assert(tEntries.iCount == 1u);
    assert(tEntries.pItems[0].sResultJson != NULL);
    assert(strcmp(tEntries.pItems[0].sResultJson, "{\"ok\":true,\"text\":\"recorded host output\"}") == 0);
    xwork_replay_entry_summary_list_reset(&tEntries);
    xwork_runtime_options_init(&tRuntimeOptions);
    tRuntimeOptions.pReplayEngine = pHostReplay;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);
    xwork_tool_result_init(&tHostResult);
    assert(xwork_runtime_invoke_host_service(
        pRuntime,
        XWORK_HOST_FILESYSTEM,
        "filesystem.read_text",
        "{\"path\":\"README.md\"}",
        &tHostResult
    ) == XWORK_OK);
    assert(iHostInvokeCount == 1);
    assert(strcmp(tHostResult.sOutputText, "{\"ok\":true,\"text\":\"recorded host output\"}") == 0);
    assert(tHostResult.sVisibleSummary != NULL);
    xwork_runtime_destroy(pRuntime);
    pRuntime = NULL;
    xwork_replay_engine_destroy(pHostReplay);
    pHostReplay = NULL;

    xwork_replay_divergence_reset(&tDivergence);
    xwork_replay_result_reset(&tResult);
    xwork_replay_manifest_reset(&tManifest);
    xwork_replay_entry_summary_list_reset(&tEntries);
    xwork_replay_filesystem_ref_summary_reset(&tFsActual);
    xwork_replay_filesystem_ref_summary_list_reset(&tFsRefs);
    xwork_replay_event_summary_list_reset(&tEvents);
    xwork_replay_entry_summary_reset(&tActual);
    xwork_replay_event_summary_reset(&tActualEvent);
    xwork_string_list_reset(&tReplayIds);
    xwork_file_persistence_reset(&tStore);
    free(sFutureReplayPath);
    free(sFutureReplaysDir);
    xwork_artifact_reset(&tReportArtifact);
    xwork_run_destroy(pRun);
    xwork_runtime_destroy(pRuntime);
    xwork_replay_engine_destroy(pHostReplay);
    xwork_replay_engine_destroy(pHostRecord);
    xwork_replay_engine_destroy(pLoadedReplay);
    xwork_replay_engine_destroy(pStrict);
    xwork_replay_engine_destroy(pAudit);
    xwork_replay_engine_destroy(pRecord);

    printf("xwork replay smoke passed\n");
    return 0;
}
