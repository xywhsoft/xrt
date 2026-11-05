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
#include <xllm/api.h>
#else
#include <xllm/api.h>
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
