#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int require_true(int condition, const char *message)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngest;
    xllm_memory_list_options tList;
    xllm_memory_search_options tSearch;
    xllm_memory_record_list_result tRecordList;
    xllm_memory_chunk_list_result tChunkList;
    xllm_memory_search_result tSearchResult;
    xllm_error tError;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngest);
    xllm_memory_list_options_init(&tList);
    xllm_memory_search_options_init(&tSearch);
    memset(&tRecordList, 0, sizeof(tRecordList));
    memset(&tChunkList, 0, sizeof(tChunkList));
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "list-filters";
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngest.sRecordId = "doc-alpha";
    tIngest.sTitle = "Alpha Design";
    tIngest.sSourceUri = "workspace://docs/alpha.md";
    tIngest.sText = "Alpha design covers retrieval planning, context composition, and editor memory workflows.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest alpha failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tIngest.sRecordId = "note-beta";
    tIngest.sTitle = "Beta Notes";
    tIngest.sSourceUri = "workspace://notes/beta.txt";
    tIngest.sText = "Beta notes mention retrieval ranking and a chunk that explicitly mentions sqlite vector search.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest beta failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tList.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tList.sSourceUriContains = "docs/";
    iStatus = xllm_memory_list_records(pMemory, &tList, &tRecordList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecordList.iRecordCount == 1u, "expected one record in docs/ filter") != 0 ||
         require_true(tRecordList.pRecords && strcmp(tRecordList.pRecords[0].sRecordId, "doc-alpha") == 0, "expected doc-alpha from record filter") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecordList);

    xllm_memory_list_options_init(&tList);
    tList.eScope = XLLM_MEMORY_SCOPE_ANY;
    tList.sRecordId = "note-beta";
    tList.sChunkId = NULL;
    tList.sTextContains = "sqlite vector";
    tList.uMaxItems = 4u;
    tList.uMaxCharsPerText = 0u;
    iStatus = xllm_memory_list_chunks(pMemory, &tList, &tChunkList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tChunkList.iChunkCount >= 1u, "expected at least one beta chunk match") != 0 ||
         require_true(tChunkList.pChunks && strcmp(tChunkList.pChunks[0].sRecordId, "note-beta") == 0, "expected note-beta chunk result") != 0 ||
         require_true(tChunkList.pChunks && tChunkList.pChunks[0].sText && strstr(tChunkList.pChunks[0].sText, "sqlite vector") != NULL, "expected filtered chunk text") != 0 ) {
        goto cleanup;
    }
    xllm_memory_chunk_list_result_reset(&tChunkList);

    tSearch.eScope = XLLM_MEMORY_SCOPE_ANY;
    tSearch.sQuery = "retrieval ranking";
    tSearch.sRecordIdContains = "beta";
    tSearch.sSourceUriContains = "notes/";
    iStatus = xllm_memory_search(pMemory, &tSearch, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount >= 1u, "expected at least one filtered search hit") != 0 ||
         require_true(tSearchResult.pHits && strcmp(tSearchResult.pHits[0].sRecordId, "note-beta") == 0, "expected filtered search hit from note-beta") != 0 ) {
        goto cleanup;
    }

    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_chunk_list_result_reset(&tChunkList);
    xllm_memory_record_list_result_reset(&tRecordList);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return iRc;
}
