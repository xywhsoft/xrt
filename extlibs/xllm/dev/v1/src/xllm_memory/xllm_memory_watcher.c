XLLM_API int xllm_memory_file_event_queue_create(
    xllm_memory_file_event_queue **ppQueue
)
{
    xllm_memory_file_event_queue *pQueue;

    if ( !ppQueue ) {
        return XRT_NET_ERROR;
    }

    *ppQueue = NULL;
    pQueue = (xllm_memory_file_event_queue *)xrtCalloc(1u, sizeof(*pQueue));
    if ( !pQueue ) {
        return XRT_NET_ERROR;
    }
    pQueue->pMutex = xrtMutexCreate();
    if ( !pQueue->pMutex ) {
        xrtFree(pQueue);
        return XRT_NET_ERROR;
    }

    *ppQueue = pQueue;
    return XRT_NET_OK;
}

XLLM_API void xllm_memory_file_event_queue_destroy(
    xllm_memory_file_event_queue *pQueue
)
{
    if ( !pQueue ) {
        return;
    }

    if ( pQueue->pMutex ) {
        xrtMutexLock(pQueue->pMutex);
    }
    xllm__memory_file_event_array_reset(pQueue->pItems, pQueue->iItemCount);
    pQueue->pItems = NULL;
    pQueue->iItemCount = 0u;
    pQueue->iItemCapacity = 0u;
    if ( pQueue->pMutex ) {
        xrtMutexUnlock(pQueue->pMutex);
        xrtMutexDestroy(pQueue->pMutex);
    }
    xrtFree(pQueue);
}

XLLM_API void xllm_memory_file_event_queue_clear(
    xllm_memory_file_event_queue *pQueue
)
{
    if ( !pQueue || !pQueue->pMutex ) {
        return;
    }

    xrtMutexLock(pQueue->pMutex);
    xllm__memory_file_event_array_reset(pQueue->pItems, pQueue->iItemCount);
    pQueue->pItems = NULL;
    pQueue->iItemCount = 0u;
    pQueue->iItemCapacity = 0u;
    xrtMutexUnlock(pQueue->pMutex);
}

XLLM_API size_t xllm_memory_file_event_queue_count(
    const xllm_memory_file_event_queue *pQueue
)
{
    size_t iCount = 0u;
    xllm_memory_file_event_queue *pMutableQueue = (xllm_memory_file_event_queue *)pQueue;

    if ( !pMutableQueue || !pMutableQueue->pMutex ) {
        return 0u;
    }

    xrtMutexLock(pMutableQueue->pMutex);
    iCount = pMutableQueue->iItemCount;
    xrtMutexUnlock(pMutableQueue->pMutex);
    return iCount;
}

XLLM_API int xllm_memory_file_event_queue_compact(
    xllm_memory_file_event_queue *pQueue,
    xllm_error *pError
)
{
    xllm_memory_file_event *pCompacted = NULL;
    size_t iCompactedCount = 0u;
    size_t i;
    size_t j;

    if ( !pQueue || !pQueue->pMutex ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event queue compact requires queue");
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pQueue->pMutex);
    if ( pQueue->iItemCount <= 1u ) {
        xrtMutexUnlock(pQueue->pMutex);
        return XRT_NET_OK;
    }

    pCompacted = (xllm_memory_file_event *)xrtCalloc(
        pQueue->iItemCount,
        sizeof(*pCompacted)
    );
    if ( !pCompacted ) {
        xrtMutexUnlock(pQueue->pMutex);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate compacted memory file event queue");
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pQueue->iItemCount; ++i ) {
        bool bHandled = false;

        for ( j = iCompactedCount; j > 0u; --j ) {
            bool bRemoveExisting = false;

            if ( xllm__memory_file_event_try_merge(
                    &pCompacted[j - 1u],
                    &pQueue->pItems[i],
                    &bHandled,
                    &bRemoveExisting,
                    pError
                 ) != XRT_NET_OK ) {
                xrtMutexUnlock(pQueue->pMutex);
                xllm__memory_file_event_array_reset(pCompacted, iCompactedCount);
                return XRT_NET_ERROR;
            }
            if ( bHandled ) {
                if ( bRemoveExisting ) {
                    xllm__memory_file_event_array_remove_at(pCompacted, &iCompactedCount, j - 1u);
                }
                break;
            }
        }

        if ( !bHandled ) {
            if ( xllm__memory_file_event_copy(&pCompacted[iCompactedCount], &pQueue->pItems[i]) != XRT_NET_OK ) {
                xrtMutexUnlock(pQueue->pMutex);
                xllm__memory_file_event_array_reset(pCompacted, iCompactedCount);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to compact memory file event queue");
                return XRT_NET_ERROR;
            }
            ++iCompactedCount;
        }
    }

    xllm__memory_file_event_array_reset(pQueue->pItems, pQueue->iItemCount);
    pQueue->pItems = pCompacted;
    pQueue->iItemCount = iCompactedCount;
    pQueue->iItemCapacity = iCompactedCount;
    xrtMutexUnlock(pQueue->pMutex);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_file_event_queue_push(
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
)
{
    xllm_memory_file_event tCopy;
    xllm_memory_file_event *pLast;
    xllm_memory_file_event *pNewItems;
    size_t iNewCapacity;

    if ( !pQueue || !pQueue->pMutex ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event queue is required");
        return XRT_NET_ERROR;
    }
    if ( xllm__memory_validate_file_event(pEvent, pError) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pQueue->pMutex);
    if ( pQueue->iItemCount > 0u ) {
        pLast = &pQueue->pItems[pQueue->iItemCount - 1u];
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_CREATED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPath) ) {
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPath) ) {
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_CREATED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPath) ) {
            xllm__memory_file_event_reset(pLast);
            --pQueue->iItemCount;
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPath) ) {
            xllm_memory_file_event tDelete;

            if ( xllm__memory_file_event_copy(&tDelete, pEvent) != XRT_NET_OK ) {
                xrtMutexUnlock(pQueue->pMutex);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to coalesce memory file delete event");
                return XRT_NET_ERROR;
            }
            xllm__memory_file_event_reset(pLast);
            *pLast = tDelete;
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_CREATED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_RENAMED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPreviousPath) ) {
            char *sNewPath = xllm__dup_cstr(pEvent->sPath);
            if ( !sNewPath ) {
                xrtMutexUnlock(pQueue->pMutex);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to coalesce memory file rename event");
                return XRT_NET_ERROR;
            }
            xllm__free_cstr((char **)&pLast->sPath);
            pLast->sPath = sNewPath;
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_RENAMED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPreviousPath) ) {
            xllm_memory_file_event tRename;

            if ( xllm__memory_file_event_copy(&tRename, pEvent) != XRT_NET_OK ) {
                xrtMutexUnlock(pQueue->pMutex);
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to coalesce memory file rename event");
                return XRT_NET_ERROR;
            }
            xllm__memory_file_event_reset(pLast);
            *pLast = tRename;
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_RENAMED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_UPDATED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPath) ) {
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
        if ( pLast->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
             pEvent->eKind == XLLM_MEMORY_FILE_EVENT_DELETED &&
             xllm__memory_path_equal_ci(pLast->sPath, pEvent->sPath) ) {
            xrtMutexUnlock(pQueue->pMutex);
            return XRT_NET_OK;
        }
    }

    if ( xllm__memory_file_event_copy(&tCopy, pEvent) != XRT_NET_OK ) {
        xrtMutexUnlock(pQueue->pMutex);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to copy memory file event");
        return XRT_NET_ERROR;
    }
    if ( pQueue->iItemCount == pQueue->iItemCapacity ) {
        iNewCapacity = pQueue->iItemCapacity > 0u ? (pQueue->iItemCapacity * 2u) : 8u;
        pNewItems = (xllm_memory_file_event *)xrtRealloc(
            pQueue->pItems,
            iNewCapacity * sizeof(*pNewItems)
        );
        if ( !pNewItems ) {
            xrtMutexUnlock(pQueue->pMutex);
            xllm__memory_file_event_reset(&tCopy);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to grow memory file event queue");
            return XRT_NET_ERROR;
        }
        pQueue->pItems = pNewItems;
        pQueue->iItemCapacity = iNewCapacity;
    }
    pQueue->pItems[pQueue->iItemCount++] = tCopy;
    xrtMutexUnlock(pQueue->pMutex);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_file_event_queue_push_created(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_CREATED;
    tEvent.sPath = sPath;
    return xllm_memory_file_event_queue_push(pQueue, &tEvent, pError);
}

XLLM_API int xllm_memory_file_event_queue_push_updated(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
    tEvent.sPath = sPath;
    return xllm_memory_file_event_queue_push(pQueue, &tEvent, pError);
}

XLLM_API int xllm_memory_file_event_queue_push_deleted(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_DELETED;
    tEvent.sPath = sPath;
    return xllm_memory_file_event_queue_push(pQueue, &tEvent, pError);
}

XLLM_API int xllm_memory_file_event_queue_push_renamed(
    xllm_memory_file_event_queue *pQueue,
    const char *sPreviousPath,
    const char *sPath,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_RENAMED;
    tEvent.sPreviousPath = sPreviousPath;
    tEvent.sPath = sPath;
    return xllm_memory_file_event_queue_push(pQueue, &tEvent, pError);
}

XLLM_API int xllm_memory_file_event_queue_drain(
    xllm_memory *pMemory,
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event_queue_drain_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    xllm_memory_file_event_queue_drain_options tDefaultOptions;
    const xllm_memory_file_event_queue_drain_options *pUseOptions = pOptions;
    xllm_memory_sync_file_events_options tSyncOptions;
    xllm_memory_file_event *pDrainedItems = NULL;
    size_t iDrainCount = 0u;
    int iStatus;

    if ( !pMemory || !pQueue || !pQueue->pMutex || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory file event queue drain requires memory, queue, and output");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_file_event_queue_drain_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm_memory_change_set_reset(pResult);
    if ( xllm_memory_file_event_queue_compact(pQueue, pError) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    xrtMutexLock(pQueue->pMutex);
    iDrainCount = pQueue->iItemCount;
    if ( pUseOptions->iMaxItems > 0u && iDrainCount > pUseOptions->iMaxItems ) {
        iDrainCount = pUseOptions->iMaxItems;
    }
    if ( iDrainCount > 0u ) {
        size_t iRemaining;

        pDrainedItems = (xllm_memory_file_event *)xrtCalloc(iDrainCount, sizeof(*pDrainedItems));
        if ( !pDrainedItems ) {
            xrtMutexUnlock(pQueue->pMutex);
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate drained memory file events");
            return XRT_NET_ERROR;
        }
        memcpy(pDrainedItems, pQueue->pItems, iDrainCount * sizeof(*pDrainedItems));
        memset(pQueue->pItems, 0, iDrainCount * sizeof(*pDrainedItems));
        iRemaining = pQueue->iItemCount - iDrainCount;
        if ( iRemaining > 0u ) {
            memmove(
                pQueue->pItems,
                pQueue->pItems + iDrainCount,
                iRemaining * sizeof(*pQueue->pItems)
            );
            memset(
                pQueue->pItems + iRemaining,
                0,
                iDrainCount * sizeof(*pQueue->pItems)
            );
        }
        pQueue->iItemCount = iRemaining;
    }
    xrtMutexUnlock(pQueue->pMutex);

    if ( iDrainCount == 0u ) {
        return XRT_NET_OK;
    }

    xllm_memory_sync_file_events_options_init(&tSyncOptions);
    tSyncOptions.tBaseOptions = pUseOptions->tBaseOptions;
    tSyncOptions.pItems = pDrainedItems;
    tSyncOptions.iItemCount = iDrainCount;
    tSyncOptions.bContinueOnError = pUseOptions->bContinueOnError;
    tSyncOptions.tVendorExtra = pUseOptions->tVendorExtra;

    iStatus = xllm_memory_sync_file_events(pMemory, &tSyncOptions, pResult, pError);
    xllm__memory_file_event_array_reset(pDrainedItems, iDrainCount);
    return iStatus;
}

XLLM_API int xllm_memory_watcher_bridge_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_bridge_options *pOptions,
    xllm_memory_watcher_bridge **ppBridge,
    xllm_error *pError
)
{
    xllm_memory_watcher_bridge_options tDefaultOptions;
    const xllm_memory_watcher_bridge_options *pUseOptions = pOptions;
    xllm_memory_watcher_bridge *pBridge;
    int iStatus;

    if ( !pMemory || !ppBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge create requires memory handle and output");
        return XRT_NET_ERROR;
    }

    *ppBridge = NULL;
    if ( !pUseOptions ) {
        xllm_memory_watcher_bridge_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    pBridge = (xllm_memory_watcher_bridge *)xrtCalloc(1u, sizeof(*pBridge));
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory watcher bridge");
        return XRT_NET_ERROR;
    }

    iStatus = xllm_memory_file_event_queue_create(&pBridge->pQueue);
    if ( iStatus != XRT_NET_OK ) {
        xrtFree(pBridge);
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to create memory watcher bridge queue");
        return iStatus;
    }

    pBridge->pMemory = pMemory;
    xllm_memory_file_event_queue_drain_options_init(&pBridge->tDrainOptions);
    pBridge->tDrainOptions.tBaseOptions = pUseOptions->tBaseOptions;
    pBridge->tDrainOptions.iMaxItems = pUseOptions->iDefaultMaxItems;
    pBridge->tDrainOptions.bContinueOnError = pUseOptions->bContinueOnError;
    pBridge->tDrainOptions.tVendorExtra = pUseOptions->tVendorExtra;
    *ppBridge = pBridge;
    return XRT_NET_OK;
}

XLLM_API void xllm_memory_watcher_bridge_destroy(
    xllm_memory_watcher_bridge *pBridge
)
{
    if ( !pBridge ) {
        return;
    }

    xllm_memory_file_event_queue_destroy(pBridge->pQueue);
    xrtFree(pBridge);
}

XLLM_API void xllm_memory_watcher_bridge_clear(
    xllm_memory_watcher_bridge *pBridge
)
{
    if ( !pBridge ) {
        return;
    }

    xllm_memory_file_event_queue_clear(pBridge->pQueue);
}

XLLM_API size_t xllm_memory_watcher_bridge_pending_count(
    const xllm_memory_watcher_bridge *pBridge
)
{
    if ( !pBridge ) {
        return 0u;
    }

    return xllm_memory_file_event_queue_count(pBridge->pQueue);
}

XLLM_API int xllm_memory_watcher_bridge_compact_pending(
    xllm_memory_watcher_bridge *pBridge,
    xllm_error *pError
)
{
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge compact requires bridge");
        return XRT_NET_ERROR;
    }

    return xllm_memory_file_event_queue_compact(pBridge->pQueue, pError);
}

XLLM_API int xllm_memory_watcher_bridge_push(
    xllm_memory_watcher_bridge *pBridge,
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
)
{
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge push requires bridge");
        return XRT_NET_ERROR;
    }

    return xllm_memory_file_event_queue_push(pBridge->pQueue, pEvent, pError);
}

XLLM_API int xllm_memory_watcher_bridge_push_created(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPath,
    xllm_error *pError
)
{
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge push created requires bridge");
        return XRT_NET_ERROR;
    }

    return xllm_memory_file_event_queue_push_created(pBridge->pQueue, sPath, pError);
}

XLLM_API int xllm_memory_watcher_bridge_push_updated(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPath,
    xllm_error *pError
)
{
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge push updated requires bridge");
        return XRT_NET_ERROR;
    }

    return xllm_memory_file_event_queue_push_updated(pBridge->pQueue, sPath, pError);
}

XLLM_API int xllm_memory_watcher_bridge_push_deleted(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPath,
    xllm_error *pError
)
{
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge push deleted requires bridge");
        return XRT_NET_ERROR;
    }

    return xllm_memory_file_event_queue_push_deleted(pBridge->pQueue, sPath, pError);
}

XLLM_API int xllm_memory_watcher_bridge_push_renamed(
    xllm_memory_watcher_bridge *pBridge,
    const char *sPreviousPath,
    const char *sPath,
    xllm_error *pError
)
{
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge push renamed requires bridge");
        return XRT_NET_ERROR;
    }

    return xllm_memory_file_event_queue_push_renamed(pBridge->pQueue, sPreviousPath, sPath, pError);
}

XLLM_API int xllm_memory_watcher_bridge_flush(
    xllm_memory_watcher_bridge *pBridge,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge flush requires bridge");
        return XRT_NET_ERROR;
    }

    return xllm_memory_file_event_queue_drain(
        pBridge->pMemory,
        pBridge->pQueue,
        &pBridge->tDrainOptions,
        pResult,
        pError
    );
}

XLLM_API int xllm_memory_watcher_bridge_flush_max(
    xllm_memory_watcher_bridge *pBridge,
    size_t iMaxItems,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    xllm_memory_file_event_queue_drain_options tDrainOptions;

    if ( !pBridge ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher bridge flush max requires bridge");
        return XRT_NET_ERROR;
    }

    tDrainOptions = pBridge->tDrainOptions;
    tDrainOptions.iMaxItems = iMaxItems;
    return xllm_memory_file_event_queue_drain(
        pBridge->pMemory,
        pBridge->pQueue,
        &tDrainOptions,
        pResult,
        pError
    );
}

XLLM_API int xllm_memory_watcher_pump_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_pump_options *pOptions,
    xllm_memory_watcher_pump **ppPump,
    xllm_error *pError
)
{
    xllm_memory_watcher_pump_options tDefaultOptions;
    const xllm_memory_watcher_pump_options *pUseOptions = pOptions;
    xllm_memory_watcher_pump *pPump;
    int iStatus;

    if ( !pMemory || !ppPump ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher pump create requires memory handle and output");
        return XRT_NET_ERROR;
    }

    *ppPump = NULL;
    if ( !pUseOptions ) {
        xllm_memory_watcher_pump_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    pPump = (xllm_memory_watcher_pump *)xrtCalloc(1u, sizeof(*pPump));
    if ( !pPump ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory watcher pump");
        return XRT_NET_ERROR;
    }

    iStatus = xllm_memory_watcher_bridge_create(
        pMemory,
        &pUseOptions->tBridgeOptions,
        &pPump->pBridge,
        pError
    );
    if ( iStatus != XRT_NET_OK ) {
        xrtFree(pPump);
        return iStatus;
    }

    pPump->iAutoFlushThreshold = pUseOptions->iAutoFlushThreshold;
    *ppPump = pPump;
    return XRT_NET_OK;
}

XLLM_API void xllm_memory_watcher_pump_destroy(
    xllm_memory_watcher_pump *pPump
)
{
    if ( !pPump ) {
        return;
    }

    xllm_memory_watcher_bridge_destroy(pPump->pBridge);
    xrtFree(pPump);
}

XLLM_API void xllm_memory_watcher_pump_clear(
    xllm_memory_watcher_pump *pPump
)
{
    if ( !pPump ) {
        return;
    }

    xllm_memory_watcher_bridge_clear(pPump->pBridge);
}

XLLM_API size_t xllm_memory_watcher_pump_pending_count(
    const xllm_memory_watcher_pump *pPump
)
{
    if ( !pPump ) {
        return 0u;
    }

    return xllm_memory_watcher_bridge_pending_count(pPump->pBridge);
}

XLLM_API int xllm_memory_watcher_pump_compact_pending(
    xllm_memory_watcher_pump *pPump,
    xllm_error *pError
)
{
    if ( !pPump ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher pump compact requires pump");
        return XRT_NET_ERROR;
    }

    return xllm_memory_watcher_bridge_compact_pending(pPump->pBridge, pError);
}

static int xllm__memory_watcher_pump_maybe_flush(
    xllm_memory_watcher_pump *pPump,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
)
{
    bool bShouldFlush;
    xllm_memory_change_set tTempChanges;

    if ( pbFlushed ) {
        *pbFlushed = false;
    }
    if ( pAutoFlushResult ) {
        xllm_memory_change_set_reset(pAutoFlushResult);
    }
    if ( !pPump ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher pump requires pump");
        return XRT_NET_ERROR;
    }

    bShouldFlush = (pPump->iAutoFlushThreshold > 0u) &&
                   (xllm_memory_watcher_bridge_pending_count(pPump->pBridge) >= pPump->iAutoFlushThreshold);
    if ( !bShouldFlush ) {
        return XRT_NET_OK;
    }

    if ( pbFlushed ) {
        *pbFlushed = true;
    }
    if ( pAutoFlushResult ) {
        return xllm_memory_watcher_bridge_flush(pPump->pBridge, pAutoFlushResult, pError);
    }
    xllm_memory_change_set_init(&tTempChanges);
    if ( xllm_memory_watcher_bridge_flush(pPump->pBridge, &tTempChanges, pError) != XRT_NET_OK ) {
        xllm_memory_change_set_reset(&tTempChanges);
        return XRT_NET_ERROR;
    }
    xllm_memory_change_set_reset(&tTempChanges);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_watcher_pump_push(
    xllm_memory_watcher_pump *pPump,
    const xllm_memory_file_event *pEvent,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
)
{
    int iStatus;

    if ( !pPump ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher pump push requires pump");
        return XRT_NET_ERROR;
    }
    if ( pAutoFlushResult ) {
        xllm_memory_change_set_reset(pAutoFlushResult);
    }
    if ( pbFlushed ) {
        *pbFlushed = false;
    }

    iStatus = xllm_memory_watcher_bridge_push(pPump->pBridge, pEvent, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }
    return xllm__memory_watcher_pump_maybe_flush(pPump, pbFlushed, pAutoFlushResult, pError);
}

XLLM_API int xllm_memory_watcher_pump_push_created(
    xllm_memory_watcher_pump *pPump,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_CREATED;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_pump_push(pPump, &tEvent, pbFlushed, pAutoFlushResult, pError);
}

XLLM_API int xllm_memory_watcher_pump_push_updated(
    xllm_memory_watcher_pump *pPump,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_pump_push(pPump, &tEvent, pbFlushed, pAutoFlushResult, pError);
}

XLLM_API int xllm_memory_watcher_pump_push_deleted(
    xllm_memory_watcher_pump *pPump,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_DELETED;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_pump_push(pPump, &tEvent, pbFlushed, pAutoFlushResult, pError);
}

XLLM_API int xllm_memory_watcher_pump_push_renamed(
    xllm_memory_watcher_pump *pPump,
    const char *sPreviousPath,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_RENAMED;
    tEvent.sPreviousPath = sPreviousPath;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_pump_push(pPump, &tEvent, pbFlushed, pAutoFlushResult, pError);
}

XLLM_API int xllm_memory_watcher_pump_flush(
    xllm_memory_watcher_pump *pPump,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    if ( !pPump ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher pump flush requires pump");
        return XRT_NET_ERROR;
    }

    return xllm_memory_watcher_bridge_flush(pPump->pBridge, pResult, pError);
}

XLLM_API int xllm_memory_watcher_pump_flush_max(
    xllm_memory_watcher_pump *pPump,
    size_t iMaxItems,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    if ( !pPump ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher pump flush max requires pump");
        return XRT_NET_ERROR;
    }

    return xllm_memory_watcher_bridge_flush_max(pPump->pBridge, iMaxItems, pResult, pError);
}

static void xllm__memory_watcher_worker_after_flush(
    xllm_memory_watcher_worker *pWorker
)
{
    if ( !pWorker ) {
        return;
    }

    if ( xllm_memory_watcher_pump_pending_count(pWorker->pPump) > 0u ) {
        pWorker->uLastActivityAtMs = 0u;
    } else {
        pWorker->uLastActivityAtMs = 0u;
    }
}

static void xllm__memory_watcher_worker_after_push(
    xllm_memory_watcher_worker *pWorker
)
{
    if ( !pWorker ) {
        return;
    }

    if ( xllm_memory_watcher_pump_pending_count(pWorker->pPump) > 0u ) {
        pWorker->uLastActivityAtMs = xllm__memory_now_ms();
    } else {
        pWorker->uLastActivityAtMs = 0u;
    }
}

static void xllm__memory_watcher_worker_get_timing(
    const xllm_memory_watcher_worker *pWorker,
    uint32 *puElapsedMs,
    uint32 *puWaitMsRemaining
)
{
    uint64 uNowMs;
    uint64 uElapsedMs = 0u;
    uint64 uWaitMsRemaining = 0u;

    if ( puElapsedMs ) {
        *puElapsedMs = 0u;
    }
    if ( puWaitMsRemaining ) {
        *puWaitMsRemaining = 0u;
    }
    if ( !pWorker ||
         pWorker->uDebounceMs == 0u ||
         pWorker->uLastActivityAtMs == 0u ||
         xllm_memory_watcher_pump_pending_count(pWorker->pPump) == 0u ) {
        return;
    }

    uNowMs = xllm__memory_now_ms();
    if ( uNowMs > pWorker->uLastActivityAtMs ) {
        uElapsedMs = uNowMs - pWorker->uLastActivityAtMs;
    }
    if ( uElapsedMs < (uint64)pWorker->uDebounceMs ) {
        uWaitMsRemaining = (uint64)pWorker->uDebounceMs - uElapsedMs;
    }

    if ( puElapsedMs ) {
        *puElapsedMs = (uint32)((uElapsedMs > 0xffffffffu) ? 0xffffffffu : uElapsedMs);
    }
    if ( puWaitMsRemaining ) {
        *puWaitMsRemaining = (uint32)((uWaitMsRemaining > 0xffffffffu) ? 0xffffffffu : uWaitMsRemaining);
    }
}

XLLM_API int xllm_memory_watcher_worker_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_worker_options *pOptions,
    xllm_memory_watcher_worker **ppWorker,
    xllm_error *pError
)
{
    xllm_memory_watcher_worker_options tDefaultOptions;
    const xllm_memory_watcher_worker_options *pUseOptions = pOptions;
    xllm_memory_watcher_worker *pWorker;
    int iStatus;

    if ( !pMemory || !ppWorker ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker create requires memory handle and output");
        return XRT_NET_ERROR;
    }

    *ppWorker = NULL;
    if ( !pUseOptions ) {
        xllm_memory_watcher_worker_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    pWorker = (xllm_memory_watcher_worker *)xrtCalloc(1u, sizeof(*pWorker));
    if ( !pWorker ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to allocate memory watcher worker");
        return XRT_NET_ERROR;
    }

    iStatus = xllm_memory_watcher_pump_create(pMemory, &pUseOptions->tPumpOptions, &pWorker->pPump, pError);
    if ( iStatus != XRT_NET_OK ) {
        xrtFree(pWorker);
        return iStatus;
    }

    pWorker->uDebounceMs = pUseOptions->uDebounceMs;
    pWorker->iDefaultMaxItems = pUseOptions->iDefaultMaxItems;
    pWorker->uLastActivityAtMs = 0u;
    *ppWorker = pWorker;
    return XRT_NET_OK;
}

XLLM_API void xllm_memory_watcher_worker_destroy(
    xllm_memory_watcher_worker *pWorker
)
{
    if ( !pWorker ) {
        return;
    }

    xllm_memory_watcher_pump_destroy(pWorker->pPump);
    xrtFree(pWorker);
}

XLLM_API void xllm_memory_watcher_worker_clear(
    xllm_memory_watcher_worker *pWorker
)
{
    if ( !pWorker ) {
        return;
    }

    xllm_memory_watcher_pump_clear(pWorker->pPump);
    pWorker->uLastActivityAtMs = 0u;
}

XLLM_API size_t xllm_memory_watcher_worker_pending_count(
    const xllm_memory_watcher_worker *pWorker
)
{
    if ( !pWorker ) {
        return 0u;
    }

    return xllm_memory_watcher_pump_pending_count(pWorker->pPump);
}

XLLM_API int xllm_memory_watcher_worker_compact_pending(
    xllm_memory_watcher_worker *pWorker,
    xllm_error *pError
)
{
    if ( !pWorker ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker compact requires worker");
        return XRT_NET_ERROR;
    }

    return xllm_memory_watcher_pump_compact_pending(pWorker->pPump, pError);
}

XLLM_API int xllm_memory_watcher_worker_get_state(
    const xllm_memory_watcher_worker *pWorker,
    xllm_memory_watcher_worker_state *pState,
    xllm_error *pError
)
{
    uint32 uElapsedMs = 0u;
    uint32 uWaitMsRemaining = 0u;

    if ( !pWorker || !pState ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker state requires worker and output");
        return XRT_NET_ERROR;
    }

    xllm_memory_watcher_worker_state_init(pState);
    pState->iPendingCount = xllm_memory_watcher_pump_pending_count(pWorker->pPump);
    pState->uDebounceMs = pWorker->uDebounceMs;
    xllm__memory_watcher_worker_get_timing(pWorker, &uElapsedMs, &uWaitMsRemaining);
    pState->uElapsedSinceActivityMs = uElapsedMs;
    pState->uWaitMsRemaining = uWaitMsRemaining;
    pState->bBlockedByDebounce = (pState->iPendingCount > 0u && uWaitMsRemaining > 0u);
    pState->bReady = (pState->iPendingCount > 0u && !pState->bBlockedByDebounce);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_watcher_worker_push(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_file_event *pEvent,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
)
{
    int iStatus;

    if ( !pWorker ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker push requires worker");
        return XRT_NET_ERROR;
    }

    iStatus = xllm_memory_watcher_pump_push(pWorker->pPump, pEvent, pbFlushed, pPushResult, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    if ( pbFlushed && *pbFlushed ) {
        xllm__memory_watcher_worker_after_flush(pWorker);
    } else {
        xllm__memory_watcher_worker_after_push(pWorker);
    }
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_watcher_worker_push_created(
    xllm_memory_watcher_worker *pWorker,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_CREATED;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_worker_push(pWorker, &tEvent, pbFlushed, pPushResult, pError);
}

XLLM_API int xllm_memory_watcher_worker_push_updated(
    xllm_memory_watcher_worker *pWorker,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_worker_push(pWorker, &tEvent, pbFlushed, pPushResult, pError);
}

XLLM_API int xllm_memory_watcher_worker_push_deleted(
    xllm_memory_watcher_worker *pWorker,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_DELETED;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_worker_push(pWorker, &tEvent, pbFlushed, pPushResult, pError);
}

XLLM_API int xllm_memory_watcher_worker_push_renamed(
    xllm_memory_watcher_worker *pWorker,
    const char *sPreviousPath,
    const char *sPath,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
)
{
    xllm_memory_file_event tEvent;

    xllm_memory_sync_file_event_init(&tEvent);
    tEvent.eKind = XLLM_MEMORY_FILE_EVENT_RENAMED;
    tEvent.sPreviousPath = sPreviousPath;
    tEvent.sPath = sPath;
    return xllm_memory_watcher_worker_push(pWorker, &tEvent, pbFlushed, pPushResult, pError);
}

XLLM_API int xllm_memory_watcher_worker_poll_max(
    xllm_memory_watcher_worker *pWorker,
    size_t iMaxItems,
    bool *pbFlushed,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    uint32 uWaitMsRemaining;
    int iStatus;

    if ( pbFlushed ) {
        *pbFlushed = false;
    }
    if ( !pWorker || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker poll requires worker and output");
        return XRT_NET_ERROR;
    }

    xllm_memory_change_set_reset(pResult);
    if ( xllm_memory_watcher_pump_pending_count(pWorker->pPump) == 0u ) {
        pWorker->uLastActivityAtMs = 0u;
        return XRT_NET_OK;
    }

    xllm__memory_watcher_worker_get_timing(pWorker, NULL, &uWaitMsRemaining);
    if ( uWaitMsRemaining > 0u ) {
        return XRT_NET_OK;
    }

    if ( iMaxItems > 0u ) {
        iStatus = xllm_memory_watcher_pump_flush_max(pWorker->pPump, iMaxItems, pResult, pError);
    } else {
        iStatus = xllm_memory_watcher_pump_flush(pWorker->pPump, pResult, pError);
    }
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    if ( pbFlushed ) {
        *pbFlushed = (pResult->iChangeCount > 0u);
    }
    xllm__memory_watcher_worker_after_flush(pWorker);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_watcher_worker_poll(
    xllm_memory_watcher_worker *pWorker,
    bool *pbFlushed,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    size_t iMaxItems = 0u;

    if ( pWorker ) {
        iMaxItems = pWorker->iDefaultMaxItems;
    }
    return xllm_memory_watcher_worker_poll_max(pWorker, iMaxItems, pbFlushed, pResult, pError);
}

XLLM_API int xllm_memory_watcher_worker_flush(
    xllm_memory_watcher_worker *pWorker,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    return xllm_memory_watcher_worker_flush_max(pWorker, 0u, pResult, pError);
}

XLLM_API int xllm_memory_watcher_worker_flush_max(
    xllm_memory_watcher_worker *pWorker,
    size_t iMaxItems,
    xllm_memory_change_set *pResult,
    xllm_error *pError
)
{
    int iStatus;

    if ( !pWorker || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker flush requires worker and output");
        return XRT_NET_ERROR;
    }

    xllm_memory_change_set_reset(pResult);
    if ( iMaxItems > 0u ) {
        iStatus = xllm_memory_watcher_pump_flush_max(pWorker->pPump, iMaxItems, pResult, pError);
    } else {
        iStatus = xllm_memory_watcher_pump_flush(pWorker->pPump, pResult, pError);
    }
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    xllm__memory_watcher_worker_after_flush(pWorker);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_watcher_worker_run_ready(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_watcher_worker_run_options *pOptions,
    xllm_memory_watcher_worker_batch_fn fnOnBatch,
    void *pBatchCtx,
    xllm_memory_watcher_worker_run_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_watcher_worker_run_options tDefaultOptions;
    const xllm_memory_watcher_worker_run_options *pUseOptions = pOptions;
    xllm_memory_change_set tBatchChanges;
    bool bFlushed = false;
    int iStatus;

    if ( !pWorker || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker run ready requires worker and output");
        return XRT_NET_ERROR;
    }

    xllm_memory_watcher_worker_run_result_init(pResult);
    if ( !pUseOptions ) {
        xllm_memory_watcher_worker_run_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    xllm_memory_change_set_init(&tBatchChanges);
    while ( xllm_memory_watcher_worker_pending_count(pWorker) > 0u ) {
        if ( pUseOptions->iMaxBatches > 0u && pResult->iBatchCount >= pUseOptions->iMaxBatches ) {
            break;
        }

        iStatus = xllm_memory_watcher_worker_poll_max(
            pWorker,
            pUseOptions->iMaxItemsPerBatch,
            &bFlushed,
            &tBatchChanges,
            pError
        );
        if ( iStatus != XRT_NET_OK ) {
            xllm_memory_change_set_reset(&tBatchChanges);
            pResult->iPendingCount = xllm_memory_watcher_worker_pending_count(pWorker);
            return iStatus;
        }

        if ( !bFlushed ) {
            pResult->bBlockedByDebounce = true;
            break;
        }

        pResult->iBatchCount += 1u;
        pResult->iChangeCount += tBatchChanges.iChangeCount;

        if ( fnOnBatch ) {
            iStatus = fnOnBatch(pBatchCtx, &tBatchChanges, pError);
            if ( iStatus != XRT_NET_OK ) {
                xllm_memory_change_set_reset(&tBatchChanges);
                pResult->iPendingCount = xllm_memory_watcher_worker_pending_count(pWorker);
                return iStatus;
            }
        }

        xllm_memory_change_set_reset(&tBatchChanges);
    }

    xllm_memory_change_set_reset(&tBatchChanges);
    pResult->iPendingCount = xllm_memory_watcher_worker_pending_count(pWorker);
    return XRT_NET_OK;
}

XLLM_API int xllm_memory_watcher_worker_run_loop(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_watcher_worker_loop_options *pOptions,
    xllm_memory_watcher_worker_batch_fn fnOnBatch,
    void *pBatchCtx,
    xllm_memory_watcher_worker_loop_result *pResult,
    xllm_error *pError
)
{
    xllm_memory_watcher_worker_loop_options tDefaultOptions;
    const xllm_memory_watcher_worker_loop_options *pUseOptions = pOptions;
    xllm_memory_watcher_worker_run_result tStepResult;
    xllm_memory_change_set tForcedChanges;
    uint32 uWaitMsRemaining;
    uint32 uSleepMs;
    uint32 uStepSleepMs;
    int iStatus;

    if ( !pWorker || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory watcher worker run loop requires worker and output");
        return XRT_NET_ERROR;
    }

    xllm_memory_watcher_worker_loop_result_init(pResult);
    if ( !pUseOptions ) {
        xllm_memory_watcher_worker_loop_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    uSleepMs = pUseOptions->uSleepMs;

    xllm_memory_watcher_worker_run_result_init(&tStepResult);
    xllm_memory_change_set_init(&tForcedChanges);
    while ( xllm_memory_watcher_worker_pending_count(pWorker) > 0u ) {
        xllm_memory_watcher_worker_run_result_init(&tStepResult);
        iStatus = xllm_memory_watcher_worker_run_ready(
            pWorker,
            &pUseOptions->tRunOptions,
            fnOnBatch,
            pBatchCtx,
            &tStepResult,
            pError
        );
        if ( iStatus != XRT_NET_OK ) {
            xllm_memory_change_set_reset(&tForcedChanges);
            pResult->iPendingCount = xllm_memory_watcher_worker_pending_count(pWorker);
            return iStatus;
        }

        pResult->uLoopCount += 1u;
        pResult->iBatchCount += tStepResult.iBatchCount;
        pResult->iChangeCount += tStepResult.iChangeCount;
        pResult->iPendingCount = tStepResult.iPendingCount;

        if ( tStepResult.iPendingCount == 0u ) {
            break;
        }

        if ( tStepResult.bBlockedByDebounce ) {
            if ( pUseOptions->uMaxWaitMs > 0u && pResult->uWaitedMs >= pUseOptions->uMaxWaitMs ) {
                pResult->bBlockedByDebounce = true;
                pResult->bTimedOut = true;
                break;
            }
            if ( uSleepMs == 0u ) {
                xllm__memory_watcher_worker_get_timing(pWorker, NULL, &uWaitMsRemaining);
                if ( uWaitMsRemaining == 0u ) {
                    pResult->bBlockedByDebounce = true;
                    break;
                }
                uStepSleepMs = uWaitMsRemaining;
            } else {
                uStepSleepMs = uSleepMs;
            }
            if ( pUseOptions->uMaxWaitMs > 0u ) {
                if ( pResult->uWaitedMs >= pUseOptions->uMaxWaitMs ) {
                    pResult->bBlockedByDebounce = true;
                    pResult->bTimedOut = true;
                    break;
                }
                if ( uStepSleepMs > (pUseOptions->uMaxWaitMs - pResult->uWaitedMs) ) {
                    uStepSleepMs = pUseOptions->uMaxWaitMs - pResult->uWaitedMs;
                }
                if ( uStepSleepMs == 0u ) {
                    pResult->bBlockedByDebounce = true;
                    pResult->bTimedOut = true;
                    break;
                }
            }
            xrtSleep(uStepSleepMs);
            pResult->uWaitedMs += uStepSleepMs;
            continue;
        }

        if ( pUseOptions->tRunOptions.iMaxBatches > 0u && tStepResult.iPendingCount > 0u ) {
            pResult->bStoppedByBatchLimit = true;
            break;
        }
    }

    if ( xllm_memory_watcher_worker_pending_count(pWorker) > 0u &&
         pUseOptions->bForceFlushOnTimeout &&
         pResult->bBlockedByDebounce &&
         pUseOptions->uMaxWaitMs > 0u &&
         pResult->uWaitedMs >= pUseOptions->uMaxWaitMs ) {
        xllm_memory_change_set_reset(&tForcedChanges);
        iStatus = xllm_memory_watcher_worker_flush_max(
            pWorker,
            pUseOptions->tRunOptions.iMaxItemsPerBatch,
            &tForcedChanges,
            pError
        );
        if ( iStatus != XRT_NET_OK ) {
            xllm_memory_change_set_reset(&tForcedChanges);
            pResult->iPendingCount = xllm_memory_watcher_worker_pending_count(pWorker);
            return iStatus;
        }
        if ( tForcedChanges.iChangeCount > 0u ) {
            pResult->iBatchCount += 1u;
            pResult->iChangeCount += tForcedChanges.iChangeCount;
            if ( fnOnBatch ) {
                iStatus = fnOnBatch(pBatchCtx, &tForcedChanges, pError);
                if ( iStatus != XRT_NET_OK ) {
                    xllm_memory_change_set_reset(&tForcedChanges);
                    pResult->iPendingCount = xllm_memory_watcher_worker_pending_count(pWorker);
                    return iStatus;
                }
            }
        }
        xllm_memory_change_set_reset(&tForcedChanges);
    }

    xllm_memory_change_set_reset(&tForcedChanges);
    pResult->iPendingCount = xllm_memory_watcher_worker_pending_count(pWorker);
    return XRT_NET_OK;
}
