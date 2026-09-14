#ifndef XLLM_H
#define XLLM_H

/*
 * xllm v3: the shared foundation for agent workloads.
 *
 * One model call, one API, three provider wire dialects (Chat Completions,
 * OpenAI Responses, Anthropic Messages). The library owns request
 * serialization, HTTP transport, SSE decoding, tool-call assembly and
 * diagnostics. It deliberately does not execute tools, manage conversation
 * history, compact context, or run an agent loop.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XLLM_VERSION_MAJOR 3
#define XLLM_VERSION_MINOR 0
#define XLLM_VERSION_PATCH 0

typedef struct xllm_client xllm_client;
typedef struct xllm_call xllm_call;
typedef struct xcancel xcancel;
typedef struct xllm_request xllm_request;
/* Borrowed XRT runtime handles; only meaningful when building against XRT. */
typedef struct xnetengine xnetengine;
typedef struct xfuture xfuture;
typedef struct xx509store xx509store;

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
    XLLM_ERROR_PARSE,
    /* Session-layer additions (appended: existing values stay stable). */
    XLLM_ERROR_LIMIT,   /* a single message exceeds the configured byte cap */
    XLLM_ERROR_HOOK     /* a host-supplied session hook failed or re-entered */
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
    uint64_t uFirstTokenMs;
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
    char sDialect[24];
} xllm_diagnostics;

typedef struct xllm_error {
    xllm_error_code eCode;
    int32_t iTransportStatus;
    int32_t iHttpStatus;
    char sMessage[512];
    char sProviderMessage[2048];
    char sProviderCode[64];
    char sProviderType[64];
    char sRequestId[160];
    xllm_diagnostics tDiagnostics;
} xllm_error;

typedef enum xllm_role {
    XLLM_ROLE_SYSTEM = 0,
    XLLM_ROLE_USER,
    XLLM_ROLE_ASSISTANT,
    XLLM_ROLE_TOOL
} xllm_role;

typedef enum xllm_provider {
    XLLM_PROVIDER_OPENAI_COMPAT = 0,
    XLLM_PROVIDER_GLM,
    XLLM_PROVIDER_OPENAI_RESPONSES,
    XLLM_PROVIDER_ANTHROPIC
} xllm_provider;

/* ------------------------------------------------------------------ */
/* Multimodal content parts                                            */
/* ------------------------------------------------------------------ */

typedef enum xllm_part_kind {
    XLLM_PART_TEXT = 0,   /* UTF-8 text */
    XLLM_PART_REASONING,  /* replayed assistant reasoning text */
    XLLM_PART_IMAGE,      /* image bytes or URL reference */
    XLLM_PART_AUDIO,      /* audio bytes */
    XLLM_PART_FILE,       /* generic file/document bytes */
    XLLM_PART_NATIVE      /* provider-native JSON part, replayed verbatim */
} xllm_part_kind;

typedef struct xllm_part {
    xllm_part_kind eKind;
    char* sText;          /* TEXT/REASONING: content; NATIVE: raw JSON object */
    char* sNativeType;    /* NATIVE: provider block type for exact replay */
    char* sMediaType;     /* "image/png", "audio/wav", ... */
    char* sSourceUrl;     /* remote URL reference when the dialect supports it */
    char* sDetail;        /* image detail hint: auto/low/high */
    uint8_t* pData;       /* owned bytes for IMAGE/AUDIO/FILE */
    size_t iDataSize;
} xllm_part;

void xllmPartInit(xllm_part* pPart, xllm_part_kind eKind);
bool xllmPartSetText(xllm_part* pPart, const char* sText);
bool xllmPartSetImageData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
bool xllmPartSetImageUrl(xllm_part* pPart, const char* sUrl, const char* sMediaType);
bool xllmPartSetAudioData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
bool xllmPartSetFileData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
bool xllmPartSetNative(xllm_part* pPart, const char* sNativeType, const char* sJson);

/* ------------------------------------------------------------------ */
/* Messages: text fast path + optional parts                           */
/* ------------------------------------------------------------------ */

/* A message carries either a plain text content (sContent, the fast path
 * session and memory build on) or a parts array. When iPartCount > 0 the
 * parts are authoritative and sContent must be NULL; every setter keeps
 * that invariant. */
typedef struct xllm_tool_call {
    char* sId;
    char* sName;
    char* sArgumentsJson;
} xllm_tool_call;

typedef struct xllm_message {
    xllm_role eRole;
    /* Text content must be valid UTF-8; the setters reject anything else
     * before it can ride into provider JSON. */
    char* sContent;            /* text fast path (NULL once parts are used) */
    char* sReasoningContent;   /* assistant reasoning replay text */
    char* sToolCallId;         /* tool role: result correlation */
    xllm_tool_call* pToolCalls;
    size_t iToolCallCount;
    size_t iToolCallCap;
    xllm_part* pParts;
    size_t iPartCount;
    size_t iPartCap;
    char* sNative;             /* whole-message provider-native replay blob */
} xllm_message;

/* ------------------------------------------------------------------ */
/* Tools                                                               */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Usage, finish, response                                             */
/* ------------------------------------------------------------------ */

typedef struct xllm_usage {
    uint64_t uInputTokens;
    uint64_t uOutputTokens;
    uint64_t uTotalTokens;
    uint64_t uCachedInputTokens;
    uint64_t uCacheWriteTokens;
    uint64_t uReasoningTokens;
} xllm_usage;

typedef enum xllm_finish {
    XLLM_FINISH_STOP = 0,
    XLLM_FINISH_LENGTH,
    XLLM_FINISH_TOOL_CALLS,
    XLLM_FINISH_CONTENT_FILTER,
    XLLM_FINISH_REFUSAL,
    XLLM_FINISH_OTHER
} xllm_finish;

typedef enum xllm_block_kind {
    XLLM_BLOCK_TEXT = 0,
    XLLM_BLOCK_REASONING,
    XLLM_BLOCK_TOOL_CALL
} xllm_block_kind;

/* Responses arrive as ordered blocks so callers can render interleaved
 * text, reasoning, and tool calls in arrival order. */
typedef struct xllm_block {
    xllm_block_kind eKind;
    char* sText;          /* TEXT / REASONING */
    size_t iToolIndex;    /* TOOL_CALL: index into xllm_response.pToolCalls */
    char* sNative;        /* provider-native replay blob for this block */
} xllm_block;

typedef struct xllm_stats {
    xllm_usage tUsage;
    uint64_t uConnectMs;
    uint64_t uFirstByteMs;
    uint64_t uFirstTokenMs;
    uint64_t uTotalMs;
    double fOutputTokensPerSec;
    uint64_t uRequestBytes;
    uint64_t uResponseBytes;
    uint32_t uAttempts;
    bool bReusedConnection;
} xllm_stats;

typedef struct xllm_response {
    char* sId;
    char* sModel;
    char* sContent;            /* convenience: all TEXT blocks joined */
    char* sReasoningContent;   /* convenience: all REASONING blocks joined */
    char* sRefusal;            /* provider safety refusal, when reported */
    char* sFinishReason;       /* raw provider finish value */
    xllm_finish eFinish;       /* normalized finish */
    char* sRequestId;
    xllm_block* pBlocks;
    size_t iBlockCount;
    xllm_tool_call* pToolCalls;
    size_t iToolCallCount;
    size_t iToolCallCap;
    xllm_usage tUsage;
    uint32_t uHttpStatus;
    xllm_diagnostics tDiagnostics;
    xllm_stats tStats;
} xllm_response;

/* ------------------------------------------------------------------ */
/* Streaming events                                                    */
/* ------------------------------------------------------------------ */

typedef enum xllm_event_kind {
    XLLM_EVENT_RESPONSE_START = 0,
    XLLM_EVENT_TEXT_DELTA,
    XLLM_EVENT_REASONING_DELTA,
    XLLM_EVENT_TOOL_CALL_DELTA,
    XLLM_EVENT_BLOCK_META,   /* block opened or closed (arrival order) */
    XLLM_EVENT_USAGE,
    XLLM_EVENT_RESPONSE_DONE
} xllm_event_kind;

typedef struct xllm_event {
    xllm_event_kind eKind;
    union {
        struct {
            size_t iBlock;          /* block the delta belongs to */
            const char* sData;
            size_t iLen;
        } tText;
        struct {
            size_t iIndex;          /* tool-call index */
            size_t iBlock;
            const char* sIdDelta;
            const char* sNameDelta;
            const char* sArgumentsDelta;
        } tToolCall;
        struct {
            size_t iBlock;
            xllm_block_kind eKind;
            bool bEnd;
        } tBlockMeta;
        xllm_usage tUsage;
        struct {
            uint32_t uHttpStatus;
            const char* sRequestId;
        } tResponse;
    } as;
} xllm_event;

typedef bool (*xllm_event_fn)(void* pUserData, const xllm_event* pEvent);

/* Stream callbacks fire on the client engine's background workers, never on
 * the thread that started or waits on the call. Callbacks must be thread
 * safe with respect to any state they touch, must not block (they occupy an
 * engine worker), and returning false cancels the call. */
typedef struct xllm_stream_callbacks {
    void* pUserData;
    xllm_event_fn OnEvent;
} xllm_stream_callbacks;

/* ------------------------------------------------------------------ */
/* Model profiles                                                      */
/* ------------------------------------------------------------------ */

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
/* Chat Completions wire conventions of reasoning-first models: the endpoint
 * takes max_completion_tokens instead of max_tokens, and top-level instructions
 * ride the developer role instead of system. Mirrors of the legacy protocol
 * keep the legacy spellings, so these bits are opt-in through a profile. */
#define XLLM_CAP_MAX_COMPLETION_TOKENS (1ull << 9)
#define XLLM_CAP_DEVELOPER_ROLE        (1ull << 10)
/* The dialect accepts binary media input parts. Despite the historical
 * name this bit gates image, audio, and file parts alike. */
#define XLLM_CAP_IMAGE_IN              (1ull << 11)

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

void xllmModelProfileInit(xllm_model_profile* pProfile);
const xllm_model_profile* xllmModelProfileBuiltin(const char* sIdOrModel);
bool xllmModelProfileValidate(const xllm_model_profile* pProfile, xllm_error* pError);
bool xllmModelProfileSupports(const xllm_model_profile* pProfile, xllm_capability_flags uRequired);
bool xllmModelProfileValidateRequest(const xllm_model_profile* pProfile,
    const xllm_request* pRequest, xllm_error* pError);

void xllmErrorInit(xllm_error* pError);
const char* xllmErrorCodeName(xllm_error_code eCode);
const char* xllmFinishReasonName(xllm_finish eFinish);
bool xllmErrorRetryable(const xllm_error* pError);

/* ------------------------------------------------------------------ */
/* Message and request construction                                    */
/* ------------------------------------------------------------------ */

void xllmMessageInit(xllm_message* pMessage, xllm_role eRole);
void xllmMessageUnit(xllm_message* pMessage);
bool xllmMessageSetContent(xllm_message* pMessage, const char* sContent);
bool xllmMessageSetReasoning(xllm_message* pMessage, const char* sReasoningContent);
bool xllmMessageSetToolCallId(xllm_message* pMessage, const char* sToolCallId);
bool xllmMessageAddToolCall(xllm_message* pMessage, const char* sId, const char* sName, const char* sArgumentsJson);
bool xllmMessageAddPart(xllm_message* pMessage, const xllm_part* pPart);
bool xllmMessageSetNative(xllm_message* pMessage, const char* sNativeJson);

/* Convenience: build a history assistant message from a completed response
 * (text, reasoning, tool calls; provider-native replay blobs are preserved). */
bool xllmMessageFromResponse(const xllm_response* pResponse, xllm_message* pMessage);

typedef enum xllm_json_mode {
    XLLM_JSON_NONE = 0,
    XLLM_JSON_OBJECT
} xllm_json_mode;

/* Borrowed extra transport header. */
typedef struct xllm_header {
    const char* sName;
    const char* sValue;
} xllm_header;

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
    uint32_t uReasoningBudgetTokens;   /* thinking budget for dialects that take one */
    uint32_t uMaxOutputTokens;
    double fTemperature;
    bool bHasTemperature;
    double fTopP;
    bool bHasTopP;
    char* sStop;                       /* stop sequence */
    bool bParallelToolCalls;
    xllm_tool_choice eToolChoice;
    xllm_json_mode eJsonMode;
    bool bStream;                      /* default true */
    const xllm_header* pExtraHeaders;  /* borrowed */
    size_t iExtraHeaderCount;
    char* sExtraBodyJson;              /* owned raw JSON object, shallow-merged */
    /* Borrowed cancellation token; it must outlive this request's model call. */
    xcancel* pCancel;
    /* Absolute xrtClock() deadline in microseconds; UINT64_MAX disables it. */
    uint64_t uDeadline;
} xllm_request;

void xllmRequestInit(xllm_request* pRequest);
void xllmRequestUnit(xllm_request* pRequest);
bool xllmRequestSetModel(xllm_request* pRequest, const char* sModel);
bool xllmRequestSetReasoningEffort(xllm_request* pRequest, const char* sEffort);
bool xllmRequestSetStop(xllm_request* pRequest, const char* sStop);
bool xllmRequestSetExtraBody(xllm_request* pRequest, const char* sJsonObject);
void xllmRequestSetCancel(xllm_request* pRequest, xcancel* pCancel);
void xllmRequestSetDeadline(xllm_request* pRequest, uint64_t uDeadline);
bool xllmRequestSetToolChoice(xllm_request* pRequest, xllm_tool_choice eChoice, const char* sNamedTool);
bool xllmRequestAddMessage(xllm_request* pRequest, const xllm_message* pMessage);
bool xllmRequestAddTextMessage(xllm_request* pRequest, xllm_role eRole, const char* sContent);
bool xllmRequestAddToolResult(xllm_request* pRequest, const char* sToolCallId, const char* sContent);
bool xllmRequestAddTool(xllm_request* pRequest, const char* sName, const char* sDescription, const char* sParametersJson, bool bStrict);

void xllmResponseDestroy(xllm_response* pResponse);

/* Deterministic lexical token estimates used by budget governance. */
uint64_t xllmEstimateTextTokens(const char* sText);
uint64_t xllmEstimateMessageTokens(const xllm_message* pMessage);

/* ------------------------------------------------------------------ */
/* Client lifecycle                                                    */
/* ------------------------------------------------------------------ */

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
    uint32_t uMaxIdleConnections;   /* 0 = default 4; capped at 8 */
    /* Borrowed shared XRT engine; multiple clients may share one. NULL = the
     * client creates and owns a private engine. Must outlive the client. */
    xnetengine* pNetEngine;
    bool bVerifyPeer;
    /* Private-CA trust: PEM text (may carry a chain) or a borrowed store.
     * Either one implies verification; pX509Store wins over sCaPem. */
    const char* sCaPem;
    xx509store* pX509Store;
    xllm_provider eProvider;
    /* Optional borrowed profile. When present, model/provider/limits are
     * validated and the client retains an owned snapshot. */
    const xllm_model_profile* pModelProfile;
} xllm_client_config;

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
/* The call's completion future; transport runs on the engine workers.
 * The returned future is borrowed: valid until xllmCallWait/Destroy. */
xfuture* xllmCallFuture(xllm_call* pCall);
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
