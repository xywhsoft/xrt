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
        if ( fputc('Z' - (int)(i % 26u), pFile) == EOF ) {
            fclose(pFile);
            return 2;
        }
    }
    fclose(pFile);
    return 0;
}

static int verify_record_metadata(const xllm_memory_record_info *pInfo)
{
    const char *sPath;
    const char *sRelativePath;
    const char *sBasename;
    const char *sExtension;
    int64 iBytes;
    int64 iMtime;

    if ( !pInfo || !pInfo->tMetadata || xvoType(pInfo->tMetadata) != XVO_DT_TABLE ) {
        return 1;
    }

    sPath = (const char *)xvoTableGetText(pInfo->tMetadata, "path", 0u);
    sRelativePath = (const char *)xvoTableGetText(pInfo->tMetadata, "relative_path", 0u);
    sBasename = (const char *)xvoTableGetText(pInfo->tMetadata, "basename", 0u);
    sExtension = (const char *)xvoTableGetText(pInfo->tMetadata, "extension", 0u);
    iBytes = xvoTableGetInt(pInfo->tMetadata, "bytes", 0u);
    iMtime = xvoTableGetInt(pInfo->tMetadata, "mtime_unix", 0u);

    if ( !sPath || !sPath[0] || !sRelativePath || !sRelativePath[0] ||
         !sBasename || !sBasename[0] || !sExtension || !sExtension[0] ||
         iBytes <= 0 || iMtime <= 0 ) {
        return 2;
    }
    return 0;
}

static const xllm_memory_record_info *find_record_by_source_uri(
    const xllm_memory_record_info *pInfos,
    size_t iInfoCount,
    const char *sSourceUri
)
{
    size_t i;

    if ( !pInfos || !sSourceUri || !sSourceUri[0] ) {
        return NULL;
    }
    for ( i = 0u; i < iInfoCount; ++i ) {
        if ( pInfos[i].sSourceUri && strcmp(pInfos[i].sSourceUri, sSourceUri) == 0 ) {
            return &pInfos[i];
        }
    }
    return NULL;
}

static const xllm_memory_skipped_file_info *find_skipped_file(
    const xllm_memory_skipped_file_info *pInfos,
    size_t iInfoCount,
    const char *sRelativePath,
    xllm_memory_skip_reason eReason
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
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_ingest_directory_result tResult;
    xllm_memory_list_options tListOptions;
    xllm_memory_record_list_result tRecords;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_workspace_incremental_tmp";
    const char *sSrcDir = "build\\smoke_memory_workspace_incremental_tmp\\src";
    const char *sDocsDir = "build\\smoke_memory_workspace_incremental_tmp\\docs";
    const char *sReadmePath = "build\\smoke_memory_workspace_incremental_tmp\\README.md";
    const char *sSourcePath = "build\\smoke_memory_workspace_incremental_tmp\\src\\main.c";
    const char *sLargePath = "build\\smoke_memory_workspace_incremental_tmp\\docs\\large.md";
    size_t i;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tResult);
    xllm_memory_list_options_init(&tListOptions);
    memset(&tRecords, 0, sizeof(tRecords));

    if ( !xrtDirCreateAll((str)sSrcDir) || !xrtDirCreateAll((str)sDocsDir) ) {
        fprintf(stderr, "failed to prepare workspace directories\n");
        return 1;
    }
    if ( write_text_file(sReadmePath, "# Incremental workspace\nThis workspace is indexed incrementally.\n") != 0 ||
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

    tMemoryOptions.sNamespace = "smoke-workspace-incremental";
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

    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }
    if ( tResult.uVisitedFileCount != 3u ||
         tResult.uIngestedFileCount != 2u ||
         tResult.uCreatedRecordCount != 2u ||
         tResult.uUpdatedRecordCount != 0u ||
         tResult.iCreatedDetailCount != 2u ||
         tResult.iUpdatedDetailCount != 0u ||
         tResult.iSkippedDetailCount != 1u ||
         tResult.uSkippedFileCount != 1u ||
         tResult.uFailedFileCount != 0u ) {
        fprintf(stderr, "unexpected first ingest result\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }
    if ( !find_record_by_source_uri(tResult.pCreatedRecords, tResult.iCreatedDetailCount, "workspace://README.md") ||
         !find_record_by_source_uri(tResult.pCreatedRecords, tResult.iCreatedDetailCount, "workspace://src/main.c") ) {
        fprintf(stderr, "missing created record details after first ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }
    if ( !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "docs\\large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "missing skipped file detail after first ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }

    xllm_memory_ingest_directory_result_reset(&tResult);
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }
    if ( tResult.uVisitedFileCount != 3u ||
         tResult.uIngestedFileCount != 0u ||
         tResult.uCreatedRecordCount != 0u ||
         tResult.uUpdatedRecordCount != 0u ||
         tResult.iCreatedDetailCount != 0u ||
         tResult.iUpdatedDetailCount != 0u ||
         tResult.iSkippedDetailCount != 3u ||
         tResult.uSkippedFileCount != 3u ||
         tResult.uFailedFileCount != 0u ) {
        fprintf(stderr, "unexpected second ingest result\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }
    if ( !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "README.md", XLLM_MEMORY_SKIP_UNCHANGED) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "src\\main.c", XLLM_MEMORY_SKIP_UNCHANGED) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "docs\\large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "unexpected skipped file details after second ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 11;
    }

    if ( write_text_file(sSourcePath, "int main(void) { return 0; }\n") != 0 ) {
        fprintf(stderr, "failed to rewrite source file with same content\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }

    xllm_memory_ingest_directory_result_reset(&tResult);
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "same-content workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 12;
    }
    if ( tResult.uVisitedFileCount != 3u ||
         tResult.uIngestedFileCount != 0u ||
         tResult.uCreatedRecordCount != 0u ||
         tResult.uUpdatedRecordCount != 0u ||
         tResult.iCreatedDetailCount != 0u ||
         tResult.iUpdatedDetailCount != 0u ||
         tResult.iSkippedDetailCount != 3u ||
         tResult.uSkippedFileCount != 3u ||
         tResult.uFailedFileCount != 0u ) {
        fprintf(stderr, "unexpected same-content ingest result\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 13;
    }
    if ( !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "README.md", XLLM_MEMORY_SKIP_UNCHANGED) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "src\\main.c", XLLM_MEMORY_SKIP_UNCHANGED) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "docs\\large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "unexpected skipped file details after same-content ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 14;
    }

    if ( write_text_file(sSourcePath, "int main(void) { return 1; } /* changed */\n") != 0 ) {
        fprintf(stderr, "failed to rewrite source file\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }

    xllm_memory_ingest_directory_result_reset(&tResult);
    iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "third workspace ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 12;
    }
    if ( tResult.uVisitedFileCount != 3u ||
         tResult.uIngestedFileCount != 1u ||
         tResult.uCreatedRecordCount != 0u ||
         tResult.uUpdatedRecordCount != 1u ||
         tResult.iCreatedDetailCount != 0u ||
         tResult.iUpdatedDetailCount != 1u ||
         tResult.iSkippedDetailCount != 2u ||
         tResult.uSkippedFileCount != 2u ||
         tResult.uFailedFileCount != 0u ) {
        fprintf(stderr, "unexpected third ingest result\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 13;
    }
    if ( !find_record_by_source_uri(tResult.pUpdatedRecords, tResult.iUpdatedDetailCount, "workspace://src/main.c") ) {
        fprintf(stderr, "missing updated record details after third ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 14;
    }
    if ( !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "README.md", XLLM_MEMORY_SKIP_UNCHANGED) ||
         !find_skipped_file(tResult.pSkippedFiles, tResult.iSkippedDetailCount, "docs\\large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "unexpected skipped file details after third ingest\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 15;
    }

    tListOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    iStatus = xllm_memory_list_records(pMemory, &tListOptions, &tRecords, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 15;
    }
    if ( tRecords.iRecordCount != 2u ) {
        fprintf(stderr, "expected 2 records, got %u\n", (unsigned)tRecords.iRecordCount);
        xllm_memory_record_list_result_reset(&tRecords);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 16;
    }

    for ( i = 0u; i < tRecords.iRecordCount; ++i ) {
        const xllm_memory_record_info *pInfo = &tRecords.pRecords[i];
        if ( verify_record_metadata(pInfo) != 0 ) {
            fprintf(stderr, "record metadata verification failed\n");
            xllm_memory_record_list_result_reset(&tRecords);
            xllm_memory_destroy(pMemory);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 17;
        }
    }

    printf("smoke_memory_workspace_incremental ok\n");

    xllm_memory_record_list_result_reset(&tRecords);
    xllm_memory_ingest_directory_result_reset(&tResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
