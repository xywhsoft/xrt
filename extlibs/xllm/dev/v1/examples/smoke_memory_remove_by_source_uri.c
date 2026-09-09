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
    xllm_error tError;
    uint32 uRemoved = 0u;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngest);
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "remove-by-source-uri";
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tIngest.sRecordId = "knowledge-main";
    tIngest.sTitle = "Knowledge Main";
    tIngest.sSourceUri = "workspace://src/main.c";
    tIngest.sText = "Knowledge version of main.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest knowledge failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tIngest.sRecordId = "memory-main";
    tIngest.sTitle = "Memory Main";
    tIngest.sSourceUri = "workspace://src/main.c";
    tIngest.sText = "Memory version of main.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest memory failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngest.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tIngest.sRecordId = "memory-other";
    tIngest.sTitle = "Memory Other";
    tIngest.sSourceUri = "workspace://src/other.c";
    tIngest.sText = "Another memory record.";
    iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "ingest other failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = xllm_memory_remove_by_source_uri(
        pMemory,
        XLLM_MEMORY_SCOPE_MEMORY,
        "workspace://src/main.c",
        &uRemoved,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove by source uri failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one memory-scope record removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 1u, "expected knowledge record to remain") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "expected one memory record to remain") != 0 ) {
        goto cleanup;
    }

    iStatus = xllm_memory_remove_by_source_uri(
        pMemory,
        XLLM_MEMORY_SCOPE_ANY,
        "workspace://src/main.c",
        &uRemoved,
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove by source uri(any) failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one remaining any-scope record removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 0u, "expected knowledge record removed on any-scope pass") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "expected unrelated memory record to remain") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_remove_by_source_uri ok\n");
    iRc = 0;

cleanup:
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return iRc;
}
