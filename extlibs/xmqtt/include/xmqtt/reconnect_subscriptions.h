#ifndef XMQTT_RECONNECT_SUBSCRIPTIONS_H
#define XMQTT_RECONNECT_SUBSCRIPTIONS_H
#include <xmqtt/reconnect.h>
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS) && !defined(XMQTT_FEATURE_XMQTT_RECONNECT)
#error "Subscription recovery requires the reconnect controller"
#endif
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
XRT_EXTERN_C_BEGIN
/* One event per accepted Client control result, on the controller worker and
 * outside locks. Revision identifies its original intent group; Request is the
 * exact ordered subset attempted, including properties. Wire is empty (no extra
 * encoded-wire snapshot). The intent can already be superseded.
 * All views/results are borrowed through this callback. Retain Result->Ack with
 * xmqttMessageRetain when needed beyond it. Code/User live through Destroy;
 * callbacks may run before Create returns. May submit/query/Close, never wait.
 * Local preflight restrictions have no Client result event; query their status. */
typedef void (*xmqttsubscriptionsnotify)(xmqttreconnect* Controller,uint64 Revision,
    const xmqttpacket* Request,const xmqttsubscriptionresult* Result,ptr User);
typedef struct xmqttsubscriptionsconfig {
    size_t Size,MaxFilters,MaxBytes,MaxBatch;
    int64 OperationTimeout,RetryDelay;
    bool FailOnRejected;
    xmqttsubscriptionsnotify Notify;ptr User;
} xmqttsubscriptionsconfig;
typedef struct xmqttsubscriptionsinfo {
    size_t Size,Filters,Desired,Pending,Rejected,Bytes,Reserved,Watchers;
    uint64 Revision,SettledRevision,Generation,Requests,Recoveries,Admitted,Overloaded,AdmissionRetries;
    bool Connected,Synchronized;
    xmqttclientstatus Status;
} xmqttsubscriptionsinfo;
typedef struct xmqttsubscriptionintentinfo {
    size_t Size;uint64 Revision,AppliedRevision,Generation;
    bool Desired,Pending,Confirmed,KnownPresent;
    uint8 Options,Reason;xmqttclientstatus Status;
} xmqttsubscriptionintentinfo;
/* Defaults: 1024 distinct filters, 4MiB including fixed storage, snapshots and
 * at most two watcher tails, 32 filters/request, 30s synchronization deadline,
 * 5ms admission retry delay, and terminal failure on a rejected/restricted filter. Durations are ms.
 * MaxFilters <=65535; MaxBatch <=MaxFilters and the Client codec topic limit.
 * OperationTimeout bounds accepted requests and an online synchronization wave,
 * including Client admission. Initial/outage admission shares RecoveryTimeout.
 * RetryDelay is 1..60000ms; pending ACKs use a watch, not retry polling. */
XMQTT_API bool xmqttSubscriptionsConfigInit(xmqttsubscriptionsconfig* Config);
/* Copy all initial SUBSCRIBE packets before starting. Later occurrences of a
 * filter replace earlier intent, including options and all group properties.
 * Initial=NULL/Count=0 is allowed. The old Create ABI/behavior is unchanged.
 * ONLINE is delayed until current intent has settled on the accepted session;
 * the original reconnect outage deadline includes this restoration. With
 * FailOnRejected=false ONLINE can include explicitly reported rejected filters.
 * No disk state, offline publication queue or private thread is introduced. */
XMQTT_API bool xmqttReconnectCreateSubscribed(xnetengine* Engine,xtaskpool* CleanupPool,
    uint64 Affinity,const xmqttreconnectconfig* Config,const xmqttsubscriptionsconfig* Subscriptions,
    const xmqttpacket* Connect,const xmqttpacket* Initial,size_t Count,xmqttreconnect** Controller);
/* Publish an atomic, bounded desired-state snapshot, online or offline. These
 * are intent admissions, not broker operation Futures. Packet Identifier must
 * be zero; Type must match, with codec-valid flags/options/properties. Reject
 * duplicate exact filters within one packet. Preserve all MQTT5 properties;
 * a replay batch may contain a subset of its original group. Inputs are borrowed
 * only during this call. ACCEPTED publishes a new revision and changes Revision
 * only on success. OVERLOADED/CLOSED preserve ambient error and output.
 * A newer intent may supersede an older one before transmission. No promise
 * that every admitted revision is sent, nor atomic broker application, is made.
 * Raw leased Client Subscribe/Unsubscribe bypass this registry: do not mix them
 * for managed filters. A lost ACK is reconciled by the next accepted session;
 * a retained session can require repeating an uncertain control operation. */
XMQTT_API xmqttclientadmission xmqttReconnectSubscribeIntent(xmqttreconnect* Controller,
    const xmqttpacket* Subscribe,uint64* Revision);
XMQTT_API xmqttclientadmission xmqttReconnectUnsubscribeIntent(xmqttreconnect* Controller,
    const xmqttpacket* Unsubscribe,uint64* Revision);
/* Pending includes the in-flight latest intent. SettledRevision advances when
 * all current intents settle; superseded revisions are covered by their latest
 * replacements. Inspect Status/Rejected as well as Synchronized. No broker
 * durability or subscriber delivery is implied. Bytes includes fixed/live
 * storage; Reserved is additionally charged during concurrent snapshot builds.
 * Requests counts accepted Client controls, not wire delivery. Admitted counts
 * intent groups; Overloaded counts public capacity refusals; AdmissionRetries
 * counts Client quota refusals. Recoveries counts accepted sessions after the
 * first accepted session, independently of failed setup attempts.
 * The original Client Notify stays direct. Controller Notify also fires on
 * completion of a synchronization wave (outside locks, on its Engine worker). */
XMQTT_API bool xmqttReconnectSubscriptionsInfo(xmqttreconnect* Controller,xmqttsubscriptionsinfo* Info);
/* Exact-filter lookup. A confirmed successful removal is reclaimed, so normal
 * missing-filter false preserves Info and ambient error. Confirmed requires
 * the current accepted session and latest successful revision. KnownPresent is
 * only the last confirmed state, and may be stale while offline or uncertain.
 * Reason is the exact broker reason for the latest settled revision, or zero
 * for a local failure. Per-filter results survive until replaced or removed.
 * These APIs follow the controller's stable-owner/Destroy exclusion contract. */
XMQTT_API bool xmqttReconnectSubscriptionInfo(xmqttreconnect* Controller,xstrview Filter,
    xmqttsubscriptionintentinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
