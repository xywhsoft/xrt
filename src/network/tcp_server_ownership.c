#include "../internal/xrt_tcp_server.h"
#if defined(XRT_FEATURE_NET_TCP_SERVER)

bool __xrtNetServerEnter(xnetserver* s, bool submit)
{
	if (!s) {__xrtErrorSetInvalidArgument();return false;}
	xrtownershipscope scope = {0};if (!xrtOwnershipMutationBegin(&scope)) return false;
	uint32 state = xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE);
	uint32 active = xrtAtomic32Load(&s->OwnershipActive, XMEMORY_ACQUIRE);bool entered = false;
	/* An independently referenced CLOSED shell remains usable while the
	 * runtime slot retires. Its methods never borrow Engine on that path. */
	if (!(state & XRT_NET_SERVER_CLEARED) &&
		(!submit || !xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE))) {
		while (active < UINT32_MAX) {
			uint32 expected = active;
			if (xrtAtomic32CompareExchange(&s->OwnershipActive, &expected, active + 1, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {entered = true;break;}
			active = expected;
		}
	}
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	if (!entered) __xrtErrorSetInvalidState();
	return entered;
}
/* CLOSED alone is not physical retirement. Every Listener Drop tail and
 * public borrow must leave before the actual Engine runtime slot is taken. */
static bool __xrtNetServerRetireLocked(xnetserver* s)
{
	bool retire = s->RuntimeHeld && s->StartDone && xrtNetServerState(s) == XNET_SERVER_CLOSED &&
		!xrtAtomic32Load(&s->Bindings, XMEMORY_ACQUIRE) && !xrtAtomic32Load(&s->OwnershipActive, XMEMORY_ACQUIRE) &&
		!xrtAtomic32Load(&s->TailCommands, XMEMORY_ACQUIRE) &&
		!xrtAtomic32Load(&s->OwnershipStop, XMEMORY_ACQUIRE) &&
		!xrtAtomic32Load(&s->Retiring, XMEMORY_ACQUIRE);
	for (size_t i = 0; retire && i < s->StartedListeners; ++i) {
		const xnetlistener* listener = s->Children[i].Listener;
		if (!listener || !(xrtAtomic32Load(&listener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_READY)) retire = false;
	}
	if (retire) xrtAtomic32Store(&s->Retiring, 1, XMEMORY_RELEASE);
	return retire;
}
static const xnetruntimeownershipv1* __xrtNetServerRetireTake(xnetserver* s)
{
	const xnetruntimeownershipv1* policy = NULL;
	if (!s->EngineHeld || __xrtNetEngineRuntimeTake(s->Engine, &s->RuntimeOwner, &policy) != s ||
		policy != xrtNetServerRuntimeOwnershipV1Get()) abort();
	for (size_t i = 0; i < s->StartedListeners; ++i) {
		__xrtSpinLock(&s->Lock);
		xnetlistener* listener = s->Children[i].Listener;s->Children[i].Listener = NULL;
		__xrtSpinUnlock(&s->Lock);
		xrtNetListenerDestroy(listener);
	}
	__xrtSpinLock(&s->Lock);
	s->RuntimeHeld = false;s->EngineHeld = false;
	__xrtSpinUnlock(&s->Lock);
	__xrtNetEngineObjectRelease(s->Engine);
	xrtAtomic32Store(&s->OwnershipState, XRT_NET_SERVER_READY, XMEMORY_RELEASE);
	return policy;
}
void __xrtNetServerLeave(xnetserver* s)
{
	xrtownershipscope scope = {0};if (!xrtOwnershipMutationBegin(&scope)) abort();
	/* The last borrow and retirement election share this lock. A concurrent
	 * last binding Drop cannot free an unreferenced runtime-borrowed receiver
	 * in a gap between Active reaching zero and the retirement decision. */
	__xrtSpinLock(&s->Lock);
	uint32 before = xrtAtomic32FetchSub(&s->OwnershipActive, 1, XMEMORY_ACQ_REL);if (!before) abort();
	bool retire = before == 1 && __xrtNetServerRetireLocked(s);
	__xrtSpinUnlock(&s->Lock);
	const xnetruntimeownershipv1* policy = retire ? __xrtNetServerRetireTake(s) : NULL;
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	if (policy) policy->Release(s); /* no receiver access after the borrow */
}
void __xrtNetServerTryRetire(xnetserver* s)
{
	xrtownershipscope scope = {0};if (!xrtOwnershipMutationBegin(&scope)) abort();
	__xrtSpinLock(&s->Lock);bool retire = __xrtNetServerRetireLocked(s);__xrtSpinUnlock(&s->Lock);
	const xnetruntimeownershipv1* policy = retire ? __xrtNetServerRetireTake(s) : NULL;
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	if (policy) policy->Release(s);
}
void __xrtNetServerBindingRelease(const void* data)
{
	__xrt_net_server_child* c = (__xrt_net_server_child*)data;
	xrtownershipscope scope = {0};if (!xrtOwnershipMutationBegin(&scope)) abort();
	int32 left = xrtRefRelease(&c->References);if (left < 0) abort();xnetserver* parent = NULL;
	if (!left) {if (c->BindingHeld || c->TailPending || !c->ParentHeld) abort();c->ParentHeld = false;parent = c->Server;}
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	if (parent) xrtNetServerDestroy(parent); /* enclosing storage can now disappear */
}
static bool __xrtNetServerBusy(const void* data)
{
	const xnetserver* s = data;if (!s) return false;
	if (xrtAtomic32Load(&s->OwnershipActive, XMEMORY_ACQUIRE)) return true;
	if (xrtAtomic32Load(&s->OwnershipStop, XMEMORY_ACQUIRE)) return true;
	if (xrtAtomic32Load(&s->TailCommands, XMEMORY_ACQUIRE)) return true;
	if (xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_SERVER_READY) return false;
	return xrtAtomic32Load(&s->Retiring, XMEMORY_ACQUIRE) || xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->Engine);
}
static bool __xrtNetServerReady(const void* data)
{
	const xnetserver* s = data;return (xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_SERVER_READY) &&
		!xrtAtomic32Load(&s->OwnershipActive, XMEMORY_ACQUIRE);
}
static bool __xrtNetServerCount(const void* data, size_t* count)
{
	const xnetserver* s = data;
	if (!s || !count || __xrtNetServerBusy(s) || (xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_SERVER_CLEARED) ||
		s->Data || s->Events.Accept || s->Events.Error || s->Events.Close || s->RawStreamEvents || !s->StartDone ||
		s->StartedListeners > s->ListenerCount) return false;
	size_t bindings = 0, listeners = 0;
	for (size_t i = 0; i < s->ListenerCount; ++i) {
		const __xrt_net_server_child* c = &s->Children[i];
		if (c->BindingHeld) {
			if (!c->ParentHeld || c->Cleared || c->Server != s || __xrtAtomicRefLoad(&c->References) <= 0) return false;
			++bindings;
		}
		if (c->Listener) {
			if (c->BindingHeld) {
				if (c->Listener->EventPolicy != xrtNetServerListenerEventPolicyV1Get() || xrtNetListenerData(c->Listener) != c) return false;
			} else if (!c->Closed || c->Listener->EventPolicy || xrtNetListenerData(c->Listener)) return false;
			++listeners;
		}
	}
	if (bindings != xrtAtomic32Load(&s->Bindings, XMEMORY_ACQUIRE)) return false;
	size_t queued = xrtAtomic32Load(&s->QueuedAccepts, XMEMORY_ACQUIRE), seen = 0;const xnetstream* tail = NULL;
	for (const xnetstream* stream = s->AcceptHead; stream; stream = stream->AcceptNext) {
		if (++seen > queued || __xrtAtomicRefLoad(&stream->References) <= 0) return false;
		tail = stream;
	}
	if (seen != queued || tail != s->AcceptTail) return false;
	#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
		if (!__xrtNetServerWaitOwnershipWalk(s, NULL, NULL)) return false;
	#endif
	if (__xrtNetServerReady(s)) {
		if (s->RuntimeHeld || s->EngineHeld || s->RuntimeOwner.Data || s->RuntimeOwner.Previous || bindings || listeners || queued ||
			xrtNetServerState(s) != XNET_SERVER_CLOSED) return false;
	} else if (!s->EngineHeld || !s->RuntimeHeld || s->RuntimeOwner.Data != s || !s->RuntimeOwner.Previous ||
		s->RuntimeOwner.Engine != s->Engine || s->RuntimeOwner.Policy != xrtNetServerRuntimeOwnershipV1Get()) return false;
	int32 refs = __xrtAtomicRefLoad(&s->References);if (refs <= 0) return false;*count = (size_t)refs;return true;
}
static bool __xrtNetServerTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const xnetserver* s = data;size_t count;if (!visit || !__xrtNetServerCount(s, &count)) return false;
	if (s->EngineHeld && !visit(xrtNetEngineOwnership(s->Engine), context)) return false;
	for (size_t i = 0; i < s->ListenerCount; ++i)
		if (s->Children[i].Listener && !visit(xrtNetListenerOwnership(s->Children[i].Listener), context)) return false;
	for (const xnetstream* stream = s->AcceptHead; stream; stream = stream->AcceptNext)
		if (!visit(xrtNetStreamOwnership(stream), context)) return false;
	#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)
		if (!__xrtNetServerWaitOwnershipWalk(s, visit, context)) return false;
	#endif
	xrtownershipref error = xrtErrorOwnership(s->StartupError);return !error.Data || visit(error, context);
}
const xrtownershipops __xrtNetServerOps = {__xrtNetServerCount, __xrtNetServerTrace};
static bool __xrtNetServerBindingBusy(const void* data)
{const __xrt_net_server_child* c = data;return c && c->ParentHeld && __xrtNetServerBusy(c->Server);}
static bool __xrtNetServerBindingCount(const void* data, size_t* count)
{
	const __xrt_net_server_child* c = data;
	if (!c || !count || c->Cleared || !c->ParentHeld || !c->Server || __xrtNetServerBindingBusy(c)) return false;
	int32 refs = __xrtAtomicRefLoad(&c->References);if (refs <= 0) return false;*count = (size_t)refs;return true;
}
static bool __xrtNetServerBindingTrace(const void* data, xrtownershipvisitor visit, ptr context)
{const __xrt_net_server_child* c = data;size_t count;return visit && __xrtNetServerBindingCount(c, &count) && visit(xrtNetServerOwnership(c->Server), context);}
const xrtownershipops __xrtNetServerBindingOps = {__xrtNetServerBindingCount, __xrtNetServerBindingTrace};
static void __xrtNetServerRuntimeRelease(ptr data) {xrtNetServerDestroy(data);}
XRT_API const xnetruntimeownershipv1* xrtNetServerRuntimeOwnershipV1Get(void)
{static const xnetruntimeownershipv1 policy = {sizeof(policy), &__xrtNetServerOps, __xrtNetServerRuntimeRelease, NULL, 0};return &policy;}
XRT_API xrtownershipref xrtNetServerOwnership(const xnetserver* s)
{return (xrtownershipref){s, s ? &__xrtNetServerOps : NULL};}
XRT_API const xrtownershipquiescencev1* xrtNetServerOwnershipQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy = {sizeof(policy), &__xrtNetServerOps, __xrtNetServerBusy};return &policy;}
XRT_API const xrtownershipquiescencev1* xrtNetServerBindingOwnershipQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy = {sizeof(policy), &__xrtNetServerBindingOps, __xrtNetServerBindingBusy};return &policy;}
static bool __xrtNetServerHold(const void* data) {return xrtNetServerRef((xnetserver*)data) != NULL;}
static void __xrtNetServerDrop(const void* data) {xrtNetServerDestroy((xnetserver*)data);}
static bool __xrtNetServerClaim(const void* data, const void* token)
{
	xnetserver* s = (xnetserver*)data;size_t count;ptr claim = xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!token || (claim && claim != token) || !__xrtNetServerCount(s, &count)) return false;
	xrtAtomicPtrStore(&s->OwnershipClaim, (ptr)token, XMEMORY_RELEASE);return true;
}
static void __xrtNetServerRestore(const void* data, const void* token)
{
	xnetserver* s = (xnetserver*)data;
	if (!token || xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_SERVER_CLEARED)) abort();
	xrtAtomicPtrStore(&s->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
/* Borrow the existing Engine runtime slot, not a new hidden strong root.
 * OwnershipStop prevents it from retiring until this command enters Active.
 * Foreign Future completion runs on the Worker, never inside host Prepare. */
static void __xrtNetServerStopTask(xnetworker* worker, ptr data)
{
	xnetserver* s = data;(void)worker;
	if (!s->RuntimeHeld || !__xrtNetServerEnter(s, false)) abort();
	xrtAtomic32Store(&s->OwnershipStop, 0, XMEMORY_RELEASE);
	if (!xrtNetServerClose(s)) abort();
	__xrtNetServerLeave(s); /* last access */
}
static bool __xrtNetServerStop(xnetserver* s)
{
	if (xrtAtomic32Load(&s->OwnershipStop, XMEMORY_ACQUIRE)) return true;
	if (!s->RuntimeHeld || !s->EngineHeld) return false;
	xrtAtomic32Store(&s->OwnershipStop, 1, XMEMORY_RELEASE);
	if (__xrtNetEnginePostInternal(&s->Engine->Workers[0], &s->OwnershipCommand, __xrtNetServerStopTask, s)) return true;
	xrtAtomic32Store(&s->OwnershipStop, 0, XMEMORY_RELEASE);return false;
}
static xrtownershipprepareresult __xrtNetServerPrepare(const void* data, const void* token)
{
	xnetserver* s = (xnetserver*)data;xrtownershipscope scope = {0};
	if (!xrtOwnershipFreezeTryBegin(&scope)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE) != token) abort();
	bool ready = __xrtNetServerReady(s), busy = __xrtNetServerBusy(s);
	bool posted = ready || busy || __xrtNetServerStop(s);
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	if (ready) return XRT_OWNERSHIP_PREPARE_READY;
	if (!posted) return XRT_OWNERSHIP_PREPARE_FAILED;
	return XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetServerClear(const void* data, const void* token)
{
	xnetserver* s = (xnetserver*)data;
	if (!token || xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE) != token || !__xrtNetServerReady(s) ||
		(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_SERVER_CLEARED)) abort();
	xrtAtomic32Store(&s->OwnershipState, XRT_NET_SERVER_READY | XRT_NET_SERVER_CLEARED, XMEMORY_RELEASE);
}
static bool __xrtNetServerBindingHold(const void* data)
{
	__xrt_net_server_child* c = (__xrt_net_server_child*)data;xrtownershipscope scope = {0};
	if (!xrtOwnershipMutationBegin(&scope)) return false;
	bool held = !c->Cleared && c->ParentHeld && xrtRefRetain(&c->References) >= 0;
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	return held;
}
static bool __xrtNetServerBindingClaim(const void* data, const void* token)
{
	__xrt_net_server_child* c = (__xrt_net_server_child*)data;size_t count;
	if (!token || (c->Claim && c->Claim != token) || !__xrtNetServerBindingCount(c, &count)) return false;
	c->Claim = token;return true;
}
static void __xrtNetServerBindingRestore(const void* data, const void* token)
{__xrt_net_server_child* c = (__xrt_net_server_child*)data;if (!token || c->Claim != token || c->Cleared) abort();c->Claim = NULL;}
static bool __xrtNetServerBindingReady(const void* data)
{const __xrt_net_server_child* c = data;size_t count;return __xrtNetServerBindingCount(c, &count) && !c->BindingHeld && !c->TailPending && __xrtNetServerReady(c->Server);}
static xrtownershipprepareresult __xrtNetServerBindingPrepare(const void* data, const void* token)
{
	const __xrt_net_server_child* c = data;xrtownershipscope scope = {0};
	if (!xrtOwnershipFreezeTryBegin(&scope)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || c->Claim != token || c->Cleared) abort();
	bool ready = __xrtNetServerBindingReady(c);
	if (!xrtOwnershipScopeEnd(&scope)) abort();
	return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetServerBindingClear(const void* data, const void* token)
{
	__xrt_net_server_child* c = (__xrt_net_server_child*)data;
	if (!token || c->Claim != token || c->Cleared || !__xrtNetServerBindingReady(c)) abort();
	/* Counted parent keeps this INTERIOR shell allocated until its last pin.
	 * Removing it in Clear would free still-pinned enclosing storage. */
	c->Cleared = true;
}
XRT_API const xrtownershipadapterv1* xrtNetServerOwnershipAdapterV1(xrtownershipref ref, const xrtownershippreparationv1** prep)
{
	static const xrtownershipadapterv1 server = {sizeof(server), __xrtNetServerHold, __xrtNetServerDrop, __xrtNetServerClaim, __xrtNetServerRestore, NULL, __xrtNetServerClear, NULL};
	static const xrtownershippreparationv1 serverPrep = {sizeof(serverPrep), &server, __xrtNetServerReady, __xrtNetServerPrepare};
	static const xrtownershipadapterv1 binding = {sizeof(binding), __xrtNetServerBindingHold, __xrtNetServerBindingRelease, __xrtNetServerBindingClaim, __xrtNetServerBindingRestore, NULL, __xrtNetServerBindingClear, NULL};
	static const xrtownershippreparationv1 bindingPrep = {sizeof(bindingPrep), &binding, __xrtNetServerBindingReady, __xrtNetServerBindingPrepare};
	size_t count;if (!prep) return NULL;
	if (ref.Ops == &__xrtNetServerOps && __xrtNetServerCount(ref.Data, &count)) {*prep = &serverPrep;return &server;}
	if (ref.Ops == &__xrtNetServerBindingOps && __xrtNetServerBindingCount(ref.Data, &count)) {*prep = &bindingPrep;return &binding;}
	return NULL;
}
#endif
