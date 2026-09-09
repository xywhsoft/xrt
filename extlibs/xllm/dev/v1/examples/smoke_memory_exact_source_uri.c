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

    tMemoryOptions.sNamespace = "exact-source-uri";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.sMemoryProfileId = "builtin_sparse.v1";
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngest.sRecordId = "workspace-main";
    tIngest.sTitle = "Workspace Main";
    tIngest.sSourceUri = "workspace://src/main.c";
    tIngest.sText = "Main entry point mentions retrieval planning and IDE workflows.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest main failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngest.sRecordId = "workspace-main-copy";
    tIngest.sTitle = "Workspace Main Copy";
    tIngest.sSourceUri = "workspace://src/main_copy.c";
    tIngest.sText = "Copy entry point also mentions retrieval planning but should not match exact source uri.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest main copy failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tList.eScope = XLLM_MEMORY_SCOPE_ANY;
    tList.sSourceUri = "workspace://src/main.c";
    iStatus = xllm_memory_list_records(pMemory, &tList, &tRecordList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tRecordList.iRecordCount == 1u, "expected exactly one record from exact source uri list") != 0 ||
         require_true(tRecordList.pRecords && strcmp(tRecordList.pRecords[0].sRecordId, "workspace-main") == 0, "expected workspace-main exact source uri record") != 0 ) {
        goto cleanup;
    }
    xllm_memory_record_list_result_reset(&tRecordList);

    xllm_memory_list_options_init(&tList);
    tList.eScope = XLLM_MEMORY_SCOPE_ANY;
    tList.sSourceUri = "workspace://src/main.c";
    tList.sTextContains = "retrieval planning";
    iStatus = xllm_memory_list_chunks(pMemory, &tList, &tChunkList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tChunkList.iChunkCount >= 1u, "expected exact source uri chunk hit") != 0 ||
         require_true(tChunkList.pChunks && strcmp(tChunkList.pChunks[0].sRecordId, "workspace-main") == 0, "expected chunk from workspace-main") != 0 ) {
        goto cleanup;
    }
    xllm_memory_chunk_list_result_reset(&tChunkList);

    tSearch.eScope = XLLM_MEMORY_SCOPE_ANY;
    tSearch.sQuery = "retrieval planning";
    tSearch.sSourceUri = "workspace://src/main.c";
    iStatus = xllm_memory_search(pMemory, &tSearch, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(tSearchResult.iHitCount >= 1u, "expected at least one exact source uri search hit") != 0 ||
         require_true(tSearchResult.pHits && strcmp(tSearchResult.pHits[0].sRecordId, "workspace-main") == 0, "expected exact source uri search to return workspace-main") != 0 ) {
        goto cleanup;
    }

    iRc = 0;
    printf("smoke_memory_exact_source_uri ok\n");

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_chunk_list_result_reset(&tChunkList);
    xllm_memory_record_list_result_reset(&tRecordList);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return iRc;
}
