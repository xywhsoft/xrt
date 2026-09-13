/*
 * Live smoke test against real LLM endpoints (local llama.cpp/qwen or the
 * GLM cloud); exercises all wire dialects over a real HTTP/TLS stack.
 * Not part of the default gate; fully environment driven:
 *   XLLM_LIVE_URL/_MODEL/_KEY        base completions sections
 *   XLLM_LIVE_ANTHROPIC_URL          anthropic section (skip when unset)
 *   XLLM_LIVE_RESPONSES_URL          responses section (skip when unset)
 *   XLLM_LIVE_VISION_URL/_MODEL/_KEY multimodal section
 *   XLLM_LIVE_VISION_SKIP_COMPLETIONS=1  endpoint rejects image content
 *   XLLM_LIVE_THINKING_OFF=1         llama.cpp chat_template_kwargs hatch
 * GLM cloud:
 *   XLLM_LIVE_URL=https://open.bigmodel.cn/api/paas/v4
 *   XLLM_LIVE_ANTHROPIC_URL=https://open.bigmodel.cn/api/anthropic
 *   XLLM_LIVE_VISION_SKIP_COMPLETIONS=1 (v4 rejects images; GLM multimodal
 *   rides the anthropic endpoint)
 */

#include "../xllm.c"

#include <stdio.h>

static int g_iFailures = 0;
static const char* g_sBaseUrl;
static const char* g_sModel;
static const char* g_sVisionUrl;
static const char* g_sVisionModel;
static const char* g_sVisionKey;
static const char* g_sApiKey;
static const char* g_sAnthropicUrl;
static const char* g_sResponsesUrl;
static bool g_bThinkingOff;

#define CHECK(expr, name) do { \
    bool xllm_live_ok__ = !!(expr); \
    printf("  %-58s %s\n", (name), xllm_live_ok__ ? "PASS" : "FAIL"); \
    if ( !xllm_live_ok__ ) { ++g_iFailures; } \
} while (0)

static xllm_client* live_make_client_url(xllm_provider eProvider, const char* sUrl)
{
    xllm_client_config tConfig;
    xllm_error tError;
    xllmClientConfigInit(&tConfig);
    tConfig.sBaseUrl = sUrl;
    tConfig.sApiKey = g_sApiKey;
    tConfig.sModel = g_sModel;
    tConfig.eProvider = eProvider;
    tConfig.uMaxOutputTokens = 96u;
    tConfig.uTimeoutMs = 60u * 1000u;
    tConfig.uMaxAttempts = 2u;
    tConfig.bVerifyPeer = true;
    return xllmClientCreate(&tConfig, &tError);
}

static xllm_client* live_make_client(xllm_provider eProvider, const char* sSuffix)
{
    char sUrl[256];
    (void)snprintf(sUrl, sizeof(sUrl), "%s%s", g_sBaseUrl, sSuffix);
    return live_make_client_url(eProvider, sUrl);
}

typedef struct live_events {
    int iText;
    int iReasoning;
    int iTool;
    int iUsage;
} live_events;

static bool live_on_event(void* pUserData, const xllm_event* pEvent)
{
    live_events* pEvents = (live_events*)pUserData;
    switch ( pEvent->eKind ) {
        case XLLM_EVENT_TEXT_DELTA: ++pEvents->iText; break;
        case XLLM_EVENT_REASONING_DELTA: ++pEvents->iReasoning; break;
        case XLLM_EVENT_TOOL_CALL_DELTA: ++pEvents->iTool; break;
        case XLLM_EVENT_USAGE: ++pEvents->iUsage; break;
        default: break;
    }
    return true;
}

static void live_dump_error(const char* sLabel, const xllm_error* pError)
{
    printf("    %s: code=%s http=%d msg=%s provider=%s\n", sLabel,
        xllmErrorCodeName(pError->eCode), pError->iHttpStatus,
        pError->sMessage, pError->sProviderMessage);
}

static void live_completions(void)
{
    const char* sSuffix = strstr(g_sBaseUrl, "/chat/completions") != NULL ||
        strstr(g_sBaseUrl, "/v4") != NULL ? "" : "/v1";
    xllm_client* pClient = live_make_client(XLLM_PROVIDER_OPENAI_COMPAT, sSuffix);
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_stream_callbacks tCallbacks;
    live_events tEvents = {0};
    xllm_error tError;
    printf("completions dialect\n");
    CHECK(pClient != NULL, "client created");
    if ( !pClient ) { return; }
    memset(&tCallbacks, 0, sizeof(tCallbacks));
    tCallbacks.pUserData = &tEvents;
    tCallbacks.OnEvent = live_on_event;
    xllmRequestInit(&tRequest);
    xllmRequestSetModel(&tRequest, g_sModel);
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Reply with the single word: PONG");
    CHECK(xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError) == XLLM_RESULT_OK,
        "streaming completion succeeds");
    if ( pResponse ) {
        CHECK((pResponse->sContent && pResponse->sContent[0]) ||
            (pResponse->sReasoningContent && pResponse->sReasoningContent[0]),
            "stream text assembled");
        CHECK(tEvents.iText > 0 || tEvents.iReasoning > 0, "text delta events observed");
        CHECK(pResponse->tUsage.uOutputTokens > 0u, "usage reported");
        CHECK(pResponse->tStats.uFirstTokenMs > 0u && pResponse->tStats.uTotalMs > 0u &&
            pResponse->tStats.uFirstTokenMs <= pResponse->tStats.uTotalMs,
            "first-token and total timing captured");
        CHECK(pResponse->eFinish == XLLM_FINISH_STOP || pResponse->eFinish == XLLM_FINISH_LENGTH,
            "finish reason normalized");
        xllmResponseDestroy(pResponse);
        pResponse = NULL;
    } else {
        live_dump_error("completion", &tError);
    }
    pResponse = NULL;
    tEvents.iText = 0;
    tRequest.bStream = false;
    CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) == XLLM_RESULT_OK &&
        pResponse && ((pResponse->sContent && pResponse->sContent[0]) ||
            (pResponse->sReasoningContent && pResponse->sReasoningContent[0])),
        "non-streaming completion succeeds");
    if ( !pResponse ) { live_dump_error("non-stream", &tError); }
    else if ( !pResponse->sContent[0] && pResponse->sReasoningContent ) {
        printf("    note: reasoning-only answer (%.60s...)\n", pResponse->sReasoningContent);
    }
    xllmResponseDestroy(pResponse);
    xllmRequestUnit(&tRequest);

    xllmRequestInit(&tRequest);
    xllmRequestSetModel(&tRequest, g_sModel);
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER,
        "What is the weather in Paris? You must call the get_weather tool.");
    xllmRequestAddTool(&tRequest, "get_weather", "Get current weather for a city",
        "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"}},\"required\":[\"city\"]}", false);
    pResponse = NULL;
    tEvents.iTool = 0;
    CHECK(xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError) == XLLM_RESULT_OK,
        "tool-call completion round trip succeeds");
    if ( pResponse ) {
        if ( pResponse->iToolCallCount >= 1u ) {
            CHECK(pResponse->pToolCalls[0].sName &&
                strcmp(pResponse->pToolCalls[0].sName, "get_weather") == 0 &&
                pResponse->pToolCalls[0].sArgumentsJson && pResponse->pToolCalls[0].sArgumentsJson[0],
                "tool call captured with arguments");
        } else {
            printf("    %-58s SKIP (model declined the tool)\n",
                "tool call captured with arguments");
        }
        xllmResponseDestroy(pResponse);
    } else {
        live_dump_error("tool-call", &tError);
    }
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
}

static void live_anthropic(void)
{
    xllm_client* pClient = NULL;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_stream_callbacks tCallbacks;
    live_events tEvents = {0};
    xllm_error tError;
    printf("anthropic dialect\n");
    if ( g_sAnthropicUrl == NULL ) {
        printf("  %-58s SKIP (no XLLM_LIVE_ANTHROPIC_URL)\n",
            "client created on /v1/messages");
        return;
    }
    pClient = live_make_client_url(XLLM_PROVIDER_ANTHROPIC, g_sAnthropicUrl);
    CHECK(pClient != NULL && strstr(pClient->sTarget, "/v1/messages") != NULL, "client created on /v1/messages");
    if ( !pClient ) { return; }
    memset(&tCallbacks, 0, sizeof(tCallbacks));
    tCallbacks.pUserData = &tEvents;
    tCallbacks.OnEvent = live_on_event;
    xllmRequestInit(&tRequest);
    xllmRequestSetModel(&tRequest, g_sModel);
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Answer in one short sentence.");
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Name the capital of France.");
    pResponse = NULL;
    CHECK(xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError) == XLLM_RESULT_OK,
        "typed-event stream succeeds");
    if ( pResponse ) {
        CHECK((pResponse->sContent && pResponse->sContent[0]) ||
            (pResponse->sReasoningContent && pResponse->sReasoningContent[0]),
            "anthropic text assembled");
        CHECK(pResponse->tUsage.uInputTokens > 0u && pResponse->tUsage.uOutputTokens > 0u,
            "anthropic usage (message_start + message_delta) captured");
        CHECK(pResponse->eFinish == XLLM_FINISH_STOP || pResponse->eFinish == XLLM_FINISH_LENGTH,
            "anthropic stop reason normalized");
        printf("    content: %.100s\n", pResponse->sContent ? pResponse->sContent : "");
        xllmResponseDestroy(pResponse);
    } else {
        live_dump_error("anthropic", &tError);
    }

    xllmRequestInit(&tRequest);
    xllmRequestSetModel(&tRequest, g_sModel);
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Use tools when asked.");
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER,
        "Check the weather in Tokyo. You must call the get_weather tool.");
    xllmRequestAddTool(&tRequest, "get_weather", "Get current weather for a city",
        "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"}},\"required\":[\"city\"]}", false);
    pResponse = NULL;
    tEvents.iTool = 0;
    CHECK(xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError) == XLLM_RESULT_OK,
        "anthropic tool round trip succeeds");
    if ( pResponse ) {
        if ( pResponse->iToolCallCount >= 1u ) {
            CHECK(pResponse->pToolCalls[0].sName,
                "tool_use block captured via input_json_delta");
            printf("    tool: %s %s\n", pResponse->pToolCalls[0].sName,
                pResponse->pToolCalls[0].sArgumentsJson ? pResponse->pToolCalls[0].sArgumentsJson : "");
        } else {
            printf("    %-58s SKIP (model declined the tool)\n",
                "tool_use block captured via input_json_delta");
        }
        xllmResponseDestroy(pResponse);
    } else {
        live_dump_error("anthropic-tool", &tError);
    }
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
}

static void live_responses(void)
{
    xllm_client* pClient = NULL;
    xllm_request tRequest;
    xllm_response* pResponse = NULL;
    xllm_stream_callbacks tCallbacks;
    live_events tEvents = {0};
    xllm_error tError;
    printf("responses dialect\n");
    if ( g_sResponsesUrl == NULL ) {
        printf("  %-58s SKIP (no XLLM_LIVE_RESPONSES_URL)\n",
            "client created on /v1/responses");
        return;
    }
    pClient = live_make_client_url(XLLM_PROVIDER_OPENAI_RESPONSES, g_sResponsesUrl);
    CHECK(pClient != NULL && strstr(pClient->sTarget, "/v1/responses") != NULL, "client created on /v1/responses");
    if ( !pClient ) { return; }
    memset(&tCallbacks, 0, sizeof(tCallbacks));
    tCallbacks.pUserData = &tEvents;
    tCallbacks.OnEvent = live_on_event;
    xllmRequestInit(&tRequest);
    xllmRequestSetModel(&tRequest, g_sModel);
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_SYSTEM, "Answer in one short sentence.");
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "Name the capital of Germany.");
    pResponse = NULL;
    CHECK(xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError) == XLLM_RESULT_OK,
        "typed-event stream succeeds");
    if ( pResponse ) {
        CHECK((pResponse->sContent && pResponse->sContent[0]) ||
            (pResponse->sReasoningContent && pResponse->sReasoningContent[0]),
            "responses text assembled");
        CHECK(pResponse->tUsage.uOutputTokens > 0u, "responses usage captured");
        printf("    content: %.100s\n", pResponse->sContent ? pResponse->sContent : "");
        xllmResponseDestroy(pResponse);
    } else {
        live_dump_error("responses", &tError);
    }

    xllmRequestInit(&tRequest);
    xllmRequestSetModel(&tRequest, g_sModel);
    xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER,
        "Check the weather in Oslo. You must call the get_weather tool.");
    xllmRequestAddTool(&tRequest, "get_weather", "Get current weather for a city",
        "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"}},\"required\":[\"city\"]}", false);
    pResponse = NULL;
    tEvents.iTool = 0;
    CHECK(xllmClientComplete(pClient, &tRequest, &tCallbacks, &pResponse, &tError) == XLLM_RESULT_OK,
        "responses function_call round trip succeeds");
    if ( pResponse ) {
        if ( pResponse->iToolCallCount >= 1u ) {
            CHECK(pResponse->pToolCalls[0].sName,
                "function_call captured via item deltas");
            printf("    tool: %s %s\n", pResponse->pToolCalls[0].sName,
                pResponse->pToolCalls[0].sArgumentsJson ? pResponse->pToolCalls[0].sArgumentsJson : "");
        } else {
            printf("    %-58s SKIP (model declined the tool)\n",
                "function_call captured via item deltas");
        }
        xllmResponseDestroy(pResponse);
    } else {
        live_dump_error("responses-tool", &tError);
    }
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
}

/* ------------------------------------------------------------------ */
/* Multimodal + API-key live section (qwen3.5 vision service).          */
/* ------------------------------------------------------------------ */

static unsigned char* live_read_file(const char* sPath, size_t* piSize)
{
    FILE* pFile = fopen(sPath, "rb");
    unsigned char* pData;
    long iSize;
    if ( !pFile ) { return NULL; }
    if ( fseek(pFile, 0, SEEK_END) != 0 || (iSize = ftell(pFile)) <= 0 ) {
        fclose(pFile);
        return NULL;
    }
    rewind(pFile);
    pData = (unsigned char*)malloc((size_t)iSize);
    if ( !pData || fread(pData, 1u, (size_t)iSize, pFile) != (size_t)iSize ) {
        free(pData);
        fclose(pFile);
        return NULL;
    }
    fclose(pFile);
    *piSize = (size_t)iSize;
    return pData;
}

static xllm_client* live_vision_client(xllm_provider eProvider, const char* sSuffix,
    const char* sKey)
{
    char sUrl[256];
    xllm_client_config tConfig;
    xllm_error tError;
    const char* sBase = g_sVisionUrl;
    if ( eProvider == XLLM_PROVIDER_ANTHROPIC && g_sAnthropicUrl ) { sBase = g_sAnthropicUrl; }
    (void)snprintf(sUrl, sizeof(sUrl), "%s%s", sBase, sSuffix);
    xllmClientConfigInit(&tConfig);
    tConfig.sBaseUrl = sUrl;
    tConfig.sApiKey = sKey;
    tConfig.sModel = g_sVisionModel;
    tConfig.eProvider = eProvider;
    tConfig.uMaxOutputTokens = 128u;
    tConfig.uTimeoutMs = 90u * 1000u;
    tConfig.uMaxAttempts = 1u;
    tConfig.bVerifyPeer = true;
    return xllmClientCreate(&tConfig, &tError);
}

static live_events tVisionEvents;

static void live_vision_case(xllm_provider eProvider, const char* sSuffixIn,
    const char* sLabel, const unsigned char* pImage, size_t iImageSize)
{
    const char* sSuffix = strstr(g_sVisionUrl, "/chat/completions") != NULL ||
        strstr(g_sVisionUrl, "/v4") != NULL ||
        strstr(g_sVisionUrl, "/anthropic") != NULL ? "" : sSuffixIn;
    xllm_client* pClient = live_vision_client(eProvider, sSuffix, g_sVisionKey);
    xllm_request tRequest;
    xllm_message tMessage;
    xllm_part tPart;
    xllm_response* pResponse = NULL;
    xllm_stream_callbacks tCallbacks;
    xllm_error tError;
    printf("%s\n", sLabel);
    if ( !pClient ) {
        printf("  %-58s FAIL (client)\n", "vision client created");
        return;
    }
    memset(&tCallbacks, 0, sizeof(tCallbacks));
    tCallbacks.pUserData = &tVisionEvents;
    tCallbacks.OnEvent = live_on_event;
    xllmRequestInit(&tRequest);
    xllmRequestSetModel(&tRequest, g_sVisionModel);
    xllmMessageInit(&tMessage, XLLM_ROLE_USER);
    xllmPartInit(&tPart, XLLM_PART_IMAGE);
    if ( !xllmPartSetImageData(&tPart, pImage, iImageSize, "image/png") ) {
        printf("  %-58s FAIL (image part)\n", "vision image part");
        xllmClientDestroy(pClient);
        return;
    }
    if ( g_bThinkingOff && eProvider == XLLM_PROVIDER_OPENAI_COMPAT &&
         !xllmRequestSetExtraBody(&tRequest,
             "{\"chat_template_kwargs\":{\"enable_thinking\":false}}") ) {
        printf("  %-58s FAIL (extra body)\n", "vision extra body");
    }
    if ( !xllmMessageAddPart(&tMessage, &tPart) ) {
        printf("  %-58s FAIL (request)\n", "vision request built");
        xllmMessageUnit(&tMessage);
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pClient);
        return;
    }
    {
        xllm_part tText;
        xllmPartInit(&tText, XLLM_PART_TEXT);
        if ( !xllmPartSetText(&tText, "Describe this image in one short sentence.") ||
             !xllmMessageAddPart(&tMessage, &tText) ) {
            printf("  %-58s FAIL (prompt part)\n", "vision request built");
            xllmMessageUnit(&tMessage);
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
            return;
        }
    }
    if ( !xllmRequestAddMessage(&tRequest, &tMessage) ) {
        printf("  %-58s FAIL (request)\n", "vision request built");
        xllm__part_unit(&tPart);
        xllmMessageUnit(&tMessage);
        xllmRequestUnit(&tRequest);
        xllmClientDestroy(pClient);
        return;
    }
    free(tPart.sText);
    free(tPart.sNativeType);
    free(tPart.sMediaType);
    free(tPart.sSourceUrl);
    free(tPart.sDetail);
    free(tPart.pData);
    {
        xllm_result eResult = xllmClientComplete(pClient, &tRequest, &tCallbacks,
            &pResponse, &tError);
        bool bText = pResponse && (
            (pResponse->sContent && pResponse->sContent[0]) ||
            (pResponse->sReasoningContent && pResponse->sReasoningContent[0]));
        CHECK(eResult == XLLM_RESULT_OK && bText, "vision round trip returns model text");
        if ( pResponse ) {
            printf("    content: %.110s\n",
                pResponse->sContent ? pResponse->sContent : "(reasoning only)");
            CHECK(pResponse->tUsage.uOutputTokens > 0u, "vision usage reported");
        } else {
            live_dump_error("vision", &tError);
        }
        xllmResponseDestroy(pResponse);
    }
    xllmMessageUnit(&tMessage);
    xllmRequestUnit(&tRequest);
    xllmClientDestroy(pClient);
}

static void live_vision(void)
{
    const char* sImage = getenv("XLLM_LIVE_IMAGE");
    unsigned char* pImage;
    size_t iImageSize = 0u;
    if ( !sImage || !sImage[0] ) { sImage = "../../res/logo.png"; }
    pImage = live_read_file(sImage, &iImageSize);
    printf("multimodal + api-key (qwen3.5)\n");
    if ( !pImage ) {
        printf("  %-58s SKIP (no image at %s)\n", "vision suite", sImage);
        return;
    }
    printf("  image: %s (%llu bytes)\n", sImage, (unsigned long long)iImageSize);
    memset(&tVisionEvents, 0, sizeof(tVisionEvents));
    if ( getenv("XLLM_LIVE_VISION_SKIP_COMPLETIONS") == NULL ) {
        live_vision_case(XLLM_PROVIDER_OPENAI_COMPAT, "/v1",
            "vision: completions dialect (Bearer key, image_url)", pImage, iImageSize);
    } else {
        printf("vision: completions dialect (Bearer key, image_url)\n");
        printf("  %-58s SKIP (endpoint rejects image content)\n",
            "vision round trip returns model text");
    }
    live_vision_case(XLLM_PROVIDER_ANTHROPIC, "",
        "vision: anthropic dialect (x-api-key, image block)", pImage, iImageSize);
    if ( g_sResponsesUrl != NULL ) {
        live_vision_case(XLLM_PROVIDER_OPENAI_RESPONSES, "/v1",
            "vision: responses dialect (input_image)", pImage, iImageSize);
    } else {
        printf("vision: responses dialect (input_image)\n");
        printf("  %-58s SKIP (no XLLM_LIVE_RESPONSES_URL)\n",
            "vision round trip returns model text");
    }
    {
        /* wrong key must map to the AUTH error class */
        xllm_client* pClient = live_vision_client(XLLM_PROVIDER_OPENAI_COMPAT, "/v1",
            "sk-wrong-key-on-purpose");
        xllm_request tRequest;
        xllm_response* pResponse = NULL;
        xllm_error tError;
        if ( pClient ) {
            xllmRequestInit(&tRequest);
            (void)xllmRequestAddTextMessage(&tRequest, XLLM_ROLE_USER, "hi");
            CHECK(xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) != XLLM_RESULT_OK &&
                pResponse == NULL && tError.eCode == XLLM_ERROR_AUTH,
                "wrong api key maps to the auth error class");
            xllmResponseDestroy(pResponse);
            xllmRequestUnit(&tRequest);
            xllmClientDestroy(pClient);
        }
    }
    free(pImage);
}

int main(void)
{
    g_sVisionUrl = getenv("XLLM_LIVE_VISION_URL");
    if ( !g_sVisionUrl || !g_sVisionUrl[0] ) { g_sVisionUrl = "http://127.0.0.1:8080"; }
    g_sVisionModel = getenv("XLLM_LIVE_VISION_MODEL");
    if ( !g_sVisionModel || !g_sVisionModel[0] ) { g_sVisionModel = "qwen3.5-0.8B"; }
    g_sVisionKey = getenv("XLLM_LIVE_VISION_KEY");
    if ( !g_sVisionKey || !g_sVisionKey[0] ) {
        g_sVisionKey = "sk-qwen35-t3sL8vXp2Nw9";
    }
    g_sAnthropicUrl = getenv("XLLM_LIVE_ANTHROPIC_URL");
    if ( g_sAnthropicUrl && !g_sAnthropicUrl[0] ) { g_sAnthropicUrl = NULL; }
    g_sResponsesUrl = getenv("XLLM_LIVE_RESPONSES_URL");
    if ( g_sResponsesUrl && !g_sResponsesUrl[0] ) { g_sResponsesUrl = NULL; }
    g_bThinkingOff = getenv("XLLM_LIVE_THINKING_OFF") != NULL;
    g_sApiKey = getenv("XLLM_LIVE_KEY");
    if ( !g_sApiKey || !g_sApiKey[0] ) { g_sApiKey = "local-smoke"; }
    g_sBaseUrl = getenv("XLLM_LIVE_URL");
    if ( !g_sBaseUrl || !g_sBaseUrl[0] ) { g_sBaseUrl = "http://127.0.0.1:8080"; }
    g_sModel = getenv("XLLM_LIVE_MODEL");
    if ( !g_sModel || !g_sModel[0] ) { g_sModel = "ling-3.0-tiny"; }
    printf("xllm live smoke against %s (model %s)\n", g_sBaseUrl, g_sModel);
    live_completions();
    live_anthropic();
    live_responses();
    live_vision();
    printf("xllm live smoke: %s (%d failures)\n", g_iFailures ? "FAIL" : "PASS", g_iFailures);
    return g_iFailures ? 1 : 0;
}
