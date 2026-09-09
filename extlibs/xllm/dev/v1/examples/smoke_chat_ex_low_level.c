#include "xllm-session.h"

#include <stdio.h>
#include <string.h>

static char *demo_dupstr(const char *sText)
{
    size_t iLen;
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }

    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_request tRequest;
    xllm_error tError;
    int iStatus;

    xllm_profile_init(&tProfile);
    xllm_request_init(&tRequest);
    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "create runtime failed\n");
        return 1;
    }
    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "register adapter failed\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    tProfile.sId = "openai-chat-ex";
    tProfile.sProvider = "openai";
    tProfile.sAdapter = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl = "http://127.0.0.1:1/v1";
    tProfile.tAuth.eKind = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret = "test-key";
    tProfile.tModels.tText.sModelId = "gpt-mock";
    tProfile.tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT;
    tProfile.tModels.tMultimodal.sModelId = "gpt-mock-mm";
    tProfile.tModels.tMultimodal.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_IMAGE_IN | XLLM_CAP_TEXT_OUT;
    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "register profile failed\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    tRequest.sProfileId = demo_dupstr("openai-chat-ex");
    tRequest.eSlot = XLLM_SLOT_MULTIMODAL;
    tRequest.iMessageCount = 1u;
    tRequest.pMessages = (xllm_message *)xrtCalloc(1u, sizeof(xllm_message));
    if ( !tRequest.sProfileId || !tRequest.pMessages ) {
        fprintf(stderr, "allocate request failed\n");
        return 4;
    }

    tRequest.pMessages[0].eRole = XLLM_ROLE_USER;
    tRequest.pMessages[0].iPartCount = 2u;
    tRequest.pMessages[0].pParts = (xllm_content_part *)xrtCalloc(2u, sizeof(xllm_content_part));
    if ( !tRequest.pMessages[0].pParts ) {
        fprintf(stderr, "allocate message parts failed\n");
        return 5;
    }

    tRequest.pMessages[0].pParts[0].eKind = XLLM_PART_TEXT;
    tRequest.pMessages[0].pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    tRequest.pMessages[0].pParts[0].as.tSource.sMimeType = demo_dupstr("text/plain");
    tRequest.pMessages[0].pParts[0].as.tSource.as.sText = demo_dupstr("describe this file-id image");

    tRequest.pMessages[0].pParts[1].eKind = XLLM_PART_IMAGE;
    tRequest.pMessages[0].pParts[1].as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
    tRequest.pMessages[0].pParts[1].as.tSource.sMimeType = demo_dupstr("image/png");
    tRequest.pMessages[0].pParts[1].as.tSource.as.sFileId = demo_dupstr("file-img-low-level");

    if ( !tRequest.pMessages[0].pParts[0].as.tSource.sMimeType ||
         !tRequest.pMessages[0].pParts[0].as.tSource.as.sText ||
         !tRequest.pMessages[0].pParts[1].as.tSource.sMimeType ||
         !tRequest.pMessages[0].pParts[1].as.tSource.as.sFileId ) {
        fprintf(stderr, "allocate part content failed\n");
        return 6;
    }

    iStatus = xllm_chat_ex(pRuntime, &tRequest, NULL, &pResponse, &tError);
    if ( iStatus == XRT_NET_OK || pResponse ) {
        fprintf(stderr, "xllm_chat_ex unexpectedly succeeded\n");
        return 7;
    }
    if ( tError.eCode != XLLM_ERROR_UNSUPPORTED_INPUT_TYPE ) {
        fprintf(stderr, "unexpected error code: %d\n", (int)tError.eCode);
        return 8;
    }
    if ( !tError.sMessage || strstr(tError.sMessage, "provider file_id") == NULL ) {
        fprintf(stderr, "unexpected error message: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        return 9;
    }

    xllm_error_free(&tError);
    xllm_request_reset(&tRequest);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
