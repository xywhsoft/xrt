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
    xllm_memory_watcher_bridge_options tBridgeOptions;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xllm_memory_watcher_bridge *pBridge = NULL;
    const char *sRootDir = "build\\smoke_memory_watcher_bridge_tmp";
    const char *sAlphaPath = "build\\smoke_memory_watcher_bridge_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_watcher_bridge_tmp\\beta.c";
    const char *sGammaPath = "build\\smoke_memory_watcher_bridge_tmp\\gamma.c";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_watcher_bridge_options_init(&tBridgeOptions);
    xllm_memory_change_set_init(&tChanges);

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

    tMemoryOptions.sNamespace = "smoke-watcher-bridge";
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

    tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tBridgeOptions.tBaseOptions.sRootPath = sRootDir;
    tBridgeOptions.tBaseOptions.sRecordIdPrefix = "workspace";
    tBridgeOptions.tBaseOptions.sSourceUriPrefix = "workspace://";
    tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
    tBridgeOptions.tBaseOptions.sAllowedExtensions = ".c";
    tBridgeOptions.tBaseOptions.uMaxFileBytes = 4096u;
    tBridgeOptions.iDefaultMaxItems = 1u;

    iStatus = xllm_memory_watcher_bridge_create(pMemory, &tBridgeOptions, &pBridge, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "bridge create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 6;
    }

    if ( xllm_memory_watcher_bridge_push_created(pBridge, sAlphaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "bridge push alpha create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 7;
    }
    if ( xllm_memory_watcher_bridge_push_updated(pBridge, sAlphaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "bridge push alpha update failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 8;
    }
    if ( xllm_memory_watcher_bridge_push_created(pBridge, sBetaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "bridge push beta create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 9;
    }
    if ( xllm_memory_watcher_bridge_pending_count(pBridge) != 2u ) {
        fprintf(stderr, "unexpected bridge pending count after create/update coalescing\n");
        return 10;
    }

    iStatus = xllm_memory_watcher_bridge_flush(pBridge, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "bridge flush 1 failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 11;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         xllm_memory_watcher_bridge_pending_count(pBridge) != 1u ) {
        fprintf(stderr, "unexpected bridge first flush result\n");
        return 12;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_watcher_bridge_flush_max(pBridge, 0u, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "bridge flush 2 failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 13;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         xllm_memory_watcher_bridge_pending_count(pBridge) != 0u ) {
        fprintf(stderr, "unexpected bridge second flush result\n");
        return 14;
    }

    if ( rename(sAlphaPath, sGammaPath) != 0 ) {
        fprintf(stderr, "failed to rename alpha\n");
        return 15;
    }
    if ( remove(sBetaPath) != 0 ) {
        fprintf(stderr, "failed to remove beta\n");
        return 16;
    }

    if ( xllm_memory_watcher_bridge_push_renamed(pBridge, sAlphaPath, sGammaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "bridge push rename failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 17;
    }
    if ( xllm_memory_watcher_bridge_push_deleted(pBridge, sBetaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "bridge push delete failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 18;
    }
    if ( xllm_memory_watcher_bridge_pending_count(pBridge) != 2u ) {
        fprintf(stderr, "unexpected bridge pending count before rename/delete flush\n");
        return 19;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_watcher_bridge_flush_max(pBridge, 1u, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "bridge flush rename failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 20;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 1u ||
         xllm_memory_watcher_bridge_pending_count(pBridge) != 1u ) {
        fprintf(stderr, "unexpected bridge rename flush result\n");
        return 21;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_watcher_bridge_flush(pBridge, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "bridge final flush failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 22;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 1u ||
         xllm_memory_watcher_bridge_pending_count(pBridge) != 0u ) {
        fprintf(stderr, "unexpected bridge final flush result\n");
        return 23;
    }

    if ( xllm_memory_watcher_bridge_push_updated(NULL, sGammaPath, &tError) == XRT_NET_OK ) {
        fprintf(stderr, "expected invalid bridge push to fail\n");
        return 24;
    }
    xllm_error_reset(&tError);

    xllm_memory_watcher_bridge_push_created(pBridge, sGammaPath, &tError);
    xllm_memory_watcher_bridge_clear(pBridge);
    if ( xllm_memory_watcher_bridge_pending_count(pBridge) != 0u ) {
        fprintf(stderr, "bridge clear did not empty queue\n");
        return 25;
    }

    printf("smoke_memory_watcher_bridge ok\n");

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_watcher_bridge_destroy(pBridge);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
