#ifndef XLLM_MEMORY_BRIDGE_H
#define XLLM_MEMORY_BRIDGE_H

#if defined(XLLM_MEMORY_BRIDGE_IMPLEMENTATION) && !defined(XLLM_IMPLEMENTATION)
#define XLLM__MEMORY_BRIDGE_BRIDGED_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#endif

#include "xllm-session.h"
#include "xllm-memory.h"

#ifdef XLLM__MEMORY_BRIDGE_BRIDGED_IMPLEMENTATION
#undef XLLM_IMPLEMENTATION
#undef XLLM__MEMORY_BRIDGE_BRIDGED_IMPLEMENTATION
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool bSearchBeforeChat;
    bool bIngestAfterChat;
    xllm_memory_turn_search_apply_options tSearch;
    xllm_memory_ingest_turn_response_options tIngest;
    xvalue tVendorExtra;
} xllm_memory_chat_bridge_options;

XLLM_API void xllm_memory_chat_bridge_options_init(xllm_memory_chat_bridge_options *pOptions);

XLLM_API int xllm_memory_bridge_send(
    xllm_memory *pMemory,
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_memory_bridge_send_ex(
    xllm_memory *pMemory,
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);

XLLM_API xfuture *xllm_memory_bridge_send_async_thread(
    xllm_memory *pMemory,
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
);

XLLM_API xfuture *xllm_memory_bridge_send_async_engine(
    xllm_memory *pMemory,
    xllm *pLlm,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
);

XLLM_API xfuture *xllm_memory_bridge_send_async_co(
    xllm_memory *pMemory,
    xllm *pLlm,
    xcosched *pSched,
    const xllm_turn *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    size_t iStackSize
);

XLLM_API int xllm_memory_bridge_session_chat(
    xllm_memory *pMemory,
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_memory_bridge_session_chat_ex(
    xllm_memory *pMemory,
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);

XLLM_API xfuture *xllm_memory_bridge_session_chat_async_thread(
    xllm_memory *pMemory,
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
);

XLLM_API xfuture *xllm_memory_bridge_session_chat_async_engine(
    xllm_memory *pMemory,
    xllm_session *pSession,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions
);

XLLM_API xfuture *xllm_memory_bridge_session_chat_async_co(
    xllm_memory *pMemory,
    xllm_session *pSession,
    xcosched *pSched,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pCallOptions,
    const xllm_memory_chat_bridge_options *pBridgeOptions,
    size_t iStackSize
);

#ifdef __cplusplus
}
#endif

#if defined(XLLM_IMPLEMENTATION) || defined(XLLM_MEMORY_BRIDGE_IMPLEMENTATION)
#include "src/xllm_memory/xllm_memory_bridge.c"
#endif

#endif
