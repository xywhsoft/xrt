static const char *xllm__ollama_component_name(void)
{
    return "xllm.ollama_native";
}

static void xllm__ollama_trace_request(
    xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const char *sModel,
    bool bStreaming,
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
    xllm__openai_trace_table_set_text(tPayload, "adapter", XLLM_ADAPTER_OLLAMA_NATIVE);
    xllm__openai_trace_table_set_text(tPayload, "provider", pProfile && pProfile->sProvider ? pProfile->sProvider : "ollama");
    xllm__openai_trace_table_set_text(tPayload, "profile_id", pProfile ? pProfile->sId : NULL);
    xllm__openai_trace_table_set_text(tPayload, "model", sModel);
    xllm__openai_trace_table_set_bool(tPayload, "streaming", bStreaming);
    xllm__openai_trace_table_set_bool(tPayload, "live", false);
    xllm__openai_trace_table_set_u32(tPayload, "attempt", uAttempt);
    xllm__openai_trace_table_set_u32(tPayload, "body_bytes", (uint32)iBodyBytes);
    xllm__openai_trace_table_set_u32(tPayload, "message_count", (uint32)(pRequest ? pRequest->iMessageCount : 0u));
    xllm__openai_trace_emit(pRuntime, XLLM_TRACE_REQUEST, tPayload);
}

static void xllm__ollama_trace_response(
    xllm_runtime *pRuntime,
    const xllm_response *pResponse,
    const char *sModel,
    const xhttpresponse *pHttpResponse,
    const xllm_error *pError,
    int32 iTransportStatus,
    uint32 uAttempt,
    bool bRetryable,
    bool bStreaming
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
    xllm__openai_trace_table_set_text(tPayload, "adapter", XLLM_ADAPTER_OLLAMA_NATIVE);
    xllm__openai_trace_table_set_text(
        tPayload,
        "provider",
        pResponse && pResponse->sProvider ? pResponse->sProvider : "ollama"
    );
    xllm__openai_trace_table_set_text(
        tPayload,
        "profile_id",
        pResponse ? pResponse->sProfileId : NULL
    );
    xllm__openai_trace_table_set_text(
        tPayload,
        "model",
        pResponse && pResponse->sModel ? pResponse->sModel : sModel
    );
    xllm__openai_trace_table_set_bool(tPayload, "streaming", bStreaming);
    xllm__openai_trace_table_set_bool(tPayload, "live", false);
    xllm__openai_trace_table_set_bool(tPayload, "success", pResponse != NULL);
    xllm__openai_trace_table_set_u32(tPayload, "attempt", uAttempt);
    xllm__openai_trace_table_set_i32(tPayload, "transport_status", iTransportStatus);
    xllm__openai_trace_table_set_bool(tPayload, "retryable", bRetryable);
    if ( pHttpResponse ) {
        xllm__openai_trace_table_set_u32(tPayload, "http_status", pHttpResponse->iStatusCode);
    }
    if ( pResponse ) {
        xllm__openai_trace_table_set_text(
            tPayload,
            "response_status",
            xllm__openai_response_status_name(pResponse->eStatus)
        );
        xllm__openai_trace_table_set_text(tPayload, "finish_reason", pResponse->sFinishReason);
        xllm__openai_trace_table_set_u32(tPayload, "output_count", (uint32)pResponse->iOutputCount);
    } else if ( pError ) {
        xllm__openai_trace_table_set_text(
            tPayload,
            "error_code",
            xllm__openai_error_code_name(pError->eCode)
        );
        xllm__openai_trace_table_set_text(tPayload, "error_message", pError->sMessage);
        xllm__openai_trace_table_set_text(tPayload, "request_id", pError->sRequestId);
    }

    xllm__openai_trace_emit(pRuntime, XLLM_TRACE_RESPONSE, tPayload);
}

static void xllm__ollama_trace_stream(
    xllm_runtime *pRuntime,
    const xllm__openai_stream_context *pCtx,
    const char *sPhase,
    size_t iPayloadBytes
)
{
    xvalue tPayload;

    if ( !pRuntime || !pRuntime->tOptions.pfnTrace || !pCtx || !sPhase ) {
        return;
    }

    tPayload = xvoCreateTable();
    if ( !tPayload ) {
        return;
    }

    xllm__openai_trace_table_set_text(tPayload, "phase", sPhase);
    xllm__openai_trace_table_set_text(tPayload, "adapter", XLLM_ADAPTER_OLLAMA_NATIVE);
    xllm__openai_trace_table_set_bool(tPayload, "streaming", true);
    xllm__openai_trace_table_set_u32(tPayload, "payload_bytes", (uint32)iPayloadBytes);
    xllm__openai_trace_table_set_u32(tPayload, "payload_count", pCtx->uPayloadCount);
    xllm__openai_trace_table_set_u32(tPayload, "text_delta_count", pCtx->uTextDeltaCount);
    xllm__openai_trace_table_set_u32(tPayload, "thinking_delta_count", pCtx->uThinkingDeltaCount);
    xllm__openai_trace_table_set_u32(tPayload, "tool_delta_count", pCtx->uToolDeltaCount);
    xllm__openai_trace_table_set_u32(tPayload, "usage_count", pCtx->uUsageCount);
    xllm__openai_trace_table_set_u32(tPayload, "refusal_count", pCtx->uRefusalCount);
    xllm__openai_trace_table_set_bool(tPayload, "done", pCtx->bDone);
    xllm__openai_trace_table_set_bool(tPayload, "cancelled", pCtx->bCancelled);
    if ( pCtx->sSelectedModel ) {
        xllm__openai_trace_table_set_text(tPayload, "model", pCtx->sSelectedModel);
    }
    if ( pCtx->pResponse && pCtx->pResponse->sFinishReason ) {
        xllm__openai_trace_table_set_text(tPayload, "finish_reason", pCtx->pResponse->sFinishReason);
    }
    xllm__openai_trace_emit(pRuntime, XLLM_TRACE_STREAM, tPayload);
}

static char *xllm__ollama_build_url(const char *sBaseUrl)
{
    static const char sPath[] = "api/chat";
    size_t iLen;
    bool bNeedsSlash;
    char *sUrl;

    if ( !sBaseUrl || !sBaseUrl[0] ) {
        return NULL;
    }

    if ( strstr(sBaseUrl, "/api/chat") != NULL ) {
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

static int xllm__ollama_append_options(
    xllm__json_builder *pBody,
    const xllm_effective_params *pEffectiveParams
)
{
    bool bHasOption = false;
    size_t i;

    if ( !pBody || !pEffectiveParams ) {
        return XRT_NET_ERROR;
    }

    if ( !pEffectiveParams->tGeneration.tTemperature.bSet &&
         !pEffectiveParams->tGeneration.tTopP.bSet &&
         !pEffectiveParams->tGeneration.tMaxOutputTokens.bSet &&
         !pEffectiveParams->tGeneration.tSeed.bSet &&
         pEffectiveParams->tGeneration.iStopCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( !xllm__json_builder_append_cstr(pBody, ",\"options\":{") ) {
        return XRT_NET_ERROR;
    }

    if ( pEffectiveParams->tGeneration.tTemperature.bSet ) {
        if ( bHasOption && !xllm__json_builder_append_char(pBody, ',') ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBody, "\"temperature\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTemperature.fValue) ) return XRT_NET_ERROR;
        bHasOption = true;
    }
    if ( pEffectiveParams->tGeneration.tTopP.bSet ) {
        if ( bHasOption && !xllm__json_builder_append_char(pBody, ',') ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBody, "\"top_p\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTopP.fValue) ) return XRT_NET_ERROR;
        bHasOption = true;
    }
    if ( pEffectiveParams->tGeneration.tMaxOutputTokens.bSet ) {
        if ( bHasOption && !xllm__json_builder_append_char(pBody, ',') ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBody, "\"num_predict\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_u32(pBody, pEffectiveParams->tGeneration.tMaxOutputTokens.iValue) ) return XRT_NET_ERROR;
        bHasOption = true;
    }
    if ( pEffectiveParams->tGeneration.tSeed.bSet ) {
        if ( bHasOption && !xllm__json_builder_append_char(pBody, ',') ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBody, "\"seed\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_u32(pBody, pEffectiveParams->tGeneration.tSeed.iValue) ) return XRT_NET_ERROR;
        bHasOption = true;
    }
    if ( pEffectiveParams->tGeneration.iStopCount > 0u && pEffectiveParams->tGeneration.psStop ) {
        if ( bHasOption && !xllm__json_builder_append_char(pBody, ',') ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBody, "\"stop\":[") ) return XRT_NET_ERROR;
        for ( i = 0; i < pEffectiveParams->tGeneration.iStopCount; ++i ) {
            if ( i > 0u && !xllm__json_builder_append_char(pBody, ',') ) return XRT_NET_ERROR;
            if ( !xllm__json_builder_append_escaped(
                    pBody,
                    pEffectiveParams->tGeneration.psStop[i] ? pEffectiveParams->tGeneration.psStop[i] : ""
                 ) ) {
                return XRT_NET_ERROR;
            }
        }
        if ( !xllm__json_builder_append_char(pBody, ']') ) return XRT_NET_ERROR;
    }

    return xllm__json_builder_append_char(pBody, '}') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__ollama_append_message_tool_calls(
    xllm__json_builder *pBuilder,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    size_t i;

    if ( !pBuilder || !pMessage || pMessage->iToolCallCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_calls\":[") ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
        const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
        const char *sToolName = pCall->sToolName ? pCall->sToolName : pCall->sToolId;
        xvalue tArguments = NULL;
        char *sNormalized = NULL;

        if ( !sToolName || !sToolName[0] ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native assistant tool call missing tool name");
            return XRT_NET_ERROR;
        }
        if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }

        if ( pCall->sArgumentsJson && pCall->sArgumentsJson[0] ) {
            tArguments = xllm__parse_json_range(pCall->sArgumentsJson, strlen(pCall->sArgumentsJson), &sNormalized);
            if ( !tArguments ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native assistant tool call arguments_json is not valid json");
                return XRT_NET_ERROR;
            }
        }

        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"function\":{\"name\":") ) {
            if ( tArguments ) {
                xvoUnref(tArguments);
            }
            xrtFree(sNormalized);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_escaped(pBuilder, sToolName) ) {
            if ( tArguments ) {
                xvoUnref(tArguments);
            }
            xrtFree(sNormalized);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"arguments\":") ) {
            if ( tArguments ) {
                xvoUnref(tArguments);
            }
            xrtFree(sNormalized);
            return XRT_NET_ERROR;
        }
        if ( sNormalized ) {
            if ( !xllm__json_builder_append_cstr(pBuilder, sNormalized) ) {
                if ( tArguments ) {
                    xvoUnref(tArguments);
                }
                xrtFree(sNormalized);
                return XRT_NET_ERROR;
            }
        } else if ( !xllm__json_builder_append_cstr(pBuilder, "{}") ) {
            if ( tArguments ) {
                xvoUnref(tArguments);
            }
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
            if ( tArguments ) {
                xvoUnref(tArguments);
            }
            xrtFree(sNormalized);
            return XRT_NET_ERROR;
        }

        if ( tArguments ) {
            xvoUnref(tArguments);
        }
        xrtFree(sNormalized);
    }

    return xllm__json_builder_append_char(pBuilder, ']') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__ollama_append_image_input(
    xllm__json_builder *pBuilder,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_call_options *pOptions,
    const xllm_content_part *pPart,
    xllm_error *pError
)
{
    char *sBase64 = NULL;
    xhttprequest tHttpRequest;
    xhttpresponse *pHttpResponse = NULL;
    xnet_result iNetStatus = XRT_NET_ERROR;

    memset(&tHttpRequest, 0, sizeof(tHttpRequest));

    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_INLINE_BYTES:
            break;
        case XLLM_SOURCE_URL:
            if ( !pPart->as.tSource.as.sUrl || !pPart->as.tSource.as.sUrl[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native image url input is empty");
                return XRT_NET_ERROR;
            }
            xrtHttpRequestInit(&tHttpRequest);
            if ( !xrtHttpRequestSetURL(&tHttpRequest, pPart->as.tSource.as.sUrl) ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to set ollama-native image url request");
                xrtHttpRequestUnit(&tHttpRequest);
                return XRT_NET_ERROR;
            }
            if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) {
                xrtHttpRequestUnit(&tHttpRequest);
                return XRT_NET_ERROR;
            }
            pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
            xrtHttpRequestUnit(&tHttpRequest);
            if ( !pHttpResponse ) {
                if ( iNetStatus == XRT_NET_TIMEOUT ) {
                    xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "ollama-native image url download timed out");
                } else {
                    xllm__error_set(pError, XLLM_ERROR_NETWORK, "ollama-native image url download failed");
                }
                return XRT_NET_ERROR;
            }
            if ( pHttpResponse->iStatusCode >= 400u ) {
                if ( pHttpResponse->iStatusCode >= 500u ) {
                    xllm__error_set(pError, XLLM_ERROR_UPSTREAM_5XX, "ollama-native image url download returned 5xx");
                } else {
                    xllm__error_set(pError, XLLM_ERROR_UPSTREAM_4XX, "ollama-native image url download returned 4xx");
                }
                pError->iHttpStatus = (int)pHttpResponse->iStatusCode;
                xrtHttpResponseDestroy(pHttpResponse);
                return XRT_NET_ERROR;
            }
            if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
                xllm__error_set(pError, XLLM_ERROR_PARSE, "ollama-native image url download returned empty body");
                xrtHttpResponseDestroy(pHttpResponse);
                return XRT_NET_ERROR;
            }
            sBase64 = (char *)xrtBase64Encode((ptr)pHttpResponse->pBody, pHttpResponse->iBodyLen, NULL);
            xrtHttpResponseDestroy(pHttpResponse);
            pHttpResponse = NULL;
            if ( !sBase64 ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode downloaded ollama-native image bytes");
                return XRT_NET_ERROR;
            }
            break;
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            xllm__error_set(
                pError,
                XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                "ollama-native image provider file_id input is not supported by the REST chat API; use inline bytes"
            );
            return XRT_NET_ERROR;
        case XLLM_SOURCE_INLINE_TEXT:
        default:
            xllm__error_set(
                pError,
                XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                "ollama-native image input only supports inline bytes"
            );
            return XRT_NET_ERROR;
    }

    if ( !sBase64 ) {
        if ( !pPart->as.tSource.as.tBytes.pData || pPart->as.tSource.as.tBytes.iSize == 0u ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native image bytes input is empty");
            return XRT_NET_ERROR;
        }
        sBase64 = (char *)xrtBase64Encode(
            (ptr)pPart->as.tSource.as.tBytes.pData,
            pPart->as.tSource.as.tBytes.iSize,
            NULL
        );
        if ( !sBase64 ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode ollama-native image bytes");
            return XRT_NET_ERROR;
        }
    }

    if ( !xllm__json_builder_append_escaped(pBuilder, sBase64) ) {
        xrtFree(sBase64);
        return XRT_NET_ERROR;
    }

    xrtFree(sBase64);
    return XRT_NET_OK;
}

static int xllm__ollama_append_message(
    xllm__json_builder *pBuilder,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_call_options *pOptions,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    const char *sRole;
    xllm__json_builder tContent;
    xllm__json_builder tImages;
    bool bHasContent = false;
    bool bHasImages = false;
    size_t i;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    memset(&tContent, 0, sizeof(tContent));
    memset(&tImages, 0, sizeof(tImages));
    sRole = xllm__openai_role_name(pMessage->eRole);

    if ( !xllm__json_builder_append_cstr(pBuilder, "{\"role\":") ) goto fail;
    if ( !xllm__json_builder_append_escaped(pBuilder, sRole) ) goto fail;

    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        if ( !pMessage->sToolName || !pMessage->sToolName[0] ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native tool result message missing tool_name");
            goto fail;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_name\":") ) goto fail;
        if ( !xllm__json_builder_append_escaped(pBuilder, pMessage->sToolName) ) goto fail;
    }

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u ) {
        if ( xllm__ollama_append_message_tool_calls(pBuilder, pMessage, pError) != XRT_NET_OK ) {
            goto fail;
        }
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        const xllm_content_part *pPart = &pMessage->pParts[i];

        switch ( pPart->eKind ) {
            case XLLM_PART_TEXT:
                if ( pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                    xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "ollama-native adapter currently only supports inline text content");
                    goto fail;
                }
                if ( bHasContent && !xllm__json_builder_append_char(&tContent, '\n') ) goto fail;
                if ( !xllm__json_builder_append_cstr(&tContent, pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "") ) goto fail;
                bHasContent = true;
                break;
            case XLLM_PART_JSON: {
                char *sJson = (char *)xrtStringifyJSON(pPart->as.tJsonValue, 0, NULL);
                if ( !sJson ) {
                    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify ollama-native json part");
                    goto fail;
                }
                if ( bHasContent && !xllm__json_builder_append_char(&tContent, '\n') ) {
                    xrtFree(sJson);
                    goto fail;
                }
                if ( !xllm__json_builder_append_cstr(&tContent, sJson) ) {
                    xrtFree(sJson);
                    goto fail;
                }
                xrtFree(sJson);
                bHasContent = true;
                break;
            }
            case XLLM_PART_IMAGE:
                if ( pMessage->eRole != XLLM_ROLE_USER &&
                     pMessage->eRole != XLLM_ROLE_ASSISTANT ) {
                    xllm__error_set(
                        pError,
                        XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                        "ollama-native multimodal input currently only supports user or assistant image messages"
                    );
                    goto fail;
                }
                if ( !bHasImages ) {
                    if ( !xllm__json_builder_append_char(&tImages, '[') ) goto fail;
                } else if ( !xllm__json_builder_append_char(&tImages, ',') ) {
                    goto fail;
                }
                if ( xllm__ollama_append_image_input(&tImages, pRuntime, pProfile, pOptions, pPart, pError) != XRT_NET_OK ) goto fail;
                bHasImages = true;
                break;
            default:
                xllm__error_set(
                    pError,
                    XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                    "ollama-native multimodal input currently only supports text, json, and image parts"
                );
                goto fail;
        }
    }

    if ( bHasContent || bHasImages ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"content\":") ) goto fail;
        if ( bHasContent ) {
            if ( !xllm__json_builder_append_escaped(pBuilder, tContent.pData ? tContent.pData : "") ) goto fail;
        } else if ( !xllm__json_builder_append_escaped(pBuilder, "") ) {
            goto fail;
        }
    }

    if ( bHasImages ) {
        if ( !xllm__json_builder_append_char(&tImages, ']') ) goto fail;
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"images\":") ) goto fail;
        if ( !xllm__json_builder_append_cstr(pBuilder, tImages.pData ? tImages.pData : "[]") ) goto fail;
    }

    if ( !xllm__json_builder_append_char(pBuilder, '}') ) goto fail;

    xllm__json_builder_reset(&tContent);
    xllm__json_builder_reset(&tImages);
    return XRT_NET_OK;

fail:
    xllm__json_builder_reset(&tContent);
    xllm__json_builder_reset(&tImages);
    return XRT_NET_ERROR;
}

static int xllm__ollama_append_context_messages(
    xllm__json_builder *pBuilder,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_error *pError
)
{
    size_t i;
    bool bNeedComma = false;

    if ( !pBuilder || !pRequest ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, "\"messages\":[") ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            if ( bNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
                return XRT_NET_ERROR;
            }
            if ( xllm__ollama_append_message(
                     pBuilder,
                     pRuntime,
                     pProfile,
                     pOptions,
                     &pRequest->pContextBlocks[i].pMessages[j],
                     pError) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            bNeedComma = true;
        }
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        if ( bNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__ollama_append_message(pBuilder, pRuntime, pProfile, pOptions, &pRequest->pMessages[i], pError) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        bNeedComma = true;
    }

    return xllm__json_builder_append_char(pBuilder, ']') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__ollama_append_tools(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    xllm_error *pError
)
{
    size_t i;
    bool bHasTool = false;

    if ( !pBuilder || !pRequest || pRequest->iToolCount == 0u ) {
        return XRT_NET_OK;
    }

    if ( pRequest->tToolPolicy.eMode == XLLM_TOOL_CHOICE_NONE ) {
        return XRT_NET_OK;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tools\":[") ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pRequest->iToolCount; ++i ) {
        const xllm_tool_def *pTool = &pRequest->pTools[i];

        if ( bHasTool && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }

        if ( pTool->eKind == XLLM_TOOL_PROVIDER ) {
            char *sProviderToolJson = NULL;

            if ( !pTool->tVendorExtra || xvoType(pTool->tVendorExtra) != XVO_DT_TABLE ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    "ollama-native provider tool requires vendor_extra object"
                );
                return XRT_NET_ERROR;
            }

            sProviderToolJson = (char *)xrtStringifyJSON(pTool->tVendorExtra, 0, NULL);
            if ( !sProviderToolJson ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INTERNAL,
                    "failed to stringify ollama-native provider tool"
                );
                return XRT_NET_ERROR;
            }

            if ( !xllm__json_builder_append_cstr(pBuilder, sProviderToolJson) ) {
                xrtFree(sProviderToolJson);
                return XRT_NET_ERROR;
            }
            xrtFree(sProviderToolJson);
            bHasTool = true;
            continue;
        }

        {
            const char *sWireName = pTool->sWireName ? pTool->sWireName : pTool->sToolId;
            char *sSchema = NULL;

            if ( !sWireName || !sWireName[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native tool definition missing wire_name");
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

            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"function\",\"function\":{\"name\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sWireName) ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            if ( pTool->sDescription ) {
                if ( !xllm__json_builder_append_cstr(pBuilder, ",\"description\":") ||
                     !xllm__json_builder_append_escaped(pBuilder, pTool->sDescription) ) {
                    xrtFree(sSchema);
                    return XRT_NET_ERROR;
                }
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"parameters\":") ||
                 !xllm__json_builder_append_cstr(pBuilder, sSchema) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }

            xrtFree(sSchema);
            bHasTool = true;
        }
    }

    return xllm__json_builder_append_char(pBuilder, ']') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__ollama_append_tool_policy(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    xllm_error *pError
)
{
    if ( !pBuilder || !pRequest || pRequest->iToolCount == 0u ) {
        return XRT_NET_OK;
    }

    switch ( pRequest->tToolPolicy.eMode ) {
        case XLLM_TOOL_CHOICE_AUTO:
        case XLLM_TOOL_CHOICE_NONE:
            return XRT_NET_OK;
        case XLLM_TOOL_CHOICE_REQUIRED:
        case XLLM_TOOL_CHOICE_NAMED:
        default:
            xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "ollama-native adapter currently supports only tool_choice=auto or none");
            return XRT_NET_ERROR;
    }
}

static int xllm__ollama_append_response_format(
    xllm__json_builder *pBuilder,
    const xllm_response_format *pFormat,
    xllm_error *pError
)
{
    char *sSchema = NULL;

    if ( !pBuilder || !pFormat ) {
        return XRT_NET_OK;
    }

    switch ( pFormat->eKind ) {
        case XLLM_RESPONSE_JSON:
            return xllm__json_builder_append_cstr(pBuilder, ",\"format\":\"json\"") ? XRT_NET_OK : XRT_NET_ERROR;
        case XLLM_RESPONSE_JSON_SCHEMA:
            if ( pFormat->tJsonSchema && xvoType(pFormat->tJsonSchema) != XVO_DT_NULL ) {
                sSchema = (char *)xrtStringifyJSON(pFormat->tJsonSchema, 0, NULL);
            }
            if ( !sSchema ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native json schema response requires a schema");
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"format\":") ||
                 !xllm__json_builder_append_cstr(pBuilder, sSchema) ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            xrtFree(sSchema);
            return XRT_NET_OK;
        case XLLM_RESPONSE_TEXT:
        default:
            return XRT_NET_OK;
    }
}

static int xllm__ollama_append_reasoning(
    xllm__json_builder *pBuilder,
    const xllm_reasoning_options *pReasoning,
    xllm_error *pError
)
{
    bool bThink = false;

    (void)pError;

    if ( !pBuilder || !pReasoning ) {
        return XRT_NET_OK;
    }

    if ( pReasoning->tEnabled.bSet && !pReasoning->tEnabled.bValue ) {
        return xllm__json_builder_append_cstr(pBuilder, ",\"think\":false") ? XRT_NET_OK : XRT_NET_ERROR;
    }
    if ( pReasoning->eLevel == XLLM_REASONING_OFF ) {
        return xllm__json_builder_append_cstr(pBuilder, ",\"think\":false") ? XRT_NET_OK : XRT_NET_ERROR;
    }

    bThink =
        (pReasoning->tEnabled.bSet && pReasoning->tEnabled.bValue) ||
        pReasoning->eLevel != XLLM_REASONING_DEFAULT ||
        pReasoning->tBudgetTokens.bSet ||
        (pReasoning->tExposeThinking.bSet && pReasoning->tExposeThinking.bValue);

    if ( !bThink ) {
        return XRT_NET_OK;
    }

    return xllm__json_builder_append_cstr(pBuilder, ",\"think\":true") ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__ollama_build_chat_body(
    xllm__json_builder *pBody,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_effective_params *pEffectiveParams,
    const xllm_call_options *pOptions,
    const char *sModel,
    bool bStream,
    xllm_error *pError
)
{
    bool bSendNativeResponseFormat = false;

    if ( !pBody || !pRequest || !pEffectiveParams || !sModel ) {
        return XRT_NET_ERROR;
    }

    bSendNativeResponseFormat = xllm__openai_should_send_native_response_format(
        pProfile,
        pRequest,
        &pEffectiveParams->tResponseFormat,
        pOptions
    );

    if ( pEffectiveParams->tResponseFormat.eKind != XLLM_RESPONSE_TEXT &&
         !bSendNativeResponseFormat &&
         !(pOptions && pOptions->bBestEffortStructuredOutput) ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "ollama-native adapter currently supports structured output only in best-effort mode");
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBody, '{') ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_cstr(pBody, "\"model\":") ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_escaped(pBody, sModel) ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_cstr(pBody, bStream ? ",\"stream\":true," : ",\"stream\":false,") ) return XRT_NET_ERROR;
    if ( xllm__ollama_append_context_messages(pBody, pRuntime, pProfile, pRequest, pOptions, pError) != XRT_NET_OK ) return XRT_NET_ERROR;
    if ( xllm__ollama_append_tools(pBody, pRequest, pError) != XRT_NET_OK ) return XRT_NET_ERROR;
    if ( xllm__ollama_append_tool_policy(pBody, pRequest, pError) != XRT_NET_OK ) return XRT_NET_ERROR;
    if ( bSendNativeResponseFormat &&
         xllm__ollama_append_response_format(pBody, &pEffectiveParams->tResponseFormat, pError) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__ollama_append_reasoning(pBody, &pEffectiveParams->tReasoning, pError) != XRT_NET_OK ) return XRT_NET_ERROR;
    if ( xllm__ollama_append_options(pBody, pEffectiveParams) != XRT_NET_OK ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_char(pBody, '}') ) return XRT_NET_ERROR;
    return XRT_NET_OK;
}

static void xllm__ollama_fill_error_from_http(xllm_error *pError, const xhttpresponse *pHttpResponse, xvalue tRoot)
{
    const char *sMessage = NULL;
    xllm_error_code eCode = XLLM_ERROR_UPSTREAM_4XX;

    if ( !pError ) {
        return;
    }

    if ( tRoot && xvoType(tRoot) == XVO_DT_TABLE ) {
        sMessage = xllm__json_table_get_text(tRoot, "error");
    }
    if ( !sMessage || !sMessage[0] ) {
        sMessage = "ollama-native request failed";
    }

    if ( pHttpResponse ) {
        pError->iHttpStatus = (int32)pHttpResponse->iStatusCode;
        if ( pHttpResponse->iStatusCode == 400u ) {
            eCode = XLLM_ERROR_INVALID_REQUEST;
        } else if ( pHttpResponse->iStatusCode == 401u || pHttpResponse->iStatusCode == 403u ) {
            eCode = XLLM_ERROR_AUTH;
        } else if ( pHttpResponse->iStatusCode == 404u ) {
            eCode = XLLM_ERROR_MODEL_NOT_FOUND;
        } else if ( pHttpResponse->iStatusCode == 408u ) {
            eCode = XLLM_ERROR_TIMEOUT;
        } else if ( pHttpResponse->iStatusCode == 429u ) {
            eCode = XLLM_ERROR_RATE_LIMIT;
        } else if ( pHttpResponse->iStatusCode >= 500u ) {
            eCode = XLLM_ERROR_UPSTREAM_5XX;
        }
    }

    xllm__error_set(pError, eCode, sMessage);
    if ( sMessage && sMessage[0] ) {
        pError->sProviderMessage = xllm__dup_cstr(sMessage);
    }
}

static int xllm__ollama_apply_terminal_status(
    xllm_response *pResponse,
    bool bDone,
    bool bCancelled
)
{
    if ( !pResponse ) {
        return XRT_NET_OK;
    }

    if ( bCancelled ) {
        pResponse->eStatus = XLLM_STATUS_CANCELLED;
        return XRT_NET_OK;
    }

    if ( pResponse->tRefusal.sText && pResponse->tRefusal.sText[0] ) {
        pResponse->eStatus = XLLM_STATUS_REFUSED;
        return XRT_NET_OK;
    }

    if ( pResponse->sFinishReason ) {
        if ( strcmp(pResponse->sFinishReason, "tool_calls") == 0 ||
             strcmp(pResponse->sFinishReason, "tool_call") == 0 ||
             strcmp(pResponse->sFinishReason, "tool_use") == 0 ) {
            pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
            return XRT_NET_OK;
        }
        if ( strcmp(pResponse->sFinishReason, "length") == 0 ||
             strcmp(pResponse->sFinishReason, "max_tokens") == 0 ||
             strcmp(pResponse->sFinishReason, "model_context_window_exceeded") == 0 ||
             strcmp(pResponse->sFinishReason, "pause_turn") == 0 ) {
            pResponse->eStatus = XLLM_STATUS_INCOMPLETE;
            return XRT_NET_OK;
        }
        if ( strcmp(pResponse->sFinishReason, "content_filter") == 0 ) {
            pResponse->eStatus = XLLM_STATUS_CONTENT_FILTERED;
            if ( !pResponse->tSafety.sBlockReason || !pResponse->tSafety.sBlockReason[0] ) {
                pResponse->tSafety.sBlockReason = xllm__dup_cstr("content_filter");
                if ( !pResponse->tSafety.sBlockReason ) {
                    return XRT_NET_ERROR;
                }
            }
            return XRT_NET_OK;
        }
        if ( strcmp(pResponse->sFinishReason, "refusal") == 0 ) {
            pResponse->eStatus = XLLM_STATUS_REFUSED;
            return XRT_NET_OK;
        }
    }

    pResponse->eStatus = bDone ? XLLM_STATUS_COMPLETED : XLLM_STATUS_INCOMPLETE;
    return XRT_NET_OK;
}

static int xllm__ollama_build_image_part(
    const char *sBase64,
    size_t iImageIndex,
    xllm_content_part *pPart,
    xllm_error *pError
)
{
    char sName[64];
    size_t iDecodedSize;
    void *pDecoded;

    if ( !sBase64 || !sBase64[0] || !pPart ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "ollama-native image output is empty");
        return XRT_NET_ERROR;
    }

    memset(pPart, 0, sizeof(*pPart));
    iDecodedSize = xllm__openai_base64_decoded_size(sBase64);
    pDecoded = xrtBase64Decode((str)sBase64, strlen(sBase64), NULL);
    if ( !pDecoded ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to decode ollama-native image output");
        return XRT_NET_ERROR;
    }

    (void)snprintf(sName, sizeof(sName), "ollama-image-%u.bin", (unsigned)iImageIndex);
    pPart->eKind = XLLM_PART_IMAGE;
    pPart->as.tSource.eKind = XLLM_SOURCE_INLINE_BYTES;
    pPart->as.tSource.sMimeType = xllm__dup_cstr("image/*");
    pPart->as.tSource.sName = xllm__dup_cstr(sName);
    pPart->as.tSource.as.tBytes.pData = pDecoded;
    pPart->as.tSource.as.tBytes.iSize = iDecodedSize;
    if ( !pPart->as.tSource.sMimeType || !pPart->as.tSource.sName ) {
        xllm__content_part_free(pPart);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__ollama_append_output_images(
    xvalue tImages,
    xllm_content_part **ppParts,
    size_t *piPartCount,
    size_t *piPartCapacity,
    xllm_error *pError
)
{
    size_t i;

    if ( !tImages || xvoType(tImages) != XVO_DT_ARRAY ) {
        return XRT_NET_OK;
    }
    if ( !ppParts || !piPartCount || !piPartCapacity ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < (size_t)xvoArrayItemCount(tImages); ++i ) {
        xvalue tImage = xvoArrayGetValue(tImages, (uint32)i);
        const char *sBase64 = NULL;
        xllm_content_part tPart;

        if ( !tImage || xvoType(tImage) != XVO_DT_TEXT ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "ollama-native image output item is not a base64 string");
            return XRT_NET_ERROR;
        }

        sBase64 = (const char *)xvoGetText(tImage);
        if ( !sBase64 || !sBase64[0] ) {
            continue;
        }

        if ( xllm__ollama_build_image_part(sBase64, i, &tPart, pError) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__openai_message_add_part(ppParts, piPartCount, piPartCapacity, &tPart) != XRT_NET_OK ) {
            xllm__content_part_free(&tPart);
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

static int xllm__ollama_stream_append_images(
    xllm__openai_stream_context *pCtx,
    xvalue tImages
)
{
    size_t i;

    if ( !pCtx || !tImages || xvoType(tImages) != XVO_DT_ARRAY ) {
        return XRT_NET_OK;
    }

    for ( i = 0u; i < (size_t)xvoArrayItemCount(tImages); ++i ) {
        xvalue tImage = xvoArrayGetValue(tImages, (uint32)i);
        const char *sBase64 = NULL;
        xllm_content_part tPart;

        if ( !tImage || xvoType(tImage) != XVO_DT_TEXT ) {
            xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "ollama-native streamed image output item is not a base64 string");
            return XRT_NET_ERROR;
        }

        sBase64 = (const char *)xvoGetText(tImage);
        if ( !sBase64 || !sBase64[0] ) {
            continue;
        }

        if ( xllm__ollama_build_image_part(sBase64, i, &tPart, pCtx->pError) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__openai_stream_append_message_part(pCtx, &tPart) != XRT_NET_OK ) {
            xllm__content_part_free(&tPart);
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

static int xllm__ollama_build_response(
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
    xvalue tMessage;
    xvalue tImages;
    xvalue tToolCalls;
    xllm_content_part *pMessageParts = NULL;
    const char *sText;
    const char *sThinking;
    const char *sModel;
    const char *sDoneReason;
    char *sNormalizedJson = NULL;
    xvalue tJsonValue = NULL;
    bool bJsonOutput = false;
    bool bDone = true;
    size_t iMessagePartCount = 0u;
    size_t iMessagePartCapacity = 0u;
    size_t iImageCount = 0u;
    size_t iToolCallCount = 0u;
    size_t iOutputCount = 0u;

    if ( !pProfile || !pRequest || !ppResponse || !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "invalid ollama-native response");
        return XRT_NET_ERROR;
    }

    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    if ( xllm__openai_fill_effective_params(&tEffectiveParams, pProfile, pRequest, XLLM_STREAM_OFF) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    tMessage = xllm__json_table_get(tRoot, "message");
    if ( !tMessage || xvoType(tMessage) != XVO_DT_TABLE ) {
        xllm__effective_params_reset(&tEffectiveParams);
        xllm__error_set(pError, XLLM_ERROR_PARSE, "ollama-native response missing message");
        return XRT_NET_ERROR;
    }

    sText = xllm__json_table_get_text(tMessage, "content");
    sThinking = xllm__json_table_get_text(tMessage, "thinking");
    tImages = xllm__json_table_get(tMessage, "images");
    if ( tImages && xvoType(tImages) == XVO_DT_ARRAY ) {
        iImageCount = (size_t)xvoArrayItemCount(tImages);
    }
    tToolCalls = xllm__json_table_get(tMessage, "tool_calls");
    if ( tToolCalls && xvoType(tToolCalls) == XVO_DT_ARRAY ) {
        iToolCallCount = (size_t)xvoArrayItemCount(tToolCalls);
    }
    sDoneReason = xllm__json_table_get_text(tRoot, "done_reason");
    if ( iToolCallCount > 0u ) {
        sDoneReason = "tool_calls";
    } else if ( !sDoneReason || !sDoneReason[0] ) {
        sDoneReason = "stop";
    }
    (void)xllm__json_table_get_bool(tRoot, "done", &bDone);

    if ( tEffectiveParams.tResponseFormat.eKind != XLLM_RESPONSE_TEXT && sText && sText[0] ) {
        if ( xllm__openai_parse_structured_output(
                &tEffectiveParams.tResponseFormat,
                pOptions,
                sText,
                &tJsonValue,
                &sNormalizedJson,
                pError
             ) != XRT_NET_OK ) {
            xllm__effective_params_reset(&tEffectiveParams);
            return XRT_NET_ERROR;
        }
        bJsonOutput = (tJsonValue != NULL);
    }

    if ( sText && sText[0] ) {
        xllm_content_part tPart;

        memset(&tPart, 0, sizeof(tPart));
        if ( bJsonOutput && tJsonValue ) {
            tPart.eKind = XLLM_PART_JSON;
            tPart.as.tJsonValue = tJsonValue;
            tJsonValue = NULL;
        } else {
            tPart.eKind = XLLM_PART_TEXT;
            tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
            tPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
            tPart.as.tSource.as.sText = xllm__dup_cstr(sNormalizedJson ? sNormalizedJson : sText);
            if ( !tPart.as.tSource.sMimeType || !tPart.as.tSource.as.sText ) {
                xllm__content_part_free(&tPart);
                goto fail;
            }
        }

        if ( xllm__openai_message_add_part(&pMessageParts, &iMessagePartCount, &iMessagePartCapacity, &tPart) != XRT_NET_OK ) {
            xllm__content_part_free(&tPart);
            goto fail;
        }
    }

    if ( iImageCount > 0u &&
         xllm__ollama_append_output_images(tImages, &pMessageParts, &iMessagePartCount, &iMessagePartCapacity, pError) != XRT_NET_OK ) {
        goto fail;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        xllm__effective_params_reset(&tEffectiveParams);
        xllm__free_cstr(&sNormalizedJson);
        if ( tJsonValue ) {
            xvoUnref(tJsonValue);
        }
        return XRT_NET_ERROR;
    }

    pResponse->sProvider = xllm__dup_cstr(pProfile->sProvider ? pProfile->sProvider : "ollama");
    pResponse->sProfileId = xllm__dup_cstr(pProfile->sId);
    sModel = xllm__json_table_get_text(tRoot, "model");
    pResponse->sModel = xllm__dup_cstr(sModel ? sModel : xllm__openai_select_model(pProfile, pRequest, NULL));
    pResponse->sFinishReason = xllm__dup_cstr(sDoneReason);
    pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tRoot, "prompt_eval_count");
    pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tRoot, "eval_count");
    pResponse->tEffectiveParams = tEffectiveParams;
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));

    if ( sThinking && sThinking[0] ) {
        ++iOutputCount;
    }
    if ( iMessagePartCount > 0u ) {
        ++iOutputCount;
    }
    iOutputCount += iToolCallCount;

    if ( sText && sText[0] ) {
        pResponse->sVisibleText = xllm__dup_cstr(sNormalizedJson ? sNormalizedJson : sText);
    }

    if ( iOutputCount > 0u ) {
        size_t iOutputIndex = 0u;

        pResponse->pOutputs = (xllm_output_item *)xrtCalloc(iOutputCount, sizeof(xllm_output_item));
        if ( !pResponse->pOutputs ) {
            goto fail;
        }

        pResponse->iOutputCount = iOutputCount;
        if ( sThinking && sThinking[0] ) {
            pResponse->pOutputs[iOutputIndex].eKind = XLLM_OUTPUT_THINKING;
            pResponse->pOutputs[iOutputIndex].as.tThinking.bVisible = true;
            pResponse->pOutputs[iOutputIndex].as.tThinking.sFormat = xllm__dup_cstr("full");
            pResponse->pOutputs[iOutputIndex].as.tThinking.sText = xllm__dup_cstr(sThinking);
            if ( !pResponse->pOutputs[iOutputIndex].as.tThinking.sFormat ||
                 !pResponse->pOutputs[iOutputIndex].as.tThinking.sText ) {
                goto fail;
            }
            ++iOutputIndex;
        }

        if ( iMessagePartCount > 0u ) {
            pResponse->pOutputs[iOutputIndex].eKind = XLLM_OUTPUT_MESSAGE;
            pResponse->pOutputs[iOutputIndex].as.tMessage.pParts = pMessageParts;
            pResponse->pOutputs[iOutputIndex].as.tMessage.iPartCount = iMessagePartCount;
            pMessageParts = NULL;
            iMessagePartCount = 0u;
            iMessagePartCapacity = 0u;
            ++iOutputIndex;
        }

        if ( iToolCallCount > 0u ) {
            size_t i;
            for ( i = 0; i < iToolCallCount; ++i ) {
                xvalue tToolCall = xvoArrayGetValue(tToolCalls, (uint32)i);
                xvalue tFunction = xllm__json_table_get(tToolCall, "function");
                const char *sToolName = xllm__json_table_get_text(tFunction, "name");
                xvalue tArguments = xllm__json_table_get(tFunction, "arguments");
                char *sArguments = NULL;
                xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];

                if ( tArguments && xvoType(tArguments) != XVO_DT_NULL ) {
                    if ( xvoType(tArguments) == XVO_DT_TEXT ) {
                        sArguments = xllm__dup_cstr((const char *)xvoGetText(tArguments));
                    } else {
                        sArguments = (char *)xrtStringifyJSON(tArguments, 0, NULL);
                    }
                }
                if ( !sArguments ) {
                    sArguments = xllm__dup_cstr("{}");
                }
                if ( !sArguments ) {
                    goto fail;
                }

                pOutput->eKind = XLLM_OUTPUT_TOOL_CALL;
                pOutput->as.tToolCall.sCallId = xllm__dup_cstr(sToolName ? sToolName : "");
                pOutput->as.tToolCall.sToolId = xllm__dup_cstr(sToolName);
                pOutput->as.tToolCall.sToolName = xllm__dup_cstr(sToolName);
                pOutput->as.tToolCall.sArgumentsJson = sArguments;
            }
        }
    }

    if ( xllm__ollama_apply_terminal_status(pResponse, bDone, false) != XRT_NET_OK ) {
        goto fail;
    }

    xllm__free_cstr(&sNormalizedJson);
    if ( tJsonValue ) {
        xvoUnref(tJsonValue);
    }
    *ppResponse = pResponse;
    return XRT_NET_OK;

fail:
    if ( pMessageParts ) {
        size_t i;
        for ( i = 0u; i < iMessagePartCount; ++i ) {
            xllm__content_part_free(&pMessageParts[i]);
        }
        xrtFree(pMessageParts);
    }
    xllm__free_cstr(&sNormalizedJson);
    if ( tJsonValue ) {
        xvoUnref(tJsonValue);
    }
    xllm_response_free(pResponse);
    return XRT_NET_ERROR;
}

static int xllm__ollama_stream_apply_usage(xllm__openai_stream_context *pCtx, xvalue tRoot)
{
    xllm_event tEvent;
    uint32 uPrompt;
    uint32 uEval;

    if ( !pCtx || !pCtx->pResponse || !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        return XRT_NET_OK;
    }

    uPrompt = xllm__json_table_get_u32(tRoot, "prompt_eval_count");
    uEval = xllm__json_table_get_u32(tRoot, "eval_count");
    if ( uPrompt == 0u && uEval == 0u ) {
        return XRT_NET_OK;
    }

    pCtx->pResponse->tUsage.uInputTokens = uPrompt;
    pCtx->pResponse->tUsage.uOutputTokens = uEval;
    ++pCtx->uUsageCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_USAGE;
    tEvent.as.tUsage.tUsage = pCtx->pResponse->tUsage;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__ollama_stream_process_payload(
    xllm__openai_stream_context *pCtx,
    const char *sPayload,
    size_t iPayloadLen
)
{
    xvalue tRoot = NULL;
    xvalue tMessage;
    xvalue tImages;
    xvalue tToolCalls;
    char *sPayloadCopy = NULL;
    const char *sModel;
    const char *sContent;
    const char *sThinking;
    const char *sDoneReason;
    bool bDone = false;

    if ( !pCtx || !sPayload ) {
        return XRT_NET_ERROR;
    }

    sPayloadCopy = (char *)xrtCalloc(iPayloadLen + 1u, sizeof(char));
    if ( !sPayloadCopy ) {
        return XRT_NET_ERROR;
    }
    memcpy(sPayloadCopy, sPayload, iPayloadLen);

    tRoot = xrtParseJSON((str)sPayloadCopy, iPayloadLen);
    if ( !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "failed to parse ollama-native stream payload");
        xrtFree(sPayloadCopy);
        if ( tRoot ) {
            xvoUnref(tRoot);
        }
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_response(pCtx) != XRT_NET_OK ) {
        xrtFree(sPayloadCopy);
        xvoUnref(tRoot);
        return XRT_NET_ERROR;
    }
    sModel = xllm__json_table_get_text(tRoot, "model");
    if ( !pCtx->pResponse->sModel && sModel ) {
        pCtx->pResponse->sModel = xllm__dup_cstr(sModel);
    }
    if ( xllm__openai_stream_emit_start(pCtx) != XRT_NET_OK ) {
        xrtFree(sPayloadCopy);
        xvoUnref(tRoot);
        return XRT_NET_CANCELLED;
    }

    tMessage = xllm__json_table_get(tRoot, "message");
    if ( tMessage && xvoType(tMessage) == XVO_DT_TABLE ) {
        sThinking = xllm__json_table_get_text(tMessage, "thinking");
        if ( sThinking && xllm__openai_stream_append_thinking(pCtx, sThinking) != XRT_NET_OK ) {
            xrtFree(sPayloadCopy);
            xvoUnref(tRoot);
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }
        sContent = xllm__json_table_get_text(tMessage, "content");
        if ( sContent && xllm__openai_stream_append_text(pCtx, sContent) != XRT_NET_OK ) {
            xrtFree(sPayloadCopy);
            xvoUnref(tRoot);
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }

        tImages = xllm__json_table_get(tMessage, "images");
        if ( tImages && xllm__ollama_stream_append_images(pCtx, tImages) != XRT_NET_OK ) {
            xrtFree(sPayloadCopy);
            xvoUnref(tRoot);
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }

        tToolCalls = xllm__json_table_get(tMessage, "tool_calls");
        if ( tToolCalls && xvoType(tToolCalls) == XVO_DT_ARRAY ) {
            size_t i;
            for ( i = 0u; i < (size_t)xvoArrayItemCount(tToolCalls); ++i ) {
                xvalue tToolCall = xvoArrayGetValue(tToolCalls, (uint32)i);
                xvalue tFunction = xllm__json_table_get(tToolCall, "function");
                xvalue tArguments = xllm__json_table_get(tFunction, "arguments");
                const char *sToolName = xllm__json_table_get_text(tFunction, "name");
                const char *sCallId = xllm__json_table_get_text(tToolCall, "id");
                char *sArguments = NULL;

                if ( tArguments && xvoType(tArguments) != XVO_DT_NULL ) {
                    if ( xvoType(tArguments) == XVO_DT_TEXT ) {
                        sArguments = xllm__dup_cstr((const char *)xvoGetText(tArguments));
                    } else {
                        sArguments = (char *)xrtStringifyJSON(tArguments, 0, NULL);
                    }
                }
                if ( !sArguments ) {
                    sArguments = xllm__dup_cstr("{}");
                }
                if ( !sArguments ) {
                    xrtFree(sPayloadCopy);
                    xvoUnref(tRoot);
                    return XRT_NET_ERROR;
                }
                if ( xllm__openai_stream_append_tool_delta(
                        pCtx,
                        i,
                        (sCallId && sCallId[0]) ? sCallId : sToolName,
                        sToolName,
                        sArguments
                     ) != XRT_NET_OK ) {
                    xllm__free_cstr(&sArguments);
                    xrtFree(sPayloadCopy);
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
                xllm__free_cstr(&sArguments);
            }
        }
    }

    if ( xllm__ollama_stream_apply_usage(pCtx, tRoot) != XRT_NET_OK ) {
        xrtFree(sPayloadCopy);
        xvoUnref(tRoot);
        return XRT_NET_CANCELLED;
    }

    (void)xllm__json_table_get_bool(tRoot, "done", &bDone);
    if ( bDone ) {
        pCtx->bDone = true;
        sDoneReason = xllm__json_table_get_text(tRoot, "done_reason");
        if ( pCtx->uToolDeltaCount > 0u ) {
            sDoneReason = "tool_calls";
        } else if ( !sDoneReason || !sDoneReason[0] ) {
            sDoneReason = "stop";
        }
        xllm__free_cstr((char **)&pCtx->pResponse->sFinishReason);
        pCtx->pResponse->sFinishReason = xllm__dup_cstr(sDoneReason);
    }

    ++pCtx->uPayloadCount;
    xllm__ollama_trace_stream(pCtx->pRuntime, pCtx, bDone ? "done" : "payload", iPayloadLen);
    xrtFree(sPayloadCopy);
    xvoUnref(tRoot);
    return XRT_NET_OK;
}

static int xllm__ollama_stream_process_buffer(
    xllm__openai_stream_context *pCtx,
    const char *sBuffer,
    size_t iLen
)
{
    size_t iCursor = 0u;

    if ( !pCtx || !sBuffer ) {
        return XRT_NET_ERROR;
    }

    while ( iCursor < iLen ) {
        size_t iObjectStart;
        size_t iObjectLen;
        size_t iDepth = 0u;
        bool bInString = false;
        bool bEscape = false;

        while ( iCursor < iLen ) {
            char ch = sBuffer[iCursor];
            if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
                break;
            }
            ++iCursor;
        }
        if ( iCursor >= iLen ) {
            continue;
        }

        iObjectStart = iCursor;
        if ( sBuffer[iCursor] != '{' ) {
            xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "ollama-native stream payload is not a json object");
            return XRT_NET_ERROR;
        }

        while ( iCursor < iLen ) {
            char ch = sBuffer[iCursor++];

            if ( bInString ) {
                if ( bEscape ) {
                    bEscape = false;
                    continue;
                }
                if ( ch == '\\' ) {
                    bEscape = true;
                    continue;
                }
                if ( ch == '"' ) {
                    bInString = false;
                }
                continue;
            }

            if ( ch == '"' ) {
                bInString = true;
                continue;
            }
            if ( ch == '{' ) {
                ++iDepth;
                continue;
            }
            if ( ch == '}' ) {
                if ( iDepth == 0u ) {
                    xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "ollama-native stream payload has invalid object depth");
                    return XRT_NET_ERROR;
                }
                --iDepth;
                if ( iDepth == 0u ) {
                    break;
                }
            }
        }

        if ( iDepth != 0u || bInString ) {
            xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "ollama-native stream payload is incomplete");
            return XRT_NET_ERROR;
        }

        iObjectLen = iCursor - iObjectStart;
        if ( iObjectLen == 0u ) {
            continue;
        }

        if ( xllm__ollama_stream_process_payload(pCtx, sBuffer + iObjectStart, iObjectLen) != XRT_NET_OK ) {
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }
        pCtx->iParsedBytes = iCursor;
    }

    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_OK;
}

static int xllm__ollama_stream_finalize_response(xllm__openai_stream_context *pCtx)
{
    int iStatus = xllm__openai_stream_finalize_response(pCtx);

    if ( iStatus != XRT_NET_OK || !pCtx || !pCtx->pResponse ) {
        return iStatus;
    }

    if ( xllm__ollama_apply_terminal_status(pCtx->pResponse, pCtx->bDone, pCtx->bCancelled) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    xllm__ollama_trace_stream(pCtx->pRuntime, pCtx, "finalize", 0u);
    return XRT_NET_OK;
}

static int32 xllm__ollama_native_chat_stream_buffered(
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
    xvalue tRoot = NULL;
    char *sBody = NULL;
    char *sUrl = NULL;
    const char *sModel;
    xnet_result iNetStatus = XRT_NET_ERROR;
    int32 iStatus = XRT_NET_ERROR;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts = 1u;
    bool bRetryable = false;
    bool bMultimodal = false;

    if ( !pRuntime || !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    *ppResponse = NULL;
    memset(&tBody, 0, sizeof(tBody));
    memset(&tStream, 0, sizeof(tStream));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    xrtHttpRequestInit(&tHttpRequest);

    tStream.pRuntime = pRuntime;
    tStream.pProfile = pProfile;
    tStream.pRequest = pRequest;
    tStream.pOptions = pOptions;
    tStream.pError = pError;
    tStream.iMessageOutputIndex = (size_t)-1;
    tStream.iThinkingOutputIndex = (size_t)-1;
    tStream.iRefusalOutputIndex = (size_t)-1;

    if ( pOptions && pOptions->uMaxRetries > 0u ) {
        uMaxAttempts += pOptions->uMaxRetries;
    }

    sModel = xllm__openai_select_model(pProfile, pRequest, &bMultimodal);
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for ollama-native request");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    tStream.sSelectedModel = sModel;

    if ( xllm__openai_fill_effective_params(&tEffectiveParams, pProfile, pRequest, XLLM_STREAM_PREFER) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__ollama_build_chat_body(&tBody, pRuntime, pProfile, pRequest, &tEffectiveParams, pOptions, sModel, true, pError) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    sUrl = xllm__ollama_build_url(pProfile->sBaseUrl);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native profile missing base url");
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
        xllm__ollama_component_name(),
        "request start: model=%s streaming=true live=false attempt=%u/%u body_bytes=%u",
        sModel,
        (unsigned)uAttempt,
        (unsigned)uMaxAttempts,
        (unsigned)strlen(sBody)
    );
    xllm__ollama_trace_request(pRuntime, pProfile, pRequest, sModel, true, uAttempt, strlen(sBody));

    pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
    if ( !pHttpResponse ) {
        if ( iNetStatus == XRT_NET_TIMEOUT ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "ollama-native request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "ollama-native request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "ollama-native request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__ollama_fill_error_from_http(pError, pHttpResponse, tRoot);
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "ollama-native streaming response body is empty");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( xllm__ollama_stream_process_buffer(&tStream, pHttpResponse->pBody, pHttpResponse->iBodyLen) != XRT_NET_OK ) {
        if ( tStream.bCancelled && pError && pError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "ollama-native stream cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    if ( tStream.pResponse == NULL ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "ollama-native stream did not produce a response");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( xllm__ollama_stream_finalize_response(&tStream) != XRT_NET_OK ) {
        if ( tStream.bCancelled && pError && pError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "ollama-native stream cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    *ppResponse = tStream.pResponse;
    tStream.pResponse = NULL;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__ollama_component_name(),
        "response complete: model=%s streaming=true live=false attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount
    );
    xllm__ollama_trace_response(pRuntime, *ppResponse, sModel, pHttpResponse, NULL, XRT_NET_OK, uAttempt, false, true);
    iStatus = XRT_NET_OK;

fail:
    if ( iStatus != XRT_NET_OK ) {
        int32 iTraceTransportStatus = pHttpResponse ? XRT_NET_OK : (iNetStatus != XRT_NET_OK ? (int32)iNetStatus : iStatus);
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
                xllm__ollama_component_name(),
                "response failed: model=%s streaming=true live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
                (unsigned)uDelayMs
            );
            xllm__ollama_trace_response(pRuntime, NULL, sModel, pHttpResponse, pError, iTraceTransportStatus, uAttempt, true, true);
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
            iNetStatus = XRT_NET_ERROR;
            iStatus = XRT_NET_ERROR;
            xllm__openai_stream_reset_attempt_state(&tStream);
            if ( pError ) {
                xllm_error_reset(pError);
            }
            if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
                if ( pError ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "ollama-native request cancelled");
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
            xllm__ollama_component_name(),
            "response failed: model=%s streaming=true live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)uAttempt,
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
            bRetryable ? "true" : "false"
        );
        xllm__ollama_trace_response(pRuntime, NULL, sModel, pHttpResponse, pError, iTraceTransportStatus, uAttempt, bRetryable, true);
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

static int32 xllm__ollama_native_chat(
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
    xvalue tRoot = NULL;
    char *sBody = NULL;
    char *sUrl = NULL;
    const char *sModel;
    xnet_result iNetStatus = XRT_NET_ERROR;
    int32 iStatus = XRT_NET_ERROR;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts;
    bool bRetryable = false;
    bool bMultimodal = false;

    if ( !pRuntime || !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    memset(&tBody, 0, sizeof(tBody));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    xrtHttpRequestInit(&tHttpRequest);

    if ( pOptions &&
         (pOptions->eStreamMode == XLLM_STREAM_REQUIRE ||
          pOptions->eStreamMode == XLLM_STREAM_PREFER) ) {
        return xllm__ollama_native_chat_stream_buffered(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
    }

    sModel = xllm__openai_select_model(pProfile, pRequest, &bMultimodal);
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for ollama-native request");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__openai_fill_effective_params(&tEffectiveParams, pProfile, pRequest, XLLM_STREAM_OFF) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__ollama_build_chat_body(&tBody, pRuntime, pProfile, pRequest, &tEffectiveParams, pOptions, sModel, false, pError) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    sUrl = xllm__ollama_build_url(pProfile->sBaseUrl);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "ollama-native profile missing base url");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) goto fail;
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) goto fail;
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) goto fail;
    if ( xllm__openai_fill_request_headers(&tHttpRequest, pProfile) != XRT_NET_OK ) goto fail;
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) goto fail;

    uMaxAttempts = 1u;
    if ( pOptions && pOptions->uMaxRetries > 0u ) {
        uMaxAttempts += pOptions->uMaxRetries;
    }

retry_execute:
    ++uAttempt;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__ollama_component_name(),
        "request start: model=%s streaming=false live=false attempt=%u/%u body_bytes=%u",
        sModel,
        (unsigned)uAttempt,
        (unsigned)uMaxAttempts,
        (unsigned)strlen(sBody)
    );
    xllm__ollama_trace_request(pRuntime, pProfile, pRequest, sModel, false, uAttempt, strlen(sBody));

    pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
    if ( !pHttpResponse ) {
        if ( iNetStatus == XRT_NET_TIMEOUT ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "ollama-native request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "ollama-native request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "ollama-native request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__ollama_fill_error_from_http(pError, pHttpResponse, tRoot);
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "ollama-native response body is empty");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse ollama-native response json");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__ollama_build_response(pProfile, pRequest, pOptions, tRoot, &pResponse, pError) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
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
        xllm__ollama_component_name(),
        "response complete: model=%s streaming=false live=false attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount
    );
    xllm__ollama_trace_response(pRuntime, *ppResponse, sModel, pHttpResponse, NULL, XRT_NET_OK, uAttempt, false, false);
    iStatus = XRT_NET_OK;

fail:
    if ( iStatus != XRT_NET_OK ) {
        int32 iTraceTransportStatus = pHttpResponse ? XRT_NET_OK : (iNetStatus != XRT_NET_OK ? (int32)iNetStatus : iStatus);
        int32 iHttpStatus = pHttpResponse ? (int32)pHttpResponse->iStatusCode : (pError ? pError->iHttpStatus : 0);
        bRetryable = xllm__openai_error_is_retryable(pError ? pError->eCode : XLLM_ERROR_NONE);
        if ( bRetryable && uAttempt < uMaxAttempts ) {
            uint32 uDelayMs = xllm__openai_retry_delay_ms(pOptions, uAttempt);
            xllm__openai_logf(
                pRuntime,
                XLLM_LOG_WARN,
                xllm__ollama_component_name(),
                "response failed: model=%s streaming=false live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
                (unsigned)uDelayMs
            );
            xllm__ollama_trace_response(pRuntime, NULL, sModel, pHttpResponse, pError, iTraceTransportStatus, uAttempt, true, false);
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
            iNetStatus = XRT_NET_ERROR;
            iStatus = XRT_NET_ERROR;
            if ( pError ) {
                xllm_error_reset(pError);
            }
            if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
                if ( pError ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "ollama-native request cancelled");
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
            xllm__ollama_component_name(),
            "response failed: model=%s streaming=false live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)uAttempt,
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->eCode != XLLM_ERROR_NONE) ? xllm__openai_error_code_name(pError->eCode) : "unknown",
            bRetryable ? "true" : "false"
        );
        xllm__ollama_trace_response(pRuntime, NULL, sModel, pHttpResponse, pError, iTraceTransportStatus, uAttempt, bRetryable, false);
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

XLLM_API int xllm_register_ollama_native_adapter(xllm_runtime *pRuntime)
{
    xllm_adapter tAdapter;

    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    memset(&tAdapter, 0, sizeof(tAdapter));
    tAdapter.sName = XLLM_ADAPTER_OLLAMA_NATIVE;
    tAdapter.pCtx = pRuntime;
    tAdapter.pfnChat = xllm__ollama_native_chat;
    return xllm_register_adapter(pRuntime, &tAdapter);
}
