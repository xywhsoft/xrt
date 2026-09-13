#include "xllm_internal.h"

/* Response assembly: dialect decoders push normalized deltas and complete
 * values into the response under construction. Appends are length-tracked
 * (no tail rescans), blocks record arrival order, and finalize joins the
 * convenience text fields in one pass. */

static xllm_finish xllm__finish_from_reason(const char* sRaw, bool bHasToolCalls);

xvalue* xllm__json_get(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = (pObject && xrtValueType(pObject) == XVALUE_OBJECT)
        ? xrtValueObjectGet(pObject, (xstrview){ sKey, strlen(sKey) }) : NULL;
    return (pValue && xrtValueType(pValue) != XVALUE_NULL) ? pValue : NULL;
}

xstrview xllm__json_text(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = xllm__json_get(pObject, sKey);
    xstrview tText = {0};
    if ( pValue ) (void)xrtValueGetString(pValue, &tText);
    return tText;
}

uint64_t xllm__json_u64(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = xllm__json_get(pObject, sKey);
    int64 iValue = 0;
    if ( !pValue || !xrtValueGetInt(pValue, &iValue) || iValue < 0 ) return 0u;
    return (uint64_t)iValue;
}

bool xllm__emit(xllm_call* pCall, const xllm_event* pEvent)
{
    if ( !pCall || !pEvent || !pCall->tCallbacks.OnEvent ) { return true; }
    if ( pCall->tCallbacks.OnEvent(pCall->tCallbacks.pUserData, pEvent) ) { return true; }
    pCall->bCallbackCancelled = true;
    xllm__error_set(&pCall->tError, XLLM_ERROR_CANCELLED, "stream callback cancelled the model call");
    return false;
}

/* Append provider-native metadata (e.g. an Anthropic thinking signature)
 * to the most recent block of the given kind. */
bool xllm__assemble_native(xllm_call* pCall, xllm_block_kind eKind, xstrview tNative)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    size_t i;
    size_t iLen;
    char* pNew;
    if ( !pResponse || !tNative.Data || !tNative.Size ) { return true; }
    for ( i = pResponse->iBlockCount; i > 0u; --i ) {
        if ( pResponse->pBlocks[i - 1u].eKind == eKind ) { break; }
    }
    if ( !i ) { return true; }
    --i;
    iLen = pResponse->pBlocks[i].sNative ? strlen(pResponse->pBlocks[i].sNative) : 0u;
    if ( iLen > SIZE_MAX - tNative.Size - 1u ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append block metadata");
        return false;
    }
    pNew = (char*)xllm__realloc(pResponse->pBlocks[i].sNative, iLen + tNative.Size + 1u);
    if ( !pNew ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append block metadata");
        return false;
    }
    memcpy(pNew + iLen, tNative.Data, tNative.Size);
    pNew[iLen + tNative.Size] = 0;
    pResponse->pBlocks[i].sNative = pNew;
    return true;
}

void xllm__assemble_first_token(xllm_call* pCall)
{
    if ( pCall && !pCall->tHttpDiagnostics.uFirstTokenMs ) {
        pCall->tHttpDiagnostics.uFirstTokenMs = xrtClock() / UINT64_C(1000);
    }
}

/* Record a TOOL_CALL block in arrival order; finalize leaves these alone. */
bool xllm__assemble_block_mark_tool(xllm_call* pCall, size_t iToolIndex)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    xllm_block* pNew;
    xllm_block_state* pStateNew;
    xllm_event tEvent;
    if ( !pResponse ) { return false; }
    pNew = (xllm_block*)xllm__realloc(pResponse->pBlocks, sizeof(*pNew) * (pResponse->iBlockCount + 1u));
    if ( !pNew ) { goto oom; }
    pResponse->pBlocks = pNew;
    pStateNew = (xllm_block_state*)xllm__realloc(pCall->pBlockState,
        sizeof(*pStateNew) * (pResponse->iBlockCount + 1u));
    if ( !pStateNew ) { goto oom; }
    pCall->pBlockState = pStateNew;
    memset(&pResponse->pBlocks[pResponse->iBlockCount], 0, sizeof(xllm_block));
    pCall->pBlockState[pResponse->iBlockCount].iTextLen = 0u;
    pResponse->pBlocks[pResponse->iBlockCount].eKind = XLLM_BLOCK_TOOL_CALL;
    pResponse->pBlocks[pResponse->iBlockCount].iToolIndex = iToolIndex;
    ++pResponse->iBlockCount;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_BLOCK_META;
    tEvent.as.tBlockMeta.iBlock = pResponse->iBlockCount - 1u;
    tEvent.as.tBlockMeta.eKind = XLLM_BLOCK_TOOL_CALL;
    tEvent.as.tBlockMeta.bEnd = false;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append response block");
    return false;
}

/* Wire block index -> tool index mapping (value is tool index + 1). */
static bool xllm__block_map_reserve(xllm_call* pCall, size_t iWireIndex)
{
    size_t* pNew;
    size_t iCap;
    if ( iWireIndex >= pCall->iBlockMapCap ) {
        iCap = pCall->iBlockMapCap ? pCall->iBlockMapCap : 8u;
        while ( iCap <= iWireIndex ) { iCap *= 2u; }
        pNew = (size_t*)xllm__realloc(pCall->pBlockMap, sizeof(*pNew) * iCap);
        if ( !pNew ) { return false; }
        memset(pNew + pCall->iBlockMapCap, 0, sizeof(*pNew) * (iCap - pCall->iBlockMapCap));
        pCall->pBlockMap = pNew;
        pCall->iBlockMapCap = iCap;
    }
    return true;
}

size_t xllm__assemble_map_block_tool(xllm_call* pCall, size_t iWireIndex)
{
    if ( !pCall || iWireIndex >= pCall->iBlockMapCap ) { return 0u; }
    return pCall->pBlockMap[iWireIndex];
}

bool xllm__assemble_set_block_tool(xllm_call* pCall, size_t iWireIndex, size_t iToolIndex)
{
    if ( !pCall || !xllm__block_map_reserve(pCall, iWireIndex) ) { return false; }
    pCall->pBlockMap[iWireIndex] = iToolIndex + 1u;
    return true;
}

size_t xllm__assemble_find_item_tool(xllm_call* pCall, const char* sItemId)
{
    size_t i;
    if ( !pCall || !sItemId ) { return (size_t)-1; }
    for ( i = 0u; i < pCall->iItemCount; ++i ) {
        if ( strcmp(pCall->pItemMap[i].sId, sItemId) == 0 ) { return pCall->pItemMap[i].iTool; }
    }
    return (size_t)-1;
}

size_t xllm__assemble_add_item_tool(xllm_call* pCall, const char* sItemId,
    xstrview tId, xstrview tName)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    struct xllm_item_map* pNew;
    size_t iCap;
    size_t iTool;
    if ( !pResponse ) { return (size_t)-1; }
    if ( !sItemId || strlen(sItemId) >= sizeof(pCall->pItemMap[0].sId) ) { return (size_t)-1; }
    if ( pCall->iItemCount == pCall->iItemCap ) {
        iCap = pCall->iItemCap ? pCall->iItemCap * 2u : 8u;
        pNew = (struct xllm_item_map*)xllm__realloc(pCall->pItemMap, sizeof(*pNew) * iCap);
        if ( !pNew ) { return (size_t)-1; }
        pCall->pItemMap = pNew;
        pCall->iItemCap = iCap;
    }
    iTool = pResponse->iToolCallCount;
    if ( !xllm__assemble_tool(pCall, iTool, tId, tName, (xstrview){0}) ) { return (size_t)-1; }
    strcpy(pCall->pItemMap[pCall->iItemCount].sId, sItemId);
    pCall->pItemMap[pCall->iItemCount].iTool = iTool;
    ++pCall->iItemCount;
    return iTool;
}

xllm_response* xllm__assemble_ensure(xllm_call* pCall)
{
    if ( !pCall ) { return NULL; }
    if ( !pCall->pResponse ) {
        pCall->pResponse = (xllm_response*)xllm__calloc(1u, sizeof(*pCall->pResponse));
        if ( !pCall->pResponse ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate model response");
            return NULL;
        }
        pCall->pResponse->uHttpStatus = pCall->uHttpStatus;
        if ( pCall->sRequestId[0] ) {
            pCall->pResponse->sRequestId = xllm__strdup(pCall->sRequestId);
            if ( !pCall->pResponse->sRequestId ) {
                xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate request id");
                return NULL;
            }
        }
    }
    return pCall->pResponse;
}

static bool xllm__assemble_block_open(xllm_call* pCall, xllm_block_kind eKind, size_t* piBlock)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    xllm_block* pNew;
    xllm_block_state* pStateNew;
    xllm_event tEvent;
    if ( !pResponse ) { return false; }
    pNew = (xllm_block*)xllm__realloc(pResponse->pBlocks, sizeof(*pNew) * (pResponse->iBlockCount + 1u));
    if ( !pNew ) goto oom;
    pResponse->pBlocks = pNew;
    pStateNew = (xllm_block_state*)xllm__realloc(pCall->pBlockState,
        sizeof(*pStateNew) * (pResponse->iBlockCount + 1u));
    if ( !pStateNew ) goto oom;
    pCall->pBlockState = pStateNew;
    memset(&pResponse->pBlocks[pResponse->iBlockCount], 0, sizeof(xllm_block));
    pCall->pBlockState[pResponse->iBlockCount].iTextLen = 0u;
    *piBlock = pResponse->iBlockCount;
    pResponse->pBlocks[*piBlock].eKind = eKind;
    ++pResponse->iBlockCount;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_BLOCK_META;
    tEvent.as.tBlockMeta.iBlock = *piBlock;
    tEvent.as.tBlockMeta.eKind = eKind;
    tEvent.as.tBlockMeta.bEnd = false;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append response block");
    return false;
}

bool xllm__assemble_text(xllm_call* pCall, xllm_block_kind eKind, xstrview tText, char* sNative)
{
    xllm_response* pResponse;
    xllm_block* pBlock;
    xllm_block_state* pState;
    xllm_event tEvent;
    size_t iBlock;
    size_t iBefore;
    if ( !pCall || (!tText.Data && !sNative) ) { return true; }
    if ( !tText.Data || !tText.Size ) {
        if ( sNative ) { xllm__free(sNative); }
        return true;
    }
    pResponse = xllm__assemble_ensure(pCall);
    if ( !pResponse ) { xllm__free(sNative); return false; }
    if ( pResponse->iBlockCount &&
         pResponse->pBlocks[pResponse->iBlockCount - 1u].eKind == eKind ) {
        iBlock = pResponse->iBlockCount - 1u;
    } else if ( !xllm__assemble_block_open(pCall, eKind, &iBlock) ) {
        xllm__free(sNative);
        return false;
    }
    pBlock = &pResponse->pBlocks[iBlock];
    pState = &pCall->pBlockState[iBlock];
    if ( sNative && !pBlock->sNative ) {
        pBlock->sNative = sNative;
    } else if ( sNative ) {
        xllm__free(sNative);
    }
    iBefore = pState->iTextLen;
    if ( !xllm__append_tracked(&pBlock->sText, &pState->iTextLen, tText.Data, tText.Size) ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append model stream delta");
        return false;
    }
    xllm__assemble_first_token(pCall);
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = eKind == XLLM_BLOCK_TEXT ? XLLM_EVENT_TEXT_DELTA : XLLM_EVENT_REASONING_DELTA;
    tEvent.as.tText.iBlock = iBlock;
    tEvent.as.tText.sData = pBlock->sText + iBefore;
    tEvent.as.tText.iLen = tText.Size;
    return xllm__emit(pCall, &tEvent);
}

static bool xllm__assemble_ensure_tool(xllm_call* pCall, size_t iIndex)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    xllm_tool_call* pNew;
    xllm_tool_state* pStateNew;
    size_t iCap;
    if ( !pResponse ) { return false; }
    if ( iIndex < pResponse->iToolCallCount ) { return true; }
    if ( iIndex >= pResponse->iToolCallCap ) {
        iCap = pResponse->iToolCallCap ? pResponse->iToolCallCap : 4u;
        while ( iCap <= iIndex ) {
            if ( iCap > SIZE_MAX / 2u ) { return false; }
            iCap *= 2u;
        }
        pNew = (xllm_tool_call*)xllm__realloc(pResponse->pToolCalls, sizeof(*pNew) * iCap);
        if ( !pNew ) goto oom;
        pResponse->pToolCalls = pNew;
        pStateNew = (xllm_tool_state*)xllm__realloc(pCall->pToolState, sizeof(*pStateNew) * iCap);
        if ( !pStateNew ) goto oom;
        pCall->pToolState = pStateNew;
        memset(pResponse->pToolCalls + pResponse->iToolCallCap, 0,
            sizeof(*pNew) * (iCap - pResponse->iToolCallCap));
        memset(pCall->pToolState + pResponse->iToolCallCap, 0,
            sizeof(*pStateNew) * (iCap - pResponse->iToolCallCap));
        pResponse->iToolCallCap = iCap;
    }
    pResponse->iToolCallCount = iIndex + 1u;
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append tool-call slot");
    return false;
}

bool xllm__assemble_tool(xllm_call* pCall, size_t iIndex, xstrview tId, xstrview tName, xstrview tArguments)
{
    xllm_response* pResponse;
    xllm_tool_call* pCallOut;
    xllm_tool_state* pState;
    xllm_event tEvent;
    const char* sIdDelta = NULL;
    const char* sNameDelta = NULL;
    const char* sArgsDelta = NULL;
    if ( !pCall || !xllm__assemble_ensure_tool(pCall, iIndex) ) { return false; }
    pResponse = pCall->pResponse;
    pCallOut = &pResponse->pToolCalls[iIndex];
    pState = &pCall->pToolState[iIndex];
    if ( tId.Size ) {
        if ( !xllm__append_tracked(&pCallOut->sId, &pState->iIdLen, tId.Data, tId.Size) ) goto oom;
        sIdDelta = pCallOut->sId + pState->iIdLen - tId.Size;
    }
    if ( tName.Size ) {
        if ( !xllm__append_tracked(&pCallOut->sName, &pState->iNameLen, tName.Data, tName.Size) ) goto oom;
        sNameDelta = pCallOut->sName + pState->iNameLen - tName.Size;
    }
    if ( tArguments.Size ) {
        if ( !xllm__append_tracked(&pCallOut->sArgumentsJson, &pState->iArgsLen, tArguments.Data, tArguments.Size) ) goto oom;
        sArgsDelta = pCallOut->sArgumentsJson + pState->iArgsLen - tArguments.Size;
    }
    xllm__assemble_first_token(pCall);
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_TOOL_CALL_DELTA;
    tEvent.as.tToolCall.iIndex = iIndex;
    tEvent.as.tToolCall.iBlock = pResponse->iBlockCount;
    tEvent.as.tToolCall.sIdDelta = sIdDelta;
    tEvent.as.tToolCall.sNameDelta = sNameDelta;
    tEvent.as.tToolCall.sArgumentsDelta = sArgsDelta;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append tool-call delta");
    return false;
}

bool xllm__assemble_usage(xllm_call* pCall, const xllm_usage* pUsage)
{
    xllm_response* pResponse;
    xllm_event tEvent;
    if ( !pUsage ) { return true; }
    pResponse = xllm__assemble_ensure(pCall);
    if ( !pResponse ) { return false; }
    pResponse->tUsage = *pUsage;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_USAGE;
    tEvent.as.tUsage = *pUsage;
    return xllm__emit(pCall, &tEvent);
}

void xllm__assemble_finish(xllm_call* pCall, xstrview tRaw)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    char* sRaw = NULL;
    if ( !pResponse ) { return; }
    if ( tRaw.Data && tRaw.Size ) {
        sRaw = (char*)xllm__malloc(tRaw.Size + 1u);
        if ( sRaw ) {
            memcpy(sRaw, tRaw.Data, tRaw.Size);
            sRaw[tRaw.Size] = 0;
        }
    }
    xllm__free(pResponse->sFinishReason);
    pResponse->sFinishReason = sRaw;
    pResponse->eFinish = xllm__finish_from_reason(sRaw, pResponse->iToolCallCount != 0u);
}

bool xllm__assemble_refusal(xllm_call* pCall, xstrview tRefusal)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    char* sCopy = NULL;
    if ( !pResponse || !tRefusal.Data || !tRefusal.Size ) { return true; }
    sCopy = (char*)xllm__malloc(tRefusal.Size + 1u);
    if ( !sCopy ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to record provider refusal");
        return false;
    }
    memcpy(sCopy, tRefusal.Data, tRefusal.Size);
    sCopy[tRefusal.Size] = 0;
    xllm__free(pResponse->sRefusal);
    pResponse->sRefusal = sCopy;
    pResponse->eFinish = XLLM_FINISH_REFUSAL;
    return true;
}

static xllm_finish xllm__finish_from_reason(const char* sRaw, bool bHasToolCalls)
{
    if ( !sRaw ) { return bHasToolCalls ? XLLM_FINISH_TOOL_CALLS : XLLM_FINISH_STOP; }
    if ( strcmp(sRaw, "stop") == 0 || strcmp(sRaw, "end_turn") == 0 ||
         strcmp(sRaw, "stop_sequence") == 0 ) { return XLLM_FINISH_STOP; }
    if ( strcmp(sRaw, "length") == 0 || strcmp(sRaw, "max_tokens") == 0 ) { return XLLM_FINISH_LENGTH; }
    if ( strcmp(sRaw, "tool_calls") == 0 || strcmp(sRaw, "function_call") == 0 ||
         strcmp(sRaw, "tool_use") == 0 ) { return XLLM_FINISH_TOOL_CALLS; }
    if ( strcmp(sRaw, "content_filter") == 0 ) { return XLLM_FINISH_CONTENT_FILTER; }
    return XLLM_FINISH_OTHER;
}

static bool xllm__join_blocks(xllm_call* pCall, xllm_block_kind eKind, char** ppOut, size_t* pOutLen)
{
    xllm_response* pResponse = pCall->pResponse;
    size_t i;
    size_t iTotal = 0u;
    char* sOut;
    if ( !pResponse ) { return true; }
    for ( i = 0u; i < pResponse->iBlockCount; ++i ) {
        if ( pResponse->pBlocks[i].eKind == eKind ) {
            const char* sText = pResponse->pBlocks[i].sText;
            iTotal += sText ? strlen(sText) : 0u;
        }
    }
    if ( !iTotal ) {
        *ppOut = NULL;
        if ( pOutLen ) { *pOutLen = 0u; }
        return true;
    }
    sOut = (char*)xllm__malloc(iTotal + 1u);
    if ( !sOut ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to join response blocks");
        return false;
    }
    iTotal = 0u;
    for ( i = 0u; i < pResponse->iBlockCount; ++i ) {
        if ( pResponse->pBlocks[i].eKind == eKind ) {
            const char* sText = pResponse->pBlocks[i].sText;
            if ( sText ) {
                size_t iLen = strlen(sText);
                memcpy(sOut + iTotal, sText, iLen);
                iTotal += iLen;
            }
        }
    }
    sOut[iTotal] = 0;
    *ppOut = sOut;
    if ( pOutLen ) { *pOutLen = iTotal; }
    return true;
}

bool xllm__assemble_finalize(xllm_call* pCall)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    xllm_event tEvent;
    size_t i;
    char sGeneratedId[64];
    if ( !pResponse ) { return false; }
    pResponse->uHttpStatus = pCall->uHttpStatus;
    if ( !pResponse->sRequestId && pCall->sRequestId[0] ) {
        pResponse->sRequestId = xllm__strdup(pCall->sRequestId);
        if ( !pResponse->sRequestId ) goto oom;
    }
    if ( !pResponse->sModel && pCall->sSelectedModel ) {
        pResponse->sModel = xllm__strdup(pCall->sSelectedModel);
        if ( !pResponse->sModel ) goto oom;
    }
    for ( i = 0u; i < pResponse->iToolCallCount; ++i ) {
        xllm_block* pToolBlock;
        xllm_tool_call* pTool = &pResponse->pToolCalls[i];
        bool bHasBlock = false;
        size_t j;
        if ( !pTool->sId || !pTool->sId[0] ) {
            (void)snprintf(sGeneratedId, sizeof(sGeneratedId), "call_%u", (unsigned)i);
            if ( !xllm__replace(&pTool->sId, sGeneratedId) ) goto oom;
        }
        if ( !pTool->sName || !pTool->sName[0] ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL, "provider returned a tool call without a function name");
            return false;
        }
        if ( !pTool->sArgumentsJson || !pTool->sArgumentsJson[0] ) {
            if ( !xllm__replace(&pTool->sArgumentsJson, "{}") ) goto oom;
        }
        if ( !xrtJsonValid((xstrview){ pTool->sArgumentsJson, strlen(pTool->sArgumentsJson) }) ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "provider returned invalid tool-call arguments JSON");
            return false;
        }
        for ( j = 0u; j < pResponse->iBlockCount; ++j ) {
            if ( pResponse->pBlocks[j].eKind == XLLM_BLOCK_TOOL_CALL &&
                 pResponse->pBlocks[j].iToolIndex == i ) { bHasBlock = true; break; }
        }
        if ( bHasBlock ) { continue; }
        pToolBlock = (xllm_block*)xllm__realloc(pResponse->pBlocks,
            sizeof(*pToolBlock) * (pResponse->iBlockCount + 1u));
        if ( !pToolBlock ) goto oom;
        pResponse->pBlocks = pToolBlock;
        memset(&pResponse->pBlocks[pResponse->iBlockCount], 0, sizeof(xllm_block));
        pResponse->pBlocks[pResponse->iBlockCount].eKind = XLLM_BLOCK_TOOL_CALL;
        pResponse->pBlocks[pResponse->iBlockCount].iToolIndex = i;
        ++pResponse->iBlockCount;
    }
    if ( pResponse->sRefusal && pResponse->sRefusal[0] ) {
        pResponse->eFinish = XLLM_FINISH_REFUSAL;
    } else if ( !pResponse->sFinishReason ) {
        pResponse->eFinish = pResponse->iToolCallCount ? XLLM_FINISH_TOOL_CALLS : XLLM_FINISH_STOP;
    } else {
        pResponse->eFinish = xllm__finish_from_reason(pResponse->sFinishReason, pResponse->iToolCallCount != 0u);
    }
    if ( !pResponse->sContent && !xllm__join_blocks(pCall, XLLM_BLOCK_TEXT, &pResponse->sContent, NULL) ) { return false; }
    if ( !pResponse->sContent ) {
        pResponse->sContent = xllm__strdup("");
        if ( !pResponse->sContent ) goto oom;
    }
    if ( !pResponse->sReasoningContent && !xllm__join_blocks(pCall, XLLM_BLOCK_REASONING, &pResponse->sReasoningContent, NULL) ) { return false; }
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_RESPONSE_DONE;
    tEvent.as.tResponse.uHttpStatus = pCall->uHttpStatus;
    tEvent.as.tResponse.sRequestId = pCall->sRequestId;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to finalize model response");
    return false;
}
