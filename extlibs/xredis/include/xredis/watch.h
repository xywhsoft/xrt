#ifndef XREDIS_WATCH_H
#define XREDIS_WATCH_H
#include <xredis/transaction.h>

#if defined(XREDIS_FEATURE_XREDIS_WATCH) && !defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS_WATCH requires XREDIS_FEATURE_XREDIS_TRANSACTION"
#endif
#if defined(XREDIS_FEATURE_XREDIS_WATCH)
typedef struct xrediswatch xrediswatch;
typedef struct xrediswatchconfig {
    size_t Size;
    /* Positive total lifetime, including admission, application think time,
     * reads and commit. No renewal; at most 24 hours. Default 5000 ms. */
    int64 Timeout;
    /* Separate bounded UNWATCH request deadline, default 5000 ms (max 60 s).
     * Physical retirement also includes the client's existing drain grace. */
    int64 CleanupTimeout;
    size_t MaxKeys;
    size_t MaxWireBytes;
    xredistransactionconfig Transaction;
} xrediswatchconfig;

/* Owned by the start Future. Watch is borrowed on EVERY accepted outcome;
 * only OK permits commands. On failure it still exposes the physical Closed
 * barrier. Retain it before destroying the Future. A retained completed result
 * does not pin the engine. Request.Reply is the WATCH response, not EXEC. */
typedef struct xrediswatchresult {
    xredisrequestresult Request;
    xrediswatch* Watch;
} xrediswatchresult;
/* Owned by Closed's Future. Reason is the scope's first terminal reason;
 * Reusable certifies physical idle plus a validated clearing boundary. */
typedef struct xrediswatchclosedresult {
    xredisrequeststatus Reason;
    bool Reusable;
} xrediswatchclosedresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisWatchConfigInit(xrediswatchconfig* Config);
/* Claims an OPEN, ready, idle managed client. A busy/not-ready/already claimed
 * client returns OVERLOADED. Ordinary ClientCall/Transaction admissions are
 * refused while claimed. Keys are binary and copied only during admission.
 * Count must be 1..MaxKeys (default 256, hard limit 65536); default wire 64 KiB.
 * Config is copied; output changes only on ACCEPTED. No implicit retry.
 * Cancelling a pending start Future abandons the scope. */
XREDIS_API xredisadmission xredisClientWatchAsync(xredisclient* Client,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API xredisadmission xredisClientWatch(xredisclient* Client,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API bool xredisWatchRetain(xrediswatch* Watch);
XREDIS_API void xredisWatchRelease(xrediswatch* Watch);
/* Ordinary one-reply reads/commands within this connection's WATCH scope.
 * Connection/mode/transaction controls are reserved. The effective deadline
 * is min(call entry + Timeout, scope deadline). Release alone does not abort.
 * Application writes have their ordinary Redis effects and are not rolled back. */
XREDIS_API xredisadmission xredisWatchCallAsync(xrediswatch* Watch,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisWatchCall(xrediswatch* Watch,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
/* Exactly one accepted commit seals the scope against new calls/commits.
 * Earlier reads drain before MULTI. Result is xredistransactionresult; a null
 * EXEC is ABORTED, never an automatic retry. Logical completion precedes
 * physical Closed. Cancelling an accepted commit may retire the connection. */
XREDIS_API xredisadmission xredisWatchCommitAsync(xrediswatch* Watch,
    const xredistransactioncommand* Commands, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisWatchCommit(xrediswatch* Watch,
    const xredistransactioncommand* Commands, size_t Count, int64 Timeout, xfuture** Future);
/* Idempotent nonblocking abandonment. Idle scopes send UNWATCH; abandoning
 * while physical requests remain may retire the connection. No dirty reuse.
 * Closed is a retained observer, not a request to abort. It resolves only
 * after scoped request storage, timers, notifications and the claim retire.
 * Sync facades reject every worker of the client's engine. Stop/join the
 * engine before unloading code, including the final notification return tail. */
XREDIS_API bool xredisWatchAbort(xrediswatch* Watch);
XREDIS_API xfuture* xredisWatchClosed(xrediswatch* Watch);
XRT_EXTERN_C_END
#endif
#endif
