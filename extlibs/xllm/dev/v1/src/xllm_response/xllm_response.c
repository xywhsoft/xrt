#include "xllm_response.h"

static void xllm__output_item_free(xllm_output_item *pOutput)
{
    size_t i;

    if ( !pOutput ) {
        return;
    }

    switch ( pOutput->eKind ) {
        case XLLM_OUTPUT_MESSAGE:
            if ( pOutput->as.tMessage.pParts ) {
                for ( i = 0; i < pOutput->as.tMessage.iPartCount; ++i ) {
                    xllm__content_part_free(&pOutput->as.tMessage.pParts[i]);
                }
                xrtFree(pOutput->as.tMessage.pParts);
            }
            break;
        case XLLM_OUTPUT_THINKING:
            xllm__free_cstr((char **)&pOutput->as.tThinking.sFormat);
            xllm__free_cstr((char **)&pOutput->as.tThinking.sText);
            xllm__xvalue_release(&pOutput->as.tThinking.tVendorExtra);
            break;
        case XLLM_OUTPUT_TOOL_CALL:
            xllm__free_cstr((char **)&pOutput->as.tToolCall.sCallId);
            xllm__free_cstr((char **)&pOutput->as.tToolCall.sToolId);
            xllm__free_cstr((char **)&pOutput->as.tToolCall.sToolName);
            xllm__free_cstr((char **)&pOutput->as.tToolCall.sArgumentsJson);
            xllm__xvalue_release(&pOutput->as.tToolCall.tContinuation);
            xllm__xvalue_release(&pOutput->as.tToolCall.tVendorExtra);
            break;
        case XLLM_OUTPUT_REFUSAL:
            xllm__free_cstr((char **)&pOutput->as.tRefusal.sText);
            xllm__free_cstr((char **)&pOutput->as.tRefusal.sCategory);
            xllm__xvalue_release(&pOutput->as.tRefusal.tVendorExtra);
            break;
        default:
            break;
    }

    memset(pOutput, 0, sizeof(*pOutput));
}

XLLM_API const char *xllm_response_get_text(const xllm_response *pResponse)
{
    size_t i;

    if ( !pResponse ) {
        return NULL;
    }

    if ( pResponse->sVisibleText ) {
        return pResponse->sVisibleText;
    }

    if ( pResponse->tRefusal.sText ) {
        return pResponse->tRefusal.sText;
    }

    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];
        if ( pOutput->eKind == XLLM_OUTPUT_REFUSAL && pOutput->as.tRefusal.sText ) {
            return pOutput->as.tRefusal.sText;
        }
        if ( pOutput->eKind == XLLM_OUTPUT_MESSAGE && pOutput->as.tMessage.iPartCount > 0 ) {
            size_t j;
            for ( j = 0; j < pOutput->as.tMessage.iPartCount; ++j ) {
                const xllm_content_part *pPart = &pOutput->as.tMessage.pParts[j];
                if ( pPart->eKind == XLLM_PART_TEXT && pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT ) {
                    return pPart->as.tSource.as.sText;
                }
            }
        }
    }

    return NULL;
}

XLLM_API size_t xllm_response_get_output_count(const xllm_response *pResponse)
{
    return pResponse ? pResponse->iOutputCount : 0;
}

XLLM_API const xllm_output_item *xllm_response_get_output(const xllm_response *pResponse, size_t iIndex)
{
    if ( !pResponse || iIndex >= pResponse->iOutputCount ) {
        return NULL;
    }
    return &pResponse->pOutputs[iIndex];
}

XLLM_API size_t xllm_response_get_tool_call_count(const xllm_response *pResponse)
{
    size_t i;
    size_t iCount = 0;

    if ( !pResponse ) {
        return 0;
    }

    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        if ( pResponse->pOutputs[i].eKind == XLLM_OUTPUT_TOOL_CALL ) {
            ++iCount;
        }
    }

    return iCount;
}

XLLM_API const xllm_output_tool_call *xllm_response_get_tool_call(const xllm_response *pResponse, size_t iIndex)
{
    size_t i;
    size_t iSeen = 0;

    if ( !pResponse ) {
        return NULL;
    }

    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        if ( pResponse->pOutputs[i].eKind == XLLM_OUTPUT_TOOL_CALL ) {
            if ( iSeen == iIndex ) {
                return &pResponse->pOutputs[i].as.tToolCall;
            }
            ++iSeen;
        }
    }

    return NULL;
}

XLLM_API const xvalue *xllm_response_get_json(const xllm_response *pResponse, size_t iOutputIndex, size_t iPartIndex)
{
    const xllm_output_item *pOutput;

    if ( !pResponse || iOutputIndex >= pResponse->iOutputCount ) {
        return NULL;
    }

    pOutput = &pResponse->pOutputs[iOutputIndex];
    if ( pOutput->eKind != XLLM_OUTPUT_MESSAGE || iPartIndex >= pOutput->as.tMessage.iPartCount ) {
        return NULL;
    }

    if ( pOutput->as.tMessage.pParts[iPartIndex].eKind != XLLM_PART_JSON ) {
        return NULL;
    }

    return &pOutput->as.tMessage.pParts[iPartIndex].as.tJsonValue;
}

XLLM_API const xvalue *xllm_response_get_first_json(const xllm_response *pResponse, size_t *piOutputIndex, size_t *piPartIndex)
{
    size_t iOutput;

    if ( piOutputIndex ) {
        *piOutputIndex = (size_t)-1;
    }
    if ( piPartIndex ) {
        *piPartIndex = (size_t)-1;
    }
    if ( !pResponse ) {
        return NULL;
    }

    for ( iOutput = 0u; iOutput < pResponse->iOutputCount; ++iOutput ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[iOutput];
        size_t iPart;

        if ( pOutput->eKind != XLLM_OUTPUT_MESSAGE ) {
            continue;
        }
        for ( iPart = 0u; iPart < pOutput->as.tMessage.iPartCount; ++iPart ) {
            if ( pOutput->as.tMessage.pParts[iPart].eKind == XLLM_PART_JSON ) {
                if ( piOutputIndex ) {
                    *piOutputIndex = iOutput;
                }
                if ( piPartIndex ) {
                    *piPartIndex = iPart;
                }
                return &pOutput->as.tMessage.pParts[iPart].as.tJsonValue;
            }
        }
    }

    return NULL;
}

XLLM_API void xllm_response_free(xllm_response *pResponse)
{
    size_t i;

    if ( !pResponse ) {
        return;
    }

    xllm__free_cstr((char **)&pResponse->sId);
    xllm__free_cstr((char **)&pResponse->sProvider);
    xllm__free_cstr((char **)&pResponse->sProfileId);
    xllm__free_cstr((char **)&pResponse->sModel);
    xllm__free_cstr((char **)&pResponse->sFinishReason);
    xllm__free_cstr((char **)&pResponse->sVisibleText);

    if ( pResponse->pOutputs ) {
        for ( i = 0; i < pResponse->iOutputCount; ++i ) {
            xllm__output_item_free(&pResponse->pOutputs[i]);
        }
        xrtFree(pResponse->pOutputs);
    }

    xllm__xvalue_release(&pResponse->tUsage.tVendorExtra);
    xllm__free_cstr((char **)&pResponse->tRefusal.sText);
    xllm__free_cstr((char **)&pResponse->tRefusal.sCategory);
    xllm__xvalue_release(&pResponse->tRefusal.tVendorExtra);
    xllm__free_cstr((char **)&pResponse->tSafety.sBlockReason);
    xllm__xvalue_release(&pResponse->tSafety.tRatings);
    xllm__xvalue_release(&pResponse->tSafety.tVendorExtra);
    xllm__effective_params_reset(&pResponse->tEffectiveParams);
    xllm_error_reset(&pResponse->tError);
    xllm__xvalue_release(&pResponse->tRaw);
    xllm__xvalue_release(&pResponse->tVendorExtra);
    xrtFree(pResponse);
}
