#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

typedef struct {
    uint32 uDimensions;
} smoke_embedder_ctx;

static int require_true(int condition, const char *message)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", message);
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
    size_t i;

    (void)eTask;
    if ( !pState || !pEmbedding ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INVALID_REQUEST;
        }
        return XRT_NET_ERROR;
    }

    pfValues = (float *)calloc((size_t)pState->uDimensions, sizeof(*pfValues));
    if ( !pfValues ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INTERNAL;
        }
        return XRT_NET_ERROR;
    }
    if ( sText ) {
        for ( i = 0u; sText[i] != '\0'; ++i ) {
            pfValues[((unsigned char)sText[i] + i) % pState->uDimensions] += 1.0f;
        }
    }
    pEmbedding->pfValues = pfValues;
    pEmbedding->uValueCount = pState->uDimensions;
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

static int make_stub_embedder(xllm_memory_embedder *pEmbedder)
{
    smoke_embedder_ctx *pCtx;

    if ( !pEmbedder ) {
        return XRT_NET_ERROR;
    }

    pCtx = (smoke_embedder_ctx *)calloc(1u, sizeof(*pCtx));
    if ( !pCtx ) {
        return XRT_NET_ERROR;
    }
    pCtx->uDimensions = 8u;
    xllm_memory_embedder_init(pEmbedder);
    pEmbedder->pfnEmbedText = smoke_embed_text;
    pEmbedder->pfnResetEmbedding = smoke_reset_embedding;
    pEmbedder->pfnDisposeCtx = smoke_dispose_ctx;
    pEmbedder->pCtx = pCtx;
    return XRT_NET_OK;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngest;
    xllm_memory_diagnostics tDiagnostics;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecordList;
    xllm_memory_chunk_list_result tChunkList;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    int iStatus;
    int iRc = 1;
    const char *sDbPath = "build\\smoke_memory_diagnostics.db";

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngest);
    xllm_memory_diagnostics_init(&tDiagnostics);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecordList, 0, sizeof(tRecordList));
    memset(&tChunkList, 0, sizeof(tChunkList));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-diagnostics";
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.uDefaultChunkChars = 32u;
    tMemoryOptions.uDefaultChunkOverlapChars = 4u;
    tMemoryOptions.uDefaultMaxHits = 7u;

    remove(sDbPath);
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = xllm_memory_get_diagnostics(pMemory, &tDiagnostics, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "initial diagnostics failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tDiagnostics.eScheme == XLLM_MEMORY_SCHEME_BUILTIN_SPARSE, "diagnostics scheme mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.sMemoryProfileId && strcmp(tDiagnostics.sMemoryProfileId, "builtin_sparse.v1") == 0, "diagnostics profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.sNamespace && strcmp(tDiagnostics.sNamespace, "smoke-memory-diagnostics") == 0, "diagnostics namespace mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.uDefaultChunkChars == 32u, "diagnostics chunk chars mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.uDefaultChunkOverlapChars == 4u, "diagnostics overlap mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.uDefaultMaxHits == 7u, "diagnostics max hits mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.bSqliteOpen, "diagnostics sqlite open mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.uSqliteSchemaVersion == 1u, "diagnostics schema version mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.sStoredMemoryProfileId && strcmp(tDiagnostics.sStoredMemoryProfileId, "builtin_sparse.v1") == 0, "diagnostics stored profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.bStorageProfileMatch, "diagnostics storage profile match mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.iRecordCount == 0u && tDiagnostics.iChunkCount == 0u, "initial diagnostics counts mismatch") != 0 ) {
        goto cleanup;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngest.sRecordId = "diag-knowledge";
    tIngest.sTitle = "diagnostics knowledge";
    tIngest.sSourceUri = "memory://diagnostics/knowledge";
    tIngest.sText = "diagnostics should report knowledge records and chunks";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "knowledge ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_ingest_options_init(&tIngest);
    tIngest.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tIngest.sRecordId = "diag-memory";
    tIngest.sTitle = "diagnostics memory";
    tIngest.sSourceUri = "memory://diagnostics/memory";
    tIngest.sText = "diagnostics should report memory records and chunks";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = xllm_memory_get_diagnostics(pMemory, &tDiagnostics, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "post-ingest diagnostics failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tDiagnostics.iRecordCount == 2u, "diagnostics total record count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.iKnowledgeRecordCount == 1u, "diagnostics knowledge record count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.iMemoryRecordCount == 1u, "diagnostics memory record count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.iChunkCount == xllm_memory_chunk_count(pMemory, XLLM_MEMORY_SCOPE_ANY), "diagnostics total chunk count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.iKnowledgeChunkCount == xllm_memory_chunk_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE), "diagnostics knowledge chunk count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tDiagnostics.iMemoryChunkCount == xllm_memory_chunk_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY), "diagnostics memory chunk count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(!tDiagnostics.bVectorTableReady, "builtin sparse should not report vector table ready") != 0 ) {
        goto cleanup;
    }
    if ( require_true(!tDiagnostics.bEmbedderConfigured, "builtin sparse should not report configured embedder") != 0 ) {
        goto cleanup;
    }
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecordList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "record list failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecordList.iRecordCount == 2u, "record list count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tRecordList.pRecords[0].sMemoryProfileId && strcmp(tRecordList.pRecords[0].sMemoryProfileId, "builtin_sparse.v1") == 0, "record memory profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tRecordList.pRecords[0].sRetrievalProfileId && strcmp(tRecordList.pRecords[0].sRetrievalProfileId, "builtin_sparse.bm25.v1") == 0, "record retrieval profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tRecordList.pRecords[0].sEmbedProfileId && strcmp(tRecordList.pRecords[0].sEmbedProfileId, "none.v1") == 0, "record embed profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tRecordList.pRecords[0].sIndexProfileId && strcmp(tRecordList.pRecords[0].sIndexProfileId, "sqlite.postings.v1") == 0, "record index profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tRecordList.pRecords[0].uProfileVersion == 1u, "record profile version mismatch") != 0 ) {
        goto cleanup;
    }
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunkList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "chunk list failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tChunkList.iChunkCount == tDiagnostics.iChunkCount, "chunk list count mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].sChunkProfileId && strcmp(tChunkList.pChunks[0].sChunkProfileId, "rule_chunker.v1") == 0, "chunk profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].sMemoryProfileId && strcmp(tChunkList.pChunks[0].sMemoryProfileId, "builtin_sparse.v1") == 0, "chunk memory profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].sRetrievalProfileId && strcmp(tChunkList.pChunks[0].sRetrievalProfileId, "builtin_sparse.bm25.v1") == 0, "chunk retrieval profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].sEmbedProfileId && strcmp(tChunkList.pChunks[0].sEmbedProfileId, "none.v1") == 0, "chunk embed profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].sIndexProfileId && strcmp(tChunkList.pChunks[0].sIndexProfileId, "sqlite.postings.v1") == 0, "chunk index profile mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].uProfileVersion == 1u, "chunk profile version mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].uContentHash != 0u, "chunk content hash missing") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].iEndByte > tChunkList.pChunks[0].iStartByte, "chunk byte range mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.pChunks[0].sPreviousChunkId == NULL, "first chunk previous id mismatch") != 0 ) {
        goto cleanup;
    }
    if ( require_true(tChunkList.iChunkCount < 2u || (tChunkList.pChunks[0].sNextChunkId && tChunkList.pChunks[0].sNextChunkId[0]), "first chunk next id missing") != 0 ) {
        goto cleanup;
    }

    xllm_memory_destroy(pMemory);
    pMemory = NULL;

    xllm_memory_options_init(&tMemoryOptions);
    tMemoryOptions.sNamespace = "smoke-memory-diagnostics";
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_CUSTOM;
    tMemoryOptions.sMemoryProfileId = "custom.v1";
    if ( make_stub_embedder(&tMemoryOptions.tEmbedder) != XRT_NET_OK ) {
        fprintf(stderr, "stub embedder create failed\n");
        goto cleanup;
    }
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "profile mismatch unexpectedly opened existing sqlite namespace\n");
        goto cleanup;
    }

    printf("smoke_memory_diagnostics ok records=%zu chunks=%zu\n", tDiagnostics.iRecordCount, tDiagnostics.iChunkCount);
    iRc = 0;

cleanup:
    if ( pMemory ) {
        xllm_memory_destroy(pMemory);
    }
    xllm_memory_record_list_result_reset(&tRecordList);
    xllm_memory_chunk_list_result_reset(&tChunkList);
    if ( pRuntime ) {
        xllm_runtime_destroy(pRuntime);
    }
    xllm_error_free(&tError);
    return iRc;
}
