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

static const xllm_memory_change_info *find_change(
    const xllm_memory_change_set *pSet,
    xllm_memory_change_kind eKind
)
{
    size_t i;

    if ( !pSet ) {
        return NULL;
    }
    for ( i = 0u; i < pSet->iChangeCount; ++i ) {
        if ( pSet->pChanges[i].eKind == eKind ) {
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
    xllm_memory_sync_file_options tSyncOptions;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_sync_file_tmp";
    const char *sFilePath = "build\\smoke_memory_sync_file_tmp\\main.c";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_sync_file_options_init(&tSyncOptions);
    xllm_memory_change_set_init(&tChanges);

    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sFilePath, "int main(void) { return 0; }\n") != 0 ) {
        fprintf(stderr, "failed to write temp file\n");
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

    tMemoryOptions.sNamespace = "smoke-sync-file";
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

    tSyncOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSyncOptions.sPath = sFilePath;
    tSyncOptions.sRootPath = sRootDir;
    tSyncOptions.sRecordIdPrefix = "workspace";
    tSyncOptions.sSourceUriPrefix = "workspace://";
    tSyncOptions.bUseWorkspaceDefaults = true;
    tSyncOptions.sAllowedExtensions = ".c";
    tSyncOptions.uMaxFileBytes = 2048u;

    iStatus = xllm_memory_sync_file(pMemory, &tSyncOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || tChanges.iChangeCount != 1u || !find_change(&tChanges, XLLM_MEMORY_CHANGE_CREATED) ) {
        fprintf(stderr, "unexpected created sync result: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 6;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_sync_file(pMemory, &tSyncOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || tChanges.iChangeCount != 1u ) {
        fprintf(stderr, "unexpected unchanged sync result: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 7;
    }
    if ( !find_change(&tChanges, XLLM_MEMORY_CHANGE_SKIPPED) ||
         tChanges.pChanges[0].tSkipped.eReason != XLLM_MEMORY_SKIP_UNCHANGED ) {
        fprintf(stderr, "missing unchanged skip change\n");
        return 8;
    }

    if ( write_text_file(sFilePath, "int main(void) { return 1; } /* changed */\n") != 0 ) {
        fprintf(stderr, "failed to rewrite file\n");
        return 9;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_sync_file(pMemory, &tSyncOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || tChanges.iChangeCount != 1u || !find_change(&tChanges, XLLM_MEMORY_CHANGE_UPDATED) ) {
        fprintf(stderr, "unexpected updated sync result: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 10;
    }

    tSyncOptions.sIgnoredPathPatterns = "*.c";
    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_sync_file(pMemory, &tSyncOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || tChanges.iChangeCount != 2u ) {
        fprintf(stderr, "unexpected ignored-path sync result: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 11;
    }
    if ( !find_change(&tChanges, XLLM_MEMORY_CHANGE_SKIPPED) ||
         !find_change(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) ) {
        fprintf(stderr, "expected skipped+removed for ignored file\n");
        return 12;
    }

    tSyncOptions.sIgnoredPathPatterns = NULL;
    if ( write_text_file(sFilePath, "int main(void) { return 2; } /* recreated */\n") != 0 ) {
        fprintf(stderr, "failed to recreate file content\n");
        return 13;
    }
    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_sync_file(pMemory, &tSyncOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || tChanges.iChangeCount != 1u || !find_change(&tChanges, XLLM_MEMORY_CHANGE_CREATED) ) {
        fprintf(stderr, "unexpected recreated sync result: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 14;
    }

    if ( remove(sFilePath) != 0 ) {
        fprintf(stderr, "failed to delete file\n");
        return 15;
    }
    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_sync_file(pMemory, &tSyncOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || tChanges.iChangeCount != 1u || !find_change(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) ) {
        fprintf(stderr, "unexpected removed sync result: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 16;
    }

    printf("smoke_memory_sync_file ok\n");

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
