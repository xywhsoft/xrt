#include <xredis/client.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)

typedef struct xredisrequest xredisrequest;
enum { XREDIS_R_SUBMITTED, XREDIS_R_PENDING, XREDIS_R_SENT, XREDIS_R_DONE };

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
	bool CancelQueued;
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
	xnetpost Wake;
	xredisrequest *SubmitHead, *SubmitTail, *CancelHead, *CancelTail;
	xredisrequest *PendingHead, *PendingTail, *SentHead, *SentTail;
	xredisclientinfo Info;
	xerror* CloseCause;
	size_t StorageBytes, SentCount, PushHead;
	bool WakeQueued, CloseDone, EnginePinned;
	/* Only the owning worker accesses these fields. */
	bool Started, TransportClosed, ReadEnded, Reading, ReadYielded, ReadPaused;
	xredisreply* PushQueue[];
};

static void xredisClientWork(xnetworker* Worker, ptr Data);
static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data);
static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data);

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

static void xredisRequestRelease(xredisrequest* Request)
{
	if (xrtAtomic32FetchSub(&Request->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Request->Client;
	size_t WireBytes = Request->WireBytes;
	/* The reservation protects actual storage, including the free tail. Keep
	 * the independent Client reference while dropping it AFTER the buffer. */
	xrtFree(Request);
	xrtMutexLock(&Client->Gate);
	--Client->Info.Requests; Client->Info.CommandBytes -= WireBytes;
	if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
	xrtMutexUnlock(&Client->Gate);
	xredisClientRelease(Client);
}

static void xredisClientWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisRequestRelease((xredisrequest*)Data); }

static void xredisClientResultDrop(ptr Value, ptr Data)
{
	(void)Data; xredisrequestresult* Result = (xredisrequestresult*)Value;
	xredisReplyRelease(Result->Reply); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

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
	*Result = (xredisrequestresult){Status, Delivery, Reply, xrtErrorRef(Cause)};
	xrtMutexLock(&Request->Client->Gate); ++Request->Client->Info.Completed; xrtMutexUnlock(&Request->Client->Gate);
	if (!xrtPromiseResolveOwned(Promise, Result, xredisClientResultDrop, NULL)) xredisClientResultDrop(Result, NULL);
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
	if (Client->Stream != NULL) xrtNetStreamAbort(Client->Stream); else Client->TransportClosed = true;
	xredisClientWake(Client);
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
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State == XREDIS_CLIENT_CONNECTING) Client->Info.State = XREDIS_CLIENT_OPEN;
	xrtMutexUnlock(&Client->Gate); xredisClientWake(Client);
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
	xredisrequest* Request = Client->SentHead;
	if (Request == NULL) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
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

static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
	xredisclient* Client = (xredisclient*)Data;
	if (Client->Reading || xredisClientClosing(Client)) return;
	Client->Reading = true; size_t Bytes = 0, Frames = 0;
	while (Bytes < Client->Config.ReadBytes && Frames < Client->Config.ReadFrames && !xredisClientClosing(Client)) {
		xnetspan Span; if (!xrtNetBufFront(Buffer, &Span)) break;
		size_t Available = Client->Config.ReadBytes - Bytes;
		if (Span.Size > Available) Span.Size = Available;
		xredisreply* Reply = NULL; size_t Consumed = 0;
		xredisreplystatus Status = xredisReplyReaderFeed(Client->Reader, (xbytesview){Span.Data, Span.Size}, &Consumed, &Reply);
		xrtNetBufConsume(Buffer, Consumed); Bytes += Consumed;
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
		Client->ReadYielded = true; Client->ReadPaused = true; xrtNetStreamPause(Stream); xredisClientWake(Client);
	}
}

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

static void xredisClientPump(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests && Client->PendingHead != NULL &&
		Client->SentCount < Client->Config.MaxInFlight && !xredisClientClosing(Client) &&
		xrtNetStreamState(Client->Stream) == XNET_STREAM_OPEN; ++Work) {
		xredisrequest* Request = Client->PendingHead;
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
		if (!xredisClientClosing(Client)) {
			Client->Stream = xrtNetStreamConnect(Client->Engine, &Client->Remote, xrtNetWorkerIndex(Client->Worker),
				&Client->Config.Stream, &xredisClientEvents, Client);
			if (Client->Stream == NULL) xredisClientFail(Client, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
				XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, xrtGetError());
		} else Client->TransportClosed = true;
	}
	xredisClientIngest(Client);
	if (!xredisClientClosing(Client) && Client->Stream != NULL) {
		const xnetbuf* Buffer = xrtNetStreamBuffer(Client->Stream);
		if (Buffer != NULL && xrtNetBufSize(Buffer) != 0) xredisClientRead(Client->Stream, (xnetbuf*)Buffer, Client);
		if (!xredisClientClosing(Client) && !Client->ReadEnded && !Client->ReadYielded && Client->ReadPaused) {
			Client->ReadPaused = false; xrtNetStreamResume(Client->Stream);
		}
		if (Client->ReadEnded && !Client->ReadYielded && !xredisClientClosing(Client)) {
			bool Complete = xredisReplyReaderFinish(Client->Reader);
			xredisClientFail(Client, Complete ? XREDIS_REQUEST_DISCONNECTED : XREDIS_REQUEST_PROTOCOL,
				Complete ? NULL : xrtGetError());
		}
		xredisClientPump(Client);
	}
	if (xredisClientClosing(Client)) {
		if (Client->Stream != NULL && !Client->TransportClosed) xrtNetStreamAbort(Client->Stream);
		xredisClientRetire(Client);
	}
	xrtMutexLock(&Client->Gate);
	bool Again = Client->SubmitHead != NULL || Client->CancelHead != NULL ||
		(Client->Info.State >= XREDIS_CLIENT_CLOSING && (Client->PendingHead != NULL || Client->SentHead != NULL));
	if (Again) (void)xredisClientWakeLocked(Client);
	bool Done = Client->TransportClosed && Client->Info.Requests == 0 && !Client->WakeQueued && !Client->CloseDone;
	if (Done) { Client->CloseDone = true; Client->Info.State = XREDIS_CLIENT_CLOSED; Client->EnginePinned = false; }
	xrtMutexUnlock(&Client->Gate);
	if (Done) {
		xpromise* Promise = Client->ClosePromise; Client->ClosePromise = NULL;
		xrtNetStreamDestroy(Client->Stream); Client->Stream = NULL;
		xredisReplyReaderDestroy(Client->Reader); Client->Reader = NULL;
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

XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result)
{
	xredisclientconfig Copy;
	if (Engine == NULL || Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
		!xrtMemRangeValid(Result, sizeof(*Result)) || xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client creation arguments");
	if (Config == NULL) xredisClientConfigInit(&Copy); else Copy = *Config;
	if (Copy.Size != sizeof(Copy) || Copy.MaxRequests == 0 || Copy.MaxRequests > 1000000u ||
		Copy.MaxInFlight == 0 || Copy.MaxInFlight > Copy.MaxRequests || Copy.MaxCommandBytes == 0 ||
		Copy.MaxCommandBytes > Copy.MaxCommandBytesTotal || Copy.MaxCommandBytes > Copy.Stream.WriteLimit ||
		Copy.MaxCommandBytes > SIZE_MAX - sizeof(xredisrequest) || Copy.WorkRequests == 0 ||
		Copy.ReadBytes == 0 || Copy.ReadFrames == 0 || Copy.DrainTimeout <= 0 ||
		Copy.MaxPushes > (SIZE_MAX - sizeof(xredisclient)) / sizeof(xredisreply*) ||
		(Remote->Family != XNET_FAMILY_IPV4 && Remote->Family != XNET_FAMILY_IPV6) || Remote->Port == 0 ||
		!xrtNetStreamConfigValid(&Copy.Stream))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config or endpoint");
	if (!xrtNetEnginePin(Engine)) return false;
	size_t Bytes = sizeof(xredisclient) + Copy.MaxPushes * sizeof(xredisreply*);
	xredisclient* Client = (xredisclient*)xrtCalloc(1, Bytes);
	if (Client == NULL) { xrtNetEngineUnpin(Engine); return false; }
	Client->Engine = Engine; Client->EnginePinned = true; Client->Config = Copy; Client->Remote = *Remote;
	Client->StorageBytes = Bytes; Client->Info.Size = sizeof(Client->Info); Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED;
	Client->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
	xrtAtomic32Init(&Client->References, 1);
	if (!xrtMutexInit(&Client->Gate)) { xrtNetEngineUnpin(Engine); xrtFree(Client); return false; }
	if (!xrtNetPostInit(&Client->Wake) || !xredisReplyReaderCreate(&Copy.Replies, &Client->Reader) ||
		(Client->ClosePromise = xrtPromiseCreate(&Client->CloseFuture, NULL)) == NULL) { xredisClientRelease(Client); return false; }
	xredisClientRef(Client); /* Active connection, even before Stream is created. */
	xrtMutexLock(&Client->Gate); bool Posted = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
	if (!Posted) { xredisClientRelease(Client); xredisClientRelease(Client); return false; }
	*Result = Client; return true;
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

XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{
	double Entered = xrtTimer(); size_t Bytes = 0;
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
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
		Client->Info.Requests >= Client->Config.MaxRequests || Bytes > Client->Config.MaxCommandBytes ||
		Bytes > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Bytes; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Bytes);
	if (Request == NULL) {
		xrtMutexLock(&Client->Gate); --Client->Info.Requests; Client->Info.CommandBytes -= Bytes;
		if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
		xrtMutexUnlock(&Client->Gate); xredisClientRelease(Client); return XREDIS_ADMISSION_ERROR;
	}
	/* Initialize metadata only: opaque wire bytes are written by the encoder,
	 * without a redundant full-payload zero pass. */
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
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
		if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
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
		} else { Admission = XREDIS_ADMISSION_CLOSED; ++Client->Info.Rejected; }
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_CLOSED ? XREDIS_ADMISSION_CLOSED : XREDIS_ADMISSION_ERROR;
}

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
