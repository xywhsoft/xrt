#include "xllm_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define xllm__popen _popen
#define xllm__pclose _pclose
#else
#include <unistd.h>
#define xllm__popen popen
#define xllm__pclose pclose
#endif

static const char *xllm__gemini_component_name(const xllm_profile *pProfile)
{
    if ( pProfile && pProfile->sAdapter &&
         strcmp(pProfile->sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return "xllm.vertex_gemini_native";
    }
    return "xllm.gemini_native";
}

static const char *xllm__gemini_provider_name(const xllm_profile *pProfile)
{
    if ( pProfile && pProfile->sProvider && pProfile->sProvider[0] ) {
        return pProfile->sProvider;
    }
    if ( pProfile && pProfile->sAdapter &&
         strcmp(pProfile->sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return "google_vertex";
    }
    return "google";
}

static xllm_response_status xllm__gemini_status_from_finish_reason(const char *sFinishReason);

static const char *xllm__gemini_reasoning_vendor_text(const xllm_reasoning_options *pReasoning, const char *sKey)
{
    if ( !pReasoning || !sKey || !pReasoning->tVendorExtra || xvoType(pReasoning->tVendorExtra) != XVO_DT_TABLE ) {
        return NULL;
    }
    return xllm__json_table_get_text(pReasoning->tVendorExtra, sKey);
}

static uint32 xllm__gemini_reasoning_budget_for_level(const xllm_reasoning_options *pReasoning)
{
    const char *sBudget = NULL;
    uint32 uBudget = 0u;

    if ( !pReasoning ) {
        return 0u;
    }

    sBudget = xllm__gemini_reasoning_vendor_text(pReasoning, "thinking_budget");
    if ( !sBudget ) {
        sBudget = xllm__gemini_reasoning_vendor_text(pReasoning, "thinkingBudget");
    }
    if ( sBudget && sBudget[0] ) {
        uBudget = (uint32)strtoul(sBudget, NULL, 10);
        return uBudget;
    }

    switch ( pReasoning->eLevel ) {
        case XLLM_REASONING_OFF:
            return 0u;
        case XLLM_REASONING_LOW:
            return 1024u;
        case XLLM_REASONING_MEDIUM:
            return 4096u;
        case XLLM_REASONING_HIGH:
            return 8192u;
        case XLLM_REASONING_DEFAULT:
        default:
            break;
    }

    if ( pReasoning->tEnabled.bSet && pReasoning->tEnabled.bValue ) {
        return 4096u;
    }
    return 0u;
}

static xvalue xllm__gemini_create_thinking_vendor_extra(const char *sSignature)
{
    xvalue tTable = xvoCreateTable();

    if ( !tTable ) {
        return NULL;
    }

    xvoTableSetText(tTable, (str)"gemini_part_kind", 0u, (str)"thought", 0u, FALSE);
    if ( sSignature && sSignature[0] ) {
        xvoTableSetText(tTable, (str)"thoughtSignature", 0u, (str)sSignature, 0u, FALSE);
    }

    return tTable;
}

static int xllm__gemini_set_thinking_vendor_extra(xllm_output_thinking *pThinking, const char *sSignature)
{
    xvalue tVendorExtra;

    if ( !pThinking ) {
        return XRT_NET_ERROR;
    }

    tVendorExtra = xllm__gemini_create_thinking_vendor_extra(sSignature);
    if ( !tVendorExtra ) {
        return XRT_NET_ERROR;
    }

    xllm__xvalue_release(&pThinking->tVendorExtra);
    pThinking->tVendorExtra = tVendorExtra;
    return XRT_NET_OK;
}

static void xllm__gemini_logf(
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

static const char *xllm__gemini_vendor_text(const xllm_profile *pProfile, const char *sKey)
{
    const char *sValue = NULL;

    if ( !pProfile || !sKey ) {
        return NULL;
    }

    sValue = xllm__json_table_get_text(pProfile->tProviderOptions.tVendorExtra, sKey);
    if ( sValue && sValue[0] ) {
        return sValue;
    }
    sValue = xllm__json_table_get_text(pProfile->tVendorExtra, sKey);
    if ( sValue && sValue[0] ) {
        return sValue;
    }
    return NULL;
}

static char *xllm__gemini_build_url(const xllm_profile *pProfile, const char *sModel, bool bStreaming)
{
    const char *sBaseUrl;
    size_t iLen;
    bool bNeedsSlash;
    const char *sSuffix = bStreaming ? ":streamGenerateContent?alt=sse" : ":generateContent";
    char *sUrl;
    int iWritten;

    if ( !pProfile || !sModel || !sModel[0] ) {
        return NULL;
    }

    sBaseUrl = pProfile->sBaseUrl;
    if ( !sBaseUrl || !sBaseUrl[0] ) {
        if ( pProfile->sAdapter && strcmp(pProfile->sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
            return NULL;
        }
        sBaseUrl = "https://generativelanguage.googleapis.com/v1beta/models";
    }

    if ( strstr(sBaseUrl, bStreaming ? ":streamGenerateContent" : ":generateContent") != NULL ) {
        return xllm__dup_cstr(sBaseUrl);
    }

    if ( bStreaming && strstr(sBaseUrl, ":generateContent") != NULL ) {
        const char *sExisting = ":generateContent";
        const char *sFound = strstr(sBaseUrl, sExisting);
        size_t iPrefixLen = (size_t)(sFound - sBaseUrl);
        size_t iSuffixLen = strlen(":streamGenerateContent?alt=sse");

        sUrl = (char *)xrtCalloc(iPrefixLen + iSuffixLen + 1u, sizeof(char));
        if ( !sUrl ) {
            return NULL;
        }
        memcpy(sUrl, sBaseUrl, iPrefixLen);
        memcpy(sUrl + iPrefixLen, ":streamGenerateContent?alt=sse", iSuffixLen);
        sUrl[iPrefixLen + iSuffixLen] = '\0';
        return sUrl;
    }

    iLen = strlen(sBaseUrl);
    bNeedsSlash = (iLen > 0u && sBaseUrl[iLen - 1u] != '/');
    sUrl = (char *)xrtCalloc(iLen + (bNeedsSlash ? 1u : 0u) + strlen(sModel) + strlen(sSuffix) + 1u, sizeof(char));
    if ( !sUrl ) {
        return NULL;
    }

    iWritten = snprintf(
        sUrl,
        iLen + (bNeedsSlash ? 1u : 0u) + strlen(sModel) + strlen(sSuffix) + 1u,
        "%s%s%s%s",
        sBaseUrl,
        bNeedsSlash ? "/" : "",
        sModel,
        sSuffix
    );
    if ( iWritten <= 0 ) {
        xrtFree(sUrl);
        return NULL;
    }

    return sUrl;
}

static int xllm__gemini_stream_apply_usage(xllm__openai_stream_context *pCtx, xvalue tUsage)
{
    xllm_event tEvent;

    if ( !pCtx || !pCtx->pResponse || !tUsage || xvoType(tUsage) != XVO_DT_TABLE ) {
        return XRT_NET_OK;
    }

    pCtx->pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tUsage, "promptTokenCount");
    pCtx->pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tUsage, "candidatesTokenCount");
    pCtx->pResponse->tUsage.uReasoningTokens = xllm__json_table_get_u32(tUsage, "thoughtsTokenCount");
    pCtx->pResponse->tUsage.uCachedInputTokens = xllm__json_table_get_u32(tUsage, "cachedContentTokenCount");
    ++pCtx->uUsageCount;

    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eType = XLLM_EVENT_USAGE;
    tEvent.as.tUsage.tUsage = pCtx->pResponse->tUsage;
    return xllm__openai_stream_dispatch(pCtx, &tEvent);
}

static int xllm__gemini_stream_process_root(
    xllm__openai_stream_context *pCtx,
    xvalue tRoot,
    size_t iPayloadLen
)
{
    xvalue tCandidates;
    xvalue tCandidate;
    xvalue tContent;
    xvalue tParts;
    xvalue tUsage;
    const char *sResponseId;
    const char *sModelVersion;
    const char *sFinishReason;
    size_t i;

    if ( !pCtx || !tRoot || xvoType(tRoot) != XVO_DT_TABLE ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_stream_ensure_response(pCtx) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    sResponseId = xllm__json_table_get_text(tRoot, "responseId");
    if ( sResponseId && sResponseId[0] && !pCtx->pResponse->sId ) {
        pCtx->pResponse->sId = xllm__dup_cstr(sResponseId);
        if ( !pCtx->pResponse->sId ) {
            return XRT_NET_ERROR;
        }
    }

    sModelVersion = xllm__json_table_get_text(tRoot, "modelVersion");
    if ( sModelVersion && sModelVersion[0] ) {
        xllm__free_cstr((char **)&pCtx->pResponse->sModel);
        pCtx->pResponse->sModel = xllm__dup_cstr(sModelVersion);
        if ( !pCtx->pResponse->sModel ) {
            return XRT_NET_ERROR;
        }
    }

    if ( xllm__openai_stream_emit_start(pCtx) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    tUsage = xllm__json_table_get(tRoot, "usageMetadata");
    if ( xllm__gemini_stream_apply_usage(pCtx, tUsage) != XRT_NET_OK ) {
        return XRT_NET_CANCELLED;
    }

    tCandidates = xllm__json_table_get(tRoot, "candidates");
    if ( !tCandidates || xvoType(tCandidates) != XVO_DT_ARRAY || xvoArrayItemCount(tCandidates) == 0u ) {
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "payload", iPayloadLen);
        return XRT_NET_OK;
    }

    tCandidate = xvoArrayGetValue(tCandidates, 0u);
    if ( !tCandidate || xvoType(tCandidate) != XVO_DT_TABLE ) {
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "payload", iPayloadLen);
        return XRT_NET_OK;
    }

    sFinishReason = xllm__json_table_get_text(tCandidate, "finishReason");
    if ( sFinishReason && sFinishReason[0] ) {
        xllm_response_status eStatus = xllm__gemini_status_from_finish_reason(sFinishReason);

        xllm__free_cstr((char **)&pCtx->pResponse->sFinishReason);
        pCtx->pResponse->sFinishReason = xllm__dup_cstr(sFinishReason);
        if ( !pCtx->pResponse->sFinishReason ) {
            return XRT_NET_ERROR;
        }

        if ( eStatus == XLLM_STATUS_CONTENT_FILTERED ) {
            xllm__free_cstr((char **)&pCtx->pResponse->tSafety.sBlockReason);
            pCtx->pResponse->tSafety.sBlockReason = xllm__dup_cstr(sFinishReason);
            if ( !pCtx->pResponse->tSafety.sBlockReason ) {
                return XRT_NET_ERROR;
            }
        }

        if ( !(eStatus == XLLM_STATUS_COMPLETED && pCtx->iToolOutputIndexCount > 0u) ) {
            pCtx->pResponse->eStatus = eStatus;
        }

        if ( strcmp(sFinishReason, "STOP") == 0 ||
             strcmp(sFinishReason, "MAX_TOKENS") == 0 ||
             strcmp(sFinishReason, "SAFETY") == 0 ||
             strcmp(sFinishReason, "PROHIBITED_CONTENT") == 0 ||
             strcmp(sFinishReason, "SPII") == 0 ||
             strcmp(sFinishReason, "RECITATION") == 0 ||
             strcmp(sFinishReason, "BLOCKLIST") == 0 ) {
            pCtx->bDone = true;
        }
    }

    tContent = xllm__json_table_get(tCandidate, "content");
    tParts = xllm__json_table_get(tContent, "parts");
    if ( !tParts || xvoType(tParts) != XVO_DT_ARRAY ) {
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "payload", iPayloadLen);
        return XRT_NET_OK;
    }

    for ( i = 0u; i < xvoArrayItemCount(tParts); ++i ) {
        xvalue tPartObj = xvoArrayGetValue(tParts, (uint32)i);
        xvalue tFunctionCall = xllm__json_table_get(tPartObj, "functionCall");
        const char *sText = xllm__json_table_get_text(tPartObj, "text");
        const char *sThoughtSignature = xllm__json_table_get_text(tPartObj, "thoughtSignature");
        bool bThought = false;

        (void)xllm__json_table_get_bool(tPartObj, "thought", &bThought);

        if ( tFunctionCall && xvoType(tFunctionCall) == XVO_DT_TABLE ) {
            const char *sName = xllm__json_table_get_text(tFunctionCall, "name");
            xvalue tArgs = xllm__json_table_get(tFunctionCall, "args");
            char *sArgumentsJson = tArgs ? (char *)xrtStringifyJSON(tArgs, FALSE, NULL) : xllm__dup_cstr("{}");
            char sCallId[32];
            size_t iToolIndex = pCtx->iToolOutputIndexCount;

            snprintf(sCallId, sizeof(sCallId), "gemini_call_%u", (unsigned)iToolIndex);
            if ( !sArgumentsJson ) {
                return XRT_NET_ERROR;
            }
            if ( xllm__openai_stream_append_tool_delta(
                     pCtx,
                     iToolIndex,
                     sCallId,
                     sName ? sName : "",
                     sArgumentsJson
                 ) != XRT_NET_OK ) {
                xrtFree(sArgumentsJson);
                return XRT_NET_CANCELLED;
            }
            xrtFree(sArgumentsJson);
            pCtx->pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
            continue;
        }

        if ( !sText || !sText[0] ) {
            continue;
        }

        if ( bThought ) {
            xllm_output_item *pThinkingOutput = NULL;

            if ( xllm__openai_stream_append_thinking(pCtx, sText) != XRT_NET_OK ) {
                return XRT_NET_CANCELLED;
            }
            if ( pCtx->iThinkingOutputIndex != (size_t)-1 ) {
                pThinkingOutput = &pCtx->pResponse->pOutputs[pCtx->iThinkingOutputIndex];
                if ( xllm__gemini_set_thinking_vendor_extra(&pThinkingOutput->as.tThinking, sThoughtSignature) != XRT_NET_OK ) {
                    return XRT_NET_ERROR;
                }
            }
            continue;
        }

        if ( xllm__openai_stream_append_text(pCtx, sText) != XRT_NET_OK ) {
            return XRT_NET_CANCELLED;
        }
    }

    xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "payload", iPayloadLen);
    return XRT_NET_OK;
}

static int xllm__gemini_stream_process_payload(
    xllm__openai_stream_context *pCtx,
    const char *sPayload,
    size_t iPayloadLen
)
{
    xvalue tRoot;
    int iStatus;

    if ( !pCtx || !sPayload || iPayloadLen == 0u ) {
        return XRT_NET_ERROR;
    }

    tRoot = xrtParseJSON((str)sPayload, iPayloadLen);
    if ( !tRoot ) {
        xllm__error_set(pCtx->pError, XLLM_ERROR_PARSE, "failed to parse gemini stream event");
        return XRT_NET_ERROR;
    }

    ++pCtx->uPayloadCount;
    iStatus = xllm__gemini_stream_process_root(pCtx, tRoot, iPayloadLen);
    xvoUnref(tRoot);
    return iStatus;
}

static int xllm__gemini_stream_process_event_block(
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

    iStatus = xllm__gemini_stream_process_payload(pCtx, tPayload.pData, tPayload.iLen);
    if ( iStatus == XRT_NET_OK ) {
        xllm__openai_trace_stream(pCtx->pRuntime, pCtx, "event_block", tPayload.iLen);
    }
    xllm__json_builder_reset(&tPayload);
    return iStatus;
}

static int xllm__gemini_stream_process_buffer(
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

        if ( xllm__gemini_stream_process_event_block(pCtx, sBuffer + iCursor, iEventEnd - iCursor) != XRT_NET_OK ) {
            return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
        }
        iCursor = iEventEnd + iDelimiterLen;
        pCtx->iParsedBytes = iCursor;
    }

    return pCtx->bCancelled ? XRT_NET_CANCELLED : XRT_NET_OK;
}

static char *xllm__vertex_fetch_access_token(const xllm_profile *pProfile)
{
    const char *sEnvPath;
    const char *sCredentialsPath;
    const char *sCredentialsJson;
    FILE *pPipe;
    char sBuffer[4096];
    char *sToken;
    char sCommand[8192];
    char *sTempCredentialsPath = NULL;

    sCredentialsJson = xllm__gemini_vendor_text(pProfile, "vertex_credentials_json");

    /* Allow callers to pass raw service account JSON content in either
       vertex_credentials_json or vertex_credentials_path. We materialize it
       to a temporary file so the existing ADC/gcloud flow can consume it. */
    if ( sCredentialsJson && sCredentialsJson[0] && sCredentialsJson[0] == '{' ) {
#ifdef _WIN32
        char aTempDir[MAX_PATH];
        char aTempFile[MAX_PATH];
        DWORD uDirLen;
        FILE *pFile;

        uDirLen = GetTempPathA((DWORD)sizeof(aTempDir), aTempDir);
        if ( uDirLen == 0u || uDirLen >= sizeof(aTempDir) ) {
            return NULL;
        }
        if ( GetTempFileNameA(aTempDir, "xgv", 0u, aTempFile) == 0u ) {
            return NULL;
        }
        pFile = fopen(aTempFile, "wb");
        if ( !pFile ) {
            DeleteFileA(aTempFile);
            return NULL;
        }
        if ( fwrite(sCredentialsJson, 1u, strlen(sCredentialsJson), pFile) != strlen(sCredentialsJson) ) {
            fclose(pFile);
            DeleteFileA(aTempFile);
            return NULL;
        }
        fclose(pFile);
        sTempCredentialsPath = xllm__dup_cstr(aTempFile);
        if ( !sTempCredentialsPath ) {
            DeleteFileA(aTempFile);
            return NULL;
        }
#else
        char aTemplate[] = "/tmp/xllm_vertex_XXXXXX";
        int iFd;
        FILE *pFile;

        iFd = mkstemp(aTemplate);
        if ( iFd < 0 ) {
            return NULL;
        }
        pFile = fdopen(iFd, "wb");
        if ( !pFile ) {
            close(iFd);
            unlink(aTemplate);
            return NULL;
        }
        if ( fwrite(sCredentialsJson, 1u, strlen(sCredentialsJson), pFile) != strlen(sCredentialsJson) ) {
            fclose(pFile);
            unlink(aTemplate);
            return NULL;
        }
        fclose(pFile);
        sTempCredentialsPath = xllm__dup_cstr(aTemplate);
        if ( !sTempCredentialsPath ) {
            unlink(aTemplate);
            return NULL;
        }
#endif
    }

    sCredentialsPath = xllm__gemini_vendor_text(pProfile, "vertex_credentials_path");
    sEnvPath = getenv("GOOGLE_APPLICATION_CREDENTIALS");

    if ( !sCredentialsPath || !sCredentialsPath[0] ) {
        sCredentialsPath = sTempCredentialsPath;
    }
    if ( sCredentialsPath && sCredentialsPath[0] == '{' ) {
        /* Backward-compatible escape hatch: if vertex_credentials_path itself
           contains raw JSON, treat it like inline credentials content. */
        sCredentialsJson = sCredentialsPath;
#ifdef _WIN32
        {
            char aTempDir[MAX_PATH];
            char aTempFile[MAX_PATH];
            DWORD uDirLen;
            FILE *pFile;

            uDirLen = GetTempPathA((DWORD)sizeof(aTempDir), aTempDir);
            if ( uDirLen == 0u || uDirLen >= sizeof(aTempDir) ) {
                return NULL;
            }
            if ( GetTempFileNameA(aTempDir, "xgv", 0u, aTempFile) == 0u ) {
                return NULL;
            }
            pFile = fopen(aTempFile, "wb");
            if ( !pFile ) {
                DeleteFileA(aTempFile);
                return NULL;
            }
            if ( fwrite(sCredentialsJson, 1u, strlen(sCredentialsJson), pFile) != strlen(sCredentialsJson) ) {
                fclose(pFile);
                DeleteFileA(aTempFile);
                return NULL;
            }
            fclose(pFile);
            xllm__free_cstr(&sTempCredentialsPath);
            sTempCredentialsPath = xllm__dup_cstr(aTempFile);
            if ( !sTempCredentialsPath ) {
                DeleteFileA(aTempFile);
                return NULL;
            }
            sCredentialsPath = sTempCredentialsPath;
        }
#else
        {
            char aTemplate[] = "/tmp/xllm_vertex_XXXXXX";
            int iFd;
            FILE *pFile;

            iFd = mkstemp(aTemplate);
            if ( iFd < 0 ) {
                return NULL;
            }
            pFile = fdopen(iFd, "wb");
            if ( !pFile ) {
                close(iFd);
                unlink(aTemplate);
                return NULL;
            }
            if ( fwrite(sCredentialsJson, 1u, strlen(sCredentialsJson), pFile) != strlen(sCredentialsJson) ) {
                fclose(pFile);
                unlink(aTemplate);
                return NULL;
            }
            fclose(pFile);
            xllm__free_cstr(&sTempCredentialsPath);
            sTempCredentialsPath = xllm__dup_cstr(aTemplate);
            if ( !sTempCredentialsPath ) {
                unlink(aTemplate);
                return NULL;
            }
            sCredentialsPath = sTempCredentialsPath;
        }
#endif
    }

    if ( !sCredentialsPath || !sCredentialsPath[0] ) {
        sCredentialsPath = sEnvPath;
    }
    if ( !sCredentialsPath || !sCredentialsPath[0] ) {
        return NULL;
    }

#ifdef _WIN32
    _snprintf(
        sCommand,
        sizeof(sCommand),
        "cmd /d /c \"set \\\"GOOGLE_APPLICATION_CREDENTIALS=%s\\\" && gcloud auth application-default print-access-token\"",
        sCredentialsPath
    );
#else
    snprintf(
        sCommand,
        sizeof(sCommand),
        "GOOGLE_APPLICATION_CREDENTIALS='%s' gcloud auth application-default print-access-token",
        sCredentialsPath
    );
#endif

    pPipe = xllm__popen(sCommand, "r");
    if ( !pPipe ) {
        if ( sTempCredentialsPath ) {
#ifdef _WIN32
            DeleteFileA(sTempCredentialsPath);
#else
            unlink(sTempCredentialsPath);
#endif
            xllm__free_cstr(&sTempCredentialsPath);
        }
        return NULL;
    }
    if ( !fgets(sBuffer, (int)sizeof(sBuffer), pPipe) ) {
        xllm__pclose(pPipe);
        if ( sTempCredentialsPath ) {
#ifdef _WIN32
            DeleteFileA(sTempCredentialsPath);
#else
            unlink(sTempCredentialsPath);
#endif
            xllm__free_cstr(&sTempCredentialsPath);
        }
        return NULL;
    }
    xllm__pclose(pPipe);
    if ( sTempCredentialsPath ) {
#ifdef _WIN32
        DeleteFileA(sTempCredentialsPath);
#else
        unlink(sTempCredentialsPath);
#endif
        xllm__free_cstr(&sTempCredentialsPath);
    }

    sToken = xllm__dup_cstr(sBuffer);
    if ( !sToken ) {
        return NULL;
    }

    while ( sToken[0] ) {
        size_t iLen = strlen(sToken);
        if ( iLen == 0u ) {
            break;
        }
        if ( sToken[iLen - 1u] == '\r' || sToken[iLen - 1u] == '\n' ||
             sToken[iLen - 1u] == ' ' || sToken[iLen - 1u] == '\t' ) {
            sToken[iLen - 1u] = '\0';
            continue;
        }
        break;
    }

    if ( !sToken[0] ) {
        xrtFree(sToken);
        return NULL;
    }

    return sToken;
}

static bool xllm__gemini_append_data_part(
    xllm__json_builder *pBuilder,
    const xllm_content_part *pPart,
    xllm_error *pError
)
{
    const char *sMimeType;
    char *sBase64 = NULL;
    const char *sUri;

    if ( !pBuilder || !pPart ) {
        return false;
    }

    switch ( pPart->eKind ) {
        case XLLM_PART_TEXT:
            return xllm__json_builder_append_cstr(pBuilder, "{\"text\":") &&
                   xllm__json_builder_append_escaped(pBuilder, pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "") &&
                   xllm__json_builder_append_char(pBuilder, '}');

        case XLLM_PART_JSON: {
            char *sJson = (char *)xrtStringifyJSON(pPart->as.tJsonValue, FALSE, NULL);
            bool bOK;

            if ( !sJson ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify gemini json part");
                return false;
            }
            bOK = xllm__json_builder_append_cstr(pBuilder, "{\"text\":") &&
                  xllm__json_builder_append_escaped(pBuilder, sJson) &&
                  xllm__json_builder_append_char(pBuilder, '}');
            xrtFree(sJson);
            return bOK;
        }

        case XLLM_PART_IMAGE:
        case XLLM_PART_FILE:
        case XLLM_PART_AUDIO:
        case XLLM_PART_VIDEO:
            sMimeType = pPart->as.tSource.sMimeType ? pPart->as.tSource.sMimeType : "application/octet-stream";
            switch ( pPart->as.tSource.eKind ) {
                case XLLM_SOURCE_INLINE_BYTES:
                    if ( !pPart->as.tSource.as.tBytes.pData || pPart->as.tSource.as.tBytes.iSize == 0u ) {
                        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini adapter input bytes are empty");
                        return false;
                    }
                    sBase64 = (char *)xrtBase64Encode(
                        (ptr)pPart->as.tSource.as.tBytes.pData,
                        pPart->as.tSource.as.tBytes.iSize,
                        NULL
                    );
                    if ( !sBase64 ) {
                        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to base64 encode gemini input bytes");
                        return false;
                    }
                    if ( !xllm__json_builder_append_cstr(pBuilder, "{\"inlineData\":{\"mimeType\":") ||
                         !xllm__json_builder_append_escaped(pBuilder, sMimeType) ||
                         !xllm__json_builder_append_cstr(pBuilder, ",\"data\":") ||
                         !xllm__json_builder_append_escaped(pBuilder, sBase64) ||
                         !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                        xrtFree(sBase64);
                        return false;
                    }
                    xrtFree(sBase64);
                    return true;

                case XLLM_SOURCE_URL:
                    sUri = pPart->as.tSource.as.sUrl;
                    if ( !sUri || !sUri[0] ) {
                        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini adapter url input is empty");
                        return false;
                    }
                    return xllm__json_builder_append_cstr(pBuilder, "{\"fileData\":{\"mimeType\":") &&
                           xllm__json_builder_append_escaped(pBuilder, sMimeType) &&
                           xllm__json_builder_append_cstr(pBuilder, ",\"fileUri\":") &&
                           xllm__json_builder_append_escaped(pBuilder, sUri) &&
                           xllm__json_builder_append_cstr(pBuilder, "}}");

                case XLLM_SOURCE_PROVIDER_FILE_ID:
                    sUri = pPart->as.tSource.as.sFileId;
                    if ( !sUri || !sUri[0] ) {
                        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini adapter provider file id is empty");
                        return false;
                    }
                    return xllm__json_builder_append_cstr(pBuilder, "{\"fileData\":{\"mimeType\":") &&
                           xllm__json_builder_append_escaped(pBuilder, sMimeType) &&
                           xllm__json_builder_append_cstr(pBuilder, ",\"fileUri\":") &&
                           xllm__json_builder_append_escaped(pBuilder, sUri) &&
                           xllm__json_builder_append_cstr(pBuilder, "}}");

                default:
                    xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "gemini adapter only supports text/json/url/file-uri/inline-bytes content");
                    return false;
            }

        default:
            xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "gemini adapter unsupported content part");
            return false;
    }
}

static bool xllm__gemini_append_parts_array(
    xllm__json_builder *pBuilder,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    size_t i;
    bool bNeedComma = false;

    if ( !pBuilder || !pMessage ) {
        return false;
    }

    if ( !xllm__json_builder_append_char(pBuilder, '[') ) {
        return false;
    }

    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        if ( bNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return false;
        }
        if ( !xllm__gemini_append_data_part(pBuilder, &pMessage->pParts[i], pError) ) {
            return false;
        }
        bNeedComma = true;
    }

    if ( pMessage->eRole == XLLM_ROLE_ASSISTANT ) {
        for ( i = 0u; i < pMessage->iToolCallCount; ++i ) {
            char *sArgsJson;

            if ( bNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
                return false;
            }
            sArgsJson = xllm__dup_cstr(
                pMessage->pToolCalls[i].sArgumentsJson ? pMessage->pToolCalls[i].sArgumentsJson : "{}"
            );
            if ( !sArgsJson ) {
                return false;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, "{\"functionCall\":{\"name\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pMessage->pToolCalls[i].sToolName ? pMessage->pToolCalls[i].sToolName : "") ||
                 !xllm__json_builder_append_cstr(pBuilder, ",\"args\":") ||
                 !xllm__json_builder_append_cstr(pBuilder, sArgsJson) ||
                 !xllm__json_builder_append_cstr(pBuilder, "}}") ) {
                xrtFree(sArgsJson);
                return false;
            }
            xrtFree(sArgsJson);
            bNeedComma = true;
        }
    }

    if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
        return false;
    }
    return true;
}

static bool xllm__gemini_append_function_response_part(
    xllm__json_builder *pBuilder,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    char *sResponseJson = NULL;
    char *sText = NULL;
    size_t i;
    xvalue tResponse = NULL;
    bool bOK = false;

    if ( !pBuilder || !pMessage ) {
        return false;
    }
    if ( !pMessage->sToolName || !pMessage->sToolName[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini tool result message missing tool name");
        return false;
    }

    for ( i = 0u; i < pMessage->iPartCount; ++i ) {
        const xllm_content_part *pPart = &pMessage->pParts[i];
        if ( pPart->eKind == XLLM_PART_JSON ) {
            if ( tResponse ) {
                xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "gemini tool result currently supports at most one json part");
                goto done;
            }
            tResponse = xvoCopy(pPart->as.tJsonValue);
        } else if ( pPart->eKind == XLLM_PART_TEXT ) {
            const char *sPartText = pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "";
            size_t iOldLen = sText ? strlen(sText) : 0u;
            size_t iPartLen = strlen(sPartText);
            char *sNew = (char *)xrtRealloc(sText, iOldLen + (iOldLen ? 1u : 0u) + iPartLen + 1u);
            if ( !sNew ) {
                goto done;
            }
            sText = sNew;
            if ( iOldLen ) {
                sText[iOldLen++] = '\n';
            }
            memcpy(sText + iOldLen, sPartText, iPartLen);
            sText[iOldLen + iPartLen] = '\0';
        } else {
            xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_INPUT_TYPE, "gemini tool result currently supports only text/json parts");
            goto done;
        }
    }

    if ( !tResponse ) {
        tResponse = xvoCreateTable();
        if ( !tResponse ) {
            goto done;
        }
        if ( !xvoTableSetText(tResponse, (str)"text", 4u, (str)(sText ? sText : ""), 0u, FALSE) ) {
            goto done;
        }
    }

    sResponseJson = (char *)xrtStringifyJSON(tResponse, FALSE, NULL);
    if ( !sResponseJson ) {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify gemini tool response");
        goto done;
    }

    bOK = xllm__json_builder_append_cstr(pBuilder, "[{\"functionResponse\":{\"name\":") &&
          xllm__json_builder_append_escaped(pBuilder, pMessage->sToolName) &&
          xllm__json_builder_append_cstr(pBuilder, ",\"response\":") &&
          xllm__json_builder_append_cstr(pBuilder, sResponseJson) &&
          xllm__json_builder_append_cstr(pBuilder, "}}]");

done:
    if ( sText ) {
        xrtFree(sText);
    }
    if ( sResponseJson ) {
        xrtFree(sResponseJson);
    }
    xllm__xvalue_release(&tResponse);
    return bOK;
}

static bool xllm__gemini_append_message_object(
    xllm__json_builder *pBuilder,
    const xllm_message *pMessage,
    xllm_error *pError
)
{
    const char *sRole;

    if ( !pBuilder || !pMessage ) {
        return false;
    }
    if ( pMessage->eRole == XLLM_ROLE_SYSTEM ) {
        return true;
    }

    if ( pMessage->eRole == XLLM_ROLE_TOOL ) {
        return xllm__json_builder_append_cstr(pBuilder, "{\"role\":\"user\",\"parts\":") &&
               xllm__gemini_append_function_response_part(pBuilder, pMessage, pError) &&
               xllm__json_builder_append_char(pBuilder, '}');
    }

    sRole = (pMessage->eRole == XLLM_ROLE_ASSISTANT) ? "model" : "user";
    return xllm__json_builder_append_cstr(pBuilder, "{\"role\":") &&
           xllm__json_builder_append_escaped(pBuilder, sRole) &&
           xllm__json_builder_append_cstr(pBuilder, ",\"parts\":") &&
           xllm__gemini_append_parts_array(pBuilder, pMessage, pError) &&
           xllm__json_builder_append_char(pBuilder, '}');
}

static bool xllm__gemini_collect_system_instruction(
    const xllm_request *pRequest,
    char **psOut
)
{
    xllm__json_builder tBuilder;
    size_t i;
    bool bHasText = false;

    if ( psOut ) {
        *psOut = NULL;
    }
    if ( !psOut || !pRequest ) {
        return false;
    }

    memset(&tBuilder, 0, sizeof(tBuilder));

    for ( i = 0u; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0u; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            const xllm_message *pMsg = &pRequest->pContextBlocks[i].pMessages[j];
            size_t k;
            if ( pMsg->eRole != XLLM_ROLE_SYSTEM ) {
                continue;
            }
            for ( k = 0u; k < pMsg->iPartCount; ++k ) {
                const xllm_content_part *pPart = &pMsg->pParts[k];
                const char *sText;
                if ( pPart->eKind != XLLM_PART_TEXT || pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                    continue;
                }
                sText = pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "";
                if ( bHasText && !xllm__json_builder_append_char(&tBuilder, '\n') ) {
                    goto fail;
                }
                if ( !xllm__json_builder_append_cstr(&tBuilder, sText) ) {
                    goto fail;
                }
                bHasText = true;
            }
        }
    }

    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        const xllm_message *pMsg = &pRequest->pMessages[i];
        size_t k;
        if ( pMsg->eRole != XLLM_ROLE_SYSTEM ) {
            continue;
        }
        for ( k = 0u; k < pMsg->iPartCount; ++k ) {
            const xllm_content_part *pPart = &pMsg->pParts[k];
            const char *sText;
            if ( pPart->eKind != XLLM_PART_TEXT || pPart->as.tSource.eKind != XLLM_SOURCE_INLINE_TEXT ) {
                continue;
            }
            sText = pPart->as.tSource.as.sText ? pPart->as.tSource.as.sText : "";
            if ( bHasText && !xllm__json_builder_append_char(&tBuilder, '\n') ) {
                goto fail;
            }
            if ( !xllm__json_builder_append_cstr(&tBuilder, sText) ) {
                goto fail;
            }
            bHasText = true;
        }
    }

    if ( !bHasText ) {
        xllm__json_builder_reset(&tBuilder);
        return true;
    }

    *psOut = xllm__json_builder_detach(&tBuilder);
    return *psOut != NULL;

fail:
    xllm__json_builder_reset(&tBuilder);
    return false;
}

static bool xllm__gemini_append_contents(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    xllm_error *pError,
    uint32 *puMessageCount
)
{
    size_t i;
    bool bNeedComma = false;
    uint32 uMessageCount = 0u;

    if ( !pBuilder || !pRequest ) {
        return false;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, "\"contents\":[") ) {
        return false;
    }

    for ( i = 0u; i < pRequest->iContextBlockCount; ++i ) {
        size_t j;
        for ( j = 0u; j < pRequest->pContextBlocks[i].iMessageCount; ++j ) {
            const xllm_message *pMessage = &pRequest->pContextBlocks[i].pMessages[j];
            if ( pMessage->eRole == XLLM_ROLE_SYSTEM ) {
                continue;
            }
            if ( bNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
                return false;
            }
            if ( !xllm__gemini_append_message_object(pBuilder, pMessage, pError) ) {
                return false;
            }
            bNeedComma = true;
            ++uMessageCount;
        }
    }

    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        if ( pMessage->eRole == XLLM_ROLE_SYSTEM ) {
            continue;
        }
        if ( bNeedComma && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return false;
        }
        if ( !xllm__gemini_append_message_object(pBuilder, pMessage, pError) ) {
            return false;
        }
        bNeedComma = true;
        ++uMessageCount;
    }

    if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
        return false;
    }
    if ( puMessageCount ) {
        *puMessageCount = uMessageCount;
    }
    return true;
}

static bool xllm__gemini_append_tools(
    xllm__json_builder *pBuilder,
    const xllm_request *pRequest,
    xllm_error *pError
)
{
    size_t i;

    if ( !pBuilder || !pRequest || pRequest->iToolCount == 0u ) {
        return true;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, ",\"tools\":[{\"functionDeclarations\":[") ) {
        return false;
    }
    for ( i = 0u; i < pRequest->iToolCount; ++i ) {
        const xllm_tool_def *pTool = &pRequest->pTools[i];
        const char *sWireName = (pTool->sWireName && pTool->sWireName[0]) ? pTool->sWireName : pTool->sToolId;
        char *sSchema;

        if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) {
            return false;
        }
        if ( !sWireName || !sWireName[0] ) {
            xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini tool definition missing wire_name");
            return false;
        }

        sSchema = pTool->tInputSchema ? (char *)xrtStringifyJSON(pTool->tInputSchema, FALSE, NULL) : xllm__dup_cstr("{}");
        if ( !sSchema ) {
            xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify gemini tool schema");
            return false;
        }

        if ( !xllm__json_builder_append_cstr(pBuilder, "{\"name\":") ||
             !xllm__json_builder_append_escaped(pBuilder, sWireName) ) {
            xrtFree(sSchema);
            return false;
        }
        if ( pTool->sDescription && pTool->sDescription[0] ) {
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"description\":") ||
                 !xllm__json_builder_append_escaped(pBuilder, pTool->sDescription) ) {
                xrtFree(sSchema);
                return false;
            }
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"parameters\":") ||
             !xllm__json_builder_append_cstr(pBuilder, sSchema) ||
             !xllm__json_builder_append_char(pBuilder, '}') ) {
            xrtFree(sSchema);
            return false;
        }
        xrtFree(sSchema);
    }
    if ( !xllm__json_builder_append_cstr(pBuilder, "]}]") ) {
        return false;
    }

    switch ( pRequest->tToolPolicy.eMode ) {
        case XLLM_TOOL_CHOICE_NONE:
            return xllm__json_builder_append_cstr(
                pBuilder,
                ",\"toolConfig\":{\"functionCallingConfig\":{\"mode\":\"NONE\"}}"
            );
        case XLLM_TOOL_CHOICE_REQUIRED:
            return xllm__json_builder_append_cstr(
                pBuilder,
                ",\"toolConfig\":{\"functionCallingConfig\":{\"mode\":\"ANY\"}}"
            );
        case XLLM_TOOL_CHOICE_NAMED:
            if ( !pRequest->tToolPolicy.sToolName || !pRequest->tToolPolicy.sToolName[0] ) {
                xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini named tool choice missing tool name");
                return false;
            }
            return xllm__json_builder_append_cstr(pBuilder, ",\"toolConfig\":{\"functionCallingConfig\":{\"mode\":\"ANY\",\"allowedFunctionNames\":[") &&
                   xllm__json_builder_append_escaped(pBuilder, pRequest->tToolPolicy.sToolName) &&
                   xllm__json_builder_append_cstr(pBuilder, "]}}");
        case XLLM_TOOL_CHOICE_AUTO:
        default:
            return xllm__json_builder_append_cstr(
                pBuilder,
                ",\"toolConfig\":{\"functionCallingConfig\":{\"mode\":\"AUTO\"}}"
            );
    }
}

static bool xllm__gemini_append_generation_config(
    xllm__json_builder *pBuilder,
    const xllm_effective_params *pEffectiveParams,
    xllm_error *pError
)
{
    bool bHasField = false;
    size_t i;
    char *sSchema = NULL;
    bool bReasoningEnabled = false;
    bool bIncludeThoughts = false;
    uint32 uThinkingBudget = 0u;

    if ( !pBuilder || !pEffectiveParams ) {
        return false;
    }

    if ( !xllm__json_builder_append_cstr(pBuilder, ",\"generationConfig\":{") ) {
        return false;
    }

    if ( pEffectiveParams->tGeneration.tTemperature.bSet ) {
        if ( bHasField && !xllm__json_builder_append_char(pBuilder, ',') ) return false;
        if ( !xllm__json_builder_append_cstr(pBuilder, "\"temperature\":") ||
             !xllm__json_builder_append_f64(pBuilder, pEffectiveParams->tGeneration.tTemperature.fValue) ) {
            return false;
        }
        bHasField = true;
    }
    if ( pEffectiveParams->tGeneration.tTopP.bSet ) {
        if ( bHasField && !xllm__json_builder_append_char(pBuilder, ',') ) return false;
        if ( !xllm__json_builder_append_cstr(pBuilder, "\"topP\":") ||
             !xllm__json_builder_append_f64(pBuilder, pEffectiveParams->tGeneration.tTopP.fValue) ) {
            return false;
        }
        bHasField = true;
    }
    if ( pEffectiveParams->tGeneration.tMaxOutputTokens.bSet ) {
        if ( bHasField && !xllm__json_builder_append_char(pBuilder, ',') ) return false;
        if ( !xllm__json_builder_append_cstr(pBuilder, "\"maxOutputTokens\":") ||
             !xllm__json_builder_append_u32(pBuilder, pEffectiveParams->tGeneration.tMaxOutputTokens.iValue) ) {
            return false;
        }
        bHasField = true;
    }
    if ( pEffectiveParams->tGeneration.iStopCount > 0u ) {
        if ( bHasField && !xllm__json_builder_append_char(pBuilder, ',') ) return false;
        if ( !xllm__json_builder_append_cstr(pBuilder, "\"stopSequences\":[") ) {
            return false;
        }
        for ( i = 0u; i < pEffectiveParams->tGeneration.iStopCount; ++i ) {
            if ( i > 0u && !xllm__json_builder_append_char(pBuilder, ',') ) return false;
            if ( !xllm__json_builder_append_escaped(pBuilder, pEffectiveParams->tGeneration.psStop[i] ? pEffectiveParams->tGeneration.psStop[i] : "") ) {
                return false;
            }
        }
        if ( !xllm__json_builder_append_char(pBuilder, ']') ) {
            return false;
        }
        bHasField = true;
    }
    if ( pEffectiveParams->tResponseFormat.eKind == XLLM_RESPONSE_JSON ||
         pEffectiveParams->tResponseFormat.eKind == XLLM_RESPONSE_JSON_SCHEMA ) {
        if ( bHasField && !xllm__json_builder_append_char(pBuilder, ',') ) return false;
        if ( !xllm__json_builder_append_cstr(pBuilder, "\"responseMimeType\":\"application/json\"") ) {
            return false;
        }
        bHasField = true;

        if ( pEffectiveParams->tResponseFormat.eKind == XLLM_RESPONSE_JSON_SCHEMA ) {
            sSchema = pEffectiveParams->tResponseFormat.tJsonSchema ?
                (char *)xrtStringifyJSON(pEffectiveParams->tResponseFormat.tJsonSchema, FALSE, NULL) :
                xllm__dup_cstr("{}");
            if ( !sSchema ) {
                xllm__error_set(pError, XLLM_ERROR_INTERNAL, "failed to stringify gemini response schema");
                return false;
            }
            if ( !xllm__json_builder_append_cstr(pBuilder, ",\"responseSchema\":") ||
                 !xllm__json_builder_append_cstr(pBuilder, sSchema) ) {
                xrtFree(sSchema);
                return false;
            }
            xrtFree(sSchema);
        }
    }

    bReasoningEnabled =
        (pEffectiveParams->tReasoning.tEnabled.bSet && pEffectiveParams->tReasoning.tEnabled.bValue) ||
        (pEffectiveParams->tReasoning.eLevel != XLLM_REASONING_DEFAULT &&
         pEffectiveParams->tReasoning.eLevel != XLLM_REASONING_OFF);
    bIncludeThoughts =
        pEffectiveParams->tReasoning.tExposeThinking.bSet &&
        pEffectiveParams->tReasoning.tExposeThinking.bValue;
    uThinkingBudget = xllm__gemini_reasoning_budget_for_level(&pEffectiveParams->tReasoning);

    if ( bReasoningEnabled || bIncludeThoughts || pEffectiveParams->tReasoning.eLevel == XLLM_REASONING_OFF ) {
        if ( bHasField && !xllm__json_builder_append_char(pBuilder, ',') ) return false;
        if ( !xllm__json_builder_append_cstr(pBuilder, "\"thinkingConfig\":{") ) {
            return false;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, "\"thinkingBudget\":") ||
             !xllm__json_builder_append_u32(pBuilder, uThinkingBudget) ) {
            return false;
        }
        if ( !xllm__json_builder_append_cstr(pBuilder, ",\"includeThoughts\":") ||
             !xllm__json_builder_append_cstr(pBuilder, bIncludeThoughts ? "true" : "false") ||
             !xllm__json_builder_append_char(pBuilder, '}') ) {
            return false;
        }
        bHasField = true;
    }

    return xllm__json_builder_append_char(pBuilder, '}');
}

static int xllm__gemini_build_body(
    xllm__json_builder *pBody,
    const xllm_request *pRequest,
    const xllm_effective_params *pEffectiveParams,
    uint32 *puMessageCount,
    xllm_error *pError
)
{
    char *sSystem = NULL;
    bool bOK = false;

    if ( puMessageCount ) {
        *puMessageCount = 0u;
    }
    if ( !pBody || !pRequest || !pEffectiveParams ) {
        return XRT_NET_ERROR;
    }
    if ( !xllm__json_builder_append_char(pBody, '{') ) {
        return XRT_NET_ERROR;
    }

    if ( !xllm__gemini_collect_system_instruction(pRequest, &sSystem) ) {
        return XRT_NET_ERROR;
    }
    if ( sSystem && sSystem[0] ) {
        if ( !xllm__json_builder_append_cstr(pBody, "\"systemInstruction\":{\"parts\":[{\"text\":") ||
             !xllm__json_builder_append_escaped(pBody, sSystem) ||
             !xllm__json_builder_append_cstr(pBody, "}]},") ) {
            goto done;
        }
    }

    if ( !xllm__gemini_append_contents(pBody, pRequest, pError, puMessageCount) ) {
        goto done;
    }
    if ( !xllm__gemini_append_tools(pBody, pRequest, pError) ) {
        goto done;
    }
    if ( !xllm__gemini_append_generation_config(pBody, pEffectiveParams, pError) ) {
        goto done;
    }
    if ( !xllm__json_builder_append_char(pBody, '}') ) {
        goto done;
    }

    bOK = true;

done:
    if ( sSystem ) {
        xrtFree(sSystem);
    }
    return bOK ? XRT_NET_OK : XRT_NET_ERROR;
}

static xllm_response_status xllm__gemini_status_from_finish_reason(const char *sFinishReason)
{
    if ( !sFinishReason || !sFinishReason[0] || strcmp(sFinishReason, "STOP") == 0 ) {
        return XLLM_STATUS_COMPLETED;
    }
    if ( strcmp(sFinishReason, "MAX_TOKENS") == 0 ) {
        return XLLM_STATUS_INCOMPLETE;
    }
    if ( strcmp(sFinishReason, "SAFETY") == 0 ||
         strcmp(sFinishReason, "PROHIBITED_CONTENT") == 0 ||
         strcmp(sFinishReason, "SPII") == 0 ||
         strcmp(sFinishReason, "RECITATION") == 0 ||
         strcmp(sFinishReason, "BLOCKLIST") == 0 ) {
        return XLLM_STATUS_CONTENT_FILTERED;
    }
    return XLLM_STATUS_COMPLETED;
}

static void xllm__gemini_fill_error_from_http(xllm_error *pError, const xhttpresponse *pHttpResponse, xvalue tRoot)
{
    xvalue tError;
    const char *sStatus = NULL;
    const char *sMessage = NULL;
    const char *sRequestId = NULL;

    if ( !pError ) {
        return;
    }

    tError = xllm__json_table_get(tRoot, "error");
    if ( tError ) {
        sStatus = xllm__json_table_get_text(tError, "status");
        sMessage = xllm__json_table_get_text(tError, "message");
    }
    if ( !sMessage || !sMessage[0] ) {
        sMessage = "gemini request failed";
    }

    if ( sStatus && strcmp(sStatus, "INVALID_ARGUMENT") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, sMessage);
    } else if ( sStatus && strcmp(sStatus, "UNAUTHENTICATED") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_AUTH, sMessage);
    } else if ( sStatus && strcmp(sStatus, "NOT_FOUND") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, sMessage);
    } else if ( sStatus && strcmp(sStatus, "RESOURCE_EXHAUSTED") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_RATE_LIMIT, sMessage);
    } else if ( sStatus && strcmp(sStatus, "DEADLINE_EXCEEDED") == 0 ) {
        xllm__error_set(pError, XLLM_ERROR_TIMEOUT, sMessage);
    } else if ( sStatus && (strcmp(sStatus, "UNAVAILABLE") == 0 || strcmp(sStatus, "INTERNAL") == 0) ) {
        xllm__error_set(pError, XLLM_ERROR_UPSTREAM_5XX, sMessage);
    } else if ( pHttpResponse && pHttpResponse->iStatusCode >= 500u ) {
        xllm__error_set(pError, XLLM_ERROR_UPSTREAM_5XX, sMessage);
    } else if ( pHttpResponse && pHttpResponse->iStatusCode >= 400u ) {
        xllm__error_set(pError, XLLM_ERROR_UPSTREAM_4XX, sMessage);
    } else {
        xllm__error_set(pError, XLLM_ERROR_INTERNAL, sMessage);
    }

    if ( pHttpResponse ) {
        pError->iHttpStatus = (int32)pHttpResponse->iStatusCode;
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-goog-request-id");
        if ( !sRequestId || !sRequestId[0] ) {
            sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
        }
        if ( sRequestId && sRequestId[0] ) {
            pError->sRequestId = xllm__dup_cstr(sRequestId);
        }
    }
    if ( sStatus && sStatus[0] ) {
        pError->sProviderCode = xllm__dup_cstr(sStatus);
    }
    if ( sMessage && sMessage[0] ) {
        pError->sProviderMessage = xllm__dup_cstr(sMessage);
    }
}

static int xllm__gemini_parse_response(
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
    xvalue tCandidates;
    xvalue tCandidate;
    xvalue tContent;
    xvalue tParts;
    xvalue tUsage;
    xllm_response *pResponse = NULL;
    size_t i;
    size_t iOutputCap = 0u;
    size_t iMessagePartCap = 0u;
    size_t iMessageOutputIndex = (size_t)-1;
    xllm__json_builder tVisible;
    char *sNormalizedJson = NULL;
    xvalue tJsonValue = NULL;
    const char *sFinishReason;

    if ( !ppResponse || !pEffectiveParams ) {
        return XRT_NET_ERROR;
    }
    *ppResponse = NULL;

    tRoot = xrtParseJSON((str)sBody, iBodyLen);
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "invalid gemini response");
        return XRT_NET_ERROR;
    }

    pResponse = (xllm_response *)xrtCalloc(1u, sizeof(*pResponse));
    if ( !pResponse ) {
        xllm__xvalue_release(&tRoot);
        return XRT_NET_ERROR;
    }
    memset(&tVisible, 0, sizeof(tVisible));

    pResponse->sProvider = xllm__dup_cstr(xllm__gemini_provider_name(pProfile));
    pResponse->sProfileId = xllm__dup_cstr(pProfile ? pProfile->sId : NULL);
    pResponse->sModel = xllm__dup_cstr(sSelectedModel);
    pResponse->tEffectiveParams = *pEffectiveParams;
    memset(pEffectiveParams, 0, sizeof(*pEffectiveParams));

    tUsage = xllm__json_table_get(tRoot, "usageMetadata");
    if ( tUsage ) {
        pResponse->tUsage.uInputTokens = xllm__json_table_get_u32(tUsage, "promptTokenCount");
        pResponse->tUsage.uOutputTokens = xllm__json_table_get_u32(tUsage, "candidatesTokenCount");
        pResponse->tUsage.uReasoningTokens = xllm__json_table_get_u32(tUsage, "thoughtsTokenCount");
        pResponse->tUsage.uCachedInputTokens = xllm__json_table_get_u32(tUsage, "cachedContentTokenCount");
    }

    tCandidates = xllm__json_table_get(tRoot, "candidates");
    if ( !tCandidates || xvoType(tCandidates) != XVO_DT_ARRAY || xvoArrayItemCount(tCandidates) == 0u ) {
        pResponse->eStatus = XLLM_STATUS_ERRORED;
        pResponse->sFinishReason = xllm__dup_cstr("error");
        pResponse->sVisibleText = xllm__dup_cstr("");
        pResponse->tRaw = tRoot;
        *ppResponse = pResponse;
        return XRT_NET_OK;
    }

    tCandidate = xvoArrayGetValue(tCandidates, 0u);
    sFinishReason = xllm__json_table_get_text(tCandidate, "finishReason");
    pResponse->eStatus = xllm__gemini_status_from_finish_reason(sFinishReason);
    pResponse->sFinishReason = xllm__dup_cstr(sFinishReason ? sFinishReason : "STOP");
    tContent = xllm__json_table_get(tCandidate, "content");
    tParts = xllm__json_table_get(tContent, "parts");

    if ( tParts && xvoType(tParts) == XVO_DT_ARRAY ) {
        for ( i = 0u; i < xvoArrayItemCount(tParts); ++i ) {
            xvalue tPartObj = xvoArrayGetValue(tParts, (uint32)i);
            const char *sText = xllm__json_table_get_text(tPartObj, "text");
            xvalue tFunctionCall = xllm__json_table_get(tPartObj, "functionCall");
            const char *sThoughtSignature = xllm__json_table_get_text(tPartObj, "thoughtSignature");
            bool bThought = false;

            (void)xllm__json_table_get_bool(tPartObj, "thought", &bThought);

            if ( tFunctionCall && xvoType(tFunctionCall) == XVO_DT_TABLE ) {
                const char *sName = xllm__json_table_get_text(tFunctionCall, "name");
                xvalue tArgs = xllm__json_table_get(tFunctionCall, "args");
                xllm_output_item tToolOutput;

                memset(&tToolOutput, 0, sizeof(tToolOutput));
                tToolOutput.eKind = XLLM_OUTPUT_TOOL_CALL;
                tToolOutput.as.tToolCall.sCallId = xllm__dup_cstr("gemini_call_0");
                tToolOutput.as.tToolCall.sToolId = xllm__dup_cstr(sName ? sName : "");
                tToolOutput.as.tToolCall.sToolName = xllm__dup_cstr(sName ? sName : "");
                tToolOutput.as.tToolCall.sArgumentsJson = tArgs ? (char *)xrtStringifyJSON(tArgs, FALSE, NULL) : xllm__dup_cstr("{}");
                if ( xllm__append_buffer((void **)&pResponse->pOutputs, sizeof(xllm_output_item), &pResponse->iOutputCount, &iOutputCap, &tToolOutput) != XRT_NET_OK ) {
                    xllm_response_free(pResponse);
                    xllm__xvalue_release(&tRoot);
                    return XRT_NET_ERROR;
                }
                pResponse->eStatus = XLLM_STATUS_TOOL_CALL_REQUIRED;
                continue;
            }

            if ( !sText ) {
                continue;
            }

            if ( bThought ) {
                xllm_output_item tThinkingOutput;

                memset(&tThinkingOutput, 0, sizeof(tThinkingOutput));
                tThinkingOutput.eKind = XLLM_OUTPUT_THINKING;
                tThinkingOutput.as.tThinking.bVisible = true;
                tThinkingOutput.as.tThinking.sFormat = xllm__dup_cstr("full");
                tThinkingOutput.as.tThinking.sText = xllm__dup_cstr(sText);
                if ( xllm__gemini_set_thinking_vendor_extra(&tThinkingOutput.as.tThinking, sThoughtSignature) != XRT_NET_OK ) {
                    xllm__output_item_free(&tThinkingOutput);
                    xllm_response_free(pResponse);
                    xllm__xvalue_release(&tRoot);
                    return XRT_NET_ERROR;
                }
                if ( xllm__append_buffer((void **)&pResponse->pOutputs, sizeof(xllm_output_item), &pResponse->iOutputCount, &iOutputCap, &tThinkingOutput) != XRT_NET_OK ) {
                    xllm__output_item_free(&tThinkingOutput);
                    xllm_response_free(pResponse);
                    xllm__xvalue_release(&tRoot);
                    return XRT_NET_ERROR;
                }
                continue;
            }

            if ( iMessageOutputIndex == (size_t)-1 ) {
                xllm_output_item tMessageOutput;
                memset(&tMessageOutput, 0, sizeof(tMessageOutput));
                tMessageOutput.eKind = XLLM_OUTPUT_MESSAGE;
                if ( xllm__append_buffer((void **)&pResponse->pOutputs, sizeof(xllm_output_item), &pResponse->iOutputCount, &iOutputCap, &tMessageOutput) != XRT_NET_OK ) {
                    xllm_response_free(pResponse);
                    xllm__xvalue_release(&tRoot);
                    return XRT_NET_ERROR;
                }
                iMessageOutputIndex = pResponse->iOutputCount - 1u;
            }

            {
                xllm_content_part tMsgPart;
                memset(&tMsgPart, 0, sizeof(tMsgPart));
                tMsgPart.eKind = XLLM_PART_TEXT;
                tMsgPart.as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
                tMsgPart.as.tSource.sMimeType = xllm__dup_cstr("text/plain");
                tMsgPart.as.tSource.as.sText = xllm__dup_cstr(sText);
                if ( xllm__append_buffer(
                         (void **)&pResponse->pOutputs[iMessageOutputIndex].as.tMessage.pParts,
                         sizeof(xllm_content_part),
                         &pResponse->pOutputs[iMessageOutputIndex].as.tMessage.iPartCount,
                         &iMessagePartCap,
                         &tMsgPart) != XRT_NET_OK ) {
                    xllm__content_part_free(&tMsgPart);
                    xllm_response_free(pResponse);
                    xllm__xvalue_release(&tRoot);
                    return XRT_NET_ERROR;
                }
            }

            if ( tVisible.iLen > 0u && !xllm__json_builder_append_char(&tVisible, '\n') ) {
                xllm_response_free(pResponse);
                xllm__xvalue_release(&tRoot);
                return XRT_NET_ERROR;
            }
            if ( !xllm__json_builder_append_cstr(&tVisible, sText) ) {
                xllm_response_free(pResponse);
                xllm__xvalue_release(&tRoot);
                return XRT_NET_ERROR;
            }
        }
    }

    pResponse->sVisibleText = xllm__json_builder_detach(&tVisible);
    if ( !pResponse->sVisibleText ) {
        pResponse->sVisibleText = xllm__dup_cstr("");
    }

    if ( pRequest && pEffectiveParams->tResponseFormat.eKind != XLLM_RESPONSE_TEXT && iMessageOutputIndex != (size_t)-1 ) {
        if ( xllm__openai_parse_structured_output(
                 &pEffectiveParams->tResponseFormat,
                 pOptions,
                 pResponse->sVisibleText,
                 &tJsonValue,
                 &sNormalizedJson,
                 pError) != XRT_NET_OK ) {
            xllm_response_free(pResponse);
            xllm__xvalue_release(&tRoot);
            return XRT_NET_ERROR;
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

    if ( pResponse->eStatus == XLLM_STATUS_CONTENT_FILTERED ) {
        pResponse->tSafety.sBlockReason = xllm__dup_cstr(sFinishReason ? sFinishReason : "SAFETY");
    }

    pResponse->sId = xllm__dup_cstr(xllm__json_table_get_text(tRoot, "responseId"));
    pResponse->tRaw = tRoot;
    *ppResponse = pResponse;
    xllm__free_cstr(&sNormalizedJson);
    xllm__xvalue_release(&tJsonValue);
    return XRT_NET_OK;
}

static int xllm__gemini_fill_request_headers(
    xhttprequest *pHttpRequest,
    const xllm_profile *pProfile,
    xllm_error *pError
)
{
    xllm_profile tShadowProfile;
    char *sAccessToken = NULL;

    if ( !pHttpRequest || !pProfile ) {
        return XRT_NET_ERROR;
    }

    memset(&tShadowProfile, 0, sizeof(tShadowProfile));
    tShadowProfile = *pProfile;

    if ( pProfile->sAdapter &&
         strcmp(pProfile->sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 &&
         pProfile->tAuth.eKind == XLLM_AUTH_NONE ) {
        sAccessToken = xllm__vertex_fetch_access_token(pProfile);
        if ( !sAccessToken || !sAccessToken[0] ) {
            xllm__error_set(
                pError,
                XLLM_ERROR_AUTH,
                "vertex gemini auth requires api key or GOOGLE_APPLICATION_CREDENTIALS/vertex_credentials_path"
            );
            return XRT_NET_ERROR;
        }
        tShadowProfile.tAuth.eKind = XLLM_AUTH_BEARER;
        tShadowProfile.tAuth.sScheme = "Bearer";
        tShadowProfile.tAuth.sSecret = sAccessToken;
        tShadowProfile.tAuth.sHeaderName = NULL;
    } else if ( tShadowProfile.tAuth.eKind == XLLM_AUTH_API_KEY_HEADER &&
                (!tShadowProfile.tAuth.sHeaderName || !tShadowProfile.tAuth.sHeaderName[0]) ) {
        tShadowProfile.tAuth.sHeaderName = "x-goog-api-key";
    }

    if ( xllm__openai_fill_request_headers(pHttpRequest, &tShadowProfile) != XRT_NET_OK ) {
        xllm__free_cstr(&sAccessToken);
        return XRT_NET_ERROR;
    }

    xllm__free_cstr(&sAccessToken);
    return XRT_NET_OK;
}

static int32 xllm__gemini_chat_stream_buffered(
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
    bool bParsedStream = false;
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

    sModel = xllm__openai_select_model(pProfile, pRequest, NULL);
    tStream.sSelectedModel = sModel;
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for gemini request");
        return XRT_NET_ERROR;
    }

    if ( xllm__openai_fill_effective_params(
            &tEffectiveParams,
            pProfile,
            pRequest,
            pOptions ? pOptions->eStreamMode : XLLM_STREAM_PREFER
         ) != XRT_NET_OK ) goto fail;

    if ( xllm__gemini_build_body(&tBody, pRequest, &tEffectiveParams, NULL, pError) != XRT_NET_OK ) goto fail;
    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) goto fail;

    sUrl = xllm__gemini_build_url(pProfile, sModel, true);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini profile missing base url");
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) goto fail;
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) goto fail;
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) goto fail;
    if ( xllm__gemini_fill_request_headers(&tHttpRequest, pProfile, pError) != XRT_NET_OK ) goto fail;
    if ( !xrtHttpRequestSetHeader(&tHttpRequest, "Accept", "text/event-stream") ) goto fail;
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) goto fail;

retry_execute:
    ++uAttempt;
    xllm__gemini_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__gemini_component_name(pProfile),
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
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "gemini stream request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini stream request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "gemini stream request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-goog-request-id");
    if ( !sRequestId || !sRequestId[0] ) {
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__gemini_fill_error_from_http(pError, pHttpResponse, tRoot);
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "gemini stream response body is empty");
        goto fail;
    }

    sContentType = xrtHttpResponseHeader(pHttpResponse, "content-type");
    bTreatAsSse = xllm__buffer_starts_with_sse_data(pHttpResponse->pBody, pHttpResponse->iBodyLen);
    if ( !bTreatAsSse && sContentType ) {
        bTreatAsSse = xllm__text_contains_ci(sContentType, "text/event-stream");
    }

    if ( bTreatAsSse ) {
        if ( xllm__gemini_stream_process_buffer(&tStream, pHttpResponse->pBody, pHttpResponse->iBodyLen) != XRT_NET_OK ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }
        if ( tStream.iParsedBytes < pHttpResponse->iBodyLen ) {
            size_t iRemain = pHttpResponse->iBodyLen - tStream.iParsedBytes;
            if ( xllm__gemini_stream_process_event_block(&tStream, pHttpResponse->pBody + tStream.iParsedBytes, iRemain) != XRT_NET_OK ) {
                if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini stream cancelled");
                }
                iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                goto fail;
            }
            tStream.iParsedBytes = pHttpResponse->iBodyLen;
        }
        bParsedStream = tStream.iParsedBytes > 0u || tStream.bDone || tStream.pResponse != NULL;
    } else {
        tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        if ( tRoot && xvoType(tRoot) == XVO_DT_ARRAY ) {
            size_t i;

            for ( i = 0u; i < xvoGetSize(tRoot); ++i ) {
                xvalue tItem = xvoArrayGetValue(tRoot, (uint32)i);
                if ( xllm__gemini_stream_process_root(&tStream, tItem, 0u) != XRT_NET_OK ) {
                    if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini stream cancelled");
                    }
                    iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                    goto fail;
                }
            }
            bParsedStream = xvoGetSize(tRoot) > 0u;
        } else if ( tRoot && xvoType(tRoot) == XVO_DT_TABLE ) {
            if ( xllm__gemini_stream_process_root(&tStream, tRoot, pHttpResponse->iBodyLen) != XRT_NET_OK ) {
                if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                    xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini stream cancelled");
                }
                iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
                goto fail;
            }
            bParsedStream = true;
        }
    }

    if ( bParsedStream ) {
        if ( xllm__openai_stream_finalize_response(&tStream) != XRT_NET_OK ) {
            if ( pError && pError->eCode == XLLM_ERROR_NONE && tStream.bCancelled ) {
                xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini stream cancelled");
            }
            iStatus = tStream.bCancelled ? XRT_NET_CANCELLED : XRT_NET_ERROR;
            goto fail;
        }

        pResponse = tStream.pResponse;
        tStream.pResponse = NULL;
        if ( !pResponse ) {
            xllm__error_set(pError, XLLM_ERROR_PARSE, "gemini stream did not produce a response");
            goto fail;
        }

        *ppResponse = pResponse;
        pResponse = NULL;
        xllm__gemini_logf(
            pRuntime,
            XLLM_LOG_INFO,
            xllm__gemini_component_name(pProfile),
            "response complete: model=%s streaming=true attempt=%u status=%s outputs=%u",
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
        xllm__error_set(pError, XLLM_ERROR_UNSUPPORTED_CAPABILITY, "gemini upstream did not return a stream response");
        goto fail;
    }

    if ( !tRoot ) {
        tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
    }
    if ( !tRoot ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "failed to parse gemini streaming fallback response");
        goto fail;
    }
    if ( xllm__gemini_parse_response(
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
    xllm__xvalue_release(&tRoot);
    if ( xllm__openai_emit_synthetic_events(pResponse, pOptions) != XRT_NET_OK ) {
        if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini stream cancelled");
        }
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__gemini_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__gemini_component_name(pProfile),
        "response fallback complete: model=%s streaming=true attempt=%u status=%s outputs=%u",
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

            xllm__gemini_logf(
                pRuntime,
                XLLM_LOG_WARN,
                xllm__gemini_component_name(pProfile),
                "response failed: model=%s streaming=true attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->sMessage) ? pError->sMessage : "",
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
            if ( tRoot ) {
                xvoUnref(tRoot);
                tRoot = NULL;
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

        xllm__gemini_logf(
            pRuntime,
            XLLM_LOG_WARN,
            xllm__gemini_component_name(pProfile),
            "response failed: model=%s streaming=true attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            (unsigned)(uAttempt ? uAttempt : 1u),
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->sMessage) ? pError->sMessage : "",
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

    if ( tRoot ) {
        xvoUnref(tRoot);
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

static int32 xllm__gemini_chat_common(
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
    int iStatus = XRT_NET_ERROR;
    bool bRetryable = false;
    bool bStreaming = false;
    uint32 uAttempt = 0u;
    uint32 uMaxAttempts = 1u;

    if ( !pProfile || !pRequest || !ppResponse ) {
        return XRT_NET_ERROR;
    }
    *ppResponse = NULL;

    memset(&tBody, 0, sizeof(tBody));
    memset(&tEffectiveParams, 0, sizeof(tEffectiveParams));
    xrtHttpRequestInit(&tHttpRequest);

    bStreaming = (pOptions && pOptions->eStreamMode != XLLM_STREAM_OFF);
    if ( pOptions && pOptions->uMaxRetries > 0u ) {
        uMaxAttempts += pOptions->uMaxRetries;
    }

    sModel = xllm__openai_select_model(pProfile, pRequest, NULL);
    if ( !sModel || !sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "no model bound for gemini request");
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

    if ( xllm__gemini_build_body(&tBody, pRequest, &tEffectiveParams, NULL, pError) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    sBody = xllm__json_builder_detach(&tBody);
    if ( !sBody ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    sUrl = xllm__gemini_build_url(pProfile, sModel, false);
    if ( !sUrl ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_REQUEST, "gemini profile missing base url");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( !xrtHttpRequestSetMethod(&tHttpRequest, "POST") ) goto fail;
    if ( !xrtHttpRequestSetURL(&tHttpRequest, sUrl) ) goto fail;
    if ( !xrtHttpRequestSetBodyCopy(&tHttpRequest, sBody, strlen(sBody), "application/json") ) goto fail;
    if ( xllm__gemini_fill_request_headers(&tHttpRequest, pProfile, pError) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }
    if ( xllm__openai_apply_transport(&tHttpRequest, pRuntime, pProfile, pOptions, pError) != XRT_NET_OK ) {
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

retry_execute:
    ++uAttempt;
    xllm__gemini_logf(
        pRuntime,
        XLLM_LOG_DEBUG,
        xllm__gemini_component_name(pProfile),
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
            xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "gemini request timed out");
            iStatus = XRT_NET_TIMEOUT;
        } else if ( iNetStatus == XRT_NET_CANCELLED ) {
            xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini request cancelled");
            iStatus = XRT_NET_CANCELLED;
        } else {
            xllm__error_set(pError, XLLM_ERROR_NETWORK, "gemini request failed");
            iStatus = XRT_NET_ERROR;
        }
        goto fail;
    }

    sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-goog-request-id");
    if ( !sRequestId || !sRequestId[0] ) {
        sRequestId = xrtHttpResponseHeader(pHttpResponse, "x-request-id");
    }

    if ( pHttpResponse->iStatusCode >= 400u ) {
        xvalue tRoot = NULL;

        if ( pHttpResponse->pBody && pHttpResponse->iBodyLen > 0u ) {
            tRoot = xrtParseJSON((str)pHttpResponse->pBody, pHttpResponse->iBodyLen);
        }
        xllm__gemini_fill_error_from_http(pError, pHttpResponse, tRoot);
        xllm__xvalue_release(&tRoot);
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( !pHttpResponse->pBody || pHttpResponse->iBodyLen == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_PARSE, "gemini response body is empty");
        iStatus = XRT_NET_ERROR;
        goto fail;
    }

    if ( xllm__gemini_parse_response(
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
        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "gemini synthetic event stream cancelled");
        iStatus = XRT_NET_CANCELLED;
        goto fail;
    }

    *ppResponse = pResponse;
    pResponse = NULL;
    xllm__gemini_logf(
        pRuntime,
        XLLM_LOG_INFO,
        xllm__gemini_component_name(pProfile),
        "response complete: model=%s streaming=%s attempt=%u status=%s outputs=%u",
        (*ppResponse)->sModel ? (*ppResponse)->sModel : sModel,
        bStreaming ? "true" : "false",
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
            bStreaming,
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

            xllm__gemini_logf(
                pRuntime,
                XLLM_LOG_WARN,
                xllm__gemini_component_name(pProfile),
                "response failed: model=%s streaming=%s attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=true retry_in_ms=%u",
                sModel ? sModel : "",
                bStreaming ? "true" : "false",
                (unsigned)uAttempt,
                (unsigned)uMaxAttempts,
                (int)iTraceTransportStatus,
                (int)iHttpStatus,
                (pError && pError->sMessage) ? pError->sMessage : "",
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

        xllm__gemini_logf(
            pRuntime,
            XLLM_LOG_WARN,
            xllm__gemini_component_name(pProfile),
            "response failed: model=%s streaming=%s attempt=%u/%u transport_status=%d http_status=%d error=%s retryable=%s",
            sModel ? sModel : "",
            bStreaming ? "true" : "false",
            (unsigned)(uAttempt ? uAttempt : 1u),
            (unsigned)uMaxAttempts,
            (int)iTraceTransportStatus,
            (int)iHttpStatus,
            (pError && pError->sMessage) ? pError->sMessage : "",
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

static int32 xllm__gemini_native_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    if ( pOptions && pOptions->eStreamMode != XLLM_STREAM_OFF ) {
        return xllm__gemini_chat_stream_buffered(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
    }
    return xllm__gemini_chat_common(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
}

static int32 xllm__vertex_gemini_native_chat(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    if ( pOptions && pOptions->eStreamMode != XLLM_STREAM_OFF ) {
        return xllm__gemini_chat_stream_buffered(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
    }
    return xllm__gemini_chat_common(pCtx, pProfile, pRequest, pOptions, ppResponse, pError);
}

XLLM_API int xllm_register_gemini_native_adapter(xllm_runtime *pRuntime)
{
    xllm_adapter tAdapter;

    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    memset(&tAdapter, 0, sizeof(tAdapter));
    tAdapter.sName = XLLM_ADAPTER_GEMINI_NATIVE;
    tAdapter.pCtx = pRuntime;
    tAdapter.pfnChat = xllm__gemini_native_chat;
    return xllm_register_adapter(pRuntime, &tAdapter);
}

XLLM_API int xllm_register_vertex_gemini_native_adapter(xllm_runtime *pRuntime)
{
    xllm_adapter tAdapter;

    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    memset(&tAdapter, 0, sizeof(tAdapter));
    tAdapter.sName = XLLM_ADAPTER_VERTEX_GEMINI_NATIVE;
    tAdapter.pCtx = pRuntime;
    tAdapter.pfnChat = xllm__vertex_gemini_native_chat;
    return xllm_register_adapter(pRuntime, &tAdapter);
}
