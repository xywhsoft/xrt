#include "xllm_request.h"

#include <stdio.h>

static bool xllm__part_requires_multimodal_kind(xllm_part_kind eKind)
{
    return eKind == XLLM_PART_IMAGE ||
           eKind == XLLM_PART_FILE ||
           eKind == XLLM_PART_AUDIO ||
           eKind == XLLM_PART_VIDEO;
}

static bool xllm__message_requires_multimodal(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage ) {
        return false;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        if ( xllm__part_requires_multimodal_kind(pMessage->pParts[i].eKind) ) {
            return true;
        }
    }
    return false;
}

static bool xllm__request_requires_multimodal(const xllm_request *pRequest)
{
    size_t i;

    if ( !pRequest ) {
        return false;
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        if ( xllm__message_requires_multimodal(&pRequest->pMessages[i]) ) {
            return true;
        }
    }

    for ( i = 0; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            if ( xllm__message_requires_multimodal(&pRequest->pContextBlocks[i].pMessages[j]) ) {
                return true;
            }
        }
    }

    return false;
}

static const xllm_model_binding *xllm__select_request_binding(
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    bool bNeedsMultimodal,
    const char **psSelectedModel
);

static void xllm__generation_params_clear_stops(xllm_generation_params *pParams)
{
    size_t i;

    if ( !pParams || !pParams->psStop ) {
        return;
    }

    for ( i = 0; i < pParams->iStopCount; ++i ) {
        xllm__free_cstr((char **)&pParams->psStop[i]);
    }

    xrtFree((void *)pParams->psStop);
    pParams->psStop = NULL;
    pParams->iStopCount = 0u;
}

static int xllm__generation_params_copy_stops(
    xllm_generation_params *pParams,
    const char **psStop,
    size_t iStopCount
)
{
    const char **psNewStops;
    size_t i;

    if ( !pParams ) {
        return XRT_NET_ERROR;
    }

    xllm__generation_params_clear_stops(pParams);
    if ( !psStop || iStopCount == 0u ) {
        return XRT_NET_OK;
    }

    psNewStops = (const char **)xrtCalloc(iStopCount, sizeof(char *));
    if ( !psNewStops ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iStopCount; ++i ) {
        psNewStops[i] = xllm__dup_cstr(psStop[i]);
        if ( !psNewStops[i] ) {
            size_t j;
            for ( j = 0; j < i; ++j ) {
                xllm__free_cstr((char **)&psNewStops[j]);
            }
            xrtFree((void *)psNewStops);
            return XRT_NET_ERROR;
        }
    }

    pParams->psStop = psNewStops;
    pParams->iStopCount = iStopCount;
    return XRT_NET_OK;
}

static int xllm__merge_generation_params(
    xllm_generation_params *pOut,
    const xllm_generation_params *pDefaults,
    const xllm_generation_params *pOverride
)
{
    if ( !pOut || !pDefaults || !pOverride ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    if ( xllm__generation_params_clone(pOut, pDefaults) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( pOverride->tTemperature.bSet ) {
        pOut->tTemperature = pOverride->tTemperature;
    }
    if ( pOverride->tTopP.bSet ) {
        pOut->tTopP = pOverride->tTopP;
    }
    if ( pOverride->tMaxOutputTokens.bSet ) {
        pOut->tMaxOutputTokens = pOverride->tMaxOutputTokens;
    }
    if ( pOverride->tSeed.bSet ) {
        pOut->tSeed = pOverride->tSeed;
    }
    if ( pOverride->iStopCount > 0u &&
         xllm__generation_params_copy_stops(pOut, pOverride->psStop, pOverride->iStopCount) != XRT_NET_OK ) {
        xllm__generation_params_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__merge_reasoning_options(
    xllm_reasoning_options *pOut,
    const xllm_reasoning_options *pDefaults,
    const xllm_reasoning_options *pOverride
)
{
    if ( !pOut || !pDefaults || !pOverride ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    if ( xllm__reasoning_options_clone(pOut, pDefaults) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( pOverride->tEnabled.bSet ) {
        pOut->tEnabled = pOverride->tEnabled;
    }
    if ( pOverride->eLevel != XLLM_REASONING_DEFAULT ) {
        pOut->eLevel = pOverride->eLevel;
    }
    if ( pOverride->tBudgetTokens.bSet ) {
        pOut->tBudgetTokens = pOverride->tBudgetTokens;
    }
    if ( pOverride->tExposeThinking.bSet ) {
        pOut->tExposeThinking = pOverride->tExposeThinking;
    }
    if ( pOverride->tVendorExtra ) {
        xllm__xvalue_release(&pOut->tVendorExtra);
        pOut->tVendorExtra = pOverride->tVendorExtra;
        xllm__xvalue_addref(pOut->tVendorExtra);
    }

    return XRT_NET_OK;
}

static int xllm__merge_response_format(
    xllm_response_format *pOut,
    const xllm_response_format *pDefaults,
    const xllm_response_format *pOverride
)
{
    bool bHasOverride;

    if ( !pOut || !pDefaults || !pOverride ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    if ( xllm__response_format_clone(pOut, pDefaults) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    bHasOverride = (pOverride->eKind != XLLM_RESPONSE_TEXT) ||
                   (pOverride->sSchemaName != NULL) ||
                   (pOverride->tJsonSchema != NULL) ||
                   (pOverride->tVendorExtra != NULL);
    if ( !bHasOverride ) {
        return XRT_NET_OK;
    }

    xllm__response_format_reset(pOut);
    if ( xllm__response_format_clone(pOut, pOverride) != XRT_NET_OK ) {
        xllm__response_format_reset(pOut);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__apply_f64_rule(xllm_opt_f64 *pValue, const xllm_float_rule *pRule)
{
    if ( !pValue || !pRule ) {
        return;
    }

    switch ( pRule->eKind ) {
        case XLLM_PARAM_RULE_UNSUPPORTED:
            memset(pValue, 0, sizeof(*pValue));
            break;
        case XLLM_PARAM_RULE_FIXED:
            pValue->bSet = true;
            pValue->fValue = pRule->fFixed;
            break;
        case XLLM_PARAM_RULE_RANGE:
            if ( pValue->bSet ) {
                if ( pValue->fValue < pRule->fMin ) {
                    pValue->fValue = pRule->fMin;
                }
                if ( pValue->fValue > pRule->fMax ) {
                    pValue->fValue = pRule->fMax;
                }
            }
            break;
        case XLLM_PARAM_RULE_PASSTHROUGH:
        case XLLM_PARAM_RULE_UNSPECIFIED:
        default:
            break;
    }
}

static void xllm__apply_u32_rule(xllm_opt_u32 *pValue, const xllm_u32_rule *pRule)
{
    if ( !pValue || !pRule ) {
        return;
    }

    switch ( pRule->eKind ) {
        case XLLM_PARAM_RULE_UNSUPPORTED:
            memset(pValue, 0, sizeof(*pValue));
            break;
        case XLLM_PARAM_RULE_FIXED:
            pValue->bSet = true;
            pValue->iValue = pRule->uFixed;
            break;
        case XLLM_PARAM_RULE_RANGE:
            if ( pValue->bSet ) {
                if ( pValue->iValue < pRule->uMin ) {
                    pValue->iValue = pRule->uMin;
                }
                if ( pValue->iValue > pRule->uMax ) {
                    pValue->iValue = pRule->uMax;
                }
            }
            break;
        case XLLM_PARAM_RULE_PASSTHROUGH:
        case XLLM_PARAM_RULE_UNSPECIFIED:
        default:
            break;
    }
}

static void xllm__apply_generation_param_rules(
    xllm_generation_params *pGeneration,
    const xllm_model_binding *pBinding
)
{
    if ( !pGeneration || !pBinding ) {
        return;
    }

    xllm__apply_f64_rule(&pGeneration->tTemperature, &pBinding->tCaps.tTemperatureRule);
    xllm__apply_f64_rule(&pGeneration->tTopP, &pBinding->tCaps.tTopPRule);
    xllm__apply_u32_rule(&pGeneration->tMaxOutputTokens, &pBinding->tCaps.tMaxOutputTokensRule);
}

static int xllm__resolve_effective_params(
    xllm_effective_params *pOut,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    xllm_stream_mode eStreamMode
)
{
    const xllm_model_binding *pBinding;
    bool bNeedsMultimodal;

    if ( !pOut || !pProfile || !pRequest ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    if ( xllm__merge_generation_params(&pOut->tGeneration, &pProfile->tDefaults.tGeneration, &pRequest->tGeneration) != XRT_NET_OK ) {
        xllm__effective_params_reset(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__merge_reasoning_options(&pOut->tReasoning, &pProfile->tDefaults.tReasoning, &pRequest->tReasoning) != XRT_NET_OK ) {
        xllm__effective_params_reset(pOut);
        return XRT_NET_ERROR;
    }
    if ( xllm__merge_response_format(&pOut->tResponseFormat, &pProfile->tDefaults.tResponseFormat, &pRequest->tResponseFormat) != XRT_NET_OK ) {
        xllm__effective_params_reset(pOut);
        return XRT_NET_ERROR;
    }

    bNeedsMultimodal = xllm__request_requires_multimodal(pRequest);
    pBinding = xllm__select_request_binding(pProfile, pRequest, bNeedsMultimodal, NULL);
    xllm__apply_generation_param_rules(&pOut->tGeneration, pBinding);
    pOut->eStreamMode = eStreamMode;
    return XRT_NET_OK;
}

static bool xllm__binding_has_known_caps(const xllm_model_binding *pBinding)
{
    const xllm_model_caps *pCaps;

    if ( !pBinding ) {
        return false;
    }

    if ( pBinding->eCapMode == XLLM_CAP_MODE_EXACT ) {
        return true;
    }

    pCaps = &pBinding->tCaps;
    return pCaps->uFlags != 0u ||
           pCaps->iSupportedMimeTypeCount != 0u ||
           pCaps->uMaxPartsPerMessage != 0u ||
           pCaps->uMaxImages != 0u ||
           pCaps->uMaxFiles != 0u ||
           pCaps->uMaxPartBytes != 0u ||
           pCaps->tTemperatureRule.eKind != XLLM_PARAM_RULE_UNSPECIFIED ||
           pCaps->tTopPRule.eKind != XLLM_PARAM_RULE_UNSPECIFIED ||
           pCaps->tMaxOutputTokensRule.eKind != XLLM_PARAM_RULE_UNSPECIFIED;
}

static bool xllm__binding_supports_flag(const xllm_model_binding *pBinding, xllm_capability_flags uFlag)
{
    if ( !pBinding || !uFlag ) {
        return true;
    }

    if ( !xllm__binding_has_known_caps(pBinding) ) {
        return true;
    }

    return (pBinding->tCaps.uFlags & uFlag) != 0u;
}

static bool xllm__binding_supports_any_flag(const xllm_model_binding *pBinding, xllm_capability_flags uFlags)
{
    if ( !pBinding || !uFlags ) {
        return true;
    }

    if ( !xllm__binding_has_known_caps(pBinding) ) {
        return true;
    }

    return (pBinding->tCaps.uFlags & uFlags) != 0u;
}

static const xllm_model_binding *xllm__select_request_binding(
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    bool bNeedsMultimodal,
    const char **psSelectedModel
)
{
    const xllm_model_binding *pBinding = NULL;

    if ( psSelectedModel ) {
        *psSelectedModel = NULL;
    }

    if ( !pProfile || !pRequest ) {
        return NULL;
    }

    switch ( pRequest->eSlot ) {
        case XLLM_SLOT_MULTIMODAL:
            pBinding = &pProfile->tModels.tMultimodal;
            break;
        case XLLM_SLOT_TEXT:
            pBinding = pProfile->tModels.tText.sModelId ? &pProfile->tModels.tText : &pProfile->tModels.tMultimodal;
            break;
        case XLLM_SLOT_AUTO:
        default:
            if ( bNeedsMultimodal ) {
                pBinding = &pProfile->tModels.tMultimodal;
            } else {
                pBinding = pProfile->tModels.tText.sModelId ? &pProfile->tModels.tText : &pProfile->tModels.tMultimodal;
            }
            break;
    }

    if ( psSelectedModel && pBinding ) {
        *psSelectedModel = pBinding->sModelId;
    }

    return pBinding;
}

static void xllm__validation_error_detail(
    xllm_error *pError,
    xllm_error_code eCode,
    const char *sMessage,
    const char *sSelectedModel,
    int32 iMessageIndex,
    int32 iPartIndex,
    xllm_capability_flags uRequiredCapability,
    const char *sMimeType
)
{
    xllm__error_set(pError, eCode, sMessage);
    if ( !pError ) {
        return;
    }

    pError->iMessageIndex = iMessageIndex;
    pError->iPartIndex = iPartIndex;
    pError->uRequiredCapability = uRequiredCapability;
    if ( sSelectedModel ) {
        pError->sSelectedModel = xllm__dup_cstr(sSelectedModel);
    }
    if ( sMimeType ) {
        pError->sMimeType = xllm__dup_cstr(sMimeType);
    }
}

static bool xllm__mime_matches_pattern(const char *sMimeType, const char *sPattern)
{
    const char *sMimeSlash;
    const char *sPatternSlash;
    size_t iMimeTypeLen;
    size_t iPatternTypeLen;

    if ( !sMimeType || !sPattern ) {
        return false;
    }

    if ( strcmp(sMimeType, sPattern) == 0 ) {
        return true;
    }

    sMimeSlash = strchr(sMimeType, '/');
    sPatternSlash = strchr(sPattern, '/');
    if ( !sMimeSlash || !sPatternSlash ) {
        return false;
    }

    iMimeTypeLen = (size_t)(sMimeSlash - sMimeType);
    iPatternTypeLen = (size_t)(sPatternSlash - sPattern);
    if ( iPatternTypeLen == 1u &&
         sPattern[0] == '*' &&
         strcmp(sPatternSlash + 1, "*") == 0 ) {
        return true;
    }

    if ( strcmp(sPatternSlash + 1, "*") == 0 &&
         iMimeTypeLen == iPatternTypeLen &&
         strncmp(sMimeType, sPattern, iMimeTypeLen) == 0 ) {
        return true;
    }

    return false;
}

static bool xllm__mime_is_supported(const xllm_model_binding *pBinding, const char *sMimeType)
{
    size_t i;

    if ( !pBinding || !sMimeType || !sMimeType[0] ) {
        return true;
    }

    if ( pBinding->tCaps.iSupportedMimeTypeCount == 0u ) {
        return true;
    }

    for ( i = 0; i < pBinding->tCaps.iSupportedMimeTypeCount; ++i ) {
        const char *sPattern = pBinding->tCaps.psSupportedMimeTypes[i];
        if ( sPattern && xllm__mime_matches_pattern(sMimeType, sPattern) ) {
            return true;
        }
    }

    return false;
}

static bool xllm__tool_policy_matches_name(const xllm_request *pRequest, const char *sToolName)
{
    size_t i;

    if ( !pRequest || !sToolName || !sToolName[0] ) {
        return false;
    }

    for ( i = 0; i < pRequest->iToolCount; ++i ) {
        const xllm_tool_def *pTool = &pRequest->pTools[i];
        if ( (pTool->sWireName && strcmp(pTool->sWireName, sToolName) == 0) ||
             (pTool->sToolId && strcmp(pTool->sToolId, sToolName) == 0) ) {
            return true;
        }
    }

    return false;
}

static bool xllm__reasoning_requested(const xllm_reasoning_options *pReasoning)
{
    if ( !pReasoning ) {
        return false;
    }

    return (pReasoning->tEnabled.bSet && pReasoning->tEnabled.bValue) ||
           pReasoning->eLevel != XLLM_REASONING_DEFAULT ||
           pReasoning->tBudgetTokens.bSet;
}

static uint64 xllm__part_payload_size(const xllm_content_part *pPart)
{
    if ( !pPart ) {
        return 0u;
    }

    if ( pPart->eKind == XLLM_PART_JSON ) {
        return 0u;
    }

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_INLINE_TEXT:
            return pPart->as.tSource.as.sText ? (uint64)strlen(pPart->as.tSource.as.sText) : 0u;
        case XLLM_SOURCE_INLINE_BYTES:
            return (uint64)pPart->as.tSource.as.tBytes.iSize;
        case XLLM_SOURCE_URL:
            return pPart->as.tSource.as.sUrl ? (uint64)strlen(pPart->as.tSource.as.sUrl) : 0u;
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            return pPart->as.tSource.as.sFileId ? (uint64)strlen(pPart->as.tSource.as.sFileId) : 0u;
        default:
            return 0u;
    }
}

static bool xllm__part_source_is_valid(const xllm_content_part *pPart)
{
    if ( !pPart ) {
        return false;
    }

    if ( pPart->eKind == XLLM_PART_JSON ) {
        return true;
    }

    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
            return pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT &&
                   pPart->as.tSource.as.sText != NULL;
        case XLLM_PART_IMAGE:
        case XLLM_PART_FILE:
        case XLLM_PART_AUDIO:
        case XLLM_PART_VIDEO:
            switch ( pPart->as.tSource.eKind ) {
                case XLLM_SOURCE_INLINE_BYTES:
                    return pPart->as.tSource.as.tBytes.pData != NULL &&
                           pPart->as.tSource.as.tBytes.iSize > 0u;
                case XLLM_SOURCE_URL:
                    return pPart->as.tSource.as.sUrl != NULL;
                case XLLM_SOURCE_PROVIDER_FILE_ID:
                    return pPart->as.tSource.as.sFileId != NULL;
                default:
                    return false;
            }
        default:
            return false;
    }
}

static int xllm__validate_message_against_binding(
    const xllm_message *pMessage,
    int32 iMessageIndex,
    const xllm_model_binding *pBinding,
    const char *sSelectedModel,
    uint32 *puImageCount,
    uint32 *puFileCount,
    xllm_error *pError
)
{
    size_t i;

    if ( !pMessage ) {
        return XRT_NET_OK;
    }

    if ( pBinding && pBinding->tCaps.uMaxPartsPerMessage != 0u &&
         pMessage->iPartCount > pBinding->tCaps.uMaxPartsPerMessage ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_TOO_MANY_INPUT_PARTS,
            "message exceeds model max parts per message",
            sSelectedModel,
            iMessageIndex,
            -1,
            0u,
            NULL
        );
        return XRT_NET_ERROR;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL && !pMessage->sToolCallId ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "tool message is missing tool_call_id",
            sSelectedModel,
            iMessageIndex,
            -1,
            0u,
            NULL
        );
        return XRT_NET_ERROR;
    }

    if ( pMessage->iToolCallCount != 0u && pMessage->eRole != XLLM_ROLE_ASSISTANT ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "tool calls are only allowed on assistant messages",
            sSelectedModel,
            iMessageIndex,
            -1,
            0u,
            NULL
        );
        return XRT_NET_ERROR;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL &&
         !xllm__binding_supports_flag(pBinding, XLLM_CAP_TOOL_RESULT_IN) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not support tool result input",
            sSelectedModel,
            iMessageIndex,
            -1,
            XLLM_CAP_TOOL_RESULT_IN,
            NULL
        );
        return XRT_NET_ERROR;
    }

    if ( pMessage->iToolCallCount != 0u &&
         !xllm__binding_supports_flag(pBinding, XLLM_CAP_TOOL_CALL_OUT) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not support tool call messages",
            sSelectedModel,
            iMessageIndex,
            -1,
            XLLM_CAP_TOOL_CALL_OUT,
            NULL
        );
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        const xllm_content_part *pPart = &pMessage->pParts[i];
        xllm_capability_flags uRequiredCapability = 0u;
        uint64 uPayloadSize;

        if ( !xllm__part_source_is_valid(pPart) ) {
            xllm__validation_error_detail(
                pError,
                XLLM_ERROR_INVALID_REQUEST,
                "content part source is invalid",
                sSelectedModel,
                iMessageIndex,
                (int32)i,
                0u,
                pPart->eKind == XLLM_PART_JSON ? NULL : pPart->as.tSource.sMimeType
            );
            return XRT_NET_ERROR;
        }

        switch ( pPart->eKind ) {
            case XLLM_PART_IMAGE:
                uRequiredCapability = XLLM_CAP_IMAGE_IN;
                if ( puImageCount ) {
                    ++(*puImageCount);
                }
                break;
            case XLLM_PART_FILE:
                uRequiredCapability = XLLM_CAP_FILE_IN;
                if ( puFileCount ) {
                    ++(*puFileCount);
                }
                break;
            case XLLM_PART_AUDIO:
                uRequiredCapability = XLLM_CAP_AUDIO_IN;
                break;
            case XLLM_PART_VIDEO:
                uRequiredCapability = XLLM_CAP_VIDEO_IN;
                break;
            case XLLM_PART_TEXT:
            case XLLM_PART_JSON:
            default:
                break;
        }

        if ( uRequiredCapability != 0u &&
             !xllm__binding_supports_flag(pBinding, uRequiredCapability) ) {
            xllm__validation_error_detail(
                pError,
                XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                "selected model does not support this input type",
                sSelectedModel,
                iMessageIndex,
                (int32)i,
                uRequiredCapability,
                pPart->as.tSource.sMimeType
            );
            return XRT_NET_ERROR;
        }

        if ( pPart->eKind != XLLM_PART_TEXT &&
             pPart->eKind != XLLM_PART_JSON &&
             !xllm__mime_is_supported(pBinding, pPart->as.tSource.sMimeType) ) {
            xllm__validation_error_detail(
                pError,
                XLLM_ERROR_UNSUPPORTED_MIME_TYPE,
                "mime type is not supported by the selected model",
                sSelectedModel,
                iMessageIndex,
                (int32)i,
                uRequiredCapability,
                pPart->as.tSource.sMimeType
            );
            return XRT_NET_ERROR;
        }

        uPayloadSize = xllm__part_payload_size(pPart);
        if ( pBinding && pBinding->tCaps.uMaxPartBytes != 0u &&
             uPayloadSize > pBinding->tCaps.uMaxPartBytes ) {
            xllm__validation_error_detail(
                pError,
                XLLM_ERROR_INPUT_TOO_LARGE,
                "content part exceeds model size limit",
                sSelectedModel,
                iMessageIndex,
                (int32)i,
                uRequiredCapability,
                pPart->eKind == XLLM_PART_JSON ? NULL : pPart->as.tSource.sMimeType
            );
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

static uint32 xllm__estimate_text_tokens(const char *sText)
{
    size_t iLen;

    if ( !sText || !sText[0] ) {
        return 0;
    }

    iLen = strlen(sText);
    return (uint32)((iLen + 3u) / 4u);
}

static uint32 xllm__estimate_part_tokens(const xllm_content_part *pPart)
{
    if ( !pPart ) {
        return 0;
    }

    if ( pPart->eKind == XLLM_PART_JSON ) {
        return 32;
    }

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_INLINE_TEXT:
            return xllm__estimate_text_tokens(pPart->as.tSource.as.sText);
        case XLLM_SOURCE_INLINE_BYTES:
            return (uint32)((pPart->as.tSource.as.tBytes.iSize + 255u) / 256u);
        case XLLM_SOURCE_URL:
            return 32 + xllm__estimate_text_tokens(pPart->as.tSource.as.sUrl);
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            return 16 + xllm__estimate_text_tokens(pPart->as.tSource.as.sFileId);
        default:
            return 0;
    }
}

static uint32 xllm__estimate_message_tokens(const xllm_message *pMessage)
{
    uint32 uTokens = 4;
    size_t i;

    if ( !pMessage ) {
        return 0;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        uTokens += xllm__estimate_part_tokens(&pMessage->pParts[i]);
    }

    for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
        uTokens += 16;
        uTokens += xllm__estimate_text_tokens(pMessage->pToolCalls[i].sToolId);
        uTokens += xllm__estimate_text_tokens(pMessage->pToolCalls[i].sToolName);
        uTokens += xllm__estimate_text_tokens(pMessage->pToolCalls[i].sArgumentsJson);
    }

    return uTokens;
}

static int xllm__read_file_bytes(const char *sPath, void **ppData, size_t *piSize)
{
    FILE *pFile;
    long iFileSize;
    void *pData;

    if ( !sPath || !ppData || !piSize ) {
        return XRT_NET_ERROR;
    }

    *ppData = NULL;
    *piSize = 0;

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return XRT_NET_ERROR;
    }

    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        return XRT_NET_ERROR;
    }

    iFileSize = ftell(pFile);
    if ( iFileSize < 0 ) {
        fclose(pFile);
        return XRT_NET_ERROR;
    }

    if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
        fclose(pFile);
        return XRT_NET_ERROR;
    }

    pData = xrtCalloc(1, (size_t)iFileSize);
    if ( iFileSize > 0 && !pData ) {
        fclose(pFile);
        return XRT_NET_ERROR;
    }

    if ( iFileSize > 0 && fread(pData, 1, (size_t)iFileSize, pFile) != (size_t)iFileSize ) {
        xrtFree(pData);
        fclose(pFile);
        return XRT_NET_ERROR;
    }

    fclose(pFile);
    *ppData = pData;
    *piSize = (size_t)iFileSize;
    return XRT_NET_OK;
}

static const char *xllm__path_basename(const char *sPath)
{
    const char *sSlash;
    const char *sBackslash;
    const char *sBase;

    if ( !sPath ) {
        return NULL;
    }

    sSlash = strrchr(sPath, '/');
    sBackslash = strrchr(sPath, '\\');
    sBase = sSlash;
    if ( !sBase || (sBackslash && sBackslash > sBase) ) {
        sBase = sBackslash;
    }

    return sBase ? (sBase + 1) : sPath;
}

static int xllm__turn_add_part_as_user_message(xllm_turn *pTurn, xllm_content_part *pPart)
{
    xllm_message tMessage;
    xllm_message *pMessages;
    xllm_message *pLastMessage;

    if ( !pTurn || !pPart ) {
        return XRT_NET_ERROR;
    }

    if ( pTurn->iMessageCount > 0u ) {
        pLastMessage = &pTurn->pMessages[pTurn->iMessageCount - 1u];
        if ( pLastMessage->eRole == XLLM_ROLE_USER &&
             pLastMessage->sToolCallId == NULL &&
             pLastMessage->iToolCallCount == 0u ) {
            xllm_content_part *pParts = (xllm_content_part *)xrtRealloc(
                pLastMessage->pParts,
                (pLastMessage->iPartCount + 1u) * sizeof(xllm_content_part)
            );

            if ( !pParts ) {
                return XRT_NET_ERROR;
            }

            pLastMessage->pParts = pParts;
            pLastMessage->pParts[pLastMessage->iPartCount] = *pPart;
            ++pLastMessage->iPartCount;
            memset(pPart, 0, sizeof(*pPart));
            return XRT_NET_OK;
        }
    }

    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.eRole = XLLM_ROLE_USER;
    tMessage.pParts = (xllm_content_part *)xrtCalloc(1, sizeof(xllm_content_part));
    if ( !tMessage.pParts ) {
        return XRT_NET_ERROR;
    }

    tMessage.iPartCount = 1;
    tMessage.pParts[0] = *pPart;
    memset(pPart, 0, sizeof(*pPart));

    pMessages = (xllm_message *)xrtRealloc(pTurn->pMessages, (pTurn->iMessageCount + 1) * sizeof(xllm_message));
    if ( !pMessages ) {
        xllm__message_free(&tMessage);
        return XRT_NET_ERROR;
    }

    pTurn->pMessages = pMessages;
    pTurn->pMessages[pTurn->iMessageCount] = tMessage;
    ++pTurn->iMessageCount;

    return XRT_NET_OK;
}

XLLM_API int xllm_validate_request(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_error *pError
)
{
    const xllm_profile *pProfile;
    const xllm_adapter *pAdapter;
    const xllm_model_binding *pBinding;
    const char *sSelectedModel = NULL;
    bool bNeedsMultimodal;
    uint32 uImageCount = 0u;
    uint32 uFileCount = 0u;
    xllm_effective_params tEffectiveParams;
    size_t i;

    if ( pError ) {
        xllm_error_reset(pError);
    }
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));

    if ( !pRuntime || !pRequest || !pRequest->sProfileId ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "request or profile_id is missing");
        return XRT_NET_ERROR;
    }

    pProfile = xllm__runtime_find_profile(pRuntime, pRequest->sProfileId);
    if ( !pProfile ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "profile not found");
        return XRT_NET_ERROR;
    }

    pAdapter = xllm__runtime_find_adapter(pRuntime, pProfile->sAdapter);
    if ( !pAdapter ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "adapter not found");
        return XRT_NET_ERROR;
    }

    if ( pRequest->iMessageCount == 0 && pRequest->iContextBlockCount == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "request has no messages or context blocks");
        return XRT_NET_ERROR;
    }

    bNeedsMultimodal = xllm__request_requires_multimodal(pRequest);
    if ( pRequest->eSlot == XLLM_SLOT_TEXT && bNeedsMultimodal ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "text slot cannot accept multimodal input");
        return XRT_NET_ERROR;
    }

    if ( bNeedsMultimodal ) {
        if ( !pProfile->tModels.tMultimodal.sModelId || !pProfile->tModels.tMultimodal.sModelId[0] ) {
            xllm__error_set(pError, XLLM_ERROR_MISSING_MULTIMODAL_MODEL, "multimodal input requires a multimodal model binding");
            return XRT_NET_ERROR;
        }
    } else if ( !pProfile->tModels.tText.sModelId && !pProfile->tModels.tMultimodal.sModelId ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "profile has no available model binding");
        return XRT_NET_ERROR;
    }

    if ( pRequest->eSlot == XLLM_SLOT_MULTIMODAL &&
         (!pProfile->tModels.tMultimodal.sModelId || !pProfile->tModels.tMultimodal.sModelId[0]) ) {
        xllm__error_set(pError, XLLM_ERROR_MISSING_MULTIMODAL_MODEL, "multimodal slot requires a multimodal model binding");
        return XRT_NET_ERROR;
    }

    pBinding = xllm__select_request_binding(pProfile, pRequest, bNeedsMultimodal, &sSelectedModel);
    if ( !pBinding || !sSelectedModel || !sSelectedModel[0] ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_MODEL_NOT_FOUND,
            "request could not resolve a target model",
            sSelectedModel,
            -1,
            -1,
            0u,
            NULL
        );
        return XRT_NET_ERROR;
    }

    if ( xllm__resolve_effective_params(
        &tEffectiveParams,
        pProfile,
        pRequest,
        pOptions ? pOptions->eStreamMode : XLLM_STREAM_AUTO
    ) != XRT_NET_OK ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_INTERNAL,
            "failed to resolve effective request parameters",
            sSelectedModel,
            -1,
            -1,
            0u,
            NULL
        );
        return XRT_NET_ERROR;
    }

    if ( pRequest->iToolCount != 0u &&
         !xllm__binding_supports_flag(pBinding, XLLM_CAP_TOOL_CALL_OUT) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not support tool calling",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_TOOL_CALL_OUT,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    if ( pRequest->tToolPolicy.eMode == XLLM_TOOL_CHOICE_REQUIRED && pRequest->iToolCount == 0u ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_INVALID_REQUEST,
            "tool_choice=required requires at least one tool",
            sSelectedModel,
            -1,
            -1,
            0u,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    if ( pRequest->tToolPolicy.eMode == XLLM_TOOL_CHOICE_NAMED ) {
        if ( pRequest->iToolCount == 0u || !xllm__tool_policy_matches_name(pRequest, pRequest->tToolPolicy.sToolName) ) {
            xllm__validation_error_detail(
                pError,
                XLLM_ERROR_INVALID_REQUEST,
                "named tool choice does not match any registered tool",
                sSelectedModel,
                -1,
                -1,
                0u,
                NULL
            );
            xllm__effective_params_reset(&tEffectiveParams);
            return XRT_NET_ERROR;
        }
    }

    if ( pRequest->tToolPolicy.bAllowParallel && pRequest->iToolCount != 0u &&
         !xllm__binding_supports_flag(pBinding, XLLM_CAP_PARALLEL_TOOL_CALL) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not support parallel tool calling",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_PARALLEL_TOOL_CALL,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    if ( pOptions && pOptions->eStreamMode == XLLM_STREAM_REQUIRE &&
         !xllm__binding_supports_flag(pBinding, XLLM_CAP_STREAM) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not support streaming",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_STREAM,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    if ( tEffectiveParams.tResponseFormat.eKind != XLLM_RESPONSE_TEXT &&
         !(pOptions && pOptions->bBestEffortStructuredOutput) &&
         !xllm__binding_supports_flag(pBinding, XLLM_CAP_JSON_OUT) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not support structured output",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_JSON_OUT,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    if ( xllm__reasoning_requested(&tEffectiveParams.tReasoning) &&
         !xllm__binding_supports_flag(pBinding, XLLM_CAP_REASONING_CONTROL) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not support reasoning controls",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_REASONING_CONTROL,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    if ( tEffectiveParams.tReasoning.tExposeThinking.bSet &&
         tEffectiveParams.tReasoning.tExposeThinking.bValue &&
         !xllm__binding_supports_any_flag(pBinding, XLLM_CAP_THINKING_SUMMARY_OUT | XLLM_CAP_THINKING_FULL_OUT) ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "selected model does not expose thinking output",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_THINKING_SUMMARY_OUT | XLLM_CAP_THINKING_FULL_OUT,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        if ( xllm__validate_message_against_binding(
            &pRequest->pMessages[i],
            (int32)i,
            pBinding,
            sSelectedModel,
            &uImageCount,
            &uFileCount,
            pError
        ) != XRT_NET_OK ) {
            xllm__effective_params_reset(&tEffectiveParams);
            return XRT_NET_ERROR;
        }
    }

    for ( i = 0; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            if ( xllm__validate_message_against_binding(
                &pRequest->pContextBlocks[i].pMessages[j],
                (int32)j,
                pBinding,
                sSelectedModel,
                &uImageCount,
                &uFileCount,
                pError
            ) != XRT_NET_OK ) {
                xllm__effective_params_reset(&tEffectiveParams);
                return XRT_NET_ERROR;
            }
        }
    }

    if ( pBinding && pBinding->tCaps.uMaxImages != 0u && uImageCount > pBinding->tCaps.uMaxImages ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_TOO_MANY_INPUT_PARTS,
            "image input count exceeds model limit",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_IMAGE_IN,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    if ( pBinding && pBinding->tCaps.uMaxFiles != 0u && uFileCount > pBinding->tCaps.uMaxFiles ) {
        xllm__validation_error_detail(
            pError,
            XLLM_ERROR_TOO_MANY_INPUT_PARTS,
            "file input count exceeds model limit",
            sSelectedModel,
            -1,
            -1,
            XLLM_CAP_FILE_IN,
            NULL
        );
        xllm__effective_params_reset(&tEffectiveParams);
        return XRT_NET_ERROR;
    }

    xllm__effective_params_reset(&tEffectiveParams);
    return XRT_NET_OK;
}

XLLM_API int xllm_count_tokens(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    xllm_token_count_result *pResult,
    xllm_error *pError
)
{
    const xllm_profile *pProfile;
    const xllm_adapter *pAdapter;
    uint32 uTotalTokens = 0;
    size_t i;

    if ( pError ) {
        xllm_error_reset(pError);
    }

    if ( !pRuntime || !pRequest || !pResult ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "token count arguments are invalid");
        return XRT_NET_ERROR;
    }

    memset(pResult, 0, sizeof(*pResult));
    pProfile = xllm__runtime_find_profile(pRuntime, pRequest->sProfileId);
    pAdapter = pProfile ? xllm__runtime_find_adapter(pRuntime, pProfile->sAdapter) : NULL;
    if ( pAdapter && pAdapter->pfnCountTokens ) {
        return pAdapter->pfnCountTokens(pAdapter->pCtx, pProfile, pRequest, pResult, pError);
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        uTotalTokens += xllm__estimate_message_tokens(&pRequest->pMessages[i]);
    }

    for ( i = 0; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            uTotalTokens += xllm__estimate_message_tokens(&pRequest->pContextBlocks[i].pMessages[j]);
        }
    }

    for ( i = 0; i < pRequest->iToolCount; ++i ) {
        uTotalTokens += 16;
        uTotalTokens += xllm__estimate_text_tokens(pRequest->pTools[i].sToolId);
        uTotalTokens += xllm__estimate_text_tokens(pRequest->pTools[i].sWireName);
        uTotalTokens += xllm__estimate_text_tokens(pRequest->pTools[i].sDescription);
    }

    pResult->uInputTokens = uTotalTokens;
    pResult->bEstimated = true;
    if ( pProfile ) {
        const xllm_model_binding *pBinding;
        bool bNeedsMultimodal = xllm__request_requires_multimodal(pRequest);
        pBinding = bNeedsMultimodal ? &pProfile->tModels.tMultimodal : &pProfile->tModels.tText;
        pResult->uEstimatedOutputReserve = pBinding->tCaps.uRecommendedOutputReserve;
    }
    if ( pResult->uEstimatedOutputReserve == 0 ) {
        pResult->uEstimatedOutputReserve = 1024;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_turn_set_system_prompt(xllm_turn *pTurn, const char *sText)
{
    char *sCopy = NULL;

    if ( !pTurn ) {
        return XRT_NET_ERROR;
    }

    if ( sText ) {
        sCopy = xllm__dup_cstr(sText);
        if ( !sCopy ) {
            return XRT_NET_ERROR;
        }
    }

    xllm__free_cstr((char **)&pTurn->sSystemPrompt);
    pTurn->sSystemPrompt = sCopy;
    return XRT_NET_OK;
}

XLLM_API int xllm_turn_set_system_mode(xllm_turn *pTurn, xllm_system_mode eMode)
{
    if ( !pTurn ) {
        return XRT_NET_ERROR;
    }

    pTurn->eSystemMode = eMode;
    return XRT_NET_OK;
}

XLLM_API int xllm_turn_set_tool_choice(
    xllm_turn *pTurn,
    xllm_tool_choice_mode eMode,
    const char *sToolName,
    bool bAllowParallel
)
{
    char *sToolNameCopy = NULL;

    if ( !pTurn ) {
        return XRT_NET_ERROR;
    }

    if ( eMode == XLLM_TOOL_CHOICE_NAMED ) {
        if ( !sToolName || !sToolName[0] ) {
            return XRT_NET_ERROR;
        }
        sToolNameCopy = xllm__dup_cstr(sToolName);
        if ( !sToolNameCopy ) {
            return XRT_NET_ERROR;
        }
    }

    xllm__free_cstr((char **)&pTurn->tToolPolicy.sToolName);
    pTurn->tToolPolicy.sToolName = sToolNameCopy;
    pTurn->tToolPolicy.eMode = eMode;
    pTurn->tToolPolicy.bAllowParallel = bAllowParallel;
    return XRT_NET_OK;
}

XLLM_API int xllm_turn_set_stop_sequences(xllm_turn *pTurn, const char **psStop, size_t iStopCount)
{
    if ( !pTurn ) {
        return XRT_NET_ERROR;
    }

    return xllm__generation_params_copy_stops(&pTurn->tGeneration, psStop, iStopCount);
}

XLLM_API int xllm_turn_set_json_schema_response(
    xllm_turn *pTurn,
    const char *sSchemaName,
    xvalue tJsonSchema,
    xvalue tVendorExtra
)
{
    char *sSchemaNameCopy = NULL;

    if ( !pTurn || !tJsonSchema ) {
        return XRT_NET_ERROR;
    }

    if ( sSchemaName ) {
        sSchemaNameCopy = xllm__dup_cstr(sSchemaName);
        if ( !sSchemaNameCopy ) {
            return XRT_NET_ERROR;
        }
    }

    xllm__response_format_reset(&pTurn->tResponseFormat);
    pTurn->tResponseFormat.eKind = XLLM_RESPONSE_JSON_SCHEMA;
    pTurn->tResponseFormat.sSchemaName = sSchemaNameCopy;
    pTurn->tResponseFormat.tJsonSchema = tJsonSchema;
    pTurn->tResponseFormat.tVendorExtra = tVendorExtra;
    xllm__xvalue_addref(pTurn->tResponseFormat.tJsonSchema);
    xllm__xvalue_addref(pTurn->tResponseFormat.tVendorExtra);
    return XRT_NET_OK;
}

XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText)
{
    xllm_content_part tPart;

    if ( !pTurn || !sText ) {
        return XRT_NET_ERROR;
    }

    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_TEXT;
    tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    tPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
    tPart.as.tSource.as.sText = xllm__dup_cstr(sText);
    if ( !tPart.as.tSource.as.sText ) {
        xllm__content_part_free(&tPart);
        return XRT_NET_ERROR;
    }

    return xllm__turn_add_part_as_user_message(pTurn, &tPart);
}

XLLM_API int xllm_turn_add_image_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType)
{
    xllm_content_part tPart;

    if ( !pTurn || !sUrl ) {
        return XRT_NET_ERROR;
    }

    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_IMAGE;
    tPart.as.tSource.eKind = XLLM_SOURCE_URL;
    tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType ? sMimeType : "application/octet-stream");
    tPart.as.tSource.as.sUrl = xllm__dup_cstr(sUrl);
    if ( !tPart.as.tSource.as.sUrl ) {
        xllm__content_part_free(&tPart);
        return XRT_NET_ERROR;
    }

    return xllm__turn_add_part_as_user_message(pTurn, &tPart);
}

XLLM_API int xllm_turn_add_image_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType)
{
    xllm_content_part tPart;
    void *pData;
    size_t iSize;
    int32 iStatus;

    if ( !pTurn || !sPath ) {
        return XRT_NET_ERROR;
    }

    memset(&tPart, 0, sizeof(tPart));
    pData = NULL;
    iSize = 0;
    iStatus = xllm__read_file_bytes(sPath, &pData, &iSize);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    tPart.eKind = XLLM_PART_IMAGE;
    tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_BYTES;
    tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType ? sMimeType : "application/octet-stream");
    tPart.as.tSource.sName = xllm__dup_cstr(xllm__path_basename(sPath));
    tPart.as.tSource.as.tBytes.pData = pData;
    tPart.as.tSource.as.tBytes.iSize = iSize;
    return xllm__turn_add_part_as_user_message(pTurn, &tPart);
}

XLLM_API int xllm_turn_add_image_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType)
{
    xllm_content_part tPart;

    if ( !pTurn || !sFileId ) {
        return XRT_NET_ERROR;
    }

    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_IMAGE;
    tPart.as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
    tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType ? sMimeType : "application/octet-stream");
    tPart.as.tSource.as.sFileId = xllm__dup_cstr(sFileId);
    if ( !tPart.as.tSource.as.sFileId ) {
        xllm__content_part_free(&tPart);
        return XRT_NET_ERROR;
    }

    return xllm__turn_add_part_as_user_message(pTurn, &tPart);
}

XLLM_API int xllm_turn_add_file_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType)
{
    xllm_content_part tPart;

    if ( !pTurn || !sUrl ) {
        return XRT_NET_ERROR;
    }

    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_FILE;
    tPart.as.tSource.eKind = XLLM_SOURCE_URL;
    tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType ? sMimeType : "application/octet-stream");
    tPart.as.tSource.as.sUrl = xllm__dup_cstr(sUrl);
    if ( !tPart.as.tSource.as.sUrl ) {
        xllm__content_part_free(&tPart);
        return XRT_NET_ERROR;
    }

    return xllm__turn_add_part_as_user_message(pTurn, &tPart);
}

XLLM_API int xllm_turn_add_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType)
{
    xllm_content_part tPart;
    void *pData;
    size_t iSize;
    int32 iStatus;

    if ( !pTurn || !sPath ) {
        return XRT_NET_ERROR;
    }

    memset(&tPart, 0, sizeof(tPart));
    pData = NULL;
    iSize = 0;
    iStatus = xllm__read_file_bytes(sPath, &pData, &iSize);
    if ( iStatus != XRT_NET_OK ) {
        return iStatus;
    }

    tPart.eKind = XLLM_PART_FILE;
    tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_BYTES;
    tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType ? sMimeType : "application/octet-stream");
    tPart.as.tSource.sName = xllm__dup_cstr(xllm__path_basename(sPath));
    tPart.as.tSource.as.tBytes.pData = pData;
    tPart.as.tSource.as.tBytes.iSize = iSize;
    return xllm__turn_add_part_as_user_message(pTurn, &tPart);
}

XLLM_API int xllm_turn_add_file_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType)
{
    xllm_content_part tPart;

    if ( !pTurn || !sFileId ) {
        return XRT_NET_ERROR;
    }

    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_FILE;
    tPart.as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
    tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType ? sMimeType : "application/octet-stream");
    tPart.as.tSource.as.sFileId = xllm__dup_cstr(sFileId);
    if ( !tPart.as.tSource.as.sFileId ) {
        xllm__content_part_free(&tPart);
        return XRT_NET_ERROR;
    }

    return xllm__turn_add_part_as_user_message(pTurn, &tPart);
}

XLLM_API int xllm_turn_add_tool(xllm_turn *pTurn, const xllm_tool_def *pTool)
{
    xllm_tool_def tCopy;
    xllm_tool_def *pTools;

    if ( !pTurn || !pTool ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__tool_def_clone(&tCopy, pTool) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    pTools = (xllm_tool_def *)xrtRealloc(pTurn->pTools, (pTurn->iToolCount + 1) * sizeof(xllm_tool_def));
    if ( !pTools ) {
        xllm__tool_def_free(&tCopy);
        return XRT_NET_ERROR;
    }

    pTurn->pTools = pTools;
    pTurn->pTools[pTurn->iToolCount] = tCopy;
    ++pTurn->iToolCount;

    return XRT_NET_OK;
}
