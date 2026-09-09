#include "xllm_internal.h"

static xvalue* xllm__json_get(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = (pObject && xrtValueType(pObject) == XVALUE_OBJECT)
        ? xrtValueObjectGet(pObject, (xstrview){ sKey, strlen(sKey) }) : NULL;
    return (pValue && xrtValueType(pValue) != XVALUE_NULL) ? pValue : NULL;
}

static xstrview xllm__json_text(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = xllm__json_get(pObject, sKey);
    xstrview tText = {0};
    if ( pValue ) (void)xrtValueGetString(pValue, &tText);
    return tText;
}

static uint64_t xllm__json_u64(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = xllm__json_get(pObject, sKey);
    int64 iValue = 0;
    if ( !pValue || !xrtValueGetInt(pValue, &iValue) || iValue < 0 ) return 0u;
    return (uint64_t)iValue;
}

static bool xllm__response_replace_view(char** ppDst, xstrview tText)
{
    char* sCopy;
    if ( !tText.Data ) return true;
    sCopy = (char*)malloc(tText.Size + 1u);
    if ( !sCopy ) return false;
    if ( tText.Size ) memcpy(sCopy, tText.Data, tText.Size);
    sCopy[tText.Size] = 0;
    free(*ppDst);
    *ppDst = sCopy;
    return true;
}

xllm_response* xllm__ensure_response(xllm_call* pCall)
{
    if ( !pCall ) { return NULL; }
    if ( !pCall->pResponse ) {
        pCall->pResponse = (xllm_response*)calloc(1u, sizeof(*pCall->pResponse));
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

bool xllm__emit(xllm_call* pCall, const xllm_event* pEvent)
{
    if ( !pCall || !pEvent || !pCall->tCallbacks.OnEvent ) { return true; }
    if ( pCall->tCallbacks.OnEvent(pCall->tCallbacks.pUserData, pEvent) ) { return true; }
    pCall->bCallbackCancelled = true;
    xllm__error_set(&pCall->tError, XLLM_ERROR_CANCELLED, "stream callback cancelled the model call");
    return false;
}

static bool xllm__emit_text(xllm_call* pCall, xllm_event_kind eKind, xstrview tText)
{
    xllm_event tEvent;
    if ( !tText.Data || !tText.Size ) return true;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = eKind;
    tEvent.as.tText.sData = tText.Data;
    tEvent.as.tText.iLen = tText.Size;
    return xllm__emit(pCall, &tEvent);
}

static bool xllm__append_tool_delta(
    xllm_call* pCall,
    size_t iIndex,
    xstrview tId,
    xstrview tName,
    xstrview tArguments
)
{
    xllm_response* pResponse = xllm__ensure_response(pCall);
    xllm_tool_call* pCallOut;
    xllm_event tEvent;
    if ( !pResponse || !xllm__response_ensure_tool(pResponse, iIndex) ) goto oom;
    pCallOut = &pResponse->pToolCalls[iIndex];
    if ( tId.Size && !xllm__response_append_text(&pCallOut->sId, tId.Data, tId.Size) ) goto oom;
    if ( tName.Size && !xllm__response_append_text(&pCallOut->sName, tName.Data, tName.Size) ) goto oom;
    if ( tArguments.Size && !xllm__response_append_text(&pCallOut->sArgumentsJson, tArguments.Data, tArguments.Size) ) goto oom;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_TOOL_CALL_DELTA;
    tEvent.as.tToolCall.iIndex = iIndex;
    tEvent.as.tToolCall.sIdDelta = tId.Size ? pCallOut->sId + strlen(pCallOut->sId) - tId.Size : NULL;
    tEvent.as.tToolCall.sNameDelta = tName.Size ? pCallOut->sName + strlen(pCallOut->sName) - tName.Size : NULL;
    tEvent.as.tToolCall.sArgumentsDelta = tArguments.Size ? pCallOut->sArgumentsJson + strlen(pCallOut->sArgumentsJson) - tArguments.Size : NULL;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append tool-call delta");
    return false;
}

static bool xllm__apply_usage(xllm_call* pCall, xvalue* pUsage)
{
    xllm_response* pResponse;
    xvalue* pPromptDetails;
    xvalue* pCompletionDetails;
    xllm_event tEvent;
    if ( !pUsage || xrtValueType(pUsage) != XVALUE_OBJECT ) return true;
    pResponse = xllm__ensure_response(pCall);
    if ( !pResponse ) { return false; }
    pResponse->tUsage.uInputTokens = xllm__json_u64(pUsage, "prompt_tokens");
    pResponse->tUsage.uOutputTokens = xllm__json_u64(pUsage, "completion_tokens");
    pResponse->tUsage.uTotalTokens = xllm__json_u64(pUsage, "total_tokens");
    pPromptDetails = xllm__json_get(pUsage, "prompt_tokens_details");
    pCompletionDetails = xllm__json_get(pUsage, "completion_tokens_details");
    pResponse->tUsage.uCachedInputTokens = xllm__json_u64(pPromptDetails, "cached_tokens");
    pResponse->tUsage.uReasoningTokens = xllm__json_u64(pCompletionDetails, "reasoning_tokens");
    if ( pResponse->tUsage.uReasoningTokens == 0u ) {
        pResponse->tUsage.uReasoningTokens = xllm__json_u64(pUsage, "reasoning_tokens");
    }
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_USAGE;
    tEvent.as.tUsage = pResponse->tUsage;
    return xllm__emit(pCall, &tEvent);
}

static void xllm__fill_provider_error(xllm_call* pCall, xvalue* pRoot)
{
    xvalue* pError = xllm__json_get(pRoot, "error");
    xvalue* pCode = xllm__json_get(pError, "code");
    xstrview tMessage = xllm__json_text(pError, "message");
    xstrview tCode = xllm__json_text(pError, "code");
    int64 iCode = 0;
    if ( !tMessage.Data ) tMessage = xllm__json_text(pRoot, "message");
    if ( !tCode.Data ) {
        if ( !pCode ) pCode = xllm__json_get(pRoot, "code");
        tCode = xllm__json_text(pRoot, "code");
    }
    if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
        sizeof(pCall->tError.sProviderMessage), tMessage);
    if ( tCode.Data ) {
        xllm__copy_view(pCall->tError.sProviderCode, sizeof(pCall->tError.sProviderCode), tCode);
    } else if ( pCode && xrtValueGetInt(pCode, &iCode) ) {
        (void)snprintf(pCall->tError.sProviderCode, sizeof(pCall->tError.sProviderCode),
            "%lld", (long long)iCode);
    }
    if ( !pCall->tError.sMessage[0] ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
    }
}

void xllm__parse_error_body(xllm_call* pCall, const char* sData, size_t iLen)
{
    xvalue* pRoot;
    if ( !pCall || !sData || iLen == 0u ) { return; }
    if ( !xrtJsonValid((xstrview){ sData, iLen }) ) {
        size_t iCopy = iLen;
        if ( iCopy >= sizeof(pCall->tError.sProviderMessage) ) {
            iCopy = sizeof(pCall->tError.sProviderMessage) - 1u;
        }
        memcpy(pCall->tError.sProviderMessage, sData, iCopy);
        pCall->tError.sProviderMessage[iCopy] = '\0';
        return;
    }
    pRoot = xrtJsonParse((xstrview){ sData, iLen });
    if ( !pRoot ) {
        size_t iCopy = iLen;
        if ( iCopy >= sizeof(pCall->tError.sProviderMessage) ) {
            iCopy = sizeof(pCall->tError.sProviderMessage) - 1u;
        }
        memcpy(pCall->tError.sProviderMessage, sData, iCopy);
        pCall->tError.sProviderMessage[iCopy] = '\0';
        return;
    }
    xllm__fill_provider_error(pCall, pRoot);
    xrtValueRelease(pRoot);
}

static bool xllm__apply_choice_delta(xllm_call* pCall, xvalue* pChoice)
{
    xvalue* pDelta = xllm__json_get(pChoice, "delta");
    xvalue* pToolCalls;
    xstrview tContent;
    xstrview tReasoning;
    xstrview tFinish;
    xllm_response* pResponse = xllm__ensure_response(pCall);
    uint32_t i;
    if ( !pResponse ) { return false; }
    if ( !pDelta || xrtValueType(pDelta) != XVALUE_OBJECT ) pDelta = xllm__json_get(pChoice, "message");
    tContent = xllm__json_text(pDelta, "content");
    tReasoning = xllm__json_text(pDelta, "reasoning_content");
    if ( !tReasoning.Data ) tReasoning = xllm__json_text(pDelta, "reasoning");
    if ( !tReasoning.Data ) tReasoning = xllm__json_text(pDelta, "thinking");
    if ( tContent.Size ) {
        if ( !xllm__response_append_text(&pResponse->sContent, tContent.Data, tContent.Size) ) goto oom;
        if ( !xllm__emit_text(pCall, XLLM_EVENT_TEXT_DELTA, tContent) ) return false;
    }
    if ( tReasoning.Size ) {
        if ( !xllm__response_append_text(&pResponse->sReasoningContent, tReasoning.Data, tReasoning.Size) ) goto oom;
        if ( !xllm__emit_text(pCall, XLLM_EVENT_REASONING_DELTA, tReasoning) ) return false;
    }
    pToolCalls = xllm__json_get(pDelta, "tool_calls");
    if ( pToolCalls && xrtValueType(pToolCalls) == XVALUE_ARRAY ) {
        size_t uCount = xrtValueCount(pToolCalls);
        for ( i = 0u; i < uCount; ++i ) {
            xvalue* pToolCall = xrtValueArrayGet(pToolCalls, i);
            xvalue* pFunction = xllm__json_get(pToolCall, "function");
            xvalue* pIndex = xllm__json_get(pToolCall, "index");
            int64 iValue = (int64)i;
            size_t iIndex;
            if ( pIndex ) (void)xrtValueGetInt(pIndex, &iValue);
            iIndex = iValue >= 0 ? (size_t)iValue : (size_t)i;
            if ( !xllm__append_tool_delta(
                    pCall,
                    iIndex,
                    xllm__json_text(pToolCall, "id"),
                    xllm__json_text(pFunction, "name"),
                    xllm__json_text(pFunction, "arguments")
                 ) ) return false;
        }
    }
    tFinish = xllm__json_text(pChoice, "finish_reason");
    if ( tFinish.Data && !xllm__response_replace_view(&pResponse->sFinishReason, tFinish) ) goto oom;
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append model stream delta");
    return false;
}

bool xllm__parse_stream_event(xllm_call* pCall, const char* sData, size_t iLen)
{
    xvalue* pRoot;
    xvalue* pChoices;
    xllm_response* pResponse;
    xstrview tId;
    xstrview tModel;
    uint32_t i;
    if ( !pCall || !sData ) { return false; }
    while ( iLen && isspace((unsigned char)*sData) ) { ++sData; --iLen; }
    while ( iLen && isspace((unsigned char)sData[iLen - 1u]) ) { --iLen; }
    if ( iLen == 0u ) { return true; }
    if ( iLen == 6u && memcmp(sData, "[DONE]", 6u) == 0 ) { pCall->bDone = true; return true; }
    if ( !xrtJsonValid((xstrview){ sData, iLen }) ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pRoot = xrtJsonParse((xstrview){ sData, iLen });
    if ( !pRoot ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    if ( xllm__json_get(pRoot, "error") ) {
        xllm__fill_provider_error(pCall, pRoot);
        xrtValueRelease(pRoot);
        return false;
    }
    pResponse = xllm__ensure_response(pCall);
    if ( !pResponse ) { xrtValueRelease(pRoot); return false; }
    tId = xllm__json_text(pRoot, "id");
    tModel = xllm__json_text(pRoot, "model");
    if ( tId.Data && !xllm__response_replace_view(&pResponse->sId, tId) ) goto oom;
    if ( tModel.Data && !xllm__response_replace_view(&pResponse->sModel, tModel) ) goto oom;
    pChoices = xllm__json_get(pRoot, "choices");
    if ( pChoices && xrtValueType(pChoices) == XVALUE_ARRAY ) {
        size_t uCount = xrtValueCount(pChoices);
        for ( i = 0u; i < uCount; ++i ) {
            xvalue* pChoice = xrtValueArrayGet(pChoices, i);
            xvalue* pIndex = xllm__json_get(pChoice, "index");
            int64 iIndex = 0;
            if ( pIndex && xrtValueGetInt(pIndex, &iIndex) && iIndex != 0 ) continue;
            if ( !xllm__apply_choice_delta(pCall, pChoice) ) { xrtValueRelease(pRoot); return false; }
            break;
        }
    }
    if ( !xllm__apply_usage(pCall, xllm__json_get(pRoot, "usage")) ) { xrtValueRelease(pRoot); return false; }
    pCall->bSawEvent = true;
    xrtValueRelease(pRoot);
    return true;
oom:
    xrtValueRelease(pRoot);
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to copy provider metadata");
    return false;
}

static bool xllm__sse_process_block(xllm_call* pCall)
{
    bool bOk = true;
    if ( pCall->tEventData.iLen ) {
        bOk = xllm__parse_stream_event(pCall, pCall->tEventData.pData, pCall->tEventData.iLen);
    }
    pCall->tEventData.iLen = 0u;
    if ( pCall->tEventData.pData ) { pCall->tEventData.pData[0] = '\0'; }
    return bOk;
}

static bool xllm__sse_process_line(xllm_call* pCall, const char* sLine, size_t iLen)
{
    const char* sValue;
    size_t iValueLen;
    if ( iLen && sLine[iLen - 1u] == '\r' ) { --iLen; }
    if ( iLen == 0u ) { return xllm__sse_process_block(pCall); }
    if ( sLine[0] == ':' ) { return true; }
    if ( iLen < 5u || memcmp(sLine, "data:", 5u) != 0 ) { return true; }
    sValue = sLine + 5u;
    iValueLen = iLen - 5u;
    if ( iValueLen && *sValue == ' ' ) { ++sValue; --iValueLen; }
    if ( pCall->tEventData.iLen && !xllm__buf_append_char(&pCall->tEventData, '\n') ) return false;
    return xllm__buf_append(&pCall->tEventData, sValue, iValueLen);
}

bool xllm__sse_feed(xllm_call* pCall, const void* pData, size_t iLen)
{
    const unsigned char* p = (const unsigned char*)pData;
    size_t iStart = 0u;
    size_t i;
    if ( !pCall || (!pData && iLen) ) { return false; }
    for ( i = 0u; i < iLen; ++i ) {
        if ( p[i] != '\n' ) { continue; }
        if ( i > iStart && !xllm__buf_append(&pCall->tLine, p + iStart, i - iStart) ) goto oom;
        if ( !xllm__sse_process_line(pCall, pCall->tLine.pData ? pCall->tLine.pData : "", pCall->tLine.iLen) ) return false;
        pCall->tLine.iLen = 0u;
        if ( pCall->tLine.pData ) { pCall->tLine.pData[0] = '\0'; }
        iStart = i + 1u;
    }
    if ( iStart < iLen && !xllm__buf_append(&pCall->tLine, p + iStart, iLen - iStart) ) goto oom;
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to buffer provider event stream");
    return false;
}

bool xllm__sse_finish(xllm_call* pCall)
{
    if ( !pCall ) { return false; }
    if ( pCall->tLine.iLen ) {
        if ( !xllm__sse_process_line(pCall, pCall->tLine.pData, pCall->tLine.iLen) ) return false;
        pCall->tLine.iLen = 0u;
    }
    if ( pCall->tEventData.iLen && !xllm__sse_process_block(pCall) ) return false;
    return true;
}

bool xllm__parse_json_response(xllm_call* pCall, const char* sData, size_t iLen)
{
    xvalue* pRoot;
    xvalue* pChoices;
    xvalue* pChoice;
    xvalue* pMessage;
    xvalue* pTools;
    xllm_response* pResponse;
    xstrview tText;
    xstrview tReasoning;
    uint32_t i;
    if ( !pCall || !sData || !iLen ) { return false; }
    if ( !xrtJsonValid((xstrview){ sData, iLen }) ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    pRoot = xrtJsonParse((xstrview){ sData, iLen });
    if ( !pRoot ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    if ( xllm__json_get(pRoot, "error") ) {
        xllm__fill_provider_error(pCall, pRoot);
        xrtValueRelease(pRoot);
        return false;
    }
    pResponse = xllm__ensure_response(pCall);
    if ( !pResponse ) { xrtValueRelease(pRoot); return false; }
    if ( !xllm__response_replace_view(&pResponse->sId, xllm__json_text(pRoot, "id")) ||
         !xllm__response_replace_view(&pResponse->sModel, xllm__json_text(pRoot, "model")) ) goto oom;
    pChoices = xllm__json_get(pRoot, "choices");
    pChoice = (pChoices && xrtValueType(pChoices) == XVALUE_ARRAY && xrtValueCount(pChoices))
        ? xrtValueArrayGet(pChoices, 0u) : NULL;
    pMessage = xllm__json_get(pChoice, "message");
    if ( !pMessage ) {
        xrtValueRelease(pRoot);
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "provider response has no assistant message");
        return false;
    }
    tText = xllm__json_text(pMessage, "content");
    tReasoning = xllm__json_text(pMessage, "reasoning_content");
    if ( tText.Size && !xllm__response_append_text(&pResponse->sContent, tText.Data, tText.Size) ) goto oom;
    if ( tReasoning.Size && !xllm__response_append_text(&pResponse->sReasoningContent, tReasoning.Data, tReasoning.Size) ) goto oom;
    if ( !xllm__response_replace_view(&pResponse->sFinishReason, xllm__json_text(pChoice, "finish_reason")) ) goto oom;
    pTools = xllm__json_get(pMessage, "tool_calls");
    if ( pTools && xrtValueType(pTools) == XVALUE_ARRAY ) {
        size_t uCount = xrtValueCount(pTools);
        for ( i = 0u; i < uCount; ++i ) {
            xvalue* pToolCall = xrtValueArrayGet(pTools, i);
            xvalue* pFunction = xllm__json_get(pToolCall, "function");
            if ( !xllm__append_tool_delta(
                    pCall,
                    i,
                    xllm__json_text(pToolCall, "id"),
                    xllm__json_text(pFunction, "name"),
                    xllm__json_text(pFunction, "arguments")
                 ) ) { xrtValueRelease(pRoot); return false; }
        }
    }
    if ( !xllm__apply_usage(pCall, xllm__json_get(pRoot, "usage")) ) { xrtValueRelease(pRoot); return false; }
    xrtValueRelease(pRoot);
    return true;
oom:
    xrtValueRelease(pRoot);
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to copy provider response");
    return false;
}
