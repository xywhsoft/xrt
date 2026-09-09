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

static int hit_priority(const xllm_memory_hit *pHit)
{
    if ( !pHit || !pHit->tMetadata || xvoType(pHit->tMetadata) != XVO_DT_TABLE ) {
        return 0;
    }

    return (int)xvoTableGetInt(pHit->tMetadata, (str)"priority", 0u);
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_turn_response_options tTurnResponseOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tDefaultResult;
    xllm_memory_search_result tWeightedResult;
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
    memset(&tDefaultResult, 0, sizeof(tDefaultResult));
    memset(&tWeightedResult, 0, sizeof(tWeightedResult));
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

    tMemoryOptions.sNamespace = "priority-search";
    tMemoryOptions.bEnableHybridSearch = false;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_turn_add_user_text(&tTurn, "Remember the on-call guidance.");
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "add user text failed: %d\n", iStatus);
        goto cleanup;
    }

    if ( init_mock_response(&tResponse, "Deployment rollback checklist with exact verification steps.") != 0 ) {
        fprintf(stderr, "failed to initialize low-priority mock response\n");
        goto cleanup;
    }

    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.pResponse = &tResponse;
    tTurnResponseOptions.sConversationId = "thread-priority";
    tTurnResponseOptions.sTurnId = "low";
    tTurnResponseOptions.bUseStableIdentity = true;
    tTurnResponseOptions.iPriority = 1;
    tTurnResponseOptions.uChunkChars = 4096u;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "low-priority ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    reset_mock_response(&tResponse);
    if ( init_mock_response(&tResponse, "Deployment rollback procedure with owner handoff.") != 0 ) {
        fprintf(stderr, "failed to initialize high-priority mock response\n");
        goto cleanup;
    }

    tTurnResponseOptions.sTurnId = "high";
    tTurnResponseOptions.iPriority = 10;
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "high-priority ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "deployment rollback checklist";
    tSearchOptions.uMaxHits = 2u;
    tSearchOptions.uMaxCharsPerHit = 4096u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tDefaultResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "default memory search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tDefaultResult.iHitCount == 2u, "expected two default search hits") != 0 ||
         require_true(tDefaultResult.pHits[0].sSourceUri != NULL, "default top hit source uri missing") != 0 ||
         require_true(strcmp(tDefaultResult.pHits[0].sSourceUri, "conversation://thread-priority/low") == 0, "default search should rank lexical best match first") != 0 ||
         require_true(hit_priority(&tDefaultResult.pHits[0]) == 1, "default top hit priority metadata mismatch") != 0 ) {
        goto cleanup;
    }

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "deployment rollback checklist";
    tSearchOptions.uMaxHits = 2u;
    tSearchOptions.uMaxCharsPerHit = 4096u;
    tSearchOptions.tPriorityWeight.bSet = true;
    tSearchOptions.tPriorityWeight.fValue = 0.5;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tWeightedResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "weighted memory search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tWeightedResult.iHitCount == 2u, "expected two weighted search hits") != 0 ||
         require_true(tWeightedResult.pHits[0].sSourceUri != NULL, "weighted top hit source uri missing") != 0 ||
         require_true(strcmp(tWeightedResult.pHits[0].sSourceUri, "conversation://thread-priority/high") == 0, "priority weighting should rank high-priority memory first") != 0 ||
         require_true(hit_priority(&tWeightedResult.pHits[0]) == 10, "weighted top hit priority metadata mismatch") != 0 ||
         require_true(tWeightedResult.pHits[0].fScore > tWeightedResult.pHits[1].fScore, "weighted top hit score should exceed second hit score") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_priority_search ok\n");
    iRc = 0;

cleanup:
    reset_mock_response(&tResponse);
    xllm_turn_reset(&tTurn);
    xllm_memory_search_result_reset(&tWeightedResult);
    xllm_memory_search_result_reset(&tDefaultResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_reset(&tError);
    return iRc;
}
