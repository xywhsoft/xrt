#include "xllm_internal.h"

static const char* xllm__role_name(xllm_role eRole)
{
    switch ( eRole ) {
        case XLLM_ROLE_SYSTEM: return "system";
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

static bool xllm__append_message(xllm_buf* pBuf, const xllm_message* pMessage, xllm_error* pError)
{
    const char* sRole = xllm__role_name(pMessage->eRole);
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

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->sReasoningContent && pMessage->sReasoningContent[0] ) {
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

    if ( pMessage->sContent || pMessage->eRole != XLLM_ROLE_ASSISTANT || pMessage->iToolCallCount == 0u ) {
        if ( !xllm__buf_append_cstr(pBuf, ",\"content\":") ||
             !xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : "") ) return false;
    } else if ( !xllm__buf_append_cstr(pBuf, ",\"content\":null") ) {
        return false;
    }
    return xllm__buf_append_char(pBuf, '}');
}

static bool xllm__append_tools(xllm_buf* pBuf, const xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError)
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
        if ( pTool->bStrict && pClient->eProvider != XLLM_PROVIDER_GLM &&
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
    if ( pClient->eProvider == XLLM_PROVIDER_GLM ) {
        return xllm__buf_append_cstr(pBuf, ",\"tool_stream\":true");
    }
    return xllm__buf_append_cstr(pBuf,
        pRequest->bParallelToolCalls ? ",\"parallel_tool_calls\":true" : ",\"parallel_tool_calls\":false");
}

char* xllm__build_request_json(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError)
{
    xllm_buf tBody = {0};
    const char* sModel;
    const char* sEffort;
    uint32_t uMaxTokens;
    size_t i;
    char* sResult = NULL;
    if ( !pClient || !pRequest ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client and request are required");
        return NULL;
    }
    sModel = (pRequest->sModel && pRequest->sModel[0]) ? pRequest->sModel : pClient->sModel;
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "no model is configured");
        return NULL;
    }
    if ( pRequest->iMessageCount == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request has no messages");
        return NULL;
    }
    if ( !xllm__buf_append_cstr(&tBody, "{\"model\":") || !xllm__json_string(&tBody, sModel) ||
         !xllm__buf_append_cstr(&tBody, ",\"messages\":[") ) goto oom;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        if ( i && !xllm__buf_append_char(&tBody, ',') ) goto oom;
        if ( !xllm__append_message(&tBody, &pRequest->pMessages[i], pError) ) goto fail;
    }
    if ( !xllm__buf_append_char(&tBody, ']') ) goto oom;

    uMaxTokens = pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pClient->uMaxOutputTokens;
    if ( uMaxTokens ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"max_tokens\":") || !xllm__json_u32(&tBody, uMaxTokens) ) goto oom;
    }
    if ( pRequest->bHasTemperature ) {
        if ( !xllm__buf_append_cstr(&tBody, ",\"temperature\":") || !xllm__json_double(&tBody, pRequest->fTemperature) ) goto oom;
    }

    sEffort = (pRequest->sReasoningEffort && pRequest->sReasoningEffort[0])
        ? pRequest->sReasoningEffort : pClient->sReasoningEffort;
    if ( sEffort && sEffort[0] && strcmp(sEffort, "off") != 0 ) {
        if ( pClient->eProvider == XLLM_PROVIDER_GLM ) {
            if ( !xllm__buf_append_cstr(&tBody, ",\"thinking\":{\"type\":\"enabled\",\"clear_thinking\":false}") ) goto oom;
        } else {
            if ( !xllm__buf_append_cstr(&tBody, ",\"reasoning_effort\":") || !xllm__json_string(&tBody, sEffort) ) goto oom;
        }
    }
    if ( pRequest->iToolCount > 0u && !xllm__append_tools(&tBody, pClient, pRequest, pError) ) goto fail;
    if ( pClient->eProvider == XLLM_PROVIDER_GLM ) {
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

char* xllmClientBuildRequestJson(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError)
{
    if ( pError ) { xllmErrorInit(pError); }
    return xllm__build_request_json(pClient, pRequest, pError);
}
