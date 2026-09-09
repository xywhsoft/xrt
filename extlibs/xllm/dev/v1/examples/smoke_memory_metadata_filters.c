#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int ingest_with_metadata(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    const char *sTitle,
    const char *sSourceUri,
    const char *sText,
    const char *sKind,
    const char *sLang,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tOptions;
    xvalue tMetadata = xvoCreateTable();
    int iStatus;

    if ( !tMetadata ||
         !xvoTableSetText(tMetadata, (str)"kind", 0u, (str)sKind, 0u, FALSE) ||
         !xvoTableSetText(tMetadata, (str)"lang", 0u, (str)sLang, 0u, FALSE) ) {
        if ( tMetadata ) {
            xvoUnref(tMetadata);
        }
        fprintf(stderr, "failed to prepare metadata table\n");
        return XRT_NET_ERROR;
    }

    xllm_memory_ingest_options_init(&tOptions);
    tOptions.eScope = eScope;
    tOptions.sRecordId = sRecordId;
    tOptions.sTitle = sTitle;
    tOptions.sSourceUri = sSourceUri;
    tOptions.sText = sText;
    tOptions.tMetadata = tMetadata;
    iStatus = xllm_memory_ingest_text(pMemory, &tOptions, pError);
    xvoUnref(tMetadata);
    return iStatus;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecordResult;
    xllm_memory_chunk_list_result tChunkResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    memset(&tRecordResult, 0, sizeof(tRecordResult));
    memset(&tChunkResult, 0, sizeof(tChunkResult));

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        return 1;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 2;
    }

    tMemoryOptions.sNamespace = "smoke-meta";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 3;
    }

    iStatus = ingest_with_metadata(
        pMemory,
        XLLM_MEMORY_SCOPE_KNOWLEDGE,
        "guide-c",
        "Guide C",
        "workspace://guide-c",
        "C guide: memory retrieval should turn code chunks into context blocks.",
        "guide",
        "c",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 4;
    }

    iStatus = ingest_with_metadata(
        pMemory,
        XLLM_MEMORY_SCOPE_KNOWLEDGE,
        "note-rust",
        "Note Rust",
        "workspace://note-rust",
        "Rust note: vector search and workspace indexing should stay incremental.",
        "note",
        "rust",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 5;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "How should retrieval create context blocks?";
    tSearchOptions.sMetadataKey = "kind";
    tSearchOptions.sMetadataValue = "guide";
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }
    if ( tSearchResult.iHitCount != 1u ||
         !tSearchResult.pHits[0].sRecordId ||
         strcmp(tSearchResult.pHits[0].sRecordId, "guide-c") != 0 ) {
        fprintf(stderr, "unexpected search result count/filter behavior\n");
        xllm_memory_search_result_reset(&tSearchResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }
    xllm_memory_search_result_reset(&tSearchResult);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tListOptions.sMetadataKey = "lang";
    tListOptions.sMetadataValue = "rust";
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecordResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }
    if ( tRecordResult.iRecordCount != 1u ||
         !tRecordResult.pRecords[0].sRecordId ||
         strcmp(tRecordResult.pRecords[0].sRecordId, "note-rust") != 0 ) {
        fprintf(stderr, "unexpected record list metadata filter behavior\n");
        xllm_memory_record_list_result_reset(&tRecordResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }
    xllm_memory_record_list_result_reset(&tRecordResult);

    xllm_memory_list_options_init(&tListOptions);
    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tListOptions.sMetadataKey = "kind";
    tListOptions.sMetadataValue = "guide";
    tListOptions.sTextContains = "context";
    iStatus = xllm_memory_list_chunks(pMemory, &tListOptions, &tChunkResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }
    if ( tChunkResult.iChunkCount == 0u ||
         !tChunkResult.pChunks[0].sRecordId ||
         strcmp(tChunkResult.pChunks[0].sRecordId, "guide-c") != 0 ) {
        fprintf(stderr, "unexpected chunk list metadata filter behavior\n");
        xllm_memory_chunk_list_result_reset(&tChunkResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 11;
    }

    printf("smoke_memory_metadata_filters ok\n");

    xllm_memory_chunk_list_result_reset(&tChunkResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
