#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#define demo_stricmp _stricmp
#else
#include <strings.h>
#define demo_stricmp strcasecmp
#endif

typedef struct {
    int iTextDeltaCount;
    int iThinkingDeltaCount;
    int iToolReadyCount;
    int iArtifactBeginCount;
    int iArtifactChunkCount;
    int iArtifactReadyCount;
    int iEndCount;
} demo_event_state;

static uint64 demo_now_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return (uint64)GetTickCount64();
#else
    struct timespec tNow;

    if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) {
        return 0u;
    }

    return ((uint64)tNow.tv_sec * 1000u) + ((uint64)tNow.tv_nsec / 1000000u);
#endif
}

static char *demo_dupstr(const char *sText)
{
    size_t iLen;
    char *sCopy;

    if ( sText == NULL ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( sCopy == NULL ) {
        return NULL;
    }

    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

static const char *demo_env_required(const char *sName)
{
    const char *sValue = getenv(sName);
    if ( sValue == NULL || *sValue == '\0' ) {
        fprintf(stderr, "missing required environment variable: %s\n", sName);
        return NULL;
    }
    return sValue;
}

static const char *demo_env_optional(const char *sName, const char *sDefaultValue)
{
    const char *sValue = getenv(sName);
    if ( sValue == NULL || *sValue == '\0' ) {
        return sDefaultValue;
    }
    return sValue;
}

static const char *demo_env_optional2(const char *sNameA, const char *sNameB, const char *sDefaultValue)
{
    const char *sValue = demo_env_optional(sNameA, NULL);

    if ( sValue != NULL && *sValue != '\0' ) {
        return sValue;
    }
    return demo_env_optional(sNameB, sDefaultValue);
}

static const char *demo_adapter_base_url(const char *sAdapter)
{
    const char *sValue = demo_env_optional("XLLM_REAL_BASE_URL", NULL);

    if ( sValue != NULL && *sValue != '\0' ) {
        return sValue;
    }
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_BASE_URL", "OPENAI_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional("OLLAMA_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_BASE_URL", "DASHSCOPE_BASE_URL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_BASE_URL", "ARK_BASE_URL", NULL);
    }
    return NULL;
}

static const char *demo_adapter_model(const char *sAdapter)
{
    const char *sValue = demo_env_optional("XLLM_REAL_MODEL", NULL);

    if ( sValue != NULL && *sValue != '\0' ) {
        return sValue;
    }
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_DEPLOYMENT", "OPENAI_MODEL", demo_env_optional("AZURE_OPENAI_MODEL", NULL));
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional("OLLAMA_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_MODEL", "DASHSCOPE_MODEL", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_MODEL", "ARK_MODEL", NULL);
    }
    return NULL;
}

static const char *demo_adapter_api_key(const char *sAdapter)
{
    const char *sValue = demo_env_optional("XLLM_REAL_API_KEY", NULL);

    if ( sValue != NULL && *sValue != '\0' ) {
        return sValue;
    }
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_API_KEY", "OPENAI_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_API_KEY", "DASHSCOPE_API_KEY", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_API_KEY", "ARK_API_KEY", NULL);
    }
    return NULL;
}

static bool demo_env_bool(const char *sName, bool bDefaultValue)
{
    const char *sValue = getenv(sName);
    if ( sValue == NULL || *sValue == '\0' ) {
        return bDefaultValue;
    }
    if ( strcmp(sValue, "1") == 0 || demo_stricmp(sValue, "true") == 0 || demo_stricmp(sValue, "yes") == 0 ) {
        return true;
    }
    if ( strcmp(sValue, "0") == 0 || demo_stricmp(sValue, "false") == 0 || demo_stricmp(sValue, "no") == 0 ) {
        return false;
    }
    return bDefaultValue;
}

static bool demo_env_has_value(const char *sName);

static bool demo_should_capture_events(xllm_stream_mode eStreamMode)
{
    if ( eStreamMode != XLLM_STREAM_OFF ) {
        return true;
    }

    return demo_env_has_value("XLLM_REAL_EXPECT_ARTIFACT_BEGIN_MIN") ||
           demo_env_has_value("XLLM_REAL_EXPECT_ARTIFACT_CHUNK_MIN") ||
           demo_env_has_value("XLLM_REAL_EXPECT_ARTIFACT_READY_MIN");
}

static uint32 demo_env_u32(const char *sName, uint32 uDefaultValue)
{
    const char *sValue = getenv(sName);
    char *pEnd = NULL;
    unsigned long uValue;

    if ( sValue == NULL || *sValue == '\0' ) {
        return uDefaultValue;
    }

    uValue = strtoul(sValue, &pEnd, 10);
    if ( pEnd == sValue || *pEnd != '\0' ) {
        return uDefaultValue;
    }
    return (uint32)uValue;
}

static xllm_proxy_kind demo_proxy_kind_from_env(bool *pbConfigured)
{
    const char *sValue = getenv("XLLM_REAL_PROXY_KIND");

    if ( pbConfigured ) {
        *pbConfigured = false;
    }
    if ( sValue == NULL || *sValue == '\0' ) {
        return XLLM_PROXY_UNSPECIFIED;
    }
    if ( pbConfigured ) {
        *pbConfigured = true;
    }
    if ( demo_stricmp(sValue, "none") == 0 ) {
        return XLLM_PROXY_NONE;
    }
    if ( demo_stricmp(sValue, "socks5") == 0 ) {
        return XLLM_PROXY_SOCKS5;
    }
    if ( demo_stricmp(sValue, "http_connect") == 0 ||
         demo_stricmp(sValue, "http-connect") == 0 ||
         demo_stricmp(sValue, "http") == 0 ||
         demo_stricmp(sValue, "connect") == 0 ) {
        return XLLM_PROXY_HTTP_CONNECT;
    }
    return XLLM_PROXY_UNSPECIFIED;
}

static bool demo_is_space(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

static char *demo_dup_trimmed_range(const char *sText, size_t iLen)
{
    size_t iStart = 0u;
    size_t iEnd = iLen;

    if ( sText == NULL ) {
        return NULL;
    }

    while ( iStart < iLen && demo_is_space(sText[iStart]) ) {
        ++iStart;
    }
    while ( iEnd > iStart && demo_is_space(sText[iEnd - 1u]) ) {
        --iEnd;
    }

    iLen = iEnd - iStart;
    sText += iStart;

    {
        char *sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
        if ( sCopy == NULL ) {
            return NULL;
        }
        if ( iLen > 0u ) {
            memcpy(sCopy, sText, iLen);
        }
        sCopy[iLen] = '\0';
        return sCopy;
    }
}

static void demo_free_owned_string_list(const char ***ppsValues, size_t *piCount)
{
    size_t i;

    if ( ppsValues == NULL || *ppsValues == NULL ) {
        return;
    }

    if ( piCount != NULL ) {
        for ( i = 0u; i < *piCount; ++i ) {
            if ( (*ppsValues)[i] != NULL ) {
                xrtFree((void *)(*ppsValues)[i]);
            }
        }
        *piCount = 0u;
    }
    xrtFree((void *)*ppsValues);
    *ppsValues = NULL;
}

static int demo_parse_string_list(const char *sInput, char chDelimiter, const char ***ppsValues, size_t *piCount)
{
    size_t iCount = 0u;
    const char *sCursor;
    const char **psValues = NULL;
    size_t iWrite = 0u;

    if ( ppsValues == NULL || piCount == NULL ) {
        return XRT_NET_ERROR;
    }

    *ppsValues = NULL;
    *piCount = 0u;
    if ( sInput == NULL || *sInput == '\0' ) {
        return XRT_NET_OK;
    }

    sCursor = sInput;
    while ( *sCursor != '\0' ) {
        const char *sNext = strchr(sCursor, chDelimiter);
        size_t iLen = (size_t)((sNext != NULL) ? (size_t)(sNext - sCursor) : strlen(sCursor));
        char *sValue = demo_dup_trimmed_range(sCursor, iLen);
        if ( sValue == NULL ) {
            return XRT_NET_ERROR;
        }
        if ( sValue[0] != '\0' ) {
            ++iCount;
        }
        xrtFree(sValue);
        if ( sNext == NULL ) {
            break;
        }
        sCursor = sNext + 1;
    }

    if ( iCount == 0u ) {
        return XRT_NET_OK;
    }

    psValues = (const char **)xrtCalloc(iCount, sizeof(const char *));
    if ( psValues == NULL ) {
        return XRT_NET_ERROR;
    }

    sCursor = sInput;
    while ( *sCursor != '\0' ) {
        const char *sNext = strchr(sCursor, chDelimiter);
        size_t iLen = (size_t)((sNext != NULL) ? (size_t)(sNext - sCursor) : strlen(sCursor));
        char *sValue = demo_dup_trimmed_range(sCursor, iLen);
        if ( sValue == NULL ) {
            demo_free_owned_string_list(&psValues, &iWrite);
            return XRT_NET_ERROR;
        }
        if ( sValue[0] != '\0' ) {
            psValues[iWrite++] = sValue;
        } else {
            xrtFree(sValue);
        }
        if ( sNext == NULL ) {
            break;
        }
        sCursor = sNext + 1;
    }

    *ppsValues = psValues;
    *piCount = iWrite;
    return XRT_NET_OK;
}

static void demo_free_owned_header_array(xllm_header **ppHeaders, size_t *piCount)
{
    size_t i;

    if ( ppHeaders == NULL || *ppHeaders == NULL ) {
        return;
    }

    if ( piCount != NULL ) {
        for ( i = 0u; i < *piCount; ++i ) {
            xrtFree((void *)(*ppHeaders)[i].sName);
            xrtFree((void *)(*ppHeaders)[i].sValue);
        }
        *piCount = 0u;
    }
    xrtFree(*ppHeaders);
    *ppHeaders = NULL;
}

static int demo_parse_header_list(const char *sInput, xllm_header **ppHeaders, size_t *piCount)
{
    size_t iCount = 0u;
    const char *sCursor;
    xllm_header *pHeaders = NULL;
    size_t iWrite = 0u;

    if ( ppHeaders == NULL || piCount == NULL ) {
        return XRT_NET_ERROR;
    }

    *ppHeaders = NULL;
    *piCount = 0u;
    if ( sInput == NULL || *sInput == '\0' ) {
        return XRT_NET_OK;
    }

    sCursor = sInput;
    while ( *sCursor != '\0' ) {
        const char *sNext = strchr(sCursor, ';');
        size_t iLen = (size_t)((sNext != NULL) ? (size_t)(sNext - sCursor) : strlen(sCursor));
        char *sEntry = demo_dup_trimmed_range(sCursor, iLen);
        if ( sEntry == NULL ) {
            return XRT_NET_ERROR;
        }
        if ( sEntry[0] != '\0' ) {
            char *sEquals = strchr(sEntry, '=');
            if ( sEquals == NULL ) {
                fprintf(stderr, "invalid XLLM_REAL_DEFAULT_HEADERS entry (missing '='): %s\n", sEntry);
                xrtFree(sEntry);
                return XRT_NET_ERROR;
            }
            ++iCount;
        }
        xrtFree(sEntry);
        if ( sNext == NULL ) {
            break;
        }
        sCursor = sNext + 1;
    }

    if ( iCount == 0u ) {
        return XRT_NET_OK;
    }

    pHeaders = (xllm_header *)xrtCalloc(iCount, sizeof(xllm_header));
    if ( pHeaders == NULL ) {
        return XRT_NET_ERROR;
    }

    sCursor = sInput;
    while ( *sCursor != '\0' ) {
        const char *sNext = strchr(sCursor, ';');
        size_t iLen = (size_t)((sNext != NULL) ? (size_t)(sNext - sCursor) : strlen(sCursor));
        char *sEntry = demo_dup_trimmed_range(sCursor, iLen);
        if ( sEntry == NULL ) {
            demo_free_owned_header_array(&pHeaders, &iWrite);
            return XRT_NET_ERROR;
        }
        if ( sEntry[0] != '\0' ) {
            char *sEquals = strchr(sEntry, '=');
            if ( sEquals == NULL ) {
                fprintf(stderr, "invalid XLLM_REAL_DEFAULT_HEADERS entry (missing '='): %s\n", sEntry);
                xrtFree(sEntry);
                demo_free_owned_header_array(&pHeaders, &iWrite);
                return XRT_NET_ERROR;
            }
            *sEquals = '\0';
            pHeaders[iWrite].sName = demo_dup_trimmed_range(sEntry, strlen(sEntry));
            pHeaders[iWrite].sValue = demo_dup_trimmed_range(sEquals + 1, strlen(sEquals + 1));
            xrtFree(sEntry);
            if ( pHeaders[iWrite].sName == NULL || pHeaders[iWrite].sValue == NULL ) {
                demo_free_owned_header_array(&pHeaders, &iWrite);
                return XRT_NET_ERROR;
            }
            if ( pHeaders[iWrite].sName[0] == '\0' ) {
                fprintf(stderr, "invalid XLLM_REAL_DEFAULT_HEADERS entry (empty header name)\n");
                demo_free_owned_header_array(&pHeaders, &iWrite);
                return XRT_NET_ERROR;
            }
            ++iWrite;
        } else {
            xrtFree(sEntry);
        }
        if ( sNext == NULL ) {
            break;
        }
        sCursor = sNext + 1;
    }

    *ppHeaders = pHeaders;
    *piCount = iWrite;
    return XRT_NET_OK;
}

static const char *demo_adapter_timeout_env(const char *sAdapter)
{
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return "AZURE_OPENAI_MULTIMODAL_TIMEOUT_MS";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return "GLM_TIMEOUT_MS";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return "KIMI_TIMEOUT_MS";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return "QWEN_TIMEOUT_MS";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return "DOUBAO_TIMEOUT_MS";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return "GEMINI_TIMEOUT_MS";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return "VERTEX_GEMINI_TIMEOUT_MS";
    }
    return NULL;
}

static const char *demo_adapter_timeout_env2(const char *sAdapter)
{
    if ( sAdapter != NULL && strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return "ARK_TIMEOUT_MS";
    }
    return NULL;
}

static const char *demo_adapter_stream_env(const char *sAdapter)
{
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return "AZURE_OPENAI_MULTIMODAL_STREAM";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return "QWEN_MULTIMODAL_STREAM";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return "DOUBAO_MULTIMODAL_STREAM";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return "GEMINI_MULTIMODAL_STREAM";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return "VERTEX_GEMINI_MULTIMODAL_STREAM";
    }
    return NULL;
}

static const char *demo_adapter_stream_env2(const char *sAdapter)
{
    if ( sAdapter != NULL && strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return "ARK_MULTIMODAL_STREAM";
    }
    return NULL;
}

static xllm_stream_mode demo_stream_mode_from_env(const char *sAdapter)
{
    const char *sValue = getenv("XLLM_REAL_STREAM");
    if ( sValue == NULL || *sValue == '\0' ) {
        const char *sAlias = demo_adapter_stream_env(sAdapter);
        const char *sAlias2 = demo_adapter_stream_env2(sAdapter);

        if ( (sAlias && demo_env_bool(sAlias, false)) ||
             (sAlias2 && demo_env_bool(sAlias2, false)) ) {
            return XLLM_STREAM_PREFER;
        }
        return XLLM_STREAM_OFF;
    }
    if ( demo_stricmp(sValue, "require") == 0 ) {
        return XLLM_STREAM_REQUIRE;
    }
    if ( demo_stricmp(sValue, "prefer") == 0 ) {
        return XLLM_STREAM_PREFER;
    }
    if ( demo_stricmp(sValue, "auto") == 0 ) {
        return XLLM_STREAM_AUTO;
    }
    return demo_env_bool("XLLM_REAL_STREAM", false) ? XLLM_STREAM_PREFER : XLLM_STREAM_OFF;
}

static bool demo_env_has_value(const char *sName)
{
    const char *sValue = getenv(sName);
    return (sValue != NULL && *sValue != '\0');
}

static xllm_reasoning_level demo_reasoning_level_from_env(bool *pbConfigured)
{
    const char *sValue = getenv("XLLM_REAL_REASONING");

    if ( pbConfigured ) {
        *pbConfigured = false;
    }
    if ( sValue == NULL || *sValue == '\0' ) {
        return XLLM_REASONING_DEFAULT;
    }
    if ( pbConfigured ) {
        *pbConfigured = true;
    }
    if ( demo_stricmp(sValue, "off") == 0 ) {
        return XLLM_REASONING_OFF;
    }
    if ( demo_stricmp(sValue, "low") == 0 ) {
        return XLLM_REASONING_LOW;
    }
    if ( demo_stricmp(sValue, "medium") == 0 ) {
        return XLLM_REASONING_MEDIUM;
    }
    if ( demo_stricmp(sValue, "high") == 0 ) {
        return XLLM_REASONING_HIGH;
    }
    return XLLM_REASONING_DEFAULT;
}

static bool demo_response_format_is_json(const char *sFormat)
{
    return (
        sFormat != NULL &&
        (demo_stricmp(sFormat, "json") == 0 || demo_stricmp(sFormat, "json_schema") == 0)
    );
}

static bool demo_tool_kind_is_provider(void)
{
    const char *sKind = demo_env_optional("XLLM_REAL_TOOL_KIND", "client");
    return (sKind != NULL && demo_stricmp(sKind, "provider") == 0);
}

static const char *demo_adapter_tool_choice(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_CHOICE") ) {
        return getenv("XLLM_REAL_TOOL_CHOICE");
    }
    if ( sAdapter == NULL ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_TOOL_CHOICE", "OPENAI_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional("OLLAMA_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_TOOL_CHOICE", "DASHSCOPE_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_TOOL_CHOICE", "ARK_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_TOOL_CHOICE", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_TOOL_CHOICE", NULL);
    }
    return NULL;
}

static const char *demo_adapter_tool_choice_name(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_CHOICE_NAME") ) {
        return getenv("XLLM_REAL_TOOL_CHOICE_NAME");
    }
    if ( demo_env_has_value("XLLM_REAL_TOOL_WIRE_NAME") ) {
        return getenv("XLLM_REAL_TOOL_WIRE_NAME");
    }
    if ( sAdapter == NULL ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2(
            "AZURE_OPENAI_TOOL_CHOICE_NAME",
            "OPENAI_TOOL_CHOICE_NAME",
            demo_env_optional2("AZURE_OPENAI_TOOL_WIRE_NAME", "OPENAI_TOOL_WIRE_NAME", NULL)
        );
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional2("OLLAMA_TOOL_CHOICE_NAME", "OLLAMA_TOOL_WIRE_NAME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional2("ANTHROPIC_TOOL_CHOICE_NAME", "ANTHROPIC_TOOL_WIRE_NAME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional2("GLM_TOOL_CHOICE_NAME", "GLM_TOOL_WIRE_NAME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional2("MINIMAX_TOOL_CHOICE_NAME", "MINIMAX_TOOL_WIRE_NAME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional2("KIMI_TOOL_CHOICE_NAME", "KIMI_TOOL_WIRE_NAME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_TOOL_CHOICE_NAME", "QWEN_TOOL_WIRE_NAME", demo_env_optional2("DASHSCOPE_TOOL_CHOICE_NAME", "DASHSCOPE_TOOL_WIRE_NAME", NULL));
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_TOOL_CHOICE_NAME", "DOUBAO_TOOL_WIRE_NAME", demo_env_optional2("ARK_TOOL_CHOICE_NAME", "ARK_TOOL_WIRE_NAME", NULL));
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional2("GEMINI_TOOL_CHOICE_NAME", "GEMINI_TOOL_WIRE_NAME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional2("VERTEX_GEMINI_TOOL_CHOICE_NAME", "VERTEX_GEMINI_TOOL_WIRE_NAME", NULL);
    }
    return NULL;
}

static const char *demo_adapter_tool_result_text(const char *sAdapter);
static const char *demo_adapter_tool_result_image_url(const char *sAdapter);
static const char *demo_adapter_tool_result_image_file_id(const char *sAdapter);
static const char *demo_adapter_tool_result_file_url(const char *sAdapter);
static const char *demo_adapter_tool_result_file_file_id(const char *sAdapter);

static bool demo_adapter_tool_enabled(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_ENABLE_TOOL") ) {
        return demo_env_bool("XLLM_REAL_ENABLE_TOOL", false);
    }
    if ( sAdapter != NULL ) {
        if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
            if ( demo_env_has_value("AZURE_OPENAI_ENABLE_TOOL") ||
                 demo_env_has_value("OPENAI_ENABLE_TOOL") ) {
                return demo_env_bool("AZURE_OPENAI_ENABLE_TOOL", demo_env_bool("OPENAI_ENABLE_TOOL", false));
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
            if ( demo_env_has_value("OLLAMA_ENABLE_TOOL") ) {
                return demo_env_bool("OLLAMA_ENABLE_TOOL", false);
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
            if ( demo_env_has_value("ANTHROPIC_ENABLE_TOOL") ) {
                return demo_env_bool("ANTHROPIC_ENABLE_TOOL", false);
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
            if ( demo_env_has_value("GLM_ENABLE_TOOL") ) {
                return demo_env_bool("GLM_ENABLE_TOOL", false);
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
            if ( demo_env_has_value("MINIMAX_ENABLE_TOOL") ) {
                return demo_env_bool("MINIMAX_ENABLE_TOOL", false);
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
            if ( demo_env_has_value("KIMI_ENABLE_TOOL") ) {
                return demo_env_bool("KIMI_ENABLE_TOOL", false);
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
            if ( demo_env_has_value("QWEN_ENABLE_TOOL") ||
                 demo_env_has_value("DASHSCOPE_ENABLE_TOOL") ) {
                return demo_env_bool("QWEN_ENABLE_TOOL", demo_env_bool("DASHSCOPE_ENABLE_TOOL", false));
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
            if ( demo_env_has_value("DOUBAO_ENABLE_TOOL") ||
                 demo_env_has_value("ARK_ENABLE_TOOL") ) {
                return demo_env_bool("DOUBAO_ENABLE_TOOL", demo_env_bool("ARK_ENABLE_TOOL", false));
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
            if ( demo_env_has_value("GEMINI_ENABLE_TOOL") ) {
                return demo_env_bool("GEMINI_ENABLE_TOOL", false);
            }
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
            if ( demo_env_has_value("VERTEX_GEMINI_ENABLE_TOOL") ) {
                return demo_env_bool("VERTEX_GEMINI_ENABLE_TOOL", false);
            }
        }
    }
    if ( demo_env_has_value("XLLM_REAL_TOOL_KIND") ||
         demo_env_has_value("XLLM_REAL_TOOL_CHOICE") ||
         demo_env_has_value("XLLM_REAL_TOOL_REQUIRED") ||
         demo_env_has_value("XLLM_REAL_TOOL_ID") ||
         demo_env_has_value("XLLM_REAL_TOOL_WIRE_NAME") ||
         demo_env_has_value("XLLM_REAL_TOOL_SCHEMA") ||
         demo_env_has_value("XLLM_REAL_PROVIDER_TOOL_JSON") ||
         demo_env_has_value("XLLM_REAL_TOOL_RESULT_TEXT") ||
         demo_env_has_value("XLLM_REAL_TOOL_RESULT_IMAGE_URL") ||
         demo_env_has_value("XLLM_REAL_TOOL_RESULT_IMAGE_FILE_ID") ||
         demo_env_has_value("XLLM_REAL_TOOL_RESULT_FILE_URL") ||
         demo_env_has_value("XLLM_REAL_TOOL_RESULT_FILE_FILE_ID") ) {
        return true;
    }
    return (
        demo_adapter_tool_choice(sAdapter) != NULL ||
        demo_adapter_tool_choice_name(sAdapter) != NULL ||
        demo_adapter_tool_result_text(sAdapter) != NULL ||
        demo_adapter_tool_result_image_url(sAdapter) != NULL ||
        demo_adapter_tool_result_image_file_id(sAdapter) != NULL ||
        demo_adapter_tool_result_file_url(sAdapter) != NULL ||
        demo_adapter_tool_result_file_file_id(sAdapter) != NULL
    );
}

static int demo_apply_tool_choice_from_env(const char *sAdapter, xllm_turn *pTurn)
{
    const char *sToolChoice = demo_adapter_tool_choice(sAdapter);
    const char *sToolChoiceName = demo_adapter_tool_choice_name(sAdapter);
    bool bAllowParallel = demo_env_bool("XLLM_REAL_ALLOW_PARALLEL_TOOL_CALLS", false);
    xllm_tool_choice_mode eMode = XLLM_TOOL_CHOICE_AUTO;

    if ( pTurn == NULL ) {
        return XRT_NET_ERROR;
    }

    if ( sToolChoice == NULL || *sToolChoice == '\0' ) {
        if ( demo_env_bool("XLLM_REAL_TOOL_REQUIRED", false) ) {
            eMode = XLLM_TOOL_CHOICE_REQUIRED;
        } else {
            return XRT_NET_OK;
        }
    } else if ( demo_stricmp(sToolChoice, "auto") == 0 ) {
        eMode = XLLM_TOOL_CHOICE_AUTO;
    } else if ( demo_stricmp(sToolChoice, "none") == 0 ) {
        eMode = XLLM_TOOL_CHOICE_NONE;
    } else if ( demo_stricmp(sToolChoice, "required") == 0 ) {
        eMode = XLLM_TOOL_CHOICE_REQUIRED;
    } else if ( demo_stricmp(sToolChoice, "named") == 0 ) {
        eMode = XLLM_TOOL_CHOICE_NAMED;
    } else {
        fprintf(stderr, "unsupported XLLM_REAL_TOOL_CHOICE: %s\n", sToolChoice);
        return XRT_NET_ERROR;
    }

    return xllm_turn_set_tool_choice(pTurn, eMode, sToolChoiceName, bAllowParallel);
}

static const char *demo_default_provider_tool_json(const char *sAdapter)
{
    if ( sAdapter != NULL && strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return "{\"type\":\"web_search_preview\"}";
    }
    if ( sAdapter != NULL && strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return "{\"type\":\"web_search_20250305\",\"name\":\"web_search\",\"max_uses\":2}";
    }
    if ( sAdapter != NULL && strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return "{\"type\":\"web_search\",\"name\":\"web_search\",\"max_results\":3}";
    }
    return "{\"type\":\"provider_tool\",\"name\":\"provider_tool\"}";
}

static const char *demo_status_name(xllm_response_status eStatus)
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

static const char *demo_error_code_name(xllm_error_code eCode)
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

static void demo_summary_set_text(xvalue tSummary, const char *sKey, const char *sValue)
{
    if ( tSummary == NULL || sKey == NULL || sValue == NULL ) {
        return;
    }
    (void)xvoTableSetText(tSummary, (str)sKey, 0u, (str)sValue, 0u, FALSE);
}

static void demo_summary_set_bool(xvalue tSummary, const char *sKey, bool bValue)
{
    if ( tSummary == NULL || sKey == NULL ) {
        return;
    }
    (void)xvoTableSetBool(tSummary, (str)sKey, 0u, bValue);
}

static void demo_summary_set_int(xvalue tSummary, const char *sKey, int64 iValue)
{
    if ( tSummary == NULL || sKey == NULL ) {
        return;
    }
    (void)xvoTableSetInt(tSummary, (str)sKey, 0u, iValue);
}

static void demo_count_output_parts(
    const xllm_response *pResponse,
    size_t *piTextCount,
    size_t *piImageCount,
    size_t *piFileCount,
    size_t *piAudioCount,
    size_t *piVideoCount,
    size_t *piJsonCount
)
{
    size_t i;

    if ( piTextCount ) *piTextCount = 0u;
    if ( piImageCount ) *piImageCount = 0u;
    if ( piFileCount ) *piFileCount = 0u;
    if ( piAudioCount ) *piAudioCount = 0u;
    if ( piVideoCount ) *piVideoCount = 0u;
    if ( piJsonCount ) *piJsonCount = 0u;

    if ( pResponse == NULL ) {
        return;
    }

    for ( i = 0u; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];
        size_t j;

        if ( pOutput->eKind != XLLM_OUTPUT_MESSAGE ) {
            continue;
        }

        for ( j = 0u; j < pOutput->as.tMessage.iPartCount; ++j ) {
            const xllm_content_part *pPart = &pOutput->as.tMessage.pParts[j];

            switch ( pPart->eKind ) {
                case XLLM_PART_TEXT:
                    if ( piTextCount ) ++(*piTextCount);
                    break;
                case XLLM_PART_IMAGE:
                    if ( piImageCount ) ++(*piImageCount);
                    break;
                case XLLM_PART_FILE:
                    if ( piFileCount ) ++(*piFileCount);
                    break;
                case XLLM_PART_AUDIO:
                    if ( piAudioCount ) ++(*piAudioCount);
                    break;
                case XLLM_PART_VIDEO:
                    if ( piVideoCount ) ++(*piVideoCount);
                    break;
                case XLLM_PART_JSON:
                    if ( piJsonCount ) ++(*piJsonCount);
                    break;
                default:
                    break;
            }
        }
    }
}

static void demo_write_summary_file(const char *sPath, const char *sJson)
{
    FILE *pFile;
    size_t iSize;

    if ( sPath == NULL || *sPath == '\0' || sJson == NULL ) {
        return;
    }

    pFile = fopen(sPath, "wb");
    if ( pFile == NULL ) {
        fprintf(stderr, "failed to open summary file: %s\n", sPath);
        return;
    }

    iSize = strlen(sJson);
    if ( iSize > 0u && fwrite(sJson, 1u, iSize, pFile) != iSize ) {
        fprintf(stderr, "failed to write summary file: %s\n", sPath);
    }
    fclose(pFile);
}

static void demo_emit_json_summary(
    const char *sAdapter,
    const char *sProvider,
    const char *sBaseUrl,
    const char *sRequestedModel,
    xllm_stream_mode eStreamMode,
    bool bUseMultimodal,
    const char *sResponseFormat,
    bool bEnableTool,
    bool bBestEffortStructuredOutput,
    bool bSuccess,
    int iRc,
    const xllm_response *pResponse,
    const xllm_error *pError,
    const demo_event_state *pEvents,
    const char *sJsonOutput,
    uint64 uDurationMs
)
{
    xvalue tSummary = xvoCreateTable();
    const char *sSummaryPath = demo_env_optional("XLLM_REAL_SUMMARY_PATH", NULL);
    const char *sCaseName = demo_env_optional("XLLM_REAL_CASE_NAME", NULL);
    char *sSummary = NULL;
    size_t iTextCount = 0u;
    size_t iImageCount = 0u;
    size_t iFileCount = 0u;
    size_t iAudioCount = 0u;
    size_t iVideoCount = 0u;
    size_t iJsonCount = 0u;

    if ( tSummary == NULL ) {
        return;
    }

    demo_summary_set_bool(tSummary, "success", bSuccess);
    demo_summary_set_int(tSummary, "rc", iRc);
    demo_summary_set_text(tSummary, "adapter", sAdapter);
    demo_summary_set_text(tSummary, "provider", sProvider);
    demo_summary_set_text(tSummary, "base_url", sBaseUrl);
    demo_summary_set_text(tSummary, "requested_model", sRequestedModel);
    demo_summary_set_int(tSummary, "stream_mode", (int)eStreamMode);
    demo_summary_set_bool(tSummary, "multimodal", bUseMultimodal);
    demo_summary_set_text(tSummary, "response_format", sResponseFormat);
    demo_summary_set_bool(tSummary, "tool_enabled", bEnableTool);
    demo_summary_set_bool(tSummary, "best_effort_structured_output", bBestEffortStructuredOutput);
    demo_summary_set_text(tSummary, "case_name", sCaseName);
    demo_summary_set_int(tSummary, "duration_ms", (int64)uDurationMs);

    if ( pEvents != NULL ) {
        demo_summary_set_int(tSummary, "event_text_delta_count", pEvents->iTextDeltaCount);
        demo_summary_set_int(tSummary, "event_thinking_delta_count", pEvents->iThinkingDeltaCount);
        demo_summary_set_int(tSummary, "event_tool_ready_count", pEvents->iToolReadyCount);
        demo_summary_set_int(tSummary, "event_artifact_begin_count", pEvents->iArtifactBeginCount);
        demo_summary_set_int(tSummary, "event_artifact_chunk_count", pEvents->iArtifactChunkCount);
        demo_summary_set_int(tSummary, "event_artifact_ready_count", pEvents->iArtifactReadyCount);
        demo_summary_set_int(tSummary, "event_end_count", pEvents->iEndCount);
    }

    if ( pResponse != NULL ) {
        demo_count_output_parts(
            pResponse,
            &iTextCount,
            &iImageCount,
            &iFileCount,
            &iAudioCount,
            &iVideoCount,
            &iJsonCount
        );
        demo_summary_set_text(tSummary, "status", demo_status_name(pResponse->eStatus));
        demo_summary_set_text(tSummary, "model", pResponse->sModel);
        demo_summary_set_text(tSummary, "finish_reason", pResponse->sFinishReason);
        demo_summary_set_text(tSummary, "visible_text", xllm_response_get_text(pResponse));
        demo_summary_set_int(tSummary, "usage_input_tokens", (int64)pResponse->tUsage.uInputTokens);
        demo_summary_set_int(tSummary, "usage_output_tokens", (int64)pResponse->tUsage.uOutputTokens);
        demo_summary_set_int(tSummary, "usage_reasoning_tokens", (int64)pResponse->tUsage.uReasoningTokens);
        demo_summary_set_int(tSummary, "usage_cached_input_tokens", (int64)pResponse->tUsage.uCachedInputTokens);
        demo_summary_set_int(tSummary, "tool_call_count", (int64)xllm_response_get_tool_call_count(pResponse));
        demo_summary_set_int(tSummary, "text_part_count", (int64)iTextCount);
        demo_summary_set_int(tSummary, "image_part_count", (int64)iImageCount);
        demo_summary_set_int(tSummary, "file_part_count", (int64)iFileCount);
        demo_summary_set_int(tSummary, "audio_part_count", (int64)iAudioCount);
        demo_summary_set_int(tSummary, "video_part_count", (int64)iVideoCount);
        demo_summary_set_int(tSummary, "json_part_count", (int64)iJsonCount);
        demo_summary_set_int(tSummary, "artifact_part_count", (int64)(iImageCount + iFileCount + iAudioCount + iVideoCount));
    }

    if ( pError != NULL ) {
        demo_summary_set_text(tSummary, "error_code", demo_error_code_name(pError->eCode));
        demo_summary_set_int(tSummary, "error_status", pError->iStatus);
        demo_summary_set_int(tSummary, "http_status", pError->iHttpStatus);
        demo_summary_set_text(tSummary, "request_id", pError->sRequestId);
        demo_summary_set_text(tSummary, "error_message", pError->sMessage);
    }

    if ( sJsonOutput != NULL ) {
        demo_summary_set_text(tSummary, "json_output", sJsonOutput);
    }

    sSummary = (char *)xrtStringifyJSON(tSummary, 0, NULL);
    if ( sSummary != NULL ) {
        printf("probe_summary_json: %s\n", sSummary);
        demo_write_summary_file(sSummaryPath, sSummary);
        xrtFree(sSummary);
    }

    xvoUnref(tSummary);
}

static int demo_fail_with_summary(
    int iExitCode,
    xllm_error_code eErrorCode,
    const char *sMessage,
    const char *sAdapter,
    const char *sProvider,
    const char *sBaseUrl,
    const char *sRequestedModel,
    xllm_stream_mode eStreamMode,
    bool bUseMultimodal,
    const char *sResponseFormat,
    bool bEnableTool,
    bool bBestEffortStructuredOutput,
    const demo_event_state *pEvents,
    uint64 uStartMs
)
{
    xllm_error tError;
    uint64 uDurationMs = 0u;

    xllm_error_init(&tError);
    tError.eCode = eErrorCode;
    tError.iStatus = iExitCode;
    tError.sMessage = demo_dupstr(sMessage ? sMessage : "");
    if ( uStartMs != 0u ) {
        uDurationMs = demo_now_ms() - uStartMs;
    }

    demo_emit_json_summary(
        sAdapter,
        sProvider,
        sBaseUrl,
        sRequestedModel,
        eStreamMode,
        bUseMultimodal,
        sResponseFormat,
        bEnableTool,
        bBestEffortStructuredOutput,
        false,
        iExitCode,
        NULL,
        &tError,
        pEvents,
        NULL,
        uDurationMs
    );

    xllm_error_free(&tError);
    return iExitCode;
}

static void demo_log_callback(void *pCtx, xllm_log_level eLevel, const char *sComponent, const char *sMessage)
{
    (void)pCtx;
    fprintf(stderr, "[log/%d] %s: %s\n", (int)eLevel, sComponent ? sComponent : "(null)", sMessage ? sMessage : "");
}

static void demo_trace_callback(void *pCtx, xllm_trace_kind eKind, const xvalue *pPayload)
{
    const char *sPhase = NULL;
    const char *sResponseStatus = NULL;
    const char *sErrorCode = NULL;

    (void)pCtx;
    if ( pPayload == NULL || *pPayload == NULL ) {
        return;
    }

    sPhase = (const char *)xvoTableGetText(*pPayload, (str)"phase", 0u);
    sResponseStatus = (const char *)xvoTableGetText(*pPayload, (str)"response_status", 0u);
    sErrorCode = (const char *)xvoTableGetText(*pPayload, (str)"error_code", 0u);
    fprintf(
        stderr,
        "[trace/%d] phase=%s response_status=%s error_code=%s\n",
        (int)eKind,
        sPhase ? sPhase : "",
        sResponseStatus ? sResponseStatus : "",
        sErrorCode ? sErrorCode : ""
    );
}

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    demo_event_state *pState = (demo_event_state *)pUserData;

    if ( pEvent == NULL ) {
        return true;
    }

    switch ( pEvent->eType ) {
        case XLLM_EVENT_TEXT_DELTA:
            if ( pState ) {
                ++pState->iTextDeltaCount;
            }
            printf("%s", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
            fflush(stdout);
            break;
        case XLLM_EVENT_THINKING_DELTA:
            if ( pState ) {
                ++pState->iThinkingDeltaCount;
            }
            break;
        case XLLM_EVENT_TOOL_CALL_READY:
            if ( pState ) {
                ++pState->iToolReadyCount;
            }
            break;
        case XLLM_EVENT_ARTIFACT_BEGIN:
            if ( pState ) {
                ++pState->iArtifactBeginCount;
            }
            break;
        case XLLM_EVENT_ARTIFACT_CHUNK:
            if ( pState ) {
                ++pState->iArtifactChunkCount;
            }
            break;
        case XLLM_EVENT_ARTIFACT_READY:
            if ( pState ) {
                ++pState->iArtifactReadyCount;
            }
            break;
        case XLLM_EVENT_END:
            if ( pState ) {
                ++pState->iEndCount;
            }
            break;
        default:
            break;
    }

    return true;
}

static int demo_register_adapter(xllm_runtime *pRuntime, const char *sAdapter)
{
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return xllm_register_openai_compat_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return xllm_register_glm_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return xllm_register_minimax_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return xllm_register_kimi_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return xllm_register_anthropic_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return xllm_register_ollama_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return xllm_register_gemini_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return xllm_register_vertex_gemini_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return xllm_register_qwen_native_adapter(pRuntime);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return xllm_register_doubao_native_adapter(pRuntime);
    }
    fprintf(stderr, "unsupported adapter: %s\n", sAdapter);
    return -1;
}

static const char *demo_provider_name(const char *sAdapter)
{
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return "openai";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return "glm";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return "minimax";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return "moonshot";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return "anthropic";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return "ollama";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return "google";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return "google_vertex";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return "qwen";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return "volcengine";
    }
    return "unknown";
}

static const char *demo_adapter_auth_kind(const char *sAdapter)
{
    const char *sValue = demo_env_optional("XLLM_REAL_AUTH_KIND", NULL);

    if ( sValue != NULL && *sValue != '\0' ) {
        return sValue;
    }
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_AUTH_KIND", "OPENAI_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional("OLLAMA_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_AUTH_KIND", "DASHSCOPE_AUTH_KIND", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_AUTH_KIND", "ARK_AUTH_KIND", NULL);
    }
    return NULL;
}

static const char *demo_adapter_auth_header(const char *sAdapter)
{
    const char *sValue = getenv("XLLM_REAL_AUTH_HEADER");

    if ( sValue != NULL && *sValue != '\0' ) {
        return sValue;
    }
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_AUTH_HEADER", "OPENAI_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional("OLLAMA_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_AUTH_HEADER", "DASHSCOPE_AUTH_HEADER", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_AUTH_HEADER", "ARK_AUTH_HEADER", NULL);
    }
    return NULL;
}

static const char *demo_adapter_auth_scheme(const char *sAdapter)
{
    const char *sValue = demo_env_optional("XLLM_REAL_AUTH_SCHEME", NULL);

    if ( sValue != NULL && *sValue != '\0' ) {
        return sValue;
    }
    if ( sAdapter == NULL || *sAdapter == '\0' ) {
        return NULL;
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_AUTH_SCHEME", "OPENAI_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional("OLLAMA_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_AUTH_SCHEME", "DASHSCOPE_AUTH_SCHEME", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_AUTH_SCHEME", "ARK_AUTH_SCHEME", NULL);
    }
    return NULL;
}

static int demo_configure_auth(xllm_profile *pProfile, const char *sAdapter)
{
    const char *sAuthKind = demo_adapter_auth_kind(sAdapter);
    const char *sApiKey = demo_adapter_api_key(sAdapter);
    const char *sHeaderName = demo_adapter_auth_header(sAdapter);

    if ( sAuthKind == NULL || *sAuthKind == '\0' ) {
        if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
            sAuthKind = "bearer";
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ||
                    strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ||
                    strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ||
                    strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ||
                    strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
            sAuthKind = "bearer";
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
            sAuthKind = "api_key_header";
        } else if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ||
                    strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
            sAuthKind = "api_key_header";
        } else {
            sAuthKind = "none";
        }
    }

    if ( demo_stricmp(sAuthKind, "none") == 0 ) {
        pProfile->tAuth.eKind = XLLM_AUTH_NONE;
        return 0;
    }

    if ( sApiKey == NULL || *sApiKey == '\0' ) {
        fprintf(stderr, "API key is required for auth kind %s (set XLLM_REAL_API_KEY or adapter-specific *_API_KEY)\n", sAuthKind);
        return -1;
    }

    if ( demo_stricmp(sAuthKind, "bearer") == 0 ) {
        pProfile->tAuth.eKind = XLLM_AUTH_BEARER;
        pProfile->tAuth.sSecret = sApiKey;
        pProfile->tAuth.sScheme = demo_env_optional(demo_adapter_auth_scheme(sAdapter), "Bearer");
        return 0;
    }

    if ( demo_stricmp(sAuthKind, "api_key_header") == 0 ) {
        pProfile->tAuth.eKind = XLLM_AUTH_API_KEY_HEADER;
        pProfile->tAuth.sSecret = sApiKey;
        if ( sHeaderName == NULL || *sHeaderName == '\0' ) {
            if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
                sHeaderName = "x-api-key";
            } else if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ||
                        strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
                sHeaderName = "x-goog-api-key";
            } else {
                sHeaderName = "Authorization";
            }
        }
        pProfile->tAuth.sHeaderName = sHeaderName;
        return 0;
    }

    fprintf(stderr, "unsupported XLLM_REAL_AUTH_KIND: %s\n", sAuthKind);
    return -1;
}

static int demo_apply_vertex_credentials_from_env(xllm_profile *pProfile, const char *sAdapter)
{
    const char *sCredentialsPath;
    const char *sCredentialsJson;
    bool bUseAdc;
    xvalue tVendorExtra;

    if ( pProfile == NULL || sAdapter == NULL || strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) != 0 ) {
        return 0;
    }

    sCredentialsPath = demo_env_optional2(
        "XLLM_REAL_VERTEX_CREDENTIALS_PATH",
        "VERTEX_GEMINI_CREDENTIALS_PATH",
        NULL
    );
    sCredentialsJson = demo_env_optional2(
        "XLLM_REAL_VERTEX_CREDENTIALS_JSON",
        "VERTEX_GEMINI_CREDENTIALS_JSON",
        NULL
    );
    bUseAdc = demo_env_bool("XLLM_REAL_VERTEX_USE_ADC", false) ||
              demo_env_bool("VERTEX_GEMINI_USE_ADC", false);

    if ( (!sCredentialsPath || !sCredentialsPath[0]) &&
         (!sCredentialsJson || !sCredentialsJson[0]) &&
         !bUseAdc ) {
        return 0;
    }

    tVendorExtra = xvoCreateTable();
    if ( tVendorExtra == NULL ) {
        fprintf(stderr, "failed to create vertex vendor extra table\n");
        return -1;
    }

    if ( sCredentialsPath && sCredentialsPath[0] ) {
        if ( !xvoTableSetText(tVendorExtra, "vertex_credentials_path", 0u, (ptr)sCredentialsPath, 0u, TRUE) ) {
            xvoUnref(tVendorExtra);
            fprintf(stderr, "failed to set vertex_credentials_path\n");
            return -1;
        }
    }
    if ( sCredentialsJson && sCredentialsJson[0] ) {
        if ( !xvoTableSetText(tVendorExtra, "vertex_credentials_json", 0u, (ptr)sCredentialsJson, 0u, TRUE) ) {
            xvoUnref(tVendorExtra);
            fprintf(stderr, "failed to set vertex_credentials_json\n");
            return -1;
        }
    }
    if ( bUseAdc ) {
        if ( !xvoTableSetText(tVendorExtra, "vertex_credentials_path", 0u, (ptr)"USE_ADC", 0u, TRUE) ) {
            xvoUnref(tVendorExtra);
            fprintf(stderr, "failed to set vertex ADC marker\n");
            return -1;
        }
    }

    pProfile->tProviderOptions.tVendorExtra = tVendorExtra;
    return 0;
}

static int demo_apply_proxy_from_env(xllm_profile *pProfile)
{
    bool bConfigured = false;
    xllm_proxy_kind eKind = demo_proxy_kind_from_env(&bConfigured);
    const char *sHost;
    uint32 uPort;

    if ( pProfile == NULL || !bConfigured ) {
        return 0;
    }
    if ( eKind == XLLM_PROXY_UNSPECIFIED ) {
        fprintf(stderr, "unsupported XLLM_REAL_PROXY_KIND: %s\n", getenv("XLLM_REAL_PROXY_KIND"));
        return -1;
    }

    pProfile->tTransport.eProxyKind = eKind;
    if ( eKind == XLLM_PROXY_NONE ) {
        return 0;
    }

    sHost = demo_env_optional("XLLM_REAL_PROXY_HOST", NULL);
    uPort = demo_env_u32("XLLM_REAL_PROXY_PORT", 0u);
    if ( sHost == NULL || *sHost == '\0' ) {
        fprintf(stderr, "missing XLLM_REAL_PROXY_HOST while proxy is enabled\n");
        return -1;
    }
    if ( uPort == 0u ) {
        fprintf(stderr, "missing or invalid XLLM_REAL_PROXY_PORT while proxy is enabled\n");
        return -1;
    }

    pProfile->tTransport.sProxyHost = sHost;
    pProfile->tTransport.tProxyPort.bSet = true;
    pProfile->tTransport.tProxyPort.iValue = uPort;
    pProfile->tTransport.sProxyUser = demo_env_optional("XLLM_REAL_PROXY_USER", NULL);
    pProfile->tTransport.sProxyPass = demo_env_optional("XLLM_REAL_PROXY_PASS", NULL);
    return 0;
}

static void demo_configure_profile_caps(
    xllm_profile *pProfile,
    const char *sModelId,
    bool bUseMultimodal,
    xllm_stream_mode eStreamMode,
    bool bUseStructuredOutput,
    bool bEnableTool
)
{
    bool bReasoningConfigured = false;
    bool bExposeThinking = demo_env_has_value("XLLM_REAL_EXPOSE_THINKING")
        ? demo_env_bool("XLLM_REAL_EXPOSE_THINKING", false)
        : false;
    xllm_capability_flags uExtraFlags = 0;

    if ( demo_reasoning_level_from_env(&bReasoningConfigured) != XLLM_REASONING_DEFAULT || bReasoningConfigured ) {
        uExtraFlags |= XLLM_CAP_REASONING_CONTROL;
    }
    if ( bExposeThinking ) {
        uExtraFlags |= XLLM_CAP_THINKING_SUMMARY_OUT | XLLM_CAP_THINKING_FULL_OUT;
    }
    if ( bUseStructuredOutput ) {
        uExtraFlags |= XLLM_CAP_JSON_OUT;
    }
    if ( bEnableTool ) {
        uExtraFlags |= XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_TOOL_RESULT_IN;
    }

    pProfile->tModels.tText.sModelId = sModelId;
    pProfile->tModels.tText.tCaps.uFlags = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | uExtraFlags;
    if ( eStreamMode != XLLM_STREAM_OFF ) {
        pProfile->tModels.tText.tCaps.uFlags |= XLLM_CAP_STREAM;
    }

    if ( bUseMultimodal ) {
        pProfile->tModels.tMultimodal.sModelId = demo_env_optional("XLLM_REAL_MULTIMODAL_MODEL", sModelId);
        pProfile->tModels.tMultimodal.tCaps.uFlags =
            XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_IMAGE_IN | XLLM_CAP_FILE_IN | uExtraFlags;
        if ( eStreamMode != XLLM_STREAM_OFF ) {
            pProfile->tModels.tMultimodal.tCaps.uFlags |= XLLM_CAP_STREAM;
        }
    }
}

static const char *demo_adapter_tool_result_text(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_TEXT") ) {
        return getenv("XLLM_REAL_TOOL_RESULT_TEXT");
    }
    if ( sAdapter == NULL ) {
        return "probe-value";
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_TEXT", "OPENAI_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        return demo_env_optional("OLLAMA_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        return demo_env_optional("ANTHROPIC_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) {
        return demo_env_optional("GLM_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) {
        return demo_env_optional("MINIMAX_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) {
        return demo_env_optional("KIMI_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) {
        return demo_env_optional2("QWEN_TOOL_RESULT_TEXT", "DASHSCOPE_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) {
        return demo_env_optional2("DOUBAO_TOOL_RESULT_TEXT", "ARK_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("GEMINI_TOOL_RESULT_TEXT", "probe-value");
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) {
        return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_TEXT", "probe-value");
    }
    return "probe-value";
}

static const char *demo_adapter_tool_result_image_url(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_IMAGE_URL") ) return getenv("XLLM_REAL_TOOL_RESULT_IMAGE_URL");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_IMAGE_URL", "OPENAI_TOOL_RESULT_IMAGE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_TOOL_RESULT_IMAGE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_TOOL_RESULT_IMAGE_URL", "DASHSCOPE_TOOL_RESULT_IMAGE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_TOOL_RESULT_IMAGE_URL", "ARK_TOOL_RESULT_IMAGE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_TOOL_RESULT_IMAGE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_IMAGE_URL", NULL);
    return NULL;
}

static const char *demo_adapter_tool_result_image_file_id(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_IMAGE_FILE_ID") ) return getenv("XLLM_REAL_TOOL_RESULT_IMAGE_FILE_ID");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_IMAGE_FILE_ID", "OPENAI_TOOL_RESULT_IMAGE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_TOOL_RESULT_IMAGE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_TOOL_RESULT_IMAGE_FILE_ID", "DASHSCOPE_TOOL_RESULT_IMAGE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_TOOL_RESULT_IMAGE_FILE_ID", "ARK_TOOL_RESULT_IMAGE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_TOOL_RESULT_IMAGE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_IMAGE_FILE_ID", NULL);
    return NULL;
}

static const char *demo_adapter_tool_result_image_mime(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_IMAGE_MIME") ) return getenv("XLLM_REAL_TOOL_RESULT_IMAGE_MIME");
    if ( sAdapter == NULL ) return "image/png";
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_IMAGE_MIME", "OPENAI_TOOL_RESULT_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_TOOL_RESULT_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_TOOL_RESULT_IMAGE_MIME", "DASHSCOPE_TOOL_RESULT_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_TOOL_RESULT_IMAGE_MIME", "ARK_TOOL_RESULT_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_TOOL_RESULT_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_IMAGE_MIME", "image/png");
    return "image/png";
}

static const char *demo_adapter_tool_result_file_url(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_FILE_URL") ) return getenv("XLLM_REAL_TOOL_RESULT_FILE_URL");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_FILE_URL", "OPENAI_TOOL_RESULT_FILE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_TOOL_RESULT_FILE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_TOOL_RESULT_FILE_URL", "DASHSCOPE_TOOL_RESULT_FILE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_TOOL_RESULT_FILE_URL", "ARK_TOOL_RESULT_FILE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_TOOL_RESULT_FILE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_FILE_URL", NULL);
    return NULL;
}

static const char *demo_adapter_tool_result_file_file_id(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_FILE_FILE_ID") ) return getenv("XLLM_REAL_TOOL_RESULT_FILE_FILE_ID");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_FILE_FILE_ID", "OPENAI_TOOL_RESULT_FILE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_TOOL_RESULT_FILE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_TOOL_RESULT_FILE_FILE_ID", "DASHSCOPE_TOOL_RESULT_FILE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_TOOL_RESULT_FILE_FILE_ID", "ARK_TOOL_RESULT_FILE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_TOOL_RESULT_FILE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_FILE_FILE_ID", NULL);
    return NULL;
}

static const char *demo_adapter_tool_result_file_mime(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_FILE_MIME") ) return getenv("XLLM_REAL_TOOL_RESULT_FILE_MIME");
    if ( sAdapter == NULL ) return "application/pdf";
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_FILE_MIME", "OPENAI_TOOL_RESULT_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_TOOL_RESULT_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_TOOL_RESULT_FILE_MIME", "DASHSCOPE_TOOL_RESULT_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_TOOL_RESULT_FILE_MIME", "ARK_TOOL_RESULT_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_TOOL_RESULT_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_FILE_MIME", "application/pdf");
    return "application/pdf";
}

static const char *demo_adapter_tool_result_file_name(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_TOOL_RESULT_FILE_NAME") ) return getenv("XLLM_REAL_TOOL_RESULT_FILE_NAME");
    if ( sAdapter == NULL ) return "tool-result.bin";
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional2("AZURE_OPENAI_TOOL_RESULT_FILE_NAME", "OPENAI_TOOL_RESULT_FILE_NAME", "tool-result.bin");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_TOOL_RESULT_FILE_NAME", "tool-result.bin");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_TOOL_RESULT_FILE_NAME", "DASHSCOPE_TOOL_RESULT_FILE_NAME", "tool-result.bin");
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_TOOL_RESULT_FILE_NAME", "ARK_TOOL_RESULT_FILE_NAME", "tool-result.bin");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_TOOL_RESULT_FILE_NAME", "tool-result.bin");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_TOOL_RESULT_FILE_NAME", "tool-result.bin");
    return "tool-result.bin";
}

static bool demo_tool_result_requires_multimodal_model(const char *sAdapter)
{
    return
        (demo_adapter_tool_result_image_url(sAdapter) != NULL) ||
        (demo_adapter_tool_result_image_file_id(sAdapter) != NULL) ||
        (demo_adapter_tool_result_file_url(sAdapter) != NULL) ||
        (demo_adapter_tool_result_file_file_id(sAdapter) != NULL);
}

static char *demo_xrt_dup_cstr(const char *sText)
{
    size_t iLen;
    char *sCopy;

    if ( sText == NULL ) {
        return NULL;
    }

    iLen = strlen(sText);
    sCopy = (char *)xrtCalloc(iLen + 1u, sizeof(char));
    if ( sCopy == NULL ) {
        return NULL;
    }

    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

static int demo_override_optional_input_names(xllm_turn *pTurn)
{
    const char *sImageName = demo_env_optional(
        "XLLM_REAL_IMAGE_NAME",
        demo_env_optional2(
            "AZURE_OPENAI_IMAGE_NAME",
            "OPENAI_IMAGE_NAME",
            demo_env_optional(
                "ANTHROPIC_IMAGE_NAME",
                demo_env_optional(
                    "GLM_IMAGE_NAME",
                    demo_env_optional(
                        "MINIMAX_IMAGE_NAME",
                        demo_env_optional(
                            "KIMI_IMAGE_NAME",
                            demo_env_optional(
                                "QWEN_IMAGE_NAME",
                                demo_env_optional(
                                    "DOUBAO_IMAGE_NAME",
                                    demo_env_optional(
                                        "GEMINI_IMAGE_NAME",
                                        demo_env_optional("VERTEX_GEMINI_IMAGE_NAME", NULL)
                                    )
                                )
                            )
                        )
                    )
                )
            )
        )
    );
    const char *sFileName = demo_env_optional(
        "XLLM_REAL_FILE_NAME",
        demo_env_optional2(
            "AZURE_OPENAI_FILE_NAME",
            "OPENAI_FILE_NAME",
            demo_env_optional(
                "ANTHROPIC_FILE_NAME",
                demo_env_optional(
                    "GLM_FILE_NAME",
                    demo_env_optional(
                        "KIMI_FILE_NAME",
                        demo_env_optional(
                    "MINIMAX_FILE_NAME",
                    demo_env_optional(
                        "QWEN_FILE_NAME",
                        demo_env_optional(
                            "DOUBAO_FILE_NAME",
                            demo_env_optional(
                                "GEMINI_FILE_NAME",
                                demo_env_optional("VERTEX_GEMINI_FILE_NAME", NULL)
                            )
                        )
                    )
                        )
                    )
                )
            )
        )
    );
    size_t i;

    if ( pTurn == NULL ) {
        return 0;
    }
    if ( (sImageName == NULL || sImageName[0] == '\0') &&
         (sFileName == NULL || sFileName[0] == '\0') ) {
        return 0;
    }

    for ( i = 0; i < pTurn->iMessageCount; ++i ) {
        xllm_message *pMessage = &pTurn->pMessages[i];
        size_t j;

        for ( j = 0; j < pMessage->iPartCount; ++j ) {
            xllm_content_part *pPart = &pMessage->pParts[j];
            const char *sOverride = NULL;
            char *sCopy;

            if ( pPart->eKind == XLLM_PART_IMAGE ) {
                sOverride = sImageName;
            } else if ( pPart->eKind == XLLM_PART_FILE ) {
                sOverride = sFileName;
            } else {
                continue;
            }

            if ( sOverride == NULL || sOverride[0] == '\0' ) {
                continue;
            }

            sCopy = demo_xrt_dup_cstr(sOverride);
            if ( sCopy == NULL ) {
                return XRT_NET_ERROR;
            }

            if ( pPart->as.tSource.sName != NULL ) {
                xrtFree((ptr)pPart->as.tSource.sName);
            }
            pPart->as.tSource.sName = sCopy;
        }
    }

    return 0;
}

static const char *demo_adapter_image_url(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_IMAGE_URL") ) return getenv("XLLM_REAL_IMAGE_URL");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return getenv("AZURE_OPENAI_IMAGE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) return getenv("OLLAMA_IMAGE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return getenv("ANTHROPIC_IMAGE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return getenv("GLM_IMAGE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return getenv("MINIMAX_IMAGE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return getenv("KIMI_IMAGE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_IMAGE_URL", "DASHSCOPE_IMAGE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_IMAGE_URL", "ARK_IMAGE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return getenv("GEMINI_IMAGE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return getenv("VERTEX_GEMINI_IMAGE_URL");
    return NULL;
}

static const char *demo_adapter_image_path(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_IMAGE_PATH") ) return getenv("XLLM_REAL_IMAGE_PATH");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return getenv("AZURE_OPENAI_IMAGE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) return getenv("OLLAMA_IMAGE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return getenv("ANTHROPIC_IMAGE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return getenv("GLM_IMAGE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return getenv("MINIMAX_IMAGE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return getenv("KIMI_IMAGE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_IMAGE_PATH", "DASHSCOPE_IMAGE_PATH", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_IMAGE_PATH", "ARK_IMAGE_PATH", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return getenv("GEMINI_IMAGE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return getenv("VERTEX_GEMINI_IMAGE_PATH");
    return NULL;
}

static const char *demo_adapter_image_file_id(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_IMAGE_FILE_ID") ) return getenv("XLLM_REAL_IMAGE_FILE_ID");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        return demo_env_optional2("AZURE_OPENAI_IMAGE_FILE_ID", "OPENAI_IMAGE_FILE_ID", NULL);
    }
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) return getenv("OLLAMA_IMAGE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return getenv("ANTHROPIC_IMAGE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return getenv("GLM_IMAGE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return getenv("MINIMAX_IMAGE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return getenv("KIMI_IMAGE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_IMAGE_FILE_ID", "DASHSCOPE_IMAGE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_IMAGE_FILE_ID", "ARK_IMAGE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return getenv("GEMINI_IMAGE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return getenv("VERTEX_GEMINI_IMAGE_FILE_ID");
    return NULL;
}

static const char *demo_adapter_image_mime(const char *sAdapter)
{
    const char *sValue = demo_env_optional("XLLM_REAL_IMAGE_MIME", NULL);
    if ( sValue && *sValue ) return sValue;
    if ( sAdapter == NULL ) return "image/png";
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional("AZURE_OPENAI_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) return demo_env_optional("OLLAMA_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return demo_env_optional("GLM_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return demo_env_optional("MINIMAX_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return demo_env_optional("KIMI_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_IMAGE_MIME", "DASHSCOPE_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_IMAGE_MIME", "ARK_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_IMAGE_MIME", "image/png");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_IMAGE_MIME", "image/png");
    return "image/png";
}

static const char *demo_adapter_file_url(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_FILE_URL") ) return getenv("XLLM_REAL_FILE_URL");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) return getenv("OLLAMA_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return getenv("ANTHROPIC_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return getenv("GEMINI_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return getenv("VERTEX_GEMINI_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return getenv("AZURE_OPENAI_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return getenv("GLM_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return getenv("MINIMAX_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return getenv("KIMI_FILE_URL");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_FILE_URL", "DASHSCOPE_FILE_URL", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_FILE_URL", "ARK_FILE_URL", NULL);
    return NULL;
}

static const char *demo_adapter_file_path(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_FILE_PATH") ) return getenv("XLLM_REAL_FILE_PATH");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) return getenv("OLLAMA_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return getenv("ANTHROPIC_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return getenv("GEMINI_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return getenv("VERTEX_GEMINI_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return getenv("AZURE_OPENAI_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return getenv("GLM_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return getenv("MINIMAX_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return getenv("KIMI_FILE_PATH");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_FILE_PATH", "DASHSCOPE_FILE_PATH", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_FILE_PATH", "ARK_FILE_PATH", NULL);
    return NULL;
}

static const char *demo_adapter_file_file_id(const char *sAdapter)
{
    if ( demo_env_has_value("XLLM_REAL_FILE_FILE_ID") ) return getenv("XLLM_REAL_FILE_FILE_ID");
    if ( sAdapter == NULL ) return NULL;
    if ( strcmp(sAdapter, XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) return getenv("OLLAMA_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return getenv("ANTHROPIC_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return getenv("GEMINI_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return getenv("VERTEX_GEMINI_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return getenv("AZURE_OPENAI_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return getenv("GLM_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return getenv("MINIMAX_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return getenv("KIMI_FILE_FILE_ID");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_FILE_FILE_ID", "DASHSCOPE_FILE_FILE_ID", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_FILE_FILE_ID", "ARK_FILE_FILE_ID", NULL);
    return NULL;
}

static const char *demo_adapter_file_mime(const char *sAdapter)
{
    const char *sValue = demo_env_optional("XLLM_REAL_FILE_MIME", NULL);
    if ( sValue && *sValue ) return sValue;
    if ( sAdapter == NULL ) return "application/pdf";
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) return demo_env_optional("ANTHROPIC_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GEMINI_NATIVE) == 0 ) return demo_env_optional("GEMINI_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_VERTEX_GEMINI_NATIVE) == 0 ) return demo_env_optional("VERTEX_GEMINI_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) return demo_env_optional("AZURE_OPENAI_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_GLM_NATIVE) == 0 ) return demo_env_optional("GLM_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_MINIMAX_NATIVE) == 0 ) return demo_env_optional("MINIMAX_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_KIMI_NATIVE) == 0 ) return demo_env_optional("KIMI_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_QWEN_NATIVE) == 0 ) return demo_env_optional2("QWEN_FILE_MIME", "DASHSCOPE_FILE_MIME", "application/pdf");
    if ( strcmp(sAdapter, XLLM_ADAPTER_DOUBAO_NATIVE) == 0 ) return demo_env_optional2("DOUBAO_FILE_MIME", "ARK_FILE_MIME", "application/pdf");
    return "application/pdf";
}

static int demo_add_optional_inputs(const char *sAdapter, xllm_turn *pTurn, bool *pbUseMultimodal)
{
    const char *sImageUrl = demo_adapter_image_url(sAdapter);
    const char *sImagePath = demo_adapter_image_path(sAdapter);
    const char *sImageFileId = demo_adapter_image_file_id(sAdapter);
    const char *sImageMime = demo_adapter_image_mime(sAdapter);
    const char *sFileUrl = demo_adapter_file_url(sAdapter);
    const char *sFilePath = demo_adapter_file_path(sAdapter);
    const char *sFileFileId = demo_adapter_file_file_id(sAdapter);
    const char *sFileMime = demo_adapter_file_mime(sAdapter);
    int iStatus;

    if ( sImageUrl && *sImageUrl ) {
        iStatus = xllm_turn_add_image_url(pTurn, sImageUrl, sImageMime);
        if ( iStatus != 0 ) {
            return iStatus;
        }
        *pbUseMultimodal = true;
    }
    if ( sImagePath && *sImagePath ) {
        iStatus = xllm_turn_add_image_file(pTurn, sImagePath, sImageMime);
        if ( iStatus != 0 ) {
            return iStatus;
        }
        *pbUseMultimodal = true;
    }
    if ( sImageFileId && *sImageFileId ) {
        iStatus = xllm_turn_add_image_file_id(pTurn, sImageFileId, sImageMime);
        if ( iStatus != 0 ) {
            return iStatus;
        }
        *pbUseMultimodal = true;
    }
    if ( sFileUrl && *sFileUrl ) {
        iStatus = xllm_turn_add_file_url(pTurn, sFileUrl, sFileMime);
        if ( iStatus != 0 ) {
            return iStatus;
        }
        *pbUseMultimodal = true;
    }
    if ( sFilePath && *sFilePath ) {
        iStatus = xllm_turn_add_file(pTurn, sFilePath, sFileMime);
        if ( iStatus != 0 ) {
            return iStatus;
        }
        *pbUseMultimodal = true;
    }
    if ( sFileFileId && *sFileFileId ) {
        iStatus = xllm_turn_add_file_file_id(pTurn, sFileFileId, sFileMime);
        if ( iStatus != 0 ) {
            return iStatus;
        }
        *pbUseMultimodal = true;
    }

    return 0;
}

static const char *demo_default_prompt(bool bEnableTool, const char *sResponseFormat)
{
    if ( bEnableTool ) {
        return "Use the provided tool once, then answer with the tool result in one short sentence.";
    }
    if ( sResponseFormat != NULL && demo_stricmp(sResponseFormat, "json_schema") == 0 ) {
        return "Return a JSON object with a string field named value set to pong.";
    }
    if ( sResponseFormat != NULL && demo_stricmp(sResponseFormat, "json") == 0 ) {
        return "Return a JSON object exactly like {\"value\":\"pong\"}.";
    }
    return "Reply with exactly: pong";
}

static bool demo_text_contains(const char *sText, const char *sNeedle)
{
    if ( sNeedle == NULL || *sNeedle == '\0' ) {
        return true;
    }
    if ( sText == NULL ) {
        return false;
    }
    return strstr(sText, sNeedle) != NULL;
}

static int demo_validate_expectations(
    const xllm_response *pResponse,
    const char *sVisibleText,
    const char *sJsonOutput,
    const demo_event_state *pEvents,
    char **psMessage
)
{
    const char *sExpectedStatus = demo_env_optional("XLLM_REAL_EXPECT_STATUS", NULL);
    const char *sExpectedTextContains = demo_env_optional("XLLM_REAL_EXPECT_TEXT_CONTAINS", NULL);
    const char *sExpectedJsonContains = demo_env_optional("XLLM_REAL_EXPECT_JSON_CONTAINS", NULL);
    bool bHasExpectedToolCallsMin = demo_env_has_value("XLLM_REAL_EXPECT_TOOL_CALLS_MIN");
    bool bHasExpectedImagePartsMin = demo_env_has_value("XLLM_REAL_EXPECT_IMAGE_PARTS_MIN");
    bool bHasExpectedFilePartsMin = demo_env_has_value("XLLM_REAL_EXPECT_FILE_PARTS_MIN");
    bool bHasExpectedArtifactPartsMin = demo_env_has_value("XLLM_REAL_EXPECT_ARTIFACT_PARTS_MIN");
    bool bHasExpectedArtifactBeginMin = demo_env_has_value("XLLM_REAL_EXPECT_ARTIFACT_BEGIN_MIN");
    bool bHasExpectedArtifactChunkMin = demo_env_has_value("XLLM_REAL_EXPECT_ARTIFACT_CHUNK_MIN");
    bool bHasExpectedArtifactReadyMin = demo_env_has_value("XLLM_REAL_EXPECT_ARTIFACT_READY_MIN");
    uint32 uExpectedToolCallsMin = bHasExpectedToolCallsMin ? demo_env_u32("XLLM_REAL_EXPECT_TOOL_CALLS_MIN", 0u) : 0u;
    uint32 uExpectedImagePartsMin = bHasExpectedImagePartsMin ? demo_env_u32("XLLM_REAL_EXPECT_IMAGE_PARTS_MIN", 0u) : 0u;
    uint32 uExpectedFilePartsMin = bHasExpectedFilePartsMin ? demo_env_u32("XLLM_REAL_EXPECT_FILE_PARTS_MIN", 0u) : 0u;
    uint32 uExpectedArtifactPartsMin = bHasExpectedArtifactPartsMin ? demo_env_u32("XLLM_REAL_EXPECT_ARTIFACT_PARTS_MIN", 0u) : 0u;
    uint32 uExpectedArtifactBeginMin = bHasExpectedArtifactBeginMin ? demo_env_u32("XLLM_REAL_EXPECT_ARTIFACT_BEGIN_MIN", 0u) : 0u;
    uint32 uExpectedArtifactChunkMin = bHasExpectedArtifactChunkMin ? demo_env_u32("XLLM_REAL_EXPECT_ARTIFACT_CHUNK_MIN", 0u) : 0u;
    uint32 uExpectedArtifactReadyMin = bHasExpectedArtifactReadyMin ? demo_env_u32("XLLM_REAL_EXPECT_ARTIFACT_READY_MIN", 0u) : 0u;
    const char *sActualStatus = pResponse ? demo_status_name(pResponse->eStatus) : "";
    size_t iActualToolCalls = pResponse ? xllm_response_get_tool_call_count(pResponse) : 0u;
    size_t iActualTextParts = 0u;
    size_t iActualImageParts = 0u;
    size_t iActualFileParts = 0u;
    size_t iActualAudioParts = 0u;
    size_t iActualVideoParts = 0u;
    size_t iActualJsonParts = 0u;
    uint32 uActualArtifactBeginCount = pEvents ? (uint32)pEvents->iArtifactBeginCount : 0u;
    uint32 uActualArtifactChunkCount = pEvents ? (uint32)pEvents->iArtifactChunkCount : 0u;
    uint32 uActualArtifactReadyCount = pEvents ? (uint32)pEvents->iArtifactReadyCount : 0u;
    char aMessage[512];

    if ( psMessage ) {
        *psMessage = NULL;
    }

    demo_count_output_parts(
        pResponse,
        &iActualTextParts,
        &iActualImageParts,
        &iActualFileParts,
        &iActualAudioParts,
        &iActualVideoParts,
        &iActualJsonParts
    );

    if ( sExpectedStatus && *sExpectedStatus && demo_stricmp(sActualStatus, sExpectedStatus) != 0 ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected status '%s' but got '%s'",
            sExpectedStatus,
            sActualStatus
        );
    } else if ( sExpectedTextContains && *sExpectedTextContains && !demo_text_contains(sVisibleText, sExpectedTextContains) ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected visible_text to contain '%s'",
            sExpectedTextContains
        );
    } else if ( sExpectedJsonContains && *sExpectedJsonContains && !demo_text_contains(sJsonOutput, sExpectedJsonContains) ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected json_output to contain '%s'",
            sExpectedJsonContains
        );
    } else if ( bHasExpectedToolCallsMin && iActualToolCalls < (size_t)uExpectedToolCallsMin ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected at least %u tool call(s) but got %u",
            (unsigned)uExpectedToolCallsMin,
            (unsigned)iActualToolCalls
        );
    } else if ( bHasExpectedImagePartsMin && iActualImageParts < (size_t)uExpectedImagePartsMin ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected at least %u image part(s) but got %u",
            (unsigned)uExpectedImagePartsMin,
            (unsigned)iActualImageParts
        );
    } else if ( bHasExpectedFilePartsMin && iActualFileParts < (size_t)uExpectedFilePartsMin ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected at least %u file part(s) but got %u",
            (unsigned)uExpectedFilePartsMin,
            (unsigned)iActualFileParts
        );
    } else if ( bHasExpectedArtifactPartsMin &&
                (iActualImageParts + iActualFileParts + iActualAudioParts + iActualVideoParts) < (size_t)uExpectedArtifactPartsMin ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected at least %u artifact part(s) but got %u",
            (unsigned)uExpectedArtifactPartsMin,
            (unsigned)(iActualImageParts + iActualFileParts + iActualAudioParts + iActualVideoParts)
        );
    } else if ( bHasExpectedArtifactBeginMin && uActualArtifactBeginCount < uExpectedArtifactBeginMin ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected at least %u artifact begin event(s) but got %u",
            (unsigned)uExpectedArtifactBeginMin,
            (unsigned)uActualArtifactBeginCount
        );
    } else if ( bHasExpectedArtifactChunkMin && uActualArtifactChunkCount < uExpectedArtifactChunkMin ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected at least %u artifact chunk event(s) but got %u",
            (unsigned)uExpectedArtifactChunkMin,
            (unsigned)uActualArtifactChunkCount
        );
    } else if ( bHasExpectedArtifactReadyMin && uActualArtifactReadyCount < uExpectedArtifactReadyMin ) {
        snprintf(
            aMessage,
            sizeof(aMessage),
            "expected at least %u artifact ready event(s) but got %u",
            (unsigned)uExpectedArtifactReadyMin,
            (unsigned)uActualArtifactReadyCount
        );
    } else {
        return XRT_NET_OK;
    }

    if ( psMessage ) {
        *psMessage = demo_dupstr(aMessage);
        if ( *psMessage == NULL ) {
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_ERROR;
}

static const xvalue *demo_find_first_json_output(
    const xllm_response *pResponse,
    size_t *piOutputIndex,
    size_t *piPartIndex
)
{
    size_t i;

    if ( pResponse == NULL ) {
        return NULL;
    }

    for ( i = 0u; i < pResponse->iOutputCount; ++i ) {
        const xllm_output_item *pOutput = &pResponse->pOutputs[i];
        size_t j;

        if ( pOutput->eKind != XLLM_OUTPUT_MESSAGE ) {
            continue;
        }

        for ( j = 0u; j < pOutput->as.tMessage.iPartCount; ++j ) {
            const xllm_content_part *pPart = &pOutput->as.tMessage.pParts[j];
            if ( pPart->eKind != XLLM_PART_JSON || pPart->as.tJsonValue == NULL ) {
                continue;
            }
            if ( piOutputIndex ) {
                *piOutputIndex = i;
            }
            if ( piPartIndex ) {
                *piPartIndex = j;
            }
            return &pPart->as.tJsonValue;
        }
    }

    return NULL;
}

static int32 demo_tool_execute(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
)
{
    const char *sAdapter = (const char *)pCtx;
    const char *sExpectedToolId = demo_env_optional("XLLM_REAL_TOOL_ID", "app.probe.get_value");
    const char *sResultText = demo_adapter_tool_result_text(sAdapter);
    const char *sResultImageUrl = demo_adapter_tool_result_image_url(sAdapter);
    const char *sResultImageFileId = demo_adapter_tool_result_image_file_id(sAdapter);
    const char *sResultImageMime = demo_adapter_tool_result_image_mime(sAdapter);
    const char *sResultFileUrl = demo_adapter_tool_result_file_url(sAdapter);
    const char *sResultFileFileId = demo_adapter_tool_result_file_file_id(sAdapter);
    const char *sResultFileMime = demo_adapter_tool_result_file_mime(sAdapter);
    const char *sResultFileName = demo_adapter_tool_result_file_name(sAdapter);
    size_t iPartCount = 1u;
    size_t iWrite = 0u;

    if ( pRequest == NULL || pResult == NULL || pRequest->sToolId == NULL ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INVALID_REQUEST;
            pError->sMessage = "tool executor received an invalid request";
        }
        return XRT_NET_ERROR;
    }
    if ( strcmp(pRequest->sToolId, sExpectedToolId) != 0 ) {
        if ( pError ) {
            pError->eCode = XLLM_ERROR_INVALID_REQUEST;
            pError->sMessage = "unexpected tool id";
        }
        return XRT_NET_ERROR;
    }

    if ( (sResultImageUrl && sResultImageUrl[0] != '\0') ||
         (sResultImageFileId && sResultImageFileId[0] != '\0') ) {
        ++iPartCount;
    }
    if ( (sResultFileUrl && sResultFileUrl[0] != '\0') ||
         (sResultFileFileId && sResultFileFileId[0] != '\0') ) {
        ++iPartCount;
    }

    pResult->pParts = (xllm_content_part *)xrtCalloc(iPartCount, sizeof(xllm_content_part));
    if ( pResult->pParts == NULL ) {
        return XRT_NET_ERROR;
    }

    pResult->iPartCount = iPartCount;

    pResult->pParts[iWrite].eKind = XLLM_PART_TEXT;
    pResult->pParts[iWrite].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pResult->pParts[iWrite].as.tSource.sMimeType = demo_dupstr("text/plain");
    pResult->pParts[iWrite].as.tSource.as.sText = demo_dupstr(sResultText);
    if ( pResult->pParts[iWrite].as.tSource.sMimeType == NULL ||
         pResult->pParts[iWrite].as.tSource.as.sText == NULL ) {
        xllm_tool_exec_result_free(pResult);
        return XRT_NET_ERROR;
    }
    ++iWrite;

    if ( (sResultImageUrl && sResultImageUrl[0] != '\0') ||
         (sResultImageFileId && sResultImageFileId[0] != '\0') ) {
        pResult->pParts[iWrite].eKind = XLLM_PART_IMAGE;
        if ( sResultImageUrl && sResultImageUrl[0] != '\0' ) {
            pResult->pParts[iWrite].as.tSource.eKind = XLLM_SOURCE_URL;
            pResult->pParts[iWrite].as.tSource.as.sUrl = demo_dupstr(sResultImageUrl);
            if ( pResult->pParts[iWrite].as.tSource.as.sUrl == NULL ) {
                xllm_tool_exec_result_free(pResult);
                return XRT_NET_ERROR;
            }
        } else {
            pResult->pParts[iWrite].as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
            pResult->pParts[iWrite].as.tSource.as.sFileId = demo_dupstr(sResultImageFileId);
            if ( pResult->pParts[iWrite].as.tSource.as.sFileId == NULL ) {
                xllm_tool_exec_result_free(pResult);
                return XRT_NET_ERROR;
            }
        }
        pResult->pParts[iWrite].as.tSource.sMimeType = demo_dupstr(sResultImageMime);
        if ( pResult->pParts[iWrite].as.tSource.sMimeType == NULL ) {
            xllm_tool_exec_result_free(pResult);
            return XRT_NET_ERROR;
        }
        ++iWrite;
    }

    if ( (sResultFileUrl && sResultFileUrl[0] != '\0') ||
         (sResultFileFileId && sResultFileFileId[0] != '\0') ) {
        pResult->pParts[iWrite].eKind = XLLM_PART_FILE;
        if ( sResultFileUrl && sResultFileUrl[0] != '\0' ) {
            pResult->pParts[iWrite].as.tSource.eKind = XLLM_SOURCE_URL;
            pResult->pParts[iWrite].as.tSource.as.sUrl = demo_dupstr(sResultFileUrl);
            if ( pResult->pParts[iWrite].as.tSource.as.sUrl == NULL ) {
                xllm_tool_exec_result_free(pResult);
                return XRT_NET_ERROR;
            }
        } else {
            pResult->pParts[iWrite].as.tSource.eKind = XLLM_SOURCE_PROVIDER_FILE_ID;
            pResult->pParts[iWrite].as.tSource.as.sFileId = demo_dupstr(sResultFileFileId);
            if ( pResult->pParts[iWrite].as.tSource.as.sFileId == NULL ) {
                xllm_tool_exec_result_free(pResult);
                return XRT_NET_ERROR;
            }
        }
        pResult->pParts[iWrite].as.tSource.sMimeType = demo_dupstr(sResultFileMime);
        pResult->pParts[iWrite].as.tSource.sName = demo_dupstr(sResultFileName);
        if ( pResult->pParts[iWrite].as.tSource.sMimeType == NULL ||
             pResult->pParts[iWrite].as.tSource.sName == NULL ) {
            xllm_tool_exec_result_free(pResult);
            return XRT_NET_ERROR;
        }
    }

    return XRT_NET_OK;
}

int main(void)
{
    const char *sAdapter = demo_env_required("XLLM_REAL_ADAPTER");
    const char *sBaseUrl = demo_adapter_base_url(sAdapter);
    const char *sModelId = demo_adapter_model(sAdapter);
    const char *sProfileId = demo_env_optional("XLLM_REAL_PROFILE_ID", "real-profile");
    const char *sSystemPrompt = demo_env_optional("XLLM_REAL_SYSTEM_PROMPT", "You are a concise connectivity test assistant.");
    const char *sPrompt = NULL;
    const char *sPromptEnv = getenv("XLLM_REAL_PROMPT");
    const char *sResponseFormat = demo_env_optional("XLLM_REAL_RESPONSE_FORMAT", "text");
    const char *sProvider = NULL;
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_profile tProfile;
    xllm_create_options tCreate;
    xllm *pLlm = NULL;
    xllm_turn tTurn;
    xllm_call_options tCall;
    xllm_tool_def tTool;
    xllm_tool_executor tExecutor;
    xllm_response *pResponse = NULL;
    xllm_error tError;
    demo_event_state tEvents;
    xllm_stream_mode eStreamMode;
    uint32 uTimeoutMs;
    uint32 uMaxOutputTokens;
    bool bUseMultimodal = false;
    bool bEnableTool = demo_adapter_tool_enabled(sAdapter);
    bool bProviderTool = false;
    bool bBestEffortStructuredOutput = demo_env_bool("XLLM_REAL_BEST_EFFORT_JSON", false);
    bool bUseStructuredOutput = demo_response_format_is_json(sResponseFormat);
    bool bReasoningConfigured = false;
    xllm_reasoning_level eReasoningLevel;
    xvalue tSchema = NULL;
    size_t iJsonOutputIndex = 0u;
    size_t iJsonPartIndex = 0u;
    size_t iTextPartCount = 0u;
    size_t iImagePartCount = 0u;
    size_t iFilePartCount = 0u;
    size_t iAudioPartCount = 0u;
    size_t iVideoPartCount = 0u;
    size_t iJsonPartCount = 0u;
    const char **psAnthropicBetaHeaders = NULL;
    size_t iAnthropicBetaHeaderCount = 0u;
    xllm_header *pDefaultHeaders = NULL;
    size_t iDefaultHeaderCount = 0u;
    const xvalue *pJson = NULL;
    char *sJson = NULL;
    char *sExpectationMessage = NULL;
    const char *sVisibleText = NULL;
    int iStatus;
    uint64 uStartMs = demo_now_ms();
    uint64 uDurationMs = 0u;

    memset(&tEvents, 0, sizeof(tEvents));
    eStreamMode = demo_stream_mode_from_env(sAdapter);
    sProvider = (sAdapter != NULL && *sAdapter != '\0') ? demo_provider_name(sAdapter) : NULL;
    bProviderTool = bEnableTool && demo_tool_kind_is_provider();

    if ( sAdapter == NULL || sBaseUrl == NULL || sModelId == NULL ) {
        return demo_fail_with_summary(
            2,
            XLLM_ERROR_INVALID_REQUEST,
            "missing required environment variables (set XLLM_REAL_* or adapter-specific aliases)",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            false,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    memset(&tCreate, 0, sizeof(tCreate));
    memset(&tTool, 0, sizeof(tTool));
    memset(&tExecutor, 0, sizeof(tExecutor));
    {
        const char *sTimeoutEnv = demo_adapter_timeout_env(sAdapter);
        const char *sTimeoutEnv2 = demo_adapter_timeout_env2(sAdapter);
        uint32 uDefaultTimeout = 60000u;

        if ( sTimeoutEnv && demo_env_has_value(sTimeoutEnv) ) {
            uDefaultTimeout = demo_env_u32(sTimeoutEnv, uDefaultTimeout);
        }
        if ( sTimeoutEnv2 && demo_env_has_value(sTimeoutEnv2) ) {
            uDefaultTimeout = demo_env_u32(sTimeoutEnv2, uDefaultTimeout);
        }
        uTimeoutMs = demo_env_u32("XLLM_REAL_TIMEOUT_MS", uDefaultTimeout);
    }
    uMaxOutputTokens = demo_env_u32("XLLM_REAL_MAX_OUTPUT_TOKENS", 128u);
    eReasoningLevel = demo_reasoning_level_from_env(&bReasoningConfigured);
    sPrompt = (sPromptEnv && *sPromptEnv) ? sPromptEnv : demo_default_prompt(bEnableTool, sResponseFormat);

    xllm_runtime_options_init(&tRuntimeOptions);
    if ( demo_env_bool("XLLM_REAL_ENABLE_LOG", true) ) {
        tRuntimeOptions.pfnLog = demo_log_callback;
    }
    if ( demo_env_bool("XLLM_REAL_ENABLE_TRACE", true) ) {
        tRuntimeOptions.pfnTrace = demo_trace_callback;
    }

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != 0 || pRuntime == NULL ) {
        fprintf(stderr, "xllm_runtime_create failed: %d\n", iStatus);
        return demo_fail_with_summary(
            3,
            XLLM_ERROR_INTERNAL,
            "xllm_runtime_create failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            false,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    iStatus = demo_register_adapter(pRuntime, sAdapter);
    if ( iStatus != 0 ) {
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            4,
            XLLM_ERROR_INVALID_REQUEST,
            "demo_register_adapter failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            false,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    xllm_profile_init(&tProfile);
    tProfile.sId = sProfileId;
    tProfile.sName = "real-provider-probe";
    tProfile.sProvider = demo_provider_name(sAdapter);
    tProfile.sAdapter = sAdapter;
    tProfile.sBaseUrl = sBaseUrl;
    tProfile.tDefaults.tGeneration.tMaxOutputTokens.bSet = true;
    tProfile.tDefaults.tGeneration.tMaxOutputTokens.iValue = uMaxOutputTokens;
    tProfile.tProviderOptions.sOpenAIOrganizationId =
        demo_env_optional("XLLM_REAL_OPENAI_ORGANIZATION", NULL);
    tProfile.tProviderOptions.sOpenAIProjectId =
        demo_env_optional("XLLM_REAL_OPENAI_PROJECT", NULL);
    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        tProfile.tProviderOptions.sAnthropicApiVersion =
            demo_env_optional("XLLM_REAL_ANTHROPIC_VERSION", "2023-06-01");
    }
    if ( demo_configure_auth(&tProfile, sAdapter) != 0 ) {
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            5,
            XLLM_ERROR_INVALID_REQUEST,
            "demo_configure_auth failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            false,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }
    if ( demo_apply_vertex_credentials_from_env(&tProfile, sAdapter) != 0 ) {
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            5,
            XLLM_ERROR_INVALID_REQUEST,
            "demo_apply_vertex_credentials_from_env failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            false,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }
    if ( demo_apply_proxy_from_env(&tProfile) != 0 ) {
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            6,
            XLLM_ERROR_INVALID_REQUEST,
            "demo_apply_proxy_from_env failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            false,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    xllm_turn_init(&tTurn);
    iStatus = xllm_turn_add_user_text(&tTurn, sPrompt);
    if ( iStatus != 0 ) {
        fprintf(stderr, "xllm_turn_add_user_text failed: %d\n", iStatus);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            6,
            XLLM_ERROR_INTERNAL,
            "xllm_turn_add_user_text failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            false,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    iStatus = demo_add_optional_inputs(sAdapter, &tTurn, &bUseMultimodal);
    if ( iStatus != 0 ) {
        fprintf(stderr, "adding optional multimodal inputs failed: %d\n", iStatus);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            7,
            XLLM_ERROR_INVALID_REQUEST,
            "adding optional multimodal inputs failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            bUseMultimodal,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }
    iStatus = demo_override_optional_input_names(&tTurn);
    if ( iStatus != 0 ) {
        fprintf(stderr, "overriding optional input names failed: %d\n", iStatus);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            8,
            XLLM_ERROR_INTERNAL,
            "overriding optional input names failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            bUseMultimodal,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    if ( bEnableTool && demo_tool_result_requires_multimodal_model(sAdapter) ) {
        bUseMultimodal = true;
    }

    demo_configure_profile_caps(
        &tProfile,
        sModelId,
        bUseMultimodal,
        eStreamMode,
        bUseStructuredOutput && !bBestEffortStructuredOutput,
        bEnableTool
    );

    if ( bReasoningConfigured ) {
        tTurn.tReasoning.tEnabled.bSet = true;
        tTurn.tReasoning.tEnabled.bValue = (eReasoningLevel != XLLM_REASONING_OFF);
        tTurn.tReasoning.eLevel = eReasoningLevel;
    }
    if ( demo_env_has_value("XLLM_REAL_EXPOSE_THINKING") ) {
        tTurn.tReasoning.tExposeThinking.bSet = true;
        tTurn.tReasoning.tExposeThinking.bValue = demo_env_bool("XLLM_REAL_EXPOSE_THINKING", false);
    }
    if ( demo_env_has_value("XLLM_REAL_REASONING_BUDGET_TOKENS") ) {
        tTurn.tReasoning.tBudgetTokens.bSet = true;
        tTurn.tReasoning.tBudgetTokens.iValue = demo_env_u32("XLLM_REAL_REASONING_BUDGET_TOKENS", 0u);
    }
    if ( bUseStructuredOutput ) {
        if ( demo_stricmp(sResponseFormat, "json") == 0 ) {
            tTurn.tResponseFormat.eKind = XLLM_RESPONSE_JSON;
        } else if ( demo_stricmp(sResponseFormat, "json_schema") == 0 ) {
            const char *sSchemaJson = demo_env_optional(
                "XLLM_REAL_JSON_SCHEMA",
                "{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"required\":[\"value\"]}"
            );
            tSchema = xrtParseJSON((str)sSchemaJson, strlen(sSchemaJson));
            if ( tSchema == NULL ) {
                fprintf(stderr, "failed to parse XLLM_REAL_JSON_SCHEMA\n");
                xllm_turn_reset(&tTurn);
                xllm_runtime_destroy(pRuntime);
                return demo_fail_with_summary(
                    8,
                    XLLM_ERROR_INVALID_REQUEST,
                    "failed to parse XLLM_REAL_JSON_SCHEMA",
                    sAdapter,
                    sProvider,
                    sBaseUrl,
                    sModelId,
                    eStreamMode,
                    bUseMultimodal,
                    sResponseFormat,
                    bEnableTool,
                    bBestEffortStructuredOutput,
                    &tEvents,
                    uStartMs
                );
            }
            iStatus = xllm_turn_set_json_schema_response(
                &tTurn,
                demo_env_optional("XLLM_REAL_SCHEMA_NAME", "probe_schema"),
                tSchema,
                NULL
            );
            xvoUnref(tSchema);
            tSchema = NULL;
            if ( iStatus != XRT_NET_OK ) {
                fprintf(stderr, "xllm_turn_set_json_schema_response failed: %d\n", iStatus);
                xllm_turn_reset(&tTurn);
                xllm_runtime_destroy(pRuntime);
                return demo_fail_with_summary(
                    8,
                    XLLM_ERROR_INVALID_REQUEST,
                    "xllm_turn_set_json_schema_response failed",
                    sAdapter,
                    sProvider,
                    sBaseUrl,
                    sModelId,
                    eStreamMode,
                    bUseMultimodal,
                    sResponseFormat,
                    bEnableTool,
                    bBestEffortStructuredOutput,
                    &tEvents,
                    uStartMs
                );
            }
        } else {
            fprintf(stderr, "unsupported XLLM_REAL_RESPONSE_FORMAT: %s\n", sResponseFormat);
            xllm_turn_reset(&tTurn);
            xllm_runtime_destroy(pRuntime);
            return demo_fail_with_summary(
                8,
                XLLM_ERROR_INVALID_REQUEST,
                "unsupported XLLM_REAL_RESPONSE_FORMAT",
                sAdapter,
                sProvider,
                sBaseUrl,
                sModelId,
                eStreamMode,
                bUseMultimodal,
                sResponseFormat,
                bEnableTool,
                bBestEffortStructuredOutput,
                &tEvents,
                uStartMs
            );
        }
    }
    if ( bEnableTool ) {
        tTool.sToolId = demo_env_optional("XLLM_REAL_TOOL_ID", "app.probe.get_value");
        if ( bProviderTool ) {
            const char *sProviderToolJson = demo_env_optional(
                "XLLM_REAL_PROVIDER_TOOL_JSON",
                demo_default_provider_tool_json(sAdapter)
            );

            tSchema = xrtParseJSON((str)sProviderToolJson, strlen(sProviderToolJson));
            if ( tSchema == NULL || xvoType(tSchema) != XVO_DT_TABLE ) {
                if ( tSchema != NULL ) {
                    xvoUnref(tSchema);
                    tSchema = NULL;
                }
                fprintf(stderr, "failed to parse XLLM_REAL_PROVIDER_TOOL_JSON\n");
                xllm_turn_reset(&tTurn);
                xllm_runtime_destroy(pRuntime);
                return demo_fail_with_summary(
                    8,
                    XLLM_ERROR_INVALID_REQUEST,
                    "failed to parse XLLM_REAL_PROVIDER_TOOL_JSON",
                    sAdapter,
                    sProvider,
                    sBaseUrl,
                    sModelId,
                    eStreamMode,
                    bUseMultimodal,
                    sResponseFormat,
                    bEnableTool,
                    bBestEffortStructuredOutput,
                    &tEvents,
                    uStartMs
                );
            }

            tTool.sWireName = demo_env_optional("XLLM_REAL_TOOL_WIRE_NAME", demo_adapter_tool_choice_name(sAdapter));
            tTool.sDescription = demo_env_optional("XLLM_REAL_TOOL_DESCRIPTION", "Provider-native tool for connectivity testing.");
            tTool.eKind = XLLM_TOOL_PROVIDER;
            tTool.tVendorExtra = tSchema;
            iStatus = xllm_turn_add_tool(&tTurn, &tTool);
            xvoUnref(tSchema);
            tSchema = NULL;
        } else {
            const char *sToolSchemaJson = demo_env_optional(
                "XLLM_REAL_TOOL_SCHEMA",
                "{\"type\":\"object\",\"properties\":{\"input\":{\"type\":\"string\"}},\"required\":[\"input\"]}"
            );

            tSchema = xrtParseJSON((str)sToolSchemaJson, strlen(sToolSchemaJson));
            if ( tSchema == NULL ) {
                fprintf(stderr, "failed to parse XLLM_REAL_TOOL_SCHEMA\n");
                xllm_turn_reset(&tTurn);
                xllm_runtime_destroy(pRuntime);
                return demo_fail_with_summary(
                    8,
                    XLLM_ERROR_INVALID_REQUEST,
                    "failed to parse XLLM_REAL_TOOL_SCHEMA",
                    sAdapter,
                    sProvider,
                    sBaseUrl,
                    sModelId,
                    eStreamMode,
                    bUseMultimodal,
                    sResponseFormat,
                    bEnableTool,
                    bBestEffortStructuredOutput,
                    &tEvents,
                    uStartMs
                );
            }

            tTool.sWireName = demo_env_optional("XLLM_REAL_TOOL_WIRE_NAME", demo_adapter_tool_choice_name(sAdapter));
            if ( tTool.sWireName == NULL || tTool.sWireName[0] == '\0' ) {
                tTool.sWireName = "get_probe_value";
            }
            tTool.sDescription = demo_env_optional("XLLM_REAL_TOOL_DESCRIPTION", "Return a probe value for connectivity testing.");
            tTool.tInputSchema = tSchema;
            iStatus = xllm_turn_add_tool(&tTurn, &tTool);
            xvoUnref(tSchema);
            tSchema = NULL;
        }
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "xllm_turn_add_tool failed: %d\n", iStatus);
            xllm_turn_reset(&tTurn);
            xllm_runtime_destroy(pRuntime);
            return demo_fail_with_summary(
                8,
                XLLM_ERROR_INVALID_REQUEST,
                "xllm_turn_add_tool failed",
                sAdapter,
                sProvider,
                sBaseUrl,
                sModelId,
                eStreamMode,
                bUseMultimodal,
                sResponseFormat,
                bEnableTool,
                bBestEffortStructuredOutput,
                &tEvents,
                uStartMs
            );
        }

        iStatus = demo_apply_tool_choice_from_env(sAdapter, &tTurn);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "failed to apply tool choice configuration: %d\n", iStatus);
            xllm_turn_reset(&tTurn);
            xllm_runtime_destroy(pRuntime);
            return demo_fail_with_summary(
                8,
                XLLM_ERROR_INVALID_REQUEST,
                "failed to apply tool choice configuration",
                sAdapter,
                sProvider,
                sBaseUrl,
                sModelId,
                eStreamMode,
                bUseMultimodal,
                sResponseFormat,
                bEnableTool,
                bBestEffortStructuredOutput,
                &tEvents,
                uStartMs
            );
        }
    }

    if ( strcmp(sAdapter, XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        iStatus = demo_parse_string_list(
            demo_env_optional("XLLM_REAL_ANTHROPIC_BETA_HEADERS", NULL),
            ',',
            &psAnthropicBetaHeaders,
            &iAnthropicBetaHeaderCount
        );
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "failed to parse XLLM_REAL_ANTHROPIC_BETA_HEADERS\n");
            xllm_turn_reset(&tTurn);
            xllm_runtime_destroy(pRuntime);
            return demo_fail_with_summary(
                8,
                XLLM_ERROR_INVALID_REQUEST,
                "failed to parse XLLM_REAL_ANTHROPIC_BETA_HEADERS",
                sAdapter,
                sProvider,
                sBaseUrl,
                sModelId,
                eStreamMode,
                bUseMultimodal,
                sResponseFormat,
                bEnableTool,
                bBestEffortStructuredOutput,
                &tEvents,
                uStartMs
            );
        }
        tProfile.tProviderOptions.psAnthropicBetaHeaders = psAnthropicBetaHeaders;
        tProfile.tProviderOptions.iAnthropicBetaHeaderCount = iAnthropicBetaHeaderCount;
    }

    iStatus = demo_parse_header_list(
        demo_env_optional("XLLM_REAL_DEFAULT_HEADERS", NULL),
        &pDefaultHeaders,
        &iDefaultHeaderCount
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "failed to parse XLLM_REAL_DEFAULT_HEADERS\n");
        demo_free_owned_string_list(&psAnthropicBetaHeaders, &iAnthropicBetaHeaderCount);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            8,
            XLLM_ERROR_INVALID_REQUEST,
            "failed to parse XLLM_REAL_DEFAULT_HEADERS",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            bUseMultimodal,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }
    tProfile.pDefaultHeaders = pDefaultHeaders;
    tProfile.iDefaultHeaderCount = iDefaultHeaderCount;

    iStatus = xllm_register_profile(pRuntime, &tProfile);
    demo_free_owned_header_array(&pDefaultHeaders, &iDefaultHeaderCount);
    demo_free_owned_string_list(&psAnthropicBetaHeaders, &iAnthropicBetaHeaderCount);
    if ( iStatus != 0 ) {
        fprintf(stderr, "xllm_register_profile failed: %d\n", iStatus);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            8,
            XLLM_ERROR_INVALID_REQUEST,
            "xllm_register_profile failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            bUseMultimodal,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    memset(&tCreate, 0, sizeof(tCreate));
    tCreate.sInitialProfileId = sProfileId;
    tCreate.sSystemPrompt = sSystemPrompt;
    tCreate.tDefaultCallOptions.eStreamMode = eStreamMode;
    tCreate.tDefaultCallOptions.uTimeoutMs = uTimeoutMs;
    tCreate.tDefaultCallOptions.pfnOnEvent = demo_should_capture_events(eStreamMode) ? demo_on_event : NULL;
    tCreate.tDefaultCallOptions.pUserData = &tEvents;
    pLlm = xllm_create(pRuntime, &tCreate);
    if ( pLlm == NULL ) {
        fprintf(stderr, "xllm_create failed\n");
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return demo_fail_with_summary(
            9,
            XLLM_ERROR_INTERNAL,
            "xllm_create failed",
            sAdapter,
            sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            bUseMultimodal,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            &tEvents,
            uStartMs
        );
    }

    xllm_call_options_init(&tCall);
    tCall.eStreamMode = eStreamMode;
    tCall.uTimeoutMs = uTimeoutMs;
    tCall.bBestEffortStructuredOutput = bBestEffortStructuredOutput;
    if ( demo_should_capture_events(eStreamMode) ) {
        tCall.pfnOnEvent = demo_on_event;
        tCall.pUserData = &tEvents;
    }
    if ( bEnableTool && !bProviderTool ) {
        tExecutor.pCtx = (void *)sAdapter;
        tExecutor.pfnExecute = demo_tool_execute;
        iStatus = xllm_set_tool_executor(pLlm, &tExecutor);
        if ( iStatus != XRT_NET_OK ) {
            fprintf(stderr, "xllm_set_tool_executor failed: %d\n", iStatus);
            xllm_destroy(pLlm);
            xllm_turn_reset(&tTurn);
            xllm_runtime_destroy(pRuntime);
            return demo_fail_with_summary(
                10,
                XLLM_ERROR_INTERNAL,
                "xllm_set_tool_executor failed",
                sAdapter,
                sProvider,
                sBaseUrl,
                sModelId,
                eStreamMode,
                bUseMultimodal,
                sResponseFormat,
                bEnableTool,
                bBestEffortStructuredOutput,
                &tEvents,
                uStartMs
            );
        }
    }

    xllm_error_init(&tError);
    fprintf(
        stderr,
        "probe adapter=%s provider=%s base_url=%s model=%s stream=%d multimodal=%d response_format=%s tool=%d\n",
        sAdapter,
        tProfile.sProvider ? tProfile.sProvider : "",
        sBaseUrl,
        sModelId,
        (int)eStreamMode,
        bUseMultimodal ? 1 : 0,
        sResponseFormat ? sResponseFormat : "text",
        bEnableTool ? 1 : 0
    );

    iStatus = xllm_send_ex(pLlm, &tTurn, &tCall, &pResponse, &tError);
    if ( iStatus != 0 ) {
        fprintf(
            stderr,
            "probe failed: rc=%d error=%s http=%d status=%d request_id=%s message=%s\n",
            iStatus,
            demo_error_code_name(tError.eCode),
            (int)tError.iHttpStatus,
            (int)tError.iStatus,
            tError.sRequestId ? tError.sRequestId : "",
            tError.sMessage ? tError.sMessage : ""
        );
        demo_emit_json_summary(
            sAdapter,
            tProfile.sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            bUseMultimodal,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            false,
            iStatus,
            NULL,
            &tError,
            &tEvents,
            NULL,
            demo_now_ms() - uStartMs
        );
        xllm_error_free(&tError);
        xllm_destroy(pLlm);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 11;
    }

    if ( eStreamMode != XLLM_STREAM_OFF && tEvents.iTextDeltaCount > 0 ) {
        printf("\n");
    }

    printf("status: %s\n", demo_status_name(pResponse->eStatus));
    printf("model: %s\n", pResponse->sModel ? pResponse->sModel : "");
    printf("finish_reason: %s\n", pResponse->sFinishReason ? pResponse->sFinishReason : "");
    sVisibleText = xllm_response_get_text(pResponse);
    printf("visible_text: %s\n", sVisibleText ? sVisibleText : "");
    printf(
        "usage: input=%u output=%u reasoning=%u cached=%u\n",
        (unsigned)pResponse->tUsage.uInputTokens,
        (unsigned)pResponse->tUsage.uOutputTokens,
        (unsigned)pResponse->tUsage.uReasoningTokens,
        (unsigned)pResponse->tUsage.uCachedInputTokens
    );
    printf(
        "events: text_delta=%d thinking_delta=%d tool_ready=%d artifact_begin=%d artifact_chunk=%d artifact_ready=%d end=%d\n",
        tEvents.iTextDeltaCount,
        tEvents.iThinkingDeltaCount,
        tEvents.iToolReadyCount,
        tEvents.iArtifactBeginCount,
        tEvents.iArtifactChunkCount,
        tEvents.iArtifactReadyCount,
        tEvents.iEndCount
    );
    uDurationMs = demo_now_ms() - uStartMs;
    printf("duration_ms: %u\n", (unsigned)uDurationMs);
    printf("tool_calls: %u\n", (unsigned)xllm_response_get_tool_call_count(pResponse));
    demo_count_output_parts(
        pResponse,
        &iTextPartCount,
        &iImagePartCount,
        &iFilePartCount,
        &iAudioPartCount,
        &iVideoPartCount,
        &iJsonPartCount
    );
    printf(
        "parts: text=%u image=%u file=%u audio=%u video=%u json=%u\n",
        (unsigned)iTextPartCount,
        (unsigned)iImagePartCount,
        (unsigned)iFilePartCount,
        (unsigned)iAudioPartCount,
        (unsigned)iVideoPartCount,
        (unsigned)iJsonPartCount
    );
    pJson = demo_find_first_json_output(pResponse, &iJsonOutputIndex, &iJsonPartIndex);
    if ( pJson != NULL ) {
        sJson = (char *)xrtStringifyJSON(*pJson, 0, NULL);
        printf(
            "json_output[%u:%u]: %s\n",
            (unsigned)iJsonOutputIndex,
            (unsigned)iJsonPartIndex,
            sJson ? sJson : ""
        );
    }
    iStatus = demo_validate_expectations(pResponse, sVisibleText, sJson, &tEvents, &sExpectationMessage);
    if ( iStatus != XRT_NET_OK ) {
        xllm_error tExpectationError;

        fprintf(
            stderr,
            "probe expectation failed: %s\n",
            sExpectationMessage ? sExpectationMessage : "unexpected response"
        );
        xllm_error_init(&tExpectationError);
        tExpectationError.eCode = XLLM_ERROR_PARSE;
        tExpectationError.iStatus = 12;
        tExpectationError.sMessage = demo_dupstr(
            sExpectationMessage ? sExpectationMessage : "unexpected response"
        );
        uDurationMs = demo_now_ms() - uStartMs;
        demo_emit_json_summary(
            sAdapter,
            tProfile.sProvider,
            sBaseUrl,
            sModelId,
            eStreamMode,
            bUseMultimodal,
            sResponseFormat,
            bEnableTool,
            bBestEffortStructuredOutput,
            false,
            12,
            pResponse,
            &tExpectationError,
            &tEvents,
            sJson,
            uDurationMs
        );
        xllm_error_free(&tExpectationError);
        if ( sExpectationMessage ) {
            xrtFree(sExpectationMessage);
            sExpectationMessage = NULL;
        }
        if ( sJson ) {
            xrtFree(sJson);
            sJson = NULL;
        }
        xllm_response_free(pResponse);
        xllm_destroy(pLlm);
        xllm_turn_reset(&tTurn);
        xllm_runtime_destroy(pRuntime);
        return 12;
    }
    if ( pResponse->eStatus == XLLM_STATUS_REFUSED || pResponse->eStatus == XLLM_STATUS_CONTENT_FILTERED ) {
        printf("probe reached provider successfully but ended with status=%s\n", demo_status_name(pResponse->eStatus));
    } else {
        printf("probe ok\n");
    }
    demo_emit_json_summary(
        sAdapter,
        tProfile.sProvider,
        sBaseUrl,
        sModelId,
        eStreamMode,
        bUseMultimodal,
        sResponseFormat,
        bEnableTool,
        bBestEffortStructuredOutput,
        true,
        iStatus,
        pResponse,
        NULL,
        &tEvents,
        sJson,
        uDurationMs
    );

    if ( sJson ) {
        xrtFree(sJson);
        sJson = NULL;
    }
    if ( sExpectationMessage ) {
        xrtFree(sExpectationMessage);
        sExpectationMessage = NULL;
    }
    xllm_response_free(pResponse);
    xllm_destroy(pLlm);
    xllm_turn_reset(&tTurn);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
