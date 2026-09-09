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

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_builtin_embedder_options tEmbedderOptions;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    xllm_memory_watcher_bridge *pBridge = NULL;
    xllm_memory_watcher_pump *pPump = NULL;
    xllm_memory_watcher_worker *pWorker = NULL;
    xllm_memory_watcher_bridge_options tBridgeOptions;
    xllm_memory_watcher_pump_options tPumpOptions;
    xllm_memory_watcher_worker_options tWorkerOptions;
    const char *sRootDir = "build\\smoke_memory_watcher_compact_pending_tmp";
    const char *sAlphaPath = "build\\smoke_memory_watcher_compact_pending_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_watcher_compact_pending_tmp\\beta.c";
    const char *sTempPath = "build\\smoke_memory_watcher_compact_pending_tmp\\temp.c";
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_error_init(&tError);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_builtin_embedder_options_init(&tEmbedderOptions);
    xllm_memory_watcher_bridge_options_init(&tBridgeOptions);
    xllm_memory_watcher_pump_options_init(&tPumpOptions);
    xllm_memory_watcher_worker_options_init(&tWorkerOptions);

    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sAlphaPath, "int alpha(void) { return 1; }\n") != 0 ||
         write_text_file(sBetaPath, "int beta(void) { return 2; }\n") != 0 ||
         write_text_file(sTempPath, "int temp(void) { return 3; }\n") != 0 ) {
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

    tMemoryOptions.sNamespace = "smoke-watcher-compact-pending";
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

    iStatus = xllm_memory_watcher_bridge_create(pMemory, &tBridgeOptions, &pBridge, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "bridge create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 6;
    }
    if ( xllm_memory_watcher_bridge_push_updated(pBridge, sAlphaPath, &tError) != XRT_NET_OK ||
         xllm_memory_watcher_bridge_push_updated(pBridge, sBetaPath, &tError) != XRT_NET_OK ||
         xllm_memory_watcher_bridge_push_updated(pBridge, sAlphaPath, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "bridge push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 7;
    }
    if ( xllm_memory_watcher_bridge_pending_count(pBridge) != 3u ) {
        fprintf(stderr, "unexpected bridge pending count before compact\n");
        return 8;
    }
    if ( xllm_memory_watcher_bridge_compact_pending(pBridge, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "bridge compact failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 9;
    }
    if ( xllm_memory_watcher_bridge_pending_count(pBridge) != 2u ) {
        fprintf(stderr, "unexpected bridge pending count after compact\n");
        return 10;
    }

    tPumpOptions.tBridgeOptions = tBridgeOptions;
    iStatus = xllm_memory_watcher_pump_create(pMemory, &tPumpOptions, &pPump, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "pump create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 11;
    }
    if ( xllm_memory_watcher_pump_push_created(pPump, sTempPath, NULL, NULL, &tError) != XRT_NET_OK ||
         xllm_memory_watcher_pump_push_updated(pPump, sBetaPath, NULL, NULL, &tError) != XRT_NET_OK ||
         xllm_memory_watcher_pump_push_deleted(pPump, sTempPath, NULL, NULL, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "pump push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 12;
    }
    if ( xllm_memory_watcher_pump_pending_count(pPump) != 3u ) {
        fprintf(stderr, "unexpected pump pending count before compact\n");
        return 13;
    }
    if ( xllm_memory_watcher_pump_compact_pending(pPump, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "pump compact failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 14;
    }
    if ( xllm_memory_watcher_pump_pending_count(pPump) != 1u ) {
        fprintf(stderr, "unexpected pump pending count after compact\n");
        return 15;
    }

    tWorkerOptions.tPumpOptions.tBridgeOptions = tBridgeOptions;
    tWorkerOptions.uDebounceMs = 0u;
    iStatus = xllm_memory_watcher_worker_create(pMemory, &tWorkerOptions, &pWorker, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "worker create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 16;
    }
    if ( xllm_memory_watcher_worker_push_updated(pWorker, sAlphaPath, NULL, NULL, &tError) != XRT_NET_OK ||
         xllm_memory_watcher_worker_push_updated(pWorker, sBetaPath, NULL, NULL, &tError) != XRT_NET_OK ||
         xllm_memory_watcher_worker_push_updated(pWorker, sAlphaPath, NULL, NULL, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "worker push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 17;
    }
    if ( xllm_memory_watcher_worker_pending_count(pWorker) != 3u ) {
        fprintf(stderr, "unexpected worker pending count before compact\n");
        return 18;
    }
    if ( xllm_memory_watcher_worker_compact_pending(pWorker, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "worker compact failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 19;
    }
    if ( xllm_memory_watcher_worker_pending_count(pWorker) != 2u ) {
        fprintf(stderr, "unexpected worker pending count after compact\n");
        return 20;
    }

    if ( xllm_memory_watcher_bridge_compact_pending(NULL, &tError) == XRT_NET_OK ) {
        fprintf(stderr, "expected null bridge compact to fail\n");
        return 21;
    }
    xllm_error_reset(&tError);
    if ( xllm_memory_watcher_pump_compact_pending(NULL, &tError) == XRT_NET_OK ) {
        fprintf(stderr, "expected null pump compact to fail\n");
        return 22;
    }
    xllm_error_reset(&tError);
    if ( xllm_memory_watcher_worker_compact_pending(NULL, &tError) == XRT_NET_OK ) {
        fprintf(stderr, "expected null worker compact to fail\n");
        return 23;
    }

    printf("smoke_memory_watcher_compact_pending ok\n");

    xllm_memory_watcher_worker_destroy(pWorker);
    xllm_memory_watcher_pump_destroy(pPump);
    xllm_memory_watcher_bridge_destroy(pBridge);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
