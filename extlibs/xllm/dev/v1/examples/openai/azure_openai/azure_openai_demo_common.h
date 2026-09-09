#ifndef DEMO_AZURE_OPENAI_DEMO_COMMON_H
#define DEMO_AZURE_OPENAI_DEMO_COMMON_H

#include "azure_openai_config.h"
#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

static void demo_setup_console(void)
{
#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static const char *demo_dup_text(const char *sText)
{
    return (const char *)xrtCopyStr((str)sText, 0u);
}

static const char *demo_env_optional(const char *sName, const char *sDefaultValue)
{
    const char *sValue = getenv(sName);
    if ( sValue == NULL || sValue[0] == '\0' ) {
        return sDefaultValue;
    }
    return sValue;
}

static bool demo_env_enabled(const char *sName)
{
    const char *sValue = getenv(sName);

    if ( sValue == NULL || sValue[0] == '\0' ) {
        return false;
    }

    return strcmp(sValue, "1") == 0 ||
           strcmp(sValue, "true") == 0 ||
           strcmp(sValue, "TRUE") == 0 ||
           strcmp(sValue, "on") == 0 ||
           strcmp(sValue, "ON") == 0;
}

static uint32 demo_env_u32(const char *sName, uint32 uDefaultValue)
{
    const char *sValue = getenv(sName);
    char *pEnd = NULL;
    unsigned long uParsed;

    if ( sValue == NULL || sValue[0] == '\0' ) {
        return uDefaultValue;
    }

    errno = 0;
    uParsed = strtoul(sValue, &pEnd, 10);
    if ( errno != 0 || pEnd == sValue || (pEnd && *pEnd != '\0') ) {
        return uDefaultValue;
    }
    if ( uParsed == 0ul ) {
        return uDefaultValue;
    }
    if ( uParsed > 0xfffffffful ) {
        return 0xffffffffu;
    }
    return (uint32)uParsed;
}

static const char *demo_log_level_name(xllm_log_level eLevel)
{
    switch ( eLevel ) {
        case XLLM_LOG_ERROR: return "error";
        case XLLM_LOG_WARN: return "warn";
        case XLLM_LOG_INFO: return "info";
        case XLLM_LOG_DEBUG: return "debug";
        case XLLM_LOG_TRACE: return "trace";
        default: return "unknown";
    }
}

static void demo_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    FILE *pOut = (FILE *)pCtx;

    if ( pOut == NULL ) {
        pOut = stderr;
    }

    fprintf(
        pOut,
        "[xllm:%s:%s] %s\n",
        demo_log_level_name(eLevel),
        sComponent ? sComponent : "(null)",
        sMessage ? sMessage : "(null)"
    );
    fflush(pOut);
}

static void demo_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    FILE *pOut = (FILE *)pCtx;
    char *sJson = NULL;

    if ( pOut == NULL ) {
        pOut = stderr;
    }

    if ( pPayload && *pPayload ) {
        sJson = (char *)xrtStringifyJSON(*pPayload, 0, NULL);
    }

    fprintf(
        pOut,
        "[xllm:trace:%d] %s\n",
        (int)eKind,
        sJson ? sJson : "(null)"
    );
    fflush(pOut);

    if ( sJson ) {
        xrtFree(sJson);
    }
}

static bool demo_on_event_print(const xllm_event *pEvent, void *pUserData)
{
    (void)pUserData;

    if ( pEvent == NULL ) {
        return true;
    }

    switch ( pEvent->eType ) {
        case XLLM_EVENT_TEXT_DELTA:
            printf("%s", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
            fflush(stdout);
            break;
        case XLLM_EVENT_THINKING_DELTA:
            printf("[thinking:%s]", pEvent->as.tThinkingDelta.sText ? pEvent->as.tThinkingDelta.sText : "");
            fflush(stdout);
            break;
        case XLLM_EVENT_ARTIFACT_BEGIN:
            printf("\n[artifact-begin id=%s mime=%s name=%s]\n",
                   pEvent->as.tArtifactBegin.tInfo.sArtifactId ? pEvent->as.tArtifactBegin.tInfo.sArtifactId : "(null)",
                   pEvent->as.tArtifactBegin.tInfo.sMimeType ? pEvent->as.tArtifactBegin.tInfo.sMimeType : "(null)",
                   pEvent->as.tArtifactBegin.tInfo.sName ? pEvent->as.tArtifactBegin.tInfo.sName : "(null)");
            break;
        case XLLM_EVENT_ARTIFACT_READY:
            printf("[artifact-ready id=%s mime=%s name=%s]\n",
                   pEvent->as.tArtifactReady.tInfo.sArtifactId ? pEvent->as.tArtifactReady.tInfo.sArtifactId : "(null)",
                   pEvent->as.tArtifactReady.tInfo.sMimeType ? pEvent->as.tArtifactReady.tInfo.sMimeType : "(null)",
                   pEvent->as.tArtifactReady.tInfo.sName ? pEvent->as.tArtifactReady.tInfo.sName : "(null)");
            break;
        default:
            break;
    }

    return true;
}

static void demo_print_error(const char *sPrefix, int iStatus, const xllm_error *pError)
{
    fprintf(stderr, "%s (status=%d", sPrefix, iStatus);
    if ( pError != NULL ) {
        fprintf(stderr,
                ", code=%d, http=%d, msg=%s",
                (int)pError->eCode,
                (int)pError->iHttpStatus,
                pError->sMessage ? pError->sMessage : "(null)");
        if ( pError->sRequestId != NULL && pError->sRequestId[0] != '\0' ) {
            fprintf(stderr, ", request_id=%s", pError->sRequestId);
        }
    }
    fprintf(stderr, ")\n");
}

static int demo_create_runtime_and_llm(
    uint64 uTextCaps,
    uint64 uMultimodalCaps,
    xllm_runtime **ppRuntime,
    xllm **ppLlm
)
{
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm_runtime *pRuntime = NULL;
    xllm *pLlm = NULL;
    char aBaseUrl[1024];
    const char *sApiKey;

    if ( ppRuntime == NULL || ppLlm == NULL ) {
        return XRT_NET_ERROR;
    }

    *ppRuntime = NULL;
    *ppLlm = NULL;

    sApiKey = demo_get_azure_openai_api_key();
    if ( sApiKey == NULL || sApiKey[0] == '\0' ) {
        fprintf(stderr, "missing environment variable: AZURE_OPENAI_API_KEY\n");
        return XRT_NET_ERROR;
    }

    if ( demo_build_azure_openai_chat_url(aBaseUrl, sizeof(aBaseUrl)) != 0 ) {
        fprintf(stderr, "failed to build Azure OpenAI chat URL\n");
        return XRT_NET_ERROR;
    }

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || pRuntime == NULL ) {
        return XRT_NET_ERROR;
    }
    if ( demo_env_enabled("AZURE_OPENAI_DEBUG") ) {
        (void)xllm_runtime_set_log_callback(pRuntime, demo_log_callback, stderr);
        (void)xllm_runtime_set_trace_callback(pRuntime, demo_trace_callback, stderr);
        (void)xllm_runtime_set_debug_mode(pRuntime, XLLM_DEBUG_BODY, XLLM_REDACT_DEFAULT);
    }
    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        xllm_runtime_destroy(pRuntime);
        return XRT_NET_ERROR;
    }

    memset(&tCreate, 0, sizeof(tCreate));
    xllm_profile_init(&tProfile);
    tProfile.sId = "azure-openai";
    tProfile.sProvider = "azure-openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = aBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sHeaderName = "api-key";
    tProfile.tAuth.sSecret = sApiKey;
    tProfile.tModels.tText.sModelId = AZURE_OPENAI_DEPLOYMENT;
    tProfile.tModels.tText.tCaps.uFlags = uTextCaps;
    tProfile.tModels.tMultimodal.sModelId = AZURE_OPENAI_DEPLOYMENT;
    tProfile.tModels.tMultimodal.tCaps.uFlags = uMultimodalCaps;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        xllm_runtime_destroy(pRuntime);
        return XRT_NET_ERROR;
    }

    tCreate.sInitialProfileId = "azure-openai";
    tCreate.sSystemPrompt = AZURE_OPENAI_SYSTEM_PROMPT;
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( pLlm == NULL ) {
        xllm_runtime_destroy(pRuntime);
        return XRT_NET_ERROR;
    }

    *ppRuntime = pRuntime;
    *ppLlm = pLlm;
    return XRT_NET_OK;
}

static int demo_prepare_text_turn(xllm_turn *pTurn, const char *sText)
{
    xllm_turn_init(pTurn);
    return xllm_turn_add_user_text(pTurn, sText);
}

#endif
