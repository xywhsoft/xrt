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

static const xllm_memory_record_info *find_removed_record_by_source_uri(
    const xllm_memory_sync_workspace_result *pResult,
    const char *sSourceUri
)
{
    size_t i;

    if ( !pResult || !sSourceUri || !sSourceUri[0] ) {
        return NULL;
    }
    for ( i = 0u; i < pResult->iRemovedDetailCount; ++i ) {
        if ( pResult->pRemovedRecords[i].sSourceUri &&
             strcmp(pResult->pRemovedRecords[i].sSourceUri, sSourceUri) == 0 ) {
            return &pResult->pRemovedRecords[i];
        }
    }
    return NULL;
}

static const xllm_memory_skipped_file_info *find_skipped_file(
    const xllm_memory_sync_workspace_result *pResult,
    const char *sRelativePath,
    xllm_memory_skip_reason eReason
)
{
    size_t i;

    if ( !pResult || !sRelativePath || !sRelativePath[0] ) {
        return NULL;
    }
    for ( i = 0u; i < pResult->tIngest.iSkippedDetailCount; ++i ) {
        if ( pResult->tIngest.pSkippedFiles[i].sRelativePath &&
             strcmp(pResult->tIngest.pSkippedFiles[i].sRelativePath, sRelativePath) == 0 &&
             pResult->tIngest.pSkippedFiles[i].eReason == eReason ) {
            return &pResult->tIngest.pSkippedFiles[i];
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
    xllm_memory_sync_workspace_result tSyncResult;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_sync_workspace_tmp";
    const char *sSrcDir = "build\\smoke_memory_sync_workspace_tmp\\src";
    const char *sDocsDir = "build\\smoke_memory_sync_workspace_tmp\\docs";
    const char *sReadmePath = "build\\smoke_memory_sync_workspace_tmp\\README.md";
    const char *sSourcePath = "build\\smoke_memory_sync_workspace_tmp\\src\\main.c";
    const char *sLargePath = "build\\smoke_memory_sync_workspace_tmp\\docs\\large.md";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_sync_workspace_result_init(&tSyncResult);

    if ( !xrtDirCreateAll((str)sSrcDir) || !xrtDirCreateAll((str)sDocsDir) ) {
        fprintf(stderr, "failed to prepare workspace directories\n");
        return 1;
    }
    if ( write_text_file(sReadmePath, "# Workspace sync\nsync should remove stale records.\n") != 0 ||
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

    tMemoryOptions.sNamespace = "smoke-workspace-sync";
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

    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first sync failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }
    if ( tSyncResult.tIngest.uVisitedFileCount != 3u ||
         tSyncResult.tIngest.uIngestedFileCount != 2u ||
         tSyncResult.tIngest.iCreatedDetailCount != 2u ||
         tSyncResult.tIngest.iUpdatedDetailCount != 0u ||
         tSyncResult.tIngest.iSkippedDetailCount != 1u ||
         tSyncResult.tIngest.uSkippedFileCount != 1u ||
         tSyncResult.tIngest.uFailedFileCount != 0u ||
         tSyncResult.uRemovedRecordCount != 0u ||
         tSyncResult.iRemovedDetailCount != 0u ) {
        fprintf(stderr, "unexpected first sync result\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }
    if ( !find_skipped_file(&tSyncResult, "docs\\large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "missing skipped large file detail after first sync\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }

    if ( remove(sReadmePath) != 0 ) {
        fprintf(stderr, "failed to delete readme\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }

    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second sync failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }
    if ( tSyncResult.tIngest.uVisitedFileCount != 2u ||
         tSyncResult.tIngest.uIngestedFileCount != 0u ||
         tSyncResult.tIngest.iSkippedDetailCount != 2u ||
         tSyncResult.tIngest.uSkippedFileCount != 2u ||
         tSyncResult.tIngest.uFailedFileCount != 0u ||
         tSyncResult.uRemovedRecordCount != 1u ||
         tSyncResult.iRemovedDetailCount != 1u ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY) != 1u ) {
        fprintf(stderr, "unexpected second sync result\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 11;
    }
    if ( !find_removed_record_by_source_uri(&tSyncResult, "workspace://README.md") ) {
        fprintf(stderr, "missing removed record detail for README\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 12;
    }
    if ( !find_skipped_file(&tSyncResult, "src\\main.c", XLLM_MEMORY_SKIP_UNCHANGED) ||
         !find_skipped_file(&tSyncResult, "docs\\large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "unexpected skipped file details after second sync\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 13;
    }

    tWorkspaceOptions.sIgnoredPathPatterns = "*.c";
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "third sync failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 12;
    }
    if ( tSyncResult.tIngest.uVisitedFileCount != 2u ||
         tSyncResult.tIngest.uIngestedFileCount != 0u ||
         tSyncResult.tIngest.iSkippedDetailCount != 2u ||
         tSyncResult.tIngest.uSkippedFileCount != 2u ||
         tSyncResult.tIngest.uFailedFileCount != 0u ||
         tSyncResult.uRemovedRecordCount != 1u ||
         tSyncResult.iRemovedDetailCount != 1u ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY) != 0u ) {
        fprintf(stderr, "unexpected third sync result\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 14;
    }
    if ( !find_removed_record_by_source_uri(&tSyncResult, "workspace://src/main.c") ) {
        fprintf(stderr, "missing removed record detail for source file\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 15;
    }
    if ( !find_skipped_file(&tSyncResult, "src\\main.c", XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN) ||
         !find_skipped_file(&tSyncResult, "docs\\large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "unexpected skipped file details after third sync\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 16;
    }

    printf("smoke_memory_sync_workspace ok\n");

    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
