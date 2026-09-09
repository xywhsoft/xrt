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
    uint32 uChunkChars,
    uint32 uChunkOverlapChars,
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
    pOptions->uChunkChars = uChunkChars;
    pOptions->uChunkOverlapChars = uChunkOverlapChars;
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
    size_t iCharNewestLength = 0u;
    size_t iCharMidLength = 0u;
    uint32 uChunkNewestCount = 0u;
    uint32 uChunkMidCount = 0u;
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

    tMemoryOptions.sNamespace = "trim-conversation-budget";
    tMemoryOptions.bEnableHybridSearch = false;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_turn_add_user_text(&tTurn, "Keep conversation memory within budgets.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.bUseStableIdentity = true;

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-budget-chars",
        "turn-old",
        1000,
        4096u,
        0u,
        "aaaaaa",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }
    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-budget-chars",
        "turn-mid",
        2000,
        4096u,
        0u,
        "bbbbbbbbbb",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }
    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-budget-chars",
        "turn-new",
        3000,
        4096u,
        0u,
        "cccccccc",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "thread-budget-chars";
    tListOptions.tSortByUpdatedAtDesc.bSet = true;
    tListOptions.tSortByUpdatedAtDesc.bValue = true;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list char-budget conversation before trim failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 3u, "expected three char-budget records before trim") != 0 ) {
        goto cleanup;
    }
    iCharNewestLength = tRecords.pRecords[0].iTextLength;
    iCharMidLength = tRecords.pRecords[1].iTextLength;
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_trim_conversation_options_init(&tTrimOptions);
    tTrimOptions.sConversationId = "thread-budget-chars";
    tTrimOptions.uKeepLatestRecords = 10u;
    tTrimOptions.uMaxTotalChars = (uint64)iCharNewestLength + (uint64)iCharMidLength;
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrimOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "char-budget trim failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one char-budget record removed") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list char-budget conversation failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two retained char-budget records") != 0 ||
         require_true(strcmp(tRecords.pRecords[0].sSourceUri, "conversation://thread-budget-chars/turn-new") == 0, "expected newest char-budget record retained") != 0 ||
         require_true(strcmp(tRecords.pRecords[1].sSourceUri, "conversation://thread-budget-chars/turn-mid") == 0, "expected second newest char-budget record retained") != 0 ||
         require_true(tRecords.pRecords[0].iTextLength == iCharNewestLength, "expected char-budget newest text length preserved") != 0 ||
         require_true(tRecords.pRecords[1].iTextLength == iCharMidLength, "expected char-budget second text length preserved") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-budget-chunks",
        "turn-old",
        1000,
        8u,
        0u,
        "aaaaaaaa",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }
    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-budget-chunks",
        "turn-mid",
        2000,
        8u,
        0u,
        "bbbbbbbbcccccccc",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }
    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "thread-budget-chunks",
        "turn-new",
        3000,
        8u,
        0u,
        "ddddddddeeeeeeeeffffffff",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "thread-budget-chunks";
    tListOptions.tSortByUpdatedAtDesc.bSet = true;
    tListOptions.tSortByUpdatedAtDesc.bValue = true;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunk-budget conversation before trim failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 3u, "expected three chunk-budget records before trim") != 0 ) {
        goto cleanup;
    }
    uChunkNewestCount = tRecords.pRecords[0].uChunkCount;
    uChunkMidCount = tRecords.pRecords[1].uChunkCount;
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_trim_conversation_options_init(&tTrimOptions);
    tTrimOptions.sConversationId = "thread-budget-chunks";
    tTrimOptions.uKeepLatestRecords = 10u;
    tTrimOptions.uMaxTotalChunks = uChunkNewestCount + uChunkMidCount;
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrimOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "chunk-budget trim failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one chunk-budget record removed") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunk-budget conversation failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two retained chunk-budget records") != 0 ||
         require_true(strcmp(tRecords.pRecords[0].sSourceUri, "conversation://thread-budget-chunks/turn-new") == 0, "expected newest chunk-budget record retained") != 0 ||
         require_true(strcmp(tRecords.pRecords[1].sSourceUri, "conversation://thread-budget-chunks/turn-mid") == 0, "expected second newest chunk-budget record retained") != 0 ||
         require_true(tRecords.pRecords[0].uChunkCount == uChunkNewestCount, "expected newest chunk-budget chunk count preserved") != 0 ||
         require_true(tRecords.pRecords[1].uChunkCount == uChunkMidCount, "expected second newest chunk-budget chunk count preserved") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_trim_conversation_budget ok\n");
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
