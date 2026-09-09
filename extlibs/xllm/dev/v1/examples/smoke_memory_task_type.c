#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static const xllm_memory_record_info *find_record(
    const xllm_memory_record_list_result *pRecords,
    const char *sRecordId
)
{
    size_t i;

    if ( !pRecords || !sRecordId ) {
        return NULL;
    }
    for ( i = 0u; i < pRecords->iRecordCount; ++i ) {
        if ( pRecords->pRecords[i].sRecordId &&
             strcmp(pRecords->pRecords[i].sRecordId, sRecordId) == 0 ) {
            return &pRecords->pRecords[i];
        }
    }
    return NULL;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_task_options tTaskOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_memory_record_info *pRecord;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_task_options_init(&tTaskOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-task-type";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    xllm_memory_ingest_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "ide-index-001";
    tTaskOptions.sTitle = "Index workspace symbols";
    tTaskOptions.sText = "Build the initial symbol memory for the active AI IDE workspace.";
    tTaskOptions.sOwner = "xwork";
    tTaskOptions.iDeadlineUnix = 1893456000;
    tTaskOptions.sSourceConversationId = "conv-42";
    tTaskOptions.sSourceTurnId = "turn-007";
    tTaskOptions.iPriority = 7;
    tTaskOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_task(pMemory, &tTaskOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "open task ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_ingest_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "ide-test-002";
    tTaskOptions.sTitle = "Run smoke validation";
    tTaskOptions.sText = "Execute the xllm agent infrastructure smoke baseline.";
    tTaskOptions.eStatus = XLLM_MEMORY_TASK_STATUS_DONE;
    tTaskOptions.sOwner = "ci";
    iStatus = xllm_memory_ingest_task(pMemory, &tTaskOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "done task ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_ingest_task_options_init(&tTaskOptions);
    tTaskOptions.sTaskId = "ide-old-003";
    tTaskOptions.sTitle = "Legacy context migration";
    tTaskOptions.sText = "Canceled migration task should remain typed and searchable by status.";
    tTaskOptions.eStatus = XLLM_MEMORY_TASK_STATUS_CANCELED;
    iStatus = xllm_memory_ingest_task(pMemory, &tTaskOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "canceled task ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sMetadataKey = "memory_type";
    tListOptions.sMetadataValue = "task.v1";
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list task records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 3u, "expected three typed task records") != 0 ) {
        goto cleanup;
    }

    pRecord = find_record(&tRecords, "task:ide-index-001");
    if ( require_true(pRecord != NULL, "missing open task record") != 0 ||
         require_true(strcmp(pRecord->sSourceUri, "task://ide-index-001") == 0, "task source uri mismatch") != 0 ||
         require_true(pRecord->tMetadata && xvoType(pRecord->tMetadata) == XVO_DT_TABLE, "task metadata table missing") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"memory_type", 0u), "task.v1") == 0, "task memory_type mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"extraction_policy", 0u), "task") == 0, "task extraction policy mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"task_status", 0u), "open") == 0, "open task status mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"task_owner", 0u), "xwork") == 0, "task owner mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"source_conversation_id", 0u), "conv-42") == 0, "source conversation mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"source_turn_id", 0u), "turn-007") == 0, "source turn mismatch") != 0 ||
         require_true((int64)xvoTableGetInt(pRecord->tMetadata, (str)"task_deadline_unix", 0u) == 1893456000, "task deadline mismatch") != 0 ||
         require_true((int)xvoTableGetInt(pRecord->tMetadata, (str)"priority", 0u) == 7, "task priority mismatch") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "initial symbol memory active AI IDE workspace";
    tSearchOptions.sMetadataKey = "memory_type";
    tSearchOptions.sMetadataValue = "task.v1";
    tSearchOptions.uMaxHits = 3u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "open task search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount >= 1u, "task search should return at least one hit") != 0 ||
         require_true(tSearchResult.pHits[0].sRecordId &&
                      strcmp(tSearchResult.pHits[0].sRecordId, "task:ide-index-001") == 0,
                      "task search returned wrong record") != 0 ) {
        goto cleanup;
    }
    xllm_memory_search_result_reset(&tSearchResult);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sMetadataKey = "task_status";
    tListOptions.sMetadataValue = "canceled";
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list canceled task failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 1u, "expected one canceled task") != 0 ||
         require_true(tRecords.pRecords[0].sRecordId &&
                      strcmp(tRecords.pRecords[0].sRecordId, "task:ide-old-003") == 0,
                      "canceled task record mismatch") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_task_type ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return iRc;
}
