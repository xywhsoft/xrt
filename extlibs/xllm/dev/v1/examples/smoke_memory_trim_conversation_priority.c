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
    const char *sTurnId,
    int32 iPriority,
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
    pOptions->sTurnId = sTurnId;
    pOptions->iPriority = iPriority;
    pOptions->iUpdatedAtUnix = iUpdatedAtUnix;
    iStatus = xllm_memory_ingest_turn_response(pMemory, pOptions, pError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(
            stderr,
            "ingest %s failed: %s\n",
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

    tMemoryOptions.sNamespace = "trim-conversation-priority";
    tMemoryOptions.bEnableHybridSearch = false;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_turn_add_user_text(&tTurn, "Keep the highest-priority memory when trimming.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.sConversationId = "thread-trim-priority";
    tTurnResponseOptions.bUseStableIdentity = true;
    tTurnResponseOptions.uChunkChars = 4096u;

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "turn-high-old",
        10,
        1000,
        "High priority but older memory.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "turn-low-mid",
        0,
        2000,
        "Lower priority middle memory.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    iStatus = ingest_turn_response(
        pMemory,
        &tTurnResponseOptions,
        &tResponse,
        "turn-low-new",
        0,
        3000,
        "Lower priority newest memory.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    tTrimOptions.sConversationId = "thread-trim-priority";
    tTrimOptions.uKeepLatestRecords = 2u;
    tTrimOptions.tPreferPriorityDesc.bSet = true;
    tTrimOptions.tPreferPriorityDesc.bValue = true;
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrimOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "priority-aware trim failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one low-priority record removed during priority-aware trim") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 2u, "expected two records after priority-aware trim") != 0 ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "thread-trim-priority";
    tListOptions.tSortByUpdatedAtDesc.bSet = true;
    tListOptions.tSortByUpdatedAtDesc.bValue = true;
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list after priority-aware trim failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two retained records after priority-aware trim") != 0 ||
         require_true(tRecords.pRecords[0].sSourceUri != NULL, "first retained source uri missing") != 0 ||
         require_true(strcmp(tRecords.pRecords[0].sSourceUri, "conversation://thread-trim-priority/turn-low-new") == 0, "expected newest low-priority record to remain") != 0 ||
         require_true(tRecords.pRecords[1].sSourceUri != NULL, "second retained source uri missing") != 0 ||
         require_true(strcmp(tRecords.pRecords[1].sSourceUri, "conversation://thread-trim-priority/turn-high-old") == 0, "expected older high-priority record to be preserved") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_trim_conversation_priority ok\n");
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
