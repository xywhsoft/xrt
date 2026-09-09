#include "xllm_internal.h"

char* xllm__strdup(const char* sText)
{
    size_t iLen;
    char* sCopy;
    if ( !sText ) { return NULL; }
    iLen = strlen(sText);
    sCopy = (char*)malloc(iLen + 1u);
    if ( !sCopy ) { return NULL; }
    memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

bool xllm__replace(char** ppDst, const char* sText)
{
    char* sCopy = sText ? xllm__strdup(sText) : NULL;
    if ( sText && !sCopy ) { return false; }
    free(*ppDst);
    *ppDst = sCopy;
    return true;
}

bool xllm__buf_reserve(xllm_buf* pBuf, size_t iNeed)
{
    size_t iCap;
    char* pNew;
    if ( !pBuf ) { return false; }
    if ( iNeed <= pBuf->iCap ) { return true; }
    iCap = pBuf->iCap ? pBuf->iCap : 256u;
    while ( iCap < iNeed ) {
        if ( iCap > SIZE_MAX / 2u ) { iCap = iNeed; break; }
        iCap *= 2u;
    }
    if ( iCap < iNeed ) { return false; }
    pNew = (char*)realloc(pBuf->pData, iCap);
    if ( !pNew ) { return false; }
    pBuf->pData = pNew;
    pBuf->iCap = iCap;
    return true;
}

bool xllm__buf_append(xllm_buf* pBuf, const void* pData, size_t iLen)
{
    if ( !pBuf || (!pData && iLen) || pBuf->iLen > SIZE_MAX - iLen - 1u ) { return false; }
    if ( !xllm__buf_reserve(pBuf, pBuf->iLen + iLen + 1u) ) { return false; }
    if ( iLen ) { memcpy(pBuf->pData + pBuf->iLen, pData, iLen); }
    pBuf->iLen += iLen;
    pBuf->pData[pBuf->iLen] = '\0';
    return true;
}

bool xllm__buf_append_cstr(xllm_buf* pBuf, const char* sText)
{
    return xllm__buf_append(pBuf, sText ? sText : "", sText ? strlen(sText) : 0u);
}

bool xllm__buf_append_char(xllm_buf* pBuf, char ch)
{
    return xllm__buf_append(pBuf, &ch, 1u);
}

void xllm__buf_reset(xllm_buf* pBuf)
{
    if ( !pBuf ) { return; }
    free(pBuf->pData);
    memset(pBuf, 0, sizeof(*pBuf));
}

char* xllm__buf_detach(xllm_buf* pBuf)
{
    char* pData;
    if ( !pBuf ) { return NULL; }
    if ( !pBuf->pData ) {
        pBuf->pData = (char*)calloc(1u, 1u);
        if ( !pBuf->pData ) { return NULL; }
    }
    pData = pBuf->pData;
    pBuf->pData = NULL;
    pBuf->iLen = 0u;
    pBuf->iCap = 0u;
    return pData;
}

bool xllm__json_string(xllm_buf* pBuf, const char* sText)
{
    const unsigned char* p = (const unsigned char*)(sText ? sText : "");
    char sEscape[7];
    if ( !xllm__buf_append_char(pBuf, '"') ) { return false; }
    while ( *p ) {
        switch ( *p ) {
            case '"': if ( !xllm__buf_append_cstr(pBuf, "\\\"") ) return false; break;
            case '\\': if ( !xllm__buf_append_cstr(pBuf, "\\\\") ) return false; break;
            case '\b': if ( !xllm__buf_append_cstr(pBuf, "\\b") ) return false; break;
            case '\f': if ( !xllm__buf_append_cstr(pBuf, "\\f") ) return false; break;
            case '\n': if ( !xllm__buf_append_cstr(pBuf, "\\n") ) return false; break;
            case '\r': if ( !xllm__buf_append_cstr(pBuf, "\\r") ) return false; break;
            case '\t': if ( !xllm__buf_append_cstr(pBuf, "\\t") ) return false; break;
            default:
                if ( *p < 0x20u ) {
                    (void)snprintf(sEscape, sizeof(sEscape), "\\u%04x", (unsigned)*p);
                    if ( !xllm__buf_append_cstr(pBuf, sEscape) ) return false;
                } else if ( !xllm__buf_append(pBuf, p, 1u) ) {
                    return false;
                }
                break;
        }
        ++p;
    }
    return xllm__buf_append_char(pBuf, '"');
}

void xllm__copy_text(char* sDst, size_t iCap, const char* sSrc)
{
    size_t iLen;
    if ( !sDst || iCap == 0u ) { return; }
    if ( !sSrc ) { sDst[0] = '\0'; return; }
    iLen = strlen(sSrc);
    if ( iLen >= iCap ) { iLen = iCap - 1u; }
    memcpy(sDst, sSrc, iLen);
    sDst[iLen] = '\0';
}

void xllmErrorInit(xllm_error* pError)
{
    if ( pError ) { memset(pError, 0, sizeof(*pError)); }
}

void xllm__error_set(xllm_error* pError, xllm_error_code eCode, const char* sMessage)
{
    if ( !pError ) { return; }
    pError->eCode = eCode;
    xllm__copy_text(pError->sMessage, sizeof(pError->sMessage), sMessage);
}

void xllm__error_copy(xllm_error* pDst, const xllm_error* pSrc)
{
    if ( pDst ) {
        if ( pSrc ) { memcpy(pDst, pSrc, sizeof(*pDst)); }
        else { xllmErrorInit(pDst); }
    }
}

const char* xllmErrorCodeName(xllm_error_code eCode)
{
    switch ( eCode ) {
        case XLLM_ERROR_NONE: return "none";
        case XLLM_ERROR_INVALID_ARGUMENT: return "invalid_argument";
        case XLLM_ERROR_OUT_OF_MEMORY: return "out_of_memory";
        case XLLM_ERROR_NETWORK: return "network";
        case XLLM_ERROR_TIMEOUT: return "timeout";
        case XLLM_ERROR_CANCELLED: return "cancelled";
        case XLLM_ERROR_AUTH: return "auth";
        case XLLM_ERROR_RATE_LIMIT: return "rate_limit";
        case XLLM_ERROR_MODEL_NOT_FOUND: return "model_not_found";
        case XLLM_ERROR_UPSTREAM: return "upstream";
        case XLLM_ERROR_PROTOCOL: return "protocol";
        case XLLM_ERROR_PARSE: return "parse";
        default: return "unknown";
    }
}

bool xllmErrorRetryable(const xllm_error* pError)
{
    int32_t iHttpStatus;
    if ( !pError || pError->eCode == XLLM_ERROR_NONE ||
         pError->tDiagnostics.bModelDataDelivered ) {
        return false;
    }
    /* Provider policy code 1313 requires an explicit account-side review.
       Retrying it as a transient 429 only creates avoidable traffic. */
    if ( strcmp(pError->sProviderCode, "1313") == 0 ) { return false; }
    iHttpStatus = pError->iHttpStatus;
    if ( iHttpStatus == 408 || iHttpStatus == 409 || iHttpStatus == 425 ||
         iHttpStatus == 429 || iHttpStatus == 500 || iHttpStatus == 502 ||
         iHttpStatus == 503 || iHttpStatus == 504 ) {
        return true;
    }
    if ( pError->eCode == XLLM_ERROR_RATE_LIMIT || pError->eCode == XLLM_ERROR_NETWORK ) {
        return true;
    }
    if ( pError->eCode == XLLM_ERROR_TIMEOUT ) {
        return strcmp(pError->tDiagnostics.sTransportError, "timeout_total") != 0 &&
            strcmp(pError->tDiagnostics.sTransportError, "deadline_exceeded") != 0;
    }
    return false;
}


bool xllm__contains_ci(const char* sText, const char* sNeedle)
{
    size_t iNeedle;
    if ( !sText || !sNeedle ) { return false; }
    iNeedle = strlen(sNeedle);
    if ( iNeedle == 0u ) { return true; }
    for ( ; *sText; ++sText ) {
        size_t i;
        for ( i = 0u; i < iNeedle; ++i ) {
            unsigned char a = (unsigned char)sText[i];
            unsigned char b = (unsigned char)sNeedle[i];
            if ( !a || (unsigned char)tolower(a) != (unsigned char)tolower(b) ) { break; }
        }
        if ( i == iNeedle ) { return true; }
    }
    return false;
}

void xllm__tool_call_unit(xllm_tool_call* pCall)
{
    if ( !pCall ) { return; }
    free(pCall->sId);
    free(pCall->sName);
    free(pCall->sArgumentsJson);
    memset(pCall, 0, sizeof(*pCall));
}

bool xllm__tool_call_clone(xllm_tool_call* pDst, const xllm_tool_call* pSrc)
{
    if ( !pDst || !pSrc ) { return false; }
    memset(pDst, 0, sizeof(*pDst));
    if ( pSrc->sId && !(pDst->sId = xllm__strdup(pSrc->sId)) ) goto fail;
    if ( pSrc->sName && !(pDst->sName = xllm__strdup(pSrc->sName)) ) goto fail;
    if ( pSrc->sArgumentsJson && !(pDst->sArgumentsJson = xllm__strdup(pSrc->sArgumentsJson)) ) goto fail;
    return true;
fail:
    xllm__tool_call_unit(pDst);
    return false;
}

void xllmMessageInit(xllm_message* pMessage, xllm_role eRole)
{
    if ( !pMessage ) { return; }
    memset(pMessage, 0, sizeof(*pMessage));
    pMessage->eRole = eRole;
}

void xllmMessageUnit(xllm_message* pMessage)
{
    size_t i;
    if ( !pMessage ) { return; }
    free(pMessage->sContent);
    free(pMessage->sReasoningContent);
    free(pMessage->sToolCallId);
    for ( i = 0u; i < pMessage->iToolCallCount; ++i ) { xllm__tool_call_unit(&pMessage->pToolCalls[i]); }
    free(pMessage->pToolCalls);
    memset(pMessage, 0, sizeof(*pMessage));
}

bool xllmMessageSetContent(xllm_message* pMessage, const char* sContent)
{
    return pMessage && xllm__replace(&pMessage->sContent, sContent ? sContent : "");
}

bool xllmMessageSetReasoning(xllm_message* pMessage, const char* sReasoningContent)
{
    return pMessage && xllm__replace(&pMessage->sReasoningContent, sReasoningContent);
}

bool xllmMessageSetToolCallId(xllm_message* pMessage, const char* sToolCallId)
{
    return pMessage && xllm__replace(&pMessage->sToolCallId, sToolCallId);
}

bool xllmMessageAddToolCall(xllm_message* pMessage, const char* sId, const char* sName, const char* sArgumentsJson)
{
    xllm_tool_call* pNew;
    size_t iCap;
    xllm_tool_call* pCall;
    if ( !pMessage || !sName || !sName[0] ) { return false; }
    if ( pMessage->iToolCallCount == pMessage->iToolCallCap ) {
        iCap = pMessage->iToolCallCap ? pMessage->iToolCallCap * 2u : 4u;
        pNew = (xllm_tool_call*)realloc(pMessage->pToolCalls, sizeof(*pNew) * iCap);
        if ( !pNew ) { return false; }
        memset(pNew + pMessage->iToolCallCap, 0, sizeof(*pNew) * (iCap - pMessage->iToolCallCap));
        pMessage->pToolCalls = pNew;
        pMessage->iToolCallCap = iCap;
    }
    pCall = &pMessage->pToolCalls[pMessage->iToolCallCount];
    pCall->sId = xllm__strdup(sId ? sId : "");
    pCall->sName = xllm__strdup(sName);
    pCall->sArgumentsJson = xllm__strdup(sArgumentsJson ? sArgumentsJson : "{}");
    if ( !pCall->sId || !pCall->sName || !pCall->sArgumentsJson ) {
        xllm__tool_call_unit(pCall);
        return false;
    }
    ++pMessage->iToolCallCount;
    return true;
}

bool xllm__message_clone(xllm_message* pDst, const xllm_message* pSrc)
{
    size_t i;
    if ( !pDst || !pSrc ) { return false; }
    xllmMessageInit(pDst, pSrc->eRole);
    if ( pSrc->sContent && !xllmMessageSetContent(pDst, pSrc->sContent) ) goto fail;
    if ( pSrc->sReasoningContent && !xllmMessageSetReasoning(pDst, pSrc->sReasoningContent) ) goto fail;
    if ( pSrc->sToolCallId && !xllmMessageSetToolCallId(pDst, pSrc->sToolCallId) ) goto fail;
    for ( i = 0u; i < pSrc->iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pSrc->pToolCalls[i];
        if ( !xllmMessageAddToolCall(pDst, pCall->sId, pCall->sName, pCall->sArgumentsJson) ) goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pDst);
    return false;
}

void xllm__tool_unit(xllm_tool* pTool)
{
    if ( !pTool ) { return; }
    free(pTool->sName);
    free(pTool->sDescription);
    free(pTool->sParametersJson);
    memset(pTool, 0, sizeof(*pTool));
}

void xllmRequestInit(xllm_request* pRequest)
{
    if ( !pRequest ) { return; }
    memset(pRequest, 0, sizeof(*pRequest));
    pRequest->bParallelToolCalls = true;
    pRequest->eToolChoice = XLLM_TOOL_CHOICE_AUTO;
    pRequest->uDeadline = UINT64_MAX;
}

void xllmRequestUnit(xllm_request* pRequest)
{
    size_t i;
    if ( !pRequest ) { return; }
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) { xllmMessageUnit(&pRequest->pMessages[i]); }
    for ( i = 0u; i < pRequest->iToolCount; ++i ) { xllm__tool_unit(&pRequest->pTools[i]); }
    free(pRequest->pMessages);
    free(pRequest->pTools);
    free(pRequest->sModel);
    free(pRequest->sReasoningEffort);
    free(pRequest->sNamedTool);
    memset(pRequest, 0, sizeof(*pRequest));
}

bool xllmRequestSetModel(xllm_request* pRequest, const char* sModel)
{
    return pRequest && xllm__replace(&pRequest->sModel, sModel);
}

bool xllmRequestSetReasoningEffort(xllm_request* pRequest, const char* sEffort)
{
    return pRequest && xllm__replace(&pRequest->sReasoningEffort, sEffort);
}

void xllmRequestSetCancel(xllm_request* pRequest, xcancel* pCancel)
{
    if ( pRequest ) { pRequest->pCancel = pCancel; }
}

void xllmRequestSetDeadline(xllm_request* pRequest, uint64_t uDeadline)
{
    if ( pRequest ) { pRequest->uDeadline = uDeadline; }
}

bool xllmRequestSetToolChoice(xllm_request* pRequest, xllm_tool_choice eChoice, const char* sNamedTool)
{
    if ( !pRequest || eChoice < XLLM_TOOL_CHOICE_AUTO || eChoice > XLLM_TOOL_CHOICE_NAMED ) { return false; }
    if ( eChoice == XLLM_TOOL_CHOICE_NAMED && (!sNamedTool || !sNamedTool[0]) ) { return false; }
    if ( !xllm__replace(&pRequest->sNamedTool, eChoice == XLLM_TOOL_CHOICE_NAMED ? sNamedTool : NULL) ) { return false; }
    pRequest->eToolChoice = eChoice;
    return true;
}

bool xllmRequestAddMessage(xllm_request* pRequest, const xllm_message* pMessage)
{
    xllm_message* pNew;
    size_t iCap;
    if ( !pRequest || !pMessage ) { return false; }
    if ( pRequest->iMessageCount == pRequest->iMessageCap ) {
        iCap = pRequest->iMessageCap ? pRequest->iMessageCap * 2u : 8u;
        pNew = (xllm_message*)realloc(pRequest->pMessages, sizeof(*pNew) * iCap);
        if ( !pNew ) { return false; }
        memset(pNew + pRequest->iMessageCap, 0, sizeof(*pNew) * (iCap - pRequest->iMessageCap));
        pRequest->pMessages = pNew;
        pRequest->iMessageCap = iCap;
    }
    if ( !xllm__message_clone(&pRequest->pMessages[pRequest->iMessageCount], pMessage) ) { return false; }
    ++pRequest->iMessageCount;
    return true;
}

bool xllmRequestAddTextMessage(xllm_request* pRequest, xllm_role eRole, const char* sContent)
{
    xllm_message tMessage;
    bool bOk;
    if ( !pRequest || eRole == XLLM_ROLE_TOOL ) { return false; }
    xllmMessageInit(&tMessage, eRole);
    bOk = xllmMessageSetContent(&tMessage, sContent ? sContent : "") && xllmRequestAddMessage(pRequest, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmRequestAddToolResult(xllm_request* pRequest, const char* sToolCallId, const char* sContent)
{
    xllm_message tMessage;
    bool bOk;
    if ( !pRequest || !sToolCallId || !sToolCallId[0] ) { return false; }
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    bOk = xllmMessageSetToolCallId(&tMessage, sToolCallId) &&
        xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
        xllmRequestAddMessage(pRequest, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmRequestAddTool(xllm_request* pRequest, const char* sName, const char* sDescription, const char* sParametersJson, bool bStrict)
{
    xllm_tool* pNew;
    xllm_tool* pTool;
    size_t iCap;
    const char* sSchema = sParametersJson ? sParametersJson : "{\"type\":\"object\",\"properties\":{}}";
    if ( !pRequest || !sName || !sName[0] ) { return false; }
    if ( pRequest->iToolCount == pRequest->iToolCap ) {
        iCap = pRequest->iToolCap ? pRequest->iToolCap * 2u : 8u;
        pNew = (xllm_tool*)realloc(pRequest->pTools, sizeof(*pNew) * iCap);
        if ( !pNew ) { return false; }
        memset(pNew + pRequest->iToolCap, 0, sizeof(*pNew) * (iCap - pRequest->iToolCap));
        pRequest->pTools = pNew;
        pRequest->iToolCap = iCap;
    }
    pTool = &pRequest->pTools[pRequest->iToolCount];
    pTool->sName = xllm__strdup(sName);
    pTool->sDescription = xllm__strdup(sDescription ? sDescription : "");
    pTool->sParametersJson = xllm__strdup(sSchema);
    pTool->bStrict = bStrict;
    if ( !pTool->sName || !pTool->sDescription || !pTool->sParametersJson ) {
        xllm__tool_unit(pTool);
        return false;
    }
    ++pRequest->iToolCount;
    return true;
}

bool xllm__response_append_text(char** ppText, const char* sDelta, size_t iLen)
{
    size_t iOld;
    char* pNew;
    if ( !ppText || (!sDelta && iLen) ) { return false; }
    iOld = *ppText ? strlen(*ppText) : 0u;
    if ( iOld > SIZE_MAX - iLen - 1u ) { return false; }
    pNew = (char*)realloc(*ppText, iOld + iLen + 1u);
    if ( !pNew ) { return false; }
    if ( iLen ) { memcpy(pNew + iOld, sDelta, iLen); }
    pNew[iOld + iLen] = '\0';
    *ppText = pNew;
    return true;
}

bool xllm__response_ensure_tool(xllm_response* pResponse, size_t iIndex)
{
    xllm_tool_call* pNew;
    size_t iCap;
    if ( !pResponse ) { return false; }
    if ( iIndex < pResponse->iToolCallCount ) { return true; }
    if ( iIndex >= pResponse->iToolCallCap ) {
        iCap = pResponse->iToolCallCap ? pResponse->iToolCallCap : 4u;
        while ( iCap <= iIndex ) {
            if ( iCap > SIZE_MAX / 2u ) { return false; }
            iCap *= 2u;
        }
        pNew = (xllm_tool_call*)realloc(pResponse->pToolCalls, sizeof(*pNew) * iCap);
        if ( !pNew ) { return false; }
        memset(pNew + pResponse->iToolCallCap, 0, sizeof(*pNew) * (iCap - pResponse->iToolCallCap));
        pResponse->pToolCalls = pNew;
        pResponse->iToolCallCap = iCap;
    }
    pResponse->iToolCallCount = iIndex + 1u;
    return true;
}

void xllmResponseDestroy(xllm_response* pResponse)
{
    size_t i;
    if ( !pResponse ) { return; }
    free(pResponse->sId);
    free(pResponse->sModel);
    free(pResponse->sContent);
    free(pResponse->sReasoningContent);
    free(pResponse->sFinishReason);
    free(pResponse->sRequestId);
    for ( i = 0u; i < pResponse->iToolCallCount; ++i ) { xllm__tool_call_unit(&pResponse->pToolCalls[i]); }
    free(pResponse->pToolCalls);
    free(pResponse);
}

void xllmFree(void* pMemory)
{
    free(pMemory);
}
