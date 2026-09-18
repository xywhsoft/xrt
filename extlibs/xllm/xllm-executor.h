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

#if defined(__TINYC__)
#include <xllm.h>
#else
#include "xllm.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xllm_executor xllm_executor;

/* Per-call execution context, supplied by whoever drives the loop. */
typedef struct xllm_executor_ctx {
    /* Borrowed cooperative cancel token; NULL when the host has none. */
    xcancel* pCancel;
    /* Absolute xrtClock() deadline in microseconds; 0 and UINT64_MAX both
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
 * is acceptable); the driving loop copies them into the session. */
typedef struct xllm_executor_result {
    char* sContent;
    bool bSuccess;
    uint32_t uReserved[4];
} xllm_executor_result;

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
    uint32_t uReserved[4];
};

#ifdef __cplusplus
}
#endif

#endif
