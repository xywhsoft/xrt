#ifndef XREDIS_SUBSCRIPTIONS_H
#define XREDIS_SUBSCRIPTIONS_H
#include <xredis/subscriber.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#error "Subscription recovery requires the native subscriber"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
typedef struct xredissubscriptions xredissubscriptions;
typedef struct xredissubscriptionsnapshot xredissubscriptionsnapshot;
typedef struct xredissubscriptionreplay xredissubscriptionreplay;
typedef struct xredissubscriptionsconfig {size_t Size,MaxSubscriptions,MaxNameBytes,MaxChangeBytes;} xredissubscriptionsconfig;
typedef struct xredissubscriptionsinfo {
    size_t Size;uint64 Revision;size_t Entries,NameBytes,SnapshotBytes,ReferencedEntryBytes;bool Preparing;
} xredissubscriptionsinfo;
typedef struct xredissubscriptionview {xredispubsubcommand Family;xbytesview Name;} xredissubscriptionview;
typedef enum xredissubscriptionstatus {
    XREDIS_SUBSCRIPTIONS_PENDING=0,XREDIS_SUBSCRIPTIONS_OK,XREDIS_SUBSCRIPTIONS_STALE,
    XREDIS_SUBSCRIPTIONS_CANCELLED,XREDIS_SUBSCRIPTIONS_TIMEOUT,XREDIS_SUBSCRIPTIONS_CLOSED,
    XREDIS_SUBSCRIPTIONS_SERVER_ERROR,XREDIS_SUBSCRIPTIONS_PROTOCOL,
    XREDIS_SUBSCRIPTIONS_LIMIT,XREDIS_SUBSCRIPTIONS_MEMORY
} xredissubscriptionstatus;
typedef enum xredissubscriptionstep {
    XREDIS_SUBSCRIPTIONS_STEP_ERROR=-1,XREDIS_SUBSCRIPTIONS_WAIT=0,
    XREDIS_SUBSCRIPTIONS_MORE=1,XREDIS_SUBSCRIPTIONS_DONE=2
} xredissubscriptionstep;
typedef struct xredissubscriptionreplayconfig {
    size_t Size;xredissubscriberconfig Registry;size_t MaxBatchNames,MaxBatchBytes;int64 Timeout;
} xredissubscriptionreplayconfig;
/* Acknowledged/counts cover native outcomes observed by Step, not unobserved
 * remote effects. NativeStatus is the observed native failure or the corresponding
 * local stop class; the retained native observer can have its own later outcome. */
typedef struct xredissubscriptionreplayresult {
    size_t Size;xredissubscriptionstatus Status;xredisrequeststatus NativeStatus;
    uint64 Revision;size_t Acknowledged,Batches;uint64 Subscriptions,ShardSubscriptions;
    xredisreply* ErrorReply;const xerror* Cause;
} xredissubscriptionreplayresult;
XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisSubscriptionsConfigInit(xredissubscriptionsconfig* Config);
/* Independent in-memory desired set. Defaults4096 names/1MiB stored names/1MiB input names per edit. Config is
 * copied; success owns one reference. No Client/Engine or new worker is owned. */
XREDIS_API bool xredisSubscriptionsCreate(const xredissubscriptionsconfig* Config,xredissubscriptions** Store);
XREDIS_API bool xredisSubscriptionsRetain(xredissubscriptions* Store);
XREDIS_API void xredisSubscriptionsRelease(xredissubscriptions* Store);
/* Atomic local intent, not a wire operation or durability acknowledgement.
 * Six subscription commands only; duplicates are idempotent. Empty unsubscribe
 * removes its family. Inputs borrowed only through return. Changed sets advance
 * revision; no-op keeps it. Failure preserves all state and Revision output.
 * One bounded constructor runs outside Gate; concurrent Apply is OVERLOADED.
 * Cold edits merge pointer vectors; unchanged binary names are shared, not copied.
 * Snapshots remain readable during edits. Outputs are caller-owned/disjoint. */
XREDIS_API xredisadmission xredisSubscriptionsApply(xredissubscriptions* Store,
    xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision);
XREDIS_API bool xredisSubscriptionsInfo(xredissubscriptions* Store,xredissubscriptionsinfo* Info);
/* O(1), no allocation or payload copy. Each snapshot reference is caller-owned;
 * a snapshot/its borrowed views outlive Store and future local edits. Retained
 * old snapshots pin old entry allocations as caller-owned results. */
XREDIS_API bool xredisSubscriptionsSnapshot(xredissubscriptions* Store,xredissubscriptionsnapshot** Snapshot);
XREDIS_API bool xredisSubscriptionSnapshotRetain(xredissubscriptionsnapshot* Snapshot);
XREDIS_API void xredisSubscriptionSnapshotRelease(xredissubscriptionsnapshot* Snapshot);
XREDIS_API bool xredisSubscriptionSnapshotInfo(const xredissubscriptionsnapshot* Snapshot,xredissubscriptionsinfo* Info);
/* Canonical family/shard-slot/hash/binary order; index-at-end false preserves output and
 * ambient error. View borrows immutable name bytes from this snapshot. */
XREDIS_API bool xredisSubscriptionSnapshotAt(const xredissubscriptionsnapshot* Snapshot,size_t Index,xredissubscriptionview* View);
XREDIS_API bool xredisSubscriptionReplayConfigInit(xredissubscriptionreplayconfig* Config);
/* Retain Store/current snapshot/Client, dedicate a fresh ready PRIVATE Client.
 * Caller keeps its original Client close/reference duty. An internal token rejects
 * other native controls until terminal result or destruction; no token is stored
 * in accepted requests. No subscription is sent
 * inline. Success alone changes Replay; failure consumes nothing. Client must
 * not be shared or receive other controls through replay completion/destruction.
 * Registry must fit the captured set. Default64 names/64KiB encoded batch/5s.
 * Every command shares the original monotonic timeout, including caller delay.
 * Shard batches stay within one cached hash slot; endpoint selection and MOVED
 * routing remain with the caller/future Cluster controller.
 * A desired-revision change returns STALE and retires the partial connection.
 * Subscription restoration cannot recover Pub/Sub messages missed while offline. */
XREDIS_API xredisadmission xredisSubscriptionReplayCreate(xredissubscriptions* Store,xredisclient* Client,
    const xredissubscriptionreplayconfig* Config,xredissubscriptionreplay** Replay);
/* Unique caller serialization, no blocking/no private scheduler. One call submits
 * OR observes at most one bounded batch; MORE requires another scheduled turn.
 * WAIT has one pending native Future. A transient physical request tail returns
 * MORE; a private idle Client admission limit is a terminal LIMIT, not busy retry.
 * Failure/cancel/timeout closes Client; OK leaves it subscribed to Result.Revision.
 * Later desired edits do not change a terminal result. Drive again on MORE or a
 * pending-Future notification; never block an Engine worker in a wait. */
XREDIS_API xredissubscriptionstep xredisSubscriptionReplayStep(xredissubscriptionreplay* Replay);
/* Retained native observer if currently pending, otherwise NULL. Does not add a
 * wrapper Future. Cancelling it cancels that control; Step then retires Client. */
XREDIS_API xfuture* xredisSubscriptionReplayPending(xredissubscriptionreplay* Replay);
XREDIS_API bool xredisSubscriptionReplayResult(xredissubscriptionreplay* Replay,xredissubscriptionreplayresult* Result);
/* Same unique-owner serialization as Step. Nonblocking; Destroy aborts unfinished
 * work and releases all helper refs. Native accepted wire owns its copied inputs,
 * so no callback/future retains Replay storage. ErrorReply/Cause borrow a retained
 * failed native Future until Destroy; retain escaping replies explicitly. */
XREDIS_API bool xredisSubscriptionReplayCancel(xredissubscriptionreplay* Replay);
XREDIS_API void xredisSubscriptionReplayDestroy(xredissubscriptionreplay* Replay);
XRT_EXTERN_C_END
#endif
#endif
