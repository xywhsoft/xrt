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
    xllm_memory_watcher_pump_options tPumpOptions;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xllm_memory_watcher_pump *pPump = NULL;
    const char *sRootDir = "build\\smoke_memory_watcher_pump_tmp";
    const char *sAlphaPath = "build\\smoke_memory_watcher_pump_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_watcher_pump_tmp\\beta.c";
    const char *sGammaPath = "build\\smoke_memory_watcher_pump_tmp\\gamma.c";
    bool bFlushed = false;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_watcher_pump_options_init(&tPumpOptions);
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

    tMemoryOptions.sNamespace = "smoke-watcher-pump";
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

    tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = sRootDir;
    tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = "workspace";
    tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = "workspace://";
    tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
    tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = ".c";
    tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = 4096u;
    tPumpOptions.iAutoFlushThreshold = 2u;

    iStatus = xllm_memory_watcher_pump_create(pMemory, &tPumpOptions, &pPump, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "pump create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 6;
    }

    iStatus = xllm_memory_watcher_pump_push_created(pPump, sAlphaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed || xllm_memory_watcher_pump_pending_count(pPump) != 1u ) {
        fprintf(stderr, "unexpected first pump push result\n");
        return 7;
    }

    iStatus = xllm_memory_watcher_pump_push_created(pPump, sBetaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || !bFlushed ) {
        fprintf(stderr, "unexpected second pump push result\n");
        return 8;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 2u ||
         xllm_memory_watcher_pump_pending_count(pPump) != 0u ) {
        fprintf(stderr, "unexpected auto flush change set\n");
        return 9;
    }

    xllm_memory_change_set_reset(&tChanges);
    if ( rename(sAlphaPath, sGammaPath) != 0 ) {
        fprintf(stderr, "failed to rename alpha\n");
        return 10;
    }
    iStatus = xllm_memory_watcher_pump_push_renamed(pPump, sAlphaPath, sGammaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed ) {
        fprintf(stderr, "unexpected rename pump push result\n");
        return 11;
    }
    if ( remove(sBetaPath) != 0 ) {
        fprintf(stderr, "failed to remove beta\n");
        return 12;
    }
    iStatus = xllm_memory_watcher_pump_push_deleted(pPump, sBetaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || !bFlushed ) {
        fprintf(stderr, "unexpected delete pump push result\n");
        return 13;
    }
    if ( count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_REMOVED) != 2u ||
         xllm_memory_watcher_pump_pending_count(pPump) != 0u ) {
        fprintf(stderr, "unexpected auto flush rename/delete result\n");
        return 14;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_watcher_pump_push_updated(NULL, sGammaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "expected invalid pump push to fail\n");
        return 15;
    }
    xllm_error_reset(&tError);

    iStatus = xllm_memory_watcher_pump_push_created(pPump, sGammaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "pump push before clear failed\n");
        return 16;
    }
    xllm_memory_watcher_pump_clear(pPump);
    if ( xllm_memory_watcher_pump_pending_count(pPump) != 0u ) {
        fprintf(stderr, "pump clear did not empty queue\n");
        return 17;
    }

    printf("smoke_memory_watcher_pump ok\n");

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_watcher_pump_destroy(pPump);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
