#include "xllm_internal.h"

/* ------------------------------------------------------------------ */
/* Parts                                                               */
/* ------------------------------------------------------------------ */

void xllmPartInit(xllm_part* pPart, xllm_part_kind eKind)
{
    if ( !pPart ) { return; }
    memset(pPart, 0, sizeof(*pPart));
    pPart->eKind = eKind;
}

static bool xllm__part_take_bytes(xllm_part* pPart, const void* pData, size_t iSize)
{
    uint8_t* pCopy;
    if ( !pPart ) { return false; }
    if ( !pData || !iSize ) {
        pPart->pData = NULL;
        pPart->iDataSize = 0u;
        return true;
    }
    pCopy = (uint8_t*)xllm__malloc(iSize);
    if ( !pCopy ) { return false; }
    memcpy(pCopy, pData, iSize);
    xllm__free(pPart->pData);
    pPart->pData = pCopy;
    pPart->iDataSize = iSize;
    return true;
}

bool xllmPartSetText(xllm_part* pPart, const char* sText)
{
    if ( !pPart || !xllm__utf8_valid(sText) ) { return false; }
    pPart->eKind = XLLM_PART_TEXT;
    return xllm__replace(&pPart->sText, sText ? sText : "");
}

bool xllmPartSetImageData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType)
{
    if ( !pPart || !sMediaType || !sMediaType[0] ) { return false; }
    pPart->eKind = XLLM_PART_IMAGE;
    if ( !xllm__replace(&pPart->sMediaType, sMediaType) ||
         !xllm__part_take_bytes(pPart, pData, iSize) ) { return false; }
    xllm__replace(&pPart->sSourceUrl, NULL);
    return true;
}

bool xllmPartSetImageUrl(xllm_part* pPart, const char* sUrl, const char* sMediaType)
{
    if ( !pPart || !sUrl || !sUrl[0] ) { return false; }
    pPart->eKind = XLLM_PART_IMAGE;
    if ( !xllm__replace(&pPart->sSourceUrl, sUrl) ) { return false; }
    if ( sMediaType && !xllm__replace(&pPart->sMediaType, sMediaType) ) { return false; }
    xllm__free(pPart->pData);
    pPart->pData = NULL;
    pPart->iDataSize = 0u;
    return true;
}

bool xllmPartSetAudioData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType)
{
    if ( !pPart || !sMediaType || !sMediaType[0] ) { return false; }
    pPart->eKind = XLLM_PART_AUDIO;
    if ( !xllm__replace(&pPart->sMediaType, sMediaType) ||
         !xllm__part_take_bytes(pPart, pData, iSize) ) { return false; }
    xllm__replace(&pPart->sSourceUrl, NULL);
    return true;
}

bool xllmPartSetFileData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType)
{
    if ( !pPart || !sMediaType || !sMediaType[0] ) { return false; }
    pPart->eKind = XLLM_PART_FILE;
    if ( !xllm__replace(&pPart->sMediaType, sMediaType) ||
         !xllm__part_take_bytes(pPart, pData, iSize) ) { return false; }
    xllm__replace(&pPart->sSourceUrl, NULL);
    return true;
}

bool xllmPartSetNative(xllm_part* pPart, const char* sNativeType, const char* sJson)
{
    if ( !pPart || !sJson || !sJson[0] ||
         !xrtJsonValid((xstrview){ sJson, strlen(sJson) }) ) { return false; }
    pPart->eKind = XLLM_PART_NATIVE;
    if ( !xllm__replace(&pPart->sNativeType, sNativeType ? sNativeType : "") ||
         !xllm__replace(&pPart->sText, sJson) ) { return false; }
    return true;
}

void xllm__part_unit(xllm_part* pPart)
{
    if ( !pPart ) { return; }
    xllm__free(pPart->sText);
    xllm__free(pPart->sNativeType);
    xllm__free(pPart->sMediaType);
    xllm__free(pPart->sSourceUrl);
    xllm__free(pPart->sDetail);
    xllm__free(pPart->pData);
    memset(pPart, 0, sizeof(*pPart));
}

bool xllm__part_clone(xllm_part* pDst, const xllm_part* pSrc)
{
    if ( !pDst || !pSrc ) { return false; }
    memset(pDst, 0, sizeof(*pDst));
    pDst->eKind = pSrc->eKind;
    if ( pSrc->sText && !xllm__replace(&pDst->sText, pSrc->sText) ) goto fail;
    if ( pSrc->sNativeType && !xllm__replace(&pDst->sNativeType, pSrc->sNativeType) ) goto fail;
    if ( pSrc->sMediaType && !xllm__replace(&pDst->sMediaType, pSrc->sMediaType) ) goto fail;
    if ( pSrc->sSourceUrl && !xllm__replace(&pDst->sSourceUrl, pSrc->sSourceUrl) ) goto fail;
    if ( pSrc->sDetail && !xllm__replace(&pDst->sDetail, pSrc->sDetail) ) goto fail;
    if ( pSrc->pData && pSrc->iDataSize ) {
        pDst->pData = (uint8_t*)xllm__malloc(pSrc->iDataSize);
        if ( !pDst->pData ) { goto fail; }
        memcpy(pDst->pData, pSrc->pData, pSrc->iDataSize);
        pDst->iDataSize = pSrc->iDataSize;
    }
    return true;
fail:
    xllm__part_unit(pDst);
    return false;
}

/* ------------------------------------------------------------------ */
/* Messages                                                            */
/* ------------------------------------------------------------------ */

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
    xllm__free(pMessage->sContent);
    xllm__free(pMessage->sReasoningContent);
    xllm__free(pMessage->sToolCallId);
    xllm__free(pMessage->sNative);
    for ( i = 0u; i < pMessage->iToolCallCount; ++i ) { xllm__tool_call_unit(&pMessage->pToolCalls[i]); }
    xllm__free(pMessage->pToolCalls);
    for ( i = 0u; i < pMessage->iPartCount; ++i ) { xllm__part_unit(&pMessage->pParts[i]); }
    xllm__free(pMessage->pParts);
    memset(pMessage, 0, sizeof(*pMessage));
}

bool xllmMessageSetContent(xllm_message* pMessage, const char* sContent)
{
    size_t i;
    if ( !pMessage || !xllm__utf8_valid(sContent) ) { return false; }
    if ( pMessage->pParts ) {
        for ( i = 0u; i < pMessage->iPartCount; ++i ) { xllm__part_unit(&pMessage->pParts[i]); }
        pMessage->iPartCount = 0u;
    }
    return xllm__replace(&pMessage->sContent, sContent ? sContent : "");
}

bool xllmMessageSetReasoning(xllm_message* pMessage, const char* sReasoningContent)
{
    if ( !xllm__utf8_valid(sReasoningContent) ) { return false; }
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
        pNew = (xllm_tool_call*)xllm__realloc(pMessage->pToolCalls, sizeof(*pNew) * iCap);
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

bool xllmMessageAddPart(xllm_message* pMessage, const xllm_part* pPart)
{
    xllm_part* pNew;
    size_t iCap;
    if ( !pMessage || !pPart ) { return false; }
    if ( pMessage->iPartCount == pMessage->iPartCap ) {
        iCap = pMessage->iPartCap ? pMessage->iPartCap * 2u : 4u;
        pNew = (xllm_part*)xllm__realloc(pMessage->pParts, sizeof(*pNew) * iCap);
        if ( !pNew ) { return false; }
        memset(pNew + pMessage->iPartCap, 0, sizeof(*pNew) * (iCap - pMessage->iPartCap));
        pMessage->pParts = pNew;
        pMessage->iPartCap = iCap;
    }
    if ( !xllm__part_clone(&pMessage->pParts[pMessage->iPartCount], pPart) ) { return false; }
    ++pMessage->iPartCount;
    /* Parts become authoritative: drop the text fast path. */
    if ( pMessage->sContent ) {
        xllm__free(pMessage->sContent);
        pMessage->sContent = NULL;
    }
    return true;
}

bool xllmMessageSetNative(xllm_message* pMessage, const char* sNativeJson)
{
    if ( !pMessage || !sNativeJson || !sNativeJson[0] ||
         !xrtJsonValid((xstrview){ sNativeJson, strlen(sNativeJson) }) ) { return false; }
    return xllm__replace(&pMessage->sNative, sNativeJson);
}

bool xllm__message_clone(xllm_message* pDst, const xllm_message* pSrc)
{
    size_t i;
    if ( !pDst || !pSrc ) { return false; }
    xllmMessageInit(pDst, pSrc->eRole);
    if ( pSrc->sContent && !xllmMessageSetContent(pDst, pSrc->sContent) ) goto fail;
    if ( pSrc->sReasoningContent && !xllmMessageSetReasoning(pDst, pSrc->sReasoningContent) ) goto fail;
    if ( pSrc->sToolCallId && !xllmMessageSetToolCallId(pDst, pSrc->sToolCallId) ) goto fail;
    if ( pSrc->sNative && !xllmMessageSetNative(pDst, pSrc->sNative) ) goto fail;
    for ( i = 0u; i < pSrc->iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pSrc->pToolCalls[i];
        if ( !xllmMessageAddToolCall(pDst, pCall->sId, pCall->sName, pCall->sArgumentsJson) ) goto fail;
    }
    for ( i = 0u; i < pSrc->iPartCount; ++i ) {
        if ( !xllmMessageAddPart(pDst, &pSrc->pParts[i]) ) goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pDst);
    return false;
}

bool xllmMessageFromResponse(const xllm_response* pResponse, xllm_message* pMessage)
{
    size_t i;
    if ( !pResponse || !pMessage ) { return false; }
    xllmMessageInit(pMessage, XLLM_ROLE_ASSISTANT);
    if ( pResponse->sContent && !xllmMessageSetContent(pMessage, pResponse->sContent) ) goto fail;
    if ( pResponse->sReasoningContent && !xllmMessageSetReasoning(pMessage, pResponse->sReasoningContent) ) goto fail;
    for ( i = 0u; i < pResponse->iBlockCount; ++i ) {
        const xllm_block* pBlock = &pResponse->pBlocks[i];
        if ( pBlock->eKind == XLLM_BLOCK_REASONING && pBlock->sNative && pBlock->sNative[0] ) {
            /* The signature is an opaque provider token, not JSON: build the
             * NATIVE part fields directly instead of xllmPartSetNative. */
            xllm_part tPart;
            bool bOk;
            xllmPartInit(&tPart, XLLM_PART_NATIVE);
            bOk = xllm__replace(&tPart.sNativeType, "thinking_signature") &&
                xllm__replace(&tPart.sText, pBlock->sNative) &&
                xllmMessageAddPart(pMessage, &tPart);
            xllm__part_unit(&tPart);
            if ( !bOk ) { goto fail; }
        }
    }
    for ( i = 0u; i < pResponse->iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pResponse->pToolCalls[i];
        if ( !xllmMessageAddToolCall(pMessage, pCall->sId, pCall->sName, pCall->sArgumentsJson) ) goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pMessage);
    return false;
}

/* ------------------------------------------------------------------ */
/* Requests                                                            */
/* ------------------------------------------------------------------ */

void xllm__tool_unit(xllm_tool* pTool)
{
    if ( !pTool ) { return; }
    xllm__free(pTool->sName);
    xllm__free(pTool->sDescription);
    xllm__free(pTool->sParametersJson);
    memset(pTool, 0, sizeof(*pTool));
}

void xllmRequestInit(xllm_request* pRequest)
{
    if ( !pRequest ) { return; }
    memset(pRequest, 0, sizeof(*pRequest));
    pRequest->bParallelToolCalls = true;
    pRequest->eToolChoice = XLLM_TOOL_CHOICE_AUTO;
    pRequest->eJsonMode = XLLM_JSON_NONE;
    pRequest->bStream = true;
    pRequest->uDeadline = UINT64_MAX;
}

void xllmRequestUnit(xllm_request* pRequest)
{
    size_t i;
    if ( !pRequest ) { return; }
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) { xllmMessageUnit(&pRequest->pMessages[i]); }
    for ( i = 0u; i < pRequest->iToolCount; ++i ) { xllm__tool_unit(&pRequest->pTools[i]); }
    xllm__free(pRequest->pMessages);
    xllm__free(pRequest->pTools);
    xllm__free(pRequest->sModel);
    xllm__free(pRequest->sReasoningEffort);
    xllm__free(pRequest->sNamedTool);
    xllm__free(pRequest->sStop);
    xllm__free(pRequest->sExtraBodyJson);
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

bool xllmRequestSetStop(xllm_request* pRequest, const char* sStop)
{
    return pRequest && xllm__replace(&pRequest->sStop, sStop ? sStop : "");
}

bool xllmRequestSetExtraBody(xllm_request* pRequest, const char* sJsonObject)
{
    if ( !pRequest ) { return false; }
    if ( !sJsonObject || !sJsonObject[0] ) {
        return xllm__replace(&pRequest->sExtraBodyJson, NULL);
    }
    if ( !xrtJsonValid((xstrview){ sJsonObject, strlen(sJsonObject) }) ) { return false; }
    return xllm__replace(&pRequest->sExtraBodyJson, sJsonObject);
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
        pNew = (xllm_message*)xllm__realloc(pRequest->pMessages, sizeof(*pNew) * iCap);
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
        pNew = (xllm_tool*)xllm__realloc(pRequest->pTools, sizeof(*pNew) * iCap);
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

uint64_t xllmEstimateTextTokens(const char* sText)
{
    const unsigned char* p = (const unsigned char*)sText;
    uint64_t uAscii = 0u;
    uint64_t uNonAscii = 0u;
    if ( !p ) { return 0u; }
    while ( *p ) {
        if ( *p < 0x80u ) {
            ++uAscii;
            ++p;
        } else {
            ++uNonAscii;
            if ( (*p & 0xE0u) == 0xC0u && p[1] ) { p += 2; }
            else if ( (*p & 0xF0u) == 0xE0u && p[1] && p[2] ) { p += 3; }
            else if ( (*p & 0xF8u) == 0xF0u && p[1] && p[2] && p[3] ) { p += 4; }
            else { ++p; }
        }
    }
    return (uAscii + 3u) / 4u + uNonAscii;
}

uint64_t xllmEstimateMessageTokens(const xllm_message* pMessage)
{
    uint64_t uTokens = 12u;
    size_t i;
    if ( !pMessage ) { return 0u; }
    uTokens += xllmEstimateTextTokens(pMessage->sContent);
    uTokens += xllmEstimateTextTokens(pMessage->sReasoningContent);
    uTokens += xllmEstimateTextTokens(pMessage->sToolCallId);
    for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
        const xllm_tool_call* pCall = &pMessage->pToolCalls[i];
        uTokens += 16u + xllmEstimateTextTokens(pCall->sId) +
            xllmEstimateTextTokens(pCall->sName) + xllmEstimateTextTokens(pCall->sArgumentsJson);
    }
    return uTokens;
}

/* ------------------------------------------------------------------ */
/* Response destruction                                                */
/* ------------------------------------------------------------------ */

void xllmResponseDestroy(xllm_response* pResponse)
{
    size_t i;
    if ( !pResponse ) { return; }
    xllm__free(pResponse->sId);
    xllm__free(pResponse->sModel);
    xllm__free(pResponse->sContent);
    xllm__free(pResponse->sReasoningContent);
    xllm__free(pResponse->sRefusal);
    xllm__free(pResponse->sFinishReason);
    xllm__free(pResponse->sRequestId);
    for ( i = 0u; i < pResponse->iBlockCount; ++i ) {
        xllm__free(pResponse->pBlocks[i].sText);
        xllm__free(pResponse->pBlocks[i].sNative);
    }
    xllm__free(pResponse->pBlocks);
    for ( i = 0u; i < pResponse->iToolCallCount; ++i ) { xllm__tool_call_unit(&pResponse->pToolCalls[i]); }
    xllm__free(pResponse->pToolCalls);
    xllm__free(pResponse);
}
