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

static size_t count_change_kind(
    const xllm_memory_change_set *pSet,
    xllm_memory_change_kind eKind
)
{
    size_t i;
    size_t iCount = 0u;

    if ( !pSet ) {
        return 0u;
    }
    for ( i = 0u; i < pSet->iChangeCount; ++i ) {
        if ( pSet->pChanges[i].eKind == eKind ) {
            ++iCount;
        }
    }
    return iCount;
}

static const xllm_memory_change_info *find_failed_change(const xllm_memory_change_set *pSet)
{
    size_t i;

    if ( !pSet ) {
        return NULL;
    }
    for ( i = 0u; i < pSet->iChangeCount; ++i ) {
        if ( pSet->pChanges[i].eKind == XLLM_MEMORY_CHANGE_FAILED ) {
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
    xllm_memory_sync_file_options tItems[3];
    xllm_memory_sync_files_options tBatchOptions;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_sync_files_tmp";
    const char *sAlphaPath = "build\\smoke_memory_sync_files_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_sync_files_tmp\\beta.c";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_sync_files_options_init(&tBatchOptions);
    xllm_memory_change_set_init(&tChanges);
    xllm_memory_sync_file_options_init(&tItems[0]);
    xllm_memory_sync_file_options_init(&tItems[1]);
    xllm_memory_sync_file_options_init(&tItems[2]);

    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sAlphaPath, "int alpha(void) { return 1; }\n") != 0 ||
         write_text_file(sBetaPath, "int beta(void) { return 2; }\n") != 0 ) {
        fprintf(stderr, "failed to write temp files\n");
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

    tMemoryOptions.sNamespace = "smoke-sync-files";
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

    tItems[0].eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tItems[0].sPath = sAlphaPath;
    tItems[0].sRootPath = sRootDir;
    tItems[0].sRecordIdPrefix = "workspace";
    tItems[0].sSourceUriPrefix = "workspace://";
    tItems[0].bUseWorkspaceDefaults = true;
    tItems[0].sAllowedExtensions = ".c";
    tItems[0].uMaxFileBytes = 4096u;

    tItems[1] = tItems[0];
    tItems[1].sPath = NULL;

    tItems[2] = tItems[0];
    tItems[2].sPath = sBetaPath;

    tBatchOptions.pItems = tItems;
    tBatchOptions.iItemCount = 3u;
    tBatchOptions.bContinueOnError = true;

    iStatus = xllm_memory_sync_files(pMemory, &tBatchOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "initial sync_files failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 6;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 2u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_FAILED) != 1u ) {
        fprintf(stderr, "unexpected initial change counts: total=%u\n", (unsigned)tChanges.iChangeCount);
        return 7;
    }
    if ( !find_failed_change(&tChanges) ||
         find_failed_change(&tChanges)->tFailed.eReason != XLLM_MEMORY_FAIL_SYNC_FILE ) {
        fprintf(stderr, "missing sync_file failure change\n");
        return 8;
    }

    if ( write_text_file(sAlphaPath, "int alpha(void) { return 11; }\n") != 0 ) {
        fprintf(stderr, "failed to rewrite alpha\n");
        return 9;
    }
    if ( remove(sBetaPath) != 0 ) {
        fprintf(stderr, "failed to delete beta\n");
        return 10;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_sync_files(pMemory, &tBatchOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second sync_files failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 11;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_UPDATED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_FAILED) != 1u ) {
        fprintf(stderr, "unexpected second change counts: total=%u\n", (unsigned)tChanges.iChangeCount);
        return 12;
    }

    xllm_memory_change_set_reset(&tChanges);
    tBatchOptions.bContinueOnError = false;
    iStatus = xllm_memory_sync_files(pMemory, &tBatchOptions, &tChanges, &tError);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "expected stop-on-error batch failure\n");
        return 13;
    }
    if ( tChanges.iChangeCount != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_SKIPPED) != 1u ) {
        fprintf(stderr, "unexpected stop-on-error partial result: total=%u\n", (unsigned)tChanges.iChangeCount);
        return 14;
    }

    printf("smoke_memory_sync_files ok\n");

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
