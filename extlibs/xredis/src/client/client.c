#include <xredis/client.h>
#include "../internal/client_connect.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/number.h>
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)

typedef struct xredisrequest xredisrequest;
typedef struct xredisinitwire xredisinitwire;
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
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request);
	}
}

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
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE;
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
	if (Root.Type == XREDIS_REPLY_PUSH) {
		xrtMutexLock(&Client->Gate);
		bool Fits = Client->Info.Pushes < Client->Config.MaxPushes &&
			Info.ReservedBytes <= Client->Config.MaxPushBytes - Client->Info.PushBytes;
		if (Fits) {
			size_t Index = (Client->PushHead + Client->Info.Pushes) % Client->Config.MaxPushes;
			Client->PushQueue[Index] = Reply; ++Client->Info.Pushes; Client->Info.PushBytes += Info.ReservedBytes;
		}
		xrtMutexUnlock(&Client->Gate);
		if (!Fits) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_LIMIT, NULL); }
		return;
	}
	if (Client->ReadyPromise != NULL) { xredisClientInitFrame(Client, Reply, &Root); return; }
	xredisrequest* Request = Client->SentHead;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Request != NULL && Request == Client->TlsWrite
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
		&& !Request->Transaction
#endif
	) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#endif
	if (Request == NULL) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) { xredisTransactionFrame(Request, Reply, &Root); return; }
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
		xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	} else {
		xredisReplyRelease(Reply); xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE;
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
		if (Request->Promise == NULL) { Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request); continue; }
		if (xredisClientClosing(Client)) {
			xredisClientComplete(Request, xredisClientFailureStatus(Client), XREDIS_NOT_SENT, NULL, Client->CloseCause);
			Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request); continue;
		}
		xredisClientAppend(Client, Request, false);
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) xredisClientAbandon(Request, XREDIS_REQUEST_CANCELLED);
		else if (xrtTimer() >= Request->Deadline) xredisClientAbandon(Request, XREDIS_REQUEST_TIMEOUT);
		else if (!xredisClientArm(Request, Request->Deadline)) {
			xredisClientComplete(Request, XREDIS_REQUEST_MEMORY, XREDIS_NOT_SENT, NULL, xrtGetError());
			xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request);
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
		xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire, Request->WireBytes, xredisClientWireDrop, Request);
		if (Result == XNET_RESULT_OK) continue;
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
		Request->Phase = XREDIS_R_DONE; xredisClientCancelTimer(Request); xredisRequestRelease(Request);
	}
}

static void xredisClientRetire(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests &&
		(Client->PendingHead != NULL || Client->SentHead != NULL); ++Work) {
		xredisrequest* Request = Client->PendingHead != NULL ? Client->PendingHead : Client->SentHead;
		xredisdelivery Delivery = Request->Phase == XREDIS_R_SENT ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT;
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE;
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
	}
	xrtMutexLock(&Client->Gate);
	bool Again = Client->SubmitHead != NULL || Client->CancelHead != NULL ||
		(Client->Info.State >= XREDIS_CLIENT_CLOSING && (Client->PendingHead != NULL || Client->SentHead != NULL));
	if (Again) (void)xredisClientWakeLocked(Client);
	bool Done = Client->TransportClosed && Client->Info.Requests == 0 && Client->Info.InitBytes == 0 &&
		Client->InitTimer == 0 && !Client->WakeQueued && !Client->CloseDone;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Done = Done && !Client->PlanPending;
#endif
	if (Done) { Client->CloseDone = true; Client->Info.State = XREDIS_CLIENT_CLOSED; Client->EnginePinned = false; }
	xrtMutexUnlock(&Client->Gate);
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
        Client->Info.State != XREDIS_CLIENT_OPEN || !Client->Info.Ready || Client->Info.Requests || Client->Owner ?
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
        Client->Info.Requests || Client->Owner ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
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
    double Entered, xredisexclusiveowner* Owner, bool Control)
{
	size_t Bytes = 0;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client call arguments"); return XREDIS_ADMISSION_ERROR;
	}
	if (!xredisCommandSize(Arguments, Count, &Bytes)) return XREDIS_ADMISSION_ERROR;
	if (xrtMemRangesOverlap(Arguments, Count * sizeof(*Arguments), Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps arguments"); return XREDIS_ADMISSION_ERROR;
	}
	for (size_t Index = 0; Index < Count; ++Index) if (xrtMemRangesOverlap(Arguments[Index].Data, Arguments[Index].Size, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps argument payload"); return XREDIS_ADMISSION_ERROR;
	}
	if (!xredisClientCommandMode(Arguments, Count)) {
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
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission = xredisClientOwnerAdmission(Client, Owner, Control);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) Admission = Client->Info.Requests >= Client->Config.MaxRequests || Bytes > Client->Config.MaxCommandBytes ||
		Bytes > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Admission == XREDIS_ADMISSION_ACCEPTED && !xredisClientQuotaReserve(Owner, Bytes)) Admission = XREDIS_ADMISSION_OVERLOADED;
#endif
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Bytes; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Bytes);
	if (Request == NULL) {
		xredisClientReservationDrop(Client, Bytes); return XREDIS_ADMISSION_ERROR;
	}
	/* Initialize metadata only: opaque wire bytes are written by the encoder,
	 * without a redundant full-payload zero pass. */
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && (!Control || xredisClientWord(Arguments[0], "WATCH")) && Request->Deadline > Owner->Deadline) Request->Deadline = Owner->Deadline;
#endif
	Request->Sensitive = xredisClientWord(Arguments[0], "AUTH") || xredisClientWord(Arguments[0], "HELLO");
	Request->Result = (xredisrequestresult*)xrtMalloc(sizeof(*Request->Result));
	xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token); /* FutureCancelToken returns a retained reference. */
	}
	if (Request->Watch != NULL && xredisCommandWrite(Request->Wire, Bytes, Arguments, Count, NULL)) {
		xrtMutexLock(&Client->Gate);
        Admission = xredisClientOwnerAdmission(Client, Owner, Control);
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

XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, xrtTimer(), NULL, false); }
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedCall(xredisclient* Client, xredisexclusiveowner* Owner,
    const xbytesview* Arguments, size_t Count, int64 Timeout, double Entered, bool Control, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, Entered, Owner, Control); }
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

#endif
