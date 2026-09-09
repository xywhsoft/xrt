#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"

#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"

#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"

#include "../xwork.c"

typedef struct {
    size_t iExecCount;
    size_t iRequestTraceCount;
    size_t iResponseTraceCount;
    size_t iStreamTraceCount;
    size_t iToolLoopTraceCount;
} xwork_provider_smoke_ctx;

typedef enum {
    XWORK_PROVIDER_STUB_OPENAI = 0,
    XWORK_PROVIDER_STUB_ANTHROPIC,
    XWORK_PROVIDER_STUB_OLLAMA
} xwork_provider_stub_kind;

typedef struct {
    SOCKET hListener;
    HANDLE hThread;
    unsigned short uPort;
    volatile LONG iRequestCount;
    xwork_provider_stub_kind eKind;
    const char *sModelId;
    int iForceHttpStatusCode;
    const char *sForceHttpStatusText;
    const char *sForceResponseBody;
    char *asRequestTargets[2];
    char *asRequestHeaders[2];
    char *asRequestBodies[2];
    char sError[256];
} xwork_provider_smoke_stub;

static const xllm_profile *xwork_test_find_runtime_profile(
    const xwork_runtime *pRuntime,
    const char *sProfileId
)
{
    if ( !pRuntime || !sProfileId || !sProfileId[0] ) {
        return NULL;
    }

    return xllm__runtime_find_profile(
        xwork_runtime_get_llm_runtime(pRuntime),
        sProfileId
    );
}

static const char *xwork_test_env_non_empty(const char *sName)
{
    const char *sValue;

    if ( !sName || !sName[0] ) {
        return NULL;
    }

    sValue = getenv(sName);
    return (sValue && sValue[0]) ? sValue : NULL;
}

static bool xwork_test_ascii_case_equal(const char *sLeft, const char *sRight)
{
    if ( !sLeft || !sRight ) {
        return false;
    }

    while ( *sLeft && *sRight ) {
        if ( tolower((unsigned char)*sLeft) != tolower((unsigned char)*sRight) ) {
            return false;
        }
        ++sLeft;
        ++sRight;
    }

    return *sLeft == '\0' && *sRight == '\0';
}

static bool xwork_test_parse_size_t(const char *sText, size_t *piValue)
{
    unsigned long long iValue;
    char *sEnd;

    if ( !sText || !sText[0] || !piValue ) {
        return false;
    }

    errno = 0;
    iValue = strtoull(sText, &sEnd, 10);
    if ( errno != 0 || !sEnd || sEnd == sText || *sEnd != '\0' ) {
        return false;
    }
    if ( iValue > (unsigned long long)((size_t)-1) ) {
        return false;
    }

    *piValue = (size_t)iValue;
    return true;
}

static bool xwork_test_parse_bool(const char *sText, bool *pbValue)
{
    if ( !sText || !sText[0] || !pbValue ) {
        return false;
    }

    if ( xwork_test_ascii_case_equal(sText, "1") ||
         xwork_test_ascii_case_equal(sText, "true") ||
         xwork_test_ascii_case_equal(sText, "yes") ||
         xwork_test_ascii_case_equal(sText, "on") ) {
        *pbValue = true;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "0") ||
         xwork_test_ascii_case_equal(sText, "false") ||
         xwork_test_ascii_case_equal(sText, "no") ||
         xwork_test_ascii_case_equal(sText, "off") ) {
        *pbValue = false;
        return true;
    }

    return false;
}

static bool xwork_test_parse_debug_mode(
    const char *sText,
    xwork_xllm_debug_mode *peValue
)
{
    if ( !sText || !sText[0] || !peValue ) {
        return false;
    }

    if ( xwork_test_ascii_case_equal(sText, "none") ) {
        *peValue = XWORK_XLLM_DEBUG_NONE;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "headers") ) {
        *peValue = XWORK_XLLM_DEBUG_HEADERS;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "body") ) {
        *peValue = XWORK_XLLM_DEBUG_BODY;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "wire") ) {
        *peValue = XWORK_XLLM_DEBUG_WIRE;
        return true;
    }

    return false;
}

static bool xwork_test_parse_redact_mode(
    const char *sText,
    xwork_xllm_redact_mode *peValue
)
{
    if ( !sText || !sText[0] || !peValue ) {
        return false;
    }

    if ( xwork_test_ascii_case_equal(sText, "default") ) {
        *peValue = XWORK_XLLM_REDACT_DEFAULT;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "off") ) {
        *peValue = XWORK_XLLM_REDACT_OFF;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "strict") ) {
        *peValue = XWORK_XLLM_REDACT_STRICT;
        return true;
    }

    return false;
}

static bool xwork_test_parse_proxy_kind(
    const char *sText,
    xwork_xllm_proxy_kind *peValue
)
{
    if ( !sText || !sText[0] || !peValue ) {
        return false;
    }

    if ( xwork_test_ascii_case_equal(sText, "unspecified") ) {
        *peValue = XWORK_XLLM_PROXY_UNSPECIFIED;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "none") ) {
        *peValue = XWORK_XLLM_PROXY_NONE;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "socks5") ) {
        *peValue = XWORK_XLLM_PROXY_SOCKS5;
        return true;
    }
    if ( xwork_test_ascii_case_equal(sText, "http_connect") ||
         xwork_test_ascii_case_equal(sText, "http-connect") ||
         xwork_test_ascii_case_equal(sText, "httpconnect") ) {
        *peValue = XWORK_XLLM_PROXY_HTTP_CONNECT;
        return true;
    }

    return false;
}

static bool xwork_test_provider_stub_kind_for_adapter(
    const char *sAdapter,
    xwork_provider_stub_kind *peKind
)
{
    if ( !sAdapter || !sAdapter[0] || !peKind ) {
        return false;
    }

    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_OPENAI_COMPAT) == 0 ) {
        *peKind = XWORK_PROVIDER_STUB_OPENAI;
        return true;
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0 ) {
        *peKind = XWORK_PROVIDER_STUB_ANTHROPIC;
        return true;
    }
    if ( strcmp(sAdapter, XWORK_XLLM_ADAPTER_OLLAMA_NATIVE) == 0 ) {
        *peKind = XWORK_PROVIDER_STUB_OLLAMA;
        return true;
    }

    return false;
}

static char *xwork_test_dup_n(const char *sText, size_t iLen)
{
    char *sCopy;

    sCopy = (char *)calloc(iLen + 1u, sizeof(char));
    if ( !sCopy ) {
        return NULL;
    }
    if ( sText && iLen > 0u ) {
        memcpy(sCopy, sText, iLen);
    }
    sCopy[iLen] = '\0';
    return sCopy;
}

static void xwork_test_stub_set_error(
    xwork_provider_smoke_stub *pStub,
    const char *sError
)
{
    if ( !pStub || pStub->sError[0] ) {
        return;
    }

    (void)snprintf(
        pStub->sError,
        sizeof(pStub->sError),
        "%s",
        (sError && sError[0]) ? sError : "stub error"
    );
}

static bool xwork_test_socket_send_all(
    SOCKET hSocket,
    const char *pData,
    size_t iLen
)
{
    size_t iSentTotal = 0u;

    if ( !pData && iLen != 0u ) {
        return false;
    }

    while ( iSentTotal < iLen ) {
        size_t iRemaining = iLen - iSentTotal;
        int iChunk = (int)((iRemaining > (size_t)INT_MAX) ? INT_MAX : iRemaining);
        int iSent = send(hSocket, pData + iSentTotal, iChunk, 0);

        if ( iSent <= 0 ) {
            return false;
        }
        iSentTotal += (size_t)iSent;
    }

    return true;
}

static bool xwork_test_recv_http_request(
    SOCKET hSocket,
    char **ppsTarget,
    char **ppsHeaders,
    char **ppsBody
)
{
    char *sBuffer = NULL;
    size_t iCap = 65536u;
    size_t iLen = 0u;
    size_t iContentLength = 0u;
    size_t iTotalNeeded = 0u;
    char *sHeadersEnd;
    char *sRequestLineEnd;
    char *sFirstSpace;
    char *sSecondSpace;
    char *sContentLengthHeader;
    char *sBodyStart;

    if ( ppsTarget ) {
        *ppsTarget = NULL;
    }
    if ( ppsHeaders ) {
        *ppsHeaders = NULL;
    }
    if ( ppsBody ) {
        *ppsBody = NULL;
    }
    if ( !ppsTarget || !ppsHeaders || !ppsBody ) {
        return false;
    }

    sBuffer = (char *)calloc(iCap + 1u, sizeof(char));
    if ( !sBuffer ) {
        return false;
    }

    for (;;) {
        int iReceived;

        if ( iLen >= iCap ) {
            free(sBuffer);
            return false;
        }

        iReceived = recv(hSocket, sBuffer + iLen, (int)(iCap - iLen), 0);
        if ( iReceived <= 0 ) {
            free(sBuffer);
            return false;
        }

        iLen += (size_t)iReceived;
        sBuffer[iLen] = '\0';

        sHeadersEnd = strstr(sBuffer, "\r\n\r\n");
        if ( !sHeadersEnd ) {
            continue;
        }

        sContentLengthHeader = strstr(sBuffer, "\r\nContent-Length:");
        if ( !sContentLengthHeader &&
             strncmp(sBuffer, "Content-Length:", strlen("Content-Length:")) == 0 ) {
            sContentLengthHeader = sBuffer;
        }
        if ( sContentLengthHeader ) {
            char *sNumberStart;
            char *sNumberEnd;
            char *sNumberText;

            if ( sContentLengthHeader != sBuffer ) {
                sContentLengthHeader += 2;
            }
            sNumberStart = sContentLengthHeader + strlen("Content-Length:");
            while ( *sNumberStart == ' ' || *sNumberStart == '\t' ) {
                ++sNumberStart;
            }
            sNumberEnd = strstr(sNumberStart, "\r\n");
            if ( !sNumberEnd ) {
                free(sBuffer);
                return false;
            }
            sNumberText = xwork_test_dup_n(sNumberStart, (size_t)(sNumberEnd - sNumberStart));
            if ( !sNumberText ) {
                free(sBuffer);
                return false;
            }
            if ( !xwork_test_parse_size_t(sNumberText, &iContentLength) ) {
                free(sNumberText);
                free(sBuffer);
                return false;
            }
            free(sNumberText);
        }

        iTotalNeeded = (size_t)((sHeadersEnd + 4) - sBuffer) + iContentLength;
        if ( iLen >= iTotalNeeded ) {
            break;
        }
    }

    sRequestLineEnd = strstr(sBuffer, "\r\n");
    if ( !sRequestLineEnd ) {
        free(sBuffer);
        return false;
    }
    sFirstSpace = strchr(sBuffer, ' ');
    if ( !sFirstSpace || sFirstSpace > sRequestLineEnd ) {
        free(sBuffer);
        return false;
    }
    sSecondSpace = strchr(sFirstSpace + 1, ' ');
    if ( !sSecondSpace || sSecondSpace > sRequestLineEnd ) {
        free(sBuffer);
        return false;
    }
    if ( (size_t)(sFirstSpace - sBuffer) != strlen("POST") ||
         strncmp(sBuffer, "POST", strlen("POST")) != 0 ) {
        free(sBuffer);
        return false;
    }

    *ppsTarget = xwork_test_dup_n(
        sFirstSpace + 1,
        (size_t)(sSecondSpace - (sFirstSpace + 1))
    );
    if ( !*ppsTarget ) {
        free(sBuffer);
        return false;
    }

    sHeadersEnd = strstr(sBuffer, "\r\n\r\n");
    *ppsHeaders = xwork_test_dup_n(sBuffer, (size_t)(sHeadersEnd - sBuffer));
    if ( !*ppsHeaders ) {
        free(*ppsTarget);
        *ppsTarget = NULL;
        free(sBuffer);
        return false;
    }
    sBodyStart = sHeadersEnd ? (sHeadersEnd + 4) : NULL;
    *ppsBody = xwork_test_dup_n(sBodyStart, iContentLength);
    free(sBuffer);
    if ( !*ppsBody ) {
        free(*ppsHeaders);
        *ppsHeaders = NULL;
        free(*ppsTarget);
        *ppsTarget = NULL;
        return false;
    }

    return true;
}

static DWORD WINAPI xwork_test_provider_stub_thread(LPVOID pParam)
{
    xwork_provider_smoke_stub *pStub = (xwork_provider_smoke_stub *)pParam;

    if ( !pStub ) {
        return 1u;
    }

    for (;;) {
        SOCKET hClient = accept(pStub->hListener, NULL, NULL);
        char *sTarget = NULL;
        char *sRequestHeaders = NULL;
        char *sBody = NULL;
        char sResponseBody[2048];
        char sHeaders[256];
        size_t iRequestIndex;
        int iHttpStatusCode = 200;
        const char *sHttpStatusText = "OK";
        bool bStopAfterResponse = false;
        bool bStoredTarget = false;
        bool bStoredHeaders = false;
        bool bStoredBody = false;
        int iHeaderLen;
        int iBodyLen = 0;

        if ( hClient == INVALID_SOCKET ) {
            if ( pStub->hListener == INVALID_SOCKET ) {
                return 0u;
            }
            xwork_test_stub_set_error(pStub, "accept failed");
            return 1u;
        }

        if ( !xwork_test_recv_http_request(hClient, &sTarget, &sRequestHeaders, &sBody) ) {
            xwork_test_stub_set_error(pStub, "failed to read HTTP request");
            closesocket(hClient);
            return 1u;
        }

        iRequestIndex = (size_t)InterlockedExchangeAdd(&pStub->iRequestCount, 1);
        if ( iRequestIndex < 2u ) {
            pStub->asRequestTargets[iRequestIndex] = sTarget;
            pStub->asRequestHeaders[iRequestIndex] = sRequestHeaders;
            pStub->asRequestBodies[iRequestIndex] = sBody;
            bStoredTarget = true;
            bStoredHeaders = true;
            bStoredBody = true;
        }

        if ( pStub->iForceHttpStatusCode > 0 ) {
            iHttpStatusCode = pStub->iForceHttpStatusCode;
            sHttpStatusText = pStub->sForceHttpStatusText ?
                pStub->sForceHttpStatusText :
                "Internal Server Error";
            iBodyLen = snprintf(
                sResponseBody,
                sizeof(sResponseBody),
                "%s",
                pStub->sForceResponseBody ?
                    pStub->sForceResponseBody :
                    "{\"error\":{\"message\":\"forced provider failure\"}}"
            );
            bStopAfterResponse = true;
        } else {
            switch ( pStub->eKind ) {
                case XWORK_PROVIDER_STUB_OPENAI:
                    if ( !sTarget || strstr(sTarget, "/chat/completions") == NULL ) {
                        xwork_test_stub_set_error(pStub, "unexpected openai-compatible request target");
                        if ( !bStoredTarget ) {
                            free(sTarget);
                        }
                        if ( !bStoredHeaders ) {
                            free(sRequestHeaders);
                        }
                        if ( !bStoredBody ) {
                            free(sBody);
                        }
                        closesocket(hClient);
                        return 1u;
                    }
                    if ( iRequestIndex == 0u ) {
                        if ( !sBody ||
                             strstr(sBody, "\"mock.echo\"") == NULL ||
                             strstr(sBody, "provider smoke") == NULL ||
                             strstr(sBody, "\"tools\"") == NULL ) {
                            xwork_test_stub_set_error(
                                pStub,
                                "openai-compatible first request missing tool call setup"
                            );
                            if ( !bStoredTarget ) {
                                free(sTarget);
                            }
                            if ( !bStoredHeaders ) {
                                free(sRequestHeaders);
                            }
                            if ( !bStoredBody ) {
                                free(sBody);
                            }
                            closesocket(hClient);
                            return 1u;
                        }
                        iBodyLen = snprintf(
                            sResponseBody,
                            sizeof(sResponseBody),
                            "{\"id\":\"stub-tool-1\",\"object\":\"chat.completion\",\"created\":1,"
                            "\"model\":\"%s\",\"choices\":[{\"index\":0,\"message\":{"
                            "\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{\"id\":\"call_mock_echo_1\","
                            "\"type\":\"function\",\"function\":{\"name\":\"mock.echo\",\"arguments\":"
                            "\"{\\\"payload\\\":\\\"provider smoke\\\"}\"}}]},"
                            "\"finish_reason\":\"tool_calls\"}]}",
                            pStub->sModelId ? pStub->sModelId : "stub-openai-model"
                        );
                    } else if ( iRequestIndex == 1u ) {
                        if ( !sBody ||
                             strstr(sBody, "\"tool_call_id\":\"call_mock_echo_1\"") == NULL ||
                             strstr(sBody, "provider smoke") == NULL ) {
                            xwork_test_stub_set_error(
                                pStub,
                                "openai-compatible second request missing tool result followup"
                            );
                            if ( !bStoredTarget ) {
                                free(sTarget);
                            }
                            if ( !bStoredHeaders ) {
                                free(sRequestHeaders);
                            }
                            if ( !bStoredBody ) {
                                free(sBody);
                            }
                            closesocket(hClient);
                            return 1u;
                        }
                        iBodyLen = snprintf(
                            sResponseBody,
                            sizeof(sResponseBody),
                            "{\"id\":\"stub-final-2\",\"object\":\"chat.completion\",\"created\":2,"
                            "\"model\":\"%s\",\"choices\":[{\"index\":0,\"message\":{"
                            "\"role\":\"assistant\",\"content\":\"PROVIDER_SMOKE_COMPLETE\"},"
                            "\"finish_reason\":\"stop\"}]}",
                            pStub->sModelId ? pStub->sModelId : "stub-openai-model"
                        );
                    } else {
                        xwork_test_stub_set_error(
                            pStub,
                            "openai-compatible stub received unexpected extra request"
                        );
                        if ( !bStoredTarget ) {
                            free(sTarget);
                        }
                        if ( !bStoredHeaders ) {
                            free(sRequestHeaders);
                        }
                        if ( !bStoredBody ) {
                            free(sBody);
                        }
                        closesocket(hClient);
                        return 1u;
                    }
                    break;

            case XWORK_PROVIDER_STUB_ANTHROPIC:
                if ( !sTarget || strstr(sTarget, "/messages") == NULL ) {
                    xwork_test_stub_set_error(pStub, "unexpected anthropic-native request target");
                    if ( !bStoredTarget ) {
                        free(sTarget);
                    }
                    if ( !bStoredHeaders ) {
                        free(sRequestHeaders);
                    }
                    if ( !bStoredBody ) {
                        free(sBody);
                    }
                    closesocket(hClient);
                    return 1u;
                }
                if ( iRequestIndex == 0u ) {
                    if ( !sBody ||
                         strstr(sBody, "\"tools\"") == NULL ||
                         strstr(sBody, "\"mock.echo\"") == NULL ||
                         strstr(sBody, "provider smoke") == NULL ||
                         strstr(sBody, "\"max_tokens\"") == NULL ) {
                        xwork_test_stub_set_error(
                            pStub,
                            "anthropic-native first request missing tool call setup"
                        );
                        if ( !bStoredTarget ) {
                            free(sTarget);
                        }
                        if ( !bStoredHeaders ) {
                            free(sRequestHeaders);
                        }
                        if ( !bStoredBody ) {
                            free(sBody);
                        }
                        closesocket(hClient);
                        return 1u;
                    }
                    iBodyLen = snprintf(
                        sResponseBody,
                        sizeof(sResponseBody),
                        "{\"id\":\"msg_stub_1\",\"type\":\"message\",\"role\":\"assistant\","
                        "\"model\":\"%s\",\"content\":[{\"type\":\"tool_use\",\"id\":\"call_mock_echo_1\","
                        "\"name\":\"mock.echo\",\"input\":{\"payload\":\"provider smoke\"}}],"
                        "\"stop_reason\":\"tool_use\",\"usage\":{\"input_tokens\":1,\"output_tokens\":1}}",
                        pStub->sModelId ? pStub->sModelId : "stub-claude-model"
                    );
                } else if ( iRequestIndex == 1u ) {
                    if ( !sBody ||
                         strstr(sBody, "\"type\":\"tool_result\"") == NULL ||
                         strstr(sBody, "\"tool_use_id\":\"call_mock_echo_1\"") == NULL ||
                         strstr(sBody, "provider smoke") == NULL ) {
                        xwork_test_stub_set_error(
                            pStub,
                            "anthropic-native second request missing tool result followup"
                        );
                        if ( !bStoredTarget ) {
                            free(sTarget);
                        }
                        if ( !bStoredHeaders ) {
                            free(sRequestHeaders);
                        }
                        if ( !bStoredBody ) {
                            free(sBody);
                        }
                        closesocket(hClient);
                        return 1u;
                    }
                    iBodyLen = snprintf(
                        sResponseBody,
                        sizeof(sResponseBody),
                        "{\"id\":\"msg_stub_2\",\"type\":\"message\",\"role\":\"assistant\","
                        "\"model\":\"%s\",\"content\":[{\"type\":\"text\",\"text\":\"PROVIDER_SMOKE_COMPLETE\"}],"
                        "\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":1,\"output_tokens\":1}}",
                        pStub->sModelId ? pStub->sModelId : "stub-claude-model"
                    );
                } else {
                    xwork_test_stub_set_error(
                        pStub,
                        "anthropic-native stub received unexpected extra request"
                    );
                    if ( !bStoredTarget ) {
                        free(sTarget);
                    }
                    if ( !bStoredHeaders ) {
                        free(sRequestHeaders);
                    }
                    if ( !bStoredBody ) {
                        free(sBody);
                    }
                    closesocket(hClient);
                    return 1u;
                }
                break;

            case XWORK_PROVIDER_STUB_OLLAMA:
                if ( !sTarget || strstr(sTarget, "/api/chat") == NULL ) {
                    xwork_test_stub_set_error(pStub, "unexpected ollama-native request target");
                    if ( !bStoredTarget ) {
                        free(sTarget);
                    }
                    if ( !bStoredHeaders ) {
                        free(sRequestHeaders);
                    }
                    if ( !bStoredBody ) {
                        free(sBody);
                    }
                    closesocket(hClient);
                    return 1u;
                }
                if ( iRequestIndex == 0u ) {
                    if ( !sBody ||
                         strstr(sBody, "\"tools\"") == NULL ||
                         strstr(sBody, "\"mock.echo\"") == NULL ||
                         strstr(sBody, "provider smoke") == NULL ) {
                        xwork_test_stub_set_error(
                            pStub,
                            "ollama-native first request missing tool call setup"
                        );
                        if ( !bStoredTarget ) {
                            free(sTarget);
                        }
                        if ( !bStoredHeaders ) {
                            free(sRequestHeaders);
                        }
                        if ( !bStoredBody ) {
                            free(sBody);
                        }
                        closesocket(hClient);
                        return 1u;
                    }
                    iBodyLen = snprintf(
                        sResponseBody,
                        sizeof(sResponseBody),
                        "{\"model\":\"%s\",\"created_at\":\"2026-04-01T00:00:00Z\","
                        "\"message\":{\"role\":\"assistant\",\"content\":\"\","
                        "\"tool_calls\":[{\"function\":{\"name\":\"mock.echo\","
                        "\"arguments\":{\"payload\":\"provider smoke\"}}}]},"
                        "\"done\":true,\"done_reason\":\"tool_calls\","
                        "\"prompt_eval_count\":1,\"eval_count\":1}",
                        pStub->sModelId ? pStub->sModelId : "stub-ollama-model"
                    );
                } else if ( iRequestIndex == 1u ) {
                    if ( !sBody ||
                         strstr(sBody, "\"role\":\"tool\"") == NULL ||
                         strstr(sBody, "\"tool_name\":\"mock.echo\"") == NULL ||
                         strstr(sBody, "provider smoke") == NULL ) {
                        xwork_test_stub_set_error(
                            pStub,
                            "ollama-native second request missing tool result followup"
                        );
                        if ( !bStoredTarget ) {
                            free(sTarget);
                        }
                        if ( !bStoredHeaders ) {
                            free(sRequestHeaders);
                        }
                        if ( !bStoredBody ) {
                            free(sBody);
                        }
                        closesocket(hClient);
                        return 1u;
                    }
                    iBodyLen = snprintf(
                        sResponseBody,
                        sizeof(sResponseBody),
                        "{\"model\":\"%s\",\"created_at\":\"2026-04-01T00:00:01Z\","
                        "\"message\":{\"role\":\"assistant\",\"content\":\"PROVIDER_SMOKE_COMPLETE\"},"
                        "\"done\":true,\"done_reason\":\"stop\","
                        "\"prompt_eval_count\":1,\"eval_count\":1}",
                        pStub->sModelId ? pStub->sModelId : "stub-ollama-model"
                    );
                } else {
                    xwork_test_stub_set_error(
                        pStub,
                        "ollama-native stub received unexpected extra request"
                    );
                    if ( !bStoredTarget ) {
                        free(sTarget);
                    }
                    if ( !bStoredHeaders ) {
                        free(sRequestHeaders);
                    }
                    if ( !bStoredBody ) {
                        free(sBody);
                    }
                    closesocket(hClient);
                    return 1u;
                }
                break;

            default:
                xwork_test_stub_set_error(pStub, "unsupported provider stub kind");
                if ( !bStoredTarget ) {
                    free(sTarget);
                }
                if ( !bStoredHeaders ) {
                    free(sRequestHeaders);
                }
                if ( !bStoredBody ) {
                    free(sBody);
                }
                closesocket(hClient);
                return 1u;
        }
        }

        if ( iBodyLen <= 0 || (size_t)iBodyLen >= sizeof(sResponseBody) ) {
            xwork_test_stub_set_error(pStub, "failed to build stub response body");
            if ( !bStoredTarget ) {
                free(sTarget);
            }
            if ( !bStoredHeaders ) {
                free(sRequestHeaders);
            }
            if ( !bStoredBody ) {
                free(sBody);
            }
            closesocket(hClient);
            return 1u;
        }

        iHeaderLen = snprintf(
            sHeaders,
            sizeof(sHeaders),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n",
            iHttpStatusCode,
            sHttpStatusText,
            (unsigned)iBodyLen
        );
        if ( iHeaderLen <= 0 || (size_t)iHeaderLen >= sizeof(sHeaders) ) {
            xwork_test_stub_set_error(pStub, "failed to build HTTP response headers");
            if ( !bStoredTarget ) {
                free(sTarget);
            }
            if ( !bStoredHeaders ) {
                free(sRequestHeaders);
            }
            if ( !bStoredBody ) {
                free(sBody);
            }
            closesocket(hClient);
            return 1u;
        }

        if ( !xwork_test_socket_send_all(hClient, sHeaders, (size_t)iHeaderLen) ||
             !xwork_test_socket_send_all(hClient, sResponseBody, (size_t)iBodyLen) ) {
            xwork_test_stub_set_error(pStub, "failed to send HTTP response");
            if ( !bStoredTarget ) {
                free(sTarget);
            }
            if ( !bStoredHeaders ) {
                free(sRequestHeaders);
            }
            if ( !bStoredBody ) {
                free(sBody);
            }
            closesocket(hClient);
            return 1u;
        }

        shutdown(hClient, SD_BOTH);
        closesocket(hClient);
        if ( !bStoredTarget ) {
            free(sTarget);
        }
        if ( !bStoredHeaders ) {
            free(sRequestHeaders);
        }
        if ( !bStoredBody ) {
            free(sBody);
        }

        if ( bStopAfterResponse || iRequestIndex >= 1u ) {
            return 0u;
        }
    }
}

static bool xwork_test_provider_stub_start(xwork_provider_smoke_stub *pStub)
{
    WSADATA tWsaData;
    SOCKET hListener;
    struct sockaddr_in tAddr;
    int iAddrLen;

    if ( !pStub ) {
        return false;
    }

    memset(pStub, 0, sizeof(*pStub));
    pStub->hListener = INVALID_SOCKET;

    if ( WSAStartup(MAKEWORD(2, 2), &tWsaData) != 0 ) {
        xwork_test_stub_set_error(pStub, "WSAStartup failed");
        return false;
    }

    hListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( hListener == INVALID_SOCKET ) {
        xwork_test_stub_set_error(pStub, "failed to create listener socket");
        return false;
    }

    memset(&tAddr, 0, sizeof(tAddr));
    tAddr.sin_family = AF_INET;
    tAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    tAddr.sin_port = htons(0u);
    if ( bind(hListener, (const struct sockaddr *)&tAddr, sizeof(tAddr)) != 0 ) {
        xwork_test_stub_set_error(pStub, "failed to bind listener socket");
        closesocket(hListener);
        return false;
    }
    if ( listen(hListener, 4) != 0 ) {
        xwork_test_stub_set_error(pStub, "failed to listen on stub socket");
        closesocket(hListener);
        return false;
    }

    iAddrLen = (int)sizeof(tAddr);
    if ( getsockname(hListener, (struct sockaddr *)&tAddr, &iAddrLen) != 0 ) {
        xwork_test_stub_set_error(pStub, "failed to resolve stub listener port");
        closesocket(hListener);
        return false;
    }

    pStub->uPort = ntohs(tAddr.sin_port);
    pStub->hListener = hListener;
    pStub->hThread = CreateThread(
        NULL,
        0u,
        xwork_test_provider_stub_thread,
        pStub,
        0u,
        NULL
    );
    if ( !pStub->hThread ) {
        xwork_test_stub_set_error(pStub, "failed to start stub thread");
        closesocket(pStub->hListener);
        pStub->hListener = INVALID_SOCKET;
        return false;
    }

    return true;
}

static void xwork_test_provider_stub_stop(xwork_provider_smoke_stub *pStub)
{
    size_t i;

    if ( !pStub ) {
        return;
    }

    if ( pStub->hListener != INVALID_SOCKET ) {
        shutdown(pStub->hListener, SD_BOTH);
        closesocket(pStub->hListener);
        pStub->hListener = INVALID_SOCKET;
    }

    if ( pStub->hThread ) {
        (void)WaitForSingleObject(pStub->hThread, 5000u);
        CloseHandle(pStub->hThread);
        pStub->hThread = NULL;
    }

    for ( i = 0u; i < 2u; ++i ) {
        free(pStub->asRequestTargets[i]);
        pStub->asRequestTargets[i] = NULL;
        free(pStub->asRequestHeaders[i]);
        pStub->asRequestHeaders[i] = NULL;
        free(pStub->asRequestBodies[i]);
        pStub->asRequestBodies[i] = NULL;
    }

    (void)WSACleanup();
}

static void xwork_test_trim_ascii_in_place(char *sText)
{
    char *sStart;
    char *sEnd;

    if ( !sText || !sText[0] ) {
        return;
    }

    sStart = sText;
    while ( *sStart == ' ' || *sStart == '\t' || *sStart == '\r' || *sStart == '\n' ) {
        ++sStart;
    }
    if ( sStart != sText ) {
        memmove(sText, sStart, strlen(sStart) + 1u);
    }

    sEnd = sText + strlen(sText);
    while ( sEnd > sText ) {
        char c = sEnd[-1];
        if ( c != ' ' && c != '\t' && c != '\r' && c != '\n' ) {
            break;
        }
        --sEnd;
    }
    *sEnd = '\0';
}

static bool xwork_test_split_csv(
    const char *sText,
    char ***ppsItems,
    size_t *piItemCount
)
{
    char *sBuffer;
    char **psItems;
    size_t iCapacity;
    size_t iCount;
    char *sCursor;

    if ( ppsItems ) {
        *ppsItems = NULL;
    }
    if ( piItemCount ) {
        *piItemCount = 0u;
    }
    if ( !sText || !sText[0] || !ppsItems || !piItemCount ) {
        return true;
    }

    sBuffer = (char *)calloc(strlen(sText) + 1u, sizeof(char));
    if ( !sBuffer ) {
        return false;
    }
    memcpy(sBuffer, sText, strlen(sText));

    iCapacity = 4u;
    psItems = (char **)calloc(iCapacity, sizeof(char *));
    if ( !psItems ) {
        free(sBuffer);
        return false;
    }

    iCount = 0u;
    sCursor = sBuffer;
    while ( sCursor && *sCursor ) {
        char *sNext = strchr(sCursor, ',');

        if ( sNext ) {
            *sNext = '\0';
        }
        xwork_test_trim_ascii_in_place(sCursor);
        if ( sCursor[0] ) {
            char *sItem;

            if ( iCount == iCapacity ) {
                char **psNewItems;

                iCapacity *= 2u;
                psNewItems = (char **)realloc(psItems, iCapacity * sizeof(char *));
                if ( !psNewItems ) {
                    size_t i;
                    for ( i = 0u; i < iCount; ++i ) {
                        free(psItems[i]);
                    }
                    free(psItems);
                    free(sBuffer);
                    return false;
                }
                psItems = psNewItems;
            }

            sItem = (char *)calloc(strlen(sCursor) + 1u, sizeof(char));
            if ( !sItem ) {
                size_t i;
                for ( i = 0u; i < iCount; ++i ) {
                    free(psItems[i]);
                }
                free(psItems);
                free(sBuffer);
                return false;
            }
            memcpy(sItem, sCursor, strlen(sCursor));
            psItems[iCount++] = sItem;
        }

        sCursor = sNext ? (sNext + 1) : NULL;
    }

    free(sBuffer);
    *ppsItems = psItems;
    *piItemCount = iCount;
    return true;
}

static void xwork_test_free_string_array(char **psItems, size_t iItemCount)
{
    size_t i;

    if ( !psItems ) {
        return;
    }
    for ( i = 0u; i < iItemCount; ++i ) {
        free(psItems[i]);
    }
    free(psItems);
}

static void xwork_test_trace_callback(
    void *pCtx,
    xllm_trace_kind eKind,
    const xvalue *pPayload
)
{
    xwork_provider_smoke_ctx *pState = (xwork_provider_smoke_ctx *)pCtx;

    (void)pPayload;

    if ( !pState ) {
        return;
    }

    switch ( eKind ) {
    case XLLM_TRACE_REQUEST:
        ++pState->iRequestTraceCount;
        break;
    case XLLM_TRACE_RESPONSE:
        ++pState->iResponseTraceCount;
        break;
    case XLLM_TRACE_STREAM:
        ++pState->iStreamTraceCount;
        break;
    case XLLM_TRACE_TOOL_LOOP:
        ++pState->iToolLoopTraceCount;
        break;
    case XLLM_TRACE_EVENT:
    case XLLM_TRACE_COMPACT:
    default:
        break;
    }
}

static xwork_status xwork_provider_smoke_tool_exec(
    xwork_run *pRun,
    const xwork_tool_call *pCall,
    xwork_tool_result *pResult,
    void *pUserData
)
{
    xwork_provider_smoke_ctx *pState = (xwork_provider_smoke_ctx *)pUserData;

    (void)pRun;

    if ( !pCall || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pCall->sToolId || strcmp(pCall->sToolId, "mock.echo") != 0 ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( !pCall->sArgumentsJson ||
         strstr(pCall->sArgumentsJson, "provider smoke") == NULL ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( pState ) {
        ++pState->iExecCount;
    }

    pResult->sOutputText = "{\"ok\":true,\"echo\":\"provider smoke\"}";
    pResult->sVisibleSummary = "mock.echo executed successfully.";
    pResult->bRetryable = false;
    return XWORK_OK;
}

static void xwork_test_run_local_provider_case(
    const xwork_profile *pProductProfile,
    const char *sAdapter,
    const char *sProvider,
    const char *sProfileId,
    const char *sRunId,
    const char *sModelId,
    const char *sApiKey,
    const char *sOpenAIOrganizationId,
    const char *sOpenAIProjectId,
    const char *sAnthropicApiVersion,
    const char **psAnthropicBetaHeaders,
    size_t iAnthropicBetaHeaderCount
)
{
    xwork_provider_stub_kind eStubKind;
    xwork_provider_smoke_stub tStub;
    xwork_provider_smoke_ctx tSmokeCtx;
    xwork_xllm_profile_options tProviderProfile;
    xwork_xllm_bootstrap_options tProviderBootstrap;
    xwork_runtime_options tRuntimeOptions;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace_options tWorkspaceOptions;
    xwork_workspace *pWorkspace = NULL;
    xwork_tool_def tToolDef;
    xwork_run_options tRunOptions;
    xwork_run *pRun = NULL;
    xwork_orchestrator_options tExecOptions;
    xwork_event tEvent;
    xwork_status iExecuteStatus;
    const char *asWorkspaceIds[1];
    char sBaseUrl[128];

    assert(pProductProfile != NULL);
    assert(sAdapter != NULL && sAdapter[0] != '\0');
    assert(sProvider != NULL && sProvider[0] != '\0');
    assert(sProfileId != NULL && sProfileId[0] != '\0');
    assert(sRunId != NULL && sRunId[0] != '\0');
    assert(sModelId != NULL && sModelId[0] != '\0');
    assert(xwork_test_provider_stub_kind_for_adapter(sAdapter, &eStubKind));

    memset(&tStub, 0, sizeof(tStub));
    tStub.hListener = INVALID_SOCKET;
    memset(&tSmokeCtx, 0, sizeof(tSmokeCtx));
    memset(sBaseUrl, 0, sizeof(sBaseUrl));

    assert(xwork_test_provider_stub_start(&tStub));
    tStub.eKind = eStubKind;
    tStub.sModelId = sModelId;
    assert(tStub.sError[0] == '\0');
    assert(tStub.uPort != 0u);

    if ( eStubKind == XWORK_PROVIDER_STUB_OLLAMA ) {
        (void)snprintf(
            sBaseUrl,
            sizeof(sBaseUrl),
            "http://127.0.0.1:%u",
            (unsigned)tStub.uPort
        );
    } else {
        (void)snprintf(
            sBaseUrl,
            sizeof(sBaseUrl),
            "http://127.0.0.1:%u/v1",
            (unsigned)tStub.uPort
        );
    }

    xwork_runtime_options_init(&tRuntimeOptions);
    assert(xwork_profile_apply_runtime_options(pProductProfile, &tRuntimeOptions) == XWORK_OK);
    xwork_xllm_profile_options_init(&tProviderProfile);
    xwork_xllm_bootstrap_options_init(&tProviderBootstrap);
    assert(
        xwork_profile_apply_xllm_bootstrap_options(
            pProductProfile,
            &tProviderProfile,
            &tProviderBootstrap
        ) == XWORK_OK
    );
    tProviderProfile.sProfileId = sProfileId;
    tProviderProfile.sDisplayName = sProfileId;
    tProviderProfile.sProvider = sProvider;
    tProviderProfile.sAdapter = sAdapter;
    tProviderProfile.sBaseUrl = sBaseUrl;
    tProviderProfile.sModelId = sModelId;
    tProviderProfile.sApiKey = sApiKey;
    tProviderProfile.sOpenAIOrganizationId = sOpenAIOrganizationId;
    tProviderProfile.sOpenAIProjectId = sOpenAIProjectId;
    tProviderProfile.sAnthropicApiVersion = sAnthropicApiVersion;
    tProviderProfile.psAnthropicBetaHeaders = psAnthropicBetaHeaders;
    tProviderProfile.iAnthropicBetaHeaderCount = iAnthropicBetaHeaderCount;
    tProviderProfile.iMaxOutputTokens = 256u;
    tRuntimeOptions.pLlmBootstrap = &tProviderBootstrap;

    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);
    assert(xwork_runtime_get_llm_runtime(pRuntime) != NULL);
    assert(
        xllm_runtime_set_trace_callback(
            xwork_runtime_get_llm_runtime(pRuntime),
            xwork_test_trace_callback,
            &tSmokeCtx
        ) == XRT_NET_OK
    );

    xwork_workspace_options_init(&tWorkspaceOptions);
    tWorkspaceOptions.sWorkspaceId = "main";
    tWorkspaceOptions.sRootPath = ".";
    assert(xwork_runtime_add_workspace(pRuntime, &tWorkspaceOptions, &pWorkspace) == XWORK_OK);
    assert(pWorkspace != NULL);

    xwork_tool_def_init(&tToolDef);
    tToolDef.sToolId = "mock.echo";
    tToolDef.sDisplayName = "Mock Echo";
    tToolDef.sDescription = "Echo a small payload for provider smoke validation.";
    tToolDef.eKind = XWORK_TOOL_VIRTUAL;
    tToolDef.eSideEffect = XWORK_SIDE_EFFECT_READ_ONLY;
    tToolDef.eApprovalMode = XWORK_APPROVAL_NEVER;
    assert(xwork_runtime_register_tool(pRuntime, &tToolDef) == XWORK_OK);

    asWorkspaceIds[0] = "main";
    xwork_run_options_init(&tRunOptions);
    assert(xwork_profile_apply_run_options(pProductProfile, &tRunOptions) == XWORK_OK);
    tRunOptions.sRunId = sRunId;
    tRunOptions.sInstruction =
        "Follow these rules exactly:\n"
        "1. Call the tool `mock.echo` exactly once.\n"
        "2. Pass a JSON object that contains the text `provider smoke`.\n"
        "3. After the tool result arrives, respond with the exact token "
        "`PROVIDER_SMOKE_COMPLETE` and nothing else.";
    tRunOptions.sLlmProfileId = sProfileId;
    tRunOptions.sSessionProfileId = sProfileId;
    tRunOptions.psWorkspaceIds = asWorkspaceIds;
    tRunOptions.iWorkspaceCount = 1u;
    assert(xwork_run_create(pRuntime, &tRunOptions, &pRun) == XWORK_OK);

    xwork_orchestrator_options_init(&tExecOptions);
    assert(
        xwork_profile_apply_orchestrator_options(
            pProductProfile,
            &tExecOptions
        ) == XWORK_OK
    );
    tExecOptions.pfnToolExec = xwork_provider_smoke_tool_exec;
    tExecOptions.pUserData = &tSmokeCtx;
    tExecOptions.bAutoApprove = true;
    iExecuteStatus = xwork_run_execute(pRun, &tExecOptions);
    if ( iExecuteStatus != XWORK_OK ) {
        fprintf(
            stderr,
            "local provider case failed: adapter=%s status=%d stub_error=%s request_count=%ld\n",
            sAdapter,
            (int)iExecuteStatus,
            tStub.sError[0] ? tStub.sError : "<none>",
            (long)tStub.iRequestCount
        );
        if ( tStub.asRequestTargets[0] ) {
            fprintf(stderr, "target0=%s\n", tStub.asRequestTargets[0]);
        }
        if ( tStub.asRequestBodies[0] ) {
            fprintf(stderr, "body0=%s\n", tStub.asRequestBodies[0]);
        }
        if ( tStub.asRequestTargets[1] ) {
            fprintf(stderr, "target1=%s\n", tStub.asRequestTargets[1]);
        }
        if ( tStub.asRequestBodies[1] ) {
            fprintf(stderr, "body1=%s\n", tStub.asRequestBodies[1]);
        }
    }
    assert(iExecuteStatus == XWORK_OK);

    assert(xwork_run_get_state(pRun) == XWORK_RUN_COMPLETED);
    assert(tSmokeCtx.iExecCount == 1u);
    assert(tSmokeCtx.iRequestTraceCount >= 2u);
    assert(tSmokeCtx.iResponseTraceCount >= 2u);
    assert(xwork_run_get_last_output_text(pRun) != NULL);
    assert(
        strstr(
            xwork_run_get_last_output_text(pRun),
            "PROVIDER_SMOKE_COMPLETE"
        ) != NULL
    );

    xwork_event_init(&tEvent);
    assert(xwork_run_get_last_event(pRun, &tEvent) == XWORK_OK);
    assert(tEvent.eKind == XWORK_EVENT_RUN_COMPLETED);
    assert(xwork_run_get_artifact_count(pRun) == 0u);
    xwork_event_reset(&tEvent);

    xwork_run_destroy(pRun);
    pRun = NULL;
    xwork_runtime_destroy(pRuntime);
    pRuntime = NULL;

    assert(tStub.sError[0] == '\0');
    assert((size_t)tStub.iRequestCount == 2u);
    assert(tStub.asRequestTargets[0] != NULL);
    assert(tStub.asRequestTargets[1] != NULL);
    assert(tStub.asRequestHeaders[0] != NULL);
    assert(tStub.asRequestHeaders[1] != NULL);
    assert(tStub.asRequestBodies[0] != NULL);
    assert(tStub.asRequestBodies[1] != NULL);
    assert(strstr(tStub.asRequestHeaders[0], "\r\nAccept: application/json") != NULL);
    assert(strstr(tStub.asRequestHeaders[1], "\r\nAccept: application/json") != NULL);
    assert(strstr(tStub.asRequestHeaders[0], "\r\nUser-Agent: xllm/0.1.0") != NULL);
    assert(strstr(tStub.asRequestHeaders[1], "\r\nUser-Agent: xllm/0.1.0") != NULL);

    switch ( eStubKind ) {
        case XWORK_PROVIDER_STUB_OPENAI:
            assert(strstr(tStub.asRequestTargets[0], "/chat/completions") != NULL);
            assert(strstr(tStub.asRequestTargets[1], "/chat/completions") != NULL);
            assert(strstr(tStub.asRequestBodies[0], "\"tools\"") != NULL);
            assert(strstr(tStub.asRequestBodies[1], "\"tool_call_id\":\"call_mock_echo_1\"") != NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nAuthorization: Bearer stub-openai-key") != NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nAuthorization: Bearer stub-openai-key") != NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nOpenAI-Organization: org-local-openai") != NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nOpenAI-Organization: org-local-openai") != NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nOpenAI-Project: proj-local-openai") != NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nOpenAI-Project: proj-local-openai") != NULL);
            break;
        case XWORK_PROVIDER_STUB_ANTHROPIC:
            assert(strstr(tStub.asRequestTargets[0], "/messages") != NULL);
            assert(strstr(tStub.asRequestTargets[1], "/messages") != NULL);
            assert(strstr(tStub.asRequestBodies[0], "\"tools\"") != NULL);
            assert(strstr(tStub.asRequestBodies[1], "\"type\":\"tool_result\"") != NULL);
            assert(strstr(tStub.asRequestBodies[1], "\"tool_use_id\":\"call_mock_echo_1\"") != NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nx-api-key: stub-anthropic-key") != NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nx-api-key: stub-anthropic-key") != NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nanthropic-version: 2023-06-01") != NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nanthropic-version: 2023-06-01") != NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nanthropic-beta: beta-tools-2024-04-04") != NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nanthropic-beta: beta-tools-2024-04-04") != NULL);
            break;
        case XWORK_PROVIDER_STUB_OLLAMA:
            assert(strstr(tStub.asRequestTargets[0], "/api/chat") != NULL);
            assert(strstr(tStub.asRequestTargets[1], "/api/chat") != NULL);
            assert(strstr(tStub.asRequestBodies[0], "\"tools\"") != NULL);
            assert(strstr(tStub.asRequestBodies[1], "\"role\":\"tool\"") != NULL);
            assert(strstr(tStub.asRequestBodies[1], "\"tool_name\":\"mock.echo\"") != NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nAuthorization: ") == NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nAuthorization: ") == NULL);
            assert(strstr(tStub.asRequestHeaders[0], "\r\nx-api-key: ") == NULL);
            assert(strstr(tStub.asRequestHeaders[1], "\r\nx-api-key: ") == NULL);
            break;
    }
    assert(strstr(tStub.asRequestBodies[1], "provider smoke") != NULL);
    xwork_test_provider_stub_stop(&tStub);
}

static void xwork_test_run_local_provider_failure_case(
    const xwork_profile *pProductProfile
)
{
    xwork_provider_smoke_stub tStub;
    xwork_provider_smoke_ctx tSmokeCtx;
    xwork_xllm_profile_options tProviderProfile;
    xwork_xllm_bootstrap_options tProviderBootstrap;
    xwork_runtime_options tRuntimeOptions;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace_options tWorkspaceOptions;
    xwork_workspace *pWorkspace = NULL;
    xwork_tool_def tToolDef;
    xwork_run_options tRunOptions;
    xwork_run *pRun = NULL;
    xwork_run_async *pAsync = NULL;
    xwork_orchestrator_options tExecOptions;
    xwork_event tEvent;
    xwork_status iAsyncStatus;
    bool bAsyncCompleted = false;
    const char *asWorkspaceIds[1];
    char sBaseUrl[128];

    assert(pProductProfile != NULL);

    memset(&tStub, 0, sizeof(tStub));
    tStub.hListener = INVALID_SOCKET;
    memset(&tSmokeCtx, 0, sizeof(tSmokeCtx));
    memset(sBaseUrl, 0, sizeof(sBaseUrl));

    assert(xwork_test_provider_stub_start(&tStub));
    tStub.eKind = XWORK_PROVIDER_STUB_OPENAI;
    tStub.sModelId = "stub-openai-model";
    tStub.iForceHttpStatusCode = 500;
    tStub.sForceHttpStatusText = "Internal Server Error";
    tStub.sForceResponseBody = "{\"error\":{\"message\":\"forced provider failure\"}}";
    assert(tStub.uPort != 0u);

    (void)snprintf(
        sBaseUrl,
        sizeof(sBaseUrl),
        "http://127.0.0.1:%u/v1",
        (unsigned)tStub.uPort
    );

    xwork_runtime_options_init(&tRuntimeOptions);
    assert(xwork_profile_apply_runtime_options(pProductProfile, &tRuntimeOptions) == XWORK_OK);
    xwork_xllm_profile_options_init(&tProviderProfile);
    xwork_xllm_bootstrap_options_init(&tProviderBootstrap);
    assert(
        xwork_profile_apply_xllm_bootstrap_options(
            pProductProfile,
            &tProviderProfile,
            &tProviderBootstrap
        ) == XWORK_OK
    );
    tProviderProfile.sProfileId = "provider-local-openai-failure";
    tProviderProfile.sDisplayName = "provider-local-openai-failure";
    tProviderProfile.sProvider = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
    tProviderProfile.sAdapter = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
    tProviderProfile.sBaseUrl = sBaseUrl;
    tProviderProfile.sModelId = "stub-openai-model";
    tProviderProfile.sApiKey = "stub-openai-key";
    tProviderProfile.iMaxOutputTokens = 256u;
    tRuntimeOptions.pLlmBootstrap = &tProviderBootstrap;

    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);
    assert(xwork_runtime_get_llm_runtime(pRuntime) != NULL);
    assert(
        xllm_runtime_set_trace_callback(
            xwork_runtime_get_llm_runtime(pRuntime),
            xwork_test_trace_callback,
            &tSmokeCtx
        ) == XRT_NET_OK
    );

    xwork_workspace_options_init(&tWorkspaceOptions);
    tWorkspaceOptions.sWorkspaceId = "main";
    tWorkspaceOptions.sRootPath = ".";
    assert(xwork_runtime_add_workspace(pRuntime, &tWorkspaceOptions, &pWorkspace) == XWORK_OK);
    assert(pWorkspace != NULL);

    xwork_tool_def_init(&tToolDef);
    tToolDef.sToolId = "mock.echo";
    tToolDef.sDisplayName = "Mock Echo";
    tToolDef.sDescription = "Echo a small payload for provider failure validation.";
    tToolDef.eKind = XWORK_TOOL_VIRTUAL;
    tToolDef.eSideEffect = XWORK_SIDE_EFFECT_READ_ONLY;
    tToolDef.eApprovalMode = XWORK_APPROVAL_NEVER;
    assert(xwork_runtime_register_tool(pRuntime, &tToolDef) == XWORK_OK);

    asWorkspaceIds[0] = "main";
    xwork_run_options_init(&tRunOptions);
    assert(xwork_profile_apply_run_options(pProductProfile, &tRunOptions) == XWORK_OK);
    tRunOptions.sRunId = "run-provider-local-openai-failure";
    tRunOptions.sInstruction =
        "Call the tool `mock.echo` with a JSON object that contains "
        "the text `provider smoke`.";
    tRunOptions.sLlmProfileId = "provider-local-openai-failure";
    tRunOptions.sSessionProfileId = "provider-local-openai-failure";
    tRunOptions.psWorkspaceIds = asWorkspaceIds;
    tRunOptions.iWorkspaceCount = 1u;
    assert(xwork_run_create(pRuntime, &tRunOptions, &pRun) == XWORK_OK);

    xwork_orchestrator_options_init(&tExecOptions);
    assert(
        xwork_profile_apply_orchestrator_options(
            pProductProfile,
            &tExecOptions
        ) == XWORK_OK
    );
    tExecOptions.pfnToolExec = xwork_provider_smoke_tool_exec;
    tExecOptions.pUserData = &tSmokeCtx;
    tExecOptions.bAutoApprove = true;

    assert(xwork_run_execute_async(pRun, &tExecOptions, &pAsync) == XWORK_OK);
    assert(xwork_run_async_wait(pAsync) == XWORK_ERROR_EXTERNAL_FAILURE);
    assert(
        xwork_run_async_get_status(
            pAsync,
            &iAsyncStatus,
            &bAsyncCompleted
        ) == XWORK_OK
    );
    assert(bAsyncCompleted);
    assert(iAsyncStatus == XWORK_ERROR_EXTERNAL_FAILURE);
    xwork_run_async_destroy(pAsync);
    pAsync = NULL;

    assert(xwork_run_get_state(pRun) == XWORK_RUN_FAILED);
    assert(tSmokeCtx.iExecCount == 0u);
    assert(tSmokeCtx.iRequestTraceCount >= 1u);
    assert(xwork_run_get_last_output_text(pRun) != NULL);
    assert(
        strstr(
            xwork_run_get_last_output_text(pRun),
            "xllm session chat failed"
        ) != NULL
    );

    xwork_event_init(&tEvent);
    assert(xwork_run_get_last_event(pRun, &tEvent) == XWORK_OK);
    assert(tEvent.eKind == XWORK_EVENT_RUN_FAILED);
    xwork_event_reset(&tEvent);

    xwork_run_destroy(pRun);
    pRun = NULL;
    xwork_runtime_destroy(pRuntime);
    pRuntime = NULL;

    assert(tStub.sError[0] == '\0');
    assert((size_t)tStub.iRequestCount == 1u);
    assert(tStub.asRequestTargets[0] != NULL);
    assert(tStub.asRequestHeaders[0] != NULL);
    assert(tStub.asRequestBodies[0] != NULL);
    assert(strstr(tStub.asRequestTargets[0], "/chat/completions") != NULL);
    assert(strstr(tStub.asRequestBodies[0], "\"tools\"") != NULL);
    assert(strstr(tStub.asRequestBodies[0], "\"mock.echo\"") != NULL);
    assert(strstr(tStub.asRequestHeaders[0], "\r\nAuthorization: Bearer stub-openai-key") != NULL);
    xwork_test_provider_stub_stop(&tStub);
}

int main(void)
{
    const char *sAdapter = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_ADAPTER");
    const char *sBaseUrl = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_BASE_URL");
    const char *sModel = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_MODEL");
    const char *sApiKey = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_API_KEY");
    const char *sProfileId = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_PROFILE_ID");
    const char *sXworkProfileId = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_XWORK_PROFILE");
    const char *sProvider = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_PROVIDER");
    const char *sOpenAIOrganizationId = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_OPENAI_ORG_ID"
    );
    const char *sOpenAIProjectId = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_OPENAI_PROJECT_ID"
    );
    const char *sAnthropicApiVersion = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_ANTHROPIC_API_VERSION"
    );
    const char *sAnthropicBetaHeadersEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_ANTHROPIC_BETA_HEADERS"
    );
    const char *sDebugModeEnv = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_DEBUG_MODE");
    const char *sRedactModeEnv = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_REDACT_MODE");
    const char *sConnectTimeoutMsEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_CONNECT_TIMEOUT_MS"
    );
    const char *sReadTimeoutMsEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_READ_TIMEOUT_MS"
    );
    const char *sVerifyPeerEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_VERIFY_PEER"
    );
    const char *sProxyKindEnv = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_PROXY_KIND");
    const char *sProxyHostEnv = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_PROXY_HOST");
    const char *sProxyPortEnv = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_PROXY_PORT");
    const char *sProxyUserEnv = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_PROXY_USER");
    const char *sProxyPassEnv = xwork_test_env_non_empty("XWORK_PROVIDER_SMOKE_PROXY_PASS");
    const char *sCaBundlePathEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_CA_BUNDLE_PATH"
    );
    const char *sClientCertPathEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_CLIENT_CERT_PATH"
    );
    const char *sClientKeyPathEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_CLIENT_KEY_PATH"
    );
    const char *sExpectErrorEnv = xwork_test_env_non_empty(
        "XWORK_PROVIDER_SMOKE_EXPECT_ERROR"
    );
    xwork_provider_smoke_ctx tSmokeCtx;
    xwork_profile tProductProfile;
    xwork_xllm_profile_options tBootstrapProfile;
    xwork_xllm_bootstrap_options tBootstrap;
    xwork_runtime_options tBootstrapRuntimeOptions;
    xwork_runtime *pBootstrapRuntime = NULL;
    const xllm_profile *pRegisteredProfile;
    xwork_xllm_profile_options tProviderProfile;
    xwork_xllm_bootstrap_options tProviderBootstrap;
    xwork_runtime_options tRuntimeOptions;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace_options tWorkspaceOptions;
    xwork_workspace *pWorkspace = NULL;
    xwork_tool_def tToolDef;
    xwork_run_options tRunOptions;
    xwork_run *pRun = NULL;
    xwork_run_async *pAsync = NULL;
    xwork_orchestrator_options tExecOptions;
    xwork_event tEvent;
    xwork_status iAsyncStatus;
    const char *asWorkspaceIds[1];
    const char *asBootstrapAnthropicBetaHeaders[2];
    char **psProviderAnthropicBetaHeaders = NULL;
    size_t iProviderAnthropicBetaHeaderCount = 0u;
    bool bParsedVerifyPeer = false;
    bool bExpectProviderError = false;
    size_t iParsedConnectTimeoutMs = 0u;
    size_t iParsedReadTimeoutMs = 0u;
    size_t iParsedProxyPort = 0u;
    bool bAsyncCompleted = false;
    xwork_xllm_debug_mode eParsedDebugMode = XWORK_XLLM_DEBUG_NONE;
    xwork_xllm_redact_mode eParsedRedactMode = XWORK_XLLM_REDACT_DEFAULT;
    xwork_xllm_proxy_kind eParsedProxyKind = XWORK_XLLM_PROXY_UNSPECIFIED;

    if ( !sAdapter ) {
        sAdapter = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
    }
    if ( !sXworkProfileId ) {
        sXworkProfileId = XWORK_PROFILE_XCODE;
    }
    if ( !sProvider ) {
        sProvider = sAdapter;
    }

    xwork_profile_init(&tProductProfile);
    assert(xwork_profile_get_builtin(sXworkProfileId, &tProductProfile) == XWORK_OK);

    xwork_xllm_profile_options_init(&tBootstrapProfile);
    xwork_xllm_bootstrap_options_init(&tBootstrap);
    assert(
        xwork_profile_apply_xllm_bootstrap_options(
            &tProductProfile,
            &tBootstrapProfile,
            &tBootstrap
        ) == XWORK_OK
    );
    tBootstrapProfile.sProfileId = "provider-bootstrap-smoke";
    tBootstrapProfile.sDisplayName = "provider-bootstrap-smoke";
    tBootstrapProfile.sProvider = "bootstrap-smoke";
    tBootstrapProfile.sAdapter = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
    tBootstrapProfile.sBaseUrl = "http://127.0.0.1:1/v1";
    tBootstrapProfile.sModelId = "dummy-model";
    tBootstrapProfile.sApiKey = "bootstrap-openai-key";
    tBootstrapProfile.sOpenAIOrganizationId = "org-bootstrap";
    tBootstrapProfile.sOpenAIProjectId = "proj-bootstrap";
    tBootstrapProfile.iMaxOutputTokens = 32u;
    tBootstrap.eDebugMode = XWORK_XLLM_DEBUG_BODY;
    tBootstrap.eRedactMode = XWORK_XLLM_REDACT_STRICT;
    tBootstrap.tTransportDefaults.bSetConnectTimeoutMs = true;
    tBootstrap.tTransportDefaults.iConnectTimeoutMs = 1234u;
    tBootstrap.tTransportDefaults.bSetReadTimeoutMs = true;
    tBootstrap.tTransportDefaults.iReadTimeoutMs = 5678u;
    tBootstrap.tTransportDefaults.bSetVerifyPeer = true;
    tBootstrap.tTransportDefaults.bVerifyPeer = false;
    tBootstrap.tTransportDefaults.eProxyKind = XWORK_XLLM_PROXY_HTTP_CONNECT;
    tBootstrap.tTransportDefaults.sProxyHost = "proxy.bootstrap.local";
    tBootstrap.tTransportDefaults.bSetProxyPort = true;
    tBootstrap.tTransportDefaults.iProxyPort = 8443u;
    tBootstrap.tTransportDefaults.sProxyUser = "bootstrap-user";
    tBootstrap.tTransportDefaults.sProxyPass = "bootstrap-pass";
    tBootstrap.tTransportDefaults.sCaBundlePath = "certs/bootstrap-ca.pem";
    tBootstrap.tTransportDefaults.sClientCertPath = "certs/bootstrap-client.pem";
    tBootstrap.tTransportDefaults.sClientKeyPath = "certs/bootstrap-client.key";

    xwork_runtime_options_init(&tBootstrapRuntimeOptions);
    tBootstrapRuntimeOptions.pLlmBootstrap = &tBootstrap;
    assert(xwork_runtime_create(&tBootstrapRuntimeOptions, &pBootstrapRuntime) == XWORK_OK);
    assert(xwork_runtime_get_llm_runtime(pBootstrapRuntime) != NULL);
    pRegisteredProfile = xwork_test_find_runtime_profile(
        pBootstrapRuntime,
        "provider-bootstrap-smoke"
    );
    assert(pRegisteredProfile != NULL);
    assert(strcmp(pRegisteredProfile->sId, "provider-bootstrap-smoke") == 0);
    assert(strcmp(pRegisteredProfile->sName, "provider-bootstrap-smoke") == 0);
    assert(strcmp(pRegisteredProfile->sProvider, "bootstrap-smoke") == 0);
    assert(strcmp(pRegisteredProfile->sAdapter, XWORK_XLLM_ADAPTER_OPENAI_COMPAT) == 0);
    assert(strcmp(pRegisteredProfile->sBaseUrl, "http://127.0.0.1:1/v1") == 0);
    assert(strcmp(pRegisteredProfile->tModels.tText.sModelId, "dummy-model") == 0);
    assert(
        strcmp(
            pRegisteredProfile->tProviderOptions.sOpenAIOrganizationId,
            "org-bootstrap"
        ) == 0
    );
    assert(
        strcmp(
            pRegisteredProfile->tProviderOptions.sOpenAIProjectId,
            "proj-bootstrap"
        ) == 0
    );
    assert(pRegisteredProfile->tDefaults.tGeneration.tMaxOutputTokens.bSet);
    assert(pRegisteredProfile->tDefaults.tGeneration.tMaxOutputTokens.iValue == 32u);
    assert(pRegisteredProfile->tAuth.eKind == XLLM_AUTH_BEARER);
    assert(strcmp(pRegisteredProfile->tAuth.sSecret, "bootstrap-openai-key") == 0);
    assert(strcmp(pRegisteredProfile->tAuth.sScheme, "Bearer") == 0);
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.eDebugMode == XLLM_DEBUG_BODY
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.eRedactMode == XLLM_REDACT_STRICT
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.tConnectTimeoutMs.bSet
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.tConnectTimeoutMs.iValue == 1234u
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.tReadTimeoutMs.bSet
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.tReadTimeoutMs.iValue == 5678u
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.tVerifyPeer.bSet
    );
    assert(
        !xwork_runtime_get_llm_runtime(pBootstrapRuntime)
             ->tOptions.tTransportDefaults.tVerifyPeer.bValue
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.eProxyKind == XLLM_PROXY_HTTP_CONNECT
    );
    assert(
        strcmp(
            xwork_runtime_get_llm_runtime(pBootstrapRuntime)
                ->tOptions.tTransportDefaults.sProxyHost,
            "proxy.bootstrap.local"
        ) == 0
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.tProxyPort.bSet
    );
    assert(
        xwork_runtime_get_llm_runtime(pBootstrapRuntime)
            ->tOptions.tTransportDefaults.tProxyPort.iValue == 8443u
    );
    assert(
        strcmp(
            xwork_runtime_get_llm_runtime(pBootstrapRuntime)
                ->tOptions.tTransportDefaults.sProxyUser,
            "bootstrap-user"
        ) == 0
    );
    assert(
        strcmp(
            xwork_runtime_get_llm_runtime(pBootstrapRuntime)
                ->tOptions.tTransportDefaults.sProxyPass,
            "bootstrap-pass"
        ) == 0
    );
    assert(
        strcmp(
            xwork_runtime_get_llm_runtime(pBootstrapRuntime)
                ->tOptions.tTransportDefaults.sCaBundlePath,
            "certs/bootstrap-ca.pem"
        ) == 0
    );
    assert(
        strcmp(
            xwork_runtime_get_llm_runtime(pBootstrapRuntime)
                ->tOptions.tTransportDefaults.sClientCertPath,
            "certs/bootstrap-client.pem"
        ) == 0
    );
    assert(
        strcmp(
            xwork_runtime_get_llm_runtime(pBootstrapRuntime)
                ->tOptions.tTransportDefaults.sClientKeyPath,
            "certs/bootstrap-client.key"
        ) == 0
    );
    xwork_runtime_destroy(pBootstrapRuntime);
    pBootstrapRuntime = NULL;

    xwork_xllm_profile_options_init(&tBootstrapProfile);
    xwork_xllm_bootstrap_options_init(&tBootstrap);
    assert(
        xwork_profile_apply_xllm_bootstrap_options(
            &tProductProfile,
            &tBootstrapProfile,
            &tBootstrap
        ) == XWORK_OK
    );
    tBootstrapProfile.sProfileId = "provider-bootstrap-anthropic";
    tBootstrapProfile.sDisplayName = "provider-bootstrap-anthropic";
    tBootstrapProfile.sProvider = "bootstrap-anthropic";
    tBootstrapProfile.sAdapter = XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tBootstrapProfile.sBaseUrl = "https://example.invalid";
    tBootstrapProfile.sModelId = "claude-bootstrap";
    tBootstrapProfile.sApiKey = "bootstrap-anthropic-key";
    tBootstrapProfile.sAnthropicApiVersion = "2023-06-01";
    asBootstrapAnthropicBetaHeaders[0] = "beta-tools-2024-04-04";
    asBootstrapAnthropicBetaHeaders[1] = "beta-output-128k-2024-02-29";
    tBootstrapProfile.psAnthropicBetaHeaders = asBootstrapAnthropicBetaHeaders;
    tBootstrapProfile.iAnthropicBetaHeaderCount = 2u;

    xwork_runtime_options_init(&tBootstrapRuntimeOptions);
    tBootstrapRuntimeOptions.pLlmBootstrap = &tBootstrap;
    assert(xwork_runtime_create(&tBootstrapRuntimeOptions, &pBootstrapRuntime) == XWORK_OK);
    pRegisteredProfile = xwork_test_find_runtime_profile(
        pBootstrapRuntime,
        "provider-bootstrap-anthropic"
    );
    assert(pRegisteredProfile != NULL);
    assert(strcmp(pRegisteredProfile->sAdapter, XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE) == 0);
    assert(
        strcmp(
            pRegisteredProfile->tProviderOptions.sAnthropicApiVersion,
            "2023-06-01"
        ) == 0
    );
    assert(!pRegisteredProfile->tDefaults.tGeneration.tMaxOutputTokens.bSet);
    assert(pRegisteredProfile->tAuth.eKind == XLLM_AUTH_API_KEY_HEADER);
    assert(strcmp(pRegisteredProfile->tAuth.sSecret, "bootstrap-anthropic-key") == 0);
    assert(pRegisteredProfile->tProviderOptions.iAnthropicBetaHeaderCount == 2u);
    assert(
        strcmp(
            pRegisteredProfile->tProviderOptions.psAnthropicBetaHeaders[0],
            "beta-tools-2024-04-04"
        ) == 0
    );
    assert(
        strcmp(
            pRegisteredProfile->tProviderOptions.psAnthropicBetaHeaders[1],
            "beta-output-128k-2024-02-29"
        ) == 0
    );
    xwork_runtime_destroy(pBootstrapRuntime);
    pBootstrapRuntime = NULL;

    xwork_xllm_profile_options_init(&tBootstrapProfile);
    xwork_xllm_bootstrap_options_init(&tBootstrap);
    assert(
        xwork_profile_apply_xllm_bootstrap_options(
            &tProductProfile,
            &tBootstrapProfile,
            &tBootstrap
        ) == XWORK_OK
    );
    tBootstrapProfile.sProfileId = "provider-bootstrap-ollama";
    tBootstrapProfile.sAdapter = XWORK_XLLM_ADAPTER_OLLAMA_NATIVE;
    tBootstrapProfile.sBaseUrl = "http://127.0.0.1:11434";
    tBootstrapProfile.sModelId = "llama3.2";

    xwork_runtime_options_init(&tBootstrapRuntimeOptions);
    tBootstrapRuntimeOptions.pLlmBootstrap = &tBootstrap;
    assert(xwork_runtime_create(&tBootstrapRuntimeOptions, &pBootstrapRuntime) == XWORK_OK);
    pRegisteredProfile = xwork_test_find_runtime_profile(
        pBootstrapRuntime,
        "provider-bootstrap-ollama"
    );
    assert(pRegisteredProfile != NULL);
    assert(strcmp(pRegisteredProfile->sId, "provider-bootstrap-ollama") == 0);
    assert(strcmp(pRegisteredProfile->sName, tProductProfile.sDisplayName) == 0);
    assert(strcmp(pRegisteredProfile->sProvider, XWORK_XLLM_ADAPTER_OLLAMA_NATIVE) == 0);
    assert(strcmp(pRegisteredProfile->sAdapter, XWORK_XLLM_ADAPTER_OLLAMA_NATIVE) == 0);
    assert(strcmp(pRegisteredProfile->sBaseUrl, "http://127.0.0.1:11434") == 0);
    assert(strcmp(pRegisteredProfile->tModels.tText.sModelId, "llama3.2") == 0);
    assert(!pRegisteredProfile->tDefaults.tGeneration.tMaxOutputTokens.bSet);
    assert(pRegisteredProfile->tAuth.eKind == XLLM_AUTH_NONE);
    assert(xwork_test_find_runtime_profile(pBootstrapRuntime, "missing-profile") == NULL);
    xwork_runtime_destroy(pBootstrapRuntime);
    pBootstrapRuntime = NULL;

    if ( !sBaseUrl || !sModel ) {
        static const char *asLocalAnthropicBetaHeaders[1] = {
            "beta-tools-2024-04-04"
        };

        xwork_test_run_local_provider_case(
            &tProductProfile,
            XWORK_XLLM_ADAPTER_OPENAI_COMPAT,
            XWORK_XLLM_ADAPTER_OPENAI_COMPAT,
            "provider-local-openai",
            "run-provider-local-openai",
            "stub-openai-model",
            "stub-openai-key",
            "org-local-openai",
            "proj-local-openai",
            NULL,
            NULL,
            0u
        );
        xwork_test_run_local_provider_case(
            &tProductProfile,
            XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE,
            XWORK_XLLM_ADAPTER_ANTHROPIC_NATIVE,
            "provider-local-anthropic",
            "run-provider-local-anthropic",
            "stub-claude-model",
            "stub-anthropic-key",
            NULL,
            NULL,
            "2023-06-01",
            asLocalAnthropicBetaHeaders,
            1u
        );
        xwork_test_run_local_provider_case(
            &tProductProfile,
            XWORK_XLLM_ADAPTER_OLLAMA_NATIVE,
            XWORK_XLLM_ADAPTER_OLLAMA_NATIVE,
            "provider-local-ollama",
            "run-provider-local-ollama",
            "stub-ollama-model",
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            0u
        );
        xwork_test_run_local_provider_failure_case(&tProductProfile);
        return 0;
    }

    if ( sAnthropicBetaHeadersEnv && sAnthropicBetaHeadersEnv[0] ) {
        assert(
            xwork_test_split_csv(
                sAnthropicBetaHeadersEnv,
                &psProviderAnthropicBetaHeaders,
                &iProviderAnthropicBetaHeaderCount
            )
        );
    }
    if ( sExpectErrorEnv ) {
        assert(xwork_test_parse_bool(sExpectErrorEnv, &bExpectProviderError));
    }

    memset(&tSmokeCtx, 0, sizeof(tSmokeCtx));
    xwork_runtime_options_init(&tRuntimeOptions);
    assert(xwork_profile_apply_runtime_options(&tProductProfile, &tRuntimeOptions) == XWORK_OK);
    xwork_xllm_profile_options_init(&tProviderProfile);
    xwork_xllm_bootstrap_options_init(&tProviderBootstrap);
    assert(
        xwork_profile_apply_xllm_bootstrap_options(
            &tProductProfile,
            &tProviderProfile,
            &tProviderBootstrap
        ) == XWORK_OK
    );
    if ( sProfileId && sProfileId[0] ) {
        tProviderProfile.sProfileId = sProfileId;
    }
    tProviderProfile.sDisplayName = "xwork-provider-smoke";
    tProviderProfile.sProvider = sProvider;
    tProviderProfile.sAdapter = sAdapter;
    tProviderProfile.sBaseUrl = sBaseUrl;
    tProviderProfile.sModelId = sModel;
    tProviderProfile.sApiKey = sApiKey;
    tProviderProfile.sOpenAIOrganizationId = sOpenAIOrganizationId;
    tProviderProfile.sOpenAIProjectId = sOpenAIProjectId;
    tProviderProfile.sAnthropicApiVersion = sAnthropicApiVersion;
    tProviderProfile.psAnthropicBetaHeaders =
        (const char **)psProviderAnthropicBetaHeaders;
    tProviderProfile.iAnthropicBetaHeaderCount =
        iProviderAnthropicBetaHeaderCount;
    tProviderProfile.iMaxOutputTokens = 256u;
    assert(tProviderProfile.sProfileId != NULL);
    assert(tProviderProfile.sProfileId[0] != '\0');

    if ( sDebugModeEnv ) {
        assert(xwork_test_parse_debug_mode(sDebugModeEnv, &eParsedDebugMode));
        tProviderBootstrap.eDebugMode = eParsedDebugMode;
    }
    if ( sRedactModeEnv ) {
        assert(xwork_test_parse_redact_mode(sRedactModeEnv, &eParsedRedactMode));
        tProviderBootstrap.eRedactMode = eParsedRedactMode;
    }
    if ( sConnectTimeoutMsEnv ) {
        assert(xwork_test_parse_size_t(sConnectTimeoutMsEnv, &iParsedConnectTimeoutMs));
        tProviderBootstrap.tTransportDefaults.bSetConnectTimeoutMs = true;
        tProviderBootstrap.tTransportDefaults.iConnectTimeoutMs = iParsedConnectTimeoutMs;
    }
    if ( sReadTimeoutMsEnv ) {
        assert(xwork_test_parse_size_t(sReadTimeoutMsEnv, &iParsedReadTimeoutMs));
        tProviderBootstrap.tTransportDefaults.bSetReadTimeoutMs = true;
        tProviderBootstrap.tTransportDefaults.iReadTimeoutMs = iParsedReadTimeoutMs;
    }
    if ( sVerifyPeerEnv ) {
        assert(xwork_test_parse_bool(sVerifyPeerEnv, &bParsedVerifyPeer));
        tProviderBootstrap.tTransportDefaults.bSetVerifyPeer = true;
        tProviderBootstrap.tTransportDefaults.bVerifyPeer = bParsedVerifyPeer;
    }
    if ( sProxyKindEnv ) {
        assert(xwork_test_parse_proxy_kind(sProxyKindEnv, &eParsedProxyKind));
        tProviderBootstrap.tTransportDefaults.eProxyKind = eParsedProxyKind;
    }
    if ( sProxyHostEnv ) {
        tProviderBootstrap.tTransportDefaults.sProxyHost = sProxyHostEnv;
    }
    if ( sProxyPortEnv ) {
        assert(xwork_test_parse_size_t(sProxyPortEnv, &iParsedProxyPort));
        tProviderBootstrap.tTransportDefaults.bSetProxyPort = true;
        tProviderBootstrap.tTransportDefaults.iProxyPort = iParsedProxyPort;
    }
    if ( sProxyUserEnv ) {
        tProviderBootstrap.tTransportDefaults.sProxyUser = sProxyUserEnv;
    }
    if ( sProxyPassEnv ) {
        tProviderBootstrap.tTransportDefaults.sProxyPass = sProxyPassEnv;
    }
    if ( sCaBundlePathEnv ) {
        tProviderBootstrap.tTransportDefaults.sCaBundlePath = sCaBundlePathEnv;
    }
    if ( sClientCertPathEnv ) {
        tProviderBootstrap.tTransportDefaults.sClientCertPath = sClientCertPathEnv;
    }
    if ( sClientKeyPathEnv ) {
        tProviderBootstrap.tTransportDefaults.sClientKeyPath = sClientKeyPathEnv;
    }

    tRuntimeOptions.pLlmBootstrap = &tProviderBootstrap;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);
    assert(xwork_runtime_get_llm_runtime(pRuntime) != NULL);
    if ( sDebugModeEnv ) {
        switch ( eParsedDebugMode ) {
        case XWORK_XLLM_DEBUG_NONE:
            assert(xwork_runtime_get_llm_runtime(pRuntime)->tOptions.eDebugMode == XLLM_DEBUG_NONE);
            break;
        case XWORK_XLLM_DEBUG_HEADERS:
            assert(
                xwork_runtime_get_llm_runtime(pRuntime)->tOptions.eDebugMode ==
                XLLM_DEBUG_HEADERS
            );
            break;
        case XWORK_XLLM_DEBUG_BODY:
            assert(xwork_runtime_get_llm_runtime(pRuntime)->tOptions.eDebugMode == XLLM_DEBUG_BODY);
            break;
        case XWORK_XLLM_DEBUG_WIRE:
            assert(xwork_runtime_get_llm_runtime(pRuntime)->tOptions.eDebugMode == XLLM_DEBUG_WIRE);
            break;
        }
    }
    if ( sRedactModeEnv ) {
        switch ( eParsedRedactMode ) {
        case XWORK_XLLM_REDACT_DEFAULT:
            assert(
                xwork_runtime_get_llm_runtime(pRuntime)->tOptions.eRedactMode ==
                XLLM_REDACT_DEFAULT
            );
            break;
        case XWORK_XLLM_REDACT_OFF:
            assert(xwork_runtime_get_llm_runtime(pRuntime)->tOptions.eRedactMode == XLLM_REDACT_OFF);
            break;
        case XWORK_XLLM_REDACT_STRICT:
            assert(
                xwork_runtime_get_llm_runtime(pRuntime)->tOptions.eRedactMode ==
                XLLM_REDACT_STRICT
            );
            break;
        }
    }
    if ( sConnectTimeoutMsEnv ) {
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tConnectTimeoutMs.bSet
        );
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tConnectTimeoutMs.iValue ==
            iParsedConnectTimeoutMs
        );
    }
    if ( sReadTimeoutMsEnv ) {
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tReadTimeoutMs.bSet
        );
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tReadTimeoutMs.iValue ==
            iParsedReadTimeoutMs
        );
    }
    if ( sVerifyPeerEnv ) {
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tVerifyPeer.bSet
        );
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tVerifyPeer.bValue ==
            bParsedVerifyPeer
        );
    }
    if ( sProxyKindEnv ) {
        switch ( eParsedProxyKind ) {
        case XWORK_XLLM_PROXY_UNSPECIFIED:
            assert(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.eProxyKind ==
                XLLM_PROXY_UNSPECIFIED
            );
            break;
        case XWORK_XLLM_PROXY_NONE:
            assert(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.eProxyKind ==
                XLLM_PROXY_NONE
            );
            break;
        case XWORK_XLLM_PROXY_SOCKS5:
            assert(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.eProxyKind ==
                XLLM_PROXY_SOCKS5
            );
            break;
        case XWORK_XLLM_PROXY_HTTP_CONNECT:
            assert(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.eProxyKind ==
                XLLM_PROXY_HTTP_CONNECT
            );
            break;
        }
    }
    if ( sProxyHostEnv ) {
        assert(
            strcmp(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.sProxyHost,
                sProxyHostEnv
            ) == 0
        );
    }
    if ( sProxyPortEnv ) {
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tProxyPort.bSet
        );
        assert(
            xwork_runtime_get_llm_runtime(pRuntime)
                ->tOptions.tTransportDefaults.tProxyPort.iValue ==
            iParsedProxyPort
        );
    }
    if ( sProxyUserEnv ) {
        assert(
            strcmp(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.sProxyUser,
                sProxyUserEnv
            ) == 0
        );
    }
    if ( sProxyPassEnv ) {
        assert(
            strcmp(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.sProxyPass,
                sProxyPassEnv
            ) == 0
        );
    }
    if ( sCaBundlePathEnv ) {
        assert(
            strcmp(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.sCaBundlePath,
                sCaBundlePathEnv
            ) == 0
        );
    }
    if ( sClientCertPathEnv ) {
        assert(
            strcmp(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.sClientCertPath,
                sClientCertPathEnv
            ) == 0
        );
    }
    if ( sClientKeyPathEnv ) {
        assert(
            strcmp(
                xwork_runtime_get_llm_runtime(pRuntime)
                    ->tOptions.tTransportDefaults.sClientKeyPath,
                sClientKeyPathEnv
            ) == 0
        );
    }
    assert(
        xllm_runtime_set_trace_callback(
            xwork_runtime_get_llm_runtime(pRuntime),
            xwork_test_trace_callback,
            &tSmokeCtx
        ) == XRT_NET_OK
    );

    xwork_workspace_options_init(&tWorkspaceOptions);
    tWorkspaceOptions.sWorkspaceId = "main";
    tWorkspaceOptions.sRootPath = ".";
    assert(xwork_runtime_add_workspace(pRuntime, &tWorkspaceOptions, &pWorkspace) == XWORK_OK);
    assert(pWorkspace != NULL);

    xwork_tool_def_init(&tToolDef);
    tToolDef.sToolId = "mock.echo";
    tToolDef.sDisplayName = "Mock Echo";
    tToolDef.sDescription = "Echo a small payload for provider smoke validation.";
    tToolDef.eKind = XWORK_TOOL_VIRTUAL;
    tToolDef.eSideEffect = XWORK_SIDE_EFFECT_READ_ONLY;
    tToolDef.eApprovalMode = XWORK_APPROVAL_NEVER;
    assert(xwork_runtime_register_tool(pRuntime, &tToolDef) == XWORK_OK);

    asWorkspaceIds[0] = "main";
    xwork_run_options_init(&tRunOptions);
    assert(xwork_profile_apply_run_options(&tProductProfile, &tRunOptions) == XWORK_OK);
    tRunOptions.sRunId = "run-provider-smoke";
    tRunOptions.sInstruction =
        "Follow these rules exactly:\n"
        "1. Call the tool `mock.echo` exactly once.\n"
        "2. Pass a JSON object that contains the text `provider smoke`.\n"
        "3. After the tool result arrives, respond with the exact token "
        "`PROVIDER_SMOKE_COMPLETE` and nothing else.";
    if ( sProfileId && sProfileId[0] ) {
        tRunOptions.sLlmProfileId = sProfileId;
        tRunOptions.sSessionProfileId = sProfileId;
    }
    tRunOptions.psWorkspaceIds = asWorkspaceIds;
    tRunOptions.iWorkspaceCount = 1u;
    assert(xwork_run_create(pRuntime, &tRunOptions, &pRun) == XWORK_OK);

    xwork_orchestrator_options_init(&tExecOptions);
    assert(
        xwork_profile_apply_orchestrator_options(
            &tProductProfile,
            &tExecOptions
        ) == XWORK_OK
    );
    tExecOptions.pfnToolExec = xwork_provider_smoke_tool_exec;
    tExecOptions.pUserData = &tSmokeCtx;
    tExecOptions.bAutoApprove = true;
    assert(xwork_run_execute_async(pRun, &tExecOptions, &pAsync) == XWORK_OK);
    iAsyncStatus = xwork_run_async_wait(pAsync);
    if ( bExpectProviderError ) {
        assert(iAsyncStatus == XWORK_ERROR_EXTERNAL_FAILURE);
    } else {
        assert(iAsyncStatus == XWORK_OK);
    }
    assert(xwork_run_async_get_status(pAsync, &iAsyncStatus, &bAsyncCompleted) == XWORK_OK);
    assert(bAsyncCompleted);
    if ( bExpectProviderError ) {
        assert(iAsyncStatus == XWORK_ERROR_EXTERNAL_FAILURE);
    } else {
        assert(iAsyncStatus == XWORK_OK);
    }
    xwork_run_async_destroy(pAsync);
    pAsync = NULL;

    if ( bExpectProviderError ) {
        assert(xwork_run_get_state(pRun) == XWORK_RUN_FAILED);
        assert(tSmokeCtx.iExecCount == 0u);
        assert(tSmokeCtx.iRequestTraceCount >= 1u);
        assert(xwork_run_get_last_output_text(pRun) != NULL);
        assert(
            strstr(
                xwork_run_get_last_output_text(pRun),
                "xllm session chat failed"
            ) != NULL
        );

        xwork_event_init(&tEvent);
        assert(xwork_run_get_last_event(pRun, &tEvent) == XWORK_OK);
        assert(tEvent.eKind == XWORK_EVENT_RUN_FAILED);
        xwork_event_reset(&tEvent);

        xwork_run_destroy(pRun);
        xwork_runtime_destroy(pRuntime);
        xwork_test_free_string_array(
            psProviderAnthropicBetaHeaders,
            iProviderAnthropicBetaHeaderCount
        );
        return 0;
    }

    assert(xwork_run_get_state(pRun) == XWORK_RUN_COMPLETED);
    assert(tSmokeCtx.iExecCount == 1u);
    assert(tSmokeCtx.iRequestTraceCount >= 2u);
    assert(tSmokeCtx.iResponseTraceCount >= 2u);
    assert(xwork_run_get_last_output_text(pRun) != NULL);
    assert(
        strstr(
            xwork_run_get_last_output_text(pRun),
            "PROVIDER_SMOKE_COMPLETE"
        ) != NULL
    );

    xwork_event_init(&tEvent);
    assert(xwork_run_get_last_event(pRun, &tEvent) == XWORK_OK);
    assert(tEvent.eKind == XWORK_EVENT_RUN_COMPLETED);
    assert(xwork_run_get_artifact_count(pRun) == 0u);
    xwork_event_reset(&tEvent);

    xwork_run_destroy(pRun);
    xwork_runtime_destroy(pRuntime);
    xwork_test_free_string_array(
        psProviderAnthropicBetaHeaders,
        iProviderAnthropicBetaHeaderCount
    );
    return 0;
}
