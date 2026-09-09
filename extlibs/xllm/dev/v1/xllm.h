#ifndef XLLM_H
#define XLLM_H

#if defined(__has_include)
#  if __has_include("xrt.h")
#    include "xrt.h"
#  elif __has_include("lib/xrt.h")
#    include "lib/xrt.h"
#  else
#    error "xllm requires xrt.h to be available in the include path"
#  endif
#else
#  include "lib/xrt.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XLLM_API
#ifdef XXAPI
#define XLLM_API XXAPI
#else
#define XLLM_API
#endif
#endif

#define XLLM_VERSION_MAJOR 0
#define XLLM_VERSION_MINOR 1
#define XLLM_VERSION_PATCH 0

#define XLLM_ADAPTER_OPENAI_COMPAT "openai_compat"
#define XLLM_ADAPTER_GLM_NATIVE "glm_native"
#define XLLM_ADAPTER_MINIMAX_NATIVE "minimax_native"
#define XLLM_ADAPTER_KIMI_NATIVE "kimi_native"
#define XLLM_ADAPTER_GEMINI_NATIVE "gemini_native"
#define XLLM_ADAPTER_VERTEX_GEMINI_NATIVE "vertex_gemini_native"
#define XLLM_ADAPTER_QWEN_NATIVE "qwen_native"
#define XLLM_ADAPTER_DOUBAO_NATIVE "doubao_native"
#define XLLM_ADAPTER_ANTHROPIC_NATIVE "anthropic_native"
#define XLLM_ADAPTER_OLLAMA_NATIVE "ollama_native"

typedef struct xllm_runtime xllm_runtime;
typedef struct xllm_cancel_token xllm_cancel_token;

typedef struct {
    bool bSet;
    bool bValue;
} xllm_opt_bool;

typedef struct {
    bool bSet;
    int32 iValue;
} xllm_opt_i32;

typedef struct {
    bool bSet;
    uint32 iValue;
} xllm_opt_u32;

typedef struct {
    bool bSet;
    uint64 uValue;
} xllm_opt_u64;

typedef struct {
    bool bSet;
    double fValue;
} xllm_opt_f64;

typedef enum {
    XLLM_LOG_ERROR = 1,
    XLLM_LOG_WARN,
    XLLM_LOG_INFO,
    XLLM_LOG_DEBUG,
    XLLM_LOG_TRACE
} xllm_log_level;

typedef enum {
    XLLM_LOG_EVENT_UNKNOWN = 0,
    XLLM_LOG_EVENT_RUNTIME_CREATE,
    XLLM_LOG_EVENT_RUNTIME_DESTROY,
    XLLM_LOG_EVENT_PROVIDER_REQUEST_START,
    XLLM_LOG_EVENT_PROVIDER_RESPONSE_COMPLETE,
    XLLM_LOG_EVENT_PROVIDER_RESPONSE_FAILED,
    XLLM_LOG_EVENT_PROVIDER_RETRY_SCHEDULED,
    XLLM_LOG_EVENT_STREAM_EVENT,
    XLLM_LOG_EVENT_SESSION_COMPACT_TRIGGERED,
    XLLM_LOG_EVENT_SESSION_COMPACT_RESULT,
    XLLM_LOG_EVENT_TOOL_LOOP_ROUND,
    XLLM_LOG_EVENT_TOOL_LOOP_EXECUTE,
    XLLM_LOG_EVENT_TOOL_LOOP_STOP,
    XLLM_LOG_EVENT_MEMORY_INGEST,
    XLLM_LOG_EVENT_MEMORY_SEARCH,
    XLLM_LOG_EVENT_MEMORY_HEALTH_CHECK,
    XLLM_LOG_EVENT_WORKSPACE_SYNC,
    XLLM_LOG_EVENT_WATCHER_EVENT
} xllm_log_event;

typedef enum {
    XLLM_TRACE_EVENT = 1,
    XLLM_TRACE_REQUEST,
    XLLM_TRACE_RESPONSE,
    XLLM_TRACE_STREAM,
    XLLM_TRACE_COMPACT,
    XLLM_TRACE_TOOL_LOOP
} xllm_trace_kind;

typedef enum {
    XLLM_DEBUG_NONE = 0,
    XLLM_DEBUG_HEADERS = 1,
    XLLM_DEBUG_BODY = 2,
    XLLM_DEBUG_WIRE = 3
} xllm_debug_mode;

typedef enum {
    XLLM_REDACT_DEFAULT = 0,
    XLLM_REDACT_OFF,
    XLLM_REDACT_STRICT
} xllm_redact_mode;

typedef void *(*xllm_malloc_fn)(void *pCtx, size_t iSize);
typedef void *(*xllm_realloc_fn)(void *pCtx, void *pPtr, size_t iSize);
typedef void (*xllm_free_fn)(void *pCtx, void *pPtr);

typedef struct {
    xllm_malloc_fn pfnMalloc;
    xllm_realloc_fn pfnRealloc;
    xllm_free_fn pfnFree;
    void *pCtx;
} xllm_allocator;

typedef void (*xllm_log_callback)(
    void *pCtx,
    xllm_log_level eLevel,
    const char *sComponent,
    const char *sMessage
);

typedef void (*xllm_trace_callback)(
    void *pCtx,
    xllm_trace_kind eKind,
    const xvalue *pPayload
);

typedef struct {
    const char *sName;
    const char *sValue;
} xllm_header;

typedef enum {
    XLLM_AUTH_NONE = 0,
    XLLM_AUTH_BEARER,
    XLLM_AUTH_API_KEY_HEADER
} xllm_auth_kind;

typedef struct {
    xllm_auth_kind eKind;
    const char *sSecret;
    const char *sHeaderName;
    const char *sScheme;
} xllm_auth;

typedef enum {
    XLLM_PROXY_UNSPECIFIED = 0,
    XLLM_PROXY_NONE,
    XLLM_PROXY_SOCKS5,
    XLLM_PROXY_HTTP_CONNECT
} xllm_proxy_kind;

typedef struct {
    xllm_opt_u32 tConnectTimeoutMs;
    xllm_opt_u32 tReadTimeoutMs;
    xllm_opt_bool tVerifyPeer;
    xllm_proxy_kind eProxyKind;
    const char *sProxyHost;
    xllm_opt_u32 tProxyPort;
    const char *sProxyUser;
    const char *sProxyPass;
    const char *sCaBundlePath;
    const char *sClientCertPath;
    const char *sClientKeyPath;
    xvalue tVendorExtra;
} xllm_transport_options;

typedef struct {
    const char *sOpenAIOrganizationId;
    const char *sOpenAIProjectId;
    const char *sAnthropicApiVersion;
    const char **psAnthropicBetaHeaders;
    size_t iAnthropicBetaHeaderCount;
    xvalue tVendorExtra;
} xllm_provider_options;

typedef enum {
    XLLM_CAP_MODE_AUTO = 0,
    XLLM_CAP_MODE_MERGE,
    XLLM_CAP_MODE_EXACT
} xllm_cap_mode;

typedef enum {
    XLLM_PARAM_RULE_UNSPECIFIED = 0,
    XLLM_PARAM_RULE_UNSUPPORTED,
    XLLM_PARAM_RULE_FIXED,
    XLLM_PARAM_RULE_RANGE,
    XLLM_PARAM_RULE_PASSTHROUGH
} xllm_param_rule_kind;

typedef struct {
    xllm_param_rule_kind eKind;
    double fFixed;
    double fMin;
    double fMax;
} xllm_float_rule;

typedef struct {
    xllm_param_rule_kind eKind;
    uint32 uFixed;
    uint32 uMin;
    uint32 uMax;
} xllm_u32_rule;

typedef enum {
    XLLM_WINDOW_UNSPECIFIED = 0,
    XLLM_WINDOW_SHARED_CONTEXT,
    XLLM_WINDOW_SPLIT_INPUT_OUTPUT
} xllm_window_mode;

typedef uint64 xllm_capability_flags;

#define XLLM_CAP_TEXT_IN              (1ull << 0)
#define XLLM_CAP_IMAGE_IN             (1ull << 1)
#define XLLM_CAP_FILE_IN              (1ull << 2)
#define XLLM_CAP_AUDIO_IN             (1ull << 3)
#define XLLM_CAP_VIDEO_IN             (1ull << 4)
#define XLLM_CAP_TOOL_RESULT_IN       (1ull << 5)
#define XLLM_CAP_TEXT_OUT             (1ull << 6)
#define XLLM_CAP_IMAGE_OUT            (1ull << 7)
#define XLLM_CAP_FILE_OUT             (1ull << 8)
#define XLLM_CAP_AUDIO_OUT            (1ull << 9)
#define XLLM_CAP_VIDEO_OUT            (1ull << 10)
#define XLLM_CAP_JSON_OUT             (1ull << 11)
#define XLLM_CAP_TOOL_CALL_OUT        (1ull << 12)
#define XLLM_CAP_THINKING_SUMMARY_OUT (1ull << 13)
#define XLLM_CAP_THINKING_FULL_OUT    (1ull << 14)
#define XLLM_CAP_STREAM               (1ull << 15)
#define XLLM_CAP_REASONING_CONTROL    (1ull << 16)
#define XLLM_CAP_PARALLEL_TOOL_CALL   (1ull << 17)
#define XLLM_CAP_CITATION_OUT         (1ull << 18)

typedef struct {
    xllm_capability_flags uFlags;
    const char **psSupportedMimeTypes;
    size_t iSupportedMimeTypeCount;
    xllm_window_mode eWindowMode;
    uint32 uMaxContextTokens;
    uint32 uMaxInputTokens;
    uint32 uMaxOutputTokens;
    uint32 uRecommendedOutputReserve;
    uint32 uMaxPartsPerMessage;
    uint32 uMaxImages;
    uint32 uMaxFiles;
    uint64 uMaxPartBytes;
    const char *sTokenizerId;
    xllm_float_rule tTemperatureRule;
    xllm_float_rule tTopPRule;
    xllm_u32_rule tMaxOutputTokensRule;
    xvalue tVendorExtra;
} xllm_model_caps;

typedef struct {
    const char *sModelId;
    const char *sAliasOf;
    xllm_cap_mode eCapMode;
    xllm_model_caps tCaps;
    xvalue tVendorExtra;
} xllm_model_binding;

typedef struct {
    xllm_model_binding tText;
    xllm_model_binding tMultimodal;
} xllm_profile_models;

typedef enum {
    XLLM_REASONING_DEFAULT = 0,
    XLLM_REASONING_OFF,
    XLLM_REASONING_LOW,
    XLLM_REASONING_MEDIUM,
    XLLM_REASONING_HIGH
} xllm_reasoning_level;

typedef struct {
    xllm_opt_bool tEnabled;
    xllm_reasoning_level eLevel;
    xllm_opt_u32 tBudgetTokens;
    xllm_opt_bool tExposeThinking;
    xvalue tVendorExtra;
} xllm_reasoning_options;

typedef enum {
    XLLM_RESPONSE_TEXT = 0,
    XLLM_RESPONSE_JSON,
    XLLM_RESPONSE_JSON_SCHEMA
} xllm_response_format_kind;

typedef struct {
    xllm_response_format_kind eKind;
    const char *sSchemaName;
    xvalue tJsonSchema;
    xvalue tVendorExtra;
} xllm_response_format;

typedef struct {
    xllm_opt_f64 tTemperature;
    xllm_opt_f64 tTopP;
    xllm_opt_u32 tMaxOutputTokens;
    xllm_opt_u32 tSeed;
    const char **psStop;
    size_t iStopCount;
} xllm_generation_params;

typedef struct {
    xllm_generation_params tGeneration;
    xllm_reasoning_options tReasoning;
    xllm_response_format tResponseFormat;
    xvalue tVendorExtra;
} xllm_profile_defaults;

typedef struct {
    const char *sId;
    const char *sName;
    const char *sProvider;
    const char *sAdapter;
    const char *sBaseUrl;
    xllm_auth tAuth;
    xllm_header *pDefaultHeaders;
    size_t iDefaultHeaderCount;
    xllm_provider_options tProviderOptions;
    xllm_transport_options tTransport;
    xllm_profile_models tModels;
    xllm_profile_defaults tDefaults;
    xvalue tVendorExtra;
} xllm_profile;

typedef enum {
    XLLM_SLOT_AUTO = 0,
    XLLM_SLOT_TEXT,
    XLLM_SLOT_MULTIMODAL
} xllm_slot;

typedef enum {
    XLLM_ROLE_SYSTEM = 1,
    XLLM_ROLE_USER,
    XLLM_ROLE_ASSISTANT,
    XLLM_ROLE_TOOL
} xllm_role;

typedef enum {
    XLLM_PART_TEXT = 1,
    XLLM_PART_IMAGE,
    XLLM_PART_FILE,
    XLLM_PART_AUDIO,
    XLLM_PART_VIDEO,
    XLLM_PART_JSON
} xllm_part_kind;

typedef enum {
    XLLM_SOURCE_INLINE_TEXT = 1,
    XLLM_SOURCE_INLINE_BYTES,
    XLLM_SOURCE_URL,
    XLLM_SOURCE_PROVIDER_FILE_ID
} xllm_source_kind;

typedef struct {
    xllm_source_kind eKind;
    const char *sMimeType;
    const char *sName;
    union {
        const char *sText;
        struct {
            const void *pData;
            size_t iSize;
        } tBytes;
        const char *sUrl;
        const char *sFileId;
    } as;
} xllm_data_source;

typedef struct {
    xllm_part_kind eKind;
    union {
        xllm_data_source tSource;
        xvalue tJsonValue;
    } as;
    xvalue tVendorExtra;
} xllm_content_part;

typedef struct {
    const char *sCallId;
    const char *sToolId;
    const char *sToolName;
    const char *sArgumentsJson;
    xvalue tContinuation;
    xvalue tVendorExtra;
} xllm_tool_call;

typedef struct {
    xllm_role eRole;
    const char *sToolCallId;
    const char *sToolName;
    xllm_content_part *pParts;
    size_t iPartCount;
    xllm_tool_call *pToolCalls;
    size_t iToolCallCount;
    xvalue tVendorExtra;
} xllm_message;

typedef enum {
    XLLM_CONTEXT_SYSTEM = 1,
    XLLM_CONTEXT_SESSION_SUMMARY,
    XLLM_CONTEXT_HISTORY,
    XLLM_CONTEXT_MEMORY,
    XLLM_CONTEXT_KNOWLEDGE,
    XLLM_CONTEXT_USER,
    XLLM_CONTEXT_TOOL_RESULT
} xllm_context_block_kind;

typedef struct {
    xllm_context_block_kind eKind;
    int32 iPriority;
    bool bPinned;
    xllm_message *pMessages;
    size_t iMessageCount;
    xvalue tVendorExtra;
} xllm_context_block;

typedef enum {
    XLLM_TOOL_CLIENT = 0,
    XLLM_TOOL_PROVIDER
} xllm_tool_kind;

typedef struct {
    const char *sToolId;
    const char *sWireName;
    const char *sDescription;
    xllm_tool_kind eKind;
    xvalue tInputSchema;
    xvalue tVendorExtra;
} xllm_tool_def;

typedef enum {
    XLLM_TOOL_CHOICE_AUTO = 0,
    XLLM_TOOL_CHOICE_NONE,
    XLLM_TOOL_CHOICE_REQUIRED,
    XLLM_TOOL_CHOICE_NAMED
} xllm_tool_choice_mode;

typedef struct {
    xllm_tool_choice_mode eMode;
    const char *sToolName;
    bool bAllowParallel;
} xllm_tool_policy;

typedef enum {
    XLLM_SYSTEM_INHERIT = 0,
    XLLM_SYSTEM_REPLACE,
    XLLM_SYSTEM_APPEND
} xllm_system_mode;

typedef struct {
    const char *sProfileId;
    xllm_slot eSlot;
    xllm_message *pMessages;
    size_t iMessageCount;
    xllm_context_block *pContextBlocks;
    size_t iContextBlockCount;
    xllm_tool_def *pTools;
    size_t iToolCount;
    xllm_tool_policy tToolPolicy;
    xllm_generation_params tGeneration;
    xllm_response_format tResponseFormat;
    xllm_reasoning_options tReasoning;
    xvalue tVendorExtra;
} xllm_request;

typedef struct {
    xllm_slot eSlot;
    const char *sSystemPrompt;
    xllm_system_mode eSystemMode;
    xllm_message *pMessages;
    size_t iMessageCount;
    xllm_context_block *pContextBlocks;
    size_t iContextBlockCount;
    xllm_tool_def *pTools;
    size_t iToolCount;
    xllm_tool_policy tToolPolicy;
    xllm_generation_params tGeneration;
    xllm_response_format tResponseFormat;
    xllm_reasoning_options tReasoning;
    xvalue tVendorExtra;
} xllm_turn;

typedef xllm_turn xllm_turn_request;

typedef enum {
    XLLM_STREAM_AUTO = 0,
    XLLM_STREAM_OFF,
    XLLM_STREAM_PREFER,
    XLLM_STREAM_REQUIRE
} xllm_stream_mode;

typedef enum {
    XLLM_ARTIFACT_REFERENCE_ONLY = 0,
    XLLM_ARTIFACT_INLINE_SMALL,
    XLLM_ARTIFACT_STREAM_TO_SINK
} xllm_artifact_policy;

typedef enum {
    XLLM_LOCAL_FILE_AUTO = 0,
    XLLM_LOCAL_FILE_INLINE_FIRST,
    XLLM_LOCAL_FILE_UPLOAD_REUSE_FIRST
} xllm_local_file_policy;

typedef struct {
    const char *sArtifactId;
    const char *sMimeType;
    const char *sName;
    uint64 uExpectedSize;
    uint32 uOutputIndex;
    xvalue tVendorExtra;
} xllm_artifact_info;

typedef bool (*xllm_artifact_begin_fn)(void *pCtx, const xllm_artifact_info *pInfo);
typedef bool (*xllm_artifact_write_fn)(void *pCtx, const char *sArtifactId, const void *pData, size_t iSize);
typedef bool (*xllm_artifact_end_fn)(void *pCtx, const char *sArtifactId, bool bCompleted);

typedef struct {
    void *pCtx;
    xllm_artifact_begin_fn pfnBegin;
    xllm_artifact_write_fn pfnWrite;
    xllm_artifact_end_fn pfnEnd;
} xllm_artifact_sink;

typedef enum {
    XLLM_ERROR_NONE = 0,
    XLLM_ERROR_AUTH,
    XLLM_ERROR_QUOTA,
    XLLM_ERROR_RATE_LIMIT,
    XLLM_ERROR_TIMEOUT,
    XLLM_ERROR_NETWORK,
    XLLM_ERROR_CANCELLED,
    XLLM_ERROR_INVALID_REQUEST,
    XLLM_ERROR_UNSUPPORTED_CAPABILITY,
    XLLM_ERROR_UNSUPPORTED_INPUT_TYPE,
    XLLM_ERROR_UNSUPPORTED_MIME_TYPE,
    XLLM_ERROR_INPUT_TOO_LARGE,
    XLLM_ERROR_TOO_MANY_INPUT_PARTS,
    XLLM_ERROR_MISSING_MULTIMODAL_MODEL,
    XLLM_ERROR_MODEL_NOT_FOUND,
    XLLM_ERROR_UPSTREAM_4XX,
    XLLM_ERROR_UPSTREAM_5XX,
    XLLM_ERROR_PARSE,
    XLLM_ERROR_INTERNAL,
    XLLM_ERROR_SESSION_CONTEXT_OVERFLOW,
    XLLM_ERROR_SESSION_COMPACT_FAILED,
    XLLM_ERROR_SESSION_SUMMARY_FAILED,
    XLLM_ERROR_SESSION_REQUIRES_MODEL_LIMITS
} xllm_error_code;

typedef struct {
    xllm_error_code eCode;
    int32 iStatus;
    int32 iHttpStatus;
    const char *sMessage;
    const char *sProviderCode;
    const char *sProviderMessage;
    const char *sRequestId;
    int32 iMessageIndex;
    int32 iPartIndex;
    xllm_capability_flags uRequiredCapability;
    const char *sSelectedModel;
    const char *sMimeType;
    xvalue tVendorExtra;
} xllm_error;

typedef enum {
    XLLM_OUTPUT_MESSAGE = 1,
    XLLM_OUTPUT_THINKING,
    XLLM_OUTPUT_TOOL_CALL,
    XLLM_OUTPUT_REFUSAL
} xllm_output_kind;

typedef struct {
    xllm_content_part *pParts;
    size_t iPartCount;
} xllm_output_message;

typedef struct {
    bool bVisible;
    const char *sFormat;
    const char *sText;
    xvalue tVendorExtra;
} xllm_output_thinking;

typedef struct {
    const char *sCallId;
    const char *sToolId;
    const char *sToolName;
    const char *sArgumentsJson;
    xvalue tContinuation;
    xvalue tVendorExtra;
} xllm_output_tool_call;

typedef struct {
    const char *sText;
    const char *sCategory;
    xvalue tVendorExtra;
} xllm_output_refusal;

typedef struct {
    xllm_output_kind eKind;
    union {
        xllm_output_message tMessage;
        xllm_output_thinking tThinking;
        xllm_output_tool_call tToolCall;
        xllm_output_refusal tRefusal;
    } as;
} xllm_output_item;

typedef struct {
    uint32 uInputTokens;
    uint32 uOutputTokens;
    uint32 uReasoningTokens;
    uint32 uCachedInputTokens;
    xvalue tVendorExtra;
} xllm_usage;

typedef enum {
    XLLM_STATUS_COMPLETED = 0,
    XLLM_STATUS_INCOMPLETE,
    XLLM_STATUS_TOOL_CALL_REQUIRED,
    XLLM_STATUS_REFUSED,
    XLLM_STATUS_CONTENT_FILTERED,
    XLLM_STATUS_CANCELLED,
    XLLM_STATUS_ERRORED
} xllm_response_status;

typedef struct {
    const char *sText;
    const char *sCategory;
    xvalue tVendorExtra;
} xllm_refusal_info;

typedef struct {
    const char *sBlockReason;
    xvalue tRatings;
    xvalue tVendorExtra;
} xllm_safety_info;

typedef struct {
    xllm_generation_params tGeneration;
    xllm_reasoning_options tReasoning;
    xllm_response_format tResponseFormat;
    xllm_stream_mode eStreamMode;
    xvalue tVendorExtra;
} xllm_effective_params;

typedef struct {
    const char *sId;
    const char *sProvider;
    const char *sProfileId;
    const char *sModel;
    xllm_response_status eStatus;
    const char *sFinishReason;
    xllm_output_item *pOutputs;
    size_t iOutputCount;
    const char *sVisibleText;
    xllm_usage tUsage;
    xllm_refusal_info tRefusal;
    xllm_safety_info tSafety;
    xllm_effective_params tEffectiveParams;
    bool bHasError;
    xllm_error tError;
    xvalue tRaw;
    xvalue tVendorExtra;
} xllm_response;

typedef enum {
    XLLM_EVENT_START = 1,
    XLLM_EVENT_OUTPUT_BEGIN,
    XLLM_EVENT_TEXT_DELTA,
    XLLM_EVENT_THINKING_DELTA,
    XLLM_EVENT_TOOL_CALL_DELTA,
    XLLM_EVENT_TOOL_CALL_READY,
    XLLM_EVENT_ARTIFACT_BEGIN,
    XLLM_EVENT_ARTIFACT_CHUNK,
    XLLM_EVENT_ARTIFACT_READY,
    XLLM_EVENT_REFUSAL,
    XLLM_EVENT_USAGE,
    XLLM_EVENT_OUTPUT_END,
    XLLM_EVENT_ERROR,
    XLLM_EVENT_END
} xllm_event_type;

typedef struct {
    xllm_event_type eType;
    bool bSynthetic;
    uint32 uOutputIndex;
    union {
        struct {
            const char *sResponseId;
            const char *sModel;
        } tStart;
        struct {
            xllm_output_kind eKind;
        } tOutputBegin;
        struct {
            const char *sText;
        } tTextDelta;
        struct {
            const char *sText;
            const char *sFormat;
        } tThinkingDelta;
        struct {
            const char *sCallId;
            const char *sToolId;
            const char *sToolName;
            const char *sArgumentsDelta;
        } tToolCallDelta;
        struct {
            xllm_output_tool_call tToolCall;
        } tToolCallReady;
        struct {
            xllm_artifact_info tInfo;
        } tArtifactBegin;
        struct {
            const char *sArtifactId;
            const void *pData;
            size_t iSize;
        } tArtifactChunk;
        struct {
            xllm_artifact_info tInfo;
        } tArtifactReady;
        struct {
            xllm_output_refusal tRefusal;
        } tRefusal;
        struct {
            xllm_usage tUsage;
        } tUsage;
        struct {
            xllm_error tError;
        } tError;
    } as;
} xllm_event;

typedef bool (*xllm_event_callback)(const xllm_event *pEvent, void *pUserData);

typedef struct {
    xllm_stream_mode eStreamMode;
    uint32 uTimeoutMs;
    xllm_cancel_token *pCancelToken;
    xllm_event_callback pfnOnEvent;
    void *pUserData;
    xllm_artifact_policy eArtifactPolicy;
    xllm_artifact_sink *pArtifactSink;
    uint32 uMaxRetries;
    uint32 uRetryBackoffBaseMs;
    uint32 uRetryBackoffMaxMs;
    double fRetryJitter;
    bool bBestEffortStructuredOutput;
    xllm_local_file_policy eLocalFilePolicy;
    xvalue tVendorExtra;
} xllm_call_options;

typedef struct {
    const char *sToolId;
    const char *sWireName;
    const char *sCallId;
    const char *sArgumentsJson;
    xvalue tContinuation;
    xvalue tVendorExtra;
} xllm_tool_exec_request;

typedef struct {
    xllm_content_part *pParts;
    size_t iPartCount;
    xvalue tVendorExtra;
} xllm_tool_exec_result;

typedef int32 (*xllm_tool_execute_fn)(
    void *pCtx,
    const xllm_tool_exec_request *pRequest,
    xllm_tool_exec_result *pResult,
    xllm_error *pError
);

typedef xfuture *(*xllm_tool_execute_async_fn)(
    void *pCtx,
    const xllm_tool_exec_request *pRequest
);

typedef struct {
    void *pCtx;
    xllm_tool_execute_fn pfnExecute;
} xllm_tool_executor;

typedef struct {
    void *pCtx;
    xllm_tool_execute_async_fn pfnExecute;
} xllm_tool_executor_async;

typedef struct {
    const char *sInitialProfileId;
    const char *sSystemPrompt;
    xllm_call_options tDefaultCallOptions;
    xvalue tVendorExtra;
} xllm_create_options;

typedef enum {
    XLLM_COMPACT_TRUNCATE = 0,
    XLLM_COMPACT_SUMMARIZE,
    XLLM_COMPACT_CUSTOM
} xllm_compact_strategy;

typedef enum {
    XLLM_COMPACT_TO_FIT_CURRENT_MODEL = 0,
    XLLM_COMPACT_TO_TARGET_INPUT_TOKENS,
    XLLM_COMPACT_SUMMARIZE_OLDER_THAN_TURN,
    XLLM_COMPACT_TRUNCATE_ONLY
} xllm_compact_mode;

typedef struct {
    const char *sProfileId;
    const char *sSystemPrompt;
    bool bEnableAutoCompact;
    double fCompactTriggerRatio;
    uint32 uCompactTriggerTurns;
    uint32 uReserveOutputTokens;
    uint32 uKeepRecentTurns;
    bool bKeepActiveToolChain;
    xllm_compact_strategy eCompactStrategy;
    const char *sSummarizerProfileId;
    xvalue tVendorExtra;
} xllm_session_options;

typedef struct {
    xllm_compact_mode eMode;
    xllm_compact_strategy eStrategy;
    uint32 uTargetInputTokens;
    uint32 uOlderThanTurn;
    xvalue tVendorExtra;
} xllm_compact_options;

typedef struct {
    bool bCompacted;
    bool bSummarized;
    uint32 uInputTokensBefore;
    uint32 uInputTokensAfter;
    xvalue tVendorExtra;
} xllm_compact_result;

typedef struct {
    uint32 uInputTokens;
    uint32 uEstimatedOutputReserve;
    bool bEstimated;
    xvalue tVendorExtra;
} xllm_token_count_result;

typedef struct {
    xllm_allocator tAllocator;
    xllm_log_callback pfnLog;
    void *pLogCtx;
    xllm_trace_callback pfnTrace;
    void *pTraceCtx;
    xllm_debug_mode eDebugMode;
    xllm_redact_mode eRedactMode;
    xllm_transport_options tTransportDefaults;
    xvalue tVendorExtra;
} xllm_runtime_options;

typedef int32 (*xllm_adapter_count_tokens_fn)(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    xllm_token_count_result *pResult,
    xllm_error *pError
);

typedef int32 (*xllm_adapter_chat_fn)(
    void *pCtx,
    const xllm_profile *pProfile,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);

typedef struct {
    const char *sName;
    void *pCtx;
    xllm_adapter_count_tokens_fn pfnCountTokens;
    xllm_adapter_chat_fn pfnChat;
    xvalue tVendorExtra;
} xllm_adapter;

XLLM_API const char *xllm_version(void);

XLLM_API void xllm_runtime_options_init(xllm_runtime_options *pOptions);
XLLM_API void xllm_profile_init(xllm_profile *pProfile);
XLLM_API void xllm_request_init(xllm_request *pRequest);
XLLM_API void xllm_request_reset(xllm_request *pRequest);
XLLM_API void xllm_call_options_init(xllm_call_options *pOptions);
XLLM_API void xllm_error_init(xllm_error *pError);

XLLM_API int xllm_runtime_create(const xllm_runtime_options *pOptions, xllm_runtime **ppRuntime);
XLLM_API void xllm_runtime_destroy(xllm_runtime *pRuntime);

XLLM_API const char *xllm_log_level_name(xllm_log_level eLevel);
XLLM_API const char *xllm_log_event_name(xllm_log_event eEvent);
XLLM_API const char *xllm_trace_kind_name(xllm_trace_kind eKind);

XLLM_API int xllm_runtime_set_log_callback(
    xllm_runtime *pRuntime,
    xllm_log_callback pfnLog,
    void *pLogCtx
);

XLLM_API int xllm_runtime_set_trace_callback(
    xllm_runtime *pRuntime,
    xllm_trace_callback pfnTrace,
    void *pTraceCtx
);

XLLM_API int xllm_runtime_set_debug_mode(
    xllm_runtime *pRuntime,
    xllm_debug_mode eMode,
    xllm_redact_mode eRedactMode
);

XLLM_API int xllm_register_adapter(xllm_runtime *pRuntime, const xllm_adapter *pAdapter);
XLLM_API int xllm_register_openai_compat_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_glm_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_minimax_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_kimi_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_gemini_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_vertex_gemini_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_qwen_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_doubao_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_anthropic_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_ollama_native_adapter(xllm_runtime *pRuntime);
XLLM_API int xllm_register_profile(xllm_runtime *pRuntime, const xllm_profile *pProfile);

XLLM_API int xllm_validate_request(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_error *pError
);

XLLM_API int xllm_count_tokens(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    xllm_token_count_result *pResult,
    xllm_error *pError
);

XLLM_API int xllm_chat(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_chat_ex(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);

XLLM_API xfuture *xllm_chat_async_thread(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
);

XLLM_API xfuture *xllm_chat_async_engine(
    xllm_runtime *pRuntime,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
);

XLLM_API xfuture *xllm_chat_async_co(
    xllm_runtime *pRuntime,
    xcosched *pSched,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    size_t iStackSize
);

XLLM_API int xllm_cancel_token_create(xllm_cancel_token **ppToken);
XLLM_API void xllm_cancel_token_destroy(xllm_cancel_token *pToken);
XLLM_API void xllm_cancel_token_cancel(xllm_cancel_token *pToken, const char *sReason);
XLLM_API bool xllm_cancel_token_is_cancelled(const xllm_cancel_token *pToken);

XLLM_API const char *xllm_response_get_text(const xllm_response *pResponse);
XLLM_API size_t xllm_response_get_output_count(const xllm_response *pResponse);
XLLM_API const xllm_output_item *xllm_response_get_output(const xllm_response *pResponse, size_t iIndex);
XLLM_API size_t xllm_response_get_tool_call_count(const xllm_response *pResponse);
XLLM_API const xllm_output_tool_call *xllm_response_get_tool_call(const xllm_response *pResponse, size_t iIndex);
XLLM_API const xvalue *xllm_response_get_json(const xllm_response *pResponse, size_t iOutputIndex, size_t iPartIndex);
XLLM_API const xvalue *xllm_response_get_first_json(const xllm_response *pResponse, size_t *piOutputIndex, size_t *piPartIndex);

XLLM_API void xllm_tool_exec_result_free(xllm_tool_exec_result *pResult);
XLLM_API void xllm_response_free(xllm_response *pResponse);
XLLM_API void xllm_error_reset(xllm_error *pError);
XLLM_API void xllm_error_free(xllm_error *pError);

#ifdef __cplusplus
}
#endif

#if defined(XLLM_IMPLEMENTATION)
#include "src/xllm_core_all.c"
#endif

#endif
