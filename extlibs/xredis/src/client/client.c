#include <xredis/client.h>
#include <xredis/tracking.h>
#include <xredis/subscriber.h>
#include <xredis/subscriber_notify.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#include "../internal/subscription_replay.h"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#include <xrt/hash.h>
#endif
#include "../internal/client_connect.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/number.h>
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)

typedef struct xredisrequest xredisrequest;
typedef struct xredisinitwire xredisinitwire;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
typedef struct xredisobservation {size_t Bytes;xatomic64 Sequence;uint64 Pushes[];} xredisobservation;
/* Observe tags 3/4 select a pair without enlarging ordinary requests. */
typedef struct xrediscachingstate {
    size_t MetadataBytes;
    xredisreply *Control,*Query;
    uint64 ControlSequence,QuerySequence;
    xredisrequeststatus ControlStatus,QueryStatus;
    unsigned Seen;
    bool Written;
} xrediscachingstate;
static const uint8 xredisCachingYes[]="*3\r\n$6\r\nCLIENT\r\n$7\r\nCACHING\r\n$3\r\nYES\r\n";
static const uint8 xredisCachingNo[]="*3\r\n$6\r\nCLIENT\r\n$7\r\nCACHING\r\n$2\r\nNO\r\n";
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) || defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#define XREDIS_OBSERVE_PARAMETER ,unsigned Observe
#define XREDIS_OBSERVE_ORDINARY ,false
#else
#define XREDIS_OBSERVE_PARAMETER
#define XREDIS_OBSERVE_ORDINARY
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
enum { XREDIS_TX_MULTI, XREDIS_TX_COMMANDS, XREDIS_TX_EXEC, XREDIS_TX_DISCARD };
typedef struct xredistransactionstate {
	size_t Count, Seen, Errors, CommandsBytes, Written, MetadataBytes;
	xredisreply* Error;
	xredistransactionexecution Execution;
	unsigned Phase;
	bool Await;
} xredistransactionstate;
static const uint8 xredisTransactionMulti[] = "*1\r\n$5\r\nMULTI\r\n";
static const uint8 xredisTransactionExec[] = "*1\r\n$4\r\nEXEC\r\n";
static const uint8 xredisTransactionDiscard[] = "*1\r\n$7\r\nDISCARD\r\n";
#endif
enum { XREDIS_R_SUBMITTED, XREDIS_R_PENDING, XREDIS_R_SENT, XREDIS_R_DONE };
enum { XREDIS_I_HELLO, XREDIS_I_AUTH, XREDIS_I_NAME, XREDIS_I_SELECT, XREDIS_I_END };

struct xredisinitwire {
	xatomic32 References;
	xredisclient* Client;
	size_t Size;
	unsigned char Data[];
};

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
typedef struct xredissubentry {
    struct xredissubentry *BucketNext,*Next,*Previous;
    uint64 Hash;size_t Bytes,Reservations;unsigned Family;bool Confirmed;
    unsigned char Name[];
} xredissubentry;
typedef struct xredissubscriber {
    xredissubscriberconfig Config;xredissubscriberinfo Info;
    size_t StorageBytes,Buckets,Entries,NameBytes,EntryBytes,Counts[3];
    xredissubentry* Head;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xpromise* ReadablePromise;
    xfuture* ReadableFuture;
    bool ReadablePreparing;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
    ptr ReplayOwner;
#endif
    xredissubentry* Table[];
} xredissubscriber;
typedef struct xredissubcall {
    size_t MetadataBytes,Count,Cursor,Acknowledged,Expected,Reserved;
    xredispubsubcommand Command;
    bool Prepared,Written,Finished,AllInitialized,Absent;
    xredisrequeststatus Status;xredisreply* Final;
    xredissubentry* Entries[];
} xredissubcall;
static void xredisSubscriberRetire(xredisrequest* Request);
static void xredisSubscriberRelease(xredisclient* Client);
static bool xredisSubscriberCloseStep(xredisclient* Client);
static bool xredisSubscriberPrepare(xredisrequest* Request);
static void xredisSubscriberWritten(xredisrequest* Request);
static void xredisSubscriberFrame(xredisclient* Client,xredisreply* Reply,const xredisreplyvalue* Root,size_t Bytes);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
static void xredisSubscriberNotify(xpromise* Promise);
#endif
#define XREDIS_SUBSCRIBER_RETIRE(R) do{if((R)->Observe>=5)xredisSubscriberRetire(R);}while(0)
#else
#define XREDIS_SUBSCRIBER_RETIRE(R) ((void)0)
#endif

struct xredisrequest {
	xatomic32 References, Cancelled;
	xredisclient* Client;
	xpromise* Promise;
	xcancelwatch* Watch;
	xredisrequestresult* Result;
	xredisrequest *Next, *Previous, *SubmitNext, *CancelNext;
	size_t WireBytes;
	double Deadline;
	uint64 Timer;
	unsigned Phase;
	bool CancelQueued, Sensitive;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) || defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    uint8 Observe;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	bool Transaction;
#endif
	unsigned char Wire[];
};

struct xredisclient {
	xatomic32 References;
	xmutex Gate;
	xnetengine* Engine;
	xnetworker* Worker;
	xnetstream* Stream;
	xnetaddr Remote;
	xredisclientconfig Config;
	xredisreplyreader* Reader;
	xpromise* ClosePromise;
	xfuture* CloseFuture;
	xpromise* ReadyPromise;
	xfuture* ReadyFuture;
	xredisreadyresult* ReadyResult;
	xredisreply* Hello;
	xredisinitwire* InitWire;
	xredisinitmode InitMode;
	uint32 InitFlags;
	size_t InitOffset[XREDIS_I_END], InitLength[XREDIS_I_END], InitNext, InitAwait;
	double InitDeadline;
	uint64 InitTimer;
	xnetpost Wake;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
	xredisexclusiveowner* Owner;
#endif
	xredisrequest *SubmitHead, *SubmitTail, *CancelHead, *CancelTail;
	xredisrequest *PendingHead, *PendingTail, *SentHead, *SentTail;
	xredisclientinfo Info;
	xerror* CloseCause;
	size_t StorageBytes, SentCount, PushHead;
	bool WakeQueued, CloseDone, EnginePinned;
	/* Only the owning worker accesses these fields. */
	bool Started, TransportClosed, ReadEnded, Reading, ReadYielded, ReadPaused;
	bool Managed, TransportOpen;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	const xredisconnectplan* Plan;
	bool PlanOwned, PlanPending, PlanCancelled;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xtlsstream* TLS;
	xredisrequest* TlsWrite;
	size_t TlsWritten, TlsWriteBytes, TlsTurnBytes, TlsTurnCalls, InitWritten;
	bool InitSending;
#endif
	#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xatomicptr Observation;uint64 ObservationRejected;bool ObservationPreparing;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xatomicptr Subscriber;
#endif
    xredisreply* PushQueue[];
};

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
/* The tag occupies existing tail padding. Only transactions compute their
 * aligned state address; ordinary request allocation size stays unchanged. */
static xredistransactionstate* xredisTransactionState(xredisrequest* Request)
{
    size_t Alignment = _Alignof(xredistransactionstate);
    size_t Padding = (Alignment - (offsetof(xredisrequest, Wire) + Request->WireBytes) % Alignment) % Alignment;
    return (xredistransactionstate*)(Request->Wire + Request->WireBytes + Padding);
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisCachingPair(const xredisrequest* R) { return R->Observe==3 || R->Observe==4; }
static xrediscachingstate* xredisCachingState(xredisrequest* R)
{
    size_t A=_Alignof(xrediscachingstate);
    size_t Padding=(A-(offsetof(xredisrequest,Wire)+R->WireBytes)%A)%A;
    return (xrediscachingstate*)(R->Wire+R->WireBytes+Padding);
}
static bool xredisTrackingOutput(xredisclient* C,const void* Output,size_t Bytes);
static void xredisCachingWritten(xredisrequest* R);
static void xredisCachingFrame(xredisrequest* R,xredisreply* Reply,const xredisreplyvalue* Root,uint64 Sequence);
#endif
static void xredisClientWork(xnetworker* Worker, ptr Data);
static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data);
static void xredisClientReadBuffer(xredisclient* Client, const xnetbuf* Buffer);
static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data);
static void xredisClientRef(xredisclient* Client);
static bool xredisClientWakeLocked(xredisclient* Client);
static void xredisClientWake(xredisclient* Client);
static void xredisClientFail(xredisclient* Client, xredisrequeststatus Status, const xerror* Cause);
static void xredisClientReadyFinish(xredisclient* Client, xredisreadystatus Status, const xerror* Cause);
static void xredisClientInitPump(xredisclient* Client);
static void xredisClientInitFrame(xredisclient* Client, xredisreply* Reply, const xredisreplyvalue* Root);
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionPump(xredisrequest* Request);
static void xredisTransactionFrame(xredisrequest* Request, xredisreply* Reply, const xredisreplyvalue* Root);
#endif

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
static xredissubcall* xredisSubscriberState(xredisrequest* R)
{
    size_t A=_Alignof(xredissubcall),Padding=(A-(offsetof(xredisrequest,Wire)+R->WireBytes)%A)%A;
    return (xredissubcall*)(R->Wire+R->WireBytes+Padding);
}
#endif
static void xredisClientTransportAbort(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { (void)xrtTlsStreamAbort(Client->TLS); return; }
#endif
	if (Client->Stream != NULL) xrtNetStreamAbort(Client->Stream); else Client->TransportClosed = true;
}

static bool xredisClientTransportOpen(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) return xrtTlsStreamState(Client->TLS) == XTLS_STREAM_OPEN;
#endif
	return Client->Stream != NULL && xrtNetStreamState(Client->Stream) == XNET_STREAM_OPEN;
}

static const xnetbuf* xredisClientTransportBuffer(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) return Client->TransportOpen ? xrtTlsStreamBuffer(Client->TLS) : NULL;
#endif
	return Client->Stream == NULL ? NULL : xrtNetStreamBuffer(Client->Stream);
}

static bool xredisClientReport(xerrkind Kind, xredisclienterror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message); return false;
}

static double xredisClientDeadline(double Now, int64 Timeout)
{
	if (Timeout == XRT_WAIT_FOREVER) return INFINITY;
	if (Timeout == 0) return Now;
	return nextafter(Now + (double)(Timeout / 1000) + (double)(Timeout % 1000) * 0.001, INFINITY);
}

static void xredisInitWireRelease(xredisinitwire* Wire)
{
	if (Wire == NULL || xrtAtomic32FetchSub(&Wire->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Wire->Client;
	xrtSecureZero(Wire->Data, Wire->Size); xrtFree(Wire);
	if (Client != NULL) {
		xrtMutexLock(&Client->Gate); Client->Info.InitBytes = 0;
		if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
		xrtMutexUnlock(&Client->Gate); xredisClientRelease(Client);
	}
}

static void xredisInitWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisInitWireRelease((xredisinitwire*)Data); }

static void xredisClientReadyDrop(ptr Data, ptr Context)
{
	(void)Context; xredisreadyresult* Result = (xredisreadyresult*)Data;
	xredisReplyRelease(Result->Hello); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

XREDIS_API bool xredisClientInitConfigInit(xredisclientinitconfig* Init)
{
	if (Init == NULL || !xrtMemRangeValid(Init, sizeof(*Init)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization config output");
	*Init = (xredisclientinitconfig){0}; Init->Size = sizeof(*Init); Init->Mode = XREDIS_INIT_RESP3;
	Init->Timeout = 5000; Init->MaxWireBytes = 64u * 1024u; return true;
}

static bool xredisClientInitValid(const xredisclientinitconfig* Init, const xredisclientconfig* Config)
{
	const uint32 Known = XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME | XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT;
	if (Init->Size != sizeof(*Init) || (Init->Mode != XREDIS_INIT_AUTO && Init->Mode != XREDIS_INIT_RESP2 && Init->Mode != XREDIS_INIT_RESP3) ||
		(Init->Flags & ~Known) != 0 || Init->Timeout < XRT_WAIT_FOREVER || Init->MaxWireBytes == 0 ||
		((Init->Flags & XREDIS_INIT_USERNAME) && !(Init->Flags & XREDIS_INIT_AUTH)) ||
		(!(Init->Flags & XREDIS_INIT_AUTH) && Init->Password.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_USERNAME) && Init->Username.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_SETNAME) && Init->Name.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_SELECT) && Init->Database != 0) ||
		(Init->Mode != XREDIS_INIT_RESP2 && Config->Replies.Reader.Protocol != XREDIS_RESP3))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization mode, flags, limits or reader");
	const xbytesview Views[] = {Init->Username, Init->Password, Init->Name};
	for (size_t Index = 0; Index < sizeof(Views) / sizeof(Views[0]); ++Index)
		if ((Views[Index].Data == NULL && Views[Index].Size != 0) || !xrtMemRangeValid(Views[Index].Data, Views[Index].Size))
			return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization input span");
	return true;
}

static bool xredisClientInitBuild(xredisclient* Client, const xredisclientinitconfig* Init)
{
	xbytesview Args[XREDIS_I_END][7]; size_t Counts[XREDIS_I_END] = {0};
	char Database[32]; size_t DatabaseBytes = 0;
	if (Init->Flags & XREDIS_INIT_SELECT) {
		if (!xrtUIntWrite(Init->Database, 10, Database, sizeof(Database), &DatabaseBytes, 0)) return false;
	}
	if (Init->Mode != XREDIS_INIT_RESP2) {
		size_t N = 0; Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("HELLO");
		Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("3");
		if (Init->Flags & XREDIS_INIT_AUTH) {
			Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("AUTH");
			Args[XREDIS_I_HELLO][N++] = (Init->Flags & XREDIS_INIT_USERNAME) ? Init->Username : (xbytesview)XRT_BYTES_LITERAL("default");
			Args[XREDIS_I_HELLO][N++] = Init->Password;
		}
		if (Init->Flags & XREDIS_INIT_SETNAME) {
			Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("SETNAME"); Args[XREDIS_I_HELLO][N++] = Init->Name;
		}
		Counts[XREDIS_I_HELLO] = N;
	}
	if (Init->Mode != XREDIS_INIT_RESP3) {
		if (Init->Flags & XREDIS_INIT_AUTH) {
			size_t N = 0; Args[XREDIS_I_AUTH][N++] = (xbytesview)XRT_BYTES_LITERAL("AUTH");
			if (Init->Flags & XREDIS_INIT_USERNAME) Args[XREDIS_I_AUTH][N++] = Init->Username;
			Args[XREDIS_I_AUTH][N++] = Init->Password; Counts[XREDIS_I_AUTH] = N;
		}
		if (Init->Flags & XREDIS_INIT_SETNAME) {
			Args[XREDIS_I_NAME][0] = (xbytesview)XRT_BYTES_LITERAL("CLIENT"); Args[XREDIS_I_NAME][1] = (xbytesview)XRT_BYTES_LITERAL("SETNAME");
			Args[XREDIS_I_NAME][2] = Init->Name; Counts[XREDIS_I_NAME] = 3;
		}
	}
	if (Init->Flags & XREDIS_INIT_SELECT) {
		Args[XREDIS_I_SELECT][0] = (xbytesview)XRT_BYTES_LITERAL("SELECT");
		Args[XREDIS_I_SELECT][1] = (xbytesview){(cbytes)Database, DatabaseBytes}; Counts[XREDIS_I_SELECT] = 2;
	}
	size_t Total = 0;
	for (size_t Index = 0; Index < XREDIS_I_END; ++Index) if (Counts[Index] != 0) {
		size_t Bytes = 0;
		if (!xredisCommandSize(Args[Index], Counts[Index], &Bytes)) return false;
		if (Bytes > Client->Config.Stream.WriteLimit || Bytes > Init->MaxWireBytes - Total ||
			Bytes > SIZE_MAX - sizeof(xredisinitwire) - Total)
			return xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "initialization encoded wire exceeds configured limits");
		Client->InitOffset[Index] = Total; Client->InitLength[Index] = Bytes; Total += Bytes;
	}
	Client->InitNext = Init->Mode == XREDIS_INIT_RESP2 ? XREDIS_I_AUTH : XREDIS_I_HELLO;
	Client->InitAwait = XREDIS_I_END;
	if (Total == 0) return true;
	xredisinitwire* Wire = (xredisinitwire*)xrtMalloc(sizeof(*Wire) + Total);
	if (Wire == NULL) return false;
	xrtAtomic32Init(&Wire->References, 1); Wire->Client = NULL; Wire->Size = Total;
	for (size_t Index = 0; Index < XREDIS_I_END; ++Index) if (Counts[Index] != 0 &&
		!xredisCommandWrite(Wire->Data + Client->InitOffset[Index], Client->InitLength[Index], Args[Index], Counts[Index], NULL)) {
		xredisInitWireRelease(Wire); return false;
	}
	Wire->Client = Client; xredisClientRef(Client); Client->InitWire = Wire; Client->Info.InitBytes = Total;
	return true;
}

static void xredisClientRef(xredisclient* Client)
{ (void)xrtAtomic32FetchAdd(&Client->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisClientRetain(xredisclient* Client)
{
	if (Client == NULL) return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL");
	uint32 Count = xrtAtomic32Load(&Client->References, XMEMORY_RELAXED);
	for (;;) {
		/* Reserve the upper half for bounded private request/control references. */
		if (Count >= UINT32_MAX / 2u) return xredisClientReport(XERR_RANGE, XREDIS_CLIENT_STATE, "client reference count limit");
		if (xrtAtomic32CompareExchange(&Client->References, &Count, Count + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}

XREDIS_API void xredisClientRelease(xredisclient* Client)
{
	if (Client == NULL || xrtAtomic32FetchSub(&Client->References, 1, XMEMORY_ACQ_REL) != 1) return;
	for (size_t Index = 0; Index < Client->Config.MaxPushes; ++Index) xredisReplyRelease(Client->PushQueue[Index]);
	xredisReplyReaderDestroy(Client->Reader); xrtNetStreamDestroy(Client->Stream);
	xrtPromiseDestroy(Client->ClosePromise); xrtFutureDestroy(Client->CloseFuture); xrtErrorFree(Client->CloseCause);
	xrtPromiseDestroy(Client->ReadyPromise); xrtFutureDestroy(Client->ReadyFuture);
	xrtFree(Client->ReadyResult); xredisReplyRelease(Client->Hello);
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	if (Client->PlanOwned) Client->Plan->Drop(Client->Plan->Data);
#endif
	if (Client->EnginePinned) xrtNetEngineUnpin(Client->Engine);
	#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xrtFree(xrtAtomicPtrLoad(&Client->Observation,XMEMORY_RELAXED));
#endif
    #if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xredisSubscriberRelease(Client);
#endif
	xrtMutexUnit(&Client->Gate); xrtFree(Client);
}

/* Gate is held. One reference protects each accepted embedded-post callback. */
static bool xredisClientWakeLocked(xredisclient* Client)
{
	if (Client->CloseDone || Client->WakeQueued) return true;
	Client->WakeQueued = true; xredisClientRef(Client);
	if (xrtNetPost(Client->Worker, &Client->Wake, xredisClientWork, Client)) return true;
	Client->WakeQueued = false; xredisClientRelease(Client); return false;
}

static void xredisClientWake(xredisclient* Client)
{
	xrtMutexLock(&Client->Gate); (void)xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
}

static bool xredisClientClosing(xredisclient* Client)
{
	xrtMutexLock(&Client->Gate); bool Closing = Client->Info.State >= XREDIS_CLIENT_CLOSING;
	xrtMutexUnlock(&Client->Gate); return Closing;
}

/* A connection retired by ANOTHER request's drain grace must not report that
 * this request's own deadline expired. CloseStatus keeps the connection cause. */
static xredisrequeststatus xredisClientFailureStatus(xredisclient* Client)
{ return Client->Info.CloseStatus == XREDIS_REQUEST_TIMEOUT ? XREDIS_REQUEST_DISCONNECTED : Client->Info.CloseStatus; }

static void xredisRequestRef(xredisrequest* Request)
{ (void)xrtAtomic32FetchAdd(&Request->References, 1, XMEMORY_RELAXED); }

#if defined(XREDIS_FEATURE_XREDIS_POOL)
static bool xredisClientQuotaReserve(xredisexclusiveowner* Owner, size_t Bytes)
{
    xredisownerquota* Quota = Owner == NULL ? NULL : Owner->Quota;
    if (Quota == NULL) return true;
    uint64 Requests = xrtAtomic64Load(&Quota->Requests, XMEMORY_RELAXED);
    for (;;) {
        if (Requests >= Quota->MaxRequests) return false;
        if (xrtAtomic64CompareExchange(&Quota->Requests, &Requests, Requests + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) break;
    }
    uint64 Current = xrtAtomic64Load(&Quota->Bytes, XMEMORY_RELAXED);
    for (;;) {
        if ((uint64)Bytes > Quota->MaxBytes - Current) {
            xrtAtomic64FetchSub(&Quota->Requests, 1, XMEMORY_RELAXED); return false;
        }
        if (xrtAtomic64CompareExchange(&Quota->Bytes, &Current, Current + (uint64)Bytes, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
    }
}
#endif

/* Drop the real reservation only after its storage/constructor has retired.
 * An idle transition wakes the exclusive session without calling it under Gate. */
static void xredisClientReservationDrop(xredisclient* Client, size_t Bytes)
{
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    xredisexclusiveowner* Owner = NULL;
#endif
    xrtMutexLock(&Client->Gate);
    --Client->Info.Requests; Client->Info.CommandBytes -= Bytes;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Owner != NULL && Client->Owner->Quota != NULL) {
        xrtAtomic64FetchSub(&Client->Owner->Quota->Requests, 1, XMEMORY_RELAXED);
        xrtAtomic64FetchSub(&Client->Owner->Quota->Bytes, (uint64)Bytes, XMEMORY_RELAXED);
    }
#endif
    if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Info.Requests == 0 && Client->Owner != NULL) {
        Owner = Client->Owner; Owner->Retain(Owner->Data);
    }
#endif
    xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL) { Owner->Wake(Owner->Data); Owner->Release(Owner->Data); }
#endif
    xredisClientRelease(Client);
}

static void xredisRequestRelease(xredisrequest* Request)
{
	if (xrtAtomic32FetchSub(&Request->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Request->Client;
	size_t WireBytes = Request->WireBytes;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Request->Observe>=5){xredissubcall* S=xredisSubscriberState(Request);WireBytes+=S->MetadataBytes;xredisReplyRelease(S->Final);}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){
        xrediscachingstate* S=xredisCachingState(Request);WireBytes+=S->MetadataBytes;
        xredisReplyRelease(S->Control);xredisReplyRelease(S->Query);
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) {
		WireBytes += xredisTransactionState(Request)->MetadataBytes;
		xredisReplyRelease(xredisTransactionState(Request)->Error);
	}
#endif
	/* The reservation protects actual storage, including the free tail. Keep
	 * the independent Client reference while dropping it AFTER the buffer. */
	if (Request->Sensitive) xrtSecureZero(Request->Wire, Request->WireBytes);
	xrtFree(Request);
	xredisClientReservationDrop(Client, WireBytes);
}

static void xredisClientWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisRequestRelease((xredisrequest*)Data); }

static void xredisClientResultDrop(ptr Value, ptr Data)
{
	(void)Data; xredisrequestresult* Result = (xredisrequestresult*)Value;
	xredisReplyRelease(Result->Reply); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static void xredisCachingResultDrop(ptr Value,ptr Data)
{
    xrediscachingresult* R=Value;
    xredisReplyRelease(R->Control.Request.Reply);xrtErrorFree((xerror*)R->Control.Request.Cause);
    xredisClientResultDrop(&R->Query.Request,Data);
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionResultDrop(ptr Value, ptr Data)
{
	xredistransactionresult* Result = (xredistransactionresult*)Value;
	xredisReplyRelease(Result->QueueError); xredisClientResultDrop(&Result->Request, Data);
}
#endif

static void xredisClientCancel(ptr Data)
{
	xredisrequest* Request = (xredisrequest*)Data; xredisclient* Client = Request->Client;
	xrtAtomic32Store(&Request->Cancelled, 1, XMEMORY_RELEASE);
	xrtMutexLock(&Client->Gate);
	if (!Request->CancelQueued && !Client->CloseDone) {
		Request->CancelQueued = true; Request->CancelNext = NULL; xredisRequestRef(Request);
		if (Client->CancelTail != NULL) Client->CancelTail->CancelNext = Request; else Client->CancelHead = Request;
		Client->CancelTail = Request; (void)xredisClientWakeLocked(Client);
	}
	xrtMutexUnlock(&Client->Gate);
}

static void xredisClientComplete(xredisrequest* Request, xredisrequeststatus Status,
	xredisdelivery Delivery, xredisreply* Reply, const xerror* Cause)
{
	if (Request->Promise == NULL) { xredisReplyRelease(Reply); return; }
	/* No Gate is held. Our cancellation callback only queues/wakes and never
	 * waits for this worker, so physical Unwatch retirement cannot self-deadlock. */
	xrtCancelUnwatch(Request->Watch); Request->Watch = NULL;
	xpromise* Promise = Request->Promise; Request->Promise = NULL;
	xredisrequestresult* Result = Request->Result; Request->Result = NULL;
	xfuturefreeproc Drop = xredisClientResultDrop;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){
        xrediscachingstate* S=xredisCachingState(Request);xrediscachingresult* R=(xrediscachingresult*)Result;
        R->ControlReceived=S->Seen!=0;
        R->Control=(xredistrackingresult){
            {S->Seen?S->ControlStatus:Status,S->Seen?XREDIS_REPLIED:Delivery,S->Control,
             S->Seen?NULL:xrtErrorRef(Cause)},S->Seen?S->ControlSequence:0};
        S->Control=NULL;R->Query.Sequence=Reply?S->QuerySequence:0;Drop=xredisCachingResultDrop;
        if(S->Seen==2)Delivery=XREDIS_REPLIED;
        if(S->Query && !Reply){
            xredisReplyRelease(S->Query);S->Query=NULL;
            xrtMutexLock(&Request->Client->Gate);++Request->Client->Info.DiscardedReplies;xrtMutexUnlock(&Request->Client->Gate);
        }
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) {
		xredistransactionstate* State = xredisTransactionState(Request);
		xredistransactionresult* Transaction = (xredistransactionresult*)Result;
		Transaction->Execution = State->Execution; Transaction->Commands = State->Count;
		Transaction->Queued = State->Seen - State->Errors; Transaction->QueueErrors = State->Errors;
		Transaction->QueueError = State->Error; State->Error = NULL; Drop = xredisTransactionResultDrop;
		if (Delivery != XREDIS_REPLIED) Delivery = State->Execution == XREDIS_TRANSACTION_NOT_EXECUTED ? XREDIS_NOT_SENT : XREDIS_MAY_HAVE_EXECUTED;
	}
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Request->Observe>=5){
        xredissubcall* S=xredisSubscriberState(Request);xredissubscriber* M=xrtAtomicPtrLoad(&Request->Client->Subscriber,XMEMORY_ACQUIRE);
        xredissubscriberresult* R=(xredissubscriberresult*)Result;
        R->Acknowledged=S->Acknowledged;R->Subscriptions=M->Counts[0]+M->Counts[1];R->ShardSubscriptions=M->Counts[2];
        if(S->Finished)Delivery=XREDIS_REPLIED;
    }
#endif
	*Result = (xredisrequestresult){Status, Delivery, Reply, xrtErrorRef(Cause)};
	xrtMutexLock(&Request->Client->Gate); ++Request->Client->Info.Completed; xrtMutexUnlock(&Request->Client->Gate);
	if (!xrtPromiseResolveOwned(Promise, Result, Drop, NULL)) Drop(Result, NULL);
	xrtPromiseDestroy(Promise);
}

static void xredisClientCancelTimer(xredisrequest* Request)
{
	uint64 Timer = Request->Timer;
	if (Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Request->Client->Engine, Timer);
}

static bool xredisClientArm(xredisrequest* Request, double Deadline)
{
	if (Deadline == INFINITY) return true;
	double Milliseconds = ceil((Deadline - xrtTimer()) * 1000.0);
	int64 Timeout = Milliseconds <= 0 ? 0 : Milliseconds >= 0x1p63 ? INT64_MAX : (int64)Milliseconds;
	xredisRequestRef(Request);
	Request->Timer = xrtNetEngineAfter(Request->Client->Engine, xrtNetWorkerIndex(Request->Client->Worker),
		Timeout, xredisClientTimer, Request);
	if (Request->Timer != 0) return true;
	xredisRequestRelease(Request); return false;
}

static void xredisClientUnlink(xredisrequest* Request)
{
	xredisclient* Client = Request->Client;
	xredisrequest** Head = Request->Phase == XREDIS_R_SENT ? &Client->SentHead : &Client->PendingHead;
	xredisrequest** Tail = Request->Phase == XREDIS_R_SENT ? &Client->SentTail : &Client->PendingTail;
	if (Request->Previous != NULL) Request->Previous->Next = Request->Next; else *Head = Request->Next;
	if (Request->Next != NULL) Request->Next->Previous = Request->Previous; else *Tail = Request->Previous;
	if (Request->Phase == XREDIS_R_SENT) --Client->SentCount;
	Request->Previous = NULL; Request->Next = NULL;
}

static void xredisClientAppend(xredisclient* Client, xredisrequest* Request, bool Sent)
{
	xredisrequest** Head = Sent ? &Client->SentHead : &Client->PendingHead;
	xredisrequest** Tail = Sent ? &Client->SentTail : &Client->PendingTail;
	Request->Previous = *Tail; Request->Next = NULL;
	if (*Tail != NULL) (*Tail)->Next = Request; else *Head = Request;
	*Tail = Request; Request->Phase = Sent ? XREDIS_R_SENT : XREDIS_R_PENDING;
	if (Sent) ++Client->SentCount;
}

static void xredisClientFail(xredisclient* Client, xredisrequeststatus Status, const xerror* Cause)
{
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
		Client->Info.State = XREDIS_CLIENT_CLOSING; Client->Info.CloseStatus = Status;
		Client->CloseCause = xrtErrorRef(Cause);
	}
	xrtMutexUnlock(&Client->Gate);
	xredisClientTransportAbort(Client);
	xredisClientWake(Client);
}

static xredisreadystatus xredisClientReadyFailure(xredisrequeststatus Status)
{
	switch (Status) {
		case XREDIS_REQUEST_CLOSED: return XREDIS_READY_CLOSED;
		case XREDIS_REQUEST_TIMEOUT: return XREDIS_READY_TIMEOUT;
		case XREDIS_REQUEST_PROTOCOL: return XREDIS_READY_PROTOCOL;
		case XREDIS_REQUEST_LIMIT: return XREDIS_READY_LIMIT;
		case XREDIS_REQUEST_MEMORY: return XREDIS_READY_MEMORY;
		case XREDIS_REQUEST_INITIALIZATION: return XREDIS_READY_SERVER_ERROR;
		default: return XREDIS_READY_DISCONNECTED;
	}
}

static void xredisClientReadyFinish(xredisclient* Client, xredisreadystatus Status, const xerror* Cause)
{
	if (Client->ReadyPromise == NULL) return;
	xpromise* Promise = Client->ReadyPromise; Client->ReadyPromise = NULL;
	xredisreadyresult* Result = Client->ReadyResult; Client->ReadyResult = NULL;
	xrtMutexLock(&Client->Gate);
	if (Status == XREDIS_READY_OK && Client->Info.State >= XREDIS_CLIENT_CLOSING)
		Status = xredisClientReadyFailure(Client->Info.CloseStatus);
	if (Status == XREDIS_READY_OK) {
		Client->Info.Ready = true; Client->Info.State = XREDIS_CLIENT_OPEN; Client->Info.InitStep = XREDIS_INIT_DONE;
	}
	*Result = (xredisreadyresult){Status, Client->Info.InitStep, Client->Info.Protocol,
		Client->Info.Negotiated, Client->Info.Downgraded, NULL, xrtErrorRef(Cause)};
	xrtMutexUnlock(&Client->Gate);
	if (Status == XREDIS_READY_OK) Result->Hello = Client->Hello;
	else xredisReplyRelease(Client->Hello);
	Client->Hello = NULL;
	if (Client->InitTimer != 0) (void)xrtNetEngineTimerCancelCurrent(Client->Engine, Client->InitTimer);
	xredisinitwire* Wire = Client->InitWire; Client->InitWire = NULL; xredisInitWireRelease(Wire);
	if (!xrtPromiseResolveOwned(Promise, Result, xredisClientReadyDrop, NULL)) xredisClientReadyDrop(Result, NULL);
	xrtPromiseDestroy(Promise);
}

static void xredisClientInitReject(xredisclient* Client, xredisreadystatus Status)
{
	xerrkind Kind = Status == XREDIS_READY_AUTH_FAILED ? XERR_PERMISSION :
		Status == XREDIS_READY_UNSUPPORTED ? XERR_UNSUPPORTED : Status == XREDIS_READY_TIMEOUT ? XERR_TIMEOUT :
		Status == XREDIS_READY_PROTOCOL ? XERR_PROTOCOL : XERR_VALUE;
	/* Deliberately resident/redacted diagnostics: an unknown-command response
	 * can echo HELLO AUTH arguments. Never wrap/copy that server text. */
	xerror* Cause = xrtErrorCreate(Kind, "xrt.redis", XREDIS_CLIENT_INITIALIZATION_FAILED, "connection initialization failed");
	xredisClientFail(Client, XREDIS_REQUEST_INITIALIZATION, Cause);
	xredisClientReadyFinish(Client, Status, Cause); xrtErrorFree(Cause);
}

static void xredisClientInitTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker; xredisclient* Client = (xredisclient*)Data;
	if (Client->InitTimer == Id) Client->InitTimer = 0;
	if (Client->ReadyPromise != NULL) {
		if (Result == XNET_RESULT_OK) xredisClientInitReject(Client, XREDIS_READY_TIMEOUT);
		else if (Result != XNET_RESULT_CANCELLED) xredisClientFail(Client, XREDIS_REQUEST_CLOSED, NULL);
	}
	xredisClientWake(Client); xredisClientRelease(Client);
}

static bool xredisClientInitArm(xredisclient* Client)
{
	if (!Client->Managed || Client->InitDeadline == INFINITY) return true;
	double Remaining = ceil((Client->InitDeadline - xrtTimer()) * 1000.0);
	int64 Timeout = Remaining <= 0 ? 0 : Remaining >= 0x1p63 ? INT64_MAX : (int64)Remaining;
	xredisClientRef(Client);
	Client->InitTimer = xrtNetEngineAfter(Client->Engine, xrtNetWorkerIndex(Client->Worker), Timeout, xredisClientInitTimer, Client);
	if (Client->InitTimer != 0) return true;
	xredisClientRelease(Client); return false;
}

#if defined(XREDIS_FEATURE_XREDIS_TLS)
/* TLS copies accepted plaintext into authenticated record storage. Nothing
 * borrows input after Send returns. A short prefix must never be replayed. */
static xtlsresult xredisClientTlsSend(xredisclient* Client, cbytes Data, size_t Size, size_t* Written)
{
	*Written = 0;
	if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) { xredisClientWake(Client); return XTLS_AGAIN; }
	if (Size > Client->TlsTurnBytes) Size = Client->TlsTurnBytes;
	--Client->TlsTurnCalls; xtlsresult Result = xrtTlsStreamSend(Client->TLS, Data, Size, Written);
	Client->TlsTurnBytes -= *Written; return Result;
}

static void xredisClientTlsInitPump(xredisclient* Client)
{
	if (Client->InitAwait != XREDIS_I_END && !Client->InitSending) return;
	while (Client->InitNext < XREDIS_I_END && Client->InitLength[Client->InitNext] == 0) ++Client->InitNext;
	if (Client->InitNext == XREDIS_I_END) { xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL); return; }
	size_t Index = Client->InitNext; const xredisinitstep Steps[] = {XREDIS_INIT_HELLO, XREDIS_INIT_AUTHENTICATE, XREDIS_INIT_NAME, XREDIS_INIT_DATABASE};
	xrtMutexLock(&Client->Gate); Client->Info.InitStep = Steps[Index]; xrtMutexUnlock(&Client->Gate);
	Client->InitAwait = Index; Client->InitSending = true;
	while (Client->InitWritten < Client->InitLength[Index] && !xredisClientClosing(Client)) {
		xredisinitwire* Wire = Client->InitWire; (void)xrtAtomic32FetchAdd(&Wire->References, 1, XMEMORY_RELAXED);
		size_t Written = 0; xtlsresult Result = xredisClientTlsSend(Client, Wire->Data + Client->InitOffset[Index] + Client->InitWritten,
			Client->InitLength[Index] - Client->InitWritten, &Written);
		Client->InitWritten += Written; xredisInitWireRelease(Wire);
		if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
			xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XTLS_ERROR ? xrtGetError() : NULL); return;
		}
		if (Written == 0) return;
	}
	if (Client->InitWritten == Client->InitLength[Index]) { Client->InitWritten = 0; Client->InitSending = false; }
}
#endif

static void xredisClientInitPump(xredisclient* Client)
{
	if (Client->ReadyPromise == NULL || !Client->TransportOpen) return;
	if (xrtTimer() >= Client->InitDeadline) { xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { xredisClientTlsInitPump(Client); return; }
#endif
	if (Client->InitAwait != XREDIS_I_END) return;
	while (Client->InitNext < XREDIS_I_END && Client->InitLength[Client->InitNext] == 0) ++Client->InitNext;
	if (Client->InitNext == XREDIS_I_END) { xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL); return; }
	size_t Index = Client->InitNext;
	const xredisinitstep Steps[] = {XREDIS_INIT_HELLO, XREDIS_INIT_AUTHENTICATE, XREDIS_INIT_NAME, XREDIS_INIT_DATABASE};
	xrtMutexLock(&Client->Gate); Client->Info.InitStep = Steps[Index]; xrtMutexUnlock(&Client->Gate);
	xredisinitwire* Wire = Client->InitWire;
	Client->InitAwait = Index; (void)xrtAtomic32FetchAdd(&Wire->References, 1, XMEMORY_RELAXED);
	xnetresult Result = xrtNetStreamSendRef(Client->Stream, Wire->Data + Client->InitOffset[Index], Client->InitLength[Index], xredisInitWireDrop, Wire);
	if (Result == XNET_RESULT_OK) return;
	Client->InitAwait = XREDIS_I_END; xredisInitWireRelease(Wire);
	if (Result == XNET_RESULT_AGAIN) return;
	xredisClientFail(Client, Result == XNET_RESULT_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
		XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
}

static bool xredisClientInitLiteral(xbytesview Bytes, cstr Text)
{ size_t Length = strlen(Text); return Bytes.Size == Length && (Length == 0 || memcmp(Bytes.Data, Text, Length) == 0); }

static bool xredisClientInitCode(xbytesview Bytes, cstr Code)
{
	size_t Length = strlen(Code);
	return Bytes.Size >= Length && memcmp(Bytes.Data, Code, Length) == 0 && (Bytes.Size == Length || Bytes.Data[Length] == ' ');
}

static bool xredisClientInitUnsupported(xbytesview Bytes)
{
	if (xredisClientInitCode(Bytes, "NOPROTO")) return true;
	const cstr Prefix = "ERR unknown command 'HELLO'"; size_t Length = strlen(Prefix);
	if (Bytes.Size < Length || (Bytes.Size > Length && Bytes.Data[Length] != ',' && Bytes.Data[Length] != ' ')) return false;
	for (size_t Index = 0; Index < Length; ++Index) {
		unsigned char A = Bytes.Data[Index], B = (unsigned char)Prefix[Index];
		if (A >= 'a' && A <= 'z') A -= 'a' - 'A';
		if (B >= 'a' && B <= 'z') B -= 'a' - 'A';
		if (A != B) return false;
	}
	return true;
}

static bool xredisClientInitHello(xredisreply* Reply, const xredisreplyvalue* Root)
{
	if (Root->Type != XREDIS_REPLY_MAP || (Root->Flags & XREDIS_REPLY_NULL_VALUE)) return false;
	uint32 Seen = 0;
	for (size_t Index = Root->FirstChild; Index != XREDIS_REPLY_NONE;) {
		xredisreplyvalue Key, Value;
		if (!xredisReplyGet(Reply, Index, &Key) || Key.Next == XREDIS_REPLY_NONE || !xredisReplyGet(Reply, Key.Next, &Value)) return false;
		if ((Key.Type == XREDIS_REPLY_SIMPLE_STRING || Key.Type == XREDIS_REPLY_BULK_STRING) && !(Key.Flags & XREDIS_REPLY_NULL_VALUE)) {
			uint32 Bit = xredisClientInitLiteral(Key.Data, "proto") ? 1u : xredisClientInitLiteral(Key.Data, "server") ? 2u :
				xredisClientInitLiteral(Key.Data, "version") ? 4u : 0;
			if (Bit != 0) {
				if ((Seen & Bit) || (Bit == 1u ? Value.Type != XREDIS_REPLY_INTEGER || Value.Integer != 3 :
					(Value.Type != XREDIS_REPLY_SIMPLE_STRING && Value.Type != XREDIS_REPLY_BULK_STRING) || (Value.Flags & XREDIS_REPLY_NULL_VALUE))) return false;
				Seen |= Bit;
			}
		}
		Index = Value.Next;
	}
	return Seen == 7u;
}

static void xredisClientInitFrame(xredisclient* Client, xredisreply* Reply, const xredisreplyvalue* Root)
{
	if (xrtTimer() >= Client->InitDeadline) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
	size_t Index = Client->InitAwait;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->InitSending) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
#endif
	if (Index >= XREDIS_I_END) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
	if (Root->Type == XREDIS_REPLY_SIMPLE_ERROR || Root->Type == XREDIS_REPLY_BULK_ERROR) {
		bool Unsupported = Index == XREDIS_I_HELLO && xredisClientInitUnsupported(Root->Data);
		bool Authentication = xredisClientInitCode(Root->Data, "WRONGPASS") ||
			xredisClientInitCode(Root->Data, "NOAUTH") ||
			((Index == XREDIS_I_HELLO || Index == XREDIS_I_AUTH) && (Client->InitFlags & XREDIS_INIT_AUTH) &&
			 xredisClientInitLiteral(Root->Data, "ERR invalid password"));
		xredisReplyRelease(Reply);
		if (Unsupported && Client->InitMode == XREDIS_INIT_AUTO) {
			xrtMutexLock(&Client->Gate); Client->Info.Downgraded = true; Client->Info.Protocol = XREDIS_RESP2; xrtMutexUnlock(&Client->Gate);
			Client->InitNext = XREDIS_I_AUTH; Client->InitAwait = XREDIS_I_END; xredisClientWake(Client); return;
		}
		xredisClientInitReject(Client, Unsupported ? XREDIS_READY_UNSUPPORTED : Authentication ? XREDIS_READY_AUTH_FAILED : XREDIS_READY_SERVER_ERROR);
		return;
	}
	bool Valid = Index == XREDIS_I_HELLO ? xredisClientInitHello(Reply, Root) :
		Root->Type == XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data, "OK");
	if (!Valid) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
	if (Index == XREDIS_I_HELLO) {
		Client->Hello = Reply;
		xrtMutexLock(&Client->Gate); Client->Info.Protocol = XREDIS_RESP3; Client->Info.Negotiated = true; xrtMutexUnlock(&Client->Gate);
		Client->InitNext = XREDIS_I_SELECT;
	} else { xredisReplyRelease(Reply); Client->InitNext = Index + 1u; }
	Client->InitAwait = XREDIS_I_END; xredisClientWake(Client);
}

static void xredisClientAbandon(xredisrequest* Request, xredisrequeststatus Status)
{
	xredisclient* Client = Request->Client;
	if (Request->Promise == NULL) return;
	xredisClientComplete(Request, Status, Request->Phase == XREDIS_R_SENT ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT, NULL, NULL);
	xredisClientCancelTimer(Request);
	if (Request->Phase == XREDIS_R_SENT) {
		if (!xredisClientArm(Request, xredisClientDeadline(xrtTimer(), Client->Config.DrainTimeout)))
			xredisClientFail(Client, XREDIS_REQUEST_MEMORY, xrtGetError());
	} else if (Request->Phase == XREDIS_R_PENDING) {
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request);
	}
}

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static void xredisCachingFinish(xredisrequest* R)
{
    xrediscachingstate* S=xredisCachingState(R);xredisclient* C=R->Client;
    if(!S->Written || S->Seen!=2)return;
    if(R->Promise){
        xredisrequeststatus Status=S->QueryStatus;
        if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE))Status=XREDIS_REQUEST_CANCELLED;
        else if(xrtTimer()>=R->Deadline)Status=XREDIS_REQUEST_TIMEOUT;
        xredisreply* Reply=S->Query;S->Query=NULL;
        if(Status==XREDIS_REQUEST_CANCELLED || Status==XREDIS_REQUEST_TIMEOUT){
            xredisReplyRelease(Reply);Reply=NULL;
            xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);
        }
        xredisClientComplete(R,Status,XREDIS_REPLIED,Reply,NULL);
    }
    xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;
    xredisClientCancelTimer(R);xredisRequestRelease(R);xredisClientWake(C);
}
static void xredisCachingWritten(xredisrequest* R)
{
    if(!xredisCachingPair(R))return;
    xredisCachingState(R)->Written=true;xredisCachingFinish(R);
}
static void xredisCachingFrame(xredisrequest* R,xredisreply* Reply,const xredisreplyvalue* Root,uint64 Sequence)
{
    xrediscachingstate* S=xredisCachingState(R);xredisclient* C=R->Client;
    if(S->Seen>=2){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;}
    bool Error=Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR;
    if(S->Seen++==0){
        S->ControlStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(!Error && !(Root->Type==XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data,"OK"))){
            S->ControlStatus=XREDIS_REQUEST_PROTOCOL;xredisReplyRelease(Reply);
            xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;
        }
        if(R->Promise){S->Control=Reply;S->ControlSequence=Sequence;}
        else {xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
    }else{
        S->QueryStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(R->Promise){S->Query=Reply;S->QuerySequence=Sequence;}
        else {xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
        xredisCachingFinish(R);
    }
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionObserve(xredisrequest* Request)
{
	if (Request->Promise != NULL) {
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) xredisClientAbandon(Request, XREDIS_REQUEST_CANCELLED);
		else if (xrtTimer() >= Request->Deadline) xredisClientAbandon(Request, XREDIS_REQUEST_TIMEOUT);
	}
}

static void xredisTransactionNext(xredisrequest* Request, unsigned Phase)
{
	xredistransactionstate* State = xredisTransactionState(Request);
	State->Phase = Phase; State->Written = 0; State->Await = false; xredisClientWake(Request->Client);
}

static void xredisTransactionFinish(xredisrequest* Request, xredisrequeststatus Status, xredisreply* Reply)
{
	xredisclient* Client = Request->Client;
	if (Request->Promise == NULL) {
		xredisReplyRelease(Reply);
		xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	} else {
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) Status = XREDIS_REQUEST_CANCELLED;
		else if (xrtTimer() >= Request->Deadline) Status = XREDIS_REQUEST_TIMEOUT;
		if (Status == XREDIS_REQUEST_CANCELLED || Status == XREDIS_REQUEST_TIMEOUT) { xredisReplyRelease(Reply); Reply = NULL; }
		xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request);
	xredisClientCancelTimer(Request); xredisRequestRelease(Request); xredisClientWake(Client);
}

static void xredisTransactionPump(xredisrequest* Request)
{
	xredisclient* Client = Request->Client; xredistransactionstate* State = xredisTransactionState(Request);
	xredisTransactionObserve(Request);
	if (xredisClientClosing(Client)) return;
	if (State->Await) return;
	if (Request->Promise == NULL && State->Written == 0) {
		if (State->Phase == XREDIS_TX_MULTI) {
			xredisTransactionFinish(Request, XREDIS_REQUEST_CANCELLED, NULL); return;
		}
		if (State->Phase == XREDIS_TX_COMMANDS || State->Phase == XREDIS_TX_EXEC)
			State->Phase = XREDIS_TX_DISCARD;
	}
	size_t Offset = 0, Length = sizeof(xredisTransactionMulti) - 1u;
	if (State->Phase == XREDIS_TX_COMMANDS) { Offset = Length; Length = State->CommandsBytes; }
	else if (State->Phase == XREDIS_TX_EXEC) { Offset = Length + State->CommandsBytes; Length = sizeof(xredisTransactionExec) - 1u; }
	else if (State->Phase == XREDIS_TX_DISCARD) {
		Offset = Length + State->CommandsBytes + sizeof(xredisTransactionExec) - 1u; Length = sizeof(xredisTransactionDiscard) - 1u;
	}
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) {
		while (State->Written < Length && !xredisClientClosing(Client)) {
			if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) { xredisClientWake(Client); return; }
			if (Client->TlsWrite == NULL) { Client->TlsWrite = Request; xredisRequestRef(Request); }
			size_t Written = 0;
			xtlsresult Result = xredisClientTlsSend(Client, Request->Wire + Offset + State->Written, Length - State->Written, &Written);
			State->Written += Written;
			if (State->Phase == XREDIS_TX_EXEC && Written != 0) State->Execution = XREDIS_TRANSACTION_MAY_HAVE_EXECUTED;
			if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
				xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
					XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, Result == XTLS_ERROR ? xrtGetError() : NULL); return;
			}
			if (Written == 0) return;
		}
		if (State->Written != Length) return; /* Concurrent Close owns the remaining write reference. */
		Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Request);
	} else
#endif
	{
		xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire + Offset, Length, xredisClientWireDrop, Request);
		if (Result != XNET_RESULT_OK) {
			xredisRequestRelease(Request);
			if (Result != XNET_RESULT_AGAIN) xredisClientFail(Client,
				xrtErrorKind(xrtGetError()) == XERR_MEMORY && Result == XNET_RESULT_ERROR ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
			return;
		}
		State->Written = Length;
		if (State->Phase == XREDIS_TX_EXEC) State->Execution = XREDIS_TRANSACTION_MAY_HAVE_EXECUTED;
	}
	State->Await = true;
	/* QUEUED replies may arrive while a fragmented TLS command batch is still
	 * being sent. Only cross this boundary after BOTH the full send and N replies. */
	if (State->Phase == XREDIS_TX_COMMANDS && State->Seen == State->Count) {
		xredisTransactionObserve(Request);
		xredisTransactionNext(Request, State->Errors != 0 || Request->Promise == NULL ? XREDIS_TX_DISCARD : XREDIS_TX_EXEC);
	}
}

static void xredisTransactionFrame(xredisrequest* Request, xredisreply* Reply, const xredisreplyvalue* Root)
{
	xredisclient* Client = Request->Client; xredistransactionstate* State = xredisTransactionState(Request);
	bool Error = Root->Type == XREDIS_REPLY_SIMPLE_ERROR || Root->Type == XREDIS_REPLY_BULK_ERROR;
	if (State->Phase == XREDIS_TX_COMMANDS && State->Seen < State->Count) {
		xredisTransactionObserve(Request);
		if (!Error && !(Root->Type == XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data, "QUEUED"))) goto protocol;
		++State->Seen;
		if (Error) {
			++State->Errors;
			if (Request->Promise != NULL && State->Error == NULL) { State->Error = Reply; Reply = NULL; }
		}
		xredisReplyRelease(Reply);
		if (State->Await && State->Seen == State->Count)
			xredisTransactionNext(Request, State->Errors != 0 || Request->Promise == NULL ? XREDIS_TX_DISCARD : XREDIS_TX_EXEC);
		return;
	}
	if (!State->Await) goto protocol;
	if (State->Phase == XREDIS_TX_MULTI) {
		if (Error) { xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return; }
		if (Root->Type != XREDIS_REPLY_SIMPLE_STRING || !xredisClientInitLiteral(Root->Data, "OK")) goto protocol;
		xredisTransactionObserve(Request);
		xredisReplyRelease(Reply);
		xredisTransactionNext(Request, Request->Promise == NULL ? XREDIS_TX_DISCARD : State->Count ? XREDIS_TX_COMMANDS : XREDIS_TX_EXEC); return;
	}
	if (State->Phase == XREDIS_TX_EXEC) {
		if (Root->Type == XREDIS_REPLY_ARRAY && !(Root->Flags & XREDIS_REPLY_NULL_VALUE) && Root->ChildCount == State->Count)
			State->Execution = XREDIS_TRANSACTION_EXECUTED;
		else if (Root->Type == XREDIS_REPLY_NULL || (Root->Type == XREDIS_REPLY_ARRAY && (Root->Flags & XREDIS_REPLY_NULL_VALUE)))
			State->Execution = XREDIS_TRANSACTION_ABORTED;
		else if (Error) {
			if (xredisClientInitCode(Root->Data, "EXECABORT")) State->Execution = XREDIS_TRANSACTION_ABORTED;
			else {
				/* An unknown EXEC error does not certify that the connection left
				 * MULTI. Do not expose possibly dirty state to following calls. */
				xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
			}
			xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return;
		} else goto protocol;
		xredisTransactionFinish(Request, XREDIS_REQUEST_OK, Reply); return;
	}
	if (State->Phase == XREDIS_TX_DISCARD) {
		if (Error) {
			xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
			xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return;
		}
		if (Root->Type != XREDIS_REPLY_SIMPLE_STRING || !xredisClientInitLiteral(Root->Data, "OK")) goto protocol;
		xredisReplyRelease(Reply); xredisTransactionFinish(Request, State->Errors ? XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_CANCELLED, NULL); return;
	}
protocol:
	xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL);
}
#endif

static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker; xredisrequest* Request = (xredisrequest*)Data; xredisclient* Client = Request->Client;
	if (Request->Timer == Id) Request->Timer = 0;
	if (Result == XNET_RESULT_OK) {
		if (Request->Promise != NULL) xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ?
			XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT);
		else if (Request->Phase == XREDIS_R_SENT) xredisClientFail(Client, XREDIS_REQUEST_TIMEOUT, NULL);
	} else if (Result != XNET_RESULT_CANCELLED && Request->Phase != XREDIS_R_DONE)
		xredisClientFail(Client, XREDIS_REQUEST_CLOSED, NULL);
	xredisClientWake(Client); xredisRequestRelease(Request);
}

static void xredisClientOnOpen(xnetstream* Stream, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data;
	Client->TransportOpen = true;
	if (!Client->Managed) xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL);
	else if (Client->InitMode == XREDIS_INIT_RESP2) {
		xrtMutexLock(&Client->Gate); Client->Info.Protocol = XREDIS_RESP2; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientWake(Client);
}

static void xredisClientOnEnd(xnetstream* Stream, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data; Client->ReadEnded = true; xredisClientWake(Client);
}

static void xredisClientOnWritable(xnetstream* Stream, ptr Data)
{ (void)Stream; xredisClientWake((xredisclient*)Data); }

static void xredisClientOnLowWater(xnetstream* Stream, size_t Queued, ptr Data)
{ (void)Queued; xredisClientOnWritable(Stream, Data); }

static void xredisClientOnClose(xnetstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Stream; (void)Result; xredisclient* Client = (xredisclient*)Data;
	Client->TransportClosed = true;
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
		Client->Info.State = XREDIS_CLIENT_CLOSING;
		Client->Info.CloseStatus = xrtErrorKind(Error) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED;
		Client->CloseCause = xrtErrorRef(Error);
	}
	xrtMutexUnlock(&Client->Gate); xredisClientWake(Client);
}

static const xnetstreamevents xredisClientEvents = {
	xredisClientOnOpen, xredisClientRead, xredisClientOnEnd, NULL,
	xredisClientOnLowWater, xredisClientOnWritable, xredisClientOnClose
};

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
static xredisrequeststatus xredisClientConnectFailure(const xerror* Cause)
{
	xerrkind Kind = xrtErrorKind(Cause);
	return Kind == XERR_MEMORY ? XREDIS_REQUEST_MEMORY :
		(Kind == XERR_AGAIN || Kind == XERR_RANGE) ? XREDIS_REQUEST_LIMIT : XREDIS_REQUEST_DISCONNECTED;
}

void __xredisClientConnectDone(xredisclient* Client, xnetresult Result,
	xnetstream* Stream, const xerror* Cause)
{
	if (Stream != NULL) {
		Client->Stream = Stream;
		/* A close requested during DNS may have observed no transport. A late
		 * winning reference still has to reach its ACTUAL TCP close barrier. */
		Client->TransportClosed = xrtNetStreamState(Stream) == XNET_STREAM_CLOSED;
		if (xredisClientClosing(Client)) xrtNetStreamAbort(Stream);
	} else {
		/* A cancelled pending Dial never created a stream. Its terminal callback
		 * is the physical transport barrier even when public Close won first. */
		Client->TransportClosed = true;
		if (!xredisClientClosing(Client)) {
		xredisrequeststatus Status = Result == XNET_RESULT_TIMEOUT || Result == XNET_RESULT_CANCELLED ?
			XREDIS_REQUEST_DISCONNECTED : xredisClientConnectFailure(Cause);
		xredisClientFail(Client, Status, Cause);
		xredisreadystatus Ready = Result == XNET_RESULT_TIMEOUT ? XREDIS_READY_TIMEOUT :
			Result == XNET_RESULT_CANCELLED ? XREDIS_READY_CLOSED : xredisClientReadyFailure(Status);
		xredisClientReadyFinish(Client, Ready, Cause);
		}
	}
	xredisClientWake(Client);
}

void __xredisClientConnectRetired(xredisclient* Client)
{ Client->PlanPending = false; xredisClientWake(Client); }
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void xredisClientTlsOpen(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientOnOpen(NULL, Data); }
static void xredisClientTlsRead(xtlsstream* Stream, const xnetbuf* Buffer, ptr Data)
{ (void)Stream; xredisClientReadBuffer((xredisclient*)Data, Buffer); }
static void xredisClientTlsEnd(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientOnEnd(NULL, Data); }
static void xredisClientTlsWritable(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientWake((xredisclient*)Data); }
static void xredisClientTlsClose(xtlsstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data; xredisClientOnClose(NULL, Result, Error, Data);
	if (Client->ReadyPromise != NULL && xrtErrorKind(Error) == XERR_TIMEOUT) xredisClientReadyFinish(Client, XREDIS_READY_TIMEOUT, Error);
}
static const xtlsstreamevents xredisClientTlsEvents = {
	xredisClientTlsOpen, xredisClientTlsRead, xredisClientTlsEnd, xredisClientTlsWritable, xredisClientTlsWritable, xredisClientTlsClose, NULL
};
const xnetstreamevents* __xredisClientTlsPendingEvents(void)
{
	static const xnetstreamevents Events = {NULL, NULL, NULL, NULL, NULL, NULL, xredisClientOnClose}; return &Events;
}
bool __xredisClientConnectTls(xredisclient* Client, xnetstream* Transport, const xredistlsplan* Plan)
{
	if (xredisClientClosing(Client) || xrtNetStreamState(Transport) != XNET_STREAM_OPEN) {
		__xredisClientConnectDone(Client, XNET_RESULT_OK, Transport, NULL);
		if (!xredisClientClosing(Client)) xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
		return false;
	}
	Client->TransportClosed = false; Client->TlsWriteBytes = Plan->WriteBytes;
	if (xrtTlsStreamAttach(Transport, Plan->Session, &Plan->Config, &xredisClientTlsEvents, Client, &Client->TLS)) return true;
	xerror* Cause = xrtErrorRef(xrtGetError()); __xredisClientConnectDone(Client, XNET_RESULT_ERROR, Transport, Cause);
	xredisClientFail(Client, xredisClientConnectFailure(Cause), Cause); xrtErrorFree(Cause); return false;
}
#endif

static void xredisClientFrame(xredisclient* Client, xredisreply* Reply)
{
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Reply, &Info);
	xredisreplyvalue Root; xredisReplyGet(Reply, Info.Root, &Root);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)){xredisSubscriberFrame(Client,Reply,&Root,Info.ReservedBytes);return;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xredisobservation* Observation=xrtAtomicPtrLoad(&Client->Observation,XMEMORY_ACQUIRE);uint64 Sequence=0;
    if(Observation){
        /* One IO worker writes; external Info readers only load this atomic. */
        Sequence=xrtAtomic64Load(&Observation->Sequence,XMEMORY_RELAXED);
        if(Sequence==UINT64_MAX){xredisReplyRelease(Reply);xredisClientFail(Client,XREDIS_REQUEST_LIMIT,NULL);return;}
        ++Sequence;xrtAtomic64Store(&Observation->Sequence,Sequence,XMEMORY_RELEASE);
    }
#endif
    if (Root.Type == XREDIS_REPLY_PUSH) {
		xrtMutexLock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        /* Adoption can race the initial mode load, but cannot publish between
         * this recheck and ordinary push enqueue under the same Gate. */
        if(xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)){
            xrtMutexUnlock(&Client->Gate);xredisSubscriberFrame(Client,Reply,&Root,Info.ReservedBytes);return;
        }
#endif
		bool Fits = Client->Info.Pushes < Client->Config.MaxPushes &&
			Info.ReservedBytes <= Client->Config.MaxPushBytes - Client->Info.PushBytes;
		if (Fits) {
			size_t Index = (Client->PushHead + Client->Info.Pushes) % Client->Config.MaxPushes;
			Client->PushQueue[Index] = Reply; ++Client->Info.Pushes; Client->Info.PushBytes += Info.ReservedBytes;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            if(Observation)Observation->Pushes[Index]=Sequence;
#endif
		}
		xrtMutexUnlock(&Client->Gate);
		if (!Fits) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_LIMIT, NULL); }
		return;
	}
	if (Client->ReadyPromise != NULL) { xredisClientInitFrame(Client, Reply, &Root); return; }
	xredisrequest* Request = Client->SentHead;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Request != NULL && Request == Client->TlsWrite
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        && !xredisCachingPair(Request)
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
		&& !Request->Transaction
#endif
	) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#endif
	if (Request == NULL) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) { xredisTransactionFrame(Request, Reply, &Root); return; }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){xredisCachingFrame(Request,Reply,&Root,Sequence);return;}
    if(Request->Observe==2 && Root.Type!=XREDIS_REPLY_SIMPLE_ERROR && Root.Type!=XREDIS_REPLY_BULK_ERROR &&
        !(Root.Type==XREDIS_REPLY_SIMPLE_STRING && Root.Data.Size==2 && !memcmp(Root.Data.Data,"OK",2))){
        xredisReplyRelease(Reply);xredisClientFail(Client,XREDIS_REQUEST_PROTOCOL,NULL);return;
    }
#endif
	if (Request->Promise != NULL) {
		xredisrequeststatus Status = Root.Type == XREDIS_REPLY_SIMPLE_ERROR || Root.Type == XREDIS_REPLY_BULK_ERROR ?
			XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_OK;
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) Status = XREDIS_REQUEST_CANCELLED;
		else if (xrtTimer() >= Request->Deadline) Status = XREDIS_REQUEST_TIMEOUT;
		if (Status == XREDIS_REQUEST_CANCELLED || Status == XREDIS_REQUEST_TIMEOUT) {
			xredisReplyRelease(Reply); Reply = NULL;
			xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
		}
		#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        if(Request->Observe)((xredistrackingresult*)Request->Result)->Sequence=Reply?Sequence:0;
#endif
        xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	} else {
		xredisReplyRelease(Reply); xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request);
	xredisClientCancelTimer(Request); xredisRequestRelease(Request); xredisClientWake(Client);
}

static void xredisClientReadBuffer(xredisclient* Client, const xnetbuf* Buffer)
{
	if (Client->Reading || xredisClientClosing(Client)) return;
	Client->Reading = true; size_t Bytes = 0, Frames = 0;
	while (Bytes < Client->Config.ReadBytes && Frames < Client->Config.ReadFrames && !xredisClientClosing(Client)) {
		xnetspan Span; if (!xrtNetBufFront(Buffer, &Span)) break;
		size_t Available = Client->Config.ReadBytes - Bytes;
		if (Span.Size > Available) Span.Size = Available;
		xredisreply* Reply = NULL; size_t Consumed = 0;
		xredisreplystatus Status = xredisReplyReaderFeed(Client->Reader, (xbytesview){Span.Data, Span.Size}, &Consumed, &Reply);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TLS != NULL) (void)xrtTlsStreamConsume(Client->TLS, Consumed); else
#endif
		(void)xrtNetBufConsume((xnetbuf*)Buffer, Consumed);
		Bytes += Consumed;
		if (Status == XREDIS_REPLY_ERROR) {
			xredisreplyreaderinfo State = {0}; State.Size = sizeof(State); xredisReplyReaderInfo(Client->Reader, &State);
			xredisrequeststatus Error = State.Error == XREDIS_REPLY_MEMORY ? XREDIS_REQUEST_MEMORY :
				State.Error == XREDIS_REPLY_LIMIT ? XREDIS_REQUEST_LIMIT : XREDIS_REQUEST_PROTOCOL;
			xredisClientFail(Client, Error, xrtGetError()); break;
		}
		if (Status == XREDIS_REPLY_READY) { ++Frames; xredisClientFrame(Client, Reply); }
		else if (Consumed == 0) break;
	}
	Client->Reading = false;
	if (!xredisClientClosing(Client) && (Bytes == Client->Config.ReadBytes || Frames == Client->Config.ReadFrames)) {
		Client->ReadYielded = true; Client->ReadPaused = true;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TLS == NULL)
#endif
		xrtNetStreamPause(Client->Stream);
		xredisClientWake(Client);
	}
}

static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{ (void)Stream; xredisClientReadBuffer((xredisclient*)Data, Buffer); }

static void xredisClientIngest(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests; ++Work) {
		xrtMutexLock(&Client->Gate); xredisrequest* Request = Client->SubmitHead;
		if (Request != NULL) {
			Client->SubmitHead = Request->SubmitNext;
			if (Client->SubmitHead == NULL) Client->SubmitTail = NULL;
		}
		xrtMutexUnlock(&Client->Gate);
		if (Request == NULL) break;
		if (Request->Promise == NULL) { Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request); continue; }
		if (xredisClientClosing(Client)) {
			xredisClientComplete(Request, xredisClientFailureStatus(Client), XREDIS_NOT_SENT, NULL, Client->CloseCause);
			Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request); continue;
		}
		xredisClientAppend(Client, Request, false);
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) xredisClientAbandon(Request, XREDIS_REQUEST_CANCELLED);
		else if (xrtTimer() >= Request->Deadline) xredisClientAbandon(Request, XREDIS_REQUEST_TIMEOUT);
		else if (!xredisClientArm(Request, Request->Deadline)) {
			xredisClientComplete(Request, XREDIS_REQUEST_MEMORY, XREDIS_NOT_SENT, NULL, xrtGetError());
			xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisRequestRelease(Request);
		}
	}
}

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void xredisClientTlsPump(xredisclient* Client)
{
	while (!xredisClientClosing(Client) && xredisClientTransportOpen(Client)) {
		if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) {
			if (Client->TlsWrite != NULL || (Client->PendingHead != NULL && Client->SentCount < Client->Config.MaxInFlight)) xredisClientWake(Client);
			return;
		}
		xredisrequest* Request = Client->TlsWrite;
		if (Request == NULL) {
			if (Client->PendingHead == NULL || Client->SentCount >= Client->Config.MaxInFlight) return;
			Request = Client->PendingHead;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
			if (Request->Transaction) {
				if (Client->SentCount != 0) return;
				xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisTransactionPump(Request); return;
			}
#endif
			if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) || xrtTimer() >= Request->Deadline) {
				--Client->TlsTurnCalls;
				xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT); continue;
			}

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        if(Request->Observe>=5 && !xredisSubscriberPrepare(Request))continue;
#endif
        xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
			Client->TlsWrite = Request; Client->TlsWritten = 0;
		}
		size_t Written = 0; xtlsresult Result = xredisClientTlsSend(Client, Request->Wire + Client->TlsWritten, Request->WireBytes - Client->TlsWritten, &Written);
		Client->TlsWritten += Written;
		if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
			/* Zero accepted bytes still means NOT_SENT. A partial command is a
			 * sent FIFO obligation even after the caller abandons its Future. */
			if (Client->TlsWritten == 0) { xredisClientUnlink(Request); xredisClientAppend(Client, Request, false); }
			xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XTLS_ERROR ? xrtGetError() : NULL); return;
		}
		if (Client->TlsWritten == Request->WireBytes) {
			Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Request);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            xredisCachingWritten(Request);
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
            if(Request->Observe>=5)xredisSubscriberWritten(Request);
#endif
			continue;
		}
		if (Written == 0) {
			if (Client->TlsWritten == 0 && Client->TlsWrite != NULL) {
				/* No byte was accepted: keep original FIFO position and report
				 * NOT_SENT if the caller now expires/cancels while blocked. */
				xredisClientUnlink(Request); Request->Phase = XREDIS_R_PENDING;
				Request->Next = Client->PendingHead;
				if (Client->PendingHead != NULL) Client->PendingHead->Previous = Request; else Client->PendingTail = Request;
				Client->PendingHead = Request; Client->TlsWrite = NULL; xredisRequestRelease(Request);
			}
			return;
		}
	}
}
#endif

static void xredisClientPump(xredisclient* Client)
{
	if (Client->ReadyPromise != NULL) return;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Client->SentHead != NULL && Client->SentHead->Transaction) { xredisTransactionPump(Client->SentHead); return; }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { xredisClientTlsPump(Client); return; }
#endif
	for (size_t Work = 0; Work < Client->Config.WorkRequests && Client->PendingHead != NULL &&
		Client->SentCount < Client->Config.MaxInFlight && !xredisClientClosing(Client) &&
		xredisClientTransportOpen(Client); ++Work) {
		xredisrequest* Request = Client->PendingHead;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
		if (Request->Transaction) {
			if (Client->SentCount != 0) return;
			xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisTransactionPump(Request); return;
		}
#endif
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) || xrtTimer() >= Request->Deadline) {
			xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ?
				XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT); continue;
		}

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        if(Request->Observe>=5 && !xredisSubscriberPrepare(Request))continue;
#endif
        xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire, Request->WireBytes, xredisClientWireDrop, Request);
		if (Result == XNET_RESULT_OK) {
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            xredisCachingWritten(Request);
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
            if(Request->Observe>=5)xredisSubscriberWritten(Request);
#endif
            continue;
        }
		xredisRequestRelease(Request); xredisClientUnlink(Request);
		Request->Phase = XREDIS_R_PENDING;
		if (Result == XNET_RESULT_AGAIN) {
			Request->Next = Client->PendingHead;
			if (Client->PendingHead != NULL) Client->PendingHead->Previous = Request; else Client->PendingTail = Request;
			Client->PendingHead = Request; break;
		}
		xredisClientComplete(Request, Result == XNET_RESULT_CLOSED ? XREDIS_REQUEST_DISCONNECTED :
			xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
			XREDIS_NOT_SENT, NULL, Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
		Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request); xredisClientCancelTimer(Request); xredisRequestRelease(Request);
	}
}

static void xredisClientRetire(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests &&
		(Client->PendingHead != NULL || Client->SentHead != NULL); ++Work) {
		xredisrequest* Request = Client->PendingHead != NULL ? Client->PendingHead : Client->SentHead;
		xredisdelivery Delivery = Request->Phase == XREDIS_R_SENT ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT;
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; XREDIS_SUBSCRIBER_RETIRE(Request);
		xredisClientComplete(Request, xredisClientFailureStatus(Client), Delivery, NULL, Client->CloseCause);
		xredisClientCancelTimer(Request); xredisRequestRelease(Request);
	}
}

static void xredisClientWork(xnetworker* Worker, ptr Data)
{
	(void)Worker; xredisclient* Client = (xredisclient*)Data;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	Client->TlsTurnBytes = Client->TlsWriteBytes; Client->TlsTurnCalls = Client->Config.WorkRequests;
#endif
	xrtMutexLock(&Client->Gate); Client->WakeQueued = false;
	xredisrequest* Cancel = Client->CancelHead;
	xredisrequest* Last = Cancel;
	for (size_t Count = 1; Last != NULL && Count < Client->Config.WorkRequests && Last->CancelNext != NULL; ++Count)
		Last = Last->CancelNext;
	if (Last != NULL) { Client->CancelHead = Last->CancelNext; Last->CancelNext = NULL; }
	if (Client->CancelHead == NULL) Client->CancelTail = NULL;
	xrtMutexUnlock(&Client->Gate);
	Client->ReadYielded = false;
	while (Cancel != NULL) {
		xredisrequest* Next = Cancel->CancelNext;
		if (Cancel->Phase != XREDIS_R_DONE) xredisClientAbandon(Cancel, XREDIS_REQUEST_CANCELLED);
		xredisRequestRelease(Cancel); Cancel = Next;
	}
	if (!Client->Started) {
		Client->Started = true;
		if (!xredisClientClosing(Client) && Client->Managed) {
			if (xrtTimer() >= Client->InitDeadline) xredisClientInitReject(Client, XREDIS_READY_TIMEOUT);
			else if (!xredisClientInitArm(Client)) xredisClientFail(Client, XREDIS_REQUEST_MEMORY, xrtGetError());
		}
		if (!xredisClientClosing(Client)) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
			if (Client->Plan != NULL) {
				Client->PlanPending = true;
				if (!Client->Plan->Start(Client->Plan->Data, Client, Client->Engine,
					xrtNetWorkerIndex(Client->Worker), &Client->Config.Stream, &xredisClientEvents)) {
					Client->PlanPending = false;
					xredisClientFail(Client, xredisClientConnectFailure(xrtGetError()), xrtGetError());
				}
			} else
#endif
			{
			Client->Stream = xrtNetStreamConnect(Client->Engine, &Client->Remote, xrtNetWorkerIndex(Client->Worker),
				&Client->Config.Stream, &xredisClientEvents, Client);
			if (Client->Stream == NULL) xredisClientFail(Client, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
				XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, xrtGetError());
			}
		} else Client->TransportClosed = true;
	}
	xredisClientIngest(Client);
	if (!xredisClientClosing(Client)) {
		const xnetbuf* Buffer = xredisClientTransportBuffer(Client);
		if (Buffer != NULL && xrtNetBufSize(Buffer) != 0) xredisClientReadBuffer(Client, Buffer);
		if (!xredisClientClosing(Client) && !Client->ReadEnded && !Client->ReadYielded && Client->ReadPaused) {
			Client->ReadPaused = false;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
			if (Client->TLS == NULL && Client->Stream != NULL)
#endif
			xrtNetStreamResume(Client->Stream);
		}
		if (Client->ReadEnded && !Client->ReadYielded && !xredisClientClosing(Client)) {
			bool Complete = xredisReplyReaderFinish(Client->Reader);
			xredisClientFail(Client, Complete ? XREDIS_REQUEST_DISCONNECTED : XREDIS_REQUEST_PROTOCOL,
				Complete ? NULL : xrtGetError());
		}
		if (!xredisClientClosing(Client) && xredisClientTransportOpen(Client)) {
			if (Client->Managed && Client->ReadyPromise != NULL) xredisClientInitPump(Client);
			else xredisClientPump(Client);
		}
	}
	if (xredisClientClosing(Client)) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		if (Client->PlanPending && !Client->PlanCancelled) {
			Client->PlanCancelled = true; Client->Plan->Cancel(Client->Plan->Data);
		}
#endif
		if (Client->ReadyPromise != NULL)
			xredisClientReadyFinish(Client, xredisClientReadyFailure(Client->Info.CloseStatus), Client->CloseCause);
		if (!Client->TransportClosed) xredisClientTransportAbort(Client);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TlsWrite != NULL) {
			xredisrequest* Write = Client->TlsWrite; Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Write);
		}
#endif
		xredisClientRetire(Client);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        (void)xredisSubscriberCloseStep(Client);
#endif
	}
	xrtMutexLock(&Client->Gate);
	bool Again = Client->SubmitHead != NULL || Client->CancelHead != NULL ||
		(Client->Info.State >= XREDIS_CLIENT_CLOSING && (Client->PendingHead != NULL || Client->SentHead != NULL));
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xredissubscriber* Subscriber=xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE);
    if(Client->Info.State>=XREDIS_CLIENT_CLOSING && !Client->Info.Requests && Subscriber && Subscriber->Entries)Again=true;
#endif
	if (Again) (void)xredisClientWakeLocked(Client);
	bool Done = Client->TransportClosed && Client->Info.Requests == 0 && Client->Info.InitBytes == 0 &&
		Client->InitTimer == 0 && !Client->WakeQueued && !Client->CloseDone;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    Done=Done && (!Subscriber || !Subscriber->Entries);
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    Done=Done && !Client->ObservationPreparing;
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Done = Done && !Client->PlanPending;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    Done=Done && (!Subscriber || !Subscriber->ReadablePreparing);
    xpromise* ReadableSignal=NULL;
    if(Subscriber && Subscriber->ReadablePromise && (Client->Info.Pushes || Done)){
        ReadableSignal=Subscriber->ReadablePromise;Subscriber->ReadablePromise=NULL;
    }
#endif
	if (Done) { Client->CloseDone = true; Client->Info.State = XREDIS_CLIENT_CLOSED; Client->EnginePinned = false; }
	xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xredisSubscriberNotify(ReadableSignal);
#endif
	if (Done) {
		xpromise* Promise = Client->ClosePromise; Client->ClosePromise = NULL;
		xrtNetStreamDestroy(Client->Stream); Client->Stream = NULL;
		xredisReplyReaderDestroy(Client->Reader); Client->Reader = NULL;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		if (Client->PlanOwned) {
			const xredisconnectplan* Plan = Client->Plan; Client->Plan = NULL; Client->PlanOwned = false;
			Plan->Drop(Plan->Data);
		}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		xrtTlsStreamDestroy(Client->TLS); Client->TLS = NULL;
#endif
		xrtNetEngineUnpin(Client->Engine); xrtPromiseResolve(Promise, NULL); xrtPromiseDestroy(Promise);
		xredisClientRelease(Client); /* Runtime activation. */
	}
	xredisClientRelease(Client); /* This embedded post; last access. */
}

XREDIS_API bool xredisClientConfigInit(xredisclientconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config output");
	*Config = (xredisclientconfig){0}; Config->Size = sizeof(*Config);
	xrtNetStreamConfigInit(&Config->Stream); xredisReplyReaderConfigInit(&Config->Replies);
	Config->MaxRequests = 1024; Config->MaxCommandBytes = 1024u * 1024u;
	if (Config->MaxCommandBytes > Config->Stream.WriteLimit) Config->MaxCommandBytes = Config->Stream.WriteLimit;
	Config->MaxCommandBytesTotal = 8u * 1024u * 1024u; Config->MaxInFlight = 128;
	Config->MaxPushes = 128; Config->MaxPushBytes = 16u * 1024u * 1024u;
	Config->WorkRequests = 128; Config->ReadBytes = 64u * 1024u; Config->ReadFrames = 128; Config->DrainTimeout = 5000;
	return true;
}

static bool xredisClientCreate(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result)
{
	xredisclientconfig Copy;
	if (Engine == NULL || (Remote == NULL && Plan == NULL) || Result == NULL ||
		(Remote != NULL && (!xrtMemRangeValid(Remote, sizeof(*Remote)) || xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)))) ||
		!xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client creation arguments");
	if (Config == NULL) xredisClientConfigInit(&Copy); else Copy = *Config;
	xredisclientinitconfig InitCopy = {0};
	if (Init != NULL) {
		if (!xrtMemRangeValid(Init, sizeof(*Init)) || xrtMemRangesOverlap(Init, sizeof(*Init), Result, sizeof(*Result)))
			return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization descriptor or output overlap");
		InitCopy = *Init;
		if (!xredisClientInitValid(&InitCopy, &Copy)) return false;
		const xbytesview Views[] = {InitCopy.Username, InitCopy.Password, InitCopy.Name};
		for (size_t Index = 0; Index < sizeof(Views) / sizeof(Views[0]); ++Index)
			if (xrtMemRangesOverlap(Views[Index].Data, Views[Index].Size, Result, sizeof(*Result)))
				return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "initialization output overlaps input payload");
	}
	if (Copy.Size != sizeof(Copy) || Copy.MaxRequests == 0 || Copy.MaxRequests > 1000000u ||
		Copy.MaxInFlight == 0 || Copy.MaxInFlight > Copy.MaxRequests || Copy.MaxCommandBytes == 0 ||
		Copy.MaxCommandBytes > Copy.MaxCommandBytesTotal || Copy.MaxCommandBytes > Copy.Stream.WriteLimit ||
		Copy.MaxCommandBytes > SIZE_MAX - sizeof(xredisrequest) || Copy.WorkRequests == 0 ||
		Copy.ReadBytes == 0 || Copy.ReadFrames == 0 || Copy.DrainTimeout <= 0 ||
		Copy.MaxPushes > (SIZE_MAX - sizeof(xredisclient)) / sizeof(xredisreply*) ||
		(Remote != NULL && ((Remote->Family != XNET_FAMILY_IPV4 && Remote->Family != XNET_FAMILY_IPV6) || Remote->Port == 0)) ||
		!xrtNetStreamConfigValid(&Copy.Stream))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config or endpoint");
	if (!xrtNetEnginePin(Engine)) return false;
	size_t Bytes = sizeof(xredisclient) + Copy.MaxPushes * sizeof(xredisreply*);
	xredisclient* Client = (xredisclient*)xrtCalloc(1, Bytes);
	if (Client == NULL) { xrtNetEngineUnpin(Engine); return false; }
	Client->Engine = Engine; Client->EnginePinned = true; Client->Config = Copy;
	if (Remote != NULL) Client->Remote = *Remote;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Client->Plan = Plan;
#endif
	Client->StorageBytes = Bytes; Client->Info.Size = sizeof(Client->Info); Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED;
	Client->Managed = Init != NULL; Client->InitAwait = XREDIS_I_END;
	if (Client->Managed) {
		Client->InitMode = InitCopy.Mode; Client->InitFlags = InitCopy.Flags;
		Client->InitDeadline = xredisClientDeadline(Entered, InitCopy.Timeout);
	}
	Client->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
	xrtAtomic32Init(&Client->References, 1);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    xrtAtomicPtrInit(&Client->Subscriber,NULL);
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xrtAtomicPtrInit(&Client->Observation,NULL);
#endif
	if (!xrtMutexInit(&Client->Gate)) { xrtNetEngineUnpin(Engine); xrtFree(Client); return false; }
	if (!xrtNetPostInit(&Client->Wake) || !xredisReplyReaderCreate(&Copy.Replies, &Client->Reader) ||
		(Client->ClosePromise = xrtPromiseCreate(&Client->CloseFuture, NULL)) == NULL ||
		(Client->ReadyPromise = xrtPromiseCreate(&Client->ReadyFuture, NULL)) == NULL ||
		(Client->ReadyResult = (xredisreadyresult*)xrtMalloc(sizeof(*Client->ReadyResult))) == NULL ||
		(Client->Managed && !xredisClientInitBuild(Client, &InitCopy))) { xredisClientRelease(Client); return false; }
	xredisClientRef(Client); /* Active connection, even before Stream is created. */
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Client->PlanOwned = Plan != NULL;
#endif
	xrtMutexLock(&Client->Gate); bool Posted = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
	if (!Posted) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		Client->PlanOwned = false;
#endif
		xredisinitwire* Wire = Client->InitWire; Client->InitWire = NULL; xredisInitWireRelease(Wire);
		xredisClientRelease(Client); xredisClientRelease(Client); return false;
	}
	*Result = Client; return true;
}

XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result)
{ return xredisClientCreate(Engine, Remote, Affinity, Config, NULL, xrtTimer(), NULL, Result); }

XREDIS_API bool xredisClientConnectInit(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init, xredisclient** Result)
{
	double Entered = xrtTimer(); xredisclientinitconfig Defaults;
	if (Init == NULL) { xredisClientInitConfigInit(&Defaults); Init = &Defaults; }
	return xredisClientCreate(Engine, Remote, Affinity, Config, Init, Entered, NULL, Result);
}

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
bool __xredisClientCreatePlanned(xnetengine* Engine, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result)
{ return xredisClientCreate(Engine, NULL, Affinity, Config, Init, Entered, Plan, Result); }
#endif

XREDIS_API xfuture* xredisClientReady(xredisclient* Client)
{
	if (Client == NULL) { xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return NULL; }
	return xrtFutureRef(Client->ReadyFuture);
}

static bool xredisClientWord(xbytesview Word, cstr Text)
{
	size_t Size = strlen(Text); if (Word.Size != Size) return false;
	for (size_t Index = 0; Index < Size; ++Index) {
		unsigned char Byte = Word.Data[Index]; if (Byte >= 'a' && Byte <= 'z') Byte -= 'a' - 'A';
		if (Byte != (unsigned char)Text[Index]) return false;
	}
	return true;
}

static bool xredisClientCommandMode(const xbytesview* Args, size_t Count)
{
	const cstr Modes[] = {"SUBSCRIBE", "PSUBSCRIBE", "SSUBSCRIBE", "UNSUBSCRIBE", "PUNSUBSCRIBE",
		"SUNSUBSCRIBE", "MONITOR", "SYNC", "PSYNC"};
	for (size_t Index = 0; Index < sizeof(Modes) / sizeof(Modes[0]); ++Index)
		if (xredisClientWord(Args[0], Modes[Index])) return false;
	return !(Count > 1 && xredisClientWord(Args[0], "CLIENT") && xredisClientWord(Args[1], "REPLY"));
}

static bool xredisClientManagedControl(const xbytesview* Arguments, size_t Count)
{
	return xredisClientWord(Arguments[0], "HELLO") || xredisClientWord(Arguments[0], "AUTH") ||
		xredisClientWord(Arguments[0], "SELECT") || xredisClientWord(Arguments[0], "RESET") ||
		xredisClientWord(Arguments[0], "MULTI") || xredisClientWord(Arguments[0], "EXEC") ||
		xredisClientWord(Arguments[0], "DISCARD") || xredisClientWord(Arguments[0], "WATCH") ||
		xredisClientWord(Arguments[0], "UNWATCH") ||
		(Count > 1 && xredisClientWord(Arguments[0], "CLIENT") && xredisClientWord(Arguments[1], "SETNAME"));
}

/* Gate held. Ordinary calls cannot enter a claimed connection. Owned calls
 * must recheck at publication, since a concurrent commit/abort may seal it
 * while the caller is allocating/encoding outside Gate. */
static xredisadmission xredisClientOwnerAdmission(xredisclient* Client, xredisexclusiveowner* Owner, bool Control)
{
    if (Client->Info.State >= XREDIS_CLIENT_CLOSING) return XREDIS_ADMISSION_CLOSED;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE))return XREDIS_ADMISSION_OVERLOADED;
#endif
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Owner != Owner) return Owner == NULL ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_CLOSED;
    if (Owner != NULL) {
#if defined(XREDIS_FEATURE_XREDIS_POOL)
        if (Owner->Quota != NULL && xrtAtomic32Load(&Owner->Quota->Closing, XMEMORY_ACQUIRE)) return XREDIS_ADMISSION_CLOSED;
#endif
        uint32 State = xrtAtomic32Load(&Owner->State, XMEMORY_ACQUIRE);
        if ((!Control && State != XREDIS_OWNER_ACTIVE) || State == XREDIS_OWNER_CLOSED) return XREDIS_ADMISSION_CLOSED;
    }
#else
    (void)Owner; (void)Control;
#endif
    return XREDIS_ADMISSION_ACCEPTED;
}

#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static bool xredisClientOwnerSwap(xredisexclusiveowner* Owner, uint32 From, uint32 To)
{ return xrtAtomic32CompareExchange(&Owner->State, &From, To, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE); }
#endif

xredisadmission __xredisClientClaim(xredisclient* Client, xredisexclusiveowner* Owner,
    xnetengine** Engine, xnetworker** Worker, ptr Output)
{
    if (Client == NULL || Owner == NULL || Engine == NULL || Worker == NULL ||
        xrtMemRangesOverlap(Client, Client ? Client->StorageBytes : 0, Output, sizeof(xfuture*))) {
        xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid exclusive owner"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
        !Client->Managed ? XREDIS_ADMISSION_ERROR :
        Client->Info.State != XREDIS_CLIENT_OPEN || !Client->Info.Ready || Client->Info.Requests || Client->Owner
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        || xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)
#endif
        ?
        XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xrtNetEnginePin(Client->Engine)) Admission = XREDIS_ADMISSION_ERROR;
        else { Client->Owner = Owner; xredisClientRef(Client); *Engine = Client->Engine; *Worker = Client->Worker; }
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) ++Client->Info.Rejected;
    xrtMutexUnlock(&Client->Gate);
    if (Admission == XREDIS_ADMISSION_ERROR) xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "exclusive sessions require a live managed connection");
    return Admission;
}

#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisClientClaimStarting(xredisclient* Client, xredisexclusiveowner* Owner, xnetengine* Engine)
{
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
        !Client->Managed || Client->Engine != Engine ? XREDIS_ADMISSION_ERROR :
        Client->Info.Requests || Client->Owner
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        || xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)
#endif
        ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xrtNetEnginePin(Engine)) Admission = XREDIS_ADMISSION_ERROR;
        else { Client->Owner = Owner; xredisClientRef(Client); }
    }
    xrtMutexUnlock(&Client->Gate); return Admission;
}
#endif
static bool xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner, bool Unused)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xredisexclusiveowner* Parent = NULL;
#else
    (void)Unused;
#endif
    xrtMutexLock(&Client->Gate);
    bool Idle = Client->Owner == Owner && Client->Info.Requests == 0;
    if (Idle) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
        Parent = Owner->Parent;
        if (Parent != NULL) {
            if (Unused) {
                Parent->Retain(Parent->Data);
                xrtAtomic32Store(&Parent->Nested, 0, XMEMORY_RELEASE);
                if (!xrtAtomic32Load(&Parent->Quota->Closing, XMEMORY_ACQUIRE))
                    (void)xredisClientOwnerSwap(Parent, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE);
            }
        }
        Client->Owner = Parent;
#else
        Client->Owner = NULL;
#endif
    }
    xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    if (Parent != NULL && Unused) { Parent->Wake(Parent->Data); Parent->Release(Parent->Data); }
#endif
    return Idle;
}
bool __xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner)
{ return xredisClientUnclaim(Client, Owner, false); }

#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
bool __xredisClientUnclaimUnused(xredisclient* Client, xredisexclusiveowner* Owner)
{ return xredisClientUnclaim(Client, Owner, true); }

xredisadmission __xredisClientClaimNested(xredisclient* Client, xredisexclusiveowner* Parent,
    xredisexclusiveowner* Owner, xnetengine** Engine, xnetworker** Worker, ptr Output)
{
    if (xrtMemRangesOverlap(Client, Client->StorageBytes, Output, sizeof(xfuture*))) {
        xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "nested output overlaps client"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING || Client->Owner != Parent ||
        xrtAtomic32Load(&Parent->Quota->Closing, XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&Parent->State, XMEMORY_ACQUIRE) != XREDIS_OWNER_ACTIVE ? XREDIS_ADMISSION_CLOSED :
        Client->Info.Requests ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xredisClientOwnerSwap(Parent, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED)) Admission = XREDIS_ADMISSION_CLOSED;
        else if (!xrtNetEnginePin(Client->Engine)) {
            (void)xredisClientOwnerSwap(Parent, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE); Admission = XREDIS_ADMISSION_ERROR;
        } else {
            Owner->Parent = Parent; Owner->Quota = Parent->Quota;
            xrtAtomic32Store(&Parent->Nested, 1, XMEMORY_RELEASE);
            if (Owner->Deadline > Parent->Deadline) Owner->Deadline = Parent->Deadline;
            Client->Owner = Owner; xredisClientRef(Client); *Engine = Client->Engine; *Worker = Client->Worker;
        }
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) ++Client->Info.Rejected;
    xrtMutexUnlock(&Client->Gate); return Admission;
}

/* Caller owns the parent/client lifetime. Retain the child under the native
 * Gate, then request abandonment outside it. A temporary zero-request WATCH
 * interval is not permission to return the parent slot. */
bool __xredisClientOwnerReady(xredisclient* Client, xredisexclusiveowner* Parent, bool Abandon)
{
    xredisexclusiveowner* Child = NULL;
    xrtMutexLock(&Client->Gate); bool Ready = Client->Owner == Parent && !xrtAtomic32Load(&Parent->Nested, XMEMORY_ACQUIRE);
    if (!Ready && Abandon && Client->Owner != NULL && Client->Owner->Parent == Parent) {
        Child = Client->Owner; Child->Retain(Child->Data);
    }
    xrtMutexUnlock(&Client->Gate);
    if (Child != NULL) { Child->Abandon(Child->Data); Child->Release(Child->Data); }
    return Ready;
}
void __xredisClientChildDone(xredisexclusiveowner* Owner)
{
    xredisexclusiveowner* Parent = Owner->Parent;
    if (Parent == NULL) return;
    Parent->Retain(Parent->Data);
    xrtAtomic32Store(&Parent->Nested, 0, XMEMORY_RELEASE);
    Parent->Wake(Parent->Data); Parent->Release(Parent->Data);
}
#endif

bool __xredisClientIsWorker(xredisclient* Client)
{
    xrtMutexLock(&Client->Gate);
    bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL;
    xrtMutexUnlock(&Client->Gate); return Worker;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static xredisadmission xredisClientTransactionAdmit(xredisclient* Client,
	const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
	int64 Timeout, double Entered, xfuture** Future, xredisexclusiveowner* Owner)
{
	xredistransactionconfig Options;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Future, sizeof(*Future))))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid transaction arguments/output"); return XREDIS_ADMISSION_ERROR;
	}
	if (Config == NULL) xredisClientTransactionConfigInit(&Options); else Options = *Config;
	if (Options.Size != sizeof(Options) || Options.MaxCommands == 0 || Options.MaxCommands > XREDIS_TRANSACTION_COMMANDS_MAX ||
		Count > Options.MaxCommands || (Count != 0 && Commands == NULL) || !xrtMemRangeValid(Commands, Count * sizeof(*Commands)) ||
		xrtMemRangesOverlap(Commands, Count * sizeof(*Commands), Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid or excessive transaction descriptors"); return XREDIS_ADMISSION_ERROR;
	}
	if (!Client->Managed) {
		xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "transactions require a managed connection"); return XREDIS_ADMISSION_ERROR;
	}
	size_t CommandsBytes = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		const xredistransactioncommand* Command = &Commands[Index]; size_t Bytes = 0;
		if (!xredisCommandSize(Command->Arguments, Command->Count, &Bytes)) return XREDIS_ADMISSION_ERROR;
		if (xrtMemRangesOverlap(Command->Arguments, Command->Count * sizeof(*Command->Arguments), Future, sizeof(*Future))) {
			xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "transaction output overlaps arguments"); return XREDIS_ADMISSION_ERROR;
		}
		for (size_t Argument = 0; Argument < Command->Count; ++Argument)
			if (xrtMemRangesOverlap(Command->Arguments[Argument].Data, Command->Arguments[Argument].Size, Future, sizeof(*Future))) {
				xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "transaction output overlaps payload"); return XREDIS_ADMISSION_ERROR;
			}
		if (!xredisClientCommandMode(Command->Arguments, Command->Count) || xredisClientManagedControl(Command->Arguments, Command->Count) ||
			xredisClientWord(Command->Arguments[0], "QUIT") || xredisClientWord(Command->Arguments[0], "READONLY") ||
			xredisClientWord(Command->Arguments[0], "READWRITE") || xredisClientWord(Command->Arguments[0], "ASKING") ||
			(Command->Count > 1 && xredisClientWord(Command->Arguments[0], "CLIENT") &&
			 (xredisClientWord(Command->Arguments[1], "TRACKING") || xredisClientWord(Command->Arguments[1], "CACHING")))) {
			xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "transaction payload changes connection/control mode"); return XREDIS_ADMISSION_ERROR;
		}
		if (Bytes > SIZE_MAX - CommandsBytes) {
			xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "transaction wire size overflow"); return XREDIS_ADMISSION_ERROR;
		}
		CommandsBytes += Bytes;
	}
	size_t ControlBytes = sizeof(xredisTransactionMulti) + sizeof(xredisTransactionExec) + sizeof(xredisTransactionDiscard) - 3u;
	if (CommandsBytes > SIZE_MAX - ControlBytes - sizeof(xredisrequest) - sizeof(xredistransactionstate) - _Alignof(xredistransactionstate)) {
		xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "transaction allocation size overflow"); return XREDIS_ADMISSION_ERROR;
	}
	size_t Bytes = CommandsBytes + ControlBytes;
	size_t Padding = (_Alignof(xredistransactionstate) - (offsetof(xredisrequest, Wire) + Bytes) % _Alignof(xredistransactionstate)) % _Alignof(xredistransactionstate);
	size_t MetadataBytes = Padding + sizeof(xredistransactionstate), Charged = Bytes + MetadataBytes;
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission = xredisClientOwnerAdmission(Client, Owner, false);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) Admission = Client->Info.Requests >= Client->Config.MaxRequests || Charged > Client->Config.MaxCommandBytes ||
		Charged > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Admission == XREDIS_ADMISSION_ACCEPTED && !xredisClientQuotaReserve(Owner, Charged)) Admission = XREDIS_ADMISSION_OVERLOADED;
#endif
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Charged; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Charged);
	if (Request == NULL) {
		xredisClientReservationDrop(Client, Charged); return XREDIS_ADMISSION_ERROR;
	}
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && Request->Deadline > Owner->Deadline) Request->Deadline = Owner->Deadline;
#endif
	Request->Transaction = true;
	xredistransactionstate* State = xredisTransactionState(Request);
	*State = (xredistransactionstate){0}; State->Count = Count;
	State->CommandsBytes = CommandsBytes; State->MetadataBytes = MetadataBytes;
	Request->Result = (xredisrequestresult*)xrtMalloc(sizeof(xredistransactionresult));
	xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token);
	}
	bool Encoded = Request->Watch != NULL; size_t Offset = sizeof(xredisTransactionMulti) - 1u;
	if (Encoded) memcpy(Request->Wire, xredisTransactionMulti, Offset);
	for (size_t Index = 0; Encoded && Index < Count; ++Index) {
		size_t Written = 0;
		Encoded = xredisCommandWrite(Request->Wire + Offset, CommandsBytes - (Offset - (sizeof(xredisTransactionMulti) - 1u)),
			Commands[Index].Arguments, Commands[Index].Count, &Written);
		Offset += Written;
	}
	if (Encoded) {
		memcpy(Request->Wire + Offset, xredisTransactionExec, sizeof(xredisTransactionExec) - 1u); Offset += sizeof(xredisTransactionExec) - 1u;
		memcpy(Request->Wire + Offset, xredisTransactionDiscard, sizeof(xredisTransactionDiscard) - 1u);
		xrtMutexLock(&Client->Gate);
        Admission = xredisClientOwnerAdmission(Client, Owner, false);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
        if (Admission == XREDIS_ADMISSION_ACCEPTED && Owner != NULL &&
            !xredisClientOwnerSwap(Owner, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED)) Admission = XREDIS_ADMISSION_CLOSED;
#endif
		if (Admission == XREDIS_ADMISSION_ACCEPTED) {
			if (Client->SubmitTail != NULL) Client->SubmitTail->SubmitNext = Request; else Client->SubmitHead = Request;
			Client->SubmitTail = Request;
			if (xredisClientWakeLocked(Client)) {
				++Client->Info.Accepted; *Future = Observer; xrtMutexUnlock(&Client->Gate); return XREDIS_ADMISSION_ACCEPTED;
			}
			xredisrequest* Previous = NULL;
			for (xredisrequest* Item = Client->SubmitHead; Item != Request; Item = Item->SubmitNext) Previous = Item;
			if (Previous != NULL) Previous->SubmitNext = NULL; else Client->SubmitHead = NULL;
			Client->SubmitTail = Previous;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
            if (Owner != NULL) (void)xredisClientOwnerSwap(Owner, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE);
#endif
		} else ++Client->Info.Rejected;
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_ACCEPTED ? XREDIS_ADMISSION_ERROR : Admission;
}

xredisadmission __xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future)
{ return xredisClientTransactionAdmit(Client, Commands, Count, Config, Timeout, Entered, Future, NULL); }
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedTransaction(xredisclient* Client, xredisexclusiveowner* Owner,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future)
{ return xredisClientTransactionAdmit(Client, Commands, Count, Config, Timeout, Entered, Future, Owner); }
#endif

xredisadmission __xredisClientTransaction(xredisclient* Client,
	const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
	int64 Timeout, double Entered, xfuture** Future)
{
	if (Client == NULL) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return XREDIS_ADMISSION_ERROR;
	}
	xrtMutexLock(&Client->Gate); bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL; xrtMutexUnlock(&Client->Gate);
	if (Worker) { xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "sync transaction cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
	xredisadmission Admission = __xredisClientTransactionAsync(Client, Commands, Count, Config, Timeout, Entered, Future);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
	return Admission;
}
#endif

static xredisadmission xredisClientCallAdmit(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future,
    double Entered, ptr Scope, bool Control XREDIS_OBSERVE_PARAMETER)
{
	size_t Bytes = 0;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client call arguments"); return XREDIS_ADMISSION_ERROR;
	}
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && Observe<5 && !xredisTrackingOutput(Client,Future,sizeof(*Future))){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"tracking output overlaps owned storage");return XREDIS_ADMISSION_ERROR;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    bool Subscription=Observe>=5;
    bool Measured=Subscription?xredisPubSubCommandSize((xredispubsubcommand)(Observe-5),Arguments,Count,&Bytes):xredisCommandSize(Arguments,Count,&Bytes);
#else
    bool Measured=xredisCommandSize(Arguments,Count,&Bytes);
#endif
    if(!Measured)return XREDIS_ADMISSION_ERROR;
    xredisexclusiveowner* Owner=Scope;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Subscription)Owner=NULL;
#endif
	if (xrtMemRangesOverlap(Arguments, Count * sizeof(*Arguments), Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps arguments"); return XREDIS_ADMISSION_ERROR;
	}
	for (size_t Index = 0; Index < Count; ++Index) if (xrtMemRangesOverlap(Arguments[Index].Data, Arguments[Index].Size, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps argument payload"); return XREDIS_ADMISSION_ERROR;
	}
	if (
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        !Subscription &&
#endif
        !xredisClientCommandMode(Arguments, Count)) {
		xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "command changes response cardinality/mode"); return XREDIS_ADMISSION_ERROR;
	}
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && !Control && (xredisClientWord(Arguments[0], "QUIT") ||
        xredisClientWord(Arguments[0], "READONLY") || xredisClientWord(Arguments[0], "READWRITE") ||
        xredisClientWord(Arguments[0], "ASKING") ||
        (Count > 1 && xredisClientWord(Arguments[0], "CLIENT") &&
         (xredisClientWord(Arguments[1], "TRACKING") || xredisClientWord(Arguments[1], "CACHING"))))) {
        xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "exclusive payload changes connection/control mode"); return XREDIS_ADMISSION_ERROR;
    }
#endif
	if (!Control && Client->Managed && xredisClientManagedControl(Arguments, Count)) {
		xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "managed initialization/transaction controls require their dedicated API");
		return XREDIS_ADMISSION_ERROR;
	}
	size_t Charged=Bytes;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    size_t SubscriberMetadata=0;
    if(Subscription){size_t A=_Alignof(xredissubcall),Padding=(A-(offsetof(xredisrequest,Wire)+Bytes)%A)%A;
        SubscriberMetadata=Padding+sizeof(xredissubcall)+(Observe<=7?Count*sizeof(xredissubentry*):0);
        if(Bytes>SIZE_MAX-sizeof(xredisrequest)-SubscriberMetadata){xredisClientReport(XERR_RANGE,XREDIS_CLIENT_ARGUMENT,"subscriber request size overflow");return XREDIS_ADMISSION_ERROR;}
        Charged+=SubscriberMetadata;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    size_t PrefixBytes=0,MetadataBytes=0,QueryBytes=Bytes;
    if((Observe==3 || Observe==4)){
        if(xredisClientWord(Arguments[0],"CLIENT") || xredisClientWord(Arguments[0],"QUIT") ||
           xredisClientWord(Arguments[0],"READONLY") || xredisClientWord(Arguments[0],"READWRITE") ||
           xredisClientWord(Arguments[0],"ASKING")){
            xredisClientReport(XERR_UNSUPPORTED,XREDIS_CLIENT_COMMAND_MODE,"caching query changes connection/control mode");return XREDIS_ADMISSION_ERROR;
        }
        PrefixBytes=Observe==3?sizeof(xredisCachingYes)-1:sizeof(xredisCachingNo)-1;
        if(Bytes>SIZE_MAX-sizeof(xredisrequest)-sizeof(xrediscachingstate)-_Alignof(xrediscachingstate)-PrefixBytes){
            xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"caching group size overflow");return XREDIS_ADMISSION_ERROR;
        }
        Bytes+=PrefixBytes;
        size_t A=_Alignof(xrediscachingstate),Padding=(A-(offsetof(xredisrequest,Wire)+Bytes)%A)%A;
        MetadataBytes=Padding+sizeof(xrediscachingstate);Charged=Bytes+MetadataBytes;
    }
#endif
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission =
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        Subscription ? (Client->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:
            xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)?
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
            (((xredissubscriber*)xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_RELAXED))->ReplayOwner==Scope?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_OVERLOADED):
#else
            XREDIS_ADMISSION_ACCEPTED:
#endif
            XREDIS_ADMISSION_ERROR) :
#endif
        xredisClientOwnerAdmission(Client, Owner, Control);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Subscription && Admission==XREDIS_ADMISSION_ERROR){xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode has not been adopted");return XREDIS_ADMISSION_ERROR;}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && Observe<5 && Admission==XREDIS_ADMISSION_ACCEPTED && !xrtAtomicPtrLoad(&Client->Observation,XMEMORY_ACQUIRE)){
        xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"tracking observation is not enabled");return XREDIS_ADMISSION_ERROR;
    }
    if((Observe==3 || Observe==4) && Admission==XREDIS_ADMISSION_ACCEPTED && !Client->Managed){
        xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"caching groups require a managed client");return XREDIS_ADMISSION_ERROR;
    }
#endif
	if (Admission == XREDIS_ADMISSION_ACCEPTED) Admission = Client->Info.Requests >= Client->Config.MaxRequests || Charged > Client->Config.MaxCommandBytes ||
		Charged > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Admission == XREDIS_ADMISSION_ACCEPTED && !xredisClientQuotaReserve(Owner, Charged)) Admission = XREDIS_ADMISSION_OVERLOADED;
#endif
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Charged; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Charged);
	if (Request == NULL) {
		xredisClientReservationDrop(Client, Charged); return XREDIS_ADMISSION_ERROR;
	}
	/* Initialize metadata only: opaque wire bytes are written by the encoder,
	 * without a redundant full-payload zero pass. */
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && (!Control || xredisClientWord(Arguments[0], "WATCH")) && Request->Deadline > Owner->Deadline) Request->Deadline = Owner->Deadline;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    Request->Sensitive=!Subscription && (xredisClientWord(Arguments[0],"AUTH") || xredisClientWord(Arguments[0],"HELLO"));
#else
    Request->Sensitive=xredisClientWord(Arguments[0],"AUTH") || xredisClientWord(Arguments[0],"HELLO");
#endif
    size_t ResultBytes=sizeof(*Request->Result);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) || defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    Request->Observe=(uint8)Observe;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe==3 || Observe==4){xrediscachingstate* S=xredisCachingState(Request);memset(S,0,sizeof(*S));S->MetadataBytes=MetadataBytes;ResultBytes=sizeof(xrediscachingresult);}
    else if(Observe==1 || Observe==2)ResultBytes=sizeof(xredistrackingresult);
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(Subscription){
        xredissubcall* S=xredisSubscriberState(Request);memset(S,0,SubscriberMetadata-(size_t)((unsigned char*)S-(Request->Wire+Bytes)));
        S->MetadataBytes=SubscriberMetadata;S->Count=Count;S->Command=(xredispubsubcommand)(Observe-5);ResultBytes=sizeof(xredissubscriberresult);
    }
#endif
    Request->Result=xrtMalloc(ResultBytes);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && Observe<5 && Request->Result)memset(Request->Result,0,ResultBytes);
#endif
    xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token); /* FutureCancelToken returns a retained reference. */
	}
    bool Encoded=false;
    if(Request->Watch!=NULL){
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        if(PrefixBytes){
            memcpy(Request->Wire,Observe==3?xredisCachingYes:xredisCachingNo,PrefixBytes);
            Encoded=xredisCommandWrite(Request->Wire+PrefixBytes,QueryBytes,Arguments,Count,NULL);
        }else
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        if(Subscription)Encoded=xredisPubSubCommandWrite(Request->Wire,Bytes,(xredispubsubcommand)(Observe-5),Arguments,Count,NULL);else
#endif
        Encoded=xredisCommandWrite(Request->Wire,Bytes,Arguments,Count,NULL);
    }
	if (Encoded) {
		xrtMutexLock(&Client->Gate);
        Admission =
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
        Subscription ? (Client->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:
            xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_ACQUIRE)?
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
            (((xredissubscriber*)xrtAtomicPtrLoad(&Client->Subscriber,XMEMORY_RELAXED))->ReplayOwner==Scope?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_OVERLOADED):
#else
            XREDIS_ADMISSION_ACCEPTED:
#endif
            XREDIS_ADMISSION_ERROR) :
#endif
        xredisClientOwnerAdmission(Client, Owner, Control);
		if (Admission == XREDIS_ADMISSION_ACCEPTED) {
			Request->SubmitNext = NULL;
			if (Client->SubmitTail != NULL) Client->SubmitTail->SubmitNext = Request; else Client->SubmitHead = Request;
			Client->SubmitTail = Request;
			/* A live pinned worker accepts this embedded post without allocation. */
			if (xredisClientWakeLocked(Client)) {
				++Client->Info.Accepted; *Future = Observer; xrtMutexUnlock(&Client->Gate); return XREDIS_ADMISSION_ACCEPTED;
			}
			/* The failed wake accepted no callback. Remove this exact tail. */
			xredisrequest* Previous = NULL;
			for (xredisrequest* Item = Client->SubmitHead; Item != Request; Item = Item->SubmitNext) Previous = Item;
			if (Previous != NULL) Previous->SubmitNext = NULL; else Client->SubmitHead = NULL;
			Client->SubmitTail = Previous;
		} else ++Client->Info.Rejected;
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_ACCEPTED ? XREDIS_ADMISSION_ERROR : Admission;
}

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
/* Registry mutation belongs to the IO worker. Info is a Gate-protected snapshot;
 * the installed mode/bucket allocation lives through the last Client reference. */
static void xredisSubscriberPublish(xredisclient* C,xredissubscriber* M)
{
    xrtMutexLock(&C->Gate);M->Info.Entries=M->Entries;M->Info.NameBytes=M->NameBytes;
    M->Info.RegistryBytes=M->StorageBytes+M->EntryBytes;
    M->Info.Subscriptions=M->Counts[0]+M->Counts[1];M->Info.ShardSubscriptions=M->Counts[2];xrtMutexUnlock(&C->Gate);
}
static xredissubentry* xredisSubscriberFind(xredissubscriber* M,unsigned Family,xbytesview Name,uint64 Hash)
{
    for(xredissubentry* E=M->Table[Hash&(M->Buckets-1)];E;E=E->BucketNext)
        if(E->Hash==Hash && E->Family==Family && E->Bytes==Name.Size && (!Name.Size || !memcmp(E->Name,Name.Data,Name.Size)))return E;
    return NULL;
}
static void xredisSubscriberDelete(xredissubscriber* M,xredissubentry* E,bool Closing)
{
    if(!Closing){xredissubentry** Link=&M->Table[E->Hash&(M->Buckets-1)];while(*Link!=E)Link=&(*Link)->BucketNext;*Link=E->BucketNext;}
    if(E->Previous)E->Previous->Next=E->Next;else M->Head=E->Next;
    if(E->Next)E->Next->Previous=E->Previous;
    --M->Entries;M->NameBytes-=E->Bytes;M->EntryBytes-=sizeof(*E)+E->Bytes;xrtFree(E);
}
static bool xredisSubscriberName(xredisrequest* R,size_t* Cursor,xbytesview* Name)
{
    /* Only traverses bytes produced by our own bounded RESP encoder. */
    size_t N=*Cursor,Bytes=0;if(N>=R->WireBytes || R->Wire[N++]!='$')return false;
    while(N<R->WireBytes && R->Wire[N]>='0' && R->Wire[N]<='9')Bytes=Bytes*10+(R->Wire[N++]-'0');
    if(N+2>R->WireBytes || R->Wire[N]!='\r' || R->Wire[N+1]!='\n')return false;
    N+=2;
    if(Bytes>R->WireBytes-N || R->WireBytes-N-Bytes<2)return false;
    *Name=(xbytesview){R->Wire+N,Bytes};*Cursor=N+Bytes+2;return true;
}
static size_t xredisSubscriberFirstName(xredisrequest* R)
{
    size_t N=0;while(N<R->WireBytes && R->Wire[N]!='\n')++N;++N;
    xbytesview Word;if(!xredisSubscriberName(R,&N,&Word))return R->WireBytes;return N;
}
static void xredisSubscriberRetire(xredisrequest* R)
{
    xredissubcall* S=xredisSubscriberState(R);xredissubscriber* M=xrtAtomicPtrLoad(&R->Client->Subscriber,XMEMORY_ACQUIRE);
    for(size_t N=0;N<S->Reserved;++N){xredissubentry* E=S->Entries[N];--E->Reservations;if(!E->Reservations && !E->Confirmed)xredisSubscriberDelete(M,E,false);}
    S->Reserved=0;if(M)xredisSubscriberPublish(R->Client,M);
}
static bool xredisSubscriberCloseStep(xredisclient* C)
{
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);if(!M)return true;
    xrtMutexLock(&C->Gate);bool Idle=!C->Info.Requests;xrtMutexUnlock(&C->Gate);if(!Idle)return false;
    for(size_t N=0;N<C->Config.WorkRequests && M->Head;++N){xredissubentry* E=M->Head;if(E->Confirmed)--M->Counts[E->Family];xredisSubscriberDelete(M,E,true);}
    xredisSubscriberPublish(C,M);return !M->Head;
}
static void xredisSubscriberRelease(xredisclient* C)
{
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_RELAXED);if(!M)return;
    while(M->Head)xredisSubscriberDelete(M,M->Head,true);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xrtPromiseDestroy(M->ReadablePromise);xrtFutureDestroy(M->ReadableFuture);
#endif
    xrtFree(M);
}
static bool xredisSubscriberPrepare(xredisrequest* R)
{
    if(R->Observe<5)return true;
    xredissubcall* S=xredisSubscriberState(R);if(S->Prepared)return true;
    xredisclient* C=R->Client;xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    size_t Cursor=xredisSubscriberFirstName(R);S->Cursor=Cursor;S->Expected=S->Count?S->Count:1;
    xredisrequeststatus Status=XREDIS_REQUEST_OK;
    if(S->Command<=XREDIS_PUBSUB_CMD_SSUBSCRIBE)for(size_t N=0;N<S->Count;++N){
        xbytesview Name;if(!xredisSubscriberName(R,&Cursor,&Name)){Status=XREDIS_REQUEST_PROTOCOL;break;}
        uint64 Hash=xrtHash64(Name.Data,Name.Size);xredissubentry* E=xredisSubscriberFind(M,(unsigned)S->Command,Name,Hash);
        if(!E){
            if(M->Entries==M->Config.MaxSubscriptions || Name.Size>M->Config.MaxNameBytes-M->NameBytes){Status=XREDIS_REQUEST_LIMIT;break;}
            E=xrtMalloc(sizeof(*E)+Name.Size);if(!E){Status=XREDIS_REQUEST_MEMORY;break;}
            *E=(xredissubentry){0};E->Hash=Hash;E->Bytes=Name.Size;E->Family=(unsigned)S->Command;
            if(Name.Size)memcpy(E->Name,Name.Data,Name.Size);
            size_t Bucket=Hash&(M->Buckets-1);E->BucketNext=M->Table[Bucket];M->Table[Bucket]=E;
            E->Next=M->Head;if(M->Head)M->Head->Previous=E;M->Head=E;
            ++M->Entries;M->NameBytes+=Name.Size;M->EntryBytes+=sizeof(*E)+Name.Size;
        }
        ++E->Reservations;S->Entries[S->Reserved++]=E;
    }
    xredisSubscriberPublish(C,M);
    if(Status!=XREDIS_REQUEST_OK){
        xredisClientComplete(R,Status,XREDIS_NOT_SENT,NULL,Status==XREDIS_REQUEST_MEMORY?xrtGetError():NULL);
        xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;xredisSubscriberRetire(R);xredisClientCancelTimer(R);xredisRequestRelease(R);return false;
    }
    S->Prepared=true;
    if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE) || xrtTimer()>=R->Deadline){
        xredisClientAbandon(R,xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE)?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT);return false;
    }
    return true;
}
static void xredisSubscriberFinish(xredisrequest* R)
{
    xredissubcall* S=xredisSubscriberState(R);if(!S->Finished || !S->Written)return;
    xredisclient* C=R->Client;xredisreply* Reply=S->Final;S->Final=NULL;
    if(R->Promise){
        xredisrequeststatus Status=S->Status;
        if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE))Status=XREDIS_REQUEST_CANCELLED;
        else if(xrtTimer()>=R->Deadline)Status=XREDIS_REQUEST_TIMEOUT;
        if(Status==XREDIS_REQUEST_CANCELLED || Status==XREDIS_REQUEST_TIMEOUT){xredisReplyRelease(Reply);Reply=NULL;}
        xredisClientComplete(R,Status,XREDIS_REPLIED,Reply,NULL);
    }else{xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
    xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;xredisSubscriberRetire(R);
    xredisClientCancelTimer(R);xredisRequestRelease(R);xredisClientWake(C);
}
static void xredisSubscriberWritten(xredisrequest* R)
{if(R->Observe>=5){xredisSubscriberState(R)->Written=true;xredisSubscriberFinish(R);}}
static bool xredisSubscriberQueue(xredisclient* C,xredissubscriber* M,xredisreply* Reply,size_t Bytes,bool Message)
{
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xpromise* Signal=NULL;
#endif
    xrtMutexLock(&C->Gate);bool Fits=C->Info.Pushes<C->Config.MaxPushes && Bytes<=C->Config.MaxPushBytes-C->Info.PushBytes;
    if(Fits){size_t N=(C->PushHead+C->Info.Pushes)%C->Config.MaxPushes;C->PushQueue[N]=Reply;++C->Info.Pushes;C->Info.PushBytes+=Bytes;
        if(Message)++M->Info.Messages;else ++M->Info.Sidebands;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
        Signal=M->ReadablePromise;if(Signal)M->ReadablePromise=NULL;
#endif
    }
    xrtMutexUnlock(&C->Gate);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xredisSubscriberNotify(Signal);
#endif
    return Fits;
}
static void xredisSubscriberFrame(xredisclient* C,xredisreply* Reply,const xredisreplyvalue* Root,size_t Bytes)
{
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);xredispubsubevent E;
    xredispubsubstatus Decoded=xredisPubSubEventRead(Reply,&E);
    if(Decoded==XREDIS_PUBSUB_EVENT && E.Kind<=XREDIS_PUBSUB_SMESSAGE){
        unsigned Family=E.Kind==XREDIS_PUBSUB_PMESSAGE?1:E.Kind==XREDIS_PUBSUB_SMESSAGE?2:0;
        xbytesview Name=Family==1?E.Pattern:E.Channel;
        xredissubentry* Entry=xredisSubscriberFind(M,Family,Name,xrtHash64(Name.Data,Name.Size));
        if(!Entry || !Entry->Confirmed)goto Protocol;
        if(!xredisSubscriberQueue(C,M,Reply,Bytes,true)){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_LIMIT,NULL);}return;
    }
    if(Decoded==XREDIS_PUBSUB_OTHER && Root->Type==XREDIS_REPLY_PUSH){
        if(!xredisSubscriberQueue(C,M,Reply,Bytes,false)){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_LIMIT,NULL);}return;
    }
    if(Decoded==XREDIS_PUBSUB_MALFORMED || Decoded==XREDIS_PUBSUB_ERROR)goto Protocol;
    xredisrequest* R=C->SentHead;if(!R || R->Observe<5)goto Protocol;
    xredissubcall* S=xredisSubscriberState(R);if(S->Finished)goto Protocol;
    if(Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR){
        if(S->Acknowledged)goto Protocol;
        S->Status=XREDIS_REQUEST_SERVER_ERROR;S->Finished=true;S->Final=Reply;xredisSubscriberFinish(R);return;
    }
    if(S->Command==XREDIS_PUBSUB_CMD_PING){
        xbytesview Expected;size_t Cursor=S->Cursor;
        if(S->Count && !xredisSubscriberName(R,&Cursor,&Expected))goto Protocol;
        if(xredisPubSubPongRead(Reply,S->Count?&Expected:NULL,&E)!=XREDIS_PUBSUB_EVENT)goto Protocol;
        S->Status=XREDIS_REQUEST_OK;S->Finished=true;S->Final=Reply;xredisSubscriberFinish(R);return;
    }
    if(Decoded!=XREDIS_PUBSUB_EVENT || E.Kind!=(xredispubsubkind)(S->Command+XREDIS_PUBSUB_SUBSCRIBE))goto Protocol;
    unsigned Family=(unsigned)S->Command%3;bool Subscribe=S->Command<=XREDIS_PUBSUB_CMD_SSUBSCRIBE;
    xbytesview Name=Family==1?E.Pattern:E.Channel;xredissubentry* Entry=NULL;
    if(S->Count){
        xbytesview Expected;size_t Cursor=S->Cursor;
        if(E.Flags&XREDIS_PUBSUB_NAME_ABSENT || !xredisSubscriberName(R,&Cursor,&Expected) || Name.Size!=Expected.Size || (Name.Size && memcmp(Name.Data,Expected.Data,Name.Size)))goto Protocol;
        S->Cursor=Cursor;Entry=Subscribe?S->Entries[S->Acknowledged]:xredisSubscriberFind(M,Family,Name,xrtHash64(Name.Data,Name.Size));
    }else{
        if(!S->AllInitialized){S->Expected=M->Counts[Family]?M->Counts[Family]:1;S->Absent=!M->Counts[Family];S->AllInitialized=true;}
        if(S->Absent){if(!(E.Flags&XREDIS_PUBSUB_NAME_ABSENT))goto Protocol;}
        else{
            if(E.Flags&XREDIS_PUBSUB_NAME_ABSENT)goto Protocol;
            Entry=xredisSubscriberFind(M,Family,Name,xrtHash64(Name.Data,Name.Size));if(!Entry || !Entry->Confirmed)goto Protocol;
        }
    }
    size_t Before=Family==2?M->Counts[2]:M->Counts[0]+M->Counts[1];
    bool Changes=Subscribe?Entry && !Entry->Confirmed:Entry && Entry->Confirmed;
    size_t After=Changes?(Subscribe?Before+1:Before-1):Before;if(E.Subscriptions!=After)goto Protocol;
    if(Changes){Entry->Confirmed=Subscribe;if(Subscribe)++M->Counts[Family];else --M->Counts[Family];}
    if(Entry && !Entry->Confirmed && !Entry->Reservations)xredisSubscriberDelete(M,Entry,false);
    ++S->Acknowledged;xrtMutexLock(&C->Gate);++M->Info.Acknowledgements;xrtMutexUnlock(&C->Gate);xredisSubscriberPublish(C,M);
    if(S->Acknowledged==S->Expected){S->Status=XREDIS_REQUEST_OK;S->Finished=true;S->Final=Reply;xredisSubscriberFinish(R);}else xredisReplyRelease(Reply);
    return;
Protocol:
    xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);
}
static xredisadmission xredisSubscriberCanStart(xredisclient* C)
{
    return C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:
        C->Info.State!=XREDIS_CLIENT_OPEN || !C->Info.Ready || C->Info.Requests || C->Info.Pushes || xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE)
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
        || C->Owner
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        || C->ObservationPreparing || xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE)
#endif
        ?XREDIS_ADMISSION_OVERLOADED:XREDIS_ADMISSION_ACCEPTED;
}
static xredisadmission xredisSubscriberStartScope(xredisclient* C,const xredissubscriberconfig* Config,ptr Scope)
{
    xredissubscriberconfig K;if(!C || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(C,C->StorageBytes,Config,sizeof(*Config))))){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber adoption arguments");return XREDIS_ADMISSION_ERROR;}
    if(Config)K=*Config;else xredisSubscriberConfigInit(&K);
    if(K.Size!=sizeof(K) || !K.MaxSubscriptions || K.MaxSubscriptions>100000 || !K.MaxNameBytes ||
       K.MaxNameBytes>SIZE_MAX-sizeof(xredissubscriber)-K.MaxSubscriptions*(sizeof(xredissubentry)+4*sizeof(xredissubentry*))){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber registry bounds");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);xredisadmission A=xredisSubscriberCanStart(C);if(A!=XREDIS_ADMISSION_ACCEPTED)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    if(A!=XREDIS_ADMISSION_ACCEPTED)return A;
    size_t Buckets=1;while(Buckets<2*K.MaxSubscriptions)Buckets*=2;
    size_t Bytes=sizeof(xredissubscriber)+Buckets*sizeof(xredissubentry*);xredissubscriber* M=xrtCalloc(1,Bytes);if(!M)return XREDIS_ADMISSION_ERROR;
    M->Config=K;M->Buckets=Buckets;M->StorageBytes=Bytes;M->Info.Size=sizeof(M->Info);M->Info.RegistryBytes=Bytes;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
    M->ReplayOwner=Scope;
#else
    (void)Scope;
#endif
    xrtMutexLock(&C->Gate);
    A=xredisSubscriberCanStart(C);
    if(A==XREDIS_ADMISSION_ACCEPTED)xrtAtomicPtrStore(&C->Subscriber,M,XMEMORY_RELEASE);else ++C->Info.Rejected;
    xrtMutexUnlock(&C->Gate);if(A!=XREDIS_ADMISSION_ACCEPTED)xrtFree(M);return A;
}
XREDIS_API xredisadmission xredisSubscriberStart(xredisclient* C,const xredissubscriberconfig* Config)
{return xredisSubscriberStartScope(C,Config,NULL);}
static xredisadmission xredisSubscriberAdmit(xredisclient* C,xredispubsubcommand Command,const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future,double Entered)
{
    if((unsigned)Command>XREDIS_PUBSUB_CMD_PING){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber command");return XREDIS_ADMISSION_ERROR;}
    return xredisClientCallAdmit(C,Names,Count,Timeout,Future,Entered,NULL,true,(unsigned)Command+5);
}
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
xredisadmission __xredisSubscriberReplayStart(xredisclient* C,const xredissubscriberconfig* Config,ptr Owner)
{return xredisSubscriberStartScope(C,Config,Owner);}
void __xredisSubscriberReplayRelease(xredisclient* C,ptr Owner)
{
    xrtMutexLock(&C->Gate);xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    if(M && M->ReplayOwner==Owner)M->ReplayOwner=NULL;
    xrtMutexUnlock(&C->Gate);
}
xredisadmission __xredisSubscriberCommandBudget(xredisclient* C,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,double Entered,ptr Owner,xfuture** Future)
{return xredisClientCallAdmit(C,Names,Count,Timeout,Future,Entered,Owner,true,(unsigned)Command+5);}
#endif
XREDIS_API xredisadmission xredisSubscriberCommandAsync(xredisclient* C,xredispubsubcommand Command,const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future)
{return xredisSubscriberAdmit(C,Command,Names,Count,Timeout,Future,xrtTimer());}
XREDIS_API xredisadmission xredisSubscriberCommand(xredisclient* C,xredispubsubcommand Command,const xbytesview* Names,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"subscriber client is NULL");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync subscriber cannot block an Engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisSubscriberAdmit(C,Command,Names,Count,Timeout,Future,Entered);if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);return A;
}
XREDIS_API bool xredisSubscriberInfo(xredisclient* C,xredissubscriberinfo* Info)
{
    if(!C || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || xrtMemRangesOverlap(C,C?C->StorageBytes:0,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber info output");
    xredissubscriber* M=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    if(!M || xrtMemRangesOverlap(M,M->StorageBytes,Info,sizeof(*Info)))return xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode is not active or output overlaps registry");
    xrtMutexLock(&C->Gate);*Info=M->Info;xrtMutexUnlock(&C->Gate);return true;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisTrackingOutput(xredisclient* C,const void* Output,size_t Bytes)
{
    if(!C || !Output || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(C,C->StorageBytes,Output,Bytes))return false;
    xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    return !S || !xrtMemRangesOverlap(S,S->Bytes,Output,Bytes);
}
XREDIS_API xredisadmission xredisClientTrackingObserve(xredisclient* C,size_t MaxBytes)
{
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking client");return XREDIS_ADMISSION_ERROR;}
    if(!MaxBytes)MaxBytes=XREDIS_TRACKING_OBSERVATION_BYTES_DEFAULT;
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    size_t Bytes=sizeof(xredisobservation);
    xredisadmission A=C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    if(A==XREDIS_ADMISSION_ACCEPTED && xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE))A=XREDIS_ADMISSION_OVERLOADED;
#endif
    if(A==XREDIS_ADMISSION_ACCEPTED && (C->Config.MaxPushes>(SIZE_MAX-Bytes)/sizeof(uint64) ||
        (Bytes+=C->Config.MaxPushes*sizeof(uint64))>MaxBytes || C->ObservationPreparing))A=XREDIS_ADMISSION_OVERLOADED;
    if(A!=XREDIS_ADMISSION_ACCEPTED || S){
        if(A==XREDIS_ADMISSION_OVERLOADED && C->ObservationRejected!=UINT64_MAX)++C->ObservationRejected;
        xrtMutexUnlock(&C->Gate);return A;
    }
    C->ObservationPreparing=true;xrtMutexUnlock(&C->Gate);
    S=xrtCalloc(1,Bytes);
    if(S){S->Bytes=Bytes;xrtAtomic64Init(&S->Sequence,0);}
    xrtMutexLock(&C->Gate);
    A=!S?XREDIS_ADMISSION_ERROR:C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
    if(A==XREDIS_ADMISSION_ACCEPTED)xrtAtomicPtrStore(&C->Observation,S,XMEMORY_RELEASE);
    else {xrtFree(S);if(C->ObservationRejected!=UINT64_MAX)++C->ObservationRejected;}
    /* Refund unpublished storage before physical close can observe quiescence. */
    C->ObservationPreparing=false;
    if(C->Info.State>=XREDIS_CLIENT_CLOSING)(void)xredisClientWakeLocked(C);
    xrtMutexUnlock(&C->Gate);return A;
}
XREDIS_API bool xredisClientTrackingInfo(xredisclient* C,xredistrackinginfo* Info)
{
    if(!xredisTrackingOutput(C,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking info output");
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    *Info=(xredistrackinginfo){sizeof(*Info),S?S->Bytes:0,C->ObservationPreparing?sizeof(xredisobservation)+C->Config.MaxPushes*sizeof(uint64):0,S?xrtAtomic64Load(&S->Sequence,XMEMORY_ACQUIRE):0,C->ObservationRejected,S!=NULL,C->ObservationPreparing};
    xrtMutexUnlock(&C->Gate);return true;
}
XREDIS_API xredisadmission xredisClientTrackingCallAsync(xredisclient* C,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{return xredisClientCallAdmit(C,Args,Count,Timeout,Future,xrtTimer(),NULL,false,true);}
XREDIS_API xredisadmission xredisClientCachingCallAsync(xredisclient* C,bool Caching,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{return xredisClientCallAdmit(C,Args,Count,Timeout,Future,xrtTimer(),NULL,false,Caching?3:4);}
XREDIS_API xredisadmission xredisClientCachingCall(xredisclient* C,bool Caching,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid caching client");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync caching call cannot block an engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,Caching?3:4);
    if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);
    return A;
}
XREDIS_API xredisadmission xredisClientTrackingCall(xredisclient* C,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking client");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync tracking call cannot block an engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,true);
    if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);
    return A;
}
XREDIS_API bool xredisClientTrackingTake(xredisclient* C,xredistrackingpush* Push)
{
    if(!xredisTrackingOutput(C,Push,sizeof(*Push)))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking push output");
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    if(!S){xrtMutexUnlock(&C->Gate);return xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"tracking observation is not enabled");}
    if(!C->Info.Pushes){xrtMutexUnlock(&C->Gate);return false;}
    size_t Index=C->PushHead;xredisreply* Value=C->PushQueue[Index];C->PushQueue[Index]=NULL;
    *Push=(xredistrackingpush){Value,S->Pushes[Index]};S->Pushes[Index]=0;
    C->PushHead=(Index+1)%C->Config.MaxPushes;--C->Info.Pushes;
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(Value,&I);C->Info.PushBytes-=I.ReservedBytes;
    xrtMutexUnlock(&C->Gate);return true;
}
XREDIS_API xredisadmission xredisClientTrackingConfigureAsync(xredisclient* C,const xredistrackingconfig* K,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!K || !xrtMemRangeValid(K,sizeof(*K)) || K->Size!=sizeof(*K) || K->Flags&~31u || K->Redirect<0 ||
        K->PrefixCount>XREDIS_TRACKING_PREFIXES_MAX || !xrtMemRangeValid(K->Prefixes,K->PrefixCount*sizeof(*K->Prefixes)) ||
        ((K->Flags&XREDIS_TRACKING_OPTIN) && (K->Flags&XREDIS_TRACKING_OPTOUT)) ||
        ((K->Flags&XREDIS_TRACKING_BCAST) && (K->Flags&(XREDIS_TRACKING_OPTIN|XREDIS_TRACKING_OPTOUT))) ||
        (K->PrefixCount && !(K->Flags&XREDIS_TRACKING_BCAST)) || (K->Redirect && !(K->Flags&XREDIS_TRACKING_REDIRECT)) ||
        (!K->Enabled && (K->Flags || K->Redirect || K->PrefixCount)) || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) ||
        xrtMemRangesOverlap(K,sizeof(*K),Future,sizeof(*Future)) || xrtMemRangesOverlap(K->Prefixes,K->PrefixCount*sizeof(*K->Prefixes),Future,sizeof(*Future))){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking configuration");return XREDIS_ADMISSION_ERROR;
    }
    if(C && K->Enabled && (!(K->Flags&XREDIS_TRACKING_REDIRECT) || !K->Redirect)){
        xrtMutexLock(&C->Gate);
        bool Unknown=C->Info.State<XREDIS_CLIENT_CLOSING && !(C->Managed &&
            (C->InitMode==XREDIS_INIT_RESP3 || (C->Info.Ready && C->Info.Protocol==XREDIS_RESP3)));
        xrtMutexUnlock(&C->Gate);
        if(Unknown){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"direct invalidations require managed RESP3 selection");return XREDIS_ADMISSION_ERROR;}
    }
    xbytesview Args[3+2+2*XREDIS_TRACKING_PREFIXES_MAX+4];size_t Count=0;
    Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("CLIENT");Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("TRACKING");
    Args[Count++]=K->Enabled?(xbytesview)XRT_BYTES_LITERAL("ON"):(xbytesview)XRT_BYTES_LITERAL("OFF");
    unsigned char Number[20];size_t Start=sizeof(Number);uint64 Id=(uint64)K->Redirect;
    if(K->Flags&XREDIS_TRACKING_REDIRECT){do {Number[--Start]=(unsigned char)('0'+Id%10u);Id/=10u;}while(Id);
        Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("REDIRECT");Args[Count++]=(xbytesview){Number+Start,sizeof(Number)-Start};}
    for(size_t I=0;I<K->PrefixCount;++I){Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("PREFIX");Args[Count++]=K->Prefixes[I];}
    const xbytesview Flags[]={XRT_BYTES_LITERAL("BCAST"),XRT_BYTES_LITERAL("OPTIN"),XRT_BYTES_LITERAL("OPTOUT"),XRT_BYTES_LITERAL("NOLOOP")};
    for(unsigned I=0;I<4;++I)if(K->Flags&(1u<<I))Args[Count++]=Flags[I];
    return xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,2);
}
#endif

XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, xrtTimer(), NULL, false XREDIS_OBSERVE_ORDINARY); }
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedCall(xredisclient* Client, xredisexclusiveowner* Owner,
    const xbytesview* Arguments, size_t Count, int64 Timeout, double Entered, bool Control, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, Entered, Owner, Control XREDIS_OBSERVE_ORDINARY); }
#endif

XREDIS_API xredisadmission xredisClientCall(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{
	if (Client == NULL) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return XREDIS_ADMISSION_ERROR;
	}
	xrtMutexLock(&Client->Gate);
	bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL;
	xrtMutexUnlock(&Client->Gate);
	if (Worker) { xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "sync client call cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
	xredisadmission Admission = xredisClientCallAsync(Client, Arguments, Count, Timeout, Future);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
	return Admission;
}

XREDIS_API bool xredisClientTakePush(xredisclient* Client, xredisreply** Reply)
{
	if (Client == NULL || Reply == NULL || !xrtMemRangeValid(Reply, sizeof(*Reply)) ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Reply, sizeof(*Reply)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid push output");
	xrtMutexLock(&Client->Gate);
	if (Client->Info.Pushes == 0) { xrtMutexUnlock(&Client->Gate); return false; }
	xredisreply* Value = Client->PushQueue[Client->PushHead]; Client->PushQueue[Client->PushHead] = NULL;
	Client->PushHead = (Client->PushHead + 1u) % Client->Config.MaxPushes; --Client->Info.Pushes;
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Value, &Info); Client->Info.PushBytes -= Info.ReservedBytes;
	*Reply = Value; xrtMutexUnlock(&Client->Gate); return true;
}

XREDIS_API bool xredisClientInfo(xredisclient* Client, xredisclientinfo* Info)
{
	if (Client == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client info output");
	xrtMutexLock(&Client->Gate); *Info = Client->Info; xrtMutexUnlock(&Client->Gate); return true;
}

XREDIS_API bool xredisClientClose(xredisclient* Client)
{
	if (Client == NULL) return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL");
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) { Client->Info.State = XREDIS_CLIENT_CLOSING; Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED; }
	bool Result = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate); return Result;
}

XREDIS_API xfuture* xredisClientClosed(xredisclient* Client)
{
	if (Client == NULL) { xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return NULL; }
	return xrtFutureRef(Client->CloseFuture);
}


#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
static void xredisSubscriberNotify(xpromise* Promise)
{if(Promise){(void)xrtPromiseResolve(Promise,NULL);xrtPromiseDestroy(Promise);}}
static bool xredisSubscriberOutput(xredisclient* C,void* Output,size_t Bytes,xredissubscriber** Mode)
{
    if(!C || !Output || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(C,C?C->StorageBytes:0,Output,Bytes))return false;
    *Mode=xrtAtomicPtrLoad(&C->Subscriber,XMEMORY_ACQUIRE);
    return !*Mode || !xrtMemRangesOverlap(*Mode,(*Mode)->StorageBytes,Output,Bytes);
}
/* Preparing protects the owning Engine activation until unexposed Future
 * resources are destroyed and the replacement epoch is fully published. */
static void xredisSubscriberPrepared(xredisclient* C,xredissubscriber* M)
{
    xrtMutexLock(&C->Gate);M->ReadablePreparing=false;
    if(C->Info.State>=XREDIS_CLIENT_CLOSING && !C->CloseDone)(void)xredisClientWakeLocked(C);
    xrtMutexUnlock(&C->Gate);
}
XREDIS_API xredisadmission xredisSubscriberReadable(xredisclient* C,xfuture** Output)
{
    xredissubscriber* M=NULL;
    if(!xredisSubscriberOutput(C,Output,sizeof(*Output),&M)){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber readiness output");return XREDIS_ADMISSION_ERROR;
    }
    if(!M){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode is not active");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->CloseDone && !C->Info.Pushes){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_CLOSED;}
    if(M->ReadableFuture && (M->ReadablePromise || C->Info.Pushes)){
        xfuture* F=xrtFutureRef(M->ReadableFuture);if(F)*Output=F;
        xrtMutexUnlock(&C->Gate);return F?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_ERROR;
    }
    if(M->ReadablePreparing){xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_OVERLOADED;}
    M->ReadablePreparing=true;xrtMutexUnlock(&C->Gate);
    xfuture* F=NULL;xfuture* Returned=NULL;xpromise* P=xrtPromiseCreate(&F,NULL);
    if(P)Returned=xrtFutureRef(F);
    if(!P || !Returned){xrtPromiseDestroy(P);xrtFutureDestroy(F);xredisSubscriberPrepared(C,M);return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->CloseDone && !C->Info.Pushes){
        xrtMutexUnlock(&C->Gate);xrtPromiseDestroy(P);xrtFutureDestroy(F);xrtFutureDestroy(Returned);
        xredisSubscriberPrepared(C,M);return XREDIS_ADMISSION_CLOSED;
    }
    if(C->Info.Pushes || C->CloseDone){
        xrtMutexUnlock(&C->Gate);xredisSubscriberNotify(P);P=NULL;xrtMutexLock(&C->Gate);
    }
    xfuture* Old=M->ReadableFuture;M->ReadableFuture=F;M->ReadablePromise=P;
    xrtMutexUnlock(&C->Gate);xrtFutureDestroy(Old);
    /* No fallible operation follows publication. Keep Preparing until the old
     * reference is dropped; a closing worker must observe the final epoch. */
    xrtMutexLock(&C->Gate);M->ReadablePreparing=false;*Output=Returned;
    if(C->Info.State>=XREDIS_CLIENT_CLOSING && !C->CloseDone)(void)xredisClientWakeLocked(C);
    xrtMutexUnlock(&C->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API bool xredisSubscriberTakeBatch(xredisclient* C,xredisreply** Replies,size_t Capacity,size_t* Taken)
{
    xredissubscriber *M=NULL,*Other=NULL;
    if(!Capacity || Capacity>SIZE_MAX/sizeof(*Replies) ||
        !xredisSubscriberOutput(C,Replies,Capacity*sizeof(*Replies),&M) ||
        !xredisSubscriberOutput(C,Taken,sizeof(*Taken),&Other) ||
        xrtMemRangesOverlap(Replies,Capacity*sizeof(*Replies),Taken,sizeof(*Taken)))
        return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid subscriber batch outputs");
    if(!M)return xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"subscriber mode is not active");
    xrtMutexLock(&C->Gate);size_t Count=C->Info.Pushes<Capacity?C->Info.Pushes:Capacity;
    if(!Count){xrtMutexUnlock(&C->Gate);return false;}
    size_t Bytes=0,Head=C->PushHead,Limit=C->Config.MaxPushes;
    for(size_t N=0;N<Count;++N){
        xredisreply* R=C->PushQueue[Head];C->PushQueue[Head]=NULL;
        if(++Head==Limit)Head=0;
        xredisreplyinfo I={0};I.Size=sizeof(I);(void)xredisReplyInfo(R,&I);Bytes+=I.ReservedBytes;Replies[N]=R;
    }
    C->PushHead=Head;C->Info.Pushes-=Count;C->Info.PushBytes-=Bytes;*Taken=Count;xrtMutexUnlock(&C->Gate);return true;
}
#endif

#endif

#undef XREDIS_OBSERVE_PARAMETER
#undef XREDIS_OBSERVE_ORDINARY
#undef XREDIS_SUBSCRIBER_RETIRE
