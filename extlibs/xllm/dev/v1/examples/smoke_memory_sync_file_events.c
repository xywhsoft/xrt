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
    xllm_memory_sync_file_events_options tEventOptions;
    xllm_memory_file_event tEvents[3];
    xllm_memory_change_set tChanges;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    const char *sRootDir = "build\\smoke_memory_sync_file_events_tmp";
    const char *sAlphaPath = "build\\smoke_memory_sync_file_events_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_sync_file_events_tmp\\beta.c";
    const char *sGammaPath = "build\\smoke_memory_sync_file_events_tmp\\gamma.c";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_sync_file_events_options_init(&tEventOptions);
    xllm_memory_change_set_init(&tChanges);
    xllm_memory_sync_file_event_init(&tEvents[0]);
    xllm_memory_sync_file_event_init(&tEvents[1]);
    xllm_memory_sync_file_event_init(&tEvents[2]);

    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
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

    tMemoryOptions.sNamespace = "smoke-sync-file-events";
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

    tEventOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tEventOptions.tBaseOptions.sRootPath = sRootDir;
    tEventOptions.tBaseOptions.sRecordIdPrefix = "workspace";
    tEventOptions.tBaseOptions.sSourceUriPrefix = "workspace://";
    tEventOptions.tBaseOptions.bUseWorkspaceDefaults = true;
    tEventOptions.tBaseOptions.sAllowedExtensions = ".c";
    tEventOptions.tBaseOptions.uMaxFileBytes = 4096u;
    tEventOptions.pItems = tEvents;
    tEventOptions.iItemCount = 2u;
    tEventOptions.bContinueOnError = true;

    tEvents[0].eKind = XLLM_MEMORY_FILE_EVENT_CREATED;
    tEvents[0].sPath = sAlphaPath;
    tEvents[1].eKind = XLLM_MEMORY_FILE_EVENT_CREATED;
    tEvents[1].sPath = sBetaPath;

    iStatus = xllm_memory_sync_file_events(pMemory, &tEventOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "initial sync events failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 6;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 2u ) {
        fprintf(stderr, "unexpected initial event counts: total=%u\n", (unsigned)tChanges.iChangeCount);
        return 7;
    }

    if ( rename(sAlphaPath, sGammaPath) != 0 ) {
        fprintf(stderr, "failed to rename alpha\n");
        return 8;
    }
    if ( remove(sBetaPath) != 0 ) {
        fprintf(stderr, "failed to remove beta\n");
        return 9;
    }

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_sync_file_event_init(&tEvents[0]);
    xllm_memory_sync_file_event_init(&tEvents[1]);
    xllm_memory_sync_file_event_init(&tEvents[2]);
    tEventOptions.iItemCount = 3u;
    tEventOptions.bContinueOnError = true;

    tEvents[0].eKind = XLLM_MEMORY_FILE_EVENT_RENAMED;
    tEvents[0].sPreviousPath = sAlphaPath;
    tEvents[0].sPath = sGammaPath;
    tEvents[1].eKind = XLLM_MEMORY_FILE_EVENT_DELETED;
    tEvents[1].sPath = sBetaPath;
    tEvents[2].eKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
    tEvents[2].sPath = NULL;

    iStatus = xllm_memory_sync_file_events(pMemory, &tEventOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second sync events failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 10;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 2u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_FAILED) != 1u ) {
        fprintf(stderr, "unexpected second event counts: total=%u\n", (unsigned)tChanges.iChangeCount);
        return 11;
    }
    if ( !find_failed_change(&tChanges) ||
         find_failed_change(&tChanges)->tFailed.eReason != XLLM_MEMORY_FAIL_SYNC_FILE ) {
        fprintf(stderr, "missing event failure change\n");
        return 12;
    }

    xllm_memory_change_set_reset(&tChanges);
    tEventOptions.bContinueOnError = false;
    iStatus = xllm_memory_sync_file_events(pMemory, &tEventOptions, &tChanges, &tError);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "expected stop-on-error event batch failure\n");
        return 13;
    }
    if ( tChanges.iChangeCount != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_SKIPPED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_FAILED) != 0u ) {
        fprintf(stderr, "unexpected stop-on-error partial result: total=%u\n", (unsigned)tChanges.iChangeCount);
        return 14;
    }

    printf("smoke_memory_sync_file_events ok\n");

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
