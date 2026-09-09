#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

typedef struct {
    uint32 uDimensions;
} smoke_embedder_ctx;

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static int smoke_embed_text(
    void *pCtx,
    xllm_memory_embed_task eTask,
    const char *sText,
    xllm_memory_embedding *pEmbedding,
    xllm_error *pError
)
{
    smoke_embedder_ctx *pState = (smoke_embedder_ctx *)pCtx;
    float *pfValues;

    (void)eTask;
    (void)pError;

    if ( !pState || !pEmbedding || pState->uDimensions < 2u ) {
        return XRT_NET_ERROR;
    }

    pfValues = (float *)calloc((size_t)pState->uDimensions, sizeof(*pfValues));
    if ( !pfValues ) {
        return XRT_NET_ERROR;
    }
    pEmbedding->pfValues = pfValues;
    pEmbedding->uValueCount = pState->uDimensions;

    if ( sText && strstr(sText, "vector beacon") ) {
        pfValues[0] = 1.0f;
        pfValues[1] = 0.0f;
    } else if ( sText && strstr(sText, "alpha rollback") ) {
        pfValues[0] = 0.8f;
        pfValues[1] = 0.6f;
    } else {
        pfValues[0] = 0.0f;
        pfValues[1] = 1.0f;
    }
    return XRT_NET_OK;
}

static void smoke_reset_embedding(void *pCtx, xllm_memory_embedding *pEmbedding)
{
    (void)pCtx;
    if ( !pEmbedding ) {
        return;
    }
    free(pEmbedding->pfValues);
    pEmbedding->pfValues = NULL;
    pEmbedding->uValueCount = 0u;
}

static void smoke_dispose_ctx(void *pCtx)
{
    free(pCtx);
}

static int smoke_clone_ctx(void *pCtx, void **ppClonedCtx, xllm_error *pError)
{
    smoke_embedder_ctx *pSource = (smoke_embedder_ctx *)pCtx;
    smoke_embedder_ctx *pClone;

    (void)pError;
    if ( !ppClonedCtx ) {
        return XRT_NET_ERROR;
    }
    *ppClonedCtx = NULL;
    if ( !pSource ) {
        return XRT_NET_OK;
    }
    pClone = (smoke_embedder_ctx *)calloc(1u, sizeof(*pClone));
    if ( !pClone ) {
        return XRT_NET_ERROR;
    }
    *pClone = *pSource;
    *ppClonedCtx = pClone;
    return XRT_NET_OK;
}

static int init_mock_embedder(xllm_memory_embedder *pEmbedder)
{
    smoke_embedder_ctx *pCtx;

    pCtx = (smoke_embedder_ctx *)calloc(1u, sizeof(*pCtx));
    if ( !pCtx ) {
        return XRT_NET_ERROR;
    }
    pCtx->uDimensions = 2u;
    xllm_memory_embedder_init(pEmbedder);
    pEmbedder->pfnEmbedText = smoke_embed_text;
    pEmbedder->pfnResetEmbedding = smoke_reset_embedding;
    pEmbedder->pfnCloneCtx = smoke_clone_ctx;
    pEmbedder->pfnDisposeCtx = smoke_dispose_ctx;
    pEmbedder->pCtx = pCtx;
    return XRT_NET_OK;
}

static int ingest_text(
    xllm_memory *pMemory,
    const char *sRecordId,
    const char *sText,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tOptions;

    xllm_memory_ingest_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sRecordId = sRecordId;
    tOptions.sTitle = sRecordId;
    tOptions.sSourceUri = sRecordId;
    tOptions.sText = sText;
    tOptions.bReplaceExisting = true;
    tOptions.uChunkChars = 4096u;
    return xllm_memory_ingest_text(pMemory, &tOptions, pError);
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    memset(&tResult, 0, sizeof(tResult));
    xllm_error_init(&tError);

    if ( init_mock_embedder(&tMemoryOptions.tEmbedder) != XRT_NET_OK ) {
        fprintf(stderr, "mock embedder init failed\n");
        goto cleanup;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_CUSTOM;
    tMemoryOptions.sMemoryProfileId = "custom.hybrid_rrf.test";
    tMemoryOptions.sNamespace = "smoke-hybrid-rrf";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tLexicalWeight.bSet = true;
    tMemoryOptions.tLexicalWeight.fValue = 1.0;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    if ( ingest_text(
            pMemory,
            "lexical-best",
            "alpha rollback exact lexical runbook with vector beacon",
            &tError
         ) != XRT_NET_OK ||
         ingest_text(
            pMemory,
            "vector-only",
            "vector beacon semantic neighbor without the requested words",
            &tError
         ) != XRT_NET_OK ||
         ingest_text(
            pMemory,
            "irrelevant",
            "unrelated operational note",
            &tError
         ) != XRT_NET_OK ) {
        fprintf(stderr, "ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "alpha rollback";
    tSearchOptions.uMaxHits = 3u;
    tSearchOptions.uMaxCharsPerHit = 4096u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "hybrid rrf search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    if ( require_true(tResult.iHitCount >= 2u, "expected hybrid rrf hits") != 0 ||
         require_true(tResult.pHits[0].sRecordId && strcmp(tResult.pHits[0].sRecordId, "lexical-best") == 0, "hybrid rrf should preserve lexical best hit") != 0 ||
         require_true(tResult.pHits[0].fLexicalScore > 0.0, "top hit lexical diagnostic missing") != 0 ||
         require_true(tResult.pHits[0].fVectorScore > 0.0, "top hit vector diagnostic missing") != 0 ||
         require_true(tResult.pHits[0].fRrfScore > 0.0, "top hit rrf diagnostic missing") != 0 ||
         require_true(tResult.pHits[0].uLexicalRank == 1u, "top hit lexical rank mismatch") != 0 ||
         require_true(tResult.pHits[0].uVectorRank > 0u, "top hit vector rank missing") != 0 ||
         require_true(tResult.pHits[0].sRetrievalProfileId && strcmp(tResult.pHits[0].sRetrievalProfileId, "custom.hybrid_rrf.v1") == 0, "top hit retrieval profile missing") != 0 ||
         require_true(tResult.pHits[0].sEmbedProfileId && strcmp(tResult.pHits[0].sEmbedProfileId, "custom.embedder.v1") == 0, "top hit embed profile missing") != 0 ||
         require_true(tResult.pHits[0].sIndexProfileId && strcmp(tResult.pHits[0].sIndexProfileId, "custom.memory_index.v1") == 0, "top hit index profile missing") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_hybrid_rrf ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tResult);
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
