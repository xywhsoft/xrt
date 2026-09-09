static const char *xllm__anthropic_component_name(void)
{
    return "xllm.anthropic_native";
}

static void xllm__anthropic_trace_request(
    xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const char *sModel,
    bool bStreaming,
    bool bLive,
    uint32 uAttempt,
    size_t iBodyBytes
)
{
    xvalue tPayload;

    if ( !pRuntime || !pRuntime->tOptions.pfnTrace ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__openai_trace_table_set_text(tPayload, "phase", "request");
    xllm__openai_trace_table_set_text(tPayload, "adapter", XLLM_ADAPTER_ANTHROPIC_NATIVE);
    if ( pProfile && pProfile->sId ) {
        xllm__openai_trace_table_set_text(tPayload, "profile_id", pProfile->sId);
    }
    if ( sModel ) {
        xllm__openai_trace_table_set_text(tPayload, "model", sModel);
    }
    xllm__openai_trace_table_set_bool(tPayload, "streaming", bStreaming);
    xllm__openai_trace_table_set_bool(tPayload, "live", bLive);
    if ( pRequest ) {
        xllm__openai_trace_table_set_u32(tPayload, "message_count", (uint32)pRequest->iMessageCount);
        xllm__openai_trace_table_set_u32(tPayload, "context_block_count", (uint32)pRequest->iContextBlockCount);
        xllm__openai_trace_table_set_u32(tPayload, "tool_count", (uint32)pRequest->iToolCount);
    }
    xllm__openai_trace_table_set_u32(tPayload, "body_bytes", (uint32)iBodyBytes);
    xllm__openai_trace_table_set_u32(tPayload, "attempt", uAttempt);
    xllm__openai_trace_emit(pRuntime, XLLM_TRACE_REQUEST, tPayload);
}

static void xllm__anthropic_trace_response(
    xllm_runtime *pRuntime,
    const xllm_response *pResponse,
    const char *sModel,
    const xhttpresponse *pHttpResponse,
    const char *sRequestId,
    const xllm_error *pError,
    int32 iTransportStatus,
    uint32 uAttempt,
    bool bRetryable,
    bool bStreaming,
    bool bLive
)
{
    xvalue tPayload;

    if ( !pRuntime || !pRuntime->tOptions.pfnTrace ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__openai_trace_table_set_text(tPayload, "phase", "response");
    xllm__openai_trace_table_set_text(tPayload, "adapter", XLLM_ADAPTER_ANTHROPIC_NATIVE);
    xllm__openai_trace_table_set_bool(tPayload, "streaming", bStreaming);
    xllm__openai_trace_table_set_bool(tPayload, "live", bLive);
    xllm__openai_trace_table_set_i32(tPayload, "transport_status", iTransportStatus);
    xllm__openai_trace_table_set_u32(tPayload, "attempt", uAttempt);
    xllm__openai_trace_table_set_bool(tPayload, "retryable", bRetryable);
    if ( pHttpResponse ) {
        xllm__openai_trace_table_set_u32(tPayload, "http_status", pHttpResponse->iStatusCode);
    } else if ( pError && pError->iHttpStatus > 0 ) {
        xllm__openai_trace_table_set_i32(tPayload, "http_status", pError->iHttpStatus);
    }
    if ( sRequestId ) {
        xllm__openai_trace_table_set_text(tPayload, "request_id", sRequestId);
    } else if ( pError && pError->sRequestId ) {
        xllm__openai_trace_table_set_text(tPayload, "request_id", pError->sRequestId);
    }
    if ( pResponse ) {
        xllm__openai_trace_table_set_bool(tPayload, "success", true);
        xllm__openai_trace_table_set_text(tPayload, "response_status", xllm__openai_response_status_name(pResponse->eStatus));
        xllm__openai_trace_table_set_u32(tPayload, "output_count", (uint32)pResponse->iOutputCount);
        xllm__openai_trace_table_set_u32(tPayload, "input_tokens", pResponse->tUsage.uInputTokens);
        xllm__openai_trace_table_set_u32(tPayload, "output_tokens", pResponse->tUsage.uOutputTokens);
        if ( pResponse->sModel ) {
            xllm__openai_trace_table_set_text(tPayload, "model", pResponse->sModel);
        } else if ( sModel ) {
            xllm__openai_trace_table_set_text(tPayload, "model", sModel);
        }
        if ( pResponse->sFinishReason ) {
            xllm__openai_trace_table_set_text(tPayload, "finish_reason", pResponse->sFinishReason);
        }
    } else {
        xllm__openai_trace_table_set_bool(tPayload, "success", false);
        xllm__openai_trace_table_set_text(tPayload, "response_status", "errored");
        if ( sModel ) {
            xllm__openai_trace_table_set_text(tPayload, "model", sModel);
        }
    }
    if ( pError && pError->eCode != XLLM_ERROR_NONE ) {
        xllm__openai_trace_table_set_text(tPayload, "error_code", xllm__openai_error_code_name(pError->eCode));
        if ( pError->sMessage ) {
            xllm__openai_trace_table_set_text(tPayload, "error_message", pError->sMessage);
        }
        if ( pError->sProviderCode ) {
            xllm__openai_trace_table_set_text(tPayload, "provider_code", pError->sProviderCode);
        }
        if ( pError->sProviderMessage ) {
            xllm__openai_trace_table_set_text(tPayload, "provider_message", pError->sProviderMessage);
        }
    }
    xllm__openai_trace_emit(pRuntime, XLLM_TRACE_RESPONSE, tPayload);
}

static bool xllm__anthropic_reasoning_requested(const xllm_reasoning_options *pReasoning)
{
    if ( !pReasoning ) {
        return false;
    }
    if ( pReasoning->tEnabled.bSet ) {
        return pReasoning->tEnabled.bValue;
    }
    if ( pReasoning->eLevel != XLLM_REASONING_DEFAULT ) {
        return true;
    }
    if ( pReasoning->tBudgetTokens.bSet ) {
        return true;
    }
    if ( pReasoning->tExposeThinking.bSet && pReasoning->tExposeThinking.bValue ) {
        return true;
    }
    return (pReasoning->tVendorExtra && xvoType(pReasoning->tVendorExtra) != XVO_DT_NULL);
}

static const char *xllm__anthropic_reasoning_vendor_text(const xllm_reasoning_options *pReasoning, const char *sKey)
{
    if ( !pReasoning || !sKey || !pReasoning->tVendorExtra || xvoType(pReasoning->tVendorExtra) != XVO_DT_TABLE ) {
        return NULL;
    }

    return xllm__json_table_get_text(pReasoning->tVendorExtra, sKey);
}

static const char *xllm__anthropic_reasoning_display_name(const xllm_reasoning_options *pReasoning)
{
    const char *sDisplay;

    sDisplay = xllm__anthropic_reasoning_vendor_text(pReasoning, "display");
    if ( sDisplay && sDisplay[0] ) {
        return sDisplay;
    }
    if ( !pReasoning || !pReasoning->tExposeThinking.bSet ) {
        return NULL;
    }

    return pReasoning->tExposeThinking.bValue ? "summarized" : "omitted";
}

static const char *xllm__anthropic_reasoning_effort_name(const xllm_reasoning_options *pReasoning)
{
    const char *sEffort;

    sEffort = xllm__anthropic_reasoning_vendor_text(pReasoning, "effort");
    if ( sEffort && sEffort[0] ) {
        return sEffort;
    }

    sEffort = xllm__anthropic_reasoning_vendor_text(pReasoning, "reasoning_effort");
    if ( sEffort && sEffort[0] ) {
        return sEffort;
    }

    if ( !pReasoning ) {
        return NULL;
    }

    switch ( pReasoning->eLevel ) {
        case XLLM_REASONING_LOW:
            return "low";
        case XLLM_REASONING_MEDIUM:
            return "medium";
        case XLLM_REASONING_HIGH:
            return "high";
        default:
            break;
    }

    return "medium";
}

static uint32 xllm__anthropic_reasoning_budget(const xllm_reasoning_options *pReasoning, uint32 uMaxTokens)
{
    uint32 uBudget = 2048u;

    if ( pReasoning ) {
        if ( pReasoning->tBudgetTokens.bSet && pReasoning->tBudgetTokens.iValue > 0u ) {
            uBudget = pReasoning->tBudgetTokens.iValue;
        } else {
            switch ( pReasoning->eLevel ) {
                case XLLM_REASONING_LOW:
                    uBudget = 1024u;
                    break;
                case XLLM_REASONING_MEDIUM:
                    uBudget = 4096u;
                    break;
                case XLLM_REASONING_HIGH:
                    uBudget = 8192u;
                    break;
                default:
                    break;
            }
        }
    }

    if ( uMaxTokens > 1u && uBudget >= uMaxTokens ) {
        uBudget = uMaxTokens - 1u;
    }
    if ( uBudget == 0u ) {
        uBudget = 1u;
    }

    return uBudget;
}

static int xllm__anthropic_append_reasoning(
    xllm__json_builder *pBody,
    const xllm_reasoning_options *pReasoning,
    uint32 uMaxTokens
)
{
    const char *sType;
    const char *sDisplay;
    const char *sEffort;

    if ( !pBody || !pReasoning || !xllm__anthropic_reasoning_requested(pReasoning) ) {
        return XRT_NET_OK;
    }

    sType = xllm__anthropic_reasoning_vendor_text(pReasoning, "type");
    if ( !sType || !sType[0] ) {
        sType = "enabled";
    }
    sDisplay = xllm__anthropic_reasoning_display_name(pReasoning);

    if ( !xllm__json_builder_append_cstr(pBody, ",\"thinking\":{\"type\":") ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_escaped(pBody, sType) ) return XRT_NET_ERROR;

    if ( strcmp(sType, "adaptive") == 0 ) {
        sEffort = xllm__anthropic_reasoning_effort_name(pReasoning);
        if ( sEffort && sEffort[0] ) {
            if ( !xllm__json_builder_append_cstr(pBody, ",\"effort\":") ) return XRT_NET_ERROR;
            if ( !xllm__json_builder_append_escaped(pBody, sEffort) ) return XRT_NET_ERROR;
        }
    } else if ( strcmp(sType, "disabled") != 0 ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"budget_tokens\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_u32(pBody, xllm__anthropic_reasoning_budget(pReasoning, uMaxTokens)) ) {
            return XRT_NET_ERROR;
        }
    }

    if ( sDisplay && sDisplay[0] ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"display\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_escaped(pBody, sDisplay) ) return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBody, '}') ) return XRT_NET_ERROR;
    return XRT_NET_OK;
}

static bool xllm__anthropic_vendor_is_thinking_block(xvalue tVendorExtra)
{
    const char *sBlockType;

    if ( !tVendorExtra || xvoType(tVendorExtra) != XVO_DT_TABLE ) {
        return false;
    }

    sBlockType = xllm__json_table_get_text(tVendorExtra, "anthropic_block_type");
    return (sBlockType && strcmp(sBlockType, "thinking") == 0);
}

static const char *xllm__anthropic_vendor_signature(xvalue tVendorExtra)
{
    if ( !xllm__anthropic_vendor_is_thinking_block(tVendorExtra) ) {
        return NULL;
    }

    return xllm__json_table_get_text(tVendorExtra, "signature");
}

static xvalue xllm__anthropic_create_thinking_vendor_extra(const char *sSignature)
{
    xvalue tTable = xvoCreateTable();

    if ( !tTable ) {
        return NULL;
    }

    xvoTableSetText(tTable, (str)"anthropic_block_type", 0u, (str)"thinking", 0u, FALSE);
    if ( sSignature && sSignature[0] ) {
        xvoTableSetText(tTable, (str)"signature", 0u, (str)sSignature, 0u, FALSE);
    }

    return tTable;
}

static int xllm__anthropic_set_thinking_vendor_extra(xllm_output_thinking *pThinking, const char *sSignature)
{
    xvalue tVendorExtra;

    if ( !pThinking ) {
        return XRT_NET_ERROR;
    }

    tVendorExtra = xllm__anthropic_create_thinking_vendor_extra(sSignature);
    if ( !tVendorExtra ) {
        return XRT_NET_ERROR;
    }

    xllm__xvalue_release(&pThinking->tVendorExtra);
    pThinking->tVendorExtra = tVendorExtra;
    return XRT_NET_OK;
}

static char *xllm__anthropic_find_thinking_signature(xvalue tContent)
{
    size_t i;

    if ( !tContent || xvoType(tContent) != XVO_DT_ARRAY ) {
        return NULL;
    }

    for ( i = 0; i < (size_t)xvoArrayItemCount(tContent); ++i ) {
        xvalue tItem = xvoArrayGetValue(tContent, (uint32)i);
        const char *sType;
        const char *sSignature;

        if ( !tItem || xvoType(tItem) != XVO_DT_TABLE ) {
            continue;
        }

        sType = xllm__json_table_get_text(tItem, "type");
        if ( !sType || strcmp(sType, "thinking") != 0 ) {
            continue;
        }

        sSignature = xllm__json_table_get_text(tItem, "signature");
        if ( sSignature && sSignature[0] ) {
            return xllm__dup_cstr(sSignature);
        }
    }

    return NULL;
}

static bool xllm__anthropic_message_has_thinking_parts(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage || pMessage->eRole != XLLM_ROLE_ASSISTANT || !pMessage->pParts ) {
        return false;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        if ( xllm__anthropic_vendor_is_thinking_block(pMessage->pParts[i].tVendorExtra) ) {
            return true;
        }
    }

    return false;
}

static bool xllm__anthropic_message_has_image_parts(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage || !pMessage->pParts ) {
        return false;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        if ( pMessage->pParts[i].eKind == XLLM_PART_IMAGE ) {
            return true;
        }
    }

    return false;
}

static bool xllm__anthropic_message_has_file_parts(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage || !pMessage->pParts ) {
        return false;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        if ( pMessage->pParts[i].eKind == XLLM_PART_FILE ) {
            return true;
        }
    }

    return false;
}

static int xllm__anthropic_message_to_text(const xllm_message *pMessage, char **psText, xllm_error *pError)
{
    xllm__json_builder tBuilder;
    size_t i;
    bool bHasContent = false;

    if ( !psText ) {
        return XRT_NET_ERROR;
    }

    *psText = NULL;
    memset(&tBuilder, 0, sizeof(tBuilder));

    if ( !pMessage || pMessage->iPartCount == 0u ) {
        *psText = xllm__dup_cstr("");
        return *psText ? XRT_NET_OK : XRT_NET_ERROR;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        const xllm_content_part *pPart = &pMessage->pParts[i];

        if ( bHasContent && !xllm__json_builder_append_char(&tBuilder, '\n') ) {
            xllm__json_builder_reset(&tBuilder);
            return XRT_NET_ERROR;
        }

        switch ( pPart->eKind ) {
            case XLLM_PART_TEXT:
                if ( pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                    xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "anthropic-native adapter currently only supports inline text content");
                    xllm__json_builder_reset(&tBuilder);
                    return XRT_NET_ERROR;
                }
                if ( !xllm__json_builder_append_cstr(&tBuilder, pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "") ) {
                    xllm__json_builder_reset(&tBuilder);
                    return XRT_NET_ERROR;
                }
                bHasContent = true;
                break;
            case XLLM_PART_JSON: {
                char *sJson = (char *)xrtStringifyJSON(pPart->as.tJsonValue, 0, NULL);
                if ( !sJson ) {
                    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify anthropic-native json part");
                    xllm__json_builder_reset(&tBuilder);
                    return XRT_NET_ERROR;
                }
                if ( !xllm__json_builder_append_cstr(&tBuilder, sJson) ) {
                    xrtFree(sJson);
                    xllm__json_builder_reset(&tBuilder);
                    return XRT_NET_ERROR;
                }
                xrtFree(sJson);
                bHasContent = true;
                break;
            }
            default:
                xllm__error_set(
                    pError,
                    XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                    "anthropic-native text-only content currently supports only text and json parts"
                );
                xllm__json_builder_reset(&tBuilder);
                return XRT_NET_ERROR;
        }
    }

    *psText = xllm__json_builder_detach(&tBuilder);
    if ( !*psText ) {
        xllm__json_builder_reset(&tBuilder);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static bool xllm__anthropic_message_requires_array_content(const xllm_message *pMessage)
{
    if ( !pMessage ) {
        return false;
    }

    return
        ((pMessage->eRole == XLLM_ROLE_USER || pMessage->eRole == XLLM_ROLE_ASSISTANT) &&
         (xllm__anthropic_message_has_image_parts(pMessage) ||
          xllm__anthropic_message_has_file_parts(pMessage))) ||
        pMessage->eRole == XLLM_ROLE_TOOL ||
        (pMessage->eRole == XLLM_ROLE_ASSISTANT &&
         (pMessage->iToolCallCount > 0u || xllm__anthropic_message_has_thinking_parts(pMessage)));
}

static bool xllm__anthropic_tool_result_requires_block_array(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage || !pMessage->pParts || pMessage->iPartCount == 0u ) {
        return false;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        switch ( pMessage->pParts[i].eKind ) {
            case XLLM_PART_TEXT:
            case XLLM_PART_JSON:
                break;
            case XLLM_PART_IMAGE:
            case XLLM_PART_FILE:
                return true;
            default:
                return true;
        }
    }

    return false;
}

static int xllm__anthropic_append_image_block(
    xllm__json_builder *pBuilder,
    const xllm_content_part *pPart,
    xllm_error *pError
)
{
    const char *sMimeType;
    char *sBase64 = NULL;

    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_INLINE_BYTES:
            break;
        case XLLM_SOURCE_URL:
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":") ||
                 !xllm__json_builder_append_escaped(
                    pBuilder,
                    pPart->as.tSource.as.sUrl ? pPart->as.tSource.as.sUrl : ""
                 ) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            if ( !pPart->as.tSource.as.sFileId || pPart->as.tSource.as.sFileId[0] == '\0' ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    "anthropic-native image file_id input is empty"
                );
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"image\",\"source\":{\"type\":\"file\",\"file_id\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pPart->as.tSource.as.sFileId) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        case XLLM_SOURCE_INLINE_TEXT:
        default:
            xllm__error_set(
                pError,
                XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                "anthropic-native image input only supports url, provider file_id, or inline bytes"
            );
            return XRT_NET_ERROR;
    }

    if ( !pPart->as.tSource.as.tBytes.pData || pPart->as.tSource.as.tBytes.iSize == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native image bytes input is empty");
        return XRT_NET_ERROR;
    }

    sBase64 = (char *)xrtBase64Encode(
        (ptr)pPart->as.tSource.as.tBytes.pData,
        pPart->as.tSource.as.tBytes.iSize,
        NULL
    );
    if ( !sBase64 ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode anthropic-native image bytes");
        return XRT_NET_ERROR;
    }

    sMimeType = pPart->as.tSource.sMimeType ? pPart->as.tSource.sMimeType : "application/octet-stream";
    if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":") ||
         !xllm__json_builder_append_escaped(pBuilder, sMimeType) ||
         !xllm__json_builder_append_cstr(pBuilder, ",\"data\":") ||
         !xllm__json_builder_append_escaped(pBuilder, sBase64) ||
         !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
        xrtFree(sBase64);
        return XRT_NET_ERROR;
    }

    xrtFree(sBase64);
    return XRT_NET_OK;
}

static int xllm__anthropic_append_file_block(
    xllm__json_builder *pBuilder,
    const xllm_content_part *pPart,
    xllm_error *pError
)
{
    const char *sMimeType;
    char *sBase64 = NULL;

    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            if ( !pPart->as.tSource.as.sFileId || pPart->as.tSource.as.sFileId[0] == '\0' ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    "anthropic-native document file_id input is empty"
                );
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"document\",\"source\":{\"type\":\"file\",\"file_id\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pPart->as.tSource.as.sFileId) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        case XLLM_SOURCE_INLINE_BYTES:
            break;
        case XLLM_SOURCE_URL:
            sMimeType = pPart->as.tSource.sMimeType ? pPart->as.tSource.sMimeType : NULL;
            if ( sMimeType && sMimeType[0] && strcmp(sMimeType, "application/pdf") != 0 ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                    "anthropic-native document url input currently only supports application/pdf"
                );
                return XRT_NET_ERROR;
            }
            if ( !pPart->as.tSource.as.sUrl || pPart->as.tSource.as.sUrl[0] == '\0' ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    "anthropic-native document url input is empty"
                );
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"document\",\"source\":{\"type\":\"url\",\"url\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pPart->as.tSource.as.sUrl) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        case XLLM_SOURCE_INLINE_TEXT:
        default:
            xllm__error_set(
                pError,
                XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                "anthropic-native document input only supports url, provider file_id, or inline bytes"
            );
            return XRT_NET_ERROR;
    }

    if ( !pPart->as.tSource.as.tBytes.pData || pPart->as.tSource.as.tBytes.iSize == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native document bytes input is empty");
        return XRT_NET_ERROR;
    }

    sMimeType = pPart->as.tSource.sMimeType ? pPart->as.tSource.sMimeType : "application/octet-stream";
    if ( strcmp(sMimeType, "application/pdf") != 0 ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
            "anthropic-native inline document input currently only supports application/pdf"
        );
        return XRT_NET_ERROR;
    }

    sBase64 = (char *)xrtBase64Encode(
        (ptr)pPart->as.tSource.as.tBytes.pData,
        pPart->as.tSource.as.tBytes.iSize,
        NULL
    );
    if ( !sBase64 ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode anthropic-native document bytes");
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"document\",\"source\":{\"type\":\"base64\",\"media_type\":") ||
         !xllm__json_builder_append_escaped(pBuilder, sMimeType) ||
         !xllm__json_builder_append_cstr(pBuilder, ",\"data\":") ||
         !xllm__json_builder_append_escaped(pBuilder, sBase64) ||
         !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
        xrtFree(sBase64);
        return XRT_NET_ERROR;
    }

    xrtFree(sBase64);
    return XRT_NET_OK;
}

static int xllm__anthropic_append_tool_result_content_array(
    xllm__json_builder *pBuilder,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    size_t i;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBuilder, '[') ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        const xllm_content_part *pPart = &pMessage->pParts[i];
        char *sJson = NULL;

        if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }

        switch ( pPart->eKind ) {
            case XLLM_PART_TEXT:
                if ( pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                    xllm__error_set(
                        pError,
                        XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                        "anthropic-native tool result text content must be inline text"
                    );
                    return XRT_NET_ERROR;
                }
                if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"text\",\"text\":") ||
                     !xllm__json_builder_append_escaped(
                        pBuilder,
                        pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : ""
                     ) ||
                     !xllm__json_builder_append_char(pBuilder, '}') ) {
                    return XRT_NET_ERROR;
                }
                break;
            case XLLM_PART_JSON:
                sJson = (char *)xrtStringifyJSON(pPart->as.tJsonValue, 0, NULL);
                if ( !sJson ) {
                    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify anthropic-native tool result json part");
                    return XRT_NET_ERROR;
                }
                if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"text\",\"text\":") ||
                     !xllm__json_builder_append_escaped(pBuilder, sJson) ||
                     !xllm__json_builder_append_char(pBuilder, '}') ) {
                    xrtFree(sJson);
                    return XRT_NET_ERROR;
                }
                xrtFree(sJson);
                break;
            case XLLM_PART_IMAGE:
                if ( xllm__anthropic_append_image_block(pBuilder, pPart, pError) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                break;
            case XLLM_PART_FILE:
                if ( xllm__anthropic_append_file_block(pBuilder, pPart, pError) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                break;
            default:
                xllm__error_set(
                    pError,
                    XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                    "anthropic-native tool result currently only supports text, json, image, and file parts"
                );
                return XRT_NET_ERROR;
        }
    }

    return xllm__json_builder_append_char(pBuilder, ']') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__anthropic_append_message_blocks(
    xllm__json_builder *pBuilder,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    size_t i;
    bool bHasBlock = false;
    char *sToolText = NULL;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBuilder, '[') ) {
        return XRT_NET_ERROR;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        if ( !pMessage->sToolCallId || !pMessage->sToolCallId[0] ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native tool result message missing tool_call_id");
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"tool_result\",\"tool_use_id\":") ) {
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_escaped(pBuilder, pMessage->sToolCallId) ) {
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"content\":") ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__anthropic_tool_result_requires_block_array(pMessage) ) {
            if ( xllm__anthropic_append_tool_result_content_array(pBuilder, pMessage, pError) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
        } else {
            if ( xllm__anthropic_message_to_text(pMessage, &sToolText, pError) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_escaped(pBuilder, sToolText ? sToolText : "") ) {
                xllm__free_cstr(&sToolText);
                return XRT_NET_ERROR;
            }
        }
        if ( !xllm__json_builder_append_char(pBuilder, '}') ) {
            xllm__free_cstr(&sToolText);
            return XRT_NET_ERROR;
        }
        xllm__free_cstr(&sToolText);
        bHasBlock = true;
    } else {
        for ( i = 0; i < pMessage->iPartCount; ++i ) {
            const xllm_content_part *pPart = &pMessage->pParts[i];
            const char *sSignature;
            char *sJson = NULL;

            if ( bHasBlock && !xllm__json_builder_append_char(pBuilder, ',') ) {
                return XRT_NET_ERROR;
            }

            switch ( pPart->eKind ) {
                case XLLM_PART_TEXT:
                    if ( pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                        if ( xllm__anthropic_vendor_is_thinking_block(pPart->tVendorExtra) ) {
                            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native thinking continuation block must be inline text");
                        } else {
                            xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "anthropic-native adapter currently only supports inline text content");
                        }
                        return XRT_NET_ERROR;
                    }
                    if ( xllm__anthropic_vendor_is_thinking_block(pPart->tVendorExtra) ) {
                        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"thinking\",\"thinking\":") ) {
                            return XRT_NET_ERROR;
                        }
                        if ( !xllm__json_builder_append_escaped(pBuilder, pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "") ) {
                            return XRT_NET_ERROR;
                        }
                        sSignature = xllm__anthropic_vendor_signature(pPart->tVendorExtra);
                        if ( sSignature && sSignature[0] ) {
                            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"signature\":") ) {
                                return XRT_NET_ERROR;
                            }
                            if ( !xllm__json_builder_append_escaped(pBuilder, sSignature) ) {
                                return XRT_NET_ERROR;
                            }
                        }
                        if ( !xllm__json_builder_append_char(pBuilder, '}') ) {
                            return XRT_NET_ERROR;
                        }
                    } else {
                        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"text\",\"text\":") ||
                             !xllm__json_builder_append_escaped(
                                pBuilder,
                                pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : ""
                             ) ||
                             !xllm__json_builder_append_char(pBuilder, '}') ) {
                            return XRT_NET_ERROR;
                        }
                    }
                    break;
                case XLLM_PART_JSON:
                    sJson = (char *)xrtStringifyJSON(pPart->as.tJsonValue, 0, NULL);
                    if ( !sJson ) {
                        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify anthropic-native json part");
                        return XRT_NET_ERROR;
                    }
                    if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"text\",\"text\":") ||
                         !xllm__json_builder_append_escaped(pBuilder, sJson) ||
                         !xllm__json_builder_append_char(pBuilder, '}') ) {
                        xrtFree(sJson);
                        return XRT_NET_ERROR;
                    }
                    xrtFree(sJson);
                    break;
                case XLLM_PART_IMAGE:
                    if ( pMessage->eRole != XLLM_ROLE_USER &&
                         pMessage->eRole != XLLM_ROLE_ASSISTANT ) {
                        xllm__error_set(
                            pError,
                            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                            "anthropic-native multimodal input currently only supports user and assistant image messages"
                        );
                        return XRT_NET_ERROR;
                    }
                    if ( xllm__anthropic_append_image_block(pBuilder, pPart, pError) != XRT_NET_OK ) {
                        return XRT_NET_ERROR;
                    }
                    break;
                case XLLM_PART_FILE:
                    if ( pMessage->eRole != XLLM_ROLE_USER &&
                         pMessage->eRole != XLLM_ROLE_ASSISTANT ) {
                        xllm__error_set(
                            pError,
                            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                            "anthropic-native multimodal input currently only supports user and assistant document messages"
                        );
                        return XRT_NET_ERROR;
                    }
                    if ( xllm__anthropic_append_file_block(pBuilder, pPart, pError) != XRT_NET_OK ) {
                        return XRT_NET_ERROR;
                    }
                    break;
                default:
                    xllm__error_set(
                        pError,
                        XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                        "anthropic-native multimodal input currently only supports text, json, image, and file parts"
                    );
                    return XRT_NET_ERROR;
            }

            bHasBlock = true;
        }
    }

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u ) {
        for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
            const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
            const char *sToolName = pCall->sToolName ? pCall->sToolName : pCall->sToolId;
            xvalue tInput = NULL;
            char *sNormalized = NULL;

            if ( !sToolName || !sToolName[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native assistant tool call missing tool name");
                return XRT_NET_ERROR;
            }
            if ( pCall->sArgumentsJson && pCall->sArgumentsJson[0] ) {
                tInput = xllm__parse_json_range(pCall->sArgumentsJson, strlen(pCall->sArgumentsJson), &sNormalized);
                if ( !tInput ) {
                    xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native assistant tool call arguments_json is not valid json");
                    return XRT_NET_ERROR;
                }
            }

            if ( bHasBlock && !xllm__json_builder_append_char(pBuilder, ',') ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"tool_use\",\"id\":") ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_escaped(pBuilder, pCall->sCallId ? pCall->sCallId : "") ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"name\":") ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_escaped(pBuilder, sToolName) ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"input\":") ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
            if ( sNormalized ) {
                if ( !xllm__json_builder_append_cstr(pBuilder, sNormalized) ) {
                    if ( tInput ) {
                        xvoUnref(tInput);
                    }
                    xrtFree(sNormalized);
                    return XRT_NET_ERROR;
                }
            } else if ( !xllm__json_builder_append_cstr(pBuilder, "{}") ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_char(pBuilder, '}') ) {
                if ( tInput ) {
                    xvoUnref(tInput);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }

            if ( tInput ) {
                xvoUnref(tInput);
            }
            xrtFree(sNormalized);
            bHasBlock = true;
        }
    }

    if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__anthropic_append_tools(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    xllm_error *pError
)
{
    size_t i;

    if ( !pBuilder || !pRequest || pRequest->iToolCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tools\":[") ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pRequest->iToolCount; ++i ) {
        const xllm_tool_def *pTool = &pRequest->pTools[i];
        const char *sWireName = pTool->sWireName ? pTool->sWireName : pTool->sToolId;
        char *sSchema = NULL;
        char *sProviderToolJson = NULL;

        if ( pTool->eKind == XLLM_TOOL_PROVIDER ) {
            if ( !pTool->tVendorExtra || xvoType(pTool->tVendorExtra) != XVO_DT_TABLE ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    "anthropic-native provider tool requires vendor_extra object"
                );
                return XRT_NET_ERROR;
            }
            sProviderToolJson = (char *)xrtStringifyJSON(pTool->tVendorExtra, 0, NULL);
            if ( !sProviderToolJson ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INTERNAL,
                    "failed to stringify anthropic-native provider tool"
                );
                return XRT_NET_ERROR;
            }
            if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
                xrtFree(sProviderToolJson);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, sProviderToolJson) ) {
                xrtFree(sProviderToolJson);
                return XRT_NET_ERROR;
            }
            xrtFree(sProviderToolJson);
            continue;
        }

        if ( !sWireName || !sWireName[0] ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native tool definition missing wire_name");
            return XRT_NET_ERROR;
        }
        if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }

        if ( pTool->tInputSchema && xvoType(pTool->tInputSchema) != XVO_DT_NULL ) {
            sSchema = (char *)xrtStringifyJSON(pTool->tInputSchema, 0, NULL);
        }
        if ( !sSchema ) {
            sSchema = xllm__dup_cstr("{}");
        }
        if ( !sSchema ) {
            return XRT_NET_ERROR;
        }

        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"name\":") ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_escaped(pBuilder, sWireName) ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( pTool->sDescription && pTool->sDescription[0] ) {
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"description\":") ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_escaped(pBuilder, pTool->sDescription) ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"input_schema\":") ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, sSchema) ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_char(pBuilder, '}') ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }

        xrtFree(sSchema);
    }

    return xllm__json_builder_append_char(pBuilder, ']') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__anthropic_append_tool_policy(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    const xllm_reasoning_options *pReasoning,
    xllm_error *pError
)
{
    if ( !pBuilder || !pRequest || pRequest->iToolCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( pReasoning && xllm__anthropic_reasoning_requested(pReasoning) ) {
        if ( pRequest->tToolPolicy.eMode == XLLM_TOOL_CHOICE_REQUIRED ||
             pRequest->tToolPolicy.eMode == XLLM_TOOL_CHOICE_NAMED ) {
            xllm__error_set(
                pError,
                XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                "anthropic-native reasoning cannot be used with forced tool_choice"
            );
            return XRT_NET_ERROR;
        }
    }

    switch ( pRequest->tToolPolicy.eMode ) {
        case XLLM_TOOL_CHOICE_NONE:
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":{\"type\":\"none\"") ) return XRT_NET_ERROR;
            break;
        case XLLM_TOOL_CHOICE_REQUIRED:
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":{\"type\":\"any\"") ) return XRT_NET_ERROR;
            break;
        case XLLM_TOOL_CHOICE_NAMED:
            if ( !pRequest->tToolPolicy.sToolName || !pRequest->tToolPolicy.sToolName[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native named tool_choice missing tool name");
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":{\"type\":\"tool\",\"name\":") ) return XRT_NET_ERROR;
            if ( !xllm__json_builder_append_escaped(pBuilder, pRequest->tToolPolicy.sToolName) ) return XRT_NET_ERROR;
            break;
        case XLLM_TOOL_CHOICE_AUTO:
        default:
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":{\"type\":\"auto\"") ) return XRT_NET_ERROR;
            break;
    }

    if ( !pRequest->tToolPolicy.bAllowParallel ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"disable_parallel_tool_use\":true") ) {
            return XRT_NET_ERROR;
        }
    }

    return xllm__json_builder_append_char(pBuilder, '}') ? XRT_NET_OK : XRT_NET_ERROR;
}

static char *xllm__anthropic_build_url(const char *sBaseUrl)
{
    static const char sPath[] = "messages";
    size_t iLen;
    bool bNeedsSlash;
    char *sUrl;

    if ( !sBaseUrl || !sBaseUrl[0] ) {
        return NULL;
    }

    if ( strstr(sBaseUrl, "/messages") != NULL ) {
        return xllm__dup_cstr(sBaseUrl);
    }

    iLen = strlen(sBaseUrl);
    bNeedsSlash = (iLen > 0u && sBaseUrl[iLen - 1u] != '/');
    sUrl = (char *)xrtCalloc(iLen + (bNeedsSlash ? 1u : 0u) + sizeof(sPath), sizeof(char));
    if ( !sUrl ) {
        return NULL;
    }

    memcpy(sUrl, sBaseUrl, iLen);
    if ( bNeedsSlash ) {
        sUrl[iLen++] = '/';
    }
    memcpy(sUrl + iLen, sPath, sizeof(sPath));
    return sUrl;
}

static int xllm__anthropic_fill_request_headers(xhttprequest *pHttpRequest, const xllm_profile *pProfile)
{
    size_t i;
    const char *sVersion;

    if ( !pHttpRequest || !pProfile ) {
        return XRT_NET_ERROR;
    }

    if ( !xrtHttpRequestSetHeader(pHttpRequest, "Accept", "application/json") ) {
        return XRT_NET_ERROR;
    }
    if ( !xrtHttpRequestSetHeader(pHttpRequest, "User-Agent", "xllm/0.1.0") ) {
        return XRT_NET_ERROR;
    }

    switch ( pProfile->tAuth.eKind ) {
        case XLLM_AUTH_API_KEY_HEADER: {
            const char *sHeaderName = pProfile->tAuth.sHeaderName ? pProfile->tAuth.sHeaderName : "x-api-key";
            if ( pProfile->tAuth.sSecret ) {
                if ( !xrtHttpRequestSetHeader(pHttpRequest, sHeaderName, pProfile->tAuth.sSecret) ) {
                    return XRT_NET_ERROR;
                }
            }
            break;
        }
        case XLLM_AUTH_BEARER: {
            const char *sScheme = pProfile->tAuth.sScheme ? pProfile->tAuth.sScheme : "Bearer";
            size_t iSchemeLen = strlen(sScheme);
            size_t iSecretLen = pProfile->tAuth.sSecret ? strlen(pProfile->tAuth.sSecret) : 0u;
            char *sValue = (char *)xrtCalloc(iSchemeLen + iSecretLen + 2u, sizeof(char));
            if ( !sValue ) {
                return XRT_NET_ERROR;
            }
            memcpy(sValue, sScheme, iSchemeLen);
            sValue[iSchemeLen] = ' ';
            if ( pProfile->tAuth.sSecret ) {
                memcpy(sValue + iSchemeLen + 1u, pProfile->tAuth.sSecret, iSecretLen);
            }
            if ( !xrtHttpRequestSetHeader(pHttpRequest, "Authorization", sValue) ) {
                xrtFree(sValue);
                return XRT_NET_ERROR;
            }
            xrtFree(sValue);
            break;
        }
        case XLLM_AUTH_NONE:
        default:
            break;
    }

    sVersion = pProfile->tProviderOptions.sAnthropicApiVersion;
    if ( !sVersion || !sVersion[0] ) {
        sVersion = "2023-06-01";
    }
    if ( !xrtHttpRequestSetHeader(pHttpRequest, "anthropic-version", sVersion) ) {
        return XRT_NET_ERROR;
    }

    if ( pProfile->tProviderOptions.iAnthropicBetaHeaderCount > 0u &&
         pProfile->tProviderOptions.psAnthropicBetaHeaders ) {
        size_t iTotalLen = 0u;
        char *sJoined;
        size_t iWrite = 0u;

        for ( i = 0; i < pProfile->tProviderOptions.iAnthropicBetaHeaderCount; ++i ) {
            const char *sValue = pProfile->tProviderOptions.psAnthropicBetaHeaders[i];
            if ( sValue && sValue[0] ) {
                iTotalLen += strlen(sValue) + 1u;
            }
        }

        if ( iTotalLen > 0u ) {
            sJoined = (char *)xrtCalloc(iTotalLen + 1u, sizeof(char));
            if ( !sJoined ) {
                return XRT_NET_ERROR;
            }
            for ( i = 0; i < pProfile->tProviderOptions.iAnthropicBetaHeaderCount; ++i ) {
                const char *sValue = pProfile->tProviderOptions.psAnthropicBetaHeaders[i];
                size_t iValueLen;
                if ( !sValue || !sValue[0] ) {
                    continue;
                }
                iValueLen = strlen(sValue);
                if ( iWrite > 0u ) {
                    sJoined[iWrite++] = ',';
                }
                memcpy(sJoined + iWrite, sValue, iValueLen);
                iWrite += iValueLen;
            }
            if ( !xrtHttpRequestSetHeader(pHttpRequest, "anthropic-beta", sJoined) ) {
                xrtFree(sJoined);
                return XRT_NET_ERROR;
            }
            xrtFree(sJoined);
        }
    }

    for ( i = 0; i < pProfile->iDefaultHeaderCount; ++i ) {
        if ( pProfile->pDefaultHeaders[i].sName && pProfile->pDefaultHeaders[i].sValue ) {
            if ( !xrtHttpRequestSetHeader(pHttpRequest, pProfile->pDefaultHeaders[i].sName, pProfile->pDefaultHeaders[i].sValue) ) {
                return XRT_NET_ERROR;
            }
        }
    }

    return XRT_NET_OK;
}

static int xllm__anthropic_append_message_or_system(
    xllm__json_builder *pSystem,
    bool *pbHasSystem,
    xllm__json_builder *pMessages,
    bool *pbHasMessage,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    char *sContent = NULL;
    int iStatus = XRT_NET_OK;
    const char *sRole;
    bool bArrayContent;

    if ( !pSystem || !pbHasSystem || !pMessages || !pbHasMessage || !pMessage ) {
        return XRT_NET_ERROR;
    }

    if ( pMessage->eRole == XLLM_ROLE_SYSTEM ) {
        iStatus = xllm__anthropic_message_to_text(pMessage, &sContent, pError);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
        if ( *pbHasSystem && !xllm__json_builder_append_cstr(pSystem, "\n\n") ) {
            xllm__free_cstr(&sContent);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pSystem, sContent ? sContent : "") ) {
            xllm__free_cstr(&sContent);
            return XRT_NET_ERROR;
        }
        *pbHasSystem = true;
        xllm__free_cstr(&sContent);
        return XRT_NET_OK;
    }

    bArrayContent = xllm__anthropic_message_requires_array_content(pMessage);

    if ( pMessage->eRole == XLLM_ROLE_USER || pMessage->eRole == XLLM_ROLE_TOOL ) {
        sRole = "user";
    } else if ( pMessage->eRole == XLLM_ROLE_ASSISTANT ) {
        sRole = "assistant";
    } else {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "anthropic-native adapter only supports system, user, assistant, and tool messages");
        return XRT_NET_ERROR;
    }

    if ( *pbHasMessage && !xllm__json_builder_append_char(pMessages, ',') ) {
        return XRT_NET_ERROR;
    }
    if ( !xllm__json_builder_append_cstr(pMessages, "{\"role\":") ) {
        return XRT_NET_ERROR;
    }
    if ( !xllm__json_builder_append_escaped(pMessages, sRole) ) {
        return XRT_NET_ERROR;
    }
    if ( !xllm__json_builder_append_cstr(pMessages, ",\"content\":") ) {
        return XRT_NET_ERROR;
    }
    if ( bArrayContent ) {
        if ( xllm__anthropic_append_message_blocks(pMessages, pMessage, pError) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    } else {
        iStatus = xllm__anthropic_message_to_text(pMessage, &sContent, pError);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
        if ( !xllm__json_builder_append_escaped(pMessages, sContent ? sContent : "") ) {
            xllm__free_cstr(&sContent);
            return XRT_NET_ERROR;
        }
    }
    if ( !xllm__json_builder_append_char(pMessages, '}') ) {
        xllm__free_cstr(&sContent);
        return XRT_NET_ERROR;
    }

    *pbHasMessage = true;
    xllm__free_cstr(&sContent);
    return XRT_NET_OK;
}

static uint32 xllm__anthropic_default_max_tokens(const xllm_profile *pProfile, const xllm_request *pRequest)
{
    const xllm_model_binding *pBinding;
    bool bNeedsMultimodal;

    if ( !pProfile || !pRequest ) {
        return 1024u;
    }

    bNeedsMultimodal = xllm__openai_request_uses_multimodal(pRequest);
    pBinding = xllm__select_request_binding(pProfile, pRequest, bNeedsMultimodal, NULL);
    if ( pBinding ) {
        if ( pBinding->tCaps.uRecommendedOutputReserve > 0u ) {
            return pBinding->tCaps.uRecommendedOutputReserve;
        }
        if ( pBinding->tCaps.uMaxOutputTokens > 0u ) {
            return pBinding->tCaps.uMaxOutputTokens;
        }
        if ( pBinding->tCaps.tMaxOutputTokensRule.eKind == XLLM_PARAM_RULE_FIXED &&
             pBinding->tCaps.tMaxOutputTokensRule.uFixed > 0u ) {
            return pBinding->tCaps.tMaxOutputTokensRule.uFixed;
        }
    }

    return 1024u;
}

static int xllm__anthropic_build_chat_body(
    xllm__json_builder *pBody,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_effective_params *pEffectiveParams,
    const xllm_call_options *pOptions,
    const char *sModel,
    bool bStream,
    xllm_error *pError
)
{
    xllm__json_builder tSystem;
    xllm__json_builder tMessages;
    bool bHasSystem = false;
    bool bHasMessage = false;
    size_t i;
    uint32 uMaxTokens;

    if ( !pBody || !pProfile || !pRequest || !pEffectiveParams || !sModel ) {
        return XRT_NET_ERROR;
    }

    if ( pEffectiveParams->tResponseFormat.eKind != XLLM_RESPONSE_TEXT &&
         (!pOptions || !pOptions->bBestEffortStructuredOutput) ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "anthropic-native adapter currently supports structured output only in best-effort mode");
        return XRT_NET_ERROR;
    }

    memset(&tSystem, 0, sizeof(tSystem));
    memset(&tMessages, 0, sizeof(tMessages));

    for ( i = 0; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            if ( xllm__anthropic_append_message_or_system(
                    &tSystem,
                    &bHasSystem,
                    &tMessages,
                    &bHasMessage,
                    &pRequest->pContextBlocks[i].pMessages[j],
                    pError
                 ) != XRT_NET_OK ) {
                xllm__json_builder_reset(&tSystem);
                xllm__json_builder_reset(&tMessages);
                return XRT_NET_ERROR;
            }
        }
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        if ( xllm__anthropic_append_message_or_system(
                &tSystem,
                &bHasSystem,
                &tMessages,
                &bHasMessage,
                &pRequest->pMessages[i],
                pError
             ) != XRT_NET_OK ) {
            xllm__json_builder_reset(&tSystem);
            xllm__json_builder_reset(&tMessages);
            return XRT_NET_ERROR;
        }
    }

    if ( !bHasMessage ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native request has no user/assistant messages");
        xllm__json_builder_reset(&tSystem);
        xllm__json_builder_reset(&tMessages);
        return XRT_NET_ERROR;
    }

    uMaxTokens = pEffectiveParams->tGeneration.tMaxOutputTokens.bSet
        ? pEffectiveParams->tGeneration.tMaxOutputTokens.iValue
        : xllm__anthropic_default_max_tokens(pProfile, pRequest);

    if ( !xllm__json_builder_append_char(pBody, '{') ) goto fail;
    if ( !xllm__json_builder_append_cstr(pBody, "\"model\":") ) goto fail;
    if ( !xllm__json_builder_append_escaped(pBody, sModel) ) goto fail;
    if ( !xllm__json_builder_append_cstr(pBody, ",\"max_tokens\":") ) goto fail;
    if ( !xllm__json_builder_append_u32(pBody, uMaxTokens) ) goto fail;

    if ( bHasSystem ) {
        char *sSystem = xllm__json_builder_detach(&tSystem);
        if ( !sSystem ) goto fail;
        if ( !xllm__json_builder_append_cstr(pBody, ",\"system\":") ) {
            xrtFree(sSystem);
            goto fail;
        }
        if ( !xllm__json_builder_append_escaped(pBody, sSystem) ) {
            xrtFree(sSystem);
            goto fail;
        }
        xrtFree(sSystem);
    }

    if ( !xllm__json_builder_append_cstr(pBody, ",\"messages\":[") ) goto fail;
    if ( tMessages.iLen > 0u && !xllm__json_builder_append_bytes(pBody, tMessages.pData, tMessages.iLen) ) goto fail;
    if ( !xllm__json_builder_append_char(pBody, ']') ) goto fail;

    if ( pEffectiveParams->tGeneration.tTemperature.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"temperature\":") ) goto fail;
        if ( !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTemperature.fValue) ) goto fail;
    }
    if ( pEffectiveParams->tGeneration.tTopP.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"top_p\":") ) goto fail;
        if ( !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTopP.fValue) ) goto fail;
    }
    if ( pEffectiveParams->tGeneration.iStopCount > 0u && pEffectiveParams->tGeneration.psStop ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"stop_sequences\":[") ) goto fail;
        for ( i = 0; i < pEffectiveParams->tGeneration.iStopCount; ++i ) {
            if ( i > 0u && !xllm__json_builder_append_char(pBody, ',') ) goto fail;
            if ( !xllm__json_builder_append_escaped(
                    pBody,
                    pEffectiveParams->tGeneration.psStop[i] ? pEffectiveParams->tGeneration.psStop[i] : ""
                 ) ) goto fail;
        }
        if ( !xllm__json_builder_append_char(pBody, ']') ) goto fail;
    }
    if ( xllm__anthropic_append_reasoning(pBody, &pEffectiveParams->tReasoning, uMaxTokens) != XRT_NET_OK ) {
        goto fail;
    }
    if ( pRequest->iToolCount > 0u ) {
        if ( xllm__anthropic_append_tools(pBody, pRequest, pError) != XRT_NET_OK ) goto fail;
        if ( xllm__anthropic_append_tool_policy(pBody, pRequest, &pEffectiveParams->tReasoning, pError) != XRT_NET_OK ) goto fail;
    }
    if ( bStream ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"stream\":true") ) goto fail;
    }

    if ( !xllm__json_builder_append_char(pBody, '}') ) goto fail;

    xllm__json_builder_reset(&tSystem);
    xllm__json_builder_reset(&tMessages);
    return XRT_NET_OK;

fail:
    xllm__json_builder_reset(&tSystem);
    xllm__json_builder_reset(&tMessages);
    return XRT_NET_ERROR;
}

static void xllm__anthropic_fill_error_from_http(xllm_error *pError, const xhttpresponse *pHttpResponse, xvalue tRoot, const char *sRequestId)
{
    xvalue tErrorObj;
    const char *sMessage = "upstream request failed";

    if ( !pError ) {
        return;
    }

    if ( pHttpResponse ) {
        if ( pHttpResponse->iStatusCode == 400u ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, sMessage);
        } else if ( pHttpResponse->iStatusCode == 401u || pHttpResponse->iStatusCode == 403u ) {
            xllm__error_set(pError, XLLM_ERROR_AUTH, sMessage);
        } else if ( pHttpResponse->iStatusCode == 404u ) {
            xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, sMessage);
        } else if ( pHttpResponse->iStatusCode == 429u ) {
            xllm__error_set(pError, XLLM_ERROR_RATE_LIMIT, sMessage);
        } else if ( pHttpResponse->iStatusCode >= 500u || pHttpResponse->iStatusCode == 529u ) {
            xllm__error_set(pError, XLLM_ERROR_UPSTREAM_5XX, sMessage);
        } else if ( pHttpResponse->iStatusCode >= 400u ) {
            xllm__error_set(pError, XLLM_ERROR_UPSTREAM_4XX, sMessage);
        } else {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, sMessage);
        }
        pError->iHttpStatus = (int32)pHttpResponse->iStatusCode;
    }

    if ( sRequestId ) {
        pError->sRequestId = xllm__dup_cstr(sRequestId);
    }

    tErrorObj = xllm__json_table_get(tRoot, "error");
    if ( tErrorObj && xvoType(tErrorObj) == XVO_DT_TABLE ) {
        const char *sProviderMessage = xllm__json_table_get_text(tErrorObj, "message");
        const char *sProviderCode = xllm__json_table_get_text(tErrorObj, "type");
        if ( sProviderMessage ) {
            xllm__free_cstr((char **)&pError->sMessage);
            pError->sMessage = xllm__dup_cstr(sProviderMessage);
            pError->sProviderMessage = xllm__dup_cstr(sProviderMessage);
        }
        if ( sProviderCode ) {
            pError->sProviderCode = xllm__dup_cstr(sProviderCode);
        }
    }
}

static int xllm__anthropic_parse_content(
    xvalue tContent,
    xllm_content_part **ppParts,
    size_t *piPartCount,
    char **psVisibleText,
    char **psThinking,
    size_t *piToolUseCount,
    xllm_error *pError
)
{
    xllm__json_builder tVisibleText;
    xllm__json_builder tThinking;
    xllm_content_part *pParts = NULL;
    size_t iPartCount = 0u;
    size_t iPartCapacity = 0u;
    size_t i;

    if ( ppParts ) {
        *ppParts = NULL;
    }
    if ( piPartCount ) {
        *piPartCount = 0u;
    }
    if ( psVisibleText ) {
        *psVisibleText = NULL;
    }
    if ( psThinking ) {
        *psThinking = NULL;
    }
    if ( piToolUseCount ) {
        *piToolUseCount = 0u;
    }
    if ( !tContent || xvoType(tContent) != XVO_DT_ARRAY ) {
        return XRT_NET_OK;
    }

    memset(&tVisibleText, 0, sizeof(tVisibleText));
    memset(&tThinking, 0, sizeof(tThinking));

    for ( i = 0; i < (size_t)xvoArrayItemCount(tContent); ++i ) {
        xvalue tItem = xvoArrayGetValue(tContent, (uint32)i);
        const char *sType;

        if ( !tItem || xvoType(tItem) != XVO_DT_TABLE ) {
            continue;
        }

        sType = xllm__json_table_get_text(tItem, "type");
        if ( sType && strcmp(sType, "text") == 0 ) {
            const char *sText = xllm__json_table_get_text(tItem, "text");
            xllm_content_part tPart;

            if ( !sText ) {
                continue;
            }

            memset(&tPart, 0, sizeof(tPart));
            tPart.eKind = XLLM_PART_TEXT;
            tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
            tPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
            tPart.as.tSource.as.sText = xllm__dup_cstr(sText);
            if ( !tPart.as.tSource.sMimeType || !tPart.as.tSource.as.sText ) {
                xllm__content_part_free(&tPart);
                goto fail;
            }
            if ( xllm__openai_message_add_part(&pParts, &iPartCount, &iPartCapacity, &tPart) != XRT_NET_OK ) {
                xllm__content_part_free(&tPart);
                goto fail;
            }
            if ( tVisibleText.iLen > 0u && !xllm__json_builder_append_char(&tVisibleText, '\n') ) {
                goto fail;
            }
            if ( !xllm__json_builder_append_cstr(&tVisibleText, sText) ) {
                goto fail;
            }
        } else if ( sType && strcmp(sType, "thinking") == 0 ) {
            const char *sThinking = xllm__json_table_get_text(tItem, "thinking");
            if ( !sThinking ) {
                sThinking = xllm__json_table_get_text(tItem, "text");
            }
            if ( sThinking && sThinking[0] ) {
                if ( tThinking.iLen > 0u && !xllm__json_builder_append_char(&tThinking, '\n') ) goto fail;
                if ( !xllm__json_builder_append_cstr(&tThinking, sThinking) ) goto fail;
            }
        } else if ( sType && strcmp(sType, "tool_use") == 0 ) {
            if ( piToolUseCount ) {
                ++(*piToolUseCount);
            }
        } else if ( sType && (strcmp(sType, "image") == 0 || strcmp(sType, "document") == 0) ) {
            xvalue tSource = xllm__json_table_get(tItem, "source");
            const char *sSourceType = xllm__json_table_get_text(tSource, "type");
            const char *sMimeType = xllm__json_table_get_text(tSource, "media_type");
            const char *sName = xllm__json_table_get_text(tItem, "name");
            xllm_content_part tPart;

            memset(&tPart, 0, sizeof(tPart));
            tPart.eKind = (strcmp(sType, "image") == 0) ? XLLM_PART_IMAGE : XLLM_PART_FILE;
            tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType);
            tPart.as.tSource.sName = xllm__dup_cstr(sName);

            if ( sSourceType && strcmp(sSourceType, "url") == 0 ) {
                const char *sUrl = xllm__json_table_get_text(tSource, "url");
                if ( !sUrl || !sUrl[0] ) {
                    xllm__content_part_free(&tPart);
                    continue;
                }
                tPart.as.tSource.eKind = XLLM_SOURCE_URL;
                tPart.as.tSource.as.sUrl = xllm__dup_cstr(sUrl);
                if ( !tPart.as.tSource.as.sUrl ) {
                    xllm__content_part_free(&tPart);
                    goto fail;
                }
            } else if ( sSourceType && strcmp(sSourceType, "file") == 0 ) {
                const char *sFileId = xllm__json_table_get_text(tSource, "file_id");
                if ( !sFileId || !sFileId[0] ) {
                    xllm__content_part_free(&tPart);
                    continue;
                }
                tPart.as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
                tPart.as.tSource.as.sFileId = xllm__dup_cstr(sFileId);
                if ( !tPart.as.tSource.as.sFileId ) {
                    xllm__content_part_free(&tPart);
                    goto fail;
                }
            } else if ( sSourceType && strcmp(sSourceType, "base64") == 0 ) {
                const char *sData = xllm__json_table_get_text(tSource, "data");
                size_t iDecodedSize;
                void *pDecoded;

                if ( !sData || !sData[0] ) {
                    xllm__content_part_free(&tPart);
                    continue;
                }

                iDecodedSize = xllm__openai_base64_decoded_size(sData);
                pDecoded = xrtBase64Decode((str)sData, strlen(sData), NULL);
                if ( !pDecoded ) {
                    xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to decode anthropic-native artifact block");
                    xllm__content_part_free(&tPart);
                    goto fail;
                }

                tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_BYTES;
                tPart.as.tSource.as.tBytes.pData = pDecoded;
                tPart.as.tSource.as.tBytes.iSize = iDecodedSize;
            } else {
                xllm__content_part_free(&tPart);
                continue;
            }

            if ( xllm__openai_message_add_part(&pParts, &iPartCount, &iPartCapacity, &tPart) != XRT_NET_OK ) {
                xllm__content_part_free(&tPart);
                goto fail;
            }
        }
    }

    if ( psVisibleText && tVisibleText.iLen > 0u ) {
        *psVisibleText = xllm__json_builder_detach(&tVisibleText);
        if ( tVisibleText.iLen > 0u && !*psVisibleText ) goto fail;
    }
    if ( psThinking && tThinking.iLen > 0u ) {
        *psThinking = xllm__json_builder_detach(&tThinking);
        if ( tThinking.iLen > 0u && !*psThinking ) goto fail;
    }

    xllm__json_builder_reset(&tVisibleText);
    xllm__json_builder_reset(&tThinking);

    if ( ppParts ) {
        *ppParts = pParts;
        pParts = NULL;
    }
    if ( piPartCount ) {
        *piPartCount = iPartCount;
    }

    return XRT_NET_OK;

fail:
    if ( pParts ) {
        size_t j;
        for ( j = 0u; j < iPartCount; ++j ) {
            xllm__content_part_free(&pParts[j]);
        }
        xrtFree(pParts);
    }
    if ( psVisibleText && *psVisibleText ) {
        xrtFree(*psVisibleText);
        *psVisibleText = NULL;
    }
    if ( psThinking && *psThinking ) {
        xrtFree(*psThinking);
        *psThinking = NULL;
    }
    xllm__json_builder_reset(&tVisibleText);
    xllm__json_builder_reset(&tThinking);
    return XRT_NET_ERROR;
}

static int xllm__anthropic_build_response(
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xvalue tRoot,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_response *pResponse = NULL;
    xllm_effective_params tEffectiveParams;
    xvalue tContent;
    xvalue tUsage;
    const char *sStopReason;
    const char *sModel;
    xllm_content_part *pMessageParts = NULL;
    size_t iMessagePartCount = 0u;
    char *sVisibleText = NULL;
    char *sThinkingText = NULL;
    char *sThinkingSignature = NULL;
    char *sNormalizedJson = NULL;
    char *sRefusalText = NULL;
    xvalue tJsonValue = NULL;
    bool bJsonOutput = false;
    size_t iToolUseCount = 0u;
    size_t iOutputCount = 0u;
    size_t iOutputIndex = 0u;
    size_t i;

    if ( !pProfile || !pRequest || !ppResponse || !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "invalid anthropic-native response");
        return XRT_NET_ERROR;
    }

    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    if ( xllm__openai_fill_effective_params(&tEffectiveParams, pProfile, pRequest, XLLM_STREAM_OFF) != XRT_NET_OK ) {
        goto fail;
    }

    tContent = xllm__json_table_get(tRoot, "content");
    if ( xllm__anthropic_parse_content(
            tContent,
            &pMessageParts,
            &iMessagePartCount,
            &sVisibleText,
            &sThinkingText,
            &iToolUseCount,
            pError
         ) != XRT_NET_OK ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse anthropic-native content");
        goto fail;
    }
    sThinkingSignature = xllm__anthropic_find_thinking_signature(tContent);

    sStopReason = xllm__json_table_get_text(tRoot, "stop_reason");
    if ( !sStopReason || !sStopReason[0] ) {
        sStopReason = "end_turn";
    }

    if ( strcmp(sStopReason, "refusal") == 0 && sVisibleText && sVisibleText[0] ) {
        sRefusalText = xllm__dup_cstr(sVisibleText);
        if ( !sRefusalText ) {
            goto fail;
        }
    }

    if ( !sRefusalText &&
         tEffectiveParams.tResponseFormat.eKind != XLLM_RESPONSE_TEXT &&
         sVisibleText &&
         sVisibleText[0] ) {
        if ( xllm__openai_parse_structured_output(
                &tEffectiveParams.tResponseFormat,
                pOptions,
                sVisibleText,
                &tJsonValue,
                &sNormalizedJson,
                pError
             ) != XRT_NET_OK ) {
            goto fail;
        }
        bJsonOutput = (tJsonValue != NULL);
        if ( bJsonOutput && sNormalizedJson ) {
            xllm__free_cstr(&sVisibleText);
            sVisibleText = sNormalizedJson;
            sNormalizedJson = NULL;
        }
    }

    if ( sThinkingText && sThinkingText[0] ) {
        ++iOutputCount;
    }
    if ( sRefusalText && sRefusalText[0] ) {
        ++iOutputCount;
    } else if ( iMessagePartCount > 0u ) {
        ++iOutputCount;
    }
    iOutputCount += iToolUseCount;

    pResponse = (xllm_response *)xrtCalloc(1, sizeof(*pResponse));
    if ( !pResponse ) {
        goto fail;
    }

    pResponse->sId = xllm__dup_cstr(xllm__json_table_get_text(tRoot, "id"));
    pResponse->sProvider = xllm__dup_cstr(pProfile->sProvider ? pProfile->sProvider : "anthropic");
    pResponse->sProfileId = xllm__dup_cstr(pProfile->sId);
    sModel = xllm__json_table_get_text(tRoot, "model");
    pResponse->sModel = xllm__dup_cstr(sModel ? sModel : xllm__openai_select_model(pProfile, pRequest, NULL));
    pResponse->sFinishReason = xllm__dup_cstr(sStopReason);
    pResponse->sVisibleText = xllm__dup_cstr(sRefusalText ? sRefusalText : sVisibleText);

    if ( iOutputCount > 0u ) {
        pResponse->pOutputs = (xllm_output_item *)xrtCalloc(iOutputCount, sizeof(xllm_output_item));
        if ( !pResponse->pOutputs ) {
            goto fail;
        }
        pResponse->iOutputCount = iOutputCount;
    }

    if ( sThinkingText && sThinkingText[0] ) {
        xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];
        pOutput->eKind = XLLM_OUTPUT_THINKING;
        pOutput->as.tThinking.bVisible = true;
        pOutput->as.tThinking.sFormat = xllm__dup_cstr("full");
        pOutput->as.tThinking.sText = xllm__dup_cstr(sThinkingText);
        if ( xllm__anthropic_set_thinking_vendor_extra(&pOutput->as.tThinking, sThinkingSignature) != XRT_NET_OK ) {
            goto fail;
        }
    }

    if ( sRefusalText && sRefusalText[0] ) {
        xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];
        pOutput->eKind = XLLM_OUTPUT_REFUSAL;
        pOutput->as.tRefusal.sText = xllm__dup_cstr(sRefusalText);
        pResponse->tRefusal.sText = xllm__dup_cstr(sRefusalText);
        pResponse->eStatus = XLLM_STATUS_REFUSED;
    } else if ( iMessagePartCount > 0u ) {
        xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];
        pOutput->eKind = XLLM_OUTPUT_MESSAGE;
        pOutput->as.tMessage.iPartCount = iMessagePartCount;
        pOutput->as.tMessage.pParts = pMessageParts;
        pMessageParts = NULL;
        iMessagePartCount = 0u;

        if ( bJsonOutput && tJsonValue ) {
            size_t iFirstText = (size_t)-1;
            size_t iRead;
            size_t iWrite = 0u;

            for ( iRead = 0u; iRead < pOutput->as.tMessage.iPartCount; ++iRead ) {
                if ( iFirstText == (size_t)-1 &&
                     pOutput->as.tMessage.pParts[iRead].eKind == XLLM_PART_TEXT &&
                     pOutput->as.tMessage.pParts[iRead].as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT ) {
                    iFirstText = iRead;
                    break;
                }
            }

            if ( iFirstText != (size_t)-1 ) {
                xllm__content_part_free(&pOutput->as.tMessage.pParts[iFirstText]);
                pOutput->as.tMessage.pParts[iFirstText].eKind = XLLM_PART_JSON;
                pOutput->as.tMessage.pParts[iFirstText].as.tJsonValue = tJsonValue;
                tJsonValue = NULL;

                for ( iRead = 0u; iRead < pOutput->as.tMessage.iPartCount; ++iRead ) {
                    if ( iRead == iFirstText ) {
                        if ( iWrite != iRead ) {
                            pOutput->as.tMessage.pParts[iWrite] = pOutput->as.tMessage.pParts[iRead];
                            memset(&pOutput->as.tMessage.pParts[iRead], 0, sizeof(xllm_content_part));
                        }
                        ++iWrite;
                        continue;
                    }

                    if ( pOutput->as.tMessage.pParts[iRead].eKind == XLLM_PART_TEXT &&
                         pOutput->as.tMessage.pParts[iRead].as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT ) {
                        xllm__content_part_free(&pOutput->as.tMessage.pParts[iRead]);
                        continue;
                    }

                    if ( iWrite != iRead ) {
                        pOutput->as.tMessage.pParts[iWrite] = pOutput->as.tMessage.pParts[iRead];
                        memset(&pOutput->as.tMessage.pParts[iRead], 0, sizeof(xllm_content_part));
                    }
                    ++iWrite;
                }

                pOutput->as.tMessage.iPartCount = iWrite;
            }
        }
    }

    if ( iToolUseCount > 0u ) {
        for ( i = 0; i < (size_t)xvoArrayItemCount(tContent); ++i ) {
            xvalue tItem = xvoArrayGetValue(tContent, (uint32)i);
            const char *sType;
            if ( !tItem || xvoType(tItem) != XVO_DT_TABLE ) {
                continue;
            }
            sType = xllm__json_table_get_text(tItem, "type");
            if ( !sType || strcmp(sType, "tool_use") != 0 ) {
                continue;
            }

            {
                xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];
                const char *sCallId = xllm__json_table_get_text(tItem, "id");
                const char *sToolName = xllm__json_table_get_text(tItem, "name");
                xvalue tInput = xllm__json_table_get(tItem, "input");
                char *sArguments = NULL;

                if ( tInput && xvoType(tInput) != XVO_DT_NULL ) {
                    sArguments = (char *)xrtStringifyJSON(tInput, 0, NULL);
                }
                if ( !sArguments ) {
                    sArguments = xllm__dup_cstr("{}");
                }
                if ( !sArguments ) {
                    goto fail;
                }

                pOutput->eKind = XLLM_OUTPUT_TOOL_CALL;
                pOutput->as.tToolCall.sCallId = xllm__dup_cstr(sCallId);
                pOutput->as.tToolCall.sToolId = xllm__dup_cstr(sToolName);
                pOutput->as.tToolCall.sToolName = xllm__dup_cstr(sToolName);
                pOutput->as.tToolCall.sArgumentsJson = sArguments;
            }
        }
    }

    if ( strcmp(sStopReason, "tool_use") == 0 || iToolUseCount > 0u ) {
        pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
    } else if ( pResponse->eStatus != XLLM_STATUS_REFUSED ) {
        if ( strcmp(sStopReason, "max_tokens") == 0 ||
             strcmp(sStopReason, "model_context_window_exceeded") == 0 ||
             strcmp(sStopReason, "pause_turn") == 0 ) {
            pResponse->eStatus = XLLM_STATUS_INCOMPLETE;
        } else {
            pResponse->eStatus = XLLM_STATUS_COMPLETED;
        }
    }

    tUsage = xllm__json_table_get(tRoot, "usage");
    if ( tUsage && xvoType(tUsage) == XVO_DT_TABLE ) {
        pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tUsage, "input_tokens");
        pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tUsage, "output_tokens");
        pResponse->tUsage.uCachedInputTokens = xllm__json_table_get_u32(tUsage, "cache_read_input_tokens");
    }

    pResponse->tEffectiveParams = tEffectiveParams;
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    pResponse->tRaw = tRoot;

    *ppResponse = pResponse;
    xllm__free_cstr(&sVisibleText);
    xllm__free_cstr(&sThinkingText);
    xllm__free_cstr(&sThinkingSignature);
    xllm__free_cstr(&sNormalizedJson);
    xllm__free_cstr(&sRefusalText);
    if ( tJsonValue ) {
        xvoUnref(tJsonValue);
    }
    return XRT_NET_OK;

fail:
    if ( pMessageParts ) {
        size_t j;
        for ( j = 0u; j < iMessagePartCount; ++j ) {
            xllm__content_part_free(&pMessageParts[j]);
        }
        xrtFree(pMessageParts);
    }
    xllm__free_cstr(&sVisibleText);
    xllm__free_cstr(&sThinkingText);
    xllm__free_cstr(&sThinkingSignature);
    xllm__free_cstr(&sNormalizedJson);
    xllm__free_cstr(&sRefusalText);
    xllm__effective_params_reset(&tEffectiveParams);
    if ( tJsonValue ) {
        xvoUnref(tJsonValue);
    }
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    return XRT_NET_ERROR;
}

static void xllm__anthropic_fill_error_from_event(xllm_error *pError, xvalue tRoot, const char *sRequestId)
{
    xvalue tErrorObj;
    const char *sType = NULL;
    const char *sMessage = "anthropic-native stream error";

    if ( !pError ) {
        return;
    }

    tErrorObj = xllm__json_table_get(tRoot, "error");
    if ( tErrorObj && xvoType(tErrorObj) == XVO_DT_TABLE ) {
        const char *sProviderType = xllm__json_table_get_text(tErrorObj, "type");
        const char *sProviderMessage = xllm__json_table_get_text(tErrorObj, "message");
        if ( sProviderType && sProviderType[0] ) {
            sType = sProviderType;
        }
        if ( sProviderMessage && sProviderMessage[0] ) {
            sMessage = sProviderMessage;
        }
    }

    if ( sType && strcmp(sType, "invalid_request_error") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, sMessage);
    } else if ( sType && (strcmp(sType, "authentication_error") == 0 || strcmp(sType, "permission_error") == 0) ) {
        xllm__error_set(pError, XLLM_ERROR_AUTH, sMessage);
    } else if ( sType && strcmp(sType, "not_found_error") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, sMessage);
    } else if ( sType && strcmp(sType, "rate_limit_error") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_RATE_LIMIT, sMessage);
    } else if ( sType && (strcmp(sType, "overloaded_error") == 0 || strcmp(sType, "api_error") == 0) ) {
        xllm__error_set(pError, XLLM_ERROR_UPSTREAM_5XX, sMessage);
    } else {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, sMessage);
    }

    if ( sRequestId && sRequestId[0] ) {
        pError->sRequestId = xllm__dup_cstr(sRequestId);
    }
    if ( sType && sType[0] ) {
        pError->sProviderCode = xllm__dup_cstr(sType);
    }
    if ( sMessage && sMessage[0] ) {
        pError->sProviderMessage = xllm__dup_cstr(sMessage);
    }
}

static void xllm__anthropic_stream_set_finish_reason(xllm__openai_stream_context *pCtx, const char *sStopReason)
{
    if ( !pCtx || !pCtx->pResponse || !sStopReason || !sStopReason[0] ) {
        return;
    }

    xllm__free_cstr((char **)&pCtx->pResponse->sFinishReason);
    pCtx->pResponse->sFinishReason = xllm__dup_cstr(sStopReason);
}

static int xllm__anthropic_stream_apply_usage(xllm__openai_stream_context *pCtx, xvalue tUsage)
{
    xllm_event tEvent;
    xvalue tValue;

    if ( !pCtx || !pCtx->pResponse || !tUsage || xvoType(tUsage) != XVO_DT_TABLE ) {
        return XRT_NET_OK;
    }

    tValue = xllm__json_table_get(tUsage, "input_tokens");
    if ( tValue && xvoType(tValue) == XVO_DT_INT ) {
        pCtx->pResponse->tUsage.uInputTokens = (uint32)xvoGetInt(tValue);
    }
    tValue = xllm__json_table_get(tUsage, "output_tokens");
    if ( tValue && xvoType(tValue) == XVO_DT_INT ) {
        pCtx->pResponse->tUsage.uOutputTokens = (uint32)xvoGetInt(tValue);
    }
    tValue = xllm__json_table_get(tUsage, "cache_read_input_tokens");
    if ( tValue && xvoType(tValue) == XVO_DT_INT ) {
        pCtx->pResponse->tUsage.uCachedInputTokens = (uint32)xvoGetInt(tValue);
    }
    ++pCtx->uUsageCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_USAGE;
    tEvent.as.tUsage.tUsage = pCtx->pResponse->tUsage;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__anthropic_stream_attach_signature(xllm__openai_stream_context *pCtx, const char *sSignature)
{
    xllm_output_item *pOutput = NULL;

    if ( !pCtx || !sSignature || !sSignature[0] ) {
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_ensure_thinking_output(pCtx, &pOutput) != XRT_NET_OK ) {
        return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
    }
    if ( !pOutput || pOutput->eKind != XLLM_OUTPUT_THINKING ) {
        return XRT_NET_ERROR;
    }

    return xllm__anthropic_set_thinking_vendor_extra(&pOutput->as.tThinking, sSignature);
}

static int xllm__anthropic_stream_process_payload(
    xllm__openai_stream_context *pCtx,
    const char *sPayload,
    size_t iPayloadLen,
    const char *sRequestId
)
{
    xvalue tRoot = NULL;
    const char *sType;

    if ( !pCtx || !sPayload ) {
        return XRT_NET_ERROR;
    }

    tRoot = xrtParseJSON((str)sPayload, iPayloadLen);
    if ( !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "failed to parse anthropic-native stream event");
        if ( tRoot ) {
            xvoUnref(tRoot);
        }
        return XRT_NET_ERROR;
    }

    sType = xllm__json_table_get_text(tRoot, "type");
    if ( !sType || !sType[0] ) {
        xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "anthropic-native stream event missing type");
        xvoUnref(tRoot);
        return XRT_NET_ERROR;
    }

    if ( strcmp(sType, "message_stop") == 0 ) {
        pCtx->bDone = true;
        ++pCtx->uPayloadCount;
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "done", iPayloadLen);
        xvoUnref(tRoot);
        return XRT_NET_OK;
    }

    if ( strcmp(sType, "ping") == 0 || strcmp(sType, "content_block_stop") == 0 ) {
        ++pCtx->uPayloadCount;
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "payload", iPayloadLen);
        xvoUnref(tRoot);
        return XRT_NET_OK;
    }

    if ( strcmp(sType, "error") == 0 ) {
        xllm__anthropic_fill_error_from_event(pCtx->pError, tRoot, sRequestId);
        xvoUnref(tRoot);
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_response(pCtx) != XRT_NET_OK ) {
        xvoUnref(tRoot);
        return XRT_NET_ERROR;
    }
    if ( xllm__openai_stream_emit_start(pCtx) != XRT_NET_OK ) {
        xvoUnref(tRoot);
        return XRT_NET_CANCELLED;
    }

    if ( strcmp(sType, "message_start") == 0 ) {
        xvalue tMessage = xllm__json_table_get(tRoot, "message");
        if ( tMessage && xvoType(tMessage) == XVO_DT_TABLE ) {
            const char *sId = xllm__json_table_get_text(tMessage, "id");
            const char *sModel = xllm__json_table_get_text(tMessage, "model");
            if ( !pCtx->pResponse->sId && sId ) {
                pCtx->pResponse->sId = xllm__dup_cstr(sId);
            }
            if ( !pCtx->pResponse->sModel ) {
                pCtx->pResponse->sModel = xllm__dup_cstr(sModel ? sModel : pCtx->sSelectedModel);
            }
            xllm__anthropic_stream_set_finish_reason(pCtx, xllm__json_table_get_text(tMessage, "stop_reason"));
            if ( xllm__anthropic_stream_apply_usage(pCtx, xllm__json_table_get(tMessage, "usage")) != XRT_NET_OK ) {
                xvoUnref(tRoot);
                return XRT_NET_CANCELLED;
            }
        }
    } else if ( strcmp(sType, "content_block_start") == 0 ) {
        xvalue tBlock = xllm__json_table_get(tRoot, "content_block");
        xvalue tIndexValue = xllm__json_table_get(tRoot, "index");
        size_t iToolIndex = 0u;

        if ( tIndexValue && xvoType(tIndexValue) == XVO_DT_INT ) {
            iToolIndex = (size_t)xvoGetInt(tIndexValue);
        }
        if ( tBlock && xvoType(tBlock) == XVO_DT_TABLE ) {
            const char *sBlockType = xllm__json_table_get_text(tBlock, "type");
            if ( sBlockType && strcmp(sBlockType, "text") == 0 ) {
                const char *sText = xllm__json_table_get_text(tBlock, "text");
                if ( sText && xllm__openai_stream_append_text(pCtx, sText) != XRT_NET_OK ) {
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
            } else if ( sBlockType && strcmp(sBlockType, "thinking") == 0 ) {
                const char *sThinking = xllm__json_table_get_text(tBlock, "thinking");
                const char *sSignature = xllm__json_table_get_text(tBlock, "signature");
                if ( !sThinking ) {
                    sThinking = xllm__json_table_get_text(tBlock, "text");
                }
                if ( sThinking && xllm__openai_stream_append_thinking(pCtx, sThinking) != XRT_NET_OK ) {
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
                if ( sSignature && xllm__anthropic_stream_attach_signature(pCtx, sSignature) != XRT_NET_OK ) {
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
            } else if ( sBlockType && strcmp(sBlockType, "tool_use") == 0 ) {
                const char *sCallId = xllm__json_table_get_text(tBlock, "id");
                const char *sToolName = xllm__json_table_get_text(tBlock, "name");
                xvalue tInput = xllm__json_table_get(tBlock, "input");
                char *sArguments = NULL;

                if ( tInput && xvoType(tInput) != XVO_DT_NULL ) {
                    sArguments = (char *)xrtStringifyJSON(tInput, 0, NULL);
                }
                if ( !sArguments ) {
                    sArguments = xllm__dup_cstr("");
                }
                if ( !sArguments ) {
                    xvoUnref(tRoot);
                    return XRT_NET_ERROR;
                }
                if ( xllm__openai_stream_append_tool_delta(pCtx, iToolIndex, sCallId, sToolName, sArguments) != XRT_NET_OK ) {
                    xrtFree(sArguments);
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
                xrtFree(sArguments);
            } else if ( sBlockType && (strcmp(sBlockType, "image") == 0 || strcmp(sBlockType, "document") == 0) ) {
                xvalue tSource = xllm__json_table_get(tBlock, "source");
                const char *sSourceType = xllm__json_table_get_text(tSource, "type");
                const char *sMimeType = xllm__json_table_get_text(tSource, "media_type");
                const char *sName = xllm__json_table_get_text(tBlock, "name");
                bool bHavePart = false;
                xllm_content_part tPart;

                memset(&tPart, 0, sizeof(tPart));
                tPart.eKind = (strcmp(sBlockType, "image") == 0) ? XLLM_PART_IMAGE : XLLM_PART_FILE;
                tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType);
                tPart.as.tSource.sName = xllm__dup_cstr(sName);

                if ( sSourceType && strcmp(sSourceType, "url") == 0 ) {
                    const char *sUrl = xllm__json_table_get_text(tSource, "url");
                    if ( !sUrl || !sUrl[0] ) {
                        xllm__content_part_free(&tPart);
                    } else {
                        tPart.as.tSource.eKind = XLLM_SOURCE_URL;
                        tPart.as.tSource.as.sUrl = xllm__dup_cstr(sUrl);
                        if ( !tPart.as.tSource.as.sUrl ) {
                            xllm__content_part_free(&tPart);
                            xvoUnref(tRoot);
                            return XRT_NET_ERROR;
                        }
                        bHavePart = true;
                    }
                } else if ( sSourceType && strcmp(sSourceType, "file") == 0 ) {
                    const char *sFileId = xllm__json_table_get_text(tSource, "file_id");
                    if ( !sFileId || !sFileId[0] ) {
                        xllm__content_part_free(&tPart);
                    } else {
                        tPart.as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
                        tPart.as.tSource.as.sFileId = xllm__dup_cstr(sFileId);
                        if ( !tPart.as.tSource.as.sFileId ) {
                            xllm__content_part_free(&tPart);
                            xvoUnref(tRoot);
                            return XRT_NET_ERROR;
                        }
                        bHavePart = true;
                    }
                } else if ( sSourceType && strcmp(sSourceType, "base64") == 0 ) {
                    const char *sData = xllm__json_table_get_text(tSource, "data");
                    size_t iDecodedSize;
                    void *pDecoded;

                    if ( !sData || !sData[0] ) {
                        xllm__content_part_free(&tPart);
                    } else {
                        iDecodedSize = xllm__openai_base64_decoded_size(sData);
                        pDecoded = xrtBase64Decode((str)sData, strlen(sData), NULL);
                        if ( !pDecoded ) {
                            xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "failed to decode anthropic-native streamed artifact block");
                            xllm__content_part_free(&tPart);
                            xvoUnref(tRoot);
                            return XRT_NET_ERROR;
                        }

                        tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_BYTES;
                        tPart.as.tSource.as.tBytes.pData = pDecoded;
                        tPart.as.tSource.as.tBytes.iSize = iDecodedSize;
                        bHavePart = true;
                    }
                } else {
                    xllm__content_part_free(&tPart);
                }

                if ( bHavePart ) {
                    if ( xllm__openai_stream_append_message_part(pCtx, &tPart) != XRT_NET_OK ) {
                        xllm__content_part_free(&tPart);
                        xvoUnref(tRoot);
                        return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                    }
                }
            }
        }
    } else if ( strcmp(sType, "content_block_delta") == 0 ) {
        xvalue tDelta = xllm__json_table_get(tRoot, "delta");
        xvalue tIndexValue = xllm__json_table_get(tRoot, "index");
        size_t iToolIndex = 0u;

        if ( tIndexValue && xvoType(tIndexValue) == XVO_DT_INT ) {
            iToolIndex = (size_t)xvoGetInt(tIndexValue);
        }
        if ( tDelta && xvoType(tDelta) == XVO_DT_TABLE ) {
            const char *sDeltaType = xllm__json_table_get_text(tDelta, "type");
            if ( sDeltaType && strcmp(sDeltaType, "text_delta") == 0 ) {
                const char *sText = xllm__json_table_get_text(tDelta, "text");
                if ( sText && xllm__openai_stream_append_text(pCtx, sText) != XRT_NET_OK ) {
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
            } else if ( sDeltaType && strcmp(sDeltaType, "thinking_delta") == 0 ) {
                const char *sThinking = xllm__json_table_get_text(tDelta, "thinking");
                if ( !sThinking ) {
                    sThinking = xllm__json_table_get_text(tDelta, "text");
                }
                if ( sThinking && xllm__openai_stream_append_thinking(pCtx, sThinking) != XRT_NET_OK ) {
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
            } else if ( sDeltaType && strcmp(sDeltaType, "signature_delta") == 0 ) {
                const char *sSignature = xllm__json_table_get_text(tDelta, "signature");
                if ( sSignature && xllm__anthropic_stream_attach_signature(pCtx, sSignature) != XRT_NET_OK ) {
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
            } else if ( sDeltaType && strcmp(sDeltaType, "input_json_delta") == 0 ) {
                const char *sPartialJson = xllm__json_table_get_text(tDelta, "partial_json");
                if ( sPartialJson && xllm__openai_stream_append_tool_delta(pCtx, iToolIndex, NULL, NULL, sPartialJson) != XRT_NET_OK ) {
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
            }
        }
    } else if ( strcmp(sType, "message_delta") == 0 ) {
        xvalue tDelta = xllm__json_table_get(tRoot, "delta");
        if ( tDelta && xvoType(tDelta) == XVO_DT_TABLE ) {
            xllm__anthropic_stream_set_finish_reason(pCtx, xllm__json_table_get_text(tDelta, "stop_reason"));
        }
        if ( xllm__anthropic_stream_apply_usage(pCtx, xllm__json_table_get(tRoot, "usage")) != XRT_NET_OK ) {
            xvoUnref(tRoot);
            return XRT_NET_CANCELLED;
        }
    }

    ++pCtx->uPayloadCount;
    xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "payload", iPayloadLen);
    xvoUnref(tRoot);
    return XRT_NET_OK;
}

static int xllm__anthropic_stream_process_event_block(
    xllm__openai_stream_context *pCtx,
    const char *sEvent,
    size_t iEventLen,
    const char *sRequestId
)
{
    xllm__json_builder tPayload;
    size_t iOffset = 0u;
    bool bSawData = false;
    int iStatus;

    if ( !pCtx || !sEvent ) {
        return XRT_NET_ERROR;
    }

    memset(&tPayload, 0, sizeof(tPayload));
    while ( iOffset < iEventLen ) {
        size_t iLineStart = iOffset;
        size_t iLineLen;
        const char *sLine;

        while ( iOffset < iEventLen && sEvent[iOffset] != '\n' ) {
            ++iOffset;
        }
        iLineLen = iOffset - iLineStart;
        if ( iOffset < iEventLen && sEvent[iOffset] == '\n' ) {
            ++iOffset;
        }
        if ( iLineLen > 0u && sEvent[iLineStart + iLineLen - 1u] == '\r' ) {
            --iLineLen;
        }
        sLine = sEvent + iLineStart;

        if ( iLineLen == 0u || sLine[0] == ':' ) {
            continue;
        }
        if ( iLineLen >= 5u && memcmp(sLine, "data:", 5u) == 0 ) {
            const char *sData = sLine + 5u;
            size_t iDataLen = iLineLen - 5u;
            while ( iDataLen > 0u && (*sData == ' ' || *sData == '\t') ) {
                ++sData;
                --iDataLen;
            }
            if ( bSawData && !xllm__json_builder_append_char(&tPayload, '\n') ) {
                xllm__json_builder_reset(&tPayload);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_bytes(&tPayload, sData, iDataLen) ) {
                xllm__json_builder_reset(&tPayload);
                return XRT_NET_ERROR;
            }
            bSawData = true;
        }
    }

    if ( !bSawData || !tPayload.pData ) {
        xllm__json_builder_reset(&tPayload);
        return XRT_NET_OK;
    }

    iStatus = xllm__anthropic_stream_process_payload(pCtx, tPayload.pData, tPayload.iLen, sRequestId);
    if ( iStatus == XRT_NET_OK ) {
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "event_block", tPayload.iLen);
    }
    xllm__json_builder_reset(&tPayload);
    return iStatus;
}

static int xllm__anthropic_stream_process_buffer(
    xllm__openai_stream_context *pCtx,
    const char *sBuffer,
    size_t iLen,
    const char *sRequestId
)
{
    size_t iCursor;

    if ( !pCtx || !sBuffer ) {
        return XRT_NET_ERROR;
    }

    if ( iLen <= pCtx->iParsedBytes ) {
        return XRT_NET_OK;
    }

    iCursor = pCtx->iParsedBytes;
    while ( iCursor < iLen ) {
        size_t i;
        size_t iEventEnd = (size_t)-1;
        size_t iDelimiterLen = 0u;

        for ( i = iCursor; i + 1u < iLen; ++i ) {
            if ( sBuffer[i] == '\n' && sBuffer[i + 1u] == '\n' ) {
                iEventEnd = i;
                iDelimiterLen = 2u;
                break;
            }
            if ( i + 3u < iLen &&
                 sBuffer[i] == '\r' &&
                 sBuffer[i + 1u] == '\n' &&
                 sBuffer[i + 2u] == '\r' &&
                 sBuffer[i + 3u] == '\n' ) {
                iEventEnd = i;
                iDelimiterLen = 4u;
                break;
            }
        }

        if ( iEventEnd == (size_t)-1 ) {
            break;
        }

        if ( xllm__anthropic_stream_process_event_block(pCtx, sBuffer + iCursor, iEventEnd - iCursor, sRequestId) != XRT_NET_OK ) {
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }
        iCursor = iEventEnd + iDelimiterLen;
        pCtx->iParsedBytes = iCursor;
    }

    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_OK;
}

static int xllm__anthropic_stream_finalize_response(xllm__openai_stream_context *pCtx)
{
    int iStatus = xllm__openai_stream_finalize_response(pCtx);

    if ( iStatus != XRT_NET_OK || !pCtx || !pCtx->pResponse ) {
        return iStatus;
    }

    if ( pCtx->pResponse->sFinishReason ) {
        if ( strcmp(pCtx->pResponse->sFinishReason, "tool_use") == 0 ) {
            pCtx->pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
        } else if ( strcmp(pCtx->pResponse->sFinishReason, "max_tokens") == 0 ||
                    strcmp(pCtx->pResponse->sFinishReason, "model_context_window_exceeded") == 0 ||
                    strcmp(pCtx->pResponse->sFinishReason, "pause_turn") == 0 ) {
            pCtx->pResponse->eStatus = XLLM_STATUS_INCOMPLETE;
        } else if ( strcmp(pCtx->pResponse->sFinishReason, "refusal") == 0 ) {
            xllm_output_item *pRefusalOutput = NULL;
            char *sRefusalText = NULL;

            pCtx->pResponse->eStatus = XLLM_STATUS_REFUSED;
            if ( (!pCtx->pResponse->tRefusal.sText || !pCtx->pResponse->tRefusal.sText[0]) &&
                 pCtx->pResponse->sVisibleText && pCtx->pResponse->sVisibleText[0] ) {
                pCtx->pResponse->tRefusal.sText = xllm__dup_cstr(pCtx->pResponse->sVisibleText);
            }

            if ( pCtx->pResponse->tRefusal.sText && pCtx->pResponse->tRefusal.sText[0] ) {
                sRefusalText = xllm__dup_cstr(pCtx->pResponse->tRefusal.sText);
            } else if ( pCtx->pResponse->sVisibleText && pCtx->pResponse->sVisibleText[0] ) {
                sRefusalText = xllm__dup_cstr(pCtx->pResponse->sVisibleText);
            }

            if ( sRefusalText && sRefusalText[0] && pCtx->iRefusalOutputIndex == (size_t)-1 ) {
                if ( pCtx->iMessageOutputIndex != (size_t)-1 &&
                     pCtx->iMessageOutputIndex < pCtx->pResponse->iOutputCount ) {
                    pRefusalOutput = &pCtx->pResponse->pOutputs[pCtx->iMessageOutputIndex];
                    if ( pRefusalOutput->eKind == XLLM_OUTPUT_MESSAGE ) {
                        size_t i;

                        if ( pRefusalOutput->as.tMessage.pParts ) {
                            for ( i = 0; i < pRefusalOutput->as.tMessage.iPartCount; ++i ) {
                                xllm__content_part_free(&pRefusalOutput->as.tMessage.pParts[i]);
                            }
                            xrtFree(pRefusalOutput->as.tMessage.pParts);
                        }
                        memset(pRefusalOutput, 0, sizeof(*pRefusalOutput));
                        pRefusalOutput->eKind = XLLM_OUTPUT_REFUSAL;
                        pRefusalOutput->as.tRefusal.sText = xllm__dup_cstr(sRefusalText);
                        if ( !pRefusalOutput->as.tRefusal.sText ) {
                            xllm__free_cstr(&sRefusalText);
                            return XRT_NET_ERROR;
                        }
                        pCtx->iRefusalOutputIndex = pCtx->iMessageOutputIndex;
                        pCtx->iMessageOutputIndex = (size_t)-1;
                    }
                }

                if ( pCtx->iRefusalOutputIndex == (size_t)-1 ) {
                    size_t iNewIndex = (size_t)-1;

                    if ( xllm__openai_stream_append_output(pCtx, XLLM_OUTPUT_REFUSAL, &iNewIndex) != XRT_NET_OK ) {
                        xllm__free_cstr(&sRefusalText);
                        return XRT_NET_ERROR;
                    }
                    pRefusalOutput = &pCtx->pResponse->pOutputs[iNewIndex];
                    pRefusalOutput->as.tRefusal.sText = xllm__dup_cstr(sRefusalText);
                    if ( !pRefusalOutput->as.tRefusal.sText ) {
                        xllm__free_cstr(&sRefusalText);
                        return XRT_NET_ERROR;
                    }
                    pCtx->iRefusalOutputIndex = iNewIndex;
                }
            }

            xllm__free_cstr(&sRefusalText);
        }
    }

    return XRT_NET_OK;
}

static int32 xllm__anthropic_native_chat_stream_buffered(
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
    xnet_result iNetStatus = XRT_NET_ERROR;
    bool bMultimodal = false;
    bool bTreatAsSse = false;
    bool bParsedSse = false;
    bool bRetryable = false;
    int iStatus = XRT_NET_ERROR;
    xvalue tRoot = NULL;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts = 1u;

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

    sModel = xllm__openai_select_model(pProfile, pRequest, &bMultimodal);
    tStream.sSelectedModel = sModel;
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for anthropic-native request");
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_fill_effective_params(&tEffectiveParams, pProfile, pRequest, XLLM_STREAM_PREFER) != XRT_NET_OK ) {
        goto fail;
    }
    if ( xllm__anthropic_build_chat_body(&tBody, pProfile, pRequest, &tEffectiveParams, pOptions, sModel, true, pError) != XRT_NET_OK ) {
        goto fail;
    }

    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) goto fail;

    sUrl = xllm__anthropic_build_url(pProfile->sBaseUrl);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native profile missing base url");
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) goto fail;
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) goto fail;
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) goto fail;
    if ( xllm__anthropic_fill_request_headers(&tHttpRequest, pProfile) != XRT_NET_OK ) goto fail;
    if ( !xrtHttpRequestSetHeader(&tHttpRequest, "Accept", "text/event-stream") ) goto fail;
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) goto fail;

retry_execute:
    ++uAttempt;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__anthropic_component_name(),
        "request start: model=%s streaming=true live=false attempt=%u/%u body_bytes=%u",
        sModel,
        (unsigned)uAttempt,
        (unsigned)uMaxAttempts,
        (unsigned)strlen(sBody)
    );
    xllm__anthropic_trace_request(pRuntime, pProfile, pRequest, sModel, true, false, uAttempt, strlen(sBody));

    pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
    if ( !pHttpResponse ) {
        if ( iNetStatus == XRT_NET_TIMEOUT ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "anthropic-native request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "anthropic-native request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "request-id");
    if ( !sRequestId ) {
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__anthropic_fill_error_from_http(pError, pHttpResponse, tRoot, sRequestId);
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "anthropic-native streaming response body is empty");
        goto fail;
    }

    sContentType = xrtHttpResponseHeader(pHttpResponse, "content-type");
    bTreatAsSse = xllm__buffer_starts_with_sse_data(pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !bTreatAsSse && sContentType ) {
        bTreatAsSse = xllm__text_contains_ci(sContentType, "text/event-stream");
    }

    if ( bTreatAsSse ) {
        if ( xllm__anthropic_stream_process_buffer(&tStream, pHttpResponse->pBody, pHttpResponse->iBodyLen, sRequestId) != XRT_NET_OK ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }
        if ( tStream.iParsedBytes < pHttpResponse->iBodyLen ) {
            size_t iRemain = pHttpResponse->iBodyLen - tStream.iParsedBytes;
            if ( xllm__anthropic_stream_process_event_block(&tStream, pHttpResponse->pBody + tStream.iParsedBytes, iRemain, sRequestId) != XRT_NET_OK ) {
                if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native stream cancelled");
                }
                iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                goto fail;
            }
            tStream.iParsedBytes = pHttpResponse->iBodyLen;
        }
        bParsedSse = tStream.iParsedBytes > 0u || tStream.bDone || tStream.pResponse != NULL;
    }

    if ( bParsedSse ) {
        if ( xllm__anthropic_stream_finalize_response(&tStream) != XRT_NET_OK ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }

        pResponse = tStream.pResponse;
        tStream.pResponse = NULL;
        if ( !pResponse ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "anthropic-native stream did not produce a response");
            goto fail;
        }

        *ppResponse = pResponse;
        pResponse = NULL;
        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_INFO,
            xllm__anthropic_component_name(),
            "response complete: model=%s streaming=true live=false attempt=%u status=%s outputs=%u",
            (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
            (unsigned)uAttempt,
            xllm__openai_response_status_name((*ppResponse)->eStatus),
            (unsigned)(*ppResponse)->iOutputCount
        );
        xllm__anthropic_trace_response(pRuntime, *ppResponse, (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel, pHttpResponse, sRequestId, NULL, XRT_NET_OK, uAttempt, false, true, false);
        iStatus = XRT_NET_OK;
        goto fail;
    }

    if ( pOptions && pOptions->eStreamMode == XLLM_STREAM_REQUIRE ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "anthropic-native upstream did not return an SSE stream");
        goto fail;
    }

    tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse anthropic-native streaming fallback response");
        goto fail;
    }
    if ( xllm__anthropic_build_response(pProfile, pRequest, pOptions, tRoot, &pResponse, pError) != XRT_NET_OK ) {
        goto fail;
    }
    tRoot = NULL;
    if ( xllm__openai_emit_synthetic_events(pResponse, pOptions) != XRT_NET_OK ) {
        if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native stream cancelled");
        }
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__anthropic_component_name(),
        "response fallback complete: model=%s streaming=true live=false attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount
    );
    xllm__anthropic_trace_response(pRuntime, *ppResponse, (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel, pHttpResponse, sRequestId, NULL, XRT_NET_OK, uAttempt, false, true, false);
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
                xllm__anthropic_component_name(),
                "response failed: model=%s streaming=true live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
                (unsigned)uDelayMs
            );
            xllm__anthropic_trace_response(pRuntime, NULL, sModel, pHttpResponse, sRequestId, pError, iTraceTransportStatus, uAttempt, true, true, false);
            if ( tRoot ) {
                xvoUnref(tRoot);
                tRoot = NULL;
            }
            if ( pResponse ) {
                xllm_response_free(pResponse);
                pResponse = NULL;
            }
            if ( pHttpResponse ) {
                xrtHttpResponseDestroy(pHttpResponse);
                pHttpResponse = NULL;
            }
            sRequestId = NULL;
            sContentType = NULL;
            bTreatAsSse = false;
            bParsedSse = false;
            iNetStatus = XRT_NET_ERROR;
            iStatus = XRT_NET_ERROR;
            xllm__openai_stream_reset_attempt_state(&tStream);
            if ( pError ) {
                xllm_error_reset(pError);
            }
            if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
                if ( pError ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native request cancelled");
                }
                iStatus = XRT_NET_CANCELLED;
            } else {
                xllm__openai_retry_sleep(uDelayMs);
                goto retry_execute;
            }
        }
        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_WARN,
            xllm__anthropic_component_name(),
            "response failed: model=%s streaming=true live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)uAttempt,
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
            bRetryable ? "true" : "false"
        );
        xllm__anthropic_trace_response(pRuntime, NULL, sModel, pHttpResponse, sRequestId, pError, iTraceTransportStatus, uAttempt, bRetryable, true, false);
    }
    if ( tRoot ) {
        xvoUnref(tRoot);
    }
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    if ( pHttpResponse ) {
        xrtHttpResponseDestroy(pHttpResponse);
    }
    if ( sBody ) {
        xrtFree(sBody);
    }
    if ( sUrl ) {
        xrtFree(sUrl);
    }
    xllm__openai_stream_reset_attempt_state(&tStream);
    xllm__effective_params_reset(&tEffectiveParams);
    xllm__json_builder_reset(&tBody);
    xrtHttpRequestUnit(&tHttpRequest);
    return iStatus;
}

static int32 xllm__anthropic_native_chat(
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
    xnet_result iNetStatus = XRT_NET_ERROR;
    bool bMultimodal = false;
    bool bRetryable = false;
    int iStatus = XRT_NET_ERROR;
    xvalue tRoot = NULL;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts = 1u;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    if ( pOptions && (pOptions->eStreamMode == XLLM_STREAM_PREFER || pOptions->eStreamMode == XLLM_STREAM_REQUIRE) ) {
        return xllm__anthropic_native_chat_stream_buffered(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
    }

    *ppResponse = NULL;
    memset(&tBody, 0, sizeof(tBody));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    xrtHttpRequestInit(&tHttpRequest);

    if ( pOptions && pOptions->uMaxRetries > 0u ) {
        uMaxAttempts += pOptions->uMaxRetries;
    }

    sModel = xllm__openai_select_model(pProfile, pRequest, &bMultimodal);
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for anthropic-native request");
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_fill_effective_params(&tEffectiveParams, pProfile, pRequest, XLLM_STREAM_OFF) != XRT_NET_OK ) {
        goto fail;
    }
    if ( xllm__anthropic_build_chat_body(&tBody, pProfile, pRequest, &tEffectiveParams, pOptions, sModel, false, pError) != XRT_NET_OK ) {
        goto fail;
    }

    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) goto fail;

    sUrl = xllm__anthropic_build_url(pProfile->sBaseUrl);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "anthropic-native profile missing base url");
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) goto fail;
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) goto fail;
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) goto fail;
    if ( xllm__anthropic_fill_request_headers(&tHttpRequest, pProfile) != XRT_NET_OK ) goto fail;
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) goto fail;

retry_execute:
    ++uAttempt;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__anthropic_component_name(),
        "request start: model=%s streaming=false live=false attempt=%u/%u body_bytes=%u",
        sModel,
        (unsigned)uAttempt,
        (unsigned)uMaxAttempts,
        (unsigned)strlen(sBody)
    );
    xllm__anthropic_trace_request(pRuntime, pProfile, pRequest, sModel, false, false, uAttempt, strlen(sBody));

    pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
    if ( !pHttpResponse ) {
        if ( iNetStatus == XRT_NET_TIMEOUT ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "anthropic-native request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "anthropic-native request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "request-id");
    if ( !sRequestId ) {
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__anthropic_fill_error_from_http(pError, pHttpResponse, tRoot, sRequestId);
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "anthropic-native response body is empty");
        goto fail;
    }

    tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse anthropic-native response json");
        goto fail;
    }
    if ( xllm__anthropic_build_response(pProfile, pRequest, pOptions, tRoot, &pResponse, pError) != XRT_NET_OK ) {
        goto fail;
    }
    tRoot = NULL;

    if ( xllm__openai_emit_synthetic_events(pResponse, pOptions) != XRT_NET_OK ) {
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__anthropic_component_name(),
        "response complete: model=%s streaming=false live=false attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount
    );
    xllm__anthropic_trace_response(
        pRuntime,
        *ppResponse,
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        pHttpResponse,
        sRequestId,
        NULL,
        XRT_NET_OK,
        uAttempt,
        false,
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
                xllm__anthropic_component_name(),
                "response failed: model=%s streaming=false live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
                (unsigned)uDelayMs
            );
            xllm__anthropic_trace_response(pRuntime, NULL, sModel, pHttpResponse, sRequestId, pError, iTraceTransportStatus, uAttempt, true, false, false);
            if ( tRoot ) {
                xvoUnref(tRoot);
                tRoot = NULL;
            }
            if ( pResponse ) {
                xllm_response_free(pResponse);
                pResponse = NULL;
            }
            if ( pHttpResponse ) {
                xrtHttpResponseDestroy(pHttpResponse);
                pHttpResponse = NULL;
            }
            sRequestId = NULL;
            iNetStatus = XRT_NET_ERROR;
            iStatus = XRT_NET_ERROR;
            if ( pError ) {
                xllm_error_reset(pError);
            }
            if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
                if ( pError ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "anthropic-native request cancelled");
                }
                iStatus = XRT_NET_CANCELLED;
            } else {
                xllm__openai_retry_sleep(uDelayMs);
                goto retry_execute;
            }
        }
        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_WARN,
            xllm__anthropic_component_name(),
            "response failed: model=%s streaming=false live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)uAttempt,
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
            bRetryable ? "true" : "false"
        );
        xllm__anthropic_trace_response(pRuntime, NULL, sModel, pHttpResponse, sRequestId, pError, iTraceTransportStatus, uAttempt, bRetryable, false, false);
    }
    if ( tRoot ) {
        xvoUnref(tRoot);
    }
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    if ( pHttpResponse ) {
        xrtHttpResponseDestroy(pHttpResponse);
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

XLLM_API int xllm_register_anthropic_native_adapter(xllm_runtime *pRuntime)
{
    xllm_adapter tAdapter;

    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    memset(&tAdapter, 0, sizeof(tAdapter));
    tAdapter.sName = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tAdapter.pCtx = pRuntime;
    tAdapter.pfnChat = xllm__anthropic_native_chat;
    return xllm_register_adapter(pRuntime, &tAdapter);
}

