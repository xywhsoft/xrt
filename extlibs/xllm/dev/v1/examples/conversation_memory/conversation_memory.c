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
         !pBlock->pMessages[0].pParts ) {
        return NULL;
    }
    if ( pBlock->pMessages[0].pParts[0].eKind != XLLM_PART_TEXT ||
         pBlock->pMessages[0].pParts[0].as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
        return NULL;
    }
    return pBlock->pMessages[0].pParts[0].as.tSource.as.sText;
}

static int ingest_conversation_summary(
    xllm_memory *pMemory,
    const char *sRecordId,
    const char *sTitle,
    const char *sSourceUri,
    const char *sSummaryText,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tIngestOptions;

    xllm_memory_ingest_options_init(&tIngestOptions);
    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tIngestOptions.sRecordId = sRecordId;
    tIngestOptions.sTitle = sTitle;
    tIngestOptions.sSourceUri = sSourceUri;
    tIngestOptions.sText = sSummaryText;
    tIngestOptions.bReplaceExisting = true;
    tIngestOptions.uChunkChars = 1024u;

    return xllm_memory_ingest_text(pMemory, &tIngestOptions, pError);
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory *pMemory = NULL;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_memory_search_result tSearchResult;
    xllm_request tRequest;
    xllm_error tError;
    const char *sContextText;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_context_options_init(&tContextOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_request_init(&tRequest);
    xllm_error_init(&tError);

    if ( xllm_runtime_create(&tRuntimeOptions, &pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed\n");
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "example-conversation-memory";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.uDefaultMaxHits = 3u;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = ingest_conversation_summary(
        pMemory,
        "thread-001-summary",
        "Conversation summary",
        "memory://conversation/thread-001/summary",
        "summary: User prefers concise answers and uses claw for repository automation.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "initial summary ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "What should I remember about user answer style and claw automation?";
    tSearchOptions.uMaxHits = 1u;
    tContextOptions.sLabel = "Conversation memory:";
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
        fprintf(stderr, "search-before-chat apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    sContextText = first_context_text(&tRequest);
    if ( require_true(sContextText != NULL, "expected retrieved conversation memory context") != 0 ||
         require_true(strstr(sContextText, "Conversation memory:") != NULL, "context label missing") != 0 ||
         require_true(strstr(sContextText, "concise answers") != NULL, "expected concise-answer preference") != 0 ||
         require_true(strstr(sContextText, "claw") != NULL, "expected claw project memory") != 0 ) {
        goto cleanup;
    }

    iStatus = ingest_conversation_summary(
        pMemory,
        "thread-001-turn-002-summary",
        "Conversation summary",
        "memory://conversation/thread-001/turn-002-summary",
        "summary: Assistant explained that conversation memory should be explicitly written after chat.",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "post-chat summary ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_search_options_init(&tSearchOptions);
    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tSearchOptions.sQuery = "explicitly written after chat";
    tSearchOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "post-chat summary search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount == 1u, "expected one post-chat summary hit") != 0 ||
         require_true(strcmp(tSearchResult.pHits[0].sRecordId, "thread-001-turn-002-summary") == 0, "post-chat summary hit mismatch") != 0 ) {
        goto cleanup;
    }

    printf("conversation_memory example ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_request_reset(&tRequest);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_reset(&tError);
    return iRc;
}
