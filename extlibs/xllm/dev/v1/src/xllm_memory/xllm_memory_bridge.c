#include <string.h>

typedef struct {
    xllm_memory *pMemory;
    xllm *pLlm;
    xllm_turn tTurn;
    xllm_call_options tCallOptions;
    bool bHasCallOptions;
    xllm_memory_chat_bridge_options tBridgeOptions;
    bool bHasBridgeOptions;
} xllm__memory_bridge_send_async_task;

typedef struct {
    xllm_memory *pMemory;
    xllm_session *pSession;
    xllm_turn tTurn;
    xllm_call_options tCallOptions;
    bool bHasCallOptions;
    xllm_memory_chat_bridge_options tBridgeOptions;
    bool bHasBridgeOptions;
} xllm__memory_bridge_session_async_task;

static int xllm__memory_bridge_clone_cstr(const char **psOut, const char *sIn)
{
    char *sCopy;

    if ( !psOut ) {
        return XRT_NET_ERROR;
    }

    *psOut = NULL;
    if ( !sIn ) {
        return XRT_NET_OK;
    }

    sCopy = xllm__dup_cstr(sIn);
    if ( !sCopy ) {
        return XRT_NET_ERROR;
    }

    *psOut = sCopy;
    return XRT_NET_OK;
}

static void xllm__memory_search_options_reset(xllm_memory_search_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__free_cstr((char **)&pOptions->sQuery);
    xllm__free_cstr((char **)&pOptions->sConversationId);
    xllm__free_cstr((char **)&pOptions->sTurnId);
    xllm__free_cstr((char **)&pOptions->sRecordId);
    xllm__free_cstr((char **)&pOptions->sSourceUri);
    xllm__free_cstr((char **)&pOptions->sRecordIdContains);
    xllm__free_cstr((char **)&pOptions->sTitleContains);
    xllm__free_cstr((char **)&pOptions->sSourceUriContains);
    xllm__free_cstr((char **)&pOptions->sTextContains);
    xllm__free_cstr((char **)&pOptions->sMetadataKey);
    xllm__free_cstr((char **)&pOptions->sMetadataValue);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__memory_search_options_clone(
    xllm_memory_search_options *pOut,
    const xllm_memory_search_options *pIn
)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eScope = pIn->eScope;
    pOut->uMaxHits = pIn->uMaxHits;
    pOut->tMinScore = pIn->tMinScore;
    pOut->tPriorityWeight = pIn->tPriorityWeight;
    pOut->tRecencyWeight = pIn->tRecencyWeight;
    pOut->bSkipExpired = pIn->bSkipExpired;
    pOut->iNowUnix = pIn->iNowUnix;
    pOut->uMaxCharsPerHit = pIn->uMaxCharsPerHit;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__memory_bridge_clone_cstr(&pOut->sQuery, pIn->sQuery) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sConversationId, pIn->sConversationId) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sTurnId, pIn->sTurnId) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sRecordId, pIn->sRecordId) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sSourceUri, pIn->sSourceUri) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sRecordIdContains, pIn->sRecordIdContains) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sTitleContains, pIn->sTitleContains) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sSourceUriContains, pIn->sSourceUriContains) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sTextContains, pIn->sTextContains) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sMetadataKey, pIn->sMetadataKey) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sMetadataValue, pIn->sMetadataValue) != XRT_NET_OK ) {
        xllm__memory_search_options_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__memory_context_options_reset(xllm_memory_context_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__free_cstr((char **)&pOptions->sLabel);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__memory_context_options_clone(
    xllm_memory_context_options *pOut,
    const xllm_memory_context_options *pIn
)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eKindOverride = pIn->eKindOverride;
    pOut->iPriority = pIn->iPriority;
    pOut->bPinned = pIn->bPinned;
    pOut->bDistinctByRecord = pIn->bDistinctByRecord;
    pOut->tMinScore = pIn->tMinScore;
    pOut->uMaxHits = pIn->uMaxHits;
    pOut->uMaxCharsPerHit = pIn->uMaxCharsPerHit;
    pOut->uMaxTotalChars = pIn->uMaxTotalChars;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__memory_bridge_clone_cstr(&pOut->sLabel, pIn->sLabel) != XRT_NET_OK ) {
        xllm__memory_context_options_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__memory_turn_search_apply_options_reset(xllm_memory_turn_search_apply_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__memory_search_options_reset(&pOptions->tSearchOptions);
    xllm__memory_context_options_reset(&pOptions->tContextOptions);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__memory_turn_search_apply_options_clone(
    xllm_memory_turn_search_apply_options *pOut,
    const xllm_memory_turn_search_apply_options *pIn
)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eQueryMode = pIn->eQueryMode;
    pOut->bIncludeSystemPrompt = pIn->bIncludeSystemPrompt;
    pOut->bIncludeContextBlocks = pIn->bIncludeContextBlocks;
    pOut->uMaxQueryChars = pIn->uMaxQueryChars;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__memory_search_options_clone(&pOut->tSearchOptions, &pIn->tSearchOptions) != XRT_NET_OK ||
         xllm__memory_context_options_clone(&pOut->tContextOptions, &pIn->tContextOptions) != XRT_NET_OK ) {
        xllm__memory_turn_search_apply_options_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__memory_ingest_turn_response_options_reset(xllm_memory_ingest_turn_response_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__free_cstr((char **)&pOptions->sConversationId);
    xllm__free_cstr((char **)&pOptions->sTurnId);
    xllm__free_cstr((char **)&pOptions->sRecordId);
    xllm__free_cstr((char **)&pOptions->sTitle);
    xllm__free_cstr((char **)&pOptions->sSourceUri);
    xllm__xvalue_release(&pOptions->tMetadata);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__memory_ingest_turn_response_options_clone(
    xllm_memory_ingest_turn_response_options *pOut,
    const xllm_memory_ingest_turn_response_options *pIn
)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eScope = pIn->eScope;
    pOut->eExtractionPolicy = pIn->eExtractionPolicy;
    pOut->bReplaceExisting = pIn->bReplaceExisting;
    pOut->bUseStableIdentity = pIn->bUseStableIdentity;
    pOut->iPriority = pIn->iPriority;
    pOut->iExpiresAtUnix = pIn->iExpiresAtUnix;
    pOut->iUpdatedAtUnix = pIn->iUpdatedAtUnix;
    pOut->bIncludeSystemPrompt = pIn->bIncludeSystemPrompt;
    pOut->bIncludeContextBlocks = pIn->bIncludeContextBlocks;
    pOut->bIncludeThinking = pIn->bIncludeThinking;
    pOut->uChunkChars = pIn->uChunkChars;
    pOut->uChunkOverlapChars = pIn->uChunkOverlapChars;
    pOut->tMetadata = pIn->tMetadata;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tMetadata);
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__memory_bridge_clone_cstr(&pOut->sConversationId, pIn->sConversationId) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sTurnId, pIn->sTurnId) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sRecordId, pIn->sRecordId) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sTitle, pIn->sTitle) != XRT_NET_OK ||
         xllm__memory_bridge_clone_cstr(&pOut->sSourceUri, pIn->sSourceUri) != XRT_NET_OK ) {
        xllm__memory_ingest_turn_response_options_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__memory_chat_bridge_options_reset(xllm_memory_chat_bridge_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__memory_turn_search_apply_options_reset(&pOptions->tSearch);
    xllm__memory_ingest_turn_response_options_reset(&pOptions->tIngest);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__memory_chat_bridge_options_clone(
    xllm_memory_chat_bridge_options *pOut,
    const xllm_memory_chat_bridge_options *pIn
)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->bSearchBeforeChat = pIn->bSearchBeforeChat;
    pOut->bIngestAfterChat = pIn->bIngestAfterChat;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__memory_turn_search_apply_options_clone(&pOut->tSearch, &pIn->tSearch) != XRT_NET_OK ||
         xllm__memory_ingest_turn_response_options_clone(&pOut->tIngest, &pIn->tIngest) != XRT_NET_OK ) {
        xllm__memory_chat_bridge_options_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__memory_bridge_prepare_turn(
    xllm_memory *pMemory,
    const xllm_turn *pSourceTurn,
    const xllm_memory_chat_bridge_options *pOptions,
    xllm_turn *pAugmentedTurn,
    xllm_error *pError
)
{
    xllm_memory_chat_bridge_options tDefaultOptions;
    const xllm_memory_chat_bridge_options *pUseOptions = pOptions;
    int iStatus;

    if ( !pSourceTurn || !pAugmentedTurn ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory bridge requires source turn and output turn");
        return XRT_NET_ERROR;
    }

    if ( !pUseOptions ) {
        xllm_memory_chat_bridge_options_init(&tDefaultOptions);
        pUseOptions = &tDefaultOptions;
    }

    iStatus = xllm_turn_clone(pAugmentedTurn, pSourceTurn);
    if ( iStatus != XRT_NET_OK ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to clone turn for memory bridge");
        return iStatus;
    }

    if ( pUseOptions->bSearchBeforeChat ) {
        if ( !pMemory ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory bridge search requires memory handle");
            xllm_turn_reset(pAugmentedTurn);
            return XRT_NET_ERROR;
        }
        iStatus = xllm_memory_search_and_apply_from_turn_to_turn(
            pMemory,
            pSourceTurn,
            pAugmentedTurn,
            &pUseOptions->tSearch,
            pError
        );
        if ( iStatus != XRT_NET_OK ) {
            xllm_turn_reset(pAugmentedTurn);
            return iStatus;
        }
    }

    return XRT_NET_OK;
}

static int xllm__memory_bridge_post_ingest(
    xllm_memory *pMemory,
    const xllm_turn *pAugmentedTurn,
    const xllm_response *pResponse,
    const xllm_memory_chat_bridge_options *pOptions,
    xllm_error *pError
)
{
    xllm_memory_ingest_turn_response_options tIngestOptions;

    if ( !pOptions || !pOptions->bIngestAfterChat ) {
        return XRT_NET_OK;
    }
    if ( !pMemory ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory bridge ingest requires memory handle");
        return XRT_NET_ERROR;
    }
    if ( !pAugmentedTurn || !pResponse ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory bridge ingest requires turn and response");
        return XRT_NET_ERROR;
    }

    tIngestOptions = pOptions->tIngest;
    tIngestOptions.pTurn = pAugmentedTurn;
    tIngestOptions.pResponse = pResponse;
    return xllm_memory_ingest_turn_response(pMemory, &tIngestOptions, pError);
}

static xllm__memory_bridge_send_async_task *xllm__memory_bridge_send_async_task_create(
    xllm_memory *pMemory,
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
)
{
    xllm__memory_bridge_send_async_task *pTask;

    if ( !pLlm || !pTurn ) {
        return NULL;
    }

    pTask = (xllm__memory_bridge_send_async_task *)xrtCalloc(1u, sizeof(*pTask));
    if ( !pTask ) {
        return NULL;
    }

    pTask->pMemory = pMemory;
    pTask->pLlm = pLlm;
    if ( xllm_turn_clone(&pTask->tTurn, pTurn) != XRT_NET_OK ) {
        xrtFree(pTask);
        return NULL;
    }

    if ( pCallOptions ) {
        if ( xllm__call_options_clone(&pTask->tCallOptions, pCallOptions) != XRT_NET_OK ) {
            xllm_turn_reset(&pTask->tTurn);
            xrtFree(pTask);
            return NULL;
        }
        pTask->bHasCallOptions = true;
    }

    if ( pBridgeOptions ) {
        if ( xllm__memory_chat_bridge_options_clone(&pTask->tBridgeOptions, pBridgeOptions) != XRT_NET_OK ) {
            xllm_turn_reset(&pTask->tTurn);
            if ( pTask->bHasCallOptions ) {
                xllm__call_options_reset(&pTask->tCallOptions);
            }
            xrtFree(pTask);
            return NULL;
        }
        pTask->bHasBridgeOptions = true;
    }

    return pTask;
}

static void xllm__memory_bridge_send_async_task_destroy(xllm__memory_bridge_send_async_task *pTask)
{
    if ( !pTask ) {
        return;
    }

    xllm_turn_reset(&pTask->tTurn);
    if ( pTask->bHasCallOptions ) {
        xllm__call_options_reset(&pTask->tCallOptions);
    }
    if ( pTask->bHasBridgeOptions ) {
        xllm__memory_chat_bridge_options_reset(&pTask->tBridgeOptions);
    }
    xrtFree(pTask);
}

static int32 xllm__memory_bridge_send_async_task_run(
    xllm__memory_bridge_send_async_task *pTask,
    xfuture_result *pOut
)
{
    xllm_response *pResponse = NULL;
    xllm_error tError;
    int32 iStatus;

    xllm_error_init(&tError);
    if ( !pTask ) {
        xllm_error_free(&tError);
        return xllm__async_future_result_error(pOut, XRT_NET_ERROR, "xllm memory bridge async send task is null");
    }

    iStatus = xllm_memory_bridge_send_ex(
        pTask->pMemory,
        pTask->pLlm,
        &pTask->tTurn,
        pTask->bHasCallOptions ? &pTask->tCallOptions : NULL,
        pTask->bHasBridgeOptions ? &pTask->tBridgeOptions : NULL,
        &pResponse,
        &tError
    );
    if ( iStatus == XRT_NET_OK ) {
        memset(pOut, 0, sizeof(*pOut));
        pOut->iStatus = XRT_NET_OK;
        pOut->pValue = pResponse;
    } else {
        if ( pResponse ) {
            xllm_response_free(pResponse);
        }
        (void)xllm__async_future_result_error(
            pOut,
            iStatus,
            tError.sMessage ? tError.sMessage : "xllm memory bridge async send failed"
        );
    }

    xllm_error_free(&tError);
    xllm__memory_bridge_send_async_task_destroy(pTask);
    return pOut ? pOut->iStatus : iStatus;
}

static int32 xllm__memory_bridge_send_async_task_thread_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__memory_bridge_send_async_task_run((xllm__memory_bridge_send_async_task *)pArg, pOut);
}

static int32 xllm__memory_bridge_send_async_task_engine_fn(xnetworker *pWorker, ptr pArg, xfuture_result *pOut)
{
    (void)pWorker;
    return xllm__memory_bridge_send_async_task_run((xllm__memory_bridge_send_async_task *)pArg, pOut);
}

static int32 xllm__memory_bridge_send_async_task_co_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__memory_bridge_send_async_task_run((xllm__memory_bridge_send_async_task *)pArg, pOut);
}

static xllm__memory_bridge_session_async_task *xllm__memory_bridge_session_async_task_create(
    xllm_memory *pMemory,
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
)
{
    xllm__memory_bridge_session_async_task *pTask;

    if ( !pSession || !pTurn ) {
        return NULL;
    }

    pTask = (xllm__memory_bridge_session_async_task *)xrtCalloc(1u, sizeof(*pTask));
    if ( !pTask ) {
        return NULL;
    }

    pTask->pMemory = pMemory;
    pTask->pSession = pSession;
    if ( xllm_turn_clone(&pTask->tTurn, pTurn) != XRT_NET_OK ) {
        xrtFree(pTask);
        return NULL;
    }

    if ( pCallOptions ) {
        if ( xllm__call_options_clone(&pTask->tCallOptions, pCallOptions) != XRT_NET_OK ) {
            xllm_turn_reset(&pTask->tTurn);
            xrtFree(pTask);
            return NULL;
        }
        pTask->bHasCallOptions = true;
    }

    if ( pBridgeOptions ) {
        if ( xllm__memory_chat_bridge_options_clone(&pTask->tBridgeOptions, pBridgeOptions) != XRT_NET_OK ) {
            xllm_turn_reset(&pTask->tTurn);
            if ( pTask->bHasCallOptions ) {
                xllm__call_options_reset(&pTask->tCallOptions);
            }
            xrtFree(pTask);
            return NULL;
        }
        pTask->bHasBridgeOptions = true;
    }

    return pTask;
}

static void xllm__memory_bridge_session_async_task_destroy(xllm__memory_bridge_session_async_task *pTask)
{
    if ( !pTask ) {
        return;
    }

    xllm_turn_reset(&pTask->tTurn);
    if ( pTask->bHasCallOptions ) {
        xllm__call_options_reset(&pTask->tCallOptions);
    }
    if ( pTask->bHasBridgeOptions ) {
        xllm__memory_chat_bridge_options_reset(&pTask->tBridgeOptions);
    }
    xrtFree(pTask);
}

static int32 xllm__memory_bridge_session_async_task_run(
    xllm__memory_bridge_session_async_task *pTask,
    xfuture_result *pOut
)
{
    xllm_response *pResponse = NULL;
    xllm_error tError;
    int32 iStatus;

    xllm_error_init(&tError);
    if ( !pTask ) {
        xllm_error_free(&tError);
        return xllm__async_future_result_error(pOut, XRT_NET_ERROR, "xllm memory bridge async session task is null");
    }

    iStatus = xllm_memory_bridge_session_chat_ex(
        pTask->pMemory,
        pTask->pSession,
        &pTask->tTurn,
        pTask->bHasCallOptions ? &pTask->tCallOptions : NULL,
        pTask->bHasBridgeOptions ? &pTask->tBridgeOptions : NULL,
        &pResponse,
        &tError
    );
    if ( iStatus == XRT_NET_OK ) {
        memset(pOut, 0, sizeof(*pOut));
        pOut->iStatus = XRT_NET_OK;
        pOut->pValue = pResponse;
    } else {
        if ( pResponse ) {
            xllm_response_free(pResponse);
        }
        (void)xllm__async_future_result_error(
            pOut,
            iStatus,
            tError.sMessage ? tError.sMessage : "xllm memory bridge async session chat failed"
        );
    }

    xllm_error_free(&tError);
    xllm__memory_bridge_session_async_task_destroy(pTask);
    return pOut ? pOut->iStatus : iStatus;
}

static int32 xllm__memory_bridge_session_async_task_thread_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__memory_bridge_session_async_task_run((xllm__memory_bridge_session_async_task *)pArg, pOut);
}

static int32 xllm__memory_bridge_session_async_task_engine_fn(xnetworker *pWorker, ptr pArg, xfuture_result *pOut)
{
    (void)pWorker;
    return xllm__memory_bridge_session_async_task_run((xllm__memory_bridge_session_async_task *)pArg, pOut);
}

static int32 xllm__memory_bridge_session_async_task_co_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__memory_bridge_session_async_task_run((xllm__memory_bridge_session_async_task *)pArg, pOut);
}

XLLM_API void xllm_memory_chat_bridge_options_init(xllm_memory_chat_bridge_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->bSearchBeforeChat = true;
    pOptions->bIngestAfterChat = false;
    xllm_memory_turn_search_apply_options_init(&pOptions->tSearch);
    xllm_memory_ingest_turn_response_options_init(&pOptions->tIngest);
}

XLLM_API int xllm_memory_bridge_send_ex(
    xllm_memory *pMemory,
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_turn tAugmentedTurn;
    int iStatus;

    if ( !pLlm || !pTurn || !ppResponse ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory bridge send requires llm, turn, and response output");
        return XRT_NET_ERROR;
    }

    *ppResponse = NULL;
    xllm_turn_init(&tAugmentedTurn);
    iStatus = xllm__memory_bridge_prepare_turn(pMemory, pTurn, pBridgeOptions, &tAugmentedTurn, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    iStatus = xllm_send_ex(pLlm, &tAugmentedTurn, pCallOptions, ppResponse, pError);
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm__memory_bridge_post_ingest(
            pMemory,
            &tAugmentedTurn,
            *ppResponse,
            pBridgeOptions,
            pError
        );
        if ( iStatus != XRT_NET_OK && *ppResponse ) {
            xllm_response_free(*ppResponse);
            *ppResponse = NULL;
        }
    }

    xllm_turn_reset(&tAugmentedTurn);
    return iStatus;
}

XLLM_API int xllm_memory_bridge_send(
    xllm_memory *pMemory,
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse
)
{
    xllm_error tError;
    int iStatus;

    xllm_error_init(&tError);
    iStatus = xllm_memory_bridge_send_ex(
        pMemory,
        pLlm,
        pTurn,
        pCallOptions,
        pBridgeOptions,
        ppResponse,
        &tError
    );
    xllm_error_free(&tError);
    return iStatus;
}

XLLM_API xfuture *xllm_memory_bridge_send_async_thread(
    xllm_memory *pMemory,
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
)
{
    xllm__memory_bridge_send_async_task *pTask;
    xfuture *pFuture;

    if ( !pLlm || !pTurn || !pLlm->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge send arguments are invalid");
    }

    pTask = xllm__memory_bridge_send_async_task_create(pMemory, pLlm, pTurn, pCallOptions, pBridgeOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build memory bridge async send task");
    }

    pFuture = xTaskRunThread(xllm__memory_bridge_send_async_task_thread_fn, pTask, 0u);
    if ( !pFuture ) {
        xllm__memory_bridge_send_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge async send task create failed");
    }

    return pFuture;
}

XLLM_API xfuture *xllm_memory_bridge_send_async_engine(
    xllm_memory *pMemory,
    xllm *pLlm,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
)
{
    xllm__memory_bridge_send_async_task *pTask;
    xfuture *pFuture;

    if ( !pLlm || !pTurn || !pLlm->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge send arguments are invalid");
    }
    if ( !pEngine ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm engine is null");
    }

    pTask = xllm__memory_bridge_send_async_task_create(pMemory, pLlm, pTurn, pCallOptions, pBridgeOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build memory bridge async send task");
    }

    pFuture = xTaskRunEngine(pEngine, uAffinityKey, xllm__memory_bridge_send_async_task_engine_fn, pTask);
    if ( !pFuture ) {
        xllm__memory_bridge_send_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge async engine send task create failed");
    }

    return pFuture;
}

XLLM_API xfuture *xllm_memory_bridge_send_async_co(
    xllm_memory *pMemory,
    xllm *pLlm,
    xcosched *pSched,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    size_t iStackSize
)
{
    xllm__memory_bridge_send_async_task *pTask;
    xfuture *pFuture;

    if ( !pLlm || !pTurn || !pLlm->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge send arguments are invalid");
    }
    if ( !pSched ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm coroutine scheduler is null");
    }

    pTask = xllm__memory_bridge_send_async_task_create(pMemory, pLlm, pTurn, pCallOptions, pBridgeOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build memory bridge async send task");
    }

#if !defined(XRT_NO_COROUTINE)
    pFuture = xTaskRunCo(pSched, xllm__memory_bridge_send_async_task_co_fn, pTask, iStackSize);
    if ( !pFuture ) {
        xllm__memory_bridge_send_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge async coroutine send task create failed");
    }
    return pFuture;
#else
    xllm__memory_bridge_send_async_task_destroy(pTask);
    return xllm__make_error_future(XRT_NET_ERROR, "xrt coroutine support is disabled");
#endif
}

XLLM_API int xllm_memory_bridge_session_chat_ex(
    xllm_memory *pMemory,
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_turn tAugmentedTurn;
    int iStatus;

    if ( !pSession || !pTurn || !ppResponse ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "memory bridge session chat requires session, turn, and response output");
        return XRT_NET_ERROR;
    }

    *ppResponse = NULL;
    xllm_turn_init(&tAugmentedTurn);
    iStatus = xllm__memory_bridge_prepare_turn(pMemory, pTurn, pBridgeOptions, &tAugmentedTurn, pError);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    iStatus = xllm_session_chat_ex(pSession, &tAugmentedTurn, pCallOptions, ppResponse, pError);
    if ( iStatus == XRT_NET_OK ) {
        iStatus = xllm__memory_bridge_post_ingest(
            pMemory,
            &tAugmentedTurn,
            *ppResponse,
            pBridgeOptions,
            pError
        );
        if ( iStatus != XRT_NET_OK && *ppResponse ) {
            xllm_response_free(*ppResponse);
            *ppResponse = NULL;
        }
    }

    xllm_turn_reset(&tAugmentedTurn);
    return iStatus;
}

XLLM_API int xllm_memory_bridge_session_chat(
    xllm_memory *pMemory,
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse
)
{
    xllm_error tError;
    int iStatus;

    xllm_error_init(&tError);
    iStatus = xllm_memory_bridge_session_chat_ex(
        pMemory,
        pSession,
        pTurn,
        pCallOptions,
        pBridgeOptions,
        ppResponse,
        &tError
    );
    xllm_error_free(&tError);
    return iStatus;
}

XLLM_API xfuture *xllm_memory_bridge_session_chat_async_thread(
    xllm_memory *pMemory,
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
)
{
    xllm__memory_bridge_session_async_task *pTask;
    xfuture *pFuture;

    if ( !pSession || !pTurn || !pSession->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge session arguments are invalid");
    }

    pTask = xllm__memory_bridge_session_async_task_create(pMemory, pSession, pTurn, pCallOptions, pBridgeOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build memory bridge async session task");
    }

    pFuture = xTaskRunThread(xllm__memory_bridge_session_async_task_thread_fn, pTask, 0u);
    if ( !pFuture ) {
        xllm__memory_bridge_session_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge async session task create failed");
    }

    return pFuture;
}

XLLM_API xfuture *xllm_memory_bridge_session_chat_async_engine(
    xllm_memory *pMemory,
    xllm_session *pSession,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
)
{
    xllm__memory_bridge_session_async_task *pTask;
    xfuture *pFuture;

    if ( !pSession || !pTurn || !pSession->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge session arguments are invalid");
    }
    if ( !pEngine ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm engine is null");
    }

    pTask = xllm__memory_bridge_session_async_task_create(pMemory, pSession, pTurn, pCallOptions, pBridgeOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build memory bridge async session task");
    }

    pFuture = xTaskRunEngine(pEngine, uAffinityKey, xllm__memory_bridge_session_async_task_engine_fn, pTask);
    if ( !pFuture ) {
        xllm__memory_bridge_session_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge async engine session task create failed");
    }

    return pFuture;
}

XLLM_API xfuture *xllm_memory_bridge_session_chat_async_co(
    xllm_memory *pMemory,
    xllm_session *pSession,
    xcosched *pSched,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    size_t iStackSize
)
{
    xllm__memory_bridge_session_async_task *pTask;
    xfuture *pFuture;

    if ( !pSession || !pTurn || !pSession->sProfileId ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge session arguments are invalid");
    }
    if ( !pSched ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm coroutine scheduler is null");
    }

    pTask = xllm__memory_bridge_session_async_task_create(pMemory, pSession, pTurn, pCallOptions, pBridgeOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm failed to build memory bridge async session task");
    }

#if !defined(XRT_NO_COROUTINE)
    pFuture = xTaskRunCo(pSched, xllm__memory_bridge_session_async_task_co_fn, pTask, iStackSize);
    if ( !pFuture ) {
        xllm__memory_bridge_session_async_task_destroy(pTask);
        return xllm__make_error_future(XRT_NET_ERROR, "xllm memory bridge async coroutine session task create failed");
    }
    return pFuture;
#else
    xllm__memory_bridge_session_async_task_destroy(pTask);
    return xllm__make_error_future(XRT_NET_ERROR, "xrt coroutine support is disabled");
#endif
}
