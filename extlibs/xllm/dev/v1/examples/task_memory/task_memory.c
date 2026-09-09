#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int require_true(int bCondition, const char *sMessage)
{
    if ( !bCondition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static const char *first_context_text(const xllm_request *pRequest)
{
    const xllm_context_block *pBlock;

    if ( !pRequest || pRequest->iContextBlockCount == 0u || !pRequest->pContextBlocks ) {
        return NULL;
    }
    pBlock = &pRequest->pContextBlocks[0];
    if ( pBlock->iMessageCount == 0u ||
         !pBlock->pMessages ||
         pBlock->pMessages[0].iPartCount == 0u ||
         !pBlock->pMessages[0].pParts ||
         pBlock->pMessages[0].pParts[0].eKind != XLLM_PART_TEXT ||
         pBlock->pMessages[0].pParts[0].as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
        return NULL;
    }
    return pBlock->pMessages[0].pParts[0].as.tSource.as.sText;
}

static int ingest_task(
    xllm_memory *pMemory,
    const char *sTaskId,
    xllm_memory_task_status eStatus,
    const char *sTitle,
    const char *sText,
    const char *sOwner,
    xllm_error *pError
)
{
    xllm_memory_ingest_task_options tOptions;

    xllm_memory_ingest_task_options_init(&tOptions);
    tOptions.sTaskId = sTaskId;
    tOptions.eStatus = eStatus;
    tOptions.sTitle = sTitle;
    tOptions.sText = sText;
    tOptions.sOwner = sOwner;
    tOptions.sSourceConversationId = "conv-task-demo";
    tOptions.sSourceTurnId = "turn-task-demo";
    tOptions.iDeadlineUnix = 1893456000;
    tOptions.iPriority = 5;
    tOptions.uChunkChars = 1024u;
    return xllm_memory_ingest_task(pMemory, &tOptions, pError);
}

static int count_tasks_by_status(
    xllm_memory *pMemory,
    const char *sStatus,
    size_t *piCount,
    xllm_error *pError
)
{
    xllm_memory_list_options tOptions;
    xllm_memory_record_list_result tRecords;
    int iStatus;

    if ( piCount ) {
        *piCount = 0u;
    }
    memset(&tRecords, 0, sizeof(tRecords));
    xllm_memory_list_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tOptions.sMetadataKey = "task_status";
    tOptions.sMetadataValue = sStatus;
    iStatus = xllm_memory_list_records(pMemory, &tOptions, &tRecords, pError);
    if ( iStatus == XRT_NET_OK && piCount ) {
        *piCount = tRecords.iRecordCount;
    }
    xllm_memory_record_list_result_reset(&tRecords);
    return iStatus;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory *pMemory = NULL;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_request tRequest;
    xllm_error tError;
    const char *sContextText;
    uint32 uRemovedCount = 0u;
    size_t iOpenCount = 0u;
    size_t iDoneCount = 0u;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_context_options_init(&tContextOptions);
    xllm_request_init(&tRequest);
    xllm_error_init(&tError);

    if ( xllm_runtime_create(&tRuntimeOptions, &pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "example-task-memory";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.uDefaultMaxHits = 3u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = ingest_task(
        pMemory,
        "task-index-symbols",
        XLLM_MEMORY_TASK_STATUS_OPEN,
        "Index workspace symbols",
        "Build symbol memory for the active AI IDE workspace before answering code navigation questions.",
        "xwork",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "open task ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = ingest_task(
        pMemory,
        "task-release-check",
        XLLM_MEMORY_TASK_STATUS_OPEN,
        "Run release checks",
        "Run smoke-list, singlehead, and focused task memory smoke before release.",
        "ci",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second task ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "workspace symbol memory code navigation";
    tSearchOptions.sMetadataKey = "memory_type";
    tSearchOptions.sMetadataValue = "task.v1";
    tSearchOptions.uMaxHits = 1u;

    xllm_memory_context_options_init(&tContextOptions);
    tContextOptions.sLabel = "Open task memory:";
    tContextOptions.uMaxHits = 1u;
    tContextOptions.uMaxCharsPerHit = 512u;
    iStatus = xllm_memory_search_and_apply_to_request(
        pMemory,
        &tSearchOptions,
        &tRequest,
        &tContextOptions,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "task search/apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    sContextText = first_context_text(&tRequest);
    if ( require_true(sContextText != NULL, "expected task memory context") != 0 ||
         require_true(strstr(sContextText, "Open task memory:") != NULL, "task context label missing") != 0 ||
         require_true(strstr(sContextText, "Index workspace symbols") != NULL, "task title missing") != 0 ||
         require_true(strstr(sContextText, "code navigation") != NULL, "task details missing") != 0 ) {
        goto cleanup;
    }

    if ( count_tasks_by_status(pMemory, "open", &iOpenCount, &tError) != XRT_NET_OK ||
         require_true(iOpenCount == 2u, "expected two open tasks") != 0 ) {
        goto cleanup;
    }

    iStatus = ingest_task(
        pMemory,
        "task-index-symbols",
        XLLM_MEMORY_TASK_STATUS_DONE,
        "Index workspace symbols",
        "Symbol memory was built for the active AI IDE workspace.",
        "xwork",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "done task update failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( count_tasks_by_status(pMemory, "open", &iOpenCount, &tError) != XRT_NET_OK ||
         count_tasks_by_status(pMemory, "done", &iDoneCount, &tError) != XRT_NET_OK ||
         require_true(iOpenCount == 1u, "expected one remaining open task") != 0 ||
         require_true(iDoneCount == 1u, "expected one done task") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_remove_by_source_uri(
        pMemory,
        XLLM_MEMORY_SCOPE_MEMORY,
        "task://task-release-check",
        &uRemovedCount,
        &tError
    );
    if ( iStatus != XRT_NET_OK ||
         require_true(uRemovedCount == 1u, "expected release-check task removal") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "expected one remaining task record") != 0 ) {
        goto cleanup;
    }

    printf("task_memory example ok\n");
    iRc = 0;

cleanup:
    xllm_request_reset(&tRequest);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return iRc;
}
