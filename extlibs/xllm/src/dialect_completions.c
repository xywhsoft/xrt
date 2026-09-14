#include "xllm_internal.h"

/* Chat Completions dialect, including the GLM tweak set. Serialization
 * flags are resolved once per request from the provider and the active
 * model profile; the decoders map wire chunks into the unified events. */

typedef struct xllm_completions_dialect {
    bool bReasoningContentField;   /* assistant history carries reasoning_content */
    bool bThinkingObject;          /* reasoning control serializes as a thinking object */
    bool bToolStreamField;         /* tool_stream hint belongs to the wire contract */
    bool bStrictSchemaField;       /* tool schemas accept strict:true */
    bool bParallelToolCallsField;  /* parallel_tool_calls belongs to the wire contract */
    bool bStreamOptionsUsage;      /* stream_options.include_usage is honored */
    bool bMaxCompletionTokens;     /* max_completion_tokens replaces max_tokens */
    bool bDeveloperRole;           /* system instructions map to the developer role */
} xllm_completions_dialect;

static xllm_completions_dialect xllm__completions_flags(const xllm_client* pClient)
{
    xllm_completions_dialect tFlags;
    memset(&tFlags, 0, sizeof(tFlags));
    if ( pClient && pClient->eProvider == XLLM_PROVIDER_GLM ) {
        tFlags.bReasoningContentField = true;
        tFlags.bThinkingObject = true;
        tFlags.bToolStreamField = true;
    } else {
        tFlags.bStrictSchemaField = true;
        tFlags.bParallelToolCallsField = true;
        tFlags.bStreamOptionsUsage = true;
        if ( pClient && pClient->bHasModelProfile ) {
            tFlags.bMaxCompletionTokens =
                (pClient->tModelProfile.uCapabilities & XLLM_CAP_MAX_COMPLETION_TOKENS) != 0u;
            tFlags.bDeveloperRole =
                (pClient->tModelProfile.uCapabilities & XLLM_CAP_DEVELOPER_ROLE) != 0u;
        }
    }
    return tFlags;
}

/* ------------------------------------------------------------------ */
/* Request serialization                                               */
/* ------------------------------------------------------------------ */

static const char* xllm__role_name(xllm_role eRole, const xllm_completions_dialect* pFlags)
{
    switch ( eRole ) {
        case XLLM_ROLE_SYSTEM:
            return pFlags->bDeveloperRole ? "developer" : "system";
        case XLLM_ROLE_USER: return "user";
        case XLLM_ROLE_ASSISTANT: return "assistant";
        case XLLM_ROLE_TOOL: return "tool";
        default: return NULL;
    }
}

static bool xllm__json_u32(xllm_buf* pBuf, uint32_t uValue)
{
    char sValue[32];
    (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)uValue);
    return xllm__buf_append_cstr(pBuf, sValue);
}

static bool xllm__json_double(xllm_buf* pBuf, double fValue)
{
    char sValue[64];
    (void)snprintf(sValue, sizeof(sValue), "%.17g", fValue);
    return xllm__buf_append_cstr(pBuf, sValue);
}

static bool xllm__append_data_url(xllm_buf* pBuf, const xllm_part* pPart)
{
    str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
    bool bOk;
    if ( !sEncoded ) { return false; }
    bOk = xllm__buf_append_cstr(pBuf, "data:") &&
        xllm__buf_append_cstr(pBuf, pPart->sMediaType ? pPart->sMediaType : "application/octet-stream") &&
        xllm__buf_append_cstr(pBuf, ";base64,") &&
        xllm__buf_append_cstr(pBuf, sEncoded);
    xrtFree(sEncoded);
    return bOk;
}

static const char* xllm__media_subtype(const char* sMediaType)
{
    const char* sSlash = sMediaType ? strchr(sMediaType, '/') : NULL;
    return sSlash ? sSlash + 1 : NULL;
}

/* Serialize one content part as a completions content-array element. */
static bool xllm__append_part_object(xllm_buf* pBuf, const xllm_part* pPart, xllm_error* pError)
{
    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"text\",\"text\":") ||
                 !xllm__json_string(pBuf, pPart->sText ? pPart->sText : "") ||
                 !xllm__buf_append_char(pBuf, '}') ) { return false; }
            return true;
        case XLLM_PART_IMAGE:
            if ( pPart->sSourceUrl && pPart->sSourceUrl[0] ) {
                if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"image_url\",\"image_url\":{\"url\":") ||
                     !xllm__json_string(pBuf, pPart->sSourceUrl) ) { return false; }
            } else if ( pPart->pData && pPart->iDataSize ) {
                xllm_buf tUrl = {0};
                bool bUrl;
                if ( !xllm__append_data_url(&tUrl, pPart) ) { xllm__buf_reset(&tUrl); return false; }
                bUrl = xllm__buf_append_cstr(pBuf, "{\"type\":\"image_url\",\"image_url\":{\"url\":") &&
                    xllm__json_string(pBuf, tUrl.pData ? tUrl.pData : "");
                xllm__buf_reset(&tUrl);
                if ( !bUrl ) { return false; }
            } else {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "image part has neither bytes nor URL");
                return false;
            }
            if ( pPart->sDetail && pPart->sDetail[0] ) {
                if ( !xllm__buf_append_cstr(pBuf, ",\"detail\":") ||
                     !xllm__json_string(pBuf, pPart->sDetail) ) { return false; }
            }
            return xllm__buf_append_cstr(pBuf, "}}");
        case XLLM_PART_AUDIO:
            if ( !pPart->pData || !pPart->iDataSize || !pPart->sMediaType ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "audio part requires bytes and a media type");
                return false;
            }
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"input_audio\",\"input_audio\":{\"data\":") ) { return false; }
            {
                str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
                bool bOk;
                if ( !sEncoded ) { return false; }
                bOk = xllm__json_string(pBuf, sEncoded);
                xrtFree(sEncoded);
                if ( !bOk ) { return false; }
            }
            if ( !xllm__buf_append_cstr(pBuf, ",\"format\":") ||
                 !xllm__json_string(pBuf, xllm__media_subtype(pPart->sMediaType)) ) { return false; }
            return xllm__buf_append_cstr(pBuf, "}}");
        case XLLM_PART_FILE:
            if ( !pPart->pData || !pPart->iDataSize || !pPart->sMediaType ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "file part requires bytes and a media type");
                return false;
            }
            {
                xllm_buf tUrl = {0};
                bool bUrl;
                if ( !xllm__append_data_url(&tUrl, pPart) ) { xllm__buf_reset(&tUrl); return false; }
                bUrl = xllm__buf_append_cstr(pBuf, "{\"type\":\"file\",\"file\":{\"filename\":") &&
                    xllm__json_string(pBuf, pPart->sMediaType) &&
                    xllm__buf_append_cstr(pBuf, ",\"file_data\":") &&
                    xllm__json_string(pBuf, tUrl.pData ? tUrl.pData : "");
                xllm__buf_reset(&tUrl);
                if ( !bUrl ) { return false; }
            }
            return xllm__buf_append_cstr(pBuf, "}}");
        case XLLM_PART_NATIVE:
            /* Spliced verbatim; callers guarantee valid JSON. */
            return xllm__buf_append_cstr(pBuf, pPart->sText ? pPart->sText : "{}");
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "message part has an unsupported kind");
            return false;
    }
}

static bool xllm__append_parts(xllm_buf* pBuf, const xllm_message* pMessage,
    const xllm_completions_dialect* pFlags, xllm_error* pError)
{
    size_t i;
    size_t iUsable = 0u;
    bool bArray = false;
    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        xllm_part_kind eKind = pMessage->pParts[i].eKind;
        if ( eKind == XLLM_PART_REASONING && !pFlags->bReasoningContentField ) { continue; }
        ++iUsable;
        if ( eKind != XLLM_PART_TEXT && eKind != XLLM_PART_REASONING ) { bArray = true; }
    }
    if ( !iUsable ) {
        /* Every part was dialect-irrelevant; fall back to empty text. */
        return xllm__buf_append_cstr(pBuf, "\"\"") ;
    }
    if ( !bArray ) {
        /* Pure text: keep the compact string shape. */
        if ( !xllm__buf_append_char(pBuf, '"') ) { return false; }
        for ( i = 0u; i < pMessage->iPartCount; ++i ) {
            const char* sText = pMessage->pParts[i].sText;
            if ( pMessage->pParts[i].eKind == XLLM_PART_REASONING && !pFlags->bReasoningContentField ) { continue; }
            if ( sText && !xllm__buf_append_cstr(pBuf, sText) ) { return false; }
        }
        return xllm__buf_append_char(pBuf, '"');
    }
    if ( !xllm__buf_append_char(pBuf, '[') ) { return false; }
    {
        bool bFirst = true;
        for ( i = 0u; i < pMessage->iPartCount; ++i ) {
            if ( pMessage->pParts[i].eKind == XLLM_PART_REASONING && !pFlags->bReasoningContentField ) { continue; }
            if ( !bFirst && !xllm__buf_append_char(pBuf, ',') ) { return false; }
            if ( !xllm__append_part_object(pBuf, &pMessage->pParts[i], pError) ) { return false; }
            bFirst = false;
        }
    }
    return xllm__buf_append_char(pBuf, ']');
}

static bool xllm__append_message(xllm_buf* pBuf, const xllm_message* pMessage,
    const xllm_completions_dialect* pFlags, xllm_error* pError)
{
    const char* sRole = xllm__role_name(pMessage->eRole, pFlags);
    size_t i;
    if ( !sRole ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "message has an invalid role");
        return false;
    }
    if ( !xllm__buf_append_cstr(pBuf, "{\"role\":") || !xllm__json_string(pBuf, sRole) ) return false;

    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        if ( !pMessage->sToolCallId || !pMessage->sToolCallId[0] ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "tool message is missing tool_call_id");
            return false;
        }
        if ( !xllm__buf_append_cstr(pBuf, ",\"tool_call_id\":") ||
             !xllm__json_string(pBuf, pMessage->sToolCallId) ) return false;
    }

    if ( pFlags->bReasoningContentField && pMessage->eRole == XLLM_ROLE_ASSISTANT &&
         pMessage->sReasoningContent && pMessage->sReasoningContent[0] ) {
        if ( !xllm__buf_append_cstr(pBuf, ",\"reasoning_content\":") ||
             !xllm__json_string(pBuf, pMessage->sReasoningContent) ) return false;
    }

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u ) {
        if ( !xllm__buf_append_cstr(pBuf, ",\"tool_calls\":[") ) return false;
        for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
            const xllm_tool_call* pCall = &pMessage->pToolCalls[i];
            if ( !pCall->sName || !pCall->sName[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "assistant tool call is missing its function name");
                return false;
            }
            if ( i && !xllm__buf_append_char(pBuf, ',') ) return false;
            if ( !xllm__buf_append_cstr(pBuf, "{\"id\":") ||
                 !xllm__json_string(pBuf, pCall->sId ? pCall->sId : "") ||
                 !xllm__buf_append_cstr(pBuf, ",\"type\":\"function\",\"function\":{\"name\":") ||
                 !xllm__json_string(pBuf, pCall->sName) ||
                 !xllm__buf_append_cstr(pBuf, ",\"arguments\":") ||
                 !xllm__json_string(pBuf, pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                 !xllm__buf_append_cstr(pBuf, "}}") ) return false;
        }
        if ( !xllm__buf_append_char(pBuf, ']') ) return false;
    }

    if ( pMessage->iPartCount > 0u ) {
        if ( !xllm__buf_append_cstr(pBuf, ",\"content\":") ||
             !xllm__append_parts(pBuf, pMessage, pFlags, pError) ) return false;
    } else if ( pMessage->sContent || pMessage->eRole != XLLM_ROLE_ASSISTANT ||
                pMessage->iToolCallCount == 0u ) {
        if ( !xllm__buf_append_cstr(pBuf, ",\"content\":") ||
             !xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : "") ) return false;
    } else if ( !xllm__buf_append_cstr(pBuf, ",\"content\":null") ) {
        return false;
    }
    return xllm__buf_append_char(pBuf, '}');
}

static bool xllm__append_tools(xllm_buf* pBuf, const xllm_completions_dialect* pFlags,
    const xllm_request* pRequest, xllm_error* pError)
{
    size_t i;
    if ( !xllm__buf_append_cstr(pBuf, ",\"tools\":[") ) return false;
    for ( i = 0u; i < pRequest->iToolCount; ++i ) {
        const xllm_tool* pTool = &pRequest->pTools[i];
        const char* sSchema = pTool->sParametersJson ? pTool->sParametersJson : "{\"type\":\"object\",\"properties\":{}}";
        if ( !pTool->sName || !pTool->sName[0] ||
             !xrtJsonValid((xstrview){ sSchema, strlen(sSchema) }) ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "tool has an invalid name or JSON parameter schema");
            return false;
        }
        if ( i && !xllm__buf_append_char(pBuf, ',') ) return false;
        if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"function\",\"function\":{\"name\":") ||
             !xllm__json_string(pBuf, pTool->sName) ||
             !xllm__buf_append_cstr(pBuf, ",\"description\":") ||
             !xllm__json_string(pBuf, pTool->sDescription ? pTool->sDescription : "") ||
             !xllm__buf_append_cstr(pBuf, ",\"parameters\":") ||
             !xllm__buf_append_cstr(pBuf, sSchema) ) return false;
        if ( pTool->bStrict && pFlags->bStrictSchemaField &&
             !xllm__buf_append_cstr(pBuf, ",\"strict\":true") ) return false;
        if ( !xllm__buf_append_cstr(pBuf, "}}") ) return false;
    }
    if ( !xllm__buf_append_char(pBuf, ']') ) return false;

    if ( !xllm__buf_append_cstr(pBuf, ",\"tool_choice\":") ) return false;
    switch ( pRequest->eToolChoice ) {
        case XLLM_TOOL_CHOICE_AUTO: if ( !xllm__json_string(pBuf, "auto") ) return false; break;
        case XLLM_TOOL_CHOICE_NONE: if ( !xllm__json_string(pBuf, "none") ) return false; break;
        case XLLM_TOOL_CHOICE_REQUIRED: if ( !xllm__json_string(pBuf, "required") ) return false; break;
        case XLLM_TOOL_CHOICE_NAMED:
            if ( !pRequest->sNamedTool || !pRequest->sNamedTool[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "named tool choice is missing a tool name");
                return false;
            }
            if ( !xllm__buf_append_cstr(pBuf, "{\"type\":\"function\",\"function\":{\"name\":") ||
                 !xllm__json_string(pBuf, pRequest->sNamedTool) ||
                 !xllm__buf_append_cstr(pBuf, "}}") ) return false;
            break;
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid tool choice");
            return false;
    }
    if ( pFlags->bToolStreamField ) {
        return xllm__buf_append_cstr(pBuf, ",\"tool_stream\":true");
    }
    if ( !pFlags->bParallelToolCallsField ) { return true; }
    return xllm__buf_append_cstr(pBuf,
        pRequest->bParallelToolCalls ? ",\"parallel_tool_calls\":true" : ",\"parallel_tool_calls\":false");
}

static char* xllm__completions_build_request(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError)
{
    xllm_buf tBody = {0};
    xllm_completions_dialect tFlags;
    const char* sModel;
    const char* sEffort;
    const char* sMaxTokensField;
    uint32_t uMaxTokens;
    size_t i;
    char* sResult = NULL;
    tFlags = xllm__completions_flags(pClient);
    sModel = (pRequest->sModel && pRequest->sModel[0]) ? pRequest->sModel : pClient->sModel;
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "no model is configured");
        return NULL;
    }
    if ( pRequest->iMessageCount == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request has no messages");
        return NULL;
    }
    if ( pRequest->sExtraBodyJson && pRequest->sExtraBodyJson[0] &&
         !xrtJsonValid((xstrview){ pRequest->sExtraBodyJson, strlen(pRequest->sExtraBodyJson) }) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "extra body JSON is not a valid JSON object");
        return NULL;
    }
    if ( !xllm__buf_append_cstr(&tBody, "{\"model\":") || !xllm__json_string(&tBody, sModel) ||
         !xllm__buf_append_cstr(&tBody, ",\"messages\":[") ) goto oom;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        if ( i && !xllm__buf_append_char(&tBody, ',') ) goto oom;
        if ( !xllm__append_message(&tBody, &pRequest->pMessages[i], &tFlags, pError) ) goto fail;
    }
    if ( !xllm__buf_append_char(&tBody, ']') ) goto oom;
    /* Wire alignment (pi behavior): never persist this exchange server-side.
     * store governs data retention, not prompt caching. A caller-provided
     * extraBody "store" key wins to avoid duplicate keys in the merge. */
    if ( !pRequest->sExtraBodyJson ||
         strstr(pRequest->sExtraBodyJson, "\"store\"") == NULL ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"store\":false") ) goto oom;
    }

    uMaxTokens = pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pClient->uMaxOutputTokens;
    sMaxTokensField = tFlags.bMaxCompletionTokens ? ",\"max_completion_tokens\":" : ",\"max_tokens\":";
    if ( uMaxTokens && ( !xllm__buf_append_cstr(&tBody, sMaxTokensField) ||
         !xllm__json_u32(&tBody, uMaxTokens) ) ) goto oom;
    if ( pRequest->bHasTemperature ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"temperature\":") || !xllm__json_double(&tBody, pRequest->fTemperature) ) goto oom;
    }
    if ( pRequest->bHasTopP ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"top_p\":") || !xllm__json_double(&tBody, pRequest->fTopP) ) goto oom;
    }
    if ( pRequest->sStop && pRequest->sStop[0] ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"stop\":") ||
             !xllm__json_string(&tBody, pRequest->sStop) ) goto oom;
    }
    if ( pRequest->eJsonMode == XLLM_JSON_OBJECT ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"response_format\":{\"type\":\"json_object\"}") ) goto oom;
    }

    sEffort = (pRequest->sReasoningEffort && pRequest->sReasoningEffort[0])
        ? pRequest->sReasoningEffort : pClient->sReasoningEffort;
    if ( sEffort && sEffort[0] && strcmp(sEffort, "off") != 0 ) {
        if ( tFlags.bThinkingObject ) {
            if ( !xllm__buf_append_cstr(&tBody, ",\"thinking\":{\"type\":\"enabled\",\"clear_thinking\":false}") ) goto oom;
        } else {
            if ( !xllm__buf_append_cstr(&tBody, ",\"reasoning_effort\":") || !xllm__json_string(&tBody, sEffort) ) goto oom;
        }
    }
    if ( pRequest->iToolCount > 0u && !xllm__append_tools(&tBody, &tFlags, pRequest, pError) ) goto fail;
    if ( pRequest->sExtraBodyJson && pRequest->sExtraBodyJson[0] ) {
        /* Shallow-merge a caller-supplied JSON object before closing. */
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
    if ( !pRequest->bStream ) {
        if ( !xllm__buf_append_char(&tBody, '}') ) goto oom;
    } else if ( !tFlags.bStreamOptionsUsage ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"stream\":true}") ) goto oom;
    } else if ( !xllm__buf_append_cstr(&tBody, ",\"stream\":true,\"stream_options\":{\"include_usage\":true}}") ) {
        goto oom;
    }
    sResult = xllm__buf_detach(&tBody);
    if ( !sResult ) goto oom;
    return sResult;

oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
fail:
    xllm__buf_reset(&tBody);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Response decoding                                                   */
/* ------------------------------------------------------------------ */

static size_t xllm__completions_build_auth(const xllm_client* pClient,
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

static void xllm__completions_fill_usage(xllm_usage* pUsage, xvalue* pUsageObject)
{
    xvalue* pPromptDetails;
    xvalue* pCompletionDetails;
    if ( !pUsageObject || xrtValueType(pUsageObject) != XVALUE_OBJECT ) { return; }
    pUsage->uInputTokens = xllm__json_u64(pUsageObject, "prompt_tokens");
    pUsage->uOutputTokens = xllm__json_u64(pUsageObject, "completion_tokens");
    pUsage->uTotalTokens = xllm__json_u64(pUsageObject, "total_tokens");
    pPromptDetails = xllm__json_get(pUsageObject, "prompt_tokens_details");
    pCompletionDetails = xllm__json_get(pUsageObject, "completion_tokens_details");
    pUsage->uCachedInputTokens = xllm__json_u64(pPromptDetails, "cached_tokens");
    pUsage->uReasoningTokens = xllm__json_u64(pCompletionDetails, "reasoning_tokens");
    if ( pUsage->uReasoningTokens == 0u ) {
        pUsage->uReasoningTokens = xllm__json_u64(pUsageObject, "reasoning_tokens");
    }
}

static void xllm__completions_fill_provider_error(xllm_call* pCall, xvalue* pRoot)
{
    xvalue* pError = xllm__json_get(pRoot, "error");
    xvalue* pCode = xllm__json_get(pError, "code");
    xstrview tMessage = xllm__json_text(pError, "message");
    xstrview tCode = xllm__json_text(pError, "code");
    xstrview tType = xllm__json_text(pError, "type");
    int64 iCode = 0;
    if ( !tMessage.Data ) tMessage = xllm__json_text(pRoot, "message");
    if ( !tCode.Data ) {
        if ( !pCode ) pCode = xllm__json_get(pRoot, "code");
        tCode = xllm__json_text(pRoot, "code");
    }
    if ( !tType.Data ) tType = xllm__json_text(pRoot, "type");
    if ( tMessage.Data ) xllm__copy_view(pCall->tError.sProviderMessage,
        sizeof(pCall->tError.sProviderMessage), tMessage);
    if ( tType.Data ) xllm__copy_view(pCall->tError.sProviderType,
        sizeof(pCall->tError.sProviderType), tType);
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

static void xllm__completions_fill_error_body(xllm_call* pCall, xstrview tBody)
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
    xllm__completions_fill_provider_error(pCall, pRoot);
    xrtValueRelease(pRoot);
}

static bool xllm__completions_apply_choice(xllm_call* pCall, xvalue* pChoice)
{
    xvalue* pDelta = xllm__json_get(pChoice, "delta");
    xvalue* pToolCalls;
    xstrview tContent;
    xstrview tReasoning;
    xstrview tRefusal;
    xllm_response* pResponse = xllm__assemble_ensure(pCall);
    uint32_t i;
    if ( !pResponse ) { return false; }
    if ( !pDelta || xrtValueType(pDelta) != XVALUE_OBJECT ) {
        /* Non-streaming messages carry the same fields one level up. */
        pDelta = xllm__json_get(pChoice, "message");
    }
    tContent = xllm__json_text(pDelta, "content");
    tReasoning = xllm__json_text(pDelta, "reasoning_content");
    if ( !tReasoning.Data ) tReasoning = xllm__json_text(pDelta, "reasoning");
    if ( !tReasoning.Data ) tReasoning = xllm__json_text(pDelta, "thinking");
    tRefusal = xllm__json_text(pDelta, "refusal");
    if ( tRefusal.Data && !xllm__assemble_refusal(pCall, tRefusal) ) { return false; }
    if ( tContent.Size ) {
        if ( !xllm__assemble_text(pCall, XLLM_BLOCK_TEXT, tContent, NULL) ) { return false; }
    }
    if ( tReasoning.Size ) {
        if ( !xllm__assemble_text(pCall, XLLM_BLOCK_REASONING, tReasoning, NULL) ) { return false; }
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
            if ( !xllm__assemble_tool(pCall, iIndex,
                    xllm__json_text(pToolCall, "id"),
                    xllm__json_text(pFunction, "name"),
                    xllm__json_text(pFunction, "arguments")) ) { return false; }
        }
    }
    {
        xstrview tFinish = xllm__json_text(pChoice, "finish_reason");
        if ( tFinish.Data ) { xllm__assemble_finish(pCall, tFinish); }
    }
    return true;
}

static bool xllm__completions_decode_root(xllm_call* pCall, xvalue* pRoot)
{
    xvalue* pChoices;
    xllm_response* pResponse;
    xstrview tId;
    xstrview tModel;
    xllm_usage tUsage;
    if ( xllm__json_get(pRoot, "error") ) {
        xllm__completions_fill_provider_error(pCall, pRoot);
        return false;
    }
    pResponse = xllm__assemble_ensure(pCall);
    if ( !pResponse ) { return false; }
    tId = xllm__json_text(pRoot, "id");
    tModel = xllm__json_text(pRoot, "model");
    if ( tId.Data && !pResponse->sId ) {
        char* sId = (char*)xllm__malloc(tId.Size + 1u);
        if ( !sId ) goto oom;
        memcpy(sId, tId.Data, tId.Size);
        sId[tId.Size] = 0;
        xllm__free(pResponse->sId);
        pResponse->sId = sId;
    }
    if ( tModel.Data && !pResponse->sModel ) {
        char* sModel = (char*)xllm__malloc(tModel.Size + 1u);
        if ( !sModel ) goto oom;
        memcpy(sModel, tModel.Data, tModel.Size);
        sModel[tModel.Size] = 0;
        xllm__free(pResponse->sModel);
        pResponse->sModel = sModel;
    }
    pChoices = xllm__json_get(pRoot, "choices");
    if ( pChoices && xrtValueType(pChoices) == XVALUE_ARRAY ) {
        size_t uCount = xrtValueCount(pChoices);
        uint32_t i;
        for ( i = 0u; i < uCount; ++i ) {
            xvalue* pChoice = xrtValueArrayGet(pChoices, i);
            xvalue* pIndex = xllm__json_get(pChoice, "index");
            int64 iIndex = 0;
            if ( pIndex && xrtValueGetInt(pIndex, &iIndex) && iIndex != 0 ) continue;
            if ( !xllm__completions_apply_choice(pCall, pChoice) ) { return false; }
            break;
        }
    }
    memset(&tUsage, 0, sizeof(tUsage));
    xllm__completions_fill_usage(&tUsage, xllm__json_get(pRoot, "usage"));
    if ( tUsage.uTotalTokens || tUsage.uInputTokens || tUsage.uOutputTokens ) {
        if ( !xllm__assemble_usage(pCall, &tUsage) ) { return false; }
    }
    pCall->bSawEvent = true;
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to copy provider metadata");
    return false;
}

static bool xllm__completions_decode_sse(xllm_call* pCall, const xllm_sse_fields* pFields)
{
    xvalue* pRoot;
    bool bOk;
    if ( !pCall || !pFields ) { return false; }
    if ( pFields->tData.Size == 6u && memcmp(pFields->tData.Data, "[DONE]", 6u) == 0 ) {
        pCall->bDone = true;
        return true;
    }
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
    bOk = xllm__completions_decode_root(pCall, pRoot);
    xrtValueRelease(pRoot);
    return bOk;
}

static bool xllm__completions_decode_json(xllm_call* pCall, xstrview tBody)
{
    xvalue* pRoot;
    bool bOk;
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
    bOk = xllm__completions_decode_root(pCall, pRoot);
    xrtValueRelease(pRoot);
    return bOk;
}

static const xllm_dialect_ops XLLM_COMPLETIONS_DIALECT = {
    "completions",
    "/chat/completions",
    xllm__completions_build_auth,
    xllm__completions_build_request,
    xllm__completions_decode_sse,
    xllm__completions_decode_json,
    xllm__completions_fill_error_body,
    NULL
};

const xllm_dialect_ops* xllm__dialect_completions(void)
{
    return &XLLM_COMPLETIONS_DIALECT;
}
