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

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_file_event_queue_drain_options tDrainOptions;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xllm_memory_file_event_queue *pQueue = NULL;
    const char *sRootDir = "build\\smoke_memory_file_event_queue_tmp";
    const char *sAlphaPath = "build\\smoke_memory_file_event_queue_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_file_event_queue_tmp\\beta.c";
    const char *sGammaPath = "build\\smoke_memory_file_event_queue_tmp\\gamma.c";
    const char *sDeltaPath = "build\\smoke_memory_file_event_queue_tmp\\delta.c";
    const char *sEpsilonPath = "build\\smoke_memory_file_event_queue_tmp\\epsilon.c";
    const char *sZetaPath = "build\\smoke_memory_file_event_queue_tmp\\zeta.c";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_file_event_queue_drain_options_init(&tDrainOptions);
    xllm_memory_change_set_init(&tChanges);

    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sAlphaPath, "int alpha(void) { return 1; }\n") != 0 ||
         write_text_file(sBetaPath, "int beta(void) { return 2; }\n") != 0 ||
         write_text_file(sDeltaPath, "int delta(void) { return 4; }\n") != 0 ||
         write_text_file(sEpsilonPath, "int epsilon(void) { return 5; }\n") != 0 ||
         write_text_file(sZetaPath, "int zeta(void) { return 6; }\n") != 0 ) {
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

    tMemoryOptions.sNamespace = "smoke-file-event-queue";
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

    iStatus = xllm_memory_file_event_queue_create(&pQueue);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "queue create failed: %d\n", iStatus);
        return 6;
    }

    tDrainOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tDrainOptions.tBaseOptions.sRootPath = sRootDir;
    tDrainOptions.tBaseOptions.sRecordIdPrefix = "workspace";
    tDrainOptions.tBaseOptions.sSourceUriPrefix = "workspace://";
    tDrainOptions.tBaseOptions.bUseWorkspaceDefaults = true;
    tDrainOptions.tBaseOptions.sAllowedExtensions = ".c";
    tDrainOptions.tBaseOptions.uMaxFileBytes = 4096u;

    if ( xllm_memory_file_event_queue_push_created(pQueue, sAlphaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push alpha failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 7;
    }
    if ( xllm_memory_file_event_queue_push_updated(pQueue, sAlphaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push alpha update failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 8;
    }
    if ( xllm_memory_file_event_queue_push_created(pQueue, sBetaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push beta failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 9;
    }
    if ( xllm_memory_file_event_queue_push_created(pQueue, "build\\smoke_memory_file_event_queue_tmp\\temp.c", &tError) != XRT_NET_OK ||
         xllm_memory_file_event_queue_push_deleted(pQueue, "build\\smoke_memory_file_event_queue_tmp\\temp.c", &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push temp create/delete failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 10;
    }
    if ( xllm_memory_file_event_queue_count(pQueue) != 2u ) {
        fprintf(stderr, "unexpected queue count after coalesced create pushes\n");
        return 11;
    }

    tDrainOptions.iMaxItems = 1u;
    iStatus = xllm_memory_file_event_queue_drain(pMemory, pQueue, &tDrainOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "first drain failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 12;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         xllm_memory_file_event_queue_count(pQueue) != 1u ) {
        fprintf(stderr, "unexpected first drain result\n");
        return 13;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_file_event_queue_drain(pMemory, pQueue, &tDrainOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "second drain failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 14;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         xllm_memory_file_event_queue_count(pQueue) != 0u ) {
        fprintf(stderr, "unexpected second drain result\n");
        return 15;
    }

    if ( rename(sAlphaPath, sGammaPath) != 0 ) {
        fprintf(stderr, "failed to rename alpha\n");
        return 14;
    }
    if ( remove(sBetaPath) != 0 ) {
        fprintf(stderr, "failed to remove beta\n");
        return 15;
    }

    if ( xllm_memory_file_event_queue_push_renamed(pQueue, sAlphaPath, sGammaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push rename failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 16;
    }
    if ( xllm_memory_file_event_queue_push_updated(pQueue, sGammaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push gamma update failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 17;
    }
    if ( xllm_memory_file_event_queue_push_deleted(pQueue, sBetaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push delete failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 18;
    }
    if ( xllm_memory_file_event_queue_count(pQueue) != 2u ) {
        fprintf(stderr, "unexpected queue count after coalesced rename/update\n");
        return 19;
    }
    if ( xllm_memory_file_event_queue_push_updated(pQueue, NULL, &tError) == XRT_NET_OK ) {
        fprintf(stderr, "expected invalid event push to fail\n");
        return 20;
    }
    xllm_error_reset(&tError);

    tDrainOptions.iMaxItems = 1u;
    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_file_event_queue_drain(pMemory, pQueue, &tDrainOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "third drain failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 21;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 1u ||
         xllm_memory_file_event_queue_count(pQueue) != 1u ) {
        fprintf(stderr, "unexpected third drain result\n");
        return 22;
    }

    tDrainOptions.iMaxItems = 0u;
    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_file_event_queue_drain(pMemory, pQueue, &tDrainOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "final drain failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 23;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 1u ||
         xllm_memory_file_event_queue_count(pQueue) != 0u ) {
        fprintf(stderr, "unexpected final drain result\n");
        return 24;
    }

    if ( xllm_memory_file_event_queue_push_updated(pQueue, sDeltaPath, &tError) != XRT_NET_OK ||
         xllm_memory_file_event_queue_push_updated(pQueue, sEpsilonPath, &tError) != XRT_NET_OK ||
         xllm_memory_file_event_queue_push_updated(pQueue, sDeltaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push non-adjacent updates failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 25;
    }
    if ( xllm_memory_file_event_queue_count(pQueue) != 3u ) {
        fprintf(stderr, "unexpected queue count before explicit compact\n");
        return 26;
    }
    if ( xllm_memory_file_event_queue_compact(pQueue, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue compact failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 27;
    }
    if ( xllm_memory_file_event_queue_count(pQueue) != 2u ) {
        fprintf(stderr, "unexpected queue count after explicit compact\n");
        return 28;
    }
    xllm_memory_change_set_reset(&tChanges);
    tDrainOptions.iMaxItems = 0u;
    iStatus = xllm_memory_file_event_queue_drain(pMemory, pQueue, &tDrainOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "compact drain failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 29;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 2u ||
         xllm_memory_file_event_queue_count(pQueue) != 0u ) {
        fprintf(stderr, "unexpected compact drain result\n");
        return 30;
    }

    if ( xllm_memory_file_event_queue_push_created(pQueue, sZetaPath, &tError) != XRT_NET_OK ||
         xllm_memory_file_event_queue_push_updated(pQueue, sEpsilonPath, &tError) != XRT_NET_OK ||
         xllm_memory_file_event_queue_push_deleted(pQueue, sZetaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "queue push auto-compact scenario failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 31;
    }
    if ( xllm_memory_file_event_queue_count(pQueue) != 3u ) {
        fprintf(stderr, "unexpected queue count before auto compact drain\n");
        return 32;
    }
    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_file_event_queue_drain(pMemory, pQueue, &tDrainOptions, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "auto compact drain failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 33;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_SKIPPED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 0u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_UPDATED) != 0u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 0u ||
         xllm_memory_file_event_queue_count(pQueue) != 0u ) {
        fprintf(stderr, "unexpected auto compact drain result\n");
        return 34;
    }

    printf("smoke_memory_file_event_queue ok\n");

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_file_event_queue_destroy(pQueue);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
