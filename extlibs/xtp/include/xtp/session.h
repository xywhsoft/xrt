#ifndef XTP_SESSION_H
#define XTP_SESSION_H
#include <xtp/reader.h>
#if defined(XTP_FEATURE_XTP_SESSION)
typedef struct xtpsession xtpsession;
typedef struct xtprequesttoken {uint64_t _Opaque[4];} xtprequesttoken;
typedef struct xtpoperationtoken {uint64_t _Opaque[2];} xtpoperationtoken;
typedef struct xtpoutputticket {uint64_t _Opaque[4];} xtpoutputticket;
typedef struct xtpmessageview {
    xtpmessagekind Type;uint64_t MessageID;xtpcommand Command;
    xtpparamsview Params;xbytesview Body,Packet;xtprequesttoken Request;
} xtpmessageview;
typedef enum xtpaction {XTP_ACTION_ACCEPT=1,XTP_ACTION_BUSY,XTP_ACTION_REJECT,XTP_ACTION_PAUSE} xtpaction;
typedef struct xtpdecision {xtpaction Action;int32_t Status;} xtpdecision;
typedef enum xtpfinish {
    XTP_FINISH_RESPONSE=1,XTP_FINISH_ACK,XTP_FINISH_SENT,XTP_FINISH_TIMEOUT,
    XTP_FINISH_CLOSED,XTP_FINISH_LOCAL_ERROR
} xtpfinish;
typedef enum xtpknowledge {
    XTP_KNOW_NOT_SENT=1,XTP_KNOW_UNKNOWN,XTP_KNOW_ACCEPTED,XTP_KNOW_REPLIED,XTP_KNOW_REJECTED
} xtpknowledge;
typedef struct xtpcompletion {
    xtpoperationtoken Operation;xtpmessagekind Type;uint64_t MessageID;
    xtpfinish Reason;xtpknowledge Knowledge;xtpresult LocalResult;
    bool HasRemoteStatus;int32_t Status;xtpparamsview Params;xbytesview Body,Packet;void* Tag;
} xtpcompletion;
typedef struct xtpfault {xtpresult Reason;uint64_t BindingID;uint32_t LaneID;bool Fatal;} xtpfault;
typedef struct xtpevents {
    xtpdecision (*OnMessage)(xtpsession*,const xtpmessageview*,void* User);
    void (*OnComplete)(xtpsession*,const xtpcompletion*,void* User);
    void (*OnFault)(xtpsession*,const xtpfault*,void* User);
} xtpevents;
typedef struct xtpsubmitoptions {uint64_t DeadlineUs;uint32_t LaneID;bool Replay;void* Tag;} xtpsubmitoptions;
typedef struct xtplane {
    uint32_t ID;xtpinputmode Mode;bool Reliable;
    size_t RecordLimit; /* RECORD complete-frame limit; required, even on WS. */
} xtplane;
/* Host-authenticated, canonical public identities; never credentials. Each
 * array must contain a nonzero byte. SessionID is exchanged by the host. */
typedef struct xtpdeliveryidentity {
    unsigned char SessionID[16],Domain[16],Local[32],Peer[32];
} xtpdeliveryidentity;
typedef struct xtpdeliveryproof {
    xtpdeliveryidentity Identity;
    uint64_t UntilUs,LifetimeUs,PeerReceiveFloor,PeerResultFloor;
    bool Resume,StateRetained,OldBindingDetached,AllowRequests;
} xtpdeliveryproof;
typedef struct xtpdeliveryconfig {
    bool Enabled,AllowRequests;
    size_t MaxReplayMessages,MaxAttempts;
    uint64_t MaxLifetimeUs,RetryMinUs,RetryMaxUs;
} xtpdeliveryconfig;
typedef struct xtpbinding {
    const xtplane* Lanes;size_t LaneCount;
    bool Authenticated,AllowPlaintext; /* Explicit trusted-local/test exception. */
    const xtpdeliveryproof* Delivery; /* Borrowed only during Bind. */
} xtpbinding;
typedef struct xtpconfig {
    xtpreaderconfig Reader;
    size_t MaxPending,MaxIncomingRequests,MaxLanes,MaxTxFrames,MaxTxBytes;
    size_t ResponseReserveBytes,ResponseReserveFrames,MaxResponseBytes,MaxSessionBytes,DedupWindow;
    uint64_t DefaultOperationTimeoutUs,IncomingRequestTimeoutUs;
    xtpdeliveryconfig Delivery;
    xtpreallocproc Realloc;void* AllocatorUser;
} xtpconfig;
typedef struct xtpoutput {
    xbytesview Data;xtpoutputticket Ticket;uint64_t BindingID,DeadlineUs;
    uint32_t LaneID;xtpinputmode Mode;bool Reliable;uint8_t Priority;
} xtpoutput;
typedef struct xtpinfo {
    uint64_t BindingID,NextMessageID,Submitted,Completed,Messages,UnknownResults,Duplicates;
    size_t Pending,IncomingRequests,TxFrames,TxBytes,StorageBytes,PeakReservedBytes;
    bool Bound,Closing,Closed;
    size_t Retained,Receipts;
    uint64_t ReceiveFloor,ResultFloor,Attempts,Retries,ResultAcks;
    bool Suspended,ReplayRequests;
} xtpinfo;
XRT_EXTERN_C_BEGIN
/* No IO/thread/global route state. NULL config uses finite defaults. Config and
 * events copied. Random instance cookies use XRT secure random once at Create.
 * Same instance: serialize calls. Cross-instance calls may run concurrently. */
XTP_API void xtpConfigInit(xtpconfig* Config);
XTP_API xtpresult xtpCreate(const xtpconfig* Config,const xtpevents* Events,void* User,xtpsession** Session);
/* Destroy requires CLOSED and no callbacks/host asynchronous tasks in flight. */
XTP_API xtpresult xtpDestroy(xtpsession* Session);
/* No transport ownership/event replacement. Copies lanes, nonzero unique IDs;
 * LaneID=0 in Submit/Output selects default/any lane. Generation never wraps.
 * Delivery uses a host-verified proof, never a wire login command. Rebind after
 * Unbind retains only opted-in work. Otherwise Unbind completes all work. */
XTP_API xtpresult xtpBind(xtpsession* Session,const xtpbinding* Binding,uint64_t NowUs,uint64_t* BindingID);
XTP_API xtpresult xtpUnbind(xtpsession* Session,uint64_t BindingID,uint64_t NowUs);
XTP_API xtpresult xtpInput(xtpsession* Session,uint64_t BindingID,uint32_t LaneID,
    xbytesview Input,bool RecordEnd,uint64_t NowUs,xtpreadprogress* Progress);
XTP_API xtpresult xtpEnd(xtpsession* Session,uint64_t BindingID,uint32_t LaneID,uint64_t NowUs);
XTP_API xtpresult xtpResume(xtpsession* Session,uint64_t BindingID,uint32_t LaneID,uint64_t NowUs);
/* Success copies all input. Batch: all accepted or none; outputs preserved on
 * failure. Optional Options/Operations arrays have Count entries. No callback
 * dispatch during Submit/Reply. Replay requires enabled, authenticated Delivery;
 * requests additionally require negotiated AllowRequests. Original deadline
 * must fit the agreed replay lifetime and authenticated identity expiry. */
XTP_API xtpresult xtpSubmit(xtpsession* Session,const xtpmessage* Message,
    const xtpsubmitoptions* Options,uint64_t NowUs,xtpoperationtoken* Operation);
XTP_API xtpresult xtpSubmitBatch(xtpsession* Session,const xtpmessage* Messages,size_t Count,
    const xtpsubmitoptions* Options,uint64_t NowUs,xtpoperationtoken* Operations);
XTP_API xtpresult xtpReply(xtpsession* Session,xtprequesttoken Request,const xtpresponse* Response,uint64_t NowUs);
/* One immutable frame suffix. STREAM permits short Advance; RECORD is all or
 * zero. Zero Advance returns AGAIN. Ticket validates binding, slot, generation
 * and offset. No reentry between Output and lower-layer acceptance/Advance.
 * Output is borrowed until the next mutating call; host must copy if retaining.
 * Output(any lane) rotates queue heads after successful Advance; select a lane to keep
 * independently writable lanes progressing under per-lane backpressure. */
XTP_API xtpresult xtpOutput(xtpsession* Session,uint32_t LaneID,xtpoutput* Output);
XTP_API xtpresult xtpAdvance(xtpsession* Session,xtpoutputticket Ticket,size_t Accepted,uint64_t NowUs);
/* Drive also services receive deadlines/complete buffered frames, bounded to
 * Reader.MaxFrames callbacks per maintenance turn. AGAIN requests another turn;
 * NextDue=0 has no timer. PAUSE requires explicit Resume and retained suffix. */
XTP_API xtpresult xtpDrive(xtpsession* Session,uint64_t NowUs,uint64_t* NextDue);
/* Callback-safe deferred close. Repeated Close/Drive drains bounded completion
 * batches until CLOSED. Tokens/output invalid immediately on close/unbind.
 * All callbacks borrow views; only OnMessage/OnComplete/OnFault may Submit,
 * Reply or request Close. Recursive Input/Drive/Advance/Bind/Destroy forbidden. */
XTP_API xtpresult xtpClose(xtpsession* Session,uint64_t NowUs);
XTP_API xtpresult xtpInfo(const xtpsession* Session,xtpinfo* Info);
XRT_EXTERN_C_END
static inline void xtpDeliveryConfigInit(xtpdeliveryconfig* c)
{if(c){memset(c,0,sizeof(*c));c->Enabled=true;c->MaxReplayMessages=256;c->MaxAttempts=5;
    c->MaxLifetimeUs=60000000;c->RetryMinUs=100000;c->RetryMaxUs=2000000;}}
static inline xtpdecision xtpAccept(void){xtpdecision d={XTP_ACTION_ACCEPT,0};return d;}
static inline xtpdecision xtpBusy(void){xtpdecision d={XTP_ACTION_BUSY,XTP_STATUS_BUSY};return d;}
static inline xtpdecision xtpReject(int32_t status){xtpdecision d={XTP_ACTION_REJECT,status};return d;}
static inline xtpdecision xtpPause(void){xtpdecision d={XTP_ACTION_PAUSE,0};return d;}
#endif
#endif
