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
    FILE *pFile = fopen(sPath, "wb");
    unsigned char abBytes[5] = {0x00u, 0xffu, 0x10u, 0x00u, 0x7fu};

    if ( !pFile ) {
        return 1;
    }
    if ( fwrite(abBytes, 1u, sizeof(abBytes), pFile) != sizeof(abBytes) ) {
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

static const xllm_memory_change_info *find_change_record(
    const xllm_memory_change_set *pSet,
    xllm_memory_change_kind eKind,
    const char *sSourceUri
)
{
    size_t i;

    if ( !pSet || !sSourceUri || !sSourceUri[0] ) {
        return NULL;
    }
    for ( i = 0u; i < pSet->iChangeCount; ++i ) {
        if ( pSet->pChanges[i].eKind == eKind &&
             pSet->pChanges[i].tRecord.sSourceUri &&
             strcmp(pSet->pChanges[i].tRecord.sSourceUri, sSourceUri) == 0 ) {
            return &pSet->pChanges[i];
        }
    }
    return NULL;
}

static const xllm_memory_change_info *find_change_skipped(
    const xllm_memory_change_set *pSet,
    const char *sRelativePath,
    xllm_memory_skip_reason eReason
)
{
    size_t i;

    if ( !pSet || !sRelativePath || !sRelativePath[0] ) {
        return NULL;
    }
    for ( i = 0u; i < pSet->iChangeCount; ++i ) {
        if ( pSet->pChanges[i].eKind == XLLM_MEMORY_CHANGE_SKIPPED &&
             pSet->pChanges[i].tSkipped.eReason == eReason &&
             pSet->pChanges[i].tSkipped.sRelativePath &&
             strcmp(pSet->pChanges[i].tSkipped.sRelativePath, sRelativePath) == 0 ) {
            return &pSet->pChanges[i];
        }
    }
    return NULL;
}

static const xllm_memory_change_info *find_change_failed(
    const xllm_memory_change_set *pSet,
    const char *sRelativePath,
    xllm_memory_fail_reason eReason
)
{
    size_t i;

    if ( !pSet || !sRelativePath || !sRelativePath[0] ) {
        return NULL;
    }
    for ( i = 0u; i < pSet->iChangeCount; ++i ) {
        if ( pSet->pChanges[i].eKind == XLLM_MEMORY_CHANGE_FAILED &&
             pSet->pChanges[i].tFailed.eReason == eReason &&
             pSet->pChanges[i].tFailed.sRelativePath &&
             strcmp(pSet->pChanges[i].tFailed.sRelativePath, sRelativePath) == 0 ) {
            return &pSet->pChanges[i];
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
    xllm_memory_ingest_directory_options tDirectoryOptions;
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_ingest_directory_result tDirectoryResult;
    xllm_memory_sync_workspace_result tSyncResult;
    xllm_memory_change_set tChangeSet;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sIngestDir = "build\\smoke_memory_change_set_tmp\\ingest";
    const char *sWorkspaceDir = "build\\smoke_memory_change_set_tmp\\workspace";
    const char *sIngestText = "build\\smoke_memory_change_set_tmp\\ingest\\note.md";
    const char *sIngestBinary = "build\\smoke_memory_change_set_tmp\\ingest\\bad.bin";
    const char *sWorkspaceText = "build\\smoke_memory_change_set_tmp\\workspace\\README.md";
    const char *sWorkspaceLarge = "build\\smoke_memory_change_set_tmp\\workspace\\large.md";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_ingest_directory_options_init(&tDirectoryOptions);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_ingest_directory_result_init(&tDirectoryResult);
    xllm_memory_sync_workspace_result_init(&tSyncResult);
    xllm_memory_change_set_init(&tChangeSet);

    if ( !xrtDirCreateAll((str)sIngestDir) || !xrtDirCreateAll((str)sWorkspaceDir) ) {
        fprintf(stderr, "failed to prepare directories\n");
        return 1;
    }
    if ( write_text_file(sIngestText, "# note\nhello memory\n") != 0 ||
         write_binary_file(sIngestBinary) != 0 ||
         write_text_file(sWorkspaceText, "# workspace\nkeep track of changes\n") != 0 ||
         write_large_text_file(sWorkspaceLarge, 4096u) != 0 ) {
        fprintf(stderr, "failed to create input files\n");
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
        fprintf(stderr, "embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 4;
    }

    tMemoryOptions.sNamespace = "smoke-change-set";
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

    tDirectoryOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tDirectoryOptions.sPath = sIngestDir;
    tDirectoryOptions.sRecordIdPrefix = "ingest";
    tDirectoryOptions.sAllowedExtensions = ".md;.bin";
    tDirectoryOptions.bRecursive = true;
    tDirectoryOptions.sSourceUriPrefix = "workspace://";
    iStatus = xllm_memory_ingest_directory(pMemory, &tDirectoryOptions, &tDirectoryResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "directory ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 6;
    }
    iStatus = xllm_memory_make_change_set_from_ingest(&tDirectoryResult, &tChangeSet, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "change set from ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 7;
    }
    if ( tChangeSet.iChangeCount != 2u ||
         !find_change_record(&tChangeSet, XLLM_MEMORY_CHANGE_CREATED, "workspace://note.md") ||
         !find_change_failed(&tChangeSet, "bad.bin", XLLM_MEMORY_FAIL_INGEST) ) {
        fprintf(stderr, "unexpected ingest change set\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 8;
    }
    xllm_memory_change_set_reset(&tChangeSet);
    xllm_memory_ingest_directory_result_reset(&tDirectoryResult);

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sWorkspaceDir;
    tWorkspaceOptions.sRecordIdPrefix = "workspace";
    tWorkspaceOptions.sSourceUriPrefix = "workspace://";
    tWorkspaceOptions.uMaxFileBytes = 2048u;
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first workspace sync failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 9;
    }
    xllm_memory_sync_workspace_result_reset(&tSyncResult);

    if ( remove(sWorkspaceText) != 0 ) {
        fprintf(stderr, "failed to delete workspace file\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 10;
    }

    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second workspace sync failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 11;
    }
    iStatus = xllm_memory_make_change_set_from_sync(&tSyncResult, &tChangeSet, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "change set from sync failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 12;
    }
    if ( tChangeSet.iChangeCount != 2u ||
         !find_change_record(&tChangeSet, XLLM_MEMORY_CHANGE_REMOVED, "workspace://README.md") ||
         !find_change_skipped(&tChangeSet, "large.md", XLLM_MEMORY_SKIP_TOO_LARGE) ) {
        fprintf(stderr, "unexpected sync change set\n");
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        xllm_error_free(&tError);
        return 13;
    }

    printf("smoke_memory_change_set ok\n");

    xllm_memory_change_set_reset(&tChangeSet);
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    xllm_memory_ingest_directory_result_reset(&tDirectoryResult);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
