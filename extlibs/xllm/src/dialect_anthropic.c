#include "xllm_internal.h"

/* Anthropic Messages dialect.
 *
 * Request: system messages lift to the top-level system field, tool results
 * ride user messages as tool_result blocks, assistant tool calls become
 * tool_use blocks, and max_tokens is always sent (the endpoint requires it).
 * Streaming: typed events (message_start / content_block_start / _delta /
 * _stop / message_delta / message_stop / error / ping) are decoded through
 * the framing layer's event-name field. Thinking signatures are captured
 * into the reasoning block's native blob and replayed as thinking blocks
 * when history carries a thinking_signature NATIVE part. */

static size_t xllm__anthropic_build_auth(const xllm_client* pClient,
    xllm_auth_header* pOut, size_t iCap)
{
    size_t iKeyLen;
    if ( !pClient || !pOut || iCap < 2u ) { return 0u; }
    memset(pOut, 0, sizeof(*pOut) * 2u);
    if ( !pClient->sApiKey[0] ) {
        xllm__copy_text(pOut[0].sName, sizeof(pOut[0].sName), "anthropic-version");
        pOut[0].sValue = xllm__strdup("2023-06-01");
        return pOut[0].sValue ? 1u : 0u;
    }
    iKeyLen = strlen(pClient->sApiKey);
    pOut[0].sValue = (char*)xllm__malloc(iKeyLen + 1u);
    pOut[1].sValue = (char*)xllm__malloc(16u);
    if ( !pOut[0].sValue || !pOut[1].sValue ) {
        xllm__free(pOut[0].sValue);
        xllm__free(pOut[1].sValue);
        pOut[0].sValue = NULL;
        pOut[1].sValue = NULL;
        return 0u;
    }
    memcpy(pOut[0].sValue, pClient->sApiKey, iKeyLen + 1u);
    memcpy(pOut[1].sValue, "2023-06-01", 11u);
    xllm__copy_text(pOut[0].sName, sizeof(pOut[0].sName), "x-api-key");
    xllm__copy_text(pOut[1].sName, sizeof(pOut[1].sName), "anthropic-version");
    return 2u;
}

static bool xllm__anthropic_append_source_b64(xllm_buf* pBuf, const xllm_part* pPart)
{
    str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
    bool bOk;
    if ( !sEncoded ) { return false; }
    bOk = xllm__buf_append_cstr(pBuf, "{\"type\":\"base64\",\"media_type\":") &&
        xllm__json_string(pBuf, pPart->sMediaType ? pPart->sMediaType : "application/octet-stream") &&
        xllm__buf_append_cstr(pBuf, ",\"data\":") &&
        xllm__json_string(pBuf, sEncoded) &&
        xllm__buf_append_char(pBuf, '}');
    xrtFree(sEncoded);
    return bOk;
}

static bool xllm__anthropic_append_part(xllm_buf* pBuf, const xllm_part* pPart, xllm_error* pError)
{
    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"text\",\"text\":") ||
                 !xllm__json_string(pBuf, pPart->sText ? pPart->sText : "") ||
                 !xllm__buf_append_char(pBuf, '}') ) { return false; }
            return true;
        case XLLM_PART_IMAGE:
            if ( pPart->sSourceUrl && pPart->sSourceUrl[0] ) {
                if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":") ||
                     !xllm__json_string(pBuf, pPart->sSourceUrl) ||
                     !xllm__buf_append_cstr(pBuf, "}}") ) { return false; }
                return true;
            }
            if ( !pPart->pData || !pPart->iDataSize ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "image part has neither bytes nor URL");
                return false;
            }
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"image\",\"source\":") ||
                 !xllm__anthropic_append_source_b64(pBuf, pPart) ||
                 !xllm__buf_append_char(pBuf, '}') ) { return false; }
            return true;
        case XLLM_PART_AUDIO:
            if ( !pPart->pData || !pPart->iDataSize ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "audio part requires bytes");
                return false;
            }
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"audio\",\"source\":") ||
                 !xllm__anthropic_append_source_b64(pBuf, pPart) ||
                 !xllm__buf_append_char(pBuf, '}') ) { return false; }
            return true;
        case XLLM_PART_FILE:
            if ( !pPart->pData || !pPart->iDataSize ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "file part requires bytes");
                return false;
            }
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"document\",\"source\":") ||
                 !xllm__anthropic_append_source_b64(pBuf, pPart) ||
                 !xllm__buf_append_char(pBuf, '}') ) { return false; }
            return true;
        case XLLM_PART_NATIVE:
            return xllm__buf_append_cstr(pBuf, pPart->sText ? pPart->sText : "{}");
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "message part has an unsupported kind");
            return false;
    }
}

/* Message content: plain string for a single text part, block array else. */
static bool xllm__anthropic_append_content(xllm_buf* pBuf, const xllm_message* pMessage,
    bool bToolResult, xllm_error* pError)
{
    size_t i;
    size_t iBlocks = 0u;
    if ( pMessage->iPartCount == 0u ) {
        return xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : "");
    }
    if ( pMessage->iPartCount == 1u && pMessage->pParts[0].eKind == XLLM_PART_TEXT &&
         !bToolResult ) {
        return xllm__json_string(pBuf, pMessage->pParts[0].sText ? pMessage->pParts[0].sText : "");
    }
    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        if ( pMessage->pParts[i].eKind == XLLM_PART_REASONING ) { continue; }
        ++iBlocks;
    }
    if ( !iBlocks ) { return xllm__json_string(pBuf, ""); }
    if ( !xllm__buf_append_char(pBuf, '[') ) { return false; }
    {
        bool bFirst = true;
        for ( i = 0u; i < pMessage->iPartCount; ++i ) {
            if ( pMessage->pParts[i].eKind == XLLM_PART_REASONING ) { continue; }
            if ( !bFirst && !xllm__buf_append_char(pBuf, ',') ) { return false; }
            if ( !xllm__anthropic_append_part(pBuf, &pMessage->pParts[i], pError) ) { return false; }
            bFirst = false;
        }
    }
    return xllm__buf_append_char(pBuf, ']');
}

typedef struct xllm__anthropic_writer {
    xllm_buf* pBody;
    bool bAnyMessage;
    bool bPendingToolUser;
    xllm_error* pError;
} xllm__anthropic_writer;

static bool xllm__anthropic_message_open(xllm__anthropic_writer* pWriter, const char* sRole)
{
    if ( pWriter->bAnyMessage && !xllm__buf_append_char(pWriter->pBody, ',') ) { return false; }
    if ( !xllm__buf_append_cstr(pWriter->pBody, "{\"role\":") ||
         !xllm__json_string(pWriter->pBody, sRole) ||
         !xllm__buf_append_cstr(pWriter->pBody, ",\"content\":") ) { return false; }
    pWriter->bAnyMessage = true;
    return true;
}

static bool xllm__anthropic_append_message(xllm__anthropic_writer* pWriter, const xllm_message* pMessage)
{
    xllm_buf* pBody = pWriter->pBody;
    size_t i;
    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        /* Tool results accumulate into a shared user message. */
        if ( !pWriter->bPendingToolUser ) {
            if ( !xllm__anthropic_message_open(pWriter, "user") ||
                 !xllm__buf_append_char(pBody, '[') ) { return false; }
            pWriter->bPendingToolUser = true;
        } else if ( !xllm__buf_append_char(pBody, ',') ) {
            return false;
        }
        if ( !xllm__buf_append_cstr(pBody, "{\"type\":\"tool_result\",\"tool_use_id\":") ||
             !xllm__json_string(pBody, pMessage->sToolCallId ? pMessage->sToolCallId : "") ||
             !xllm__buf_append_cstr(pBody, ",\"content\":") ) { return false; }
        if ( pMessage->iPartCount > 1u ||
             (pMessage->iPartCount == 1u && pMessage->pParts[0].eKind != XLLM_PART_TEXT) ) {
            if ( !xllm__anthropic_append_content(pBody, pMessage, true, pWriter->pError) ||
                 !xllm__buf_append_char(pBody, '}') ) { return false; }
        } else if ( !xllm__json_string(pBody, pMessage->sContent ? pMessage->sContent : "") ||
                    !xllm__buf_append_char(pBody, '}') ) {
            return false;
        }
        return true;
    }
    if ( pWriter->bPendingToolUser ) {
        if ( !xllm__buf_append_char(pBody, ']') ||
             !xllm__buf_append_char(pBody, '}') ) { return false; }
        pWriter->bPendingToolUser = false;
    }
    if ( !xllm__anthropic_message_open(pWriter,
             pMessage->eRole == XLLM_ROLE_ASSISTANT ? "assistant" : "user") ) { return false; }
    if ( pMessage->eRole != XLLM_ROLE_ASSISTANT ) {
        if ( !xllm__anthropic_append_content(pBody, pMessage, false, pWriter->pError) ||
             !xllm__buf_append_char(pBody, '}') ) { return false; }
        return true;
    }
    /* Assistant: text and tool_use blocks ride one content array. */
    if ( !xllm__buf_append_char(pBody, '[') ) { return false; }
    {
        bool bFirst = true;
        if ( pMessage->iPartCount > 0u ) {
            for ( i = 0u; i < pMessage->iPartCount; ++i ) {
                const xllm_part* pPart = &pMessage->pParts[i];
                if ( pPart->eKind == XLLM_PART_REASONING ) { continue; }
                if ( pPart->eKind == XLLM_PART_NATIVE && pPart->sNativeType &&
                     strcmp(pPart->sNativeType, "thinking_signature") == 0 ) {
                    if ( !pMessage->sReasoningContent || !pMessage->sReasoningContent[0] ) { continue; }
                    if ( !bFirst && !xllm__buf_append_char(pBody, ',') ) { return false; }
                    if ( !xllm__buf_append_cstr(pBody, "{\"type\":\"thinking\",\"thinking\":") ||
                         !xllm__json_string(pBody, pMessage->sReasoningContent) ||
                         !xllm__buf_append_cstr(pBody, ",\"signature\":") ||
                         !xllm__json_string(pBody, pPart->sText ? pPart->sText : "") ||
                         !xllm__buf_append_char(pBody, '}') ) { return false; }
                    bFirst = false;
                    continue;
                }
                if ( !bFirst && !xllm__buf_append_char(pBody, ',') ) { return false; }
                if ( !xllm__anthropic_append_part(pBody, pPart, pWriter->pError) ) { return false; }
                bFirst = false;
            }
        }
        if ( pMessage->sContent && pMessage->sContent[0] ) {
            if ( !bFirst && !xllm__buf_append_char(pBody, ',') ) { return false; }
            if ( !xllm__buf_append_cstr(pBody, "{\"type\":\"text\",\"text\":") ||
                 !xllm__json_string(pBody, pMessage->sContent) ) { return false; }
            bFirst = false;
        }
        for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
            const xllm_tool_call* pCall = &pMessage->pToolCalls[i];
            if ( !bFirst && !xllm__buf_append_char(pBody, ',') ) { return false; }
            if ( !xllm__buf_append_cstr(pBody, "{\"type\":\"tool_use\",\"id\":") ||
                 !xllm__json_string(pBody, pCall->sId ? pCall->sId : "") ||
                 !xllm__buf_append_cstr(pBody, ",\"name\":") ||
                 !xllm__json_string(pBody, pCall->sName ? pCall->sName : "") ||
                 !xllm__buf_append_cstr(pBody, ",\"input\":") ||
                 !xllm__buf_append_cstr(pBody, pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                 !xllm__buf_append_char(pBody, '}') ) { return false; }
            bFirst = false;
        }
        if ( bFirst && !xllm__buf_append_cstr(pBody, "{\"type\":\"text\",\"text\":\"\"}") ) { return false; }
    }
    if ( !xllm__buf_append_char(pBody, ']') || !xllm__buf_append_char(pBody, '}') ) { return false; }
    return true;
}

static char* xllm__anthropic_build_request(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError)
{
    xllm_buf tBody = {0};
    xllm_buf tSystem = {0};
    xllm__anthropic_writer tWriter;
    const char* sModel;
    uint32_t uMaxTokens;
    size_t i;
    char* sResult = NULL;
    sModel = (pRequest->sModel && pRequest->sModel[0]) ? pRequest->sModel : pClient->sModel;
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "no model is configured");
        return NULL;
    }
    if ( pRequest->iMessageCount == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request has no messages");
        return NULL;
    }
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        const xllm_message* pMessage = &pRequest->pMessages[i];
        if ( pMessage->eRole != XLLM_ROLE_SYSTEM ) { continue; }
        if ( tSystem.iLen && !xllm__buf_append_cstr(&tSystem, "\n\n") ) goto oom;
        if ( pMessage->sContent && !xllm__buf_append_cstr(&tSystem, pMessage->sContent) ) goto oom;
    }
    if ( !xllm__buf_append_cstr(&tBody, "{\"model\":") || !xllm__json_string(&tBody, sModel) ) goto oom;
    uMaxTokens = pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pClient->uMaxOutputTokens;
    if ( !uMaxTokens ) { uMaxTokens = 4096u; }
    {
        char sValue[32];
        (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)uMaxTokens);
        if ( !xllm__buf_append_cstr(&tBody, ",\"max_tokens\":") ||
             !xllm__buf_append_cstr(&tBody, sValue) ) goto oom;
    }
    if ( tSystem.iLen &&
         ( !xllm__buf_append_cstr(&tBody, ",\"system\":") ||
           !xllm__json_string(&tBody, tSystem.pData ? tSystem.pData : "") ) ) goto oom;
    if ( !xllm__buf_append_cstr(&tBody, ",\"messages\":[") ) goto oom;
    tWriter.pBody = &tBody;
    tWriter.bAnyMessage = false;
    tWriter.bPendingToolUser = false;
    tWriter.pError = pError;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        const xllm_message* pMessage = &pRequest->pMessages[i];
        if ( pMessage->eRole == XLLM_ROLE_SYSTEM ) { continue; }
        if ( !xllm__anthropic_append_message(&tWriter, pMessage) ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
                xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
            }
            goto fail;
        }
    }
    if ( tWriter.bPendingToolUser ) {
        if ( !xllm__buf_append_char(&tBody, ']') || !xllm__buf_append_char(&tBody, '}') ) goto oom;
    }
    if ( !xllm__buf_append_char(&tBody, ']') ) goto oom;
    if ( pRequest->iToolCount > 0u ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"tools\":[") ) goto oom;
        for ( i = 0u; i < pRequest->iToolCount; ++i ) {
            const xllm_tool* pTool = &pRequest->pTools[i];
            const char* sSchema = pTool->sParametersJson ? pTool->sParametersJson : "{\"type\":\"object\",\"properties\":{}}";
            if ( !pTool->sName || !pTool->sName[0] ||
                 !xrtJsonValid((xstrview){ sSchema, strlen(sSchema) }) ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "tool has an invalid name or JSON parameter schema");
                goto fail;
            }
            if ( i && !xllm__buf_append_char(&tBody, ',') ) goto oom;
            if ( !xllm__buf_append_cstr(&tBody, "{\"name\":") ||
                 !xllm__json_string(&tBody, pTool->sName) ||
                 !xllm__buf_append_cstr(&tBody, ",\"description\":") ||
                 !xllm__json_string(&tBody, pTool->sDescription ? pTool->sDescription : "") ||
                 !xllm__buf_append_cstr(&tBody, ",\"input_schema\":") ||
                 !xllm__buf_append_cstr(&tBody, sSchema) ||
                 !xllm__buf_append_char(&tBody, '}') ) goto oom;
        }
        if ( !xllm__buf_append_char(&tBody, ']') ||
             !xllm__buf_append_cstr(&tBody, ",\"tool_choice\":") ) goto oom;
        switch ( pRequest->eToolChoice ) {
            case XLLM_TOOL_CHOICE_AUTO:
                if ( !xllm__buf_append_cstr(&tBody, "{\"type\":\"auto\"}") ) goto oom;
                break;
            case XLLM_TOOL_CHOICE_NONE:
                if ( !xllm__buf_append_cstr(&tBody, "{\"type\":\"none\"}") ) goto oom;
                break;
            case XLLM_TOOL_CHOICE_REQUIRED:
                if ( !xllm__buf_append_cstr(&tBody, "{\"type\":\"any\"}") ) goto oom;
                break;
            case XLLM_TOOL_CHOICE_NAMED:
                if ( !pRequest->sNamedTool || !pRequest->sNamedTool[0] ) {
                    xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "named tool choice is missing a tool name");
                    goto fail;
                }
                if ( !xllm__buf_append_cstr(&tBody, "{\"type\":\"tool\",\"name\":") ||
                     !xllm__json_string(&tBody, pRequest->sNamedTool) ||
                     !xllm__buf_append_char(&tBody, '}') ) goto oom;
                break;
            default:
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid tool choice");
                goto fail;
        }
    }
    if ( pRequest->bHasTemperature ) {
        char sValue[64];
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTemperature);
        if ( !xllm__buf_append_cstr(&tBody, ",\"temperature\":") ||
             !xllm__buf_append_cstr(&tBody, sValue) ) goto oom;
    }
    if ( pRequest->bHasTopP ) {
        char sValue[64];
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTopP);
        if ( !xllm__buf_append_cstr(&tBody, ",\"top_p\":") ||
             !xllm__buf_append_cstr(&tBody, sValue) ) goto oom;
    }
    if ( pRequest->sStop && pRequest->sStop[0] ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"stop_sequences\":[") ||
             !xllm__json_string(&tBody, pRequest->sStop) ||
             !xllm__buf_append_char(&tBody, ']') ) goto oom;
    }
    if ( pRequest->uReasoningBudgetTokens ) {
        char sValue[32];
        (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)pRequest->uReasoningBudgetTokens);
        if ( !xllm__buf_append_cstr(&tBody, ",\"thinking\":{\"type\":\"enabled\",\"budget_tokens\":") ||
             !xllm__buf_append_cstr(&tBody, sValue) ||
             !xllm__buf_append_char(&tBody, '}') ) goto oom;
    }
    if ( pRequest->sExtraBodyJson && pRequest->sExtraBodyJson[0] ) {
        const char* s = pRequest->sExtraBodyJson;
        while ( *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' ) { ++s; }
        if ( *s == '{' ) {
            size_t iEnd = strlen(s);
            /* strip trailing whitespace, then exactly one closing brace:
             * nested objects legitimately end with multiple braces. */
            while ( iEnd && (s[iEnd - 1u] == ' ' || s[iEnd - 1u] == '\t' ||
                s[iEnd - 1u] == '\r' || s[iEnd - 1u] == '\n') ) { --iEnd; }
            if ( iEnd && s[iEnd - 1u] == '}' ) { --iEnd; }
            if ( iEnd > 1u && !xllm__buf_append_cstr(&tBody, ",") ) goto oom;
            if ( iEnd > 1u && !xllm__buf_append(&tBody, s + 1u, iEnd - 1u) ) goto oom;
        }
    }
    if ( pRequest->bStream ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"stream\":true}") ) goto oom;
    } else if ( !xllm__buf_append_char(&tBody, '}') ) {
        goto oom;
    }
    sResult = xllm__buf_detach(&tBody);
    if ( !sResult ) goto oom;
    xllm__buf_reset(&tSystem);
    return sResult;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
fail:
    xllm__buf_reset(&tBody);
    xllm__buf_reset(&tSystem);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Streaming decode                                                    */
/* ------------------------------------------------------------------ */

static void xllm__anthropic_usage_in(xllm_call* pCall, xvalue* pUsage)
{
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    xllm_usage tUsage;
    if ( !pResponse || !pUsage ) { return; }
    tUsage = pResponse->tUsage;
    tUsage.uInputTokens = xllm__json_u64(pUsage, "input_tokens");
    tUsage.uCachedInputTokens = xllm__json_u64(pUsage, "cache_read_input_tokens");
    tUsage.uCacheWriteTokens = xllm__json_u64(pUsage, "cache_creation_input_tokens");
    (void)xllm__assemble_usage(pCall, &tUsage);
}

static bool xllm__anthropic_decode_sse(xllm_call* pCall, const xllm_sse_fields* pFields)
{
    xvalue* pRoot;
    bool bOk = true;
    xstrview tType = {0};
    if ( !pCall || !pFields ) { return false; }
    if ( !pFields->tData.Data || !pFields->tData.Size ) { return true; }
    if ( !xrtJsonValid(pFields->tData) ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pRoot = xrtJsonParse(pFields->tData);
    if ( !pRoot ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pCall->bSawEvent = true;
    tType = pFields->tEvent.Size ? pFields->tEvent : xllm__json_text(pRoot, "type");
#define XLLM_IS(name) (tType.Size == sizeof(name) - 1u && memcmp(tType.Data, name, sizeof(name) - 1u) == 0)
    if ( XLLM_IS("ping") || XLLM_IS("content_block_stop") ) { goto done; }
    if ( XLLM_IS("message_stop") ) { pCall->bDone = true; goto done; }
    if ( XLLM_IS("error") ) goto error;
    if ( XLLM_IS("message_start") ) {
        xvalue* pMessage = xllm__json_get(pRoot, "message");
        xllm__anthropic_usage_in(pCall, xllm__json_get(pMessage, "usage"));
        goto done;
    }
    if ( XLLM_IS("content_block_start") ) {
        xvalue* pBlock = xllm__json_get(pRoot, "content_block");
        xstrview tBlockType = xllm__json_text(pBlock, "type");
        uint64_t uIndex = xllm__json_u64(pRoot, "index");
        if ( tBlockType.Size == 8u && memcmp(tBlockType.Data, "tool_use", 8u) == 0 ) {
            size_t iTool = pCall->pResponse ? pCall->pResponse->iToolCallCount : 0u;
            if ( !xllm__assemble_tool(pCall, iTool,
                    xllm__json_text(pBlock, "id"),
                    xllm__json_text(pBlock, "name"),
                    (xstrview){0}) ) { bOk = false; goto done; }
            if ( !xllm__assemble_block_mark_tool(pCall, iTool) ||
                 !xllm__assemble_set_block_tool(pCall, (size_t)uIndex, iTool) ) { bOk = false; goto done; }
        }
        goto done;
    }
    if ( XLLM_IS("content_block_delta") ) {
        xvalue* pDelta = xllm__json_get(pRoot, "delta");
        xstrview tDeltaType = xllm__json_text(pDelta, "type");
        uint64_t uIndex = xllm__json_u64(pRoot, "index");
        if ( tDeltaType.Size == 10u && memcmp(tDeltaType.Data, "text_delta", 10u) == 0 ) {
            if ( !xllm__assemble_text(pCall, XLLM_BLOCK_TEXT,
                    xllm__json_text(pDelta, "text"), NULL) ) { bOk = false; goto done; }
        } else if ( tDeltaType.Size == 14u && memcmp(tDeltaType.Data, "thinking_delta", 14u) == 0 ) {
            if ( !xllm__assemble_text(pCall, XLLM_BLOCK_REASONING,
                    xllm__json_text(pDelta, "thinking"), NULL) ) { bOk = false; goto done; }
        } else if ( tDeltaType.Size == 15u && memcmp(tDeltaType.Data, "signature_delta", 15u) == 0 ) {
            if ( !xllm__assemble_native(pCall, XLLM_BLOCK_REASONING,
                    xllm__json_text(pDelta, "signature")) ) { bOk = false; goto done; }
        } else if ( tDeltaType.Size == 16u && memcmp(tDeltaType.Data, "input_json_delta", 16u) == 0 ) {
            size_t iTool = xllm__assemble_map_block_tool(pCall, (size_t)uIndex);
            if ( iTool ) {
                if ( !xllm__assemble_tool(pCall, iTool - 1u, (xstrview){0}, (xstrview){0},
                        xllm__json_text(pDelta, "partial_json")) ) { bOk = false; goto done; }
            }
        }
        goto done;
    }
    if ( XLLM_IS("message_delta") ) {
        xvalue* pDelta = xllm__json_get(pRoot, "delta");
        xvalue* pUsage = xllm__json_get(pRoot, "usage");
        xllm_response* pResponse = xllm__assemble_ensure(pCall);
        xstrview tStop = xllm__json_text(pDelta, "stop_reason");
        if ( !pResponse ) { bOk = false; goto done; }
        if ( tStop.Data ) { xllm__assemble_finish(pCall, tStop); }
        if ( pUsage ) {
            xllm_usage tUsage = pResponse->tUsage;
            uint64_t uInput = xllm__json_u64(pUsage, "input_tokens");
            /* Some gateways zero the message_start counters and only report
             * the real input usage here. */
            if ( uInput > tUsage.uInputTokens ) { tUsage.uInputTokens = uInput; }
            tUsage.uOutputTokens = xllm__json_u64(pUsage, "output_tokens");
            {
                uint64_t uCache = xllm__json_u64(pUsage, "cache_read_input_tokens");
                if ( uCache > tUsage.uCachedInputTokens ) {
                    tUsage.uCachedInputTokens = uCache;
                }
            }
            tUsage.uTotalTokens = tUsage.uInputTokens + tUsage.uOutputTokens;
            if ( !xllm__assemble_usage(pCall, &tUsage) ) { bOk = false; goto done; }
        }
        goto done;
    }
#undef XLLM_IS
    goto done;
error:
    {
        xvalue* pError = xllm__json_get(pRoot, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        xstrview tErrorType = xllm__json_text(pError, "type");
        if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tMessage);
        if ( tErrorType.Data ) xllm__copy_view(pCall->tError.sProviderType,
            sizeof(pCall->tError.sProviderType), tErrorType);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned a stream error");
        bOk = false;
    }
done:
    xrtValueRelease(pRoot);
    return bOk;
}

static bool xllm__anthropic_decode_json(xllm_call* pCall, xstrview tBody)
{
    xvalue* pRoot;
    xvalue* pContent;
    xllm_response* pResponse;
    xllm_usage tUsage;
    size_t i;
    bool bOk = true;
    if ( !pCall || !tBody.Data || !tBody.Size ) { return false; }
    if ( !xrtJsonValid(tBody) ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    pRoot = xrtJsonParse(tBody);
    if ( !pRoot ) {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    if ( xllm__json_get(pRoot, "type") ) {
        xstrview tType = xllm__json_text(pRoot, "type");
        if ( tType.Size == 5u && memcmp(tType.Data, "error", 5u) == 0 ) {
            xvalue* pError = xllm__json_get(pRoot, "error");
            xstrview tMessage = xllm__json_text(pError, "message");
            xstrview tType2 = xllm__json_text(pError, "type");
            if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
                sizeof(pCall->tError.sProviderMessage), tMessage);
            if ( tType2.Data ) xllm__copy_view(pCall->tError.sProviderType,
                sizeof(pCall->tError.sProviderType), tType2);
            xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
            xrtValueRelease(pRoot);
            return false;
        }
    }
    pResponse = xllm__assemble_ensure(pCall);
    if ( !pResponse ) { xrtValueRelease(pRoot); return false; }
    {
        xstrview tId = xllm__json_text(pRoot, "id");
        xstrview tModel = xllm__json_text(pRoot, "model");
        char* sCopy;
        if ( tId.Data && !pResponse->sId ) {
            sCopy = (char*)xllm__malloc(tId.Size + 1u);
            if ( sCopy ) { memcpy(sCopy, tId.Data, tId.Size); sCopy[tId.Size] = 0; pResponse->sId = sCopy; }
        }
        if ( tModel.Data && !pResponse->sModel ) {
            sCopy = (char*)xllm__malloc(tModel.Size + 1u);
            if ( sCopy ) { memcpy(sCopy, tModel.Data, tModel.Size); sCopy[tModel.Size] = 0; pResponse->sModel = sCopy; }
        }
    }
    pContent = xllm__json_get(pRoot, "content");
    if ( pContent && xrtValueType(pContent) == XVALUE_ARRAY ) {
        size_t uCount = xrtValueCount(pContent);
        for ( i = 0u; i < uCount; ++i ) {
            xvalue* pBlock = xrtValueArrayGet(pContent, i);
            xstrview tType = xllm__json_text(pBlock, "type");
            if ( tType.Size == 4u && memcmp(tType.Data, "text", 4u) == 0 ) {
                if ( !xllm__assemble_text(pCall, XLLM_BLOCK_TEXT,
                        xllm__json_text(pBlock, "text"), NULL) ) { bOk = false; break; }
            } else if ( tType.Size == 8u && memcmp(tType.Data, "thinking", 8u) == 0 ) {
                if ( !xllm__assemble_text(pCall, XLLM_BLOCK_REASONING,
                        xllm__json_text(pBlock, "thinking"), NULL) ) { bOk = false; break; }
                if ( !xllm__assemble_native(pCall, XLLM_BLOCK_REASONING,
                        xllm__json_text(pBlock, "signature")) ) { bOk = false; break; }
            } else if ( tType.Size == 8u && memcmp(tType.Data, "tool_use", 8u) == 0 ) {
                if ( !xllm__assemble_tool(pCall, pResponse->iToolCallCount,
                        xllm__json_text(pBlock, "id"),
                        xllm__json_text(pBlock, "name"),
                        (xstrview){0}) ) { bOk = false; break; }
                if ( !xllm__assemble_block_mark_tool(pCall, pResponse->iToolCallCount - 1u) ) { bOk = false; break; }
            }
        }
    }
    if ( bOk ) {
        xstrview tStop = xllm__json_text(pRoot, "stop_reason");
        if ( tStop.Data ) { xllm__assemble_finish(pCall, tStop); }
        memset(&tUsage, 0, sizeof(tUsage));
        tUsage.uInputTokens = xllm__json_u64(xllm__json_get(pRoot, "usage"), "input_tokens");
        tUsage.uOutputTokens = xllm__json_u64(xllm__json_get(pRoot, "usage"), "output_tokens");
        tUsage.uTotalTokens = tUsage.uInputTokens + tUsage.uOutputTokens;
        tUsage.uCachedInputTokens = xllm__json_u64(xllm__json_get(pRoot, "usage"), "cache_read_input_tokens");
        tUsage.uCacheWriteTokens = xllm__json_u64(xllm__json_get(pRoot, "usage"), "cache_creation_input_tokens");
        if ( !xllm__assemble_usage(pCall, &tUsage) ) { bOk = false; }
    }
    if ( bOk ) { pCall->bSawEvent = true; }
    xrtValueRelease(pRoot);
    return bOk;
}

static void xllm__anthropic_fill_error(xllm_call* pCall, xstrview tBody)
{
    xvalue* pRoot;
    if ( !pCall || !tBody.Data || !tBody.Size ) { return; }
    if ( !xrtJsonValid(tBody) ) {
        xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tBody);
        return;
    }
    pRoot = xrtJsonParse(tBody);
    if ( !pRoot ) {
        xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tBody);
        return;
    }
    {
        xvalue* pError = xllm__json_get(pRoot, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        xstrview tType = xllm__json_text(pError, "type");
        if ( !tMessage.Data ) tMessage = xllm__json_text(pRoot, "message");
        if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tMessage);
        if ( tType.Data ) xllm__copy_view(pCall->tError.sProviderType,
            sizeof(pCall->tError.sProviderType), tType);
        if ( !pCall->tError.sMessage[0] ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
        }
    }
    xrtValueRelease(pRoot);
}

static bool xllm__anthropic_is_retryable(const xllm_error* pError)
{
    return pError && strcmp(pError->sProviderType, "overloaded_error") == 0;
}

static const xllm_dialect_ops XLLM_ANTHROPIC_DIALECT = {
    "anthropic",
    "/v1/messages",
    xllm__anthropic_build_auth,
    xllm__anthropic_build_request,
    xllm__anthropic_decode_sse,
    xllm__anthropic_decode_json,
    xllm__anthropic_fill_error,
    xllm__anthropic_is_retryable
};

const xllm_dialect_ops* xllm__dialect_anthropic(void)
{
    return &XLLM_ANTHROPIC_DIALECT;
}
