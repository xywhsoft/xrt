#include "azure_openai_config.h"
#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

static const char *demo_dup_text(const char *sText)
{
    return (const char *)xrtCopyStr((str)sText, 0u);
}

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    (void)pUserData;

    if ( pEvent == NULL ) {
        return true;
    }

    if ( pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("%s", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
        fflush(stdout);
    }

    return true;
}

static int demo_prepare_text_message(xllm_message *pMessage, xllm_role eRole, const char *sText)
{
    if ( pMessage == NULL || sText == NULL ) {
        return XRT_NET_ERROR;
    }

    pMessage->eRole = eRole;
    pMessage->iPartCount = 1u;
    pMessage->pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    if ( pMessage->pParts == NULL ) {
        return XRT_NET_ERROR;
    }

    pMessage->pParts[0].eKind = XLLM_PART_TEXT;
    pMessage->pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pMessage->pParts[0].as.tSource.as.sText = demo_dup_text(sText);
    if ( pMessage->pParts[0].as.tSource.as.sText == NULL ) {
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
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

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_request tRequest;
    xllm_call_options tCallOptions;
    xllm_error tError;
    char aBaseUrl[1024];
    const char *sApiKey;
    const char *sText;
    int iStatus;

#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    xrtInit();

    if ( demo_build_azure_openai_chat_url(aBaseUrl, sizeof(aBaseUrl)) != 0 ) {
        fprintf(stderr, "failed to build Azure OpenAI chat URL\n");
        return 1;
    }

    printf("=== Azure OpenAI Stateless Example ===\n");
    printf("endpoint   : %s\n", AZURE_OPENAI_ENDPOINT);
    printf("model name : %s\n", AZURE_OPENAI_MODEL_NAME);
    printf("deployment : %s\n", AZURE_OPENAI_DEPLOYMENT);
    printf("api ver    : %s\n", AZURE_OPENAI_API_VERSION);
    printf("base url   : %s\n\n", aBaseUrl);

    sApiKey = demo_get_azure_openai_api_key();
    if ( sApiKey == NULL || sApiKey[0] == '\0' ) {
        fprintf(stderr, "missing environment variable: AZURE_OPENAI_API_KEY\n");
        return 2;
    }

    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || pRuntime == NULL ) {
        fprintf(stderr, "failed to create runtime\n");
        return 3;
    }

    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register openai-compatible adapter\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = "azure-openai";
    tProfile.sProvider = "azure-openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = aBaseUrl;
    tProfile.tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
    tProfile.tAuth.sHeaderName = "api-key";
    tProfile.tAuth.sSecret = sApiKey;
    tProfile.tModels.tText.sModelId = AZURE_OPENAI_DEPLOYMENT;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "failed to register profile\n");
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    xllm_request_init(&tRequest);
    tRequest.sProfileId = demo_dup_text("azure-openai");
    if ( tRequest.sProfileId == NULL ) {
        fprintf(stderr, "failed to copy profile id\n");
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }

    tRequest.iMessageCount = 2u;
    tRequest.pMessages = (xllm_message *)xrtCalloc(2u, sizeof(xllm_message));
    if ( tRequest.pMessages == NULL ) {
        fprintf(stderr, "failed to allocate messages\n");
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }

    if ( demo_prepare_text_message(&tRequest.pMessages[0], XLLM_ROLE_SYSTEM, AZURE_OPENAI_SYSTEM_PROMPT) != XRT_NET_OK ||
         demo_prepare_text_message(&tRequest.pMessages[1], XLLM_ROLE_USER, "Please reply with one short sentence to confirm Azure OpenAI connectivity.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build request messages\n");
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 8;
    }

    xllm_call_options_init(&tCallOptions);
    tCallOptions.eStreamMode = XLLM_STREAM_PREFER;
    tCallOptions.pfnOnEvent = demo_on_event;
    tCallOptions.uTimeoutMs = 120000u;

    printf("[assistant] ");
    iStatus = xllm_chat_ex(pRuntime, &tRequest, &tCallOptions, &pResponse, &tError);
    printf("\n");

    if ( iStatus != XRT_NET_OK || pResponse == NULL ) {
        demo_print_error("Azure OpenAI stateless request failed", iStatus, &tError);
        xllm_error_free(&tError);
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 9;
    }

    sText = xllm_response_get_text(pResponse);
    printf("\nvisible text: %s\n", sText ? sText : "(null)");
    if ( pResponse->sModel != NULL ) {
        printf("response model: %s\n", pResponse->sModel);
    }
    if ( pResponse->tUsage.uInputTokens > 0u || pResponse->tUsage.uOutputTokens > 0u ) {
        printf("usage: input=%u output=%u\n",
               pResponse->tUsage.uInputTokens,
               pResponse->tUsage.uOutputTokens);
    }

    xllm_response_free(pResponse);
    xllm_error_free(&tError);
    xllm_request_reset(&tRequest);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
