/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xllm requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XLLM_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XLLM_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XLLM_SINGLE_HEADER_H
#define XLLM_SINGLE_HEADER_H
#define XLLM_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xllm/include/xllm/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XLLM_FEATURES_H
#define XLLM_FEATURES_H

/* xllm 及其直接依赖。 */
#if defined(XLLM_MODULE_ALL) || defined(XLLM_MODULE_XLLM)
#ifndef XLLM_FEATURE_XLLM
#define XLLM_FEATURE_XLLM
#endif
#ifndef XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_READ
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL_SYNC
#define XRT_MODULE_NET_TCP_DIAL_SYNC
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL_FUTURE
#define XRT_MODULE_NET_TCP_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_NET_PROXY_DIAL
#define XRT_MODULE_NET_PROXY_DIAL
#endif
#ifndef XRT_MODULE_NET_TCP_FUTURE
#define XRT_MODULE_NET_TCP_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_FUTURE
#define XRT_MODULE_TLS_STREAM_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_LISTENER
#define XRT_MODULE_TLS_STREAM_LISTENER
#endif
#ifndef XRT_MODULE_TLS_STREAM_LISTENER_SYNC
#define XRT_MODULE_TLS_STREAM_LISTENER_SYNC
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA384
#define XRT_MODULE_TLS_SCHEDULE_SHA384
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P256
#define XRT_MODULE_TLS_KEY_EXCHANGE_P256
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_RSA
#define XRT_MODULE_TLS_IDENTITY_RSA
#endif
#ifndef XRT_MODULE_TLS_VERIFY
#define XRT_MODULE_TLS_VERIFY
#endif
#ifndef XRT_MODULE_X509_STORE_SYSTEM
#define XRT_MODULE_X509_STORE_SYSTEM
#endif
#ifndef XRT_MODULE_HTTP1_BODY
#define XRT_MODULE_HTTP1_BODY
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL_PROXY
#define XRT_MODULE_TLS_STREAM_DIAL_PROXY
#endif
#ifndef XRT_MODULE_JSON_WRITE
#define XRT_MODULE_JSON_WRITE
#endif
#endif

#endif /* XLLM_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xllm/include/xllm/api.h */
/* ========================================================================== */

#ifndef XLLM_API_H
#define XLLM_API_H


#if defined(XLLM_FEATURE_XLLM)

#if !defined(XRT_FEATURE_FUTURE_BRIDGE)
#error "xllm requires future_bridge (XRT_FEATURE_FUTURE_BRIDGE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
#error "xllm requires tls_stream_dial_proxy (XRT_FEATURE_TLS_STREAM_DIAL_PROXY)"
#endif
#if !defined(XRT_FEATURE_JSON_WRITE)
#error "xllm requires json_write (XRT_FEATURE_JSON_WRITE)"
#endif

/* The selected product requires its complete declared dependency set. */
#if !defined(XRT_FEATURE_JSON_READ)
#error "xllm requires json_read (XRT_FEATURE_JSON_READ)"
#endif
#if !defined(XRT_FEATURE_FILE_WHOLE)
#error "xllm requires file_whole (XRT_FEATURE_FILE_WHOLE)"
#endif
#if !defined(XRT_FEATURE_DIR)
#error "xllm requires dir (XRT_FEATURE_DIR)"
#endif
#if !defined(XRT_FEATURE_CODEC_BASE64)
#error "xllm requires codec_base64 (XRT_FEATURE_CODEC_BASE64)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_DIAL_SYNC)
#error "xllm requires net_tcp_dial_sync (XRT_FEATURE_NET_TCP_DIAL_SYNC)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE)
#error "xllm requires net_tcp_dial_future (XRT_FEATURE_NET_TCP_DIAL_FUTURE)"
#endif
#if !defined(XRT_FEATURE_NET_PROXY_DIAL)
#error "xllm requires net_proxy_dial (XRT_FEATURE_NET_PROXY_DIAL)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_FUTURE)
#error "xllm requires net_tcp_future (XRT_FEATURE_NET_TCP_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
#error "xllm requires tls_stream_dial_future (XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_FUTURE)
#error "xllm requires tls_stream_future (XRT_FEATURE_TLS_STREAM_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_LISTENER)
#error "xllm requires tls_stream_listener (XRT_FEATURE_TLS_STREAM_LISTENER)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_LISTENER_SYNC)
#error "xllm requires tls_stream_listener_sync (XRT_FEATURE_TLS_STREAM_LISTENER_SYNC)"
#endif
#if !defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
#error "xllm requires tls_client_verify (XRT_FEATURE_TLS_CLIENT_VERIFY)"
#endif
#if !defined(XRT_FEATURE_TLS_RECORD_AES)
#error "xllm requires tls_record_aes (XRT_FEATURE_TLS_RECORD_AES)"
#endif
#if !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
#error "xllm requires tls_schedule_sha256 (XRT_FEATURE_TLS_SCHEDULE_SHA256)"
#endif
#if !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
#error "xllm requires tls_schedule_sha384 (XRT_FEATURE_TLS_SCHEDULE_SHA384)"
#endif
#if !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)
#error "xllm requires tls_key_exchange_x25519 (XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)"
#endif
#if !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256)
#error "xllm requires tls_key_exchange_p256 (XRT_FEATURE_TLS_KEY_EXCHANGE_P256)"
#endif
#if !defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#error "xllm requires tls_identity_rsa (XRT_FEATURE_TLS_IDENTITY_RSA)"
#endif
#if !defined(XRT_FEATURE_TLS_VERIFY)
#error "xllm requires tls_verify (XRT_FEATURE_TLS_VERIFY)"
#endif
#if !defined(XRT_FEATURE_X509_STORE_SYSTEM)
#error "xllm requires x509_store_system (XRT_FEATURE_X509_STORE_SYSTEM)"
#endif
#if !defined(XRT_FEATURE_HTTP1_BODY)
#error "xllm requires http1_body (XRT_FEATURE_HTTP1_BODY)"
#endif
#if !defined(XRT_FEATURE_THREAD)
#error "xllm requires thread (XRT_FEATURE_THREAD)"
#endif
#if !defined(XRT_FEATURE_MUTEX)
#error "xllm requires mutex (XRT_FEATURE_MUTEX)"
#endif
#if !defined(XRT_FEATURE_CANCEL)
#error "xllm requires cancel (XRT_FEATURE_CANCEL)"
#endif
#if !defined(XRT_FEATURE_TIME)
#error "xllm requires time (XRT_FEATURE_TIME)"
#endif


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
#define XLLM_VERSION_MINOR 2
#define XLLM_VERSION_PATCH 0

/* Compatibility APIs remain source compatible in 3.1. Define
 * XLLM_ENABLE_DEPRECATED_WARNINGS before including this header when migrating
 * new code and the compiler will identify calls that belong in xllm-session
 * or in create/request-time configuration. */
#if defined(XLLM_ENABLE_DEPRECATED_WARNINGS)
#  if defined(_MSC_VER)
#    define XLLM_DEPRECATED(sMessage) __declspec(deprecated(sMessage))
#  elif defined(__GNUC__) || defined(__clang__)
#    define XLLM_DEPRECATED(sMessage) __attribute__((deprecated(sMessage)))
#  else
#    define XLLM_DEPRECATED(sMessage)
#  endif
#else
#  define XLLM_DEPRECATED(sMessage)
#endif

typedef struct xllm_client xllm_client;
typedef struct xllm_call xllm_call;
typedef struct xllm_hooks xllm_hooks;
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
    XLLM_ERROR_HOOK,    /* a host-supplied session hook failed or re-entered */
    XLLM_ERROR_INCOMPLETE_RESPONSE, /* valid stream ended before model termination */
    XLLM_ERROR_OUTPUT_LIMIT /* output cap interrupted a tool-call generation */
} xllm_error_code;

typedef enum xllm_error_domain {
    XLLM_ERROR_DOMAIN_NONE = 0,
    XLLM_ERROR_DOMAIN_API,
    XLLM_ERROR_DOMAIN_MEMORY,
    XLLM_ERROR_DOMAIN_TRANSPORT,
    XLLM_ERROR_DOMAIN_PROVIDER,
    XLLM_ERROR_DOMAIN_PARSER,
    XLLM_ERROR_DOMAIN_HOOK,
    XLLM_ERROR_DOMAIN_RESOURCE
} xllm_error_domain;

typedef enum xllm_error_stage {
    XLLM_ERROR_STAGE_NONE = 0,
    XLLM_ERROR_STAGE_VALIDATE,
    XLLM_ERROR_STAGE_BUILD_REQUEST,
    XLLM_ERROR_STAGE_RESOLVE,
    XLLM_ERROR_STAGE_CONNECT,
    XLLM_ERROR_STAGE_PROXY,
    XLLM_ERROR_STAGE_TLS,
    XLLM_ERROR_STAGE_WRITE,
    XLLM_ERROR_STAGE_READ,
    XLLM_ERROR_STAGE_HTTP,
    XLLM_ERROR_STAGE_PARSE,
    XLLM_ERROR_STAGE_PROVIDER,
    XLLM_ERROR_STAGE_HOOK,
    XLLM_ERROR_STAGE_LIMIT,
    XLLM_ERROR_STAGE_WAIT
} xllm_error_stage;

#define XLLM_ERROR_CAUSE_VERSION 1u

/* Owned inline cause snapshot. It never stores request headers, bodies,
 * credentials, or xrt error data. Runtime kind/code are the numeric values
 * reported by the underlying xrt error when one exists; zero means absent. */
typedef struct xllm_error_cause {
    uint32_t Size;
    uint32_t Version;
    xllm_error_domain eDomain;
    xllm_error_stage eStage;
    int32_t iRuntimeKind;
    int32_t iRuntimeCode;
    int32_t iSystemCode;
    char sOperation[48];
    char sMessage[192];
} xllm_error_cause;

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
    bool bToolCallDropped;  /* a lifecycle hook removed a tool call */
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
    /* An exact, case-insensitive Idempotency-Key extra header was present.
     * This records caller intent; the selected provider must honor the key. */
    bool bIdempotencyKeyPresent;
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
    xllm_error_cause tCause;
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

typedef enum xllm_proxy_kind {
    XLLM_PROXY_NONE = 0,
    XLLM_PROXY_SOCKS5,
    XLLM_PROXY_HTTP_CONNECT
} xllm_proxy_kind;

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

XRT_API void xllmPartInit(xllm_part* pPart, xllm_part_kind eKind);
/* Release a part constructed with the xllmPartSet* helpers (deep frees). */
XRT_API void xllmPartUnit(xllm_part* pPart);
/* Allocating setters are failure-atomic: false leaves the prior part value
 * valid and unchanged. */
XRT_API bool xllmPartSetText(xllm_part* pPart, const char* sText);
XRT_API bool xllmPartSetImageData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
XRT_API bool xllmPartSetImageUrl(xllm_part* pPart, const char* sUrl, const char* sMediaType);
XRT_API bool xllmPartSetAudioData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
XRT_API bool xllmPartSetFileData(xllm_part* pPart, const void* pData, size_t iSize, const char* sMediaType);
XRT_API bool xllmPartSetNative(xllm_part* pPart, const char* sNativeType, const char* sJson);

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

XRT_API void xllmModelProfileInit(xllm_model_profile* pProfile);
XRT_API const xllm_model_profile* xllmModelProfileBuiltin(const char* sIdOrModel);
XRT_API bool xllmModelProfileValidate(const xllm_model_profile* pProfile, xllm_error* pError);
XRT_API bool xllmModelProfileSupports(const xllm_model_profile* pProfile, xllm_capability_flags uRequired);
XRT_API bool xllmModelProfileValidateRequest(const xllm_model_profile* pProfile,
    const xllm_request* pRequest, xllm_error* pError);

XRT_API void xllmErrorInit(xllm_error* pError);
XRT_API const char* xllmErrorCodeName(xllm_error_code eCode);
XRT_API const char* xllmErrorDomainName(xllm_error_domain eDomain);
XRT_API const char* xllmErrorStageName(xllm_error_stage eStage);
XRT_API const char* xllmFinishReasonName(xllm_finish eFinish);
/* Classifies whether the failed attempt is safe for automatic replay. A
 * post-write network failure requires bIdempotencyKeyPresent. */
XRT_API bool xllmErrorRetryable(const xllm_error* pError);

/* ------------------------------------------------------------------ */
/* Message and request construction                                    */
/* ------------------------------------------------------------------ */

XRT_API void xllmMessageInit(xllm_message* pMessage, xllm_role eRole);
XRT_API void xllmMessageUnit(xllm_message* pMessage);
/* Setters preserve the prior logical value on allocation failure; Add*
 * functions preserve the element count and ownership of existing entries. */
XRT_API bool xllmMessageSetContent(xllm_message* pMessage, const char* sContent);
XRT_API bool xllmMessageSetReasoning(xllm_message* pMessage, const char* sReasoningContent);
XRT_API bool xllmMessageSetToolCallId(xllm_message* pMessage, const char* sToolCallId);
XRT_API bool xllmMessageAddToolCall(xllm_message* pMessage, const char* sId, const char* sName, const char* sArgumentsJson);
XRT_API bool xllmMessageAddPart(xllm_message* pMessage, const xllm_part* pPart);
XRT_API bool xllmMessageSetNative(xllm_message* pMessage, const char* sNativeJson);

/* Convenience: build a history assistant message from a completed response
 * (text, reasoning, tool calls). Signed thinking is stored as an ordered
 * REASONING part followed by its opaque thinking_signature NATIVE part, with
 * TEXT parts in original block order. Unsigned responses keep sContent. The
 * output is independently owned; on allocation failure it is empty. Supply an
 * uninitialized/empty output, as with xllmMessageInit. Tool calls retain order
 * in their separate array. Other provider-native block types are not mapped. */
XRT_API bool xllmMessageFromResponse(const xllm_response* pResponse, xllm_message* pMessage);

typedef enum xllm_json_mode {
    XLLM_JSON_NONE = 0,
    XLLM_JSON_OBJECT
} xllm_json_mode;

/* Borrowed extra transport header. An Idempotency-Key header explicitly
 * permits automatic network retry after request bytes have been written;
 * callers must use it only with providers that honor that contract. */
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
    /* Borrowed-view bookkeeping (allocation discipline 改造 A/B): entries
     * flagged true are shallow struct copies whose strings point into the
     * lender's storage — xllmRequestUnit skips their deep teardown. NULL
     * means "everything owned" (the default AddMessage/AddTool path). */
    bool* pbMessageBorrowed;           /* parallel to pMessages; may be NULL */
    bool* pbToolBorrowed;              /* parallel to pTools; may be NULL */
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
    /* Absolute xllmClockUs() deadline in microseconds; UINT64_MAX disables it. */
    uint64_t uDeadline;
    /* Relative milliseconds, resolved once when the call starts. */
    int64_t iTimeout;
    /* Borrowed per-call lifecycle hooks; replaces the client-level set. */
    const xllm_hooks* pHooks;
    /* Wire-prefix cache stamp (set by borrowed-view renders only): the
     * client's serialization cache reuses bytes for [0..iStableMessages)
     * while (pStablePrefixOwner, uStablePrefixStamp) match. Cleared on
     * request clones because hook mutations invalidate the prefix. */
    void* pStablePrefixOwner;
    uint64_t uStablePrefixStamp;
    size_t iStableMessages;
} xllm_request;

XRT_API void xllmRequestInit(xllm_request* pRequest);
XRT_API void xllmRequestUnit(xllm_request* pRequest);
/* Shallow-append a message whose strings the lender owns (ledger entries,
 * cached tool tables). The request must not outlive the lender; the lender
 * must not mutate the message while the request holds it. */
XRT_API bool xllmRequestAddMessageView(xllm_request* pRequest, const xllm_message* pMessage);
/* Attach a whole borrowed tool table. The replacement is prepared before old
 * owned tools are released, so false preserves the current table. Same
 * lifetime contract as AddMessageView. */
XRT_API bool xllmRequestSetToolsView(xllm_request* pRequest, const xllm_tool* pTools, size_t iCount);
XRT_API bool xllmRequestSetModel(xllm_request* pRequest, const char* sModel);
XRT_API bool xllmRequestSetReasoningEffort(xllm_request* pRequest, const char* sEffort);
XRT_API bool xllmRequestSetStop(xllm_request* pRequest, const char* sStop);
XRT_API bool xllmRequestSetExtraBody(xllm_request* pRequest, const char* sJsonObject);
XRT_API void xllmRequestSetCancel(xllm_request* pRequest, xcancel* pCancel);
XRT_API void xllmRequestSetTimeout(xllm_request* pRequest, int64_t iTimeout);
XRT_API void xllmRequestSetDeadline(xllm_request* pRequest, uint64_t uDeadline);
XRT_API bool xllmRequestSetToolChoice(xllm_request* pRequest, xllm_tool_choice eChoice, const char* sNamedTool);
XRT_API bool xllmRequestAddMessage(xllm_request* pRequest, const xllm_message* pMessage);
XRT_API bool xllmRequestAddTextMessage(xllm_request* pRequest, xllm_role eRole, const char* sContent);
XRT_API bool xllmRequestAddToolResult(xllm_request* pRequest, const char* sToolCallId, const char* sContent);
XRT_API bool xllmRequestAddTool(xllm_request* pRequest, const char* sName, const char* sDescription, const char* sParametersJson, bool bStrict);

XRT_API void xllmResponseDestroy(xllm_response* pResponse);

/* Deterministic lexical token estimates used by budget governance. */
XRT_API uint64_t xllmEstimateTextTokens(const char* sText);
XRT_API uint64_t xllmEstimateMessageTokens(const xllm_message* pMessage);

/* ------------------------------------------------------------------ */
/* Lifecycle hooks: mutable data seams around one model call          */
/* ------------------------------------------------------------------ */

/* Wire-level body handed to the byte seams. The buffer is NUL-terminated;
 * hooks may edit in place (OUT bounded by iBodyCapacity) or replace the
 * pointer wholesale (the library frees the old buffer; the replacement
 * must be xllmFree-compatible and NUL-terminated with iBodySize equal to
 * strlen). Embedded NUL bytes are rejected. */
typedef struct xllm_wire {
    uint32_t uAttempt;        /* retry ordinal, from 1 */
    uint32_t uHttpStatus;     /* response seam only */
    char* sBody;
    size_t iBodySize;
    size_t iBodyCapacity;     /* request seam only */
    uint32_t uReserved[4];
} xllm_wire;

/* Every seam is optional (NULL). Returning false aborts the call with
 * XLLM_ERROR_HOOK. All pointers are mutable in place; borrowed storage a
 * hook attaches (e.g. extra headers) must outlive the call. */
struct xllm_hooks {
    bool (*pOnRequest)(xllm_client* pClient, xllm_request* pRequest, void* pUserData);
    bool (*pOnRequestBody)(xllm_client* pClient, xllm_wire* pWire, void* pUserData);
    bool (*pOnRetry)(xllm_client* pClient, const xllm_diagnostics* pDiagnostics,
        uint32_t uNextAttempt, void* pUserData);
    bool (*pOnResponseBody)(xllm_client* pClient, xllm_wire* pWire, void* pUserData);
    bool (*pOnToolCall)(xllm_client* pClient, xllm_tool_call* pCall,
        uint32_t uIndex, void* pUserData);
    bool (*pOnResponse)(xllm_client* pClient, xllm_response* pResponse, void* pUserData);
    void* pUserData;
    uint32_t uReserved[4];
};

/* Compatibility setter for client-level default hooks. New code should set
 * xllm_request.pHooks for each operation so concurrent calls have explicit,
 * immutable behavior. The complete table is copied; NULL removes it.
 * The update affects future operations and is safe while other calls run.
 * Each call snapshots either request hooks or this default before invoking
 * callbacks, and never invokes a callback while holding the client lock. */
XLLM_DEPRECATED("set xllm_request.pHooks before starting the call")
XRT_API void xllmClientSetHooks(xllm_client* pClient, const xllm_hooks* pHooks);

/* ------------------------------------------------------------------ */
/* Compatibility history: use xllm-session for new agent code.         */
/* ------------------------------------------------------------------ */

typedef struct xllm_history xllm_history;

XLLM_DEPRECATED("use xllm-session")
XRT_API xllm_history* xllmHistoryCreate(void);
XLLM_DEPRECATED("use xllm-session")
XRT_API void xllmHistoryDestroy(xllm_history* pHistory);
XLLM_DEPRECATED("use xllm-session")
XRT_API size_t xllmHistoryCount(const xllm_history* pHistory);
/* Borrowed view of one stored message; valid until the next mutation. */
XLLM_DEPRECATED("use xllm-session")
XRT_API const xllm_message* xllmHistoryAt(const xllm_history* pHistory, size_t iIndex);

XLLM_DEPRECATED("use xllm-session")
XRT_API bool xllmHistoryAdd(xllm_history* pHistory, const xllm_message* pMessage);
XLLM_DEPRECATED("use xllm-session")
XRT_API bool xllmHistoryAddText(xllm_history* pHistory, xllm_role eRole, const char* sContent);
/* Text + reasoning + tool calls from a completed response. */
XLLM_DEPRECATED("use xllm-session")
XRT_API bool xllmHistoryAddFromResponse(xllm_history* pHistory, const xllm_response* pResponse);
XLLM_DEPRECATED("use xllm-session")
XRT_API bool xllmHistoryAddToolResult(xllm_history* pHistory, const char* sCallId,
    const char* sContent);
XLLM_DEPRECATED("use xllm-session")
XRT_API bool xllmHistoryRemove(xllm_history* pHistory, size_t iIndex, size_t iCount);
/* Appends every stored message into a request (deep copy again). Failure
 * removes every message appended by this call and preserves the old prefix. */
XLLM_DEPRECATED("use xllm-session")
XRT_API bool xllmHistoryAppendInto(const xllm_history* pHistory, xllm_request* pRequest);

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
    /* Maximum age of a pooled idle connection; zero disables age expiry.
     * Closed/failed streams are discarded regardless of this value. */
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
    /* Optional HTTPS outbound proxy. All strings are borrowed for the create
     * call and copied into immutable client/XRT state. A proxy kind requires
     * a non-empty host and non-zero port; incomplete settings are rejected.
     * Bypass entries are separated by ',' or ';', compared as ASCII without
     * case, and may use one leading and/or trailing '*' wildcard. */
    xllm_proxy_kind eProxyKind;
    const char* sProxyHost;
    uint16_t uProxyPort;
    const char* sProxyUser;
    const char* sProxyPass;
    const char* sProxyBypass;
    /* Bounded parser/transport buffers. Zero selects the documented default;
     * values above the built-in hard ceiling are rejected at create time. */
    uint32_t uMaxHttpHeadBytes;       /* default/hard max: 64 KiB */
    uint32_t uMaxResponseBodyBytes;   /* default/hard max: 64 MiB */
    uint32_t uMaxSseLineBytes;        /* default/hard max: 1 MiB */
    uint32_t uMaxSseEventBytes;       /* default/hard max: 16 MiB */
} xllm_client_config;

XRT_API void xllmClientConfigInit(xllm_client_config* pConfig);
XRT_API xllm_client* xllmClientCreate(const xllm_client_config* pConfig, xllm_error* pError);
/* Requires every call created by this client to be destroyed first and must
 * not race another client API. Drains idle connections before engine teardown. */
XRT_API void xllmClientDestroy(xllm_client* pClient);
XRT_API bool xllmClientGetModelProfile(const xllm_client* pClient, xllm_model_profile* pProfile);
/* Compatibility setter: prefer config.pModelProfile so construction is
 * failure-atomic and the client is immutable after publication. Installs a
 * capability profile before the client is first used (custom
 * endpoints such as self-hosted models; config.pModelProfile covers the
 * create-time path). The provider must match the client's fixed wire dialect.
 * A successful GetModelProfile, Start, Complete, or BuildRequestJson seals the
 * profile. Returned profile strings then remain valid until client destroy. */
XLLM_DEPRECATED("set xllm_client_config.pModelProfile before xllmClientCreate")
XRT_API bool xllmClientSetModelProfile(xllm_client* pClient, const xllm_model_profile* pProfile, xllm_error* pError);

XRT_API xllm_call* xllmClientStart(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_error* pError
);
/* The client must outlive the call and client destruction is not concurrent
 * with Start/Wait/Destroy. The request and its borrowed views only need to
 * remain valid until Start returns. Hook tables are copied during Start. */
/* The call's completion future; transport runs on the engine workers.
 * The returned future is borrowed: valid until xllmCallWait/Destroy. */
XRT_API xfuture* xllmCallFuture(xllm_call* pCall);
XRT_API xllm_result xllmCallWait(xllm_call* pCall, xllm_response** ppResponse, xllm_error* pError);
XRT_API bool xllmCallCancel(xllm_call* pCall);
XRT_API void xllmCallDestroy(xllm_call* pCall);

XRT_API xllm_result xllmClientComplete(
    xllm_client* pClient,
    const xllm_request* pRequest,
    const xllm_stream_callbacks* pCallbacks,
    xllm_response** ppResponse,
    xllm_error* pError
);

/* Builds the provider JSON body without credentials. Free with xllmFree(). */
/* Full classic serialization for inspection/testing; it never reads or
 * updates the client's wire-prefix cache (stamped view requests through
 * this API also bypass it) and works on any dialect. */
XRT_API char* xllmClientBuildRequestJson(xllm_client* pClient, const xllm_request* pRequest, xllm_error* pError);
XRT_API void xllmFree(void* pMemory);

#ifdef __cplusplus
}
#endif

#endif /* selected xllm */

#endif


/* ========================================================================== */
/* public: extlibs/xllm/include/xllm/executor.h */
/* ========================================================================== */

#ifndef XLLM_EXECUTOR_H
#define XLLM_EXECUTOR_H

/*
 * xllm executor contract: "the model's hands".
 *
 * Pure type seam, no behavior. xllm defines the contract; an implementation
 * (tool registry, permission gate, process table) lives above this library —
 * xwork provides the reference implementation via xworkExecutorBind(). The
 * bounded round-trip loop that drives this contract is xllmSessionRunWithTools
 * in xllm-session; hosts with their own policy drive it manually.
 *
 * Wiring rule: the host composes (executor borrows its owner; the caller of
 * pExecute borrows the executor). Compile-time dependencies never invert:
 * implementations include this header, xllm never includes them.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__TINYC__)
#else
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xllm_executor xllm_executor;

#define XLLM_EXECUTOR_ABI_VERSION 1u
#define XLLM_EXECUTOR_RESULT_EFFECT_APPLIED (1u << 0)

/* Per-call execution context, supplied by whoever drives the loop. */
typedef struct xllm_executor_ctx {
    /* Borrowed cooperative cancel token; NULL when the host has none. */
    xcancel* pCancel;
    /* Absolute xllmClockUs() deadline in microseconds; 0 and UINT64_MAX both
     * mean "no deadline" so a zero-initialized context is valid. */
    uint64_t uDeadline;
    /* 1-based model round within the current run. */
    uint64_t uRound;
    /* Session turn the call belongs to. */
    uint64_t uTurn;
    uint32_t uReserved[4];
} xllm_executor_ctx;

/* One tool outcome. sContent must be presentable to the model as-is: the
 * success/failure presentation (status framing, truncation notices) is the
 * executor's responsibility. Strings are owned by the executor and remain
 * valid until the next pExecute call on the same executor (rolling storage
 * is acceptable); the driving loop copies them into the session.
 * Images (read passthrough): pImageBytes/sImageMime are borrowed the same
 * way; the driver attaches them as an IMAGE part beside the text. */
typedef struct xllm_executor_result {
    char* sContent;
    bool bSuccess;
    const unsigned char* pImageBytes;   /* NULL when no image */
    size_t iImageSize;
    const char* sImageMime;             /* "image/png" etc. */
    /* Executor-owned metadata. EFFECT_APPLIED distinguishes a successful
     * external side effect from a presentable failure returned after an
     * after-tool policy hook. Drivers preserve ordering; they do not infer
     * effects from bSuccess. */
    uint32_t uFlags;
    uint32_t uReserved;
} xllm_executor_result;

typedef bool (*xllm_executor_execute_batch_fn)(void* pUserData,
    const xllm_tool_call* pCalls, size_t iCallCount,
    const xllm_executor_ctx* pCtx, xllm_executor_result* pResults);

typedef void (*xllm_executor_batch_results_unit_fn)(void* pUserData,
    xllm_executor_result* pResults, size_t iResultCount);

struct xllm_executor {
    /* Append this source's tool definitions to the request (xllmRequestAddTool).
     * Called once per model round; implementations should keep the
     * serialization stable across rounds so the request prefix stays
     * cache-friendly (a generation counter may guard this). */
    bool (*pListTools)(void* pUserData, xllm_request* pRequest);
    /* Execute one tool call. Returning false signals an infrastructure
     * failure (run aborts); a tool-level failure returns true with
     * bSuccess = false and a presentable sContent. */
    bool (*pExecute)(void* pUserData, const xllm_tool_call* pCall,
        const xllm_executor_ctx* pCtx, xllm_executor_result* pResult);
    void* pUserData;
    /* Optional v1 batch seam. Legacy executors leave these fields zero and
     * are driven serially through pExecute. A batch implementation may run
     * calls concurrently, but must write pResults in input order and keep all
     * borrowed result storage alive until pBatchResultsUnit returns. The
     * driver commits those results in the same input order.
     *
     * uAbiVersion/uStructSize occupy the first two legacy reserved words, so
     * a new driver can identify an older, smaller executor before reading the
     * appended callback fields. */
    uint32_t uAbiVersion;
    uint32_t uStructSize;
    uint32_t uReserved[2];
    xllm_executor_execute_batch_fn pExecuteBatch;
    xllm_executor_batch_results_unit_fn pBatchResultsUnit;
};

/* Initializes the complete versioned record. Call this before assigning
 * callbacks so future optional fields are never read from indeterminate
 * storage. A plain zero-initialized legacy executor remains valid. */
static inline void xllmExecutorInit(xllm_executor* pExecutor)
{
    if ( !pExecutor ) return;
    memset(pExecutor, 0, sizeof(*pExecutor));
    pExecutor->uAbiVersion = XLLM_EXECUTOR_ABI_VERSION;
    pExecutor->uStructSize = (uint32_t)sizeof(*pExecutor);
}

#ifdef __cplusplus
}
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xllm/include/xllm.h */
/* ========================================================================== */

#ifndef XLLM_H
#define XLLM_H


#endif

#endif

#if defined(XLLM_IMPLEMENTATION) && !defined(XLLM_IMPLEMENTATION_ONCE)
#define XLLM_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xllm/src/internal/xllm_time.h */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)
#ifndef XLLM_TIME_H
#define XLLM_TIME_H

/* Request and persistence APIs retain microsecond fields. XRT waits use
 * signed relative milliseconds; convert only at that boundary. Include after
 * the XRT declarations. Monotonic values are never serialized across boots. */
#include <stdint.h>
#define XLLM_DEADLINE_NEVER UINT64_MAX
/* New civil time starts at year 1; product persistence uses Unix microseconds. */
static inline int64_t xllmTimeToUnixUs(xtime time)
{
    return (time - XRT_TIME_UNIX_EPOCH) * INT64_C(1000);
}
static inline int64_t xllmUnixUs(void) { return xllmTimeToUnixUs(xrtNow()); }
static inline uint64_t xllmClockUs(void)
{
    double t = xrtTimer() * 1000000.0;
    if (!(t >= 0.0))
        return 0u;
    return t >= (double)(UINT64_MAX - 1u) ? UINT64_MAX - 1u : (uint64_t)t;
}
static inline uint64_t xllmDeadlineAfterUs(uint64_t duration)
{
    uint64_t now = xllmClockUs();
    return duration >= UINT64_MAX - 1u - now ? UINT64_MAX - 1u : now + duration;
}
static inline uint64_t xllmDeadlineRemainingUs(uint64_t deadline)
{
    uint64_t now;
    if (deadline == XLLM_DEADLINE_NEVER)
        return UINT64_MAX;
    now = xllmClockUs();
    return deadline > now ? deadline - now : 0u;
}
static inline bool xllmDeadlineExpired(uint64_t deadline)
{
    return deadline != XLLM_DEADLINE_NEVER && xllmDeadlineRemainingUs(deadline) == 0u;
}
static inline int64_t xllmDeadlineWaitMs(uint64_t deadline)
{
    uint64_t left, ms;
    if (deadline == XLLM_DEADLINE_NEVER)
        return XRT_WAIT_FOREVER;
    left = xllmDeadlineRemainingUs(deadline);
    ms = left / 1000u + (left % 1000u != 0u);
    return ms > INT64_MAX ? INT64_MAX : (int64_t)ms;
}
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xllm/src/internal/xllm_internal.h */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)
#ifndef XLLM_INTERNAL_H
#define XLLM_INTERNAL_H


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define XLLM_MAX_FALLBACK_BODY (64u * 1024u * 1024u)
#define XLLM_MAX_AUTH_HEADERS 4u
#define XLLM_MAX_IDLE_CONNECTIONS 8u
#define XLLM_HTTP_HEAD_LIMIT (64u * 1024u)
#define XLLM_HTTP_FIELD_LIMIT 100u
#define XLLM_HTTP_TRAILER_LIMIT 32u
#define XLLM_HTTP_IO_CHUNK (64u * 1024u)

/* Engine-driven transport phases; advanced only from op-watch callbacks. */
typedef enum xllm_async_phase
{
    XLLM_ASYNC_DIAL = 0,
    XLLM_ASYNC_SEND,
    XLLM_ASYNC_READ,
    XLLM_ASYNC_DONE
} xllm_async_phase;

typedef struct xllm_buf
{
    char *pData;
    size_t iLen;
    size_t iCap;
} xllm_buf;

typedef enum xllm_transport_result
{
    XLLM_TRANSPORT_OK = 0,
    XLLM_TRANSPORT_ERROR,
    XLLM_TRANSPORT_TIMEOUT,
    XLLM_TRANSPORT_CANCELLED
} xllm_transport_result;

typedef struct xllm_transport_diagnostics
{
    xllm_transport_result eResult;
    int32_t iSystemError;
    uint64_t uStartedMs;
    uint64_t uConnectedMs;
    uint64_t uRequestSentMs;
    uint64_t uFirstByteMs;
    uint64_t uFirstTokenMs;
    uint64_t uHeadersMs;
    uint64_t uCompletedMs;
    uint64_t uRequestBytes;
    uint64_t uResponseBodyBytes;
    uint64_t uEffectiveTimeoutMs;
    bool bReusedConnection;
    bool bToolCallDropped;
    char sError[32];
    char sPhase[32];
} xllm_transport_diagnostics;

typedef struct xllm_connection xllm_connection;
typedef struct xllm_op_node xllm_op_node;

/* Owned auth header produced by a dialect; the transport wipes and frees it. */
typedef struct xllm_auth_header
{
    char sName[32];
    char *sValue;
} xllm_auth_header;

/* Dialect scratch: provider streaming item id -> unified tool index. */
typedef struct xllm_item_map
{
    char sId[64];
    size_t iTool;
} xllm_item_map;

/* One assembled SSE event: field set collected by the framing layer. */
typedef struct xllm_sse_fields
{
    xstrview tEvent; /* event: line value; empty when absent */
    xstrview tData;  /* data: lines joined with \n */
    xstrview tId;    /* id: line value; empty when absent */
} xllm_sse_fields;

struct xllm_client
{
    char *sBaseUrl;
    char *sApiKey;
    char *sModel;
    char *sReasoningEffort;
    char *sUserAgent;
    uint32_t uMaxOutputTokens;
    uint32_t uTimeoutMs;
    uint32_t uIdleTimeoutMs;
    uint32_t uMaxAttempts;
    uint32_t uRetryBaseDelayMs;
    uint32_t uRetryMaxDelayMs;
    uint32_t uMaxHttpHeadBytes;
    uint32_t uMaxResponseBodyBytes;
    uint32_t uMaxSseLineBytes;
    uint32_t uMaxSseEventBytes;
    bool bVerifyPeer;
    char *sCaPem;           /* owned copy; private CA chain */
    xx509store *pX509Store; /* borrowed custom trust store */
    xllm_provider eProvider;
    xllm_model_profile tModelProfile;
    char *sProfileId;
    bool bHasModelProfile;
    bool bTls;
    xnetproxy *pProxy;  /* owned immutable proxy, if configured */
    char *sProxyBypass; /* owned bypass policy snapshot */
    char *sHost;
    char *sTarget;
    char *sHostHeader;
    uint16_t uPort;
    const struct xllm_dialect_ops *pDialect;
    bool bEngineOwned;
    xnetengine *pNetEngine;
    xnetresolver *pResolver;
    xtlsverifier *pVerifier;
    xmutex *pConnectionMutex;
    xmutex ConfigMutex; /* profile, hook snapshot, and prefix cache */
    bool bConfigMutexInit;
    bool bConfigSealed; /* profile is immutable after first use */
    xllm_hooks tHooks;
    bool bHasHooks;
    xllm_connection *pIdleConnections[XLLM_MAX_IDLE_CONNECTIONS];
    uint32_t uIdleConnectionCount;
    uint32_t uMaxIdleConnections;
    /* --- wire-prefix cache (尾账 #3): inner bytes of the serialized
     * messages array for [0..iPrefixCacheMessages). Keyed by the
     * view-render stamp; the accumulator only grows between misses. */
    void *pPrefixCacheOwner;
    uint64_t uPrefixCacheStamp;
    size_t iPrefixCacheMessages;
    char *sPrefixCacheInner;
    size_t iPrefixCacheLen;
    size_t iPrefixCacheCap;
};

/* Assembly bookkeeping parallel to the response arrays so appends stay O(n). */
typedef struct xllm_tool_state
{
    size_t iIdLen;
    size_t iNameLen;
    size_t iArgsLen;
} xllm_tool_state;

typedef struct xllm_block_state
{
    size_t iTextLen;
} xllm_block_state;

struct xllm_call
{
    xllm_client *pClient;
    const struct xllm_dialect_ops *pDialect;
    xcancel *pCancel;
    /* async engine-driven transport */
    xpromise *pPromise;
    xfuture *pFuture;
    /* The current operation is one mutex-protected, reference-counted node.
     * Its slot and Watch references jointly keep the node and operation
     * Future alive across completion, cancellation, and teardown. */
    xmutex OpMutex;
    bool bOpMutexInit;
    xllm_op_node *pOpNode;
    /* Serializes the transport state machine with watchdog and caller-side
     * cancellation. Network callbacks never expose half-published phase or
     * connection state to teardown threads. */
    xmutex TransportMutex;
    bool bTransportMutexInit;
    uint64 uTimerId;
    xcancelwatch *pCancelWatch;
    volatile long iTransportActive;
    /* Terminal latch: 0 = running, first finisher wins via atomic add.
     * Diagnostics and connection release run only in the winner. */
    volatile long iTerminal;
    /* Timer lifetime: the engine timer callback runs exactly once per
     * accepted schedule and drops its reference there; Destroy waits for
     * zero after cancelling, closing the async-cancel use-after-free. */
    volatile long iTimerRefs;
    xllm_async_phase ePhase;
    bool bHeadReady;
    bool bBodyDone;
    bool bWireEnd;
    bool bReusable;
    size_t iSendOffset;
    size_t iSendPending; /* bytes in the in-flight TLS send chunk */
    size_t iWireOffset;
    uint32_t uMaxHttpHeadBytes;
    uint32_t uMaxResponseBodyBytes;
    uint32_t uMaxSseLineBytes;
    uint32_t uMaxSseEventBytes;
    bool bIdempotencyKeyPresent;
    xllm_hooks tHooks; /* owned call snapshot (request over client) */
    bool bHasHooks;
    xllm_request *pClonedRequest; /* deep copy when pOnRequest rewrote it */
    bool bDeferParse;             /* pOnResponseBody armed: buffer, parse at wait */
    bool bOfflineBody;            /* pre-send injection: canned response, no dial */
    xllm_connection *pConnection;
    xllm_buf tWire;
    xhttpfield tHeadFields[XLLM_HTTP_FIELD_LIMIT];
    xhttpfield tTrailers[XLLM_HTTP_TRAILER_LIMIT];
    xhttp1head tHead;
    xhttp1limits tHeadLimits;
    xhttp1bodyplan tPlan;
    xhttp1bodylimits tBodyLimits;
    xhttp1body tBody;
    xhttp1errorinfo tProtoErr;
    char *sRequestBody;
    char *sRequestHeader;
    size_t iRequestHeaderSize;
    uint64_t uDeadline;
    uint64_t uScopeDeadline;
    bool bScopeAttached;
    bool bStreamWanted;
    xllm_transport_result eTransportResult;
    xllm_response *pResponse;
    xllm_stream_callbacks tCallbacks;
    xllm_error tError;
    xllm_transport_diagnostics tHttpDiagnostics;
    /* owned per-request extra headers */
    char **pExtraHeaderNames;
    char **pExtraHeaderValues;
    size_t iExtraHeaderCount;
    /* SSE framing state */
    xllm_buf tLine;
    xllm_buf tEventData;
    xllm_buf tEventName;
    bool bHaveEventName;
    /* non-SSE fallback body */
    xllm_buf tRawBody;
    /* assembly bookkeeping */
    xllm_tool_state *pToolState;
    size_t iToolStateCap;
    xllm_block_state *pBlockState;
    size_t iBlockStateCap;
    /* dialect decoder scratch */
    size_t *pBlockMap; /* wire block index -> tool index + 1 (0 = none) */
    size_t iBlockMapCap;
    struct xllm_item_map *pItemMap; /* provider item id -> tool index */
    size_t iItemCount;
    size_t iItemCap;
    uint32_t uHttpStatus;
    uint32_t uAttempt;
    uint32_t uRetryAfterMs;
    char sRequestId[160];
    char sContentType[160];
    char *sSelectedModel;
    volatile long iCallbackActive;
    volatile long iClosing;
    bool bSse;
    bool bDone;
    bool bSawEvent;
    bool bCallbackCancelled;
    bool bWaited;
    bool bResponseTaken;
};

/* ------------------------------------------------------------------ */
/* Dialect layer                                                       */
/* ------------------------------------------------------------------ */

typedef struct xllm_dialect_ops
{
    const char *sName;
    const char *sPathSuffix; /* appended to the base URL when absent */
    /* Auth headers (owned values); returns count, 0 on allocation failure. */
    size_t (*BuildAuth)(const xllm_client *pClient, xllm_auth_header *pOut, size_t iCap);
    /* Serialize the unified request into a provider JSON body (owned). */
    char *(*BuildRequest)(xllm_client *pClient, const xllm_request *pRequest, xllm_error *pError);
    /* Incremental serialization (尾账 #3): append messages
     * [iPrefixCount..iMessageCount) into pInner (which already holds the
     * cached prefix bytes) and assemble the full body around it. Only
     * dialects whose per-message serialization is stateless implement
     * this; others leave it NULL and the client falls back. */
    char *(*BuildRequestCached)(xllm_client *pClient, const xllm_request *pRequest,
                                xllm_buf *pInner, size_t iPrefixCount, xllm_error *pError);
    /* Decode one assembled SSE event into unified events/response. */
    bool (*DecodeSseEvent)(xllm_call *pCall, const xllm_sse_fields *pFields);
    /* Decode a complete non-streaming JSON body. */
    bool (*DecodeJsonBody)(xllm_call *pCall, xstrview tBody);
    /* Extract provider error details from an error response body. */
    void (*FillProviderError)(xllm_call *pCall, xstrview tBody);
    /* Provider-specific retry classification beyond the common policy. */
    bool (*IsRetryableProviderError)(const xllm_error *pError);
} xllm_dialect_ops;

const xllm_dialect_ops *xllm__dialect_ops_for(xllm_provider eProvider);
const xllm_dialect_ops *xllm__dialect_completions(void);
const xllm_dialect_ops *xllm__dialect_anthropic(void);
const xllm_dialect_ops *xllm__dialect_responses(void);

/* ------------------------------------------------------------------ */
/* Shared utilities (single copy for all layers)                       */
/* ------------------------------------------------------------------ */

/* Production allocation is process-local CRT state with no mutable global
 * selector. Fault injection exists only in explicitly compiled test TUs. */
#if defined(XLLM_TEST_ALLOCATOR)
typedef struct xllm_allocator
{
    void *(*Alloc)(size_t iSize);
    void *(*Realloc)(void *pMemory, size_t iSize);
    void (*Free)(void *pMemory);
} xllm_allocator;

void xllm__set_allocator(const xllm_allocator *pAllocator);
size_t xllm__allocation_count(void);
#endif
void *xllm__malloc(size_t iSize);
void *xllm__calloc(size_t iCount, size_t iSize);
void *xllm__realloc(void *pMemory, size_t iSize);
void xllm__free(void *pMemory);

char *xllm__strdup(const char *sText);
bool xllm__replace(char **ppDst, const char *sText);
bool xllm__buf_reserve(xllm_buf *pBuf, size_t iNeed);
bool xllm__buf_append(xllm_buf *pBuf, const void *pData, size_t iLen);
bool xllm__buf_append_cstr(xllm_buf *pBuf, const char *sText);
bool xllm__buf_append_char(xllm_buf *pBuf, char ch);
void xllm__buf_reset(xllm_buf *pBuf);
char *xllm__buf_detach(xllm_buf *pBuf);
bool xllm__json_string(xllm_buf *pBuf, const char *sText);
bool xllm__json_string_content(xllm_buf *pBuf, const char *sText);
void xllm__error_set(xllm_error *pError, xllm_error_code eCode, const char *sMessage);
void xllm__error_cause_set(xllm_error *pError, xllm_error_domain eDomain, xllm_error_stage eStage,
                           int32_t iRuntimeKind, int32_t iRuntimeCode, int32_t iSystemCode,
                           const char *sOperation, const char *sMessage);
void xllm__error_copy(xllm_error *pDst, const xllm_error *pSrc);
void xllm__copy_text(char *sDst, size_t iCap, const char *sSrc);
bool xllm__utf8_valid(const char *sText);
void xllm__copy_view(char *sDst, size_t iCap, xstrview tValue);
bool xllm__contains_ci(const char *sText, const char *sNeedle);
/* Length-tracked owned-string append (no strlen on the existing tail). */
bool xllm__append_tracked(char **ppText, size_t *piLen, const char *sDelta, size_t iDeltaLen);

bool xllm__tool_call_clone(xllm_tool_call *pDst, const xllm_tool_call *pSrc);
void xllm__tool_call_unit(xllm_tool_call *pCall);
bool xllm__message_clone(xllm_message *pDst, const xllm_message *pSrc);
bool xllm__request_clone(xllm_request *pDst, const xllm_request *pSrc);
bool xllm__part_clone(xllm_part *pDst, const xllm_part *pSrc);
bool xllm__part_is_thinking_signature(const xllm_part *pPart);
void xllm__part_unit(xllm_part *pPart);
void xllm__tool_unit(xllm_tool *pTool);

/* ------------------------------------------------------------------ */
/* Response assembly                                                   */
/* ------------------------------------------------------------------ */

xllm_response *xllm__assemble_ensure(xllm_call *pCall);
bool xllm__assemble_metadata(xllm_call *pCall, xstrview tId, xstrview tModel);
bool xllm__assemble_text(xllm_call *pCall, xllm_block_kind eKind, xstrview tText, char *sNative);
bool xllm__assemble_native(xllm_call *pCall, xllm_block_kind eKind, xstrview tNative);
bool xllm__assemble_tool(xllm_call *pCall, size_t iIndex, xstrview tId, xstrview tName,
                         xstrview tArguments);
bool xllm__assemble_block_mark_tool(xllm_call *pCall, size_t iToolIndex);
size_t xllm__assemble_map_block_tool(xllm_call *pCall, size_t iWireIndex);
bool xllm__assemble_set_block_tool(xllm_call *pCall, size_t iWireIndex, size_t iToolIndex);
size_t xllm__assemble_find_item_tool(xllm_call *pCall, const char *sItemId);
size_t xllm__assemble_add_item_tool(xllm_call *pCall, const char *sItemId, xstrview tId,
                                    xstrview tName);
bool xllm__assemble_usage(xllm_call *pCall, const xllm_usage *pUsage);
bool xllm__assemble_finish(xllm_call *pCall, xstrview tRaw);
bool xllm__assemble_finalize(xllm_call *pCall);
bool xllm__assemble_refusal(xllm_call *pCall, xstrview tRefusal);
void xllm__assemble_first_token(xllm_call *pCall);

/* Generic helpers shared by dialect decoders (JSON navigation on xvalue). */
xvalue *xllm__json_get(xvalue *pObject, const char *sKey);
xstrview xllm__json_text(xvalue *pObject, const char *sKey);
uint64_t xllm__json_u64(xvalue *pObject, const char *sKey);

/* ------------------------------------------------------------------ */
/* SSE framing                                                         */
/* ------------------------------------------------------------------ */

bool xllm__sse_feed(xllm_call *pCall, const void *pData, size_t iLen);
bool xllm__sse_finish(xllm_call *pCall);
void xllm__sse_reset(xllm_call *pCall);

/* ------------------------------------------------------------------ */
/* Transport / call                                                    */
/* ------------------------------------------------------------------ */

bool xllm__emit(xllm_call *pCall, const xllm_event *pEvent);
bool xllm__transport_headers(xllm_call *pCall, const xhttp1head *pHead);
bool xllm__transport_body(xllm_call *pCall, const void *pData, size_t iLen);
bool xllm__transport_client_init(xllm_client *pClient, xllm_error *pError);
void xllm__transport_client_unit(xllm_client *pClient);
bool xllm__transport_arm_cancel(xllm_call *pCall);
/* Resolve the call promise and publish its terminal transport state exactly
 * once. Offline replay uses the same terminal path as real I/O. */
void xllm__transport_finish(xllm_call *pCall, xllm_transport_result eResult);
/* Submits the transport onto the client engine; the call promise reaches a
 * terminal state when the exchange completes, fails, or is cancelled. */
void xllm__transport_begin(xllm_call *pCall);
/* Abort in-flight transport and wait until the call reaches a terminal
 * state; safe from any thread, used by Wait-timeout and Destroy. */
void xllm__transport_abort(xllm_call *pCall);

/* Match a host against the allocation-free proxy bypass grammar. */
bool xllm__proxy_bypassed(const char *sBypass, const char *sHost);

static inline long xllm__atomic_add(volatile long *pValue, long iDelta)
{
#if defined(_MSC_VER)
    return _InterlockedExchangeAdd(pValue, iDelta) + iDelta;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(pValue, iDelta, __ATOMIC_SEQ_CST);
#else
    *pValue += iDelta;
    return *pValue;
#endif
}

static inline long xllm__atomic_load(volatile long *pValue)
{
#if defined(_MSC_VER)
    return _InterlockedCompareExchange(pValue, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_load_n(pValue, __ATOMIC_SEQ_CST);
#else
    return *pValue;
#endif
}

static inline void xllm__atomic_store(volatile long *pValue, long iValue)
{
#if defined(_MSC_VER)
    (void)_InterlockedExchange(pValue, iValue);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(pValue, iValue, __ATOMIC_SEQ_CST);
#else
    *pValue = iValue;
#endif
}

/* Shared implementation entry points for independent translation units. */
void xllm__op_disarm(xllm_call *pCall);

char *xllm__client_serialize_body(xllm_client *, const xllm_request *, xllm_error *);
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xllm/src/internal/xllm_proxy_future.h */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)
#ifndef XLLM_PROXY_FUTURE_H
#define XLLM_PROXY_FUTURE_H
/* TLS Dial Future 桥接一个受管 TLS Dial 与一个公开 Future。 */
typedef struct xllm_proxy_future
{
    xfuturebridge Bridge;
    volatile int32 References;
    xtlsdial *Dial;
} xllm_proxy_future;

/* Future 成功值只持有一个 TLS Stream 调用方引用。 */
static void xllm__proxy_future_Free(ptr pValue, ptr pData)
{
    (void)pData;
    xrtTlsStreamDestroy((xtlsstream *)pValue);
}

/* Future 的协作取消请求转发给 DNS、TCP 或 TLS 当前有效阶段。 */
static void xllm__proxy_future_Cancel(ptr pData)
{
    xllm_proxy_future *pContext = (xllm_proxy_future *)pData;

    (void)xrtTlsDialCancel(pContext->Dial);
}

/* 把不可能出现的空成功结果转换为结构化 TLS 内部错误。 */
static xerror *xllm__proxy_future_InvalidError(void)
{
    xerrordesc d;
    memset(&d, 0, sizeof(d));
    d.Kind = XERR_INTERNAL;
    d.Domain = "xllm.proxy";
    d.Operation = "complete-tls-proxy-dial";
    d.Message = "TLS proxy dial reported success without a stream";
    return xrtErrorBuild(&d);
}

/* 把 TLS Dial 唯一终态转发到 Promise，并处理装配失败竞态。 */
static void xllm__proxy_future_Done(xtlsdial *pDial, xnetresult Result, xtlsstream *pStream,
                                    const xerror *pError, ptr pData)
{
    xllm_proxy_future *pContext = (xllm_proxy_future *)pData;
    xpromise *pPromise = xrtFutureBridgePromise(&pContext->Bridge);
    xtlsdial *pHeld;
    xerror *pFailure = NULL;
    bool bReady;

    (void)pDial;
    bReady = xrtFutureBridgeWait(&pContext->Bridge);
    /* The submitter publishes Dial before releasing the setup gate. */
    pHeld = pContext->Dial;
    xrtFutureBridgeUnwatch(&pContext->Bridge);
    if ((Result != XNET_RESULT_OK) && (pError != NULL))
    {
        pFailure = xrtErrorRef(pError);
    }
    else if ((Result == XNET_RESULT_OK) && (pStream == NULL))
    {
        pFailure = xllm__proxy_future_InvalidError();
    }
    xrtTlsDialDestroy(pHeld);
    if (xrtRefRelease(&pContext->References) == 0)
    {
        xrtFree(pContext);
    }
    if (!bReady)
    {
        if (pStream != NULL)
        {
            (void)xrtTlsStreamAbort(pStream);
            xrtTlsStreamDestroy(pStream);
        }
    }
    else if ((Result == XNET_RESULT_OK) && (pStream != NULL))
    {
        if (!xrtPromiseResolveOwned(pPromise, pStream, xllm__proxy_future_Free, NULL))
        {
            (void)xrtTlsStreamAbort(pStream);
            xrtTlsStreamDestroy(pStream);
        }
    }
    else if (Result == XNET_RESULT_CANCELLED)
    {
        (void)xrtPromiseCancel(pPromise);
    }
    else if (pFailure != NULL)
    {
        (void)xrtPromiseReject(pPromise, pFailure);
    }
    else
    {
        (void)xrtPromiseClose(pPromise);
    }
    xrtErrorFree(pFailure);
    xrtPromiseDestroy(pPromise);
}

/* 创建受管 TLS Dial、Promise 和双向取消桥接。 */
static xfuture *xllm__proxy_dial_async(xnetengine *pEngine, xnetresolver *pResolver,
                                       const xnetproxy *pProxy, cstr sHost, uint16 iPort,
                                       const xtlsclientconfig *pTls, const xtlsdialconfig *pConfig,
                                       const xtlsstreamevents *pStreamEvents, ptr pStreamData)
{
    xllm_proxy_future *pContext;
    xfuture *pFuture;
    xerror *pError;

    pContext = (xllm_proxy_future *)xrtCalloc(1, sizeof(*pContext));
    if (pContext == NULL)
    {
        return NULL;
    }
    pFuture = xrtFutureBridgeCreate(&pContext->Bridge, NULL);
    if (pFuture == NULL)
    {
        xrtFree(pContext);
        return NULL;
    }
    pContext->References = 2;
    pContext->Dial = xrtTlsDialProxy(pEngine, pResolver, pProxy, sHost, iPort, pTls, pConfig,
                                     pStreamEvents, pStreamData, xllm__proxy_future_Done, pContext);
    if (pContext->Dial == NULL)
    {
        pError = xrtTakeError();
        xrtFutureDestroy(pFuture);
        xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
        xrtFree(pContext);
        if (pError != NULL)
        {
            xrtSetError(pError);
            xrtErrorFree(pError);
        }
        return NULL;
    }
    if (!xrtFutureBridgeWatch(&pContext->Bridge, xllm__proxy_future_Cancel, pContext))
    {
        pError = xrtTakeError();
        (void)xrtFutureBridgeFail(&pContext->Bridge);
        (void)xrtTlsDialCancel(pContext->Dial);
        xrtFutureDestroy(pFuture);
        if (pError != NULL)
        {
            xrtSetError(pError);
            xrtErrorFree(pError);
        }
        if (xrtRefRelease(&pContext->References) == 0)
        {
            xrtFree(pContext);
        }
        return NULL;
    }
    (void)xrtFutureBridgeReady(&pContext->Bridge);
    if (xrtRefRelease(&pContext->References) == 0)
    {
        xrtFree(pContext);
    }
    return pFuture;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_core.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

#if defined(XLLM_TEST_ALLOCATOR)
static void *xllm__default_alloc(size_t iSize) { return malloc(iSize); }
static void *xllm__default_realloc(void *pMemory, size_t iSize) { return realloc(pMemory, iSize); }
static void xllm__default_free(void *pMemory) { free(pMemory); }

static xllm_allocator xllm__g_allocator = {xllm__default_alloc, xllm__default_realloc,
                                           xllm__default_free};
static volatile long xllm__g_allocations;
#endif

/* Strict UTF-8 scalar validation: rejects overlong forms, surrogates,
 * and code points beyond U+10FFFF. Message content rides into provider JSON
 * verbatim, so invalid UTF-8 must stop at the API boundary. */
bool xllm__utf8_valid(const char *sText)
{
    const unsigned char *p = (const unsigned char *)(sText ? sText : "");
    while (*p)
    {
        unsigned char c = *p;
        size_t iExtra;
        uint32_t cp;
        uint32_t uMinimum;
        if ( c < 0x80u ) { ++p; continue; }
        if ( c >= 0xC2u && c <= 0xDFu ) { iExtra = 1u; cp = c & 0x1Fu; uMinimum = 0x80u; }
        else if ( c >= 0xE0u && c <= 0xEFu ) { iExtra = 2u; cp = c & 0x0Fu; uMinimum = 0x800u; }
        else if ( c >= 0xF0u && c <= 0xF4u ) { iExtra = 3u; cp = c & 0x07u; uMinimum = 0x10000u; }
        else { return false; }
        ++p;
        while (iExtra--)
        {
            if ((*p & 0xC0u) != 0x80u)
            {
                return false;
            }
            cp = (cp << 6u) | (unsigned char)(*p & 0x3Fu);
            ++p;
        }
        if ( cp < uMinimum || (cp >= 0xD800u && cp <= 0xDFFFu) || cp > 0x10FFFFu ) { return false; }
    }
    return true;
}

#if defined(XLLM_TEST_ALLOCATOR)
void xllm__set_allocator(const xllm_allocator *pAllocator)
{
    if (pAllocator)
    {
        xllm__g_allocator = *pAllocator;
    }
    else
    {
        xllm__g_allocator.Alloc = xllm__default_alloc;
        xllm__g_allocator.Realloc = xllm__default_realloc;
        xllm__g_allocator.Free = xllm__default_free;
    }
    xllm__atomic_store(&xllm__g_allocations, 0);
}

size_t xllm__allocation_count(void) { return (size_t)xllm__atomic_load(&xllm__g_allocations); }
#endif

void *xllm__malloc(size_t iSize)
{
#if defined(XLLM_TEST_ALLOCATOR)
    (void)xllm__atomic_add(&xllm__g_allocations, 1);
    return xllm__g_allocator.Alloc(iSize);
#else
    return malloc(iSize);
#endif
}

void *xllm__calloc(size_t iCount, size_t iSize)
{
    void *pMemory;
    if (iCount && iSize > SIZE_MAX / iCount)
    {
        return NULL;
    }
#if defined(XLLM_TEST_ALLOCATOR)
    (void)xllm__atomic_add(&xllm__g_allocations, 1);
    pMemory = xllm__g_allocator.Alloc(iCount * iSize);
#else
    pMemory = malloc(iCount * iSize);
#endif
    if (pMemory)
    {
        memset(pMemory, 0, iCount * iSize);
    }
    return pMemory;
}

void *xllm__realloc(void *pMemory, size_t iSize)
{
#if defined(XLLM_TEST_ALLOCATOR)
    return xllm__g_allocator.Realloc(pMemory, iSize);
#else
    return realloc(pMemory, iSize);
#endif
}

void xllm__free(void *pMemory)
{
#if defined(XLLM_TEST_ALLOCATOR)
    xllm__g_allocator.Free(pMemory);
#else
    free(pMemory);
#endif
}

char *xllm__strdup(const char *sText)
{
    size_t iLen;
    char *sCopy;
    if (!sText)
    {
        return NULL;
    }
    iLen = strlen(sText);
    sCopy = (char *)xllm__malloc(iLen + 1u);
    if (!sCopy)
    {
        return NULL;
    }
    memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

bool xllm__replace(char **ppDst, const char *sText)
{
    char *sCopy = sText ? xllm__strdup(sText) : NULL;
    if (sText && !sCopy)
    {
        return false;
    }
    xllm__free(*ppDst);
    *ppDst = sCopy;
    return true;
}

bool xllm__buf_reserve(xllm_buf *pBuf, size_t iNeed)
{
    size_t iCap;
    char *pNew;
    if (!pBuf)
    {
        return false;
    }
    if (iNeed <= pBuf->iCap)
    {
        return true;
    }
    iCap = pBuf->iCap ? pBuf->iCap : 256u;
    while (iCap < iNeed)
    {
        if (iCap > SIZE_MAX / 2u)
        {
            iCap = iNeed;
            break;
        }
        iCap *= 2u;
    }
    if (iCap < iNeed)
    {
        return false;
    }
    pNew = (char *)xllm__realloc(pBuf->pData, iCap);
    if (!pNew)
    {
        return false;
    }
    pBuf->pData = pNew;
    pBuf->iCap = iCap;
    return true;
}

bool xllm__buf_append(xllm_buf *pBuf, const void *pData, size_t iLen)
{
    if (!pBuf || (!pData && iLen) || pBuf->iLen == SIZE_MAX || iLen > SIZE_MAX - pBuf->iLen - 1u)
    {
        return false;
    }
    if (!xllm__buf_reserve(pBuf, pBuf->iLen + iLen + 1u))
    {
        return false;
    }
    if (iLen)
    {
        memcpy(pBuf->pData + pBuf->iLen, pData, iLen);
    }
    pBuf->iLen += iLen;
    pBuf->pData[pBuf->iLen] = '\0';
    return true;
}

bool xllm__buf_append_cstr(xllm_buf *pBuf, const char *sText)
{
    return xllm__buf_append(pBuf, sText ? sText : "", sText ? strlen(sText) : 0u);
}

bool xllm__buf_append_char(xllm_buf *pBuf, char ch) { return xllm__buf_append(pBuf, &ch, 1u); }

void xllm__buf_reset(xllm_buf *pBuf)
{
    if (!pBuf)
    {
        return;
    }
    xllm__free(pBuf->pData);
    memset(pBuf, 0, sizeof(*pBuf));
}

char *xllm__buf_detach(xllm_buf *pBuf)
{
    char *pData;
    if (!pBuf)
    {
        return NULL;
    }
    if (!pBuf->pData)
    {
        pBuf->pData = (char *)xllm__calloc(1u, 1u);
        if (!pBuf->pData)
        {
            return NULL;
        }
    }
    pData = pBuf->pData;
    pBuf->pData = NULL;
    pBuf->iLen = 0u;
    pBuf->iCap = 0u;
    return pData;
}

bool xllm__json_string_content(xllm_buf *pBuf, const char *sText)
{
    const unsigned char *p = (const unsigned char *)(sText ? sText : "");
    char sEscape[7];
    while (*p)
    {
        switch (*p)
        {
        case '"':
            if (!xllm__buf_append_cstr(pBuf, "\\\""))
                return false;
            break;
        case '\\':
            if (!xllm__buf_append_cstr(pBuf, "\\\\"))
                return false;
            break;
        case '\b':
            if (!xllm__buf_append_cstr(pBuf, "\\b"))
                return false;
            break;
        case '\f':
            if (!xllm__buf_append_cstr(pBuf, "\\f"))
                return false;
            break;
        case '\n':
            if (!xllm__buf_append_cstr(pBuf, "\\n"))
                return false;
            break;
        case '\r':
            if (!xllm__buf_append_cstr(pBuf, "\\r"))
                return false;
            break;
        case '\t':
            if (!xllm__buf_append_cstr(pBuf, "\\t"))
                return false;
            break;
        default:
            if (*p < 0x20u)
            {
                (void)snprintf(sEscape, sizeof(sEscape), "\\u%04x", (unsigned)*p);
                if (!xllm__buf_append_cstr(pBuf, sEscape))
                    return false;
            }
            else if (!xllm__buf_append(pBuf, p, 1u))
            {
                return false;
            }
            break;
        }
        ++p;
    }
    return true;
}

bool xllm__json_string(xllm_buf *pBuf, const char *sText)
{
    return xllm__buf_append_char(pBuf, '"') && xllm__json_string_content(pBuf, sText) &&
           xllm__buf_append_char(pBuf, '"');
}

void xllm__copy_text(char *sDst, size_t iCap, const char *sSrc)
{
    size_t iLen;
    if (!sDst || iCap == 0u)
    {
        return;
    }
    if (!sSrc)
    {
        sDst[0] = '\0';
        return;
    }
    iLen = strlen(sSrc);
    if (iLen >= iCap)
    {
        iLen = iCap - 1u;
    }
    memcpy(sDst, sSrc, iLen);
    sDst[iLen] = '\0';
}

void xllm__copy_view(char *sDst, size_t iCap, xstrview tValue)
{
    size_t iCopy;
    if (!sDst || !iCap)
        return;
    iCopy = tValue.Size < iCap - 1u ? tValue.Size : iCap - 1u;
    if (iCopy)
        memcpy(sDst, tValue.Data, iCopy);
    sDst[iCopy] = 0;
}

void xllmErrorInit(xllm_error *pError)
{
    if (pError)
    {
        memset(pError, 0, sizeof(*pError));
        pError->tCause.Size = (uint32_t)sizeof(pError->tCause);
        pError->tCause.Version = XLLM_ERROR_CAUSE_VERSION;
    }
}

static xllm_error_domain xllm__error_default_domain(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_INVALID_ARGUMENT:
        return XLLM_ERROR_DOMAIN_API;
    case XLLM_ERROR_OUT_OF_MEMORY:
        return XLLM_ERROR_DOMAIN_MEMORY;
    case XLLM_ERROR_NETWORK:
    case XLLM_ERROR_TIMEOUT:
    case XLLM_ERROR_CANCELLED:
        return XLLM_ERROR_DOMAIN_TRANSPORT;
    case XLLM_ERROR_AUTH:
    case XLLM_ERROR_RATE_LIMIT:
    case XLLM_ERROR_MODEL_NOT_FOUND:
    case XLLM_ERROR_UPSTREAM:
        return XLLM_ERROR_DOMAIN_PROVIDER;
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
    case XLLM_ERROR_OUTPUT_LIMIT:
    case XLLM_ERROR_PROTOCOL:
    case XLLM_ERROR_PARSE:
        return XLLM_ERROR_DOMAIN_PARSER;
    case XLLM_ERROR_HOOK:
        return XLLM_ERROR_DOMAIN_HOOK;
    case XLLM_ERROR_LIMIT:
        return XLLM_ERROR_DOMAIN_RESOURCE;
    case XLLM_ERROR_NONE:
    default:
        return XLLM_ERROR_DOMAIN_NONE;
    }
}

static xllm_error_stage xllm__error_default_stage(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_INVALID_ARGUMENT:
        return XLLM_ERROR_STAGE_VALIDATE;
    case XLLM_ERROR_OUT_OF_MEMORY:
        return XLLM_ERROR_STAGE_NONE;
    case XLLM_ERROR_NETWORK:
        return XLLM_ERROR_STAGE_CONNECT;
    case XLLM_ERROR_TIMEOUT:
    case XLLM_ERROR_CANCELLED:
        return XLLM_ERROR_STAGE_WAIT;
    case XLLM_ERROR_AUTH:
    case XLLM_ERROR_RATE_LIMIT:
    case XLLM_ERROR_MODEL_NOT_FOUND:
    case XLLM_ERROR_UPSTREAM:
        return XLLM_ERROR_STAGE_PROVIDER;
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
    case XLLM_ERROR_OUTPUT_LIMIT:
    case XLLM_ERROR_PROTOCOL:
    case XLLM_ERROR_PARSE:
        return XLLM_ERROR_STAGE_PARSE;
    case XLLM_ERROR_HOOK:
        return XLLM_ERROR_STAGE_HOOK;
    case XLLM_ERROR_LIMIT:
        return XLLM_ERROR_STAGE_LIMIT;
    case XLLM_ERROR_NONE:
    default:
        return XLLM_ERROR_STAGE_NONE;
    }
}

static const char *xllm__error_default_operation(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_INVALID_ARGUMENT:
        return "validate";
    case XLLM_ERROR_OUT_OF_MEMORY:
        return "allocate";
    case XLLM_ERROR_NETWORK:
        return "transport";
    case XLLM_ERROR_TIMEOUT:
    case XLLM_ERROR_CANCELLED:
        return "wait";
    case XLLM_ERROR_AUTH:
    case XLLM_ERROR_RATE_LIMIT:
    case XLLM_ERROR_MODEL_NOT_FOUND:
    case XLLM_ERROR_UPSTREAM:
        return "provider-response";
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
    case XLLM_ERROR_OUTPUT_LIMIT:
    case XLLM_ERROR_PROTOCOL:
    case XLLM_ERROR_PARSE:
        return "parse-response";
    case XLLM_ERROR_HOOK:
        return "host-hook";
    case XLLM_ERROR_LIMIT:
        return "enforce-limit";
    case XLLM_ERROR_NONE:
    default:
        return "";
    }
}

void xllm__error_cause_set(xllm_error *pError, xllm_error_domain eDomain, xllm_error_stage eStage,
                           int32_t iRuntimeKind, int32_t iRuntimeCode, int32_t iSystemCode,
                           const char *sOperation, const char *sMessage)
{
    if (!pError)
    {
        return;
    }
    memset(&pError->tCause, 0, sizeof(pError->tCause));
    pError->tCause.Size = (uint32_t)sizeof(pError->tCause);
    pError->tCause.Version = XLLM_ERROR_CAUSE_VERSION;
    pError->tCause.eDomain = eDomain;
    pError->tCause.eStage = eStage;
    pError->tCause.iRuntimeKind = iRuntimeKind;
    pError->tCause.iRuntimeCode = iRuntimeCode;
    pError->tCause.iSystemCode = iSystemCode;
    xllm__copy_text(pError->tCause.sOperation, sizeof(pError->tCause.sOperation), sOperation);
    xllm__copy_text(pError->tCause.sMessage, sizeof(pError->tCause.sMessage), sMessage);
}

void xllm__error_set(xllm_error *pError, xllm_error_code eCode, const char *sMessage)
{
    bool bKeepRuntimeCause;
    if (!pError)
    {
        return;
    }
    bKeepRuntimeCause = pError->tCause.Size == sizeof(pError->tCause) &&
                        pError->tCause.Version == XLLM_ERROR_CAUSE_VERSION &&
                        pError->tCause.eDomain == XLLM_ERROR_DOMAIN_TRANSPORT &&
                        (eCode == XLLM_ERROR_NETWORK || eCode == XLLM_ERROR_TIMEOUT ||
                         eCode == XLLM_ERROR_CANCELLED);
    pError->eCode = eCode;
    xllm__copy_text(pError->sMessage, sizeof(pError->sMessage), sMessage);
    if (!bKeepRuntimeCause)
    {
        xllm__error_cause_set(pError, xllm__error_default_domain(eCode),
                              xllm__error_default_stage(eCode), 0, 0, 0,
                              xllm__error_default_operation(eCode), sMessage);
    }
}

void xllm__error_copy(xllm_error *pDst, const xllm_error *pSrc)
{
    if (pDst)
    {
        if (pSrc)
        {
            memcpy(pDst, pSrc, sizeof(*pDst));
        }
        else
        {
            xllmErrorInit(pDst);
        }
    }
}

const char *xllmErrorCodeName(xllm_error_code eCode)
{
    switch (eCode)
    {
    case XLLM_ERROR_NONE:
        return "none";
    case XLLM_ERROR_INVALID_ARGUMENT:
        return "invalid_argument";
    case XLLM_ERROR_OUT_OF_MEMORY:
        return "out_of_memory";
    case XLLM_ERROR_NETWORK:
        return "network";
    case XLLM_ERROR_TIMEOUT:
        return "timeout";
    case XLLM_ERROR_CANCELLED:
        return "cancelled";
    case XLLM_ERROR_AUTH:
        return "auth";
    case XLLM_ERROR_RATE_LIMIT:
        return "rate_limit";
    case XLLM_ERROR_MODEL_NOT_FOUND:
        return "model_not_found";
    case XLLM_ERROR_UPSTREAM:
        return "upstream";
    case XLLM_ERROR_PROTOCOL:
        return "protocol";
    case XLLM_ERROR_INCOMPLETE_RESPONSE:
        return "incomplete_response";
    case XLLM_ERROR_OUTPUT_LIMIT:
        return "output_limit";
    case XLLM_ERROR_PARSE:
        return "parse";
    case XLLM_ERROR_LIMIT:
        return "limit";
    case XLLM_ERROR_HOOK:
        return "hook";
    default:
        return "unknown";
    }
}

const char *xllmErrorDomainName(xllm_error_domain eDomain)
{
    switch (eDomain)
    {
    case XLLM_ERROR_DOMAIN_NONE:
        return "none";
    case XLLM_ERROR_DOMAIN_API:
        return "xllm.api";
    case XLLM_ERROR_DOMAIN_MEMORY:
        return "xllm.memory";
    case XLLM_ERROR_DOMAIN_TRANSPORT:
        return "xllm.transport";
    case XLLM_ERROR_DOMAIN_PROVIDER:
        return "xllm.provider";
    case XLLM_ERROR_DOMAIN_PARSER:
        return "xllm.parser";
    case XLLM_ERROR_DOMAIN_HOOK:
        return "xllm.hook";
    case XLLM_ERROR_DOMAIN_RESOURCE:
        return "xllm.resource";
    default:
        return "unknown";
    }
}

const char *xllmErrorStageName(xllm_error_stage eStage)
{
    switch (eStage)
    {
    case XLLM_ERROR_STAGE_NONE:
        return "none";
    case XLLM_ERROR_STAGE_VALIDATE:
        return "validate";
    case XLLM_ERROR_STAGE_BUILD_REQUEST:
        return "build_request";
    case XLLM_ERROR_STAGE_RESOLVE:
        return "resolve";
    case XLLM_ERROR_STAGE_CONNECT:
        return "connect";
    case XLLM_ERROR_STAGE_PROXY:
        return "proxy";
    case XLLM_ERROR_STAGE_TLS:
        return "tls";
    case XLLM_ERROR_STAGE_WRITE:
        return "write";
    case XLLM_ERROR_STAGE_READ:
        return "read";
    case XLLM_ERROR_STAGE_HTTP:
        return "http";
    case XLLM_ERROR_STAGE_PARSE:
        return "parse";
    case XLLM_ERROR_STAGE_PROVIDER:
        return "provider";
    case XLLM_ERROR_STAGE_HOOK:
        return "hook";
    case XLLM_ERROR_STAGE_LIMIT:
        return "limit";
    case XLLM_ERROR_STAGE_WAIT:
        return "wait";
    default:
        return "unknown";
    }
}

const char *xllmFinishReasonName(xllm_finish eFinish)
{
    switch (eFinish)
    {
    case XLLM_FINISH_STOP:
        return "stop";
    case XLLM_FINISH_LENGTH:
        return "length";
    case XLLM_FINISH_TOOL_CALLS:
        return "tool_calls";
    case XLLM_FINISH_CONTENT_FILTER:
        return "content_filter";
    case XLLM_FINISH_REFUSAL:
        return "refusal";
    case XLLM_FINISH_OTHER:
        return "other";
    default:
        return "unknown";
    }
}

bool xllmErrorRetryable(const xllm_error *pError)
{
    int32_t iHttpStatus;
    bool bSafeToRepeat;
    if (!pError || pError->eCode == XLLM_ERROR_NONE || pError->tDiagnostics.bModelDataDelivered)
    {
        return false;
    }
    /* Session persistence reuses the legacy NETWORK top-level code for ABI
     * compatibility, but its structured RESOURCE cause is local storage and
     * must never be retried as a provider request. */
    if (pError->tCause.eDomain == XLLM_ERROR_DOMAIN_RESOURCE &&
        (pError->tCause.eStage == XLLM_ERROR_STAGE_READ ||
         pError->tCause.eStage == XLLM_ERROR_STAGE_WRITE))
    {
        return false;
    }
    /* Provider policy code 1313 requires an explicit account-side review.
       Retrying it as a transient 429 only creates avoidable traffic. */
    if (strcmp(pError->sProviderCode, "1313") == 0)
    {
        return false;
    }
    iHttpStatus = pError->iHttpStatus;
    if (iHttpStatus == 408 || iHttpStatus == 409 || iHttpStatus == 425 || iHttpStatus == 429 ||
        iHttpStatus == 500 || iHttpStatus == 502 || iHttpStatus == 503 || iHttpStatus == 504)
    {
        return true;
    }
    if (pError->eCode == XLLM_ERROR_RATE_LIMIT)
    {
        return true;
    }
    /* Once any request byte reached the transport, a disconnect does not tell
     * us whether the provider accepted the operation. Only an explicit
     * caller-supplied idempotency key makes an automatic replay admissible. */
    bSafeToRepeat =
        pError->tDiagnostics.uRequestBytes == 0u || pError->tDiagnostics.bIdempotencyKeyPresent;
    if (pError->eCode == XLLM_ERROR_NETWORK)
    {
        return bSafeToRepeat;
    }
    if (pError->eCode == XLLM_ERROR_TIMEOUT)
    {
        return bSafeToRepeat &&
               strcmp(pError->tDiagnostics.sTransportError, "deadline_exceeded") != 0;
    }
    return false;
}

bool xllm__contains_ci(const char *sText, const char *sNeedle)
{
    size_t iNeedle;
    if (!sText || !sNeedle)
    {
        return false;
    }
    iNeedle = strlen(sNeedle);
    if (iNeedle == 0u)
    {
        return true;
    }
    for (; *sText; ++sText)
    {
        size_t i;
        for (i = 0u; i < iNeedle; ++i)
        {
            unsigned char a = (unsigned char)sText[i];
            unsigned char b = (unsigned char)sNeedle[i];
            if (!a || (unsigned char)tolower(a) != (unsigned char)tolower(b))
            {
                break;
            }
        }
        if (i == iNeedle)
        {
            return true;
        }
    }
    return false;
}

/* Length-tracked append: callers keep the current length so incremental
 * deltas never rescan the accumulated tail. */
bool xllm__append_tracked(char **ppText, size_t *piLen, const char *sDelta, size_t iDeltaLen)
{
    char *pNew;
    if (!ppText || !piLen || (!sDelta && iDeltaLen))
    {
        return false;
    }
    if (*piLen == SIZE_MAX || iDeltaLen > SIZE_MAX - *piLen - 1u)
    {
        return false;
    }
    pNew = (char *)xllm__realloc(*ppText, *piLen + iDeltaLen + 1u);
    if (!pNew)
    {
        return false;
    }
    if (iDeltaLen)
    {
        memcpy(pNew + *piLen, sDelta, iDeltaLen);
    }
    pNew[*piLen + iDeltaLen] = '\0';
    *ppText = pNew;
    *piLen += iDeltaLen;
    return true;
}

void xllm__tool_call_unit(xllm_tool_call *pCall)
{
    if (!pCall)
    {
        return;
    }
    xllm__free(pCall->sId);
    xllm__free(pCall->sName);
    xllm__free(pCall->sArgumentsJson);
    memset(pCall, 0, sizeof(*pCall));
}

bool xllm__tool_call_clone(xllm_tool_call *pDst, const xllm_tool_call *pSrc)
{
    if (!pDst || !pSrc)
    {
        return false;
    }
    memset(pDst, 0, sizeof(*pDst));
    if (pSrc->sId && !(pDst->sId = xllm__strdup(pSrc->sId)))
        goto fail;
    if (pSrc->sName && !(pDst->sName = xllm__strdup(pSrc->sName)))
        goto fail;
    if (pSrc->sArgumentsJson && !(pDst->sArgumentsJson = xllm__strdup(pSrc->sArgumentsJson)))
        goto fail;
    return true;
fail:
    xllm__tool_call_unit(pDst);
    return false;
}

void xllmFree(void *pMemory) { xllm__free(pMemory); }
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_profile.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

#define XLLM_GLM_AGENT_CAPS ( \
    XLLM_CAP_TEXT_IN | XLLM_CAP_TOOL_RESULT_IN | XLLM_CAP_TEXT_OUT | \
    XLLM_CAP_JSON_OUT | XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_REASONING_OUT | \
    XLLM_CAP_STREAM | XLLM_CAP_REASONING_CONTROL | XLLM_CAP_PARALLEL_TOOL_CALL)

static const xllm_model_profile XLLM_BUILTIN_PROFILES[] = {
    {"glm-5.2-coding", "glm-5.2", XLLM_PROVIDER_GLM, XLLM_GLM_AGENT_CAPS,
     XLLM_WINDOW_SHARED_CONTEXT, 1000000ull, 999999ull, 131072u, 65536u, 32768u},
    {"glm-5.1-coding", "glm-5.1", XLLM_PROVIDER_GLM, XLLM_GLM_AGENT_CAPS,
     XLLM_WINDOW_SHARED_CONTEXT, 204800ull, 204799ull, 131072u, 32768u, 32768u},
    {"glm-5-coding", "glm-5", XLLM_PROVIDER_GLM, XLLM_GLM_AGENT_CAPS, XLLM_WINDOW_SHARED_CONTEXT,
     204800ull, 204799ull, 131072u, 32768u, 32768u}};

#undef XLLM_GLM_AGENT_CAPS

static bool xllm_profile__equal_ci(const char* a, const char* b)
{
    if (!a || !b)
        return false;
    while (*a && *b)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

void xllmModelProfileInit(xllm_model_profile *pProfile)
{
    if (!pProfile)
        return;
    memset(pProfile, 0, sizeof(*pProfile));
    pProfile->eWindowMode = XLLM_WINDOW_SHARED_CONTEXT;
}

const xllm_model_profile *xllmModelProfileBuiltin(const char *sIdOrModel)
{
    size_t i;
    if (!sIdOrModel || !sIdOrModel[0])
        return NULL;
    for (i = 0u; i < sizeof(XLLM_BUILTIN_PROFILES) / sizeof(XLLM_BUILTIN_PROFILES[0]); ++i)
    {
        if (xllm_profile__equal_ci(sIdOrModel, XLLM_BUILTIN_PROFILES[i].sId) ||
            xllm_profile__equal_ci(sIdOrModel, XLLM_BUILTIN_PROFILES[i].sModel))
        {
            return &XLLM_BUILTIN_PROFILES[i];
        }
    }
    return NULL;
}

bool xllmModelProfileValidate(const xllm_model_profile *pProfile, xllm_error *pError)
{
    const xllm_capability_flags uBasic = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    if (pError)
        xllmErrorInit(pError);
    if (!pProfile || !pProfile->sId || !pProfile->sId[0] || !pProfile->sModel ||
        !pProfile->sModel[0])
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "model profile id and model are required");
        return false;
    }
    if ((pProfile->uCapabilities & uBasic) != uBasic ||
        pProfile->eWindowMode == XLLM_WINDOW_UNSPECIFIED || pProfile->uContextWindowTokens == 0u ||
        pProfile->uMaxOutputTokens == 0u || pProfile->uMaxInputTokens == 0u ||
        pProfile->uMaxInputTokens > pProfile->uContextWindowTokens ||
        pProfile->uMaxOutputTokens > pProfile->uContextWindowTokens ||
        pProfile->uRecommendedOutputReserveTokens > pProfile->uMaxOutputTokens ||
        pProfile->uRecommendedSummaryTokens > pProfile->uMaxOutputTokens)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "model profile has invalid capabilities or token limits");
        return false;
    }
    if ((pProfile->uCapabilities & XLLM_CAP_PARALLEL_TOOL_CALL) != 0u &&
        (pProfile->uCapabilities & XLLM_CAP_TOOL_CALL_OUT) == 0u)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "parallel tools require tool-call output capability");
        return false;
    }
    return true;
}

bool xllmModelProfileSupports(const xllm_model_profile *pProfile, xllm_capability_flags uRequired)
{
    return pProfile && (pProfile->uCapabilities & uRequired) == uRequired;
}

bool xllmModelProfileValidateRequest(const xllm_model_profile *pProfile,
                                     const xllm_request *pRequest, xllm_error *pError)
{
    xllm_capability_flags uRequired = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    uint32_t uOutput;
    size_t i;
    if (pError)
        xllmErrorInit(pError);
    if (!xllmModelProfileValidate(pProfile, pError) || !pRequest)
    {
        if (pRequest == NULL)
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "model request is required");
        return false;
    }
    if (pRequest->sModel && pRequest->sModel[0] &&
        !xllm_profile__equal_ci(pRequest->sModel, pProfile->sModel))
    {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND,
                        "request model does not match the active model profile");
        return false;
    }
    if (pRequest->iToolCount != 0u || pRequest->eToolChoice == XLLM_TOOL_CHOICE_REQUIRED ||
        pRequest->eToolChoice == XLLM_TOOL_CHOICE_NAMED)
        uRequired |= XLLM_CAP_TOOL_CALL_OUT;
    if (pRequest->bParallelToolCalls)
        uRequired |= XLLM_CAP_PARALLEL_TOOL_CALL;
    if (pRequest->sReasoningEffort && pRequest->sReasoningEffort[0])
        uRequired |= XLLM_CAP_REASONING_CONTROL;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        size_t j;
        if (pMessage->eRole == XLLM_ROLE_TOOL)
            uRequired |= XLLM_CAP_TOOL_RESULT_IN;
        if (pMessage->iToolCallCount != 0u)
            uRequired |= XLLM_CAP_TOOL_CALL_OUT;
        for (j = 0u; j < pMessage->iPartCount; ++j)
        {
            if (pMessage->pParts[j].eKind == XLLM_PART_IMAGE ||
                pMessage->pParts[j].eKind == XLLM_PART_AUDIO ||
                pMessage->pParts[j].eKind == XLLM_PART_FILE)
            {
                uRequired |= XLLM_CAP_IMAGE_IN;
            }
        }
    }
    if (!xllmModelProfileSupports(pProfile, uRequired))
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "request requires capabilities not declared by the model profile");
        return false;
    }
    uOutput = pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pProfile->uMaxOutputTokens;
    if (uOutput > pProfile->uMaxOutputTokens)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "request output limit exceeds the model profile");
        return false;
    }
    return true;
}
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_parts.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* ------------------------------------------------------------------ */
/* Parts                                                               */
/* ------------------------------------------------------------------ */

void xllmPartInit(xllm_part *pPart, xllm_part_kind eKind)
{
    if (!pPart)
    {
        return;
    }
    memset(pPart, 0, sizeof(*pPart));
    pPart->eKind = eKind;
}

static uint8_t *xllm__part_copy_bytes(const void *pData, size_t iSize)
{
    uint8_t *pCopy;
    if (!iSize)
        return NULL;
    if (!pData)
        return NULL;
    pCopy = (uint8_t *)xllm__malloc(iSize);
    if (!pCopy)
        return NULL;
    memcpy(pCopy, pData, iSize);
    return pCopy;
}

static bool xllm__part_set_data(xllm_part *pPart, xllm_part_kind eKind, const void *pData,
                                size_t iSize, const char *sMediaType)
{
    char *sMediaCopy;
    uint8_t *pDataCopy;
    if (!pPart || !sMediaType || !sMediaType[0] || (!pData && iSize))
    {
        return false;
    }
    sMediaCopy = xllm__strdup(sMediaType);
    if (!sMediaCopy)
        return false;
    pDataCopy = xllm__part_copy_bytes(pData, iSize);
    if (iSize && !pDataCopy)
    {
        xllm__free(sMediaCopy);
        return false;
    }
    xllm__free(pPart->sMediaType);
    xllm__free(pPart->pData);
    xllm__free(pPart->sSourceUrl);
    pPart->sMediaType = sMediaCopy;
    pPart->pData = pDataCopy;
    pPart->iDataSize = iSize;
    pPart->sSourceUrl = NULL;
    pPart->eKind = eKind;
    return true;
}

bool xllmPartSetText(xllm_part *pPart, const char *sText)
{
    char *sCopy;
    if (!pPart || !xllm__utf8_valid(sText))
    {
        return false;
    }
    sCopy = xllm__strdup(sText ? sText : "");
    if (!sCopy)
        return false;
    xllm__free(pPart->sText);
    pPart->sText = sCopy;
    pPart->eKind = XLLM_PART_TEXT;
    return true;
}

bool xllmPartSetImageData(xllm_part *pPart, const void *pData, size_t iSize, const char *sMediaType)
{
    return xllm__part_set_data(pPart, XLLM_PART_IMAGE, pData, iSize, sMediaType);
}

bool xllmPartSetImageUrl(xllm_part *pPart, const char *sUrl, const char *sMediaType)
{
    char *sUrlCopy;
    char *sMediaCopy = NULL;
    if (!pPart || !sUrl || !sUrl[0])
    {
        return false;
    }
    sUrlCopy = xllm__strdup(sUrl);
    if (!sUrlCopy)
        return false;
    if (sMediaType)
    {
        sMediaCopy = xllm__strdup(sMediaType);
        if (!sMediaCopy)
        {
            xllm__free(sUrlCopy);
            return false;
        }
    }
    xllm__free(pPart->sSourceUrl);
    pPart->sSourceUrl = sUrlCopy;
    if (sMediaType)
    {
        xllm__free(pPart->sMediaType);
        pPart->sMediaType = sMediaCopy;
    }
    xllm__free(pPart->pData);
    pPart->pData = NULL;
    pPart->iDataSize = 0u;
    pPart->eKind = XLLM_PART_IMAGE;
    return true;
}

bool xllmPartSetAudioData(xllm_part *pPart, const void *pData, size_t iSize, const char *sMediaType)
{
    return xllm__part_set_data(pPart, XLLM_PART_AUDIO, pData, iSize, sMediaType);
}

bool xllmPartSetFileData(xllm_part *pPart, const void *pData, size_t iSize, const char *sMediaType)
{
    return xllm__part_set_data(pPart, XLLM_PART_FILE, pData, iSize, sMediaType);
}

bool xllmPartSetNative(xllm_part *pPart, const char *sNativeType, const char *sJson)
{
    char *sTypeCopy;
    char *sJsonCopy;
    if (!pPart || !sJson || !sJson[0] || !xrtJsonValid((xstrview){sJson, strlen(sJson)}))
    {
        return false;
    }
    sTypeCopy = xllm__strdup(sNativeType ? sNativeType : "");
    if (!sTypeCopy)
        return false;
    sJsonCopy = xllm__strdup(sJson);
    if (!sJsonCopy)
    {
        xllm__free(sTypeCopy);
        return false;
    }
    xllm__free(pPart->sNativeType);
    xllm__free(pPart->sText);
    pPart->sNativeType = sTypeCopy;
    pPart->sText = sJsonCopy;
    pPart->eKind = XLLM_PART_NATIVE;
    return true;
}

void xllmPartUnit(xllm_part *pPart) { xllm__part_unit(pPart); }

void xllm__part_unit(xllm_part *pPart)
{
    if (!pPart)
    {
        return;
    }
    xllm__free(pPart->sText);
    xllm__free(pPart->sNativeType);
    xllm__free(pPart->sMediaType);
    xllm__free(pPart->sSourceUrl);
    xllm__free(pPart->sDetail);
    xllm__free(pPart->pData);
    memset(pPart, 0, sizeof(*pPart));
}

bool xllm__part_clone(xllm_part *pDst, const xllm_part *pSrc)
{
    if (!pDst || !pSrc)
    {
        return false;
    }
    memset(pDst, 0, sizeof(*pDst));
    pDst->eKind = pSrc->eKind;
    if (pSrc->sText && !xllm__replace(&pDst->sText, pSrc->sText))
        goto fail;
    if (pSrc->sNativeType && !xllm__replace(&pDst->sNativeType, pSrc->sNativeType))
        goto fail;
    if (pSrc->sMediaType && !xllm__replace(&pDst->sMediaType, pSrc->sMediaType))
        goto fail;
    if (pSrc->sSourceUrl && !xllm__replace(&pDst->sSourceUrl, pSrc->sSourceUrl))
        goto fail;
    if (pSrc->sDetail && !xllm__replace(&pDst->sDetail, pSrc->sDetail))
        goto fail;
    if (pSrc->pData && pSrc->iDataSize)
    {
        pDst->pData = (uint8_t *)xllm__malloc(pSrc->iDataSize);
        if (!pDst->pData)
        {
            goto fail;
        }
        memcpy(pDst->pData, pSrc->pData, pSrc->iDataSize);
        pDst->iDataSize = pSrc->iDataSize;
    }
    return true;
fail:
    xllm__part_unit(pDst);
    return false;
}

/* ------------------------------------------------------------------ */
/* Messages                                                            */
/* ------------------------------------------------------------------ */

void xllmMessageInit(xllm_message *pMessage, xllm_role eRole)
{
    if (!pMessage)
    {
        return;
    }
    memset(pMessage, 0, sizeof(*pMessage));
    pMessage->eRole = eRole;
}

void xllmMessageUnit(xllm_message *pMessage)
{
    size_t i;
    if (!pMessage)
    {
        return;
    }
    xllm__free(pMessage->sContent);
    xllm__free(pMessage->sReasoningContent);
    xllm__free(pMessage->sToolCallId);
    xllm__free(pMessage->sNative);
    for (i = 0u; i < pMessage->iToolCallCount; ++i)
    {
        xllm__tool_call_unit(&pMessage->pToolCalls[i]);
    }
    xllm__free(pMessage->pToolCalls);
    for (i = 0u; i < pMessage->iPartCount; ++i)
    {
        xllm__part_unit(&pMessage->pParts[i]);
    }
    xllm__free(pMessage->pParts);
    memset(pMessage, 0, sizeof(*pMessage));
}

bool xllmMessageSetContent(xllm_message *pMessage, const char *sContent)
{
    size_t i;
    if (!pMessage || !xllm__utf8_valid(sContent))
    {
        return false;
    }
    if (!xllm__replace(&pMessage->sContent, sContent ? sContent : ""))
    {
        return false;
    }
    if (pMessage->pParts)
    {
        for (i = 0u; i < pMessage->iPartCount; ++i)
        {
            xllm__part_unit(&pMessage->pParts[i]);
        }
        pMessage->iPartCount = 0u;
    }
    return true;
}

bool xllmMessageSetReasoning(xllm_message *pMessage, const char *sReasoningContent)
{
    if (!xllm__utf8_valid(sReasoningContent))
    {
        return false;
    }
    return pMessage && xllm__replace(&pMessage->sReasoningContent, sReasoningContent);
}

bool xllmMessageSetToolCallId(xllm_message *pMessage, const char *sToolCallId)
{
    return pMessage && xllm__replace(&pMessage->sToolCallId, sToolCallId);
}

bool xllmMessageAddToolCall(xllm_message *pMessage, const char *sId, const char *sName,
                            const char *sArgumentsJson)
{
    xllm_tool_call *pNew;
    size_t iCap;
    xllm_tool_call *pCall;
    if (!pMessage || !sName || !sName[0])
    {
        return false;
    }
    if (pMessage->iToolCallCount == pMessage->iToolCallCap)
    {
        iCap = pMessage->iToolCallCap ? pMessage->iToolCallCap * 2u : 4u;
        pNew = (xllm_tool_call *)xllm__realloc(pMessage->pToolCalls, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pMessage->iToolCallCap, 0, sizeof(*pNew) * (iCap - pMessage->iToolCallCap));
        pMessage->pToolCalls = pNew;
        pMessage->iToolCallCap = iCap;
    }
    pCall = &pMessage->pToolCalls[pMessage->iToolCallCount];
    pCall->sId = xllm__strdup(sId ? sId : "");
    pCall->sName = xllm__strdup(sName);
    pCall->sArgumentsJson = xllm__strdup(sArgumentsJson ? sArgumentsJson : "{}");
    if (!pCall->sId || !pCall->sName || !pCall->sArgumentsJson)
    {
        xllm__tool_call_unit(pCall);
        return false;
    }
    ++pMessage->iToolCallCount;
    return true;
}

bool xllmMessageAddPart(xllm_message *pMessage, const xllm_part *pPart)
{
    xllm_part *pNew;
    size_t iCap;
    if (!pMessage || !pPart)
    {
        return false;
    }
    if (pMessage->iPartCount == pMessage->iPartCap)
    {
        iCap = pMessage->iPartCap ? pMessage->iPartCap * 2u : 4u;
        pNew = (xllm_part *)xllm__realloc(pMessage->pParts, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pMessage->iPartCap, 0, sizeof(*pNew) * (iCap - pMessage->iPartCap));
        pMessage->pParts = pNew;
        pMessage->iPartCap = iCap;
    }
    if (!xllm__part_clone(&pMessage->pParts[pMessage->iPartCount], pPart))
    {
        return false;
    }
    ++pMessage->iPartCount;
    /* Parts become authoritative: drop the text fast path. */
    if (pMessage->sContent)
    {
        xllm__free(pMessage->sContent);
        pMessage->sContent = NULL;
    }
    return true;
}

bool xllmMessageSetNative(xllm_message *pMessage, const char *sNativeJson)
{
    if (!pMessage || !sNativeJson || !sNativeJson[0] ||
        !xrtJsonValid((xstrview){sNativeJson, strlen(sNativeJson)}))
    {
        return false;
    }
    return xllm__replace(&pMessage->sNative, sNativeJson);
}

bool xllm__message_clone(xllm_message *pDst, const xllm_message *pSrc)
{
    size_t i;
    if (!pDst || !pSrc)
    {
        return false;
    }
    xllmMessageInit(pDst, pSrc->eRole);
    if (pSrc->sContent && !xllmMessageSetContent(pDst, pSrc->sContent))
        goto fail;
    if (pSrc->sReasoningContent && !xllmMessageSetReasoning(pDst, pSrc->sReasoningContent))
        goto fail;
    if (pSrc->sToolCallId && !xllmMessageSetToolCallId(pDst, pSrc->sToolCallId))
        goto fail;
    if (pSrc->sNative && !xllmMessageSetNative(pDst, pSrc->sNative))
        goto fail;
    for (i = 0u; i < pSrc->iToolCallCount; ++i)
    {
        const xllm_tool_call *pCall = &pSrc->pToolCalls[i];
        if (!xllmMessageAddToolCall(pDst, pCall->sId, pCall->sName, pCall->sArgumentsJson))
            goto fail;
    }
    for (i = 0u; i < pSrc->iPartCount; ++i)
    {
        if (!xllmMessageAddPart(pDst, &pSrc->pParts[i]))
            goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pDst);
    return false;
}

bool xllm__part_is_thinking_signature(const xllm_part *pPart)
{
    return pPart->eKind == XLLM_PART_NATIVE && pPart->sNativeType &&
           strcmp(pPart->sNativeType, "thinking_signature") == 0;
}

bool xllmMessageFromResponse(const xllm_response *pResponse, xllm_message *pMessage)
{
    size_t i;
    bool bSigned = false, bText = false;
    if (!pResponse || !pMessage || (pResponse->iBlockCount && !pResponse->pBlocks) ||
        (pResponse->iToolCallCount && !pResponse->pToolCalls))
    {
        return false;
    }
    xllmMessageInit(pMessage, XLLM_ROLE_ASSISTANT);
    if (pResponse->sReasoningContent &&
        !xllmMessageSetReasoning(pMessage, pResponse->sReasoningContent))
        goto fail;
    for (i = 0u; i < pResponse->iBlockCount; ++i)
    {
        const xllm_block *pBlock = &pResponse->pBlocks[i];
        if (pBlock->eKind == XLLM_BLOCK_REASONING && pBlock->sNative && pBlock->sNative[0])
            bSigned = true;
    }
    if (!bSigned)
    {
        /* Keep the compact representation for ordinary provider responses. */
        if (pResponse->sContent && !xllmMessageSetContent(pMessage, pResponse->sContent))
            goto fail;
    }
    else
    {
        /* Parts are authoritative. Keep each original text/thinking block in
         * order, pairing a signature with its immediately preceding reasoning
         * part. Never bind several signatures to the joined reasoning view. */
        for (i = 0u; i < pResponse->iBlockCount; ++i)
        {
            const xllm_block *pBlock = &pResponse->pBlocks[i];
            xllm_part tPart = {0}; /* AddPart owns a deep copy of these views. */
            if (pBlock->eKind != XLLM_BLOCK_TEXT && pBlock->eKind != XLLM_BLOCK_REASONING)
                continue;
            tPart.eKind = pBlock->eKind == XLLM_BLOCK_TEXT ? XLLM_PART_TEXT : XLLM_PART_REASONING;
            if (!xllm__utf8_valid(pBlock->sText)) goto fail;
            tPart.sText = pBlock->sText;
            if (!xllmMessageAddPart(pMessage, &tPart))
                goto fail;
            if (pBlock->eKind == XLLM_BLOCK_TEXT)
                bText = true;
            if (pBlock->eKind == XLLM_BLOCK_REASONING && pBlock->sNative && pBlock->sNative[0])
            {
                tPart.eKind = XLLM_PART_NATIVE;
                if (!xllm__utf8_valid(pBlock->sNative)) goto fail;
                tPart.sText = pBlock->sNative;
                tPart.sNativeType = "thinking_signature"; /* Opaque, not JSON. */
                if (!xllmMessageAddPart(pMessage, &tPart))
                    goto fail;
            }
        }
        /* Custom completed responses may only supply the joined text view. */
        if (!bText && pResponse->sContent)
        {
            xllm_part tPart = {0};
            tPart.eKind = XLLM_PART_TEXT;
            tPart.sText = pResponse->sContent;
            if (!xllmMessageAddPart(pMessage, &tPart))
                goto fail;
        }
    }
    for (i = 0u; i < pResponse->iToolCallCount; ++i)
    {
        const xllm_tool_call *pCall = &pResponse->pToolCalls[i];
        if (!xllmMessageAddToolCall(pMessage, pCall->sId, pCall->sName, pCall->sArgumentsJson))
            goto fail;
    }
    return true;
fail:
    xllmMessageUnit(pMessage);
    return false;
}

/* ------------------------------------------------------------------ */
/* Requests                                                            */
/* ------------------------------------------------------------------ */

void xllm__tool_unit(xllm_tool *pTool)
{
    if (!pTool)
    {
        return;
    }
    xllm__free(pTool->sName);
    xllm__free(pTool->sDescription);
    xllm__free(pTool->sParametersJson);
    memset(pTool, 0, sizeof(*pTool));
}

void xllmRequestInit(xllm_request *pRequest)
{
    if (!pRequest)
    {
        return;
    }
    memset(pRequest, 0, sizeof(*pRequest));
    pRequest->bParallelToolCalls = true;
    pRequest->eToolChoice = XLLM_TOOL_CHOICE_AUTO;
    pRequest->eJsonMode = XLLM_JSON_NONE;
    pRequest->bStream = true;
    pRequest->uDeadline = UINT64_MAX;
    pRequest->iTimeout = XRT_WAIT_FOREVER;
}

void xllmRequestUnit(xllm_request *pRequest)
{
    size_t i;
    if (!pRequest)
    {
        return;
    }
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        if (pRequest->pbMessageBorrowed && pRequest->pbMessageBorrowed[i])
            continue;
        xllmMessageUnit(&pRequest->pMessages[i]);
    }
    for (i = 0u; i < pRequest->iToolCount; ++i)
    {
        if (pRequest->pbToolBorrowed && pRequest->pbToolBorrowed[i])
            continue;
        xllm__tool_unit(&pRequest->pTools[i]);
    }
    xllm__free(pRequest->pMessages);
    xllm__free(pRequest->pTools);
    xllm__free(pRequest->pbMessageBorrowed);
    xllm__free(pRequest->pbToolBorrowed);
    xllm__free(pRequest->sModel);
    xllm__free(pRequest->sReasoningEffort);
    xllm__free(pRequest->sNamedTool);
    xllm__free(pRequest->sStop);
    xllm__free(pRequest->sExtraBodyJson);
    memset(pRequest, 0, sizeof(*pRequest));
}

bool xllmRequestSetModel(xllm_request *pRequest, const char *sModel)
{
    return pRequest && xllm__replace(&pRequest->sModel, sModel);
}

bool xllmRequestSetReasoningEffort(xllm_request *pRequest, const char *sEffort)
{
    return pRequest && xllm__replace(&pRequest->sReasoningEffort, sEffort);
}

bool xllmRequestSetStop(xllm_request *pRequest, const char *sStop)
{
    return pRequest && xllm__replace(&pRequest->sStop, sStop ? sStop : "");
}

bool xllmRequestSetExtraBody(xllm_request *pRequest, const char *sJsonObject)
{
    if (!pRequest)
    {
        return false;
    }
    if (!sJsonObject || !sJsonObject[0])
    {
        return xllm__replace(&pRequest->sExtraBodyJson, NULL);
    }
    if (!xrtJsonValid((xstrview){sJsonObject, strlen(sJsonObject)}))
    {
        return false;
    }
    return xllm__replace(&pRequest->sExtraBodyJson, sJsonObject);
}

void xllmRequestSetCancel(xllm_request *pRequest, xcancel *pCancel)
{
    if (pRequest)
    {
        pRequest->pCancel = pCancel;
    }
}

void xllmRequestSetDeadline(xllm_request *pRequest, uint64_t uDeadline)
{
    if (pRequest)
    {
        pRequest->uDeadline = uDeadline;
        pRequest->iTimeout = XRT_WAIT_FOREVER;
    }
}

bool xllmRequestSetToolChoice(xllm_request *pRequest, xllm_tool_choice eChoice,
                              const char *sNamedTool)
{
    if (!pRequest || eChoice < XLLM_TOOL_CHOICE_AUTO || eChoice > XLLM_TOOL_CHOICE_NAMED)
    {
        return false;
    }
    if (eChoice == XLLM_TOOL_CHOICE_NAMED && (!sNamedTool || !sNamedTool[0]))
    {
        return false;
    }
    if (!xllm__replace(&pRequest->sNamedTool,
                       eChoice == XLLM_TOOL_CHOICE_NAMED ? sNamedTool : NULL))
    {
        return false;
    }
    pRequest->eToolChoice = eChoice;
    return true;
}

bool xllmRequestAddMessage(xllm_request *pRequest, const xllm_message *pMessage)
{
    xllm_message *pNew;
    bool *pbNew;
    size_t iCap;
    if (!pRequest || !pMessage)
    {
        return false;
    }
    if (pRequest->iMessageCount == pRequest->iMessageCap)
    {
        iCap = pRequest->iMessageCap ? pRequest->iMessageCap * 2u : 8u;
        pNew = (xllm_message *)xllm__realloc(pRequest->pMessages, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pRequest->iMessageCap, 0, sizeof(*pNew) * (iCap - pRequest->iMessageCap));
        pRequest->pMessages = pNew;
        if (pRequest->pbMessageBorrowed)
        {
            pbNew = (bool *)xllm__realloc(pRequest->pbMessageBorrowed, sizeof(bool) * iCap);
            if (!pbNew)
            {
                return false;
            }
            pRequest->pbMessageBorrowed = pbNew;
        }
        pRequest->iMessageCap = iCap;
    }
    if (pRequest->pbMessageBorrowed)
    {
        pRequest->pbMessageBorrowed[pRequest->iMessageCount] = false;
    }
    if (!xllm__message_clone(&pRequest->pMessages[pRequest->iMessageCount], pMessage))
    {
        return false;
    }
    ++pRequest->iMessageCount;
    return true;
}

bool xllmRequestAddMessageView(xllm_request *pRequest, const xllm_message *pMessage)
{
    xllm_message *pNew;
    bool *pbNew;
    size_t iCap;
    if (!pRequest || !pMessage)
    {
        return false;
    }
    if (pRequest->iMessageCount == pRequest->iMessageCap)
    {
        iCap = pRequest->iMessageCap ? pRequest->iMessageCap * 2u : 8u;
        pNew = (xllm_message *)xllm__realloc(pRequest->pMessages, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pRequest->iMessageCap, 0, sizeof(*pNew) * (iCap - pRequest->iMessageCap));
        pRequest->pMessages = pNew;
        if (pRequest->pbMessageBorrowed)
        {
            /* The bitmap must track the array capacity exactly. */
            pbNew = (bool *)xllm__realloc(pRequest->pbMessageBorrowed, sizeof(bool) * iCap);
            if (!pbNew)
            {
                return false;
            }
            pRequest->pbMessageBorrowed = pbNew;
        }
        pRequest->iMessageCap = iCap;
    }
    if (!pRequest->pbMessageBorrowed)
    {
        /* Lazy parallel bitmap; existing entries are all owned. */
        pbNew = (bool *)xllm__calloc(1u, sizeof(bool) * pRequest->iMessageCap);
        if (!pbNew)
        {
            return false;
        }
        memset(pbNew, 0, sizeof(bool) * pRequest->iMessageCap);
        pRequest->pbMessageBorrowed = pbNew;
    }
    pRequest->pMessages[pRequest->iMessageCount] = *pMessage; /* shallow */
    pRequest->pbMessageBorrowed[pRequest->iMessageCount] = true;
    ++pRequest->iMessageCount;
    return true;
}

bool xllmRequestSetToolsView(xllm_request *pRequest, const xllm_tool *pTools, size_t iCount)
{
    xllm_tool *pNew = NULL;
    bool *pbNew = NULL;
    size_t i;
    if (!pRequest || (!pTools && iCount))
    {
        return false;
    }
    if (iCount > SIZE_MAX / sizeof(*pNew) || iCount > SIZE_MAX / sizeof(*pbNew))
        return false;
    if (iCount)
    {
        pNew = (xllm_tool *)xllm__malloc(sizeof(*pNew) * iCount);
        if (!pNew)
            return false;
        pbNew = (bool *)xllm__malloc(sizeof(*pbNew) * iCount);
        if (!pbNew)
        {
            xllm__free(pNew);
            return false;
        }
        memcpy(pNew, pTools, sizeof(*pTools) * iCount); /* shallow */
        memset(pbNew, 1, sizeof(*pbNew) * iCount);
    }
    /* Commit only after the complete replacement is ready. */
    for (i = 0u; i < pRequest->iToolCount; ++i)
    {
        if (pRequest->pbToolBorrowed && pRequest->pbToolBorrowed[i])
            continue;
        xllm__tool_unit(&pRequest->pTools[i]);
    }
    xllm__free(pRequest->pTools);
    xllm__free(pRequest->pbToolBorrowed);
    pRequest->pTools = pNew;
    pRequest->pbToolBorrowed = pbNew;
    pRequest->iToolCount = iCount;
    pRequest->iToolCap = iCount;
    return true;
}

bool xllmRequestAddTextMessage(xllm_request *pRequest, xllm_role eRole, const char *sContent)
{
    xllm_message tMessage;
    bool bOk;
    if (!pRequest || eRole == XLLM_ROLE_TOOL)
    {
        return false;
    }
    xllmMessageInit(&tMessage, eRole);
    bOk = xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
          xllmRequestAddMessage(pRequest, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmRequestAddToolResult(xllm_request *pRequest, const char *sToolCallId, const char *sContent)
{
    xllm_message tMessage;
    bool bOk;
    if (!pRequest || !sToolCallId || !sToolCallId[0])
    {
        return false;
    }
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    bOk = xllmMessageSetToolCallId(&tMessage, sToolCallId) &&
          xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
          xllmRequestAddMessage(pRequest, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

bool xllmRequestAddTool(xllm_request *pRequest, const char *sName, const char *sDescription,
                        const char *sParametersJson, bool bStrict)
{
    xllm_tool *pNew;
    xllm_tool *pTool;
    size_t iCap;
    const char *sSchema =
        sParametersJson ? sParametersJson : "{\"type\":\"object\",\"properties\":{}}";
    if (!pRequest || !sName || !sName[0])
    {
        return false;
    }
    if (pRequest->iToolCount == pRequest->iToolCap)
    {
        iCap = pRequest->iToolCap ? pRequest->iToolCap * 2u : 8u;
        pNew = (xllm_tool *)xllm__realloc(pRequest->pTools, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            return false;
        }
        memset(pNew + pRequest->iToolCap, 0, sizeof(*pNew) * (iCap - pRequest->iToolCap));
        pRequest->pTools = pNew;
        if (pRequest->pbToolBorrowed)
        {
            bool *pbNew = (bool *)xllm__realloc(pRequest->pbToolBorrowed, sizeof(bool) * iCap);
            if (!pbNew)
            {
                return false;
            }
            pRequest->pbToolBorrowed = pbNew;
        }
        pRequest->iToolCap = iCap;
    }
    if (pRequest->pbToolBorrowed)
    {
        pRequest->pbToolBorrowed[pRequest->iToolCount] = false;
    }
    pTool = &pRequest->pTools[pRequest->iToolCount];
    pTool->sName = xllm__strdup(sName);
    pTool->sDescription = xllm__strdup(sDescription ? sDescription : "");
    pTool->sParametersJson = xllm__strdup(sSchema);
    pTool->bStrict = bStrict;
    if (!pTool->sName || !pTool->sDescription || !pTool->sParametersJson)
    {
        xllm__tool_unit(pTool);
        return false;
    }
    ++pRequest->iToolCount;
    return true;
}

uint64_t xllmEstimateTextTokens(const char *sText)
{
    const unsigned char *p = (const unsigned char *)sText;
    uint64_t uAscii = 0u;
    uint64_t uNonAscii = 0u;
    if (!p)
    {
        return 0u;
    }
    while (*p)
    {
        if (*p < 0x80u)
        {
            ++uAscii;
            ++p;
        }
        else
        {
            ++uNonAscii;
            if ((*p & 0xE0u) == 0xC0u && p[1])
            {
                p += 2;
            }
            else if ((*p & 0xF0u) == 0xE0u && p[1] && p[2])
            {
                p += 3;
            }
            else if ((*p & 0xF8u) == 0xF0u && p[1] && p[2] && p[3])
            {
                p += 4;
            }
            else
            {
                ++p;
            }
        }
    }
    return (uAscii + 3u) / 4u + uNonAscii;
}

uint64_t xllmEstimateMessageTokens(const xllm_message *pMessage)
{
    uint64_t uTokens = 12u;
    size_t i;
    if (!pMessage)
    {
        return 0u;
    }
    uTokens += xllmEstimateTextTokens(pMessage->sContent);
    uTokens += xllmEstimateTextTokens(pMessage->sReasoningContent);
    uTokens += xllmEstimateTextTokens(pMessage->sToolCallId);
    for (i = 0u; i < pMessage->iPartCount; ++i)
    {
        const xllm_part *pPart = &pMessage->pParts[i];
        if (pPart->eKind == XLLM_PART_IMAGE)
            uTokens += 4096u;
        else
            uTokens += xllmEstimateTextTokens(pPart->sText);
    }
    /* Native replay is opaque; retain a bounded text estimate rather than
     * silently treating a structured message as zero input. */
    uTokens += xllmEstimateTextTokens(pMessage->sNative);
    for (i = 0u; i < pMessage->iToolCallCount; ++i)
    {
        const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
        uTokens += 16u + xllmEstimateTextTokens(pCall->sId) + xllmEstimateTextTokens(pCall->sName) +
                   xllmEstimateTextTokens(pCall->sArgumentsJson);
    }
    return uTokens;
}

/* ------------------------------------------------------------------ */
/* Response destruction                                                */
/* ------------------------------------------------------------------ */

void xllmResponseDestroy(xllm_response *pResponse)
{
    size_t i;
    if (!pResponse)
    {
        return;
    }
    xllm__free(pResponse->sId);
    xllm__free(pResponse->sModel);
    xllm__free(pResponse->sContent);
    xllm__free(pResponse->sReasoningContent);
    xllm__free(pResponse->sRefusal);
    xllm__free(pResponse->sFinishReason);
    xllm__free(pResponse->sRequestId);
    for (i = 0u; i < pResponse->iBlockCount; ++i)
    {
        xllm__free(pResponse->pBlocks[i].sText);
        xllm__free(pResponse->pBlocks[i].sNative);
    }
    xllm__free(pResponse->pBlocks);
    for (i = 0u; i < pResponse->iToolCallCount; ++i)
    {
        xllm__tool_call_unit(&pResponse->pToolCalls[i]);
    }
    xllm__free(pResponse->pToolCalls);
    xllm__free(pResponse);
}

/* Relative budgets begin at submission, not when a reusable request is set up. */
void xllmRequestSetTimeout(xllm_request *request, int64_t timeout)
{
    if (request)
    {
        request->iTimeout = timeout;
        request->uDeadline = XLLM_DEADLINE_NEVER;
    }
}
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_history.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* ------------------------------------------------------------------ */
/* History: deep-copying message ledger for hand-written agents        */
/*                                                                     */
/* Zero policy on purpose: no token counting, no compaction, no        */
/* journaling -- those live in xllm-session. This container only       */
/* removes the mechanical pain of owning a growing message array.      */
/* ------------------------------------------------------------------ */

struct xllm_history
{
    xllm_message *pMessages;
    size_t iCount;
    size_t iCapacity;
};

XRT_API xllm_history *xllmHistoryCreate(void)
{
    return (xllm_history *)xllm__calloc(1u, sizeof(xllm_history));
}

XRT_API void xllmHistoryDestroy(xllm_history *pHistory)
{
    size_t i;
    if (!pHistory)
    {
        return;
    }
    for (i = 0u; i < pHistory->iCount; ++i)
    {
        xllmMessageUnit(&pHistory->pMessages[i]);
    }
    xllm__free(pHistory->pMessages);
    xllm__free(pHistory);
}

XRT_API size_t xllmHistoryCount(const xllm_history *pHistory)
{
    return pHistory ? pHistory->iCount : 0u;
}

XRT_API const xllm_message *xllmHistoryAt(const xllm_history *pHistory, size_t iIndex)
{
    if (!pHistory || iIndex >= pHistory->iCount)
    {
        return NULL;
    }
    return &pHistory->pMessages[iIndex];
}

static bool xllm__history_grow(xllm_history *pHistory, size_t iNeed)
{
    xllm_message *pNew;
    size_t iCapacity;
    if (iNeed <= pHistory->iCapacity)
    {
        return true;
    }
    iCapacity = pHistory->iCapacity ? pHistory->iCapacity : 16u;
    while (iCapacity < iNeed)
    {
        if (iCapacity > SIZE_MAX / 2u)
        {
            iCapacity = iNeed;
            break;
        }
        iCapacity *= 2u;
    }
    pNew = (xllm_message *)xllm__realloc(pHistory->pMessages, sizeof(*pNew) * iCapacity);
    if (!pNew)
    {
        return false;
    }
    memset(pNew + pHistory->iCapacity, 0, sizeof(*pNew) * (iCapacity - pHistory->iCapacity));
    pHistory->pMessages = pNew;
    pHistory->iCapacity = iCapacity;
    return true;
}

XRT_API bool xllmHistoryAdd(xllm_history *pHistory, const xllm_message *pMessage)
{
    xllm_message *pSlot;
    if (!pHistory || !pMessage)
    {
        return false;
    }
    if (!xllm__history_grow(pHistory, pHistory->iCount + 1u))
    {
        return false;
    }
    pSlot = &pHistory->pMessages[pHistory->iCount];
    if (!xllm__message_clone(pSlot, pMessage))
    {
        return false;
    }
    ++pHistory->iCount;
    return true;
}

XRT_API bool xllmHistoryAddText(xllm_history *pHistory, xllm_role eRole, const char *sContent)
{
    xllm_message tMessage;
    bool bOk;
    xllmMessageInit(&tMessage, eRole);
    bOk = xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
          xllmHistoryAdd(pHistory, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

XRT_API bool xllmHistoryAddFromResponse(xllm_history *pHistory, const xllm_response *pResponse)
{
    xllm_message tMessage;
    bool bOk;
    if (!pHistory || !pResponse)
    {
        return false;
    }
    if (!xllmMessageFromResponse(pResponse, &tMessage))
    {
        return false;
    }
    bOk = xllmHistoryAdd(pHistory, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

XRT_API bool xllmHistoryAddToolResult(xllm_history *pHistory, const char *sCallId,
                                      const char *sContent)
{
    xllm_message tMessage;
    bool bOk;
    xllmMessageInit(&tMessage, XLLM_ROLE_TOOL);
    bOk = xllmMessageSetToolCallId(&tMessage, sCallId ? sCallId : "") &&
          xllmMessageSetContent(&tMessage, sContent ? sContent : "") &&
          xllmHistoryAdd(pHistory, &tMessage);
    xllmMessageUnit(&tMessage);
    return bOk;
}

XRT_API bool xllmHistoryRemove(xllm_history *pHistory, size_t iIndex, size_t iCount)
{
    size_t i;
    if (!pHistory || iCount == 0u || iIndex >= pHistory->iCount)
    {
        return false;
    }
    if (iCount > pHistory->iCount - iIndex)
    {
        iCount = pHistory->iCount - iIndex;
    }
    for (i = 0u; i < iCount; ++i)
    {
        xllmMessageUnit(&pHistory->pMessages[iIndex + i]);
    }
    memmove(&pHistory->pMessages[iIndex], &pHistory->pMessages[iIndex + iCount],
            sizeof(xllm_message) * (pHistory->iCount - iIndex - iCount));
    pHistory->iCount -= iCount;
    return true;
}

XRT_API bool xllmHistoryAppendInto(const xllm_history *pHistory, xllm_request *pRequest)
{
    size_t i;
    size_t iOriginalCount;
    if (!pHistory || !pRequest)
    {
        return false;
    }
    iOriginalCount = pRequest->iMessageCount;
    for (i = 0u; i < pHistory->iCount; ++i)
    {
        if (!xllmRequestAddMessage(pRequest, &pHistory->pMessages[i]))
        {
            while (pRequest->iMessageCount > iOriginalCount)
            {
                --pRequest->iMessageCount;
                xllmMessageUnit(&pRequest->pMessages[pRequest->iMessageCount]);
                if (pRequest->pbMessageBorrowed)
                {
                    pRequest->pbMessageBorrowed[pRequest->iMessageCount] = false;
                }
            }
            return false;
        }
    }
    return true;
}
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_sse.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* Independent SSE framing limits: the HTTP body cap does not cover the
 * framing buffers, so a peer that never terminates a line or an event block
 * must not be able to grow them without bound. */
#define XLLM_SSE_LINE_LIMIT (1024u * 1024u)
#define XLLM_SSE_EVENT_LIMIT (16u * 1024u * 1024u)

static size_t xllm__sse_line_limit(const xllm_call *pCall)
{
    return pCall && pCall->uMaxSseLineBytes ? pCall->uMaxSseLineBytes : XLLM_SSE_LINE_LIMIT;
}

static size_t xllm__sse_event_limit(const xllm_call *pCall)
{
    return pCall && pCall->uMaxSseEventBytes ? pCall->uMaxSseEventBytes : XLLM_SSE_EVENT_LIMIT;
}

/* SSE framing: split the byte stream into lines, collect field values into
 * per-event buffers, and dispatch the assembled field set to the active
 * dialect when a blank line closes an event. Arbitrary fragmentation is
 * safe; a final unterminated line is flushed by xllm__sse_finish. */

static bool xllm__sse_dispatch(xllm_call *pCall)
{
    bool bOk = true;
    if (pCall->tEventData.iLen)
    {
        xllm_sse_fields tFields;
        tFields.tEvent.Data =
            pCall->bHaveEventName && pCall->tEventName.iLen ? pCall->tEventName.pData : NULL;
        tFields.tEvent.Size = pCall->bHaveEventName ? pCall->tEventName.iLen : 0u;
        tFields.tData.Data = pCall->tEventData.pData;
        tFields.tData.Size = pCall->tEventData.iLen;
        tFields.tId.Data = NULL;
        tFields.tId.Size = 0u;
        bOk = pCall->pDialect->DecodeSseEvent(pCall, &tFields);
    }
    pCall->tEventData.iLen = 0u;
    if (pCall->tEventData.pData)
    {
        pCall->tEventData.pData[0] = '\0';
    }
    pCall->tEventName.iLen = 0u;
    pCall->bHaveEventName = false;
    return bOk;
}

static bool xllm__sse_process_line(xllm_call *pCall, const char *sLine, size_t iLen)
{
    const char *sValue;
    size_t iValueLen;
    bool bData;
    size_t iLimit;
    size_t iSeparator;
    if (iLen && sLine[iLen - 1u] == '\r')
    {
        --iLen;
    }
    if (iLen == 0u)
    {
        return xllm__sse_dispatch(pCall);
    }
    if (sLine[0] == ':')
    {
        return true;
    }
    if (iLen >= 5u && memcmp(sLine, "data:", 5u) == 0)
    {
        sValue = sLine + 5u;
        iValueLen = iLen - 5u;
        bData = true;
    }
    else if (iLen >= 6u && memcmp(sLine, "event:", 6u) == 0)
    {
        sValue = sLine + 6u;
        iValueLen = iLen - 6u;
        bData = false;
    }
    else if (iLen >= 3u && memcmp(sLine, "id:", 3u) == 0)
    {
        /* Collected by the framing contract but unused by shipped dialects. */
        return true;
    }
    else if (iLen >= 6u && memcmp(sLine, "retry:", 6u) == 0)
    {
        return true;
    }
    else
    {
        return true;
    }
    if (iValueLen && *sValue == ' ')
    {
        ++sValue;
        --iValueLen;
    }
    if (bData)
    {
        iLimit = xllm__sse_event_limit(pCall);
        iSeparator = pCall->tEventData.iLen ? 1u : 0u;
        if (iValueLen > iLimit || iSeparator > iLimit - iValueLen ||
            pCall->tEventData.iLen > iLimit - iValueLen - iSeparator)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_LIMIT,
                            "provider event exceeds the configured SSE event limit");
            return false;
        }
        if (pCall->tEventData.iLen && !xllm__buf_append_char(&pCall->tEventData, '\n'))
            goto oom;
        if (!xllm__buf_append(&pCall->tEventData, sValue, iValueLen))
            goto oom;
        return true;
    }
    iLimit = xllm__sse_line_limit(pCall);
    iSeparator = pCall->tEventName.iLen ? 1u : 0u;
    if (iValueLen > iLimit || iSeparator > iLimit - iValueLen ||
        pCall->tEventName.iLen > iLimit - iValueLen - iSeparator)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_LIMIT,
                        "provider event name exceeds the configured SSE line limit");
        return false;
    }
    if (pCall->tEventName.iLen && !xllm__buf_append_char(&pCall->tEventName, '\n'))
        goto oom;
    pCall->bHaveEventName = true;
    if (!xllm__buf_append(&pCall->tEventName, sValue, iValueLen))
        goto oom;
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                    "failed to buffer provider event fields");
    return false;
}

bool xllm__sse_feed(xllm_call *pCall, const void *pData, size_t iLen)
{
    const unsigned char *p = (const unsigned char *)pData;
    size_t iStart = 0u;
    size_t iLimit;
    size_t i;
    if (!pCall || !pCall->pDialect || (!pData && iLen))
    {
        return false;
    }
    iLimit = xllm__sse_line_limit(pCall);
    for (i = 0u; i < iLen; ++i)
    {
        if (p[i] != '\n')
        {
            continue;
        }
        if (i > iStart && !xllm__buf_append(&pCall->tLine, p + iStart, i - iStart))
            goto oom;
        if (!xllm__sse_process_line(pCall, pCall->tLine.pData ? pCall->tLine.pData : "",
                                    pCall->tLine.iLen))
            return false;
        pCall->tLine.iLen = 0u;
        if (pCall->tLine.pData)
        {
            pCall->tLine.pData[0] = '\0';
        }
        iStart = i + 1u;
    }
    if (iStart < iLen)
    {
        size_t iTail = iLen - iStart;
        if (pCall->tLine.iLen > iLimit || iTail > iLimit - pCall->tLine.iLen)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_LIMIT,
                            "provider event exceeds the configured SSE line limit");
            return false;
        }
        if (!xllm__buf_append(&pCall->tLine, p + iStart, iLen - iStart))
            goto oom;
    }
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                    "failed to buffer provider event stream");
    return false;
}

bool xllm__sse_finish(xllm_call *pCall)
{
    if (!pCall)
    {
        return false;
    }
    if (pCall->tLine.iLen)
    {
        if (!xllm__sse_process_line(pCall, pCall->tLine.pData, pCall->tLine.iLen))
            return false;
        pCall->tLine.iLen = 0u;
    }
    if (pCall->tEventData.iLen && !xllm__sse_dispatch(pCall))
        return false;
    return true;
}

void xllm__sse_reset(xllm_call *pCall)
{
    if (!pCall)
    {
        return;
    }
    pCall->tEventData.iLen = 0u;
    pCall->tEventName.iLen = 0u;
    pCall->tLine.iLen = 0u;
    pCall->bHaveEventName = false;
}
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_assemble.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* Response assembly: dialect decoders push normalized deltas and complete
 * values into the response under construction. Appends are length-tracked
 * (no tail rescans), blocks record arrival order, and finalize joins the
 * convenience text fields in one pass. */

static xllm_finish xllm__finish_from_reason(const char *sRaw, bool bHasToolCalls);

xvalue *xllm__json_get(xvalue *pObject, const char *sKey)
{
    xvalue *pValue = (pObject && xrtValueType(pObject) == XVALUE_OBJECT)
                         ? xrtValueObjectGet(pObject, (xstrview){sKey, strlen(sKey)})
                         : NULL;
    return (pValue && xrtValueType(pValue) != XVALUE_NULL) ? pValue : NULL;
}

xstrview xllm__json_text(xvalue *pObject, const char *sKey)
{
    xvalue *pValue = xllm__json_get(pObject, sKey);
    xstrview tText = {0};
    if (pValue)
        (void)xrtValueGetString(pValue, &tText);
    return tText;
}

uint64_t xllm__json_u64(xvalue *pObject, const char *sKey)
{
    xvalue *pValue = xllm__json_get(pObject, sKey);
    int64 iValue = 0;
    if (!pValue || !xrtValueGetInt(pValue, &iValue) || iValue < 0)
        return 0u;
    return (uint64_t)iValue;
}

bool xllm__emit(xllm_call *pCall, const xllm_event *pEvent)
{
    if (!pCall || !pEvent || !pCall->tCallbacks.OnEvent)
    {
        return true;
    }
    if (pCall->tCallbacks.OnEvent(pCall->tCallbacks.pUserData, pEvent))
    {
        return true;
    }
    pCall->bCallbackCancelled = true;
    xllm__error_set(&pCall->tError, XLLM_ERROR_CANCELLED,
                    "stream callback cancelled the model call");
    return false;
}

/* Append provider-native metadata (e.g. an Anthropic thinking signature)
 * to the most recent block of the given kind. */
bool xllm__assemble_native(xllm_call *pCall, xllm_block_kind eKind, xstrview tNative)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    size_t i;
    size_t iLen;
    char *pNew;
    if (!pResponse || !tNative.Data || !tNative.Size)
    {
        return true;
    }
    for (i = pResponse->iBlockCount; i > 0u; --i)
    {
        if (pResponse->pBlocks[i - 1u].eKind == eKind)
        {
            break;
        }
    }
    if (!i)
    {
        return true;
    }
    --i;
    iLen = pResponse->pBlocks[i].sNative ? strlen(pResponse->pBlocks[i].sNative) : 0u;
    if (iLen == SIZE_MAX || tNative.Size > SIZE_MAX - iLen - 1u)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                        "failed to append block metadata");
        return false;
    }
    pNew = (char *)xllm__realloc(pResponse->pBlocks[i].sNative, iLen + tNative.Size + 1u);
    if (!pNew)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                        "failed to append block metadata");
        return false;
    }
    memcpy(pNew + iLen, tNative.Data, tNative.Size);
    pNew[iLen + tNative.Size] = 0;
    pResponse->pBlocks[i].sNative = pNew;
    return true;
}

void xllm__assemble_first_token(xllm_call *pCall)
{
    if (pCall && !pCall->tHttpDiagnostics.uFirstTokenMs)
    {
        pCall->tHttpDiagnostics.uFirstTokenMs = xllmClockUs() / UINT64_C(1000);
    }
}

/* Record a TOOL_CALL block in arrival order; finalize leaves these alone. */
bool xllm__assemble_block_mark_tool(xllm_call *pCall, size_t iToolIndex)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    xllm_block *pNew;
    xllm_block_state *pStateNew;
    xllm_event tEvent;
    if (!pResponse)
    {
        return false;
    }
    if (pResponse->iBlockCount == SIZE_MAX ||
        pResponse->iBlockCount + 1u > SIZE_MAX / sizeof(*pNew) ||
        pResponse->iBlockCount + 1u > SIZE_MAX / sizeof(*pStateNew))
    {
        goto oom;
    }
    pNew = (xllm_block *)xllm__realloc(pResponse->pBlocks,
                                       sizeof(*pNew) * (pResponse->iBlockCount + 1u));
    if (!pNew)
    {
        goto oom;
    }
    pResponse->pBlocks = pNew;
    pStateNew = (xllm_block_state *)xllm__realloc(
        pCall->pBlockState, sizeof(*pStateNew) * (pResponse->iBlockCount + 1u));
    if (!pStateNew)
    {
        goto oom;
    }
    pCall->pBlockState = pStateNew;
    memset(&pResponse->pBlocks[pResponse->iBlockCount], 0, sizeof(xllm_block));
    pCall->pBlockState[pResponse->iBlockCount].iTextLen = 0u;
    pResponse->pBlocks[pResponse->iBlockCount].eKind = XLLM_BLOCK_TOOL_CALL;
    pResponse->pBlocks[pResponse->iBlockCount].iToolIndex = iToolIndex;
    ++pResponse->iBlockCount;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_BLOCK_META;
    tEvent.as.tBlockMeta.iBlock = pResponse->iBlockCount - 1u;
    tEvent.as.tBlockMeta.eKind = XLLM_BLOCK_TOOL_CALL;
    tEvent.as.tBlockMeta.bEnd = false;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append response block");
    return false;
}

/* Wire block index -> tool index mapping (value is tool index + 1). */
static bool xllm__block_map_reserve(xllm_call *pCall, size_t iWireIndex)
{
    size_t *pNew;
    size_t iCap;
    if (iWireIndex >= pCall->iBlockMapCap)
    {
        iCap = pCall->iBlockMapCap ? pCall->iBlockMapCap : 8u;
        while (iCap <= iWireIndex)
        {
            if (iCap > SIZE_MAX / 2u)
            {
                goto oom;
            }
            iCap *= 2u;
        }
        if (iCap > SIZE_MAX / sizeof(*pNew))
        {
            goto oom;
        }
        pNew = (size_t *)xllm__realloc(pCall->pBlockMap, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            goto oom;
        }
        memset(pNew + pCall->iBlockMapCap, 0, sizeof(*pNew) * (iCap - pCall->iBlockMapCap));
        pCall->pBlockMap = pNew;
        pCall->iBlockMapCap = iCap;
    }
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to grow provider block map");
    return false;
}

size_t xllm__assemble_map_block_tool(xllm_call *pCall, size_t iWireIndex)
{
    if (!pCall || iWireIndex >= pCall->iBlockMapCap)
    {
        return 0u;
    }
    return pCall->pBlockMap[iWireIndex];
}

bool xllm__assemble_set_block_tool(xllm_call *pCall, size_t iWireIndex, size_t iToolIndex)
{
    if (!pCall)
    {
        return false;
    }
    if (iToolIndex == SIZE_MAX)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                        "provider tool index exceeds addressable capacity");
        return false;
    }
    if (!xllm__block_map_reserve(pCall, iWireIndex))
    {
        return false;
    }
    pCall->pBlockMap[iWireIndex] = iToolIndex + 1u;
    return true;
}

size_t xllm__assemble_find_item_tool(xllm_call *pCall, const char *sItemId)
{
    size_t i;
    if (!pCall || !sItemId)
    {
        return (size_t)-1;
    }
    for (i = 0u; i < pCall->iItemCount; ++i)
    {
        if (strcmp(pCall->pItemMap[i].sId, sItemId) == 0)
        {
            return pCall->pItemMap[i].iTool;
        }
    }
    return (size_t)-1;
}

size_t xllm__assemble_add_item_tool(xllm_call *pCall, const char *sItemId, xstrview tId,
                                    xstrview tName)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    struct xllm_item_map *pNew;
    size_t iCap;
    size_t iTool;
    if (!pResponse)
    {
        return (size_t)-1;
    }
    if (!sItemId || strlen(sItemId) >= sizeof(pCall->pItemMap[0].sId))
    {
        return (size_t)-1;
    }
    if (pCall->iItemCount == pCall->iItemCap)
    {
        if (pCall->iItemCap > SIZE_MAX / 2u)
        {
            goto oom;
        }
        iCap = pCall->iItemCap ? pCall->iItemCap * 2u : 8u;
        if (iCap > SIZE_MAX / sizeof(*pNew))
        {
            goto oom;
        }
        pNew = (struct xllm_item_map *)xllm__realloc(pCall->pItemMap, sizeof(*pNew) * iCap);
        if (!pNew)
        {
            goto oom;
        }
        pCall->pItemMap = pNew;
        pCall->iItemCap = iCap;
    }
    iTool = pResponse->iToolCallCount;
    if (!xllm__assemble_tool(pCall, iTool, tId, tName, (xstrview){0}))
    {
        return (size_t)-1;
    }
    strcpy(pCall->pItemMap[pCall->iItemCount].sId, sItemId);
    pCall->pItemMap[pCall->iItemCount].iTool = iTool;
    ++pCall->iItemCount;
    return iTool;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to grow provider item map");
    return (size_t)-1;
}

xllm_response *xllm__assemble_ensure(xllm_call *pCall)
{
    if (!pCall)
    {
        return NULL;
    }
    if (!pCall->pResponse)
    {
        pCall->pResponse = (xllm_response *)xllm__calloc(1u, sizeof(*pCall->pResponse));
        if (!pCall->pResponse)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to allocate model response");
            return NULL;
        }
        pCall->pResponse->uHttpStatus = pCall->uHttpStatus;
        if (pCall->sRequestId[0])
        {
            pCall->pResponse->sRequestId = xllm__strdup(pCall->sRequestId);
            if (!pCall->pResponse->sRequestId)
            {
                xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                                "failed to allocate request id");
                return NULL;
            }
        }
    }
    return pCall->pResponse;
}

bool xllm__assemble_metadata(xllm_call *pCall, xstrview tId, xstrview tModel)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    char *sId = NULL;
    char *sModel = NULL;
    if (!pResponse)
    {
        return false;
    }
    if (tId.Data && !pResponse->sId)
    {
        if (tId.Size == SIZE_MAX)
        {
            goto oom;
        }
        sId = (char *)xllm__malloc(tId.Size + 1u);
        if (!sId)
        {
            goto oom;
        }
        memcpy(sId, tId.Data, tId.Size);
        sId[tId.Size] = 0;
    }
    if (tModel.Data && !pResponse->sModel)
    {
        if (tModel.Size == SIZE_MAX)
        {
            goto oom;
        }
        sModel = (char *)xllm__malloc(tModel.Size + 1u);
        if (!sModel)
        {
            goto oom;
        }
        memcpy(sModel, tModel.Data, tModel.Size);
        sModel[tModel.Size] = 0;
    }
    if (sId)
    {
        pResponse->sId = sId;
    }
    if (sModel)
    {
        pResponse->sModel = sModel;
    }
    return true;
oom:
    xllm__free(sId);
    xllm__free(sModel);
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                    "failed to copy provider response metadata");
    return false;
}

static bool xllm__assemble_block_open(xllm_call *pCall, xllm_block_kind eKind, size_t *piBlock)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    xllm_block *pNew;
    xllm_block_state *pStateNew;
    xllm_event tEvent;
    if (!pResponse)
    {
        return false;
    }
    if (pResponse->iBlockCount == SIZE_MAX ||
        pResponse->iBlockCount + 1u > SIZE_MAX / sizeof(*pNew) ||
        pResponse->iBlockCount + 1u > SIZE_MAX / sizeof(*pStateNew))
    {
        goto oom;
    }
    pNew = (xllm_block *)xllm__realloc(pResponse->pBlocks,
                                       sizeof(*pNew) * (pResponse->iBlockCount + 1u));
    if (!pNew)
        goto oom;
    pResponse->pBlocks = pNew;
    pStateNew = (xllm_block_state *)xllm__realloc(
        pCall->pBlockState, sizeof(*pStateNew) * (pResponse->iBlockCount + 1u));
    if (!pStateNew)
        goto oom;
    pCall->pBlockState = pStateNew;
    memset(&pResponse->pBlocks[pResponse->iBlockCount], 0, sizeof(xllm_block));
    pCall->pBlockState[pResponse->iBlockCount].iTextLen = 0u;
    *piBlock = pResponse->iBlockCount;
    pResponse->pBlocks[*piBlock].eKind = eKind;
    ++pResponse->iBlockCount;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_BLOCK_META;
    tEvent.as.tBlockMeta.iBlock = *piBlock;
    tEvent.as.tBlockMeta.eKind = eKind;
    tEvent.as.tBlockMeta.bEnd = false;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append response block");
    return false;
}

bool xllm__assemble_text(xllm_call *pCall, xllm_block_kind eKind, xstrview tText, char *sNative)
{
    xllm_response *pResponse;
    xllm_block *pBlock;
    xllm_block_state *pState;
    xllm_event tEvent;
    size_t iBlock;
    size_t iBefore;
    if (!pCall || (!tText.Data && !sNative))
    {
        return true;
    }
    if (!tText.Data || !tText.Size)
    {
        if (sNative)
        {
            xllm__free(sNative);
        }
        return true;
    }
    pResponse = xllm__assemble_ensure(pCall);
    if (!pResponse)
    {
        xllm__free(sNative);
        return false;
    }
    if (pResponse->iBlockCount && pResponse->pBlocks[pResponse->iBlockCount - 1u].eKind == eKind)
    {
        iBlock = pResponse->iBlockCount - 1u;
    }
    else if (!xllm__assemble_block_open(pCall, eKind, &iBlock))
    {
        xllm__free(sNative);
        return false;
    }
    pBlock = &pResponse->pBlocks[iBlock];
    pState = &pCall->pBlockState[iBlock];
    if (sNative && !pBlock->sNative)
    {
        pBlock->sNative = sNative;
    }
    else if (sNative)
    {
        xllm__free(sNative);
    }
    iBefore = pState->iTextLen;
    if (!xllm__append_tracked(&pBlock->sText, &pState->iTextLen, tText.Data, tText.Size))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                        "failed to append model stream delta");
        return false;
    }
    xllm__assemble_first_token(pCall);
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = eKind == XLLM_BLOCK_TEXT ? XLLM_EVENT_TEXT_DELTA : XLLM_EVENT_REASONING_DELTA;
    tEvent.as.tText.iBlock = iBlock;
    tEvent.as.tText.sData = pBlock->sText + iBefore;
    tEvent.as.tText.iLen = tText.Size;
    return xllm__emit(pCall, &tEvent);
}

static bool xllm__assemble_ensure_tool(xllm_call *pCall, size_t iIndex)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    xllm_tool_call *pNew;
    xllm_tool_state *pStateNew;
    size_t iCap;
    if (!pResponse)
    {
        return false;
    }
    if (iIndex < pResponse->iToolCallCount)
    {
        return true;
    }
    if (iIndex >= pResponse->iToolCallCap)
    {
        iCap = pResponse->iToolCallCap ? pResponse->iToolCallCap : 4u;
        while (iCap <= iIndex)
        {
            if (iCap > SIZE_MAX / 2u)
            {
                goto oom;
            }
            iCap *= 2u;
        }
        if (iCap > SIZE_MAX / sizeof(*pNew) || iCap > SIZE_MAX / sizeof(*pStateNew))
        {
            goto oom;
        }
        pNew = (xllm_tool_call *)xllm__realloc(pResponse->pToolCalls, sizeof(*pNew) * iCap);
        if (!pNew)
            goto oom;
        pResponse->pToolCalls = pNew;
        pStateNew = (xllm_tool_state *)xllm__realloc(pCall->pToolState, sizeof(*pStateNew) * iCap);
        if (!pStateNew)
            goto oom;
        pCall->pToolState = pStateNew;
        memset(pResponse->pToolCalls + pResponse->iToolCallCap, 0,
               sizeof(*pNew) * (iCap - pResponse->iToolCallCap));
        memset(pCall->pToolState + pResponse->iToolCallCap, 0,
               sizeof(*pStateNew) * (iCap - pResponse->iToolCallCap));
        pResponse->iToolCallCap = iCap;
    }
    pResponse->iToolCallCount = iIndex + 1u;
    return true;
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append tool-call slot");
    return false;
}

bool xllm__assemble_tool(xllm_call *pCall, size_t iIndex, xstrview tId, xstrview tName,
                         xstrview tArguments)
{
    xllm_response *pResponse;
    xllm_tool_call *pCallOut;
    xllm_tool_state *pState;
    xllm_event tEvent;
    const char *sIdDelta = NULL;
    const char *sNameDelta = NULL;
    const char *sArgsDelta = NULL;
    if (!pCall || !xllm__assemble_ensure_tool(pCall, iIndex))
    {
        return false;
    }
    pResponse = pCall->pResponse;
    pCallOut = &pResponse->pToolCalls[iIndex];
    pState = &pCall->pToolState[iIndex];
    if (tId.Size)
    {
        if (!xllm__append_tracked(&pCallOut->sId, &pState->iIdLen, tId.Data, tId.Size))
            goto oom;
        sIdDelta = pCallOut->sId + pState->iIdLen - tId.Size;
    }
    if (tName.Size)
    {
        if (!xllm__append_tracked(&pCallOut->sName, &pState->iNameLen, tName.Data, tName.Size))
            goto oom;
        sNameDelta = pCallOut->sName + pState->iNameLen - tName.Size;
    }
    if (tArguments.Size)
    {
        if (!xllm__append_tracked(&pCallOut->sArgumentsJson, &pState->iArgsLen, tArguments.Data,
                                  tArguments.Size))
            goto oom;
        sArgsDelta = pCallOut->sArgumentsJson + pState->iArgsLen - tArguments.Size;
    }
    xllm__assemble_first_token(pCall);
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_TOOL_CALL_DELTA;
    tEvent.as.tToolCall.iIndex = iIndex;
    tEvent.as.tToolCall.iBlock = pResponse->iBlockCount;
    tEvent.as.tToolCall.sIdDelta = sIdDelta;
    tEvent.as.tToolCall.sNameDelta = sNameDelta;
    tEvent.as.tToolCall.sArgumentsDelta = sArgsDelta;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to append tool-call delta");
    return false;
}

bool xllm__assemble_usage(xllm_call *pCall, const xllm_usage *pUsage)
{
    xllm_response *pResponse;
    xllm_event tEvent;
    if (!pUsage)
    {
        return true;
    }
    pResponse = xllm__assemble_ensure(pCall);
    if (!pResponse)
    {
        return false;
    }
    pResponse->tUsage = *pUsage;
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_USAGE;
    tEvent.as.tUsage = *pUsage;
    return xllm__emit(pCall, &tEvent);
}

bool xllm__assemble_finish(xllm_call *pCall, xstrview tRaw)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    char *sRaw = NULL;
    if (!pResponse)
    {
        return false;
    }
    if (tRaw.Data && tRaw.Size)
    {
        if (tRaw.Size == SIZE_MAX)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to record provider finish reason");
            return false;
        }
        sRaw = (char *)xllm__malloc(tRaw.Size + 1u);
        if (!sRaw)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to record provider finish reason");
            return false;
        }
        memcpy(sRaw, tRaw.Data, tRaw.Size);
        sRaw[tRaw.Size] = 0;
    }
    xllm__free(pResponse->sFinishReason);
    pResponse->sFinishReason = sRaw;
    pResponse->eFinish = xllm__finish_from_reason(sRaw, pResponse->iToolCallCount != 0u);
    return true;
}

bool xllm__assemble_refusal(xllm_call *pCall, xstrview tRefusal)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    char *sCopy = NULL;
    if (!pResponse || !tRefusal.Data || !tRefusal.Size)
    {
        return true;
    }
    if (tRefusal.Size == SIZE_MAX)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                        "failed to record provider refusal");
        return false;
    }
    sCopy = (char *)xllm__malloc(tRefusal.Size + 1u);
    if (!sCopy)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                        "failed to record provider refusal");
        return false;
    }
    memcpy(sCopy, tRefusal.Data, tRefusal.Size);
    sCopy[tRefusal.Size] = 0;
    xllm__free(pResponse->sRefusal);
    pResponse->sRefusal = sCopy;
    pResponse->eFinish = XLLM_FINISH_REFUSAL;
    return true;
}

static xllm_finish xllm__finish_from_reason(const char *sRaw, bool bHasToolCalls)
{
    if (!sRaw)
    {
        return bHasToolCalls ? XLLM_FINISH_TOOL_CALLS : XLLM_FINISH_STOP;
    }
    if (strcmp(sRaw, "stop") == 0 || strcmp(sRaw, "end_turn") == 0 ||
        strcmp(sRaw, "stop_sequence") == 0 || strcmp(sRaw, "completed") == 0)
    {
        return XLLM_FINISH_STOP;
    }
    if (strcmp(sRaw, "length") == 0 || strcmp(sRaw, "max_tokens") == 0 ||
        strcmp(sRaw, "max_output_tokens") == 0)
    {
        return XLLM_FINISH_LENGTH;
    }
    if (strcmp(sRaw, "tool_calls") == 0 || strcmp(sRaw, "function_call") == 0 ||
        strcmp(sRaw, "tool_use") == 0)
    {
        return XLLM_FINISH_TOOL_CALLS;
    }
    if (strcmp(sRaw, "content_filter") == 0)
    {
        return XLLM_FINISH_CONTENT_FILTER;
    }
    return XLLM_FINISH_OTHER;
}

static bool xllm__join_blocks(xllm_call *pCall, xllm_block_kind eKind, char **ppOut,
                              size_t *pOutLen)
{
    xllm_response *pResponse = pCall->pResponse;
    size_t i;
    size_t iTotal = 0u;
    char *sOut;
    if (!pResponse)
    {
        return true;
    }
    for (i = 0u; i < pResponse->iBlockCount; ++i)
    {
        if (pResponse->pBlocks[i].eKind == eKind)
        {
            const char *sText = pResponse->pBlocks[i].sText;
            size_t iLen = sText ? strlen(sText) : 0u;
            if (iLen > SIZE_MAX - iTotal)
            {
                xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                                "response block text exceeds addressable capacity");
                return false;
            }
            iTotal += iLen;
        }
    }
    if (!iTotal)
    {
        *ppOut = NULL;
        if (pOutLen)
        {
            *pOutLen = 0u;
        }
        return true;
    }
    if (iTotal == SIZE_MAX)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                        "response block text exceeds addressable capacity");
        return false;
    }
    sOut = (char *)xllm__malloc(iTotal + 1u);
    if (!sOut)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to join response blocks");
        return false;
    }
    iTotal = 0u;
    for (i = 0u; i < pResponse->iBlockCount; ++i)
    {
        if (pResponse->pBlocks[i].eKind == eKind)
        {
            const char *sText = pResponse->pBlocks[i].sText;
            if (sText)
            {
                size_t iLen = strlen(sText);
                memcpy(sOut + iTotal, sText, iLen);
                iTotal += iLen;
            }
        }
    }
    sOut[iTotal] = 0;
    *ppOut = sOut;
    if (pOutLen)
    {
        *pOutLen = iTotal;
    }
    return true;
}

bool xllm__assemble_finalize(xllm_call *pCall)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    xllm_event tEvent;
    size_t i;
    char sGeneratedId[64];
    if (!pResponse)
    {
        return false;
    }
    pResponse->uHttpStatus = pCall->uHttpStatus;
    /* A token cap may leave even syntactically complete early tool calls in
     * an unfinished generation. Do not invoke hooks or return executable
     * tools from that draft. Text-only LENGTH responses remain available to
     * applications that support continuing partial prose. */
    if (pResponse->eFinish == XLLM_FINISH_LENGTH && pResponse->iToolCallCount)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_OUTPUT_LIMIT,
                        "model output limit interrupted tool-call generation");
        return false;
    }
    if (!pResponse->sRequestId && pCall->sRequestId[0])
    {
        pResponse->sRequestId = xllm__strdup(pCall->sRequestId);
        if (!pResponse->sRequestId)
            goto oom;
    }
    if (!pResponse->sModel && pCall->sSelectedModel)
    {
        pResponse->sModel = xllm__strdup(pCall->sSelectedModel);
        if (!pResponse->sModel)
            goto oom;
    }
    for (i = 0u; i < pResponse->iToolCallCount; ++i)
    {
        xllm_block *pToolBlock;
        xllm_tool_call *pTool = &pResponse->pToolCalls[i];
        bool bHasBlock = false;
        size_t j;
        if (!pTool->sId || !pTool->sId[0])
        {
            (void)snprintf(sGeneratedId, sizeof(sGeneratedId), "call_%u", (unsigned)i);
            if (!xllm__replace(&pTool->sId, sGeneratedId))
                goto oom;
        }
        if (!pTool->sName || !pTool->sName[0])
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL,
                            "provider returned a tool call without a function name");
            return false;
        }
        if (!pTool->sArgumentsJson || !pTool->sArgumentsJson[0])
        {
            if (!xllm__replace(&pTool->sArgumentsJson, "{}"))
                goto oom;
        }
        if (pCall->bHasHooks && pCall->tHooks.pOnToolCall)
        {
            if (!pCall->tHooks.pOnToolCall(pCall->pClient, pTool, (uint32_t)i,
                                           pCall->tHooks.pUserData))
            {
                pCall->tHttpDiagnostics.bToolCallDropped = true;
                xllm__free(pTool->sId);
                xllm__free(pTool->sName);
                xllm__free(pTool->sArgumentsJson);
                memset(pTool, 0, sizeof(*pTool));
                continue; /* dropped: no block, compacted below */
            }
        }
        if (!xrtJsonValid((xstrview){pTool->sArgumentsJson, strlen(pTool->sArgumentsJson)}))
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE,
                            "provider returned invalid tool-call arguments JSON");
            return false;
        }
        for (j = 0u; j < pResponse->iBlockCount; ++j)
        {
            if (pResponse->pBlocks[j].eKind == XLLM_BLOCK_TOOL_CALL &&
                pResponse->pBlocks[j].iToolIndex == i)
            {
                bHasBlock = true;
                break;
            }
        }
        if (bHasBlock)
        {
            continue;
        }
        if (pResponse->iBlockCount == SIZE_MAX ||
            pResponse->iBlockCount + 1u > SIZE_MAX / sizeof(*pToolBlock))
        {
            goto oom;
        }
        pToolBlock = (xllm_block *)xllm__realloc(
            pResponse->pBlocks, sizeof(*pToolBlock) * (pResponse->iBlockCount + 1u));
        if (!pToolBlock)
            goto oom;
        pResponse->pBlocks = pToolBlock;
        memset(&pResponse->pBlocks[pResponse->iBlockCount], 0, sizeof(xllm_block));
        pResponse->pBlocks[pResponse->iBlockCount].eKind = XLLM_BLOCK_TOOL_CALL;
        pResponse->pBlocks[pResponse->iBlockCount].iToolIndex = i;
        ++pResponse->iBlockCount;
    }
    { /* compaction pass: hooks may have dropped tool calls (sName NULL).
       * Block staleness consults the ORIGINAL tool array before any move;
       * prefix sums remap surviving TOOL_CALL block indexes. */
        size_t iKept = 0u;
        size_t iBlockKept = 0u;
        size_t j;
        for (j = 0u; j < pResponse->iBlockCount; ++j)
        {
            xllm_block *pBlock = &pResponse->pBlocks[j];
            size_t iBefore = 0u;
            bool bStale = false;
            if (pBlock->eKind == XLLM_BLOCK_TOOL_CALL)
            {
                if (!pResponse->pToolCalls[pBlock->iToolIndex].sName)
                {
                    bStale = true;
                }
                else
                {
                    for (i = 0u; i < pBlock->iToolIndex; ++i)
                    {
                        if (pResponse->pToolCalls[i].sName)
                        {
                            ++iBefore;
                        }
                    }
                }
            }
            if (bStale)
            {
                xllm__free(pBlock->sText);
                xllm__free(pBlock->sNative);
                continue;
            }
            if (pBlock->eKind == XLLM_BLOCK_TOOL_CALL)
            {
                pBlock->iToolIndex = iBefore;
            }
            if (iBlockKept != j)
            {
                pResponse->pBlocks[iBlockKept] = *pBlock;
            }
            ++iBlockKept;
        }
        pResponse->iBlockCount = iBlockKept;
        for (i = 0u; i < pResponse->iToolCallCount; ++i)
        {
            xllm_tool_call *pTool = &pResponse->pToolCalls[i];
            if (!pTool->sName)
            {
                continue;
            }
            if (iKept != i)
            {
                pResponse->pToolCalls[iKept] = *pTool;
            }
            ++iKept;
        }
        pResponse->iToolCallCount = iKept;
    }
    if (pResponse->sRefusal && pResponse->sRefusal[0])
    {
        pResponse->eFinish = XLLM_FINISH_REFUSAL;
    }
    else if (!pResponse->sFinishReason)
    {
        pResponse->eFinish = pResponse->iToolCallCount ? XLLM_FINISH_TOOL_CALLS : XLLM_FINISH_STOP;
    }
    else
    {
        pResponse->eFinish =
            xllm__finish_from_reason(pResponse->sFinishReason, pResponse->iToolCallCount != 0u);
    }
    if (!pResponse->sContent &&
        !xllm__join_blocks(pCall, XLLM_BLOCK_TEXT, &pResponse->sContent, NULL))
    {
        return false;
    }
    if (!pResponse->sContent)
    {
        pResponse->sContent = xllm__strdup("");
        if (!pResponse->sContent)
            goto oom;
    }
    if (!pResponse->sReasoningContent &&
        !xllm__join_blocks(pCall, XLLM_BLOCK_REASONING, &pResponse->sReasoningContent, NULL))
    {
        return false;
    }
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_RESPONSE_DONE;
    tEvent.as.tResponse.uHttpStatus = pCall->uHttpStatus;
    tEvent.as.tResponse.sRequestId = pCall->sRequestId;
    return xllm__emit(pCall, &tEvent);
oom:
    xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY, "failed to finalize model response");
    return false;
}
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_dialect.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* Dialect registry: providers resolve to a wire-dialect vtable. GLM rides
 * the completions dialect with provider tweak flags; Responses and Anthropic
 * register their own tables as they land. */

const xllm_dialect_ops *xllm__dialect_ops_for(xllm_provider eProvider)
{
    switch (eProvider)
    {
    case XLLM_PROVIDER_GLM:
    case XLLM_PROVIDER_OPENAI_COMPAT:
        return xllm__dialect_completions();
    case XLLM_PROVIDER_OPENAI_RESPONSES:
        return xllm__dialect_responses();
    case XLLM_PROVIDER_ANTHROPIC:
        return xllm__dialect_anthropic();
    default:
        return NULL;
    }
}
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/dialect_completions.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* Chat Completions dialect, including the GLM tweak set. Serialization
 * flags are resolved once per request from the provider and the active
 * model profile; the decoders map wire chunks into the unified events. */

typedef struct xllm_completions_dialect
{
    bool bReasoningContentField;  /* assistant history carries reasoning_content */
    bool bThinkingObject;         /* reasoning control serializes as a thinking object */
    bool bToolStreamField;        /* tool_stream hint belongs to the wire contract */
    bool bStrictSchemaField;      /* tool schemas accept strict:true */
    bool bParallelToolCallsField; /* parallel_tool_calls belongs to the wire contract */
    bool bStreamOptionsUsage;     /* stream_options.include_usage is honored */
    bool bMaxCompletionTokens;    /* max_completion_tokens replaces max_tokens */
    bool bDeveloperRole;          /* system instructions map to the developer role */
} xllm_completions_dialect;

static xllm_completions_dialect xllm__completions_flags(const xllm_client *pClient)
{
    xllm_completions_dialect tFlags;
    memset(&tFlags, 0, sizeof(tFlags));
    if (pClient && pClient->eProvider == XLLM_PROVIDER_GLM)
    {
        tFlags.bReasoningContentField = true;
        tFlags.bThinkingObject = true;
        tFlags.bToolStreamField = true;
    }
    else
    {
        tFlags.bStrictSchemaField = true;
        tFlags.bParallelToolCallsField = true;
        tFlags.bStreamOptionsUsage = true;
        if (pClient && pClient->bHasModelProfile)
        {
            tFlags.bMaxCompletionTokens =
                (pClient->tModelProfile.uCapabilities & XLLM_CAP_MAX_COMPLETION_TOKENS) != 0u;
            tFlags.bDeveloperRole =
                (pClient->tModelProfile.uCapabilities & XLLM_CAP_DEVELOPER_ROLE) != 0u;
        }
    }
    return tFlags;
}

/* ------------------------------------------------------------------ */
/* Request serialization                                               */
/* ------------------------------------------------------------------ */

static const char *xllm__role_name(xllm_role eRole, const xllm_completions_dialect *pFlags)
{
    switch (eRole)
    {
    case XLLM_ROLE_SYSTEM:
        return pFlags->bDeveloperRole ? "developer" : "system";
    case XLLM_ROLE_USER:
        return "user";
    case XLLM_ROLE_ASSISTANT:
        return "assistant";
    case XLLM_ROLE_TOOL:
        return "tool";
    default:
        return NULL;
    }
}

static bool xllm__json_u32(xllm_buf *pBuf, uint32_t uValue)
{
    char sValue[32];
    (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)uValue);
    return xllm__buf_append_cstr(pBuf, sValue);
}

static bool xllm__json_double(xllm_buf *pBuf, double fValue)
{
    char sValue[64];
    (void)snprintf(sValue, sizeof(sValue), "%.17g", fValue);
    return xllm__buf_append_cstr(pBuf, sValue);
}

static bool xllm__append_data_url(xllm_buf *pBuf, const xllm_part *pPart)
{
    str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
    bool bOk;
    if (!sEncoded)
    {
        return false;
    }
    bOk = xllm__buf_append_cstr(pBuf, "data:") &&
          xllm__buf_append_cstr(pBuf, pPart->sMediaType ? pPart->sMediaType
                                                        : "application/octet-stream") &&
          xllm__buf_append_cstr(pBuf, ";base64,") && xllm__buf_append_cstr(pBuf, sEncoded);
    xrtFree(sEncoded);
    return bOk;
}

static const char *xllm__media_subtype(const char *sMediaType)
{
    const char *sSlash = sMediaType ? strchr(sMediaType, '/') : NULL;
    return sSlash ? sSlash + 1 : NULL;
}

/* Serialize one content part as a completions content-array element. */
static bool xllm__append_part_object(xllm_buf *pBuf, const xllm_part *pPart, xllm_error *pError)
{
    switch (pPart->eKind)
    {
    case XLLM_PART_TEXT:
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"text\",\"text\":") ||
            !xllm__json_string(pBuf, pPart->sText ? pPart->sText : "") ||
            !xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        return true;
    case XLLM_PART_IMAGE:
        if (pPart->sSourceUrl && pPart->sSourceUrl[0])
        {
            if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"image_url\",\"image_url\":{\"url\":") ||
                !xllm__json_string(pBuf, pPart->sSourceUrl))
            {
                return false;
            }
        }
        else if (pPart->pData && pPart->iDataSize)
        {
            xllm_buf tUrl = {0};
            bool bUrl;
            if (!xllm__append_data_url(&tUrl, pPart))
            {
                xllm__buf_reset(&tUrl);
                return false;
            }
            bUrl = xllm__buf_append_cstr(pBuf, "{\"type\":\"image_url\",\"image_url\":{\"url\":") &&
                   xllm__json_string(pBuf, tUrl.pData ? tUrl.pData : "");
            xllm__buf_reset(&tUrl);
            if (!bUrl)
            {
                return false;
            }
        }
        else
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "image part has neither bytes nor URL");
            return false;
        }
        if (pPart->sDetail && pPart->sDetail[0])
        {
            if (!xllm__buf_append_cstr(pBuf, ",\"detail\":") ||
                !xllm__json_string(pBuf, pPart->sDetail))
            {
                return false;
            }
        }
        return xllm__buf_append_cstr(pBuf, "}}");
    case XLLM_PART_AUDIO:
        if (!pPart->pData || !pPart->iDataSize || !pPart->sMediaType)
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "audio part requires bytes and a media type");
            return false;
        }
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"input_audio\",\"input_audio\":{\"data\":"))
        {
            return false;
        }
        {
            str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
            bool bOk;
            if (!sEncoded)
            {
                return false;
            }
            bOk = xllm__json_string(pBuf, sEncoded);
            xrtFree(sEncoded);
            if (!bOk)
            {
                return false;
            }
        }
        if (!xllm__buf_append_cstr(pBuf, ",\"format\":") ||
            !xllm__json_string(pBuf, xllm__media_subtype(pPart->sMediaType)))
        {
            return false;
        }
        return xllm__buf_append_cstr(pBuf, "}}");
    case XLLM_PART_FILE:
        if (!pPart->pData || !pPart->iDataSize || !pPart->sMediaType)
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "file part requires bytes and a media type");
            return false;
        }
        {
            xllm_buf tUrl = {0};
            bool bUrl;
            if (!xllm__append_data_url(&tUrl, pPart))
            {
                xllm__buf_reset(&tUrl);
                return false;
            }
            bUrl = xllm__buf_append_cstr(pBuf, "{\"type\":\"file\",\"file\":{\"filename\":") &&
                   xllm__json_string(pBuf, pPart->sMediaType) &&
                   xllm__buf_append_cstr(pBuf, ",\"file_data\":") &&
                   xllm__json_string(pBuf, tUrl.pData ? tUrl.pData : "");
            xllm__buf_reset(&tUrl);
            if (!bUrl)
            {
                return false;
            }
        }
        return xllm__buf_append_cstr(pBuf, "}}");
    case XLLM_PART_NATIVE:
        /* Spliced verbatim; callers guarantee valid JSON. */
        return xllm__buf_append_cstr(pBuf, pPart->sText ? pPart->sText : "{}");
    default:
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "message part has an unsupported kind");
        return false;
    }
}

static bool xllm__append_parts(xllm_buf *pBuf, const xllm_message *pMessage, xllm_error *pError)
{
    size_t i;
    size_t iUsable = 0u;
    bool bArray = false;
    for (i = 0u; i < pMessage->iPartCount; ++i)
    {
        xllm_part_kind eKind = pMessage->pParts[i].eKind;
        /* Reasoning uses the dialect's separate reasoning_content field. An
         * Anthropic signature is metadata, never raw JSON or visible text. */
        if (eKind == XLLM_PART_REASONING || xllm__part_is_thinking_signature(&pMessage->pParts[i]))
            continue;
        ++iUsable;
        if (eKind != XLLM_PART_TEXT && eKind != XLLM_PART_REASONING)
        {
            bArray = true;
        }
    }
    if (!iUsable)
    {
        /* Every part was dialect-irrelevant; fall back to empty text. */
        return xllm__buf_append_cstr(pBuf, "\"\"");
    }
    if (!bArray)
    {
        /* Pure text: keep the compact string shape. */
        if (!xllm__buf_append_char(pBuf, '"'))
        {
            return false;
        }
        for (i = 0u; i < pMessage->iPartCount; ++i)
        {
            const char *sText = pMessage->pParts[i].sText;
            if (pMessage->pParts[i].eKind == XLLM_PART_REASONING ||
                xllm__part_is_thinking_signature(&pMessage->pParts[i]))
                continue;
            if (!xllm__json_string_content(pBuf, sText))
                return false;
        }
        return xllm__buf_append_char(pBuf, '"');
    }
    if (!xllm__buf_append_char(pBuf, '['))
    {
        return false;
    }
    {
        bool bFirst = true;
        for (i = 0u; i < pMessage->iPartCount; ++i)
        {
            if (pMessage->pParts[i].eKind == XLLM_PART_REASONING ||
                xllm__part_is_thinking_signature(&pMessage->pParts[i]))
                continue;
            if (!bFirst && !xllm__buf_append_char(pBuf, ','))
            {
                return false;
            }
            if (!xllm__append_part_object(pBuf, &pMessage->pParts[i], pError))
            {
                return false;
            }
            bFirst = false;
        }
    }
    return xllm__buf_append_char(pBuf, ']');
}

static bool xllm__append_message(xllm_buf *pBuf, const xllm_message *pMessage,
                                 const xllm_completions_dialect *pFlags, xllm_error *pError)
{
    const char *sRole = xllm__role_name(pMessage->eRole, pFlags);
    size_t i;
    if (!sRole)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "message has an invalid role");
        return false;
    }
    if (!xllm__buf_append_cstr(pBuf, "{\"role\":") || !xllm__json_string(pBuf, sRole))
        return false;

    if (pMessage->eRole == XLLM_ROLE_TOOL)
    {
        if (!pMessage->sToolCallId || !pMessage->sToolCallId[0])
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "tool message is missing tool_call_id");
            return false;
        }
        if (!xllm__buf_append_cstr(pBuf, ",\"tool_call_id\":") ||
            !xllm__json_string(pBuf, pMessage->sToolCallId))
            return false;
    }

    if (pFlags->bReasoningContentField && pMessage->eRole == XLLM_ROLE_ASSISTANT &&
        pMessage->sReasoningContent && pMessage->sReasoningContent[0])
    {
        if (!xllm__buf_append_cstr(pBuf, ",\"reasoning_content\":") ||
            !xllm__json_string(pBuf, pMessage->sReasoningContent))
            return false;
    }

    if (pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u)
    {
        if (!xllm__buf_append_cstr(pBuf, ",\"tool_calls\":["))
            return false;
        for (i = 0u; i < pMessage->iToolCallCount; ++i)
        {
            const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
            if (!pCall->sName || !pCall->sName[0])
            {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                                "assistant tool call is missing its function name");
                return false;
            }
            if (i && !xllm__buf_append_char(pBuf, ','))
                return false;
            if (!xllm__buf_append_cstr(pBuf, "{\"id\":") ||
                !xllm__json_string(pBuf, pCall->sId ? pCall->sId : "") ||
                !xllm__buf_append_cstr(pBuf, ",\"type\":\"function\",\"function\":{\"name\":") ||
                !xllm__json_string(pBuf, pCall->sName) ||
                !xllm__buf_append_cstr(pBuf, ",\"arguments\":") ||
                !xllm__json_string(pBuf, pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                !xllm__buf_append_cstr(pBuf, "}}"))
                return false;
        }
        if (!xllm__buf_append_char(pBuf, ']'))
            return false;
    }

    if (pMessage->iPartCount > 0u)
    {
        if (!xllm__buf_append_cstr(pBuf, ",\"content\":") ||
            !xllm__append_parts(pBuf, pMessage, pError))
            return false;
    }
    else if (pMessage->sContent || pMessage->eRole != XLLM_ROLE_ASSISTANT ||
             pMessage->iToolCallCount == 0u)
    {
        if (!xllm__buf_append_cstr(pBuf, ",\"content\":") ||
            !xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : ""))
            return false;
    }
    else if (!xllm__buf_append_cstr(pBuf, ",\"content\":null"))
    {
        return false;
    }
    return xllm__buf_append_char(pBuf, '}');
}

static bool xllm__append_tools(xllm_buf *pBuf, const xllm_completions_dialect *pFlags,
                               const xllm_request *pRequest, xllm_error *pError)
{
    size_t i;
    if (!xllm__buf_append_cstr(pBuf, ",\"tools\":["))
        return false;
    for (i = 0u; i < pRequest->iToolCount; ++i)
    {
        const xllm_tool *pTool = &pRequest->pTools[i];
        const char *sSchema = pTool->sParametersJson ? pTool->sParametersJson
                                                     : "{\"type\":\"object\",\"properties\":{}}";
        if (!pTool->sName || !pTool->sName[0] ||
            !xrtJsonValid((xstrview){sSchema, strlen(sSchema)}))
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "tool has an invalid name or JSON parameter schema");
            return false;
        }
        if (i && !xllm__buf_append_char(pBuf, ','))
            return false;
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"function\",\"function\":{\"name\":") ||
            !xllm__json_string(pBuf, pTool->sName) ||
            !xllm__buf_append_cstr(pBuf, ",\"description\":") ||
            !xllm__json_string(pBuf, pTool->sDescription ? pTool->sDescription : "") ||
            !xllm__buf_append_cstr(pBuf, ",\"parameters\":") ||
            !xllm__buf_append_cstr(pBuf, sSchema))
            return false;
        if (pTool->bStrict && pFlags->bStrictSchemaField &&
            !xllm__buf_append_cstr(pBuf, ",\"strict\":true"))
            return false;
        if (!xllm__buf_append_cstr(pBuf, "}}"))
            return false;
    }
    if (!xllm__buf_append_char(pBuf, ']'))
        return false;

    if (!xllm__buf_append_cstr(pBuf, ",\"tool_choice\":"))
        return false;
    switch (pRequest->eToolChoice)
    {
    case XLLM_TOOL_CHOICE_AUTO:
        if (!xllm__json_string(pBuf, "auto"))
            return false;
        break;
    case XLLM_TOOL_CHOICE_NONE:
        if (!xllm__json_string(pBuf, "none"))
            return false;
        break;
    case XLLM_TOOL_CHOICE_REQUIRED:
        if (!xllm__json_string(pBuf, "required"))
            return false;
        break;
    case XLLM_TOOL_CHOICE_NAMED:
        if (!pRequest->sNamedTool || !pRequest->sNamedTool[0])
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "named tool choice is missing a tool name");
            return false;
        }
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"function\",\"function\":{\"name\":") ||
            !xllm__json_string(pBuf, pRequest->sNamedTool) || !xllm__buf_append_cstr(pBuf, "}}"))
            return false;
        break;
    default:
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid tool choice");
        return false;
    }
    if (pFlags->bToolStreamField)
    {
        return xllm__buf_append_cstr(pBuf, ",\"tool_stream\":true");
    }
    if (!pFlags->bParallelToolCallsField)
    {
        return true;
    }
    return xllm__buf_append_cstr(pBuf, pRequest->bParallelToolCalls
                                           ? ",\"parallel_tool_calls\":true"
                                           : ",\"parallel_tool_calls\":false");
}

static char *xllm__completions_build_request_cached(xllm_client *pClient,
                                                    const xllm_request *pRequest, xllm_buf *pInner,
                                                    size_t iPrefixCount, xllm_error *pError);

static char *xllm__completions_build_request(xllm_client *pClient, const xllm_request *pRequest,
                                             xllm_error *pError)
{
    xllm_buf tInner = {0};
    char *sResult = xllm__completions_build_request_cached(pClient, pRequest, &tInner, 0u, pError);
    xllm__free(tInner.pData); /* the classic path does not keep the accumulator */
    return sResult;
}

/* Incremental core: appends messages [iPrefixCount..N) into pInner (which
 * already holds the cached prefix bytes for earlier messages) and assembles
 * the full body around it. Per-message serialization here is stateless, so
 * any message boundary is a valid prefix cut. */
static char *xllm__completions_build_request_cached(xllm_client *pClient,
                                                    const xllm_request *pRequest, xllm_buf *pInner,
                                                    size_t iPrefixCount, xllm_error *pError)
{
    xllm_buf tBody = {0};
    xllm_completions_dialect tFlags;
    const char *sModel;
    const char *sEffort;
    const char *sMaxTokensField;
    uint32_t uMaxTokens;
    size_t i;
    char *sResult = NULL;
    tFlags = xllm__completions_flags(pClient);
    sModel = (pRequest->sModel && pRequest->sModel[0]) ? pRequest->sModel : pClient->sModel;
    if (!sModel || !sModel[0])
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "no model is configured");
        return NULL;
    }
    if (pRequest->iMessageCount == 0u)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request has no messages");
        return NULL;
    }
    if (iPrefixCount > pRequest->iMessageCount)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "prefix count exceeds the message count");
        return NULL;
    }
    if (pRequest->sExtraBodyJson && pRequest->sExtraBodyJson[0] &&
        !xrtJsonValid((xstrview){pRequest->sExtraBodyJson, strlen(pRequest->sExtraBodyJson)}))
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "extra body JSON is not a valid JSON object");
        return NULL;
    }
    /* 1. Serialize the delta into the accumulator (prefix bytes ride along). */
    for (i = iPrefixCount; i < pRequest->iMessageCount; ++i)
    {
        if (pInner->iLen && !xllm__buf_append_char(pInner, ','))
            goto oom;
        if (!xllm__append_message(pInner, &pRequest->pMessages[i], &tFlags, pError))
            goto fail;
    }
    /* 2. Assemble the body: header + accumulator + footer. */
    if (!xllm__buf_append_cstr(&tBody, "{\"model\":") || !xllm__json_string(&tBody, sModel) ||
        !xllm__buf_append_cstr(&tBody, ",\"messages\":[") ||
        !xllm__buf_append(&tBody, pInner->pData, pInner->iLen) ||
        !xllm__buf_append_char(&tBody, ']'))
        goto oom;
    /* Wire alignment (pi behavior): never persist this exchange server-side.
     * store governs data retention, not prompt caching. A caller-provided
     * extraBody "store" key wins to avoid duplicate keys in the merge. */
    if (!pRequest->sExtraBodyJson || strstr(pRequest->sExtraBodyJson, "\"store\"") == NULL)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"store\":false"))
            goto oom;
    }

    uMaxTokens =
        pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pClient->uMaxOutputTokens;
    sMaxTokensField =
        tFlags.bMaxCompletionTokens ? ",\"max_completion_tokens\":" : ",\"max_tokens\":";
    if (uMaxTokens &&
        (!xllm__buf_append_cstr(&tBody, sMaxTokensField) || !xllm__json_u32(&tBody, uMaxTokens)))
        goto oom;
    if (pRequest->bHasTemperature)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"temperature\":") ||
            !xllm__json_double(&tBody, pRequest->fTemperature))
            goto oom;
    }
    if (pRequest->bHasTopP)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"top_p\":") ||
            !xllm__json_double(&tBody, pRequest->fTopP))
            goto oom;
    }
    if (pRequest->sStop && pRequest->sStop[0])
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"stop\":") ||
            !xllm__json_string(&tBody, pRequest->sStop))
            goto oom;
    }
    if (pRequest->eJsonMode == XLLM_JSON_OBJECT)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"response_format\":{\"type\":\"json_object\"}"))
            goto oom;
    }

    sEffort = (pRequest->sReasoningEffort && pRequest->sReasoningEffort[0])
                  ? pRequest->sReasoningEffort
                  : pClient->sReasoningEffort;
    if (sEffort && sEffort[0] && strcmp(sEffort, "off") != 0)
    {
        if (tFlags.bThinkingObject)
        {
            if (!xllm__buf_append_cstr(
                    &tBody, ",\"thinking\":{\"type\":\"enabled\",\"clear_thinking\":false}"))
                goto oom;
        }
        else
        {
            if (!xllm__buf_append_cstr(&tBody, ",\"reasoning_effort\":") ||
                !xllm__json_string(&tBody, sEffort))
                goto oom;
        }
    }
    if (pRequest->iToolCount > 0u && !xllm__append_tools(&tBody, &tFlags, pRequest, pError))
        goto fail;
    if (pRequest->sExtraBodyJson && pRequest->sExtraBodyJson[0])
    {
        /* Shallow-merge a caller-supplied JSON object before closing. */
        const char *s = pRequest->sExtraBodyJson;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        {
            ++s;
        }
        if (*s == '{')
        {
            size_t iEnd = strlen(s);
            /* strip trailing whitespace, then exactly one closing brace:
             * nested objects legitimately end with multiple braces. */
            while (iEnd && (s[iEnd - 1u] == ' ' || s[iEnd - 1u] == '\t' || s[iEnd - 1u] == '\r' ||
                            s[iEnd - 1u] == '\n'))
            {
                --iEnd;
            }
            if (iEnd && s[iEnd - 1u] == '}')
            {
                --iEnd;
            }
            if (iEnd > 1u && !xllm__buf_append_cstr(&tBody, ","))
                goto oom;
            if (iEnd > 1u && !xllm__buf_append(&tBody, s + 1u, iEnd - 1u))
                goto oom;
        }
    }
    if (!pRequest->bStream)
    {
        if (!xllm__buf_append_char(&tBody, '}'))
            goto oom;
    }
    else if (!tFlags.bStreamOptionsUsage)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"stream\":true}"))
            goto oom;
    }
    else if (!xllm__buf_append_cstr(
                 &tBody, ",\"stream\":true,\"stream_options\":{\"include_usage\":true}}"))
    {
        goto oom;
    }
    sResult = xllm__buf_detach(&tBody);
    if (!sResult)
        goto oom;
    return sResult;

oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
fail:
    if ( pError && pError->eCode == XLLM_ERROR_NONE ) {
        xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
    }
    xllm__buf_reset(&tBody);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Response decoding                                                   */
/* ------------------------------------------------------------------ */

static size_t xllm__completions_build_auth(const xllm_client *pClient, xllm_auth_header *pOut,
                                           size_t iCap)
{
    size_t iKeyLen;
    if (!pClient || !pOut || !iCap || !pClient->sApiKey[0])
    {
        return 0u;
    }
    iKeyLen = strlen(pClient->sApiKey);
    pOut[0].sValue = (char *)xllm__malloc(iKeyLen + 8u);
    if (!pOut[0].sValue)
    {
        return 0u;
    }
    memcpy(pOut[0].sValue, "Bearer ", 7u);
    memcpy(pOut[0].sValue + 7u, pClient->sApiKey, iKeyLen + 1u);
    xllm__copy_text(pOut[0].sName, sizeof(pOut[0].sName), "Authorization");
    return 1u;
}

static void xllm__completions_fill_usage(xllm_usage *pUsage, xvalue *pUsageObject)
{
    xvalue *pPromptDetails;
    xvalue *pCompletionDetails;
    if (!pUsageObject || xrtValueType(pUsageObject) != XVALUE_OBJECT)
    {
        return;
    }
    pUsage->uInputTokens = xllm__json_u64(pUsageObject, "prompt_tokens");
    pUsage->uOutputTokens = xllm__json_u64(pUsageObject, "completion_tokens");
    pUsage->uTotalTokens = xllm__json_u64(pUsageObject, "total_tokens");
    pPromptDetails = xllm__json_get(pUsageObject, "prompt_tokens_details");
    pCompletionDetails = xllm__json_get(pUsageObject, "completion_tokens_details");
    pUsage->uCachedInputTokens = xllm__json_u64(pPromptDetails, "cached_tokens");
    pUsage->uReasoningTokens = xllm__json_u64(pCompletionDetails, "reasoning_tokens");
    if (pUsage->uReasoningTokens == 0u)
    {
        pUsage->uReasoningTokens = xllm__json_u64(pUsageObject, "reasoning_tokens");
    }
}

static void xllm__completions_fill_provider_error(xllm_call *pCall, xvalue *pRoot)
{
    xvalue *pError = xllm__json_get(pRoot, "error");
    xvalue *pCode = xllm__json_get(pError, "code");
    xstrview tMessage = xllm__json_text(pError, "message");
    xstrview tCode = xllm__json_text(pError, "code");
    xstrview tType = xllm__json_text(pError, "type");
    int64 iCode = 0;
    if (!tMessage.Data)
        tMessage = xllm__json_text(pRoot, "message");
    if (!tCode.Data)
    {
        if (!pCode)
            pCode = xllm__json_get(pRoot, "code");
        tCode = xllm__json_text(pRoot, "code");
    }
    if (!tType.Data)
        tType = xllm__json_text(pRoot, "type");
    if (tMessage.Data)
        xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                        tMessage);
    if (tType.Data)
        xllm__copy_view(pCall->tError.sProviderType, sizeof(pCall->tError.sProviderType), tType);
    if (tCode.Data)
    {
        xllm__copy_view(pCall->tError.sProviderCode, sizeof(pCall->tError.sProviderCode), tCode);
    }
    else if (pCode && xrtValueGetInt(pCode, &iCode))
    {
        (void)snprintf(pCall->tError.sProviderCode, sizeof(pCall->tError.sProviderCode), "%lld",
                       (long long)iCode);
    }
    if (!pCall->tError.sMessage[0])
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
    }
}

static void xllm__completions_fill_error_body(xllm_call *pCall, xstrview tBody)
{
    xvalue *pRoot;
    if (!pCall || !tBody.Data || !tBody.Size)
    {
        return;
    }
    if (!xrtJsonValid(tBody))
    {
        xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                        tBody);
        return;
    }
    pRoot = xrtJsonParse(tBody);
    if (!pRoot)
    {
        xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                        tBody);
        return;
    }
    xllm__completions_fill_provider_error(pCall, pRoot);
    xrtValueRelease(pRoot);
}

static bool xllm__completions_apply_choice(xllm_call *pCall, xvalue *pChoice)
{
    xvalue *pDelta = xllm__json_get(pChoice, "delta");
    xvalue *pToolCalls;
    xstrview tContent;
    xstrview tReasoning;
    xstrview tRefusal;
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    uint32_t i;
    if (!pResponse)
    {
        return false;
    }
    if (!pDelta || xrtValueType(pDelta) != XVALUE_OBJECT)
    {
        /* Non-streaming messages carry the same fields one level up. */
        pDelta = xllm__json_get(pChoice, "message");
    }
    tContent = xllm__json_text(pDelta, "content");
    tReasoning = xllm__json_text(pDelta, "reasoning_content");
    if (!tReasoning.Data)
        tReasoning = xllm__json_text(pDelta, "reasoning");
    if (!tReasoning.Data)
        tReasoning = xllm__json_text(pDelta, "thinking");
    tRefusal = xllm__json_text(pDelta, "refusal");
    if (tRefusal.Data && !xllm__assemble_refusal(pCall, tRefusal))
    {
        return false;
    }
    if (tContent.Size)
    {
        if (!xllm__assemble_text(pCall, XLLM_BLOCK_TEXT, tContent, NULL))
        {
            return false;
        }
    }
    if (tReasoning.Size)
    {
        if (!xllm__assemble_text(pCall, XLLM_BLOCK_REASONING, tReasoning, NULL))
        {
            return false;
        }
    }
    pToolCalls = xllm__json_get(pDelta, "tool_calls");
    if (pToolCalls && xrtValueType(pToolCalls) == XVALUE_ARRAY)
    {
        size_t uCount = xrtValueCount(pToolCalls);
        for (i = 0u; i < uCount; ++i)
        {
            xvalue *pToolCall = xrtValueArrayGet(pToolCalls, i);
            xvalue *pFunction = xllm__json_get(pToolCall, "function");
            xvalue *pIndex = xllm__json_get(pToolCall, "index");
            int64 iValue = (int64)i;
            size_t iIndex;
            if (pIndex)
                (void)xrtValueGetInt(pIndex, &iValue);
            iIndex = iValue >= 0 ? (size_t)iValue : (size_t)i;
            if (!xllm__assemble_tool(pCall, iIndex, xllm__json_text(pToolCall, "id"),
                                     xllm__json_text(pFunction, "name"),
                                     xllm__json_text(pFunction, "arguments")))
            {
                return false;
            }
        }
    }
    {
        xstrview tFinish = xllm__json_text(pChoice, "finish_reason");
        if (tFinish.Data && !xllm__assemble_finish(pCall, tFinish))
        {
            return false;
        }
    }
    return true;
}

static bool xllm__completions_decode_root(xllm_call *pCall, xvalue *pRoot)
{
    xvalue *pChoices;
    xstrview tId;
    xstrview tModel;
    xllm_usage tUsage;
    if (xllm__json_get(pRoot, "error"))
    {
        xllm__completions_fill_provider_error(pCall, pRoot);
        return false;
    }
    tId = xllm__json_text(pRoot, "id");
    tModel = xllm__json_text(pRoot, "model");
    if (!xllm__assemble_metadata(pCall, tId, tModel))
    {
        return false;
    }
    pChoices = xllm__json_get(pRoot, "choices");
    if (pChoices && xrtValueType(pChoices) == XVALUE_ARRAY)
    {
        size_t uCount = xrtValueCount(pChoices);
        uint32_t i;
        for (i = 0u; i < uCount; ++i)
        {
            xvalue *pChoice = xrtValueArrayGet(pChoices, i);
            xvalue *pIndex = xllm__json_get(pChoice, "index");
            int64 iIndex = 0;
            if (pIndex && xrtValueGetInt(pIndex, &iIndex) && iIndex != 0)
                continue;
            if (!xllm__completions_apply_choice(pCall, pChoice))
            {
                return false;
            }
            break;
        }
    }
    memset(&tUsage, 0, sizeof(tUsage));
    xllm__completions_fill_usage(&tUsage, xllm__json_get(pRoot, "usage"));
    if (tUsage.uTotalTokens || tUsage.uInputTokens || tUsage.uOutputTokens)
    {
        if (!xllm__assemble_usage(pCall, &tUsage))
        {
            return false;
        }
    }
    pCall->bSawEvent = true;
    return true;
}

static bool xllm__completions_decode_sse(xllm_call *pCall, const xllm_sse_fields *pFields)
{
    xvalue *pRoot;
    bool bOk;
    if (!pCall || !pFields)
    {
        return false;
    }
    if (pFields->tData.Size == 6u && memcmp(pFields->tData.Data, "[DONE]", 6u) == 0)
    {
        pCall->bDone = true;
        return true;
    }
    if (!pFields->tData.Data || !pFields->tData.Size)
    {
        return true;
    }
    if (!xrtJsonValid(pFields->tData))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pRoot = xrtJsonParse(pFields->tData);
    if (!pRoot)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    bOk = xllm__completions_decode_root(pCall, pRoot);
    xrtValueRelease(pRoot);
    return bOk;
}

static bool xllm__completions_decode_json(xllm_call *pCall, xstrview tBody)
{
    xvalue *pRoot;
    bool bOk;
    if (!pCall || !tBody.Data || !tBody.Size)
    {
        return false;
    }
    if (!xrtJsonValid(tBody))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    pRoot = xrtJsonParse(tBody);
    if (!pRoot)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    bOk = xllm__completions_decode_root(pCall, pRoot);
    xrtValueRelease(pRoot);
    return bOk;
}

static const xllm_dialect_ops XLLM_COMPLETIONS_DIALECT = {"completions",
                                                          "/chat/completions",
                                                          xllm__completions_build_auth,
                                                          xllm__completions_build_request,
                                                          xllm__completions_build_request_cached,
                                                          xllm__completions_decode_sse,
                                                          xllm__completions_decode_json,
                                                          xllm__completions_fill_error_body,
                                                          NULL};

const xllm_dialect_ops *xllm__dialect_completions(void) { return &XLLM_COMPLETIONS_DIALECT; }
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/dialect_anthropic.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* Anthropic Messages dialect.
 *
 * Request: system messages lift to the top-level system field, tool results
 * ride user messages as tool_result blocks, assistant tool calls become
 * tool_use blocks, and max_tokens is always sent (the endpoint requires it).
 * Streaming: typed events (message_start / content_block_start / _delta /
 * _stop / message_delta / message_stop / error / ping) are decoded through
 * the framing layer's event-name field. Thinking signatures are captured
 * into the reasoning block's native blob and replayed as thinking blocks
 * when history carries a thinking_signature NATIVE part. */

static size_t xllm__anthropic_build_auth(const xllm_client *pClient, xllm_auth_header *pOut,
                                         size_t iCap)
{
    size_t iKeyLen;
    if (!pClient || !pOut || iCap < 2u)
    {
        return 0u;
    }
    memset(pOut, 0, sizeof(*pOut) * 2u);
    if (!pClient->sApiKey[0])
    {
        xllm__copy_text(pOut[0].sName, sizeof(pOut[0].sName), "anthropic-version");
        pOut[0].sValue = xllm__strdup("2023-06-01");
        return pOut[0].sValue ? 1u : 0u;
    }
    iKeyLen = strlen(pClient->sApiKey);
    pOut[0].sValue = (char *)xllm__malloc(iKeyLen + 1u);
    pOut[1].sValue = (char *)xllm__malloc(16u);
    if (!pOut[0].sValue || !pOut[1].sValue)
    {
        xllm__free(pOut[0].sValue);
        xllm__free(pOut[1].sValue);
        pOut[0].sValue = NULL;
        pOut[1].sValue = NULL;
        return 0u;
    }
    memcpy(pOut[0].sValue, pClient->sApiKey, iKeyLen + 1u);
    memcpy(pOut[1].sValue, "2023-06-01", 11u);
    xllm__copy_text(pOut[0].sName, sizeof(pOut[0].sName), "x-api-key");
    xllm__copy_text(pOut[1].sName, sizeof(pOut[1].sName), "anthropic-version");
    return 2u;
}

static bool xllm__anthropic_append_source_b64(xllm_buf *pBuf, const xllm_part *pPart)
{
    str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
    bool bOk;
    if (!sEncoded)
    {
        return false;
    }
    bOk = xllm__buf_append_cstr(pBuf, "{\"type\":\"base64\",\"media_type\":") &&
          xllm__json_string(pBuf,
                            pPart->sMediaType ? pPart->sMediaType : "application/octet-stream") &&
          xllm__buf_append_cstr(pBuf, ",\"data\":") && xllm__json_string(pBuf, sEncoded) &&
          xllm__buf_append_char(pBuf, '}');
    xrtFree(sEncoded);
    return bOk;
}

static bool xllm__anthropic_append_part(xllm_buf *pBuf, const xllm_part *pPart, xllm_error *pError)
{
    switch (pPart->eKind)
    {
    case XLLM_PART_TEXT:
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"text\",\"text\":") ||
            !xllm__json_string(pBuf, pPart->sText ? pPart->sText : "") ||
            !xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        return true;
    case XLLM_PART_IMAGE:
        if (pPart->sSourceUrl && pPart->sSourceUrl[0])
        {
            if (!xllm__buf_append_cstr(
                    pBuf, "{\"type\":\"image\",\"source\":{\"type\":\"url\",\"url\":") ||
                !xllm__json_string(pBuf, pPart->sSourceUrl) || !xllm__buf_append_cstr(pBuf, "}}"))
            {
                return false;
            }
            return true;
        }
        if (!pPart->pData || !pPart->iDataSize)
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "image part has neither bytes nor URL");
            return false;
        }
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"image\",\"source\":") ||
            !xllm__anthropic_append_source_b64(pBuf, pPart) || !xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        return true;
    case XLLM_PART_AUDIO:
        if (!pPart->pData || !pPart->iDataSize)
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "audio part requires bytes");
            return false;
        }
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"audio\",\"source\":") ||
            !xllm__anthropic_append_source_b64(pBuf, pPart) || !xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        return true;
    case XLLM_PART_FILE:
        if (!pPart->pData || !pPart->iDataSize)
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "file part requires bytes");
            return false;
        }
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"document\",\"source\":") ||
            !xllm__anthropic_append_source_b64(pBuf, pPart) || !xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        return true;
    case XLLM_PART_NATIVE:
        return xllm__buf_append_cstr(pBuf, pPart->sText ? pPart->sText : "{}");
    default:
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "message part has an unsupported kind");
        return false;
    }
}

/* Message content: plain string for a single text part, block array else. */
static bool xllm__anthropic_append_content(xllm_buf *pBuf, const xllm_message *pMessage,
                                           bool bToolResult, xllm_error *pError)
{
    size_t i;
    size_t iBlocks = 0u;
    if (pMessage->iPartCount == 0u)
    {
        return xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : "");
    }
    if (pMessage->iPartCount == 1u && pMessage->pParts[0].eKind == XLLM_PART_TEXT && !bToolResult)
    {
        return xllm__json_string(pBuf, pMessage->pParts[0].sText ? pMessage->pParts[0].sText : "");
    }
    for (i = 0u; i < pMessage->iPartCount; ++i)
    {
        if (pMessage->pParts[i].eKind == XLLM_PART_REASONING)
        {
            continue;
        }
        ++iBlocks;
    }
    if (!iBlocks)
    {
        return xllm__json_string(pBuf, "");
    }
    if (!xllm__buf_append_char(pBuf, '['))
    {
        return false;
    }
    {
        bool bFirst = true;
        for (i = 0u; i < pMessage->iPartCount; ++i)
        {
            if (pMessage->pParts[i].eKind == XLLM_PART_REASONING)
            {
                continue;
            }
            if (!bFirst && !xllm__buf_append_char(pBuf, ','))
            {
                return false;
            }
            if (!xllm__anthropic_append_part(pBuf, &pMessage->pParts[i], pError))
            {
                return false;
            }
            bFirst = false;
        }
    }
    return xllm__buf_append_char(pBuf, ']');
}

typedef struct xllm__anthropic_writer
{
    xllm_buf *pBody;
    bool bAnyMessage;
    bool bPendingToolUser;
    xllm_error *pError;
} xllm__anthropic_writer;

static bool xllm__anthropic_message_open(xllm__anthropic_writer *pWriter, const char *sRole)
{
    if (pWriter->bAnyMessage && !xllm__buf_append_char(pWriter->pBody, ','))
    {
        return false;
    }
    if (!xllm__buf_append_cstr(pWriter->pBody, "{\"role\":") ||
        !xllm__json_string(pWriter->pBody, sRole) ||
        !xllm__buf_append_cstr(pWriter->pBody, ",\"content\":"))
    {
        return false;
    }
    pWriter->bAnyMessage = true;
    return true;
}

static bool xllm__anthropic_append_message(xllm__anthropic_writer *pWriter,
                                           const xllm_message *pMessage)
{
    xllm_buf *pBody = pWriter->pBody;
    size_t i;
    if (pMessage->eRole == XLLM_ROLE_TOOL)
    {
        /* Tool results accumulate into a shared user message. */
        if (!pWriter->bPendingToolUser)
        {
            if (!xllm__anthropic_message_open(pWriter, "user") ||
                !xllm__buf_append_char(pBody, '['))
            {
                return false;
            }
            pWriter->bPendingToolUser = true;
        }
        else if (!xllm__buf_append_char(pBody, ','))
        {
            return false;
        }
        if (!xllm__buf_append_cstr(pBody, "{\"type\":\"tool_result\",\"tool_use_id\":") ||
            !xllm__json_string(pBody, pMessage->sToolCallId ? pMessage->sToolCallId : "") ||
            !xllm__buf_append_cstr(pBody, ",\"content\":"))
        {
            return false;
        }
        if (pMessage->iPartCount > 1u ||
            (pMessage->iPartCount == 1u && pMessage->pParts[0].eKind != XLLM_PART_TEXT))
        {
            if (!xllm__anthropic_append_content(pBody, pMessage, true, pWriter->pError) ||
                !xllm__buf_append_char(pBody, '}'))
            {
                return false;
            }
        }
        else if (!xllm__json_string(pBody, pMessage->sContent ? pMessage->sContent : "") ||
                 !xllm__buf_append_char(pBody, '}'))
        {
            return false;
        }
        return true;
    }
    if (pWriter->bPendingToolUser)
    {
        if (!xllm__buf_append_char(pBody, ']') || !xllm__buf_append_char(pBody, '}'))
        {
            return false;
        }
        pWriter->bPendingToolUser = false;
    }
    if (!xllm__anthropic_message_open(pWriter, pMessage->eRole == XLLM_ROLE_ASSISTANT ? "assistant"
                                                                                      : "user"))
    {
        return false;
    }
    if (pMessage->eRole != XLLM_ROLE_ASSISTANT)
    {
        if (!xllm__anthropic_append_content(pBody, pMessage, false, pWriter->pError) ||
            !xllm__buf_append_char(pBody, '}'))
        {
            return false;
        }
        return true;
    }
    /* Assistant: text and tool_use blocks ride one content array. */
    if (!xllm__buf_append_char(pBody, '['))
    {
        return false;
    }
    {
        bool bFirst = true;
        if (pMessage->iPartCount > 0u)
        {
            for (i = 0u; i < pMessage->iPartCount; ++i)
            {
                const xllm_part *pPart = &pMessage->pParts[i];
                if (pPart->eKind == XLLM_PART_REASONING)
                {
                    continue;
                }
                if (xllm__part_is_thinking_signature(pPart))
                {
                    /* New histories pair original reasoning and signature;
                     * retain the joined-view fallback for legacy messages. An
                     * empty thinking string is valid and still needs its sig. */
                    const char *sThinking = pMessage->sReasoningContent;
                    if (i && pMessage->pParts[i - 1u].eKind == XLLM_PART_REASONING)
                        sThinking = pMessage->pParts[i - 1u].sText;
                    if (!bFirst && !xllm__buf_append_char(pBody, ','))
                    {
                        return false;
                    }
                    if (!xllm__buf_append_cstr(pBody, "{\"type\":\"thinking\",\"thinking\":") ||
                        !xllm__json_string(pBody, sThinking) ||
                        !xllm__buf_append_cstr(pBody, ",\"signature\":") ||
                        !xllm__json_string(pBody, pPart->sText ? pPart->sText : "") ||
                        !xllm__buf_append_char(pBody, '}'))
                    {
                        return false;
                    }
                    bFirst = false;
                    continue;
                }
                if (!bFirst && !xllm__buf_append_char(pBody, ','))
                {
                    return false;
                }
                if (!xllm__anthropic_append_part(pBody, pPart, pWriter->pError))
                {
                    return false;
                }
                bFirst = false;
            }
        }
        if (pMessage->sContent && pMessage->sContent[0])
        {
            if (!bFirst && !xllm__buf_append_char(pBody, ','))
            {
                return false;
            }
            if (!xllm__buf_append_cstr(pBody, "{\"type\":\"text\",\"text\":") ||
                !xllm__json_string(pBody, pMessage->sContent) || !xllm__buf_append_char(pBody, '}'))
            {
                return false;
            }
            bFirst = false;
        }
        for (i = 0u; i < pMessage->iToolCallCount; ++i)
        {
            const xllm_tool_call *pCall = &pMessage->pToolCalls[i];
            if (!bFirst && !xllm__buf_append_char(pBody, ','))
            {
                return false;
            }
            if (!xllm__buf_append_cstr(pBody, "{\"type\":\"tool_use\",\"id\":") ||
                !xllm__json_string(pBody, pCall->sId ? pCall->sId : "") ||
                !xllm__buf_append_cstr(pBody, ",\"name\":") ||
                !xllm__json_string(pBody, pCall->sName ? pCall->sName : "") ||
                !xllm__buf_append_cstr(pBody, ",\"input\":") ||
                !xllm__buf_append_cstr(pBody,
                                       pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                !xllm__buf_append_char(pBody, '}'))
            {
                return false;
            }
            bFirst = false;
        }
        if (bFirst && !xllm__buf_append_cstr(pBody, "{\"type\":\"text\",\"text\":\"\"}"))
        {
            return false;
        }
    }
    if (!xllm__buf_append_char(pBody, ']') || !xllm__buf_append_char(pBody, '}'))
    {
        return false;
    }
    return true;
}

static char *xllm__anthropic_build_request(xllm_client *pClient, const xllm_request *pRequest,
                                           xllm_error *pError)
{
    xllm_buf tBody = {0};
    xllm_buf tSystem = {0};
    xllm__anthropic_writer tWriter;
    const char *sModel;
    uint32_t uMaxTokens;
    size_t i;
    char *sResult = NULL;
    sModel = (pRequest->sModel && pRequest->sModel[0]) ? pRequest->sModel : pClient->sModel;
    if (!sModel || !sModel[0])
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "no model is configured");
        return NULL;
    }
    if (pRequest->iMessageCount == 0u)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request has no messages");
        return NULL;
    }
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        if (pMessage->eRole != XLLM_ROLE_SYSTEM)
        {
            continue;
        }
        if (tSystem.iLen && !xllm__buf_append_cstr(&tSystem, "\n\n"))
            goto oom;
        if (pMessage->sContent && !xllm__buf_append_cstr(&tSystem, pMessage->sContent))
            goto oom;
    }
    if (!xllm__buf_append_cstr(&tBody, "{\"model\":") || !xllm__json_string(&tBody, sModel))
        goto oom;
    uMaxTokens =
        pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pClient->uMaxOutputTokens;
    if (!uMaxTokens)
    {
        uMaxTokens = 4096u;
    }
    {
        char sValue[32];
        (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)uMaxTokens);
        if (!xllm__buf_append_cstr(&tBody, ",\"max_tokens\":") ||
            !xllm__buf_append_cstr(&tBody, sValue))
            goto oom;
    }
    if (tSystem.iLen && (!xllm__buf_append_cstr(&tBody, ",\"system\":") ||
                         !xllm__json_string(&tBody, tSystem.pData ? tSystem.pData : "")))
        goto oom;
    if (!xllm__buf_append_cstr(&tBody, ",\"messages\":["))
        goto oom;
    tWriter.pBody = &tBody;
    tWriter.bAnyMessage = false;
    tWriter.bPendingToolUser = false;
    tWriter.pError = pError;
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        if (pMessage->eRole == XLLM_ROLE_SYSTEM)
        {
            continue;
        }
        if (!xllm__anthropic_append_message(&tWriter, pMessage))
        {
            if (pError && pError->eCode == XLLM_ERROR_NONE)
            {
                xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
            }
            goto fail;
        }
    }
    if (tWriter.bPendingToolUser)
    {
        if (!xllm__buf_append_char(&tBody, ']') || !xllm__buf_append_char(&tBody, '}'))
            goto oom;
    }
    if (!xllm__buf_append_char(&tBody, ']'))
        goto oom;
    if (pRequest->iToolCount > 0u)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"tools\":["))
            goto oom;
        for (i = 0u; i < pRequest->iToolCount; ++i)
        {
            const xllm_tool *pTool = &pRequest->pTools[i];
            const char *sSchema = pTool->sParametersJson
                                      ? pTool->sParametersJson
                                      : "{\"type\":\"object\",\"properties\":{}}";
            if (!pTool->sName || !pTool->sName[0] ||
                !xrtJsonValid((xstrview){sSchema, strlen(sSchema)}))
            {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                                "tool has an invalid name or JSON parameter schema");
                goto fail;
            }
            if (i && !xllm__buf_append_char(&tBody, ','))
                goto oom;
            if (!xllm__buf_append_cstr(&tBody, "{\"name\":") ||
                !xllm__json_string(&tBody, pTool->sName) ||
                !xllm__buf_append_cstr(&tBody, ",\"description\":") ||
                !xllm__json_string(&tBody, pTool->sDescription ? pTool->sDescription : "") ||
                !xllm__buf_append_cstr(&tBody, ",\"input_schema\":") ||
                !xllm__buf_append_cstr(&tBody, sSchema) || !xllm__buf_append_char(&tBody, '}'))
                goto oom;
        }
        if (!xllm__buf_append_char(&tBody, ']') ||
            !xllm__buf_append_cstr(&tBody, ",\"tool_choice\":"))
            goto oom;
        switch (pRequest->eToolChoice)
        {
        case XLLM_TOOL_CHOICE_AUTO:
            if (!xllm__buf_append_cstr(&tBody, "{\"type\":\"auto\"}"))
                goto oom;
            break;
        case XLLM_TOOL_CHOICE_NONE:
            if (!xllm__buf_append_cstr(&tBody, "{\"type\":\"none\"}"))
                goto oom;
            break;
        case XLLM_TOOL_CHOICE_REQUIRED:
            if (!xllm__buf_append_cstr(&tBody, "{\"type\":\"any\"}"))
                goto oom;
            break;
        case XLLM_TOOL_CHOICE_NAMED:
            if (!pRequest->sNamedTool || !pRequest->sNamedTool[0])
            {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                                "named tool choice is missing a tool name");
                goto fail;
            }
            if (!xllm__buf_append_cstr(&tBody, "{\"type\":\"tool\",\"name\":") ||
                !xllm__json_string(&tBody, pRequest->sNamedTool) ||
                !xllm__buf_append_char(&tBody, '}'))
                goto oom;
            break;
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid tool choice");
            goto fail;
        }
    }
    if (pRequest->bHasTemperature)
    {
        char sValue[64];
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTemperature);
        if (!xllm__buf_append_cstr(&tBody, ",\"temperature\":") ||
            !xllm__buf_append_cstr(&tBody, sValue))
            goto oom;
    }
    if (pRequest->bHasTopP)
    {
        char sValue[64];
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTopP);
        if (!xllm__buf_append_cstr(&tBody, ",\"top_p\":") || !xllm__buf_append_cstr(&tBody, sValue))
            goto oom;
    }
    if (pRequest->sStop && pRequest->sStop[0])
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"stop_sequences\":[") ||
            !xllm__json_string(&tBody, pRequest->sStop) || !xllm__buf_append_char(&tBody, ']'))
            goto oom;
    }
    if (pRequest->uReasoningBudgetTokens)
    {
        char sValue[32];
        (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)pRequest->uReasoningBudgetTokens);
        if (!xllm__buf_append_cstr(&tBody,
                                   ",\"thinking\":{\"type\":\"enabled\",\"budget_tokens\":") ||
            !xllm__buf_append_cstr(&tBody, sValue) || !xllm__buf_append_char(&tBody, '}'))
            goto oom;
    }
    if (pRequest->sExtraBodyJson && pRequest->sExtraBodyJson[0])
    {
        const char *s = pRequest->sExtraBodyJson;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        {
            ++s;
        }
        if (*s == '{')
        {
            size_t iEnd = strlen(s);
            /* strip trailing whitespace, then exactly one closing brace:
             * nested objects legitimately end with multiple braces. */
            while (iEnd && (s[iEnd - 1u] == ' ' || s[iEnd - 1u] == '\t' || s[iEnd - 1u] == '\r' ||
                            s[iEnd - 1u] == '\n'))
            {
                --iEnd;
            }
            if (iEnd && s[iEnd - 1u] == '}')
            {
                --iEnd;
            }
            if (iEnd > 1u && !xllm__buf_append_cstr(&tBody, ","))
                goto oom;
            if (iEnd > 1u && !xllm__buf_append(&tBody, s + 1u, iEnd - 1u))
                goto oom;
        }
    }
    if (pRequest->bStream)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"stream\":true}"))
            goto oom;
    }
    else if (!xllm__buf_append_char(&tBody, '}'))
    {
        goto oom;
    }
    sResult = xllm__buf_detach(&tBody);
    if (!sResult)
        goto oom;
    xllm__buf_reset(&tSystem);
    return sResult;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
fail:
    xllm__buf_reset(&tBody);
    xllm__buf_reset(&tSystem);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Streaming decode                                                    */
/* ------------------------------------------------------------------ */

static bool xllm__anthropic_usage_in(xllm_call *pCall, xvalue *pUsage)
{
    xllm_response *pResponse = xllm__assemble_ensure(pCall);
    xllm_usage tUsage;
    if (!pResponse)
    {
        return false;
    }
    if (!pUsage)
    {
        return true;
    }
    tUsage = pResponse->tUsage;
    tUsage.uInputTokens = xllm__json_u64(pUsage, "input_tokens");
    tUsage.uCachedInputTokens = xllm__json_u64(pUsage, "cache_read_input_tokens");
    tUsage.uCacheWriteTokens = xllm__json_u64(pUsage, "cache_creation_input_tokens");
    return xllm__assemble_usage(pCall, &tUsage);
}

static bool xllm__anthropic_decode_sse(xllm_call *pCall, const xllm_sse_fields *pFields)
{
    xvalue *pRoot;
    bool bOk = true;
    xstrview tType = {0};
    if (!pCall || !pFields)
    {
        return false;
    }
    if (!pFields->tData.Data || !pFields->tData.Size)
    {
        return true;
    }
    if (!xrtJsonValid(pFields->tData))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pRoot = xrtJsonParse(pFields->tData);
    if (!pRoot)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pCall->bSawEvent = true;
    tType = pFields->tEvent.Size ? pFields->tEvent : xllm__json_text(pRoot, "type");
#define XLLM_IS(name)                                                                              \
    (tType.Size == sizeof(name) - 1u && memcmp(tType.Data, name, sizeof(name) - 1u) == 0)
    if (XLLM_IS("ping") || XLLM_IS("content_block_stop"))
    {
        goto done;
    }
    if (XLLM_IS("message_stop"))
    {
        pCall->bDone = true;
        goto done;
    }
    if (XLLM_IS("error"))
        goto error;
    if (XLLM_IS("message_start"))
    {
        xvalue *pMessage = xllm__json_get(pRoot, "message");
        if (!xllm__anthropic_usage_in(pCall, xllm__json_get(pMessage, "usage")))
        {
            bOk = false;
        }
        goto done;
    }
    if (XLLM_IS("content_block_start"))
    {
        xvalue *pBlock = xllm__json_get(pRoot, "content_block");
        xstrview tBlockType = xllm__json_text(pBlock, "type");
        uint64_t uIndex = xllm__json_u64(pRoot, "index");
        if (tBlockType.Size == 8u && memcmp(tBlockType.Data, "tool_use", 8u) == 0)
        {
            size_t iTool = pCall->pResponse ? pCall->pResponse->iToolCallCount : 0u;
            if (!xllm__assemble_tool(pCall, iTool, xllm__json_text(pBlock, "id"),
                                     xllm__json_text(pBlock, "name"), (xstrview){0}))
            {
                bOk = false;
                goto done;
            }
            if (!xllm__assemble_block_mark_tool(pCall, iTool) ||
                !xllm__assemble_set_block_tool(pCall, (size_t)uIndex, iTool))
            {
                bOk = false;
                goto done;
            }
        }
        goto done;
    }
    if (XLLM_IS("content_block_delta"))
    {
        xvalue *pDelta = xllm__json_get(pRoot, "delta");
        xstrview tDeltaType = xllm__json_text(pDelta, "type");
        uint64_t uIndex = xllm__json_u64(pRoot, "index");
        if (tDeltaType.Size == 10u && memcmp(tDeltaType.Data, "text_delta", 10u) == 0)
        {
            if (!xllm__assemble_text(pCall, XLLM_BLOCK_TEXT, xllm__json_text(pDelta, "text"), NULL))
            {
                bOk = false;
                goto done;
            }
        }
        else if (tDeltaType.Size == 14u && memcmp(tDeltaType.Data, "thinking_delta", 14u) == 0)
        {
            if (!xllm__assemble_text(pCall, XLLM_BLOCK_REASONING,
                                     xllm__json_text(pDelta, "thinking"), NULL))
            {
                bOk = false;
                goto done;
            }
        }
        else if (tDeltaType.Size == 15u && memcmp(tDeltaType.Data, "signature_delta", 15u) == 0)
        {
            if (!xllm__assemble_native(pCall, XLLM_BLOCK_REASONING,
                                       xllm__json_text(pDelta, "signature")))
            {
                bOk = false;
                goto done;
            }
        }
        else if (tDeltaType.Size == 16u && memcmp(tDeltaType.Data, "input_json_delta", 16u) == 0)
        {
            size_t iTool = xllm__assemble_map_block_tool(pCall, (size_t)uIndex);
            if (iTool)
            {
                if (!xllm__assemble_tool(pCall, iTool - 1u, (xstrview){0}, (xstrview){0},
                                         xllm__json_text(pDelta, "partial_json")))
                {
                    bOk = false;
                    goto done;
                }
            }
        }
        goto done;
    }
    if (XLLM_IS("message_delta"))
    {
        xvalue *pDelta = xllm__json_get(pRoot, "delta");
        xvalue *pUsage = xllm__json_get(pRoot, "usage");
        xllm_response *pResponse = xllm__assemble_ensure(pCall);
        xstrview tStop = xllm__json_text(pDelta, "stop_reason");
        if (!pResponse)
        {
            bOk = false;
            goto done;
        }
        if (tStop.Data && !xllm__assemble_finish(pCall, tStop))
        {
            bOk = false;
            goto done;
        }
        if (pUsage)
        {
            xllm_usage tUsage = pResponse->tUsage;
            uint64_t uInput = xllm__json_u64(pUsage, "input_tokens");
            /* Some gateways zero the message_start counters and only report
             * the real input usage here. */
            if (uInput > tUsage.uInputTokens)
            {
                tUsage.uInputTokens = uInput;
            }
            tUsage.uOutputTokens = xllm__json_u64(pUsage, "output_tokens");
            {
                uint64_t uCache = xllm__json_u64(pUsage, "cache_read_input_tokens");
                if (uCache > tUsage.uCachedInputTokens)
                {
                    tUsage.uCachedInputTokens = uCache;
                }
            }
            tUsage.uTotalTokens = tUsage.uInputTokens + tUsage.uOutputTokens;
            if (!xllm__assemble_usage(pCall, &tUsage))
            {
                bOk = false;
                goto done;
            }
        }
        goto done;
    }
#undef XLLM_IS
    goto done;
error:
    {
        xvalue *pError = xllm__json_get(pRoot, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        xstrview tErrorType = xllm__json_text(pError, "type");
        if (tMessage.Data)
            xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                            tMessage);
        if (tErrorType.Data)
            xllm__copy_view(pCall->tError.sProviderType, sizeof(pCall->tError.sProviderType),
                            tErrorType);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned a stream error");
        bOk = false;
    }
done:
    xrtValueRelease(pRoot);
    return bOk;
}

static bool xllm__anthropic_decode_json(xllm_call *pCall, xstrview tBody)
{
    xvalue *pRoot;
    xvalue *pContent;
    xllm_response *pResponse;
    xllm_usage tUsage;
    size_t i;
    bool bOk = true;
    if (!pCall || !tBody.Data || !tBody.Size)
    {
        return false;
    }
    if (!xrtJsonValid(tBody))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    pRoot = xrtJsonParse(tBody);
    if (!pRoot)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    if (xllm__json_get(pRoot, "type"))
    {
        xstrview tType = xllm__json_text(pRoot, "type");
        if (tType.Size == 5u && memcmp(tType.Data, "error", 5u) == 0)
        {
            xvalue *pError = xllm__json_get(pRoot, "error");
            xstrview tMessage = xllm__json_text(pError, "message");
            xstrview tType2 = xllm__json_text(pError, "type");
            if (tMessage.Data)
                xllm__copy_view(pCall->tError.sProviderMessage,
                                sizeof(pCall->tError.sProviderMessage), tMessage);
            if (tType2.Data)
                xllm__copy_view(pCall->tError.sProviderType, sizeof(pCall->tError.sProviderType),
                                tType2);
            xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
            xrtValueRelease(pRoot);
            return false;
        }
    }
    pResponse = xllm__assemble_ensure(pCall);
    if (!pResponse)
    {
        xrtValueRelease(pRoot);
        return false;
    }
    if (!xllm__assemble_metadata(pCall, xllm__json_text(pRoot, "id"),
                                 xllm__json_text(pRoot, "model")))
    {
        xrtValueRelease(pRoot);
        return false;
    }
    pContent = xllm__json_get(pRoot, "content");
    if (pContent && xrtValueType(pContent) == XVALUE_ARRAY)
    {
        size_t uCount = xrtValueCount(pContent);
        for (i = 0u; i < uCount; ++i)
        {
            xvalue *pBlock = xrtValueArrayGet(pContent, i);
            xstrview tType = xllm__json_text(pBlock, "type");
            if (tType.Size == 4u && memcmp(tType.Data, "text", 4u) == 0)
            {
                if (!xllm__assemble_text(pCall, XLLM_BLOCK_TEXT, xllm__json_text(pBlock, "text"),
                                         NULL))
                {
                    bOk = false;
                    break;
                }
            }
            else if (tType.Size == 8u && memcmp(tType.Data, "thinking", 8u) == 0)
            {
                if (!xllm__assemble_text(pCall, XLLM_BLOCK_REASONING,
                                         xllm__json_text(pBlock, "thinking"), NULL))
                {
                    bOk = false;
                    break;
                }
                if (!xllm__assemble_native(pCall, XLLM_BLOCK_REASONING,
                                           xllm__json_text(pBlock, "signature")))
                {
                    bOk = false;
                    break;
                }
            }
            else if (tType.Size == 8u && memcmp(tType.Data, "tool_use", 8u) == 0)
            {
                xvalue *pInput = xllm__json_get(pBlock, "input");
                str sInput = NULL;
                xstrview tInput = {"{}", 2u};
                size_t iTool = pResponse->iToolCallCount;
                if (pInput)
                {
                    sInput = xrtJsonStringify(pInput, false, &tInput.Size);
                    if (!sInput)
                    {
                        xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                                        "failed to serialize Anthropic tool input");
                        bOk = false;
                        break;
                    }
                    tInput.Data = sInput;
                }
                if (!xllm__assemble_tool(pCall, iTool, xllm__json_text(pBlock, "id"),
                                         xllm__json_text(pBlock, "name"), tInput))
                {
                    xrtFree(sInput);
                    bOk = false;
                    break;
                }
                xrtFree(sInput);
                if (!xllm__assemble_block_mark_tool(pCall, iTool))
                {
                    bOk = false;
                    break;
                }
            }
        }
    }
    if (bOk)
    {
        xstrview tStop = xllm__json_text(pRoot, "stop_reason");
        if (tStop.Data && !xllm__assemble_finish(pCall, tStop))
        {
            bOk = false;
        }
        memset(&tUsage, 0, sizeof(tUsage));
        tUsage.uInputTokens = xllm__json_u64(xllm__json_get(pRoot, "usage"), "input_tokens");
        tUsage.uOutputTokens = xllm__json_u64(xllm__json_get(pRoot, "usage"), "output_tokens");
        tUsage.uTotalTokens = tUsage.uInputTokens + tUsage.uOutputTokens;
        tUsage.uCachedInputTokens =
            xllm__json_u64(xllm__json_get(pRoot, "usage"), "cache_read_input_tokens");
        tUsage.uCacheWriteTokens =
            xllm__json_u64(xllm__json_get(pRoot, "usage"), "cache_creation_input_tokens");
        if (!xllm__assemble_usage(pCall, &tUsage))
        {
            bOk = false;
        }
    }
    if (bOk)
    {
        pCall->bSawEvent = true;
    }
    xrtValueRelease(pRoot);
    return bOk;
}

static void xllm__anthropic_fill_error(xllm_call *pCall, xstrview tBody)
{
    xvalue *pRoot;
    if (!pCall || !tBody.Data || !tBody.Size)
    {
        return;
    }
    if (!xrtJsonValid(tBody))
    {
        xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                        tBody);
        return;
    }
    pRoot = xrtJsonParse(tBody);
    if (!pRoot)
    {
        xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                        tBody);
        return;
    }
    {
        xvalue *pError = xllm__json_get(pRoot, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        xstrview tType = xllm__json_text(pError, "type");
        if (!tMessage.Data)
            tMessage = xllm__json_text(pRoot, "message");
        if (tMessage.Data)
            xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                            tMessage);
        if (tType.Data)
            xllm__copy_view(pCall->tError.sProviderType, sizeof(pCall->tError.sProviderType),
                            tType);
        if (!pCall->tError.sMessage[0])
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
        }
    }
    xrtValueRelease(pRoot);
}

static bool xllm__anthropic_is_retryable(const xllm_error *pError)
{
    return pError && strcmp(pError->sProviderType, "overloaded_error") == 0;
}

static const xllm_dialect_ops XLLM_ANTHROPIC_DIALECT = {"anthropic",
                                                        "/v1/messages",
                                                        xllm__anthropic_build_auth,
                                                        xllm__anthropic_build_request,
                                                        NULL,
                                                        xllm__anthropic_decode_sse,
                                                        xllm__anthropic_decode_json,
                                                        xllm__anthropic_fill_error,
                                                        xllm__anthropic_is_retryable};

const xllm_dialect_ops *xllm__dialect_anthropic(void) { return &XLLM_ANTHROPIC_DIALECT; }
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/dialect_responses.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)

/* OpenAI Responses dialect.
 *
 * Request: system messages lift to the top-level instructions field; the
 * conversation rides a typed input item array (message / function_call /
 * function_call_output). Streaming: typed events in the data payload
 * (response.output_text.delta, response.reasoning_summary_text.delta,
 * response.function_call_arguments.delta, response.completed/failed/
 * incomplete). Stateless usage only: previous_response_id and server-side
 * state are deliberately not used. */

static size_t xllm__responses_build_auth(const xllm_client *pClient, xllm_auth_header *pOut,
                                         size_t iCap)
{
    size_t iKeyLen;
    if (!pClient || !pOut || !iCap || !pClient->sApiKey[0])
    {
        return 0u;
    }
    iKeyLen = strlen(pClient->sApiKey);
    pOut[0].sValue = (char *)xllm__malloc(iKeyLen + 8u);
    if (!pOut[0].sValue)
    {
        return 0u;
    }
    memcpy(pOut[0].sValue, "Bearer ", 7u);
    memcpy(pOut[0].sValue + 7u, pClient->sApiKey, iKeyLen + 1u);
    xllm__copy_text(pOut[0].sName, sizeof(pOut[0].sName), "Authorization");
    return 1u;
}

static bool xllm__responses_append_input_image(xllm_buf *pBuf, const xllm_part *pPart,
                                               xllm_error *pError)
{
    if (pPart->sSourceUrl && pPart->sSourceUrl[0])
    {
        if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"input_image\",\"image_url\":") ||
            !xllm__json_string(pBuf, pPart->sSourceUrl) || !xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        return true;
    }
    if (pPart->pData && pPart->iDataSize)
    {
        str sEncoded = xrtBase64EncodeNew(pPart->pData, pPart->iDataSize, NULL);
        bool bOk;
        if (!sEncoded)
        {
            return false;
        }
        bOk = xllm__buf_append_cstr(pBuf, "{\"type\":\"input_image\",\"image_url\":\"data:") &&
              xllm__buf_append_cstr(pBuf, pPart->sMediaType ? pPart->sMediaType : "image/png") &&
              xllm__buf_append_cstr(pBuf, ";base64,") && xllm__buf_append_cstr(pBuf, sEncoded) &&
              xllm__buf_append_cstr(pBuf, "\"}");
        xrtFree(sEncoded);
        return bOk;
    }
    xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "image part has neither bytes nor URL");
    return false;
}

static bool xllm__responses_append_content_part(xllm_buf *pBuf, const xllm_part *pPart,
                                                bool bOutput, xllm_error *pError)
{
    switch (pPart->eKind)
    {
    case XLLM_PART_TEXT:
        if (!xllm__buf_append_cstr(pBuf, bOutput ? "{\"type\":\"output_text\",\"text\":"
                                                 : "{\"type\":\"input_text\",\"text\":") ||
            !xllm__json_string(pBuf, pPart->sText ? pPart->sText : "") ||
            !xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        return true;
    case XLLM_PART_IMAGE:
        return xllm__responses_append_input_image(pBuf, pPart, pError);
    case XLLM_PART_NATIVE:
        return xllm__buf_append_cstr(pBuf, pPart->sText ? pPart->sText : "{}");
    default:
        /* audio/file inputs are not mapped on this dialect yet. */
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "content part kind is not supported by the responses dialect");
        return false;
    }
}

static bool xllm__responses_append_items(xllm_buf *pBuf, const xllm_request *pRequest,
                                         size_t iStartIndex, xllm_error *pError)
{
    size_t i;
    bool bFirst = true;
    for (i = iStartIndex; i < pRequest->iMessageCount; ++i)
    {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        size_t j;
        if (pMessage->eRole == XLLM_ROLE_SYSTEM)
        {
            continue;
        }
        if (!bFirst && !xllm__buf_append_char(pBuf, ','))
        {
            return false;
        }
        bFirst = false;
        if (pMessage->eRole == XLLM_ROLE_TOOL)
        {
            if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"function_call_output\",\"call_id\":") ||
                !xllm__json_string(pBuf, pMessage->sToolCallId ? pMessage->sToolCallId : "") ||
                !xllm__buf_append_cstr(pBuf, ",\"output\":"))
            {
                return false;
            }
            if (pMessage->iPartCount == 1u && pMessage->pParts[0].eKind == XLLM_PART_TEXT)
            {
                if (!xllm__json_string(pBuf, pMessage->pParts[0].sText ? pMessage->pParts[0].sText
                                                                       : "") ||
                    !xllm__buf_append_char(pBuf, '}'))
                {
                    return false;
                }
            }
            else if (pMessage->iPartCount > 0u)
            {
                if (!xllm__buf_append_char(pBuf, '['))
                    return false;
                for (j = 0u; j < pMessage->iPartCount; ++j)
                {
                    if (j && !xllm__buf_append_char(pBuf, ','))
                        return false;
                    if (!xllm__responses_append_content_part(pBuf, &pMessage->pParts[j], false,
                                                             pError))
                        return false;
                }
                if (!xllm__buf_append_cstr(pBuf, "]}"))
                    return false;
            }
            else if (!xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : "") ||
                     !xllm__buf_append_char(pBuf, '}'))
            {
                return false;
            }
            continue;
        }
        if (pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u &&
            (!pMessage->sContent || !pMessage->sContent[0]) && pMessage->iPartCount == 0u)
        {
            for (j = 0u; j < pMessage->iToolCallCount; ++j)
            {
                const xllm_tool_call *pCall = &pMessage->pToolCalls[j];
                if (j && !xllm__buf_append_char(pBuf, ','))
                {
                    return false;
                }
                if (!xllm__buf_append_cstr(pBuf, "{\"type\":\"function_call\",\"call_id\":") ||
                    !xllm__json_string(pBuf, pCall->sId ? pCall->sId : "") ||
                    !xllm__buf_append_cstr(pBuf, ",\"name\":") ||
                    !xllm__json_string(pBuf, pCall->sName ? pCall->sName : "") ||
                    !xllm__buf_append_cstr(pBuf, ",\"arguments\":") ||
                    !xllm__json_string(pBuf,
                                       pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                    !xllm__buf_append_char(pBuf, '}'))
                {
                    return false;
                }
            }
            continue;
        }
        size_t iUsable = 0u;
        bool bArray = false;
        for (j = 0u; j < pMessage->iPartCount; ++j)
        {
            if (pMessage->pParts[j].eKind == XLLM_PART_REASONING ||
                xllm__part_is_thinking_signature(&pMessage->pParts[j]))
            {
                continue;
            }
            ++iUsable;
            if (pMessage->pParts[j].eKind != XLLM_PART_TEXT)
            {
                bArray = true;
            }
        }
        /* An explicit assistant message type selects ResponseOutputMessage in
         * strict Responses servers, whose content must be an output-part array.
         * String history uses EasyInputMessage instead: omit the discriminator
         * rather than mixing the two schemas. Keep structured items typed. */
        bool bAssistantText = pMessage->eRole == XLLM_ROLE_ASSISTANT &&
                              (pMessage->iPartCount == 0u || (iUsable <= 1u && !bArray));
        if (!xllm__buf_append_cstr(pBuf, bAssistantText ? "{\"role\":"
                                                        : "{\"type\":\"message\",\"role\":") ||
            !xllm__json_string(pBuf,
                               pMessage->eRole == XLLM_ROLE_ASSISTANT ? "assistant" : "user") ||
            !xllm__buf_append_cstr(pBuf, ",\"content\":"))
        {
            return false;
        }
        if (pMessage->iPartCount > 0u)
        {
            if (iUsable <= 1u && !bArray)
            {
                const char *sText = NULL;
                for (j = 0u; j < pMessage->iPartCount; ++j)
                {
                    if (pMessage->pParts[j].eKind == XLLM_PART_REASONING ||
                        xllm__part_is_thinking_signature(&pMessage->pParts[j]))
                    {
                        continue;
                    }
                    sText = pMessage->pParts[j].sText;
                }
                if (!xllm__json_string(pBuf, sText ? sText : ""))
                {
                    return false;
                }
            }
            else
            {
                if (!xllm__buf_append_char(pBuf, '['))
                {
                    return false;
                }
                {
                    bool bPartFirst = true;
                    for (j = 0u; j < pMessage->iPartCount; ++j)
                    {
                        if (pMessage->pParts[j].eKind == XLLM_PART_REASONING ||
                            xllm__part_is_thinking_signature(&pMessage->pParts[j]))
                        {
                            continue;
                        }
                        if (!bPartFirst && !xllm__buf_append_char(pBuf, ','))
                        {
                            return false;
                        }
                        if (!xllm__responses_append_content_part(
                                pBuf, &pMessage->pParts[j], pMessage->eRole == XLLM_ROLE_ASSISTANT,
                                pError))
                        {
                            return false;
                        }
                        bPartFirst = false;
                    }
                }
                if (!xllm__buf_append_char(pBuf, ']'))
                {
                    return false;
                }
            }
        }
        else if (!xllm__json_string(pBuf, pMessage->sContent ? pMessage->sContent : ""))
        {
            return false;
        }
        if (!xllm__buf_append_char(pBuf, '}'))
        {
            return false;
        }
        if (pMessage->eRole == XLLM_ROLE_ASSISTANT && pMessage->iToolCallCount > 0u)
        {
            for (j = 0u; j < pMessage->iToolCallCount; ++j)
            {
                const xllm_tool_call *pCall = &pMessage->pToolCalls[j];
                if (!xllm__buf_append_char(pBuf, ',') ||
                    !xllm__buf_append_cstr(pBuf, "{\"type\":\"function_call\",\"call_id\":") ||
                    !xllm__json_string(pBuf, pCall->sId ? pCall->sId : "") ||
                    !xllm__buf_append_cstr(pBuf, ",\"name\":") ||
                    !xllm__json_string(pBuf, pCall->sName ? pCall->sName : "") ||
                    !xllm__buf_append_cstr(pBuf, ",\"arguments\":") ||
                    !xllm__json_string(pBuf,
                                       pCall->sArgumentsJson ? pCall->sArgumentsJson : "{}") ||
                    !xllm__buf_append_char(pBuf, '}'))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

static char *xllm__responses_build_request(xllm_client *pClient, const xllm_request *pRequest,
                                           xllm_error *pError)
{
    xllm_buf tBody = {0};
    xllm_buf tSystem = {0};
    const char *sModel;
    const char *sEffort;
    uint32_t uMaxTokens;
    size_t i;
    char sValue[64];
    char *sResult = NULL;
    sModel = (pRequest->sModel && pRequest->sModel[0]) ? pRequest->sModel : pClient->sModel;
    if (!sModel || !sModel[0])
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "no model is configured");
        return NULL;
    }
    if (pRequest->iMessageCount == 0u)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request has no messages");
        return NULL;
    }
    for (i = 0u; i < pRequest->iMessageCount; ++i)
    {
        const xllm_message *pMessage = &pRequest->pMessages[i];
        if (pMessage->eRole != XLLM_ROLE_SYSTEM)
        {
            continue;
        }
        if (tSystem.iLen && !xllm__buf_append_cstr(&tSystem, "\n\n"))
            goto oom;
        if (pMessage->sContent && !xllm__buf_append_cstr(&tSystem, pMessage->sContent))
            goto oom;
    }
    if (!xllm__buf_append_cstr(&tBody, "{\"model\":") || !xllm__json_string(&tBody, sModel))
        goto oom;
    if (tSystem.iLen && (!xllm__buf_append_cstr(&tBody, ",\"instructions\":") ||
                         !xllm__json_string(&tBody, tSystem.pData ? tSystem.pData : "")))
        goto oom;
    /* Wire alignment (pi behavior): never persist this exchange server-side;
     * a caller-provided extraBody "store" key wins (see completions). */
    if (!pRequest->sExtraBodyJson || strstr(pRequest->sExtraBodyJson, "\"store\"") == NULL)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"store\":false"))
            goto oom;
    }
    uMaxTokens =
        pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pClient->uMaxOutputTokens;
    if (uMaxTokens)
    {
        (void)snprintf(sValue, sizeof(sValue), "%u", (unsigned)uMaxTokens);
        if (!xllm__buf_append_cstr(&tBody, ",\"max_output_tokens\":") ||
            !xllm__buf_append_cstr(&tBody, sValue))
            goto oom;
    }
    if (pRequest->bHasTemperature)
    {
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTemperature);
        if (!xllm__buf_append_cstr(&tBody, ",\"temperature\":") ||
            !xllm__buf_append_cstr(&tBody, sValue))
            goto oom;
    }
    if (pRequest->bHasTopP)
    {
        (void)snprintf(sValue, sizeof(sValue), "%.17g", pRequest->fTopP);
        if (!xllm__buf_append_cstr(&tBody, ",\"top_p\":") || !xllm__buf_append_cstr(&tBody, sValue))
            goto oom;
    }
    sEffort = (pRequest->sReasoningEffort && pRequest->sReasoningEffort[0])
                  ? pRequest->sReasoningEffort
                  : pClient->sReasoningEffort;
    if (sEffort && sEffort[0] && strcmp(sEffort, "off") != 0)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"reasoning\":{\"effort\":") ||
            !xllm__json_string(&tBody, sEffort) || !xllm__buf_append_char(&tBody, '}'))
            goto oom;
    }
    if (pRequest->eJsonMode == XLLM_JSON_OBJECT)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"text\":{\"format\":{\"type\":\"json_object\"}}"))
            goto oom;
    }
    if (pRequest->iToolCount > 0u)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"tools\":["))
            goto oom;
        for (i = 0u; i < pRequest->iToolCount; ++i)
        {
            const xllm_tool *pTool = &pRequest->pTools[i];
            const char *sSchema = pTool->sParametersJson
                                      ? pTool->sParametersJson
                                      : "{\"type\":\"object\",\"properties\":{}}";
            if (!pTool->sName || !pTool->sName[0] ||
                !xrtJsonValid((xstrview){sSchema, strlen(sSchema)}))
            {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                                "tool has an invalid name or JSON parameter schema");
                goto fail;
            }
            if (i && !xllm__buf_append_char(&tBody, ','))
                goto oom;
            if (!xllm__buf_append_cstr(&tBody, "{\"type\":\"function\",\"name\":") ||
                !xllm__json_string(&tBody, pTool->sName) ||
                !xllm__buf_append_cstr(&tBody, ",\"description\":") ||
                !xllm__json_string(&tBody, pTool->sDescription ? pTool->sDescription : "") ||
                !xllm__buf_append_cstr(&tBody, ",\"parameters\":") ||
                !xllm__buf_append_cstr(&tBody, sSchema))
                goto oom;
            /* Preserve the caller's mode instead of requesting implicit strict normalization. */
            if (!xllm__buf_append_cstr(&tBody,
                                       pTool->bStrict ? ",\"strict\":true" : ",\"strict\":false"))
                goto oom;
            if (!xllm__buf_append_char(&tBody, '}'))
                goto oom;
        }
        if (!xllm__buf_append_char(&tBody, ']') ||
            !xllm__buf_append_cstr(&tBody, ",\"tool_choice\":"))
            goto oom;
        switch (pRequest->eToolChoice)
        {
        case XLLM_TOOL_CHOICE_AUTO:
            if (!xllm__json_string(&tBody, "auto"))
                goto oom;
            break;
        case XLLM_TOOL_CHOICE_NONE:
            if (!xllm__json_string(&tBody, "none"))
                goto oom;
            break;
        case XLLM_TOOL_CHOICE_REQUIRED:
            if (!xllm__json_string(&tBody, "required"))
                goto oom;
            break;
        case XLLM_TOOL_CHOICE_NAMED:
            if (!pRequest->sNamedTool || !pRequest->sNamedTool[0])
            {
                xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                                "named tool choice is missing a tool name");
                goto fail;
            }
            if (!xllm__buf_append_cstr(&tBody, "{\"type\":\"function\",\"name\":") ||
                !xllm__json_string(&tBody, pRequest->sNamedTool) ||
                !xllm__buf_append_char(&tBody, '}'))
                goto oom;
            break;
        default:
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "invalid tool choice");
            goto fail;
        }
    }
    if (!xllm__buf_append_cstr(&tBody, ",\"input\":") || !xllm__buf_append_char(&tBody, '[') ||
        !xllm__responses_append_items(&tBody, pRequest, 0u, pError) ||
        !xllm__buf_append_char(&tBody, ']'))
    {
        if (pError && pError->eCode == XLLM_ERROR_NONE)
        {
            xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
        }
        goto fail;
    }
    if (pRequest->sExtraBodyJson && pRequest->sExtraBodyJson[0])
    {
        const char *s = pRequest->sExtraBodyJson;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        {
            ++s;
        }
        if (*s == '{')
        {
            size_t iEnd = strlen(s);
            /* strip trailing whitespace, then exactly one closing brace:
             * nested objects legitimately end with multiple braces. */
            while (iEnd && (s[iEnd - 1u] == ' ' || s[iEnd - 1u] == '\t' || s[iEnd - 1u] == '\r' ||
                            s[iEnd - 1u] == '\n'))
            {
                --iEnd;
            }
            if (iEnd && s[iEnd - 1u] == '}')
            {
                --iEnd;
            }
            if (iEnd > 1u && !xllm__buf_append_cstr(&tBody, ","))
                goto oom;
            if (iEnd > 1u && !xllm__buf_append(&tBody, s + 1u, iEnd - 1u))
                goto oom;
        }
    }
    if (pRequest->bStream)
    {
        if (!xllm__buf_append_cstr(&tBody, ",\"stream\":true}"))
            goto oom;
    }
    else if (!xllm__buf_append_char(&tBody, '}'))
    {
        goto oom;
    }
    sResult = xllm__buf_detach(&tBody);
    if (!sResult)
        goto oom;
    xllm__buf_reset(&tSystem);
    return sResult;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to build request JSON");
fail:
    xllm__buf_reset(&tBody);
    xllm__buf_reset(&tSystem);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Streaming / body decode                                             */
/* ------------------------------------------------------------------ */

static void xllm__responses_fill_usage(xllm_usage *pUsage, xvalue *pUsageObject)
{
    if (!pUsageObject || xrtValueType(pUsageObject) != XVALUE_OBJECT)
    {
        return;
    }
    pUsage->uInputTokens = xllm__json_u64(pUsageObject, "input_tokens");
    pUsage->uOutputTokens = xllm__json_u64(pUsageObject, "output_tokens");
    pUsage->uTotalTokens = xllm__json_u64(pUsageObject, "total_tokens");
    {
        xvalue *pDetails = xllm__json_get(pUsageObject, "input_tokens_details");
        pUsage->uCachedInputTokens = xllm__json_u64(pDetails, "cached_tokens");
    }
    {
        xvalue *pDetails = xllm__json_get(pUsageObject, "output_tokens_details");
        pUsage->uReasoningTokens = xllm__json_u64(pDetails, "reasoning_tokens");
    }
}

static bool xllm__responses_decode_sse(xllm_call *pCall, const xllm_sse_fields *pFields)
{
    xvalue *pRoot;
    xstrview tType;
    bool bOk = true;
    if (!pCall || !pFields)
    {
        return false;
    }
    if (!pFields->tData.Data || !pFields->tData.Size)
    {
        return true;
    }
    if (!xrtJsonValid(pFields->tData))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pRoot = xrtJsonParse(pFields->tData);
    if (!pRoot)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON in provider event stream");
        return false;
    }
    pCall->bSawEvent = true;
    tType = xllm__json_text(pRoot, "type");
#define XLLM_IS(name)                                                                              \
    (tType.Size == sizeof(name) - 1u && memcmp(tType.Data, name, sizeof(name) - 1u) == 0)
    if (XLLM_IS("response.output_text.delta"))
    {
        if (!xllm__assemble_text(pCall, XLLM_BLOCK_TEXT, xllm__json_text(pRoot, "delta"), NULL))
        {
            bOk = false;
        }
    }
    else if (XLLM_IS("response.reasoning_summary_text.delta") ||
             XLLM_IS("response.reasoning_text.delta"))
    {
        if (!xllm__assemble_text(pCall, XLLM_BLOCK_REASONING, xllm__json_text(pRoot, "delta"),
                                 NULL))
        {
            bOk = false;
        }
    }
    else if (XLLM_IS("response.output_item.added"))
    {
        xvalue *pItem = xllm__json_get(pRoot, "item");
        xstrview tItemType = xllm__json_text(pItem, "type");
        xstrview tItemId = xllm__json_text(pItem, "id");
        if (tItemType.Size == 13u && memcmp(tItemType.Data, "function_call", 13u) == 0)
        {
            xstrview tCallId = xllm__json_text(pItem, "call_id");
            xstrview tName = xllm__json_text(pItem, "name");
            char sId[80];
            size_t iTool;
            if (!tCallId.Data && tItemId.Data && tItemId.Size < sizeof(sId))
            {
                memcpy(sId, tItemId.Data, tItemId.Size);
                sId[tItemId.Size] = 0;
                tCallId.Data = sId;
                tCallId.Size = tItemId.Size;
            }
            iTool = xllm__assemble_add_item_tool(
                pCall, tItemId.Data ? (const char *)tItemId.Data : "", tCallId, tName);
            if (iTool == (size_t)-1 || !xllm__assemble_block_mark_tool(pCall, iTool))
            {
                bOk = false;
            }
        }
    }
    else if (XLLM_IS("response.function_call_arguments.delta"))
    {
        char sId[80];
        size_t iTool;
        xstrview tItemId = xllm__json_text(pRoot, "item_id");
        if (tItemId.Data && tItemId.Size < sizeof(sId))
        {
            memcpy(sId, tItemId.Data, tItemId.Size);
            sId[tItemId.Size] = 0;
            iTool = xllm__assemble_find_item_tool(pCall, sId);
            if (iTool != (size_t)-1 &&
                !xllm__assemble_tool(pCall, iTool, (xstrview){0}, (xstrview){0},
                                     xllm__json_text(pRoot, "delta")))
            {
                bOk = false;
            }
        }
    }
    else if (XLLM_IS("response.completed"))
    {
        xvalue *pResponse = xllm__json_get(pRoot, "response");
        xllm_usage tUsage;
        memset(&tUsage, 0, sizeof(tUsage));
        xllm__responses_fill_usage(&tUsage, xllm__json_get(pResponse, "usage"));
        if (tUsage.uTotalTokens || tUsage.uInputTokens || tUsage.uOutputTokens)
        {
            if (!xllm__assemble_usage(pCall, &tUsage))
            {
                bOk = false;
            }
        }
        pCall->bDone = true;
    }
    else if (XLLM_IS("response.incomplete"))
    {
        xvalue *pResponse = xllm__json_get(pRoot, "response");
        xllm_usage tUsage;
        memset(&tUsage, 0, sizeof(tUsage));
        xllm__responses_fill_usage(&tUsage, xllm__json_get(pResponse, "usage"));
        if (tUsage.uTotalTokens || tUsage.uInputTokens || tUsage.uOutputTokens)
        {
            if (!xllm__assemble_usage(pCall, &tUsage))
            {
                bOk = false;
            }
        }
        xvalue *pDetails = xllm__json_get(pResponse, "incomplete_details");
        xstrview tReason = xllm__json_text(pDetails, "reason");
        if (!xllm__assemble_finish(pCall,
                                   tReason.Data ? tReason : (xstrview){"max_output_tokens", 17u}))
        {
            bOk = false;
        }
        pCall->bDone = true;
    }
    else if (XLLM_IS("response.failed"))
    {
        xvalue *pResponse = xllm__json_get(pRoot, "response");
        xvalue *pError = xllm__json_get(pResponse, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        xstrview tCode = xllm__json_text(pError, "code");
        if (tMessage.Data)
            xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                            tMessage);
        if (tCode.Data)
            xllm__copy_view(pCall->tError.sProviderCode, sizeof(pCall->tError.sProviderCode),
                            tCode);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider reported a failed response");
        bOk = false;
    }
    else if (XLLM_IS("error"))
    {
        xstrview tMessage = xllm__json_text(pRoot, "message");
        xstrview tCode = xllm__json_text(pRoot, "code");
        if (tMessage.Data)
            xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                            tMessage);
        if (tCode.Data)
            xllm__copy_view(pCall->tError.sProviderCode, sizeof(pCall->tError.sProviderCode),
                            tCode);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned a stream error");
        bOk = false;
    }
#undef XLLM_IS
    xrtValueRelease(pRoot);
    return bOk;
}

static bool xllm__responses_decode_json(xllm_call *pCall, xstrview tBody)
{
    xvalue *pRoot;
    xvalue *pOutput;
    xllm_response *pResponse;
    xllm_usage tUsage;
    bool bOk = true;
    if (!pCall || !tBody.Data || !tBody.Size)
    {
        return false;
    }
    if (!xrtJsonValid(tBody))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    pRoot = xrtJsonParse(tBody);
    if (!pRoot)
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_PARSE, "invalid JSON provider response");
        return false;
    }
    if (xllm__json_get(pRoot, "error"))
    {
        xvalue *pError = xllm__json_get(pRoot, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        if (tMessage.Data)
            xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                            tMessage);
        xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
        xrtValueRelease(pRoot);
        return false;
    }
    pResponse = xllm__assemble_ensure(pCall);
    if (!pResponse)
    {
        xrtValueRelease(pRoot);
        return false;
    }
    if (!xllm__assemble_metadata(pCall, xllm__json_text(pRoot, "id"),
                                 xllm__json_text(pRoot, "model")))
    {
        xrtValueRelease(pRoot);
        return false;
    }
    pOutput = xllm__json_get(pRoot, "output");
    if (pOutput && xrtValueType(pOutput) == XVALUE_ARRAY)
    {
        size_t uCount = xrtValueCount(pOutput);
        size_t i;
        for (i = 0u; i < uCount && bOk; ++i)
        {
            xvalue *pItem = xrtValueArrayGet(pOutput, i);
            xstrview tType = xllm__json_text(pItem, "type");
            if (tType.Size == 7u && memcmp(tType.Data, "message", 7u) == 0)
            {
                xvalue *pContent = xllm__json_get(pItem, "content");
                if (pContent && xrtValueType(pContent) == XVALUE_ARRAY)
                {
                    size_t uBlocks = xrtValueCount(pContent);
                    size_t j;
                    for (j = 0u; j < uBlocks && bOk; ++j)
                    {
                        xvalue *pBlock = xrtValueArrayGet(pContent, j);
                        xstrview tBlockType = xllm__json_text(pBlock, "type");
                        if (tBlockType.Size == 11u &&
                            memcmp(tBlockType.Data, "output_text", 11u) == 0)
                        {
                            if (!xllm__assemble_text(pCall, XLLM_BLOCK_TEXT,
                                                     xllm__json_text(pBlock, "text"), NULL))
                            {
                                bOk = false;
                            }
                        }
                    }
                }
            }
            else if (tType.Size == 13u && memcmp(tType.Data, "function_call", 13u) == 0)
            {
                if (!xllm__assemble_tool(
                        pCall, pResponse->iToolCallCount, xllm__json_text(pItem, "call_id"),
                        xllm__json_text(pItem, "name"), xllm__json_text(pItem, "arguments")))
                {
                    bOk = false;
                }
                else if (!xllm__assemble_block_mark_tool(pCall, pResponse->iToolCallCount - 1u))
                {
                    bOk = false;
                }
            }
            else if (tType.Size == 9u && memcmp(tType.Data, "reasoning", 9u) == 0)
            {
                xvalue *pSummary = xllm__json_get(pItem, "summary");
                if (pSummary && xrtValueType(pSummary) == XVALUE_ARRAY)
                {
                    size_t uBlocks = xrtValueCount(pSummary);
                    size_t j;
                    for (j = 0u; j < uBlocks && bOk; ++j)
                    {
                        xvalue *pBlock = xrtValueArrayGet(pSummary, j);
                        if (!xllm__assemble_text(pCall, XLLM_BLOCK_REASONING,
                                                 xllm__json_text(pBlock, "text"), NULL))
                        {
                            bOk = false;
                        }
                    }
                }
            }
        }
    }
    if (bOk)
    {
        xstrview tStatus = xllm__json_text(pRoot, "status");
        if (tStatus.Data && !xllm__assemble_finish(pCall, tStatus))
        {
            bOk = false;
        }
        memset(&tUsage, 0, sizeof(tUsage));
        xllm__responses_fill_usage(&tUsage, xllm__json_get(pRoot, "usage"));
        if (!xllm__assemble_usage(pCall, &tUsage))
        {
            bOk = false;
        }
    }
    if (bOk)
    {
        pCall->bSawEvent = true;
    }
    xrtValueRelease(pRoot);
    return bOk;
}

static void xllm__responses_fill_error(xllm_call *pCall, xstrview tBody)
{
    xvalue *pRoot;
    if (!pCall || !tBody.Data || !tBody.Size)
    {
        return;
    }
    if (!xrtJsonValid(tBody))
    {
        xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                        tBody);
        return;
    }
    pRoot = xrtJsonParse(tBody);
    if (!pRoot)
    {
        xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                        tBody);
        return;
    }
    {
        xvalue *pError = xllm__json_get(pRoot, "error");
        xstrview tMessage = xllm__json_text(pError, "message");
        xstrview tCode = xllm__json_text(pError, "code");
        xstrview tType = xllm__json_text(pError, "type");
        if (!tMessage.Data)
            tMessage = xllm__json_text(pRoot, "message");
        if (tMessage.Data)
            xllm__copy_view(pCall->tError.sProviderMessage, sizeof(pCall->tError.sProviderMessage),
                            tMessage);
        if (tCode.Data)
            xllm__copy_view(pCall->tError.sProviderCode, sizeof(pCall->tError.sProviderCode),
                            tCode);
        if (tType.Data)
            xllm__copy_view(pCall->tError.sProviderType, sizeof(pCall->tError.sProviderType),
                            tType);
        if (!pCall->tError.sMessage[0])
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_UPSTREAM, "provider returned an error");
        }
    }
    xrtValueRelease(pRoot);
}

static const xllm_dialect_ops XLLM_RESPONSES_DIALECT = {"responses",
                                                        "/responses",
                                                        xllm__responses_build_auth,
                                                        xllm__responses_build_request,
                                                        NULL,
                                                        xllm__responses_decode_sse,
                                                        xllm__responses_decode_json,
                                                        xllm__responses_fill_error,
                                                        NULL};

const xllm_dialect_ops *xllm__dialect_responses(void) { return &XLLM_RESPONSES_DIALECT; }
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_transport.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)
#include <stdio.h>

/* Engine-driven transport.
 *
 * The exchange runs as a watch-chained state machine on the client engine's
 * workers: dial -> send (header, body) -> read (head + body pumped through
 * the incremental HTTP/1.1 parser). Every asynchronous operation future gets
 * the call's embedded watch; its notify callback advances the machine. A
 * per-call engine timer enforces the deadline and a cancel watch aborts the
 * streams, so no thread is ever dedicated to a call. The call promise
 * reaches its terminal state exactly once from xllm__transport_finish. */

struct xllm_connection
{
    bool bTls;
    xnetstream *pTcp;
    xtlsstream *pTls;
    xfuture *pIdleRead;
    uint64_t uPooledAtMs;
};

/* Sentinel returned by step helpers: continue to the next step inline
 * instead of arming a watch on a new pending future. */
#define XLLM_STEP_CONTINUE ((xfuture *)1)

static xstrview xllm__sv(const char *sText)
{
    xstrview tView;
    tView.Data = sText ? sText : "";
    tView.Size = sText ? strlen(sText) : 0u;
    return tView;
}

static xbytesview xllm__bv(const void *pData, size_t iSize)
{
    xbytesview tView;
    tView.Data = (const uint8 *)pData;
    tView.Size = iSize;
    return tView;
}

static uint64_t xllm__clock_ms(void) { return xllmClockUs() / UINT64_C(1000); }

static xllm_error_stage xllm__transport_stage(const char *sPhase, const char *sName)
{
    if ((sName && strstr(sName, "proxy")) || (sPhase && strstr(sPhase, "proxy")))
        return XLLM_ERROR_STAGE_PROXY;
    if ((sName && strstr(sName, "tls")) || (sPhase && strstr(sPhase, "tls")))
        return XLLM_ERROR_STAGE_TLS;
    if (sPhase && (strcmp(sPhase, "resolve") == 0 || strcmp(sPhase, "dns") == 0))
        return XLLM_ERROR_STAGE_RESOLVE;
    if (sPhase && strcmp(sPhase, "connect") == 0)
        return XLLM_ERROR_STAGE_CONNECT;
    if (sPhase && (strcmp(sPhase, "send") == 0 || strcmp(sPhase, "request") == 0))
        return XLLM_ERROR_STAGE_WRITE;
    if (sPhase && (strcmp(sPhase, "receive") == 0 || strcmp(sPhase, "body") == 0))
        return XLLM_ERROR_STAGE_READ;
    if (sPhase && strcmp(sPhase, "headers") == 0)
        return XLLM_ERROR_STAGE_HTTP;
    return XLLM_ERROR_STAGE_WAIT;
}

static void xllm__transport_fail(xllm_call *pCall, const char *sPhase,
                                 xllm_transport_result eResult, const char *sName,
                                 const xerror *pError)
{
    const char *sOperation;
    const char *sMessage;
    if (!pCall)
        return;
    pCall->tHttpDiagnostics.eResult = eResult;
    xllm__copy_text(pCall->tHttpDiagnostics.sPhase, sizeof(pCall->tHttpDiagnostics.sPhase), sPhase);
    xllm__copy_text(pCall->tHttpDiagnostics.sError, sizeof(pCall->tHttpDiagnostics.sError), sName);
    if (pError)
    {
        pCall->tHttpDiagnostics.iSystemError = xrtErrorSystemCode(pError);
    }
    /* A parser/hook/limit failure may make the body consumer stop the
     * transport. Preserve that primary cause instead of replacing it with
     * the secondary callback cancellation. */
    if (pCall->tError.tCause.eDomain != XLLM_ERROR_DOMAIN_NONE &&
        pCall->tError.tCause.eDomain != XLLM_ERROR_DOMAIN_TRANSPORT)
    {
        return;
    }
    sOperation = pError ? xrtErrorOperation(pError) : NULL;
    if (!sOperation || !sOperation[0])
    {
        sOperation = sPhase;
    }
    sMessage = pError ? xrtErrorMessage(pError) : NULL;
    if (!sMessage || !sMessage[0])
    {
        sMessage = sName;
    }
    xllm__error_cause_set(
        &pCall->tError, XLLM_ERROR_DOMAIN_TRANSPORT, xllm__transport_stage(sPhase, sName),
        pError ? (int32_t)xrtErrorKind(pError) : 0, pError ? xrtErrorCode(pError) : 0,
        pError ? xrtErrorSystemCode(pError) : 0, sOperation, sMessage);
}

static void xllm__connection_close(xllm_connection *pConnection)
{
    if (!pConnection)
        return;
    if (pConnection->pIdleRead)
    {
        (void)xrtFutureCancel(pConnection->pIdleRead);
        xrtFutureDestroy(pConnection->pIdleRead);
        pConnection->pIdleRead = NULL;
    }
    if (pConnection->bTls && pConnection->pTls)
    {
        xfuture *pClose;
        (void)xrtTlsStreamAbort(pConnection->pTls);
        pClose = xrtTlsStreamWaitAsync(pConnection->pTls, XTLS_STREAM_WAIT_CLOSE);
        if (pClose)
        {
            (void)xrtFutureWaitFor(pClose, 1000);
            xrtFutureDestroy(pClose);
        }
        xrtTlsStreamDestroy(pConnection->pTls);
    }
    else if (pConnection->pTcp)
    {
        (void)xrtNetStreamAbort(pConnection->pTcp);
        (void)xrtNetStreamWait(pConnection->pTcp, XNET_STREAM_WAIT_CLOSE, 1000, NULL);
        xrtNetStreamDestroy(pConnection->pTcp);
    }
    xllm__free(pConnection);
}

static bool xllm__connection_open(const xllm_connection *pConnection)
{
    if (!pConnection)
        return false;
    if (pConnection->bTls)
    {
        return pConnection->pTls && xrtTlsStreamState(pConnection->pTls) == XTLS_STREAM_OPEN;
    }
    return pConnection->pTcp && xrtNetStreamState(pConnection->pTcp) == XNET_STREAM_OPEN;
}

static bool xllm__connection_idle_expired(const xllm_client *pClient,
                                          const xllm_connection *pConnection, uint64_t uNowMs)
{
    return pClient->uIdleTimeoutMs != 0u && pConnection->uPooledAtMs != 0u &&
           uNowMs >= pConnection->uPooledAtMs &&
           uNowMs - pConnection->uPooledAtMs >= pClient->uIdleTimeoutMs;
}

static bool xllm__connection_idle_arm(xllm_connection *pConnection)
{
    if (!xllm__connection_open(pConnection) || pConnection->pIdleRead)
    {
        return false;
    }
    pConnection->pIdleRead = pConnection->bTls
                                 ? xrtTlsStreamWaitAsync(pConnection->pTls, XTLS_STREAM_WAIT_READ)
                                 : xrtNetStreamWaitAsync(pConnection->pTcp, XNET_STREAM_WAIT_READ);
    return pConnection->pIdleRead != NULL;
}

/* Stop the idle read sentinel before handing the stream to a request. If a
 * peer-close/read event won the race, the connection is contaminated and is
 * discarded. Cancellation normally resolves inline; the bounded wait covers
 * a worker that is finishing publication. */
static bool xllm__connection_idle_stop(xllm_connection *pConnection)
{
    xfuture *pIdle;
    xfuturestate eState;
    bool bQuiet;
    if (!pConnection || !(pIdle = pConnection->pIdleRead))
        return false;
    pConnection->pIdleRead = NULL;
    if (xrtFutureState(pIdle) == XFUTURE_PENDING)
    {
        (void)xrtFutureCancel(pIdle);
        (void)xrtFutureWaitFor(pIdle, 1000);
    }
    eState = xrtFutureState(pIdle);
    bQuiet = eState == XFUTURE_CANCELLED;
    xrtFutureDestroy(pIdle);
    return bQuiet;
}

static xllm_connection *xllm__connection_take(xllm_client *pClient)
{
    xllm_connection *pConnection;
    uint64_t uNowMs = xllm__clock_ms();
    if (!pClient || !pClient->pConnectionMutex)
        return NULL;
    for (;;)
    {
        pConnection = NULL;
        if (!xrtMutexLock(pClient->pConnectionMutex))
            return NULL;
        if (pClient->uIdleConnectionCount)
        {
            pConnection = pClient->pIdleConnections[--pClient->uIdleConnectionCount];
            pClient->pIdleConnections[pClient->uIdleConnectionCount] = NULL;
        }
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
        if (!pConnection)
            return NULL;
        if (xllm__connection_open(pConnection) &&
            !xllm__connection_idle_expired(pClient, pConnection, uNowMs) &&
            xllm__connection_idle_stop(pConnection))
        {
            return pConnection;
        }
        xllm__connection_close(pConnection);
    }
}

static void xllm__connection_release(xllm_client *pClient, xllm_connection *pConnection,
                                     bool bReusable)
{
    if (!pConnection)
        return;
    if (bReusable && pClient && pClient->pConnectionMutex && pClient->uMaxIdleConnections &&
        xllm__connection_idle_arm(pConnection) && xrtMutexLock(pClient->pConnectionMutex))
    {
        if (pClient->uIdleConnectionCount < pClient->uMaxIdleConnections)
        {
            pConnection->uPooledAtMs = xllm__clock_ms();
            pClient->pIdleConnections[pClient->uIdleConnectionCount++] = pConnection;
            pConnection = NULL;
        }
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
    }
    xllm__connection_close(pConnection);
}

/* ------------------------------------------------------------------ */
/* Request header assembly (once per call, on the starting thread)     */
/* ------------------------------------------------------------------ */

static bool xllm__transport_build_header(xllm_call *pCall)
{
    xllm_auth_header tAuth[XLLM_MAX_AUTH_HEADERS];
    size_t iAuth = 0u;
    size_t iFieldCount = 0u;
    size_t iHeaderSize = 0u;
    size_t iBodySize = strlen(pCall->sRequestBody);
    size_t i;
    char sLength[32];
    xhttpfield *tFields;
    bool bOk = false;
    (void)snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodySize);
    if (pCall->pClient->sApiKey[0])
    {
        iAuth = pCall->pDialect->BuildAuth(pCall->pClient, tAuth, XLLM_MAX_AUTH_HEADERS);
    }
    tFields =
        (xhttpfield *)xllm__malloc(sizeof(*tFields) * (6u + iAuth + pCall->iExtraHeaderCount));
    if (!tFields)
        goto done;
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Host"), xllm__sv(pCall->pClient->sHostHeader)};
    tFields[iFieldCount++] =
        (xhttpfield){xllm__sv("Accept"),
                     xllm__sv(pCall->bStreamWanted ? "text/event-stream" : "application/json")};
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Content-Type"), xllm__sv("application/json")};
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Content-Length"), xllm__sv(sLength)};
    tFields[iFieldCount++] =
        (xhttpfield){xllm__sv("User-Agent"), xllm__sv(pCall->pClient->sUserAgent)};
    tFields[iFieldCount++] = (xhttpfield){xllm__sv("Connection"), xllm__sv("keep-alive")};
    for (i = 0u; i < iAuth; ++i)
    {
        tFields[iFieldCount++] = (xhttpfield){xllm__sv(tAuth[i].sName), xllm__sv(tAuth[i].sValue)};
    }
    for (i = 0u; i < pCall->iExtraHeaderCount; ++i)
    {
        tFields[iFieldCount++] = (xhttpfield){xllm__sv(pCall->pExtraHeaderNames[i]),
                                              xllm__sv(pCall->pExtraHeaderValues[i])};
    }
    if (!xrtHttp1RequestWrite(xllm__sv("POST"), xllm__sv(pCall->pClient->sTarget),
                              XHTTP_VERSION_1_1, tFields, iFieldCount, NULL, 0u, &iHeaderSize))
        goto done;
    pCall->sRequestHeader = (char *)xllm__malloc(iHeaderSize);
    if (!pCall->sRequestHeader ||
        !xrtHttp1RequestWrite(xllm__sv("POST"), xllm__sv(pCall->pClient->sTarget),
                              XHTTP_VERSION_1_1, tFields, iFieldCount,
                              (uint8_t *)pCall->sRequestHeader, iHeaderSize, &iHeaderSize))
        goto done;
    pCall->iRequestHeaderSize = iHeaderSize;
    bOk = true;
done:
    xllm__free(tFields);
    for (i = 0u; i < iAuth; ++i)
    {
        if (tAuth[i].sValue)
        {
            volatile char *pSecret = (volatile char *)tAuth[i].sValue;
            size_t iLen = strlen(tAuth[i].sValue);
            while (iLen--)
            {
                pSecret[iLen] = 0;
            }
            xllm__free(tAuth[i].sValue);
        }
    }
    return bOk;
}

/* ------------------------------------------------------------------ */
/* URL parsing and client runtime                                      */
/* ------------------------------------------------------------------ */

static bool xllm__parse_url(xllm_client *pClient, xllm_error *pError)
{
    const char *sUrl = pClient->sBaseUrl;
    const char *sAuthority;
    const char *sPath;
    const char *sHostBegin;
    const char *sHostEnd;
    const char *sPort = NULL;
    size_t iHostLen;
    size_t iHeaderLen;
    unsigned long uPort;
    char *sEnd;
    if (strncmp(sUrl, "https://", 8u) == 0)
    {
        pClient->bTls = true;
        sAuthority = sUrl + 8u;
        pClient->uPort = 443u;
    }
    else if (strncmp(sUrl, "http://", 7u) == 0)
    {
        pClient->bTls = false;
        sAuthority = sUrl + 7u;
        pClient->uPort = 80u;
    }
    else
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "base URL must use http or https");
        return false;
    }
    sPath = strchr(sAuthority, '/');
    if (!sPath)
        sPath = sAuthority + strlen(sAuthority);
    if (sPath == sAuthority || memchr(sAuthority, '@', (size_t)(sPath - sAuthority)) ||
        strchr(sPath, '#'))
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "base URL contains an invalid authority or fragment");
        return false;
    }
    sHostBegin = sAuthority;
    if (*sHostBegin == '[')
    {
        sHostEnd = memchr(sHostBegin, ']', (size_t)(sPath - sHostBegin));
        if (!sHostEnd || sHostEnd == sHostBegin + 1)
            goto invalid;
        ++sHostBegin;
        if (sHostEnd + 1 < sPath)
        {
            if (sHostEnd[1] != ':')
                goto invalid;
            sPort = sHostEnd + 2;
        }
    }
    else
    {
        sHostEnd = memchr(sHostBegin, ':', (size_t)(sPath - sHostBegin));
        if (!sHostEnd)
            sHostEnd = sPath;
        else
            sPort = sHostEnd + 1;
    }
    iHostLen = (size_t)(sHostEnd - sHostBegin);
    if (!iHostLen)
        goto invalid;
    if (sPort)
    {
        if (sPort >= sPath)
            goto invalid;
        uPort = strtoul(sPort, &sEnd, 10);
        if (sEnd != sPath || !uPort || uPort > 65535u)
            goto invalid;
        pClient->uPort = (uint16_t)uPort;
    }
    pClient->sHost = (char *)xllm__malloc(iHostLen + 1u);
    pClient->sTarget = xllm__strdup(*sPath ? sPath : "/");
    iHeaderLen = (size_t)(sPath - sAuthority);
    pClient->sHostHeader = (char *)xllm__malloc(iHeaderLen + 1u);
    if (!pClient->sHost || !pClient->sTarget || !pClient->sHostHeader)
        goto oom;
    memcpy(pClient->sHost, sHostBegin, iHostLen);
    pClient->sHost[iHostLen] = 0;
    memcpy(pClient->sHostHeader, sAuthority, iHeaderLen);
    pClient->sHostHeader[iHeaderLen] = 0;
    return true;
invalid:
    xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                    "base URL contains an invalid host or port");
    return false;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to parse base URL");
    return false;
}

/* IP-literal endpoints must not receive a (protocol-illegal) IP SNI:
 * per the XRT TLS contract only VerifyName carries the identity then. */
static bool xllm__host_is_ip_literal(const char *sHost)
{
    const char *p;
    unsigned uGroups = 1u;
    unsigned uValue = 0u;
    unsigned uDigits = 0u;
    if (!sHost || !sHost[0])
    {
        return false;
    }
    if (strchr(sHost, ':'))
    {
        return true;
    } /* bare IPv6 (brackets stripped) */
    for (p = sHost; *p; ++p)
    {
        if (*p >= '0' && *p <= '9')
        {
            uValue = uValue * 10u + (unsigned)(*p - '0');
            if (++uDigits > 3u)
            {
                return false;
            }
        }
        else if (*p == '.')
        {
            if (uDigits == 0u || uValue > 255u)
            {
                return false;
            }
            uValue = 0u;
            uDigits = 0u;
            ++uGroups;
        }
        else
        {
            return false;
        }
    }
    return uGroups == 4u && uDigits != 0u && uValue <= 255u;
}

static xtlsverifydecision xllm__tls_accept(const xtlspeer *pPeer, ptr pData)
{
    (void)pPeer;
    (void)pData;
    return XTLS_VERIFY_ACCEPT;
}

bool xllm__transport_client_init(xllm_client *pClient, xllm_error *pError)
{
    xnetengineconfig tEngine;
    if (!xllm__parse_url(pClient, pError))
        return false;
    pClient->pConnectionMutex = xrtMutexCreate();
    if (!pClient->pConnectionMutex)
        goto network_error;
    if (pClient->pNetEngine == NULL)
    {
        xrtNetEngineConfigInit(&tEngine);
        pClient->pNetEngine = xrtNetEngineCreate(&tEngine);
        if (!pClient->pNetEngine)
            goto network_error;
        pClient->bEngineOwned = true;
        if (!xrtNetEngineStart(pClient->pNetEngine))
            goto network_error;
    }
    pClient->pResolver = xrtNetResolverCreate(NULL);
    if (!pClient->pResolver)
        goto network_error;
    if (pClient->bTls)
    {
        xtlsverifierconfig tVerify;
        xx509store *pStore = NULL;
        xrtTlsVerifierConfigInit(&tVerify);
        if (pClient->pX509Store)
        {
            /* borrowed store wins; the verifier clones the anchors */
            tVerify.Store = pClient->pX509Store;
        }
        else if (pClient->sCaPem && pClient->sCaPem[0])
        {
            size_t iAdded = 0u;
            pStore = xrtX509StoreCreate();
            if (!pStore ||
                !xrtX509StoreAddPem(pStore, pClient->sCaPem, strlen(pClient->sCaPem), &iAdded) ||
                iAdded == 0u)
            {
                xrtX509StoreFree(pStore);
                xllm__error_set(pError, XLLM_ERROR_NETWORK,
                                "failed to load the configured CA PEM trust store");
                xllm__transport_client_unit(pClient);
                return false;
            }
            tVerify.Store = pStore;
        }
        else if (pClient->bVerifyPeer)
        {
            pStore = xrtX509StoreSystem();
            if (!pStore)
                goto network_error;
            tVerify.Store = pStore;
        }
        else
        {
            tVerify.Verify = xllm__tls_accept;
        }
        pClient->pVerifier = xrtTlsVerifierCreate(&tVerify);
        /* pStore is owned on the PEM and system paths (System() builds a
         * fresh store per call); the verifier cloned what it needs. */
        if (pStore)
        {
            xrtX509StoreFree(pStore);
        }
        if (!pClient->pVerifier)
            goto network_error;
    }
    return true;
network_error:
    xllm__error_set(pError, XLLM_ERROR_NETWORK, "failed to initialize XRT HTTP transport");
    xllm__transport_client_unit(pClient);
    return false;
}

void xllm__transport_client_unit(xllm_client *pClient)
{
    xllm_connection *pIdle[XLLM_MAX_IDLE_CONNECTIONS] = {0};
    uint32_t uIdleCount = 0u;
    uint32_t i;
    if (!pClient)
        return;
    if (pClient->pConnectionMutex && xrtMutexLock(pClient->pConnectionMutex))
    {
        uIdleCount = pClient->uIdleConnectionCount;
        pClient->uIdleConnectionCount = 0u;
        for (i = 0u; i < uIdleCount; ++i)
        {
            pIdle[i] = pClient->pIdleConnections[i];
            pClient->pIdleConnections[i] = NULL;
        }
        (void)xrtMutexUnlock(pClient->pConnectionMutex);
    }
    for (i = 0u; i < uIdleCount; ++i)
    {
        xllm__connection_close(pIdle[i]);
    }
    if (pClient->pResolver)
    {
        (void)xrtNetResolverDestroy(pClient->pResolver);
        pClient->pResolver = NULL;
    }
    if (pClient->bEngineOwned && pClient->pNetEngine)
    {
        (void)xrtNetEngineStop(pClient->pNetEngine);
        (void)xrtNetEngineDestroy(pClient->pNetEngine);
    }
    pClient->pNetEngine = NULL;
    pClient->bEngineOwned = false;
    xrtTlsVerifierRelease(pClient->pVerifier);
    pClient->pVerifier = NULL;
    if (pClient->pConnectionMutex)
    {
        (void)xrtMutexDestroy(pClient->pConnectionMutex);
        pClient->pConnectionMutex = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* State machine                                                       */
/* ------------------------------------------------------------------ */

static xfuture *xllm__step_advance(xllm_call *pCall, xfuture *pDone);

/* One heap node per armed watch: a watch may never be re-initialized while
 * its notification is unwinding, so the embedded-storage reuse pattern is
 * forbidden here. The release callback frees the node exactly once. */
struct xllm_op_node
{
    xfuturewatch Watch;
    xllm_call *pCall;
    xfuture *pFuture;
    volatile long iRefs;
};

static void xllm__op_notify(ptr pData);
static void xllm__op_node_release(xllm_op_node *pNode)
{
    if (xllm__atomic_add(&pNode->iRefs, -1) == 0)
    {
        xrtFutureDestroy(pNode->pFuture);
        xllm__free(pNode);
    }
}

static void xllm__op_release(ptr pData) { xllm__op_node_release((xllm_op_node *)pData); }

typedef enum xllm_op_arm_result
{
    XLLM_OP_ARM_PENDING = 0,
    XLLM_OP_ARM_TERMINAL, /* future already complete: advance inline */
    XLLM_OP_ARM_MEMORY,   /* watch node allocation failed */
    XLLM_OP_ARM_ERROR     /* mutex/watch registration failed */
} xllm_op_arm_result;

/* Arm a watch and transfer the pending Future to its release callback.
 * Publication and WatchAdd share the slot lock. A concurrent completion may
 * run as soon as that lock is released, so the pending path must not touch
 * the node afterwards. */
static xllm_op_arm_result xllm__op_arm(xllm_call *pCall, xfuture *pFuture)
{
    xllm_op_node *pNode = (xllm_op_node *)xllm__calloc(1u, sizeof(*pNode));
    xfuturewatchresult eWatch;
    if (!pNode)
    {
        return XLLM_OP_ARM_MEMORY;
    }
    pNode->pCall = pCall;
    pNode->pFuture = pFuture;
    /* One reference belongs to the published slot and one to the Watch.
     * A completion can run before WatchAdd returns, so both must exist before
     * registration. Non-pending registration frees the unowned node directly. */
    pNode->iRefs = 2;
    if (!xrtFutureWatchInit(&pNode->Watch, xllm__op_notify, xllm__op_release, pNode) ||
        !pCall->bOpMutexInit || !xrtMutexLock(&pCall->OpMutex))
    {
        xllm__free(pNode);
        return XLLM_OP_ARM_ERROR;
    }
    if (pCall->pOpNode != NULL)
    {
        (void)xrtMutexUnlock(&pCall->OpMutex);
        xllm__free(pNode);
        return XLLM_OP_ARM_ERROR;
    }
    pCall->pOpNode = pNode;
    eWatch = xrtFutureWatchAdd(pFuture, &pNode->Watch);
    if (eWatch == XFUTURE_WATCH_PENDING)
    {
        (void)xrtMutexUnlock(&pCall->OpMutex);
        return XLLM_OP_ARM_PENDING;
    }
    pCall->pOpNode = NULL;
    (void)xrtMutexUnlock(&pCall->OpMutex);
    /* WatchAdd on a terminal future does not take the watch and runs no
     * release: free the node ourselves. */
    xllm__free(pNode);
    return eWatch == XFUTURE_WATCH_READY ? XLLM_OP_ARM_TERMINAL : XLLM_OP_ARM_ERROR;
}

/* Remove the currently published operation. Clearing the slot under the
 * mutex prevents its callback from advancing the state machine. Remove then
 * joins an already-running callback and invokes the node release exactly
 * once; it must run outside the mutex because the callback also takes it. */
void xllm__op_disarm(xllm_call *pCall)
{
    xllm_op_node *pNode = NULL;
    if (!pCall || !pCall->bOpMutexInit || !xrtMutexLock(&pCall->OpMutex))
    {
        return;
    }
    pNode = pCall->pOpNode;
    pCall->pOpNode = NULL;
    (void)xrtMutexUnlock(&pCall->OpMutex);
    if (pNode)
    {
        xrtFutureWatchRemove(pNode->pFuture, &pNode->Watch);
        xllm__op_node_release(pNode);
    }
}

/* Borrow the current operation as a real Future reference for cancellation.
 * The slot mutex keeps the node-owned reference alive until Ref succeeds. */
static xfuture *xllm__op_future_ref(xllm_call *pCall)
{
    xllm_op_node *pNode;
    xfuture *pFuture = NULL;
    if (!pCall || !pCall->bOpMutexInit || !xrtMutexLock(&pCall->OpMutex))
    {
        return NULL;
    }
    pNode = pCall->pOpNode;
    if (pNode)
    {
        pFuture = xrtFutureRef(pNode->pFuture);
    }
    (void)xrtMutexUnlock(&pCall->OpMutex);
    return pFuture;
}

static void xllm__op_notify(ptr pData)
{
    xllm_op_node *pNode = (xllm_op_node *)pData;
    xllm_call *pCall = pNode->pCall;
    xfuture *pFuture = pNode->pFuture;
    bool bCurrent = false;
    (void)xllm__atomic_add(&pCall->iTransportActive, 1);
    if (xrtMutexLock(&pCall->OpMutex))
    {
        if (pCall->pOpNode == pNode)
        {
            pCall->pOpNode = NULL;
            bCurrent = true;
        }
        (void)xrtMutexUnlock(&pCall->OpMutex);
    }
    if (bCurrent)
    {
        xfuture *pNext = NULL;
        if (pCall->bTransportMutexInit && xrtMutexLock(&pCall->TransportMutex))
        {
            if (xllm__atomic_load(&pCall->iClosing) == 0 &&
                xllm__atomic_load(&pCall->iTerminal) == 0)
            {
                pNext = xllm__step_advance(pCall, pFuture);
            }
            while (pNext)
            {
                xfuture *pCurrent;
                xllm_op_arm_result eArm;
                if (xllm__atomic_load(&pCall->iClosing) != 0 ||
                    xllm__atomic_load(&pCall->iTerminal) != 0)
                {
                    xrtFutureDestroy(pNext);
                    pNext = NULL;
                    break;
                }
                pCurrent = pNext;
                eArm = xllm__op_arm(pCall, pCurrent);
                if (eArm == XLLM_OP_ARM_PENDING)
                {
                    break;
                }
                if (eArm == XLLM_OP_ARM_MEMORY || eArm == XLLM_OP_ARM_ERROR)
                {
                    xrtFutureDestroy(pCurrent);
                    xllm__transport_fail(pCall, "submit", XLLM_TRANSPORT_ERROR,
                                         eArm == XLLM_OP_ARM_MEMORY ? "out_of_memory" : "watch",
                                         xrtGetError());
                    xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
                    pNext = NULL;
                    break;
                }
                pNext = xllm__step_advance(pCall, pCurrent);
                xrtFutureDestroy(pCurrent);
            }
            (void)xrtMutexUnlock(&pCall->TransportMutex);
        }
    }
    (void)xllm__atomic_add(&pCall->iTransportActive, -1);
    if (bCurrent)
    {
        xllm__op_node_release(pNode);
    }
}

/* Submit an operation future and advance inline until a pending watch arms. */
static void xllm__op_submit_chain(xllm_call *pCall, xfuture *pFuture)
{
    if (!pFuture)
    {
        if (xllm__atomic_load(&pCall->iTerminal) != 0)
        {
            return;
        }
        if (pCall->tHttpDiagnostics.eResult == XLLM_TRANSPORT_OK)
        {
            xllm__transport_fail(pCall, pCall->ePhase == XLLM_ASYNC_DIAL ? "connect" : "submit",
                                 XLLM_TRANSPORT_ERROR, "submit", xrtGetError());
        }
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        return;
    }
    for (;;)
    {
        xllm_op_arm_result eArm;
        eArm = xllm__op_arm(pCall, pFuture);
        if (eArm == XLLM_OP_ARM_PENDING)
        {
            return;
        }
        if (eArm == XLLM_OP_ARM_MEMORY || eArm == XLLM_OP_ARM_ERROR)
        {
            xllm__transport_fail(pCall, "submit", XLLM_TRANSPORT_ERROR,
                                 eArm == XLLM_OP_ARM_MEMORY ? "out_of_memory" : "watch",
                                 xrtGetError());
            xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            xrtFutureDestroy(pFuture);
            return;
        }
        {
            xfuture *pNext;
            if (xllm__atomic_load(&pCall->iClosing) != 0 ||
                xllm__atomic_load(&pCall->iTerminal) != 0)
            {
                xrtFutureDestroy(pFuture);
                return;
            }
            pNext = xllm__step_advance(pCall, pFuture);
            xrtFutureDestroy(pFuture);
            if (!pNext)
            {
                return;
            }
            pFuture = pNext;
        }
    }
}

static xllm_transport_result xllm__wait_error_result(xfuture *pFuture)
{
    const xerror *pError = xrtFutureError(pFuture);
    if (pError)
    {
        if (xrtErrorKind(pError) == XERR_TIMEOUT)
            return XLLM_TRANSPORT_TIMEOUT;
        if (xrtErrorKind(pError) == XERR_CANCELLED)
            return XLLM_TRANSPORT_CANCELLED;
    }
    return XLLM_TRANSPORT_ERROR;
}

static const char *xllm__error_name(xllm_transport_result eResult, const char *sFallback)
{
    if (eResult == XLLM_TRANSPORT_TIMEOUT)
        return "timeout";
    if (eResult == XLLM_TRANSPORT_CANCELLED)
        return "cancelled";
    return sFallback;
}

/* Dial completion: adopt the stream and continue to the send phase. */
static xfuture *xllm__step_dial(xllm_call *pCall, xfuture *pFuture)
{
    xllm_client *pClient = pCall->pClient;
    if (xrtFutureState(pFuture) != XFUTURE_RESOLVED)
    {
        xllm_transport_result eResult = xllm__wait_error_result(pFuture);
        xllm__transport_fail(pCall, "connect", eResult,
                             xllm__error_name(eResult, pClient->bTls ? "tls_connect" : "connect"),
                             xrtFutureError(pFuture));
        xllm__transport_finish(pCall, eResult);
        return NULL;
    }
    pCall->pConnection = (xllm_connection *)xllm__calloc(1u, sizeof(*pCall->pConnection));
    if (!pCall->pConnection)
    {
        xllm__transport_fail(pCall, "connect", XLLM_TRANSPORT_ERROR, "out_of_memory", NULL);
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        return NULL;
    }
    pCall->pConnection->bTls = pClient->bTls;
    if (pClient->bTls)
    {
        pCall->pConnection->pTls = xrtTlsStreamRef((xtlsstream *)xrtFutureValue(pFuture));
    }
    else
    {
        pCall->pConnection->pTcp = xrtNetStreamRef((xnetstream *)xrtFutureValue(pFuture));
    }
    pCall->tHttpDiagnostics.uConnectedMs = xllm__clock_ms();
    pCall->ePhase = XLLM_ASYNC_SEND;
    pCall->iSendOffset = 0u;
    return XLLM_STEP_CONTINUE;
}

/* Submit the next send chunk; CONTINUE when written inline, NULL when done. */
static xfuture *xllm__send_submit(xllm_call *pCall)
{
    size_t iTotal = pCall->iRequestHeaderSize + strlen(pCall->sRequestBody);
    if (pCall->iSendOffset >= iTotal)
    {
        return NULL;
    }
    {
        size_t iLocal = pCall->iSendOffset < pCall->iRequestHeaderSize
                            ? pCall->iSendOffset
                            : pCall->iSendOffset - pCall->iRequestHeaderSize;
        size_t iSegmentEnd =
            pCall->iSendOffset < pCall->iRequestHeaderSize ? pCall->iRequestHeaderSize : iTotal;
        const char *sBase = pCall->iSendOffset < pCall->iRequestHeaderSize ? pCall->sRequestHeader
                                                                           : pCall->sRequestBody;
        size_t iChunk = iSegmentEnd - pCall->iSendOffset;
        const void *pData = sBase + iLocal;
        if (iChunk > XLLM_HTTP_IO_CHUNK)
            iChunk = XLLM_HTTP_IO_CHUNK;
        if (pCall->pConnection->bTls)
        {
            pCall->iSendPending = iChunk;
            return xrtTlsStreamSendAsync(pCall->pConnection->pTls, pData, iChunk);
        }
        {
            xnetresult eResult = xrtNetStreamSend(pCall->pConnection->pTcp, pData, iChunk);
            if (eResult == XNET_RESULT_OK)
            {
                pCall->iSendOffset += iChunk;
                pCall->tHttpDiagnostics.uRequestBytes += iChunk;
                return XLLM_STEP_CONTINUE;
            }
            if (eResult == XNET_RESULT_AGAIN)
            {
                return xrtNetStreamWaitAsync(pCall->pConnection->pTcp, XNET_STREAM_WAIT_WRITE);
            }
            xllm__transport_fail(pCall, "send", XLLM_TRANSPORT_ERROR, "send", xrtGetError());
            xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            return NULL;
        }
    }
}

/* Send-phase completion: account the chunk, continue or move to read. */
static xfuture *xllm__step_send(xllm_call *pCall, xfuture *pFuture)
{
    if (pFuture)
    {
        if (xrtFutureState(pFuture) != XFUTURE_RESOLVED)
        {
            xllm_transport_result eResult = xllm__wait_error_result(pFuture);
            xllm__transport_fail(pCall, "send", eResult, xllm__error_name(eResult, "send"),
                                 xrtFutureError(pFuture));
            xllm__transport_finish(pCall, eResult);
            return NULL;
        }
        if (pCall->pConnection->bTls)
        {
            /* Account exactly the chunk that was submitted: the segment
             * layout may cap a chunk below the 64 KiO io slice, and the
             * server waits for every Content-Length byte. */
            pCall->iSendOffset += pCall->iSendPending;
            pCall->tHttpDiagnostics.uRequestBytes += pCall->iSendPending;
            pCall->iSendPending = 0u;
        }
        /* Plain TCP WAIT_WRITE fired: retry the send inline below. */
    }
    for (;;)
    {
        xfuture *pNext = xllm__send_submit(pCall);
        if (pNext == NULL)
        {
            pCall->tHttpDiagnostics.uRequestSentMs = xllm__clock_ms();
            pCall->ePhase = XLLM_ASYNC_READ;
            return XLLM_STEP_CONTINUE;
        }
        if (pNext != XLLM_STEP_CONTINUE)
        {
            return pNext;
        }
    }
}

static xfuture *xllm__recv_submit(xllm_call *pCall)
{
    if (pCall->pConnection->bTls)
    {
        return xrtTlsStreamRecvAsync(pCall->pConnection->pTls, XLLM_HTTP_IO_CHUNK);
    }
    return xrtNetStreamRecvAsync(pCall->pConnection->pTcp, XLLM_HTTP_IO_CHUNK);
}

static bool xllm__response_reusable(const xhttp1head *pHead, const xhttp1bodyplan *pPlan)
{
    if (pPlan->Mode == XHTTP1_BODY_CLOSE || (pHead->Flags & XHTTP1_CONNECTION_CLOSE))
        return false;
    if (pHead->Version == XHTTP_VERSION_1_0 && !(pHead->Flags & XHTTP1_KEEP_ALIVE))
        return false;
    return true;
}

static void xllm__wire_compact(xllm_call *pCall)
{
    if (pCall->iWireOffset)
    {
        if (pCall->iWireOffset < pCall->tWire.iLen)
        {
            memmove(pCall->tWire.pData, pCall->tWire.pData + pCall->iWireOffset,
                    pCall->tWire.iLen - pCall->iWireOffset);
        }
        pCall->tWire.iLen -= pCall->iWireOffset;
        if (pCall->tWire.pData)
        {
            pCall->tWire.pData[pCall->tWire.iLen] = 0;
        }
        pCall->iWireOffset = 0u;
    }
}

static void xllm__pump_fail(xllm_call *pCall, const char *sPhase, const char *sName)
{
    xllm__transport_fail(pCall, sPhase, XLLM_TRANSPORT_ERROR, sName, NULL);
    xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
}

/* Pump buffered wire bytes through the incremental head/body parser. */
static xfuture *xllm__step_pump(xllm_call *pCall)
{
    for (;;)
    {
        if (!pCall->bHeadReady)
        {
            xhttp1status eStatus =
                xrtHttp1ResponseParse(xllm__bv(pCall->tWire.pData, pCall->tWire.iLen),
                                      &pCall->tHead, &pCall->tHeadLimits, &pCall->tProtoErr);
            if (eStatus == XHTTP1_READY)
            {
                pCall->bHeadReady = true;
                pCall->iWireOffset = pCall->tHead.Bytes;
                pCall->tHttpDiagnostics.uHeadersMs = xllm__clock_ms();
                if (!xllm__transport_headers(pCall, &pCall->tHead))
                {
                    xllm__transport_fail(pCall, "headers", XLLM_TRANSPORT_CANCELLED,
                                         "callback_cancelled", NULL);
                    xllm__transport_finish(pCall, XLLM_TRANSPORT_CANCELLED);
                    return NULL;
                }
                if (!xrtHttp1ResponseBodyPlan(&pCall->tHead, xllm__sv("POST"), &pCall->tPlan))
                {
                    xllm__pump_fail(pCall, "headers", "http_framing");
                    return NULL;
                }
                xrtHttp1BodyLimitsInit(&pCall->tBodyLimits);
                pCall->tBodyLimits.MaxBody = pCall->uMaxResponseBodyBytes
                                                 ? pCall->uMaxResponseBodyBytes
                                                 : XLLM_MAX_FALLBACK_BODY;
                if (!xrtHttp1BodyInit(&pCall->tBody, &pCall->tPlan, pCall->tTrailers,
                                      XLLM_HTTP_TRAILER_LIMIT, &pCall->tBodyLimits))
                {
                    xllm__pump_fail(pCall, "body", "http_framing");
                    return NULL;
                }
                continue;
            }
            if (eStatus == XHTTP1_ERROR || eStatus == XHTTP1_FIELDS ||
                pCall->tWire.iLen >=
                    (pCall->uMaxHttpHeadBytes ? pCall->uMaxHttpHeadBytes : XLLM_HTTP_HEAD_LIMIT))
            {
                xllm__pump_fail(pCall, "headers", "http_protocol");
                return NULL;
            }
            if (pCall->bWireEnd)
            {
                xllm__pump_fail(pCall, "headers", "unexpected_eof");
                return NULL;
            }
            return xllm__recv_submit(pCall);
        }
        {
            xbytesview tInput =
                xllm__bv(pCall->tWire.pData ? pCall->tWire.pData + pCall->iWireOffset : NULL,
                         pCall->tWire.iLen - pCall->iWireOffset);
            size_t iConsumed = 0u;
            xbytesview tData = {0};
            xhttp1bodystatus eBody = xrtHttp1BodyRead(&pCall->tBody, tInput, pCall->bWireEnd,
                                                      &iConsumed, &tData, &pCall->tProtoErr);
            pCall->iWireOffset += iConsumed;
            if (eBody == XHTTP1_BODY_DATA)
            {
                pCall->tHttpDiagnostics.uResponseBodyBytes += tData.Size;
                if (!xllm__transport_body(pCall, tData.Data, tData.Size))
                {
                    xllm__transport_fail(pCall, "body", XLLM_TRANSPORT_CANCELLED,
                                         "callback_cancelled", NULL);
                    xllm__transport_finish(pCall, XLLM_TRANSPORT_CANCELLED);
                    return NULL;
                }
                continue;
            }
            if (eBody == XHTTP1_BODY_DONE)
            {
                pCall->bReusable = xllm__response_reusable(&pCall->tHead, &pCall->tPlan) &&
                                   pCall->iWireOffset == pCall->tWire.iLen;
                xllm__transport_finish(pCall, XLLM_TRANSPORT_OK);
                return NULL;
            }
            if (eBody == XHTTP1_BODY_ERROR || eBody == XHTTP1_BODY_FIELDS)
            {
                xllm__pump_fail(pCall, "body", "http_protocol");
                return NULL;
            }
            /* A decoded SSE terminal event completes the model protocol.
             * Do not wait for a provider's delayed final chunk/EOF. Prefer
             * the BODY_DONE path above when framing is already available;
             * otherwise retire this undrained connection, never pool it.
             * A response-body hook deliberately defers decoding until EOF. */
            if (pCall->bSse && !pCall->bDeferParse && pCall->bDone)
            {
                pCall->bReusable = false;
                xllm__transport_finish(pCall, XLLM_TRANSPORT_OK);
                return NULL;
            }
            if (pCall->bWireEnd)
            {
                xllm__pump_fail(pCall, "body", "unexpected_eof");
                return NULL;
            }
            xllm__wire_compact(pCall);
            return xllm__recv_submit(pCall);
        }
    }
}

/* Read-phase completion: absorb the received bytes and pump. */
static xfuture *xllm__step_read(xllm_call *pCall, xfuture *pFuture)
{
    xfuturestate eState = xrtFutureState(pFuture);
    if (eState == XFUTURE_RESOLVED)
    {
        xnetbytes *pBytes = (xnetbytes *)xrtFutureValue(pFuture);
        xbytesview tView = xrtNetBytesView(pBytes);
        if (tView.Size)
        {
            if (!xllm__buf_append(&pCall->tWire, tView.Data, tView.Size))
            {
                xllm__pump_fail(pCall, "receive", "out_of_memory");
                return NULL;
            }
            if (!pCall->tHttpDiagnostics.uFirstByteMs)
            {
                pCall->tHttpDiagnostics.uFirstByteMs = xllm__clock_ms();
            }
        }
        return xllm__step_pump(pCall);
    }
    if (eState == XFUTURE_CLOSED)
    {
        pCall->bWireEnd = true;
        return xllm__step_pump(pCall);
    }
    {
        xllm_transport_result eResult = xllm__wait_error_result(pFuture);
        if (eResult == XLLM_TRANSPORT_ERROR && pCall->pConnection && !pCall->pConnection->bTls &&
            xrtNetStreamState(pCall->pConnection->pTcp) == XNET_STREAM_CLOSED)
        {
            pCall->bWireEnd = true;
            return xllm__step_pump(pCall);
        }
        xllm__transport_fail(pCall, "receive", eResult, xllm__error_name(eResult, "receive"),
                             xrtFutureError(pFuture));
        xllm__transport_finish(pCall, eResult);
        return NULL;
    }
}

/* Process one completed operation future; return the next operation future,
 * the CONTINUE sentinel, or NULL at a terminal. */
static xfuture *xllm__step_advance(xllm_call *pCall, xfuture *pFuture)
{
    xfuture *pNext = NULL;
    switch (pCall->ePhase)
    {
    case XLLM_ASYNC_DIAL:
        pNext = xllm__step_dial(pCall, pFuture);
        if (pNext == XLLM_STEP_CONTINUE)
        {
            pNext = xllm__step_send(pCall, NULL);
            if (pNext == XLLM_STEP_CONTINUE)
            {
                pNext = xllm__recv_submit(pCall);
            }
        }
        break;
    case XLLM_ASYNC_SEND:
        pNext = xllm__step_send(pCall, pFuture);
        if (pNext == XLLM_STEP_CONTINUE)
        {
            pNext = xllm__recv_submit(pCall);
        }
        break;
    case XLLM_ASYNC_READ:
        pNext = xllm__step_read(pCall, pFuture);
        break;
    default:
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        break;
    }
    return pNext;
}

void xllm__transport_finish(xllm_call *pCall, xllm_transport_result eResult)
{
    if (xllm__atomic_add(&pCall->iTerminal, 1) != 1)
    {
        return;
    }
    pCall->ePhase = XLLM_ASYNC_DONE;
    pCall->eTransportResult = eResult;
    pCall->tHttpDiagnostics.uCompletedMs = xllm__clock_ms();
    if (pCall->tHttpDiagnostics.eResult == XLLM_TRANSPORT_OK && eResult != XLLM_TRANSPORT_OK)
    {
        pCall->tHttpDiagnostics.eResult = eResult;
    }
    if (eResult == XLLM_TRANSPORT_OK)
    {
        xllm__copy_text(pCall->tHttpDiagnostics.sPhase, sizeof(pCall->tHttpDiagnostics.sPhase),
                        "complete");
        xllm__copy_text(pCall->tHttpDiagnostics.sError, sizeof(pCall->tHttpDiagnostics.sError),
                        "none");
    }
    if (pCall->uTimerId)
    {
        (void)xrtNetEngineTimerCancel(pCall->pClient->pNetEngine, pCall->uTimerId);
        pCall->uTimerId = 0u;
    }
    if (pCall->pConnection)
    {
        xllm__connection_release(pCall->pClient, pCall->pConnection,
                                 eResult == XLLM_TRANSPORT_OK && pCall->bReusable);
        pCall->pConnection = NULL;
    }
    if (pCall->pPromise)
    {
        (void)xrtPromiseResolve(pCall->pPromise, NULL);
    }
}

/* TransportMutex protects publication and retirement of pConnection. Stream
 * Abort is non-blocking and may run while an operation Future owns another
 * stream reference, so it is safe to issue while the mutex is held. */
static void xllm__abort_connection_locked(xllm_call *pCall)
{
    if (pCall->pConnection)
    {
        if (pCall->pConnection->bTls)
        {
            (void)xrtTlsStreamAbort(pCall->pConnection->pTls);
        }
        else
        {
            (void)xrtNetStreamAbort(pCall->pConnection->pTcp);
        }
    }
}

static void xllm__watchdog(xnetworker *pWorker, uint64 uId, xnetresult eResult, ptr pData)
{
    xllm_call *pCall = (xllm_call *)pData;
    (void)pWorker;
    (void)uId;
    /* Exactly-once callback: release the timer lifetime reference no matter
     * which terminal result this is (fired, cancelled, stopped, failed). */
    if (eResult != XNET_RESULT_OK || xllm__atomic_load(&pCall->iTerminal) != 0)
    {
        (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
        return;
    }
    if (!pCall->bTransportMutexInit || !xrtMutexLock(&pCall->TransportMutex))
    {
        (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
        return;
    }
    if (xllm__atomic_load(&pCall->iTerminal) == 0)
    {
        xllm__abort_connection_locked(pCall);
        xllm__transport_fail(pCall,
                             pCall->ePhase == XLLM_ASYNC_DIAL
                                 ? "connect"
                                 : (pCall->ePhase == XLLM_ASYNC_SEND ? "send" : "receive"),
                             XLLM_TRANSPORT_TIMEOUT, "timeout", NULL);
        xllm__transport_finish(pCall, XLLM_TRANSPORT_TIMEOUT);
    }
    (void)xrtMutexUnlock(&pCall->TransportMutex);
    (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
}

static void xllm__cancel_abort(ptr pData)
{
    xllm_call *pCall = (xllm_call *)pData;
    if (pCall->bTransportMutexInit && xrtMutexLock(&pCall->TransportMutex))
    {
        xllm__abort_connection_locked(pCall);
        (void)xrtMutexUnlock(&pCall->TransportMutex);
    }
}

bool xllm__transport_arm_cancel(xllm_call *pCall)
{
    if (!pCall || !pCall->pCancel || pCall->pCancelWatch)
        return false;
    /* CancelWatch may call back synchronously for an already-cancelled parent.
     * Register before transport_begin takes TransportMutex. */
    pCall->pCancelWatch = xrtCancelWatch(pCall->pCancel, xllm__cancel_abort, pCall);
    return pCall->pCancelWatch != NULL;
}

static void xllm__transport_begin_locked(xllm_call *pCall)
{
    xllm_client *pClient = pCall->pClient;
    xllm_connection *pConnection = NULL;
    pCall->tHttpDiagnostics.uStartedMs = xllm__clock_ms();
    pCall->tHttpDiagnostics.eResult = XLLM_TRANSPORT_OK;
    xllm__copy_text(pCall->tHttpDiagnostics.sPhase, sizeof(pCall->tHttpDiagnostics.sPhase),
                    "connect");
    xrtHttp1LimitsInit(&pCall->tHeadLimits);
    pCall->tHeadLimits.MaxHead =
        pCall->uMaxHttpHeadBytes ? pCall->uMaxHttpHeadBytes : XLLM_HTTP_HEAD_LIMIT;
    pCall->tHeadLimits.MaxFields = XLLM_HTTP_FIELD_LIMIT;
    xrtHttp1HeadInit(&pCall->tHead, pCall->tHeadFields, XLLM_HTTP_FIELD_LIMIT);
    memset(&pCall->tProtoErr, 0, sizeof(pCall->tProtoErr));
    if (xrtCancelRequested(pCall->pCancel))
    {
        xllm__transport_fail(pCall, "cancel", XLLM_TRANSPORT_CANCELLED, "cancelled", NULL);
        xllm__transport_finish(pCall, XLLM_TRANSPORT_CANCELLED);
        return;
    }
    if (!xllm__transport_build_header(pCall))
    {
        xllm__transport_fail(pCall, "request", XLLM_TRANSPORT_ERROR, "request", xrtGetError());
        xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
        return;
    }
    if (pCall->uDeadline != XLLM_DEADLINE_NEVER)
    {
        (void)xllm__atomic_add(&pCall->iTimerRefs, 1);
        pCall->uTimerId = xrtNetEngineSchedule(
            pClient->pNetEngine, 0u, xllmDeadlineWaitMs(pCall->uDeadline), xllm__watchdog, pCall);
        if (pCall->uTimerId == 0u)
        {
            /* Scheduling failed: without the watchdog the deadline is only
             * enforced by a waiting consumer, which future-only callers are
             * not -- fail closed instead of running unbounded. */
            (void)xllm__atomic_add(&pCall->iTimerRefs, -1);
            xllm__transport_fail(pCall, "connect", XLLM_TRANSPORT_ERROR, "timer_unavailable", NULL);
            xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            return;
        }
    }
    pConnection = xllm__connection_take(pClient);
    if (pConnection)
    {
        xfuture *pNext;
        pCall->tHttpDiagnostics.bReusedConnection = true;
        pCall->tHttpDiagnostics.uConnectedMs = pCall->tHttpDiagnostics.uStartedMs;
        pCall->pConnection = pConnection;
        pCall->ePhase = XLLM_ASYNC_SEND;
        pNext = xllm__step_send(pCall, NULL);
        if (pNext == XLLM_STEP_CONTINUE)
        {
            pNext = xllm__recv_submit(pCall);
        }
        xllm__op_submit_chain(pCall, pNext);
        return;
    }
    pCall->ePhase = XLLM_ASYNC_DIAL;
    if (pClient->bTls)
    {
        xtlsclientconfig tTls;
        xtlsdialconfig tDial;
        xfuture *pFuture;
        xrtTlsClientConfigInit(&tTls);
        if (xllm__host_is_ip_literal(pClient->sHost))
        {
            tTls.ServerName = xllm__sv(NULL);
            tTls.VerifyName = xllm__sv(pClient->sHost);
        }
        else
        {
            tTls.ServerName = xllm__sv(pClient->sHost);
            tTls.VerifyName = xllm__sv(pClient->sHost);
        }
        tTls.Verifier = pClient->pVerifier;
        xrtTlsDialConfigInit(&tDial);
        if (pCall->uDeadline != XLLM_DEADLINE_NEVER)
        {
            tDial.Timeout = xllmDeadlineWaitMs(pCall->uDeadline);
        }
        if (pClient->pProxy && !xllm__proxy_bypassed(pClient->sProxyBypass, pClient->sHost))
        {
            pFuture =
                xllm__proxy_dial_async(pClient->pNetEngine, pClient->pResolver, pClient->pProxy,
                                       pClient->sHost, pClient->uPort, &tTls, &tDial, NULL, NULL);
        }
        else
        {
            pFuture = xrtTlsDialAsync(pClient->pNetEngine, pClient->pResolver, pClient->sHost,
                                      pClient->uPort, &tTls, &tDial, NULL, NULL);
        }
        xllm__op_submit_chain(pCall, pFuture);
    }
    else
    {
        xnetdialconfig tDial;
        xfuture *pFuture;
        xrtNetDialConfigInit(&tDial);
        if (pCall->uDeadline != XLLM_DEADLINE_NEVER)
        {
            tDial.Timeout = xllmDeadlineWaitMs(pCall->uDeadline);
        }
        pFuture = xrtNetDialAsync(pClient->pNetEngine, pClient->pResolver, pClient->sHost,
                                  pClient->uPort, &tDial, NULL, NULL);
        xllm__op_submit_chain(pCall, pFuture);
    }
}

void xllm__transport_begin(xllm_call *pCall)
{
    if (!pCall || !pCall->bTransportMutexInit || !xrtMutexLock(&pCall->TransportMutex))
    {
        return;
    }
    xllm__transport_begin_locked(pCall);
    (void)xrtMutexUnlock(&pCall->TransportMutex);
}

void xllm__transport_abort(xllm_call *pCall)
{
    xfuture *pOpFuture;
    if (xllm__atomic_load(&pCall->iTerminal) != 0)
    {
        return;
    }
    xllm__cancel_abort((ptr)pCall);
    pOpFuture = xllm__op_future_ref(pCall);
    if (pOpFuture)
    {
        (void)xrtFutureCancel(pOpFuture);
        xrtFutureDestroy(pOpFuture);
    }
    if (pCall->pFuture)
    {
        /* xrtFutureWaitFor takes relative milliseconds. */
        uint64 uGiveUp = xllm__clock_ms() + 3000u;
        while (xllm__atomic_load(&pCall->iTerminal) == 0 && xllm__clock_ms() < uGiveUp)
        {
            (void)xrtFutureWaitFor(pCall->pFuture, 20);
        }
    }
    if (xllm__atomic_load(&pCall->iTerminal) == 0)
    {
        if (pCall->bTransportMutexInit && xrtMutexLock(&pCall->TransportMutex))
        {
            if (xllm__atomic_load(&pCall->iTerminal) == 0)
            {
                xllm__transport_fail(pCall, "abort", XLLM_TRANSPORT_ERROR, "aborted", NULL);
                xllm__transport_finish(pCall, XLLM_TRANSPORT_ERROR);
            }
            (void)xrtMutexUnlock(&pCall->TransportMutex);
        }
    }
}
#endif


/* ========================================================================== */
/* source: extlibs/xllm/src/llm/xllm_client.c */
/* ========================================================================== */

#if defined(XLLM_FEATURE_XLLM)
#include <stdio.h>

#define XLLM_MAX_ATTEMPTS 8u

static char *xllm__normalize_url(const char *sBaseUrl, const xllm_dialect_ops *pDialect)
{
    const char *sSuffix = pDialect->sPathSuffix;
    size_t iSuffixLen = strlen(sSuffix);
    size_t iLen;
    char *sUrl;
    if (!sBaseUrl || !sBaseUrl[0])
    {
        return NULL;
    }
    if (strstr(sBaseUrl, sSuffix))
    {
        return xllm__strdup(sBaseUrl);
    }
    iLen = strlen(sBaseUrl);
    while (iLen && sBaseUrl[iLen - 1u] == '/')
    {
        --iLen;
    }
    /* "https://host/v1" + "/v1/messages" must not double the version. */
    if (iSuffixLen > 4u && memcmp(sSuffix, "/v1/", 4u) == 0 && iLen >= 3u && iLen >= 3u &&
        memcmp(sBaseUrl + iLen - 3u, "/v1", 3u) == 0)
    {
        iLen -= 3u;
    }
    sUrl = (char *)xllm__malloc(iLen + iSuffixLen + 1u);
    if (!sUrl)
    {
        return NULL;
    }
    memcpy(sUrl, sBaseUrl, iLen);
    memcpy(sUrl + iLen, sSuffix, iSuffixLen + 1u);
    return sUrl;
}

static uint32_t xllm__parse_retry_after_ms(const char *sValue, uint32_t uMultiplier)
{
    unsigned long long uValue;
    char *sEnd = NULL;
    if (!sValue || !sValue[0])
    {
        return 0u;
    }
    uValue = strtoull(sValue, &sEnd, 10);
    if (sEnd == sValue)
    {
        return 0u;
    }
    while (*sEnd && isspace((unsigned char)*sEnd))
    {
        ++sEnd;
    }
    if (*sEnd || uValue > (unsigned long long)UINT32_MAX / uMultiplier)
    {
        return 0u;
    }
    return (uint32_t)(uValue * uMultiplier);
}

static const xhttpfield *xllm__header(const xhttp1head *pHead, const char *sName)
{
    return pHead ? xrtHttp1Field(pHead, (xstrview){sName, strlen(sName)}) : NULL;
}

static void xllm__capture_diagnostics(xllm_call *pCall, xllm_diagnostics *pOut)
{
    const xllm_transport_diagnostics *pHttp;
    if (!pOut)
    {
        return;
    }
    memset(pOut, 0, sizeof(*pOut));
    if (!pCall)
    {
        return;
    }
    pHttp = &pCall->tHttpDiagnostics;
    pOut->uAttemptCount = pCall->uAttempt ? pCall->uAttempt : 1u;
    pOut->uMaxAttempts = pCall->pClient ? pCall->pClient->uMaxAttempts : 1u;
    pOut->uRetryAfterMs = pCall->uRetryAfterMs;
    pOut->bResponseStarted = pCall->uHttpStatus != 0u;
    pOut->bModelDataDelivered =
        pCall->bSawEvent || pCall->pResponse != NULL || pCall->bResponseTaken;
    pOut->bIdempotencyKeyPresent = pCall->bIdempotencyKeyPresent;
    pOut->bReusedConnection = pHttp->bReusedConnection;
    pOut->bToolCallDropped = pHttp->bToolCallDropped;
    pOut->bContextAttached = pCall->bScopeAttached;
    pOut->iTransportStatus = (int32_t)pHttp->eResult;
    pOut->iSystemError = pHttp->iSystemError;
    pOut->uStartedMs = pHttp->uStartedMs;
    pOut->uConnectedMs = pHttp->uConnectedMs;
    pOut->uRequestSentMs = pHttp->uRequestSentMs;
    pOut->uFirstByteMs = pHttp->uFirstByteMs;
    pOut->uFirstTokenMs =
        pHttp->uFirstTokenMs > pHttp->uStartedMs ? pHttp->uFirstTokenMs - pHttp->uStartedMs : 0u;
    pOut->uHeadersMs = pHttp->uHeadersMs;
    pOut->uCompletedMs = pHttp->uCompletedMs;
    pOut->uConnectDurationMs =
        pHttp->uConnectedMs > pHttp->uStartedMs ? pHttp->uConnectedMs - pHttp->uStartedMs : 0u;
    pOut->uTimeToFirstByteMs =
        pHttp->uFirstByteMs > pHttp->uStartedMs ? pHttp->uFirstByteMs - pHttp->uStartedMs : 0u;
    pOut->uTransferDurationMs =
        pHttp->uCompletedMs > pHttp->uHeadersMs ? pHttp->uCompletedMs - pHttp->uHeadersMs : 0u;
    pOut->uTotalDurationMs =
        pHttp->uCompletedMs > pHttp->uStartedMs ? pHttp->uCompletedMs - pHttp->uStartedMs : 0u;
    pOut->uRequestBytes = pHttp->uRequestBytes;
    pOut->uResponseBodyBytes = pHttp->uResponseBodyBytes;
    pOut->uContextDeadlineMs =
        pCall->uScopeDeadline == XLLM_DEADLINE_NEVER ? 0u : pCall->uScopeDeadline / UINT64_C(1000);
    pOut->uEffectiveTimeoutMs = pHttp->uEffectiveTimeoutMs;
    xllm__copy_text(pOut->sTransportError, sizeof(pOut->sTransportError), pHttp->sError);
    if (pCall->bScopeAttached && pHttp->eResult == XLLM_TRANSPORT_TIMEOUT)
    {
        xllm__copy_text(pOut->sTransportError, sizeof(pOut->sTransportError), "deadline_exceeded");
    }
    xllm__copy_text(pOut->sTransportPhase, sizeof(pOut->sTransportPhase), pHttp->sPhase);
    xllm__copy_text(
        pOut->sContextStatus, sizeof(pOut->sContextStatus),
        !pCall->bScopeAttached
            ? "none"
            : (pHttp->eResult == XLLM_TRANSPORT_CANCELLED
                   ? "cancelled"
                   : (pHttp->eResult == XLLM_TRANSPORT_TIMEOUT ? "deadline_exceeded" : "active")));
    if (pCall->pDialect)
    {
        xllm__copy_text(pOut->sDialect, sizeof(pOut->sDialect), pCall->pDialect->sName);
    }
}

static void xllm__capture_stats(xllm_call *pCall, xllm_response *pResponse)
{
    xllm_stats *pStats;
    const xllm_transport_diagnostics *pHttp;
    if (!pCall || !pResponse)
    {
        return;
    }
    pStats = &pResponse->tStats;
    pHttp = &pCall->tHttpDiagnostics;
    pStats->tUsage = pResponse->tUsage;
    pStats->uConnectMs =
        pHttp->uConnectedMs > pHttp->uStartedMs ? pHttp->uConnectedMs - pHttp->uStartedMs : 0u;
    pStats->uFirstByteMs =
        pHttp->uFirstByteMs > pHttp->uStartedMs ? pHttp->uFirstByteMs - pHttp->uStartedMs : 0u;
    pStats->uFirstTokenMs =
        pHttp->uFirstTokenMs > pHttp->uStartedMs ? pHttp->uFirstTokenMs - pHttp->uStartedMs : 0u;
    pStats->uTotalMs =
        pHttp->uCompletedMs > pHttp->uStartedMs ? pHttp->uCompletedMs - pHttp->uStartedMs : 0u;
    pStats->fOutputTokensPerSec = 0.0;
    if (pResponse->tUsage.uOutputTokens && pHttp->uFirstTokenMs &&
        pHttp->uCompletedMs > pHttp->uFirstTokenMs)
    {
        double fSeconds = (double)(pHttp->uCompletedMs - pHttp->uFirstTokenMs) / 1000.0;
        if (fSeconds > 0.0)
        {
            pStats->fOutputTokensPerSec = (double)pResponse->tUsage.uOutputTokens / fSeconds;
        }
    }
    pStats->uRequestBytes = pHttp->uRequestBytes;
    pStats->uResponseBytes = pHttp->uResponseBodyBytes;
    pStats->uAttempts = pCall->uAttempt ? pCall->uAttempt : 1u;
    pStats->bReusedConnection = pHttp->bReusedConnection;
}

static xllm_result xllm__scope_result(xcancel *pCancel, uint64_t uDeadline, xllm_error *pError)
{
    if (pError && (pCancel || uDeadline != XLLM_DEADLINE_NEVER))
    {
        pError->tDiagnostics.bContextAttached = true;
        pError->tDiagnostics.uContextDeadlineMs =
            uDeadline == XLLM_DEADLINE_NEVER ? 0u : uDeadline / UINT64_C(1000);
        xllm__copy_text(pError->tDiagnostics.sTransportPhase,
                        sizeof(pError->tDiagnostics.sTransportPhase), "prepare");
    }
    if (pCancel && xrtCancelRequested(pCancel))
    {
        xllm__error_set(pError, XLLM_ERROR_CANCELLED, "model operation was cancelled");
        if (pError)
        {
            xllm__copy_text(pError->tDiagnostics.sContextStatus,
                            sizeof(pError->tDiagnostics.sContextStatus), "cancelled");
            xllm__copy_text(pError->tDiagnostics.sTransportError,
                            sizeof(pError->tDiagnostics.sTransportError), "cancelled");
        }
        return XLLM_RESULT_CANCELLED;
    }
    if (uDeadline != XLLM_DEADLINE_NEVER && xllmDeadlineExpired(uDeadline))
    {
        xllm__error_set(pError, XLLM_ERROR_TIMEOUT, "model operation deadline was exceeded");
        if (pError)
        {
            xllm__copy_text(pError->tDiagnostics.sContextStatus,
                            sizeof(pError->tDiagnostics.sContextStatus), "deadline_exceeded");
            xllm__copy_text(pError->tDiagnostics.sTransportError,
                            sizeof(pError->tDiagnostics.sTransportError), "deadline_exceeded");
        }
        return XLLM_RESULT_TIMEOUT;
    }
    return XLLM_RESULT_OK;
}

static bool xllm__scope_sleep(xcancel *pCancel, uint64_t uDeadline, uint32_t uDelayMs)
{
    uint64_t uEnd = xllmClockUs() + (uint64_t)uDelayMs * UINT64_C(1000);
    if (uDeadline != XLLM_DEADLINE_NEVER && uEnd > uDeadline)
        uEnd = uDeadline;
    for (;;)
    {
        uint64_t uNow;
        uint64_t uRemaining;
        uint32_t uSlice;
        if (pCancel && xrtCancelRequested(pCancel))
            return false;
        uNow = xllmClockUs();
        if (uNow >= uEnd)
        {
            return true;
        }
        uRemaining = uEnd - uNow;
        uSlice = uRemaining > UINT64_C(20000) ? 20u : (uint32_t)((uRemaining + 999u) / 1000u);
        if (!uSlice)
        {
            uSlice = 1u;
        }
        xrtSleep(uSlice);
    }
}

static uint32_t xllm__retry_delay_ms(const xllm_client *pClient, uint32_t uAttempt,
                                     uint32_t uRetryAfterMs)
{
    uint64_t uDelay;
    uint32_t i;
    if (!pClient)
    {
        return 0u;
    }
    uDelay = pClient->uRetryBaseDelayMs;
    for (i = 1u; i < uAttempt && uDelay < pClient->uRetryMaxDelayMs; ++i)
    {
        uDelay *= 2u;
    }
    if (uDelay < uRetryAfterMs)
    {
        uDelay = uRetryAfterMs;
    }
    if (pClient->uRetryMaxDelayMs > 0u && uDelay > pClient->uRetryMaxDelayMs)
    {
        uDelay = pClient->uRetryMaxDelayMs;
    }
    return uDelay > UINT32_MAX ? UINT32_MAX : (uint32_t)uDelay;
}

static bool xllm__ascii_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static unsigned char xllm__ascii_lower(unsigned char c)
{
    return c >= (unsigned char)'A' && c <= (unsigned char)'Z'
               ? (unsigned char)(c + ((unsigned char)'a' - (unsigned char)'A'))
               : c;
}

static bool xllm__ascii_equal(const char *sLeft, const char *sRight, size_t iSize)
{
    size_t i;
    for (i = 0u; i < iSize; ++i)
    {
        if (xllm__ascii_lower((unsigned char)sLeft[i]) !=
            xllm__ascii_lower((unsigned char)sRight[i]))
        {
            return false;
        }
    }
    return true;
}

static bool xllm__proxy_pattern_matches(const char *sPattern, size_t iPattern, const char *sHost,
                                        size_t iHost)
{
    bool bLeading = false;
    bool bTrailing = false;
    size_t i;
    if (iPattern != 0u && sPattern[0] == '*')
    {
        bLeading = true;
        ++sPattern;
        --iPattern;
    }
    if (iPattern != 0u && sPattern[iPattern - 1u] == '*')
    {
        bTrailing = true;
        --iPattern;
    }
    if (iPattern == 0u)
    {
        return bLeading || bTrailing;
    }
    /* Only edge wildcards are part of the public grammar. Treat an interior
     * '*' as an invalid, non-matching entry instead of expanding it. */
    if (memchr(sPattern, '*', iPattern) != NULL || iPattern > iHost)
    {
        return false;
    }
    if (!bLeading && !bTrailing)
    {
        return iPattern == iHost && xllm__ascii_equal(sPattern, sHost, iHost);
    }
    if (!bLeading)
    {
        return xllm__ascii_equal(sPattern, sHost, iPattern);
    }
    if (!bTrailing)
    {
        return xllm__ascii_equal(sPattern, sHost + iHost - iPattern, iPattern);
    }
    for (i = 0u; i + iPattern <= iHost; ++i)
    {
        if (xllm__ascii_equal(sPattern, sHost + i, iPattern))
        {
            return true;
        }
    }
    return false;
}

bool xllm__proxy_bypassed(const char *sBypass, const char *sHost)
{
    const char *pEntry;
    const char *pEnd;
    size_t iHost;
    if (!sBypass || !sBypass[0] || !sHost || !sHost[0])
    {
        return false;
    }
    iHost = strlen(sHost);
    pEntry = sBypass;
    while (*pEntry)
    {
        while (*pEntry == ',' || *pEntry == ';' || xllm__ascii_space(*pEntry))
        {
            ++pEntry;
        }
        pEnd = pEntry;
        while (*pEnd && *pEnd != ',' && *pEnd != ';')
        {
            ++pEnd;
        }
        while (pEnd > pEntry && xllm__ascii_space(pEnd[-1]))
        {
            --pEnd;
        }
        if (pEnd > pEntry &&
            xllm__proxy_pattern_matches(pEntry, (size_t)(pEnd - pEntry), sHost, iHost))
        {
            return true;
        }
        pEntry = pEnd;
        while (*pEntry == ',' || *pEntry == ';')
        {
            ++pEntry;
        }
    }
    return false;
}

static bool xllm__proxy_text(const char *sValue) { return sValue && sValue[0]; }

static bool xllm__proxy_requested(const xllm_client_config *pConfig)
{
    return pConfig->eProxyKind != XLLM_PROXY_NONE || xllm__proxy_text(pConfig->sProxyHost) ||
           pConfig->uProxyPort != 0u || xllm__proxy_text(pConfig->sProxyUser) ||
           xllm__proxy_text(pConfig->sProxyPass);
}

static bool xllm__proxy_config_valid(const xllm_client_config *pConfig, xllm_error *pError)
{
    if (!xllm__proxy_requested(pConfig))
    {
        return true;
    }
    if ((pConfig->eProxyKind != XLLM_PROXY_SOCKS5 &&
         pConfig->eProxyKind != XLLM_PROXY_HTTP_CONNECT) ||
        !xllm__proxy_text(pConfig->sProxyHost) || pConfig->uProxyPort == 0u)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "proxy kind, host, and port must be configured together");
        return false;
    }
    return true;
}

static bool xllm__proxy_configure(xllm_client *pClient, const xllm_client_config *pConfig,
                                  xllm_error *pError)
{
    xnetproxyconfig tProxy;
    const xerror *pXrtError;

    if (pConfig->sProxyBypass && pConfig->sProxyBypass[0])
    {
        pClient->sProxyBypass = xllm__strdup(pConfig->sProxyBypass);
        if (!pClient->sProxyBypass)
        {
            xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to store proxy bypass policy");
            return false;
        }
    }
    if (!xllm__proxy_requested(pConfig))
    {
        return true;
    }
    if (!pClient->bTls)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "outbound proxy requires an HTTPS base URL");
        return false;
    }
    xrtNetProxyConfigInit(&tProxy);
    tProxy.Type =
        pConfig->eProxyKind == XLLM_PROXY_SOCKS5 ? XNET_PROXY_SOCKS5 : XNET_PROXY_HTTP_CONNECT;
    tProxy.Host.Data = pConfig->sProxyHost;
    tProxy.Host.Size = strlen(pConfig->sProxyHost);
    tProxy.Port = pConfig->uProxyPort;
    tProxy.Username.Data = (const uint8 *)(pConfig->sProxyUser ? pConfig->sProxyUser : "");
    tProxy.Username.Size = pConfig->sProxyUser ? strlen(pConfig->sProxyUser) : 0u;
    tProxy.Password.Data = (const uint8 *)(pConfig->sProxyPass ? pConfig->sProxyPass : "");
    tProxy.Password.Size = pConfig->sProxyPass ? strlen(pConfig->sProxyPass) : 0u;
    pClient->pProxy = xrtNetProxyCreate(&tProxy);
    if (pClient->pProxy)
    {
        return true;
    }
    pXrtError = xrtGetError();
    xllm__error_set(pError,
                    pXrtError && xrtErrorKind(pXrtError) == XERR_MEMORY
                        ? XLLM_ERROR_OUT_OF_MEMORY
                        : XLLM_ERROR_INVALID_ARGUMENT,
                    "failed to create outbound proxy configuration");
    return false;
}

void xllmClientConfigInit(xllm_client_config *pConfig)
{
    if (!pConfig)
    {
        return;
    }
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->sReasoningEffort = "max";
    pConfig->sUserAgent = "xllm/3.0";
    pConfig->uMaxOutputTokens = 65536u;
    pConfig->uTimeoutMs = 30u * 60u * 1000u;
    pConfig->uIdleTimeoutMs = 5u * 60u * 1000u;
    pConfig->uMaxAttempts = 3u;
    pConfig->uRetryBaseDelayMs = 250u;
    pConfig->uRetryMaxDelayMs = 30000u;
    pConfig->uMaxIdleConnections = 4u;
    pConfig->uMaxHttpHeadBytes = XLLM_HTTP_HEAD_LIMIT;
    pConfig->uMaxResponseBodyBytes = XLLM_MAX_FALLBACK_BODY;
    pConfig->uMaxSseLineBytes = 1024u * 1024u;
    pConfig->uMaxSseEventBytes = 16u * 1024u * 1024u;
    pConfig->bVerifyPeer = true;
    pConfig->eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
}

xllm_client *xllmClientCreate(const xllm_client_config *pConfig, xllm_error *pError)
{
    xllm_client *pClient = NULL;
    const xllm_model_profile *pProfile;
    const xllm_dialect_ops *pDialect;
    const char *sModel;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    pProfile = pConfig ? pConfig->pModelProfile : NULL;
    pDialect = xllm__dialect_ops_for(
        pProfile ? pProfile->eProvider
                 : (pConfig ? pConfig->eProvider : XLLM_PROVIDER_OPENAI_COMPAT));
    if (!pDialect)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "provider has no wire dialect in this build");
        return NULL;
    }
    sModel = pConfig && pConfig->sModel && pConfig->sModel[0]
                 ? pConfig->sModel
                 : (pProfile ? pProfile->sModel : NULL);
    if (!pConfig || !pConfig->sBaseUrl || !pConfig->sBaseUrl[0] || !sModel || !sModel[0])
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "base URL and model are required");
        return NULL;
    }
    if (pConfig->uMaxHttpHeadBytes > XLLM_HTTP_HEAD_LIMIT ||
        pConfig->uMaxResponseBodyBytes > XLLM_MAX_FALLBACK_BODY ||
        pConfig->uMaxSseLineBytes > 1024u * 1024u ||
        pConfig->uMaxSseEventBytes > 16u * 1024u * 1024u)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "parser limit exceeds the library hard ceiling");
        return NULL;
    }
    if (!xllm__proxy_config_valid(pConfig, pError))
    {
        return NULL;
    }
    if (pProfile)
    {
        if (!xllmModelProfileValidate(pProfile, pError))
            return NULL;
        if (pConfig->sModel && pConfig->sModel[0] && strcmp(pConfig->sModel, pProfile->sModel) != 0)
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "client model does not match the model profile");
            return NULL;
        }
        if (pConfig->uMaxOutputTokens > pProfile->uMaxOutputTokens)
        {
            xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                            "client output limit exceeds the model profile");
            return NULL;
        }
    }
    pClient = (xllm_client *)xllm__calloc(1u, sizeof(*pClient));
    if (!pClient)
        goto oom;
    pClient->bConfigMutexInit = xrtMutexInit(&pClient->ConfigMutex);
    if (!pClient->bConfigMutexInit)
        goto oom;
    pClient->pDialect = pDialect;
    pClient->pNetEngine = pConfig->pNetEngine;
    pClient->bEngineOwned = false;
    pClient->sBaseUrl = xllm__normalize_url(pConfig->sBaseUrl, pDialect);
    pClient->sApiKey = xllm__strdup(pConfig->sApiKey ? pConfig->sApiKey : "");
    pClient->sModel = xllm__strdup(sModel);
    pClient->sReasoningEffort =
        xllm__strdup(pConfig->sReasoningEffort ? pConfig->sReasoningEffort : "");
    pClient->sUserAgent = xllm__strdup(pConfig->sUserAgent ? pConfig->sUserAgent : "xllm/3.0");
    pClient->uMaxOutputTokens = pConfig->uMaxOutputTokens
                                    ? pConfig->uMaxOutputTokens
                                    : (pProfile ? pProfile->uMaxOutputTokens : 65536u);
    pClient->uTimeoutMs = pConfig->uTimeoutMs;
    pClient->uIdleTimeoutMs = pConfig->uIdleTimeoutMs;
    pClient->uMaxAttempts = pConfig->uMaxAttempts ? pConfig->uMaxAttempts : 1u;
    if (pClient->uMaxAttempts > XLLM_MAX_ATTEMPTS)
    {
        pClient->uMaxAttempts = XLLM_MAX_ATTEMPTS;
    }
    pClient->uRetryBaseDelayMs = pConfig->uRetryBaseDelayMs;
    pClient->uRetryMaxDelayMs = pConfig->uRetryMaxDelayMs;
    pClient->uMaxHttpHeadBytes =
        pConfig->uMaxHttpHeadBytes ? pConfig->uMaxHttpHeadBytes : XLLM_HTTP_HEAD_LIMIT;
    pClient->uMaxResponseBodyBytes =
        pConfig->uMaxResponseBodyBytes ? pConfig->uMaxResponseBodyBytes : XLLM_MAX_FALLBACK_BODY;
    pClient->uMaxSseLineBytes =
        pConfig->uMaxSseLineBytes ? pConfig->uMaxSseLineBytes : 1024u * 1024u;
    pClient->uMaxSseEventBytes =
        pConfig->uMaxSseEventBytes ? pConfig->uMaxSseEventBytes : 16u * 1024u * 1024u;
    pClient->uMaxIdleConnections = pConfig->uMaxIdleConnections ? pConfig->uMaxIdleConnections : 4u;
    if (pClient->uMaxIdleConnections > XLLM_MAX_IDLE_CONNECTIONS)
    {
        pClient->uMaxIdleConnections = XLLM_MAX_IDLE_CONNECTIONS;
    }
    pClient->bVerifyPeer = pConfig->bVerifyPeer;
    pClient->sCaPem = xllm__strdup(pConfig->sCaPem ? pConfig->sCaPem : "");
    pClient->pX509Store = pConfig->pX509Store;
    pClient->eProvider = pProfile ? pProfile->eProvider : pConfig->eProvider;
    if (pProfile)
    {
        pClient->sProfileId = xllm__strdup(pProfile->sId);
        pClient->tModelProfile = *pProfile;
        pClient->tModelProfile.sId = pClient->sProfileId;
        pClient->tModelProfile.sModel = pClient->sModel;
        pClient->bHasModelProfile = true;
    }
    if (!pClient->sBaseUrl || !pClient->sApiKey || !pClient->sModel || !pClient->sReasoningEffort ||
        !pClient->sUserAgent || !pClient->sCaPem || (pProfile && !pClient->sProfileId))
        goto oom;
    if (!xllm__transport_client_init(pClient, pError))
    {
        xllmClientDestroy(pClient);
        return NULL;
    }
    if (!xllm__proxy_configure(pClient, pConfig, pError))
    {
        xllmClientDestroy(pClient);
        return NULL;
    }
    return pClient;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate model client");
    xllmClientDestroy(pClient);
    return NULL;
}

void xllmClientSetHooks(xllm_client *pClient, const xllm_hooks *pHooks)
{
    if (!pClient || !pClient->bConfigMutexInit || !xrtMutexLock(&pClient->ConfigMutex))
    {
        return;
    }
    if (pHooks)
    {
        pClient->tHooks = *pHooks;
        pClient->bHasHooks = true;
    }
    else
    {
        memset(&pClient->tHooks, 0, sizeof(pClient->tHooks));
        pClient->bHasHooks = false;
    }
    (void)xrtMutexUnlock(&pClient->ConfigMutex);
}

void xllmClientDestroy(xllm_client *pClient)
{
    size_t i;
    if (!pClient)
    {
        return;
    }
    xllm__transport_client_unit(pClient);
    xrtNetProxyRelease(pClient->pProxy);
    pClient->pProxy = NULL;
    xllm__free(pClient->sBaseUrl);
    if (pClient->sApiKey)
    {
        volatile char *pSecret = (volatile char *)pClient->sApiKey;
        i = strlen(pClient->sApiKey);
        while (i--)
        {
            pSecret[i] = 0;
        }
    }
    xllm__free(pClient->sApiKey);
    xllm__free(pClient->sModel);
    xllm__free(pClient->sReasoningEffort);
    xllm__free(pClient->sUserAgent);
    xllm__free(pClient->sProfileId);
    xllm__free(pClient->sCaPem);
    xllm__free(pClient->sProxyBypass);
    xllm__free(pClient->sHost);
    xllm__free(pClient->sTarget);
    xllm__free(pClient->sHostHeader);
    xllm__free(pClient->sPrefixCacheInner);
    if (pClient->bConfigMutexInit)
    {
        (void)xrtMutexUnit(&pClient->ConfigMutex);
    }
    xllm__free(pClient);
}

bool xllmClientGetModelProfile(const xllm_client *pClient, xllm_model_profile *pProfile)
{
    xllm_client *pMutable = (xllm_client *)pClient;
    bool bFound = false;
    if (!pMutable || !pProfile || !pMutable->bConfigMutexInit ||
        !xrtMutexLock(&pMutable->ConfigMutex))
        return false;
    if (pMutable->bHasModelProfile)
    {
        pMutable->bConfigSealed = true;
        *pProfile = pMutable->tModelProfile;
        bFound = true;
    }
    (void)xrtMutexUnlock(&pMutable->ConfigMutex);
    return bFound;
}

bool xllmClientSetModelProfile(xllm_client *pClient, const xllm_model_profile *pProfile,
                               xllm_error *pError)
{
    char *sId;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pClient || !pProfile)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client and profile are required");
        return false;
    }
    if (!xllmModelProfileValidate(pProfile, pError))
    {
        return false;
    }
    if (pProfile->eProvider != pClient->eProvider)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "profile provider must match the client's fixed wire dialect");
        return false;
    }
    sId = xllm__strdup(pProfile->sId);
    if (!sId)
    {
        xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to store the profile id");
        return false;
    }
    if (!pClient->bConfigMutexInit || !xrtMutexLock(&pClient->ConfigMutex))
    {
        xllm__free(sId);
        xllm__error_set(pError, XLLM_ERROR_PROTOCOL, "client configuration lock failed");
        return false;
    }
    if (pClient->bConfigSealed)
    {
        (void)xrtMutexUnlock(&pClient->ConfigMutex);
        xllm__free(sId);
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "model profile is immutable after the client is first used");
        return false;
    }
    /* Mirror the create-time convention: the id becomes client-owned storage
     * and the wire model stays the client's sModel (URLs and credentials are
     * client configuration, never profile business). */
    xllm__free(pClient->sProfileId);
    pClient->sProfileId = sId;
    pClient->tModelProfile = *pProfile;
    pClient->tModelProfile.sId = sId;
    pClient->tModelProfile.sModel = pClient->sModel;
    pClient->bHasModelProfile = true;
    pClient->iPrefixCacheLen = 0u;
    pClient->iPrefixCacheMessages = 0u;
    pClient->pPrefixCacheOwner = NULL;
    (void)xrtMutexUnlock(&pClient->ConfigMutex);
    return true;
}

static bool xllm__client_begin_operation(xllm_client *pClient, const xllm_hooks *pRequestHooks,
                                         xllm_hooks *pHooks, bool *pbHasHooks, xllm_error *pError)
{
    if (!pClient || !pHooks || !pbHasHooks || !pClient->bConfigMutexInit ||
        !xrtMutexLock(&pClient->ConfigMutex))
    {
        xllm__error_set(pError, XLLM_ERROR_PROTOCOL, "client configuration lock failed");
        return false;
    }
    pClient->bConfigSealed = true;
    *pbHasHooks = false;
    memset(pHooks, 0, sizeof(*pHooks));
    if (pRequestHooks)
    {
        *pHooks = *pRequestHooks;
        *pbHasHooks = true;
    }
    else if (pClient->bHasHooks)
    {
        *pHooks = pClient->tHooks;
        *pbHasHooks = true;
    }
    (void)xrtMutexUnlock(&pClient->ConfigMutex);
    return true;
}

static bool xllm__call_enter(xllm_call *pCall)
{
    if (!pCall)
    {
        return false;
    }
    (void)xllm__atomic_add(&pCall->iCallbackActive, 1);
    if (xllm__atomic_load(&pCall->iClosing) != 0)
    {
        (void)xllm__atomic_add(&pCall->iCallbackActive, -1);
        return false;
    }
    return true;
}

static void xllm__call_leave(xllm_call *pCall)
{
    (void)xllm__atomic_add(&pCall->iCallbackActive, -1);
}

bool xllm__transport_headers(xllm_call *pCall, const xhttp1head *pHead)
{
    const xhttpfield *pHeader;
    char sRetryAfter[64];
    xllm_event tEvent;
    bool bOk = true;
    if (!xllm__call_enter(pCall))
    {
        return false;
    }
    pCall->uHttpStatus = pHead ? pHead->Status : 0u;
    pHeader = xllm__header(pHead, "x-request-id");
    if (!pHeader)
        pHeader = xllm__header(pHead, "request-id");
    if (pHeader)
        xllm__copy_view(pCall->sRequestId, sizeof(pCall->sRequestId), pHeader->Value);
    pHeader = xllm__header(pHead, "content-type");
    if (pHeader)
        xllm__copy_view(pCall->sContentType, sizeof(pCall->sContentType), pHeader->Value);
    pHeader = xllm__header(pHead, "retry-after-ms");
    sRetryAfter[0] = 0;
    if (pHeader)
        xllm__copy_view(sRetryAfter, sizeof(sRetryAfter), pHeader->Value);
    pCall->uRetryAfterMs = xllm__parse_retry_after_ms(sRetryAfter, 1u);
    if (pCall->uRetryAfterMs == 0u)
    {
        pHeader = xllm__header(pHead, "retry-after");
        sRetryAfter[0] = 0;
        if (pHeader)
            xllm__copy_view(sRetryAfter, sizeof(sRetryAfter), pHeader->Value);
        pCall->uRetryAfterMs = xllm__parse_retry_after_ms(sRetryAfter, 1000u);
    }
    pCall->bSse = pCall->uHttpStatus >= 200u && pCall->uHttpStatus < 300u && pCall->bStreamWanted &&
                  xllm__contains_ci(pCall->sContentType, "text/event-stream");
    memset(&tEvent, 0, sizeof(tEvent));
    tEvent.eKind = XLLM_EVENT_RESPONSE_START;
    tEvent.as.tResponse.uHttpStatus = pCall->uHttpStatus;
    tEvent.as.tResponse.sRequestId = pCall->sRequestId;
    bOk = xllm__emit(pCall, &tEvent);
    xllm__call_leave(pCall);
    return bOk;
}

bool xllm__transport_body(xllm_call *pCall, const void *pData, size_t iLen)
{
    bool bOk = true;
    if (!xllm__call_enter(pCall))
    {
        return false;
    }
    if (pCall->bSse && !pCall->bDeferParse)
    {
        bOk = xllm__sse_feed(pCall, pData, iLen);
    }
    else
    {
        size_t iLimit =
            pCall->uMaxResponseBodyBytes ? pCall->uMaxResponseBodyBytes : XLLM_MAX_FALLBACK_BODY;
        if (pCall->tRawBody.iLen > iLimit || iLen > iLimit - pCall->tRawBody.iLen)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_LIMIT,
                            "provider response exceeds the configured body limit");
            bOk = false;
        }
        else
        {
            bOk = xllm__buf_append(&pCall->tRawBody, pData, iLen);
            if (!bOk)
            {
                xllm__error_set(&pCall->tError, XLLM_ERROR_OUT_OF_MEMORY,
                                "failed to buffer provider response");
            }
            else if (!pCall->bSse && pCall->uHttpStatus >= 200u && pCall->uHttpStatus < 300u &&
                     pCall->bStreamWanted && pCall->tRawBody.iLen >= 5u &&
                     memcmp(pCall->tRawBody.pData, "data:", 5u) == 0)
            {
                pCall->bSse = true;
                if (!pCall->bDeferParse)
                {
                    bOk = xllm__sse_feed(pCall, pCall->tRawBody.pData, pCall->tRawBody.iLen);
                    xllm__buf_reset(&pCall->tRawBody);
                }
            }
        }
    }
    xllm__call_leave(pCall);
    return bOk;
}

static void xllm__map_http_error(xllm_call *pCall)
{
    xllm_error_code eCode = XLLM_ERROR_UPSTREAM;
    const char *sMessage = "provider returned an HTTP error";
    if (pCall->uHttpStatus == 401u || pCall->uHttpStatus == 403u)
    {
        eCode = XLLM_ERROR_AUTH;
        sMessage = "provider authentication failed";
    }
    else if (pCall->uHttpStatus == 404u)
    {
        eCode = XLLM_ERROR_MODEL_NOT_FOUND;
        sMessage = "provider endpoint or model was not found";
    }
    else if (pCall->uHttpStatus == 429u)
    {
        eCode = XLLM_ERROR_RATE_LIMIT;
        sMessage = "provider rate limit exceeded";
    }
    xllm__error_set(&pCall->tError, eCode, sMessage);
    pCall->tError.iHttpStatus = (int32_t)pCall->uHttpStatus;
    xllm__copy_text(pCall->tError.sRequestId, sizeof(pCall->tError.sRequestId), pCall->sRequestId);
    pCall->pDialect->FillProviderError(pCall,
                                       (xstrview){pCall->tRawBody.pData, pCall->tRawBody.iLen});
    pCall->tError.eCode = eCode;
    if (!pCall->tError.sMessage[0])
    {
        xllm__copy_text(pCall->tError.sMessage, sizeof(pCall->tError.sMessage), sMessage);
    }
}

static bool xllm__call_copy_extra_headers(xllm_call *pCall, const xllm_request *pRequest)
{
    size_t i;
    if (!pRequest->iExtraHeaderCount)
    {
        return true;
    }
    pCall->pExtraHeaderNames = (char **)xllm__calloc(pRequest->iExtraHeaderCount, sizeof(char *));
    pCall->pExtraHeaderValues = (char **)xllm__calloc(pRequest->iExtraHeaderCount, sizeof(char *));
    if (!pCall->pExtraHeaderNames || !pCall->pExtraHeaderValues)
    {
        return false;
    }
    pCall->iExtraHeaderCount = pRequest->iExtraHeaderCount;
    for (i = 0u; i < pRequest->iExtraHeaderCount; ++i)
    {
        const xllm_header *pHeader = &pRequest->pExtraHeaders[i];
        const char *sName;
        const char *sExpected = "Idempotency-Key";
        if (!pHeader->sName || !pHeader->sName[0] || !pHeader->sValue)
        {
            return false;
        }
        sName = pHeader->sName;
        while (*sName && *sExpected &&
               (unsigned char)tolower((unsigned char)*sName) ==
                   (unsigned char)tolower((unsigned char)*sExpected))
        {
            ++sName;
            ++sExpected;
        }
        if (*sName == '\0' && *sExpected == '\0' && pHeader->sValue[0] != '\0')
        {
            pCall->bIdempotencyKeyPresent = true;
        }
        pCall->pExtraHeaderNames[i] = xllm__strdup(pHeader->sName);
        pCall->pExtraHeaderValues[i] = xllm__strdup(pHeader->sValue);
        if (!pCall->pExtraHeaderNames[i] || !pCall->pExtraHeaderValues[i])
        {
            return false;
        }
    }
    return true;
}

/* Deep request copy for the pOnRequest seam: every owned field is
 * duplicated; borrowed storage (headers, cancel, deadline, hooks) rides
 * along as borrowed. Freed on call destroy. */
bool xllm__request_clone(xllm_request *pDst, const xllm_request *pSrc)
{
    size_t i;
    if (!pDst || !pSrc)
    {
        return false;
    }
    xllmRequestInit(pDst);
    /* The prefix stamp does not survive cloning: pOnRequest may mutate any
     * message, and the cached prefix bytes would no longer match. */
    pDst->pStablePrefixOwner = NULL;
    pDst->uStablePrefixStamp = 0u;
    pDst->iStableMessages = 0u;
    pDst->uReasoningBudgetTokens = pSrc->uReasoningBudgetTokens;
    pDst->uMaxOutputTokens = pSrc->uMaxOutputTokens;
    pDst->fTemperature = pSrc->fTemperature;
    pDst->bHasTemperature = pSrc->bHasTemperature;
    pDst->fTopP = pSrc->fTopP;
    pDst->bHasTopP = pSrc->bHasTopP;
    pDst->bParallelToolCalls = pSrc->bParallelToolCalls;
    pDst->eToolChoice = pSrc->eToolChoice;
    pDst->eJsonMode = pSrc->eJsonMode;
    pDst->bStream = pSrc->bStream;
    pDst->pExtraHeaders = pSrc->pExtraHeaders;
    pDst->iExtraHeaderCount = pSrc->iExtraHeaderCount;
    pDst->pCancel = pSrc->pCancel;
    pDst->uDeadline = pSrc->uDeadline;
    pDst->iTimeout = pSrc->iTimeout;
    pDst->pHooks = pSrc->pHooks;
    if ((pSrc->sModel && !(pDst->sModel = xllm__strdup(pSrc->sModel))) ||
        (pSrc->sReasoningEffort &&
         !(pDst->sReasoningEffort = xllm__strdup(pSrc->sReasoningEffort))) ||
        (pSrc->sNamedTool && !(pDst->sNamedTool = xllm__strdup(pSrc->sNamedTool))) ||
        (pSrc->sStop && !(pDst->sStop = xllm__strdup(pSrc->sStop))) ||
        (pSrc->sExtraBodyJson && !(pDst->sExtraBodyJson = xllm__strdup(pSrc->sExtraBodyJson))))
    {
        xllmRequestUnit(pDst);
        return false;
    }
    for (i = 0u; i < pSrc->iMessageCount; ++i)
    {
        if (!xllmRequestAddMessage(pDst, &pSrc->pMessages[i]))
        {
            xllmRequestUnit(pDst);
            return false;
        }
    }
    for (i = 0u; i < pSrc->iToolCount; ++i)
    {
        const xllm_tool *pTool = &pSrc->pTools[i];
        if (!xllmRequestAddTool(pDst, pTool->sName, pTool->sDescription, pTool->sParametersJson,
                                pTool->bStrict))
        {
            xllmRequestUnit(pDst);
            return false;
        }
    }
    return true;
}

/* Wire replacement contract: NUL-terminated, no embedded NUL. */
static bool xllm__wire_valid(const xllm_wire *pWire)
{
    return pWire->sBody != NULL && pWire->iBodySize == strlen(pWire->sBody) &&
           pWire->sBody[pWire->iBodySize] == '\0';
}

static xllm_call *xllm__client_start(xllm_client *pClient, const xllm_request *pRequest,
                                     const xllm_stream_callbacks *pCallbacks, uint32_t uAttempt,
                                     xllm_error *pError);

/* Wire-prefix serialization (尾账 #3): with a stamped view request and a
 * prefix-safe dialect, the cached messages-array bytes are reused and only
 * the delta is serialized; the accumulator persists across calls. Any miss
 * (owner/stamp/monotonic-count) falls back to a full rebuild through the
 * same incremental op with an empty prefix. */
char *xllm__client_serialize_body(xllm_client *pClient, const xllm_request *pRequest,
                                  xllm_error *pError)
{
    char *sBody;
    if (pClient->pDialect->BuildRequestCached && pRequest->pStablePrefixOwner)
    {
        bool bHit = pClient->pPrefixCacheOwner == pRequest->pStablePrefixOwner &&
                    pClient->uPrefixCacheStamp == pRequest->uStablePrefixStamp &&
                    pClient->iPrefixCacheMessages <= pRequest->iStableMessages;
        xllm_buf tInner;
        if (!bHit)
        {
            pClient->iPrefixCacheLen = 0u; /* accumulator resets; buffer kept */
            pClient->iPrefixCacheMessages = 0u;
            pClient->pPrefixCacheOwner = pRequest->pStablePrefixOwner;
            pClient->uPrefixCacheStamp = pRequest->uStablePrefixStamp;
        }
        tInner.pData = pClient->sPrefixCacheInner;
        tInner.iLen = pClient->iPrefixCacheLen;
        tInner.iCap = pClient->iPrefixCacheCap;
        sBody = pClient->pDialect->BuildRequestCached(pClient, pRequest, &tInner,
                                                      pClient->iPrefixCacheMessages, pError);
        if (!sBody)
        {
            /* Failure may have appended a partial delta to the accumulator;
             * the byte length is no longer trustworthy. Invalidate the count
             * so the next call rebuilds from scratch instead of sending a
             * truncated body spliced onto half-written bytes. */
            pClient->iPrefixCacheLen = 0u;
            pClient->iPrefixCacheMessages = 0u;
            pClient->sPrefixCacheInner = tInner.pData;
            pClient->iPrefixCacheCap = tInner.iCap;
            return NULL;
        }
        pClient->sPrefixCacheInner = tInner.pData;
        pClient->iPrefixCacheLen = tInner.iLen;
        pClient->iPrefixCacheCap = tInner.iCap;
        pClient->iPrefixCacheMessages = pRequest->iStableMessages;
        return sBody;
    }
    if (pClient->sPrefixCacheInner)
    {
        pClient->iPrefixCacheLen = 0u;
        pClient->iPrefixCacheMessages = 0u;
        pClient->pPrefixCacheOwner = NULL;
    }
    return pClient->pDialect->BuildRequest(pClient, pRequest, pError);
}

static xllm_call *xllm__client_start(xllm_client *pClient, const xllm_request *pRequest,
                                     const xllm_stream_callbacks *pCallbacks, uint32_t uAttempt,
                                     xllm_error *pError)
{
    xllm_call *pCall = NULL;
    char *sBody = NULL;
    const char *sModel;
    uint64_t tTimeout = XLLM_DEADLINE_NEVER;
    xllm_hooks tHooks;
    bool bHasHooks = false;
    const xllm_hooks *pHooks = NULL;
    xllm_request *pClone = NULL;
    const xllm_request *pEffective = pRequest;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pClient || !pRequest)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client and request are required");
        return NULL;
    }
    if (!xllm__client_begin_operation(pClient, pRequest->pHooks, &tHooks, &bHasHooks, pError))
        return NULL;
    pHooks = bHasHooks ? &tHooks : NULL;
    if (pHooks && pHooks->pOnRequest)
    {
        pClone = (xllm_request *)xllm__calloc(1u, sizeof(*pClone));
        if (!pClone || !xllm__request_clone(pClone, pRequest))
        {
            xllm__free(pClone);
            xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY,
                            "failed to clone the request for the pOnRequest hook");
            return NULL;
        }
        if (!pHooks->pOnRequest(pClient, pClone, pHooks->pUserData))
        {
            xllmRequestUnit(pClone);
            xllm__free(pClone);
            xllm__error_set(pError, XLLM_ERROR_HOOK, "request rejected by the pOnRequest hook");
            return NULL;
        }
        pEffective = pClone;
    }
    if (!xrtMutexLock(&pClient->ConfigMutex))
    {
        if (pClone)
        {
            xllmRequestUnit(pClone);
            xllm__free(pClone);
        }
        xllm__error_set(pError, XLLM_ERROR_PROTOCOL, "client configuration lock failed");
        return NULL;
    }
    if (pClient->bHasModelProfile &&
        !xllmModelProfileValidateRequest(&pClient->tModelProfile, pEffective, pError))
    {
        (void)xrtMutexUnlock(&pClient->ConfigMutex);
        if (pClone)
        {
            xllmRequestUnit(pClone);
            xllm__free(pClone);
        }
        return NULL;
    }
    sBody = xllm__client_serialize_body(pClient, pEffective, pError);
    (void)xrtMutexUnlock(&pClient->ConfigMutex);
    if (!sBody)
    {
        if (pError && pError->eCode == XLLM_ERROR_NONE)
        {
            xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to serialize model request");
        }
        if (pClone)
        {
            xllmRequestUnit(pClone);
            xllm__free(pClone);
        }
        return NULL;
    }
    if (pHooks && pHooks->pOnRequestBody)
    {
        xllm_wire tWire;
        memset(&tWire, 0, sizeof(tWire));
        tWire.uAttempt = uAttempt;
        tWire.sBody = sBody;
        tWire.iBodySize = strlen(sBody);
        tWire.iBodyCapacity = tWire.iBodySize + 1u;
        if (!pHooks->pOnRequestBody(pClient, &tWire, pHooks->pUserData) ||
            !xllm__wire_valid(&tWire))
        {
            if (tWire.sBody && tWire.sBody != sBody)
            {
                xllm__free(tWire.sBody);
            }
            xllm__free(sBody);
            sBody = NULL;
            xllm__error_set(pError, XLLM_ERROR_HOOK,
                            "request body rejected or malformed after the pOnRequestBody hook");
            if (pClone)
            {
                xllmRequestUnit(pClone);
                xllm__free(pClone);
            }
            return NULL;
        }
        if (tWire.sBody != sBody)
        {
            xllm__free(sBody);
            sBody = tWire.sBody;
        }
    }
    pCall = (xllm_call *)xllm__calloc(1u, sizeof(*pCall));
    if (!pCall)
        goto oom;
    pCall->pClient = pClient;
    pCall->pDialect = pClient->pDialect;
    pCall->bOpMutexInit = xrtMutexInit(&pCall->OpMutex);
    pCall->bTransportMutexInit = xrtMutexInit(&pCall->TransportMutex);
    pCall->uAttempt = uAttempt;
    pCall->bStreamWanted = pEffective->bStream;
    pCall->uMaxHttpHeadBytes = pClient->uMaxHttpHeadBytes;
    pCall->uMaxResponseBodyBytes = pClient->uMaxResponseBodyBytes;
    pCall->uMaxSseLineBytes = pClient->uMaxSseLineBytes;
    pCall->uMaxSseEventBytes = pClient->uMaxSseEventBytes;
    if (bHasHooks)
    {
        pCall->tHooks = tHooks;
        pCall->bHasHooks = true;
    }
    pCall->pClonedRequest = pClone;
    pCall->bDeferParse = pHooks && pHooks->pOnResponseBody != NULL;
    pClone = NULL;
    if (pCallbacks)
    {
        pCall->tCallbacks = *pCallbacks;
    }
    xllmErrorInit(&pCall->tError);
    sModel = (pEffective->sModel && pEffective->sModel[0]) ? pEffective->sModel : pClient->sModel;
    pCall->sSelectedModel = xllm__strdup(sModel);
    pCall->sRequestBody = sBody;
    sBody = NULL;
    pCall->uScopeDeadline = pEffective->uDeadline;
    pCall->bScopeAttached =
        pEffective->pCancel != NULL || pEffective->uDeadline != XLLM_DEADLINE_NEVER;
    pCall->pCancel = xrtCancelChild(pEffective->pCancel);
    if (pClient->uTimeoutMs)
    {
        tTimeout = xllmDeadlineAfterUs((uint64)pClient->uTimeoutMs * UINT64_C(1000));
    }
    pCall->uDeadline = pEffective->uDeadline;
    if (tTimeout != XLLM_DEADLINE_NEVER &&
        (pCall->uDeadline == XLLM_DEADLINE_NEVER || tTimeout < pCall->uDeadline))
    {
        pCall->uDeadline = tTimeout;
    }
    if (pCall->uDeadline != XLLM_DEADLINE_NEVER)
    {
        pCall->tHttpDiagnostics.uEffectiveTimeoutMs =
            xllmDeadlineRemainingUs(pCall->uDeadline) / UINT64_C(1000);
    }
    if (!pCall->bOpMutexInit || !pCall->bTransportMutexInit || !pCall->sSelectedModel ||
        !pCall->pCancel || !xllm__call_copy_extra_headers(pCall, pEffective))
        goto oom;
    xllm__free(sBody);
    sBody = NULL;
    if (pHooks && pHooks->pOnResponseBody)
    {
        /* Pre-send probe: sBody arrives NULL. Assigning a body short-
         * circuits the network entirely (offline replay); leaving it NULL
         * lets the call proceed and the hook fires again post-receive. */
        xllm_wire tProbe;
        memset(&tProbe, 0, sizeof(tProbe));
        tProbe.uAttempt = uAttempt;
        if (!pHooks->pOnResponseBody(pClient, &tProbe, pHooks->pUserData))
        {
            if (tProbe.sBody)
            {
                xllm__free(tProbe.sBody);
            }
            xllm__error_set(pError, XLLM_ERROR_HOOK,
                            "response body rejected before send by the pOnResponseBody hook");
            xllmCallDestroy(pCall);
            return NULL;
        }
        if (tProbe.sBody && xllm__wire_valid(&tProbe))
        {
            pCall->tRawBody.pData = tProbe.sBody;
            pCall->tRawBody.iLen = tProbe.iBodySize;
            pCall->tRawBody.iCap = tProbe.iBodySize + 1u;
            pCall->uHttpStatus = 200u;
            pCall->bOfflineBody = true;
            pCall->tHttpDiagnostics.eResult = XLLM_TRANSPORT_OK;
        }
        else if (tProbe.sBody)
        {
            xllm__free(tProbe.sBody);
            xllm__error_set(pError, XLLM_ERROR_HOOK,
                            "offline response body is malformed after the pOnResponseBody hook");
            xllmCallDestroy(pCall);
            return NULL;
        }
    }
    if (!pCall->bOfflineBody && !xllm__transport_arm_cancel(pCall))
        goto oom;
    pCall->pPromise = xrtPromiseCreate(&pCall->pFuture, pCall->pCancel);
    if (!pCall->pPromise)
        goto oom;
    if (pCall->bOfflineBody)
    {
        xllm__transport_finish(pCall, XLLM_TRANSPORT_OK);
        return pCall;
    }
    xllm__transport_begin(pCall);
    return pCall;
oom:
    xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate model call");
    xllm__free(sBody);
    if (pClone)
    {
        xllmRequestUnit(pClone);
        xllm__free(pClone);
    }
    xllmCallDestroy(pCall);
    return NULL;
}

xfuture *xllmCallFuture(xllm_call *pCall) { return pCall ? pCall->pFuture : NULL; }

xllm_result xllmCallWait(xllm_call *pCall, xllm_response **ppResponse, xllm_error *pError)
{
    xllm_result eResult = XLLM_RESULT_ERROR;
    bool bParsed = false;
    const xllm_hooks *pHooks = pCall && pCall->bHasHooks ? &pCall->tHooks : NULL;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (ppResponse)
    {
        *ppResponse = NULL;
    }
    if (!pCall || !ppResponse || pCall->bWaited)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "call can only be waited once");
        return XLLM_RESULT_ERROR;
    }
    pCall->bWaited = true;
    /* The transport runs on the engine workers; this wait honours the call
     * deadline and cancel token. A timeout or cancellation aborts the
     * in-flight transport before returning. */
    {
        xwaitresult eWait = xrtFutureWaitForCancel(
            pCall->pFuture, xllmDeadlineWaitMs(pCall->uDeadline), pCall->pCancel);
        if (eWait == XWAIT_TIMEOUT)
        {
            xllm__transport_abort(pCall);
            /* The watchdog may not have landed first; unify the transport
             * result so diagnostics and retry policy see a deadline hit. */
            if (pCall->tHttpDiagnostics.eResult != XLLM_TRANSPORT_TIMEOUT)
            {
                pCall->tHttpDiagnostics.eResult = XLLM_TRANSPORT_TIMEOUT;
                xllm__copy_text(pCall->tHttpDiagnostics.sError,
                                sizeof(pCall->tHttpDiagnostics.sError), "timeout");
            }
            if (pCall->tError.eCode == XLLM_ERROR_NONE)
            {
                xllm__error_set(&pCall->tError, XLLM_ERROR_TIMEOUT, "model request timed out");
            }
            pCall->eTransportResult = XLLM_TRANSPORT_TIMEOUT;
            eResult = XLLM_RESULT_TIMEOUT;
            goto done;
        }
        if (eWait == XWAIT_CANCELLED)
        {
            xllm__transport_abort(pCall);
            if (pCall->tHttpDiagnostics.eResult != XLLM_TRANSPORT_CANCELLED &&
                pCall->tHttpDiagnostics.eResult != XLLM_TRANSPORT_OK)
            {
                pCall->tHttpDiagnostics.eResult = XLLM_TRANSPORT_CANCELLED;
                xllm__copy_text(pCall->tHttpDiagnostics.sError,
                                sizeof(pCall->tHttpDiagnostics.sError), "cancelled");
            }
            if (pCall->tError.eCode == XLLM_ERROR_NONE)
            {
                xllm__error_set(&pCall->tError, XLLM_ERROR_CANCELLED,
                                "model request was cancelled");
            }
            pCall->eTransportResult = XLLM_TRANSPORT_CANCELLED;
            eResult = XLLM_RESULT_CANCELLED;
            goto done;
        }
    }

    if (pCall->bCallbackCancelled)
    {
        eResult = XLLM_RESULT_CANCELLED;
        goto done;
    }
    if (pCall->tError.eCode != XLLM_ERROR_NONE)
    {
        eResult =
            pCall->tError.eCode == XLLM_ERROR_CANCELLED ? XLLM_RESULT_CANCELLED : XLLM_RESULT_ERROR;
        goto done;
    }
    if (pCall->eTransportResult != XLLM_TRANSPORT_OK)
    {
        pCall->tError.iTransportStatus = (int32_t)pCall->eTransportResult;
        if (pCall->eTransportResult == XLLM_TRANSPORT_TIMEOUT)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_TIMEOUT, "model request timed out");
            eResult = XLLM_RESULT_TIMEOUT;
        }
        else if (pCall->eTransportResult == XLLM_TRANSPORT_CANCELLED)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_CANCELLED, "model request was cancelled");
            eResult = XLLM_RESULT_CANCELLED;
        }
        else
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_NETWORK, "model request failed");
        }
        goto done;
    }
    if (pCall->uHttpStatus < 200u || pCall->uHttpStatus >= 300u)
    {
        xllm__map_http_error(pCall);
        goto done;
    }
    if (pCall->bOfflineBody && pCall->bStreamWanted && pCall->tRawBody.iLen >= 5u &&
        memcmp(pCall->tRawBody.pData, "data:", 5u) == 0)
    {
        pCall->bSse = true; /* canned SSE replay parses through the same path */
    }
    if (pHooks && pHooks->pOnResponseBody && !pCall->bOfflineBody)
    {
        xllm_wire tWire;
        char *sOld = pCall->tRawBody.pData;
        memset(&tWire, 0, sizeof(tWire));
        tWire.uAttempt = pCall->uAttempt;
        tWire.uHttpStatus = pCall->uHttpStatus;
        tWire.sBody = sOld ? sOld : (char *)"";
        tWire.iBodySize = pCall->tRawBody.iLen;
        if (!pHooks->pOnResponseBody(pCall->pClient, &tWire, pHooks->pUserData) ||
            !xllm__wire_valid(&tWire))
        {
            if (tWire.sBody && tWire.sBody != sOld)
            {
                xllm__free(tWire.sBody);
            }
            xllm__error_set(&pCall->tError, XLLM_ERROR_HOOK,
                            "response body rejected or malformed after the pOnResponseBody hook");
            goto done;
        }
        if (tWire.sBody != sOld)
        {
            xllm__free(sOld);
            pCall->tRawBody.pData = tWire.sBody;
            pCall->tRawBody.iLen = tWire.iBodySize;
            pCall->tRawBody.iCap = tWire.iBodySize + 1u;
        }
    }
    if (pCall->bSse)
    {
        if (pCall->bDeferParse && pCall->tRawBody.iLen &&
            !xllm__sse_feed(pCall, pCall->tRawBody.pData, pCall->tRawBody.iLen))
        {
            bParsed = false;
        }
        else
        {
            bParsed = xllm__sse_finish(pCall);
            /* EOF only completes HTTP framing. A model stream must carry a
             * terminal event; otherwise even syntactically valid partial
             * tool arguments must never be dispatched. Chat-compatible
             * providers may omit [DONE] after an explicit finish_reason. */
            if (bParsed && !pCall->bDone &&
                !(pCall->pDialect == xllm__dialect_completions() && pCall->pResponse &&
                  pCall->pResponse->sFinishReason && pCall->pResponse->sFinishReason[0]))
            {
                xllm__error_set(&pCall->tError, XLLM_ERROR_INCOMPLETE_RESPONSE,
                                "provider event stream ended before its terminal event");
                bParsed = false;
            }
        }
    }
    else if (pCall->tRawBody.iLen)
    {
        bParsed = pCall->pDialect->DecodeJsonBody(
            pCall, (xstrview){pCall->tRawBody.pData, pCall->tRawBody.iLen});
    }
    if (!bParsed)
    {
        if (pCall->tError.eCode == XLLM_ERROR_NONE)
        {
            xllm__error_set(&pCall->tError, XLLM_ERROR_PROTOCOL,
                            "provider returned no usable response payload");
        }
        goto done;
    }
    if (!xllm__assemble_finalize(pCall))
    {
        eResult = pCall->bCallbackCancelled ? XLLM_RESULT_CANCELLED : XLLM_RESULT_ERROR;
        goto done;
    }
    if (pHooks && pHooks->pOnResponse && pCall->pResponse &&
        !pHooks->pOnResponse(pCall->pClient, pCall->pResponse, pHooks->pUserData))
    {
        xllm__error_set(&pCall->tError, XLLM_ERROR_HOOK,
                        "response rejected by the pOnResponse hook");
        eResult = XLLM_RESULT_ERROR;
        goto done;
    }
    *ppResponse = pCall->pResponse;
    pCall->pResponse = NULL;
    pCall->bResponseTaken = true;
    eResult = XLLM_RESULT_OK;

done:
    xllm__capture_diagnostics(pCall, &pCall->tError.tDiagnostics);
    pCall->tError.iTransportStatus = pCall->tError.tDiagnostics.iTransportStatus;
    pCall->tError.tDiagnostics.bRetryable = xllmErrorRetryable(&pCall->tError);
    if (ppResponse && *ppResponse)
    {
        memcpy(&(*ppResponse)->tDiagnostics, &pCall->tError.tDiagnostics, sizeof(xllm_diagnostics));
        xllm__capture_stats(pCall, *ppResponse);
    }
    xllm__error_copy(pError, &pCall->tError);
    return eResult;
}

bool xllmCallCancel(xllm_call *pCall)
{
    if (!pCall || !pCall->pCancel)
    {
        return false;
    }
    return xrtCancelRequest(pCall->pCancel);
}

void xllmCallDestroy(xllm_call *pCall)
{
    size_t i;
    if (!pCall)
    {
        return;
    }
    xllm__atomic_store(&pCall->iClosing, 1);
    if (pCall->pCancel)
    {
        (void)xrtCancelRequest(pCall->pCancel);
    }
    xllm__transport_abort(pCall);
    xllm__op_disarm(pCall);
    if (pCall->pCancelWatch)
    {
        (void)xrtCancelUnwatch(pCall->pCancelWatch);
        pCall->pCancelWatch = NULL;
    }
    while (xllm__atomic_load(&pCall->iTransportActive) != 0 ||
           xllm__atomic_load(&pCall->iCallbackActive) != 0 ||
           xllm__atomic_load(&pCall->iTimerRefs) != 0)
    {
        xrtSleep(1u);
    }
    if (pCall->pClonedRequest)
    {
        xllmRequestUnit(pCall->pClonedRequest);
        xllm__free(pCall->pClonedRequest);
    }
    xllmResponseDestroy(pCall->pResponse);
    xllm__buf_reset(&pCall->tLine);
    xllm__buf_reset(&pCall->tEventData);
    xllm__buf_reset(&pCall->tEventName);
    xllm__buf_reset(&pCall->tRawBody);
    xllm__free(pCall->pToolState);
    xllm__free(pCall->pBlockState);
    xllm__free(pCall->pBlockMap);
    xllm__free(pCall->pItemMap);
    if (pCall->pExtraHeaderNames)
    {
        for (i = 0u; i < pCall->iExtraHeaderCount; ++i)
        {
            xllm__free(pCall->pExtraHeaderNames[i]);
        }
        xllm__free(pCall->pExtraHeaderNames);
    }
    if (pCall->pExtraHeaderValues)
    {
        for (i = 0u; i < pCall->iExtraHeaderCount; ++i)
        {
            if (pCall->pExtraHeaderValues[i])
            {
                volatile char *pSecret = (volatile char *)pCall->pExtraHeaderValues[i];
                size_t iLen = strlen(pCall->pExtraHeaderValues[i]);
                while (iLen--)
                {
                    pSecret[iLen] = 0;
                }
            }
            xllm__free(pCall->pExtraHeaderValues[i]);
        }
        xllm__free(pCall->pExtraHeaderValues);
    }
    xrtCancelDestroy(pCall->pCancel);
    xllm__free(pCall->sRequestBody);
    xllm__free(pCall->sSelectedModel);
    xllm__free(pCall->sRequestHeader);
    xllm__buf_reset(&pCall->tWire);
    if (pCall->pPromise)
    {
        xrtPromiseDestroy(pCall->pPromise);
    }
    if (pCall->pFuture)
    {
        xrtFutureDestroy(pCall->pFuture);
    }
    if (pCall->bTransportMutexInit)
    {
        (void)xrtMutexUnit(&pCall->TransportMutex);
    }
    if (pCall->bOpMutexInit)
    {
        (void)xrtMutexUnit(&pCall->OpMutex);
    }
    xllm__free(pCall);
}

static xllm_result xllm__client_complete(xllm_client *pClient, const xllm_request *pRequest,
                                         const xllm_stream_callbacks *pCallbacks,
                                         xllm_response **ppResponse, xllm_error *pError)
{
    xllm_call *pCall;
    xllm_error tAttemptError;
    xllm_result eResult = XLLM_RESULT_ERROR;
    uint32_t uAttempt;
    uint32_t uMaxAttempts;
    xllm_request tOperationRequest;
    xllm_hooks tOperationHooks;
    bool bHasOperationHooks = false;
    if (ppResponse)
    {
        *ppResponse = NULL;
    }
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pClient || !pRequest || !ppResponse)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT,
                        "client, request, and response output are required");
        return XLLM_RESULT_ERROR;
    }
    if (!xllm__client_begin_operation(pClient, pRequest->pHooks, &tOperationHooks,
                                      &bHasOperationHooks, pError))
    {
        return XLLM_RESULT_ERROR;
    }
    tOperationRequest = *pRequest;
    tOperationRequest.pHooks = bHasOperationHooks ? &tOperationHooks : NULL;
    pRequest = &tOperationRequest;
    uMaxAttempts = pClient->uMaxAttempts ? pClient->uMaxAttempts : 1u;
    for (uAttempt = 1u; uAttempt <= uMaxAttempts; ++uAttempt)
    {
        uint32_t uDelayMs;
        bool bCanRetry;
        bool bWillRetry;
        xllmErrorInit(&tAttemptError);
        if (xllm__scope_result(pRequest->pCancel, pRequest->uDeadline, &tAttemptError) !=
            XLLM_RESULT_OK)
        {
            xllm__error_copy(pError, &tAttemptError);
            return tAttemptError.eCode == XLLM_ERROR_CANCELLED ? XLLM_RESULT_CANCELLED
                                                               : XLLM_RESULT_TIMEOUT;
        }
        pCall = xllm__client_start(pClient, pRequest, pCallbacks, uAttempt, &tAttemptError);
        if (!pCall)
        {
            xllm__error_copy(pError, &tAttemptError);
            return XLLM_RESULT_ERROR;
        }
        pCall->uAttempt = uAttempt;
        eResult = xllmCallWait(pCall, ppResponse, &tAttemptError);
        xllmCallDestroy(pCall);
        if (eResult == XLLM_RESULT_OK)
        {
            xllm__error_copy(pError, &tAttemptError);
            return eResult;
        }
        if ((pRequest->pCancel && xrtCancelRequested(pRequest->pCancel)) ||
            (pRequest->uDeadline != XLLM_DEADLINE_NEVER &&
             xllmDeadlineExpired(pRequest->uDeadline)))
        {
            tAttemptError.tDiagnostics.bContextAttached = true;
            tAttemptError.tDiagnostics.uContextDeadlineMs =
                pRequest->uDeadline == XLLM_DEADLINE_NEVER ? 0u
                                                           : pRequest->uDeadline / UINT64_C(1000);
            xllm__copy_text(tAttemptError.tDiagnostics.sContextStatus,
                            sizeof(tAttemptError.tDiagnostics.sContextStatus),
                            pRequest->pCancel && xrtCancelRequested(pRequest->pCancel)
                                ? "cancelled"
                                : "deadline_exceeded");
            xllm__error_copy(pError, &tAttemptError);
            return pRequest->pCancel && xrtCancelRequested(pRequest->pCancel)
                       ? XLLM_RESULT_CANCELLED
                       : XLLM_RESULT_TIMEOUT;
        }
        bCanRetry = !tAttemptError.tDiagnostics.bModelDataDelivered &&
                    (xllmErrorRetryable(&tAttemptError) ||
                     (pClient->pDialect->IsRetryableProviderError &&
                      pClient->pDialect->IsRetryableProviderError(&tAttemptError)));
        bWillRetry = bCanRetry && uAttempt < uMaxAttempts;
        if (bWillRetry)
        {
            const xllm_hooks *pHooks = pRequest->pHooks;
            if (pHooks && pHooks->pOnRetry &&
                !pHooks->pOnRetry(pClient, &tAttemptError.tDiagnostics, uAttempt + 1u,
                                  pHooks->pUserData))
            {
                tAttemptError.tDiagnostics.bRetryable = false;
                xllm__error_copy(pError, &tAttemptError);
                return eResult;
            }
        }
        tAttemptError.tDiagnostics.bRetryable = bWillRetry;
        tAttemptError.tDiagnostics.bRetryExhausted =
            bCanRetry && !bWillRetry && uAttempt >= uMaxAttempts;
        if (!bWillRetry)
        {
            xllm__error_copy(pError, &tAttemptError);
            return eResult;
        }
        uDelayMs =
            xllm__retry_delay_ms(pClient, uAttempt, tAttemptError.tDiagnostics.uRetryAfterMs);
        if (uDelayMs > 0u && !xllm__scope_sleep(pRequest->pCancel, pRequest->uDeadline, uDelayMs))
        {
            eResult = xllm__scope_result(pRequest->pCancel, pRequest->uDeadline, &tAttemptError);
            xllm__error_copy(pError, &tAttemptError);
            return eResult;
        }
    }
    xllm__error_copy(pError, &tAttemptError);
    return eResult;
}

char *xllmClientBuildRequestJson(xllm_client *pClient, const xllm_request *pRequest,
                                 xllm_error *pError)
{
    char *sBody;
    if (pError)
    {
        xllmErrorInit(pError);
    }
    if (!pClient || !pRequest)
    {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "client and request are required");
        return NULL;
    }
    if (!pClient->bConfigMutexInit || !xrtMutexLock(&pClient->ConfigMutex))
    {
        xllm__error_set(pError, XLLM_ERROR_PROTOCOL, "client configuration lock failed");
        return NULL;
    }
    pClient->bConfigSealed = true;
    if (pClient->bHasModelProfile &&
        !xllmModelProfileValidateRequest(&pClient->tModelProfile, pRequest, pError))
    {
        (void)xrtMutexUnlock(&pClient->ConfigMutex);
        return NULL;
    }
    sBody = xllm__client_serialize_body(pClient, pRequest, pError);
    (void)xrtMutexUnlock(&pClient->ConfigMutex);
    if (!sBody && pError && pError->eCode == XLLM_ERROR_NONE)
    {
        xllm__error_set(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to serialize model request");
    }
    return sBody;
}

static xllm_request xllm__relative_request(const xllm_request *request)
{
    xllm_request effective = *request;
    if (effective.iTimeout >= 0)
    {
        uint64_t ms = (uint64_t)effective.iTimeout;
        uint64_t end =
            xllmDeadlineAfterUs(ms > (UINT64_MAX - 1u) / 1000u ? UINT64_MAX - 1u : ms * 1000u);
        if (effective.uDeadline == XLLM_DEADLINE_NEVER || end < effective.uDeadline)
            effective.uDeadline = end;
    }
    effective.iTimeout = XRT_WAIT_FOREVER;
    return effective;
}
xllm_call *xllmClientStart(xllm_client *client, const xllm_request *request,
                           const xllm_stream_callbacks *callbacks, xllm_error *error)
{
    xllm_request effective;
    if (!request)
        return xllm__client_start(client, request, callbacks, 1u, error);
    effective = xllm__relative_request(request);
    return xllm__client_start(client, &effective, callbacks, 1u, error);
}
xllm_result xllmClientComplete(xllm_client *client, const xllm_request *request,
                               const xllm_stream_callbacks *callbacks, xllm_response **response,
                               xllm_error *error)
{
    xllm_request effective;
    if (!request)
        return xllm__client_complete(client, request, callbacks, response, error);
    effective = xllm__relative_request(request);
    return xllm__client_complete(client, &effective, callbacks, response, error);
}
#endif

#endif
