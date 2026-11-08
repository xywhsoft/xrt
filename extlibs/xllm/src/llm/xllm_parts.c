#include "../internal/xllm_internal.h"

/* ------------------------------------------------------------------ */
/* Parts                                                               */
/* ------------------------------------------------------------------ */

void xllmPartInit(xllm_part *pPart, xllm_part_kind eKind)
{
    if (!pPart)
    {
        return;
    }
    memset(pPart, 0, sizeof(*pPart));
    pPart->eKind = eKind;
}

static uint8_t *xllm__part_copy_bytes(const void *pData, size_t iSize)
{
    uint8_t *pCopy;
    if (!iSize)
        return NULL;
    if (!pData)
        return NULL;
    pCopy = (uint8_t *)xllm__malloc(iSize);
    if (!pCopy)
        return NULL;
    memcpy(pCopy, pData, iSize);
    return pCopy;
}

static bool xllm__part_set_data(xllm_part *pPart, xllm_part_kind eKind, const void *pData,
                                size_t iSize, const char *sMediaType)
{
    char *sMediaCopy;
    uint8_t *pDataCopy;
    if (!pPart || !sMediaType || !sMediaType[0] || (!pData && iSize))
    {
        return false;
    }
    sMediaCopy = xllm__strdup(sMediaType);
    if (!sMediaCopy)
        return false;
    pDataCopy = xllm__part_copy_bytes(pData, iSize);
    if (iSize && !pDataCopy)
    {
        xllm__free(sMediaCopy);
        return false;
    }
    xllm__free(pPart->sMediaType);
    xllm__free(pPart->pData);
    xllm__free(pPart->sSourceUrl);
    pPart->sMediaType = sMediaCopy;
    pPart->pData = pDataCopy;
    pPart->iDataSize = iSize;
    pPart->sSourceUrl = NULL;
    pPart->eKind = eKind;
    return true;
}

bool xllmPartSetText(xllm_part *pPart, const char *sText)
{
    char *sCopy;
    if (!pPart || !xllm__utf8_valid(sText))
    {
        return false;
    }
    sCopy = xllm__strdup(sText ? sText : "");
    if (!sCopy)
        return false;
    xllm__free(pPart->sText);
    pPart->sText = sCopy;
    pPart->eKind = XLLM_PART_TEXT;
    return true;
}

bool xllmPartSetImageData(xllm_part *pPart, const void *pData, size_t iSize, const char *sMediaType)
{
    return xllm__part_set_data(pPart, XLLM_PART_IMAGE, pData, iSize, sMediaType);
}

bool xllmPartSetImageUrl(xllm_part *pPart, const char *sUrl, const char *sMediaType)
{
    char *sUrlCopy;
    char *sMediaCopy = NULL;
    if (!pPart || !sUrl || !sUrl[0])
    {
        return false;
    }
    sUrlCopy = xllm__strdup(sUrl);
    if (!sUrlCopy)
        return false;
    if (sMediaType)
    {
        sMediaCopy = xllm__strdup(sMediaType);
        if (!sMediaCopy)
        {
            xllm__free(sUrlCopy);
            return false;
        }
    }
    xllm__free(pPart->sSourceUrl);
    pPart->sSourceUrl = sUrlCopy;
    if (sMediaType)
    {
        xllm__free(pPart->sMediaType);
        pPart->sMediaType = sMediaCopy;
    }
    xllm__free(pPart->pData);
    pPart->pData = NULL;
    pPart->iDataSize = 0u;
    pPart->eKind = XLLM_PART_IMAGE;
    return true;
}

bool xllmPartSetAudioData(xllm_part *pPart, const void *pData, size_t iSize, const char *sMediaType)
{
    return xllm__part_set_data(pPart, XLLM_PART_AUDIO, pData, iSize, sMediaType);
}

bool xllmPartSetFileData(xllm_part *pPart, const void *pData, size_t iSize, const char *sMediaType)
{
    return xllm__part_set_data(pPart, XLLM_PART_FILE, pData, iSize, sMediaType);
}

bool xllmPartSetNative(xllm_part *pPart, const char *sNativeType, const char *sJson)
{
    char *sTypeCopy;
    char *sJsonCopy;
    if (!pPart || !sJson || !sJson[0] || !xrtJsonValid((xstrview){sJson, strlen(sJson)}))
    {
        return false;
    }
    sTypeCopy = xllm__strdup(sNativeType ? sNativeType : "");
    if (!sTypeCopy)
        return false;
    sJsonCopy = xllm__strdup(sJson);
    if (!sJsonCopy)
    {
        xllm__free(sTypeCopy);
        return false;
    }
    xllm__free(pPart->sNativeType);
    xllm__free(pPart->sText);
    pPart->sNativeType = sTypeCopy;
    pPart->sText = sJsonCopy;
    pPart->eKind = XLLM_PART_NATIVE;
    return true;
}

void xllmPartUnit(xllm_part *pPart) { xllm__part_unit(pPart); }

void xllm__part_unit(xllm_part *pPart)
{
    if (!pPart)
    {
        return;
    }
    xllm__free(pPart->sText);
    xllm__free(pPart->sNativeType);
    xllm__free(pPart->sMediaType);
    xllm__free(pPart->sSourceUrl);
    xllm__free(pPart->sDetail);
    xllm__free(pPart->pData);
    memset(pPart, 0, sizeof(*pPart));
}

bool xllm__part_clone(xllm_part *pDst, const xllm_part *pSrc)
{
    if (!pDst || !pSrc)
    {
        return false;
    }
    memset(pDst, 0, sizeof(*pDst));
    pDst->eKind = pSrc->eKind;
    if (pSrc->sText && !xllm__replace(&pDst->sText, pSrc->sText))
        goto fail;
    if (pSrc->sNativeType && !xllm__replace(&pDst->sNativeType, pSrc->sNativeType))
        goto fail;
    if (pSrc->sMediaType && !xllm__replace(&pDst->sMediaType, pSrc->sMediaType))
        goto fail;
    if (pSrc->sSourceUrl && !xllm__replace(&pDst->sSourceUrl, pSrc->sSourceUrl))
        goto fail;
    if (pSrc->sDetail && !xllm__replace(&pDst->sDetail, pSrc->sDetail))
        goto fail;
    if (pSrc->pData && pSrc->iDataSize)
    {
        pDst->pData = (uint8_t *)xllm__malloc(pSrc->iDataSize);
        if (!pDst->pData)
        {
            goto fail;
        }
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

void xllmMessageInit(xllm_message *pMessage, xllm_role eRole)
{
    if (!pMessage)
    {
        return;
    }
    memset(pMessage, 0, sizeof(*pMessage));
    pMessage->eRole = eRole;
}

void xllmMessageUnit(xllm_message *pMessage)
{
    size_t i;
    if (!pMessage)
    {
        return;
    }
    xllm__free(pMessage->sContent);
    xllm__free(pMessage->sReasoningContent);
    xllm__free(pMessage->sToolCallId);
    xllm__free(pMessage->sNative);
    for (i = 0u; i < pMessage->iToolCallCount; ++i)
    {
        xllm__tool_call_unit(&pMessage->pToolCalls[i]);
    }
    xllm__free(pMessage->pToolCalls);
    for (i = 0u; i < pMessage->iPartCount; ++i)
    {
        xllm__part_unit(&pMessage->pParts[i]);
    }
    xllm__free(pMessage->pParts);
    memset(pMessage, 0, sizeof(*pMessage));
}

bool xllmMessageSetContent(xllm_message *pMessage, const char *sContent)
{
    size_t i;
    if (!pMessage || !xllm__utf8_valid(sContent))
    {
        return false;
    }
    if (!xllm__replace(&pMessage->sContent, sContent ? sContent : ""))
    {
        return false;
    }
    if (pMessage->pParts)
    {
        for (i = 0u; i < pMessage->iPartCount; ++i)
        {
            xllm__part_unit(&pMessage->pParts[i]);
        }
        pMessage->iPartCount = 0u;
    }
    return true;
}

bool xllmMessageSetReasoning(xllm_message *pMessage, const char *sReasoningContent)
{
    if (!xllm__utf8_valid(sReasoningContent))
    {
        return false;
    }
    return pMessage && xllm__replace(&pMessage->sReasoningContent, sReasoningContent);
}

bool xllmMessageSetToolCallId(xllm_message *pMessage, const char *sToolCallId)
{
    return pMessage && xllm__replace(&pMessage->sToolCallId, sToolCallId);
}

bool xllmMessageAddToolCall(xllm_message *pMessage, const char *sId, const char *sName,
                            const char *sArgumentsJson)
{
    xllm_tool_call *pNew;
    size_t iCap;
    xllm_tool_call *pCall;
    if (!pMessage || !sName || !sName[0])
    {
        return false;
    }
    if (pMessage->iToolCallCount == pMessage->iToolCallCap)
    {
        iCap = pMessage->iToolCallCap ? pMessage->iToolCallCap * 2u : 4u;
        pNew = (xllm_tool_call *)xllm__realloc(pMessage->pToolCalls, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pMessage->iToolCallCap, 0, sizeof(*pNew) * (iCap - pMessage->iToolCallCap));
        pMessage->pToolCalls = pNew;
        pMessage->iToolCallCap = iCap;
    }
    pCall = &pMessage->pToolCalls[pMessage->iToolCallCount];
    pCall->sId = xllm__strdup(sId ? sId : "");
    pCall->sName = xllm__strdup(sName);
    pCall->sArgumentsJson = xllm__strdup(sArgumentsJson ? sArgumentsJson : "{}");
    if (!pCall->sId || !pCall->sName || !pCall->sArgumentsJson)
    {
        xllm__tool_call_unit(pCall);
        return false;
    }
    ++pMessage->iToolCallCount;
    return true;
}

bool xllmMessageAddPart(xllm_message *pMessage, const xllm_part *pPart)
{
    xllm_part *pNew;
    size_t iCap;
    if (!pMessage || !pPart)
    {
        return false;
    }
    if (pMessage->iPartCount == pMessage->iPartCap)
    {
        iCap = pMessage->iPartCap ? pMessage->iPartCap * 2u : 4u;
        pNew = (xllm_part *)xllm__realloc(pMessage->pParts, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pMessage->iPartCap, 0, sizeof(*pNew) * (iCap - pMessage->iPartCap));
        pMessage->pParts = pNew;
        pMessage->iPartCap = iCap;
    }
    if (!xllm__part_clone(&pMessage->pParts[pMessage->iPartCount], pPart))
    {
        return false;
    }
    ++pMessage->iPartCount;
    /* Parts become authoritative: drop the text fast path. */
    if (pMessage->sContent)
    {
        xllm__free(pMessage->sContent);
        pMessage->sContent = NULL;
    }
    return true;
}

bool xllmMessageSetNative(xllm_message *pMessage, const char *sNativeJson)
{
    if (!pMessage || !sNativeJson || !sNativeJson[0] ||
        !xrtJsonValid((xstrview){sNativeJson, strlen(sNativeJson)}))
    {
        return false;
    }
    return xllm__replace(&pMessage->sNative, sNativeJson);
}

bool xllm__message_clone(xllm_message *pDst, const xllm_message *pSrc)
{
    size_t i;
    if (!pDst || !pSrc)
    {
        return false;
    }
    xllmMessageInit(pDst, pSrc->eRole);
    if (pSrc->sContent && !xllmMessageSetContent(pDst, pSrc->sContent))
        goto fail;
    if (pSrc->sReasoningContent && !xllmMessageSetReasoning(pDst, pSrc->sReasoningContent))
        goto fail;
    if (pSrc->sToolCallId && !xllmMessageSetToolCallId(pDst, pSrc->sToolCallId))
        goto fail;
    if (pSrc->sNative && !xllmMessageSetNative(pDst, pSrc->sNative))
        goto fail;
    for (i = 0u; i < pSrc->iToolCallCount; ++i)
    {
        const xllm_tool_call *pCall = &pSrc->pToolCalls[i];
        if (!xllmMessageAddToolCall(pDst, pCall->sId, pCall->sName, pCall->sArgumentsJson))
            goto fail;
    }
    for (i = 0u; i < pSrc->iPartCount; ++i)
    {
        if (!xllmMessageAddPart(pDst, &pSrc->pParts[i]))
            goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pDst);
    return false;
}

bool xllm__part_is_thinking_signature(const xllm_part *pPart)
{
    return pPart->eKind == XLLM_PART_NATIVE && pPart->sNativeType &&
           strcmp(pPart->sNativeType, "thinking_signature") == 0;
}

bool xllmMessageFromResponse(const xllm_response *pResponse, xllm_message *pMessage)
{
    size_t i;
    bool bSigned = false, bText = false;
    if (!pResponse || !pMessage || (pResponse->iBlockCount && !pResponse->pBlocks) ||
        (pResponse->iToolCallCount && !pResponse->pToolCalls))
    {
        return false;
    }
    xllmMessageInit(pMessage, XLLM_ROLE_ASSISTANT);
    if (pResponse->sReasoningContent &&
        !xllmMessageSetReasoning(pMessage, pResponse->sReasoningContent))
        goto fail;
    for (i = 0u; i < pResponse->iBlockCount; ++i)
    {
        const xllm_block *pBlock = &pResponse->pBlocks[i];
        if (pBlock->eKind == XLLM_BLOCK_REASONING && pBlock->sNative && pBlock->sNative[0])
            bSigned = true;
    }
    if (!bSigned)
    {
        /* Keep the compact representation for ordinary provider responses. */
        if (pResponse->sContent && !xllmMessageSetContent(pMessage, pResponse->sContent))
            goto fail;
    }
    else
    {
        /* Parts are authoritative. Keep each original text/thinking block in
         * order, pairing a signature with its immediately preceding reasoning
         * part. Never bind several signatures to the joined reasoning view. */
        for (i = 0u; i < pResponse->iBlockCount; ++i)
        {
            const xllm_block *pBlock = &pResponse->pBlocks[i];
            xllm_part tPart = {0}; /* AddPart owns a deep copy of these views. */
            if (pBlock->eKind != XLLM_BLOCK_TEXT && pBlock->eKind != XLLM_BLOCK_REASONING)
                continue;
            tPart.eKind = pBlock->eKind == XLLM_BLOCK_TEXT ? XLLM_PART_TEXT : XLLM_PART_REASONING;
            if (!xllm__utf8_valid(pBlock->sText)) goto fail;
            tPart.sText = pBlock->sText;
            if (!xllmMessageAddPart(pMessage, &tPart))
                goto fail;
            if (pBlock->eKind == XLLM_BLOCK_TEXT)
                bText = true;
            if (pBlock->eKind == XLLM_BLOCK_REASONING && pBlock->sNative && pBlock->sNative[0])
            {
                tPart.eKind = XLLM_PART_NATIVE;
                if (!xllm__utf8_valid(pBlock->sNative)) goto fail;
                tPart.sText = pBlock->sNative;
                tPart.sNativeType = "thinking_signature"; /* Opaque, not JSON. */
                if (!xllmMessageAddPart(pMessage, &tPart))
                    goto fail;
            }
        }
        /* Custom completed responses may only supply the joined text view. */
        if (!bText && pResponse->sContent)
        {
            xllm_part tPart = {0};
            tPart.eKind = XLLM_PART_TEXT;
            tPart.sText = pResponse->sContent;
            if (!xllmMessageAddPart(pMessage, &tPart))
                goto fail;
        }
    }
    for (i = 0u; i < pResponse->iToolCallCount; ++i)
    {
        const xllm_tool_call *pCall = &pResponse->pToolCalls[i];
        if (!xllmMessageAddToolCall(pMessage, pCall->sId, pCall->sName, pCall->sArgumentsJson))
            goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pMessage);
    return false;
}

/* ------------------------------------------------------------------ */
/* Requests                                                            */
/* ------------------------------------------------------------------ */

void xllm__tool_unit(xllm_tool *pTool)
{
    if (!pTool)
    {
        return;
    }
    xllm__free(pTool->sName);
    xllm__free(pTool->sDescription);
    xllm__free(pTool->sParametersJson);
    memset(pTool, 0, sizeof(*pTool));
}

void xllmRequestInit(xllm_request *pRequest)
{
    if (!pRequest)
    {
        return;
    }
    memset(pRequest, 0, sizeof(*pRequest));
    pRequest->bParallelToolCalls = true;
    pRequest->eToolChoice = XLLM_TOOL_CHOICE_AUTO;
    pRequest->eJsonMode = XLLM_JSON_NONE;
    pRequest->bStream = true;
    pRequest->uDeadline = UINT64_MAX;
    pRequest->iTimeout = XRT_WAIT_FOREVER;
}

void xllmRequestUnit(xllm_request *pRequest)
{
    size_t i;
    if (!pRequest)
    {
        return;
    }
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        if (pRequest->pbMessageBorrowed && pRequest->pbMessageBorrowed[i])
            continue;
        xllmMessageUnit(&pRequest->pMessages[i]);
    }
    for (i = 0u; i < pRequest->iToolCount; ++i)
    {
        if (pRequest->pbToolBorrowed && pRequest->pbToolBorrowed[i])
            continue;
        xllm__tool_unit(&pRequest->pTools[i]);
    }
    xllm__free(pRequest->pMessages);
    xllm__free(pRequest->pTools);
    xllm__free(pRequest->pbMessageBorrowed);
    xllm__free(pRequest->pbToolBorrowed);
    xllm__free(pRequest->sModel);
    xllm__free(pRequest->sReasoningEffort);
    xllm__free(pRequest->sNamedTool);
    xllm__free(pRequest->sStop);
    xllm__free(pRequest->sExtraBodyJson);
    memset(pRequest, 0, sizeof(*pRequest));
}

bool xllmRequestSetModel(xllm_request *pRequest, const char *sModel)
{
    return pRequest && xllm__replace(&pRequest->sModel, sModel);
}

bool xllmRequestSetReasoningEffort(xllm_request *pRequest, const char *sEffort)
{
    return pRequest && xllm__replace(&pRequest->sReasoningEffort, sEffort);
}

bool xllmRequestSetStop(xllm_request *pRequest, const char *sStop)
{
    return pRequest && xllm__replace(&pRequest->sStop, sStop ? sStop : "");
}

bool xllmRequestSetExtraBody(xllm_request *pRequest, const char *sJsonObject)
{
    if (!pRequest)
    {
        return false;
    }
    if (!sJsonObject || !sJsonObject[0])
    {
        return xllm__replace(&pRequest->sExtraBodyJson, NULL);
    }
    if (!xrtJsonValid((xstrview){sJsonObject, strlen(sJsonObject)}))
    {
        return false;
    }
    return xllm__replace(&pRequest->sExtraBodyJson, sJsonObject);
}

void xllmRequestSetCancel(xllm_request *pRequest, xcancel *pCancel)
{
    if (pRequest)
    {
        pRequest->pCancel = pCancel;
    }
}

void xllmRequestSetDeadline(xllm_request *pRequest, uint64_t uDeadline)
{
    if (pRequest)
    {
        pRequest->uDeadline = uDeadline;
        pRequest->iTimeout = XRT_WAIT_FOREVER;
    }
}

bool xllmRequestSetToolChoice(xllm_request *pRequest, xllm_tool_choice eChoice,
                              const char *sNamedTool)
{
    if (!pRequest || eChoice < XLLM_TOOL_CHOICE_AUTO || eChoice > XLLM_TOOL_CHOICE_NAMED)
    {
        return false;
    }
    if (eChoice == XLLM_TOOL_CHOICE_NAMED && (!sNamedTool || !sNamedTool[0]))
    {
        return false;
    }
    if (!xllm__replace(&pRequest->sNamedTool,
                       eChoice == XLLM_TOOL_CHOICE_NAMED ? sNamedTool : NULL))
    {
        return false;
    }
    pRequest->eToolChoice = eChoice;
    return true;
}

bool xllmRequestAddMessage(xllm_request *pRequest, const xllm_message *pMessage)
{
    xllm_message *pNew;
    bool *pbNew;
    size_t iCap;
    if (!pRequest || !pMessage)
    {
        return false;
    }
    if (pRequest->iMessageCount == pRequest->iMessageCap)
    {
        iCap = pRequest->iMessageCap ? pRequest->iMessageCap * 2u : 8u;
        pNew = (xllm_message *)xllm__realloc(pRequest->pMessages, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pRequest->iMessageCap, 0, sizeof(*pNew) * (iCap - pRequest->iMessageCap));
        pRequest->pMessages = pNew;
        if (pRequest->pbMessageBorrowed)
        {
            pbNew = (bool *)xllm__realloc(pRequest->pbMessageBorrowed, sizeof(bool) * iCap);
            if (!pbNew)
            {
                return false;
            }
            pRequest->pbMessageBorrowed = pbNew;
        }
        pRequest->iMessageCap = iCap;
    }
    if (pRequest->pbMessageBorrowed)
    {
        pRequest->pbMessageBorrowed[pRequest->iMessageCount] = false;
    }
    if (!xllm__message_clone(&pRequest->pMessages[pRequest->iMessageCount], pMessage))
    {
        return false;
    }
    ++pRequest->iMessageCount;
    return true;
}

bool xllmRequestAddMessageView(xllm_request *pRequest, const xllm_message *pMessage)
{
    xllm_message *pNew;
    bool *pbNew;
    size_t iCap;
    if (!pRequest || !pMessage)
    {
        return false;
    }
    if (pRequest->iMessageCount == pRequest->iMessageCap)
    {
        iCap = pRequest->iMessageCap ? pRequest->iMessageCap * 2u : 8u;
        pNew = (xllm_message *)xllm__realloc(pRequest->pMessages, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pRequest->iMessageCap, 0, sizeof(*pNew) * (iCap - pRequest->iMessageCap));
        pRequest->pMessages = pNew;
        if (pRequest->pbMessageBorrowed)
        {
            /* The bitmap must track the array capacity exactly. */
            pbNew = (bool *)xllm__realloc(pRequest->pbMessageBorrowed, sizeof(bool) * iCap);
            if (!pbNew)
            {
                return false;
            }
            pRequest->pbMessageBorrowed = pbNew;
        }
        pRequest->iMessageCap = iCap;
    }
    if (!pRequest->pbMessageBorrowed)
    {
        /* Lazy parallel bitmap; existing entries are all owned. */
        pbNew = (bool *)xllm__calloc(1u, sizeof(bool) * pRequest->iMessageCap);
        if (!pbNew)
        {
            return false;
        }
        memset(pbNew, 0, sizeof(bool) * pRequest->iMessageCap);
        pRequest->pbMessageBorrowed = pbNew;
    }
    pRequest->pMessages[pRequest->iMessageCount] = *pMessage; /* shallow */
    pRequest->pbMessageBorrowed[pRequest->iMessageCount] = true;
    ++pRequest->iMessageCount;
    return true;
}

bool xllmRequestSetToolsView(xllm_request *pRequest, const xllm_tool *pTools, size_t iCount)
{
    xllm_tool *pNew = NULL;
    bool *pbNew = NULL;
    size_t i;
    if (!pRequest || (!pTools && iCount))
    {
        return false;
    }
    if (iCount > SIZE_MAX / sizeof(*pNew) || iCount > SIZE_MAX / sizeof(*pbNew))
        return false;
    if (iCount)
    {
        pNew = (xllm_tool *)xllm__malloc(sizeof(*pNew) * iCount);
        if (!pNew)
            return false;
        pbNew = (bool *)xllm__malloc(sizeof(*pbNew) * iCount);
        if (!pbNew)
        {
            xllm__free(pNew);
            return false;
        }
        memcpy(pNew, pTools, sizeof(*pTools) * iCount); /* shallow */
        memset(pbNew, 1, sizeof(*pbNew) * iCount);
    }
    /* Commit only after the complete replacement is ready. */
    for (i = 0u; i < pRequest->iToolCount; ++i)
    {
        if (pRequest->pbToolBorrowed && pRequest->pbToolBorrowed[i])
            continue;
        xllm__tool_unit(&pRequest->pTools[i]);
    }
    xllm__free(pRequest->pTools);
    xllm__free(pRequest->pbToolBorrowed);
    pRequest->pTools = pNew;
    pRequest->pbToolBorrowed = pbNew;
    pRequest->iToolCount = iCount;
    pRequest->iToolCap = iCount;
    return true;
}

bool xllmRequestAddTextMessage(xllm_request *pRequest, xllm_role eRole, const char *sContent)
{
    xllm_message tMessage;
    bool bOk;
    if (!pRequest || eRole == XLLM_ROLE_TOOL)
    {
        return false;
    }
    xllmMessageInit(&tMessage, eRole);
    bOk = xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
          xllmRequestAddMessage(pRequest, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmRequestAddToolResult(xllm_request *pRequest, const char *sToolCallId, const char *sContent)
{
    xllm_message tMessage;
    bool bOk;
    if (!pRequest || !sToolCallId || !sToolCallId[0])
    {
        return false;
    }
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    bOk = xllmMessageSetToolCallId(&tMessage, sToolCallId) &&
          xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
          xllmRequestAddMessage(pRequest, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmRequestAddTool(xllm_request *pRequest, const char *sName, const char *sDescription,
                        const char *sParametersJson, bool bStrict)
{
    xllm_tool *pNew;
    xllm_tool *pTool;
    size_t iCap;
    const char *sSchema =
        sParametersJson ? sParametersJson : "{\"type\":\"object\",\"properties\":{}}";
    if (!pRequest || !sName || !sName[0])
    {
        return false;
    }
    if (pRequest->iToolCount == pRequest->iToolCap)
    {
        iCap = pRequest->iToolCap ? pRequest->iToolCap * 2u : 8u;
        pNew = (xllm_tool *)xllm__realloc(pRequest->pTools, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pRequest->iToolCap, 0, sizeof(*pNew) * (iCap - pRequest->iToolCap));
        pRequest->pTools = pNew;
        if (pRequest->pbToolBorrowed)
        {
            bool *pbNew = (bool *)xllm__realloc(pRequest->pbToolBorrowed, sizeof(bool) * iCap);
            if (!pbNew)
            {
                return false;
            }
            pRequest->pbToolBorrowed = pbNew;
        }
        pRequest->iToolCap = iCap;
    }
    if (pRequest->pbToolBorrowed)
    {
        pRequest->pbToolBorrowed[pRequest->iToolCount] = false;
    }
    pTool = &pRequest->pTools[pRequest->iToolCount];
    pTool->sName = xllm__strdup(sName);
    pTool->sDescription = xllm__strdup(sDescription ? sDescription : "");
    pTool->sParametersJson = xllm__strdup(sSchema);
    pTool->bStrict = bStrict;
    if (!pTool->sName || !pTool->sDescription || !pTool->sParametersJson)
    {
        xllm__tool_unit(pTool);
        return false;
    }
    ++pRequest->iToolCount;
    return true;
}

uint64_t xllmEstimateTextTokens(const char *sText)
{
    const unsigned char *p = (const unsigned char *)sText;
    uint64_t uAscii = 0u;
    uint64_t uNonAscii = 0u;
    if (!p)
    {
        return 0u;
    }
    while (*p)
    {
        if (*p < 0x80u)
        {
            ++uAscii;
            ++p;
        }
        else
        {
            ++uNonAscii;
            if ((*p & 0xE0u) == 0xC0u && p[1])
            {
                p += 2;
            }
            else if ((*p & 0xF0u) == 0xE0u && p[1] && p[2])
            {
                p += 3;
            }
            else if ((*p & 0xF8u) == 0xF0u && p[1] && p[2] && p[3])
            {
                p += 4;
            }
            else
            {
                ++p;
            }
        }
    }
    return (uAscii + 3u) / 4u + uNonAscii;
}

uint64_t xllmEstimateMessageTokens(const xllm_message *pMessage)
{
    uint64_t uTokens = 12u;
    size_t i;
    if (!pMessage)
    {
        return 0u;
    }
    uTokens += xllmEstimateTextTokens(pMessage->sContent);
    uTokens += xllmEstimateTextTokens(pMessage->sReasoningContent);
    uTokens += xllmEstimateTextTokens(pMessage->sToolCallId);
    for (i = 0u; i < pMessage->iPartCount; ++i)
    {
        const xllm_part *pPart = &pMessage->pParts[i];
        if (pPart->eKind == XLLM_PART_IMAGE)
            uTokens += 4096u;
        else
            uTokens += xllmEstimateTextTokens(pPart->sText);
    }
    /* Native replay is opaque; retain a bounded text estimate rather than
     * silently treating a structured message as zero input. */
    uTokens += xllmEstimateTextTokens(pMessage->sNative);
    for (i = 0u; i < pMessage->iToolCallCount; ++i)
    {
        const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
        uTokens += 16u + xllmEstimateTextTokens(pCall->sId) + xllmEstimateTextTokens(pCall->sName) +
                   xllmEstimateTextTokens(pCall->sArgumentsJson);
    }
    return uTokens;
}

/* ------------------------------------------------------------------ */
/* Response destruction                                                */
/* ------------------------------------------------------------------ */

void xllmResponseDestroy(xllm_response *pResponse)
{
    size_t i;
    if (!pResponse)
    {
        return;
    }
    xllm__free(pResponse->sId);
    xllm__free(pResponse->sModel);
    xllm__free(pResponse->sContent);
    xllm__free(pResponse->sReasoningContent);
    xllm__free(pResponse->sRefusal);
    xllm__free(pResponse->sFinishReason);
    xllm__free(pResponse->sRequestId);
    for (i = 0u; i < pResponse->iBlockCount; ++i)
    {
        xllm__free(pResponse->pBlocks[i].sText);
        xllm__free(pResponse->pBlocks[i].sNative);
    }
    xllm__free(pResponse->pBlocks);
    for (i = 0u; i < pResponse->iToolCallCount; ++i)
    {
        xllm__tool_call_unit(&pResponse->pToolCalls[i]);
    }
    xllm__free(pResponse->pToolCalls);
    xllm__free(pResponse);
}

/* Relative budgets begin at submission, not when a reusable request is set up. */
void xllmRequestSetTimeout(xllm_request *request, int64_t timeout)
{
    if (request)
    {
        request->iTimeout = timeout;
        request->uDeadline = XLLM_DEADLINE_NEVER;
    }
}
