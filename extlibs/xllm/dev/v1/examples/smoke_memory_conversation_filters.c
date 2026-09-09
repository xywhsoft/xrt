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

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_turn_response_options tTurnResponseOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_chunk_list_result tChunks;
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    memset(&tRecords, 0, sizeof(tRecords));
    memset(&tChunks, 0, sizeof(tChunks));
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

    tMemoryOptions.sNamespace = "conversation-filters";
    tMemoryOptions.bEnableHybridSearch = false;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_turn_add_user_text(&tTurn, "Remember this thread state.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    if ( init_mock_response(&tResponse, "Conversation A first answer.") != 0 ) {
        fprintf(stderr, "failed to initialize first mock response\n");
        goto cleanup;
    }
    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.pResponse = &tResponse;
    tTurnResponseOptions.sConversationId = "thread-a";
    tTurnResponseOptions.sTurnId = "turn-001";
    tTurnResponseOptions.bUseStableIdentity = true;
    tTurnResponseOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    reset_mock_response(&tResponse);
    if ( init_mock_response(&tResponse, "Conversation A second answer.") != 0 ) {
        fprintf(stderr, "failed to initialize second mock response\n");
        goto cleanup;
    }
    tTurnResponseOptions.sConversationId = "thread-a";
    tTurnResponseOptions.sTurnId = "turn-002";
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    reset_mock_response(&tResponse);
    if ( init_mock_response(&tResponse, "Conversation B answer.") != 0 ) {
        fprintf(stderr, "failed to initialize third mock response\n");
        goto cleanup;
    }
    tTurnResponseOptions.sConversationId = "thread-b";
    tTurnResponseOptions.sTurnId = "turn-001";
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "third ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "second answer";
    tSearchOptions.sConversationId = "thread-a";
    tSearchOptions.sTurnId = "turn-002";
    tSearchOptions.uMaxHits = 4u;
    tSearchOptions.uMaxCharsPerHit = 4096u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "conversation-filtered search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount == 1u, "expected one conversation/turn-filtered search hit") != 0 ||
         require_true(tSearchResult.pHits[0].sSourceUri != NULL, "conversation-filtered hit source uri missing") != 0 ||
         require_true(strcmp(tSearchResult.pHits[0].sSourceUri, "conversation://thread-a/turn-002") == 0, "conversation/turn-filtered search hit mismatch") != 0 ) {
        goto cleanup;
    }
    xllm_memory_search_result_reset(&tSearchResult);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "thread-a";
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "conversation-filtered list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two records for thread-a") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecords);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tListOptions.sConversationId = "thread-a";
    tListOptions.sTurnId = "turn-001";
    tListOptions.uMaxCharsPerText = 4096u;
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunks, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "conversation/turn-filtered list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tChunks.iChunkCount == 1u, "expected one chunk for thread-a turn-001") != 0 ||
         require_true(tChunks.pChunks[0].sSourceUri != NULL, "conversation/turn-filtered chunk source uri missing") != 0 ||
         require_true(strcmp(tChunks.pChunks[0].sSourceUri, "conversation://thread-a/turn-001") == 0, "conversation/turn-filtered chunk mismatch") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_conversation_filters ok\n");
    iRc = 0;

cleanup:
    reset_mock_response(&tResponse);
    xllm_turn_reset(&tTurn);
    xllm_memory_chunk_list_result_reset(&tChunks);
    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_search_result_reset(&tSearchResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_reset(&tError);
    return iRc;
}
