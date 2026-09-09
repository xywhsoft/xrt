#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "xllm-memory.h"

static void print_hits(const xllm_memory_search_result *pResult)
{
    size_t i;

    printf("hits: %u\n", (unsigned)(pResult ? pResult->iHitCount : 0u));
    if ( !pResult ) {
        return;
    }

    for ( i = 0u; i < pResult->iHitCount; ++i ) {
        const xllm_memory_hit *pHit = &pResult->pHits[i];
        printf("[%u] scope=%d score=%.3f record=%s chunk=%s\n",
               (unsigned)(i + 1u),
               (int)pHit->eScope,
               pHit->fScore,
               pHit->sRecordId ? pHit->sRecordId : "(null)",
               pHit->sChunkId ? pHit->sChunkId : "(null)");
        if ( pHit->sText ) {
            printf("%s\n\n", pHit->sText);
        }
    }
}

static void print_records(const xllm_memory_record_list_result *pResult)
{
    size_t i;

    printf("records: %u\n", (unsigned)(pResult ? pResult->iRecordCount : 0u));
    if ( !pResult ) {
        return;
    }
    for ( i = 0u; i < pResult->iRecordCount; ++i ) {
        const xllm_memory_record_info *pInfo = &pResult->pRecords[i];
        printf("[%u] scope=%d record=%s title=%s source=%s chunks=%u text_chars=%u\n",
               (unsigned)(i + 1u),
               (int)pInfo->eScope,
               pInfo->sRecordId ? pInfo->sRecordId : "(null)",
               pInfo->sTitle ? pInfo->sTitle : "(null)",
               pInfo->sSourceUri ? pInfo->sSourceUri : "(null)",
               (unsigned)pInfo->uChunkCount,
               (unsigned)pInfo->iTextLength);
    }
}

static void print_chunks(const xllm_memory_chunk_list_result *pResult)
{
    size_t i;

    printf("chunks: %u\n", (unsigned)(pResult ? pResult->iChunkCount : 0u));
    if ( !pResult ) {
        return;
    }
    for ( i = 0u; i < pResult->iChunkCount; ++i ) {
        const xllm_memory_chunk_info *pInfo = &pResult->pChunks[i];
        printf("[%u] record=%s chunk=%s index=%u dim=%u\n",
               (unsigned)(i + 1u),
               pInfo->sRecordId ? pInfo->sRecordId : "(null)",
               pInfo->sChunkId ? pInfo->sChunkId : "(null)",
               (unsigned)pInfo->uChunkIndex,
               (unsigned)pInfo->uEmbeddingDim);
        if ( pInfo->sText ) {
            printf("%s\n\n", pInfo->sText);
        }
    }
}

static const char *skip_reason_name(xllm_memory_skip_reason eReason)
{
    switch ( eReason ) {
        case XLLM_MEMORY_SKIP_HIDDEN:
            return "hidden";
        case XLLM_MEMORY_SKIP_IGNORED_DIRECTORY:
            return "ignored_directory";
        case XLLM_MEMORY_SKIP_IGNORED_PATH_PATTERN:
            return "ignored_path_pattern";
        case XLLM_MEMORY_SKIP_IGNORED_EXTENSION:
            return "ignored_extension";
        case XLLM_MEMORY_SKIP_DISALLOWED_EXTENSION:
            return "disallowed_extension";
        case XLLM_MEMORY_SKIP_TOO_LARGE:
            return "too_large";
        case XLLM_MEMORY_SKIP_UNCHANGED:
            return "unchanged";
        default:
            return "unknown";
    }
}

static const char *fail_reason_name(xllm_memory_fail_reason eReason)
{
    switch ( eReason ) {
        case XLLM_MEMORY_FAIL_GET_SIZE:
            return "get_size";
        case XLLM_MEMORY_FAIL_GET_MTIME:
            return "get_mtime";
        case XLLM_MEMORY_FAIL_BUILD_METADATA:
            return "build_metadata";
        case XLLM_MEMORY_FAIL_INGEST:
            return "ingest";
        case XLLM_MEMORY_FAIL_SYNC_FILE:
            return "sync_file";
        default:
            return "unknown";
    }
}

static const char *change_kind_name(xllm_memory_change_kind eKind)
{
    switch ( eKind ) {
        case XLLM_MEMORY_CHANGE_CREATED:
            return "created";
        case XLLM_MEMORY_CHANGE_UPDATED:
            return "updated";
        case XLLM_MEMORY_CHANGE_REMOVED:
            return "removed";
        case XLLM_MEMORY_CHANGE_SKIPPED:
            return "skipped";
        case XLLM_MEMORY_CHANGE_FAILED:
            return "failed";
        default:
            return "unknown";
    }
}

static void demo_sleep_ms(uint32 uMillis)
{
    if ( uMillis == 0u ) {
        return;
    }
    xrtSleep(uMillis);
}

static const char *file_event_kind_name(xllm_memory_file_event_kind eKind)
{
    switch ( eKind ) {
        case XLLM_MEMORY_FILE_EVENT_CREATED:
            return "create";
        case XLLM_MEMORY_FILE_EVENT_UPDATED:
            return "update";
        case XLLM_MEMORY_FILE_EVENT_DELETED:
            return "delete";
        case XLLM_MEMORY_FILE_EVENT_RENAMED:
            return "rename";
        default:
            return "unknown";
    }
}

static void print_skipped_files(
    const xllm_memory_skipped_file_info *pInfos,
    size_t iInfoCount
)
{
    size_t i;

    printf("skipped details: %u\n", (unsigned)iInfoCount);
    for ( i = 0u; i < iInfoCount; ++i ) {
        printf("[%u] reason=%s path=%s relative=%s source=%s bytes=%llu\n",
               (unsigned)(i + 1u),
               skip_reason_name(pInfos[i].eReason),
               pInfos[i].sPath ? pInfos[i].sPath : "(null)",
               pInfos[i].sRelativePath ? pInfos[i].sRelativePath : "(null)",
               pInfos[i].sSourceUri ? pInfos[i].sSourceUri : "(null)",
               (unsigned long long)pInfos[i].uFileBytes);
    }
}

static void print_failed_files(
    const xllm_memory_failed_file_info *pInfos,
    size_t iInfoCount
)
{
    size_t i;

    printf("failed details: %u\n", (unsigned)iInfoCount);
    for ( i = 0u; i < iInfoCount; ++i ) {
        printf("[%u] reason=%s code=%d status=%d path=%s relative=%s source=%s bytes=%llu msg=%s\n",
               (unsigned)(i + 1u),
               fail_reason_name(pInfos[i].eReason),
               (int)pInfos[i].eErrorCode,
               (int)pInfos[i].iStatus,
               pInfos[i].sPath ? pInfos[i].sPath : "(null)",
               pInfos[i].sRelativePath ? pInfos[i].sRelativePath : "(null)",
               pInfos[i].sSourceUri ? pInfos[i].sSourceUri : "(null)",
               (unsigned long long)pInfos[i].uFileBytes,
               pInfos[i].sMessage ? pInfos[i].sMessage : "(null)");
    }
}

static void print_change_set(const xllm_memory_change_set *pChanges)
{
    size_t i;

    printf("change set: %u\n", (unsigned)(pChanges ? pChanges->iChangeCount : 0u));
    if ( !pChanges ) {
        return;
    }
    for ( i = 0u; i < pChanges->iChangeCount; ++i ) {
        const xllm_memory_change_info *pInfo = &pChanges->pChanges[i];
        printf("[%u] kind=%s", (unsigned)(i + 1u), change_kind_name(pInfo->eKind));
        if ( pInfo->eKind == XLLM_MEMORY_CHANGE_CREATED ||
             pInfo->eKind == XLLM_MEMORY_CHANGE_UPDATED ||
             pInfo->eKind == XLLM_MEMORY_CHANGE_REMOVED ) {
            printf(" record=%s source=%s\n",
                   pInfo->tRecord.sRecordId ? pInfo->tRecord.sRecordId : "(null)",
                   pInfo->tRecord.sSourceUri ? pInfo->tRecord.sSourceUri : "(null)");
        } else if ( pInfo->eKind == XLLM_MEMORY_CHANGE_SKIPPED ) {
            printf(" reason=%s relative=%s\n",
                   skip_reason_name(pInfo->tSkipped.eReason),
                   pInfo->tSkipped.sRelativePath ? pInfo->tSkipped.sRelativePath : "(null)");
        } else if ( pInfo->eKind == XLLM_MEMORY_CHANGE_FAILED ) {
            printf(" reason=%s relative=%s msg=%s\n",
                   fail_reason_name(pInfo->tFailed.eReason),
                   pInfo->tFailed.sRelativePath ? pInfo->tFailed.sRelativePath : "(null)",
                   pInfo->tFailed.sMessage ? pInfo->tFailed.sMessage : "(null)");
        } else {
            printf("\n");
        }
    }
}

typedef struct {
    size_t iBatchCount;
} watcher_worker_run_demo_ctx;

static int print_watcher_worker_batch(
    void *pCtx,
    const xllm_memory_change_set *pChanges,
    xllm_error *pError
)
{
    watcher_worker_run_demo_ctx *pPrintCtx = (watcher_worker_run_demo_ctx *)pCtx;

    (void)pError;
    if ( pPrintCtx ) {
        pPrintCtx->iBatchCount += 1u;
        printf("  run_ready batch #%u\n", (unsigned)pPrintCtx->iBatchCount);
    }
    print_change_set(pChanges);
    return XRT_NET_OK;
}

static void print_watcher_worker_state(
    const xllm_memory_watcher_worker_state *pState
)
{
    if ( !pState ) {
        return;
    }

    printf("  worker state pending: %u\n", (unsigned)pState->iPendingCount);
    printf("  worker state ready: %s\n", pState->bReady ? "true" : "false");
    printf("  worker state blocked_by_debounce: %s\n", pState->bBlockedByDebounce ? "true" : "false");
    printf("  worker state debounce ms: %u\n", (unsigned)pState->uDebounceMs);
    printf("  worker state elapsed ms: %u\n", (unsigned)pState->uElapsedSinceActivityMs);
    printf("  worker state wait remaining ms: %u\n", (unsigned)pState->uWaitMsRemaining);
}

typedef struct {
    char **pItems;
    size_t iItemCount;
} string_list;

typedef struct {
    xllm_memory_file_event *pItems;
    size_t iItemCount;
} file_event_list;

static char *dup_trimmed_segment(const char *pStart, size_t iLen)
{
    size_t iBegin = 0u;
    size_t iEnd = iLen;
    char *sValue;

    while ( iBegin < iLen && isspace((unsigned char)pStart[iBegin]) ) {
        ++iBegin;
    }
    while ( iEnd > iBegin && isspace((unsigned char)pStart[iEnd - 1u]) ) {
        --iEnd;
    }
    if ( iEnd <= iBegin ) {
        return NULL;
    }

    sValue = (char *)malloc((iEnd - iBegin) + 1u);
    if ( !sValue ) {
        return NULL;
    }
    memcpy(sValue, pStart + iBegin, iEnd - iBegin);
    sValue[iEnd - iBegin] = '\0';
    return sValue;
}

static void string_list_reset(string_list *pList)
{
    size_t i;

    if ( !pList ) {
        return;
    }
    for ( i = 0u; i < pList->iItemCount; ++i ) {
        free(pList->pItems[i]);
    }
    free(pList->pItems);
    memset(pList, 0, sizeof(*pList));
}

static void file_event_list_reset(file_event_list *pList)
{
    size_t i;

    if ( !pList ) {
        return;
    }
    for ( i = 0u; i < pList->iItemCount; ++i ) {
        free((void *)pList->pItems[i].sPath);
        free((void *)pList->pItems[i].sPreviousPath);
    }
    free(pList->pItems);
    memset(pList, 0, sizeof(*pList));
}

static int split_semicolon_list(const char *sValue, string_list *pList)
{
    const char *pCursor;
    const char *pSegment;

    if ( !pList ) {
        return 1;
    }
    memset(pList, 0, sizeof(*pList));
    if ( !sValue || !sValue[0] ) {
        return 0;
    }

    pCursor = sValue;
    pSegment = sValue;
    for (;;) {
        if ( *pCursor == ';' || *pCursor == '\0' ) {
            char *sItem = dup_trimmed_segment(pSegment, (size_t)(pCursor - pSegment));
            if ( sItem ) {
                char **pNewItems = (char **)realloc(pList->pItems, (pList->iItemCount + 1u) * sizeof(*pNewItems));
                if ( !pNewItems ) {
                    free(sItem);
                    string_list_reset(pList);
                    return 2;
                }
                pNewItems[pList->iItemCount] = sItem;
                pList->pItems = pNewItems;
                ++pList->iItemCount;
            }
            if ( *pCursor == '\0' ) {
                break;
            }
            pSegment = pCursor + 1;
        }
        ++pCursor;
    }

    return 0;
}

static int parse_file_event_kind(
    const char *sText,
    xllm_memory_file_event_kind *peKind
)
{
    if ( !sText || !peKind ) {
        return 1;
    }
    if ( strcmp(sText, "create") == 0 || strcmp(sText, "created") == 0 ) {
        *peKind = XLLM_MEMORY_FILE_EVENT_CREATED;
        return 0;
    }
    if ( strcmp(sText, "update") == 0 || strcmp(sText, "updated") == 0 ) {
        *peKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
        return 0;
    }
    if ( strcmp(sText, "delete") == 0 || strcmp(sText, "deleted") == 0 || strcmp(sText, "remove") == 0 ) {
        *peKind = XLLM_MEMORY_FILE_EVENT_DELETED;
        return 0;
    }
    if ( strcmp(sText, "rename") == 0 || strcmp(sText, "renamed") == 0 || strcmp(sText, "move") == 0 ) {
        *peKind = XLLM_MEMORY_FILE_EVENT_RENAMED;
        return 0;
    }
    return 2;
}

static int parse_sync_events(const char *sValue, file_event_list *pList)
{
    string_list tSpecs;
    size_t i;

    if ( !pList ) {
        return 1;
    }
    memset(pList, 0, sizeof(*pList));
    memset(&tSpecs, 0, sizeof(tSpecs));
    if ( split_semicolon_list(sValue, &tSpecs) != 0 ) {
        return 2;
    }
    if ( tSpecs.iItemCount == 0u ) {
        string_list_reset(&tSpecs);
        return 0;
    }

    pList->pItems = (xllm_memory_file_event *)calloc(tSpecs.iItemCount, sizeof(*pList->pItems));
    if ( !pList->pItems ) {
        string_list_reset(&tSpecs);
        return 3;
    }

    for ( i = 0u; i < tSpecs.iItemCount; ++i ) {
        const char *sSpec = tSpecs.pItems[i];
        const char *pSep1 = strchr(sSpec, '|');
        const char *pSep2 = NULL;
        char *sKind = NULL;
        char *sPath = NULL;
        char *sPrev = NULL;
        xllm_memory_file_event_kind eKind;

        xllm_memory_sync_file_event_init(&pList->pItems[i]);
        if ( !pSep1 ) {
            file_event_list_reset(pList);
            string_list_reset(&tSpecs);
            return 4;
        }
        sKind = dup_trimmed_segment(sSpec, (size_t)(pSep1 - sSpec));
        if ( !sKind || parse_file_event_kind(sKind, &eKind) != 0 ) {
            free(sKind);
            file_event_list_reset(pList);
            string_list_reset(&tSpecs);
            return 5;
        }
        free(sKind);

        pList->pItems[i].eKind = eKind;
        if ( eKind == XLLM_MEMORY_FILE_EVENT_RENAMED ) {
            pSep2 = strchr(pSep1 + 1, '|');
            if ( !pSep2 ) {
                file_event_list_reset(pList);
                string_list_reset(&tSpecs);
                return 6;
            }
            sPrev = dup_trimmed_segment(pSep1 + 1, (size_t)(pSep2 - (pSep1 + 1)));
            sPath = dup_trimmed_segment(pSep2 + 1, strlen(pSep2 + 1));
            if ( !sPrev || !sPath ) {
                free(sPrev);
                free(sPath);
                file_event_list_reset(pList);
                string_list_reset(&tSpecs);
                return 7;
            }
            pList->pItems[i].sPreviousPath = sPrev;
            pList->pItems[i].sPath = sPath;
        } else {
            sPath = dup_trimmed_segment(pSep1 + 1, strlen(pSep1 + 1));
            if ( !sPath ) {
                file_event_list_reset(pList);
                string_list_reset(&tSpecs);
                return 8;
            }
            pList->pItems[i].sPath = sPath;
        }
        ++pList->iItemCount;
    }

    string_list_reset(&tSpecs);
    return 0;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_options tIngest;
    xllm_memory_search_options tSearch;
    xllm_memory_context_options tContext;
    xllm_memory_search_result tResult;
    xllm_memory_record_list_result tRecordList;
    xllm_memory_chunk_list_result tChunkList;
    xllm_memory_ingest_directory_result tDirectoryResult;
    xllm_memory_sync_workspace_result tSyncResult;
    xllm_memory_change_set tChangeSet;
    xllm_memory_list_options tList;
    xllm_memory_builtin_embedder_options tBuiltinEmbedderOptions;
    xllm_memory_builtin_embedder_probe tBuiltinProbe;
    xllm_error tError;
    xllm_request tRequest;
    xllm_runtime *pRuntime = NULL;
    xllm_memory *pMemory = NULL;
    xllm_memory *pReloadedMemory = NULL;
    const char *sSqlitePath;
    const char *sVectorExtensionPath;
    const char *sResetDb;
    const char *sUseDemoEmbedder;
    const char *sBuiltinEmbedder;
    const char *sIngestPath;
    const char *sIngestDir;
    const char *sIngestWorkspace;
    const char *sSyncFile;
    const char *sSyncFiles;
    const char *sSyncEvents;
    const char *sSyncWatcherWorkerState;
    const char *sSyncWatcherWorkerRunLoop;
    const char *sSyncWatcherWorkerRunReady;
    const char *sSyncWatcherWorker;
    const char *sSyncWatcherPump;
    const char *sSyncWatcherBridge;
    const char *sSyncEventQueue;
    const char *sSyncWorkspace;
    const char *sFilterMetadataKey;
    const char *sFilterMetadataValue;
    int iStatus;
    bool bResetOptionsEmbedder = false;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    memset(&tResult, 0, sizeof(tResult));
    memset(&tRecordList, 0, sizeof(tRecordList));
    memset(&tChunkList, 0, sizeof(tChunkList));
    memset(&tDirectoryResult, 0, sizeof(tDirectoryResult));
    memset(&tSyncResult, 0, sizeof(tSyncResult));
    memset(&tChangeSet, 0, sizeof(tChangeSet));
    xllm_error_init(&tError);
    xllm_request_init(&tRequest);
    xllm_memory_embedder_init(&tMemoryOptions.tEmbedder);
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_options_init(&tIngest);
    xllm_memory_search_options_init(&tSearch);
    xllm_memory_list_options_init(&tList);
    xllm_memory_context_options_init(&tContext);
    xllm_memory_builtin_embedder_options_init(&tBuiltinEmbedderOptions);
    xllm_memory_change_set_init(&tChangeSet);
    memset(&tBuiltinProbe, 0, sizeof(tBuiltinProbe));

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        return 1;
    }

    tMemoryOptions.sNamespace = "demo";
    sSqlitePath = getenv("XLLM_MEMORY_SQLITE_PATH");
    sVectorExtensionPath = getenv("XLLM_MEMORY_SQLITE_VECTOR_EXTENSION_PATH");
    sResetDb = getenv("XLLM_MEMORY_RESET_DB");
    sUseDemoEmbedder = getenv("XLLM_MEMORY_USE_DEMO_EMBEDDER");
    sBuiltinEmbedder = getenv("XLLM_MEMORY_BUILTIN_EMBEDDER");
    sIngestPath = getenv("XLLM_MEMORY_INGEST_PATH");
    sIngestDir = getenv("XLLM_MEMORY_INGEST_DIR");
    sIngestWorkspace = getenv("XLLM_MEMORY_INGEST_WORKSPACE");
    sSyncFile = getenv("XLLM_MEMORY_SYNC_FILE");
    sSyncFiles = getenv("XLLM_MEMORY_SYNC_FILES");
    sSyncEvents = getenv("XLLM_MEMORY_SYNC_EVENTS");
    sSyncWatcherWorkerState = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_STATE");
    sSyncWatcherWorkerRunLoop = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_LOOP");
    sSyncWatcherWorkerRunReady = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_READY");
    sSyncWatcherWorker = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER");
    sSyncWatcherPump = getenv("XLLM_MEMORY_SYNC_WATCHER_PUMP");
    sSyncWatcherBridge = getenv("XLLM_MEMORY_SYNC_WATCHER_BRIDGE");
    sSyncEventQueue = getenv("XLLM_MEMORY_SYNC_EVENT_QUEUE");
    sSyncWorkspace = getenv("XLLM_MEMORY_SYNC_WORKSPACE");
    sFilterMetadataKey = getenv("XLLM_MEMORY_FILTER_METADATA_KEY");
    sFilterMetadataValue = getenv("XLLM_MEMORY_FILTER_METADATA_VALUE");
    if ( sResetDb && strcmp(sResetDb, "1") == 0 && sSqlitePath && sSqlitePath[0] ) {
        remove(sSqlitePath);
    }
    tMemoryOptions.sSqlitePath = sSqlitePath;
    tMemoryOptions.sSqliteVectorExtensionPath = sVectorExtensionPath;
    tMemoryOptions.bLoadSqliteVectorExtension = (sVectorExtensionPath && sVectorExtensionPath[0]) ? true : false;
    if ( (sUseDemoEmbedder && strcmp(sUseDemoEmbedder, "0") != 0) || (sBuiltinEmbedder && sBuiltinEmbedder[0]) ) {
        const char *sHashDims = getenv("XLLM_MEMORY_BUILTIN_HASH_DIMS");

        if ( !sBuiltinEmbedder || !sBuiltinEmbedder[0] || strcmp(sBuiltinEmbedder, "hash") == 0 || strcmp(sBuiltinEmbedder, "demo") == 0 ) {
            tBuiltinEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_HASH;
            if ( sHashDims && sHashDims[0] ) {
                tBuiltinEmbedderOptions.uHashDimensions = (uint32)strtoul(sHashDims, NULL, 10);
            }
        } else if ( strcmp(sBuiltinEmbedder, "e5") == 0 || strcmp(sBuiltinEmbedder, "multilingual-e5-small") == 0 ) {
            tBuiltinEmbedderOptions.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX;
            tBuiltinEmbedderOptions.sRuntimeDllPath = getenv("XLLM_MEMORY_E5_RUNTIME_DLL");
            tBuiltinEmbedderOptions.sModelPath = getenv("XLLM_MEMORY_E5_MODEL_PATH");
            tBuiltinEmbedderOptions.sTokenizerPath = getenv("XLLM_MEMORY_E5_TOKENIZER_PATH");
        } else {
            fprintf(stderr, "unknown builtin embedder: %s\n", sBuiltinEmbedder);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 1;
        }

        iStatus = xllm_memory_probe_builtin_embedder(&tBuiltinEmbedderOptions, &tBuiltinProbe, &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "builtin embedder probe failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 1;
        }
        printf("builtin embedder: %s\n", tBuiltinProbe.sMessage ? tBuiltinProbe.sMessage : "(none)");
        if ( tBuiltinProbe.sResolvedRuntimeDllPath ) {
            printf("  runtime : %s\n", tBuiltinProbe.sResolvedRuntimeDllPath);
        }
        if ( tBuiltinProbe.sResolvedModelPath ) {
            printf("  model   : %s\n", tBuiltinProbe.sResolvedModelPath);
        }
        if ( tBuiltinProbe.sResolvedTokenizerPath ) {
            printf("  tokenizer: %s\n", tBuiltinProbe.sResolvedTokenizerPath);
        }
        if ( tBuiltinEmbedderOptions.eKind == XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX &&
             (!tBuiltinProbe.bModelFound || !tBuiltinProbe.bTokenizerFound) ) {
            printf("  hint    : run dev\\minionnx\\ensure_multilingual_e5_small_assets.ps1\n");
        }

        iStatus = xllm_memory_make_builtin_embedder(&tBuiltinEmbedderOptions, &tMemoryOptions.tEmbedder, &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "builtin embedder create failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
            xllm_memory_builtin_embedder_probe_reset(&tBuiltinProbe);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 1;
        }
        xllm_memory_builtin_embedder_probe_reset(&tBuiltinProbe);
        bResetOptionsEmbedder = true;
        tMemoryOptions.bEnableHybridSearch = true;
        tMemoryOptions.tVectorWeight.bSet = true;
        tMemoryOptions.tVectorWeight.fValue = 1.0;
    }
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        if ( bResetOptionsEmbedder ) {
            xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
        }
        xllm_runtime_destroy(pRuntime);
        return 1;
    }
    if ( bResetOptionsEmbedder ) {
        xllm_memory_embedder_reset(&tMemoryOptions.tEmbedder);
        bResetOptionsEmbedder = false;
    }

    if ( sSqlitePath && sSqlitePath[0] ) {
        printf("sqlite path: %s\n", sSqlitePath);
        if ( sVectorExtensionPath && sVectorExtensionPath[0] ) {
            printf("sqlite vector extension: %s\n", sVectorExtensionPath);
        }
    }

    if ( sSyncWatcherWorkerState && sSyncWatcherWorkerState[0] ) {
        xllm_memory_watcher_worker_options tWorkerOptions;
        xllm_memory_watcher_worker_state tWorkerState;
        xllm_memory_watcher_worker *pWorker = NULL;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        const char *sAutoFlushThreshold = getenv("XLLM_MEMORY_SYNC_WATCHER_PUMP_THRESHOLD");
        const char *sDebounceMs = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_DEBOUNCE_MS");
        const char *sWorkerMaxItems = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_MAX_ITEMS");
        const char *sStateSleepMs = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_STATE_SLEEP_MS");
        uint32 uStateSleepMs = 0u;
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        xllm_memory_watcher_worker_options_init(&tWorkerOptions);
        xllm_memory_watcher_worker_state_init(&tWorkerState);
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = (uint64)_strtoui64(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = false;
        } else {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = true;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "1") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.bContinueOnError = false;
        }
        if ( sAutoFlushThreshold && sAutoFlushThreshold[0] ) {
            tWorkerOptions.tPumpOptions.iAutoFlushThreshold = (size_t)strtoull(sAutoFlushThreshold, NULL, 10);
        }
        if ( sDebounceMs && sDebounceMs[0] ) {
            tWorkerOptions.uDebounceMs = (uint32)strtoul(sDebounceMs, NULL, 10);
        }
        if ( sWorkerMaxItems && sWorkerMaxItems[0] ) {
            tWorkerOptions.iDefaultMaxItems = (size_t)strtoull(sWorkerMaxItems, NULL, 10);
        }
        if ( sStateSleepMs && sStateSleepMs[0] ) {
            uStateSleepMs = (uint32)strtoul(sStateSleepMs, NULL, 10);
        }

        iStatus = XRT_NET_ERROR;
        if ( parse_sync_events(sSyncWatcherWorkerState, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_WATCHER_WORKER_STATE\n");
        } else if ( xllm_memory_watcher_worker_create(pMemory, &tWorkerOptions, &pWorker, &tError) != XRT_NET_OK ) {
            fprintf(stderr, "failed to create memory watcher worker: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        } else {
            printf("syncing watcher worker state events (%u items)\n", (unsigned)tEvents.iItemCount);
            printf("  auto flush threshold: %u\n", (unsigned)tWorkerOptions.tPumpOptions.iAutoFlushThreshold);
            printf("  debounce ms: %u\n", (unsigned)tWorkerOptions.uDebounceMs);
            printf("  worker default max items: %u\n", (unsigned)tWorkerOptions.iDefaultMaxItems);
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                bool bFlushed = false;

                iStatus = xllm_memory_watcher_worker_push(pWorker, &tEvents.pItems[i], &bFlushed, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    break;
                }
                if ( bFlushed ) {
                    printf("  push flush result:\n");
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
                if ( xllm_memory_watcher_worker_get_state(pWorker, &tWorkerState, &tError) != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker get_state failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    iStatus = XRT_NET_ERROR;
                    break;
                }
                print_watcher_worker_state(&tWorkerState);
                if ( uStateSleepMs > 0u ) {
                    demo_sleep_ms(uStateSleepMs);
                    if ( xllm_memory_watcher_worker_get_state(pWorker, &tWorkerState, &tError) != XRT_NET_OK ) {
                        fprintf(stderr, "watcher worker get_state after sleep failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                        xllm_error_reset(&tError);
                        iStatus = XRT_NET_ERROR;
                        break;
                    }
                    printf("  state after sleep:\n");
                    print_watcher_worker_state(&tWorkerState);
                }
            }
        }

        if ( pWorker ) {
            xllm_memory_watcher_worker_destroy(pWorker);
        }
        file_event_list_reset(&tEvents);
    } else if ( sSyncWatcherWorkerRunLoop && sSyncWatcherWorkerRunLoop[0] ) {
        xllm_memory_watcher_worker_options tWorkerOptions;
        xllm_memory_watcher_worker_loop_options tLoopOptions;
        xllm_memory_watcher_worker_loop_result tLoopResult;
        xllm_memory_watcher_worker *pWorker = NULL;
        watcher_worker_run_demo_ctx tPrintCtx;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        const char *sAutoFlushThreshold = getenv("XLLM_MEMORY_SYNC_WATCHER_PUMP_THRESHOLD");
        const char *sDebounceMs = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_DEBOUNCE_MS");
        const char *sWorkerMaxItems = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_MAX_ITEMS");
        const char *sLoopMaxBatches = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_LOOP_MAX_BATCHES");
        const char *sLoopMaxItems = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_LOOP_MAX_ITEMS");
        const char *sLoopSleepMs = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_LOOP_SLEEP_MS");
        const char *sLoopMaxWaitMs = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_LOOP_MAX_WAIT_MS");
        const char *sLoopForceFlush = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_LOOP_FORCE_FLUSH");
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        memset(&tPrintCtx, 0, sizeof(tPrintCtx));
        xllm_memory_watcher_worker_options_init(&tWorkerOptions);
        xllm_memory_watcher_worker_loop_options_init(&tLoopOptions);
        xllm_memory_watcher_worker_loop_result_init(&tLoopResult);
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = (uint64)_strtoui64(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = false;
        } else {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = true;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "1") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.bContinueOnError = false;
        }
        if ( sAutoFlushThreshold && sAutoFlushThreshold[0] ) {
            tWorkerOptions.tPumpOptions.iAutoFlushThreshold = (size_t)strtoull(sAutoFlushThreshold, NULL, 10);
        }
        if ( sDebounceMs && sDebounceMs[0] ) {
            tWorkerOptions.uDebounceMs = (uint32)strtoul(sDebounceMs, NULL, 10);
        }
        if ( sWorkerMaxItems && sWorkerMaxItems[0] ) {
            tWorkerOptions.iDefaultMaxItems = (size_t)strtoull(sWorkerMaxItems, NULL, 10);
        }
        if ( sLoopMaxBatches && sLoopMaxBatches[0] ) {
            tLoopOptions.tRunOptions.iMaxBatches = (size_t)strtoull(sLoopMaxBatches, NULL, 10);
        }
        if ( sLoopMaxItems && sLoopMaxItems[0] ) {
            tLoopOptions.tRunOptions.iMaxItemsPerBatch = (size_t)strtoull(sLoopMaxItems, NULL, 10);
        }
        if ( sLoopSleepMs && sLoopSleepMs[0] ) {
            tLoopOptions.uSleepMs = (uint32)strtoul(sLoopSleepMs, NULL, 10);
        }
        if ( sLoopMaxWaitMs && sLoopMaxWaitMs[0] ) {
            tLoopOptions.uMaxWaitMs = (uint32)strtoul(sLoopMaxWaitMs, NULL, 10);
        }
        if ( sLoopForceFlush && strcmp(sLoopForceFlush, "0") != 0 ) {
            tLoopOptions.bForceFlushOnTimeout = true;
        }

        iStatus = XRT_NET_ERROR;
        if ( parse_sync_events(sSyncWatcherWorkerRunLoop, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_LOOP\n");
        } else if ( xllm_memory_watcher_worker_create(pMemory, &tWorkerOptions, &pWorker, &tError) != XRT_NET_OK ) {
            fprintf(stderr, "failed to create memory watcher worker: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        } else {
            printf("syncing watcher worker run_loop events (%u items)\n", (unsigned)tEvents.iItemCount);
            printf("  auto flush threshold: %u\n", (unsigned)tWorkerOptions.tPumpOptions.iAutoFlushThreshold);
            printf("  debounce ms: %u\n", (unsigned)tWorkerOptions.uDebounceMs);
            printf("  worker default max items: %u\n", (unsigned)tWorkerOptions.iDefaultMaxItems);
            printf("  run_loop max batches: %u\n", (unsigned)tLoopOptions.tRunOptions.iMaxBatches);
            printf("  run_loop max items: %u\n", (unsigned)tLoopOptions.tRunOptions.iMaxItemsPerBatch);
            printf("  run_loop sleep ms: %u\n", (unsigned)tLoopOptions.uSleepMs);
            printf("  run_loop max wait ms: %u\n", (unsigned)tLoopOptions.uMaxWaitMs);
            printf("  run_loop force flush: %s\n", tLoopOptions.bForceFlushOnTimeout ? "true" : "false");
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                bool bFlushed = false;

                iStatus = xllm_memory_watcher_worker_push(pWorker, &tEvents.pItems[i], &bFlushed, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    break;
                }
                printf("  pending after push: %u\n", (unsigned)xllm_memory_watcher_worker_pending_count(pWorker));
                if ( bFlushed ) {
                    printf("  push flush result:\n");
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
            if ( iStatus == XRT_NET_OK ) {
                iStatus = xllm_memory_watcher_worker_run_loop(
                    pWorker,
                    &tLoopOptions,
                    print_watcher_worker_batch,
                    &tPrintCtx,
                    &tLoopResult,
                    &tError
                );
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker run_loop failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                } else {
                    printf("  run_loop batches: %u\n", (unsigned)tLoopResult.iBatchCount);
                    printf("  run_loop changes: %u\n", (unsigned)tLoopResult.iChangeCount);
                    printf("  run_loop pending: %u\n", (unsigned)tLoopResult.iPendingCount);
                    printf("  run_loop loops: %u\n", (unsigned)tLoopResult.uLoopCount);
                    printf("  run_loop waited ms: %u\n", (unsigned)tLoopResult.uWaitedMs);
                    printf("  run_loop blocked_by_debounce: %s\n", tLoopResult.bBlockedByDebounce ? "true" : "false");
                    printf("  run_loop stopped_by_batch_limit: %s\n", tLoopResult.bStoppedByBatchLimit ? "true" : "false");
                    printf("  run_loop timed_out: %s\n", tLoopResult.bTimedOut ? "true" : "false");
                }
            }
        }

        if ( pWorker ) {
            xllm_memory_watcher_worker_destroy(pWorker);
        }
        file_event_list_reset(&tEvents);
    } else if ( sSyncWatcherWorkerRunReady && sSyncWatcherWorkerRunReady[0] ) {
        xllm_memory_watcher_worker_options tWorkerOptions;
        xllm_memory_watcher_worker_run_options tRunOptions;
        xllm_memory_watcher_worker_run_result tRunResult;
        xllm_memory_watcher_worker *pWorker = NULL;
        watcher_worker_run_demo_ctx tPrintCtx;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        const char *sAutoFlushThreshold = getenv("XLLM_MEMORY_SYNC_WATCHER_PUMP_THRESHOLD");
        const char *sDebounceMs = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_DEBOUNCE_MS");
        const char *sWorkerMaxItems = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_MAX_ITEMS");
        const char *sRunMaxBatches = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_READY_MAX_BATCHES");
        const char *sRunMaxItems = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_READY_MAX_ITEMS");
        uint32 uPollSleepMs = 0u;
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        memset(&tPrintCtx, 0, sizeof(tPrintCtx));
        xllm_memory_watcher_worker_options_init(&tWorkerOptions);
        xllm_memory_watcher_worker_run_options_init(&tRunOptions);
        xllm_memory_watcher_worker_run_result_init(&tRunResult);
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = (uint64)_strtoui64(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = false;
        } else {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = true;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "1") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.bContinueOnError = false;
        }
        if ( sAutoFlushThreshold && sAutoFlushThreshold[0] ) {
            tWorkerOptions.tPumpOptions.iAutoFlushThreshold = (size_t)strtoull(sAutoFlushThreshold, NULL, 10);
        }
        if ( sDebounceMs && sDebounceMs[0] ) {
            tWorkerOptions.uDebounceMs = (uint32)strtoul(sDebounceMs, NULL, 10);
        }
        if ( sWorkerMaxItems && sWorkerMaxItems[0] ) {
            tWorkerOptions.iDefaultMaxItems = (size_t)strtoull(sWorkerMaxItems, NULL, 10);
        }
        if ( sRunMaxBatches && sRunMaxBatches[0] ) {
            tRunOptions.iMaxBatches = (size_t)strtoull(sRunMaxBatches, NULL, 10);
        }
        if ( sRunMaxItems && sRunMaxItems[0] ) {
            tRunOptions.iMaxItemsPerBatch = (size_t)strtoull(sRunMaxItems, NULL, 10);
        }
        uPollSleepMs = tWorkerOptions.uDebounceMs;
        if ( uPollSleepMs > 0u ) {
            uPollSleepMs += 10u;
        }

        iStatus = XRT_NET_ERROR;
        if ( parse_sync_events(sSyncWatcherWorkerRunReady, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_WATCHER_WORKER_RUN_READY\n");
        } else if ( xllm_memory_watcher_worker_create(pMemory, &tWorkerOptions, &pWorker, &tError) != XRT_NET_OK ) {
            fprintf(stderr, "failed to create memory watcher worker: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        } else {
            printf("syncing watcher worker run_ready events (%u items)\n", (unsigned)tEvents.iItemCount);
            printf("  auto flush threshold: %u\n", (unsigned)tWorkerOptions.tPumpOptions.iAutoFlushThreshold);
            printf("  debounce ms: %u\n", (unsigned)tWorkerOptions.uDebounceMs);
            printf("  worker default max items: %u\n", (unsigned)tWorkerOptions.iDefaultMaxItems);
            printf("  run_ready max batches: %u\n", (unsigned)tRunOptions.iMaxBatches);
            printf("  run_ready max items: %u\n", (unsigned)tRunOptions.iMaxItemsPerBatch);
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                bool bFlushed = false;

                iStatus = xllm_memory_watcher_worker_push(pWorker, &tEvents.pItems[i], &bFlushed, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    break;
                }
                printf("  pending after push: %u\n", (unsigned)xllm_memory_watcher_worker_pending_count(pWorker));
                if ( bFlushed ) {
                    printf("  push flush result:\n");
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
            if ( iStatus == XRT_NET_OK && uPollSleepMs > 0u ) {
                demo_sleep_ms(uPollSleepMs);
            }
            if ( iStatus == XRT_NET_OK ) {
                iStatus = xllm_memory_watcher_worker_run_ready(
                    pWorker,
                    &tRunOptions,
                    print_watcher_worker_batch,
                    &tPrintCtx,
                    &tRunResult,
                    &tError
                );
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker run_ready failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                } else {
                    printf("  run_ready batches: %u\n", (unsigned)tRunResult.iBatchCount);
                    printf("  run_ready changes: %u\n", (unsigned)tRunResult.iChangeCount);
                    printf("  run_ready pending: %u\n", (unsigned)tRunResult.iPendingCount);
                    printf("  run_ready blocked_by_debounce: %s\n", tRunResult.bBlockedByDebounce ? "true" : "false");
                }
            }
        }

        if ( pWorker ) {
            xllm_memory_watcher_worker_destroy(pWorker);
        }
        file_event_list_reset(&tEvents);
    } else if ( sSyncWatcherWorker && sSyncWatcherWorker[0] ) {
        xllm_memory_watcher_worker_options tWorkerOptions;
        xllm_memory_watcher_worker *pWorker = NULL;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        const char *sAutoFlushThreshold = getenv("XLLM_MEMORY_SYNC_WATCHER_PUMP_THRESHOLD");
        const char *sDebounceMs = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_DEBOUNCE_MS");
        const char *sWorkerMaxItems = getenv("XLLM_MEMORY_SYNC_WATCHER_WORKER_MAX_ITEMS");
        uint32 uPollSleepMs = 0u;
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        xllm_memory_watcher_worker_options_init(&tWorkerOptions);
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = (uint64)_strtoui64(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = false;
        } else {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = true;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "1") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tWorkerOptions.tPumpOptions.tBridgeOptions.bContinueOnError = false;
        }
        if ( sAutoFlushThreshold && sAutoFlushThreshold[0] ) {
            tWorkerOptions.tPumpOptions.iAutoFlushThreshold = (size_t)strtoull(sAutoFlushThreshold, NULL, 10);
        }
        if ( sDebounceMs && sDebounceMs[0] ) {
            tWorkerOptions.uDebounceMs = (uint32)strtoul(sDebounceMs, NULL, 10);
        }
        if ( sWorkerMaxItems && sWorkerMaxItems[0] ) {
            tWorkerOptions.iDefaultMaxItems = (size_t)strtoull(sWorkerMaxItems, NULL, 10);
        }
        uPollSleepMs = tWorkerOptions.uDebounceMs;
        if ( uPollSleepMs > 0u ) {
            uPollSleepMs += 10u;
        }

        iStatus = XRT_NET_ERROR;
        if ( parse_sync_events(sSyncWatcherWorker, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_WATCHER_WORKER\n");
        } else if ( xllm_memory_watcher_worker_create(pMemory, &tWorkerOptions, &pWorker, &tError) != XRT_NET_OK ) {
            fprintf(stderr, "failed to create memory watcher worker: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        } else {
            printf("syncing watcher worker events (%u items)\n", (unsigned)tEvents.iItemCount);
            printf("  auto flush threshold: %u\n", (unsigned)tWorkerOptions.tPumpOptions.iAutoFlushThreshold);
            printf("  debounce ms: %u\n", (unsigned)tWorkerOptions.uDebounceMs);
            printf("  poll max items: %u\n", (unsigned)tWorkerOptions.iDefaultMaxItems);
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                bool bFlushed = false;

                iStatus = xllm_memory_watcher_worker_push(pWorker, &tEvents.pItems[i], &bFlushed, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    break;
                }
                printf("  pending after push: %u\n", (unsigned)xllm_memory_watcher_worker_pending_count(pWorker));
                if ( bFlushed ) {
                    printf("  push flush result:\n");
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
            while ( iStatus == XRT_NET_OK && xllm_memory_watcher_worker_pending_count(pWorker) > 0u ) {
                bool bFlushed = false;

                demo_sleep_ms(uPollSleepMs);
                iStatus = xllm_memory_watcher_worker_poll(pWorker, &bFlushed, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher worker poll failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    break;
                }
                printf("  pending after poll: %u\n", (unsigned)xllm_memory_watcher_worker_pending_count(pWorker));
                if ( bFlushed ) {
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                } else if ( uPollSleepMs == 0u ) {
                    iStatus = xllm_memory_watcher_worker_flush(pWorker, &tChangeSet, &tError);
                    if ( iStatus != XRT_NET_OK ) {
                        fprintf(stderr, "watcher worker flush failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                        xllm_error_reset(&tError);
                        break;
                    }
                    printf("  pending after flush: %u\n", (unsigned)xllm_memory_watcher_worker_pending_count(pWorker));
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
        }

        if ( pWorker ) {
            xllm_memory_watcher_worker_destroy(pWorker);
        }
        file_event_list_reset(&tEvents);
    } else if ( sSyncWatcherPump && sSyncWatcherPump[0] ) {
        xllm_memory_watcher_pump_options tPumpOptions;
        xllm_memory_watcher_pump *pPump = NULL;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        const char *sAutoFlushThreshold = getenv("XLLM_MEMORY_SYNC_WATCHER_PUMP_THRESHOLD");
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        xllm_memory_watcher_pump_options_init(&tPumpOptions);
        tPumpOptions.tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tPumpOptions.tBridgeOptions.tBaseOptions.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tPumpOptions.tBridgeOptions.tBaseOptions.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tPumpOptions.tBridgeOptions.tBaseOptions.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tPumpOptions.tBridgeOptions.tBaseOptions.uMaxFileBytes = (uint64)_strtoui64(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = false;
        } else {
            tPumpOptions.tBridgeOptions.tBaseOptions.bSkipUnchanged = true;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "1") == 0 ) {
            tPumpOptions.tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tPumpOptions.tBridgeOptions.bContinueOnError = false;
        }
        if ( sAutoFlushThreshold && sAutoFlushThreshold[0] ) {
            tPumpOptions.iAutoFlushThreshold = (size_t)strtoull(sAutoFlushThreshold, NULL, 10);
        }

        iStatus = XRT_NET_ERROR;
        if ( parse_sync_events(sSyncWatcherPump, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_WATCHER_PUMP\n");
        } else if ( xllm_memory_watcher_pump_create(pMemory, &tPumpOptions, &pPump, &tError) != XRT_NET_OK ) {
            fprintf(stderr, "failed to create memory watcher pump: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        } else {
            printf("syncing watcher pump events (%u items)\n", (unsigned)tEvents.iItemCount);
            printf("  auto flush threshold: %u\n", (unsigned)tPumpOptions.iAutoFlushThreshold);
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                bool bFlushed = false;
                iStatus = xllm_memory_watcher_pump_push(pPump, &tEvents.pItems[i], &bFlushed, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher pump push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    break;
                }
                printf("  pending after push: %u\n", (unsigned)xllm_memory_watcher_pump_pending_count(pPump));
                if ( bFlushed ) {
                    printf("  auto flush result:\n");
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
            if ( iStatus == XRT_NET_OK && xllm_memory_watcher_pump_pending_count(pPump) > 0u ) {
                iStatus = xllm_memory_watcher_pump_flush(pPump, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher pump flush failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                } else {
                    printf("watcher pump pending after flush: %u\n", (unsigned)xllm_memory_watcher_pump_pending_count(pPump));
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
        }

        if ( pPump ) {
            xllm_memory_watcher_pump_destroy(pPump);
        }
        file_event_list_reset(&tEvents);
    } else if ( sSyncWatcherBridge && sSyncWatcherBridge[0] ) {
        xllm_memory_watcher_bridge_options tBridgeOptions;
        xllm_memory_watcher_bridge *pBridge = NULL;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        const char *sDrainMaxItems = getenv("XLLM_MEMORY_SYNC_EVENT_QUEUE_DRAIN_MAX_ITEMS");
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        xllm_memory_watcher_bridge_options_init(&tBridgeOptions);
        tBridgeOptions.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tBridgeOptions.tBaseOptions.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tBridgeOptions.tBaseOptions.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tBridgeOptions.tBaseOptions.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tBridgeOptions.tBaseOptions.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tBridgeOptions.tBaseOptions.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tBridgeOptions.tBaseOptions.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tBridgeOptions.tBaseOptions.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tBridgeOptions.tBaseOptions.uMaxFileBytes = (uint64)_strtoui64(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tBridgeOptions.tBaseOptions.bSkipUnchanged = false;
        } else {
            tBridgeOptions.tBaseOptions.bSkipUnchanged = true;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "1") == 0 ) {
            tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tBridgeOptions.bContinueOnError = false;
        }
        if ( sDrainMaxItems && sDrainMaxItems[0] ) {
            tBridgeOptions.iDefaultMaxItems = (size_t)strtoull(sDrainMaxItems, NULL, 10);
        }

        iStatus = XRT_NET_ERROR;
        if ( parse_sync_events(sSyncWatcherBridge, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_WATCHER_BRIDGE\n");
        } else if ( xllm_memory_watcher_bridge_create(pMemory, &tBridgeOptions, &pBridge, &tError) != XRT_NET_OK ) {
            fprintf(stderr, "failed to create memory watcher bridge: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        } else {
            printf("syncing watcher bridge events (%u items)\n", (unsigned)tEvents.iItemCount);
            printf("  root path: %s\n", tBridgeOptions.tBaseOptions.sRootPath ? tBridgeOptions.tBaseOptions.sRootPath : "(none)");
            printf("  record id prefix: %s\n", tBridgeOptions.tBaseOptions.sRecordIdPrefix ? tBridgeOptions.tBaseOptions.sRecordIdPrefix : "(auto)");
            printf("  source uri prefix: %s\n", tBridgeOptions.tBaseOptions.sSourceUriPrefix ? tBridgeOptions.tBaseOptions.sSourceUriPrefix : "(default)");
            printf("  workspace defaults: %s\n", tBridgeOptions.tBaseOptions.bUseWorkspaceDefaults ? "true" : "false");
            printf("  skip unchanged: %s\n", tBridgeOptions.tBaseOptions.bSkipUnchanged ? "true" : "false");
            printf("  continue on error: %s\n", tBridgeOptions.bContinueOnError ? "true" : "false");
            printf("  flush max items: %u\n", (unsigned)tBridgeOptions.iDefaultMaxItems);
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                if ( xllm_memory_watcher_bridge_push(pBridge, &tEvents.pItems[i], &tError) != XRT_NET_OK ) {
                    fprintf(stderr, "watcher bridge push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    iStatus = XRT_NET_ERROR;
                    break;
                }
                iStatus = XRT_NET_OK;
            }

            printf("watcher bridge pending: %u\n", (unsigned)xllm_memory_watcher_bridge_pending_count(pBridge));
            if ( iStatus == XRT_NET_OK ) {
                iStatus = xllm_memory_watcher_bridge_flush(pBridge, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "watcher bridge flush failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                } else {
                    printf("watcher bridge pending after flush: %u\n", (unsigned)xllm_memory_watcher_bridge_pending_count(pBridge));
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
        }

        if ( pBridge ) {
            xllm_memory_watcher_bridge_destroy(pBridge);
        }
        file_event_list_reset(&tEvents);
    } else if ( sSyncEventQueue && sSyncEventQueue[0] ) {
        xllm_memory_sync_file_options tFileSyncTemplate;
        xllm_memory_file_event_queue_drain_options tDrainOptions;
        xllm_memory_file_event_queue *pEventQueue = NULL;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        const char *sDrainMaxItems = getenv("XLLM_MEMORY_SYNC_EVENT_QUEUE_DRAIN_MAX_ITEMS");
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        xllm_memory_sync_file_options_init(&tFileSyncTemplate);
        xllm_memory_file_event_queue_drain_options_init(&tDrainOptions);
        tFileSyncTemplate.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tFileSyncTemplate.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tFileSyncTemplate.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tFileSyncTemplate.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tFileSyncTemplate.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tFileSyncTemplate.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tFileSyncTemplate.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tFileSyncTemplate.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tFileSyncTemplate.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tFileSyncTemplate.bSkipUnchanged = false;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "0") != 0 ) {
            tFileSyncTemplate.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tDrainOptions.bContinueOnError = false;
        }
        if ( sDrainMaxItems && sDrainMaxItems[0] ) {
            tDrainOptions.iMaxItems = (size_t)strtoull(sDrainMaxItems, NULL, 10);
        }

        iStatus = XRT_NET_ERROR;
        if ( parse_sync_events(sSyncEventQueue, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_EVENT_QUEUE\n");
        } else if ( xllm_memory_file_event_queue_create(&pEventQueue) != XRT_NET_OK ) {
            fprintf(stderr, "failed to create memory file event queue\n");
        } else {
            tDrainOptions.tBaseOptions = tFileSyncTemplate;
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                if ( xllm_memory_file_event_queue_push(pEventQueue, &tEvents.pItems[i], &tError) != XRT_NET_OK ) {
                    fprintf(stderr, "event queue push failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                    iStatus = XRT_NET_ERROR;
                    break;
                }
                iStatus = XRT_NET_OK;
            }

            printf("queued file events (%u items)\n", (unsigned)xllm_memory_file_event_queue_count(pEventQueue));
            printf("  root path: %s\n", tFileSyncTemplate.sRootPath ? tFileSyncTemplate.sRootPath : "(none)");
            printf("  record id prefix: %s\n", tFileSyncTemplate.sRecordIdPrefix ? tFileSyncTemplate.sRecordIdPrefix : "(auto)");
            printf("  source uri prefix: %s\n", tFileSyncTemplate.sSourceUriPrefix ? tFileSyncTemplate.sSourceUriPrefix : "(default)");
            printf("  workspace defaults: %s\n", tFileSyncTemplate.bUseWorkspaceDefaults ? "true" : "false");
            printf("  skip unchanged: %s\n", tFileSyncTemplate.bSkipUnchanged ? "true" : "false");
            printf("  continue on error: %s\n", tDrainOptions.bContinueOnError ? "true" : "false");
            printf("  drain max items: %u\n", (unsigned)tDrainOptions.iMaxItems);
            printf("  max file bytes: %llu\n", (unsigned long long)tFileSyncTemplate.uMaxFileBytes);
            printf("  extensions: %s\n", tFileSyncTemplate.sAllowedExtensions ? tFileSyncTemplate.sAllowedExtensions : "(default/all)");
            printf("  ignored dirs: %s\n", tFileSyncTemplate.sIgnoredDirectories ? tFileSyncTemplate.sIgnoredDirectories : "(default/none)");
            printf("  ignored exts: %s\n", tFileSyncTemplate.sIgnoredExtensions ? tFileSyncTemplate.sIgnoredExtensions : "(default/none)");
            printf("  ignored patterns: %s\n", tFileSyncTemplate.sIgnoredPathPatterns ? tFileSyncTemplate.sIgnoredPathPatterns : "(none)");

            if ( iStatus == XRT_NET_OK && xllm_memory_file_event_queue_count(pEventQueue) > 0u ) {
                iStatus = xllm_memory_file_event_queue_drain(pMemory, pEventQueue, &tDrainOptions, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "event queue drain failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                } else {
                    printf("queue after drain: %u\n", (unsigned)xllm_memory_file_event_queue_count(pEventQueue));
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
        }

        if ( pEventQueue ) {
            xllm_memory_file_event_queue_destroy(pEventQueue);
        }
        file_event_list_reset(&tEvents);
    } else if ( sSyncEvents && sSyncEvents[0] ) {
        xllm_memory_sync_file_options tFileSyncTemplate;
        xllm_memory_sync_file_events_options tEventOptions;
        file_event_list tEvents;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_EVENTS_CONTINUE_ON_ERROR");
        size_t i;

        memset(&tEvents, 0, sizeof(tEvents));
        xllm_memory_sync_file_options_init(&tFileSyncTemplate);
        xllm_memory_sync_file_events_options_init(&tEventOptions);
        tFileSyncTemplate.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tFileSyncTemplate.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tFileSyncTemplate.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tFileSyncTemplate.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tFileSyncTemplate.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tFileSyncTemplate.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tFileSyncTemplate.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tFileSyncTemplate.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tFileSyncTemplate.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tFileSyncTemplate.bSkipUnchanged = false;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "0") != 0 ) {
            tFileSyncTemplate.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tEventOptions.bContinueOnError = false;
        }

        if ( parse_sync_events(sSyncEvents, &tEvents) != 0 || tEvents.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_EVENTS\n");
        } else {
            tEventOptions.tBaseOptions = tFileSyncTemplate;
            tEventOptions.pItems = tEvents.pItems;
            tEventOptions.iItemCount = tEvents.iItemCount;

            printf("syncing file events (%u items)\n", (unsigned)tEvents.iItemCount);
            printf("  root path: %s\n", tFileSyncTemplate.sRootPath ? tFileSyncTemplate.sRootPath : "(none)");
            printf("  record id prefix: %s\n", tFileSyncTemplate.sRecordIdPrefix ? tFileSyncTemplate.sRecordIdPrefix : "(auto)");
            printf("  source uri prefix: %s\n", tFileSyncTemplate.sSourceUriPrefix ? tFileSyncTemplate.sSourceUriPrefix : "(default)");
            printf("  workspace defaults: %s\n", tFileSyncTemplate.bUseWorkspaceDefaults ? "true" : "false");
            printf("  skip unchanged: %s\n", tFileSyncTemplate.bSkipUnchanged ? "true" : "false");
            printf("  continue on error: %s\n", tEventOptions.bContinueOnError ? "true" : "false");
            printf("  max file bytes: %llu\n", (unsigned long long)tFileSyncTemplate.uMaxFileBytes);
            printf("  extensions: %s\n", tFileSyncTemplate.sAllowedExtensions ? tFileSyncTemplate.sAllowedExtensions : "(default/all)");
            printf("  ignored dirs: %s\n", tFileSyncTemplate.sIgnoredDirectories ? tFileSyncTemplate.sIgnoredDirectories : "(default/none)");
            printf("  ignored exts: %s\n", tFileSyncTemplate.sIgnoredExtensions ? tFileSyncTemplate.sIgnoredExtensions : "(default/none)");
            printf("  ignored patterns: %s\n", tFileSyncTemplate.sIgnoredPathPatterns ? tFileSyncTemplate.sIgnoredPathPatterns : "(none)");
            for ( i = 0u; i < tEvents.iItemCount; ++i ) {
                printf("  [%u] %s", (unsigned)(i + 1u), file_event_kind_name(tEvents.pItems[i].eKind));
                if ( tEvents.pItems[i].eKind == XLLM_MEMORY_FILE_EVENT_RENAMED ) {
                    printf(" %s -> %s\n",
                           tEvents.pItems[i].sPreviousPath ? tEvents.pItems[i].sPreviousPath : "(null)",
                           tEvents.pItems[i].sPath ? tEvents.pItems[i].sPath : "(null)");
                } else {
                    printf(" %s\n", tEvents.pItems[i].sPath ? tEvents.pItems[i].sPath : "(null)");
                }
            }

            iStatus = xllm_memory_sync_file_events(pMemory, &tEventOptions, &tChangeSet, &tError);
            if ( iStatus != XRT_NET_OK ) {
                fprintf(stderr, "sync file events failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                xllm_error_reset(&tError);
            } else {
                print_change_set(&tChangeSet);
                xllm_memory_change_set_reset(&tChangeSet);
            }
        }

        file_event_list_reset(&tEvents);
    } else if ( sSyncFiles && sSyncFiles[0] ) {
        xllm_memory_sync_file_options tFileSyncTemplate;
        xllm_memory_sync_files_options tBatchOptions;
        string_list tPaths;
        xllm_memory_sync_file_options *pItems = NULL;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");
        const char *sContinueOnError = getenv("XLLM_MEMORY_SYNC_FILES_CONTINUE_ON_ERROR");
        size_t i;

        memset(&tPaths, 0, sizeof(tPaths));
        xllm_memory_sync_file_options_init(&tFileSyncTemplate);
        xllm_memory_sync_files_options_init(&tBatchOptions);
        tFileSyncTemplate.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tFileSyncTemplate.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tFileSyncTemplate.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tFileSyncTemplate.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tFileSyncTemplate.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tFileSyncTemplate.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tFileSyncTemplate.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tFileSyncTemplate.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tFileSyncTemplate.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tFileSyncTemplate.bSkipUnchanged = false;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "0") != 0 ) {
            tFileSyncTemplate.bUseWorkspaceDefaults = true;
        }
        if ( sContinueOnError && strcmp(sContinueOnError, "0") == 0 ) {
            tBatchOptions.bContinueOnError = false;
        }

        if ( split_semicolon_list(sSyncFiles, &tPaths) != 0 || tPaths.iItemCount == 0u ) {
            fprintf(stderr, "failed to parse XLLM_MEMORY_SYNC_FILES\n");
        } else {
            pItems = (xllm_memory_sync_file_options *)calloc(tPaths.iItemCount, sizeof(*pItems));
            if ( !pItems ) {
                fprintf(stderr, "failed to allocate sync file batch\n");
            } else {
                for ( i = 0u; i < tPaths.iItemCount; ++i ) {
                    pItems[i] = tFileSyncTemplate;
                    pItems[i].sPath = tPaths.pItems[i];
                }
                tBatchOptions.pItems = pItems;
                tBatchOptions.iItemCount = tPaths.iItemCount;

                printf("syncing files batch (%u items)\n", (unsigned)tPaths.iItemCount);
                printf("  root path: %s\n", tFileSyncTemplate.sRootPath ? tFileSyncTemplate.sRootPath : "(none)");
                printf("  record id prefix: %s\n", tFileSyncTemplate.sRecordIdPrefix ? tFileSyncTemplate.sRecordIdPrefix : "(auto)");
                printf("  source uri prefix: %s\n", tFileSyncTemplate.sSourceUriPrefix ? tFileSyncTemplate.sSourceUriPrefix : "(default)");
                printf("  workspace defaults: %s\n", tFileSyncTemplate.bUseWorkspaceDefaults ? "true" : "false");
                printf("  skip unchanged: %s\n", tFileSyncTemplate.bSkipUnchanged ? "true" : "false");
                printf("  continue on error: %s\n", tBatchOptions.bContinueOnError ? "true" : "false");
                printf("  max file bytes: %llu\n", (unsigned long long)tFileSyncTemplate.uMaxFileBytes);
                printf("  extensions: %s\n", tFileSyncTemplate.sAllowedExtensions ? tFileSyncTemplate.sAllowedExtensions : "(default/all)");
                printf("  ignored dirs: %s\n", tFileSyncTemplate.sIgnoredDirectories ? tFileSyncTemplate.sIgnoredDirectories : "(default/none)");
                printf("  ignored exts: %s\n", tFileSyncTemplate.sIgnoredExtensions ? tFileSyncTemplate.sIgnoredExtensions : "(default/none)");
                printf("  ignored patterns: %s\n", tFileSyncTemplate.sIgnoredPathPatterns ? tFileSyncTemplate.sIgnoredPathPatterns : "(none)");
                for ( i = 0u; i < tPaths.iItemCount; ++i ) {
                    printf("  [%u] %s\n", (unsigned)(i + 1u), tPaths.pItems[i]);
                }

                iStatus = xllm_memory_sync_files(pMemory, &tBatchOptions, &tChangeSet, &tError);
                if ( iStatus != XRT_NET_OK ) {
                    fprintf(stderr, "sync files failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
                    xllm_error_reset(&tError);
                } else {
                    print_change_set(&tChangeSet);
                    xllm_memory_change_set_reset(&tChangeSet);
                }
            }
        }

        free(pItems);
        string_list_reset(&tPaths);
    } else if ( sSyncFile && sSyncFile[0] ) {
        xllm_memory_sync_file_options tFileSync;
        const char *sRootPath = getenv("XLLM_MEMORY_SYNC_FILE_ROOT");
        const char *sRecordIdPrefix = getenv("XLLM_MEMORY_SYNC_FILE_RECORD_ID_PREFIX");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_SYNC_FILE_SOURCE_URI_PREFIX");
        const char *sExtensions = getenv("XLLM_MEMORY_SYNC_FILE_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_SYNC_FILE_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_SYNC_FILE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_SYNC_FILE_SKIP_UNCHANGED");
        const char *sUseWorkspaceDefaults = getenv("XLLM_MEMORY_SYNC_FILE_WORKSPACE_DEFAULTS");

        xllm_memory_sync_file_options_init(&tFileSync);
        tFileSync.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tFileSync.sPath = sSyncFile;
        tFileSync.sRootPath = (sRootPath && sRootPath[0]) ? sRootPath : NULL;
        tFileSync.sRecordIdPrefix = (sRecordIdPrefix && sRecordIdPrefix[0]) ? sRecordIdPrefix : NULL;
        tFileSync.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : NULL;
        tFileSync.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tFileSync.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tFileSync.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tFileSync.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tFileSync.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tFileSync.bSkipUnchanged = false;
        }
        if ( sUseWorkspaceDefaults && strcmp(sUseWorkspaceDefaults, "0") != 0 ) {
            tFileSync.bUseWorkspaceDefaults = true;
        }

        printf("syncing file: %s\n", sSyncFile);
        printf("  root path: %s\n", tFileSync.sRootPath ? tFileSync.sRootPath : "(none)");
        printf("  record id prefix: %s\n", tFileSync.sRecordIdPrefix ? tFileSync.sRecordIdPrefix : "(auto)");
        printf("  source uri prefix: %s\n", tFileSync.sSourceUriPrefix ? tFileSync.sSourceUriPrefix : "(default)");
        printf("  workspace defaults: %s\n", tFileSync.bUseWorkspaceDefaults ? "true" : "false");
        printf("  skip unchanged: %s\n", tFileSync.bSkipUnchanged ? "true" : "false");
        printf("  max file bytes: %llu\n", (unsigned long long)tFileSync.uMaxFileBytes);
        printf("  extensions: %s\n", tFileSync.sAllowedExtensions ? tFileSync.sAllowedExtensions : "(default/all)");
        printf("  ignored dirs: %s\n", tFileSync.sIgnoredDirectories ? tFileSync.sIgnoredDirectories : "(default/none)");
        printf("  ignored exts: %s\n", tFileSync.sIgnoredExtensions ? tFileSync.sIgnoredExtensions : "(default/none)");
        printf("  ignored patterns: %s\n", tFileSync.sIgnoredPathPatterns ? tFileSync.sIgnoredPathPatterns : "(none)");
        iStatus = xllm_memory_sync_file(pMemory, &tFileSync, &tChangeSet, &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "sync file failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        } else {
            print_change_set(&tChangeSet);
            xllm_memory_change_set_reset(&tChangeSet);
        }
    } else if ( sSyncWorkspace && sSyncWorkspace[0] ) {
        xllm_memory_ingest_workspace_options tWorkspaceIngest;
        const char *sRecursive = getenv("XLLM_MEMORY_INGEST_DIR_RECURSIVE");
        const char *sExtensions = getenv("XLLM_MEMORY_INGEST_DIR_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_INGEST_DIR_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_INGEST_DIR_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_WORKSPACE_IGNORE_PATTERNS");
        const char *sLoadGitIgnore = getenv("XLLM_MEMORY_WORKSPACE_LOAD_GITIGNORE");
        const char *sIgnoreFiles = getenv("XLLM_MEMORY_WORKSPACE_IGNORE_FILES");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_WORKSPACE_SOURCE_URI_PREFIX");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_WORKSPACE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_WORKSPACE_SKIP_UNCHANGED");

        xllm_memory_ingest_workspace_options_init(&tWorkspaceIngest);
        xllm_memory_sync_workspace_result_init(&tSyncResult);
        tWorkspaceIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tWorkspaceIngest.sPath = sSyncWorkspace;
        tWorkspaceIngest.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tWorkspaceIngest.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tWorkspaceIngest.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tWorkspaceIngest.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        tWorkspaceIngest.sIgnoreFiles = (sIgnoreFiles && sIgnoreFiles[0]) ? sIgnoreFiles : NULL;
        tWorkspaceIngest.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : tWorkspaceIngest.sSourceUriPrefix;
        if ( sRecursive && strcmp(sRecursive, "0") == 0 ) {
            tWorkspaceIngest.bRecursive = false;
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tWorkspaceIngest.bSkipUnchanged = false;
        }
        if ( sLoadGitIgnore && strcmp(sLoadGitIgnore, "0") == 0 ) {
            tWorkspaceIngest.bLoadGitIgnore = false;
        }
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tWorkspaceIngest.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        printf("syncing workspace: %s\n", sSyncWorkspace);
        printf("  recursive: %s\n", tWorkspaceIngest.bRecursive ? "true" : "false");
        printf("  skip unchanged: %s\n", tWorkspaceIngest.bSkipUnchanged ? "true" : "false");
        printf("  source uri prefix: %s\n", tWorkspaceIngest.sSourceUriPrefix ? tWorkspaceIngest.sSourceUriPrefix : "(none)");
        printf("  max file bytes: %llu\n", (unsigned long long)tWorkspaceIngest.uMaxFileBytes);
        printf("  extensions: %s\n", tWorkspaceIngest.sAllowedExtensions ? tWorkspaceIngest.sAllowedExtensions : "(workspace default)");
        printf("  ignored dirs: %s\n", tWorkspaceIngest.sIgnoredDirectories ? tWorkspaceIngest.sIgnoredDirectories : "(workspace default)");
        printf("  ignored exts: %s\n", tWorkspaceIngest.sIgnoredExtensions ? tWorkspaceIngest.sIgnoredExtensions : "(workspace default)");
        printf("  ignored patterns: %s\n", tWorkspaceIngest.sIgnoredPathPatterns ? tWorkspaceIngest.sIgnoredPathPatterns : "(none)");
        printf("  load .gitignore: %s\n", tWorkspaceIngest.bLoadGitIgnore ? "true" : "false");
        printf("  ignore files: %s\n", tWorkspaceIngest.sIgnoreFiles ? tWorkspaceIngest.sIgnoreFiles : "(none)");
        iStatus = xllm_memory_sync_workspace(pMemory, &tWorkspaceIngest, &tSyncResult, &tError);
        printf("workspace sync result: visited=%u ingested=%u created=%u updated=%u skipped=%u failed=%u examined=%u removed=%u\n",
               (unsigned)tSyncResult.tIngest.uVisitedFileCount,
               (unsigned)tSyncResult.tIngest.uIngestedFileCount,
               (unsigned)tSyncResult.tIngest.uCreatedRecordCount,
               (unsigned)tSyncResult.tIngest.uUpdatedRecordCount,
               (unsigned)tSyncResult.tIngest.uSkippedFileCount,
               (unsigned)tSyncResult.tIngest.uFailedFileCount,
               (unsigned)tSyncResult.uExaminedRecordCount,
               (unsigned)tSyncResult.uRemovedRecordCount);
        printf("  detail counts: created=%u updated=%u skipped=%u failed=%u removed=%u\n",
               (unsigned)tSyncResult.tIngest.iCreatedDetailCount,
               (unsigned)tSyncResult.tIngest.iUpdatedDetailCount,
               (unsigned)tSyncResult.tIngest.iSkippedDetailCount,
               (unsigned)tSyncResult.tIngest.iFailedDetailCount,
               (unsigned)tSyncResult.iRemovedDetailCount);
        if ( tSyncResult.tIngest.iSkippedDetailCount > 0u ) {
            print_skipped_files(tSyncResult.tIngest.pSkippedFiles, tSyncResult.tIngest.iSkippedDetailCount);
        }
        if ( tSyncResult.tIngest.iFailedDetailCount > 0u ) {
            print_failed_files(tSyncResult.tIngest.pFailedFiles, tSyncResult.tIngest.iFailedDetailCount);
        }
        if ( xllm_memory_make_change_set_from_sync(&tSyncResult, &tChangeSet, &tError) == XRT_NET_OK ) {
            print_change_set(&tChangeSet);
            xllm_memory_change_set_reset(&tChangeSet);
        } else {
            fprintf(stderr, "change set build failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        }
    } else if ( sIngestWorkspace && sIngestWorkspace[0] ) {
        xllm_memory_ingest_workspace_options tWorkspaceIngest;
        const char *sRecursive = getenv("XLLM_MEMORY_INGEST_DIR_RECURSIVE");
        const char *sExtensions = getenv("XLLM_MEMORY_INGEST_DIR_EXTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_INGEST_DIR_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_INGEST_DIR_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_WORKSPACE_IGNORE_PATTERNS");
        const char *sLoadGitIgnore = getenv("XLLM_MEMORY_WORKSPACE_LOAD_GITIGNORE");
        const char *sIgnoreFiles = getenv("XLLM_MEMORY_WORKSPACE_IGNORE_FILES");
        const char *sSourceUriPrefix = getenv("XLLM_MEMORY_WORKSPACE_SOURCE_URI_PREFIX");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_WORKSPACE_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_WORKSPACE_SKIP_UNCHANGED");

        xllm_memory_ingest_workspace_options_init(&tWorkspaceIngest);
        tWorkspaceIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tWorkspaceIngest.sPath = sIngestWorkspace;
        tWorkspaceIngest.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : NULL;
        tWorkspaceIngest.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tWorkspaceIngest.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tWorkspaceIngest.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        tWorkspaceIngest.sIgnoreFiles = (sIgnoreFiles && sIgnoreFiles[0]) ? sIgnoreFiles : NULL;
        tWorkspaceIngest.sSourceUriPrefix = (sSourceUriPrefix && sSourceUriPrefix[0]) ? sSourceUriPrefix : tWorkspaceIngest.sSourceUriPrefix;
        if ( sRecursive && strcmp(sRecursive, "0") == 0 ) {
            tWorkspaceIngest.bRecursive = false;
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") == 0 ) {
            tWorkspaceIngest.bSkipUnchanged = false;
        }
        if ( sLoadGitIgnore && strcmp(sLoadGitIgnore, "0") == 0 ) {
            tWorkspaceIngest.bLoadGitIgnore = false;
        }
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tWorkspaceIngest.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        printf("ingesting workspace: %s\n", sIngestWorkspace);
        printf("  recursive: %s\n", tWorkspaceIngest.bRecursive ? "true" : "false");
        printf("  skip unchanged: %s\n", tWorkspaceIngest.bSkipUnchanged ? "true" : "false");
        printf("  source uri prefix: %s\n", tWorkspaceIngest.sSourceUriPrefix ? tWorkspaceIngest.sSourceUriPrefix : "(none)");
        printf("  max file bytes: %llu\n", (unsigned long long)tWorkspaceIngest.uMaxFileBytes);
        printf("  extensions: %s\n", tWorkspaceIngest.sAllowedExtensions ? tWorkspaceIngest.sAllowedExtensions : "(workspace default)");
        printf("  ignored dirs: %s\n", tWorkspaceIngest.sIgnoredDirectories ? tWorkspaceIngest.sIgnoredDirectories : "(workspace default)");
        printf("  ignored exts: %s\n", tWorkspaceIngest.sIgnoredExtensions ? tWorkspaceIngest.sIgnoredExtensions : "(workspace default)");
        printf("  ignored patterns: %s\n", tWorkspaceIngest.sIgnoredPathPatterns ? tWorkspaceIngest.sIgnoredPathPatterns : "(none)");
        printf("  load .gitignore: %s\n", tWorkspaceIngest.bLoadGitIgnore ? "true" : "false");
        printf("  ignore files: %s\n", tWorkspaceIngest.sIgnoreFiles ? tWorkspaceIngest.sIgnoreFiles : "(none)");
        iStatus = xllm_memory_ingest_workspace(pMemory, &tWorkspaceIngest, &tDirectoryResult, &tError);
        printf("workspace ingest result: visited=%u ingested=%u created=%u updated=%u skipped=%u failed=%u\n",
               (unsigned)tDirectoryResult.uVisitedFileCount,
               (unsigned)tDirectoryResult.uIngestedFileCount,
               (unsigned)tDirectoryResult.uCreatedRecordCount,
               (unsigned)tDirectoryResult.uUpdatedRecordCount,
               (unsigned)tDirectoryResult.uSkippedFileCount,
               (unsigned)tDirectoryResult.uFailedFileCount);
        printf("  detail counts: created=%u updated=%u skipped=%u failed=%u\n",
               (unsigned)tDirectoryResult.iCreatedDetailCount,
               (unsigned)tDirectoryResult.iUpdatedDetailCount,
               (unsigned)tDirectoryResult.iSkippedDetailCount,
               (unsigned)tDirectoryResult.iFailedDetailCount);
        if ( tDirectoryResult.iSkippedDetailCount > 0u ) {
            print_skipped_files(tDirectoryResult.pSkippedFiles, tDirectoryResult.iSkippedDetailCount);
        }
        if ( tDirectoryResult.iFailedDetailCount > 0u ) {
            print_failed_files(tDirectoryResult.pFailedFiles, tDirectoryResult.iFailedDetailCount);
        }
        if ( xllm_memory_make_change_set_from_ingest(&tDirectoryResult, &tChangeSet, &tError) == XRT_NET_OK ) {
            print_change_set(&tChangeSet);
            xllm_memory_change_set_reset(&tChangeSet);
        } else {
            fprintf(stderr, "change set build failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        }
    } else if ( sIngestDir && sIngestDir[0] ) {
        xllm_memory_ingest_directory_options tDirectoryIngest;
        const char *sRecursive = getenv("XLLM_MEMORY_INGEST_DIR_RECURSIVE");
        const char *sExtensions = getenv("XLLM_MEMORY_INGEST_DIR_EXTS");
        const char *sWorkspaceDefaults = getenv("XLLM_MEMORY_INGEST_DIR_WORKSPACE_DEFAULTS");
        const char *sIgnoredDirs = getenv("XLLM_MEMORY_INGEST_DIR_IGNORE_DIRS");
        const char *sIgnoredExts = getenv("XLLM_MEMORY_INGEST_DIR_IGNORE_EXTS");
        const char *sIgnoredPatterns = getenv("XLLM_MEMORY_INGEST_DIR_IGNORE_PATTERNS");
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_INGEST_DIR_MAX_FILE_BYTES");
        const char *sSkipUnchanged = getenv("XLLM_MEMORY_INGEST_DIR_SKIP_UNCHANGED");

        xllm_memory_ingest_directory_options_init(&tDirectoryIngest);
        tDirectoryIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tDirectoryIngest.sPath = sIngestDir;
        tDirectoryIngest.sAllowedExtensions = (sExtensions && sExtensions[0]) ? sExtensions : ".c;.h;.md;.txt;.bat";
        tDirectoryIngest.sIgnoredDirectories = (sIgnoredDirs && sIgnoredDirs[0]) ? sIgnoredDirs : NULL;
        tDirectoryIngest.sIgnoredExtensions = (sIgnoredExts && sIgnoredExts[0]) ? sIgnoredExts : NULL;
        tDirectoryIngest.sIgnoredPathPatterns = (sIgnoredPatterns && sIgnoredPatterns[0]) ? sIgnoredPatterns : NULL;
        if ( sWorkspaceDefaults && strcmp(sWorkspaceDefaults, "0") != 0 ) {
            tDirectoryIngest.bUseWorkspaceDefaults = true;
            if ( !sExtensions || !sExtensions[0] ) {
                tDirectoryIngest.sAllowedExtensions = NULL;
            }
        }
        if ( sRecursive && strcmp(sRecursive, "0") == 0 ) {
            tDirectoryIngest.bRecursive = false;
        }
        if ( sSkipUnchanged && strcmp(sSkipUnchanged, "0") != 0 ) {
            tDirectoryIngest.bSkipUnchanged = true;
        }
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tDirectoryIngest.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        printf("ingesting directory: %s\n", sIngestDir);
        printf("  recursive: %s\n", tDirectoryIngest.bRecursive ? "true" : "false");
        printf("  workspace defaults: %s\n", tDirectoryIngest.bUseWorkspaceDefaults ? "true" : "false");
        printf("  skip unchanged: %s\n", tDirectoryIngest.bSkipUnchanged ? "true" : "false");
        printf("  max file bytes: %llu\n", (unsigned long long)tDirectoryIngest.uMaxFileBytes);
        printf("  extensions: %s\n", tDirectoryIngest.sAllowedExtensions ? tDirectoryIngest.sAllowedExtensions : "(default/all)");
        printf("  ignored dirs: %s\n", tDirectoryIngest.sIgnoredDirectories ? tDirectoryIngest.sIgnoredDirectories : "(default/none)");
        printf("  ignored exts: %s\n", tDirectoryIngest.sIgnoredExtensions ? tDirectoryIngest.sIgnoredExtensions : "(default/none)");
        printf("  ignored patterns: %s\n", tDirectoryIngest.sIgnoredPathPatterns ? tDirectoryIngest.sIgnoredPathPatterns : "(none)");
        iStatus = xllm_memory_ingest_directory(pMemory, &tDirectoryIngest, &tDirectoryResult, &tError);
        printf("directory ingest result: visited=%u ingested=%u created=%u updated=%u skipped=%u failed=%u\n",
               (unsigned)tDirectoryResult.uVisitedFileCount,
               (unsigned)tDirectoryResult.uIngestedFileCount,
               (unsigned)tDirectoryResult.uCreatedRecordCount,
               (unsigned)tDirectoryResult.uUpdatedRecordCount,
               (unsigned)tDirectoryResult.uSkippedFileCount,
               (unsigned)tDirectoryResult.uFailedFileCount);
        printf("  detail counts: created=%u updated=%u skipped=%u failed=%u\n",
               (unsigned)tDirectoryResult.iCreatedDetailCount,
               (unsigned)tDirectoryResult.iUpdatedDetailCount,
               (unsigned)tDirectoryResult.iSkippedDetailCount,
               (unsigned)tDirectoryResult.iFailedDetailCount);
        if ( tDirectoryResult.iSkippedDetailCount > 0u ) {
            print_skipped_files(tDirectoryResult.pSkippedFiles, tDirectoryResult.iSkippedDetailCount);
        }
        if ( tDirectoryResult.iFailedDetailCount > 0u ) {
            print_failed_files(tDirectoryResult.pFailedFiles, tDirectoryResult.iFailedDetailCount);
        }
        if ( xllm_memory_make_change_set_from_ingest(&tDirectoryResult, &tChangeSet, &tError) == XRT_NET_OK ) {
            print_change_set(&tChangeSet);
            xllm_memory_change_set_reset(&tChangeSet);
        } else {
            fprintf(stderr, "change set build failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
        }
    } else if ( sIngestPath && sIngestPath[0] ) {
        xllm_memory_ingest_file_options tFileIngest;
        const char *sMaxFileBytes = getenv("XLLM_MEMORY_INGEST_FILE_MAX_FILE_BYTES");

        xllm_memory_ingest_file_options_init(&tFileIngest);
        tFileIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tFileIngest.sPath = sIngestPath;
        if ( sMaxFileBytes && sMaxFileBytes[0] ) {
            tFileIngest.uMaxFileBytes = (uint64)strtoull(sMaxFileBytes, NULL, 10);
        }
        printf("ingesting file: %s\n", sIngestPath);
        iStatus = xllm_memory_ingest_file(pMemory, &tFileIngest, &tError);
    } else {
        tIngest.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
        tIngest.sRecordId = "memory-design";
        tIngest.sTitle = "memory design";
        tIngest.sSourceUri = "workspace://memory_design";
        tIngest.sText =
            "xllm-memory should provide long-term memory, knowledge ingestion, chunking, retrieval, "
            "and a bridge that turns retrieved chunks into context blocks. "
            "The first implementation can start with an in-memory store and lexical retrieval "
            "before ONNX embedding and vector indexing are added.";
        iStatus = xllm_memory_ingest_text(pMemory, &tIngest, &tError);
    }
    if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_error_reset(&tError);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        return 1;
    }

    tSearch.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tSearch.sQuery = "How should xllm memory support retrieval and context blocks?";
    tSearch.uMaxHits = 3u;
    tSearch.sMetadataKey = sFilterMetadataKey;
    tSearch.sMetadataValue = sFilterMetadataValue;
    iStatus = xllm_memory_search(pMemory, &tSearch, &tResult, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "search failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_error_reset(&tError);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        return 1;
    }

    print_hits(&tResult);

    tList.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tList.uMaxItems = 8u;
    tList.sMetadataKey = sFilterMetadataKey;
    tList.sMetadataValue = sFilterMetadataValue;
    iStatus = xllm_memory_list_records(pMemory, &tList, &tRecordList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list_records failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_error_reset(&tError);
        xllm_memory_search_result_reset(&tResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        return 1;
    }
    print_records(&tRecordList);

    xllm_memory_list_options_init(&tList);
    tList.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
    tList.sTextContains = "context";
    tList.uMaxItems = 4u;
    tList.sMetadataKey = sFilterMetadataKey;
    tList.sMetadataValue = sFilterMetadataValue;
    iStatus = xllm_memory_list_chunks(pMemory, &tList, &tChunkList, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "list_chunks failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_error_reset(&tError);
        xllm_memory_record_list_result_reset(&tRecordList);
        xllm_memory_search_result_reset(&tResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        return 1;
    }
    print_chunks(&tChunkList);
    xllm_memory_chunk_list_result_reset(&tChunkList);
    xllm_memory_record_list_result_reset(&tRecordList);

    tContext.sLabel = "Retrieved project knowledge";
    iStatus = xllm_memory_apply_search_to_request(&tRequest, &tResult, &tContext, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "apply_to_request failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        xllm_error_reset(&tError);
        xllm_memory_search_result_reset(&tResult);
        xllm_memory_destroy(pMemory);
        xllm_runtime_destroy(pRuntime);
        return 1;
    }

    printf("request context blocks: %u\n", (unsigned)tRequest.iContextBlockCount);
    printf("memory records: %u\n", (unsigned)xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_ANY));
    printf("memory chunks: %u\n", (unsigned)xllm_memory_chunk_count(pMemory, XLLM_MEMORY_SCOPE_ANY));

    xllm_request_reset(&tRequest);
    xllm_memory_search_result_reset(&tResult);
    xllm_memory_ingest_directory_result_reset(&tDirectoryResult);
    xllm_memory_sync_workspace_result_reset(&tSyncResult);
    xllm_memory_change_set_reset(&tChangeSet);
    xllm_memory_destroy(pMemory);
    pMemory = NULL;

    if ( sSqlitePath && sSqlitePath[0] ) {
        memset(&tResult, 0, sizeof(tResult));
        iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pReloadedMemory);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "memory recreate failed: %d\n", iStatus);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 1;
        }
        iStatus = xllm_memory_search(pReloadedMemory, &tSearch, &tResult, &tError);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "search after reload failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
            xllm_error_reset(&tError);
            xllm_memory_destroy(pReloadedMemory);
            xllm_runtime_destroy(pRuntime);
            xllm_error_free(&tError);
            return 1;
        }
        printf("reloaded memory records: %u\n", (unsigned)xllm_memory_record_count(pReloadedMemory, XLLM_MEMORY_SCOPE_ANY));
        printf("reloaded memory chunks: %u\n", (unsigned)xllm_memory_chunk_count(pReloadedMemory, XLLM_MEMORY_SCOPE_ANY));
        print_hits(&tResult);
        xllm_memory_search_result_reset(&tResult);
        xllm_memory_destroy(pReloadedMemory);
        pReloadedMemory = NULL;
    }

    xllm_runtime_destroy(pRuntime);
    xllm_error_free(&tError);
    return 0;
}
