#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

static int expect_error(
    const char *sCaseName,
    int iStatus,
    const xllm_error *pError,
    xllm_error_code eExpectedCode,
    const char *sExpectedMessagePart
)
{
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "%s: expected failure status\n", sCaseName);
        return 1;
    }
    if ( !pError ) {
        fprintf(stderr, "%s: expected error object\n", sCaseName);
        return 1;
    }
    if ( pError->eCode != eExpectedCode ) {
        fprintf(stderr, "%s: expected error code %d, got %d\n", sCaseName, (int)eExpectedCode, (int)pError->eCode);
        return 1;
    }
    if ( !pError->sMessage || !pError->sMessage[0] ) {
        fprintf(stderr, "%s: expected non-empty error message\n", sCaseName);
        return 1;
    }
    if ( sExpectedMessagePart && !strstr(pError->sMessage, sExpectedMessagePart) ) {
        fprintf(
            stderr,
            "%s: expected error message containing '%s', got '%s'\n",
            sCaseName,
            sExpectedMessagePart,
            pError->sMessage
        );
        return 1;
    }
    if ( pError->iStatus != (int32)pError->eCode ) {
        fprintf(stderr, "%s: expected iStatus to mirror eCode\n", sCaseName);
        return 1;
    }
    return 0;
}

static int expect_invalid_request(
    const char *sCaseName,
    int iStatus,
    xllm_error *pError,
    const char *sExpectedMessagePart
)
{
    int iRc = expect_error(sCaseName, iStatus, pError, XLLM_ERROR_INVALID_REQUEST, sExpectedMessagePart);
    xllm_error_reset(pError);
    return iRc;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngest;
    xllm_memory_search_options tSearch;
    xllm_memory_list_options tList;
    xllm_memory_remove_by_metadata_options tRemoveByMetadata;
    xllm_memory_trim_conversation_options tTrim;
    xllm_memory_search_result tSearchResult;
    xllm_memory_record_list_result tRecordList;
    xllm_memory_chunk_list_result tChunkList;
    xllm_memory_diagnostics tDiagnostics;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    uint32 uRemoved = 123u;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngest);
    xllm_memory_search_options_init(&tSearch);
    xllm_memory_list_options_init(&tList);
    xllm_memory_remove_by_metadata_options_init(&tRemoveByMetadata);
    xllm_memory_trim_conversation_options_init(&tTrim);
    memset(&tSearchResult, 0, sizeof(tSearchResult));
    memset(&tRecordList, 0, sizeof(tRecordList));
    memset(&tChunkList, 0, sizeof(tChunkList));
    xllm_memory_diagnostics_init(&tDiagnostics);
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-error-consistency";
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    xllm_memory_ingest_options_init(&tIngest);
    tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngest.sText = "missing record id should fail";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( expect_invalid_request("ingest_missing_record_id", iStatus, &tError, "record_id and text are required") != 0 ) {
        goto cleanup;
    }

    xllm_memory_ingest_options_init(&tIngest);
    tIngest.eScope = XLLM_MEMORY_SCOPE_ANY;
    tIngest.sRecordId = "bad-scope";
    tIngest.sText = "invalid scope should fail";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( expect_invalid_request("ingest_invalid_scope", iStatus, &tError, "scope must be memory or knowledge") != 0 ) {
        goto cleanup;
    }

    xllm_memory_search_options_init(&tSearch);
    tSearch.eScope = XLLM_MEMORY_SCOPE_ANY;
    iStatus = xllm_memory_search(pMemory, &tSearch, &tSearchResult, &tError);
    if ( expect_invalid_request("search_missing_query", iStatus, &tError, "search query is required") != 0 ) {
        goto cleanup;
    }

    xllm_memory_search_options_init(&tSearch);
    tSearch.eScope = XLLM_MEMORY_SCOPE_ANY;
    tSearch.sQuery = "anything";
    tSearch.sMetadataValue = "orphan-value";
    iStatus = xllm_memory_search(pMemory, &tSearch, &tSearchResult, &tError);
    if ( expect_invalid_request("search_metadata_value_without_key", iStatus, &tError, "metadata_value requires metadata_key") != 0 ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tList);
    tList.eScope = XLLM_MEMORY_SCOPE_ANY;
    tList.sMetadataValue = "orphan-value";
    iStatus = xllm_memory_list_records(pMemory, &tList, &tRecordList, &tError);
    if ( expect_invalid_request("list_records_metadata_value_without_key", iStatus, &tError, "metadata_value requires metadata_key") != 0 ) {
        goto cleanup;
    }

    xllm_memory_list_options_init(&tList);
    tList.eScope = XLLM_MEMORY_SCOPE_ANY;
    tList.sMetadataValue = "orphan-value";
    iStatus = xllm_memory_list_chunks(pMemory, &tList, &tChunkList, &tError);
    if ( expect_invalid_request("list_chunks_metadata_value_without_key", iStatus, &tError, "metadata_value requires metadata_key") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_remove(pMemory, XLLM_MEMORY_SCOPE_ANY, "", &tError);
    if ( expect_invalid_request("remove_missing_record_id", iStatus, &tError, "requires record_id") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_remove_by_source_uri(pMemory, XLLM_MEMORY_SCOPE_ANY, "", &uRemoved, &tError);
    if ( expect_invalid_request("remove_by_source_uri_missing_uri", iStatus, &tError, "requires source_uri") != 0 ) {
        goto cleanup;
    }
    if ( uRemoved != 0u ) {
        fprintf(stderr, "remove_by_source_uri failure should report zero removals\n");
        goto cleanup;
    }

    xllm_memory_remove_by_metadata_options_init(&tRemoveByMetadata);
    tRemoveByMetadata.eScope = XLLM_MEMORY_SCOPE_ANY;
    iStatus = xllm_memory_remove_by_metadata(pMemory, &tRemoveByMetadata, &uRemoved, &tError);
    if ( expect_invalid_request("remove_by_metadata_missing_key", iStatus, &tError, "requires metadata_key") != 0 ) {
        goto cleanup;
    }
    if ( uRemoved != 0u ) {
        fprintf(stderr, "remove_by_metadata failure should report zero removals\n");
        goto cleanup;
    }

    xllm_memory_trim_conversation_options_init(&tTrim);
    iStatus = xllm_memory_trim_conversation(pMemory, &tTrim, &uRemoved, &tError);
    if ( expect_invalid_request("trim_conversation_missing_id", iStatus, &tError, "requires conversation_id") != 0 ) {
        goto cleanup;
    }
    if ( uRemoved != 0u ) {
        fprintf(stderr, "trim_conversation failure should report zero removals\n");
        goto cleanup;
    }

    iStatus = xllm_memory_get_diagnostics(pMemory, NULL, &tError);
    if ( expect_invalid_request("diagnostics_missing_output", iStatus, &tError, "requires memory and output") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_ingest_text(NULL, &tIngest, &tError);
    if ( expect_invalid_request("ingest_missing_memory_handle", iStatus, &tError, "memory handle is required") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_error_consistency ok\n");
    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_record_list_result_reset(&tRecordList);
    xllm_memory_chunk_list_result_reset(&tChunkList);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return iRc;
}
