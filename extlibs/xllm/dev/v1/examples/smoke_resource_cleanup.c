#include "xllm-session.h"
#include "xllm-memory.h"

#include <stdio.h>
#include <string.h>

static int require_true(int bCondition, const char *sMessage)
{
    if ( !bCondition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static void remove_db_files(const char *sDbPath)
{
    char sWalPath[512];
    char sShmPath[512];

    remove(sDbPath);
    snprintf(sWalPath, sizeof(sWalPath), "%s-wal", sDbPath);
    snprintf(sShmPath, sizeof(sShmPath), "%s-shm", sDbPath);
    remove(sWalPath);
    remove(sShmPath);
}

static void fill_synthetic_search_result(xllm_memory_search_result *pResult, xllm_memory_hit *pHits)
{
    memset(pResult, 0, sizeof(*pResult));
    memset(pHits, 0, 2u * sizeof(*pHits));

    pHits[0].eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pHits[0].sRecordId = "synthetic-resource-record";
    pHits[0].sChunkId = "synthetic-resource-chunk-0";
    pHits[0].sTitle = "synthetic resource cleanup";
    pHits[0].sSourceUri = "memory://resource-cleanup/synthetic";
    pHits[0].sText = "synthetic cleanup context token";
    pHits[0].fScore = 1.0;

    pHits[1].eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pHits[1].sRecordId = "synthetic-resource-record";
    pHits[1].sChunkId = "synthetic-resource-chunk-1";
    pHits[1].sTitle = "synthetic resource cleanup duplicate";
    pHits[1].sSourceUri = "memory://resource-cleanup/synthetic";
    pHits[1].sText = "synthetic cleanup duplicate token";
    pHits[1].fScore = 0.9;

    pResult->pHits = pHits;
    pResult->iHitCount = 2u;
}

int main(void)
{
    const char *sDbPath = "build\\smoke_resource_cleanup.db";
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngestOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_memory_retrieval_debug_options tDebugOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_retrieval_debug_dump tDebugDump;
    xllm_memory_record_list_result tRecordList;
    xllm_memory_chunk_list_result tChunkList;
    xllm_memory_diagnostics tDiagnostics;
    xllm_memory_health_check tHealth;
    xllm_memory_workspace_status_options tWorkspaceStatusOptions;
    xllm_memory_workspace_status tWorkspaceStatus;
    xllm_memory_change_set tChangeSet;
    xllm_memory_ingest_directory_result tDirectoryResult;
    xllm_memory_sync_workspace_result tSyncWorkspaceResult;
    xllm_memory_compact_conversation_result tCompactResult;
    xllm_memory_hit aSyntheticHits[2];
    xllm_memory_search_result tSyntheticResult;
    xllm_request tRequest;
    xllm_turn tTurn;
    xllm_tool_exec_result tToolResult;
    xllm_error tError;
    int iStatus;
    int iRc = 1;

    memset(&tSearchResult, 0, sizeof(tSearchResult));
    memset(&tDebugDump, 0, sizeof(tDebugDump));
    memset(&tRecordList, 0, sizeof(tRecordList));
    memset(&tChunkList, 0, sizeof(tChunkList));
    memset(&tToolResult, 0, sizeof(tToolResult));
    memset(&tSyntheticResult, 0, sizeof(tSyntheticResult));
    remove_db_files(sDbPath);

    xllm_error_init(&tError);
    xllm_request_init(&tRequest);
    xllm_turn_init(&tTurn);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngestOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_context_options_init(&tContextOptions);
    xllm_memory_retrieval_debug_options_init(&tDebugOptions);
    xllm_memory_list_options_init(&tListOptions);
    xllm_memory_diagnostics_init(&tDiagnostics);
    xllm_memory_health_check_init(&tHealth);
    xllm_memory_workspace_status_options_init(&tWorkspaceStatusOptions);
    xllm_memory_workspace_status_init(&tWorkspaceStatus);
    xllm_memory_change_set_init(&tChangeSet);
    xllm_memory_ingest_directory_result_init(&tDirectoryResult);
    xllm_memory_sync_workspace_result_init(&tSyncWorkspaceResult);
    xllm_memory_compact_conversation_result_init(&tCompactResult);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-resource-cleanup";
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    if ( xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory) != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sRecordId = "cleanup-record";
    tIngestOptions.sTitle = "resource cleanup record";
    tIngestOptions.sSourceUri = "memory://resource-cleanup/record";
    tIngestOptions.sText = "resource cleanup reset free search list debug diagnostics context apply";
    tIngestOptions.bReplaceExisting = true;
    iStatus = xllm_memory_ingest_text(pMemory, &tIngestOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_ANY;
    tSearchOptions.sQuery = "resource cleanup diagnostics";
    tSearchOptions.uMaxHits = 4u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         require_true(tSearchResult.iHitCount > 0u, "expected search hit") != 0 ) {
        fprintf(stderr, "search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_search_result_reset(&tSearchResult);

    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecordList, &tError);
    if ( iStatus != XRT_NET_OK ||
         require_true(tRecordList.iRecordCount == 1u, "expected one listed record") != 0 ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecordList);
    xllm_memory_record_list_result_reset(&tRecordList);

    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunkList, &tError);
    if ( iStatus != XRT_NET_OK ||
         require_true(tChunkList.iChunkCount > 0u, "expected listed chunk") != 0 ) {
        fprintf(stderr, "list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_chunk_list_result_reset(&tChunkList);
    xllm_memory_chunk_list_result_reset(&tChunkList);

    tDebugOptions.tSearchOptions = tSearchOptions;
    tDebugOptions.uMaxCandidates = 8u;
    iStatus = xllm_memory_search_debug(pMemory, &tDebugOptions, &tDebugDump, &tError);
    if ( iStatus != XRT_NET_OK ||
         require_true(tDebugDump.iCandidateCount > 0u, "expected debug candidates") != 0 ||
         require_true(tDebugDump.tSearchResult.iHitCount > 0u, "expected debug search result") != 0 ) {
        fprintf(stderr, "search debug failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_retrieval_debug_dump_reset(&tDebugDump);
    xllm_memory_retrieval_debug_dump_reset(&tDebugDump);

    if ( xllm_memory_get_diagnostics(pMemory, &tDiagnostics, &tError) != XRT_NET_OK ||
         require_true(tDiagnostics.bSqliteOpen, "expected diagnostics sqlite open") != 0 ) {
        fprintf(stderr, "diagnostics failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_diagnostics_init(&tDiagnostics);

    if ( xllm_memory_check_health(pMemory, NULL, &tHealth, &tError) != XRT_NET_OK ||
         require_true(tHealth.bOk, "expected healthy db") != 0 ) {
        fprintf(stderr, "health check failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_health_check_init(&tHealth);

    tWorkspaceStatusOptions.sSourceUriPrefix = "memory://resource-cleanup/";
    if ( xllm_memory_get_workspace_status(pMemory, &tWorkspaceStatusOptions, &tWorkspaceStatus, &tError) != XRT_NET_OK ||
         require_true(tWorkspaceStatus.iRecordCount == 1u, "expected workspace status record count") != 0 ) {
        fprintf(stderr, "workspace status failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_memory_workspace_status_init(&tWorkspaceStatus);

    fill_synthetic_search_result(&tSyntheticResult, aSyntheticHits);
    tContextOptions.bDistinctByRecord = true;
    if ( xllm_memory_apply_search_to_request(&tRequest, &tSyntheticResult, &tContextOptions, &tError) != XRT_NET_OK ||
         require_true(tRequest.iContextBlockCount == 1u, "expected request context block") != 0 ) {
        fprintf(stderr, "apply to request failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    xllm_request_reset(&tRequest);
    xllm_request_reset(&tRequest);

    if ( xllm_turn_add_user_text(&tTurn, "resource cleanup turn") != XRT_NET_OK ||
         xllm_memory_apply_search_to_turn(&tTurn, &tSyntheticResult, &tContextOptions, &tError) != XRT_NET_OK ||
         require_true(tTurn.iContextBlockCount == 1u, "expected turn context block") != 0 ) {
        fprintf(stderr, "turn cleanup setup failed\n");
        goto cleanup;
    }
    xllm_turn_reset(&tTurn);
    xllm_turn_reset(&tTurn);

    xllm_tool_exec_result_free(&tToolResult);
    xllm_tool_exec_result_free(&tToolResult);
    xllm_memory_change_set_reset(&tChangeSet);
    xllm_memory_change_set_reset(&tChangeSet);
    xllm_memory_ingest_directory_result_reset(&tDirectoryResult);
    xllm_memory_ingest_directory_result_reset(&tDirectoryResult);
    xllm_memory_sync_workspace_result_reset(&tSyncWorkspaceResult);
    xllm_memory_sync_workspace_result_reset(&tSyncWorkspaceResult);
    xllm_memory_compact_conversation_result_init(&tCompactResult);

    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_retrieval_debug_dump_reset(&tDebugDump);
    xllm_memory_record_list_result_reset(&tRecordList);
    xllm_memory_chunk_list_result_reset(&tChunkList);
    xllm_request_reset(&tRequest);
    xllm_turn_reset(&tTurn);
    xllm_error_free(&tError);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    remove_db_files(sDbPath);
    return iRc;
}
