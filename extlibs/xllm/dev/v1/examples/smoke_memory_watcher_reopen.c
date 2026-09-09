#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xllm-memory.h"

static int write_text_file(const char *sPath, const char *sText)
{
    FILE *pFile = fopen(sPath, "wb");
    size_t iLen = sText ? strlen(sText) : 0u;

    if ( !pFile ) {
        return 1;
    }
    if ( iLen > 0u && fwrite(sText, 1u, iLen, pFile) != iLen ) {
        fclose(pFile);
        return 2;
    }
    fclose(pFile);
    return 0;
}

static void remove_sqlite_files(const char *sDbPath)
{
    char sWalPath[260];
    char sShmPath[260];

    remove(sDbPath);
    snprintf(sWalPath, sizeof(sWalPath), "%s-wal", sDbPath);
    snprintf(sShmPath, sizeof(sShmPath), "%s-shm", sDbPath);
    remove(sWalPath);
    remove(sShmPath);
}

static int create_runtime_memory(
    const char *sNamespace,
    const char *sDbPath,
    xllm_runtime **ppRuntime,
    xllm_memory **ppMemory
)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_memory_options tMemoryOptions;
    int iStatus;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    *ppRuntime = NULL;
    *ppMemory = NULL;

    iStatus = xllm_runtime_create(&tRuntimeOptions, ppRuntime);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    tMemoryOptions.sNamespace = sNamespace;
    tMemoryOptions.sSqlitePath = sDbPath;
    tMemoryOptions.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
    tMemoryOptions.bEnableHybridSearch = false;
    tMemoryOptions.uDefaultChunkChars = 4096u;
    iStatus = xllm_memory_create(*ppRuntime, &tMemoryOptions, ppMemory);
    if ( iStatus != XRT_NET_OK ) {
        xllm_runtime_destroy(*ppRuntime);
        *ppRuntime = NULL;
    }
    return iStatus;
}

static int create_worker(
    xllm_memory *pMemory,
    const char *sRootDir,
    xllm_memory_watcher_worker **ppWorker,
    xllm_error *pError
)
{
    xllm_memory_watcher_worker_options tOptions;

    xllm_memory_watcher_worker_options_init(&tOptions);
    tOptions.tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = sRootDir;
    tOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = "workspace";
    tOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = "workspace://";
    tOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
    tOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = ".c";
    tOptions.tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = 4096u;
    tOptions.uDebounceMs = 0u;
    tOptions.iDefaultMaxItems = 16u;
    return xllm_memory_watcher_worker_create(pMemory, &tOptions, ppWorker, pError);
}

static void destroy_all(
    xllm_runtime **ppRuntime,
    xllm_memory **ppMemory,
    xllm_memory_watcher_worker **ppWorker
)
{
    if ( ppWorker && *ppWorker ) {
        xllm_memory_watcher_worker_destroy(*ppWorker);
        *ppWorker = NULL;
    }
    if ( ppMemory && *ppMemory ) {
        xllm_memory_destroy(*ppMemory);
        *ppMemory = NULL;
    }
    if ( ppRuntime && *ppRuntime ) {
        xllm_runtime_destroy(*ppRuntime);
        *ppRuntime = NULL;
    }
}

static int search_source(
    xllm_memory *pMemory,
    const char *sQuery,
    const char *sSourceUri,
    xllm_error *pError
)
{
    xllm_memory_search_options tOptions;
    xllm_memory_search_result tResult;
    size_t i;
    int iStatus;
    int iFound = 0;

    xllm_memory_search_options_init(&tOptions);
    memset(&tResult, 0, sizeof(tResult));
    tOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tOptions.sQuery = sQuery;
    tOptions.uMaxHits = 5u;
    tOptions.uMaxCharsPerHit = 1024u;

    iStatus = xllm_memory_search(pMemory, &tOptions, &tResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        return 1;
    }
    for ( i = 0u; i < tResult.iHitCount; ++i ) {
        if ( tResult.pHits[i].sSourceUri && strcmp(tResult.pHits[i].sSourceUri, sSourceUri) == 0 ) {
            iFound = 1;
            break;
        }
    }
    xllm_memory_search_result_reset(&tResult);
    return iFound ? 0 : 2;
}

int main(void)
{
    const char *sNamespace = "smoke-watcher-reopen";
    const char *sDbPath = "build\\smoke_memory_watcher_reopen.db";
    const char *sRootDir = "build\\smoke_memory_watcher_reopen_tmp";
    const char *sAlphaPath = "build\\smoke_memory_watcher_reopen_tmp\\alpha.c";
    const char *sBetaPath = "build\\smoke_memory_watcher_reopen_tmp\\beta.c";
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory_watcher_worker *pWorker = NULL;
    xllm_memory_change_set tChanges;
    xllm_memory_ingest_workspace_options tWorkspaceOptions;
    xllm_memory_sync_workspace_result tSyncResult;
    xllm_error tError;
    bool bFlushed = false;
    int iStatus;

    xllm_error_init(&tError);
    xllm_memory_change_set_init(&tChanges);
    xllm_memory_ingest_workspace_options_init(&tWorkspaceOptions);
    xllm_memory_sync_workspace_result_init(&tSyncResult);

    remove_sqlite_files(sDbPath);
    if ( xrtPathExists((str)sRootDir) ) {
        xrtDirDelete((str)sRootDir);
    }
    if ( !xrtDirCreateAll((str)sRootDir) ) {
        fprintf(stderr, "failed to create temp dir\n");
        return 1;
    }
    if ( write_text_file(sAlphaPath, "int alpha(void) { return 11; } /* watcher persisted alpha */\n") != 0 ||
         write_text_file(sBetaPath, "int beta(void) { return 22; } /* watcher recovered beta */\n") != 0 ) {
        fprintf(stderr, "failed to write files\n");
        return 2;
    }

    iStatus = create_runtime_memory(sNamespace, sDbPath, &pRuntime, &pMemory);
    if ( iStatus != XRT_NET_OK ||
         create_worker(pMemory, sRootDir, &pWorker, &tError) != XRT_NET_OK ) {
        fprintf(stderr, "initial worker create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 3;
    }

    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sAlphaPath, &bFlushed, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "alpha push failed\n");
        return 4;
    }
    iStatus = xllm_memory_watcher_worker_flush(pWorker, &tChanges, &tError);
    if ( iStatus != XRT_NET_OK ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != 1u ) {
        fprintf(stderr, "alpha flush failed\n");
        return 5;
    }
    xllm_memory_change_set_reset(&tChanges);

    iStatus = xllm_memory_watcher_worker_push_created(pWorker, sBetaPath, &bFlushed, NULL, &tError);
    if ( iStatus != XRT_NET_OK ||
         xllm_memory_watcher_worker_pending_count(pWorker) != 1u ) {
        fprintf(stderr, "beta pending push failed\n");
        return 6;
    }

    destroy_all(&pRuntime, &pMemory, &pWorker);

    iStatus = create_runtime_memory(sNamespace, sDbPath, &pRuntime, &pMemory);
    if ( iStatus != XRT_NET_OK ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != 1u ||
         search_source(pMemory, "persisted alpha", "workspace://alpha.c", &tError) != 0 ) {
        fprintf(stderr, "reopen after crash did not preserve alpha-only state\n");
        return 7;
    }

    tWorkspaceOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tWorkspaceOptions.sPath = sRootDir;
    tWorkspaceOptions.sRecordIdPrefix = "workspace";
    tWorkspaceOptions.sSourceUriPrefix = "workspace://";
    tWorkspaceOptions.sAllowedExtensions = ".c";
    tWorkspaceOptions.uMaxFileBytes = 4096u;
    iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceOptions, &tSyncResult, &tError);
    if ( iStatus != XRT_NET_OK ||
         tSyncResult.tIngest.uCreatedRecordCount != 1u ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != 2u ) {
        fprintf(stderr, "restart sync did not recover lost pending beta\n");
        return 8;
    }

    destroy_all(&pRuntime, &pMemory, &pWorker);
    iStatus = create_runtime_memory(sNamespace, sDbPath, &pRuntime, &pMemory);
    if ( iStatus != XRT_NET_OK ||
         xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) != 2u ||
         search_source(pMemory, "recovered beta", "workspace://beta.c", &tError) != 0 ) {
        fprintf(stderr, "final reopen did not preserve recovered beta\n");
        return 9;
    }

    printf("smoke_memory_watcher_reopen ok\n");

    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    xllm_memory_change_set_reset(&tChanges);
    destroy_all(&pRuntime, &pMemory, &pWorker);
    xllm_error_free(&tError);
    return 0;
}
