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

static int ingest_turn(
    xllm_memory *pMemory,
    const char *sTurnId,
    const char *sUserText,
    const char *sResponseText,
    int64 iUpdatedAtUnix,
    xllm_error *pError
)
{
    xllm_turn tTurn;
    xllm_response tResponse;
    xllm_memory_ingest_turn_response_options tOptions;
    int iStatus;

    xllm_turn_init(&tTurn);
    memset(&tResponse, 0, sizeof(tResponse));
    xllm_memory_ingest_turn_response_options_init(&tOptions);

    if ( xllm_turn_add_user_text(&tTurn, sUserText) != XRT_NET_OK ||
         init_mock_response(&tResponse, sResponseText) != 0 ) {
        xllm_turn_reset(&tTurn);
        reset_mock_response(&tResponse);
        return XRT_NET_ERROR;
    }

    tOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tOptions.eExtractionPolicy = XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE;
    tOptions.pTurn = &tTurn;
    tOptions.pResponse = &tResponse;
    tOptions.sConversationId = "compact-conversation";
    tOptions.sTurnId = sTurnId;
    tOptions.bUseStableIdentity = true;
    tOptions.bReplaceExisting = true;
    tOptions.iUpdatedAtUnix = iUpdatedAtUnix;
    tOptions.uChunkChars = 256u;

    iStatus = xllm_memory_ingest_turn_response(pMemory, &tOptions, pError);

    xllm_turn_reset(&tTurn);
    reset_mock_response(&tResponse);
    return iStatus;
}

static int list_count_by_type(
    xllm_memory *pMemory,
    const char *sMemoryType,
    xllm_memory_record_list_result *pRecords,
    xllm_error *pError
)
{
    xllm_memory_list_options tOptions;

    xllm_memory_list_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tOptions.sConversationId = "compact-conversation";
    tOptions.sMetadataKey = "memory_type";
    tOptions.sMetadataValue = sMemoryType;
    tOptions.uMaxItems = 16u;
    tOptions.uMaxCharsPerText = 1024u;
    return xllm_memory_list_records(pMemory, &tOptions, pRecords, pError);
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory *pMemory = NULL;
    xllm_memory_compact_conversation_options tCompactOptions;
    xllm_memory_compact_conversation_result tCompactResult;
    xllm_memory_record_list_result tTurnRecords;
    xllm_memory_record_list_result tSummaryRecords;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_error tError;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_compact_conversation_options_init(&tCompactOptions);
    xllm_memory_compact_conversation_result_init(&tCompactResult);
    memset(&tTurnRecords, 0, sizeof(tTurnRecords));
    memset(&tSummaryRecords, 0, sizeof(tSummaryRecords));
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-compact-conversation";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.uDefaultChunkChars = 256u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed\n");
        goto cleanup;
    }

    if ( ingest_turn(pMemory, "turn-001", "remember alpha api", "alpha api moved to qa freeze", 1001, &tError) != XRT_NET_OK ||
         ingest_turn(pMemory, "turn-002", "remember beta cleanup", "beta cleanup owner is alice", 1002, &tError) != XRT_NET_OK ||
         ingest_turn(pMemory, "turn-003", "remember gamma release", "gamma release requires smoke baseline", 1003, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "turn ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( list_count_by_type(pMemory, "conversation.turn_response.v1", &tTurnRecords, &tError) != XRT_NET_OK ||
         require_true(tTurnRecords.iRecordCount == 3u, "expected three turn_response records before compact") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tTurnRecords);

    tCompactOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tCompactOptions.sConversationId = "compact-conversation";
    tCompactOptions.sSummaryText = "alpha api moved to qa freeze; beta cleanup owner is alice; gamma release requires smoke baseline";
    tCompactOptions.sSummaryRecordId = "conversation-summary:compact-conversation";
    tCompactOptions.sSummarySourceUri = "conversation://compact-conversation/summary";
    tCompactOptions.sSummaryTitle = "Compact conversation summary";
    tCompactOptions.bRemoveSourceRecords = true;
    tCompactOptions.bReplaceSummary = true;
    tCompactOptions.iSummaryPriority = 5;
    tCompactOptions.iSummaryUpdatedAtUnix = 2000;
    tCompactOptions.uSummaryChunkChars = 256u;

    iStatus = xllm_memory_compact_conversation(pMemory, &tCompactOptions, &tCompactResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "compact failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tCompactResult.uMatchedRecordCount == 3u, "expected compact matched count") != 0 ||
         require_true(tCompactResult.uCompactedRecordCount == 3u, "expected compacted count") != 0 ||
         require_true(tCompactResult.uRemovedRecordCount == 3u, "expected removed source count") != 0 ) {
        goto cleanup;
    }

    if ( list_count_by_type(pMemory, "conversation.turn_response.v1", &tTurnRecords, &tError) != XRT_NET_OK ||
         require_true(tTurnRecords.iRecordCount == 0u, "expected no turn_response records after compact") != 0 ) {
        goto cleanup;
    }

    if ( list_count_by_type(pMemory, "conversation.summary.v1", &tSummaryRecords, &tError) != XRT_NET_OK ||
         require_true(tSummaryRecords.iRecordCount == 1u, "expected one summary record after compact") != 0 ||
         require_true(tSummaryRecords.pRecords[0].sSourceUri != NULL, "summary source uri missing") != 0 ||
         require_true(strcmp(tSummaryRecords.pRecords[0].sSourceUri, "conversation://compact-conversation/summary") == 0, "unexpected summary source uri") != 0 ) {
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "gamma release smoke baseline";
    tSearchOptions.uMaxHits = 4u;
    tSearchOptions.uMaxCharsPerHit = 256u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         require_true(tSearchResult.iHitCount > 0u, "expected summary search hit") != 0 ||
         require_true(strcmp(tSearchResult.pHits[0].sSourceUri, "conversation://compact-conversation/summary") == 0, "expected summary search source uri") != 0 ) {
        fprintf(stderr, "summary search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_record_list_result_reset(&tSummaryRecords);
    xllm_memory_record_list_result_reset(&tTurnRecords);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    return iRc;
}
