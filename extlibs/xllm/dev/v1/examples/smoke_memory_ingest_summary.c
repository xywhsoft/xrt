#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
XLLM_API int xllm_turn_set_system_prompt(xllm_turn *pTurn, const char *sText);
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);

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

static int record_contains_text(
    const xllm_memory_chunk_list_result *pChunks,
    const char *sRecordId,
    const char *sNeedle
)
{
    size_t i;

    if ( !pChunks || !sRecordId || !sNeedle ) {
        return 0;
    }

    for ( i = 0u; i < pChunks->iChunkCount; ++i ) {
        const xllm_memory_chunk_info *pChunk = &pChunks->pChunks[i];
        if ( pChunk->sRecordId &&
             strcmp(pChunk->sRecordId, sRecordId) == 0 &&
             pChunk->sText &&
             strstr(pChunk->sText, sNeedle) != NULL ) {
            return 1;
        }
    }

    return 0;
}

static int init_mock_response(xllm_response *pResponse, const char *sVisibleText)
{
    xllm_output_item *pOutputs;
    xllm_content_part *pParts;

    if ( !pResponse || !sVisibleText ) {
        return 1;
    }

    memset(pResponse, 0, sizeof(*pResponse));
    pOutputs = (xllm_output_item *)xrtCalloc(3u, sizeof(xllm_output_item));
    if ( !pOutputs ) {
        return 2;
    }
    pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( !pParts ) {
        xrtFree(pOutputs);
        return 3;
    }

    pResponse->eStatus = XLLM_STATUS_COMPLETED;
    pResponse->sVisibleText = sVisibleText;
    pResponse->pOutputs = pOutputs;
    pResponse->iOutputCount = 3u;

    pOutputs[0].eKind = XLLM_OUTPUT_THINKING;
    pOutputs[0].as.tThinking.bVisible = false;
    pOutputs[0].as.tThinking.sText = "hidden chain should not be persisted by summary policy";

    pOutputs[1].eKind = XLLM_OUTPUT_TOOL_CALL;
    pOutputs[1].as.tToolCall.sCallId = "call_secret_lookup";
    pOutputs[1].as.tToolCall.sToolName = "lookup_secret";
    pOutputs[1].as.tToolCall.sArgumentsJson = "{\"secret\":true}";

    pOutputs[2].eKind = XLLM_OUTPUT_MESSAGE;
    pOutputs[2].as.tMessage.pParts = pParts;
    pOutputs[2].as.tMessage.iPartCount = 1u;
    pParts[0].eKind = XLLM_PART_TEXT;
    pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pParts[0].as.tSource.sMimeType = "text/plain";
    pParts[0].as.tSource.as.sText = sVisibleText;
    return 0;
}

static void reset_mock_response(xllm_response *pResponse)
{
    if ( !pResponse ) {
        return;
    }

    if ( pResponse->pOutputs ) {
        if ( pResponse->iOutputCount >= 3u &&
             pResponse->pOutputs[2].eKind == XLLM_OUTPUT_MESSAGE &&
             pResponse->pOutputs[2].as.tMessage.pParts ) {
            xrtFree(pResponse->pOutputs[2].as.tMessage.pParts);
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
    xllm_memory_ingest_turn_response_options tOptions;
    xllm_memory_list_options tListOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_record_list_result tRecords;
    xllm_memory_chunk_list_result tChunks;
    xllm_memory_search_result tSearchResult;
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_memory_record_info *pRecord;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_turn_response_options_init(&tOptions);
    xllm_memory_list_options_init(&tListOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tRecords, 0, sizeof(tRecords));
    memset(&tChunks, 0, sizeof(tChunks));
    memset(&tSearchResult, 0, sizeof(tSearchResult));
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

    tMemoryOptions.sNamespace = "smoke-summary-policy";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = xllm_turn_set_system_prompt(&tTurn, "System prompt should not enter summary memory.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "set system prompt failed: %d\n", iStatus);
        goto cleanup;
    }
    iStatus = xllm_turn_add_user_text(&tTurn, "Raw user request should not enter summary memory.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }
    if ( init_mock_response(&tResponse, "Fallback response summary says archive the final decision.") != 0 ) {
        fprintf(stderr, "mock response init failed\n");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tOptions);
    tOptions.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
    tOptions.pTurn = &tTurn;
    tOptions.pResponse = &tResponse;
    tOptions.sRecordId = "conv-summary";
    tOptions.sConversationId = "thread-summary";
    tOptions.sTurnId = "turn-001";
    tOptions.sSummaryText = "User prefers pinned account context and concise follow-up notes.";
    tOptions.bIncludeSystemPrompt = true;
    tOptions.bIncludeContextBlocks = true;
    tOptions.bIncludeThinking = true;
    tOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "explicit summary ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tOptions);
    tOptions.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_SUMMARY;
    tOptions.pResponse = &tResponse;
    tOptions.sRecordId = "conv-summary-fallback";
    tOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "fallback summary ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecords.iRecordCount == 2u, "expected two summary records") != 0 ) {
        goto cleanup;
    }

    pRecord = find_record(&tRecords, "conv-summary");
    if ( require_true(pRecord != NULL, "missing explicit summary record") != 0 ||
         require_true(strcmp(pRecord->sTitle, "Conversation summary") == 0, "summary default title mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"memory_type", 0u), "conversation.summary.v1") == 0, "summary memory_type mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"extraction_policy", 0u), "summary") == 0, "summary extraction policy mismatch") != 0 ||
         require_true(strcmp((const char *)xvoTableGetText(pRecord->tMetadata, (str)"conversation_kind", 0u), "summary") == 0, "summary conversation kind mismatch") != 0 ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunks, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( require_true(record_contains_text(&tChunks, "conv-summary", "summary: User prefers pinned account context"), "explicit summary text missing") != 0 ||
         require_true(record_contains_text(&tChunks, "conv-summary-fallback", "summary: Fallback response summary says archive"), "fallback summary text missing") != 0 ||
         require_true(!record_contains_text(&tChunks, "conv-summary", "Raw user request should not enter"), "summary should not include raw user line") != 0 ||
         require_true(!record_contains_text(&tChunks, "conv-summary", "System prompt should not enter"), "summary should not include system prompt") != 0 ||
         require_true(!record_contains_text(&tChunks, "conv-summary", "assistant_thinking:"), "summary should not include thinking") != 0 ||
         require_true(!record_contains_text(&tChunks, "conv-summary", "tool_call lookup_secret"), "summary should not include tool call") != 0 ) {
        goto cleanup;
    }

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "pinned account context";
    tSearchOptions.uMaxHits = 1u;
    tSearchOptions.uMaxCharsPerHit = 4096u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "summary search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount == 1u, "expected one summary search hit") != 0 ||
         require_true(strcmp(tSearchResult.pHits[0].sRecordId, "conv-summary") == 0, "summary search top hit mismatch") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_ingest_summary ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_chunk_list_result_reset(&tChunks);
    xllm_memory_record_list_result_reset(&tRecords);
    reset_mock_response(&tResponse);
    xllm_turn_reset(&tTurn);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
