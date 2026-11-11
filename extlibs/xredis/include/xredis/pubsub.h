#ifndef XREDIS_PUBSUB_H
#define XREDIS_PUBSUB_H
#include <xredis/command.h>
#include <xredis/reply.h>
#if defined(XREDIS_FEATURE_XREDIS_PUBSUB) && \
    (!defined(XREDIS_FEATURE_XREDIS_COMMAND) || !defined(XREDIS_FEATURE_XREDIS_REPLY))
#error "Pub/Sub wire operations require command and reply"
#endif
#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
#define XREDIS_PUBSUB_NAMES_MAX 1024u
#define XREDIS_PUBSUB_NAME_ABSENT 0x01u
#define XREDIS_PUBSUB_STRUCTURED 0x02u
typedef enum xredispubsubcommand {
    XREDIS_PUBSUB_CMD_SUBSCRIBE=0,XREDIS_PUBSUB_CMD_PSUBSCRIBE,XREDIS_PUBSUB_CMD_SSUBSCRIBE,
    XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_PUNSUBSCRIBE,XREDIS_PUBSUB_CMD_SUNSUBSCRIBE,
    XREDIS_PUBSUB_CMD_PING
} xredispubsubcommand;
typedef enum xredispubsubkind {
    XREDIS_PUBSUB_MESSAGE=0,XREDIS_PUBSUB_PMESSAGE,XREDIS_PUBSUB_SMESSAGE,
    XREDIS_PUBSUB_SUBSCRIBE,XREDIS_PUBSUB_PSUBSCRIBE,XREDIS_PUBSUB_SSUBSCRIBE,
    XREDIS_PUBSUB_UNSUBSCRIBE,XREDIS_PUBSUB_PUNSUBSCRIBE,XREDIS_PUBSUB_SUNSUBSCRIBE,XREDIS_PUBSUB_PONG
} xredispubsubkind;
typedef enum xredispubsubstatus {
    XREDIS_PUBSUB_ERROR=-2,XREDIS_PUBSUB_MALFORMED=-1,XREDIS_PUBSUB_OTHER=0,XREDIS_PUBSUB_EVENT=1
} xredispubsubstatus;
typedef enum xredispubsuberror {
    XREDIS_PUBSUB_ARGUMENT=4001,XREDIS_PUBSUB_LIMIT,XREDIS_PUBSUB_OVERLAP,XREDIS_PUBSUB_CAPACITY
} xredispubsuberror;
/* Borrows one immutable Reply, including attributes. Copying this structure
 * does not retain it. Retain Reply explicitly before dropping its owner.
 * Envelope is the actual ARRAY/PUSH (or scalar for PongRead), not negotiated
 * protocol. ACK Subscriptions is the reported count: regular channel+pattern
 * counts and shard counts are separate server domains, not a client total.
 * Pattern ACKs use Pattern; regular/shard ACKs use Channel.
 * NAME_ABSENT denotes an unsubscribe-all ACK with a null name, distinct from
 * a subscribed empty binary name; it does not imply global count zero.
 * PayloadNode is NONE for ACKs. STRUCTURED is reserved for the legacy RESP2
 * __redis__:invalidate message's fully validated array/null payload. In that
 * case Payload is empty; inspect PayloadNode or use TrackingEventRead.
 * Normal payload views are binary, including empty/NUL/CRLF, with no scan/copy. */
typedef struct xredispubsubevent {
    const xredisreply* Reply;xredispubsubkind Kind;xredisreplytype Envelope;uint32 Flags;
    xbytesview Channel,Pattern,Payload;uint64 Subscriptions;size_t PayloadNode;
} xredispubsubevent;
XRT_EXTERN_C_BEGIN
/* Measure/write SUBSCRIBE/PSUBSCRIBE/SSUBSCRIBE, their unsubscribe variants,
 * or PING. Subscribe needs 1..1024 names, unsubscribe accepts 0..1024 (zero
 * means all in that family), PING accepts 0 or 1 binary payload. Inputs are
 * borrowed for this call only. No heap, descriptor copy, payload scan or NUL.
 * No connection mode, subscription state or command admission is performed.
 * All validation precedes writes; outputs preserve their values on failure
 * and cannot alias names/descriptors/each other. Only the actual encoded span
 * is output. Name span errors use existing command argument/overflow codes. */
XREDIS_API bool xredisPubSubCommandSize(xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,size_t* Required);
XREDIS_API bool xredisPubSubCommandWrite(ptr Output,size_t Capacity,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,size_t* Written);
/* Decode complete RESP2 arrays or RESP3 pushes: all three message families,
 * all six ACKs and aggregate pong. OTHER/MALFORMED preserve Output and ambient
 * error; invalid API use reports ERROR. Output cannot overlap any Reply storage.
 * This is a view decoder, not a subscribed-connection runtime or ACK correlator.
 * Scalar PING replies require PongRead with the expected payload context. */
XREDIS_API xredispubsubstatus xredisPubSubEventRead(const xredisreply* Reply,xredispubsubevent* Output);
/* Call when a PING response is expected. NULL Expected means no PING argument;
 * non-NULL includes an explicit empty argument. Match exact binary payload in
 * RESP2 aggregate pong or an ordinary bulk echo (RESP3 or unsubscribed RESP2);
 * no-argument scalar is simple PONG.
 * Other Pub/Sub events, errors and unrecognized aggregates return OTHER for
 * separate routing; bad recognized pong/scalar responses are MALFORMED.
 * This does not correlate arbitrary business replies. Expected/output cannot
 * overlap. No allocation/copy; the same ownership/output rules apply. */
XREDIS_API xredispubsubstatus xredisPubSubPongRead(const xredisreply* Reply,
    const xbytesview* Expected,xredispubsubevent* Output);
XRT_EXTERN_C_END
#endif
#endif
