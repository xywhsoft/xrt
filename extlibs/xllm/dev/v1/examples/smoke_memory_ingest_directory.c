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

static int write_binary_file(const char *sPath)
{
    static const unsigned char aBytes[] = { 0x00u, 0x01u, 0x02u, 0x03u };
    FILE *pFile = fopen(sPath, "wb");

    if ( !pFile ) {
        return 1;
    }
    if ( fwrite(aBytes, 1u, sizeof(aBytes), pFile) != sizeof(aBytes) ) {
        fclose(pFile);
        return 2;
    }
    fclose(pFile);
    return 0;
}

static const xllm_memory_failed_file_info *find_failed_file(
    const xllm_memory_failed_file_info *pInfos,
    size_t iInfoCount,
    const char *sRelativePath,
    xllm_memory_fail_reason eReason
)
{
    size_t i;

    if ( !pInfos || !sRelativePath || !sRelativePath[0] ) {
        return NULL;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        if ( pInfos[i].sRelativePath &&
             strcmp(pInfos[i].sRelativePath, sRelativePath) == 0 &&
             pInfos[i].eReason == eReason ) {
            return &pInfos[i];
        }
    }
    return NULL;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_search_options tSearchOptions;
    xllm_memory_search_result tSearchResult;
    xllm_memory_ingest_directory_options tDirectoryOptions;
    xllm_memory_ingest_directory_result tDirectoryResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_ingest_directory_tmp";
    const char *sNestedDir = "build\\smoke_memory_ingest_directory_tmp\\nested";
    const char *sDocPath = "build\\smoke_memory_ingest_directory_tmp\\keep.md";
    const char *sHiddenPath = "build\\smoke_memory_ingest_directory_tmp\\.hidden.md";
    const char *sBinaryPath = "build\\smoke_memory_ingest_directory_tmp\\bad.bin";
    const char *sNestedPath = "build\\smoke_memory_ingest_directory_tmp\\nested\\guide.txt";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_search_options_init(&tSearchOptions);
    xllm_memory_ingest_directory_options_init(&tDirectoryOptions);
    xllm_memory_ingest_directory_result_init(&tDirectoryResult);
    memset(&tSearchResult, 0, sizeof(tSearchResult));

    if ( !xrtDirCreateAll((str)sNestedDir) ) {
        fprintf(stderr, "failed to prepare test directory\n");
        return 1;
    }
    if ( write_text_file(
            sDocPath,
            "xllm memory should support directory ingestion, retrieval, and context blocks.\n"
            "This file should be ingested.\n"
         ) != 0 ) {
        fprintf(stderr, "failed to write keep.md\n");
        return 2;
    }
    if ( write_text_file(
            sHiddenPath,
            "this hidden file should be skipped when skip-hidden is enabled\n"
         ) != 0 ) {
        fprintf(stderr, "failed to write hidden file\n");
        return 3;
    }
    if ( write_binary_file(sBinaryPath) != 0 ) {
        fprintf(stderr, "failed to write binary file\n");
        return 4;
    }
    if ( write_text_file(
            sNestedPath,
            "nested guide: retrieval should turn memory hits into context blocks.\n"
         ) != 0 ) {
        fprintf(stderr, "failed to write nested file\n");
        return 5;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        return 6;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }

    tMemoryOptions.sNamespace = "smoke-dir";
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }

    tDirectoryOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tDirectoryOptions.sPath = sRootDir;
    tDirectoryOptions.sAllowedExtensions = ".md;.txt;.bin";
    tDirectoryOptions.bRecursive = true;
    tDirectoryOptions.bSkipHidden = true;
    tDirectoryOptions.sRecordIdPrefix = "smoke-dir";
    iStatus = xllm_memory_ingest_directory(pMemory, &tDirectoryOptions, &tDirectoryResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "directory ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }

    if ( tDirectoryResult.uVisitedFileCount != 4u ||
         tDirectoryResult.uIngestedFileCount != 2u ||
         tDirectoryResult.iSkippedDetailCount != 1u ||
         tDirectoryResult.uSkippedFileCount != 1u ||
         tDirectoryResult.iFailedDetailCount != 1u ||
         tDirectoryResult.uFailedFileCount != 1u ) {
        fprintf(stderr,
                "unexpected directory ingest result: visited=%u ingested=%u skipped=%u failed=%u\n",
                (unsigned)tDirectoryResult.uVisitedFileCount,
                (unsigned)tDirectoryResult.uIngestedFileCount,
                (unsigned)tDirectoryResult.uSkippedFileCount,
                (unsigned)tDirectoryResult.uFailedFileCount);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }
    {
        const xllm_memory_failed_file_info *pFailed = find_failed_file(
            tDirectoryResult.pFailedFiles,
            tDirectoryResult.iFailedDetailCount,
            "bad.bin",
            XLLM_MEMORY_FAIL_INGEST
        );
        if ( !pFailed ||
             pFailed->eErrorCode != XLLM_ERROR_UNSUPPORTED_INPUT_TYPE ||
             !pFailed->sMessage ||
             strstr(pFailed->sMessage, "binary") == NULL ) {
            fprintf(stderr, "unexpected failed file detail for bad.bin\n");
            xllm_memory_destroy(pMemory);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 11;
        }
    }

    if ( xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY) != 2u ) {
        fprintf(stderr, "unexpected record count: %u\n",
                (unsigned)xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY));
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 12;
    }

    tSearchOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearchOptions.sQuery = "How should memory retrieval produce context blocks?";
    tSearchOptions.uMaxHits = 3u;
    iStatus = xllm_memory_search(pMemory, &tSearchOptions, &tSearchResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 13;
    }

    if ( tSearchResult.iHitCount == 0u ) {
        fprintf(stderr, "expected at least one directory ingest hit\n");
        xllm_memory_search_result_reset(&tSearchResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 14;
    }

    printf("smoke_memory_ingest_directory ok\n");

    xllm_memory_search_result_reset(&tSearchResult);
    xllm_memory_ingest_directory_result_reset(&tDirectoryResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
