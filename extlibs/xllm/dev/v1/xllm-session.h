#ifndef XLLM_SESSION_H
#define XLLM_SESSION_H

#if defined(XLLM_SESSION_IMPLEMENTATION) && !defined(XLLM_IMPLEMENTATION)
#define XLLM__SESSION_BRIDGED_IMPLEMENTATION
#define XLLM_IMPLEMENTATION
#endif

#ifndef XLLM__WITH_SESSION
#define XLLM__WITH_SESSION 1
#define XLLM__SESSION_DEFINED_WITH_SESSION
#endif

#include "xllm.h"

#ifdef XLLM__SESSION_BRIDGED_IMPLEMENTATION
#undef XLLM_IMPLEMENTATION
#undef XLLM__SESSION_BRIDGED_IMPLEMENTATION
#endif

#ifdef XLLM__SESSION_DEFINED_WITH_SESSION
#undef XLLM__WITH_SESSION
#undef XLLM__SESSION_DEFINED_WITH_SESSION
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xllm xllm;
typedef struct xllm_session xllm_session;
typedef struct xllm_session_state xllm_session_state;

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
XLLM_API int xllm_turn_clone(xllm_turn *pOut, const xllm_turn *pIn);
XLLM_API void xllm_session_options_init(xllm_session_options *pOptions);
XLLM_API void xllm_compact_options_init(xllm_compact_options *pOptions);

XLLM_API xllm *xllm_create(xllm_runtime *pRuntime, const xllm_create_options *pOptions);
XLLM_API void xllm_destroy(xllm *pLlm);

XLLM_API int xllm_bind_profile(xllm *pLlm, const char *sProfileId);
XLLM_API int xllm_set_system_prompt(xllm *pLlm, const char *sText);
XLLM_API const char *xllm_get_system_prompt(const xllm *pLlm);
XLLM_API int xllm_set_tool_executor(xllm *pLlm, const xllm_tool_executor *pExecutor);
XLLM_API int xllm_set_tool_executor_async(xllm *pLlm, const xllm_tool_executor_async *pExecutor);

XLLM_API int xllm_turn_set_system_prompt(xllm_turn *pTurn, const char *sText);
XLLM_API int xllm_turn_set_system_mode(xllm_turn *pTurn, xllm_system_mode eMode);
XLLM_API int xllm_turn_set_tool_choice(
    xllm_turn *pTurn,
    xllm_tool_choice_mode eMode,
    const char *sToolName,
    bool bAllowParallel
);
XLLM_API int xllm_turn_set_stop_sequences(xllm_turn *pTurn, const char **psStop, size_t iStopCount);
XLLM_API int xllm_turn_set_json_schema_response(
    xllm_turn *pTurn,
    const char *sSchemaName,
    xvalue tJsonSchema,
    xvalue tVendorExtra
);
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);
XLLM_API int xllm_turn_add_image_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType);
XLLM_API int xllm_turn_add_image_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType);
XLLM_API int xllm_turn_add_image_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType);
XLLM_API int xllm_turn_add_file_url(xllm_turn *pTurn, const char *sUrl, const char *sMimeType);
XLLM_API int xllm_turn_add_file(xllm_turn *pTurn, const char *sPath, const char *sMimeType);
XLLM_API int xllm_turn_add_file_file_id(xllm_turn *pTurn, const char *sFileId, const char *sMimeType);
XLLM_API int xllm_turn_add_tool(xllm_turn *pTurn, const xllm_tool_def *pTool);

XLLM_API int xllm_send(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_send_ex(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);

XLLM_API xfuture *xllm_send_async_thread(
    xllm *pLlm,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions
);

XLLM_API xfuture *xllm_send_async_engine(
    xllm *pLlm,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions
);

XLLM_API xfuture *xllm_send_async_co(
    xllm *pLlm,
    xcosched *pSched,
    const xllm_turn *pTurn,
    const xllm_call_options *pOptions,
    size_t iStackSize
);

XLLM_API int xllm_session_create(
    xllm_runtime *pRuntime,
    const xllm_session_options *pOptions,
    xllm_session **ppSession
);

XLLM_API void xllm_session_destroy(xllm_session *pSession);
XLLM_API int xllm_session_set_system_prompt(xllm_session *pSession, const char *sText);
XLLM_API int xllm_session_clear_history(xllm_session *pSession);
XLLM_API int xllm_session_set_tool_executor(xllm_session *pSession, const xllm_tool_executor *pExecutor);
XLLM_API int xllm_session_set_tool_executor_async(xllm_session *pSession, const xllm_tool_executor_async *pExecutor);

XLLM_API int xllm_session_compact(
    xllm_session *pSession,
    const xllm_compact_options *pOptions,
    xllm_compact_result *pResult
);

XLLM_API int xllm_session_chat(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
);

XLLM_API int xllm_session_chat_ex(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
);

XLLM_API xfuture *xllm_session_chat_async_thread(
    xllm_session *pSession,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions
);

XLLM_API xfuture *xllm_session_chat_async_engine(
    xllm_session *pSession,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions
);

XLLM_API xfuture *xllm_session_chat_async_co(
    xllm_session *pSession,
    xcosched *pSched,
    const xllm_turn_request *pTurn,
    const xllm_call_options *pOptions,
    size_t iStackSize
);

XLLM_API int xllm_session_export_state(
    xllm_session *pSession,
    xllm_session_state **ppState
);

XLLM_API int xllm_session_import_state(
    xllm_runtime *pRuntime,
    const xllm_session_state *pState,
    xllm_session **ppSession
);

XLLM_API int xllm_session_state_to_xvalue(
    const xllm_session_state *pState,
    xvalue *ptValue
);

XLLM_API int xllm_session_state_from_xvalue(
    xvalue tValue,
    xllm_session_state **ppState
);

XLLM_API void xllm_session_state_free(xllm_session_state *pState);

#ifdef __cplusplus
}
#endif

#if defined(XLLM_IMPLEMENTATION) || defined(XLLM_SESSION_IMPLEMENTATION)
#include "src/xllm_session/xllm_session.c"
#endif

#endif
