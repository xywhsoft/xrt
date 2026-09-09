#include "xllm_base.h"

#ifndef XLLM_ARRAY_COUNT
#define XLLM_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

struct xllm_runtime {
    xllm_runtime_options tOptions;
    xnetengine *pNetEngine;
    xllm_adapter *pAdapters;
    size_t iAdapterCount;
    size_t iAdapterCapacity;
    xllm_profile *pProfiles;
    size_t iProfileCount;
    size_t iProfileCapacity;
};

struct xllm {
    xllm_runtime *pRuntime;
    char *sProfileId;
    char *sSystemPrompt;
    xllm_call_options tDefaultCallOptions;
    bool bHasToolExecutor;
    xllm_tool_executor tToolExecutor;
    bool bHasToolExecutorAsync;
    xllm_tool_executor_async tToolExecutorAsync;
};

struct xllm_session {
    xllm_runtime *pRuntime;
    xmutex pMutex;
    char *sProfileId;
    char *sSystemPrompt;
    char *sSessionSummary;
    xllm_message *pHistory;
    size_t iHistoryCount;
    size_t iHistoryCapacity;
    uint32 uCommittedTurns;
    uint32 uSummaryTurns;
    xllm_session_options tOptions;
    bool bHasToolExecutor;
    xllm_tool_executor tToolExecutor;
    bool bHasToolExecutorAsync;
    xllm_tool_executor_async tToolExecutorAsync;
};

struct xllm_cancel_token {
    xmutex pMutex;
    bool bCancelled;
    char *sReason;
};

struct xllm_session_state {
    char *sProfileId;
    char *sSystemPrompt;
    char *sSessionSummary;
    char *sSummarizerProfileId;
    xllm_message *pHistory;
    size_t iHistoryCount;
    uint32 uCommittedTurns;
    uint32 uSummaryTurns;
    bool bEnableAutoCompact;
    double fCompactTriggerRatio;
    uint32 uCompactTriggerTurns;
    uint32 uReserveOutputTokens;
    uint32 uKeepRecentTurns;
    bool bKeepActiveToolChain;
    xllm_compact_strategy eCompactStrategy;
    xvalue tVendorExtra;
};

typedef struct {
    xllm_runtime *pRuntime;
    xllm_request tRequest;
    xllm_call_options tOptions;
    bool bHasOptions;
} xllm_async_chat_task;

static char *xllm__dup_cstr(const char *sText)
{
    size_t iSize;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    iSize = strlen(sText);
    sCopy = (char *)xrtCalloc(iSize + 1, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }

    memcpy(sCopy, sText, iSize);
    sCopy[iSize] = '\0';
    return sCopy;
}

static void xllm__free_cstr(char **psText)
{
    if ( psText && *psText ) {
        xrtFree(*psText);
        *psText = NULL;
    }
}

static void xllm__xvalue_addref(xvalue tValue)
{
    if ( tValue ) {
        xvoAddRef(tValue);
    }
}

static void xllm__xvalue_release(xvalue *ptValue)
{
    if ( ptValue && *ptValue ) {
        xvoUnref(*ptValue);
        *ptValue = NULL;
    }
}

static void xllm__error_set(xllm_error *pError, xllm_error_code eCode, const char *sMessage)
{
    if ( !pError ) {
        return;
    }

    xllm_error_reset(pError);
    pError->eCode = eCode;
    pError->iStatus = (int32)eCode;
    pError->sMessage = xllm__dup_cstr(sMessage ? sMessage : "xllm error");
}

static int xllm__append_buffer(void **ppBuffer, size_t iItemSize, size_t *piCount, size_t *piCapacity, const void *pItem)
{
    void *pNewBuffer;
    size_t iNewCapacity;
    size_t iOffset;

    if ( !ppBuffer || !piCount || !piCapacity || !pItem || iItemSize == 0 ) {
        return XRT_NET_ERROR;
    }

    if ( *piCount >= *piCapacity ) {
        iNewCapacity = (*piCapacity == 0) ? 4 : ((*piCapacity < 8) ? (*piCapacity * 2) : (*piCapacity + (*piCapacity / 2)));
        pNewBuffer = xrtRealloc(*ppBuffer, iNewCapacity * iItemSize);
        if ( !pNewBuffer ) {
            return XRT_NET_ERROR;
        }
        *ppBuffer = pNewBuffer;
        *piCapacity = iNewCapacity;
    }

    iOffset = (*piCount) * iItemSize;
    memcpy(((uint8 *)*ppBuffer) + iOffset, pItem, iItemSize);
    ++(*piCount);
    return XRT_NET_OK;
}

static void xllm__content_part_free(xllm_content_part *pPart)
{
    if ( !pPart ) {
        return;
    }

    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
        case XLLM_PART_IMAGE:
        case XLLM_PART_FILE:
        case XLLM_PART_AUDIO:
        case XLLM_PART_VIDEO:
            xllm__free_cstr((char **)&pPart->as.tSource.sMimeType);
            xllm__free_cstr((char **)&pPart->as.tSource.sName);
            switch ( pPart->as.tSource.eKind ) {
                case XLLM_SOURCE_INLINE_TEXT:
                    xllm__free_cstr((char **)&pPart->as.tSource.as.sText);
                    break;
                case XLLM_SOURCE_INLINE_BYTES:
                    if ( pPart->as.tSource.as.tBytes.pData ) {
                        xrtFree((void *)pPart->as.tSource.as.tBytes.pData);
                    }
                    pPart->as.tSource.as.tBytes.pData = NULL;
                    pPart->as.tSource.as.tBytes.iSize = 0;
                    break;
                case XLLM_SOURCE_URL:
                    xllm__free_cstr((char **)&pPart->as.tSource.as.sUrl);
                    break;
                case XLLM_SOURCE_PROVIDER_FILE_ID:
                    xllm__free_cstr((char **)&pPart->as.tSource.as.sFileId);
                    break;
                default:
                    break;
            }
            break;
        case XLLM_PART_JSON:
            xllm__xvalue_release(&pPart->as.tJsonValue);
            break;
        default:
            break;
    }

    xllm__xvalue_release(&pPart->tVendorExtra);
    memset(pPart, 0, sizeof(*pPart));
}

static int xllm__content_part_clone(xllm_content_part *pOut, const xllm_content_part *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eKind = pIn->eKind;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( pIn->eKind == XLLM_PART_JSON ) {
        pOut->as.tJsonValue = pIn->as.tJsonValue;
        xllm__xvalue_addref(pOut->as.tJsonValue);
        return XRT_NET_OK;
    }

    pOut->as.tSource.eKind = pIn->as.tSource.eKind;
    pOut->as.tSource.sMimeType = xllm__dup_cstr(pIn->as.tSource.sMimeType);
    pOut->as.tSource.sName = xllm__dup_cstr(pIn->as.tSource.sName);

    switch ( pIn->as.tSource.eKind ) {
        case XLLM_SOURCE_INLINE_TEXT:
            pOut->as.tSource.as.sText = xllm__dup_cstr(pIn->as.tSource.as.sText);
            break;
        case XLLM_SOURCE_INLINE_BYTES:
            if ( pIn->as.tSource.as.tBytes.pData && pIn->as.tSource.as.tBytes.iSize > 0 ) {
                void *pData = xrtCalloc(1, pIn->as.tSource.as.tBytes.iSize);
                if ( !pData ) {
                    xllm__content_part_free(pOut);
                    return XRT_NET_ERROR;
                }
                memcpy(pData, pIn->as.tSource.as.tBytes.pData, pIn->as.tSource.as.tBytes.iSize);
                pOut->as.tSource.as.tBytes.pData = pData;
                pOut->as.tSource.as.tBytes.iSize = pIn->as.tSource.as.tBytes.iSize;
            }
            break;
        case XLLM_SOURCE_URL:
            pOut->as.tSource.as.sUrl = xllm__dup_cstr(pIn->as.tSource.as.sUrl);
            break;
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            pOut->as.tSource.as.sFileId = xllm__dup_cstr(pIn->as.tSource.as.sFileId);
            break;
        default:
            break;
    }

    return XRT_NET_OK;
}

static void xllm__tool_call_free(xllm_tool_call *pCall)
{
    if ( !pCall ) {
        return;
    }

    xllm__free_cstr((char **)&pCall->sCallId);
    xllm__free_cstr((char **)&pCall->sToolId);
    xllm__free_cstr((char **)&pCall->sToolName);
    xllm__free_cstr((char **)&pCall->sArgumentsJson);
    xllm__xvalue_release(&pCall->tContinuation);
    xllm__xvalue_release(&pCall->tVendorExtra);
    memset(pCall, 0, sizeof(*pCall));
}

static int xllm__tool_call_clone(xllm_tool_call *pOut, const xllm_tool_call *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->sCallId = xllm__dup_cstr(pIn->sCallId);
    pOut->sToolId = xllm__dup_cstr(pIn->sToolId);
    pOut->sToolName = xllm__dup_cstr(pIn->sToolName);
    pOut->sArgumentsJson = xllm__dup_cstr(pIn->sArgumentsJson);
    pOut->tContinuation = pIn->tContinuation;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tContinuation);
    xllm__xvalue_addref(pOut->tVendorExtra);
    return XRT_NET_OK;
}

static void xllm__message_free(xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage ) {
        return;
    }

    xllm__free_cstr((char **)&pMessage->sToolCallId);
    xllm__free_cstr((char **)&pMessage->sToolName);

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        xllm__content_part_free(&pMessage->pParts[i]);
    }
    if ( pMessage->pParts ) {
        xrtFree(pMessage->pParts);
    }

    for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
        xllm__tool_call_free(&pMessage->pToolCalls[i]);
    }
    if ( pMessage->pToolCalls ) {
        xrtFree(pMessage->pToolCalls);
    }

    xllm__xvalue_release(&pMessage->tVendorExtra);
    memset(pMessage, 0, sizeof(*pMessage));
}

static int xllm__message_clone(xllm_message *pOut, const xllm_message *pIn)
{
    size_t i;

    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eRole = pIn->eRole;
    pOut->sToolCallId = xllm__dup_cstr(pIn->sToolCallId);
    pOut->sToolName = xllm__dup_cstr(pIn->sToolName);
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( pIn->iPartCount > 0 ) {
        pOut->pParts = (xllm_content_part *)xrtCalloc(pIn->iPartCount, sizeof(xllm_content_part));
        if ( !pOut->pParts ) {
            xllm__message_free(pOut);
            return XRT_NET_ERROR;
        }
        pOut->iPartCount = pIn->iPartCount;
        for ( i = 0; i < pIn->iPartCount; ++i ) {
            if ( xllm__content_part_clone(&pOut->pParts[i], &pIn->pParts[i]) != XRT_NET_OK ) {
                xllm__message_free(pOut);
                return XRT_NET_ERROR;
            }
        }
    }

    if ( pIn->iToolCallCount > 0 ) {
        pOut->pToolCalls = (xllm_tool_call *)xrtCalloc(pIn->iToolCallCount, sizeof(xllm_tool_call));
        if ( !pOut->pToolCalls ) {
            xllm__message_free(pOut);
            return XRT_NET_ERROR;
        }
        pOut->iToolCallCount = pIn->iToolCallCount;
        for ( i = 0; i < pIn->iToolCallCount; ++i ) {
            if ( xllm__tool_call_clone(&pOut->pToolCalls[i], &pIn->pToolCalls[i]) != XRT_NET_OK ) {
                xllm__message_free(pOut);
                return XRT_NET_ERROR;
            }
        }
    }

    return XRT_NET_OK;
}

static void xllm__message_array_free(xllm_message *pMessages, size_t iMessageCount)
{
    size_t i;

    if ( !pMessages ) {
        return;
    }

    for ( i = 0; i < iMessageCount; ++i ) {
        xllm__message_free(&pMessages[i]);
    }
    xrtFree(pMessages);
}

static int xllm__message_array_clone(xllm_message **ppOut, size_t *piOutCount, const xllm_message *pIn, size_t iInCount)
{
    xllm_message *pMessages;
    size_t i;

    if ( !ppOut || !piOutCount ) {
        return XRT_NET_ERROR;
    }

    *ppOut = NULL;
    *piOutCount = 0;

    if ( !pIn || iInCount == 0 ) {
        return XRT_NET_OK;
    }

    pMessages = (xllm_message *)xrtCalloc(iInCount, sizeof(xllm_message));
    if ( !pMessages ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iInCount; ++i ) {
        if ( xllm__message_clone(&pMessages[i], &pIn[i]) != XRT_NET_OK ) {
            xllm__message_array_free(pMessages, iInCount);
            return XRT_NET_ERROR;
        }
    }

    *ppOut = pMessages;
    *piOutCount = iInCount;
    return XRT_NET_OK;
}

static void xllm__context_block_free(xllm_context_block *pBlock)
{
    if ( !pBlock ) {
        return;
    }

    xllm__message_array_free(pBlock->pMessages, pBlock->iMessageCount);
    xllm__xvalue_release(&pBlock->tVendorExtra);
    memset(pBlock, 0, sizeof(*pBlock));
}

static int xllm__context_block_clone(xllm_context_block *pOut, const xllm_context_block *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eKind = pIn->eKind;
    pOut->iPriority = pIn->iPriority;
    pOut->bPinned = pIn->bPinned;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__message_array_clone(&pOut->pMessages, &pOut->iMessageCount, pIn->pMessages, pIn->iMessageCount) != XRT_NET_OK ) {
        xllm__context_block_free(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__context_block_array_free(xllm_context_block *pBlocks, size_t iBlockCount)
{
    size_t i;

    if ( !pBlocks ) {
        return;
    }

    for ( i = 0; i < iBlockCount; ++i ) {
        xllm__context_block_free(&pBlocks[i]);
    }
    xrtFree(pBlocks);
}

static int xllm__context_block_array_clone(xllm_context_block **ppOut, size_t *piOutCount, const xllm_context_block *pIn, size_t iInCount)
{
    xllm_context_block *pBlocks;
    size_t i;

    if ( !ppOut || !piOutCount ) {
        return XRT_NET_ERROR;
    }

    *ppOut = NULL;
    *piOutCount = 0;

    if ( !pIn || iInCount == 0 ) {
        return XRT_NET_OK;
    }

    pBlocks = (xllm_context_block *)xrtCalloc(iInCount, sizeof(xllm_context_block));
    if ( !pBlocks ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iInCount; ++i ) {
        if ( xllm__context_block_clone(&pBlocks[i], &pIn[i]) != XRT_NET_OK ) {
            xllm__context_block_array_free(pBlocks, iInCount);
            return XRT_NET_ERROR;
        }
    }

    *ppOut = pBlocks;
    *piOutCount = iInCount;
    return XRT_NET_OK;
}

static void xllm__tool_def_free(xllm_tool_def *pTool)
{
    if ( !pTool ) {
        return;
    }

    xllm__free_cstr((char **)&pTool->sToolId);
    xllm__free_cstr((char **)&pTool->sWireName);
    xllm__free_cstr((char **)&pTool->sDescription);
    xllm__xvalue_release(&pTool->tInputSchema);
    xllm__xvalue_release(&pTool->tVendorExtra);
    memset(pTool, 0, sizeof(*pTool));
}

static int xllm__tool_def_clone(xllm_tool_def *pOut, const xllm_tool_def *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->sToolId = xllm__dup_cstr(pIn->sToolId);
    pOut->sWireName = xllm__dup_cstr(pIn->sWireName);
    pOut->sDescription = xllm__dup_cstr(pIn->sDescription);
    pOut->eKind = pIn->eKind;
    pOut->tInputSchema = pIn->tInputSchema;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tInputSchema);
    xllm__xvalue_addref(pOut->tVendorExtra);
    return XRT_NET_OK;
}

static void xllm__tool_def_array_free(xllm_tool_def *pTools, size_t iToolCount)
{
    size_t i;

    if ( !pTools ) {
        return;
    }

    for ( i = 0; i < iToolCount; ++i ) {
        xllm__tool_def_free(&pTools[i]);
    }
    xrtFree(pTools);
}

static int xllm__tool_def_array_clone(xllm_tool_def **ppOut, size_t *piOutCount, const xllm_tool_def *pIn, size_t iInCount)
{
    xllm_tool_def *pTools;
    size_t i;

    if ( !ppOut || !piOutCount ) {
        return XRT_NET_ERROR;
    }

    *ppOut = NULL;
    *piOutCount = 0;

    if ( !pIn || iInCount == 0 ) {
        return XRT_NET_OK;
    }

    pTools = (xllm_tool_def *)xrtCalloc(iInCount, sizeof(xllm_tool_def));
    if ( !pTools ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iInCount; ++i ) {
        if ( xllm__tool_def_clone(&pTools[i], &pIn[i]) != XRT_NET_OK ) {
            xllm__tool_def_array_free(pTools, iInCount);
            return XRT_NET_ERROR;
        }
    }

    *ppOut = pTools;
    *piOutCount = iInCount;
    return XRT_NET_OK;
}

static void xllm__generation_params_reset(xllm_generation_params *pParams)
{
    if ( !pParams ) {
        return;
    }
    if ( pParams->psStop ) {
        size_t i;
        for ( i = 0; i < pParams->iStopCount; ++i ) {
            xllm__free_cstr((char **)&pParams->psStop[i]);
        }
        xrtFree((void *)pParams->psStop);
    }
    memset(pParams, 0, sizeof(*pParams));
}

static int xllm__generation_params_clone(xllm_generation_params *pOut, const xllm_generation_params *pIn)
{
    size_t i;

    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    *pOut = *pIn;

    if ( pIn->iStopCount > 0 ) {
        char **psStop = (char **)xrtCalloc(pIn->iStopCount, sizeof(char *));
        if ( !psStop ) {
            memset(pOut, 0, sizeof(*pOut));
            return XRT_NET_ERROR;
        }
        for ( i = 0; i < pIn->iStopCount; ++i ) {
            psStop[i] = xllm__dup_cstr(pIn->psStop[i]);
        }
        pOut->psStop = (const char **)psStop;
    }

    return XRT_NET_OK;
}

static void xllm__reasoning_options_reset(xllm_reasoning_options *pReasoning)
{
    if ( !pReasoning ) {
        return;
    }
    xllm__xvalue_release(&pReasoning->tVendorExtra);
    memset(pReasoning, 0, sizeof(*pReasoning));
}

static int xllm__reasoning_options_clone(xllm_reasoning_options *pOut, const xllm_reasoning_options *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }
    *pOut = *pIn;
    xllm__xvalue_addref(pOut->tVendorExtra);
    return XRT_NET_OK;
}

static void xllm__response_format_reset(xllm_response_format *pFormat)
{
    if ( !pFormat ) {
        return;
    }
    xllm__free_cstr((char **)&pFormat->sSchemaName);
    xllm__xvalue_release(&pFormat->tJsonSchema);
    xllm__xvalue_release(&pFormat->tVendorExtra);
    memset(pFormat, 0, sizeof(*pFormat));
}

static int xllm__response_format_clone(xllm_response_format *pOut, const xllm_response_format *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }
    memset(pOut, 0, sizeof(*pOut));
    pOut->eKind = pIn->eKind;
    pOut->sSchemaName = xllm__dup_cstr(pIn->sSchemaName);
    pOut->tJsonSchema = pIn->tJsonSchema;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tJsonSchema);
    xllm__xvalue_addref(pOut->tVendorExtra);
    return XRT_NET_OK;
}

static void xllm__effective_params_reset(xllm_effective_params *pParams)
{
    if ( !pParams ) {
        return;
    }

    xllm__generation_params_reset(&pParams->tGeneration);
    xllm__reasoning_options_reset(&pParams->tReasoning);
    xllm__response_format_reset(&pParams->tResponseFormat);
    xllm__xvalue_release(&pParams->tVendorExtra);
    memset(pParams, 0, sizeof(*pParams));
}

static void xllm__request_release(xllm_request *pRequest)
{
    if ( !pRequest ) {
        return;
    }

    xllm__free_cstr((char **)&pRequest->sProfileId);
    xllm__message_array_free(pRequest->pMessages, pRequest->iMessageCount);
    xllm__context_block_array_free(pRequest->pContextBlocks, pRequest->iContextBlockCount);
    xllm__tool_def_array_free(pRequest->pTools, pRequest->iToolCount);
    xllm__generation_params_reset(&pRequest->tGeneration);
    xllm__response_format_reset(&pRequest->tResponseFormat);
    xllm__reasoning_options_reset(&pRequest->tReasoning);
    xllm__xvalue_release(&pRequest->tVendorExtra);
    memset(pRequest, 0, sizeof(*pRequest));
}

static int xllm__request_clone(xllm_request *pOut, const xllm_request *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->sProfileId = xllm__dup_cstr(pIn->sProfileId);
    pOut->eSlot = pIn->eSlot;
    pOut->tToolPolicy = pIn->tToolPolicy;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__message_array_clone(&pOut->pMessages, &pOut->iMessageCount, pIn->pMessages, pIn->iMessageCount) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__context_block_array_clone(&pOut->pContextBlocks, &pOut->iContextBlockCount, pIn->pContextBlocks, pIn->iContextBlockCount) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__tool_def_array_clone(&pOut->pTools, &pOut->iToolCount, pIn->pTools, pIn->iToolCount) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__generation_params_clone(&pOut->tGeneration, &pIn->tGeneration) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__response_format_clone(&pOut->tResponseFormat, &pIn->tResponseFormat) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__reasoning_options_clone(&pOut->tReasoning, &pIn->tReasoning) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__turn_release(xllm_turn *pTurn)
{
    if ( !pTurn ) {
        return;
    }

    xllm__free_cstr((char **)&pTurn->sSystemPrompt);
    xllm__message_array_free(pTurn->pMessages, pTurn->iMessageCount);
    xllm__context_block_array_free(pTurn->pContextBlocks, pTurn->iContextBlockCount);
    xllm__tool_def_array_free(pTurn->pTools, pTurn->iToolCount);
    xllm__generation_params_reset(&pTurn->tGeneration);
    xllm__response_format_reset(&pTurn->tResponseFormat);
    xllm__reasoning_options_reset(&pTurn->tReasoning);
    xllm__xvalue_release(&pTurn->tVendorExtra);
    memset(pTurn, 0, sizeof(*pTurn));
}

#if defined(XLLM__WITH_SESSION)
static int xllm__turn_clone(xllm_turn *pOut, const xllm_turn *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->eSlot = pIn->eSlot;
    pOut->eSystemMode = pIn->eSystemMode;
    pOut->sSystemPrompt = xllm__dup_cstr(pIn->sSystemPrompt);
    pOut->tToolPolicy = pIn->tToolPolicy;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__message_array_clone(&pOut->pMessages, &pOut->iMessageCount, pIn->pMessages, pIn->iMessageCount) != XRT_NET_OK ) {
        xllm__turn_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__context_block_array_clone(&pOut->pContextBlocks, &pOut->iContextBlockCount, pIn->pContextBlocks, pIn->iContextBlockCount) != XRT_NET_OK ) {
        xllm__turn_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__tool_def_array_clone(&pOut->pTools, &pOut->iToolCount, pIn->pTools, pIn->iToolCount) != XRT_NET_OK ) {
        xllm__turn_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__generation_params_clone(&pOut->tGeneration, &pIn->tGeneration) != XRT_NET_OK ) {
        xllm__turn_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__response_format_clone(&pOut->tResponseFormat, &pIn->tResponseFormat) != XRT_NET_OK ) {
        xllm__turn_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__reasoning_options_clone(&pOut->tReasoning, &pIn->tReasoning) != XRT_NET_OK ) {
        xllm__turn_release(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_turn_clone(xllm_turn *pOut, const xllm_turn *pIn)
{
    return xllm__turn_clone(pOut, pIn);
}
#endif

static void xllm__tool_exec_result_release(xllm_tool_exec_result *pResult)
{
    if ( !pResult ) {
        return;
    }

    if ( pResult->pParts ) {
        size_t i;
        for ( i = 0; i < pResult->iPartCount; ++i ) {
            xllm__content_part_free(&pResult->pParts[i]);
        }
        xrtFree(pResult->pParts);
    }
    xllm__xvalue_release(&pResult->tVendorExtra);
    memset(pResult, 0, sizeof(*pResult));
}

static const xllm_profile *xllm__runtime_find_profile(const xllm_runtime *pRuntime, const char *sProfileId)
{
    size_t i;

    if ( !pRuntime || !sProfileId ) {
        return NULL;
    }

    for ( i = 0; i < pRuntime->iProfileCount; ++i ) {
        if ( pRuntime->pProfiles[i].sId && strcmp(pRuntime->pProfiles[i].sId, sProfileId) == 0 ) {
            return &pRuntime->pProfiles[i];
        }
    }

    return NULL;
}

static const xllm_adapter *xllm__runtime_find_adapter(const xllm_runtime *pRuntime, const char *sAdapterName)
{
    size_t i;

    if ( !pRuntime || !sAdapterName ) {
        return NULL;
    }

    for ( i = 0; i < pRuntime->iAdapterCount; ++i ) {
        if ( pRuntime->pAdapters[i].sName && strcmp(pRuntime->pAdapters[i].sName, sAdapterName) == 0 ) {
            return &pRuntime->pAdapters[i];
        }
    }

    return NULL;
}

#if defined(XLLM__WITH_SESSION)
static int xllm__build_request_from_turn(
    xllm_request *pOut,
    const char *sProfileId,
    const char *sDefaultSystemPrompt,
    const xllm_turn *pTurn
)
{
    xllm_message *pMessages;
    size_t iPrefixCount;
    size_t iTotalCount;
    size_t iWrite;

    if ( !pOut || !sProfileId || !pTurn ) {
        return XRT_NET_ERROR;
    }

    xllm_request_init(pOut);
    pOut->sProfileId = xllm__dup_cstr(sProfileId);
    pOut->eSlot = pTurn->eSlot;
    pOut->tToolPolicy = pTurn->tToolPolicy;
    pOut->tVendorExtra = pTurn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__context_block_array_clone(&pOut->pContextBlocks, &pOut->iContextBlockCount, pTurn->pContextBlocks, pTurn->iContextBlockCount) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__tool_def_array_clone(&pOut->pTools, &pOut->iToolCount, pTurn->pTools, pTurn->iToolCount) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__generation_params_clone(&pOut->tGeneration, &pTurn->tGeneration) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__response_format_clone(&pOut->tResponseFormat, &pTurn->tResponseFormat) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__reasoning_options_clone(&pOut->tReasoning, &pTurn->tReasoning) != XRT_NET_OK ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }

    iPrefixCount = 0;
    if ( sDefaultSystemPrompt && sDefaultSystemPrompt[0] &&
         (pTurn->eSystemMode == XLLM_SYSTEM_INHERIT || pTurn->eSystemMode == XLLM_SYSTEM_APPEND) ) {
        ++iPrefixCount;
    }
    if ( pTurn->sSystemPrompt && pTurn->sSystemPrompt[0] &&
         pTurn->eSystemMode != XLLM_SYSTEM_INHERIT ) {
        ++iPrefixCount;
    }

    iTotalCount = iPrefixCount + pTurn->iMessageCount;
    if ( iTotalCount == 0 ) {
        return XRT_NET_OK;
    }

    pMessages = (xllm_message *)xrtCalloc(iTotalCount, sizeof(xllm_message));
    if ( !pMessages ) {
        xllm__request_release(pOut);
        return XRT_NET_ERROR;
    }

    pOut->pMessages = pMessages;
    pOut->iMessageCount = iTotalCount;
    iWrite = 0;

    if ( sDefaultSystemPrompt && sDefaultSystemPrompt[0] &&
         (pTurn->eSystemMode == XLLM_SYSTEM_INHERIT || pTurn->eSystemMode == XLLM_SYSTEM_APPEND) ) {
        pMessages[iWrite].eRole = XLLM_ROLE_SYSTEM;
        pMessages[iWrite].pParts = (xllm_content_part *)xrtCalloc(1, sizeof(xllm_content_part));
        if ( !pMessages[iWrite].pParts ) {
            xllm__request_release(pOut);
            return XRT_NET_ERROR;
        }
        pMessages[iWrite].iPartCount = 1;
        pMessages[iWrite].pParts[0].eKind = XLLM_PART_TEXT;
        pMessages[iWrite].pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
        pMessages[iWrite].pParts[0].as.tSource.as.sText = xllm__dup_cstr(sDefaultSystemPrompt);
        ++iWrite;
    }

    if ( pTurn->sSystemPrompt && pTurn->sSystemPrompt[0] &&
         pTurn->eSystemMode != XLLM_SYSTEM_INHERIT ) {
        pMessages[iWrite].eRole = XLLM_ROLE_SYSTEM;
        pMessages[iWrite].pParts = (xllm_content_part *)xrtCalloc(1, sizeof(xllm_content_part));
        if ( !pMessages[iWrite].pParts ) {
            xllm__request_release(pOut);
            return XRT_NET_ERROR;
        }
        pMessages[iWrite].iPartCount = 1;
        pMessages[iWrite].pParts[0].eKind = XLLM_PART_TEXT;
        pMessages[iWrite].pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
        pMessages[iWrite].pParts[0].as.tSource.as.sText = xllm__dup_cstr(pTurn->sSystemPrompt);
        ++iWrite;
    }

    while ( iWrite < iTotalCount ) {
        size_t iSourceIndex = iWrite - iPrefixCount;
        if ( xllm__message_clone(&pMessages[iWrite], &pTurn->pMessages[iSourceIndex]) != XRT_NET_OK ) {
            xllm__request_release(pOut);
            return XRT_NET_ERROR;
        }
        ++iWrite;
    }

    return XRT_NET_OK;
}
#endif

static int xllm__async_future_result_error(xfuture_result *pOut, int32 iStatus, const char *sError)
{
    if ( !pOut ) {
        return iStatus;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->iStatus = iStatus;
    pOut->sError = (str)xllm__dup_cstr(sError ? sError : "xllm async error");
    pOut->iFlags = XFUTURE_RESULT_F_OWN_ERROR;
    return iStatus;
}

XLLM_API const char *xllm_version(void)
{
    return "0.1.0";
}

XLLM_API void xllm_runtime_options_init(xllm_runtime_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }
    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eDebugMode = XLLM_DEBUG_NONE;
    pOptions->eRedactMode = XLLM_REDACT_DEFAULT;
}

XLLM_API void xllm_profile_init(xllm_profile *pProfile)
{
    if ( !pProfile ) {
        return;
    }
    memset(pProfile, 0, sizeof(*pProfile));
    pProfile->tAuth.eKind = XLLM_AUTH_NONE;
    pProfile->tModels.tText.eCapMode = XLLM_CAP_MODE_AUTO;
    pProfile->tModels.tMultimodal.eCapMode = XLLM_CAP_MODE_AUTO;
}

XLLM_API void xllm_request_init(xllm_request *pRequest)
{
    if ( !pRequest ) {
        return;
    }
    memset(pRequest, 0, sizeof(*pRequest));
    pRequest->eSlot = XLLM_SLOT_AUTO;
    pRequest->tToolPolicy.eMode = XLLM_TOOL_CHOICE_AUTO;
    pRequest->tResponseFormat.eKind = XLLM_RESPONSE_TEXT;
    pRequest->tReasoning.eLevel = XLLM_REASONING_DEFAULT;
}

XLLM_API void xllm_request_reset(xllm_request *pRequest)
{
    xllm__request_release(pRequest);
    xllm_request_init(pRequest);
}

XLLM_API void xllm_turn_init(xllm_turn *pTurn)
{
    if ( !pTurn ) {
        return;
    }
    memset(pTurn, 0, sizeof(*pTurn));
    pTurn->eSlot = XLLM_SLOT_AUTO;
    pTurn->eSystemMode = XLLM_SYSTEM_INHERIT;
    pTurn->tToolPolicy.eMode = XLLM_TOOL_CHOICE_AUTO;
    pTurn->tResponseFormat.eKind = XLLM_RESPONSE_TEXT;
    pTurn->tReasoning.eLevel = XLLM_REASONING_DEFAULT;
}

XLLM_API void xllm_turn_reset(xllm_turn *pTurn)
{
    xllm__turn_release(pTurn);
    xllm_turn_init(pTurn);
}

XLLM_API void xllm_call_options_init(xllm_call_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }
    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eStreamMode = XLLM_STREAM_AUTO;
    pOptions->eArtifactPolicy = XLLM_ARTIFACT_INLINE_SMALL;
    pOptions->eLocalFilePolicy = XLLM_LOCAL_FILE_AUTO;
}

XLLM_API void xllm_session_options_init(xllm_session_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }
    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->bEnableAutoCompact = true;
    pOptions->fCompactTriggerRatio = 0.85;
    pOptions->uCompactTriggerTurns = 16;
    pOptions->uKeepRecentTurns = 8;
    pOptions->bKeepActiveToolChain = true;
    pOptions->eCompactStrategy = XLLM_COMPACT_SUMMARIZE;
}

XLLM_API void xllm_compact_options_init(xllm_compact_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }
    memset(pOptions, 0, sizeof(*pOptions));
    pOptions->eMode = XLLM_COMPACT_TO_FIT_CURRENT_MODEL;
    pOptions->eStrategy = XLLM_COMPACT_SUMMARIZE;
}

XLLM_API void xllm_error_init(xllm_error *pError)
{
    if ( !pError ) {
        return;
    }
    memset(pError, 0, sizeof(*pError));
    pError->eCode = XLLM_ERROR_NONE;
}

XLLM_API void xllm_error_reset(xllm_error *pError)
{
    if ( !pError ) {
        return;
    }
    xllm__free_cstr((char **)&pError->sMessage);
    xllm__free_cstr((char **)&pError->sProviderCode);
    xllm__free_cstr((char **)&pError->sProviderMessage);
    xllm__free_cstr((char **)&pError->sRequestId);
    xllm__free_cstr((char **)&pError->sSelectedModel);
    xllm__free_cstr((char **)&pError->sMimeType);
    xllm__xvalue_release(&pError->tVendorExtra);
    memset(pError, 0, sizeof(*pError));
    pError->eCode = XLLM_ERROR_NONE;
}

XLLM_API void xllm_error_free(xllm_error *pError)
{
    xllm_error_reset(pError);
}

XLLM_API void xllm_tool_exec_result_free(xllm_tool_exec_result *pResult)
{
    xllm__tool_exec_result_release(pResult);
}

XLLM_API int xllm_cancel_token_create(xllm_cancel_token **ppToken)
{
    xllm_cancel_token *pToken;

    if ( !ppToken ) {
        return XRT_NET_ERROR;
    }

    *ppToken = NULL;
    pToken = (xllm_cancel_token *)xrtCalloc(1, sizeof(*pToken));
    if ( !pToken ) {
        return XRT_NET_ERROR;
    }

    pToken->pMutex = xrtMutexCreate();
    if ( !pToken->pMutex ) {
        xrtFree(pToken);
        return XRT_NET_ERROR;
    }

    *ppToken = pToken;
    return XRT_NET_OK;
}

XLLM_API void xllm_cancel_token_destroy(xllm_cancel_token *pToken)
{
    if ( !pToken ) {
        return;
    }

    xllm__free_cstr(&pToken->sReason);
    if ( pToken->pMutex ) {
        xrtMutexDestroy(pToken->pMutex);
    }
    xrtFree(pToken);
}

XLLM_API void xllm_cancel_token_cancel(xllm_cancel_token *pToken, const char *sReason)
{
    char *sCopy = NULL;

    if ( !pToken ) {
        return;
    }

    if ( sReason ) {
        sCopy = xllm__dup_cstr(sReason);
    }

    if ( pToken->pMutex ) {
        xrtMutexLock(pToken->pMutex);
    }
    pToken->bCancelled = true;
    xllm__free_cstr(&pToken->sReason);
    pToken->sReason = sCopy;
    if ( pToken->pMutex ) {
        xrtMutexUnlock(pToken->pMutex);
    }
}

XLLM_API bool xllm_cancel_token_is_cancelled(const xllm_cancel_token *pToken)
{
    bool bCancelled;

    if ( !pToken ) {
        return false;
    }

    if ( pToken->pMutex ) {
        xrtMutexLock(pToken->pMutex);
    }
    bCancelled = pToken->bCancelled;
    if ( pToken->pMutex ) {
        xrtMutexUnlock(pToken->pMutex);
    }

    return bCancelled;
}
