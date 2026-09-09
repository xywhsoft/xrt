#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static int init_mock_response(xllm_response *pResponse, const char *sText)
{
    xllm_output_item *pOutputs;
    xllm_content_part *pParts;

    if ( !pResponse || !sText ) {
        return 1;
    }

    memset(pResponse, 0, sizeof(*pResponse));
    pOutputs = (xllm_output_item *)xrtCalloc(1u, sizeof(xllm_output_item));
    if ( !pOutputs ) {
        return 2;
    }
    pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pParts ) {
        xrtFree(pOutputs);
        return 3;
    }

    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sVisibleText = sText;
    pResponse->pOutputs = pOutputs;
    pResponse->iOutputCount = 1u;

    pOutputs[0].eKind = XLLM_OUTPUT_MESSAGE;
    pOutputs[0].as.tMessage.pParts = pParts;
    pOutputs[0].as.tMessage.iPartCount = 1u;
    pParts[0].eKind = XLLM_PART_TEXT;
    pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pParts[0].as.tSource.sMimeType = "text/plain";
    pParts[0].as.tSource.as.sText = sText;
    return 0;
}

static void reset_mock_response(xllm_response *pResponse)
{
    if ( !pResponse ) {
        return;
    }

    if ( pResponse->pOutputs ) {
        if ( pResponse->iOutputCount >= 1u &&
             pResponse->pOutputs[0].eKind == XLLM_OUTPUT_MESSAGE &&
             pResponse->pOutputs[0].as.tMessage.pParts ) {
            xrtFree(pResponse->pOutputs[0].as.tMessage.pParts);
        }
        xrtFree(pResponse->pOutputs);
    }
    memset(pResponse, 0, sizeof(*pResponse));
}

static int ingest_turn_response(
    xllm_memory *pMemory,
    xllm_memory_ingest_turn_response_options *pOptions,
    xllm_response *pResponse,
    const char *sConversationId,
    const char *sTurnId,
    int64 iUpdatedAtUnix,
    const char *sText,
    xllm_error *pError
)
{
    int iStatus;

    reset_mock_response(pResponse);
    if ( init_mock_response(pResponse, sText) != 0 ) {
        return 1;
    }

    pOptions->pResponse = pResponse;
    pOptions->sConversationId = sConversationId;
    pOptions->sTurnId = sTurnId;
    pOptions->iUpdatedAtUnix = iUpdatedAtUnix;
    iStatus = xllm_memory_ingest_turn_response(pMemory, pOptions, pError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(
            stderr,
            "ingest %s/%s failed: %s\n",
            sConversationId ? sConversationId : "(null)",
            sTurnId ? sTurnId : "(null)",
            pError && pError->sMessage ? pError->sMessage : "(null)"
        );
        return iStatus;
    }
    return XRT_NET_OK;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_turn_response_options tTurnResponseOptions;
    xllm_memory_trim_conversation_options tTrimOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    uint32 uRemoved = 0u;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    xllm_memory_trim_conversation_options_init(&tTrimOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    xllm_turn_init(&tTurn);
    memset(&tResponse, 0, sizeof(tResponse));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "trim-conversation";
    tMemoryOptions.bEnableHybridSearch = false;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_turn_add_user_text(&tTurn, "Remember the most recent conversation state.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.bUseStableIdentity = true;
    tTurnResponseOptions.uChunkChars = 4096u;

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-trim",
        "turn-old",
        1000,
        "Old conversation entry.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-trim",
        "turn-mid",
        2000,
        "Mid conversation entry.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-trim",
        "turn-new",
        3000,
        "New conversation entry.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-other",
        "turn-001",
        4000,
        "Other conversation entry.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    tTrimOptions.sConversationId = "thread-trim";
    tTrimOptions.uKeepLatestRecords = 2u;
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrimOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "trim conversation failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected oldest conversation record removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 3u, "expected three total memory records after trim") != 0 ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "thread-trim";
    tListOptions.tSortByUpdatedAtDesc.bSet = true;
    tListOptions.tSortByUpdatedAtDesc.bValue = true;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list after trim failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two trimmed conversation records") != 0 ||
         require_true(tRecords.pRecords[0].sSourceUri != NULL, "trimmed record source uri missing") != 0 ||
         require_true(strcmp(tRecords.pRecords[0].sSourceUri, "conversation://thread-trim/turn-new") == 0, "expected newest conversation record first after trim") != 0 ||
         require_true(tRecords.pRecords[1].sSourceUri != NULL, "second trimmed record source uri missing") != 0 ||
         require_true(strcmp(tRecords.pRecords[1].sSourceUri, "conversation://thread-trim/turn-mid") == 0, "expected second newest conversation record retained after trim") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "thread-other";
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list other conversation failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 1u, "expected other conversation record to remain") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    tTrimOptions.uKeepLatestRecords = 0u;
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrimOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "trim conversation to zero failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 2u, "expected remaining thread-trim records removed when keep_latest=0") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "expected only other conversation record after keep_latest=0 trim") != 0 ) {
        goto cleanup;
    }

    tTrimOptions.sConversationId = "thread-missing";
    tTrimOptions.uKeepLatestRecords = 2u;
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrimOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "trim missing conversation failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 0u, "expected zero removals for missing conversation trim") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "expected other conversation record to remain after missing conversation trim") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_trim_conversation ok\n");
    iRc = 0;

cleanup:
    reset_mock_response(&tResponse);
    xllm_turn_reset(&tTurn);
    xllm_memory_record_list_result_reset(&tRecords);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_reset(&tError);
    return iRc;
}
