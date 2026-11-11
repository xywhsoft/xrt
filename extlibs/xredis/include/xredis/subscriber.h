#ifndef XREDIS_SUBSCRIBER_H
#define XREDIS_SUBSCRIBER_H
#include <xredis/client.h>
#include <xredis/pubsub.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER) && \
    (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_PUBSUB) || !defined(XRT_FEATURE_HASH64))
#error "Subscriber requires client, Pub/Sub wire and hash64"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
typedef struct xredissubscriberconfig {
    size_t Size;
    size_t MaxSubscriptions;
    size_t MaxNameBytes;
} xredissubscriberconfig;

/* First member is the usual immutable request outcome. Reply is the final ACK,
 * PONG or server error, not a retained vector of every ACK. Acknowledged counts
 * validated ACKs at logical completion, including duplicates/absent-name ACKs.
 * Counts are confirmed server state at that instant; later draining cannot
 * change this result. Cancellation after send can still change subscriptions. */
typedef struct xredissubscriberresult {
    xredisrequestresult Request;
    size_t Acknowledged;
    uint64 Subscriptions,ShardSubscriptions;
} xredissubscriberresult;

typedef struct xredissubscriberinfo {
    size_t Size;
    size_t Entries,NameBytes,RegistryBytes;
    uint64 Subscriptions,ShardSubscriptions;
    uint64 Acknowledgements,Messages,Sidebands;
} xredissubscriberinfo;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisSubscriberConfigInit(xredissubscriberconfig* Config);
/* Irreversibly dedicate an idle, ready Client to Pub/Sub until Close. Existing
 * numeric/DNS/TLS factories, readiness, refs and physical close still apply.
 * Active requests, queued pushes, exclusive owners or Tracking observation
 * prevent adoption. Both admission checks and publication are under Gate.
 * No ordinary calls/transactions/session claims are admitted after adoption.
 * NULL Config defaults to4096 entries/1MiB name bytes. Entries bound confirmed
 * AND reserved distinct (family,binary name) keys; name bytes exclude fixed
 * entry headers and the cold bucket array, both bounded by MaxSubscriptions.
 * Registry allocation/limit failure before send is a typed NOT_SENT outcome. */
XREDIS_API xredisadmission xredisSubscriberStart(xredisclient* Client,const xredissubscriberconfig* Config);
/* Bounded pipelined controls using the same request/deadline/cancellation and
 * physical wire/FIFO obligations as Client. Inputs are encoded/copied during
 * admission; subscribe entries additionally own one persistent name copy.
 * All ACK names, kinds and counts are checked against confirmed state. Empty
 * unsubscribe-all ACKs and unordered unsubscribe-all families are correlated.
 * Completion Value is xredissubscriberresult. No implicit replay/reconnect.
 * Messages and unrelated RESP3 pushes use ClientTakePush (owned Reply transfer)
 * with the existing MaxPushes/MaxPushBytes bounds. Overflow retires the Client,
 * never silently drops a message. Retained replies survive all Client owners.
 * Sync rejects every worker of the same Engine. Callbacks must not block it. */
XREDIS_API xredisadmission xredisSubscriberCommandAsync(xredisclient* Client,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisSubscriberCommand(xredisclient* Client,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API bool xredisSubscriberInfo(xredisclient* Client,xredissubscriberinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
