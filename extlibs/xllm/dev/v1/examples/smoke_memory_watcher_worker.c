#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

static void smoke_sleep_ms(uint32 uMillis)
{
    if ( uMillis == 0u ) {
        return;
    }
    xrtSleep(uMillis);
}

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
    xllm_memory_watcher_worker_options tWorkerOptions;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xllm_memory_watcher_worker *pWorker = NULL;
    const char *sRootDir = "build\\smoke_memory_watcher_worker_tmp";
    const char *sAlphaPath = "build\\smoke_memory_watcher_worker_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_watcher_worker_tmp\\beta.c";
    const char *sGammaPath = "build\\smoke_memory_watcher_worker_tmp\\gamma.c";
    bool bFlushed = false;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_watcher_worker_options_init(&tWorkerOptions);
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
         write_text_file(sGammaPath, "int gamma(void) { return 3; }\n") != 0 ) {
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

    tMemoryOptions.sNamespace = "smoke-watcher-worker";
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

    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = sRootDir;
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = "workspace";
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = "workspace://";
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = ".c";
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = 4096u;
    tWorkerOptions.uDebounceMs = 40u;
    tWorkerOptions.iDefaultMaxItems = 1u;

    iStatus = xllm_memory_watcher_worker_create(pMemory, &tWorkerOptions, &pWorker, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "worker create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 6;
    }

    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sAlphaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed || xllm_memory_watcher_worker_pending_count(pWorker) != 1u ) {
        fprintf(stderr, "unexpected first worker push result\n");
        return 7;
    }

    iStatus = xllm_memory_watcher_worker_poll(pWorker, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed || tChanges.iChangeCount != 0u ||
         xllm_memory_watcher_worker_pending_count(pWorker) != 1u ) {
        fprintf(stderr, "unexpected early worker poll result\n");
        return 8;
    }

    smoke_sleep_ms(60u);
    iStatus = xllm_memory_watcher_worker_poll(pWorker, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || !bFlushed ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         xllm_memory_watcher_worker_pending_count(pWorker) != 0u ) {
        fprintf(stderr, "unexpected debounced poll result\n");
        return 9;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sBetaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed ) {
        fprintf(stderr, "unexpected second batch first push result\n");
        return 10;
    }
    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sGammaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed || xllm_memory_watcher_worker_pending_count(pWorker) != 2u ) {
        fprintf(stderr, "unexpected second batch push result\n");
        return 11;
    }

    smoke_sleep_ms(60u);
    iStatus = xllm_memory_watcher_worker_poll_max(pWorker, 1u, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || !bFlushed ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         xllm_memory_watcher_worker_pending_count(pWorker) != 1u ) {
        fprintf(stderr, "unexpected partial poll result\n");
        return 12;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_watcher_worker_poll(pWorker, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || !bFlushed ||
         count_change_kind(&tChanges, XLLM_MEMORY_CHANGE_CREATED) != 1u ||
         xllm_memory_watcher_worker_pending_count(pWorker) != 0u ) {
        fprintf(stderr, "unexpected second partial poll result\n");
        return 13;
    }

    xllm_memory_change_set_reset(&tChanges);
    iStatus = xllm_memory_watcher_worker_push_created(NULL, sAlphaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "expected invalid worker push to fail\n");
        return 14;
    }
    xllm_error_reset(&tError);

    printf("smoke_memory_watcher_worker ok\n");

    xllm_memory_change_set_reset(&tChanges);
    xllm_memory_watcher_worker_destroy(pWorker);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
