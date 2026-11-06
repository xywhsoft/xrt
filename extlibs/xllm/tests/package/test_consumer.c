#define XLLM_MODULE_XLLM
#include <xllm.h>
#include <string.h>

int main(void)
{
    char reasoning[] = "signed thought";
    char signature[] = "opaque-signature";
    char answer[] = "package answer";
    xllm_block blocks[2] = {{0}};
    xllm_response response = {0};
    xllm_message message = {0};
    xllm_request request;
    xllm_client_config config;
    xllm_client* client = NULL;
    xllm_error error;
    char* json = NULL;
    xvalue* parsed = NULL;
    int result = 1;
    xllmRequestInit(&request);
    blocks[0].eKind = XLLM_BLOCK_REASONING;
    blocks[0].sText = reasoning;
    blocks[0].sNative = signature;
    blocks[1].eKind = XLLM_BLOCK_TEXT;
    blocks[1].sText = answer;
    response.pBlocks = blocks;
    response.iBlockCount = 2u;
    response.sContent = answer;
    response.sReasoningContent = reasoning;
    if (!xrtVersion() || !xllmMessageFromResponse(&response, &message) ||
        message.iPartCount != 3u || !xllmRequestAddMessage(&request, &message)) goto done;
    xllmMessageUnit(&message);
    memset(reasoning, 'x', sizeof(reasoning) - 1u);
    memset(signature, 'x', sizeof(signature) - 1u);
    memset(answer, 'x', sizeof(answer) - 1u);
    xllmClientConfigInit(&config);
    config.eProvider = XLLM_PROVIDER_ANTHROPIC;
    config.sBaseUrl = "http://127.0.0.1:1";
    config.sModel = "package-test";
    client = xllmClientCreate(&config, &error);
    if (!client) goto done;
    json = xllmClientBuildRequestJson(client, &request, &error);
    if (!json || !strstr(json, "signed thought") || !strstr(json, "opaque-signature") ||
        !strstr(json, "package answer")) goto done;
    parsed = xrtJsonParse((xstrview){json, strlen(json)});
    if (!parsed || !xrtValueIs(parsed, XVALUE_OBJECT)) goto done;
    result = 0;
done:
    xrtValueRelease(parsed);
    xllmFree(json);
    xllmClientDestroy(client);
    xllmMessageUnit(&message);
    xllmRequestUnit(&request);
    return result;
}
