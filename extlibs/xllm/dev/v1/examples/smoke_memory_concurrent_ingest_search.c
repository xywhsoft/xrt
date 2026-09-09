#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

enum {
    SMOKE_DIRECT_RECORDS = 32,
    SMOKE_SEARCH_ITERATIONS = 80
};

typedef struct {
    xllm_memory *pMemory;
    volatile int iReady;
    volatile int iStatus;
    uint32 uOps;
    char sMessage[256];
} smoke_ingest_thread;

typedef struct {
    xllm_memory *pMemory;
    volatile int iStatus;
    uint32 uOps;
    uint32 uHits;
    char sMessage[256];
} smoke_search_thread;

static void set_thread_error(char *sMessage, size_t iMessageSize, const char *sPrefix, const xllm_error *pError)
{
    if ( !sMessage || iMessageSize == 0u ) {
        return;
    }
    snprintf(
        sMessage,
        iMessageSize,
        "%s: %s",
        sPrefix ? sPrefix : "error",
        pError && pError->sMessage ? pError->sMessage : "(null)"
    );
}

static int write_text_file(const char *sPath, const char *sText)
{
    FILE *pFile;
    size_t iLen;

    pFile = fopen(sPath, "wb");
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

static uint32 ingest_thread_proc(ptr pParam)
{
    smoke_ingest_thread *pState = (smoke_ingest_thread *)pParam;
    xllm_memory_ingest_options tOptions;
    xllm_error tError;
    char sRecordId[64];
    char sSourceUri[96];
    char sTitle[80];
    char sText[256];
    uint32 i;
    int iStatus;

    if ( !pState || !pState->pMemory ) {
        return 1u;
    }

    xllm_error_init(&tError);
    pState->iReady = 1;

    for ( i = 0u; i < SMOKE_DIRECT_RECORDS; ++i ) {
        snprintf(sRecordId, sizeof(sRecordId), "concurrent-direct-%02u", (unsigned)i);
        snprintf(sSourceUri, sizeof(sSourceUri), "concurrent://direct/%02u", (unsigned)i);
        snprintf(sTitle, sizeof(sTitle), "Concurrent direct %02u", (unsigned)i);
        snprintf(
            sText,
            sizeof(sText),
            "Concurrent ingest record %02u contains shared concurrent anchor token and direct marker direct-%02u.",
            (unsigned)i,
            (unsigned)i
        );

        xllm_memory_ingest_options_init(&tOptions);
        tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tOptions.sRecordId = sRecordId;
        tOptions.sSourceUri = sSourceUri;
        tOptions.sTitle = sTitle;
        tOptions.sText = sText;
        tOptions.bReplaceExisting = true;
        tOptions.uChunkChars = 256u;

        iStatus = xllm_memory_ingest_text(pState->pMemory, &tOptions, &tError);
        if ( iStatus != XRT_NET_OK ) {
            pState->iStatus = iStatus;
            set_thread_error(pState->sMessage, sizeof(pState->sMessage), "ingest failed", &tError);
            return 2u;
        }

        pState->uOps += 1u;
        xrtSleep(1u);
    }

    pState->iStatus = XRT_NET_OK;
    return 0u;
}

static uint32 search_thread_proc(ptr pParam)
{
    smoke_search_thread *pState = (smoke_search_thread *)pParam;
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tResult;
    xllm_error tError;
    uint32 i;
    int iStatus;

    if ( !pState || !pState->pMemory ) {
        return 1u;
    }

    xllm_error_init(&tError);
    for ( i = 0u; i < SMOKE_SEARCH_ITERATIONS; ++i ) {
        memset(&tResult, 0, sizeof(tResult));
        xllm_memory_search_options_init(&tOptions);
        tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tOptions.sQuery = "shared concurrent anchor token";
        tOptions.uMaxHits = 6u;
        tOptions.uMaxCharsPerHit = 256u;

        iStatus = xllm_memory_search(pState->pMemory, &tOptions, &tResult, &tError);
        if ( iStatus != XRT_NET_OK ) {
            pState->iStatus = iStatus;
            set_thread_error(pState->sMessage, sizeof(pState->sMessage), "search failed", &tError);
            xllm_memory_search_result_reset(&tResult);
            return 2u;
        }

        if ( tResult.iHitCount > 0u ) {
            pState->uHits += 1u;
        }
        pState->uOps += 1u;
        xllm_memory_search_result_reset(&tResult);
        xrtSleep(1u);
    }

    pState->iStatus = XRT_NET_OK;
    return 0u;
}

static int run_watcher_worker_writes(xllm_memory *pMemory, xllm_error *pError)
{
    xllm_memory_watcher_worker *pWorker = NULL;
    xllm_memory_watcher_worker_options tWorkerOptions;
    xllm_memory_watcher_worker_loop_options tLoopOptions;
    xllm_memory_watcher_worker_loop_result tLoopResult;
    const char *sRootDir = "build\\smoke_memory_concurrent_ingest_search_tmp";
    const char *sAlphaPath = "build\\smoke_memory_concurrent_ingest_search_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_concurrent_ingest_search_tmp\\beta.c";
    const char *sGammaPath = "build\\smoke_memory_concurrent_ingest_search_tmp\\gamma.c";
    bool bFlushed = false;
    int iStatus;

    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create watcher temp dir\n");
        return XRT_NET_ERROR;
    }
    if ( write_text_file(sAlphaPath, "int alpha(void){return 1;} /* watcher concurrent anchor alpha */\n") != 0 ||
         write_text_file(sBetaPath, "int beta(void){return 2;} /* watcher concurrent anchor beta */\n") != 0 ||
         write_text_file(sGammaPath, "int gamma(void){return 3;} /* watcher concurrent anchor gamma */\n") != 0 ) {
        fprintf(stderr, "failed to write watcher temp files\n");
        return XRT_NET_ERROR;
    }

    xllm_memory_watcher_worker_options_init(&tWorkerOptions);
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = sRootDir;
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = "watcher";
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = "watcher://";
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = ".c";
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = 4096u;
    tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
    tWorkerOptions.uDebounceMs = 0u;
    tWorkerOptions.iDefaultMaxItems = 3u;

    iStatus = xllm_memory_watcher_worker_create(pMemory, &tWorkerOptions, &pWorker, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sAlphaPath, &bFlushed, NULL, pError);
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm_memory_watcher_worker_push_created(pWorker, sBetaPath, &bFlushed, NULL, pError);
    }
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm_memory_watcher_worker_push_created(pWorker, sGammaPath, &bFlushed, NULL, pError);
    }
    if ( iStatus != XRT_NET_OK ) {
        xllm_memory_watcher_worker_destroy(pWorker);
        return iStatus;
    }

    xllm_memory_watcher_worker_loop_options_init(&tLoopOptions);
    xllm_memory_watcher_worker_loop_result_init(&tLoopResult);
    tLoopOptions.tRunOptions.iMaxItemsPerBatch = 1u;
    tLoopOptions.uSleepMs = 1u;
    tLoopOptions.uMaxWaitMs = 50u;

    iStatus = xllm_memory_watcher_worker_run_loop(pWorker, &tLoopOptions, NULL, NULL, &tLoopResult, pError);
    if ( iStatus != XRT_NET_OK ||
         tLoopResult.iBatchCount != 3u ||
         tLoopResult.iChangeCount != 3u ||
         tLoopResult.iPendingCount != 0u ) {
        fprintf(stderr, "unexpected watcher loop result\n");
        if ( iStatus == XRT_NET_OK ) {
            iStatus = XRT_NET_ERROR;
        }
    }

    xllm_memory_watcher_worker_destroy(pWorker);
    return iStatus;
}

static int verify_final_search(xllm_memory *pMemory, const char *sQuery)
{
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tResult;
    xllm_error tError;
    int iStatus;
    int iRc = 1;

    memset(&tResult, 0, sizeof(tResult));
    xllm_error_init(&tError);
    xllm_memory_search_options_init(&tOptions);
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sQuery = sQuery;
    tOptions.uMaxHits = 8u;
    tOptions.uMaxCharsPerHit = 256u;

    iStatus = xllm_memory_search(pMemory, &tOptions, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "final search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( tResult.iHitCount == 0u ) {
        fprintf(stderr, "final search returned no hits for %s\n", sQuery);
        goto cleanup;
    }

    iRc = 0;

cleanup:
    xllm_memory_search_result_reset(&tResult);
    return iRc;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory *pMemory = NULL;
    xllm_error tError;
    smoke_ingest_thread tIngest;
    smoke_search_thread tSearch;
    xthread hIngestThread = NULL;
    xthread hSearchThread = NULL;
    size_t iKnowledgeRecords;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_error_init(&tError);
    memset(&tIngest, 0, sizeof(tIngest));
    memset(&tSearch, 0, sizeof(tSearch));

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    tMemoryOptions.sNamespace = "smoke-memory-concurrent-ingest-search";
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 256u;

    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    tIngest.pMemory = pMemory;
    tSearch.pMemory = pMemory;
    hIngestThread = xrtThreadCreate((ptr)ingest_thread_proc, &tIngest, 0u);
    hSearchThread = xrtThreadCreate((ptr)search_thread_proc, &tSearch, 0u);
    if ( !hIngestThread || !hSearchThread ) {
        fprintf(stderr, "thread create failed\n");
        goto cleanup;
    }

    while ( !tIngest.iReady ) {
        xrtSleep(1u);
    }

    iStatus = run_watcher_worker_writes(pMemory, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "watcher writes failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xrtThreadWait(hIngestThread);
    xrtThreadWait(hSearchThread);

    if ( tIngest.iStatus != XRT_NET_OK ) {
        fprintf(stderr, "%s\n", tIngest.sMessage[0] ? tIngest.sMessage : "ingest thread failed");
        goto cleanup;
    }
    if ( tSearch.iStatus != XRT_NET_OK ) {
        fprintf(stderr, "%s\n", tSearch.sMessage[0] ? tSearch.sMessage : "search thread failed");
        goto cleanup;
    }
    if ( tIngest.uOps != SMOKE_DIRECT_RECORDS || tSearch.uOps != SMOKE_SEARCH_ITERATIONS ) {
        fprintf(stderr, "unexpected thread op counts\n");
        goto cleanup;
    }
    if ( tSearch.uHits == 0u ) {
        fprintf(stderr, "search thread never observed concurrent hits\n");
        goto cleanup;
    }

    iKnowledgeRecords = xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE);
    if ( iKnowledgeRecords < (size_t)SMOKE_DIRECT_RECORDS + 3u ) {
        fprintf(stderr, "expected direct and watcher records, got %llu\n", (unsigned long long)iKnowledgeRecords);
        goto cleanup;
    }

    if ( verify_final_search(pMemory, "direct marker direct-31") != 0 ||
         verify_final_search(pMemory, "watcher concurrent anchor gamma") != 0 ) {
        goto cleanup;
    }

    iRc = 0;

cleanup:
    if ( hIngestThread ) {
        xrtThreadWait(hIngestThread);
        xrtThreadDestroy(hIngestThread);
    }
    if ( hSearchThread ) {
        xrtThreadWait(hSearchThread);
        xrtThreadDestroy(hSearchThread);
    }
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    return iRc;
}
