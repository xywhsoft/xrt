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

static int build_worker(
    xllm_runtime **ppRuntime,
    xllm_memory **ppMemory,
    xllm_memory_watcher_worker **ppWorker,
    const char *sNamespace,
    const char *sRootDir,
    xllm_error *pError
)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_memory_watcher_worker_options tWorkerOptions;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_watcher_worker_options_init(&tWorkerOptions);

    *ppRuntime = NULL;
    *ppMemory = NULL;
    *ppWorker = NULL;

    iStatus = xllm_runtime_create(&tRuntimeOptions, ppRuntime);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    tEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
    tEmbedderOptions.uHashDimensions = 32u;
    iStatus = xllm_memory_make_builtin_embedder(&tEmbedderOptions, &tMemoryOptions.tEmbedder, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    tMemoryOptions.sNamespace = sNamespace;
    tMemoryOptions.bEnableHybridSearch = true;
    tMemoryOptions.tVectorWeight.bSet = true;
    tMemoryOptions.tVectorWeight.fValue = 1.0;
    iStatus = xllm_memory_create(*ppRuntime, &tMemoryOptions, ppMemory);
    xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
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

    return xllm_memory_watcher_worker_create(*ppMemory, &tWorkerOptions, ppWorker, pError);
}

static void cleanup_worker(
    xllm_runtime *pRuntime,
    xllm_memory *pMemory,
    xllm_memory_watcher_worker *pWorker
)
{
    xllm_memory_watcher_worker_destroy(pWorker);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_watcher_worker *pWorker = NULL;
    xllm_memory_watcher_worker_state tState;
    xllm_memory_change_set tChanges;
    xllm_error tError;
    const char *sRootDir = "build\\smoke_memory_watcher_worker_state_tmp";
    const char *sAlphaPath = "build\\smoke_memory_watcher_worker_state_tmp\\alpha.c";
    bool bFlushed = false;
    int iStatus;

    xllm_error_init(&tError);
    xllm_memory_watcher_worker_state_init(&tState);
    xllm_memory_change_set_init(&tChanges);

    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sAlphaPath, "int alpha(void){return 1;}\n") != 0 ) {
        fprintf(stderr, "failed to write temp file\n");
        return 2;
    }

    iStatus = build_worker(&pRuntime, &pMemory, &pWorker, "smoke-watcher-worker-state", sRootDir, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "build worker failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 3;
    }

    iStatus = xllm_memory_watcher_worker_get_state(pWorker, &tState, &tError);
    if ( iStatus != XRT_NET_OK ||
         tState.iPendingCount != 0u ||
         tState.bReady ||
         tState.bBlockedByDebounce ||
         tState.uWaitMsRemaining != 0u ) {
        fprintf(stderr, "unexpected empty worker state\n");
        return 4;
    }

    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sAlphaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed ) {
        fprintf(stderr, "unexpected push result\n");
        return 5;
    }
    xllm_memory_change_set_reset(&tChanges);

    iStatus = xllm_memory_watcher_worker_get_state(pWorker, &tState, &tError);
    if ( iStatus != XRT_NET_OK ||
         tState.iPendingCount != 1u ||
         tState.bReady ||
         !tState.bBlockedByDebounce ||
         tState.uDebounceMs != 40u ||
         tState.uWaitMsRemaining == 0u ) {
        fprintf(stderr, "unexpected blocked worker state\n");
        return 6;
    }

    xrtSleep(60u);
    iStatus = xllm_memory_watcher_worker_get_state(pWorker, &tState, &tError);
    if ( iStatus != XRT_NET_OK ||
         tState.iPendingCount != 1u ||
         !tState.bReady ||
         tState.bBlockedByDebounce ||
         tState.uWaitMsRemaining != 0u ) {
        fprintf(stderr, "unexpected ready worker state\n");
        return 7;
    }

    iStatus = xllm_memory_watcher_worker_flush(pWorker, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "unexpected flush failure\n");
        return 8;
    }
    xllm_memory_change_set_reset(&tChanges);

    iStatus = xllm_memory_watcher_worker_get_state(pWorker, &tState, &tError);
    if ( iStatus != XRT_NET_OK ||
         tState.iPendingCount != 0u ||
         tState.bReady ||
         tState.bBlockedByDebounce ||
         tState.uWaitMsRemaining != 0u ) {
        fprintf(stderr, "unexpected post-flush worker state\n");
        return 9;
    }

    iStatus = xllm_memory_watcher_worker_get_state(NULL, &tState, &tError);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "expected invalid get_state call to fail\n");
        return 10;
    }

    printf("smoke_memory_watcher_worker_state ok\n");
    xllm_memory_change_set_reset(&tChanges);
    cleanup_worker(pRuntime, pMemory, pWorker);
    xllm_error_free(&tError);
    return 0;
}
