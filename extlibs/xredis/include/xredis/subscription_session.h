#ifndef XREDIS_SUBSCRIPTION_SESSION_H
#define XREDIS_SUBSCRIPTION_SESSION_H
#include <xredis/subscriptions.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#error "Subscription sessions require desired-state replay"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
typedef struct xredissubscriptionsession xredissubscriptionsession;
typedef enum xredissubscriptionsessionstate {
    XREDIS_SESSION_CONNECTING=0,XREDIS_SESSION_RESTORING,XREDIS_SESSION_ONLINE,
    XREDIS_SESSION_UPDATING,XREDIS_SESSION_DRAINING,XREDIS_SESSION_BACKOFF,
    XREDIS_SESSION_CLOSING,XREDIS_SESSION_CLOSED
} xredissubscriptionsessionstate;
typedef struct xredissubscriptionsessioninfo {
    size_t Size;xredissubscriptionsessionstate State;
    uint64 Attempts,Failures,Generations,Restored,AppliedRevision,DesiredRevision;
    xredisrequeststatus LastFailure;double NextAttempt;
} xredissubscriptionsessioninfo;
/* Serialized on the controller worker, outside gates; must not block. Success
 * transfers one private managed Client reference. Failure transfers nothing.
 * Use the provided total setup limit with numeric/DNS/prepared/TLS/auth factory.
 * Every generation must use the same Engine and RESP/service/security profile. */
typedef bool (*xredissubscriptionsessionconnect)(xnetengine* Engine,uint64 Attempt,
    int64 Timeout,ptr Data,xredisclient** Client);
/* Coalesced state snapshots, not a complete transition log. Reentry is allowed.
 * Called on the controller worker; retain escaping objects explicitly. Data and
 * callback code remain resident through Drop. No per-message callback is added. */
typedef void (*xredissubscriptionsessionnotify)(xredissubscriptionsession* Session,
    const xredissubscriptionsessioninfo* Info,ptr Data);
typedef void (*xredissubscriptionsessiondrop)(ptr Data);
typedef struct xredissubscriptionsessionconfig {
    size_t Size;xredissubscriptionsessionconnect Connect;
    xredissubscriptionsessionnotify Notify;xredissubscriptionsessiondrop Drop;ptr Data;
    xredisprotocol Protocol;xredissubscriptionreplayconfig Replay;
    int64 AttemptTimeout,RetryMin,RetryMax;uint32 MaxFailures;
} xredissubscriptionsessionconfig;
/* Terminal stop reason (explicit Close is OK); LastFailure remains historical. */
typedef struct xredissubscriptionsessionclosed {
    xredisrequeststatus Status;uint64 Attempts,Generations;
} xredissubscriptionsessionclosed;
typedef struct xredissubscriptionsessioncurrent {
    xredisclient* Client;uint64 Generation,Revision;
} xredissubscriptionsessioncurrent;
XRT_EXTERN_C_BEGIN
/* RESP3, 5s per complete factory/readiness/restore attempt, 100ms..10s jittered
 * backoff, unlimited consecutive transient failures (MaxFailures=0). Positive
 * MaxFailures stops at that bound. Replay.Timeout is ignored: initial restore
 * shares the attempt's original deadline; each live diff gets AttemptTimeout. */
XREDIS_API bool xredisSubscriptionSessionConfigInit(xredissubscriptionsessionconfig* Config);
/* Retain and exclusively claim Store editing until physical close. Read-only
 * snapshots remain available; direct Store Apply is OVERLOADED while claimed.
 * All desired edits use SessionApply. Caller owns one Session reference and must
 * Close then Release it. Accepted creation consumes Data/Drop duty;
 * failure preserves it and Output. No callback runs inline on the caller, but
 * the owning worker may run before return. One candidate/generation, one native
 * control Future, three embedded Watches, one timer and one coalesced post.
 * No worker/runtime is created. Transient connect/disconnect/timeout retries only
 * after physical retirement. Invalid profile, server/protocol/limit/memory and
 * authentication failures close the controller; ordinary commands are not replayed. */
XREDIS_API xredisadmission xredisSubscriptionSessionCreate(xnetengine* Engine,uint64 Affinity,
    xredissubscriptions* Store,const xredissubscriptionsessionconfig* Config,xredissubscriptionsession** Output);
XREDIS_API bool xredisSubscriptionSessionRetain(xredissubscriptionsession* Session);
XREDIS_API void xredisSubscriptionSessionRelease(xredissubscriptionsession* Session);
/* Commit local intent and automatically schedule bounded incremental reconciliation.
 * Only removed names are unsubscribed and only added names subscribed; unchanged
 * names stay live. Finish the captured revision, then reconcile newer intent.
 * This is not a server acknowledgement. No-op edits send nothing. Native message
 * queues retain their limits; transport loss still creates an at-most-once gap. */
XREDIS_API xredisadmission xredisSubscriptionSessionApply(xredissubscriptionsession* Session,
    xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision);
XREDIS_API bool xredisSubscriptionSessionInfo(xredissubscriptionsession* Session,xredissubscriptionsessioninfo* Info);
/* ACCEPTED transfers one retained native Client for receiving/observing only.
 * Release it; do not issue commands/adopt/claim it. Controller owns close duty.
 * Available only at the last fully applied desired revision. OVERLOADED during
 * recovery/update and CLOSED after close begins, preserving Output. Old retained
 * generations may still be drained after retirement; they never become current
 * again. Native receive/readability APIs run directly, without a proxy queue. */
XREDIS_API xredisadmission xredisSubscriptionSessionCurrent(xredissubscriptionsession* Session,
    xredissubscriptionsessioncurrent* Output);
XREDIS_API bool xredisSubscriptionSessionClose(xredissubscriptionsession* Session);
/* Physical close retires accepted edits, native generations and callback access;
 * releases Store claim, runs Drop once on worker, then unpins Engine and resolves
 * this shared typed observer. Cancelling the observer does not close the session.
 * Final post/Watch release frames may finish afterwards without context/Engine
 * access. Join owner worker turns for quota-zero accounting. Retained
 * session/outcomes keep no Engine/context reference after close. */
XREDIS_API xfuture* xredisSubscriptionSessionClosed(xredissubscriptionsession* Session);
XRT_EXTERN_C_END
#endif
#endif
