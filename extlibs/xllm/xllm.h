#ifndef XLLM_H
#define XLLM_H

/*
 * xllm v2: a small, provider-call boundary for C applications.
 *
 * The library owns request serialization, HTTP transport, SSE decoding and
 * provider response normalization. It deliberately does not execute tools,
 * manage conversation history, compact context, or run an agent loop.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XLLM_VERSION_MAJOR 2
#define XLLM_VERSION_MINOR 1
#define XLLM_VERSION_PATCH 2

typedef struct xllm_client xllm_client;
typedef struct xllm_call xllm_call;
typedef struct xcancel xcancel;

typedef enum xllm_result {
    XLLM_RESULT_OK = 0,
    XLLM_RESULT_ERROR = -1,
    XLLM_RESULT_TIMEOUT = -2,
    XLLM_RESULT_CANCELLED = -3
} xllm_result;

typedef enum xllm_error_code {
    XLLM_ERROR_NONE = 0,
    XLLM_ERROR_INVALID_ARGUMENT,
    XLLM_ERROR_OUT_OF_MEMORY,
    XLLM_ERROR_NETWORK,
    XLLM_ERROR_TIMEOUT,
    XLLM_ERROR_CANCELLED,
    XLLM_ERROR_AUTH,
    XLLM_ERROR_RATE_LIMIT,
    XLLM_ERROR_MODEL_NOT_FOUND,
    XLLM_ERROR_UPSTREAM,
    XLLM_ERROR_PROTOCOL,
    XLLM_ERROR_PARSE
} xllm_error_code;

typedef struct xllm_diagnostics {
    uint32_t uAttemptCount;
    uint32_t uMaxAttempts;
    uint32_t uRetryAfterMs;
    bool bRetryable;
    bool bRetryExhausted;
    bool bResponseStarted;
    bool bModelDataDelivered;
    bool bReusedConnection;
    bool bContextAttached;
    int32_t iTransportStatus;
    int32_t iSystemError;
    uint64_t uStartedMs;
    uint64_t uConnectedMs;
    uint64_t uRequestSentMs;
    uint64_t uFirstByteMs;
    uint64_t uHeadersMs;
    uint64_t uCompletedMs;
    uint64_t uConnectDurationMs;
    uint64_t uTimeToFirstByteMs;
    uint64_t uTransferDurationMs;
    uint64_t uTotalDurationMs;
    uint64_t uRequestBytes;
    uint64_t uResponseBodyBytes;
    uint64_t uContextDeadlineMs;
    uint64_t uEffectiveTimeoutMs;
    char sTransportError[32];
    char sTransportPhase[32];
    char sContextStatus[32];
} xllm_diagnostics;

typedef struct xllm_error {
    xllm_error_code eCode;
    int32_t iTransportStatus;
    int32_t iHttpStatus;
    char sMessage[512];
    char sProviderMessage[2048];
    char sProviderCode[64];
    char sRequestId[160];
    xllm_diagnostics tDiagnostics;
} xllm_error;

typedef enum xllm_role {
    XLLM_ROLE_SYSTEM = 0,
    XLLM_ROLE_USER,
    XLLM_ROLE_ASSISTANT,
    XLLM_ROLE_TOOL
} xllm_role;

typedef struct xllm_tool_call {
    char* sId;
    char* sName;
    char* sArgumentsJson;
} xllm_tool_call;

typedef struct xllm_message {
    xllm_role eRole;
    char* sContent;
    char* sReasoningContent;
    char* sToolCallId;
    xllm_tool_call* pToolCalls;
    size_t iToolCallCount;
    size_t iToolCallCap;
} xllm_message;

typedef struct xllm_tool {
    char* sName;
    char* sDescription;
    char* sParametersJson;
    bool bStrict;
} xllm_tool;

typedef enum xllm_tool_choice {
    XLLM_TOOL_CHOICE_AUTO = 0,
    XLLM_TOOL_CHOICE_NONE,
    XLLM_TOOL_CHOICE_REQUIRED,
    XLLM_TOOL_CHOICE_NAMED
} xllm_tool_choice;

typedef struct xllm_request {
    xllm_message* pMessages;
    size_t iMessageCount;
    size_t iMessageCap;
    xllm_tool* pTools;
    size_t iToolCount;
    size_t iToolCap;
    char* sModel;
    char* sReasoningEffort;
    char* sNamedTool;
    uint32_t uMaxOutputTokens;
    double fTemperature;
    bool bHasTemperature;
    bool bParallelToolCalls;
    xllm_tool_choice eToolChoice;
    /* Borrowed cancellation token; it must outlive this request's model call. */
    xcancel* pCancel;
    /* Absolute xrtClock() deadline in microseconds; UINT64_MAX disables it. */
    uint64_t uDeadline;
} xllm_request;

typedef struct xllm_usage {
    uint64_t uInputTokens;
    uint64_t uOutputTokens;
    uint64_t uTotalTokens;
    uint64_t uCachedInputTokens;
    uint64_t uReasoningTokens;
} xllm_usage;

typedef struct xllm_response {
    char* sId;
    char* sModel;
    char* sContent;
    char* sReasoningContent;
    char* sFinishReason;
    char* sRequestId;
    xllm_tool_call* pToolCalls;
    size_t iToolCallCount;
    size_t iToolCallCap;
    xllm_usage tUsage;
    uint32_t uHttpStatus;
    xllm_diagnostics tDiagnostics;
} xllm_response;

typedef enum xllm_event_kind {
    XLLM_EVENT_RESPONSE_START = 0,
    XLLM_EVENT_TEXT_DELTA,
    XLLM_EVENT_REASONING_DELTA,
    XLLM_EVENT_TOOL_CALL_DELTA,
    XLLM_EVENT_USAGE,
    XLLM_EVENT_RESPONSE_DONE
} xllm_event_kind;

typedef struct xllm_event {
    xllm_event_kind eKind;
    union {
        struct {
            const char* sData;
            size_t iLen;
        } tText;
        struct {
            size_t iIndex;
            const char* sIdDelta;
            const char* sNameDelta;
            const char* sArgumentsDelta;
        } tToolCall;
        xllm_usage tUsage;
        struct {
            uint32_t uHttpStatus;
            const char* sRequestId;
        } tResponse;
    } as;
} xllm_event;

typedef bool (*xllm_event_fn)(void* pUserData, const xllm_event* pEvent);

typedef struct xllm_stream_callbacks {
    void* pUserData;
    xllm_event_fn OnEvent;
} xllm_stream_callbacks;

typedef enum xllm_provider {
    XLLM_PROVIDER_OPENAI_COMPAT = 0,
    XLLM_PROVIDER_GLM
} xllm_provider;

typedef uint64_t xllm_capability_flags;

#define XLLM_CAP_TEXT_IN              (1ull << 0)
#define XLLM_CAP_TOOL_RESULT_IN       (1ull << 1)
#define XLLM_CAP_TEXT_OUT             (1ull << 2)
#define XLLM_CAP_JSON_OUT             (1ull << 3)
#define XLLM_CAP_TOOL_CALL_OUT        (1ull << 4)
#define XLLM_CAP_REASONING_OUT        (1ull << 5)
#define XLLM_CAP_STREAM               (1ull << 6)
#define XLLM_CAP_REASONING_CONTROL    (1ull << 7)
#define XLLM_CAP_PARALLEL_TOOL_CALL   (1ull << 8)

typedef enum xllm_window_mode {
    XLLM_WINDOW_UNSPECIFIED = 0,
    XLLM_WINDOW_SHARED_CONTEXT,
    XLLM_WINDOW_SPLIT_INPUT_OUTPUT
} xllm_window_mode;

/* A model profile is a non-secret, inspectable capability contract. Connection
 * URLs and credentials remain client configuration. Built-in profiles are
 * immutable snapshots; hosts may provide an explicit custom profile instead. */
typedef struct xllm_model_profile {
    const char* sId;
    const char* sModel;
    xllm_provider eProvider;
    xllm_capability_flags uCapabilities;
    xllm_window_mode eWindowMode;
    uint64_t uContextWindowTokens;
    uint64_t uMaxInputTokens;
    uint32_t uMaxOutputTokens;
    uint32_t uRecommendedOutputReserveTokens;
    uint32_t uRecommendedSummaryTokens;
} xllm_model_profile;

typedef struct xllm_client_config {
    const char* sBaseUrl;
    const char* sApiKey;
    const char* sModel;
    const char* sReasoningEffort;
    const char* sUserAgent;
    uint32_t uMaxOutputTokens;
    uint32_t uTimeoutMs;
    uint32_t uIdleTimeoutMs;
    uint32_t uMaxAttempts;
    uint32_t uRetryBaseDelayMs;
    uint32_t uRetryMaxDelayMs;
    bool bVerifyPeer;
    xllm_provider eProvider;
    /* Optional borrowed profile. When present, model/provider/limits are
     * validated and the client retains an owned snapshot. */
    const xllm_model_profile* pModelProfile;
} xllm_client_config;

void xllmModelProfileInit(xllm_model_profile* pProfile);
const xllm_model_profile* xllmModelProfileBuiltin(const char* sIdOrModel);
bool xllmModelProfileValidate(const xllm_model_profile* pProfile, xllm_error* pError);
bool xllmModelProfileSupports(const xllm_model_profile* pProfile, xllm_capability_flags uRequired);
bool xllmModelProfileValidateRequest(const xllm_model_profile* pProfile,
    const xllm_request* pRequest, xllm_error* pError);

void xllmErrorInit(xllm_error* pError);
const char* xllmErrorCodeName(xllm_error_code eCode);
bool xllmErrorRetryable(const xllm_error* pError);

void xllmMessageInit(xllm_message* pMessage, xllm_role eRole);
void xllmMessageUnit(xllm_message* pMessage);
bool xllmMessageSetContent(xllm_message* pMessage, const char* sContent);
bool xllmMessageSetReasoning(xllm_message* pMessage, const char* sReasoningContent);
bool xllmMessageSetToolCallId(xllm_message* pMessage, const char* sToolCallId);
bool xllmMessageAddToolCall(xllm_message* pMessage, const char* sId, const char* sName, const char* sArgumentsJson);

void xllmRequestInit(xllm_request* pRequest);
void xllmRequestUnit(xllm_request* pRequest);
bool xllmRequestSetModel(xllm_request* pRequest, const char* sModel);
bool xllmRequestSetReasoningEffort(xllm_request* pRequest, const char* sEffort);
void xllmRequestSetCancel(xllm_request* pRequest, xcancel* pCancel);
void xllmRequestSetDeadline(xllm_request* pRequest, uint64_t uDeadline);
bool xllmRequestSetToolChoice(xllm_request* pRequest, xllm_tool_choice eChoice, const char* sNamedTool);
bool xllmRequestAddMessage(xllm_request* pRequest, const xllm_message* pMessage);
bool xllmRequestAddTextMessage(xllm_request* pRequest, xllm_role eRole, const char* sContent);
bool xllmRequestAddToolResult(xllm_request* pRequest, const char* sToolCallId, const char* sContent);
bool xllmRequestAddTool(xllm_request* pRequest, const char* sName, const char* sDescription, const char* sParametersJson, bool bStrict);

void xllmResponseDestroy(xllm_response* pResponse);

void xllmClientConfigInit(xllm_client_config* pConfig);
xllm_client* xllmClientCreate(const xllm_client_config* pConfig, xllm_error* pError);
void xllmClientDestroy(xllm_client* pClient);
bool xllmClientGetModelProfile(const xllm_client* pClient, xllm_model_profile* pProfile);

xllm_call* xllmClientStart(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_error* pError
);
xllm_result xllmCallWait(xllm_call* pCall, xllm_response** ppResponse, xllm_error* pError);
bool xllmCallCancel(xllm_call* pCall);
void xllmCallDestroy(xllm_call* pCall);

xllm_result xllmClientComplete(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
);

/* Builds the provider JSON body without credentials. Free with xllmFree(). */
char* xllmClientBuildRequestJson(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError);
void xllmFree(void* pMemory);

#ifdef __cplusplus
}
#endif

#endif
