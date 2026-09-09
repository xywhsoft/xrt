#ifndef DEMO_AZURE_OPENAI_CONFIG_H
#define DEMO_AZURE_OPENAI_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Fill these values before running the examples.
 *
 * Notes:
 * - AZURE_OPENAI_ENDPOINT is the Azure endpoint you already have.
 * - AZURE_OPENAI_DEPLOYMENT is the deployment name that Azure actually serves.
 * - AZURE_OPENAI_MODEL_NAME is kept for reference only. For Azure legacy
 *   chat-completions endpoints, xllm should send the deployment name as model.
 * - AZURE_OPENAI_API_VERSION is fixed here for this example.
 * - Azure OpenAI API key is read from environment variable:
 *   AZURE_OPENAI_API_KEY
 */

#define AZURE_OPENAI_ENDPOINT        "https://eastus2.api.cognitive.microsoft.com/"
#define AZURE_OPENAI_MODEL_NAME      "gpt-5.4"
#define AZURE_OPENAI_DEPLOYMENT      "Infrastructure-gpt54-3-us2-2"
#define AZURE_OPENAI_API_VERSION     "2024-12-01-preview"

#define AZURE_OPENAI_SYSTEM_PROMPT   "You are a helpful assistant."

static const char *demo_get_azure_openai_api_key(void)
{
    return getenv("AZURE_OPENAI_API_KEY");
}

static int demo_build_azure_openai_chat_url(char *sBuf, size_t iBufSize)
{
    const char *sEndpoint = AZURE_OPENAI_ENDPOINT;
    size_t iLen;
    int iWritten;

    if ( sBuf == NULL || iBufSize == 0u ) {
        return -1;
    }

    iLen = strlen(sEndpoint);
    iWritten = snprintf(
        sBuf,
        iBufSize,
        "%s%sopenai/deployments/%s/chat/completions?api-version=%s",
        sEndpoint,
        (iLen > 0u && sEndpoint[iLen - 1u] == '/') ? "" : "/",
        AZURE_OPENAI_DEPLOYMENT,
        AZURE_OPENAI_API_VERSION
    );

    if ( iWritten < 0 || (size_t)iWritten >= iBufSize ) {
        return -1;
    }

    return 0;
}

#endif
