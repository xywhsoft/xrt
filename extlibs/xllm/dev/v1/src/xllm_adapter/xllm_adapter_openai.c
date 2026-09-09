#include "xllm_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef struct {
    char *pData;
    size_t iLen;
    size_t iCap;
} xllm__json_builder;

typedef struct {
    xllm_runtime *pRuntime;
    xllm_response *pResponse;
    const xllm_profile *pProfile;
    const xllm_request *pRequest;
    const xllm_call_options *pOptions;
    xllm_error *pError;
    const char *sSelectedModel;
    size_t iOutputCapacity;
    size_t iParsedBytes;
    size_t iMessageOutputIndex;
    size_t iMessagePartCapacity;
    size_t iThinkingOutputIndex;
    size_t iRefusalOutputIndex;
    size_t *pToolOutputIndices;
    size_t iToolOutputIndexCount;
    size_t iToolOutputIndexCapacity;
    uint32 uPayloadCount;
    uint32 uTextDeltaCount;
    uint32 uThinkingDeltaCount;
    uint32 uToolDeltaCount;
    uint32 uUsageCount;
    uint32 uRefusalCount;
    bool bStartEmitted;
    bool bMessageOutputClosed;
    bool bCancelled;
    bool bDone;
} xllm__openai_stream_context;

typedef struct {
    xmutex pMutex;
    xcond pCond;
    xnetstream *pStream;
    xnetengine *pEngine;
    xllm__json_builder tIncoming;
    char *pRequestBytes;
    size_t iRequestLen;
    int iSysErr;
    xnet_result iCloseReason;
    bool bClosed;
} xllm__openai_live_transport;

typedef enum {
    XLLM__HTTP_BODY_MODE_UNSET = 0,
    XLLM__HTTP_BODY_MODE_CONTENT_LENGTH,
    XLLM__HTTP_BODY_MODE_CHUNKED,
    XLLM__HTTP_BODY_MODE_UNTIL_CLOSE
} xllm__http_body_mode;

typedef enum {
    XLLM__HTTP_CHUNK_STATE_SIZE = 0,
    XLLM__HTTP_CHUNK_STATE_DATA,
    XLLM__HTTP_CHUNK_STATE_DATA_CRLF,
    XLLM__HTTP_CHUNK_STATE_TRAILERS,
    XLLM__HTTP_CHUNK_STATE_DONE
} xllm__http_chunk_state;

typedef struct {
    xllm__json_builder tWire;
    xllm__json_builder tBody;
    size_t iParseOffset;
    size_t iHeaderBytes;
    size_t iChunkBytesRemaining;
    uint32 uStatusCode;
    int64_t iContentLength;
    xllm__http_body_mode eBodyMode;
    xllm__http_chunk_state eChunkState;
    bool bHeadersParsed;
    bool bBodyComplete;
    bool bTreatAsSse;
    char sContentType[128];
    char sRequestId[128];
} xllm__openai_live_decoder;

static bool xllm__json_builder_reserve(xllm__json_builder *pBuilder, size_t iExtra);
static bool xllm__json_builder_append_bytes(xllm__json_builder *pBuilder, const void *pData, size_t iLen);
static bool xllm__json_builder_append_cstr(xllm__json_builder *pBuilder, const char *sText);
static char *xllm__json_builder_detach(xllm__json_builder *pBuilder);
static void xllm__json_builder_reset(xllm__json_builder *pBuilder);
static int xllm__openai_fill_effective_params(
    xllm_effective_params *pOut,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    xllm_stream_mode eStreamMode
);
static int xllm__openai_stream_process_buffer(
    xllm__openai_stream_context *pCtx,
    const char *sBuffer,
    size_t iLen
);

static void xllm__openai_trace_table_set_text(xvalue tPayload, const char *sKey, const char *sValue)
{
    if ( tPayload && sKey && sValue ) {
        xvoTableSetText(tPayload, (str)sKey, 0u, (str)sValue, 0u, FALSE);
    }
}

static void xllm__openai_trace_table_set_bool(xvalue tPayload, const char *sKey, bool bValue)
{
    if ( tPayload && sKey ) {
        xvoTableSetBool(tPayload, (str)sKey, 0u, bValue);
    }
}

static void xllm__openai_trace_table_set_u32(xvalue tPayload, const char *sKey, uint32 uValue)
{
    if ( tPayload && sKey ) {
        xvoTableSetInt(tPayload, (str)sKey, 0u, (int64)uValue);
    }
}

static void xllm__openai_trace_table_set_i32(xvalue tPayload, const char *sKey, int32 iValue)
{
    if ( tPayload && sKey ) {
        xvoTableSetInt(tPayload, (str)sKey, 0u, (int64)iValue);
    }
}

static void xllm__openai_trace_emit(xllm_runtime *pRuntime, xllm_trace_kind eKind, xvalue tPayload)
{
    if ( pRuntime && pRuntime->tOptions.pfnTrace && tPayload ) {
        pRuntime->tOptions.pfnTrace(
            pRuntime->tOptions.pTraceCtx,
            eKind,
            &tPayload
        );
    }
    xllm__xvalue_release(&tPayload);
}

static void xllm__openai_logf(
    xllm_runtime *pRuntime,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sFormat,
    ...
)
{
    char sBuffer[512];
    va_list tArgs;

    if ( !pRuntime || !pRuntime->tOptions.pfnLog || !sComponent || !sFormat ) {
        return;
    }

    va_start(tArgs, sFormat);
    (void)vsnprintf(sBuffer, sizeof(sBuffer), sFormat, tArgs);
    va_end(tArgs);
    sBuffer[sizeof(sBuffer) - 1u] = '\0';

    pRuntime->tOptions.pfnLog(
        pRuntime->tOptions.pLogCtx,
        eLevel,
        sComponent,
        sBuffer
    );
}

static const char *xllm__openai_family_adapter_name(const xllm_profile *pProfile)
{
    if ( pProfile && pProfile->sAdapter ) {
        if ( strcmp(pProfile->sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
            return XLLM_ADAPTER_GLM_NATIVE;
        }
        if ( strcmp(pProfile->sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
            return XLLM_ADAPTER_MINIMAX_NATIVE;
        }
        if ( strcmp(pProfile->sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
            return XLLM_ADAPTER_KIMI_NATIVE;
        }
        if ( strcmp(pProfile->sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
            return XLLM_ADAPTER_QWEN_NATIVE;
        }
        if ( strcmp(pProfile->sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
            return XLLM_ADAPTER_DOUBAO_NATIVE;
        }
    }
    return XLLM_ADAPTER_OPENAI_COMPAT;
}

static const char *xllm__openai_family_component_name(const xllm_profile *pProfile)
{
    const char *sAdapter = xllm__openai_family_adapter_name(pProfile);

    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return "xllm.glm_native";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return "xllm.minimax_native";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return "xllm.kimi_native";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return "xllm.qwen_native";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return "xllm.doubao_native";
    }
    return "xllm.openai_compat";
}

static bool xllm__openai_family_uses_reasoning_content(const xllm_profile *pProfile)
{
    const char *sAdapter = xllm__openai_family_adapter_name(pProfile);

    return (
        strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ||
        strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ||
        strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ||
        strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0
    );
}

static bool xllm__openai_reasoning_vendor_is_reasoning_content(xvalue tVendorExtra)
{
    const char *sField;

    if ( !tVendorExtra || xvoType(tVendorExtra) != XVO_DT_TABLE ) {
        return false;
    }

    sField = (const char *)xvoTableGetText(tVendorExtra, (str)"openai_reasoning_field", 0u);
    return (sField && strcmp(sField, "reasoning_content") == 0);
}

static xvalue xllm__openai_create_reasoning_vendor_extra(const xllm_profile *pProfile)
{
    xvalue tTable = xvoCreateTable();
    const char *sAdapter = xllm__openai_family_adapter_name(pProfile);

    if ( !tTable ) {
        return NULL;
    }

    if ( !xvoTableSetText(tTable, (str)"openai_reasoning_field", 0u, (str)"reasoning_content", 0u, FALSE) ) {
        xvoUnref(tTable);
        return NULL;
    }
    if ( sAdapter && sAdapter[0] ) {
        if ( !xvoTableSetText(tTable, (str)"openai_family_adapter", 0u, (str)sAdapter, 0u, FALSE) ) {
            xvoUnref(tTable);
            return NULL;
        }
    }

    return tTable;
}

static int xllm__openai_set_reasoning_vendor_extra(xllm_output_thinking *pThinking, const xllm_profile *pProfile)
{
    xvalue tVendorExtra;

    if ( !pThinking || !xllm__openai_family_uses_reasoning_content(pProfile) ) {
        return XRT_NET_OK;
    }
    if ( xllm__openai_reasoning_vendor_is_reasoning_content(pThinking->tVendorExtra) ) {
        return XRT_NET_OK;
    }

    tVendorExtra = xllm__openai_create_reasoning_vendor_extra(pProfile);
    if ( !tVendorExtra ) {
        return XRT_NET_ERROR;
    }

    xllm__xvalue_release(&pThinking->tVendorExtra);
    pThinking->tVendorExtra = tVendorExtra;
    return XRT_NET_OK;
}

static const char *xllm__openai_message_reasoning_content(const xllm_message *pMessage)
{
    const char *sReasoningText;

    if ( !pMessage || !xllm__openai_reasoning_vendor_is_reasoning_content(pMessage->tVendorExtra) ) {
        return NULL;
    }

    sReasoningText = (const char *)xvoTableGetText(pMessage->tVendorExtra, (str)"reasoning_content", 0u);
    return (sReasoningText && sReasoningText[0]) ? sReasoningText : NULL;
}

static const char *xllm__openai_response_status_name(xllm_response_status eStatus)
{
    switch ( eStatus ) {
        case XLLM_STATUS_COMPLETED:
            return "completed";
        case XLLM_STATUS_INCOMPLETE:
            return "incomplete";
        case XLLM_STATUS_TOOL_CALL_REQUIRED:
            return "tool_call_required";
        case XLLM_STATUS_REFUSED:
            return "refused";
        case XLLM_STATUS_CONTENT_FILTERED:
            return "content_filtered";
        case XLLM_STATUS_CANCELLED:
            return "cancelled";
        case XLLM_STATUS_ERRORED:
            return "errored";
        default:
            return "unknown";
    }
}

static const char *xllm__openai_error_code_name(xllm_error_code eCode)
{
    switch ( eCode ) {
        case XLLM_ERROR_NONE:
            return "none";
        case XLLM_ERROR_AUTH:
            return "auth";
        case XLLM_ERROR_QUOTA:
            return "quota";
        case XLLM_ERROR_RATE_LIMIT:
            return "rate_limit";
        case XLLM_ERROR_TIMEOUT:
            return "timeout";
        case XLLM_ERROR_NETWORK:
            return "network";
        case XLLM_ERROR_CANCELLED:
            return "cancelled";
        case XLLM_ERROR_INVALID_REQUEST:
            return "invalid_request";
        case XLLM_ERROR_UNSUPPORTED_CAPABILITY:
            return "unsupported_capability";
        case XLLM_ERROR_UNSUPPORTED_INPUT_TYPE:
            return "unsupported_input_type";
        case XLLM_ERROR_UNSUPPORTED_MIME_TYPE:
            return "unsupported_mime_type";
        case XLLM_ERROR_INPUT_TOO_LARGE:
            return "input_too_large";
        case XLLM_ERROR_TOO_MANY_INPUT_PARTS:
            return "too_many_input_parts";
        case XLLM_ERROR_MISSING_MULTIMODAL_MODEL:
            return "missing_multimodal_model";
        case XLLM_ERROR_MODEL_NOT_FOUND:
            return "model_not_found";
        case XLLM_ERROR_UPSTREAM_4XX:
            return "upstream_4xx";
        case XLLM_ERROR_UPSTREAM_5XX:
            return "upstream_5xx";
        case XLLM_ERROR_PARSE:
            return "parse";
        case XLLM_ERROR_INTERNAL:
            return "internal";
        case XLLM_ERROR_SESSION_CONTEXT_OVERFLOW:
            return "session_context_overflow";
        case XLLM_ERROR_SESSION_COMPACT_FAILED:
            return "session_compact_failed";
        case XLLM_ERROR_SESSION_SUMMARY_FAILED:
            return "session_summary_failed";
        case XLLM_ERROR_SESSION_REQUIRES_MODEL_LIMITS:
            return "session_requires_model_limits";
        default:
            return "unknown";
    }
}

static void xllm__openai_trace_request(
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
    xllm__openai_trace_table_set_text(tPayload, "adapter", xllm__openai_family_adapter_name(pProfile));
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

static void xllm__openai_trace_response(
    xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
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
    xllm__openai_trace_table_set_text(tPayload, "adapter", xllm__openai_family_adapter_name(pProfile));
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
        } else if ( pError && pError->sSelectedModel ) {
            xllm__openai_trace_table_set_text(tPayload, "model", pError->sSelectedModel);
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

static void xllm__openai_trace_stream(
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
    xllm__openai_trace_table_set_text(tPayload, "adapter", xllm__openai_family_adapter_name(pCtx->pProfile));
    xllm__openai_trace_table_set_bool(tPayload, "streaming", true);
    xllm__openai_trace_table_set_u32(tPayload, "payload_bytes", (uint32)iPayloadBytes);
    xllm__openai_trace_table_set_u32(tPayload, "payload_count", pCtx->uPayloadCount);
    xllm__openai_trace_table_set_u32(tPayload, "text_delta_count", pCtx->uTextDeltaCount);
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

static bool xllm__openai_error_is_retryable(xllm_error_code eCode)
{
    switch ( eCode ) {
        case XLLM_ERROR_TIMEOUT:
        case XLLM_ERROR_NETWORK:
        case XLLM_ERROR_RATE_LIMIT:
        case XLLM_ERROR_UPSTREAM_5XX:
            return true;
        default:
            return false;
    }
}

static uint32 xllm__openai_retry_delay_ms(const xllm_call_options *pOptions, uint32 uRetryIndex)
{
    uint32 uBaseMs = 200u;
    uint32 uMaxMs = 2000u;
    double fJitter = 0.0;
    uint64 uDelay;

    if ( pOptions ) {
        if ( pOptions->uRetryBackoffBaseMs > 0u ) {
            uBaseMs = pOptions->uRetryBackoffBaseMs;
        }
        if ( pOptions->uRetryBackoffMaxMs > 0u ) {
            uMaxMs = pOptions->uRetryBackoffMaxMs;
        }
        if ( pOptions->fRetryJitter > 0.0 ) {
            fJitter = pOptions->fRetryJitter;
        }
    }

    if ( uBaseMs == 0u ) {
        uBaseMs = 1u;
    }
    if ( uMaxMs > 0u && uBaseMs > uMaxMs ) {
        uBaseMs = uMaxMs;
    }

    uDelay = (uint64)uBaseMs;
    if ( uRetryIndex > 1u ) {
        uint32 uShift = uRetryIndex - 1u;
        if ( uShift > 20u ) {
            uShift = 20u;
        }
        uDelay <<= uShift;
    }
    if ( uMaxMs > 0u && uDelay > (uint64)uMaxMs ) {
        uDelay = (uint64)uMaxMs;
    }

    if ( fJitter > 0.0 && uDelay > 0u ) {
        double fRandom = (double)(rand() & 0x7fff) / 32767.0;
        double fFactor = 1.0 - fJitter + (2.0 * fJitter * fRandom);
        if ( fFactor < 0.0 ) {
            fFactor = 0.0;
        }
        uDelay = (uint64)((double)uDelay * fFactor);
        if ( uDelay == 0u ) {
            uDelay = 1u;
        }
    }

    return (uint32)uDelay;
}

static void xllm__openai_retry_sleep(uint32 uDelayMs)
{
    if ( uDelayMs > 0u ) {
        xrtSleep(uDelayMs);
    }
}

static void xllm__openai_stream_reset_attempt_state(xllm__openai_stream_context *pCtx)
{
    if ( !pCtx ) {
        return;
    }

    if ( pCtx->pResponse ) {
        xllm_response_free(pCtx->pResponse);
        pCtx->pResponse = NULL;
    }
    if ( pCtx->pToolOutputIndices ) {
        xrtFree(pCtx->pToolOutputIndices);
        pCtx->pToolOutputIndices = NULL;
    }
    pCtx->iToolOutputIndexCount = 0u;
    pCtx->iToolOutputIndexCapacity = 0u;
    pCtx->iOutputCapacity = 0u;
    pCtx->iParsedBytes = 0u;
    pCtx->iMessageOutputIndex = (size_t)-1;
    pCtx->iThinkingOutputIndex = (size_t)-1;
    pCtx->iRefusalOutputIndex = (size_t)-1;
    pCtx->uPayloadCount = 0u;
    pCtx->uTextDeltaCount = 0u;
    pCtx->uThinkingDeltaCount = 0u;
    pCtx->uToolDeltaCount = 0u;
    pCtx->uUsageCount = 0u;
    pCtx->uRefusalCount = 0u;
    pCtx->bStartEmitted = false;
    pCtx->bMessageOutputClosed = false;
    pCtx->bCancelled = false;
    pCtx->bDone = false;
}

static bool xllm__openai_request_uses_multimodal(const xllm_request *pRequest)
{
    size_t i;

    if ( !pRequest ) {
        return false;
    }

    for ( i = 0; i < pRequest->iMessageCount; ++i ) {
        size_t j;
        for ( j = 0; j < pRequest->pMessages[i].iPartCount; ++j ) {
            switch ( pRequest->pMessages[i].pParts[j].eKind ) {
                case XLLM_PART_IMAGE:
                case XLLM_PART_FILE:
                case XLLM_PART_AUDIO:
                case XLLM_PART_VIDEO:
                    return true;
                default:
                    break;
            }
        }
    }

    for ( i = 0; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            size_t k;
            for ( k = 0; k < pRequest->pContextBlocks[i].pMessages[j].iPartCount; ++k ) {
                switch ( pRequest->pContextBlocks[i].pMessages[j].pParts[k].eKind ) {
                    case XLLM_PART_IMAGE:
                    case XLLM_PART_FILE:
                    case XLLM_PART_AUDIO:
                    case XLLM_PART_VIDEO:
                        return true;
                    default:
                        break;
                }
            }
        }
    }

    return false;
}

static const char *xllm__openai_role_name(xllm_role eRole)
{
    switch ( eRole ) {
        case XLLM_ROLE_SYSTEM:
            return "system";
        case XLLM_ROLE_USER:
            return "user";
        case XLLM_ROLE_ASSISTANT:
            return "assistant";
        case XLLM_ROLE_TOOL:
            return "tool";
        default:
            return "user";
    }
}

static char xllm__ascii_lower(char ch)
{
    if ( ch >= 'A' && ch <= 'Z' ) {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

static bool xllm__text_contains_ci(const char *sText, const char *sNeedle)
{
    size_t i;
    size_t iNeedleLen;

    if ( !sText || !sNeedle || !sNeedle[0] ) {
        return false;
    }

    iNeedleLen = strlen(sNeedle);
    for ( i = 0; sText[i]; ++i ) {
        size_t j = 0u;
        while ( j < iNeedleLen && sText[i + j] &&
                xllm__ascii_lower(sText[i + j]) == xllm__ascii_lower(sNeedle[j]) ) {
            ++j;
        }
        if ( j == iNeedleLen ) {
            return true;
        }
    }

    return false;
}

static bool xllm__buffer_starts_with_sse_data(const char *sBuffer, size_t iLen)
{
    size_t i = 0u;

    if ( !sBuffer ) {
        return false;
    }

    while ( i < iLen ) {
        char ch = sBuffer[i];
        if ( ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ) {
            ++i;
            continue;
        }
        if ( i + 5u <= iLen && memcmp(sBuffer + i, "data:", 5u) == 0 ) {
            return true;
        }
        break;
    }

    return false;
}

static bool xllm__header_name_eq_ci(const char *sLeft, const char *sRight)
{
    size_t i = 0u;

    if ( !sLeft || !sRight ) {
        return false;
    }

    while ( sLeft[i] && sRight[i] ) {
        if ( xllm__ascii_lower(sLeft[i]) != xllm__ascii_lower(sRight[i]) ) {
            return false;
        }
        ++i;
    }

    return sLeft[i] == '\0' && sRight[i] == '\0';
}

static bool xllm__http_request_has_header(const xhttprequest *pRequest, const char *sName)
{
    uint32 i;

    if ( !pRequest || !sName ) {
        return false;
    }

    for ( i = 0; i < pRequest->iHeaderCount; ++i ) {
        if ( xllm__header_name_eq_ci(pRequest->arrHeaders[i].sName, sName) ) {
            return true;
        }
    }

    return false;
}

static bool xllm__http_make_host_header(const xhttprequest *pRequest, char *sOut, size_t iOutCap)
{
    bool bDefaultPort;
    int iLen;

    if ( !pRequest || !sOut || iOutCap == 0u || !pRequest->tURL.sHost[0] ) {
        return false;
    }

    bDefaultPort = (pRequest->tURL.bHttps && pRequest->tURL.iPort == 443u) ||
                   (!pRequest->tURL.bHttps && pRequest->tURL.iPort == 80u);

    if ( bDefaultPort || pRequest->tURL.iPort == 0u ) {
        iLen = snprintf(sOut, iOutCap, "%s", pRequest->tURL.sHost);
    } else {
        iLen = snprintf(sOut, iOutCap, "%s:%u", pRequest->tURL.sHost, (unsigned)pRequest->tURL.iPort);
    }

    return iLen > 0 && (size_t)iLen < iOutCap;
}

static bool xllm__openai_build_http_request_bytes(const xhttprequest *pRequest, char **ppOut, size_t *pOutLen)
{
    xllm__json_builder tBuilder;
    char sLine[512];
    char sHostHeader[384];
    bool bChunked;
    uint32 i;
    int iLen;

    if ( !pRequest || !ppOut || !pOutLen || !pRequest->tURL.sHost[0] || !pRequest->sMethod[0] ) {
        return false;
    }

    *ppOut = NULL;
    *pOutLen = 0u;
    memset(&tBuilder, 0, sizeof(tBuilder));
    bChunked = false;
    for ( i = 0; i < pRequest->iHeaderCount; ++i ) {
        if ( xllm__header_name_eq_ci(pRequest->arrHeaders[i].sName, "Transfer-Encoding") &&
             xllm__text_contains_ci(pRequest->arrHeaders[i].sValue, "chunked") ) {
            bChunked = true;
            break;
        }
    }

    iLen = snprintf(
        sLine,
        sizeof(sLine),
        "%s %s HTTP/1.1\r\n",
        pRequest->sMethod,
        pRequest->tURL.sPath[0] ? pRequest->tURL.sPath : "/"
    );
    if ( iLen <= 0 || !xllm__json_builder_append_bytes(&tBuilder, sLine, (size_t)iLen) ) {
        goto fail;
    }

    if ( !xllm__http_request_has_header(pRequest, "Host") ) {
        if ( !xllm__http_make_host_header(pRequest, sHostHeader, sizeof(sHostHeader)) ) {
            goto fail;
        }
        iLen = snprintf(sLine, sizeof(sLine), "Host: %s\r\n", sHostHeader);
        if ( iLen <= 0 || !xllm__json_builder_append_bytes(&tBuilder, sLine, (size_t)iLen) ) {
            goto fail;
        }
    }

    if ( !xllm__http_request_has_header(pRequest, "Connection") ) {
        if ( !xllm__json_builder_append_cstr(&tBuilder, "Connection: close\r\n") ) {
            goto fail;
        }
    }

    if ( !bChunked &&
         pRequest->iBodyLen > 0u &&
         !xllm__http_request_has_header(pRequest, "Content-Length") ) {
        iLen = snprintf(sLine, sizeof(sLine), "Content-Length: %llu\r\n", (unsigned long long)pRequest->iBodyLen);
        if ( iLen <= 0 || !xllm__json_builder_append_bytes(&tBuilder, sLine, (size_t)iLen) ) {
            goto fail;
        }
    }

    for ( i = 0; i < pRequest->iHeaderCount; ++i ) {
        if ( bChunked && xllm__header_name_eq_ci(pRequest->arrHeaders[i].sName, "Content-Length") ) {
            continue;
        }
        iLen = snprintf(
            sLine,
            sizeof(sLine),
            "%s: %s\r\n",
            pRequest->arrHeaders[i].sName,
            pRequest->arrHeaders[i].sValue
        );
        if ( iLen <= 0 || !xllm__json_builder_append_bytes(&tBuilder, sLine, (size_t)iLen) ) {
            goto fail;
        }
    }

    if ( !xllm__json_builder_append_cstr(&tBuilder, "\r\n") ) {
        goto fail;
    }

    if ( bChunked ) {
        iLen = snprintf(sLine, sizeof(sLine), "%llX\r\n", (unsigned long long)pRequest->iBodyLen);
        if ( iLen <= 0 || !xllm__json_builder_append_bytes(&tBuilder, sLine, (size_t)iLen) ) {
            goto fail;
        }
        if ( pRequest->pBody && pRequest->iBodyLen > 0u &&
             !xllm__json_builder_append_bytes(&tBuilder, pRequest->pBody, pRequest->iBodyLen) ) {
            goto fail;
        }
        if ( !xllm__json_builder_append_cstr(&tBuilder, "\r\n0\r\n\r\n") ) {
            goto fail;
        }
    } else if ( pRequest->pBody && pRequest->iBodyLen > 0u ) {
        if ( !xllm__json_builder_append_bytes(&tBuilder, pRequest->pBody, pRequest->iBodyLen) ) {
            goto fail;
        }
    }

    *pOutLen = tBuilder.iLen;
    *ppOut = xllm__json_builder_detach(&tBuilder);
    if ( !*ppOut ) {
        goto fail;
    }
    return true;

fail:
    xllm__json_builder_reset(&tBuilder);
    return false;
}

static void xllm__openai_live_transport_signal(xllm__openai_live_transport *pTransport)
{
    if ( pTransport && pTransport->pCond ) {
        xrtCondSignal(pTransport->pCond);
    }
}

static void xllm__openai_live_transport_fail(xllm__openai_live_transport *pTransport, int iSysErr)
{
    if ( !pTransport ) {
        return;
    }

    if ( pTransport->pMutex ) {
        xrtMutexLock(pTransport->pMutex);
    }
    if ( pTransport->iSysErr == 0 ) {
        pTransport->iSysErr = iSysErr ? iSysErr : -1;
    }
    xllm__openai_live_transport_signal(pTransport);
    if ( pTransport->pMutex ) {
        xrtMutexUnlock(pTransport->pMutex);
    }
}

static void xllm__openai_live_on_open(ptr pOwner, xnetstream *pStream)
{
    xllm__openai_live_transport *pTransport = (xllm__openai_live_transport *)pOwner;

    if ( !pTransport || !pStream ) {
        return;
    }

    if ( xrtNetStreamSend(pStream, pTransport->pRequestBytes, pTransport->iRequestLen) != XRT_NET_OK ) {
        xllm__openai_live_transport_fail(pTransport, -1);
        xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
        return;
    }

    if ( pTransport->pMutex ) {
        xrtMutexLock(pTransport->pMutex);
    }
    xllm__openai_live_transport_signal(pTransport);
    if ( pTransport->pMutex ) {
        xrtMutexUnlock(pTransport->pMutex);
    }
}

static void xllm__openai_live_on_recv(ptr pOwner, xnetstream *pStream, xnetchain *pChain)
{
    xllm__openai_live_transport *pTransport = (xllm__openai_live_transport *)pOwner;
    size_t iBytes;

    (void)pStream;

    if ( !pTransport || !pChain ) {
        return;
    }

    iBytes = xrtNetChainBytes(pChain);
    if ( iBytes == 0u ) {
        return;
    }

    if ( pTransport->pMutex ) {
        xrtMutexLock(pTransport->pMutex);
    }
    if ( !xllm__json_builder_reserve(&pTransport->tIncoming, iBytes) ) {
        if ( pTransport->iSysErr == 0 ) {
            pTransport->iSysErr = -1;
        }
        xllm__openai_live_transport_signal(pTransport);
        if ( pTransport->pMutex ) {
            xrtMutexUnlock(pTransport->pMutex);
        }
        xrtNetChainConsume(pChain, iBytes);
        xrtNetStreamClose(pTransport->pStream, XNET_CLOSE_F_ABORT);
        return;
    }

    if ( xrtNetChainPeek(pChain, pTransport->tIncoming.pData + pTransport->tIncoming.iLen, iBytes) != iBytes ) {
        if ( pTransport->iSysErr == 0 ) {
            pTransport->iSysErr = -1;
        }
        xllm__openai_live_transport_signal(pTransport);
        if ( pTransport->pMutex ) {
            xrtMutexUnlock(pTransport->pMutex);
        }
        xrtNetChainConsume(pChain, iBytes);
        xrtNetStreamClose(pTransport->pStream, XNET_CLOSE_F_ABORT);
        return;
    }

    pTransport->tIncoming.iLen += iBytes;
    pTransport->tIncoming.pData[pTransport->tIncoming.iLen] = '\0';
    xrtNetChainConsume(pChain, iBytes);
    xllm__openai_live_transport_signal(pTransport);
    if ( pTransport->pMutex ) {
        xrtMutexUnlock(pTransport->pMutex);
    }
}

static void xllm__openai_live_on_close(ptr pOwner, xnetstream *pStream, xnet_result iReason)
{
    xllm__openai_live_transport *pTransport = (xllm__openai_live_transport *)pOwner;

    (void)pStream;

    if ( !pTransport ) {
        return;
    }

    if ( pTransport->pMutex ) {
        xrtMutexLock(pTransport->pMutex);
    }
    pTransport->bClosed = true;
    pTransport->iCloseReason = iReason;
    xllm__openai_live_transport_signal(pTransport);
    if ( pTransport->pMutex ) {
        xrtMutexUnlock(pTransport->pMutex);
    }
}

static void xllm__openai_live_on_error(ptr pOwner, xnetstream *pStream, int iSysErr)
{
    xllm__openai_live_transport *pTransport = (xllm__openai_live_transport *)pOwner;

    (void)pStream;
    xllm__openai_live_transport_fail(pTransport, iSysErr);
}

static const xnetstreamevents *xllm__openai_live_stream_events(void)
{
    static const xnetstreamevents tEvents = {
        xllm__openai_live_on_open,
        xllm__openai_live_on_recv,
        NULL,
        xllm__openai_live_on_close,
        xllm__openai_live_on_error,
        NULL,
        NULL
    };

    return &tEvents;
}

static void xllm__openai_live_transport_init(xllm__openai_live_transport *pTransport)
{
    if ( !pTransport ) {
        return;
    }

    memset(pTransport, 0, sizeof(*pTransport));
}

static void xllm__openai_live_transport_reset(xllm__openai_live_transport *pTransport)
{
    if ( !pTransport ) {
        return;
    }

    if ( pTransport->pStream ) {
        xrtNetStreamDestroy(pTransport->pStream);
    }
    if ( pTransport->pCond ) {
        xrtCondDestroy(pTransport->pCond);
    }
    if ( pTransport->pMutex ) {
        xrtMutexDestroy(pTransport->pMutex);
    }
    if ( pTransport->pRequestBytes ) {
        xrtFree(pTransport->pRequestBytes);
    }
    xllm__json_builder_reset(&pTransport->tIncoming);
    memset(pTransport, 0, sizeof(*pTransport));
}

static void xllm__openai_live_transport_close_and_wait(
    xllm__openai_live_transport *pTransport,
    xnet_result iReason,
    uint32 uWaitMs
)
{
    uint32 uRemainMs;

    if ( !pTransport || !pTransport->pStream ) {
        return;
    }

    xrtNetStreamClose(pTransport->pStream, iReason);
    if ( !pTransport->pMutex || !pTransport->pCond || pTransport->bClosed ) {
        return;
    }

    uRemainMs = uWaitMs ? uWaitMs : 100u;
    xrtMutexLock(pTransport->pMutex);
    while ( !pTransport->bClosed && uRemainMs > 0u ) {
        uint32 uStepMs = uRemainMs > 10u ? 10u : uRemainMs;
        (void)xrtCondWaitTimeout(pTransport->pCond, pTransport->pMutex, uStepMs);
        if ( uRemainMs > uStepMs ) {
            uRemainMs -= uStepMs;
        } else {
            uRemainMs = 0u;
        }
    }
    xrtMutexUnlock(pTransport->pMutex);
}

static bool xllm__openai_live_transport_take_incoming(
    xllm__openai_live_transport *pTransport,
    char **ppChunk,
    size_t *pChunkLen,
    bool *pbClosed,
    xnet_result *pCloseReason,
    int *piSysErr
)
{
    bool bHasData = false;
    size_t iChunkLen = 0u;

    if ( ppChunk ) {
        *ppChunk = NULL;
    }
    if ( pChunkLen ) {
        *pChunkLen = 0u;
    }
    if ( pbClosed ) {
        *pbClosed = false;
    }
    if ( pCloseReason ) {
        *pCloseReason = XRT_NET_OK;
    }
    if ( piSysErr ) {
        *piSysErr = 0;
    }

    if ( !pTransport || !pTransport->pMutex ) {
        return false;
    }

    xrtMutexLock(pTransport->pMutex);
    if ( pTransport->tIncoming.iLen > 0u && ppChunk && pChunkLen ) {
        iChunkLen = pTransport->tIncoming.iLen;
        *ppChunk = xllm__json_builder_detach(&pTransport->tIncoming);
        *pChunkLen = iChunkLen;
        bHasData = (*ppChunk != NULL && iChunkLen > 0u);
    }
    if ( pbClosed ) {
        *pbClosed = pTransport->bClosed;
    }
    if ( pCloseReason ) {
        *pCloseReason = pTransport->iCloseReason;
    }
    if ( piSysErr ) {
        *piSysErr = pTransport->iSysErr;
    }
    xrtMutexUnlock(pTransport->pMutex);
    return bHasData;
}

static void xllm__openai_live_decoder_init(xllm__openai_live_decoder *pDecoder)
{
    if ( !pDecoder ) {
        return;
    }

    memset(pDecoder, 0, sizeof(*pDecoder));
    pDecoder->iContentLength = -1;
    pDecoder->eBodyMode = XLLM__HTTP_BODY_MODE_UNSET;
    pDecoder->eChunkState = XLLM__HTTP_CHUNK_STATE_SIZE;
}

static void xllm__openai_live_decoder_reset(xllm__openai_live_decoder *pDecoder)
{
    if ( !pDecoder ) {
        return;
    }

    xllm__json_builder_reset(&pDecoder->tWire);
    xllm__json_builder_reset(&pDecoder->tBody);
    memset(pDecoder, 0, sizeof(*pDecoder));
}

static bool xllm__http_find_header_delimiter(const char *sBuffer, size_t iLen, size_t *piEnd, size_t *piDelimLen)
{
    size_t i;

    if ( piEnd ) {
        *piEnd = 0u;
    }
    if ( piDelimLen ) {
        *piDelimLen = 0u;
    }
    if ( !sBuffer ) {
        return false;
    }

    for ( i = 0u; i + 1u < iLen; ++i ) {
        if ( sBuffer[i] == '\n' && sBuffer[i + 1u] == '\n' ) {
            if ( piEnd ) {
                *piEnd = i;
            }
            if ( piDelimLen ) {
                *piDelimLen = 2u;
            }
            return true;
        }
        if ( i + 3u < iLen &&
             sBuffer[i] == '\r' &&
             sBuffer[i + 1u] == '\n' &&
             sBuffer[i + 2u] == '\r' &&
             sBuffer[i + 3u] == '\n' ) {
            if ( piEnd ) {
                *piEnd = i;
            }
            if ( piDelimLen ) {
                *piDelimLen = 4u;
            }
            return true;
        }
    }

    return false;
}

static void xllm__copy_trimmed_text(char *sOut, size_t iOutCap, const char *sText, size_t iLen)
{
    size_t iStart = 0u;

    if ( !sOut || iOutCap == 0u ) {
        return;
    }

    while ( iStart < iLen && (sText[iStart] == ' ' || sText[iStart] == '\t') ) {
        ++iStart;
    }
    while ( iLen > iStart && (sText[iLen - 1u] == ' ' || sText[iLen - 1u] == '\t' || sText[iLen - 1u] == '\r') ) {
        --iLen;
    }

    if ( iLen <= iStart ) {
        sOut[0] = '\0';
        return;
    }
    if ( iLen - iStart >= iOutCap ) {
        iLen = iStart + iOutCap - 1u;
    }
    memcpy(sOut, sText + iStart, iLen - iStart);
    sOut[iLen - iStart] = '\0';
}

static int xllm__parse_http_status_code(const char *sLine)
{
    const char *sSpace;

    if ( !sLine ) {
        return 0;
    }

    sSpace = strchr(sLine, ' ');
    if ( !sSpace ) {
        return 0;
    }

    return atoi(sSpace + 1);
}

static int xllm__parse_hex_size(const char *sText, size_t iLen, size_t *piOut)
{
    size_t i;
    size_t iValue = 0u;
    bool bSawDigit = false;

    if ( piOut ) {
        *piOut = 0u;
    }
    if ( !sText || !piOut ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < iLen; ++i ) {
        unsigned char ch = (unsigned char)sText[i];
        unsigned char uHex;

        if ( ch == ';' || ch == ' ' || ch == '\t' || ch == '\r' ) {
            break;
        }

        if ( ch >= '0' && ch <= '9' ) {
            uHex = (unsigned char)(ch - '0');
        } else {
            ch = (unsigned char)xllm__ascii_lower((char)ch);
            if ( ch < 'a' || ch > 'f' ) {
                return XRT_NET_ERROR;
            }
            uHex = (unsigned char)(10u + (ch - 'a'));
        }

        bSawDigit = true;
        if ( iValue > (((size_t)-1) >> 4u) ) {
            return XRT_NET_ERROR;
        }
        iValue = (iValue << 4u) | (size_t)uHex;
    }

    if ( !bSawDigit ) {
        return XRT_NET_ERROR;
    }

    *piOut = iValue;
    return XRT_NET_OK;
}

static int xllm__openai_live_try_parse_headers(xllm__openai_live_decoder *pDecoder, bool bStreamClosed)
{
    size_t iHeaderEnd = 0u;
    size_t iDelimiterLen = 0u;
    char *sHeaders = NULL;
    char *sCursor;
    char *sLine;

    if ( !pDecoder || pDecoder->bHeadersParsed ) {
        return XRT_NET_OK;
    }

    if ( !xllm__http_find_header_delimiter(pDecoder->tWire.pData, pDecoder->tWire.iLen, &iHeaderEnd, &iDelimiterLen) ) {
        return bStreamClosed ? XRT_NET_ERROR : XRT_NET_AGAIN;
    }

    sHeaders = (char *)xrtCalloc(iHeaderEnd + 1u, sizeof(char));
    if ( !sHeaders ) {
        return XRT_NET_ERROR;
    }

    memcpy(sHeaders, pDecoder->tWire.pData, iHeaderEnd);
    sCursor = sHeaders;
    sLine = sCursor;
    while ( sCursor && *sCursor ) {
        char *sNext = strchr(sCursor, '\n');
        size_t iLineLen;

        if ( sNext ) {
            *sNext = '\0';
        }
        iLineLen = strlen(sCursor);
        if ( iLineLen > 0u && sCursor[iLineLen - 1u] == '\r' ) {
            sCursor[iLineLen - 1u] = '\0';
        }

        if ( sCursor == sLine ) {
            pDecoder->uStatusCode = (uint32)xllm__parse_http_status_code(sCursor);
        } else if ( sCursor[0] ) {
            char *sColon = strchr(sCursor, ':');
            if ( sColon ) {
                char sName[64];
                char sValue[256];

                *sColon = '\0';
                xllm__copy_trimmed_text(sName, sizeof(sName), sCursor, strlen(sCursor));
                xllm__copy_trimmed_text(sValue, sizeof(sValue), sColon + 1, strlen(sColon + 1));

                if ( xllm__header_name_eq_ci(sName, "Content-Type") ) {
                    xllm__copy_trimmed_text(
                        pDecoder->sContentType,
                        sizeof(pDecoder->sContentType),
                        sValue,
                        strlen(sValue)
                    );
                } else if ( xllm__header_name_eq_ci(sName, "Content-Length") ) {
                    pDecoder->iContentLength = (int64_t)strtoll(sValue, NULL, 10);
                } else if ( xllm__header_name_eq_ci(sName, "Transfer-Encoding") &&
                            xllm__text_contains_ci(sValue, "chunked") ) {
                    pDecoder->eBodyMode = XLLM__HTTP_BODY_MODE_CHUNKED;
                } else if ( xllm__header_name_eq_ci(sName, "x-request-id") ||
                            xllm__header_name_eq_ci(sName, "request-id") ) {
                    xllm__copy_trimmed_text(
                        pDecoder->sRequestId,
                        sizeof(pDecoder->sRequestId),
                        sValue,
                        strlen(sValue)
                    );
                }
            }
        }

        if ( !sNext ) {
            break;
        }
        sCursor = sNext + 1;
    }

    if ( pDecoder->uStatusCode == 0u ) {
        xrtFree(sHeaders);
        return XRT_NET_ERROR;
    }

    if ( pDecoder->eBodyMode == XLLM__HTTP_BODY_MODE_UNSET ) {
        if ( pDecoder->iContentLength >= 0 ) {
            pDecoder->eBodyMode = XLLM__HTTP_BODY_MODE_CONTENT_LENGTH;
        } else {
            pDecoder->eBodyMode = XLLM__HTTP_BODY_MODE_UNTIL_CLOSE;
        }
    }

    pDecoder->bHeadersParsed = true;
    pDecoder->iHeaderBytes = iHeaderEnd + iDelimiterLen;
    pDecoder->iParseOffset = pDecoder->iHeaderBytes;
    pDecoder->bTreatAsSse = xllm__text_contains_ci(pDecoder->sContentType, "text/event-stream");
    xrtFree(sHeaders);
    return XRT_NET_OK;
}

static int xllm__openai_live_append_body(
    xllm__openai_live_decoder *pDecoder,
    xllm__openai_stream_context *pStream,
    const char *sData,
    size_t iLen
)
{
    if ( !pDecoder || !pStream || !sData || iLen == 0u ) {
        return XRT_NET_OK;
    }

    if ( !xllm__json_builder_append_bytes(&pDecoder->tBody, sData, iLen) ) {
        return XRT_NET_ERROR;
    }

    if ( !pDecoder->bTreatAsSse ) {
        pDecoder->bTreatAsSse = xllm__buffer_starts_with_sse_data(pDecoder->tBody.pData, pDecoder->tBody.iLen);
    }

    if ( pDecoder->bTreatAsSse ) {
        return xllm__openai_stream_process_buffer(pStream, pDecoder->tBody.pData, pDecoder->tBody.iLen);
    }

    return XRT_NET_OK;
}

static int xllm__openai_live_process_body(
    xllm__openai_live_decoder *pDecoder,
    xllm__openai_stream_context *pStream,
    bool bStreamClosed
)
{
    if ( !pDecoder || !pDecoder->bHeadersParsed ) {
        return XRT_NET_ERROR;
    }

    if ( pDecoder->eBodyMode == XLLM__HTTP_BODY_MODE_CONTENT_LENGTH ) {
        size_t iRemain;
        size_t iAvailable;
        size_t iCopy;

        if ( pDecoder->iContentLength < 0 ) {
            return XRT_NET_ERROR;
        }

        if ( pDecoder->tBody.iLen >= (size_t)pDecoder->iContentLength ) {
            pDecoder->bBodyComplete = true;
            return XRT_NET_OK;
        }

        iAvailable = pDecoder->tWire.iLen - pDecoder->iParseOffset;
        iRemain = (size_t)pDecoder->iContentLength - pDecoder->tBody.iLen;
        iCopy = (iAvailable < iRemain) ? iAvailable : iRemain;
        if ( iCopy > 0u ) {
            if ( xllm__openai_live_append_body(pDecoder, pStream, pDecoder->tWire.pData + pDecoder->iParseOffset, iCopy) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            pDecoder->iParseOffset += iCopy;
        }
        if ( pDecoder->tBody.iLen >= (size_t)pDecoder->iContentLength ) {
            pDecoder->bBodyComplete = true;
        } else if ( bStreamClosed ) {
            return XRT_NET_ERROR;
        }
        return XRT_NET_OK;
    }

    if ( pDecoder->eBodyMode == XLLM__HTTP_BODY_MODE_UNTIL_CLOSE ) {
        size_t iAvailable = pDecoder->tWire.iLen - pDecoder->iParseOffset;
        if ( iAvailable > 0u ) {
            if ( xllm__openai_live_append_body(pDecoder, pStream, pDecoder->tWire.pData + pDecoder->iParseOffset, iAvailable) != XRT_NET_OK ) {
                return XRT_NET_ERROR;
            }
            pDecoder->iParseOffset += iAvailable;
        }
        pDecoder->bBodyComplete = bStreamClosed;
        return XRT_NET_OK;
    }

    if ( pDecoder->eBodyMode == XLLM__HTTP_BODY_MODE_CHUNKED ) {
        while ( pDecoder->iParseOffset < pDecoder->tWire.iLen && !pDecoder->bBodyComplete ) {
            size_t iAvailable = pDecoder->tWire.iLen - pDecoder->iParseOffset;

            switch ( pDecoder->eChunkState ) {
                case XLLM__HTTP_CHUNK_STATE_SIZE: {
                    const char *sLineStart = pDecoder->tWire.pData + pDecoder->iParseOffset;
                    const char *sLineEnd = memchr(sLineStart, '\n', iAvailable);
                    size_t iChunkSize;
                    size_t iLineLen;

                    if ( !sLineEnd ) {
                        return bStreamClosed ? XRT_NET_ERROR : XRT_NET_OK;
                    }

                    iLineLen = (size_t)(sLineEnd - sLineStart);
                    if ( iLineLen > 0u && sLineStart[iLineLen - 1u] == '\r' ) {
                        --iLineLen;
                    }
                    if ( xllm__parse_hex_size(sLineStart, iLineLen, &iChunkSize) != XRT_NET_OK ) {
                        return XRT_NET_ERROR;
                    }

                    pDecoder->iParseOffset = (size_t)(sLineEnd - pDecoder->tWire.pData) + 1u;
                    pDecoder->iChunkBytesRemaining = iChunkSize;
                    pDecoder->eChunkState = (iChunkSize == 0u)
                        ? XLLM__HTTP_CHUNK_STATE_TRAILERS
                        : XLLM__HTTP_CHUNK_STATE_DATA;
                    break;
                }

                case XLLM__HTTP_CHUNK_STATE_DATA:
                    if ( iAvailable < pDecoder->iChunkBytesRemaining ) {
                        return bStreamClosed ? XRT_NET_ERROR : XRT_NET_OK;
                    }
                    if ( xllm__openai_live_append_body(
                             pDecoder,
                             pStream,
                             pDecoder->tWire.pData + pDecoder->iParseOffset,
                             pDecoder->iChunkBytesRemaining
                         ) != XRT_NET_OK ) {
                        return XRT_NET_ERROR;
                    }
                    pDecoder->iParseOffset += pDecoder->iChunkBytesRemaining;
                    pDecoder->iChunkBytesRemaining = 0u;
                    pDecoder->eChunkState = XLLM__HTTP_CHUNK_STATE_DATA_CRLF;
                    break;

                case XLLM__HTTP_CHUNK_STATE_DATA_CRLF:
                    if ( iAvailable == 0u ) {
                        return bStreamClosed ? XRT_NET_ERROR : XRT_NET_OK;
                    }
                    if ( pDecoder->tWire.pData[pDecoder->iParseOffset] == '\r' ) {
                        if ( iAvailable < 2u || pDecoder->tWire.pData[pDecoder->iParseOffset + 1u] != '\n' ) {
                            return bStreamClosed ? XRT_NET_ERROR : XRT_NET_OK;
                        }
                        pDecoder->iParseOffset += 2u;
                    } else if ( pDecoder->tWire.pData[pDecoder->iParseOffset] == '\n' ) {
                        pDecoder->iParseOffset += 1u;
                    } else {
                        return XRT_NET_ERROR;
                    }
                    pDecoder->eChunkState = XLLM__HTTP_CHUNK_STATE_SIZE;
                    break;

                case XLLM__HTTP_CHUNK_STATE_TRAILERS: {
                    const char *sLineStart = pDecoder->tWire.pData + pDecoder->iParseOffset;
                    const char *sLineEnd = memchr(sLineStart, '\n', iAvailable);
                    size_t iLineLen;

                    if ( !sLineEnd ) {
                        return bStreamClosed ? XRT_NET_ERROR : XRT_NET_OK;
                    }

                    iLineLen = (size_t)(sLineEnd - sLineStart);
                    if ( iLineLen > 0u && sLineStart[iLineLen - 1u] == '\r' ) {
                        --iLineLen;
                    }

                    pDecoder->iParseOffset = (size_t)(sLineEnd - pDecoder->tWire.pData) + 1u;
                    if ( iLineLen == 0u ) {
                        pDecoder->eChunkState = XLLM__HTTP_CHUNK_STATE_DONE;
                        pDecoder->bBodyComplete = true;
                    }
                    break;
                }

                case XLLM__HTTP_CHUNK_STATE_DONE:
                    pDecoder->bBodyComplete = true;
                    break;
            }
        }

        return XRT_NET_OK;
    }

    return XRT_NET_ERROR;
}

static bool xllm__json_builder_reserve(xllm__json_builder *pBuilder, size_t iExtra)
{
    char *pNew;
    size_t iNeed;
    size_t iCap;

    if ( !pBuilder ) {
        return false;
    }

    iNeed = pBuilder->iLen + iExtra + 1u;
    if ( iNeed <= pBuilder->iCap ) {
        return true;
    }

    iCap = pBuilder->iCap ? pBuilder->iCap : 256u;
    while ( iCap < iNeed ) {
        if ( iCap > ((size_t)-1) / 2u ) {
            iCap = iNeed;
            break;
        }
        iCap *= 2u;
    }

    pNew = (char *)xrtRealloc(pBuilder->pData, iCap);
    if ( !pNew ) {
        return false;
    }

    pBuilder->pData = pNew;
    pBuilder->iCap = iCap;
    return true;
}

static bool xllm__json_builder_append_bytes(xllm__json_builder *pBuilder, const void *pData, size_t iLen)
{
    if ( !pBuilder || (!pData && iLen > 0) ) {
        return false;
    }

    if ( iLen == 0 ) {
        return true;
    }

    if ( !xllm__json_builder_reserve(pBuilder, iLen) ) {
        return false;
    }

    memcpy(pBuilder->pData + pBuilder->iLen, pData, iLen);
    pBuilder->iLen += iLen;
    pBuilder->pData[pBuilder->iLen] = '\0';
    return true;
}

static bool xllm__json_builder_append_cstr(xllm__json_builder *pBuilder, const char *sText)
{
    if ( !sText ) {
        return true;
    }
    return xllm__json_builder_append_bytes(pBuilder, sText, strlen(sText));
}

static bool xllm__json_builder_append_char(xllm__json_builder *pBuilder, char ch)
{
    return xllm__json_builder_append_bytes(pBuilder, &ch, 1u);
}

static bool xllm__json_builder_append_u32(xllm__json_builder *pBuilder, uint32 uValue)
{
    char sBuf[32];
    int iLen = snprintf(sBuf, sizeof(sBuf), "%u", (unsigned)uValue);
    if ( iLen < 0 ) {
        return false;
    }
    return xllm__json_builder_append_bytes(pBuilder, sBuf, (size_t)iLen);
}

static bool xllm__json_builder_append_f64(xllm__json_builder *pBuilder, double fValue)
{
    char sBuf[64];
    int iLen = snprintf(sBuf, sizeof(sBuf), "%.17g", fValue);
    if ( iLen < 0 ) {
        return false;
    }
    return xllm__json_builder_append_bytes(pBuilder, sBuf, (size_t)iLen);
}

static bool xllm__json_builder_append_escaped(xllm__json_builder *pBuilder, const char *sText)
{
    size_t i;

    if ( !xllm__json_builder_append_char(pBuilder, '"') ) {
        return false;
    }

    if ( sText ) {
        for ( i = 0; sText[i] != '\0'; ++i ) {
            unsigned char ch = (unsigned char)sText[i];
            switch ( ch ) {
                case '"':
                    if ( !xllm__json_builder_append_cstr(pBuilder, "\\\"") ) return false;
                    break;
                case '\\':
                    if ( !xllm__json_builder_append_cstr(pBuilder, "\\\\") ) return false;
                    break;
                case '\b':
                    if ( !xllm__json_builder_append_cstr(pBuilder, "\\b") ) return false;
                    break;
                case '\f':
                    if ( !xllm__json_builder_append_cstr(pBuilder, "\\f") ) return false;
                    break;
                case '\n':
                    if ( !xllm__json_builder_append_cstr(pBuilder, "\\n") ) return false;
                    break;
                case '\r':
                    if ( !xllm__json_builder_append_cstr(pBuilder, "\\r") ) return false;
                    break;
                case '\t':
                    if ( !xllm__json_builder_append_cstr(pBuilder, "\\t") ) return false;
                    break;
                default:
                    if ( ch < 0x20u ) {
                        char sBuf[7];
                        (void)snprintf(sBuf, sizeof(sBuf), "\\u%04x", (unsigned)ch);
                        if ( !xllm__json_builder_append_cstr(pBuilder, sBuf) ) return false;
                    } else {
                        if ( !xllm__json_builder_append_char(pBuilder, (char)ch) ) return false;
                    }
                    break;
            }
        }
    }

    return xllm__json_builder_append_char(pBuilder, '"');
}

static char *xllm__json_builder_detach(xllm__json_builder *pBuilder)
{
    char *pData;

    if ( !pBuilder ) {
        return NULL;
    }

    if ( !pBuilder->pData ) {
        pData = (char *)xrtCalloc(1, sizeof(char));
        return pData;
    }

    pData = pBuilder->pData;
    pBuilder->pData = NULL;
    pBuilder->iLen = 0;
    pBuilder->iCap = 0;
    return pData;
}

static void xllm__json_builder_reset(xllm__json_builder *pBuilder)
{
    if ( !pBuilder ) {
        return;
    }

    if ( pBuilder->pData ) {
        xrtFree(pBuilder->pData);
    }

    pBuilder->pData = NULL;
    pBuilder->iLen = 0;
    pBuilder->iCap = 0;
}

static xvalue xllm__json_table_get(xvalue pValue, const char *sKey)
{
    if ( !pValue || !sKey || xvoType(pValue) != XVO_DT_TABLE ) {
        return NULL;
    }
    return xvoTableGetValue(pValue, (str)sKey, (uint32)strlen(sKey));
}

static const char *xllm__json_table_get_text(xvalue pValue, const char *sKey)
{
    xvalue pField = xllm__json_table_get(pValue, sKey);
    if ( !pField || xvoType(pField) != XVO_DT_TEXT ) {
        return NULL;
    }
    return (const char *)xvoGetText(pField);
}

static bool xllm__json_table_get_bool(xvalue pValue, const char *sKey, bool *pbOut)
{
    xvalue pField = xllm__json_table_get(pValue, sKey);
    if ( !pField || xvoType(pField) != XVO_DT_BOOL ) {
        return false;
    }
    if ( pbOut ) {
        *pbOut = xvoGetBool(pField);
    }
    return true;
}

static uint32 xllm__json_table_get_u32(xvalue pValue, const char *sKey)
{
    xvalue pField = xllm__json_table_get(pValue, sKey);
    if ( !pField ) {
        return 0;
    }

    if ( xvoType(pField) == XVO_DT_INT ) {
        return (uint32)xvoGetInt(pField);
    }
    if ( xvoType(pField) == XVO_DT_FLOAT ) {
        double fValue = xvoGetFloat(pField);
        if ( fValue <= 0.0 ) {
            return 0u;
        }
        if ( fValue >= 4294967295.0 ) {
            return 4294967295u;
        }
        return (uint32)fValue;
    }

    return 0;
}

static bool xllm__json_text_is_valid_json(const char *sText)
{
    const char *sTrim;
    size_t iLen;
    xvalue tParsed;

    if ( !sText ) {
        return false;
    }

    sTrim = sText;
    while ( *sTrim == ' ' || *sTrim == '\t' || *sTrim == '\r' || *sTrim == '\n' ) {
        ++sTrim;
    }

    iLen = strlen(sTrim);
    while ( iLen > 0u ) {
        char ch = sTrim[iLen - 1u];
        if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
            break;
        }
        --iLen;
    }

    tParsed = xrtParseJSON((str)sTrim, iLen);
    if ( !tParsed ) {
        return false;
    }

    if ( xvoType(tParsed) != XVO_DT_NULL ) {
        xvoUnref(tParsed);
        return true;
    }

    if ( iLen == 4u && memcmp(sTrim, "null", 4u) == 0 ) {
        xvoUnref(tParsed);
        return true;
    }

    xvoUnref(tParsed);
    return false;
}

static char *xllm__dup_range_trimmed(const char *sText, size_t iLen)
{
    size_t iStart = 0u;
    size_t iEnd = iLen;
    char *sOut;

    if ( !sText ) {
        return NULL;
    }

    while ( iStart < iEnd ) {
        char ch = sText[iStart];
        if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
            break;
        }
        ++iStart;
    }

    while ( iEnd > iStart ) {
        char ch = sText[iEnd - 1u];
        if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
            break;
        }
        --iEnd;
    }

    sOut = (char *)xrtCalloc(iEnd - iStart + 1u, sizeof(char));
    if ( !sOut ) {
        return NULL;
    }

    if ( iEnd > iStart ) {
        memcpy(sOut, sText + iStart, iEnd - iStart);
    }
    return sOut;
}

static xvalue xllm__parse_json_range(const char *sText, size_t iLen, char **psNormalized)
{
    char *sCandidate;
    xvalue tParsed;
    char chStart;

    if ( psNormalized ) {
        *psNormalized = NULL;
    }

    sCandidate = xllm__dup_range_trimmed(sText, iLen);
    if ( !sCandidate || !sCandidate[0] ) {
        if ( sCandidate ) {
            xrtFree(sCandidate);
        }
        return NULL;
    }

    chStart = sCandidate[0];
    if ( chStart != '{' &&
         chStart != '[' &&
         chStart != '"' &&
         chStart != '-' &&
         chStart != 't' &&
         chStart != 'f' &&
         chStart != 'n' &&
         (chStart < '0' || chStart > '9') ) {
        xrtFree(sCandidate);
        return NULL;
    }

    tParsed = xrtParseJSON((str)sCandidate, strlen(sCandidate));
    if ( !tParsed ) {
        xrtFree(sCandidate);
        return NULL;
    }

    if ( xvoType(tParsed) == XVO_DT_NULL && strcmp(sCandidate, "null") != 0 ) {
        xvoUnref(tParsed);
        xrtFree(sCandidate);
        return NULL;
    }

    if ( psNormalized ) {
        *psNormalized = sCandidate;
    } else {
        xrtFree(sCandidate);
    }
    return tParsed;
}

static xvalue xllm__extract_best_effort_json(const char *sText, char **psNormalized)
{
    const char *sFence;
    size_t i;

    if ( psNormalized ) {
        *psNormalized = NULL;
    }

    if ( !sText ) {
        return NULL;
    }

    {
        xvalue tWhole = xllm__parse_json_range(sText, strlen(sText), psNormalized);
        if ( tWhole ) {
            return tWhole;
        }
    }

    sFence = strstr(sText, "```");
    while ( sFence ) {
        const char *sFenceBody = sFence + 3u;
        const char *sLineEnd = strchr(sFenceBody, '\n');
        const char *sContentStart;
        const char *sFenceEnd;

        if ( !sLineEnd ) {
            break;
        }

        sContentStart = sLineEnd + 1;
        sFenceEnd = strstr(sContentStart, "```");
        if ( sFenceEnd && sFenceEnd > sContentStart ) {
            xvalue tFenced = xllm__parse_json_range(sContentStart, (size_t)(sFenceEnd - sContentStart), psNormalized);
            if ( tFenced ) {
                return tFenced;
            }
            sFence = strstr(sFenceEnd + 3u, "```");
        } else {
            break;
        }
    }

    for ( i = 0; sText[i]; ++i ) {
        if ( sText[i] == '{' || sText[i] == '[' ) {
            char chOpen = sText[i];
            char chClose = (chOpen == '{') ? '}' : ']';
            bool bInString = false;
            bool bEscaped = false;
            int iDepth = 0;
            size_t j;

            for ( j = i; sText[j]; ++j ) {
                char ch = sText[j];

                if ( bInString ) {
                    if ( bEscaped ) {
                        bEscaped = false;
                    } else if ( ch == '\\' ) {
                        bEscaped = true;
                    } else if ( ch == '"' ) {
                        bInString = false;
                    }
                    continue;
                }

                if ( ch == '"' ) {
                    bInString = true;
                    continue;
                }

                if ( ch == chOpen ) {
                    ++iDepth;
                } else if ( ch == chClose ) {
                    --iDepth;
                    if ( iDepth == 0 ) {
                        xvalue tCandidate = xllm__parse_json_range(sText + i, j - i + 1u, psNormalized);
                        if ( tCandidate ) {
                            return tCandidate;
                        }
                        break;
                    }
                }
            }
        }
    }

    return NULL;
}

static int xllm__openai_message_to_text(const xllm_message *pMessage, char **psText, xllm_error *pError)
{
    xllm__json_builder tBuilder;
    size_t i;
    bool bHasContent = false;

    if ( !psText ) {
        return XRT_NET_ERROR;
    }

    *psText = NULL;
    memset(&tBuilder, 0, sizeof(tBuilder));

    if ( !pMessage || pMessage->iPartCount == 0 ) {
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
                    xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "openai-compatible adapter currently only supports inline text content");
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
                    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify json part");
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
                    "openai-compatible adapter text-only content currently supports only text and json parts"
                );
                xllm__json_builder_reset(&tBuilder);
                return XRT_NET_ERROR;
        }
    }

    if ( !bHasContent ) {
        *psText = xllm__dup_cstr("");
        xllm__json_builder_reset(&tBuilder);
        return *psText ? XRT_NET_OK : XRT_NET_ERROR;
    }

    *psText = xllm__json_builder_detach(&tBuilder);
    if ( !*psText ) {
        xllm__json_builder_reset(&tBuilder);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static bool xllm__openai_message_requires_content_array(const xllm_message *pMessage)
{
    size_t i;

    if ( !pMessage ) {
        return false;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        switch ( pMessage->pParts[i].eKind ) {
            case XLLM_PART_IMAGE:
            case XLLM_PART_FILE:
                return true;
            default:
                break;
        }
    }

    return false;
}

static int xllm__openai_append_image_part(
    xllm__json_builder *pBuilder,
    const xllm_content_part *pPart,
    xllm_error *pError
)
{
    const char *sMimeType;

    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    sMimeType = pPart->as.tSource.sMimeType ? pPart->as.tSource.sMimeType : "application/octet-stream";

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_URL:
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"image_url\",\"image_url\":{\"url\":") ||
                 !xllm__json_builder_append_escaped(
                    pBuilder,
                    pPart->as.tSource.as.sUrl ? pPart->as.tSource.as.sUrl : ""
                 ) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        case XLLM_SOURCE_INLINE_BYTES: {
            char *sBase64 = NULL;

            if ( !pPart->as.tSource.as.tBytes.pData || pPart->as.tSource.as.tBytes.iSize == 0u ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "image bytes input is empty");
                return XRT_NET_ERROR;
            }

            sBase64 = (char *)xrtBase64Encode(
                (ptr)pPart->as.tSource.as.tBytes.pData,
                pPart->as.tSource.as.tBytes.iSize,
                NULL
            );
            if ( !sBase64 ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode image bytes");
                return XRT_NET_ERROR;
            }

            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"image_url\",\"image_url\":{\"url\":") ||
                 !xllm__json_builder_append_char(pBuilder, '"') ||
                 !xllm__json_builder_append_cstr(pBuilder, "data:") ||
                 !xllm__json_builder_append_cstr(pBuilder, sMimeType) ||
                 !xllm__json_builder_append_cstr(pBuilder, ";base64,") ||
                 !xllm__json_builder_append_cstr(pBuilder, sBase64) ||
                 !xllm__json_builder_append_char(pBuilder, '"') ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                xrtFree(sBase64);
                return XRT_NET_ERROR;
            }

            xrtFree(sBase64);
            return XRT_NET_OK;
        }
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            if ( !pPart->as.tSource.as.sFileId || !pPart->as.tSource.as.sFileId[0] ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    "openai-compatible image file_id input is empty"
                );
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"file\",\"file\":{\"file_id\":") ||
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
                "openai-compatible adapter image input only supports url, provider file_id, or inline bytes"
            );
            return XRT_NET_ERROR;
    }
}

static int xllm__openai_apply_transport(
    xhttprequest *pHttpRequest,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_call_options *pOptions,
    xllm_error *pError
);

static int xllm__openai_append_file_part(
    xllm__json_builder *pBuilder,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_call_options *pOptions,
    const xllm_content_part *pPart,
    xllm_error *pError
)
{
    const char *sMimeType;
    const char *sName;
    xhttprequest tHttpRequest;
    xhttpresponse *pHttpResponse = NULL;
    xnet_result iNetStatus = XRT_NET_ERROR;

    if ( !pBuilder || !pPart ) {
        return XRT_NET_ERROR;
    }

    memset(&tHttpRequest, 0, sizeof(tHttpRequest));
    sMimeType = pPart->as.tSource.sMimeType ? pPart->as.tSource.sMimeType : "application/octet-stream";
    sName = pPart->as.tSource.sName ? pPart->as.tSource.sName : "upload.bin";

    switch ( pPart->as.tSource.eKind ) {
        case XLLM_SOURCE_PROVIDER_FILE_ID:
            if ( !pPart->as.tSource.as.sFileId || !pPart->as.tSource.as.sFileId[0] ) {
                xllm__error_set(
                    pError,
                    XLLM_ERROR_INVALID_REQUEST,
                    "openai-compatible file file_id input is empty"
                );
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"file\",\"file\":{\"file_id\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pPart->as.tSource.as.sFileId) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                return XRT_NET_ERROR;
            }
            return XRT_NET_OK;
        case XLLM_SOURCE_INLINE_BYTES: {
            char *sBase64 = NULL;

            if ( !pPart->as.tSource.as.tBytes.pData || pPart->as.tSource.as.tBytes.iSize == 0u ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "file bytes input is empty");
                return XRT_NET_ERROR;
            }

            sBase64 = (char *)xrtBase64Encode(
                (ptr)pPart->as.tSource.as.tBytes.pData,
                pPart->as.tSource.as.tBytes.iSize,
                NULL
            );
            if ( !sBase64 ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode file bytes");
                return XRT_NET_ERROR;
            }

            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"file\",\"file\":{\"filename\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sName) ||
                 !xllm__json_builder_append_cstr(pBuilder, ",\"mime_type\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sMimeType) ||
                 !xllm__json_builder_append_cstr(pBuilder, ",\"file_data\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sBase64) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                xrtFree(sBase64);
                return XRT_NET_ERROR;
            }

            xrtFree(sBase64);
            return XRT_NET_OK;
        }
        case XLLM_SOURCE_URL: {
            char *sBase64 = NULL;

            if ( !pPart->as.tSource.as.sUrl || !pPart->as.tSource.as.sUrl[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "openai-compatible file url input is empty");
                return XRT_NET_ERROR;
            }

            xrtHttpRequestInit(&tHttpRequest);
            if ( !xrtHttpRequestSetURL(&tHttpRequest, pPart->as.tSource.as.sUrl) ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "failed to set openai-compatible file url request");
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
                    xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "openai-compatible file url download timed out");
                } else {
                    xllm__error_set(pError, XLLM_ERROR_NETWORK, "openai-compatible file url download failed");
                }
                return XRT_NET_ERROR;
            }
            if ( pHttpResponse->iStatusCode >= 400u ) {
                if ( pHttpResponse->iStatusCode >= 500u ) {
                    xllm__error_set(pError, XLLM_ERROR_UPSTREAM_5XX, "openai-compatible file url download returned 5xx");
                } else {
                    xllm__error_set(pError, XLLM_ERROR_UPSTREAM_4XX, "openai-compatible file url download returned 4xx");
                }
                pError->iHttpStatus = (int)pHttpResponse->iStatusCode;
                xrtHttpResponseDestroy(pHttpResponse);
                return XRT_NET_ERROR;
            }
            if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
                xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible file url download returned empty body");
                xrtHttpResponseDestroy(pHttpResponse);
                return XRT_NET_ERROR;
            }

            sBase64 = (char *)xrtBase64Encode((ptr)pHttpResponse->pBody, pHttpResponse->iBodyLen, NULL);
            xrtHttpResponseDestroy(pHttpResponse);
            pHttpResponse = NULL;
            if ( !sBase64 ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode downloaded file bytes");
                return XRT_NET_ERROR;
            }

            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"file\",\"file\":{\"filename\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sName) ||
                 !xllm__json_builder_append_cstr(pBuilder, ",\"mime_type\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sMimeType) ||
                 !xllm__json_builder_append_cstr(pBuilder, ",\"file_data\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, sBase64) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                xrtFree(sBase64);
                return XRT_NET_ERROR;
            }

            xrtFree(sBase64);
            return XRT_NET_OK;
        }
        case XLLM_SOURCE_INLINE_TEXT:
        default:
            xllm__error_set(
                pError,
                XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                "openai-compatible file input only supports provider file_id, url, or inline bytes"
            );
            return XRT_NET_ERROR;
    }
}

static int xllm__openai_append_message_content_array(
    xllm__json_builder *pBuilder,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_call_options *pOptions,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    size_t i;
    bool bNeedComma = false;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    if ( pMessage->eRole != XLLM_ROLE_USER &&
         pMessage->eRole != XLLM_ROLE_ASSISTANT &&
         pMessage->eRole != XLLM_ROLE_TOOL ) {
        xllm__error_set(
            pError,
            XLLM_ERROR_UNSUPPORTED_CAPABILITY,
            "openai-compatible adapter multimodal content currently only supports user, assistant, and tool messages"
        );
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBuilder, '[') ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pMessage->iPartCount; ++i ) {
        const xllm_content_part *pPart = &pMessage->pParts[i];

        if ( bNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }

        switch ( pPart->eKind ) {
            case XLLM_PART_TEXT:
                if ( pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                    xllm__error_set(
                        pError,
                        XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
                        "openai-compatible adapter currently only supports inline text content"
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
            case XLLM_PART_JSON: {
                char *sJson = (char *)xrtStringifyJSON(pPart->as.tJsonValue, 0, NULL);

                if ( !sJson ) {
                    xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify json part");
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
            }
            case XLLM_PART_IMAGE:
                if ( xllm__openai_append_image_part(pBuilder, pPart, pError) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                break;
            case XLLM_PART_FILE:
                if ( xllm__openai_append_file_part(pBuilder, pRuntime, pProfile, pOptions, pPart, pError) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
                break;
            default:
                xllm__error_set(
                    pError,
                    XLLM_ERROR_UNSUPPORTED_CAPABILITY,
                    "openai-compatible adapter multimodal input currently supports only text, json, image, and file parts"
                );
                return XRT_NET_ERROR;
        }

        bNeedComma = true;
    }

    return xllm__json_builder_append_char(pBuilder, ']') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__openai_append_message_tool_calls(xllm__json_builder *pBuilder, const xllm_message *pMessage)
{
    size_t i;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_calls\":[") ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < pMessage->iToolCallCount; ++i ) {
        const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
        const char *sToolName = pCall->sToolName ? pCall->sToolName : pCall->sToolId;

        if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }

        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"id\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_escaped(pBuilder, pCall->sCallId ? pCall->sCallId : "") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"type\":\"function\",\"function\":{\"name\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_escaped(pBuilder, sToolName ? sToolName : "") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"arguments\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_escaped(pBuilder, pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_cstr(pBuilder, "}}") ) return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__openai_append_message(
    xllm__json_builder *pBuilder,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_call_options *pOptions,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    const char *sRole;
    const char *sReasoningContent;
    char *sContent = NULL;
    int iStatus;
    bool bUseContentArray;

    if ( !pBuilder || !pMessage ) {
        return XRT_NET_ERROR;
    }

    sRole = xllm__openai_role_name(pMessage->eRole);
    sReasoningContent = (
        pMessage->eRole == XLLM_ROLE_ASSISTANT &&
        xllm__openai_family_uses_reasoning_content(pProfile)
    ) ? xllm__openai_message_reasoning_content(pMessage) : NULL;
    bUseContentArray = xllm__openai_message_requires_content_array(pMessage);
    if ( !bUseContentArray ) {
        iStatus = xllm__openai_message_to_text(pMessage, &sContent, pError);
        if ( iStatus != XRT_NET_OK ) {
            return iStatus;
        }
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, "{\"role\":") ) goto fail;
    if ( !xllm__json_builder_append_escaped(pBuilder, sRole) ) goto fail;
    if ( sReasoningContent ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"reasoning_content\":") ) goto fail;
        if ( !xllm__json_builder_append_escaped(pBuilder, sReasoningContent) ) goto fail;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_call_id\":") ) goto fail;
        if ( !xllm__json_builder_append_escaped(pBuilder, pMessage->sToolCallId ? pMessage->sToolCallId : "") ) goto fail;
    }

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u ) {
        if ( xllm__openai_append_message_tool_calls(pBuilder, pMessage) != XRT_NET_OK ) goto fail;
    }

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u && (!bUseContentArray && (!sContent || sContent[0] == '\0')) ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"content\":null}") ) goto fail;
    } else if ( bUseContentArray ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"content\":") ) goto fail;
        if ( xllm__openai_append_message_content_array(pBuilder, pRuntime, pProfile, pOptions, pMessage, pError) != XRT_NET_OK ) goto fail;
        if ( !xllm__json_builder_append_char(pBuilder, '}') ) goto fail;
    } else {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"content\":") ) goto fail;
        if ( !xllm__json_builder_append_escaped(pBuilder, sContent ? sContent : "") ) goto fail;
        if ( !xllm__json_builder_append_char(pBuilder, '}') ) goto fail;
    }

    xllm__free_cstr(&sContent);
    return XRT_NET_OK;

fail:
    xllm__free_cstr(&sContent);
    return XRT_NET_ERROR;
}

static int xllm__openai_append_context_messages(
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
            if ( xllm__openai_append_message(
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
        if ( xllm__openai_append_message(
                 pBuilder,
                 pRuntime,
                 pProfile,
                 pOptions,
                 &pRequest->pMessages[i],
                 pError) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        bNeedComma = true;
    }

    if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__openai_append_tools(xllm__json_builder *pBuilder, const xllm_request *pRequest)
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

        if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return XRT_NET_ERROR;
        }

        if ( pTool->eKind == XLLM_TOOL_PROVIDER ) {
            if ( !pTool->tVendorExtra || xvoType(pTool->tVendorExtra) != XVO_DT_TABLE ) {
                return XRT_NET_ERROR;
            }

            sProviderToolJson = (char *)xrtStringifyJSON(pTool->tVendorExtra, 0, NULL);
            if ( !sProviderToolJson ) {
                return XRT_NET_ERROR;
            }

            if ( !xllm__json_builder_append_cstr(pBuilder, sProviderToolJson) ) {
                xrtFree(sProviderToolJson);
                return XRT_NET_ERROR;
            }

            xrtFree(sProviderToolJson);
            continue;
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

        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"type\":\"function\",\"function\":{\"name\":") ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_escaped(pBuilder, sWireName ? sWireName : "") ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( pTool->sDescription ) {
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"description\":") ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_escaped(pBuilder, pTool->sDescription) ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"parameters\":") ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, sSchema) ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
            xrtFree(sSchema);
            return XRT_NET_ERROR;
        }

        xrtFree(sSchema);
    }

    if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static int xllm__openai_append_tool_policy(xllm__json_builder *pBuilder, const xllm_request *pRequest)
{
    if ( !pBuilder || !pRequest || pRequest->iToolCount == 0u ) {
        return XRT_NET_OK;
    }

    switch ( pRequest->tToolPolicy.eMode ) {
        case XLLM_TOOL_CHOICE_NONE:
            return xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":\"none\"") ? XRT_NET_OK : XRT_NET_ERROR;
        case XLLM_TOOL_CHOICE_REQUIRED:
            return xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":\"required\"") ? XRT_NET_OK : XRT_NET_ERROR;
        case XLLM_TOOL_CHOICE_NAMED:
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":{\"type\":\"function\",\"function\":{\"name\":") ) return XRT_NET_ERROR;
            if ( !xllm__json_builder_append_escaped(pBuilder, pRequest->tToolPolicy.sToolName ? pRequest->tToolPolicy.sToolName : "") ) return XRT_NET_ERROR;
            if ( !xllm__json_builder_append_cstr(pBuilder, "}}") ) return XRT_NET_ERROR;
            return XRT_NET_OK;
        case XLLM_TOOL_CHOICE_AUTO:
        default:
            return xllm__json_builder_append_cstr(pBuilder, ",\"tool_choice\":\"auto\"") ? XRT_NET_OK : XRT_NET_ERROR;
    }
}

static int xllm__openai_append_stop(xllm__json_builder *pBuilder, const xllm_generation_params *pGeneration)
{
    size_t i;

    if ( !pBuilder || !pGeneration || pGeneration->iStopCount == 0u || !pGeneration->psStop ) {
        return XRT_NET_OK;
    }

    if ( pGeneration->iStopCount == 1u ) {
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"stop\":") ) return XRT_NET_ERROR;
        return xllm__json_builder_append_escaped(pBuilder, pGeneration->psStop[0] ? pGeneration->psStop[0] : "") ? XRT_NET_OK : XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, ",\"stop\":[") ) return XRT_NET_ERROR;
    for ( i = 0; i < pGeneration->iStopCount; ++i ) {
        if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_escaped(pBuilder, pGeneration->psStop[i] ? pGeneration->psStop[i] : "") ) return XRT_NET_ERROR;
    }
    return xllm__json_builder_append_char(pBuilder, ']') ? XRT_NET_OK : XRT_NET_ERROR;
}

static int xllm__openai_append_response_format(xllm__json_builder *pBuilder, const xllm_response_format *pFormat)
{
    char *sSchema = NULL;
    bool bStrict = false;
    bool bHasStrict = false;

    if ( !pBuilder || !pFormat ) {
        return XRT_NET_OK;
    }

    switch ( pFormat->eKind ) {
        case XLLM_RESPONSE_JSON:
            return xllm__json_builder_append_cstr(pBuilder, ",\"response_format\":{\"type\":\"json_object\"}") ? XRT_NET_OK : XRT_NET_ERROR;
        case XLLM_RESPONSE_JSON_SCHEMA:
            if ( pFormat->tJsonSchema && xvoType(pFormat->tJsonSchema) != XVO_DT_NULL ) {
                sSchema = (char *)xrtStringifyJSON(pFormat->tJsonSchema, 0, NULL);
            }
            if ( pFormat->tVendorExtra && xvoType(pFormat->tVendorExtra) == XVO_DT_TABLE ) {
                bHasStrict = xllm__json_table_get_bool(pFormat->tVendorExtra, "strict", &bStrict);
            }
            if ( !sSchema ) {
                sSchema = xllm__dup_cstr("{}");
            }
            if ( !sSchema ) {
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"response_format\":{\"type\":\"json_schema\",\"json_schema\":{\"name\":") ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_escaped(pBuilder, pFormat->sSchemaName ? pFormat->sSchemaName : "xllm_schema") ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"schema\":") ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, sSchema) ) {
                xrtFree(sSchema);
                return XRT_NET_ERROR;
            }
            if ( bHasStrict ) {
                if ( !xllm__json_builder_append_cstr(pBuilder, bStrict ? ",\"strict\":true" : ",\"strict\":false") ) {
                    xrtFree(sSchema);
                    return XRT_NET_ERROR;
                }
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
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

static bool xllm__openai_should_send_native_response_format(
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_response_format *pFormat,
    const xllm_call_options *pOptions
)
{
    const xllm_model_binding *pBinding;
    bool bNeedsMultimodal;

    if ( !pRequest || !pFormat || pFormat->eKind == XLLM_RESPONSE_TEXT ) {
        return false;
    }

    if ( !pOptions || !pOptions->bBestEffortStructuredOutput ) {
        return true;
    }

    if ( !pProfile ) {
        return false;
    }

    bNeedsMultimodal = xllm__openai_request_uses_multimodal(pRequest);
    pBinding = xllm__select_request_binding(pProfile, pRequest, bNeedsMultimodal, NULL);
    return xllm__binding_supports_flag(pBinding, XLLM_CAP_JSON_OUT);
}

static int xllm__openai_parse_structured_output(
    const xllm_response_format *pFormat,
    const xllm_call_options *pOptions,
    const char *sText,
    xvalue *ptJsonValue,
    char **psNormalizedText,
    xllm_error *pError
)
{
    const char *sTrimmed = sText;

    if ( ptJsonValue ) {
        *ptJsonValue = NULL;
    }
    if ( psNormalizedText ) {
        *psNormalizedText = NULL;
    }

    if ( !pFormat || pFormat->eKind == XLLM_RESPONSE_TEXT || !sText || !sText[0] ) {
        return XRT_NET_OK;
    }

    while ( *sTrimmed == ' ' || *sTrimmed == '\t' || *sTrimmed == '\r' || *sTrimmed == '\n' ) {
        ++sTrimmed;
    }

    if ( (*sTrimmed == '{' ||
          *sTrimmed == '[' ||
          *sTrimmed == '"' ||
          *sTrimmed == '-' ||
          *sTrimmed == 't' ||
          *sTrimmed == 'f' ||
          *sTrimmed == 'n' ||
          (*sTrimmed >= '0' && *sTrimmed <= '9')) &&
         xllm__json_text_is_valid_json(sText) ) {
        xvalue tJsonValue = xrtParseJSON((str)sText, strlen(sText));
        if ( !tJsonValue ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "structured output response json parse failed");
            return XRT_NET_ERROR;
        }
        if ( psNormalizedText ) {
            *psNormalizedText = xllm__dup_cstr(sText);
            if ( !*psNormalizedText ) {
                xvoUnref(tJsonValue);
                return XRT_NET_ERROR;
            }
        }
        if ( ptJsonValue ) {
            *ptJsonValue = tJsonValue;
        } else {
            xvoUnref(tJsonValue);
        }
        return XRT_NET_OK;
    }

    if ( pOptions && pOptions->bBestEffortStructuredOutput ) {
        xvalue tJsonValue = xllm__extract_best_effort_json(sText, psNormalizedText);
        if ( tJsonValue ) {
            if ( ptJsonValue ) {
                *ptJsonValue = tJsonValue;
            } else {
                xvoUnref(tJsonValue);
            }
            return XRT_NET_OK;
        }
    }

    xllm__error_set(pError, XLLM_ERROR_PARSE, "structured output response is not valid json");
    return XRT_NET_ERROR;
}

static const char *xllm__openai_reasoning_effort_name(const xllm_reasoning_options *pReasoning)
{
    const char *sVendorEffort;

    if ( !pReasoning ) {
        return NULL;
    }

    if ( pReasoning->tVendorExtra && xvoType(pReasoning->tVendorExtra) == XVO_DT_TABLE ) {
        sVendorEffort = xllm__json_table_get_text(pReasoning->tVendorExtra, "reasoning_effort");
        if ( sVendorEffort && sVendorEffort[0] ) {
            return sVendorEffort;
        }
        sVendorEffort = xllm__json_table_get_text(pReasoning->tVendorExtra, "effort");
        if ( sVendorEffort && sVendorEffort[0] ) {
            return sVendorEffort;
        }
    }

    if ( (pReasoning->tEnabled.bSet && !pReasoning->tEnabled.bValue) ||
         pReasoning->eLevel == XLLM_REASONING_OFF ) {
        return "none";
    }

    switch ( pReasoning->eLevel ) {
        case XLLM_REASONING_LOW:
            return "low";
        case XLLM_REASONING_MEDIUM:
            return "medium";
        case XLLM_REASONING_HIGH:
            return "high";
        default:
            return NULL;
    }
}

static const char *xllm__openai_select_model(const xllm_profile *pProfile, const xllm_request *pRequest, bool *pbMultimodal)
{
    bool bMultimodal = false;

    if ( pbMultimodal ) {
        *pbMultimodal = false;
    }

    if ( !pProfile || !pRequest ) {
        return NULL;
    }

    if ( pRequest->eSlot == XLLM_SLOT_MULTIMODAL ) {
        bMultimodal = true;
    } else if ( pRequest->eSlot == XLLM_SLOT_AUTO ) {
        bMultimodal = xllm__openai_request_uses_multimodal(pRequest);
    }

    if ( pbMultimodal ) {
        *pbMultimodal = bMultimodal;
    }

    if ( bMultimodal ) {
        return pProfile->tModels.tMultimodal.sModelId;
    }

    return pProfile->tModels.tText.sModelId ? pProfile->tModels.tText.sModelId : pProfile->tModels.tMultimodal.sModelId;
}

static char *xllm__openai_build_url(const char *sBaseUrl)
{
    static const char sPath[] = "chat/completions";
    size_t iLen;
    bool bNeedsSlash;
    char *sUrl;

    if ( !sBaseUrl || !sBaseUrl[0] ) {
        return NULL;
    }

    if ( strstr(sBaseUrl, "/chat/completions") != NULL ) {
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

static int xllm__openai_fill_request_headers(xhttprequest *pHttpRequest, const xllm_profile *pProfile)
{
    size_t i;

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
        case XLLM_AUTH_API_KEY_HEADER:
            if ( pProfile->tAuth.sHeaderName && pProfile->tAuth.sSecret ) {
                if ( !xrtHttpRequestSetHeader(pHttpRequest, pProfile->tAuth.sHeaderName, pProfile->tAuth.sSecret) ) {
                    return XRT_NET_ERROR;
                }
            }
            break;
        case XLLM_AUTH_NONE:
        default:
            break;
    }

    if ( pProfile->tProviderOptions.sOpenAIOrganizationId ) {
        if ( !xrtHttpRequestSetHeader(pHttpRequest, "OpenAI-Organization", pProfile->tProviderOptions.sOpenAIOrganizationId) ) {
            return XRT_NET_ERROR;
        }
    }
    if ( pProfile->tProviderOptions.sOpenAIProjectId ) {
        if ( !xrtHttpRequestSetHeader(pHttpRequest, "OpenAI-Project", pProfile->tProviderOptions.sOpenAIProjectId) ) {
            return XRT_NET_ERROR;
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

static uint32 xllm__openai_resolve_connect_timeout_ms(const xllm_runtime *pRuntime, const xllm_profile *pProfile)
{
    uint32 uTimeoutMs = 0u;

    if ( pRuntime && pRuntime->tOptions.tTransportDefaults.tConnectTimeoutMs.bSet ) {
        uTimeoutMs = pRuntime->tOptions.tTransportDefaults.tConnectTimeoutMs.iValue;
    }
    if ( pProfile && pProfile->tTransport.tConnectTimeoutMs.bSet ) {
        uTimeoutMs = pProfile->tTransport.tConnectTimeoutMs.iValue;
    }

    return uTimeoutMs;
}

static uint32 xllm__openai_resolve_read_timeout_ms(const xllm_runtime *pRuntime, const xllm_profile *pProfile)
{
    uint32 uTimeoutMs = 0u;

    if ( pRuntime && pRuntime->tOptions.tTransportDefaults.tReadTimeoutMs.bSet ) {
        uTimeoutMs = pRuntime->tOptions.tTransportDefaults.tReadTimeoutMs.iValue;
    }
    if ( pProfile && pProfile->tTransport.tReadTimeoutMs.bSet ) {
        uTimeoutMs = pProfile->tTransport.tReadTimeoutMs.iValue;
    }

    return uTimeoutMs;
}

static xllm_proxy_kind xllm__transport_resolve_proxy_kind(const xllm_runtime *pRuntime, const xllm_profile *pProfile)
{
    xllm_proxy_kind eKind = XLLM_PROXY_UNSPECIFIED;

    if ( pRuntime && pRuntime->tOptions.tTransportDefaults.eProxyKind != XLLM_PROXY_UNSPECIFIED ) {
        eKind = pRuntime->tOptions.tTransportDefaults.eProxyKind;
    }
    if ( pProfile && pProfile->tTransport.eProxyKind != XLLM_PROXY_UNSPECIFIED ) {
        eKind = pProfile->tTransport.eProxyKind;
    }

    return eKind;
}

static int xllm__transport_create_proxy(
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    xnetproxy **ppProxy,
    xllm_error *pError
)
{
    xllm_proxy_kind eKind;
    const char *sHost = NULL;
    const char *sUser = NULL;
    const char *sPass = NULL;
    uint32 uPort = 0u;
    xnetproxyconfig tProxyConfig;

    if ( !ppProxy ) {
        return XRT_NET_ERROR;
    }

    *ppProxy = NULL;
    eKind = xllm__transport_resolve_proxy_kind(pRuntime, pProfile);
    if ( eKind == XLLM_PROXY_UNSPECIFIED || eKind == XLLM_PROXY_NONE ) {
        return XRT_NET_OK;
    }

    if ( pRuntime ) {
        if ( pRuntime->tOptions.tTransportDefaults.sProxyHost && pRuntime->tOptions.tTransportDefaults.sProxyHost[0] ) {
            sHost = pRuntime->tOptions.tTransportDefaults.sProxyHost;
        }
        if ( pRuntime->tOptions.tTransportDefaults.tProxyPort.bSet ) {
            uPort = pRuntime->tOptions.tTransportDefaults.tProxyPort.iValue;
        }
        if ( pRuntime->tOptions.tTransportDefaults.sProxyUser && pRuntime->tOptions.tTransportDefaults.sProxyUser[0] ) {
            sUser = pRuntime->tOptions.tTransportDefaults.sProxyUser;
        }
        if ( pRuntime->tOptions.tTransportDefaults.sProxyPass && pRuntime->tOptions.tTransportDefaults.sProxyPass[0] ) {
            sPass = pRuntime->tOptions.tTransportDefaults.sProxyPass;
        }
    }
    if ( pProfile ) {
        if ( pProfile->tTransport.sProxyHost && pProfile->tTransport.sProxyHost[0] ) {
            sHost = pProfile->tTransport.sProxyHost;
        }
        if ( pProfile->tTransport.tProxyPort.bSet ) {
            uPort = pProfile->tTransport.tProxyPort.iValue;
        }
        if ( pProfile->tTransport.sProxyUser && pProfile->tTransport.sProxyUser[0] ) {
            sUser = pProfile->tTransport.sProxyUser;
        }
        if ( pProfile->tTransport.sProxyPass && pProfile->tTransport.sProxyPass[0] ) {
            sPass = pProfile->tTransport.sProxyPass;
        }
    }

    if ( !sHost || !sHost[0] || uPort == 0u || uPort > 65535u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "proxy transport requires a valid host and port");
        return XRT_NET_ERROR;
    }

    xrtNetProxyConfigInit(&tProxyConfig);
    switch ( eKind ) {
        case XLLM_PROXY_SOCKS5:
            tProxyConfig.iType = XNET_PROXY_SOCKS5;
            break;
        case XLLM_PROXY_HTTP_CONNECT:
            tProxyConfig.iType = XNET_PROXY_HTTP_CONNECT;
            break;
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "unsupported proxy kind");
            return XRT_NET_ERROR;
    }

    if ( snprintf(tProxyConfig.sHost, sizeof(tProxyConfig.sHost), "%s", sHost) >= (int)sizeof(tProxyConfig.sHost) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "proxy host is too long");
        return XRT_NET_ERROR;
    }
    tProxyConfig.iPort = (uint16)uPort;
    if ( sUser && sUser[0] &&
         snprintf(tProxyConfig.sUser, sizeof(tProxyConfig.sUser), "%s", sUser) >= (int)sizeof(tProxyConfig.sUser) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "proxy username is too long");
        return XRT_NET_ERROR;
    }
    if ( sPass && sPass[0] &&
         snprintf(tProxyConfig.sPass, sizeof(tProxyConfig.sPass), "%s", sPass) >= (int)sizeof(tProxyConfig.sPass) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "proxy password is too long");
        return XRT_NET_ERROR;
    }

    *ppProxy = xrtNetProxyCreate(&tProxyConfig);
    if ( !*ppProxy ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to create transport proxy");
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static uint32 xllm__openai_resolve_request_timeout_ms(const xllm_call_options *pOptions)
{
    if ( pOptions && pOptions->uTimeoutMs > 0u ) {
        return pOptions->uTimeoutMs;
    }
    return 0u;
}

static uint32 xllm__openai_resolve_live_connect_timeout_ms(uint32 uConnectTimeoutMs, const xhttprequest *pHttpRequest)
{
    if ( uConnectTimeoutMs > 0u ) {
        return uConnectTimeoutMs;
    }
    if ( pHttpRequest && pHttpRequest->iTimeoutMs > 0u ) {
        return pHttpRequest->iTimeoutMs;
    }
    if ( pHttpRequest && pHttpRequest->iIdleTimeoutMs > 0u ) {
        return pHttpRequest->iIdleTimeoutMs;
    }
    return 30000u;
}

static int xllm__openai_apply_transport(
    xhttprequest *pHttpRequest,
    const xllm_runtime *pRuntime,
    const xllm_profile *pProfile,
    const xllm_call_options *pOptions,
    xllm_error *pError
)
{
    uint32 uReadTimeoutMs;
    uint32 uRequestTimeoutMs;
    bool bVerifyPeer = true;
    xnetproxy *pProxy = NULL;

    if ( !pHttpRequest ) {
        return XRT_NET_ERROR;
    }

    uReadTimeoutMs = xllm__openai_resolve_read_timeout_ms(pRuntime, pProfile);
    uRequestTimeoutMs = xllm__openai_resolve_request_timeout_ms(pOptions);

    if ( pRuntime && pRuntime->tOptions.tTransportDefaults.tVerifyPeer.bSet ) {
        bVerifyPeer = pRuntime->tOptions.tTransportDefaults.tVerifyPeer.bValue;
    }
    if ( pProfile && pProfile->tTransport.tVerifyPeer.bSet ) {
        bVerifyPeer = pProfile->tTransport.tVerifyPeer.bValue;
    }

    if ( uRequestTimeoutMs > 0u ) {
        xrtHttpRequestSetTimeout(pHttpRequest, uRequestTimeoutMs);
    }
    if ( uReadTimeoutMs > 0u ) {
        xrtHttpRequestSetIdleTimeout(pHttpRequest, uReadTimeoutMs);
    }
    xrtHttpRequestSetVerifyPeer(pHttpRequest, bVerifyPeer);

    if ( xllm__transport_create_proxy(pRuntime, pProfile, &pProxy, pError) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( pHttpRequest->pProxy ) {
        xrtNetProxyRelease(pHttpRequest->pProxy);
        pHttpRequest->pProxy = NULL;
    }
    pHttpRequest->pProxy = pProxy;
    return XRT_NET_OK;
}


static void xllm__openai_fill_error_from_http(xllm_error *pError, const xhttpresponse *pHttpResponse, xvalue tRoot, const char *sRequestId)
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
        } else if ( pHttpResponse->iStatusCode >= 500u ) {
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
        const char *sProviderCode = xllm__json_table_get_text(tErrorObj, "code");
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


static size_t xllm__openai_base64_decoded_size(const char *sBase64)
{
    size_t iLen;
    size_t iOut;

    if ( !sBase64 ) {
        return 0u;
    }

    iLen = strlen(sBase64);
    if ( iLen == 0u ) {
        return 0u;
    }

    iOut = (iLen / 4u) * 3u;
    if ( iLen > 0u && sBase64[iLen - 1u] == '=' ) {
        --iOut;
    }
    if ( iLen > 1u && sBase64[iLen - 2u] == '=' ) {
        --iOut;
    }
    return iOut;
}

static const char *xllm__openai_content_object_text(xvalue tObject, const char *sPrimaryKey, const char *sFallbackKey)
{
    const char *sValue = NULL;

    if ( tObject && xvoType(tObject) == XVO_DT_TABLE ) {
        if ( sPrimaryKey ) {
            sValue = xllm__json_table_get_text(tObject, sPrimaryKey);
        }
        if ( !sValue && sFallbackKey ) {
            sValue = xllm__json_table_get_text(tObject, sFallbackKey);
        }
    }

    return sValue;
}

static int xllm__openai_message_add_part(
    xllm_content_part **ppParts,
    size_t *piPartCount,
    size_t *piPartCapacity,
    xllm_content_part *pPart
)
{
    if ( !ppParts || !piPartCount || !piPartCapacity || !pPart ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__append_buffer(
            (void **)ppParts,
            sizeof(*pPart),
            piPartCount,
            piPartCapacity,
            pPart
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    memset(pPart, 0, sizeof(*pPart));
    return XRT_NET_OK;
}

static int xllm__openai_parse_message_content(
    xvalue tContent,
    xllm_content_part **ppParts,
    size_t *piPartCount,
    char **psVisibleText,
    char **psRefusalText,
    xllm_error *pError
)
{
    xllm__json_builder tVisibleText;
    xllm_content_part *pParts = NULL;
    size_t iPartCount = 0u;
    size_t iPartCapacity = 0u;

    if ( ppParts ) {
        *ppParts = NULL;
    }
    if ( piPartCount ) {
        *piPartCount = 0u;
    }
    if ( psVisibleText ) {
        *psVisibleText = NULL;
    }
    if ( psRefusalText ) {
        *psRefusalText = NULL;
    }

    if ( !tContent ) {
        return XRT_NET_OK;
    }

    memset(&tVisibleText, 0, sizeof(tVisibleText));

    if ( xvoType(tContent) == XVO_DT_TEXT ) {
        xllm_content_part tPart;
        const char *sText = (const char *)xvoGetText(tContent);

        memset(&tPart, 0, sizeof(tPart));
        tPart.eKind = XLLM_PART_TEXT;
        tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
        tPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
        tPart.as.tSource.as.sText = xllm__dup_cstr(sText ? sText : "");
        if ( !tPart.as.tSource.sMimeType || !tPart.as.tSource.as.sText ) {
            xllm__content_part_free(&tPart);
            return XRT_NET_ERROR;
        }
        if ( xllm__openai_message_add_part(&pParts, &iPartCount, &iPartCapacity, &tPart) != XRT_NET_OK ) {
            xllm__content_part_free(&tPart);
            goto fail;
        }
        if ( sText && sText[0] && !xllm__json_builder_append_cstr(&tVisibleText, sText) ) {
            goto fail;
        }
    } else if ( xvoType(tContent) == XVO_DT_ARRAY ) {
        size_t i;

        for ( i = 0; i < (size_t)xvoArrayItemCount(tContent); ++i ) {
            xvalue tItem = xvoArrayGetValue(tContent, (uint32)i);
            const char *sType;

            if ( !tItem || xvoType(tItem) != XVO_DT_TABLE ) {
                continue;
            }

            sType = xllm__json_table_get_text(tItem, "type");
            if ( sType && (strcmp(sType, "text") == 0 || strcmp(sType, "output_text") == 0) ) {
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
            } else if ( sType && strcmp(sType, "refusal") == 0 ) {
                const char *sRefusal = xllm__json_table_get_text(tItem, "refusal");
                if ( !sRefusal ) {
                    sRefusal = xllm__json_table_get_text(tItem, "text");
                }
                if ( sRefusal && psRefusalText && !*psRefusalText ) {
                    *psRefusalText = xllm__dup_cstr(sRefusal);
                    if ( !*psRefusalText ) {
                        goto fail;
                    }
                }
            } else if ( sType && strcmp(sType, "image_url") == 0 ) {
                xvalue tImage = xllm__json_table_get(tItem, "image_url");
                const char *sUrl = xllm__openai_content_object_text(tImage, "url", NULL);
                const char *sMimeType = xllm__openai_content_object_text(tImage, "mime_type", NULL);
                const char *sName = xllm__openai_content_object_text(tImage, "filename", "name");
                xllm_content_part tPart;

                if ( !sUrl || !sUrl[0] ) {
                    continue;
                }

                memset(&tPart, 0, sizeof(tPart));
                tPart.eKind = XLLM_PART_IMAGE;
                tPart.as.tSource.eKind = XLLM_SOURCE_URL;
                tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType);
                tPart.as.tSource.sName = xllm__dup_cstr(sName);
                tPart.as.tSource.as.sUrl = xllm__dup_cstr(sUrl);
                if ( !tPart.as.tSource.as.sUrl ) {
                    xllm__content_part_free(&tPart);
                    goto fail;
                }
                if ( xllm__openai_message_add_part(&pParts, &iPartCount, &iPartCapacity, &tPart) != XRT_NET_OK ) {
                    xllm__content_part_free(&tPart);
                    goto fail;
                }
            } else if ( sType && strcmp(sType, "file") == 0 ) {
                xvalue tFile = xllm__json_table_get(tItem, "file");
                const char *sFileId = xllm__openai_content_object_text(tFile, "file_id", NULL);
                const char *sUrl = xllm__openai_content_object_text(tFile, "url", NULL);
                const char *sFileData = xllm__openai_content_object_text(tFile, "file_data", NULL);
                const char *sMimeType = xllm__openai_content_object_text(tFile, "mime_type", NULL);
                const char *sName = xllm__openai_content_object_text(tFile, "filename", "name");
                xllm_content_part tPart;

                if ( !tFile || xvoType(tFile) != XVO_DT_TABLE ) {
                    tFile = tItem;
                    sFileId = xllm__openai_content_object_text(tFile, "file_id", NULL);
                    sUrl = xllm__openai_content_object_text(tFile, "url", NULL);
                    sFileData = xllm__openai_content_object_text(tFile, "file_data", NULL);
                    sMimeType = xllm__openai_content_object_text(tFile, "mime_type", NULL);
                    sName = xllm__openai_content_object_text(tFile, "filename", "name");
                }

                memset(&tPart, 0, sizeof(tPart));
                tPart.eKind = XLLM_PART_FILE;
                tPart.as.tSource.sMimeType = xllm__dup_cstr(sMimeType);
                tPart.as.tSource.sName = xllm__dup_cstr(sName);

                if ( sFileId && sFileId[0] ) {
                    tPart.as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
                    tPart.as.tSource.as.sFileId = xllm__dup_cstr(sFileId);
                    if ( !tPart.as.tSource.as.sFileId ) {
                        xllm__content_part_free(&tPart);
                        goto fail;
                    }
                } else if ( sUrl && sUrl[0] ) {
                    tPart.as.tSource.eKind = XLLM_SOURCE_URL;
                    tPart.as.tSource.as.sUrl = xllm__dup_cstr(sUrl);
                    if ( !tPart.as.tSource.as.sUrl ) {
                        xllm__content_part_free(&tPart);
                        goto fail;
                    }
                } else if ( sFileData && sFileData[0] ) {
                    size_t iDecodedSize = xllm__openai_base64_decoded_size(sFileData);
                    void *pDecoded = xrtBase64Decode((str)sFileData, strlen(sFileData), NULL);

                    if ( !pDecoded ) {
                        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to decode openai-compatible file output");
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
    }

    if ( psVisibleText && tVisibleText.iLen > 0u ) {
        *psVisibleText = xllm__json_builder_detach(&tVisibleText);
        if ( tVisibleText.iLen > 0u && !*psVisibleText ) {
            goto fail;
        }
    }
    xllm__json_builder_reset(&tVisibleText);

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
        size_t i;
        for ( i = 0; i < iPartCount; ++i ) {
            xllm__content_part_free(&pParts[i]);
        }
        xrtFree(pParts);
    }
    if ( psVisibleText && *psVisibleText ) {
        xllm__free_cstr(psVisibleText);
    }
    if ( psRefusalText && *psRefusalText ) {
        xllm__free_cstr(psRefusalText);
    }
    xllm__json_builder_reset(&tVisibleText);
    return XRT_NET_ERROR;
}

static int xllm__openai_apply_terminal_status(
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
        if ( strcmp(pResponse->sFinishReason, "tool_calls") == 0 ) {
            pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
            return XRT_NET_OK;
        }
        if ( strcmp(pResponse->sFinishReason, "length") == 0 ||
             strcmp(pResponse->sFinishReason, "max_tokens") == 0 ) {
            pResponse->eStatus = XLLM_STATUS_INCOMPLETE;
            return XRT_NET_OK;
        }
    }

    pResponse->eStatus = bDone ? XLLM_STATUS_COMPLETED : XLLM_STATUS_INCOMPLETE;
    return XRT_NET_OK;
}

static int xllm__openai_build_response(
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xvalue tRoot,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    xllm_response *pResponse;
    xvalue tChoices;
    xvalue tChoice;
    xvalue tMessage;
    xvalue tToolCalls;
    xvalue tUsage;
    const char *sFinishReason;
    const char *sReasoningText;
    char *sVisibleText = NULL;
    char *sRefusalText = NULL;
    char *sNormalizedJson = NULL;
    xllm_content_part *pMessageParts = NULL;
    size_t iMessagePartCount = 0u;
    size_t iToolCallCount = 0u;
    bool bJsonOutput = false;
    size_t iOutputCount = 0u;
    size_t iOutputIndex = 0u;
    xvalue tJsonValue = NULL;
    const char *sModel;
    xllm_effective_params tEffectiveParams;

    if ( !pProfile || !pRequest || !ppResponse || !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "invalid openai-compatible response");
        return XRT_NET_ERROR;
    }

    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    if ( xllm__openai_fill_effective_params(
            &tEffectiveParams,
            pProfile,
            pRequest,
            XLLM_STREAM_OFF
         ) != XRT_NET_OK ) {
        goto fail;
    }

    tChoices = xllm__json_table_get(tRoot, "choices");
    if ( !tChoices || xvoType(tChoices) != XVO_DT_ARRAY || xvoArrayItemCount(tChoices) == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible response missing choices");
        return XRT_NET_ERROR;
    }

    tChoice = xvoArrayGetValue(tChoices, 0u);
    if ( !tChoice || xvoType(tChoice) != XVO_DT_TABLE ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible choice payload is invalid");
        return XRT_NET_ERROR;
    }

    tMessage = xllm__json_table_get(tChoice, "message");
    if ( !tMessage || xvoType(tMessage) != XVO_DT_TABLE ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible response missing message");
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_parse_message_content(
            xllm__json_table_get(tMessage, "content"),
            &pMessageParts,
            &iMessagePartCount,
            &sVisibleText,
            &sRefusalText,
            pError
         ) != XRT_NET_OK ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible content");
        goto fail;
    }

    if ( !sRefusalText ) {
        const char *sMessageRefusal = xllm__json_table_get_text(tMessage, "refusal");
        if ( sMessageRefusal ) {
            sRefusalText = xllm__dup_cstr(sMessageRefusal);
        }
    }
    sReasoningText = xllm__json_table_get_text(tMessage, "reasoning_content");

    if ( tEffectiveParams.tResponseFormat.eKind != XLLM_RESPONSE_TEXT && sVisibleText && sVisibleText[0] ) {
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

    tToolCalls = xllm__json_table_get(tMessage, "tool_calls");
    if ( tToolCalls && xvoType(tToolCalls) == XVO_DT_ARRAY ) {
        iToolCallCount = (size_t)xvoArrayItemCount(tToolCalls);
    }

    if ( iMessagePartCount > 0u ) {
        ++iOutputCount;
    }
    if ( sReasoningText && sReasoningText[0] ) {
        ++iOutputCount;
    }
    iOutputCount += iToolCallCount;
    if ( sRefusalText && sRefusalText[0] ) {
        ++iOutputCount;
    }

    pResponse = (xllm_response *)xrtCalloc(1, sizeof(*pResponse));
    if ( !pResponse ) {
        goto fail;
    }

    pResponse->sId = xllm__dup_cstr(xllm__json_table_get_text(tRoot, "id"));
    pResponse->sProvider = xllm__dup_cstr(pProfile->sProvider ? pProfile->sProvider : "openai_compat");
    pResponse->sProfileId = xllm__dup_cstr(pProfile->sId);
    sModel = xllm__json_table_get_text(tRoot, "model");
    pResponse->sModel = xllm__dup_cstr(sModel ? sModel : xllm__openai_select_model(pProfile, pRequest, NULL));
    sFinishReason = xllm__json_table_get_text(tChoice, "finish_reason");
    pResponse->sFinishReason = xllm__dup_cstr(sFinishReason ? sFinishReason : "stop");
    pResponse->sVisibleText = xllm__dup_cstr((sRefusalText && sRefusalText[0]) ? sRefusalText : sVisibleText);

    if ( iOutputCount > 0u ) {
        pResponse->pOutputs = (xllm_output_item *)xrtCalloc(iOutputCount, sizeof(xllm_output_item));
        if ( !pResponse->pOutputs ) {
            xllm_response_free(pResponse);
            pResponse = NULL;
            goto fail;
        }
        pResponse->iOutputCount = iOutputCount;
    }

    if ( sReasoningText && sReasoningText[0] ) {
        xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];

        pOutput->eKind = XLLM_OUTPUT_THINKING;
        pOutput->as.tThinking.bVisible = true;
        pOutput->as.tThinking.sFormat = xllm__dup_cstr("full");
        pOutput->as.tThinking.sText = xllm__dup_cstr(sReasoningText);
        if ( !pOutput->as.tThinking.sFormat || !pOutput->as.tThinking.sText ) {
            goto fail;
        }
        if ( xllm__openai_set_reasoning_vendor_extra(&pOutput->as.tThinking, pProfile) != XRT_NET_OK ) {
            goto fail;
        }
    }

    if ( iMessagePartCount > 0u ) {
        xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];
        pOutput->eKind = XLLM_OUTPUT_MESSAGE;
        pOutput->as.tMessage.iPartCount = iMessagePartCount;
        pOutput->as.tMessage.pParts = pMessageParts;
        pMessageParts = NULL;

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

    if ( iToolCallCount > 0u ) {
        size_t i;
        for ( i = 0; i < iToolCallCount; ++i ) {
            xvalue tToolCall = xvoArrayGetValue(tToolCalls, (uint32)i);
            xvalue tFunction = xllm__json_table_get(tToolCall, "function");
            const char *sCallId = xllm__json_table_get_text(tToolCall, "id");
            const char *sToolName = xllm__json_table_get_text(tFunction, "name");
            const char *sArguments = xllm__json_table_get_text(tFunction, "arguments");
            xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];

            pOutput->eKind = XLLM_OUTPUT_TOOL_CALL;
            pOutput->as.tToolCall.sCallId = xllm__dup_cstr(sCallId);
            pOutput->as.tToolCall.sToolId = xllm__dup_cstr(sToolName);
            pOutput->as.tToolCall.sToolName = xllm__dup_cstr(sToolName);
            pOutput->as.tToolCall.sArgumentsJson = xllm__dup_cstr(sArguments ? sArguments : "{}");
        }
    }

    if ( sRefusalText && sRefusalText[0] ) {
        xllm_output_item *pOutput = &pResponse->pOutputs[iOutputIndex++];
        pOutput->eKind = XLLM_OUTPUT_REFUSAL;
        pOutput->as.tRefusal.sText = xllm__dup_cstr(sRefusalText);
        pResponse->tRefusal.sText = xllm__dup_cstr(sRefusalText);
    }

    if ( xllm__openai_apply_terminal_status(pResponse, true, false) != XRT_NET_OK ) {
        xllm_response_free(pResponse);
        pResponse = NULL;
        goto fail;
    }

    tUsage = xllm__json_table_get(tRoot, "usage");
    if ( tUsage && xvoType(tUsage) == XVO_DT_TABLE ) {
        xvalue tPromptDetails = xllm__json_table_get(tUsage, "prompt_tokens_details");
        xvalue tCompletionDetails = xllm__json_table_get(tUsage, "completion_tokens_details");
        pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tUsage, "prompt_tokens");
        pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tUsage, "completion_tokens");
        if ( tPromptDetails && xvoType(tPromptDetails) == XVO_DT_TABLE ) {
            pResponse->tUsage.uCachedInputTokens = xllm__json_table_get_u32(tPromptDetails, "cached_tokens");
        }
        if ( tCompletionDetails && xvoType(tCompletionDetails) == XVO_DT_TABLE ) {
            pResponse->tUsage.uReasoningTokens = xllm__json_table_get_u32(tCompletionDetails, "reasoning_tokens");
        }
    }

    pResponse->tEffectiveParams = tEffectiveParams;
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    pResponse->tRaw = tRoot;

    *ppResponse = pResponse;
    xllm__free_cstr(&sVisibleText);
    xllm__free_cstr(&sRefusalText);
    xllm__free_cstr(&sNormalizedJson);
    if ( pMessageParts ) {
        size_t i;
        for ( i = 0u; i < iMessagePartCount; ++i ) {
            xllm__content_part_free(&pMessageParts[i]);
        }
        xrtFree(pMessageParts);
    }
    if ( tJsonValue ) {
        xvoUnref(tJsonValue);
    }
    return XRT_NET_OK;

fail:
    xllm__free_cstr(&sVisibleText);
    xllm__free_cstr(&sRefusalText);
    xllm__free_cstr(&sNormalizedJson);
    if ( pMessageParts ) {
        size_t i;
        for ( i = 0u; i < iMessagePartCount; ++i ) {
            xllm__content_part_free(&pMessageParts[i]);
        }
        xrtFree(pMessageParts);
    }
    xllm__effective_params_reset(&tEffectiveParams);
    if ( tJsonValue ) {
        xvoUnref(tJsonValue);
    }
    return XRT_NET_ERROR;
}

static int xllm__append_owned_text(char **psTarget, const char *sDelta)
{
    size_t iOldLen;
    size_t iDeltaLen;
    char *sNew;

    if ( !psTarget || !sDelta || !sDelta[0] ) {
        return XRT_NET_OK;
    }

    iOldLen = (*psTarget) ? strlen(*psTarget) : 0u;
    iDeltaLen = strlen(sDelta);
    sNew = (char *)xrtRealloc(*psTarget, iOldLen + iDeltaLen + 1u);
    if ( !sNew ) {
        return XRT_NET_ERROR;
    }

    memcpy(sNew + iOldLen, sDelta, iDeltaLen + 1u);
    *psTarget = sNew;
    return XRT_NET_OK;
}

static int xllm__openai_fill_effective_params(
    xllm_effective_params *pOut,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    xllm_stream_mode eStreamMode
)
{
    return xllm__resolve_effective_params(pOut, pProfile, pRequest, eStreamMode);
}

static int xllm__openai_stream_dispatch(xllm__openai_stream_context *pCtx, xllm_event *pEvent)
{
    if ( !pCtx ) {
        return XRT_NET_ERROR;
    }

    if ( pCtx->pOptions && pCtx->pOptions->pCancelToken &&
         xllm_cancel_token_is_cancelled(pCtx->pOptions->pCancelToken) ) {
        pCtx->bCancelled = true;
        return XRT_NET_CANCELLED;
    }

    if ( !pCtx->pOptions || !pCtx->pOptions->pfnOnEvent ) {
        return XRT_NET_OK;
    }

    if ( !pCtx->pOptions->pfnOnEvent(pEvent, pCtx->pOptions->pUserData) ) {
        pCtx->bCancelled = true;
        return XRT_NET_CANCELLED;
    }

    return XRT_NET_OK;
}

static int xllm__openai_stream_ensure_response(xllm__openai_stream_context *pCtx)
{
    xllm_response *pResponse;

    if ( !pCtx ) {
        return XRT_NET_ERROR;
    }
    if ( pCtx->pResponse ) {
        return XRT_NET_OK;
    }

    pResponse = (xllm_response *)xrtCalloc(1, sizeof(*pResponse));
    if ( !pResponse ) {
        return XRT_NET_ERROR;
    }

    pResponse->sProvider = xllm__dup_cstr(
        (pCtx->pProfile && pCtx->pProfile->sProvider) ? pCtx->pProfile->sProvider : "openai_compat"
    );
    pResponse->sProfileId = xllm__dup_cstr(pCtx->pProfile ? pCtx->pProfile->sId : NULL);
    pResponse->sModel = xllm__dup_cstr(pCtx->sSelectedModel);
    pResponse->eStatus = XLLM_STATUS_INCOMPLETE;
    if ( xllm__openai_fill_effective_params(
            &pResponse->tEffectiveParams,
            pCtx->pProfile,
            pCtx->pRequest,
            (pCtx->pOptions ? pCtx->pOptions->eStreamMode : XLLM_STREAM_PREFER)
         ) != XRT_NET_OK ) {
        xllm_response_free(pResponse);
        return XRT_NET_ERROR;
    }

    pCtx->pResponse = pResponse;
    return XRT_NET_OK;
}

static int xllm__openai_stream_emit_start(xllm__openai_stream_context *pCtx)
{
    xllm_event tEvent;

    if ( !pCtx || pCtx->bStartEmitted ) {
        return XRT_NET_OK;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_START;
    tEvent.as.tStart.sResponseId = pCtx->pResponse ? pCtx->pResponse->sId : NULL;
    tEvent.as.tStart.sModel = (pCtx->pResponse && pCtx->pResponse->sModel) ? pCtx->pResponse->sModel : pCtx->sSelectedModel;
    if ( xllm__openai_stream_dispatch(pCtx, &tEvent) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    pCtx->bStartEmitted = true;
    return XRT_NET_OK;
}

static int xllm__openai_stream_append_output(
    xllm__openai_stream_context *pCtx,
    xllm_output_kind eKind,
    size_t *piIndex
)
{
    xllm_output_item tOutput;

    if ( !pCtx || !piIndex ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_response(pCtx) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    memset(&tOutput, 0, sizeof(tOutput));
    tOutput.eKind = eKind;
    if ( xllm__append_buffer(
            (void **)&pCtx->pResponse->pOutputs,
            sizeof(tOutput),
            &pCtx->pResponse->iOutputCount,
            &pCtx->iOutputCapacity,
            &tOutput
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    *piIndex = pCtx->pResponse->iOutputCount - 1u;
    return XRT_NET_OK;
}

static int xllm__openai_stream_emit_output_begin(
    xllm__openai_stream_context *pCtx,
    size_t iOutputIndex,
    xllm_output_kind eKind
)
{
    xllm_event tEvent;

    if ( xllm__openai_stream_emit_start(pCtx) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_OUTPUT_BEGIN;
    tEvent.uOutputIndex = (uint32)iOutputIndex;
    tEvent.as.tOutputBegin.eKind = eKind;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__openai_stream_emit_output_end(xllm__openai_stream_context *pCtx, size_t iOutputIndex)
{
    xllm_event tEvent;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_OUTPUT_END;
    tEvent.uOutputIndex = (uint32)iOutputIndex;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__openai_stream_ensure_message_output(
    xllm__openai_stream_context *pCtx,
    xllm_output_item **ppOutput
)
{
    xllm_output_item *pOutput;

    if ( !pCtx || !ppOutput ) {
        return XRT_NET_ERROR;
    }

    if ( pCtx->iMessageOutputIndex != (size_t)-1 ) {
        *ppOutput = &pCtx->pResponse->pOutputs[pCtx->iMessageOutputIndex];
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_append_output(pCtx, XLLM_OUTPUT_MESSAGE, &pCtx->iMessageOutputIndex) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    pOutput = &pCtx->pResponse->pOutputs[pCtx->iMessageOutputIndex];
    if ( xllm__openai_stream_emit_output_begin(pCtx, pCtx->iMessageOutputIndex, XLLM_OUTPUT_MESSAGE) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    *ppOutput = pOutput;
    return XRT_NET_OK;
}

static int xllm__openai_stream_ensure_message_text_part(
    xllm__openai_stream_context *pCtx,
    xllm_output_item **ppOutput,
    size_t *piPartIndex
)
{
    xllm_output_item *pOutput;
    size_t i;
    xllm_content_part tPart;

    if ( !pCtx || !ppOutput || !piPartIndex ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_message_output(pCtx, &pOutput) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0u; i < pOutput->as.tMessage.iPartCount; ++i ) {
        xllm_content_part *pPart = &pOutput->as.tMessage.pParts[i];
        if ( pPart->eKind == XLLM_PART_TEXT &&
             pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT ) {
            *ppOutput = pOutput;
            *piPartIndex = i;
            return XRT_NET_OK;
        }
    }

    memset(&tPart, 0, sizeof(tPart));
    tPart.eKind = XLLM_PART_TEXT;
    tPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    tPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
    tPart.as.tSource.as.sText = xllm__dup_cstr("");
    if ( !tPart.as.tSource.sMimeType || !tPart.as.tSource.as.sText ) {
        xllm__content_part_free(&tPart);
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_message_add_part(
            &pOutput->as.tMessage.pParts,
            &pOutput->as.tMessage.iPartCount,
            &pCtx->iMessagePartCapacity,
            &tPart
         ) != XRT_NET_OK ) {
        xllm__content_part_free(&tPart);
        return XRT_NET_ERROR;
    }

    *ppOutput = pOutput;
    *piPartIndex = pOutput->as.tMessage.iPartCount - 1u;
    return XRT_NET_OK;
}

static int xllm__openai_stream_emit_artifact_part(
    xllm__openai_stream_context *pCtx,
    size_t iOutputIndex,
    size_t iPartIndex,
    const xllm_content_part *pPart
)
{
    xllm_artifact_info tInfo;
    xllm_event tEvent;
    char sArtifactId[64];
    bool bSinkStarted = false;

    if ( !pCtx || !pPart ) {
        return XRT_NET_ERROR;
    }

    if ( pPart->eKind != XLLM_PART_IMAGE &&
         pPart->eKind != XLLM_PART_FILE &&
         pPart->eKind != XLLM_PART_AUDIO &&
         pPart->eKind != XLLM_PART_VIDEO ) {
        return XRT_NET_OK;
    }

    memset(&tInfo, 0, sizeof(tInfo));
    (void)snprintf(
        sArtifactId,
        sizeof(sArtifactId),
        "output_%u_part_%u",
        (unsigned)iOutputIndex,
        (unsigned)iPartIndex
    );
    tInfo.sArtifactId = sArtifactId;
    tInfo.sMimeType = pPart->as.tSource.sMimeType;
    tInfo.sName = pPart->as.tSource.sName;
    tInfo.uExpectedSize = (pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_BYTES)
                              ? (uint64)pPart->as.tSource.as.tBytes.iSize
                              : 0u;
    tInfo.uOutputIndex = (uint32)iOutputIndex;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_ARTIFACT_BEGIN;
    tEvent.uOutputIndex = (uint32)iOutputIndex;
    tEvent.as.tArtifactBegin.tInfo = tInfo;
    if ( xllm__openai_stream_dispatch(pCtx, &tEvent) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    if ( pCtx->pOptions && pCtx->pOptions->pArtifactSink && pCtx->pOptions->pArtifactSink->pfnBegin ) {
        bSinkStarted = pCtx->pOptions->pArtifactSink->pfnBegin(
            pCtx->pOptions->pArtifactSink->pCtx,
            &tInfo
        );
        if ( !bSinkStarted ) {
            return XRT_NET_CANCELLED;
        }
    }

    if ( pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_BYTES &&
         pPart->as.tSource.as.tBytes.pData &&
         pPart->as.tSource.as.tBytes.iSize > 0u ) {
        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_ARTIFACT_CHUNK;
        tEvent.uOutputIndex = (uint32)iOutputIndex;
        tEvent.as.tArtifactChunk.sArtifactId = sArtifactId;
        tEvent.as.tArtifactChunk.pData = pPart->as.tSource.as.tBytes.pData;
        tEvent.as.tArtifactChunk.iSize = pPart->as.tSource.as.tBytes.iSize;
        if ( xllm__openai_stream_dispatch(pCtx, &tEvent) != XRT_NET_OK ) {
            if ( pCtx->pOptions && pCtx->pOptions->pArtifactSink && pCtx->pOptions->pArtifactSink->pfnEnd && bSinkStarted ) {
                pCtx->pOptions->pArtifactSink->pfnEnd(
                    pCtx->pOptions->pArtifactSink->pCtx,
                    sArtifactId,
                    false
                );
            }
            return XRT_NET_CANCELLED;
        }

        if ( pCtx->pOptions && pCtx->pOptions->pArtifactSink && pCtx->pOptions->pArtifactSink->pfnWrite ) {
            if ( !pCtx->pOptions->pArtifactSink->pfnWrite(
                    pCtx->pOptions->pArtifactSink->pCtx,
                    sArtifactId,
                    pPart->as.tSource.as.tBytes.pData,
                    pPart->as.tSource.as.tBytes.iSize
                 ) ) {
                if ( pCtx->pOptions->pArtifactSink->pfnEnd && bSinkStarted ) {
                    pCtx->pOptions->pArtifactSink->pfnEnd(
                        pCtx->pOptions->pArtifactSink->pCtx,
                        sArtifactId,
                        false
                    );
                }
                return XRT_NET_CANCELLED;
            }
        }
    }

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_ARTIFACT_READY;
    tEvent.uOutputIndex = (uint32)iOutputIndex;
    tEvent.as.tArtifactReady.tInfo = tInfo;
    if ( xllm__openai_stream_dispatch(pCtx, &tEvent) != XRT_NET_OK ) {
        if ( pCtx->pOptions && pCtx->pOptions->pArtifactSink && pCtx->pOptions->pArtifactSink->pfnEnd && bSinkStarted ) {
            pCtx->pOptions->pArtifactSink->pfnEnd(
                pCtx->pOptions->pArtifactSink->pCtx,
                sArtifactId,
                false
            );
        }
        return XRT_NET_CANCELLED;
    }

    if ( pCtx->pOptions && pCtx->pOptions->pArtifactSink && pCtx->pOptions->pArtifactSink->pfnEnd && bSinkStarted ) {
        if ( !pCtx->pOptions->pArtifactSink->pfnEnd(
                pCtx->pOptions->pArtifactSink->pCtx,
                sArtifactId,
                true
             ) ) {
            return XRT_NET_CANCELLED;
        }
    }

    return XRT_NET_OK;
}

static int xllm__openai_stream_append_message_part(
    xllm__openai_stream_context *pCtx,
    xllm_content_part *pPart
)
{
    xllm_output_item *pOutput;
    size_t iPartIndex;

    if ( !pCtx || !pPart ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_message_output(pCtx, &pOutput) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__openai_message_add_part(
            &pOutput->as.tMessage.pParts,
            &pOutput->as.tMessage.iPartCount,
            &pCtx->iMessagePartCapacity,
            pPart
         ) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    iPartIndex = pOutput->as.tMessage.iPartCount - 1u;
    return xllm__openai_stream_emit_artifact_part(
        pCtx,
        pCtx->iMessageOutputIndex,
        iPartIndex,
        &pOutput->as.tMessage.pParts[iPartIndex]
    );
}

static int xllm__openai_stream_append_text(xllm__openai_stream_context *pCtx, const char *sDelta)
{
    xllm_output_item *pOutput;
    xllm_event tEvent;
    size_t iPartIndex;

    if ( !pCtx || !sDelta || !sDelta[0] ) {
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_ensure_message_text_part(pCtx, &pOutput, &iPartIndex) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__append_owned_text((char **)&pOutput->as.tMessage.pParts[iPartIndex].as.tSource.as.sText, sDelta) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    ++pCtx->uTextDeltaCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_TEXT_DELTA;
    tEvent.uOutputIndex = (uint32)pCtx->iMessageOutputIndex;
    tEvent.as.tTextDelta.sText = sDelta;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__openai_stream_ensure_thinking_output(
    xllm__openai_stream_context *pCtx,
    xllm_output_item **ppOutput
)
{
    xllm_output_item *pOutput;

    if ( !pCtx || !ppOutput ) {
        return XRT_NET_ERROR;
    }

    if ( pCtx->iThinkingOutputIndex != (size_t)-1 ) {
        *ppOutput = &pCtx->pResponse->pOutputs[pCtx->iThinkingOutputIndex];
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_append_output(pCtx, XLLM_OUTPUT_THINKING, &pCtx->iThinkingOutputIndex) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    pOutput = &pCtx->pResponse->pOutputs[pCtx->iThinkingOutputIndex];
    pOutput->as.tThinking.bVisible = true;
    pOutput->as.tThinking.sFormat = xllm__dup_cstr("full");
    pOutput->as.tThinking.sText = xllm__dup_cstr("");
    if ( !pOutput->as.tThinking.sFormat || !pOutput->as.tThinking.sText ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_emit_output_begin(pCtx, pCtx->iThinkingOutputIndex, XLLM_OUTPUT_THINKING) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    *ppOutput = pOutput;
    return XRT_NET_OK;
}

static int xllm__openai_stream_append_thinking(xllm__openai_stream_context *pCtx, const char *sDelta)
{
    xllm_output_item *pOutput;
    xllm_event tEvent;

    if ( !pCtx || !sDelta || !sDelta[0] ) {
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_ensure_thinking_output(pCtx, &pOutput) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__append_owned_text((char **)&pOutput->as.tThinking.sText, sDelta) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    ++pCtx->uThinkingDeltaCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_THINKING_DELTA;
    tEvent.uOutputIndex = (uint32)pCtx->iThinkingOutputIndex;
    tEvent.as.tThinkingDelta.sText = sDelta;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__openai_stream_append_reasoning_content(
    xllm__openai_stream_context *pCtx,
    const char *sDelta
)
{
    xllm_output_item *pOutput;

    if ( !pCtx || !sDelta || !sDelta[0] ) {
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_ensure_thinking_output(pCtx, &pOutput) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__openai_set_reasoning_vendor_extra(&pOutput->as.tThinking, pCtx->pProfile) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    return xllm__openai_stream_append_thinking(pCtx, sDelta);
}

static int xllm__openai_stream_ensure_refusal_output(
    xllm__openai_stream_context *pCtx,
    xllm_output_item **ppOutput
)
{
    xllm_output_item *pOutput;

    if ( !pCtx || !ppOutput ) {
        return XRT_NET_ERROR;
    }

    if ( pCtx->iRefusalOutputIndex != (size_t)-1 ) {
        *ppOutput = &pCtx->pResponse->pOutputs[pCtx->iRefusalOutputIndex];
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_append_output(pCtx, XLLM_OUTPUT_REFUSAL, &pCtx->iRefusalOutputIndex) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    pOutput = &pCtx->pResponse->pOutputs[pCtx->iRefusalOutputIndex];
    pOutput->as.tRefusal.sText = xllm__dup_cstr("");
    pCtx->pResponse->tRefusal.sText = xllm__dup_cstr("");
    if ( !pOutput->as.tRefusal.sText || !pCtx->pResponse->tRefusal.sText ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_emit_output_begin(pCtx, pCtx->iRefusalOutputIndex, XLLM_OUTPUT_REFUSAL) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    *ppOutput = pOutput;
    return XRT_NET_OK;
}

static int xllm__openai_stream_append_refusal(xllm__openai_stream_context *pCtx, const char *sText)
{
    xllm_output_item *pOutput;
    xllm_event tEvent;

    if ( !pCtx || !sText || !sText[0] ) {
        return XRT_NET_OK;
    }

    if ( xllm__openai_stream_ensure_refusal_output(pCtx, &pOutput) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__append_owned_text((char **)&pOutput->as.tRefusal.sText, sText) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__append_owned_text((char **)&pCtx->pResponse->tRefusal.sText, sText) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    ++pCtx->uRefusalCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_REFUSAL;
    tEvent.uOutputIndex = (uint32)pCtx->iRefusalOutputIndex;
    tEvent.as.tRefusal.tRefusal = pOutput->as.tRefusal;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__openai_stream_ensure_tool_output_slot(
    xllm__openai_stream_context *pCtx,
    size_t iToolIndex,
    xllm_output_item **ppOutput,
    size_t *piOutputIndex
)
{
    xllm_output_item *pOutput;
    size_t *pNew;
    size_t i;

    if ( !pCtx || !ppOutput || !piOutputIndex ) {
        return XRT_NET_ERROR;
    }

    if ( iToolIndex >= pCtx->iToolOutputIndexCount ) {
        size_t iNewCount = iToolIndex + 1u;
        if ( iNewCount > pCtx->iToolOutputIndexCapacity ) {
            size_t iNewCap = pCtx->iToolOutputIndexCapacity ? pCtx->iToolOutputIndexCapacity : 4u;
            while ( iNewCap < iNewCount ) {
                iNewCap *= 2u;
            }
            pNew = (size_t *)xrtRealloc(pCtx->pToolOutputIndices, iNewCap * sizeof(size_t));
            if ( !pNew ) {
                return XRT_NET_ERROR;
            }
            pCtx->pToolOutputIndices = pNew;
            pCtx->iToolOutputIndexCapacity = iNewCap;
        }

        for ( i = pCtx->iToolOutputIndexCount; i < iNewCount; ++i ) {
            pCtx->pToolOutputIndices[i] = (size_t)-1;
        }
        pCtx->iToolOutputIndexCount = iNewCount;
    }

    if ( pCtx->pToolOutputIndices[iToolIndex] == (size_t)-1 ) {
        size_t iOutputIndex;
        if ( xllm__openai_stream_append_output(pCtx, XLLM_OUTPUT_TOOL_CALL, &iOutputIndex) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        pCtx->pToolOutputIndices[iToolIndex] = iOutputIndex;
        pOutput = &pCtx->pResponse->pOutputs[iOutputIndex];
        pOutput->as.tToolCall.sArgumentsJson = xllm__dup_cstr("");
        if ( !pOutput->as.tToolCall.sArgumentsJson ) {
            return XRT_NET_ERROR;
        }

        if ( xllm__openai_stream_emit_output_begin(pCtx, iOutputIndex, XLLM_OUTPUT_TOOL_CALL) != XRT_NET_OK ) {
            return XRT_NET_CANCELLED;
        }
    }

    *piOutputIndex = pCtx->pToolOutputIndices[iToolIndex];
    *ppOutput = &pCtx->pResponse->pOutputs[*piOutputIndex];
    return XRT_NET_OK;
}

static int xllm__openai_stream_append_tool_delta(
    xllm__openai_stream_context *pCtx,
    size_t iToolIndex,
    const char *sCallId,
    const char *sToolNameDelta,
    const char *sArgumentsDelta
)
{
    xllm_output_item *pOutput;
    xllm_event tEvent;
    size_t iOutputIndex;

    if ( !pCtx ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_tool_output_slot(pCtx, iToolIndex, &pOutput, &iOutputIndex) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( sCallId && sCallId[0] && !pOutput->as.tToolCall.sCallId ) {
        pOutput->as.tToolCall.sCallId = xllm__dup_cstr(sCallId);
        if ( !pOutput->as.tToolCall.sCallId ) {
            return XRT_NET_ERROR;
        }
    }
    if ( sToolNameDelta && sToolNameDelta[0] ) {
        if ( xllm__append_owned_text((char **)&pOutput->as.tToolCall.sToolName, sToolNameDelta) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
        if ( xllm__append_owned_text((char **)&pOutput->as.tToolCall.sToolId, sToolNameDelta) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }
    if ( sArgumentsDelta && sArgumentsDelta[0] ) {
        if ( xllm__append_owned_text((char **)&pOutput->as.tToolCall.sArgumentsJson, sArgumentsDelta) != XRT_NET_OK ) {
            return XRT_NET_ERROR;
        }
    }
    ++pCtx->uToolDeltaCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_TOOL_CALL_DELTA;
    tEvent.uOutputIndex = (uint32)iOutputIndex;
    tEvent.as.tToolCallDelta.sCallId = sCallId ? sCallId : pOutput->as.tToolCall.sCallId;
    tEvent.as.tToolCallDelta.sToolId = sToolNameDelta ? sToolNameDelta : NULL;
    tEvent.as.tToolCallDelta.sToolName = sToolNameDelta ? sToolNameDelta : NULL;
    tEvent.as.tToolCallDelta.sArgumentsDelta = sArgumentsDelta;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__openai_stream_apply_usage(xllm__openai_stream_context *pCtx, xvalue tUsage)
{
    xllm_event tEvent;
    xvalue tPromptDetails;
    xvalue tCompletionDetails;

    if ( !pCtx || !pCtx->pResponse || !tUsage || xvoType(tUsage) != XVO_DT_TABLE ) {
        return XRT_NET_OK;
    }

    pCtx->pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tUsage, "prompt_tokens");
    pCtx->pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tUsage, "completion_tokens");
    tPromptDetails = xllm__json_table_get(tUsage, "prompt_tokens_details");
    tCompletionDetails = xllm__json_table_get(tUsage, "completion_tokens_details");
    if ( tPromptDetails && xvoType(tPromptDetails) == XVO_DT_TABLE ) {
        pCtx->pResponse->tUsage.uCachedInputTokens = xllm__json_table_get_u32(tPromptDetails, "cached_tokens");
    }
    if ( tCompletionDetails && xvoType(tCompletionDetails) == XVO_DT_TABLE ) {
        pCtx->pResponse->tUsage.uReasoningTokens = xllm__json_table_get_u32(tCompletionDetails, "reasoning_tokens");
    }
    ++pCtx->uUsageCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_USAGE;
    tEvent.as.tUsage.tUsage = pCtx->pResponse->tUsage;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__openai_stream_process_payload(
    xllm__openai_stream_context *pCtx,
    const char *sPayload,
    size_t iPayloadLen
)
{
    xvalue tRoot = NULL;
    xvalue tChoices;
    size_t i;

    if ( !pCtx || !sPayload ) {
        return XRT_NET_ERROR;
    }

    if ( iPayloadLen == 6u && memcmp(sPayload, "[DONE]", 6u) == 0 ) {
        pCtx->bDone = true;
        ++pCtx->uPayloadCount;
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "done", iPayloadLen);
        return XRT_NET_OK;
    }

    tRoot = xrtParseJSON((str)sPayload, iPayloadLen);
    if ( !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible stream event");
        if ( tRoot ) {
            xvoUnref(tRoot);
        }
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_response(pCtx) != XRT_NET_OK ) {
        xvoUnref(tRoot);
        return XRT_NET_ERROR;
    }

    if ( !pCtx->pResponse->sId ) {
        const char *sId = xllm__json_table_get_text(tRoot, "id");
        if ( sId ) {
            pCtx->pResponse->sId = xllm__dup_cstr(sId);
        }
    }
    if ( !pCtx->pResponse->sModel ) {
        const char *sModel = xllm__json_table_get_text(tRoot, "model");
        pCtx->pResponse->sModel = xllm__dup_cstr(sModel ? sModel : pCtx->sSelectedModel);
    }

    if ( xllm__openai_stream_emit_start(pCtx) != XRT_NET_OK ) {
        xvoUnref(tRoot);
        return XRT_NET_CANCELLED;
    }
    ++pCtx->uPayloadCount;
    if ( xllm__openai_stream_apply_usage(pCtx, xllm__json_table_get(tRoot, "usage")) != XRT_NET_OK ) {
        xvoUnref(tRoot);
        return XRT_NET_CANCELLED;
    }

    tChoices = xllm__json_table_get(tRoot, "choices");
    if ( tChoices && xvoType(tChoices) == XVO_DT_ARRAY ) {
        for ( i = 0; i < (size_t)xvoArrayItemCount(tChoices); ++i ) {
            xvalue tChoice = xvoArrayGetValue(tChoices, (uint32)i);
            xvalue tDelta;
            const char *sFinishReason;

            if ( !tChoice || xvoType(tChoice) != XVO_DT_TABLE ) {
                continue;
            }

            sFinishReason = xllm__json_table_get_text(tChoice, "finish_reason");
            if ( sFinishReason && sFinishReason[0] ) {
                xllm__free_cstr((char **)&pCtx->pResponse->sFinishReason);
                pCtx->pResponse->sFinishReason = xllm__dup_cstr(sFinishReason);
                pCtx->bDone = true;
            }

            tDelta = xllm__json_table_get(tChoice, "delta");
            if ( tDelta && xvoType(tDelta) == XVO_DT_TABLE ) {
                xllm_content_part *pMessageParts = NULL;
                size_t iMessagePartCount = 0u;
                char *sRefusal = NULL;
                char *sVisibleText = NULL;
                xvalue tToolCalls;
                size_t j;

                if ( xllm__openai_parse_message_content(
                        xllm__json_table_get(tDelta, "content"),
                        &pMessageParts,
                        &iMessagePartCount,
                        &sVisibleText,
                        &sRefusal,
                        pCtx->pError
                     ) != XRT_NET_OK ) {
                    xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible stream delta");
                    xllm__free_cstr(&sVisibleText);
                    xllm__free_cstr(&sRefusal);
                    xvoUnref(tRoot);
                    return XRT_NET_ERROR;
                }

                if ( !sRefusal ) {
                    const char *sDeltaRefusal = xllm__json_table_get_text(tDelta, "refusal");
                    if ( sDeltaRefusal ) {
                        sRefusal = xllm__dup_cstr(sDeltaRefusal);
                    }
                }
                {
                    const char *sReasoningContent = xllm__json_table_get_text(tDelta, "reasoning_content");

                    if ( sReasoningContent &&
                         xllm__openai_stream_append_reasoning_content(pCtx, sReasoningContent) != XRT_NET_OK ) {
                        xllm__free_cstr(&sVisibleText);
                        xllm__free_cstr(&sRefusal);
                        if ( pMessageParts ) {
                            for ( j = 0u; j < iMessagePartCount; ++j ) {
                                xllm__content_part_free(&pMessageParts[j]);
                            }
                            xrtFree(pMessageParts);
                        }
                        xvoUnref(tRoot);
                        return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                    }
                }

                if ( sVisibleText && xllm__openai_stream_append_text(pCtx, sVisibleText) != XRT_NET_OK ) {
                    xllm__free_cstr(&sVisibleText);
                    xllm__free_cstr(&sRefusal);
                    if ( pMessageParts ) {
                        for ( j = 0u; j < iMessagePartCount; ++j ) {
                            xllm__content_part_free(&pMessageParts[j]);
                        }
                        xrtFree(pMessageParts);
                    }
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }
                if ( sRefusal && xllm__openai_stream_append_refusal(pCtx, sRefusal) != XRT_NET_OK ) {
                    xllm__free_cstr(&sVisibleText);
                    xllm__free_cstr(&sRefusal);
                    if ( pMessageParts ) {
                        for ( j = 0u; j < iMessagePartCount; ++j ) {
                            xllm__content_part_free(&pMessageParts[j]);
                        }
                        xrtFree(pMessageParts);
                    }
                    xvoUnref(tRoot);
                    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                }

                xllm__free_cstr(&sVisibleText);
                xllm__free_cstr(&sRefusal);

                for ( j = 0u; j < iMessagePartCount; ++j ) {
                    if ( pMessageParts[j].eKind == XLLM_PART_TEXT ) {
                        continue;
                    }
                    if ( xllm__openai_stream_append_message_part(pCtx, &pMessageParts[j]) != XRT_NET_OK ) {
                        size_t k;
                        if ( pMessageParts ) {
                            for ( k = 0u; k < iMessagePartCount; ++k ) {
                                xllm__content_part_free(&pMessageParts[k]);
                            }
                            xrtFree(pMessageParts);
                        }
                        xvoUnref(tRoot);
                        return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                    }
                }
                if ( pMessageParts ) {
                    for ( j = 0u; j < iMessagePartCount; ++j ) {
                        xllm__content_part_free(&pMessageParts[j]);
                    }
                    xrtFree(pMessageParts);
                }

                tToolCalls = xllm__json_table_get(tDelta, "tool_calls");
                if ( tToolCalls && xvoType(tToolCalls) == XVO_DT_ARRAY ) {
                    for ( j = 0; j < (size_t)xvoArrayItemCount(tToolCalls); ++j ) {
                        xvalue tToolCall = xvoArrayGetValue(tToolCalls, (uint32)j);
                        xvalue tFunction;
                        xvalue tIndexValue;
                        size_t iToolIndex = j;
                        const char *sCallId;
                        const char *sToolName = NULL;
                        const char *sArguments = NULL;

                        if ( !tToolCall || xvoType(tToolCall) != XVO_DT_TABLE ) {
                            continue;
                        }

                        tIndexValue = xllm__json_table_get(tToolCall, "index");
                        if ( tIndexValue && xvoType(tIndexValue) == XVO_DT_INT ) {
                            iToolIndex = (size_t)xvoGetInt(tIndexValue);
                        }

                        sCallId = xllm__json_table_get_text(tToolCall, "id");
                        tFunction = xllm__json_table_get(tToolCall, "function");
                        if ( tFunction && xvoType(tFunction) == XVO_DT_TABLE ) {
                            sToolName = xllm__json_table_get_text(tFunction, "name");
                            sArguments = xllm__json_table_get_text(tFunction, "arguments");
                        }

                        if ( xllm__openai_stream_append_tool_delta(
                                pCtx,
                                iToolIndex,
                                sCallId,
                                sToolName,
                                sArguments
                             ) != XRT_NET_OK ) {
                            xvoUnref(tRoot);
                            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                        }
                    }
                }
            }
        }
    }

    xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "payload", iPayloadLen);
    xvoUnref(tRoot);
    return XRT_NET_OK;
}

static int xllm__openai_stream_process_event_block(
    xllm__openai_stream_context *pCtx,
    const char *sEvent,
    size_t iEventLen
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

    iStatus = xllm__openai_stream_process_payload(pCtx, tPayload.pData, tPayload.iLen);
    if ( iStatus == XRT_NET_OK ) {
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "event_block", tPayload.iLen);
    }
    xllm__json_builder_reset(&tPayload);
    return iStatus;
}

static int xllm__openai_stream_process_buffer(
    xllm__openai_stream_context *pCtx,
    const char *sBuffer,
    size_t iLen
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

        if ( xllm__openai_stream_process_event_block(pCtx, sBuffer + iCursor, iEventEnd - iCursor) != XRT_NET_OK ) {
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }
        iCursor = iEventEnd + iDelimiterLen;
        pCtx->iParsedBytes = iCursor;
    }

    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_OK;
}

static int xllm__openai_stream_finalize_response(xllm__openai_stream_context *pCtx)
{
    size_t i;

    if ( !pCtx || !pCtx->pResponse ) {
        return XRT_NET_OK;
    }

    if ( pCtx->iMessageOutputIndex != (size_t)-1 ) {
        xllm_output_item *pOutput = &pCtx->pResponse->pOutputs[pCtx->iMessageOutputIndex];
        xllm_content_part *pJsonTargetPart = NULL;
        xllm__json_builder tVisibleText;
        const char *sText = NULL;
        char *sNormalizedJson = NULL;
        size_t i;

        memset(&tVisibleText, 0, sizeof(tVisibleText));
        for ( i = 0u; i < pOutput->as.tMessage.iPartCount; ++i ) {
            xllm_content_part *pPart = &pOutput->as.tMessage.pParts[i];
            if ( pPart->eKind == XLLM_PART_TEXT &&
                 pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT ) {
                const char *sPartText = pPart->as.tSource.as.sText;
                if ( !pJsonTargetPart ) {
                    pJsonTargetPart = pPart;
                }
                if ( sPartText && sPartText[0] ) {
                    if ( tVisibleText.iLen > 0u && !xllm__json_builder_append_char(&tVisibleText, '\n') ) {
                        xllm__json_builder_reset(&tVisibleText);
                        return XRT_NET_ERROR;
                    }
                    if ( !xllm__json_builder_append_cstr(&tVisibleText, sPartText) ) {
                        xllm__json_builder_reset(&tVisibleText);
                        return XRT_NET_ERROR;
                    }
                }
            }
        }
        sText = tVisibleText.pData;

        xllm__free_cstr((char **)&pCtx->pResponse->sVisibleText);
        pCtx->pResponse->sVisibleText = xllm__json_builder_detach(&tVisibleText);

        if ( pCtx->pResponse->tEffectiveParams.tResponseFormat.eKind != XLLM_RESPONSE_TEXT &&
             pJsonTargetPart &&
             sText && sText[0] ) {
            xvalue tJsonValue = NULL;
            if ( xllm__openai_parse_structured_output(
                    &pCtx->pResponse->tEffectiveParams.tResponseFormat,
                    pCtx->pOptions,
                    sText,
                    &tJsonValue,
                    &sNormalizedJson,
                    pCtx->pError
                 ) != XRT_NET_OK ) {
                xllm__free_cstr(&sNormalizedJson);
                return XRT_NET_ERROR;
            }
            if ( tJsonValue ) {
                xllm__content_part_free(pJsonTargetPart);
                memset(pJsonTargetPart, 0, sizeof(*pJsonTargetPart));
                pJsonTargetPart->eKind = XLLM_PART_JSON;
                pJsonTargetPart->as.tJsonValue = tJsonValue;
                if ( sNormalizedJson ) {
                    xllm__free_cstr((char **)&pCtx->pResponse->sVisibleText);
                    pCtx->pResponse->sVisibleText = sNormalizedJson;
                    sNormalizedJson = NULL;
                }
            }
        }
        xllm__free_cstr(&sNormalizedJson);
    }

    if ( !pCtx->pResponse->sFinishReason ) {
        pCtx->pResponse->sFinishReason = xllm__dup_cstr(
            pCtx->bCancelled ? "cancelled" : (pCtx->bDone ? "stop" : "incomplete")
        );
    }

    if ( pCtx->bCancelled ) {
        pCtx->pResponse->eStatus = XLLM_STATUS_CANCELLED;
    } else if ( pCtx->pResponse->tRefusal.sText && pCtx->pResponse->tRefusal.sText[0] ) {
        xllm__free_cstr((char **)&pCtx->pResponse->sVisibleText);
        pCtx->pResponse->sVisibleText = xllm__dup_cstr(pCtx->pResponse->tRefusal.sText);
        if ( !pCtx->pResponse->sVisibleText ) {
            return XRT_NET_ERROR;
        }
    }

    if ( xllm__openai_apply_terminal_status(pCtx->pResponse, pCtx->bDone, pCtx->bCancelled) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( pCtx->pOptions && pCtx->pOptions->pfnOnEvent ) {
        for ( i = 0; i < pCtx->pResponse->iOutputCount; ++i ) {
            xllm_output_item *pOutput = &pCtx->pResponse->pOutputs[i];

            if ( pOutput->eKind == XLLM_OUTPUT_TOOL_CALL ) {
                xllm_event tToolReady;
                if ( !pOutput->as.tToolCall.sArgumentsJson || !pOutput->as.tToolCall.sArgumentsJson[0] ) {
                    xllm__free_cstr((char **)&pOutput->as.tToolCall.sArgumentsJson);
                    pOutput->as.tToolCall.sArgumentsJson = xllm__dup_cstr("{}");
                }
                memset(&tToolReady, 0, sizeof(tToolReady));
                tToolReady.eType = XLLM_EVENT_TOOL_CALL_READY;
                tToolReady.uOutputIndex = (uint32)i;
                tToolReady.as.tToolCallReady.tToolCall = pOutput->as.tToolCall;
                if ( xllm__openai_stream_dispatch(pCtx, &tToolReady) != XRT_NET_OK ) {
                    return XRT_NET_CANCELLED;
                }
            }

            if ( xllm__openai_stream_emit_output_end(pCtx, i) != XRT_NET_OK ) {
                return XRT_NET_CANCELLED;
            }
        }

        {
            xllm_event tEnd;
            memset(&tEnd, 0, sizeof(tEnd));
            tEnd.eType = XLLM_EVENT_END;
            if ( xllm__openai_stream_dispatch(pCtx, &tEnd) != XRT_NET_OK ) {
                return XRT_NET_CANCELLED;
            }
        }
    }

    xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "finalize", 0u);
    return XRT_NET_OK;
}

static int xllm__openai_build_chat_body(
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
    if ( !pBody || !pRequest || !pEffectiveParams || !sModel ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__json_builder_append_char(pBody, '{') ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_cstr(pBody, "\"model\":") ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_escaped(pBody, sModel) ) return XRT_NET_ERROR;
    if ( !xllm__json_builder_append_cstr(pBody, bStream ? ",\"stream\":true," : ",\"stream\":false,") ) return XRT_NET_ERROR;
    if ( xllm__openai_append_context_messages(pBody, pRuntime, pProfile, pRequest, pOptions, pError) != XRT_NET_OK ) return XRT_NET_ERROR;

    if ( pEffectiveParams->tGeneration.tTemperature.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"temperature\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTemperature.fValue) ) return XRT_NET_ERROR;
    }
    if ( pEffectiveParams->tGeneration.tTopP.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"top_p\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_f64(pBody, pEffectiveParams->tGeneration.tTopP.fValue) ) return XRT_NET_ERROR;
    }
    if ( pEffectiveParams->tGeneration.tMaxOutputTokens.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"max_tokens\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_u32(pBody, pEffectiveParams->tGeneration.tMaxOutputTokens.iValue) ) return XRT_NET_ERROR;
    }
    if ( pEffectiveParams->tGeneration.tSeed.bSet ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"seed\":") ) return XRT_NET_ERROR;
        if ( !xllm__json_builder_append_u32(pBody, pEffectiveParams->tGeneration.tSeed.iValue) ) return XRT_NET_ERROR;
    }
    {
        const char *sReasoningEffort = xllm__openai_reasoning_effort_name(&pEffectiveParams->tReasoning);
        if ( sReasoningEffort && sReasoningEffort[0] ) {
            if ( !xllm__json_builder_append_cstr(pBody, ",\"reasoning_effort\":") ) return XRT_NET_ERROR;
            if ( !xllm__json_builder_append_escaped(pBody, sReasoningEffort) ) return XRT_NET_ERROR;
        }
    }
    if ( xllm__openai_append_stop(pBody, &pEffectiveParams->tGeneration) != XRT_NET_OK ) return XRT_NET_ERROR;
    if ( pRequest->iToolCount > 0u ) {
        if ( xllm__openai_append_tools(pBody, pRequest) != XRT_NET_OK ) return XRT_NET_ERROR;
        if ( xllm__openai_append_tool_policy(pBody, pRequest) != XRT_NET_OK ) return XRT_NET_ERROR;
    }
    if ( xllm__openai_should_send_native_response_format(
            pProfile,
            pRequest,
            &pEffectiveParams->tResponseFormat,
            pOptions
         ) ) {
        if ( xllm__openai_append_response_format(pBody, &pEffectiveParams->tResponseFormat) != XRT_NET_OK ) return XRT_NET_ERROR;
    }
    if ( bStream ) {
        if ( !xllm__json_builder_append_cstr(pBody, ",\"stream_options\":{\"include_usage\":true}") ) return XRT_NET_ERROR;
    }
    if ( !xllm__json_builder_append_char(pBody, '}') ) return XRT_NET_ERROR;

    return XRT_NET_OK;
}

static int xllm__openai_emit_synthetic_events(const xllm_response *pResponse, const xllm_call_options *pOptions)
{
    xllm_event tEvent;
    size_t i;

    if ( !pResponse || !pOptions ) {
        return XRT_NET_OK;
    }

    if ( pOptions->pfnOnEvent ) {
        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_START;
        tEvent.bSynthetic = true;
        tEvent.as.tStart.sResponseId = pResponse->sId;
        tEvent.as.tStart.sModel = pResponse->sModel;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            return XRT_NET_CANCELLED;
        }
    }

    for ( i = 0; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];

        if ( pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
            return XRT_NET_CANCELLED;
        }

        if ( pOptions->pfnOnEvent ) {
            memset(&tEvent, 0, sizeof(tEvent));
            tEvent.eType = XLLM_EVENT_OUTPUT_BEGIN;
            tEvent.bSynthetic = true;
            tEvent.uOutputIndex = (uint32)i;
            tEvent.as.tOutputBegin.eKind = pOutput->eKind;
            if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                return XRT_NET_CANCELLED;
            }
        }

        switch ( pOutput->eKind ) {
            case XLLM_OUTPUT_MESSAGE:
                if ( pOutput->as.tMessage.iPartCount > 0u ) {
                    size_t j;
                    for ( j = 0; j < pOutput->as.tMessage.iPartCount; ++j ) {
                        const xllm_content_part *pPart = &pOutput->as.tMessage.pParts[j];
                        if ( pPart->eKind == XLLM_PART_TEXT &&
                             pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_TEXT ) {
                            memset(&tEvent, 0, sizeof(tEvent));
                            tEvent.eType = XLLM_EVENT_TEXT_DELTA;
                            tEvent.bSynthetic = true;
                            tEvent.uOutputIndex = (uint32)i;
                            tEvent.as.tTextDelta.sText = pPart->as.tSource.as.sText;
                            if ( pOptions->pfnOnEvent &&
                                 !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                                return XRT_NET_CANCELLED;
                            }
                        } else if ( pPart->eKind == XLLM_PART_IMAGE ||
                                    pPart->eKind == XLLM_PART_FILE ||
                                    pPart->eKind == XLLM_PART_AUDIO ||
                                    pPart->eKind == XLLM_PART_VIDEO ) {
                            char sArtifactId[64];
                            xllm_artifact_info tInfo;
                            bool bSinkStarted = false;

                            memset(&tInfo, 0, sizeof(tInfo));
                            (void)snprintf(
                                sArtifactId,
                                sizeof(sArtifactId),
                                "output_%u_part_%u",
                                (unsigned)i,
                                (unsigned)j
                            );
                            tInfo.sArtifactId = sArtifactId;
                            tInfo.sMimeType = pPart->as.tSource.sMimeType;
                            tInfo.sName = pPart->as.tSource.sName;
                            tInfo.uExpectedSize = (pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_BYTES)
                                                      ? (uint64)pPart->as.tSource.as.tBytes.iSize
                                                      : 0u;
                            tInfo.uOutputIndex = (uint32)i;

                            memset(&tEvent, 0, sizeof(tEvent));
                            tEvent.eType = XLLM_EVENT_ARTIFACT_BEGIN;
                            tEvent.bSynthetic = true;
                            tEvent.uOutputIndex = (uint32)i;
                            tEvent.as.tArtifactBegin.tInfo = tInfo;
                            if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                                return XRT_NET_CANCELLED;
                            }

                            if ( pOptions->pArtifactSink && pOptions->pArtifactSink->pfnBegin ) {
                                bSinkStarted = pOptions->pArtifactSink->pfnBegin(
                                    pOptions->pArtifactSink->pCtx,
                                    &tInfo
                                );
                                if ( !bSinkStarted ) {
                                    return XRT_NET_CANCELLED;
                                }
                            }

                            if ( pPart->as.tSource.eKind == XLLM_SOURCE_INLINE_BYTES &&
                                 pPart->as.tSource.as.tBytes.pData &&
                                 pPart->as.tSource.as.tBytes.iSize > 0u ) {
                                memset(&tEvent, 0, sizeof(tEvent));
                                tEvent.eType = XLLM_EVENT_ARTIFACT_CHUNK;
                                tEvent.bSynthetic = true;
                                tEvent.uOutputIndex = (uint32)i;
                                tEvent.as.tArtifactChunk.sArtifactId = sArtifactId;
                                tEvent.as.tArtifactChunk.pData = pPart->as.tSource.as.tBytes.pData;
                                tEvent.as.tArtifactChunk.iSize = pPart->as.tSource.as.tBytes.iSize;
                                if ( pOptions->pfnOnEvent &&
                                     !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                                    return XRT_NET_CANCELLED;
                                }

                                if ( pOptions->pArtifactSink && pOptions->pArtifactSink->pfnWrite ) {
                                    if ( !pOptions->pArtifactSink->pfnWrite(
                                            pOptions->pArtifactSink->pCtx,
                                            sArtifactId,
                                            pPart->as.tSource.as.tBytes.pData,
                                            pPart->as.tSource.as.tBytes.iSize
                                         ) ) {
                                        if ( pOptions->pArtifactSink->pfnEnd && bSinkStarted ) {
                                            pOptions->pArtifactSink->pfnEnd(
                                                pOptions->pArtifactSink->pCtx,
                                                sArtifactId,
                                                false
                                            );
                                        }
                                        return XRT_NET_CANCELLED;
                                    }
                                }
                            }

                            memset(&tEvent, 0, sizeof(tEvent));
                            tEvent.eType = XLLM_EVENT_ARTIFACT_READY;
                            tEvent.bSynthetic = true;
                            tEvent.uOutputIndex = (uint32)i;
                            tEvent.as.tArtifactReady.tInfo = tInfo;
                            if ( pOptions->pfnOnEvent &&
                                 !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                                if ( pOptions->pArtifactSink && pOptions->pArtifactSink->pfnEnd && bSinkStarted ) {
                                    pOptions->pArtifactSink->pfnEnd(
                                        pOptions->pArtifactSink->pCtx,
                                        sArtifactId,
                                        false
                                    );
                                }
                                return XRT_NET_CANCELLED;
                            }

                            if ( pOptions->pArtifactSink && pOptions->pArtifactSink->pfnEnd && bSinkStarted ) {
                                if ( !pOptions->pArtifactSink->pfnEnd(
                                        pOptions->pArtifactSink->pCtx,
                                        sArtifactId,
                                        true
                                     ) ) {
                                    return XRT_NET_CANCELLED;
                                }
                            }
                        }
                    }
                }
                break;
            case XLLM_OUTPUT_TOOL_CALL:
                memset(&tEvent, 0, sizeof(tEvent));
                tEvent.eType = XLLM_EVENT_TOOL_CALL_READY;
                tEvent.bSynthetic = true;
                tEvent.uOutputIndex = (uint32)i;
                tEvent.as.tToolCallReady.tToolCall = pOutput->as.tToolCall;
                if ( pOptions->pfnOnEvent &&
                     !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                    return XRT_NET_CANCELLED;
                }
                break;
            case XLLM_OUTPUT_REFUSAL:
                memset(&tEvent, 0, sizeof(tEvent));
                tEvent.eType = XLLM_EVENT_REFUSAL;
                tEvent.bSynthetic = true;
                tEvent.uOutputIndex = (uint32)i;
                tEvent.as.tRefusal.tRefusal = pOutput->as.tRefusal;
                if ( pOptions->pfnOnEvent &&
                     !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                    return XRT_NET_CANCELLED;
                }
                break;
            default:
                break;
        }

        if ( pOptions->pfnOnEvent ) {
            memset(&tEvent, 0, sizeof(tEvent));
            tEvent.eType = XLLM_EVENT_OUTPUT_END;
            tEvent.bSynthetic = true;
            tEvent.uOutputIndex = (uint32)i;
            if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
                return XRT_NET_CANCELLED;
            }
        }
    }

    if ( pOptions->pfnOnEvent ) {
        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_USAGE;
        tEvent.bSynthetic = true;
        tEvent.as.tUsage.tUsage = pResponse->tUsage;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            return XRT_NET_CANCELLED;
        }
    }

    if ( pOptions->pfnOnEvent ) {
        memset(&tEvent, 0, sizeof(tEvent));
        tEvent.eType = XLLM_EVENT_END;
        tEvent.bSynthetic = true;
        if ( !pOptions->pfnOnEvent(&tEvent, pOptions->pUserData) ) {
            return XRT_NET_CANCELLED;
        }
    }

    return XRT_NET_OK;
}

static int32 xllm__openai_compat_chat_stream_buffered(
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
    bool bMultimodal = false;
    bool bTreatAsSse = false;
    bool bParsedSse = false;
    bool bRetryable = false;
    int iStatus = XRT_NET_ERROR;
    xnet_result iNetStatus = XRT_NET_ERROR;
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
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for openai-compatible request");
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_fill_effective_params(
            &tEffectiveParams,
            pProfile,
            pRequest,
            pOptions ? pOptions->eStreamMode : XLLM_STREAM_PREFER
         ) != XRT_NET_OK ) goto fail;

            if ( xllm__openai_build_chat_body(
                    &tBody,
                    pRuntime,
                    pProfile,
                    pRequest,
                    &tEffectiveParams,
                    pOptions,
            sModel,
            true,
            pError
         ) != XRT_NET_OK ) goto fail;
    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) goto fail;

    sUrl = xllm__openai_build_url(pProfile->sBaseUrl);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "openai-compatible profile missing base url");
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
        "request start: model=%s streaming=true live=false attempt=%u/%u body_bytes=%u",
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
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "openai-compatible stream request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "openai-compatible stream request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    if ( !sRequestId ) {
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "request-id");
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__openai_fill_error_from_http(pError, pHttpResponse, tRoot, sRequestId);
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible stream response body is empty");
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
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }
        if ( tStream.iParsedBytes < pHttpResponse->iBodyLen ) {
            size_t iRemain = pHttpResponse->iBodyLen - tStream.iParsedBytes;
            if ( xllm__openai_stream_process_event_block(&tStream, pHttpResponse->pBody + tStream.iParsedBytes, iRemain) != XRT_NET_OK ) {
                if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
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
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }

        pResponse = tStream.pResponse;
        tStream.pResponse = NULL;
        if ( !pResponse ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible stream did not produce a response");
            goto fail;
        }

        *ppResponse = pResponse;
        pResponse = NULL;
        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_INFO,
            xllm__openai_family_component_name(pProfile),
            "response complete: model=%s streaming=true live=false attempt=%u status=%s outputs=%u",
            (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
            (unsigned)uAttempt,
            xllm__openai_response_status_name((*ppResponse)->eStatus),
            (unsigned)(*ppResponse)->iOutputCount
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
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "openai-compatible upstream did not return an SSE stream");
        goto fail;
    }

    tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible streaming fallback response");
        goto fail;
    }
    if ( xllm__openai_build_response(pProfile, pRequest, pOptions, tRoot, &pResponse, pError) != XRT_NET_OK ) {
        goto fail;
    }
    tRoot = NULL;
    if ( xllm__openai_emit_synthetic_events(pResponse, pOptions) != XRT_NET_OK ) {
        if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
        }
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__openai_family_component_name(pProfile),
        "response fallback complete: model=%s streaming=true live=false attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount
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
                "response failed: model=%s streaming=true live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
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
            if ( tStream.pResponse ) {
                xllm_response_free(tStream.pResponse);
                tStream.pResponse = NULL;
            }
            if ( pResponse ) {
                xllm_response_free(pResponse);
                pResponse = NULL;
            }
            xllm__openai_stream_reset_attempt_state(&tStream);
            if ( tRoot ) {
                xvoUnref(tRoot);
                tRoot = NULL;
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
            if ( pError ) {
                xllm_error_reset(pError);
            }
            if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
                if ( pError ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
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
            xllm__openai_family_component_name(pProfile),
            "response failed: model=%s streaming=true live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)uAttempt,
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
            uAttempt,
            bRetryable,
            true,
            false
        );
    }
    if ( tStream.pResponse ) {
        xllm_response_free(tStream.pResponse);
    }
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    if ( tStream.pToolOutputIndices ) {
        xrtFree(tStream.pToolOutputIndices);
    }
    if ( tRoot ) {
        xvoUnref(tRoot);
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

static int32 xllm__openai_compat_chat_stream_live(
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
    xllm__openai_live_transport tTransport;
    xllm__openai_live_decoder tDecoder;
    xllm_effective_params tEffectiveParams;
    xhttprequest tHttpRequest;
    xhttpresponse tSyntheticHttpResponse;
    xtlsconfig tTlsConfig;
    xnetconnectconfig tConnectConfig;
    xllm_response *pResponse = NULL;
    xnetengine *pEngine = NULL;
    char *sChunk = NULL;
    char *sBody = NULL;
    char *sUrl = NULL;
    const char *sModel;
    bool bMultimodal = false;
    bool bOwnEngine = false;
    bool bRetryable = false;
    int iStatus = XRT_NET_ERROR;
    xvalue tRoot = NULL;
    double fDeadlineSec = 0.0;
    double fIdleDeadlineSec = 0.0;
    size_t iChunkLen = 0u;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts = 1u;
    uint32 uConnectTimeoutMs = 0u;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }

    *ppResponse = NULL;
    memset(&tBody, 0, sizeof(tBody));
    memset(&tStream, 0, sizeof(tStream));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    memset(&tSyntheticHttpResponse, 0, sizeof(tSyntheticHttpResponse));
    memset(&tTlsConfig, 0, sizeof(tTlsConfig));
    xllm__openai_live_transport_init(&tTransport);
    xllm__openai_live_decoder_init(&tDecoder);
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
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for openai-compatible request");
        goto fail;
    }

    if ( xllm__openai_fill_effective_params(
            &tEffectiveParams,
            pProfile,
            pRequest,
            pOptions ? pOptions->eStreamMode : XLLM_STREAM_PREFER
         ) != XRT_NET_OK ) {
        goto fail;
    }

        if ( xllm__openai_build_chat_body(
                &tBody,
                pRuntime,
                pProfile,
                pRequest,
                &tEffectiveParams,
                pOptions,
            sModel,
            true,
            pError
         ) != XRT_NET_OK ) {
        goto fail;
    }
    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) {
        goto fail;
    }

    sUrl = xllm__openai_build_url(pProfile->sBaseUrl);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "openai-compatible profile missing base url");
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) {
        goto fail;
    }
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) {
        goto fail;
    }
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) {
        goto fail;
    }
    if ( xllm__openai_fill_request_headers(&tHttpRequest, pProfile) != XRT_NET_OK ) {
        goto fail;
    }
    if ( !xrtHttpRequestSetHeader(&tHttpRequest, "Accept", "text/event-stream") ) {
        goto fail;
    }
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) goto fail;

    pEngine = pRuntime ? pRuntime->pNetEngine : NULL;
    if ( !pEngine ) {
        xnetengineconfig tEngineConfig;
        xrtNetEngineConfigInit(&tEngineConfig);
        tEngineConfig.iWorkerCount = 1u;
        pEngine = xrtNetEngineCreate(&tEngineConfig);
        if ( !pEngine || xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
            if ( pEngine ) {
                xrtNetEngineDestroy(pEngine);
            }
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "openai-compatible realtime stream engine init failed");
            goto fail;
        }
        bOwnEngine = true;
    }

    xrtNetConnectConfigInit(&tConnectConfig);
    tConnectConfig.sHost = tHttpRequest.tURL.sHost;
    tConnectConfig.iPort = tHttpRequest.tURL.iPort;
    uConnectTimeoutMs = xllm__openai_resolve_connect_timeout_ms(pRuntime, pProfile);
    tConnectConfig.iConnectTimeoutMs = xllm__openai_resolve_live_connect_timeout_ms(uConnectTimeoutMs, &tHttpRequest);
    tConnectConfig.iRecvLimit = 1024u * 1024u;
    tConnectConfig.pProxy = tHttpRequest.pProxy;
    if ( tHttpRequest.tURL.bHttps ) {
        tTlsConfig.sHostName = tHttpRequest.tURL.sHost;
        tTlsConfig.bVerifyPeer = tHttpRequest.bVerifyPeer;
        tConnectConfig.pTlsConfig = &tTlsConfig;
    }

retry_execute:
    ++uAttempt;
    iStatus = XRT_NET_ERROR;
    bRetryable = false;
    fDeadlineSec = 0.0;
    fIdleDeadlineSec = 0.0;
    memset(&tSyntheticHttpResponse, 0, sizeof(tSyntheticHttpResponse));

    if ( !xllm__openai_build_http_request_bytes(&tHttpRequest, &tTransport.pRequestBytes, &tTransport.iRequestLen) ) {
        goto fail;
    }
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__openai_family_component_name(pProfile),
        "request start: model=%s streaming=true live=true attempt=%u/%u body_bytes=%u",
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
        true,
        uAttempt,
        strlen(sBody)
    );

    tTransport.pEngine = pEngine;
    tTransport.pMutex = xrtMutexCreate();
    tTransport.pCond = xrtCondCreate();
    if ( !tTransport.pMutex || !tTransport.pCond ) {
        goto fail;
    }

    tTransport.pStream = xrtNetStreamCreate(pEngine, xllm__openai_live_stream_events(), &tTransport);
    if ( !tTransport.pStream ) {
        goto fail;
    }

    if ( tHttpRequest.iTimeoutMs > 0u ) {
        fDeadlineSec = xrtTimer() + ((double)tHttpRequest.iTimeoutMs / 1000.0);
    }
    if ( tHttpRequest.iIdleTimeoutMs > 0u ) {
        fIdleDeadlineSec = xrtTimer() + ((double)tHttpRequest.iIdleTimeoutMs / 1000.0);
    }

    if ( xrtNetStreamConnect(tTransport.pStream, &tConnectConfig) != XRT_NET_OK ) {
        xllm__error_set(pError, XLLM_ERROR_NETWORK, "openai-compatible realtime stream connect failed");
        goto fail;
    }

    for ( ;; ) {
        bool bClosed = false;
        xnet_result iCloseReason = XRT_NET_OK;
        int iSysErr = 0;
        int iParseStatus;

        if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
            iStatus = XRT_NET_CANCELLED;
            goto fail;
        }

        if ( (fDeadlineSec > 0.0 && xrtTimer() >= fDeadlineSec) ||
             (fIdleDeadlineSec > 0.0 && xrtTimer() >= fIdleDeadlineSec) ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "openai-compatible realtime stream timed out");
            iStatus = XRT_NET_TIMEOUT;
            goto fail;
        }

        if ( xllm__openai_live_transport_take_incoming(&tTransport, &sChunk, &iChunkLen, &bClosed, &iCloseReason, &iSysErr) ) {
            if ( !xllm__json_builder_append_bytes(&tDecoder.tWire, sChunk, iChunkLen) ) {
                xrtFree(sChunk);
                goto fail;
            }
            xrtFree(sChunk);
            sChunk = NULL;
            iChunkLen = 0u;
            if ( tHttpRequest.iIdleTimeoutMs > 0u ) {
                fIdleDeadlineSec = xrtTimer() + ((double)tHttpRequest.iIdleTimeoutMs / 1000.0);
            }

            iParseStatus = xllm__openai_live_try_parse_headers(&tDecoder, bClosed);
            if ( iParseStatus == XRT_NET_ERROR ) {
                xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible stream headers");
                goto fail;
            }
            if ( iParseStatus == XRT_NET_OK ) {
                iParseStatus = xllm__openai_live_process_body(&tDecoder, &tStream, bClosed);
                if ( iParseStatus != XRT_NET_OK ) {
                    if ( tStream.bCancelled ) {
                        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
                        iStatus = XRT_NET_CANCELLED;
                    } else {
                        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible stream body");
                    }
                    goto fail;
                }
            }
        } else if ( iSysErr != 0 ) {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "openai-compatible realtime stream failed");
            goto fail;
        } else if ( bClosed ) {
            iParseStatus = xllm__openai_live_try_parse_headers(&tDecoder, true);
            if ( iParseStatus == XRT_NET_ERROR ) {
                xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible stream headers");
                goto fail;
            }
            if ( iParseStatus == XRT_NET_OK ) {
                iParseStatus = xllm__openai_live_process_body(&tDecoder, &tStream, true);
                if ( iParseStatus != XRT_NET_OK ) {
                    if ( tStream.bCancelled ) {
                        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
                        iStatus = XRT_NET_CANCELLED;
                    } else {
                        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible stream body");
                    }
                    goto fail;
                }
            }

            if ( !tDecoder.bBodyComplete ) {
                if ( iCloseReason == XRT_NET_CLOSED || iCloseReason == XRT_NET_OK ) {
                    break;
                }
                xllm__error_set(pError, XLLM_ERROR_NETWORK, "openai-compatible realtime stream closed unexpectedly");
                goto fail;
            }
        }

        if ( tDecoder.bBodyComplete ) {
            break;
        }

        if ( tTransport.pMutex && tTransport.pCond ) {
            uint32 uWaitMs = 100u;

            if ( fDeadlineSec > 0.0 || fIdleDeadlineSec > 0.0 ) {
                double fRemainSec = -1.0;
                if ( fDeadlineSec > 0.0 ) {
                    fRemainSec = fDeadlineSec - xrtTimer();
                }
                if ( fIdleDeadlineSec > 0.0 ) {
                    double fIdleRemainSec = fIdleDeadlineSec - xrtTimer();
                    if ( fRemainSec < 0.0 || fIdleRemainSec < fRemainSec ) {
                        fRemainSec = fIdleRemainSec;
                    }
                }
                if ( fRemainSec <= 0.0 ) {
                    xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "openai-compatible realtime stream timed out");
                    iStatus = XRT_NET_TIMEOUT;
                    goto fail;
                }
                uWaitMs = (uint32)(fRemainSec * 1000.0);
                if ( uWaitMs == 0u ) {
                    uWaitMs = 1u;
                } else if ( uWaitMs > 100u ) {
                    uWaitMs = 100u;
                }
            }

            xrtMutexLock(tTransport.pMutex);
            if ( tTransport.tIncoming.iLen == 0u && !tTransport.bClosed && tTransport.iSysErr == 0 ) {
                if ( fDeadlineSec > 0.0 || fIdleDeadlineSec > 0.0 ) {
                    (void)xrtCondWaitTimeout(tTransport.pCond, tTransport.pMutex, uWaitMs);
                } else {
                    xrtCondWait(tTransport.pCond, tTransport.pMutex);
                }
            }
            xrtMutexUnlock(tTransport.pMutex);
        }
    }

    tSyntheticHttpResponse.iStatusCode = tDecoder.uStatusCode;
    if ( tDecoder.uStatusCode >= 400u ) {
        tRoot = (tDecoder.tBody.pData && tDecoder.tBody.iLen > 0u)
            ? xrtParseJSON((str)tDecoder.tBody.pData, tDecoder.tBody.iLen)
            : NULL;
        xllm__openai_fill_error_from_http(
            pError,
            &tSyntheticHttpResponse,
            tRoot,
            tDecoder.sRequestId[0] ? tDecoder.sRequestId : NULL
        );
        goto fail;
    }

    if ( !tDecoder.bTreatAsSse && tDecoder.tBody.pData && tDecoder.tBody.iLen > 0u ) {
        tDecoder.bTreatAsSse = xllm__buffer_starts_with_sse_data(tDecoder.tBody.pData, tDecoder.tBody.iLen);
    }

    if ( tDecoder.bTreatAsSse ) {
        if ( tStream.iParsedBytes < tDecoder.tBody.iLen ) {
            size_t iRemain = tDecoder.tBody.iLen - tStream.iParsedBytes;
            if ( xllm__openai_stream_process_event_block(&tStream, tDecoder.tBody.pData + tStream.iParsedBytes, iRemain) != XRT_NET_OK ) {
                if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
                }
                iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                goto fail;
            }
            tStream.iParsedBytes = tDecoder.tBody.iLen;
        }

        if ( xllm__openai_stream_finalize_response(&tStream) != XRT_NET_OK ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }

        pResponse = tStream.pResponse;
        tStream.pResponse = NULL;
        if ( !pResponse ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible stream did not produce a response");
            goto fail;
        }

        *ppResponse = pResponse;
        pResponse = NULL;
        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_INFO,
            xllm__openai_family_component_name(pProfile),
            "response complete: model=%s streaming=true live=true attempt=%u status=%s outputs=%u",
            (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
            (unsigned)uAttempt,
            xllm__openai_response_status_name((*ppResponse)->eStatus),
            (unsigned)(*ppResponse)->iOutputCount
        );
        xllm__openai_trace_response(
            pRuntime,
            pProfile,
            *ppResponse,
            (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
            &tSyntheticHttpResponse,
            tDecoder.sRequestId[0] ? tDecoder.sRequestId : NULL,
            NULL,
            XRT_NET_OK,
            uAttempt,
            false,
            true,
            true
        );
        iStatus = XRT_NET_OK;
        goto fail;
    }

    if ( pOptions && pOptions->eStreamMode == XLLM_STREAM_REQUIRE ) {
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "openai-compatible upstream did not return an SSE stream");
        goto fail;
    }

    if ( !tDecoder.tBody.pData || tDecoder.tBody.iLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible realtime stream response body is empty");
        goto fail;
    }

    tRoot = xrtParseJSON((str)tDecoder.tBody.pData, tDecoder.tBody.iLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible streaming fallback response");
        goto fail;
    }
    if ( xllm__openai_build_response(pProfile, pRequest, pOptions, tRoot, &pResponse, pError) != XRT_NET_OK ) {
        goto fail;
    }
    tRoot = NULL;
    if ( xllm__openai_emit_synthetic_events(pResponse, pOptions) != XRT_NET_OK ) {
        if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
        }
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__openai_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__openai_family_component_name(pProfile),
        "response fallback complete: model=%s streaming=true live=true attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount
    );
    xllm__openai_trace_response(
        pRuntime,
        pProfile,
        *ppResponse,
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        &tSyntheticHttpResponse,
        tDecoder.sRequestId[0] ? tDecoder.sRequestId : NULL,
        NULL,
        XRT_NET_OK,
        uAttempt,
        false,
        true,
        true
    );
    iStatus = XRT_NET_OK;

fail:
    if ( iStatus != XRT_NET_OK ) {
        int32 iTraceTransportStatus = tDecoder.uStatusCode > 0u ? XRT_NET_OK : (int32)iStatus;
        int32 iHttpStatus = tSyntheticHttpResponse.iStatusCode > 0u
            ? (int32)tSyntheticHttpResponse.iStatusCode
            : (pError ? pError->iHttpStatus : 0);
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
                "response failed: model=%s streaming=true live=true attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
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
                (tSyntheticHttpResponse.iStatusCode > 0u) ? &tSyntheticHttpResponse : NULL,
                tDecoder.sRequestId[0] ? tDecoder.sRequestId : NULL,
                pError,
                iTraceTransportStatus,
                uAttempt,
                true,
                true,
                true
            );
            if ( sChunk ) {
                xrtFree(sChunk);
                sChunk = NULL;
            }
            iChunkLen = 0u;
            xllm__openai_live_transport_close_and_wait(&tTransport, XNET_CLOSE_F_ABORT, 100u);
            xllm__openai_live_decoder_reset(&tDecoder);
            xllm__openai_live_decoder_init(&tDecoder);
            xllm__openai_live_transport_reset(&tTransport);
            xllm__openai_live_transport_init(&tTransport);
            if ( pResponse ) {
                xllm_response_free(pResponse);
                pResponse = NULL;
            }
            xllm__openai_stream_reset_attempt_state(&tStream);
            if ( tRoot ) {
                xvoUnref(tRoot);
                tRoot = NULL;
            }
            memset(&tSyntheticHttpResponse, 0, sizeof(tSyntheticHttpResponse));
            xllm_error_reset(pError);
            if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible stream cancelled");
                iStatus = XRT_NET_CANCELLED;
            } else {
                xllm__openai_retry_sleep(uDelayMs);
                goto retry_execute;
            }
        }
        xllm__openai_logf(
            pRuntime,
            XLLM_LOG_WARN,
            xllm__openai_family_component_name(pProfile),
            "response failed: model=%s streaming=true live=true attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)uAttempt,
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
            (tSyntheticHttpResponse.iStatusCode > 0u) ? &tSyntheticHttpResponse : NULL,
            tDecoder.sRequestId[0] ? tDecoder.sRequestId : NULL,
            pError,
            iTraceTransportStatus,
            uAttempt,
            bRetryable,
            true,
            true
        );
    }
    if ( sChunk ) {
        xrtFree(sChunk);
    }
    xllm__openai_live_transport_close_and_wait(&tTransport, XNET_CLOSE_F_ABORT, 100u);
    if ( pResponse ) {
        xllm_response_free(pResponse);
    }
    xllm__openai_stream_reset_attempt_state(&tStream);
    if ( tRoot ) {
        xvoUnref(tRoot);
    }
    xllm__openai_live_decoder_reset(&tDecoder);
    xllm__openai_live_transport_reset(&tTransport);
    if ( bOwnEngine && pEngine ) {
        xrtNetEngineDestroy(pEngine);
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

static int32 xllm__openai_compat_chat(
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

    *ppResponse = NULL;
    memset(&tBody, 0, sizeof(tBody));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    xrtHttpRequestInit(&tHttpRequest);
    if ( pOptions && pOptions->uMaxRetries > 0u ) {
        uMaxAttempts += pOptions->uMaxRetries;
    }

    if ( pOptions &&
         (pOptions->eStreamMode == XLLM_STREAM_PREFER || pOptions->eStreamMode == XLLM_STREAM_REQUIRE) ) {
        if ( pOptions->pfnOnEvent ) {
            return xllm__openai_compat_chat_stream_live(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
        }
        return xllm__openai_compat_chat_stream_buffered(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
    }

    sModel = xllm__openai_select_model(pProfile, pRequest, &bMultimodal);
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for openai-compatible request");
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_fill_effective_params(
            &tEffectiveParams,
            pProfile,
            pRequest,
            XLLM_STREAM_OFF
         ) != XRT_NET_OK ) goto fail;

        if ( xllm__openai_build_chat_body(
                &tBody,
                pRuntime,
                pProfile,
                pRequest,
                &tEffectiveParams,
                pOptions,
            sModel,
            false,
            pError
         ) != XRT_NET_OK ) goto fail;

    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) goto fail;

    sUrl = xllm__openai_build_url(pProfile->sBaseUrl);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "openai-compatible profile missing base url");
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
        "request start: model=%s streaming=false live=false attempt=%u/%u body_bytes=%u",
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
        false,
        false,
        uAttempt,
        strlen(sBody)
    );

    pHttpResponse = xrtHttpExecuteSync(NULL, &tHttpRequest, &iNetStatus);
    if ( !pHttpResponse ) {
        if ( iNetStatus == XRT_NET_TIMEOUT ) {
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "openai-compatible request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "openai-compatible request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    if ( !sRequestId ) {
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "request-id");
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__openai_fill_error_from_http(pError, pHttpResponse, tRoot, sRequestId);
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "openai-compatible response body is empty");
        goto fail;
    }

    tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse openai-compatible response json");
        goto fail;
    }
    if ( xllm__openai_build_response(pProfile, pRequest, pOptions, tRoot, &pResponse, pError) != XRT_NET_OK ) {
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
        xllm__openai_family_component_name(pProfile),
        "response complete: model=%s streaming=false live=false attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        (unsigned)uAttempt,
        xllm__openai_response_status_name((*ppResponse)->eStatus),
        (unsigned)(*ppResponse)->iOutputCount
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
                "response failed: model=%s streaming=false live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
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
                false,
                false
            );
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
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "openai-compatible request cancelled");
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
            xllm__openai_family_component_name(pProfile),
            "response failed: model=%s streaming=false live=false attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)uAttempt,
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
            uAttempt,
            bRetryable,
            false,
            false
        );
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

XLLM_API int xllm_register_openai_compat_adapter(xllm_runtime *pRuntime)
{
    xllm_adapter tAdapter;

    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    memset(&tAdapter, 0, sizeof(tAdapter));
    tAdapter.sName = XLLM_ADAPTER_OPENAI_COMPAT;
    tAdapter.pCtx = pRuntime;
    tAdapter.pfnChat = xllm__openai_compat_chat;
    return xllm_register_adapter(pRuntime, &tAdapter);
}

