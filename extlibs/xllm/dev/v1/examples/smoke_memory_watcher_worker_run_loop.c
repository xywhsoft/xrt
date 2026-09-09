#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

typedef struct {
    size_t iBatchCount;
    size_t iChangeCount;
    size_t iCreatedCount;
} smoke_loop_batch_stats;

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

static int on_batch(
    void *pCtx,
    const xllm_memory_change_set *pChanges,
    xllm_error *pError
)
{
    smoke_loop_batch_stats *pStats = (smoke_loop_batch_stats *)pCtx;
    size_t i;

    (void)pError;
    if ( !pStats || !pChanges ) {
        return XRT_NET_ERROR;
    }

    pStats->iBatchCount += 1u;
    pStats->iChangeCount += pChanges->iChangeCount;
    for ( i = 0u; i < pChanges->iChangeCount; ++i ) {
        if ( pChanges->pChanges[i].eKind == XLLM_MEMORY_CHANGE_CREATED ) {
            pStats->iCreatedCount += 1u;
        }
    }
    return XRT_NET_OK;
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
    xllm_memory_watcher_worker_loop_options tLoopOptions;
    xllm_memory_watcher_worker_loop_result tLoopResult;
    xllm_memory_change_set tFlushChanges;
    xllm_error tError;
    smoke_loop_batch_stats tStats;
    const char *sRootDir = "build\\smoke_memory_watcher_worker_run_loop_tmp";
    const char *sAlphaPath = "build\\smoke_memory_watcher_worker_run_loop_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_watcher_worker_run_loop_tmp\\beta.c";
    const char *sGammaPath = "build\\smoke_memory_watcher_worker_run_loop_tmp\\gamma.c";
    const char *sDeltaPath = "build\\smoke_memory_watcher_worker_run_loop_tmp\\delta.c";
    const char *sEpsilonPath = "build\\smoke_memory_watcher_worker_run_loop_tmp\\epsilon.c";
    const char *sZetaPath = "build\\smoke_memory_watcher_worker_run_loop_tmp\\zeta.c";
    bool bFlushed = false;
    int iStatus;

    xllm_error_init(&tError);
    xllm_memory_watcher_worker_loop_options_init(&tLoopOptions);
    xllm_memory_watcher_worker_loop_result_init(&tLoopResult);
    xllm_memory_change_set_init(&tFlushChanges);
    memset(&tStats, 0, sizeof(tStats));

    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sAlphaPath, "int alpha(void){return 1;}\n") != 0 ||
         write_text_file(sBetaPath, "int beta(void){return 2;}\n") != 0 ||
         write_text_file(sGammaPath, "int gamma(void){return 3;}\n") != 0 ||
         write_text_file(sDeltaPath, "int delta(void){return 4;}\n") != 0 ||
         write_text_file(sEpsilonPath, "int epsilon(void){return 5;}\n") != 0 ||
         write_text_file(sZetaPath, "int zeta(void){return 6;}\n") != 0 ) {
        fprintf(stderr, "failed to write temp files\n");
        return 2;
    }

    iStatus = build_worker(&pRuntime, &pMemory, &pWorker, "smoke-watcher-worker-run-loop", sRootDir, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "build worker failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 3;
    }

    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sAlphaPath, &bFlushed, NULL, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed ) {
        fprintf(stderr, "unexpected alpha push result\n");
        return 4;
    }
    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sBetaPath, &bFlushed, NULL, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed || xllm_memory_watcher_worker_pending_count(pWorker) != 2u ) {
        fprintf(stderr, "unexpected beta push result\n");
        return 5;
    }

    tLoopOptions.tRunOptions.iMaxItemsPerBatch = 1u;
    tLoopOptions.uSleepMs = 20u;
    tLoopOptions.uMaxWaitMs = 100u;
    iStatus = xllm_memory_watcher_worker_run_loop(pWorker, &tLoopOptions, on_batch, &tStats, &tLoopResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tLoopResult.iBatchCount != 2u ||
         tLoopResult.iChangeCount != 2u ||
         tLoopResult.iPendingCount != 0u ||
         tLoopResult.uLoopCount < 2u ||
         tLoopResult.uWaitedMs < 40u ||
         tLoopResult.bBlockedByDebounce ||
         tLoopResult.bStoppedByBatchLimit ||
         tLoopResult.bTimedOut ||
         tStats.iBatchCount != 2u ||
         tStats.iCreatedCount != 2u ) {
        fprintf(stderr, "unexpected draining run_loop result\n");
        return 6;
    }

    memset(&tStats, 0, sizeof(tStats));
    xllm_memory_watcher_worker_loop_result_init(&tLoopResult);
    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sGammaPath, &bFlushed, NULL, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed ) {
        fprintf(stderr, "unexpected gamma push result\n");
        return 7;
    }
    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sDeltaPath, &bFlushed, NULL, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed || xllm_memory_watcher_worker_pending_count(pWorker) != 2u ) {
        fprintf(stderr, "unexpected delta push result\n");
        return 8;
    }

    tLoopOptions.tRunOptions.iMaxBatches = 1u;
    tLoopOptions.tRunOptions.iMaxItemsPerBatch = 1u;
    tLoopOptions.uSleepMs = 20u;
    tLoopOptions.uMaxWaitMs = 100u;
    iStatus = xllm_memory_watcher_worker_run_loop(pWorker, &tLoopOptions, on_batch, &tStats, &tLoopResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tLoopResult.iBatchCount != 1u ||
         tLoopResult.iChangeCount != 1u ||
         tLoopResult.iPendingCount != 1u ||
         !tLoopResult.bStoppedByBatchLimit ||
         tLoopResult.bBlockedByDebounce ||
         tLoopResult.bTimedOut ||
         tStats.iBatchCount != 1u ||
         tStats.iCreatedCount != 1u ) {
        fprintf(stderr, "unexpected batch-limited run_loop result\n");
        return 9;
    }
    iStatus = xllm_memory_watcher_worker_flush(pWorker, &tFlushChanges, &tError);
    if ( iStatus != XRT_NET_OK || xllm_memory_watcher_worker_pending_count(pWorker) != 0u ) {
        fprintf(stderr, "failed to cleanup after batch-limited run\n");
        return 10;
    }
    xllm_memory_change_set_reset(&tFlushChanges);

    memset(&tStats, 0, sizeof(tStats));
    xllm_memory_watcher_worker_loop_result_init(&tLoopResult);
    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sEpsilonPath, &bFlushed, NULL, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed ) {
        fprintf(stderr, "unexpected epsilon push result\n");
        return 11;
    }
    tLoopOptions.tRunOptions.iMaxBatches = 0u;
    tLoopOptions.tRunOptions.iMaxItemsPerBatch = 0u;
    tLoopOptions.uSleepMs = 10u;
    tLoopOptions.uMaxWaitMs = 20u;
    tLoopOptions.bForceFlushOnTimeout = false;
    iStatus = xllm_memory_watcher_worker_run_loop(pWorker, &tLoopOptions, on_batch, &tStats, &tLoopResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tLoopResult.iBatchCount != 0u ||
         tLoopResult.iChangeCount != 0u ||
         tLoopResult.iPendingCount != 1u ||
         !tLoopResult.bBlockedByDebounce ||
         tLoopResult.bStoppedByBatchLimit ||
         !tLoopResult.bTimedOut ||
         tStats.iBatchCount != 0u ) {
        fprintf(stderr, "unexpected timeout run_loop result\n");
        return 12;
    }

    iStatus = xllm_memory_watcher_worker_flush(pWorker, &tFlushChanges, &tError);
    if ( iStatus != XRT_NET_OK || xllm_memory_watcher_worker_pending_count(pWorker) != 0u ) {
        fprintf(stderr, "failed to cleanup after timeout run\n");
        return 13;
    }
    xllm_memory_change_set_reset(&tFlushChanges);

    memset(&tStats, 0, sizeof(tStats));
    xllm_memory_watcher_worker_loop_result_init(&tLoopResult);
    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sZetaPath, &bFlushed, NULL, &tError);
    if ( iStatus != XRT_NET_OK || bFlushed ) {
        fprintf(stderr, "unexpected zeta push result\n");
        return 14;
    }
    tLoopOptions.bForceFlushOnTimeout = true;
    iStatus = xllm_memory_watcher_worker_run_loop(pWorker, &tLoopOptions, on_batch, &tStats, &tLoopResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tLoopResult.iBatchCount != 1u ||
         tLoopResult.iChangeCount != 1u ||
         tLoopResult.iPendingCount != 0u ||
         !tLoopResult.bBlockedByDebounce ||
         !tLoopResult.bTimedOut) {
        fprintf(stderr,
                "unexpected force-flush timeout run_loop result: status=%d batches=%u changes=%u pending=%u loops=%u waited=%u blocked=%d batchlimit=%d timeout=%d stats_batches=%u stats_changes=%u stats_created=%u\n",
                iStatus,
                (unsigned)tLoopResult.iBatchCount,
                (unsigned)tLoopResult.iChangeCount,
                (unsigned)tLoopResult.iPendingCount,
                (unsigned)tLoopResult.uLoopCount,
                (unsigned)tLoopResult.uWaitedMs,
                tLoopResult.bBlockedByDebounce ? 1 : 0,
                tLoopResult.bStoppedByBatchLimit ? 1 : 0,
                tLoopResult.bTimedOut ? 1 : 0,
                (unsigned)tStats.iBatchCount,
                (unsigned)tStats.iChangeCount,
                (unsigned)tStats.iCreatedCount);
        return 15;
    }

    iStatus = xllm_memory_watcher_worker_run_loop(NULL, &tLoopOptions, on_batch, &tStats, &tLoopResult, &tError);
    if ( iStatus == XRT_NET_OK ) {
        fprintf(stderr, "expected invalid run_loop call to fail\n");
        return 16;
    }

    printf("smoke_memory_watcher_worker_run_loop ok\n");
    xllm_memory_change_set_reset(&tFlushChanges);
    cleanup_worker(pRuntime, pMemory, pWorker);
    xllm_error_free(&tError);
    return 0;
}
