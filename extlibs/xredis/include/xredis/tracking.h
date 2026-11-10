#ifndef XREDIS_TRACKING_H
#define XREDIS_TRACKING_H
#include <xredis/client.h>
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "Redis Tracking requires the existing Client"
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
#define XREDIS_TRACKING_PREFIXES_MAX 64u
#define XREDIS_TRACKING_BCAST 0x01u
#define XREDIS_TRACKING_OPTIN 0x02u
#define XREDIS_TRACKING_OPTOUT 0x04u
#define XREDIS_TRACKING_NOLOOP 0x08u
#define XREDIS_TRACKING_REDIRECT 0x10u
#define XREDIS_TRACKING_OBSERVATION_BYTES_DEFAULT ((size_t)65536u)
typedef struct xredistrackingconfig {
    size_t Size;bool Enabled;uint32 Flags;int64 Redirect;
    const xbytesview* Prefixes;size_t PrefixCount;
} xredistrackingconfig;
/* Request prefix and ownership match ordinary Client results. Sequence is
 * per Client, in actual received-frame order, not completion/collection order.
 * Zero means no reply was published. Counter exhaustion closes without wrap.
 * No Client/Engine reference survives in an observed result. */
typedef struct xredistrackingresult {xredisrequestresult Request;uint64 Sequence;} xredistrackingresult;
typedef struct xredistrackingpush {xredisreply* Reply;uint64 Sequence;} xredistrackingpush;
typedef struct xredistrackinginfo {size_t Size,Bytes,Reserved;uint64 Sequence,Rejected;bool Enabled,Preparing;} xredistrackinginfo;
typedef enum xredistrackingeventkind {XREDIS_TRACKING_KEYS=0,XREDIS_TRACKING_FLUSH,XREDIS_TRACKING_REDIRECT_BROKEN} xredistrackingeventkind;
typedef enum xredistrackingstatus {XREDIS_TRACKING_ERROR=-2,XREDIS_TRACKING_MALFORMED=-1,XREDIS_TRACKING_OTHER=0,XREDIS_TRACKING_EVENT=1} xredistrackingstatus;
/* Borrows Reply. Walk exactly Keys nodes using xredisReplyGet/Next, starting
 * at FirstKey: linear iteration, binary views, no allocation/copy. Retain the
 * original Reply explicitly to keep these views after its owning result. */
typedef struct xredistrackingevent {
    const xredisreply* Reply;xredistrackingeventkind Kind;size_t FirstKey,Keys;uint64 Redirect;
} xredistrackingevent;
XRT_EXTERN_C_BEGIN
/* Thread-safe one-time opt-in. MaxBytes=0 uses 64 KiB. One bounded allocation
 * (header + MaxPushes uint64 slots), retained until final Client release.
 * ACCEPTED is idempotent; concurrent installation/insufficient budget is normal
 * OVERLOADED. No command or tracking mode is sent here. Existing queued pushes
 * have sequence0 and precede all newly admitted observed calls. This activation
 * starts an observation epoch with an empty application cache. */
XREDIS_API xredisadmission xredisClientTrackingObserve(xredisclient* Client,size_t MaxBytes);
XREDIS_API bool xredisClientTrackingInfo(xredisclient* Client,xredistrackinginfo* Info);
/* Requires Observe. Same command/deadline/admission/cancel/drain semantics as
 * ordinary calls. One enlarged result allocation, no extra Future or payload
 * copy. Other callers may use ordinary requests; those results carry no order.
 * Sync form rejects every worker of this Engine before submitting. */
XREDIS_API xredisadmission xredisClientTrackingCallAsync(xredisclient* Client,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisClientTrackingCall(xredisclient* Client,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
/* Transfers one reply reference. Same push queue/credits as ordinary TakePush;
 * do not mix consumers when ordered invalidation handling is required. Empty
 * preserves output/error. Values survive Client/Engine destruction. */
XREDIS_API bool xredisClientTrackingTake(xredisclient* Client,xredistrackingpush* Push);
XREDIS_API bool xredisTrackingConfigInit(xredistrackingconfig* Config);
/* Encodes CLIENT TRACKING, including binary PREFIX, REDIRECT, BCAST, OPTIN,
 * OPTOUT and NOLOOP. Requires Observe; uses the original entry deadline.
 * Input is copied before return. Direct invalidations require managed RESP3
 * (AUTO must have selected3); RESP2 senders need positive REDIRECT. Success
 * validates a simple OK; unexpected non-error replies close as PROTOCOL.
 * The server reply/status remains explicit:
 * a sent timeout can leave mode uncertain. No replay or hidden local cache.
 * OPTIN/OPTOUT CLIENT CACHING pairs require exclusive producer serialization;
 * this API does not make two separately submitted commands atomic. */
XREDIS_API xredisadmission xredisClientTrackingConfigureAsync(xredisclient* Client,
    const xredistrackingconfig* Config,int64 Timeout,xfuture** Future);
/* Fully validate recognized RESP3 invalidations/redirection failure, or the
 * RESP2 __redis__:invalidate Pub/Sub envelope. Null invalidation means flush.
 * OTHER/malformed preserve output and ambient error. Invalid API use reports
 * ERROR; output cannot overlap any Reply allocation. Runtime receiving of RESP2
 * Pub/Sub still requires a dedicated subscribed connection implementation. */
XREDIS_API xredistrackingstatus xredisTrackingEventRead(const xredisreply* Reply,xredistrackingevent* Event);
XRT_EXTERN_C_END
#endif
#endif
