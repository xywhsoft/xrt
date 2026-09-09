#include "../xwork.h"

#include <stdio.h>
#include <string.h>

static int xwork_example_check(xwork_status iStatus, const char *sStep)
{
    if ( iStatus == XWORK_OK ) {
        return 0;
    }
    fprintf(stderr, "%s failed: %s\n", sStep, xwork_status_cstr(iStatus));
    return 1;
}

static int load_entry_from_summary(
    xwork_replay_engine *pReplay,
    const xwork_replay_entry_summary *pSummary
)
{
    xwork_replay_entry_options tEntry;

    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = pSummary->eKind;
    tEntry.sKey = pSummary->sKey;
    tEntry.sOperationId = pSummary->sOperationId;
    tEntry.sRequestHash = pSummary->sRequestHash;
    tEntry.sResponseHash = pSummary->sResponseHash;
    tEntry.sArgumentsHash = pSummary->sArgumentsHash;
    tEntry.sResultHash = pSummary->sResultHash;
    tEntry.sContentHash = pSummary->sContentHash;
    tEntry.iStatus = pSummary->iStatus;
    return xwork_example_check(
        xwork_replay_engine_load_entry(pReplay, &tEntry),
        "load replay entry"
    );
}

int main(void)
{
    xwork_runtime_options tRuntimeOptions;
    xwork_run_options tRunOptions;
    xwork_replay_options tReplayOptions;
    xwork_replay_entry_options tEntry;
    xwork_replay_entry_summary tActual;
    xwork_replay_entry_summary_list tEntries;
    xwork_replay_result tResult;
    xwork_artifact tReportArtifact;
    xwork_runtime *pRuntime = NULL;
    xwork_run *pRun = NULL;
    xwork_replay_engine *pRecord = NULL;
    xwork_replay_engine *pStrict = NULL;
    xwork_replay_engine *pAudit = NULL;
    int iExit = 1;

    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_run_options_init(&tRunOptions);
    xwork_replay_options_init(&tReplayOptions);
    xwork_replay_entry_options_init(&tEntry);
    xwork_replay_entry_summary_init(&tActual);
    xwork_replay_entry_summary_list_init(&tEntries);
    xwork_replay_result_init(&tResult);
    xwork_artifact_init(&tReportArtifact);

    if ( xwork_example_check(
             xwork_runtime_create(&tRuntimeOptions, &pRuntime),
             "create runtime"
         ) ) goto cleanup;

    tRunOptions.sRunId = "replay-example-run";
    tRunOptions.sInstruction = "demonstrate deterministic replay";
    if ( xwork_example_check(
             xwork_run_create(pRuntime, &tRunOptions, &pRun),
             "create run"
         ) ) goto cleanup;
    if ( xwork_example_check(xwork_run_start(pRun), "start run") ) goto cleanup;

    tReplayOptions.sReplayId = "replay-example";
    tReplayOptions.eMode = XWORK_REPLAY_MODE_RECORD;
    tReplayOptions.bBlockSideEffects = false;
    if ( xwork_example_check(
             xwork_replay_engine_create(&tReplayOptions, &pRecord),
             "create recording replay"
         ) ) goto cleanup;

    tEntry.eKind = XWORK_REPLAY_ENTRY_MODEL;
    tEntry.sKey = "model:plan";
    tEntry.sOperationId = "chat";
    tEntry.sRequestJson = "{\"messages\":[{\"role\":\"user\",\"content\":\"inspect README\"}]}";
    tEntry.sResponseJson = "{\"tool_call\":\"filesystem.read_text\"}";
    if ( xwork_example_check(
             xwork_replay_engine_record_entry(pRecord, &tEntry),
             "record model entry"
         ) ) goto cleanup;

    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_CHECKPOINT;
    tEntry.sKey = "after-model";
    tEntry.sOperationId = "checkpoint";
    tEntry.sContentHash = "checkpoint-after-model";
    if ( xwork_example_check(
             xwork_replay_engine_record_entry(pRecord, &tEntry),
             "record checkpoint entry"
         ) ) goto cleanup;

    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = "tool:readme";
    tEntry.sOperationId = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tEntry.sArgumentsJson = "{\"path\":\"README.md\"}";
    tEntry.sResultJson = "{\"ok\":true,\"bytes\":42}";
    if ( xwork_example_check(
             xwork_replay_engine_record_entry(pRecord, &tEntry),
             "record tool entry"
         ) ) goto cleanup;

    if ( xwork_example_check(
             xwork_replay_engine_list_entries(pRecord, &tEntries),
             "list recorded entries"
         ) ) goto cleanup;

    tReplayOptions.eMode = XWORK_REPLAY_MODE_STRICT;
    if ( xwork_example_check(
             xwork_replay_engine_create(&tReplayOptions, &pStrict),
             "create strict replay"
         ) ) goto cleanup;
    if ( load_entry_from_summary(pStrict, &tEntries.pItems[0]) ) goto cleanup;
    if ( load_entry_from_summary(pStrict, &tEntries.pItems[1]) ) goto cleanup;
    if ( load_entry_from_summary(pStrict, &tEntries.pItems[2]) ) goto cleanup;
    if ( xwork_example_check(
             xwork_replay_engine_seek_checkpoint(pStrict, "after-model"),
             "seek checkpoint"
         ) ) goto cleanup;

    xwork_replay_entry_options_init(&tEntry);
    tEntry.eKind = XWORK_REPLAY_ENTRY_TOOL;
    tEntry.sKey = "tool:readme";
    tEntry.sOperationId = XWORK_TOOL_FILESYSTEM_READ_TEXT;
    tEntry.sArgumentsJson = "{\"path\":\"README.md\"}";
    tEntry.sResultJson = "{\"ok\":true,\"bytes\":42}";
    if ( xwork_example_check(
             xwork_replay_engine_replay_entry(pStrict, &tEntry, &tActual),
             "strict replay tool entry"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_replay_engine_get_result(pStrict, &tResult),
             "get strict replay result"
         ) ) goto cleanup;
    printf(
        "strict replay: replayed=%zu diverged=%s\n",
        tResult.iReplayedCount,
        tResult.bDiverged ? "true" : "false"
    );

    xwork_replay_result_reset(&tResult);
    tReplayOptions.eMode = XWORK_REPLAY_MODE_AUDIT;
    if ( xwork_example_check(
             xwork_replay_engine_create(&tReplayOptions, &pAudit),
             "create audit replay"
         ) ) goto cleanup;
    if ( load_entry_from_summary(pAudit, &tEntries.pItems[2]) ) goto cleanup;
    tEntry.sArgumentsJson = "{\"path\":\"DEVELOPMENT_SPEC.md\"}";
    if ( xwork_example_check(
             xwork_replay_engine_replay_entry(pAudit, &tEntry, NULL),
             "audit replay divergent tool entry"
         ) ) goto cleanup;
    if ( xwork_example_check(
             xwork_replay_engine_emit_report_artifact(
                 pAudit,
                 pRun,
                 "replay-example-divergence",
                 &tReportArtifact
             ),
             "emit replay report artifact"
         ) ) goto cleanup;

    printf(
        "audit report artifact: id=%s class=%d contains_request_mismatch=%s\n",
        tReportArtifact.sArtifactId,
        (int)tReportArtifact.eReportClass,
        strstr(tReportArtifact.sContentText, "request_mismatch") ? "true" : "false"
    );

    iExit = 0;

cleanup:
    xwork_artifact_reset(&tReportArtifact);
    xwork_replay_result_reset(&tResult);
    xwork_replay_entry_summary_reset(&tActual);
    xwork_replay_entry_summary_list_reset(&tEntries);
    xwork_replay_engine_destroy(pAudit);
    xwork_replay_engine_destroy(pStrict);
    xwork_replay_engine_destroy(pRecord);
    xwork_run_destroy(pRun);
    xwork_runtime_destroy(pRuntime);
    return iExit;
}
