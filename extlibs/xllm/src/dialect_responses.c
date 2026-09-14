#include "xllm_internal.h"

/* OpenAI Responses dialect.
 *
 * Request: system messages lift to the top-level instructions field; the
 * conversation rides a typed input item array (message / function_call /
 * function_call_output). Streaming: typed events in the data payload
 * (response.output_text.delta, response.reasoning_summary_text.delta,
 * response.function_call_arguments.delta, response.completed/failed/
 * incomplete). Stateless usage only: previous_response_id and server-side
 * state are deliberately not used. */

static size_t xllm__responses_build_auth(const xllm_client* pClient,
    xllm_auth_header* pOut, size_t iCap)
{
    size_t iKeyLen;
    if ( !pClient || !pOut || !iCap || !pClient->sApiKey[0] ) { return 0u; }
    iKeyLen = strlen(pClient->sApiKey);
    pOut[0].sValue = (char*)xllm__malloc(iKeyLen + 8u);
    if ( !pOut[0].sValue ) { return 0u; }
    memcpy(pOut[0].sValue, "Bearer ", 7u);
    memcpy(pOut[0].sValue + 7u, pClient->sApiKey, iKeyLen + 1u);
    xllm__copy_text(pOut[0].sName, sizeof(pOut[0].sName), "Authorization");
    return 1u;
}

static bool xllm__responses_append_input_image(xllm_buf* pBuf, const xllm_part* pPart, xllm_error* pError)
{
    if ( pPart->sSourceUrl && pPart->sSourceUrl[0] ) {
        if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"input_image\",\"image_url\":") ||
             !xllm__json_string(pBuf, pPart->sSourceUrl) ||
             !xllm__buf_append_char(pBuf, '}') ) { return false; }
        return true;
    }
    if ( pPart->pData && pPart->iDataSize ) {
        str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
        bool bOk;
        if ( !sEncoded ) { return false; }
        bOk = xllm__buf_append_cstr(pBuf, "{\"type\":\"input_image\",\"image_url\":\"data:") &&
            xllm__buf_append_cstr(pBuf, pPart->sMediaType ? pPart->sMediaType : "image/png") &&
            xllm__buf_append_cstr(pBuf, ";base64,") &&
            xllm__buf_append_cstr(pBuf, sEncoded) &&
            xllm__buf_append_cstr(pBuf, "\"}");
        xrtFree(sEncoded);
        return bOk;
    }
    xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "image part has neither bytes nor URL");
    return false;
}

static bool xllm__responses_append_content_part(xllm_buf* pBuf, const xllm_part* pPart,
    bool bOutput, xllm_error* pError)
{
    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
            if ( !xllm__buf_append_cstr(pBuf, bOutput ? "{\"type\":\"output_text\",\"text\":" :
                    "{\"type\":\"input_text\",\"text\":") ||
                 !xllm__json_string(pBuf, pPart->sText ? pPart->sText : "") ||
                 !xllm__buf_append_char(pBuf, '}') ) { return false; }
            return true;
        case XLLM_PART_IMAGE:
            return xllm__responses_append_input_image(pBuf, pPart, pError);
        case XLLM_PART_NATIVE:
            return xllm__buf_append_cstr(pBuf, pPart->sText ? pPart->sText : "{}");
        default:
            /* audio/file inputs are not mapped on this dialect yet. */
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                "content part kind is not supported by the responses dialect");
            return false;
    }
}

static bool xllm__responses_append_items(xllm_buf* pBuf, const xllm_request* pRequest,
    size_t iStartIndex, xllm_error* pError)
{
    size_t i;
    bool bFirst = true;
    for ( i = iStartIndex; i < pRequest->iMessageCount; ++i ) {
        const xllm_message* pMessage = &pRequest->pMessages[i];
        size_t j;
        if ( pMessage->eRole == XLLM_ROLE_SYSTEM ) { continue; }
        if ( !bFirst && !xllm__buf_append_char(pBuf, ',') ) { return false; }
        bFirst = false;
        if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"function_call_output\",\"call_id\":") ||
                 !xllm__json_string(pBuf, pMessage->sToolCallId ? pMessage->sToolCallId : "") ||
                 !xllm__buf_append_cstr(pBuf, ",\"output\":") ) { return false; }
            if ( pMessage->iPartCount == 1u && pMessage->pParts[0].eKind == XLLM_PART_TEXT ) {
                if ( !xllm__json_string(pBuf, pMessage->pParts[0].sText ? pMessage->pParts[0].sText : "") ||
                     !xllm__buf_append_char(pBuf, '}') ) { return false; }
            } else if ( !xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : "") ||
                        !xllm__buf_append_char(pBuf, '}') ) {
                return false;
            }
            continue;
        }
        if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u &&
             ( !pMessage->sContent || !pMessage->sContent[0] ) && pMessage->iPartCount == 0u ) {
            for ( j = 0u; j < pMessage->iToolCallCount; ++j ) {
                const xllm_tool_call* pCall = &pMessage->pToolCalls[j];
                if ( j && !xllm__buf_append_char(pBuf, ',') ) { return false; }
                if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"function_call\",\"call_id\":") ||
                     !xllm__json_string(pBuf, pCall->sId ? pCall->sId : "") ||
                     !xllm__buf_append_cstr(pBuf, ",\"name\":") ||
                     !xllm__json_string(pBuf, pCall->sName ? pCall->sName : "") ||
                     !xllm__buf_append_cstr(pBuf, ",\"arguments\":") ||
                     !xllm__json_string(pBuf, pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                     !xllm__buf_append_char(pBuf, '}') ) { return false; }
            }
            continue;
        }
        if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"message\",\"role\":") ||
             !xllm__json_string(pBuf, pMessage->eRole == XLLM_ROLE_ASSISTANT ? "assistant" : "user") ||
             !xllm__buf_append_cstr(pBuf, ",\"content\":") ) { return false; }
        if ( pMessage->iPartCount > 0u ) {
            size_t iUsable = 0u;
            bool bArray = false;
            for ( j = 0u; j < pMessage->iPartCount; ++j ) {
                if ( pMessage->pParts[j].eKind == XLLM_PART_REASONING ) { continue; }
                ++iUsable;
                if ( pMessage->pParts[j].eKind != XLLM_PART_TEXT ) { bArray = true; }
            }
            if ( iUsable == 1u && !bArray ) {
                const char* sText = NULL;
                for ( j = 0u; j < pMessage->iPartCount; ++j ) {
                    if ( pMessage->pParts[j].eKind == XLLM_PART_REASONING ) { continue; }
                    sText = pMessage->pParts[j].sText;
                }
                if ( !xllm__json_string(pBuf, sText ? sText : "") ) { return false; }
            } else {
                if ( !xllm__buf_append_char(pBuf, '[') ) { return false; }
                {
                    bool bPartFirst = true;
                    for ( j = 0u; j < pMessage->iPartCount; ++j ) {
                        if ( pMessage->pParts[j].eKind == XLLM_PART_REASONING ) { continue; }
                        if ( !bPartFirst && !xllm__buf_append_char(pBuf, ',') ) { return false; }
                        if ( !xllm__responses_append_content_part(pBuf, &pMessage->pParts[j],
                                pMessage->eRole == XLLM_ROLE_ASSISTANT, pError) ) { return false; }
                        bPartFirst = false;
                    }
                }
                if ( !xllm__buf_append_char(pBuf, ']') ) { return false; }
            }
        } else if ( !xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : "") ) {
            return false;
        }
        if ( !xllm__buf_append_char(pBuf, '}') ) { return false; }
        if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u ) {
            for ( j = 0u; j < pMessage->iToolCallCount; ++j ) {
                const xllm_tool_call* pCall = &pMessage->pToolCalls[j];
                if ( !xllm__buf_append_char(pBuf, ',') ||
                     !xllm__buf_append_cstr(pBuf, "{\"type\":\"function_call\",\"call_id\":") ||
                     !xllm__json_string(pBuf, pCall->sId ? pCall->sId : "") ||
                     !xllm__buf_append_cstr(pBuf, ",\"name\":") ||
                     !xllm__json_string(pBuf, pCall->sName ? pCall->sName : "") ||
                     !xllm__buf_append_cstr(pBuf, ",\"arguments\":") ||
                     !xllm__json_string(pBuf, pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                     !xllm__buf_append_char(pBuf, '}') ) { return false; }
            }
        }
    }
    return true;
}

static char* xllm__responses_build_request(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError)
{
    xllm_buf tBody = {0};
    xllm_buf tSystem = {0};
    const char* sModel;
    const char* sEffort;
    uint32_t uMaxTokens;
    size_t i;
    char sValue[64];
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
    if ( tSystem.iLen &&
         ( !xllm__buf_append_cstr(&tBody, ",\"instructions\":") ||
           !xllm__json_string(&tBody, tSystem.pData ? tSystem.pData : "") ) ) goto oom;
    /* Wire alignment (pi behavior): never persist this exchange server-side;
     * a caller-provided extraBody "store" key wins (see completions). */
    if ( !pRequest->sExtraBodyJson ||
         strstr(pRequest->sExtraBodyJson, "\"store\"") == NULL ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"store\":false") ) goto oom;
    }
    uMaxTokens = pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pClient->uMaxOutputTokens;
    if ( uMaxTokens ) {
        (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)uMaxTokens);
        if ( !xllm__buf_append_cstr(&tBody, ",\"max_output_tokens\":") ||
             !xllm__buf_append_cstr(&tBody, sValue) ) goto oom;
    }
    if ( pRequest->bHasTemperature ) {
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTemperature);
        if ( !xllm__buf_append_cstr(&tBody, ",\"temperature\":") ||
             !xllm__buf_append_cstr(&tBody, sValue) ) goto oom;
    }
    if ( pRequest->bHasTopP ) {
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTopP);
        if ( !xllm__buf_append_cstr(&tBody, ",\"top_p\":") ||
             !xllm__buf_append_cstr(&tBody, sValue) ) goto oom;
    }
    sEffort = (pRequest->sReasoningEffort && pRequest->sReasoningEffort[0])
        ? pRequest->sReasoningEffort : pClient->sReasoningEffort;
    if ( sEffort && sEffort[0] && strcmp(sEffort, "off") != 0 ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"reasoning\":{\"effort\":") ||
             !xllm__json_string(&tBody, sEffort) ||
             !xllm__buf_append_char(&tBody, '}') ) goto oom;
    }
    if ( pRequest->eJsonMode == XLLM_JSON_OBJECT ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"text\":{\"format\":{\"type\":\"json_object\"}}") ) goto oom;
    }
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
            if ( !xllm__buf_append_cstr(&tBody, "{\"type\":\"function\",\"name\":") ||
                 !xllm__json_string(&tBody, pTool->sName) ||
                 !xllm__buf_append_cstr(&tBody, ",\"description\":") ||
                 !xllm__json_string(&tBody, pTool->sDescription ? pTool->sDescription : "") ||
                 !xllm__buf_append_cstr(&tBody, ",\"parameters\":") ||
                 !xllm__buf_append_cstr(&tBody, sSchema) ) goto oom;
            if ( pTool->bStrict &&
                 !xllm__buf_append_cstr(&tBody, ",\"strict\":true") ) goto oom;
            if ( !xllm__buf_append_char(&tBody, '}') ) goto oom;
        }
        if ( !xllm__buf_append_char(&tBody, ']') ||
             !xllm__buf_append_cstr(&tBody, ",\"tool_choice\":") ) goto oom;
        switch ( pRequest->eToolChoice ) {
            case XLLM_TOOL_CHOICE_AUTO:
                if ( !xllm__json_string(&tBody, "auto") ) goto oom;
                break;
            case XLLM_TOOL_CHOICE_NONE:
                if ( !xllm__json_string(&tBody, "none") ) goto oom;
                break;
            case XLLM_TOOL_CHOICE_REQUIRED:
                if ( !xllm__json_string(&tBody, "required") ) goto oom;
                break;
            case XLLM_TOOL_CHOICE_NAMED:
                if ( !pRequest->sNamedTool || !pRequest->sNamedTool[0] ) {
                    xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "named tool choice is missing a tool name");
                    goto fail;
                }
                if ( !xllm__buf_append_cstr(&tBody, "{\"type\":\"function\",\"name\":") ||
                     !xllm__json_string(&tBody, pRequest->sNamedTool) ||
                     !xllm__buf_append_char(&tBody, '}') ) goto oom;
                break;
            default:
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid tool choice");
                goto fail;
        }
    }
    if ( !xllm__buf_append_cstr(&tBody, ",\"input\":") ||
         !xllm__buf_append_char(&tBody, '[') ||
         !xllm__responses_append_items(&tBody, pRequest, 0u, pError) ||
         !xllm__buf_append_char(&tBody, ']') ) {
        if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
        }
        goto fail;
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
/* Streaming / body decode                                             */
/* ------------------------------------------------------------------ */

static void xllm__responses_fill_usage(xllm_usage* pUsage, xvalue* pUsageObject)
{
    if ( !pUsageObject || xrtValueType(pUsageObject) != XVALUE_OBJECT ) { return; }
    pUsage->uInputTokens = xllm__json_u64(pUsageObject, "input_tokens");
    pUsage->uOutputTokens = xllm__json_u64(pUsageObject, "output_tokens");
    pUsage->uTotalTokens = xllm__json_u64(pUsageObject, "total_tokens");
    {
        xvalue* pDetails = xllm__json_get(pUsageObject, "input_tokens_details");
        pUsage->uCachedInputTokens = xllm__json_u64(pDetails, "cached_tokens");
    }
    {
        xvalue* pDetails = xllm__json_get(pUsageObject, "output_tokens_details");
        pUsage->uReasoningTokens = xllm__json_u64(pDetails, "reasoning_tokens");
    }
}

static bool xllm__responses_decode_sse(xllm_call* pCall, const xllm_sse_fields* pFields)
{
    xvalue* pRoot;
    xstrview tType;
    bool bOk = true;
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
    tType = xllm__json_text(pRoot, "type");
#define XLLM_IS(name) (tType.Size == sizeof(name) - 1u && memcmp(tType.Data, name, sizeof(name) - 1u) == 0)
    if ( XLLM_IS("response.output_text.delta") ) {
        if ( !xllm__assemble_text(pCall, XLLM_BLOCK_TEXT,
                xllm__json_text(pRoot, "delta"), NULL) ) { bOk = false; }
    } else if ( XLLM_IS("response.reasoning_summary_text.delta") ||
                XLLM_IS("response.reasoning_text.delta") ) {
        if ( !xllm__assemble_text(pCall, XLLM_BLOCK_REASONING,
                xllm__json_text(pRoot, "delta"), NULL) ) { bOk = false; }
    } else if ( XLLM_IS("response.output_item.added") ) {
        xvalue* pItem = xllm__json_get(pRoot, "item");
        xstrview tItemType = xllm__json_text(pItem, "type");
        xstrview tItemId = xllm__json_text(pItem, "id");
        if ( tItemType.Size == 13u && memcmp(tItemType.Data, "function_call", 13u) == 0 ) {
            xstrview tCallId = xllm__json_text(pItem, "call_id");
            xstrview tName = xllm__json_text(pItem, "name");
            char sId[80];
            size_t iTool;
            if ( !tCallId.Data && tItemId.Data && tItemId.Size < sizeof(sId) ) {
                memcpy(sId, tItemId.Data, tItemId.Size);
                sId[tItemId.Size] = 0;
                tCallId.Data = sId;
                tCallId.Size = tItemId.Size;
            }
            iTool = xllm__assemble_add_item_tool(pCall,
                tItemId.Data ? (const char*)tItemId.Data : "", tCallId, tName);
            if ( iTool == (size_t)-1 ||
                 !xllm__assemble_block_mark_tool(pCall, iTool) ) { bOk = false; }
        }
    } else if ( XLLM_IS("response.function_call_arguments.delta") ) {
        char sId[80];
        size_t iTool;
        xstrview tItemId = xllm__json_text(pRoot, "item_id");
        if ( tItemId.Data && tItemId.Size < sizeof(sId) ) {
            memcpy(sId, tItemId.Data, tItemId.Size);
            sId[tItemId.Size] = 0;
            iTool = xllm__assemble_find_item_tool(pCall, sId);
            if ( iTool != (size_t)-1 &&
                 !xllm__assemble_tool(pCall, iTool, (xstrview){0}, (xstrview){0},
                     xllm__json_text(pRoot, "delta")) ) { bOk = false; }
        }
    } else if ( XLLM_IS("response.completed") ) {
        xvalue* pResponse = xllm__json_get(pRoot, "response");
        xllm_usage tUsage;
        memset(&tUsage, 0, sizeof(tUsage));
        xllm__responses_fill_usage(&tUsage, xllm__json_get(pResponse, "usage"));
        if ( tUsage.uTotalTokens || tUsage.uInputTokens || tUsage.uOutputTokens ) {
            if ( !xllm__assemble_usage(pCall, &tUsage) ) { bOk = false; }
        }
        pCall->bDone = true;
    } else if ( XLLM_IS("response.incomplete") ) {
        xvalue* pResponse = xllm__json_get(pRoot, "response");
        xvalue* pDetails = xllm__json_get(pResponse, "incomplete_details");
        xstrview tReason = xllm__json_text(pDetails, "reason");
        xllm__assemble_finish(pCall, tReason.Data ? tReason : (xstrview){ "max_output_tokens", 17u });
        pCall->bDone = true;
    } else if ( XLLM_IS("response.failed") ) {
        xvalue* pResponse = xllm__json_get(pRoot, "response");
        xvalue* pError = xllm__json_get(pResponse, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        xstrview tCode = xllm__json_text(pError, "code");
        if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tMessage);
        if ( tCode.Data ) xllm__copy_view(pCall->tError.sProviderCode,
            sizeof(pCall->tError.sProviderCode), tCode);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider reported a failed response");
        bOk = false;
    } else if ( XLLM_IS("error") ) {
        xstrview tMessage = xllm__json_text(pRoot, "message");
        xstrview tCode = xllm__json_text(pRoot, "code");
        if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tMessage);
        if ( tCode.Data ) xllm__copy_view(pCall->tError.sProviderCode,
            sizeof(pCall->tError.sProviderCode), tCode);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned a stream error");
        bOk = false;
    }
#undef XLLM_IS
    xrtValueRelease(pRoot);
    return bOk;
}

static bool xllm__responses_decode_json(xllm_call* pCall, xstrview tBody)
{
    xvalue* pRoot;
    xvalue* pOutput;
    xllm_response* pResponse;
    xllm_usage tUsage;
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
    if ( xllm__json_get(pRoot, "error") ) {
        xvalue* pError = xllm__json_get(pRoot, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tMessage);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
        xrtValueRelease(pRoot);
        return false;
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
    pOutput = xllm__json_get(pRoot, "output");
    if ( pOutput && xrtValueType(pOutput) == XVALUE_ARRAY ) {
        size_t uCount = xrtValueCount(pOutput);
        size_t i;
        for ( i = 0u; i < uCount && bOk; ++i ) {
            xvalue* pItem = xrtValueArrayGet(pOutput, i);
            xstrview tType = xllm__json_text(pItem, "type");
            if ( tType.Size == 7u && memcmp(tType.Data, "message", 7u) == 0 ) {
                xvalue* pContent = xllm__json_get(pItem, "content");
                if ( pContent && xrtValueType(pContent) == XVALUE_ARRAY ) {
                    size_t uBlocks = xrtValueCount(pContent);
                    size_t j;
                    for ( j = 0u; j < uBlocks && bOk; ++j ) {
                        xvalue* pBlock = xrtValueArrayGet(pContent, j);
                        xstrview tBlockType = xllm__json_text(pBlock, "type");
                        if ( tBlockType.Size == 11u && memcmp(tBlockType.Data, "output_text", 11u) == 0 ) {
                            if ( !xllm__assemble_text(pCall, XLLM_BLOCK_TEXT,
                                    xllm__json_text(pBlock, "text"), NULL) ) { bOk = false; }
                        }
                    }
                }
            } else if ( tType.Size == 13u && memcmp(tType.Data, "function_call", 13u) == 0 ) {
                if ( !xllm__assemble_tool(pCall, pResponse->iToolCallCount,
                        xllm__json_text(pItem, "call_id"),
                        xllm__json_text(pItem, "name"),
                        xllm__json_text(pItem, "arguments")) ) { bOk = false; }
                else if ( !xllm__assemble_block_mark_tool(pCall, pResponse->iToolCallCount - 1u) ) { bOk = false; }
            } else if ( tType.Size == 9u && memcmp(tType.Data, "reasoning", 9u) == 0 ) {
                xvalue* pSummary = xllm__json_get(pItem, "summary");
                if ( pSummary && xrtValueType(pSummary) == XVALUE_ARRAY ) {
                    size_t uBlocks = xrtValueCount(pSummary);
                    size_t j;
                    for ( j = 0u; j < uBlocks && bOk; ++j ) {
                        xvalue* pBlock = xrtValueArrayGet(pSummary, j);
                        if ( !xllm__assemble_text(pCall, XLLM_BLOCK_REASONING,
                                xllm__json_text(pBlock, "text"), NULL) ) { bOk = false; }
                    }
                }
            }
        }
    }
    if ( bOk ) {
        xstrview tStatus = xllm__json_text(pRoot, "status");
        if ( tStatus.Data ) { xllm__assemble_finish(pCall, tStatus); }
        memset(&tUsage, 0, sizeof(tUsage));
        xllm__responses_fill_usage(&tUsage, xllm__json_get(pRoot, "usage"));
        if ( !xllm__assemble_usage(pCall, &tUsage) ) { bOk = false; }
    }
    if ( bOk ) { pCall->bSawEvent = true; }
    xrtValueRelease(pRoot);
    return bOk;
}

static void xllm__responses_fill_error(xllm_call* pCall, xstrview tBody)
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
        xstrview tCode = xllm__json_text(pError, "code");
        xstrview tType = xllm__json_text(pError, "type");
        if ( !tMessage.Data ) tMessage = xllm__json_text(pRoot, "message");
        if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
            sizeof(pCall->tError.sProviderMessage), tMessage);
        if ( tCode.Data ) xllm__copy_view(pCall->tError.sProviderCode,
            sizeof(pCall->tError.sProviderCode), tCode);
        if ( tType.Data ) xllm__copy_view(pCall->tError.sProviderType,
            sizeof(pCall->tError.sProviderType), tType);
        if ( !pCall->tError.sMessage[0] ) {
            xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
        }
    }
    xrtValueRelease(pRoot);
}

static const xllm_dialect_ops XLLM_RESPONSES_DIALECT = {
    "responses",
    "/responses",
    xllm__responses_build_auth,
    xllm__responses_build_request,
    xllm__responses_decode_sse,
    xllm__responses_decode_json,
    xllm__responses_fill_error,
    NULL
};

const xllm_dialect_ops* xllm__dialect_responses(void)
{
    return &XLLM_RESPONSES_DIALECT;
}
