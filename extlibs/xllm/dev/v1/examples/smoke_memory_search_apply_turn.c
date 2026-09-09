#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

static int write_text_file(const char *sPath, const char *sText)
{
    FILE *pFile = fopen(sPath, "wb");
    size_t iLen;

    if ( !pFile ) {
        return 1;
    }
    iLen = sText ? strlen(sText) : 0u;
    if ( iLen > 0u && fwrite(sText, 1u, iLen, pFile) != iLen ) {
        fclose(pFile);
        return 2;
    }
    fclose(pFile);
    return 0;
}

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static const char *first_context_text(const xllm_context_block *pBlock)
{
    if ( !pBlock ||
         pBlock->iMessageCount == 0u ||
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

static int render_custom_context(
    void *pCtx,
    const xllm_memory_search_result *pResult,
    const size_t *pSelectedHitIndices,
    const size_t *pTextLengths,
    size_t iHitCount,
    char **psText,
    xllm_error *pError
)
{
    const char *sPrefix = (const char *)pCtx;
    const xllm_memory_hit *pHit;
    size_t iTextLength;
    size_t iBufferSize;

    (void)pError;

    if ( !pResult || !pSelectedHitIndices || !pTextLengths || !psText || iHitCount == 0u ) {
        return XRT_NET_ERROR;
    }

    *psText = NULL;
    pHit = &pResult->pHits[pSelectedHitIndices[0]];
    iTextLength = pTextLengths[0];
    iBufferSize = 192u + iTextLength;
    if ( sPrefix ) {
        iBufferSize += strlen(sPrefix);
    }
    if ( pHit->sRecordId ) {
        iBufferSize += strlen(pHit->sRecordId);
    }
    if ( pHit->sSourceUri ) {
        iBufferSize += strlen(pHit->sSourceUri);
    }

    *psText = (char *)xrtCalloc(iBufferSize + 1u, sizeof(char));
    if ( !*psText ) {
        return XRT_NET_ERROR;
    }

    snprintf(
        *psText,
        iBufferSize + 1u,
        "%s\nCUSTOM_CONTEXT hits=%u record=%s source=%s\nTEXT=%.*s",
        sPrefix ? sPrefix : "custom",
        (unsigned)iHitCount,
        pHit->sRecordId ? pHit->sRecordId : "(unknown)",
        pHit->sSourceUri ? pHit->sSourceUri : "(unknown)",
        (int)iTextLength,
        pHit->sText ? pHit->sText : ""
    );
    return XRT_NET_OK;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_directory_options tIngestOptions;
    xllm_memory_ingest_directory_result tIngestResult;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_context_options tContextOptions;
    xllm_memory_search_result tSearchResult;
    xllm_request tRequest;
    xllm_request tWrappedRequest;
    xllm_request tCustomRequest;
    xllm_turn_request tTurn;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const xllm_context_block *pBlock;
    const char *sText;
    const char *sRootDir = "build\\smoke_memory_search_apply_turn_tmp";
    const char *sDocPath = "build\\smoke_memory_search_apply_turn_tmp\\memory_notes.txt";
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_directory_options_init(&tIngestOptions);
    xllm_memory_ingest_directory_result_init(&tIngestResult);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_context_options_init(&tContextOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_request_init(&tRequest);
    xllm_request_init(&tWrappedRequest);
    xllm_request_init(&tCustomRequest);
    xllm_turn_init(&tTurn);

    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp root directory\n");
        goto cleanup;
    }
    if ( write_text_file(
            sDocPath,
            "memory bridge helper should attach retrieved knowledge context to request and turn objects.\n"
         ) != 0 ) {
        fprintf(stderr, "failed to write temp memory document\n");
        goto cleanup;
    }

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

    tMemoryOptions.sNamespace = "smoke-search-apply-turn";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    tIngestOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngestOptions.sPath = sRootDir;
    tIngestOptions.sRecordIdPrefix = "bridge";
    iStatus = xllm_memory_ingest_directory(pMemory, &tIngestOptions, &tIngestResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "How should retrieved knowledge context be attached to request and turn objects?";
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount > 0u, "expected at least one memory search hit") != 0 ) {
        goto cleanup;
    }

    tContextOptions.sLabel = "Request memory context:";
    tContextOptions.iPriority = 42;
    tContextOptions.bPinned = true;
    tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_apply_search_to_request(&tRequest, &tSearchResult, &tContextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "apply search to request failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRequest.iContextBlockCount == 1u, "expected one request context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tRequest.pContextBlocks[0];
    if ( require_true(pBlock->eKind == XLLM_CONTEXT_KNOWLEDGE, "request context should default to knowledge kind") != 0 ||
         require_true(pBlock->iPriority == 42, "request context priority mismatch") != 0 ||
         require_true(pBlock->bPinned, "request context should be pinned") != 0 ||
         require_true(pBlock->iMessageCount == 1u, "request context message count mismatch") != 0 ||
         require_true(pBlock->pMessages[0].eRole == XLLM_ROLE_SYSTEM, "request context role should be system") != 0 ) {
        goto cleanup;
    }
    sText = first_context_text(pBlock);
    if ( require_true(sText != NULL, "request context text missing") != 0 ||
         require_true(strstr(sText, "Request memory context:") != NULL, "request context label missing") != 0 ||
         require_true(strstr(sText, "retrieved knowledge context") != NULL, "request context content missing") != 0 ) {
        goto cleanup;
    }

    xllm_memory_context_options_init(&tContextOptions);
    tContextOptions.sLabel = "Wrapped request context:";
    tContextOptions.eKindOverride = XLLM_CONTEXT_MEMORY;
    tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search_and_apply_to_request(
        pMemory,
        &tSearchOptions,
        &tWrappedRequest,
        &tContextOptions,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search and apply to request failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tWrappedRequest.iContextBlockCount == 1u, "expected one wrapped request context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tWrappedRequest.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(pBlock->eKind == XLLM_CONTEXT_MEMORY, "wrapped request kind override mismatch") != 0 ||
         require_true(sText != NULL, "wrapped request context text missing") != 0 ||
         require_true(strstr(sText, "Wrapped request context:") != NULL, "wrapped request label missing") != 0 ) {
        goto cleanup;
    }

    xllm_memory_context_options_init(&tContextOptions);
    tContextOptions.pfnRender = render_custom_context;
    tContextOptions.pRenderCtx = "Host-rendered context:";
    tContextOptions.uMaxHits = 1u;
    tContextOptions.uMaxCharsPerHit = 48u;
    iStatus = xllm_memory_apply_search_to_request(&tCustomRequest, &tSearchResult, &tContextOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "custom renderer apply failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tCustomRequest.iContextBlockCount == 1u, "expected one custom request context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tCustomRequest.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(sText != NULL, "custom request context text missing") != 0 ||
         require_true(strstr(sText, "Host-rendered context:") != NULL, "custom renderer prefix missing") != 0 ||
         require_true(strstr(sText, "CUSTOM_CONTEXT hits=1") != NULL, "custom renderer marker missing") != 0 ||
         require_true(strstr(sText, "TEXT=memory bridge helper should attach retrieved") != NULL, "custom renderer truncated text missing") != 0 ) {
        goto cleanup;
    }

    xllm_memory_context_options_init(&tContextOptions);
    tContextOptions.sLabel = "Turn memory context:";
    tContextOptions.eKindOverride = XLLM_CONTEXT_MEMORY;
    tContextOptions.iPriority = -7;
    tContextOptions.uMaxHits = 1u;
    iStatus = xllm_memory_search_and_apply_to_turn_request(
        pMemory,
        &tSearchOptions,
        &tTurn,
        &tContextOptions,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search and apply to turn failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tTurn.iContextBlockCount == 1u, "expected one turn context block") != 0 ) {
        goto cleanup;
    }
    pBlock = &tTurn.pContextBlocks[0];
    sText = first_context_text(pBlock);
    if ( require_true(pBlock->eKind == XLLM_CONTEXT_MEMORY, "turn kind override mismatch") != 0 ||
         require_true(pBlock->iPriority == -7, "turn priority mismatch") != 0 ||
         require_true(sText != NULL, "turn context text missing") != 0 ||
         require_true(strstr(sText, "Turn memory context:") != NULL, "turn context label missing") != 0 ||
         require_true(strstr(sText, "request and turn objects") != NULL, "turn context content missing") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_search_apply_turn ok\n");
    iRc = 0;

cleanup:
    xllm_turn_reset(&tTurn);
    xllm_request_reset(&tCustomRequest);
    xllm_request_reset(&tWrappedRequest);
    xllm_request_reset(&tRequest);
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_ingest_directory_result_reset(&tIngestResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
