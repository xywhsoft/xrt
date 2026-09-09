#include "xllm_adapter.h"

#include <string.h>

static const char *xllm__minimax_provider_name(const xllm_profile *pProfile)
{
    if ( pProfile && pProfile->sProvider && pProfile->sProvider[0] ) {
        return pProfile->sProvider;
    }
    return "minimax";
}

static char *xllm__minimax_build_url(const xllm_profile *pProfile)
{
    const char *sBaseUrl = NULL;
    static const char sDefaultUrl[] = "https://api.minimaxi.com/v1/text/chatcompletion_v2";
    const char *sSuffix = NULL;
    size_t iLen;
    bool bNeedsSlash;
    char *sUrl;

    if ( pProfile ) {
        sBaseUrl = pProfile->sBaseUrl;
    }
    if ( !sBaseUrl || !sBaseUrl[0] ) {
        return xllm__dup_cstr(sDefaultUrl);
    }
    if ( strstr(sBaseUrl, "/chatcompletion_v2") != NULL ) {
        return xllm__dup_cstr(sBaseUrl);
    }
    sSuffix = (strstr(sBaseUrl, "/text") != NULL) ? "chatcompletion_v2" : "text/chatcompletion_v2";
    iLen = strlen(sBaseUrl);
    bNeedsSlash = (iLen > 0u && sBaseUrl[iLen - 1u] != '/');
    sUrl = (char *)xrtCalloc(iLen + (bNeedsSlash ? 1u : 0u) + strlen(sSuffix) + 1u, sizeof(char));

    if ( !sUrl ) {
        return NULL;
    }
    (void)snprintf(
        sUrl,
        iLen + (bNeedsSlash ? 1u : 0u) + strlen(sSuffix) + 1u,
        "%s%s%s",
        sBaseUrl,
        bNeedsSlash ? "/" : "",
        sSuffix
    );
    return sUrl;
}

static bool xllm__minimax_part_is_native_supported(const xllm_content_part *pPart)
{
    if ( !pPart ) {
        return false;
    }

    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
        case XLLM_PART_JSON:
            return true;
        case XLLM_PART_IMAGE:
            return (
                pPart->as.tSource.eKind == XLLM_SOURCE_URL ||
                pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_BYTES
            );
        default:
            return false;
    }
}

static bool xllm__minimax_message_requires_content_array(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage ) {
        return false;
    }
    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        if ( pMessage->pParts[i].eKind == XLLM_PART_IMAGE ) {
            return true;
        }
    }
    return false;
}

static bool xllm__minimax_message_has_unsupported_parts(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage ) {
        return false;
    }
    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        if ( !xllm__minimax_part_is_native_supported(&pMessage->pParts[i]) ) {
            return true;
        }
    }
    return false;
}

static int xllm__minimax_append_content_part(
    xllm__json_builder *pBuilder,
    const xllm_content_part *pPart,
    xllm_error *pError
)
{
    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
            if ( pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "minimax native text part must be inline text");
                return XRT_NET_ERROR;
            }
            return (
                xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"text\",\"text\":") &&
                xllm__json_builder_append_escaped(pBuilder, pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "") &&
                xllm__json_builder_append_char(pBuilder, '}')
            ) ? XRT_NET_OK : XRT_NET_ERROR;

        case XLLM_PART_JSON: {
            char *sJson = (char *)xrtStringifyJSON(pPart->as.tJsonValue, FALSE, NULL);
            bool bOk;

            if ( !sJson ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify minimax json part");
                return XRT_NET_ERROR;
            }
            bOk = (
                xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"text\",\"text\":") &&
                xllm__json_builder_append_escaped(pBuilder, sJson) &&
                xllm__json_builder_append_char(pBuilder, '}')
            );
            xrtFree(sJson);
            return bOk ? XRT_NET_OK : XRT_NET_ERROR;
        }

        case XLLM_PART_IMAGE:
            switch ( pPart->as.tSource.eKind ) {
                case XLLM_SOURCE_URL:
                    return (
                        xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"image_url\",\"image_url\":{\"url\":") &&
                        xllm__json_builder_append_escaped(
                            pBuilder,
                            pPart->as.tSource.as.sUrl ? pPart->as.tSource.as.sUrl : ""
                        ) &&
                        xllm__json_builder_append_cstr(pBuilder, "}}")
                    ) ? XRT_NET_OK : XRT_NET_ERROR;

                case XLLM_SOURCE_INLINE_BYTES: {
                    const char *sMimeType = pPart->as.tSource.sMimeType ? pPart->as.tSource.sMimeType : "image/png";
                    char *sBase64 = NULL;
                    bool bOk;

                    if ( !pPart->as.tSource.as.tBytes.pData || pPart->as.tSource.as.tBytes.iSize == 0u ) {
                        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "minimax native image bytes input is empty");
                        return XRT_NET_ERROR;
                    }

                    sBase64 = (char *)xrtBase64Encode(
                        (ptr)pPart->as.tSource.as.tBytes.pData,
                        pPart->as.tSource.as.tBytes.iSize,
                        NULL
                    );
                    if ( !sBase64 ) {
                        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode minimax image bytes");
                        return XRT_NET_ERROR;
                    }

                    bOk = (
                        xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"image_url\",\"image_url\":{\"url\":") &&
                        xllm__json_builder_append_char(pBuilder, '"') &&
                        xllm__json_builder_append_cstr(pBuilder, "data:") &&
                        xllm__json_builder_append_cstr(pBuilder, sMimeType) &&
                        xllm__json_builder_append_cstr(pBuilder, ";base64,") &&
                        xllm__json_builder_append_cstr(pBuilder, sBase64) &&
                        xllm__json_builder_append_char(pBuilder, '"') &&
                        xllm__json_builder_append_cstr(pBuilder, "}}")
                    );
                    xrtFree(sBase64);
                    return bOk ? XRT_NET_OK : XRT_NET_ERROR;
                }

                default:
                    xllm__error_set(
                        pError,
                        XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                        "minimax native image input only supports url or inline bytes"
                    );
                    return XRT_NET_ERROR;
            }

        default:
            xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "minimax native adapter unsupported content part");
            return XRT_NET_ERROR;
    }
}

static int xllm__minimax_append_message(
    xllm__json_builder *pBuilder,
    const xllm_message *pMessage,
    bool *pbNeedComma,
    xllm_error *pError
)
{
    const char *sRole;
    char *sContent = NULL;
    int iStatus = XRT_NET_ERROR;
    bool bNeedsContent = true;
    bool bUseContentArray = false;
    size_t i;

    if ( !pBuilder || !pMessage || !pbNeedComma ) {
        return XRT_NET_ERROR;
    }

    sRole = xllm__openai_role_name(pMessage->eRole);
    if ( !sRole ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "minimax native adapter encountered unknown role");
        return XRT_NET_ERROR;
    }
    if ( xllm__minimax_message_has_unsupported_parts(pMessage) ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "minimax native adapter only supports text/json/image inputs");
        return XRT_NET_ERROR;
    }
    bUseContentArray = xllm__minimax_message_requires_content_array(pMessage);
    if ( !bUseContentArray &&
         xllm__openai_message_to_text(pMessage, &sContent, pError) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( *pbNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
        goto done;
    }
    if ( !xllm__json_builder_append_cstr(pBuilder, "{\"role\":") ||
         !xllm__json_builder_append_escaped(pBuilder, sRole) ) {
        goto done;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL && pMessage->sToolCallId && pMessage->sToolCallId[0] ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_call_id\":") ||
             !xllm__json_builder_append_escaped(pBuilder, pMessage->sToolCallId) ) {
            goto done;
        }
    }

    if ( pMessage->eRole != XLLM_ROLE_TOOL ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"name\":") ||
             !xllm__json_builder_append_escaped(pBuilder, sRole) ) {
            goto done;
        }
    }

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_calls\":[") ) {
            goto done;
        }
        for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
            const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
            const char *sToolName = pCall->sToolName ? pCall->sToolName : pCall->sToolId;

            if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
                goto done;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"id\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pCall->sCallId ? pCall->sCallId : "") ||
                 !xllm__json_builder_append_cstr(pBuilder, ",\"type\":\"function\",\"function\":{\"name\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sToolName ? sToolName : "") ||
                 !xllm__json_builder_append_cstr(pBuilder, ",\"arguments\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                goto done;
            }
        }
        if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
            goto done;
        }
        if ( !sContent || !sContent[0] ) {
            bNeedsContent = false;
        }
    }

    if ( bNeedsContent ) {
        if ( bUseContentArray ) {
            bool bNeedPartComma = false;

            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"content\":") ||
                 !xllm__json_builder_append_char(pBuilder, '[') ) {
                goto done;
            }
            for ( i = 0u; i < pMessage->iPartCount; ++i ) {
                if ( bNeedPartComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
                    goto done;
                }
                if ( xllm__minimax_append_content_part(pBuilder, &pMessage->pParts[i], pError) != XRT_NET_OK ) {
                    goto done;
                }
                bNeedPartComma = true;
            }
            if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
                goto done;
            }
        } else if ( !xllm__json_builder_append_cstr(pBuilder, ",\"content\":") ||
                    !xllm__json_builder_append_escaped(pBuilder, sContent ? sContent : "") ) {
            goto done;
        }
    }
    if ( !xllm__json_builder_append_char(pBuilder, '}') ) {
        goto done;
    }

    *pbNeedComma = true;
    iStatus = XRT_NET_OK;

done:
    xllm__free_cstr(&sContent);
    return iStatus;
}

static int xllm__minimax_append_messages(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    xllm_error *pError,
    uint32 *puMessageCount
)
{
    size_t i;
    bool bNeedComma = false;
    uint32 uMessageCount = 0u;

    if ( puMessageCount ) {
        *puMessageCount = 0u;
    }
    if ( !pBuilder || !pRequest ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, "\"messages\":[") ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0u; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            if ( xllm__minimax_append_message(pBuilder, &pRequest->pContextBlocks[i].pMessages[j], &bNeedComma, pError) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            ++uMessageCount;
        }
    }
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        if ( xllm__minimax_append_message(pBuilder, &pRequest->pMessages[i], &bNeedComma, pError) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        ++uMessageCount;
    }

    if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
        return XRT_NET_ERROR;
    }
    if ( puMessageCount ) {
        *puMessageCount = uMessageCount;
    }
    return XRT_NET_OK;
}

static int xllm__minimax_append_tools(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest
)
{
    return xllm__openai_append_tools(pBuilder, pRequest);
}

static int xllm__minimax_append_tool_policy(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    xllm_error *pError
)
{
    if ( !pBuilder || !pRequest || pRequest->iToolCount == 0u ) {
        return XRT_NET_OK;
    }

    switch ( pRequest->tToolPolicy.eMode ) {
        case XLLM_TOOL_CHOICE_NONE:
            return xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":\"none\"") ? XRT_NET_OK : XRT_NET_ERROR;
        case XLLM_TOOL_CHOICE_AUTO:
            return xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":\"auto\"") ? XRT_NET_OK : XRT_NET_ERROR;
        case XLLM_TOOL_CHOICE_REQUIRED:
            return xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":\"required\"") ? XRT_NET_OK : XRT_NET_ERROR;
        case XLLM_TOOL_CHOICE_NAMED:
            if ( !pRequest->tToolPolicy.sToolName || !pRequest->tToolPolicy.sToolName[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "minimax native named tool_choice missing tool name");
                return XRT_NET_ERROR;
            }
            return (
                xllm__json_builder_append_cstr(
                    pBuilder,
                    ",\"tool_choice\":{\"type\":\"function\",\"function\":{\"name\":"
                ) &&
                xllm__json_builder_append_escaped(pBuilder, pRequest->tToolPolicy.sToolName) &&
                xllm__json_builder_append_cstr(pBuilder, "}}")
            ) ? XRT_NET_OK : XRT_NET_ERROR;
        default:
            return xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":\"auto\"") ? XRT_NET_OK : XRT_NET_ERROR;
    }
}

static int xllm__minimax_build_body(
    xllm__json_builder *pBody,
    const xllm_request *pRequest,
    const xllm_effective_params *pEffectiveParams,
    const xllm_call_options *pOptions,
    const char *sModel,
    uint32 *puMessageCount,
    xllm_error *pError
)
{
    (void)pOptions;
    if ( puMessageCount ) {
        *puMessageCount = 0u;
    }
    if ( !pBody || !pRequest || !pEffectiveParams || !sModel || !sModel[0] ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBody, '{') ||
         !xllm__json_builder_append_cstr(pBody, "\"model\":") ||
         !xllm__json_builder_append_escaped(pBody, sModel) ||
         !xllm__json_builder_append_char(pBody, ',') ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__minimax_append_messages(pBody, pRequest, pError, puMessageCount) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( pEffectiveParams->tGeneration.tTemperature.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"temperature\":") ||
             !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTemperature.fValue) ) {
            return XRT_NET_ERROR;
        }
    }
    if ( pEffectiveParams->tGeneration.tTopP.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"top_p\":") ||
             !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTopP.fValue) ) {
            return XRT_NET_ERROR;
        }
    }
    if ( pEffectiveParams->tGeneration.tMaxOutputTokens.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"max_completion_tokens\":") ||
             !xllm__json_builder_append_u32(pBody, pEffectiveParams->tGeneration.tMaxOutputTokens.iValue) ) {
            return XRT_NET_ERROR;
        }
    }
    if ( pEffectiveParams->tGeneration.iStopCount > 0u && pEffectiveParams->tGeneration.psStop ) {
        if ( xllm__openai_append_stop(pBody, &pEffectiveParams->tGeneration) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }
    if ( pEffectiveParams->tResponseFormat.eKind == XLLM_RESPONSE_JSON ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"response_format\":{\"type\":\"json_object\"}") ) {
            return XRT_NET_ERROR;
        }
    } else if ( pEffectiveParams->tResponseFormat.eKind == XLLM_RESPONSE_JSON_SCHEMA ) {
        if ( xllm__openai_append_response_format(pBody, &pEffectiveParams->tResponseFormat) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }
    if ( (pEffectiveParams->tReasoning.tEnabled.bSet && pEffectiveParams->tReasoning.tEnabled.bValue) ||
         (pEffectiveParams->tReasoning.tExposeThinking.bSet && pEffectiveParams->tReasoning.tExposeThinking.bValue) ||
         (pEffectiveParams->tReasoning.eLevel != XLLM_REASONING_DEFAULT &&
          pEffectiveParams->tReasoning.eLevel != XLLM_REASONING_OFF) ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"reasoning_split\":true") ) {
            return XRT_NET_ERROR;
        }
    }
    if ( pRequest->iToolCount > 0u ) {
        if ( xllm__minimax_append_tools(pBody, pRequest) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__minimax_append_tool_policy(pBody, pRequest, pError) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }
    if ( pOptions && pOptions->eStreamMode != XLLM_STREAM_OFF ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"stream\":true") ) {
            return XRT_NET_ERROR;
        }
    }

    if ( !xllm__json_builder_append_char(pBody, '}') ) {
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void xllm__minimax_fill_error_from_http(xllm_error *pError, const xhttpresponse *pHttpResponse, xvalue tRoot)
{
    const char *sCode = NULL;
    const char *sMessage = NULL;
    const char *sRequestId = NULL;
    xvalue tBaseResp;

    if ( !pError ) {
        return;
    }

    tBaseResp = xllm__json_table_get(tRoot, "base_resp");
    if ( tBaseResp && xvoType(tBaseResp) == XVO_DT_TABLE ) {
        sCode = xllm__json_table_get_text(tBaseResp, "status_code");
        sMessage = xllm__json_table_get_text(tBaseResp, "status_msg");
    }
    if ( !sMessage || !sMessage[0] ) {
        sCode = xllm__json_table_get_text(tRoot, "code");
        sMessage = xllm__json_table_get_text(tRoot, "message");
    }
    if ( !sMessage || !sMessage[0] ) {
        sMessage = "minimax native request failed";
    }

    if ( pHttpResponse && (pHttpResponse->iStatusCode == 401u || pHttpResponse->iStatusCode == 403u) ) {
        xllm__error_set(pError, XLLM_ERROR_AUTH, sMessage);
    } else if ( pHttpResponse && pHttpResponse->iStatusCode == 404u ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, sMessage);
    } else if ( pHttpResponse && pHttpResponse->iStatusCode == 429u ) {
        xllm__error_set(pError, XLLM_ERROR_RATE_LIMIT, sMessage);
    } else if ( pHttpResponse && pHttpResponse->iStatusCode >= 500u ) {
        xllm__error_set(pError, XLLM_ERROR_UPSTREAM_5XX, sMessage);
    } else if ( pHttpResponse && pHttpResponse->iStatusCode >= 400u ) {
        xllm__error_set(pError, XLLM_ERROR_UPSTREAM_4XX, sMessage);
    } else {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, sMessage);
    }

    if ( pHttpResponse ) {
        pError->iHttpStatus = (int32)pHttpResponse->iStatusCode;
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    }
    if ( sCode && sCode[0] ) {
        pError->sProviderCode = xllm__dup_cstr(sCode);
    }
    if ( sMessage && sMessage[0] ) {
        pError->sProviderMessage = xllm__dup_cstr(sMessage);
    }
    if ( sRequestId && sRequestId[0] ) {
        pError->sRequestId = xllm__dup_cstr(sRequestId);
    }
}

static xllm_response_status xllm__minimax_status_from_finish_reason(const char *sFinishReason)
{
    if ( !sFinishReason || !sFinishReason[0] ) {
        return XLLM_STATUS_COMPLETED;
    }
    if ( strcmp(sFinishReason, "length") == 0 || strcmp(sFinishReason, "max_tokens") == 0 ) {
        return XLLM_STATUS_INCOMPLETE;
    }
    if ( strcmp(sFinishReason, "tool_calls") == 0 ) {
        return XLLM_STATUS_TOOL_CALL_REQUIRED;
    }
    if ( strcmp(sFinishReason, "sensitive") == 0 || strcmp(sFinishReason, "content_filter") == 0 ) {
        return XLLM_STATUS_CONTENT_FILTERED;
    }
    return XLLM_STATUS_COMPLETED;
}

static int xllm__minimax_parse_response(
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_effective_params *pEffectiveParams,
    const char *sSelectedModel,
    const char *sBody,
    size_t iBodyLen,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xvalue tRoot = NULL;
    xvalue tChoices;
    xvalue tChoice;
    xvalue tMessage;
    xvalue tToolCalls;
    xvalue tUsage;
    xvalue tCompletionTokenDetails;
    xvalue tReasoningDetails;
    xvalue tJsonValue = NULL;
    char *sNormalizedJson = NULL;
    const char *sText = NULL;
    const char *sReasoningText = NULL;
    const char *sFinishReason = NULL;
    xllm_response *pResponse = NULL;
    size_t i;
    size_t iOutputCap = 0u;
    size_t iMessagePartCap = 0u;
    size_t iMessageOutputIndex = (size_t)-1;

    if ( !ppResponse || !pEffectiveParams ) {
        return XRT_NET_ERROR;
    }
    *ppResponse = NULL;

    tRoot = xrtParseJSON((str)sBody, iBodyLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "invalid minimax native response");
        return XRT_NET_ERROR;
    }

    tChoices = xllm__json_table_get(tRoot, "choices");
    tChoice = (tChoices && xvoType(tChoices) == XVO_DT_ARRAY && xvoArrayItemCount(tChoices) > 0u)
        ? xvoArrayGetValue(tChoices, 0u)
        : NULL;
    tMessage = xllm__json_table_get(tChoice, "message");
    tToolCalls = xllm__json_table_get(tMessage, "tool_calls");
    tReasoningDetails = xllm__json_table_get(tMessage, "reasoning_details");
    sText = xllm__json_table_get_text(tMessage, "content");
    if ( !sText || !sText[0] ) {
        sText = xllm__json_table_get_text(tMessage, "text");
    }
    sReasoningText = xllm__json_table_get_text(tMessage, "reasoning_content");
    if ( (!sReasoningText || !sReasoningText[0]) &&
         tReasoningDetails &&
         xvoType(tReasoningDetails) == XVO_DT_ARRAY &&
         xvoArrayItemCount(tReasoningDetails) > 0u ) {
        xvalue tReasoningItem = xvoArrayGetValue(tReasoningDetails, 0u);
        sReasoningText = xllm__json_table_get_text(tReasoningItem, "text");
        if ( (!sReasoningText || !sReasoningText[0]) && xvoType(tReasoningItem) == XVO_DT_TEXT ) {
            sReasoningText = (const char *)xvoGetText(tReasoningItem);
        }
    }
    sFinishReason = xllm__json_table_get_text(tChoice, "finish_reason");

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }

    pResponse->sId = xllm__dup_cstr(xllm__json_table_get_text(tRoot, "id"));
    pResponse->sProvider = xllm__dup_cstr(xllm__minimax_provider_name(pProfile));
    pResponse->sProfileId = xllm__dup_cstr(pProfile ? pProfile->sId : NULL);
    pResponse->sModel = xllm__dup_cstr(xllm__json_table_get_text(tRoot, "model"));
    if ( !pResponse->sModel ) {
        pResponse->sModel = xllm__dup_cstr(sSelectedModel);
    }
    pResponse->sFinishReason = xllm__dup_cstr(sFinishReason ? sFinishReason : "stop");
    pResponse->eStatus = xllm__minimax_status_from_finish_reason(sFinishReason);
    pResponse->tEffectiveParams = *pEffectiveParams;
    memset(pEffectiveParams, 0, sizeof(*pEffectiveParams));

    if ( sReasoningText && sReasoningText[0] ) {
        xllm_output_item tThinkingOutput;

        memset(&tThinkingOutput, 0, sizeof(tThinkingOutput));
        tThinkingOutput.eKind = XLLM_OUTPUT_THINKING;
        tThinkingOutput.as.tThinking.bVisible = true;
        tThinkingOutput.as.tThinking.sFormat = xllm__dup_cstr("full");
        tThinkingOutput.as.tThinking.sText = xllm__dup_cstr(sReasoningText);
        if ( xllm__append_buffer((void **)&pResponse->pOutputs, sizeof(xllm_output_item), &pResponse->iOutputCount, &iOutputCap, &tThinkingOutput) != XRT_NET_OK ) {
            xllm__output_item_free(&tThinkingOutput);
            goto fail;
        }
    }

    if ( sText && sText[0] ) {
        xllm_output_item tMessageOutput;
        xllm_content_part tPart;

        memset(&tMessageOutput, 0, sizeof(tMessageOutput));
        tMessageOutput.eKind = XLLM_OUTPUT_MESSAGE;
        if ( xllm__append_buffer((void **)&pResponse->pOutputs, sizeof(xllm_output_item), &pResponse->iOutputCount, &iOutputCap, &tMessageOutput) != XRT_NET_OK ) {
            goto fail;
        }
        iMessageOutputIndex = pResponse->iOutputCount - 1u;

        memset(&tPart, 0, sizeof(tPart));
        tPart.eKind = XLLM_PART_TEXT;
        tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
        tPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
        tPart.as.tSource.as.sText = xllm__dup_cstr(sText);
        if ( xllm__append_buffer(
                 (void **)&pResponse->pOutputs[iMessageOutputIndex].as.tMessage.pParts,
                 sizeof(xllm_content_part),
                 &pResponse->pOutputs[iMessageOutputIndex].as.tMessage.iPartCount,
                 &iMessagePartCap,
                 &tPart) != XRT_NET_OK ) {
            xllm__content_part_free(&tPart);
            goto fail;
        }

        pResponse->sVisibleText = xllm__dup_cstr(sText);
    } else {
        pResponse->sVisibleText = xllm__dup_cstr("");
    }

    if ( tToolCalls && xvoType(tToolCalls) == XVO_DT_ARRAY ) {
        for ( i = 0u; i < xvoArrayItemCount(tToolCalls); ++i ) {
            xvalue tToolCall = xvoArrayGetValue(tToolCalls, (uint32)i);
            xvalue tFunction = xllm__json_table_get(tToolCall, "function");
            xllm_output_item tToolOutput;
            char *sArgsJson = NULL;
            const char *sToolName = xllm__json_table_get_text(tFunction, "name");
            const char *sCallId = xllm__json_table_get_text(tToolCall, "id");

            memset(&tToolOutput, 0, sizeof(tToolOutput));
            tToolOutput.eKind = XLLM_OUTPUT_TOOL_CALL;
            tToolOutput.as.tToolCall.sCallId = xllm__dup_cstr(sCallId ? sCallId : "minimax_call");
            tToolOutput.as.tToolCall.sToolId = xllm__dup_cstr(sToolName ? sToolName : "");
            tToolOutput.as.tToolCall.sToolName = xllm__dup_cstr(sToolName ? sToolName : "");

            if ( tFunction ) {
                xvalue tArgs = xllm__json_table_get(tFunction, "arguments");
                const char *sArgsText = xllm__json_table_get_text(tFunction, "arguments");
                if ( sArgsText && sArgsText[0] ) {
                    sArgsJson = xllm__dup_cstr(sArgsText);
                } else if ( tArgs ) {
                    sArgsJson = (char *)xrtStringifyJSON(tArgs, FALSE, NULL);
                }
            }
            tToolOutput.as.tToolCall.sArgumentsJson = sArgsJson ? sArgsJson : xllm__dup_cstr("{}");

            if ( xllm__append_buffer((void **)&pResponse->pOutputs, sizeof(xllm_output_item), &pResponse->iOutputCount, &iOutputCap, &tToolOutput) != XRT_NET_OK ) {
                xllm__output_item_free(&tToolOutput);
                goto fail;
            }
            pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
        }
    }

    if ( pRequest &&
         pResponse->sVisibleText &&
         pResponse->sVisibleText[0] &&
         pResponse->tEffectiveParams.tResponseFormat.eKind != XLLM_RESPONSE_TEXT &&
         iMessageOutputIndex != (size_t)-1 ) {
        int iStructuredStatus = xllm__openai_parse_structured_output(
            &pResponse->tEffectiveParams.tResponseFormat,
            pOptions,
            pResponse->sVisibleText,
            &tJsonValue,
            &sNormalizedJson,
            pError
        );

        if ( iStructuredStatus != XRT_NET_OK &&
             (!pOptions || !pOptions->bBestEffortStructuredOutput) ) {
            xllm_call_options tFallbackOptions;

            xllm_call_options_init(&tFallbackOptions);
            if ( pOptions ) {
                tFallbackOptions = *pOptions;
            }
            tFallbackOptions.bBestEffortStructuredOutput = true;
            xllm_error_free(pError);
            xllm_error_init(pError);
            xllm__free_cstr(&sNormalizedJson);
            xllm__xvalue_release(&tJsonValue);

            iStructuredStatus = xllm__openai_parse_structured_output(
                &pResponse->tEffectiveParams.tResponseFormat,
                &tFallbackOptions,
                pResponse->sVisibleText,
                &tJsonValue,
                &sNormalizedJson,
                pError
            );
        }
        if ( iStructuredStatus != XRT_NET_OK ) {
            goto fail;
        }
        if ( tJsonValue ) {
            xllm_content_part *pPart = &pResponse->pOutputs[iMessageOutputIndex].as.tMessage.pParts[0u];

            xllm__content_part_free(pPart);
            memset(pPart, 0, sizeof(*pPart));
            pPart->eKind = XLLM_PART_JSON;
            pPart->as.tJsonValue = tJsonValue;
            tJsonValue = NULL;
            xllm__free_cstr((char **)&pResponse->sVisibleText);
            pResponse->sVisibleText = sNormalizedJson ? sNormalizedJson : xllm__dup_cstr("");
            sNormalizedJson = NULL;
        }
    }

    tUsage = xllm__json_table_get(tRoot, "usage");
    if ( tUsage && xvoType(tUsage) == XVO_DT_TABLE ) {
        pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tUsage, "total_prompt_tokens");
        if ( pResponse->tUsage.uInputTokens == 0u ) {
            pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tUsage, "prompt_tokens");
        }
        pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tUsage, "total_completion_tokens");
        if ( pResponse->tUsage.uOutputTokens == 0u ) {
            pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tUsage, "completion_tokens");
        }
        pResponse->tUsage.uReasoningTokens = xllm__json_table_get_u32(tUsage, "reasoning_tokens");
        if ( pResponse->tUsage.uReasoningTokens == 0u ) {
            tCompletionTokenDetails = xllm__json_table_get(tUsage, "completion_tokens_details");
            if ( tCompletionTokenDetails && xvoType(tCompletionTokenDetails) == XVO_DT_TABLE ) {
                pResponse->tUsage.uReasoningTokens = xllm__json_table_get_u32(tCompletionTokenDetails, "reasoning_tokens");
            }
        }
    }

    if ( xllm__openai_apply_terminal_status(pResponse, true, false) != XRT_NET_OK ) {
        goto fail;
    }

    pResponse->tRaw = tRoot;
    *ppResponse = pResponse;
    xllm__free_cstr(&sNormalizedJson);
    xllm__xvalue_release(&tJsonValue);
    return XRT_NET_OK;

fail:
    xllm__free_cstr(&sNormalizedJson);
    xllm__xvalue_release(&tJsonValue);
    xllm_response_free(pResponse);
    xllm__xvalue_release(&tRoot);
    return XRT_NET_ERROR;
}

static int32 xllm__minimax_native_chat_direct(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_runtime *pRuntime = (xllm_runtime *)pCtx;
    xllm__json_builder tBody;
    xllm_effective_params tEffectiveParams;
    xhttprequest tHttpRequest;
    xhttpresponse *pHttpResponse = NULL;
    xllm_response *pResponse = NULL;
    char *sBody = NULL;
    char *sUrl = NULL;
    const char *sModel;
    const char *sRequestId = NULL;
    bool bStreaming = false;
    xnet_result iNetStatus = XRT_NET_ERROR;
    int iStatus = XRT_NET_ERROR;
    bool bRetryable = false;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts = 1u;
    uint32 uMessageCount = 0u;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    *ppResponse = NULL;

    memset(&tBody, 0, sizeof(tBody));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    xrtHttpRequestInit(&tHttpRequest);

    if ( pOptions && pOptions->uMaxRetries > 0u ) {
        uMaxAttempts += pOptions->uMaxRetries;
    }
    bStreaming = (pOptions && pOptions->eStreamMode != XLLM_STREAM_OFF);

    sModel = xllm__openai_select_model(pProfile, pRequest, NULL);
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for minimax native request");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( xllm__openai_fill_effective_params(
            &tEffectiveParams,
            pProfile,
            pRequest,
            pOptions ? pOptions->eStreamMode : XLLM_STREAM_OFF
         ) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__minimax_build_body(&tBody, pRequest, &tEffectiveParams, pOptions, sModel, &uMessageCount, pError) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    sUrl = xllm__minimax_build_url(pProfile);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "minimax native profile missing base url");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) goto fail;
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) goto fail;
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) goto fail;
    if ( xllm__openai_fill_request_headers(&tHttpRequest, pProfile) != XRT_NET_OK ) goto fail;
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) goto fail;

retry_execute:
    ++uAttempt;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__openai_family_component_name(pProfile),
        "request start: model=%s streaming=%s attempt=%u/%u body_bytes=%u",
        sModel,
        bStreaming ? "true" : "false",
        (unsigned)uAttempt,
        (unsigned)uMaxAttempts,
        (unsigned)strlen(sBody)
    );
    xllm__openai_trace_request(
        pRuntime,
        pProfile,
        pRequest,
        sModel,
        bStreaming,
        false,
        uAttempt,
        strlen(sBody)
    );

    pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
    if ( !pHttpResponse ) {
        if ( iNetStatus == XRT_NET_TIMEOUT ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "minimax native request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "minimax native request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "minimax native request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    if ( pHttpResponse->iStatusCode >= 400u ) {
        xvalue tErrorRoot = NULL;
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tErrorRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__minimax_fill_error_from_http(pError, pHttpResponse, tErrorRoot);
        xllm__xvalue_release(&tErrorRoot);
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "minimax native response body is empty");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__minimax_parse_response(
            pProfile,
            pRequest,
            pOptions,
            &tEffectiveParams,
            sModel,
            (const char *)pHttpResponse->pBody,
            pHttpResponse->iBodyLen,
            &pResponse,
            pError
         ) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__openai_emit_synthetic_events(pResponse, pOptions) != XRT_NET_OK ) {
        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "minimax native synthetic event stream cancelled");
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__openai_family_component_name(pProfile),
        "response complete: model=%s streaming=%s attempt=%u status=%s outputs=%u messages=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        bStreaming ? "true" : "false",
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount,
        (unsigned)uMessageCount
    );
    xllm__openai_trace_response(
        pRuntime,
        pProfile,
        *ppResponse,
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        pHttpResponse,
        sRequestId,
        NULL,
        XRT_NET_OK,
        uAttempt,
        bStreaming,
        false,
        false
    );
    iStatus = XRT_NET_OK;

fail:
    if ( iStatus != XRT_NET_OK ) {
        int32 iTraceTransportStatus = pHttpResponse ? XRT_NET_OK : (iNetStatus != XRT_NET_OK ? (int32)iNetStatus : (int32)iStatus);
        int32 iHttpStatus = pHttpResponse ? (int32)pHttpResponse->iStatusCode : (pError ? pError->iHttpStatus : 0);

        bRetryable = xllm__openai_error_is_retryable(pError ? pError->eCode : XLLM_ERROR_NONE);
        if ( bRetryable && uAttempt < uMaxAttempts ) {
            uint32 uDelayMs = xllm__openai_retry_delay_ms(pOptions, uAttempt);
            xllm__openai_logf(
                pRuntime,
                XLLM_LOG_WARN,
                xllm__openai_family_component_name(pProfile),
                "response failed: model=%s streaming=%s attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                bStreaming ? "true" : "false",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
                (unsigned)uDelayMs
            );
            xllm__openai_trace_response(
                pRuntime,
                pProfile,
                NULL,
                sModel,
                pHttpResponse,
                sRequestId,
                pError,
                iTraceTransportStatus,
                uAttempt,
                true,
                bStreaming,
                false
            );
            if ( pHttpResponse ) {
                xrtHttpResponseDestroy(pHttpResponse);
                pHttpResponse = NULL;
            }
            if ( pResponse ) {
                xllm_response_free(pResponse);
                pResponse = NULL;
            }
            xllm_error_free(pError);
            if ( pError ) {
                xllm_error_init(pError);
            }
            xllm__openai_retry_sleep(uDelayMs);
            goto retry_execute;
        }

        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_WARN,
            xllm__openai_family_component_name(pProfile),
            "response failed: model=%s streaming=%s attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            bStreaming ? "true" : "false",
            (unsigned)(uAttempt ? uAttempt : 1u),
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
            bRetryable ? "true" : "false"
        );
        xllm__openai_trace_response(
            pRuntime,
            pProfile,
            NULL,
            sModel,
            pHttpResponse,
            sRequestId,
            pError,
            iTraceTransportStatus,
            (uAttempt ? uAttempt : 1u),
            bRetryable,
            bStreaming,
            false
        );
    }

    if ( pHttpResponse ) {
        xrtHttpResponseDestroy(pHttpResponse);
    }
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    if ( sBody ) {
        xrtFree(sBody);
    }
    if ( sUrl ) {
        xrtFree(sUrl);
    }
    xllm__effective_params_reset(&tEffectiveParams);
    xllm__json_builder_reset(&tBody);
    xrtHttpRequestUnit(&tHttpRequest);
    return iStatus;
}

static int32 xllm__minimax_native_chat_stream_buffered(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_runtime *pRuntime = (xllm_runtime *)pCtx;
    xllm__json_builder tBody;
    xllm__openai_stream_context tStream;
    xllm_effective_params tEffectiveParams;
    xhttprequest tHttpRequest;
    xhttpresponse *pHttpResponse = NULL;
    xllm_response *pResponse = NULL;
    char *sBody = NULL;
    char *sUrl = NULL;
    const char *sModel;
    const char *sRequestId = NULL;
    const char *sContentType = NULL;
    bool bTreatAsSse = false;
    bool bParsedSse = false;
    bool bRetryable = false;
    int iStatus = XRT_NET_ERROR;
    xnet_result iNetStatus = XRT_NET_ERROR;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts = 1u;
    uint32 uMessageCount = 0u;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    *ppResponse = NULL;

    memset(&tBody, 0, sizeof(tBody));
    memset(&tStream, 0, sizeof(tStream));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    xrtHttpRequestInit(&tHttpRequest);
    if ( pOptions && pOptions->uMaxRetries > 0u ) {
        uMaxAttempts += pOptions->uMaxRetries;
    }
    tStream.pProfile = pProfile;
    tStream.pRequest = pRequest;
    tStream.pOptions = pOptions;
    tStream.pError = pError;
    tStream.pRuntime = pRuntime;
    tStream.iMessageOutputIndex = (size_t)-1;
    tStream.iThinkingOutputIndex = (size_t)-1;
    tStream.iRefusalOutputIndex = (size_t)-1;

    sModel = xllm__openai_select_model(pProfile, pRequest, NULL);
    tStream.sSelectedModel = sModel;
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for minimax native request");
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_fill_effective_params(
            &tEffectiveParams,
            pProfile,
            pRequest,
            pOptions ? pOptions->eStreamMode : XLLM_STREAM_PREFER
         ) != XRT_NET_OK ) goto fail;
    if ( xllm__minimax_build_body(&tBody, pRequest, &tEffectiveParams, pOptions, sModel, &uMessageCount, pError) != XRT_NET_OK ) {
        goto fail;
    }
    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) {
        goto fail;
    }
    sUrl = xllm__minimax_build_url(pProfile);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "minimax native profile missing base url");
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) goto fail;
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) goto fail;
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) goto fail;
    if ( xllm__openai_fill_request_headers(&tHttpRequest, pProfile) != XRT_NET_OK ) goto fail;
    if ( !xrtHttpRequestSetHeader(&tHttpRequest, "Accept", "text/event-stream") ) goto fail;
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) goto fail;

retry_execute:
    ++uAttempt;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__openai_family_component_name(pProfile),
        "request start: model=%s streaming=true attempt=%u/%u body_bytes=%u",
        sModel,
        (unsigned)uAttempt,
        (unsigned)uMaxAttempts,
        (unsigned)strlen(sBody)
    );
    xllm__openai_trace_request(
        pRuntime,
        pProfile,
        pRequest,
        sModel,
        true,
        false,
        uAttempt,
        strlen(sBody)
    );

    pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
    if ( !pHttpResponse ) {
        if ( iNetStatus == XRT_NET_TIMEOUT ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "minimax native stream request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "minimax native stream request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "minimax native stream request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    if ( pHttpResponse->iStatusCode >= 400u ) {
        xvalue tErrorRoot = NULL;
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tErrorRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__minimax_fill_error_from_http(pError, pHttpResponse, tErrorRoot);
        xllm__xvalue_release(&tErrorRoot);
        goto fail;
    }
    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "minimax native stream response body is empty");
        goto fail;
    }

    sContentType = xrtHttpResponseHeader(pHttpResponse, "content-type");
    bTreatAsSse = xllm__buffer_starts_with_sse_data(pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !bTreatAsSse && sContentType ) {
        bTreatAsSse = xllm__text_contains_ci(sContentType, "text/event-stream");
    }

    if ( bTreatAsSse ) {
        if ( xllm__openai_stream_process_buffer(&tStream, pHttpResponse->pBody, pHttpResponse->iBodyLen) != XRT_NET_OK ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "minimax native stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }
        if ( tStream.iParsedBytes < pHttpResponse->iBodyLen ) {
            size_t iRemain = pHttpResponse->iBodyLen - tStream.iParsedBytes;
            if ( xllm__openai_stream_process_event_block(&tStream, pHttpResponse->pBody + tStream.iParsedBytes, iRemain) != XRT_NET_OK ) {
                if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "minimax native stream cancelled");
                }
                iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                goto fail;
            }
            tStream.iParsedBytes = pHttpResponse->iBodyLen;
        }

        bParsedSse = tStream.iParsedBytes > 0u || tStream.bDone || tStream.pResponse != NULL;
    }

    if ( bParsedSse ) {
        if ( xllm__openai_stream_finalize_response(&tStream) != XRT_NET_OK ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "minimax native stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }

        pResponse = tStream.pResponse;
        tStream.pResponse = NULL;
        if ( !pResponse ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "minimax native stream did not produce a response");
            goto fail;
        }

        *ppResponse = pResponse;
        pResponse = NULL;
        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_INFO,
            xllm__openai_family_component_name(pProfile),
            "response complete: model=%s streaming=true attempt=%u status=%s outputs=%u messages=%u",
            (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
            (unsigned)uAttempt,
            xllm__openai_response_status_name((*ppResponse)->eStatus),
            (unsigned)(*ppResponse)->iOutputCount,
            (unsigned)uMessageCount
        );
        xllm__openai_trace_response(
            pRuntime,
            pProfile,
            *ppResponse,
            (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
            pHttpResponse,
            sRequestId,
            NULL,
            XRT_NET_OK,
            uAttempt,
            false,
            true,
            false
        );
        iStatus = XRT_NET_OK;
        goto fail;
    }

    if ( pOptions && pOptions->eStreamMode == XLLM_STREAM_REQUIRE ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "minimax native upstream did not return an SSE stream");
        goto fail;
    }

    if ( xllm__minimax_parse_response(
            pProfile,
            pRequest,
            pOptions,
            &tEffectiveParams,
            sModel,
            (const char *)pHttpResponse->pBody,
            pHttpResponse->iBodyLen,
            &pResponse,
            pError
         ) != XRT_NET_OK ) {
        goto fail;
    }
    if ( xllm__openai_emit_synthetic_events(pResponse, pOptions) != XRT_NET_OK ) {
        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "minimax native stream cancelled");
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__openai_family_component_name(pProfile),
        "response fallback complete: model=%s streaming=true attempt=%u status=%s outputs=%u messages=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount,
        (unsigned)uMessageCount
    );
    xllm__openai_trace_response(
        pRuntime,
        pProfile,
        *ppResponse,
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        pHttpResponse,
        sRequestId,
        NULL,
        XRT_NET_OK,
        uAttempt,
        false,
        true,
        false
    );
    iStatus = XRT_NET_OK;

fail:
    if ( iStatus != XRT_NET_OK ) {
        int32 iTraceTransportStatus = pHttpResponse ? XRT_NET_OK : (iNetStatus != XRT_NET_OK ? (int32)iNetStatus : (int32)iStatus);
        int32 iHttpStatus = pHttpResponse ? (int32)pHttpResponse->iStatusCode : (pError ? pError->iHttpStatus : 0);

        bRetryable =
            xllm__openai_error_is_retryable(pError ? pError->eCode : XLLM_ERROR_NONE) &&
            !tStream.bStartEmitted &&
            !tStream.bDone &&
            tStream.uPayloadCount == 0u &&
            tStream.pResponse == NULL;
        if ( bRetryable && uAttempt < uMaxAttempts ) {
            uint32 uDelayMs = xllm__openai_retry_delay_ms(pOptions, uAttempt);
            xllm__openai_logf(
                pRuntime,
                XLLM_LOG_WARN,
                xllm__openai_family_component_name(pProfile),
                "response failed: model=%s streaming=true attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
                (unsigned)uDelayMs
            );
            xllm__openai_trace_response(
                pRuntime,
                pProfile,
                NULL,
                sModel,
                pHttpResponse,
                sRequestId,
                pError,
                iTraceTransportStatus,
                uAttempt,
                true,
                true,
                false
            );
            if ( pHttpResponse ) {
                xrtHttpResponseDestroy(pHttpResponse);
                pHttpResponse = NULL;
            }
            if ( pResponse ) {
                xllm_response_free(pResponse);
                pResponse = NULL;
            }
            if ( tStream.pResponse ) {
                xllm_response_free(tStream.pResponse);
                tStream.pResponse = NULL;
            }
            xllm_error_free(pError);
            if ( pError ) {
                xllm_error_init(pError);
            }
            memset(&tStream, 0, sizeof(tStream));
            tStream.pProfile = pProfile;
            tStream.pRequest = pRequest;
            tStream.pOptions = pOptions;
            tStream.pError = pError;
            tStream.pRuntime = pRuntime;
            tStream.iMessageOutputIndex = (size_t)-1;
            tStream.iThinkingOutputIndex = (size_t)-1;
            tStream.iRefusalOutputIndex = (size_t)-1;
            tStream.sSelectedModel = sModel;
            xllm__openai_retry_sleep(uDelayMs);
            goto retry_execute;
        }

        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_WARN,
            xllm__openai_family_component_name(pProfile),
            "response failed: model=%s streaming=true attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)(uAttempt ? uAttempt : 1u),
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
            bRetryable ? "true" : "false"
        );
        xllm__openai_trace_response(
            pRuntime,
            pProfile,
            NULL,
            sModel,
            pHttpResponse,
            sRequestId,
            pError,
            iTraceTransportStatus,
            (uAttempt ? uAttempt : 1u),
            bRetryable,
            true,
            false
        );
    }

    if ( pHttpResponse ) {
        xrtHttpResponseDestroy(pHttpResponse);
    }
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    if ( tStream.pResponse ) {
        xllm_response_free(tStream.pResponse);
    }
    if ( sBody ) {
        xrtFree(sBody);
    }
    if ( sUrl ) {
        xrtFree(sUrl);
    }
    xllm__effective_params_reset(&tEffectiveParams);
    xllm__json_builder_reset(&tBody);
    xrtHttpRequestUnit(&tHttpRequest);
    return iStatus;
}

static int32 xllm__minimax_native_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    if ( pOptions && pOptions->eStreamMode != XLLM_STREAM_OFF ) {
        return xllm__minimax_native_chat_stream_buffered(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
    }
    return xllm__minimax_native_chat_direct(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
}

XLLM_API int xllm_register_minimax_native_adapter(xllm_runtime *pRuntime)
{
    xllm_adapter tAdapter;

    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    memset(&tAdapter, 0, sizeof(tAdapter));
    tAdapter.sName = XLLM_ADAPTER_MINIMAX_NATIVE;
    tAdapter.pCtx = pRuntime;
    tAdapter.pfnChat = xllm__minimax_native_chat;
    return xllm_register_adapter(pRuntime, &tAdapter);
}
