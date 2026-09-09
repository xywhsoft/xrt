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

static int write_large_text_file(const char *sPath, size_t iBytes)
{
    FILE *pFile = fopen(sPath, "wb");
    size_t i;

    if ( !pFile ) {
        return 1;
    }
    for ( i = 0u; i < iBytes; ++i ) {
        if ( fputc('A' + (int)(i % 26u), pFile) == EOF ) {
            fclose(pFile);
            return 2;
        }
    }
    fclose(pFile);
    return 0;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_ingest_directory_result tWorkspaceResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_ingest_workspace_tmp";
    const char *sSrcDir = "build\\smoke_memory_ingest_workspace_tmp\\src";
    const char *sDocsDir = "build\\smoke_memory_ingest_workspace_tmp\\docs";
    const char *sReadmePath = "build\\smoke_memory_ingest_workspace_tmp\\README.md";
    const char *sSourcePath = "build\\smoke_memory_ingest_workspace_tmp\\src\\main.c";
    const char *sLargePath = "build\\smoke_memory_ingest_workspace_tmp\\docs\\large.md";
    size_t i;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tWorkspaceResult);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));

    if ( !xrtDirCreateAll((str)sSrcDir) || !xrtDirCreateAll((str)sDocsDir) ) {
        fprintf(stderr, "failed to prepare workspace directories\n");
        return 1;
    }
    if ( write_text_file(sReadmePath, "# Demo workspace\nThis workspace keeps project notes.\n") != 0 ||
         write_text_file(sSourcePath, "int main(void) { return 0; }\n") != 0 ||
         write_large_text_file(sLargePath, 4096u) != 0 ) {
        fprintf(stderr, "failed to write workspace files\n");
        return 2;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        return 3;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 4;
    }

    tMemoryOptions.sNamespace = "smoke-workspace-api";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 5;
    }

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sRootDir;
    tWorkspaceOptions.sRecordIdPrefix = "workspace";
    tWorkspaceOptions.sSourceUriPrefix = "workspace://";
    tWorkspaceOptions.uMaxFileBytes = 2048u;
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tWorkspaceResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }

    if ( tWorkspaceResult.uVisitedFileCount != 3u ||
         tWorkspaceResult.uIngestedFileCount != 2u ||
         tWorkspaceResult.uSkippedFileCount != 1u ||
         tWorkspaceResult.uFailedFileCount != 0u ) {
        fprintf(stderr,
                "unexpected workspace ingest result: visited=%u ingested=%u skipped=%u failed=%u\n",
                (unsigned)tWorkspaceResult.uVisitedFileCount,
                (unsigned)tWorkspaceResult.uIngestedFileCount,
                (unsigned)tWorkspaceResult.uSkippedFileCount,
                (unsigned)tWorkspaceResult.uFailedFileCount);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }

    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }
    if ( tRecords.iRecordCount != 2u ) {
        fprintf(stderr, "expected 2 workspace records, got %u\n", (unsigned)tRecords.iRecordCount);
        xllm_memory_record_list_result_reset(&tRecords);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }

    for ( i = 0u; i < tRecords.iRecordCount; ++i ) {
        const xllm_memory_record_info *pInfo = &tRecords.pRecords[i];
        if ( !pInfo->sSourceUri || strncmp(pInfo->sSourceUri, "workspace://", 12u) != 0 ) {
            fprintf(stderr, "expected workspace source uri, got %s\n", pInfo->sSourceUri ? pInfo->sSourceUri : "(null)");
            xllm_memory_record_list_result_reset(&tRecords);
            xllm_memory_destroy(pMemory);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 10;
        }
        if ( !pInfo->sRecordId || strncmp(pInfo->sRecordId, "workspace:", 10u) != 0 ) {
            fprintf(stderr, "expected workspace-prefixed record id, got %s\n", pInfo->sRecordId ? pInfo->sRecordId : "(null)");
            xllm_memory_record_list_result_reset(&tRecords);
            xllm_memory_destroy(pMemory);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 11;
        }
    }

    printf("smoke_memory_ingest_workspace ok\n");

    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
