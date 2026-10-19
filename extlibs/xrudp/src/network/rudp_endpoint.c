#include <xrudp/endpoint.h>
#include <xrt/atomic.h>
#include <xrt/time.h>
#include <xrt/random.h>
#include <xrt/crypto.h>
#include <xrt/memory.h>
#include <string.h>
#if defined(XRUDP_FEATURE_RUDP_ENDPOINT_FUTURE)
#include <xrudp/future.h>
#include <xrt/sync.h>
#endif

typedef struct xrudp__asyncstate xrudp__asyncstate;

struct xrudppeer {
	xatomic32 References, State;
	xrudpendpoint* Endpoint;
	xnetaddr Address;
	xrudphandshake* Handshake;
	xrudpcipher* Cipher; /* Owned until Session admission; then borrowed for final abort CLOSE only. */
	xrudpsession* Session;
	xrudpsessionstate Snapshot;
	xrudphandshakeinfo Info;
	uint64_t NextPoll, SessionPoll, NotifyAt;
	size_t Slot, RuntimeBytes, PendingSize;
	uint32_t PendingEvents;
	bool FinalQueued, Notifying, HadSession, OutputWaiting, OutputAttempted;
	unsigned char Pending[XRUDP_HANDSHAKE_PACKET_MAX];
	xrudppeer* AsyncCloseNext;
	xrudpresult AsyncCloseReason;
};

typedef struct xrudp__addressrate {
	xrudpaddress Address;
	uint64_t Second;
	uint32_t Count;
	bool Used;
} xrudp__addressrate;

typedef struct xrudp__discoveryjob {
	xrudpdiscovery Core;
	xnetaddr Server;
	xrudpdiscoveryproc Callback;
	void* Context;
	xerror* Error;
	uint64_t Id, NotifyAt;
	bool Used;
} xrudp__discoveryjob;
typedef struct xrudp__punchjob {
	xrudppunch Core;
	xrudppunchproc Callback;
	void* Context;
	xerror* Error;
	uint64_t Id, NotifyAt;
	bool Used;
} xrudp__punchjob;

#define XRUDP__CHECK_PACKET 72u
typedef struct xrudp__checkoutput {
	xnetaddr Address;
	unsigned char Packet[XRUDP__CHECK_PACKET];
	bool Used;
} xrudp__checkoutput;
typedef struct xrudp__check {
	xrudpcheckstate State;
	xnetaddr Candidates[XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX];
	xrudp__checkoutput Pending[XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX];
	unsigned char Key[32], Nonce[16];
	uint64_t Until, SendUntil, Next, Due[XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX], RateSecond;
	uint32_t Count, Cursor, OutputCursor, RateCount;
	uint8_t Attempts[XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX], Direction;
	bool Started;
} xrudp__check;

struct xrudpendpoint {
	xatomic32 References, State;
	xatomic64 Reserved;
	xnetworker* Worker;
	xnetengine* Engine;
	xnetudp* Udp; /* Stable owned handle; CLOSED UDP no longer holds Engine. */
	xrudpendpointconfig Config;
	xrudpendpointevents Events;
	void* Context;
	size_t Allocation, PeerRuntime;
	uint64_t Timer, TimerDue, LastNow, RateSecond;
	double Epoch;
	uint32_t RateCount, Peers, HalfOpen, ServiceCursor, WaitingPeers;
	bool UdpClosed, CloseNotified;
	xrudpresult CloseReason;
	xerror* LocalError;
	xrudp__asyncstate* Async;
	unsigned char Private[32], Public[32], Scope[16], Pin[32], Application[32], CookieKey[32];
	xrudpendpointinfo Stats;
	xrudp__addressrate Rates[XRUDP_ENDPOINT_ADDRESS_BUCKETS];
	xrudp__discoveryjob Discoveries[XRUDP_ENDPOINT_DISCOVERIES_MAX];
	uint64_t DiscoveryId, DiscoveryAt;
	uint32_t DiscoveryCount, DiscoveryCursor;
	bool DiscoveryNotifying, DiscoveryFirst;
	xrudp__punchjob Punches[XRUDP_ENDPOINT_PUNCHES_MAX];
	uint64_t PunchId;
	uint32_t PunchCount, PunchCursor;
	bool PunchNotifying, PunchFirst, CheckFirst;
	xrudp__check Check;
	xrudppeer* Slots[];
};

static void xrudp__endpoint_schedule(xrudpendpoint* e);
static void xrudp__endpoint_finish(xrudpendpoint* e, const xerror* error);
#if defined(XRUDP_FEATURE_RUDP_ENDPOINT_FUTURE)
static size_t xrudp__async_budget(const xrudpendpointconfig* c);
static xrudpresult xrudp__async_init(xrudpendpoint* e);
static void xrudp__async_destroy(xrudpendpoint* e);
static void xrudp__async_stop(xrudpendpoint* e);
static bool xrudp__async_peer_closed(xrudppeer* p, xrudpresult reason);
static void xrudp__async_pump(xrudpendpoint* e);
static uint64_t xrudp__async_due(xrudpendpoint* e);
static bool xrudp__async_idle(xrudpendpoint* e);
#else
static size_t xrudp__async_budget(const xrudpendpointconfig* c) { return c->AsyncOperations || c->AsyncBytes ? SIZE_MAX:0; }
static xrudpresult xrudp__async_init(xrudpendpoint* e) { (void)e; return XRUDP_OK; }
static void xrudp__async_destroy(xrudpendpoint* e) { (void)e; }
static void xrudp__async_stop(xrudpendpoint* e) { (void)e; }
static bool xrudp__async_peer_closed(xrudppeer* p, xrudpresult r) { (void)p; (void)r; return false; }
static void xrudp__async_pump(xrudpendpoint* e) { (void)e; }
static uint64_t xrudp__async_due(xrudpendpoint* e) { (void)e; return UINT64_MAX; }
static bool xrudp__async_idle(xrudpendpoint* e) { (void)e; return true; }
#endif
static void xrudp__peer_waiting(xrudppeer* p, bool waiting)
{
	if (p->OutputWaiting == waiting) return;
	p->OutputWaiting = waiting;
	if (waiting) ++p->Endpoint->WaitingPeers; else --p->Endpoint->WaitingPeers;
}
static uint32_t xrudp__endpoint_waiter(const xrudpendpoint* e)
{
	uint32_t i = e->ServiceCursor;
	if (e->WaitingPeers != 0) for (uint32_t n = 0; n < e->Config.MaxPeers; ++n) {
		if (e->Slots[i] != NULL && e->Slots[i]->OutputWaiting) return i;
		if (++i == e->Config.MaxPeers) i = 0;
	}
	return e->Config.MaxPeers;
}

static bool xrudp__endpoint_nonzero(const void* data, size_t size)
{
	const unsigned char* p = (const unsigned char*)data; unsigned char bits = 0;
	if (p == NULL) return false;
	for (size_t i = 0; i < size; ++i) bits |= p[i];
	return bits != 0;
}
static bool xrudp__endpoint_owner(const xrudpendpoint* e)
{
	return e != NULL && xrtRudpEndpointState(e) != XRUDP_ENDPOINT_CLOSED && xrtNetWorkerIsCurrent(e->Worker);
}
static uint64_t xrudp__endpoint_now(xrudpendpoint* e)
{
	double elapsed = xrtTimer() - e->Epoch;
	uint64_t now = elapsed >= 0.0 && elapsed < (double)UINT64_MAX / 1000000.0 ? (uint64_t)(elapsed * 1000000.0) : e->LastNow;
	if (now > e->LastNow) e->LastNow = now;
	return e->LastNow;
}
static bool xrudp__endpoint_address(const xnetaddr* source, xrudpaddress* output)
{
	if (source == NULL || source->Port == 0 || (source->Family != XNET_FAMILY_IPV4 && source->Family != XNET_FAMILY_IPV6) ||
		xrtNetAddrIsUnspecified(source) || xrtNetAddrIsMulticast(source) || xrtNetAddrIsMapped(source)) return false;
	if (source->Family == XNET_FAMILY_IPV4 && (source->Scope != 0 || source->Address[0] == 0 || source->Address[0] >= 224 ||
		xrudp__endpoint_nonzero(source->Address + 4, 12))) return false;
	if (source->Family == XNET_FAMILY_IPV6 && xrtNetAddrIsLinkLocal(source) && source->Scope == 0) return false;
	memset(output, 0, sizeof(*output)); output->Family = (uint8_t)source->Family;
	output->Port = source->Port; output->Zone = source->Scope; memcpy(output->Address, source->Address, 16);
	return true;
}
xrudpendpoint* xrtRudpEndpointRef(xrudpendpoint* e)
{
	if (e != NULL) (void)xrtAtomic32FetchAdd(&e->References, 1, XMEMORY_RELAXED);
	return e;
}
void xrtRudpEndpointDestroy(xrudpendpoint* e)
{
	if (e != NULL && xrtAtomic32FetchSub(&e->References, 1, XMEMORY_ACQ_REL) == 1) {
		xrudp__async_destroy(e);
		xrtNetUdpDestroy(e->Udp); xrtErrorFree(e->LocalError);
		size_t size = e->Allocation; xrtSecureZero(e, size); xrtFree(e);
	}
}
xrudppeer* xrtRudpPeerRef(xrudppeer* p)
{
	if (p != NULL) (void)xrtAtomic32FetchAdd(&p->References, 1, XMEMORY_RELAXED);
	return p;
}
void xrtRudpPeerDestroy(xrudppeer* p)
{
	if (p != NULL && xrtAtomic32FetchSub(&p->References, 1, XMEMORY_ACQ_REL) == 1) {
		xrudpendpoint* e = p->Endpoint;
		(void)xrtAtomic64FetchSub(&e->Reserved, sizeof(*p), XMEMORY_ACQ_REL);
		xrtSecureZero(p, sizeof(*p)); xrtFree(p); xrtRudpEndpointDestroy(e);
	}
}
xrudpendpointstate xrtRudpEndpointState(const xrudpendpoint* e)
{
	return e == NULL ? XRUDP_ENDPOINT_CLOSED : (xrudpendpointstate)xrtAtomic32Load(&e->State, XMEMORY_ACQUIRE);
}
xrudppeerstate xrtRudpPeerState(const xrudppeer* p)
{
	return p == NULL ? XRUDP_PEER_CLOSED : (xrudppeerstate)xrtAtomic32Load(&p->State, XMEMORY_ACQUIRE);
}
static void xrudp__peer_close(xrudppeer* p, xrudpresult reason)
{
	xrudpendpoint* e = p->Endpoint; xrudppeerstate previous = xrtRudpPeerState(p);
	if (previous == XRUDP_PEER_CLOSED) return;
	xrudp__peer_waiting(p, false);
	e->Slots[p->Slot] = NULL; --e->Peers; if (previous == XRUDP_PEER_NEGOTIATING) --e->HalfOpen;
	xrtRudpHandshakeDestroy(p->Handshake); p->Handshake = NULL;
	if (p->Session != NULL) {
		(void)xrtRudpSessionAbort(p->Session, reason); (void)xrtRudpSessionState(p->Session, &p->Snapshot);
		xrtRudpSessionDestroy(p->Session); p->Session = NULL;
	} else xrtRudpCipherDestroy(p->Cipher);
	p->Cipher = NULL;
	(void)xrtAtomic64FetchSub(&e->Reserved, p->RuntimeBytes, XMEMORY_ACQ_REL); p->RuntimeBytes = 0;
	p->PendingSize = 0; p->PendingEvents = 0;
	/* Publish the closed snapshot only after all mutable session work. */
	xrtAtomic32Store(&p->State, XRUDP_PEER_CLOSED, XMEMORY_RELEASE);
	if (!xrudp__async_peer_closed(p, reason) && e->Events.PeerClose != NULL) e->Events.PeerClose(e, p, reason, e->Context);
	xrtRudpPeerDestroy(p); /* Slot ownership; caller must hold its own temporary reference. */
}
static xrudppeer* xrudp__endpoint_find(xrudpendpoint* e, const unsigned char id[16])
{
	for (size_t i = 0; i < e->Config.MaxPeers; ++i)
		if (e->Slots[i] != NULL && memcmp(e->Slots[i]->Info.LocalId, id, 16) == 0) return e->Slots[i];
	return NULL;
}
static bool xrudp__endpoint_reserve(xrudpendpoint* e, size_t* slot)
{
	size_t cost = sizeof(xrudppeer) + e->PeerRuntime;
	if (e->Peers >= e->Config.MaxPeers || e->HalfOpen >= e->Config.MaxHalfOpen ||
		cost > e->Config.MemoryLimit - (size_t)xrtAtomic64Load(&e->Reserved, XMEMORY_ACQUIRE)) return false;
	for (*slot = 0; *slot < e->Config.MaxPeers && e->Slots[*slot] != NULL; ++*slot) {}
	if (*slot == e->Config.MaxPeers) return false;
	(void)xrtAtomic64FetchAdd(&e->Reserved, cost, XMEMORY_ACQ_REL); return true;
}
static void xrudp__endpoint_unreserve(xrudpendpoint* e)
{
	(void)xrtAtomic64FetchSub(&e->Reserved, sizeof(xrudppeer) + e->PeerRuntime, XMEMORY_ACQ_REL);
}
static xrudppeer* xrudp__endpoint_peer(xrudpendpoint* e, size_t slot, const xnetaddr* address, xrudphandshake* handshake)
{
	xrudppeer* p = (xrudppeer*)xrtMalloc(sizeof(*p));
	if (p == NULL) return NULL;
	memset(p, 0, sizeof(*p)); xrtAtomic32Init(&p->References, 1); xrtAtomic32Init(&p->State, XRUDP_PEER_NEGOTIATING);
	p->Endpoint = xrtRudpEndpointRef(e); p->Address = *address; p->Handshake = handshake; p->Slot = slot;
	p->RuntimeBytes = e->PeerRuntime; p->SessionPoll = UINT64_MAX; (void)xrtRudpHandshakeInfo(handshake, &p->Info);
	p->NextPoll = e->LastNow; e->Slots[slot] = p; ++e->Peers; ++e->HalfOpen; return p;
}
static bool xrudp__endpoint_rate(xrudpendpoint* e, const xrudpaddress* address, uint64_t now)
{
	uint64_t second = now / 1000000;
	if (e->RateSecond != second) { e->RateSecond = second; e->RateCount = 0; }
	if (e->RateCount >= e->Config.HandshakesPerSecond) return false;
	size_t slot = XRUDP_ENDPOINT_ADDRESS_BUCKETS, reuse = XRUDP_ENDPOINT_ADDRESS_BUCKETS;
	for (size_t i = 0; i < XRUDP_ENDPOINT_ADDRESS_BUCKETS; ++i) {
		xrudp__addressrate* r = &e->Rates[i];
		if (r->Used && r->Address.Family == address->Family && r->Address.Zone == address->Zone &&
			memcmp(r->Address.Address, address->Address, 16) == 0) { slot = i; break; }
		if (!r->Used || r->Second < second) reuse = i;
	}
	if (slot == XRUDP_ENDPOINT_ADDRESS_BUCKETS) {
		if (reuse == XRUDP_ENDPOINT_ADDRESS_BUCKETS) return false;
		slot = reuse; memset(&e->Rates[slot], 0, sizeof(e->Rates[slot])); e->Rates[slot].Used = true;
		e->Rates[slot].Address = *address;
	}
	xrudp__addressrate* r = &e->Rates[slot];
	if (r->Second != second) { r->Second = second; r->Count = 0; }
	if (r->Count >= e->Config.AddressHandshakesPerSecond) return false;
	++r->Count; ++e->RateCount; return true;
}
static xnetresult xrudp__endpoint_send(xrudpendpoint* e, const xnetaddr* address, const void* data, size_t size)
{
	xnetresult result = xrtNetUdpSendTo(e->Udp, address, data, size);
	if (result == XNET_RESULT_OK) ++e->Stats.Sent;
	else if (result != XNET_RESULT_AGAIN && xrtRudpEndpointState(e) == XRUDP_ENDPOINT_OPEN) {
		if (xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY) ++e->Stats.CapacityDropped;
		else ++e->Stats.IoErrors;
	}
	return result;
}
static bool xrudp__check_equal(const void* a,const void* b,size_t n)
{
	const unsigned char* x=a;const unsigned char* y=b;volatile unsigned char difference=0;
	for(size_t i=0;i<n;++i)difference|=x[i]^y[i];
	return difference==0;
}
static void xrudp__check_stop(xrudpendpoint* e,xrudpresult reason)
{
	xrudp__check* c=&e->Check;
	c->State.Active=false;c->State.Reason=reason;c->Next=UINT64_MAX;
	xrtSecureZero(c->Key,sizeof(c->Key));xrtSecureZero(c->Nonce,sizeof(c->Nonce));
	xrtSecureZero(c->Pending,sizeof(c->Pending));
}
static bool xrudp__check_init(xrudpendpoint* e)
{
	unsigned char shared[32],info[120]={0};bool low=memcmp(e->Public,e->Pin,32)<0;
	memcpy(info,"xrudp.connectivity.v1",21);
	memcpy(info+24,low?e->Public:e->Pin,32);memcpy(info+56,low?e->Pin:e->Public,32);
	memcpy(info+88,e->Application,32);
	bool ok=memcmp(e->Public,e->Pin,32)!=0 && xrtX25519Shared(e->Private,e->Pin,shared) &&
		xrtHkdfSha256(e->Scope,16,shared,32,info,sizeof(info),e->Check.Key,32) &&
		xrtSecureRandom(e->Check.Nonce,16) && xrudp__endpoint_nonzero(e->Check.Nonce,16);
	xrtSecureZero(shared,sizeof(shared));xrtSecureZero(info,sizeof(info));
	e->Check.Direction=low?0u:1u;e->Check.Next=UINT64_MAX;return ok;
}
static bool xrudp__check_cookie(xrudpendpoint* e,const xnetaddr* source,const void* nonce,unsigned char out[16])
{
	unsigned char input[43]={0},mac[32];
	input[0]=(unsigned char)source->Family;input[1]=(unsigned char)(source->Port>>8);input[2]=(unsigned char)source->Port;
	for(unsigned i=0;i<4;++i)input[3+i]=(unsigned char)(source->Scope>>(24u-i*8u));
	memcpy(input+7,source->Address,16);memcpy(input+23,nonce,16);memcpy(input+39,"XRC1",4);
	bool ok=xrtHmacSha256(e->CookieKey,32,input,sizeof(input),mac);
	if(ok)memcpy(out,mac,16);
	xrtSecureZero(mac,sizeof(mac));return ok;
}
static void xrudp__check_observe(xnetaddr* list,uint32_t* count,const xnetaddr* address)
{
	for(uint32_t i=0;i<*count;++i)if(xrtNetAddrEqual(&list[i],address))return;
	if(*count<XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX)list[(*count)++]=*address;
}
static void xrudp__check_queue(xrudpendpoint* e,const xnetaddr* target,unsigned type,const void* nonce,const void* cookie,uint64_t now)
{
	xrudp__check* c=&e->Check;uint32_t slot=XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX;++i) {
		xrudp__checkoutput* p=&c->Pending[i];
		if(p->Used && p->Packet[4]==type && xrtNetAddrEqual(&p->Address,target) && xrudp__check_equal(p->Packet+8,nonce,16))return;
		if(!p->Used)slot=i;
	}
	if(slot==XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX)return;
	xrudp__checkoutput* p=&c->Pending[slot];memset(p,0,sizeof(*p));p->Address=*target;
	memcpy(p->Packet,"XRC1",4);p->Packet[4]=(unsigned char)type;p->Packet[5]=c->Direction;
	memcpy(p->Packet+8,nonce,16);if(cookie)memcpy(p->Packet+24,cookie,16);
	if(!xrtHmacSha256(c->Key,32,p->Packet,40,p->Packet+40)) {xrtSecureZero(p,sizeof(*p));return;}
	p->Used=true;c->Next=now;
}
static void xrudp__check_input(xrudpendpoint* e,const xnetudpmessage* message,uint64_t now)
{
	xrudp__check* c=&e->Check;const unsigned char* p=message->Data;unsigned char mac[32],cookie[16];xrudpaddress address;
	if(!c->State.Active)return;
	if(now>=c->Until) {xrudp__check_stop(e,c->State.Replies||c->State.Confirmations?XRUDP_DONE:XRUDP_TIMEOUT);return;}
	uint64_t second=now/UINT64_C(1000000);
	if(c->RateSecond!=second) {c->RateSecond=second;c->RateCount=0;}
	if(c->RateCount>=64u) {if(c->State.Rejected!=UINT32_MAX)++c->State.Rejected;return;}
	++c->RateCount;
	bool ok=message->Size==XRUDP__CHECK_PACKET && p[4]>=1 && p[4]<=3 && p[5]==(c->Direction^1u) &&
		!p[6] && !p[7] && xrudp__endpoint_nonzero(p+8,16) && xrudp__endpoint_address(&message->Remote,&address) &&
		xrtHmacSha256(c->Key,32,p,40,mac) && xrudp__check_equal(mac,p+40,32);
	xrtSecureZero(mac,sizeof(mac));
	if(!ok) {if(c->State.Rejected!=UINT32_MAX)++c->State.Rejected;return;}
	if(p[4]==1) {
		if(xrudp__endpoint_nonzero(p+24,16) || !xrudp__check_cookie(e,&message->Remote,p+8,cookie))return;
		xrudp__check_queue(e,&message->Remote,2,p+8,cookie,now);
	} else if(p[4]==2) {
        if(!c->Started || !c->State.Sent || !xrudp__check_equal(p+8,c->Nonce,16) || !xrudp__endpoint_nonzero(p+24,16)) {
            if(c->State.Rejected!=UINT32_MAX)++c->State.Rejected;
            return;
        }
		xrudp__check_observe(c->State.Reply,&c->State.Replies,&message->Remote);
		xrudp__check_queue(e,&message->Remote,3,p+8,p+24,now);
	} else {
        if(!xrudp__check_cookie(e,&message->Remote,p+8,cookie) || !xrudp__check_equal(cookie,p+24,16)) {
            if(c->State.Rejected!=UINT32_MAX)++c->State.Rejected;
            xrtSecureZero(cookie,sizeof(cookie));return;
        }
		xrudp__check_observe(c->State.Confirmed,&c->State.Confirmations,&message->Remote);
	}
	xrtSecureZero(cookie,sizeof(cookie));
}
static bool xrudp__check_pump(xrudpendpoint* e,uint64_t now,bool allowSend)
{
	xrudp__check* c=&e->Check;if(!c->State.Active)return false;
	if(now>=c->Until) {xrudp__check_stop(e,c->State.Replies||c->State.Confirmations?XRUDP_DONE:XRUDP_TIMEOUT);return false;}
	c->Next=c->Until;
	uint32_t pending=XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX,candidate=c->Count;
	for(uint32_t n=0;n<XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX;++n) {
		uint32_t i=(c->OutputCursor+n)%XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX;
		if(c->Pending[i].Used) {pending=i;break;}
	}
	for(uint32_t n=0;n<c->Count && now<c->SendUntil;++n) {
		uint32_t i=(c->Cursor+n)%c->Count;
		if(c->Attempts[i]<5) {
			if(c->Due[i]<c->Next)c->Next=c->Due[i];
			if(candidate==c->Count && c->Due[i]<=now)candidate=i;
		}
	}
	if(pending!=XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX)c->Next=now;
	if(c->Next<=now && (!allowSend || now<e->DiscoveryAt)) {c->Next=e->DiscoveryAt>now?e->DiscoveryAt:now+1000u;return false;}
	if(c->Next>now)return false;
	unsigned char packet[XRUDP__CHECK_PACKET]={0};const void* bytes=packet;const xnetaddr* target;
	if(pending!=XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX) {bytes=c->Pending[pending].Packet;target=&c->Pending[pending].Address;}
	else if(candidate<c->Count) {
		memcpy(packet,"XRC1",4);packet[4]=1;packet[5]=c->Direction;memcpy(packet+8,c->Nonce,16);
		if(!xrtHmacSha256(c->Key,32,packet,40,packet+40)) {xrudp__check_stop(e,XRUDP_IO_ERROR);return false;}
		target=&c->Candidates[candidate];
	} else return false;
	xnetresult sent=xrudp__endpoint_send(e,target,bytes,sizeof(packet));
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return false;
	if(sent==XNET_RESULT_OK) {
		++c->State.Sent;e->DiscoveryAt=now+50000u;c->Next=e->DiscoveryAt;
		if(pending!=XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX) {xrtSecureZero(&c->Pending[pending],sizeof(c->Pending[pending]));c->OutputCursor=(pending+1u)%XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX;}
		else {++c->Attempts[candidate];c->Due[candidate]=now+(UINT64_C(100000)<<c->Attempts[candidate]);c->Cursor=(candidate+1u)%c->Count;}
		return true;
	}
	/* Hard route errors only retire that destination; they cannot tear down
	 * unrelated candidates or the pinned listener. Queue pressure retains it. */
	c->Next=now+1000u;e->DiscoveryAt=c->Next;
	if(sent!=XNET_RESULT_AGAIN && !(xrtGetError()!=NULL && xrtErrorKind(xrtGetError())==XERR_MEMORY)) {
		if(pending!=XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX)xrtSecureZero(&c->Pending[pending],sizeof(c->Pending[pending]));
		else c->Attempts[candidate]=5;
	}
	return false;
}
static uint64_t xrudp__discovery_after(uint64_t now,uint64_t delay)
{ return now>UINT64_MAX-delay?UINT64_MAX:now+delay; }
static void xrudp__endpoint_discovery_end(xrudp__discoveryjob* j,xrudpresult reason,uint64_t now,const xerror* error)
{
	if(j->Core.State.Reason==XRUDP_OK) {
		(void)xrtRudpDiscoveryCancel(&j->Core);j->Core.State.Reason=reason;
		if(j->Error==NULL && error!=NULL)j->Error=xrtErrorRef((xerror*)error);
	}
	j->NotifyAt=xrudp__discovery_after(now,1000);
}
static void xrudp__endpoint_discovery_notify(xrudpendpoint* e,uint64_t now,bool closing)
{
	if(e->DiscoveryNotifying)return;
	e->DiscoveryNotifying=true;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_DISCOVERIES_MAX;++i) {
		xrudp__discoveryjob* j=&e->Discoveries[i];
		if(e->UdpClosed)closing=true;
		if(!j->Used || j->Core.State.Reason==XRUDP_OK || (!closing && now<j->NotifyAt))continue;
		xrudpdiscoverystate result=j->Core.State;xrudpdiscoveryproc callback=j->Callback;void* context=j->Context;
		uint64_t id=j->Id;xerror* error=j->Error;
		xrtSecureZero(j,sizeof(*j));--e->DiscoveryCount;
		if(result.Reason==XRUDP_DONE)++e->Stats.DiscoverySucceeded;else ++e->Stats.DiscoveryFailed;
		callback(e,id,&result,error,context);xrtErrorFree(error);
	}
	e->DiscoveryNotifying=false;
}
static bool xrudp__endpoint_discovery_pump(xrudpendpoint* e,uint64_t now,bool allowSend)
{
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return false;
	bool attempted=false,admitted=false;uint32_t first=e->DiscoveryCursor;
	for(uint32_t n=0;n<XRUDP_ENDPOINT_DISCOVERIES_MAX;++n) {
		if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)break;
		uint32_t i=(first+n)%XRUDP_ENDPOINT_DISCOVERIES_MAX;xrudp__discoveryjob* j=&e->Discoveries[i];
		if(!j->Used || j->Core.State.Reason!=XRUDP_OK)continue;
		unsigned char packet[28];size_t size;uint64_t due;
		xrudpresult r=xrtRudpDiscoveryNext(&j->Core,now,packet,sizeof(packet),&size,&due);
		if(j->Core.State.Reason!=XRUDP_OK) { j->NotifyAt=xrudp__discovery_after(now,1000);continue; }
		if(r!=XRUDP_OK || attempted || !allowSend || now<e->DiscoveryAt)continue;
		attempted=true;xnetresult sent=xrudp__endpoint_send(e,&j->Server,packet,size);
		/* A concurrent close request owns cancellation of pending tasks. A
		 * rejected send observed after that request must not replace it with
		 * an unrelated route error before the asynchronous close callback. */
		if(sent!=XNET_RESULT_OK && xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)break;
		if(sent==XNET_RESULT_OK) {
			admitted=true;
			if(xrtRudpDiscoverySent(&j->Core,now)!=XRUDP_OK)xrudp__endpoint_discovery_end(j,XRUDP_IO_ERROR,now,NULL);
			e->DiscoveryAt=xrudp__discovery_after(now,50000);e->DiscoveryCursor=(i+1)%XRUDP_ENDPOINT_DISCOVERIES_MAX;
		} else if(sent==XNET_RESULT_AGAIN || (sent==XNET_RESULT_ERROR && xrtGetError()!=NULL && xrtErrorKind(xrtGetError())==XERR_MEMORY)) {
			++e->Stats.DiscoveryBackpressure;e->DiscoveryAt=xrudp__discovery_after(now,1000);
		} else xrudp__endpoint_discovery_end(j,XRUDP_IO_ERROR,now,xrtGetError());
	}
	xrudp__endpoint_discovery_notify(e,now,false);
	return admitted;
}
static void xrudp__endpoint_discovery_input(xrudpendpoint* e,const xnetudpmessage* message,const xrudpaddress* source,uint64_t now)
{
	for(uint32_t i=0;i<XRUDP_ENDPOINT_DISCOVERIES_MAX;++i) {
		xrudp__discoveryjob* j=&e->Discoveries[i];if(!j->Used || j->Core.State.Reason!=XRUDP_OK)continue;
		(void)xrtRudpDiscoveryInput(&j->Core,source,message->Data,message->Size,now);
		if(j->Core.State.Reason!=XRUDP_OK)j->NotifyAt=xrudp__discovery_after(now,1000);
	}
}
static void xrudp__endpoint_punch_end(xrudp__punchjob* j,xrudpresult reason,uint64_t now,const xerror* error)
{
	if(j->Core.State.Reason==XRUDP_OK) {
		(void)xrtRudpPunchCancel(&j->Core);j->Core.State.Reason=reason;
		for(uint32_t i=0;i<j->Core.State.Count;++i)if(j->Core.State.Candidates[i].Reason==XRUDP_CANCELLED)j->Core.State.Candidates[i].Reason=reason;
		if(j->Error==NULL && error!=NULL)j->Error=xrtErrorRef((xerror*)error);
	}
	j->NotifyAt=xrudp__discovery_after(now,1000);
}
static void xrudp__endpoint_punch_notify(xrudpendpoint* e,uint64_t now,bool closing)
{
	if(e->PunchNotifying)return;
	e->PunchNotifying=true;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_PUNCHES_MAX;++i) {
		xrudp__punchjob* j=&e->Punches[i];if(e->UdpClosed)closing=true;
		if(!j->Used || j->Core.State.Reason==XRUDP_OK || (!closing && now<j->NotifyAt))continue;
		xrudppunchstate result=j->Core.State;xrudppunchproc callback=j->Callback;void* context=j->Context;
		uint64_t id=j->Id;xerror* error=j->Error;xrtSecureZero(j,sizeof(*j));--e->PunchCount;
		if(result.Reason==XRUDP_DONE)++e->Stats.PunchCompleted;else ++e->Stats.PunchFailed;
		callback(e,id,&result,error,context);xrtErrorFree(error);
	}
	e->PunchNotifying=false;
}
static bool xrudp__endpoint_punch_pump(xrudpendpoint* e,uint64_t now,bool allowSend)
{
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return false;
	bool attempted=false,admitted=false;uint32_t first=e->PunchCursor;
	for(uint32_t n=0;n<XRUDP_ENDPOINT_PUNCHES_MAX;++n) {
		if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)break;
		uint32_t i=(first+n)%XRUDP_ENDPOINT_PUNCHES_MAX;xrudp__punchjob* j=&e->Punches[i];
		if(!j->Used || j->Core.State.Reason!=XRUDP_OK)continue;
		unsigned char packet[28];size_t size;uint64_t due;xrudpaddress address;uint32_t index;
		xrudpresult r=xrtRudpPunchNext(&j->Core,now,packet,sizeof(packet),&size,&address,&index,&due);
		if(j->Core.State.Reason!=XRUDP_OK) {j->NotifyAt=xrudp__discovery_after(now,1000);continue;}
		if(r!=XRUDP_OK || attempted || !allowSend || now<e->DiscoveryAt)continue;
		xnetaddr target;memset(&target,0,sizeof(target));target.Family=(xnetfamily)address.Family;
		target.Port=address.Port;target.Scope=address.Zone;memcpy(target.Address,address.Address,16);
		attempted=true;xnetresult sent=xrudp__endpoint_send(e,&target,packet,size);
		if(sent!=XNET_RESULT_OK && xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)break;
		if(sent==XNET_RESULT_OK) {
			admitted=true;(void)xrtRudpPunchSent(&j->Core,index,now);
			e->DiscoveryAt=xrudp__discovery_after(now,50000);e->PunchCursor=(i+1)%XRUDP_ENDPOINT_PUNCHES_MAX;
		} else if(sent==XNET_RESULT_AGAIN || (sent==XNET_RESULT_ERROR && xrtGetError()!=NULL && xrtErrorKind(xrtGetError())==XERR_MEMORY)) {
			++e->Stats.PunchBackpressure;e->DiscoveryAt=xrudp__discovery_after(now,1000);
		} else {
			if(j->Error==NULL && xrtGetError()!=NULL)j->Error=xrtErrorRef(xrtGetError());
			(void)xrtRudpPunchReject(&j->Core,index,now);
		}
		if(j->Core.State.Reason!=XRUDP_OK)j->NotifyAt=xrudp__discovery_after(now,1000);
	}
	xrudp__endpoint_punch_notify(e,now,false);return admitted;
}
static bool xrudp__endpoint_other_aux_pump(xrudpendpoint* e,uint64_t now,bool allowSend)
{
	/* Both auxiliary classes share one physical gate. Alternate their first
	 * chance only on admission; update the clock after user callbacks. */
	bool first=e->PunchFirst,admitted;
	if(first) {
		admitted=xrudp__endpoint_punch_pump(e,now,allowSend);
		if(admitted)e->PunchFirst=false;
		if(xrudp__endpoint_discovery_pump(e,xrudp__endpoint_now(e),allowSend&&!admitted)) {admitted=true;e->PunchFirst=true;}
	} else {
		admitted=xrudp__endpoint_discovery_pump(e,now,allowSend);
		if(admitted)e->PunchFirst=true;
		if(xrudp__endpoint_punch_pump(e,xrudp__endpoint_now(e),allowSend&&!admitted)) {admitted=true;e->PunchFirst=false;}
	}
	return admitted;
}
static bool xrudp__endpoint_aux_pump(xrudpendpoint* e,uint64_t now,bool allowSend)
{
	bool first=e->CheckFirst,admitted=false;
	if(first)admitted=xrudp__check_pump(e,now,allowSend);
	if(xrudp__endpoint_other_aux_pump(e,xrudp__endpoint_now(e),allowSend&&!admitted)) {admitted=true;e->CheckFirst=true;}
	else if(admitted)e->CheckFirst=false;
	if(!first && xrudp__check_pump(e,xrudp__endpoint_now(e),allowSend&&!admitted)) {admitted=true;e->CheckFirst=false;}
	return admitted;
}
static xnetresult xrudp__peer_output_to(xrudppeer* p, const xnetaddr* address, const void* data, size_t size)
{
	xrudpendpoint* e = p->Endpoint;
	p->OutputAttempted = true;
	uint32_t first = xrudp__endpoint_waiter(e);
	if (first != e->Config.MaxPeers && first != p->Slot) {
		xrudp__peer_waiting(p, true); return XNET_RESULT_AGAIN;
	}
	xnetresult result = xrudp__endpoint_send(e, address, data, size);
	xrudp__peer_waiting(p, result == XNET_RESULT_AGAIN || (result == XNET_RESULT_ERROR &&
		xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY));
	/* A peer that consumed shared UDP capacity yields the next service start.
	 * Failed admission must not advance it: retry cadence otherwise phase-locks
	 * the first successful peers to the socket's capacity-replenishment period. */
	if (result == XNET_RESULT_OK) {
		uint32_t next = (uint32_t)p->Slot + 1;
		e->ServiceCursor = next == e->Config.MaxPeers ? 0 : next;
	}
	return result;
}
static xnetresult xrudp__peer_output(xrudppeer* p, const void* data, size_t size)
{
	return xrudp__peer_output_to(p, &p->Address, data, size);
}
static xrudpresult xrudp__peer_send_path(const void* data, size_t size, const xrudpaddress* address, void* context)
{
	xrudppeer* p = (xrudppeer*)context;
	if (xrtRudpEndpointState(p->Endpoint) != XRUDP_ENDPOINT_OPEN) return XRUDP_BACKPRESSURE;
	xnetaddr destination; memset(&destination, 0, sizeof(destination)); destination.Family = (xnetfamily)address->Family;
	destination.Port = address->Port; destination.Scope = address->Zone; memcpy(destination.Address, address->Address, 16);
	xnetresult r = xrudp__peer_output_to(p, &destination, data, size);
	if (r == XNET_RESULT_OK) return XRUDP_OK;
	if (r == XNET_RESULT_AGAIN || xrtRudpEndpointState(p->Endpoint) != XRUDP_ENDPOINT_OPEN) return XRUDP_BACKPRESSURE;
	return xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR;
}
static xrudpresult xrudp__peer_send(const void* data, size_t size, void* context)
{
	xrudppeer* p = (xrudppeer*)context; xrudpendpoint* e = p->Endpoint;
	if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return XRUDP_BACKPRESSURE;
	xnetresult r = xrudp__peer_output(p, data, size);
	if (r == XNET_RESULT_OK) return XRUDP_OK;
	if (r == XNET_RESULT_AGAIN || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return XRUDP_BACKPRESSURE;
	return xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR;
}
static bool xrudp__peer_eof(const xrudpsessionstate* s)
{
	return s->Receive.HasFinal && !s->Receive.Reset && !s->Receive.Stopped && s->Receive.ReadOffset == s->Receive.FinalOffset;
}
static bool xrudp__peer_write_done(const xrudpsessionstate* s)
{
	return s->WriteDone;
}
static void xrudp__peer_capture(xrudppeer* p)
{
	xrudpsessionstate next;
	if (p->Session == NULL || xrtRudpSessionState(p->Session, &next) != XRUDP_OK) return;
	if (next.Path.Validations > p->Snapshot.Path.Validations) {
		p->PendingEvents |= XRUDP_PEER_PATH_CHANGED;
		memset(&p->Address, 0, sizeof(p->Address)); p->Address.Family = (xnetfamily)next.Path.Active.Family;
		p->Address.Port = next.Path.Active.Port; p->Address.Scope = next.Path.Active.Zone; memcpy(p->Address.Address, next.Path.Active.Address, 16);
	}
	if (next.Path.Failures > p->Snapshot.Path.Failures) p->PendingEvents |= XRUDP_PEER_PATH_FAILED;
	if (next.Mtu.Effective != p->Snapshot.Mtu.Effective || next.Mtu.Fallbacks != p->Snapshot.Mtu.Fallbacks) p->PendingEvents |= XRUDP_PEER_MTU_CHANGED;
	if (next.ReadReady && !p->Snapshot.ReadReady) p->PendingEvents |= XRUDP_PEER_READ_READY;
	if (next.Shutdown.PeerRequested && !p->Snapshot.Shutdown.PeerRequested) p->PendingEvents |= XRUDP_PEER_SHUTDOWN_REQUESTED;
	if (next.Transmit.Cancelled || next.Shutdown.Requested) p->PendingEvents &= ~XRUDP_PEER_WRITE_READY;
	else if (next.Transmit.BaseOffset > p->Snapshot.Transmit.BaseOffset && !next.Transmit.FinQueued) p->PendingEvents |= XRUDP_PEER_WRITE_READY;
	if (xrudp__peer_write_done(&next) && !xrudp__peer_write_done(&p->Snapshot)) p->PendingEvents |= XRUDP_PEER_WRITE_DONE;
	if (xrudp__peer_eof(&next) && !xrudp__peer_eof(&p->Snapshot)) p->PendingEvents |= XRUDP_PEER_READ_EOF;
	if (next.ReadDone && !p->Snapshot.ReadDone) p->PendingEvents |= XRUDP_PEER_READ_DONE;
	if (next.OutgoingLimit > p->Snapshot.OutgoingLimit) p->PendingEvents |= XRUDP_PEER_STREAMS_READY;
	if (next.Shutdown.Requested || next.Shutdown.PeerRequested) p->PendingEvents &= ~XRUDP_PEER_STREAMS_READY;
	if (next.WriteStopped && !p->Snapshot.WriteStopped) p->PendingEvents |= XRUDP_PEER_WRITE_STOPPED;
	if (next.Datagrams.ReceiveQueued != 0 && (next.Datagrams.Accepted > p->Snapshot.Datagrams.Accepted ||
		next.Datagrams.ReceiveQueued > p->Snapshot.Datagrams.ReceiveQueued)) p->PendingEvents |= XRUDP_PEER_DATAGRAM_READ_READY;
	if (next.Shutdown.Requested) p->PendingEvents &= ~(XRUDP_PEER_DATAGRAM_READ_READY | XRUDP_PEER_DATAGRAM_WRITE_READY | XRUDP_PEER_STREAMS_READY);
	else if (next.Datagrams.SendQueued < p->Snapshot.Datagrams.SendQueued || next.DatagramSendLimit > p->Snapshot.DatagramSendLimit) p->PendingEvents |= XRUDP_PEER_DATAGRAM_WRITE_READY;
	p->Snapshot = next;
}
static void xrudp__peer_notify(xrudppeer* p)
{
	xrudpendpoint* e = p->Endpoint;
	if (p->Notifying || (p->PendingEvents == 0 && p->Snapshot.PendingStreamEvents == 0) ||
		xrudp__endpoint_now(e) < p->NotifyAt || xrtRudpPeerState(p) != XRUDP_PEER_CONFIRMED ||
		xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	uint32_t events = p->PendingEvents; p->PendingEvents = 0;
	xrudpstreamevent streams[XRUDP_PEER_STREAM_EVENT_BURST]; size_t count = 0;
	/* Snapshot before entering application code. Newly generated hints cannot
	 * recursively enter this batch or make its work unbounded. */
	while (count < XRUDP_PEER_STREAM_EVENT_BURST &&
		xrtRudpSessionStreamNextEvent(p->Session, &streams[count]) == XRUDP_OK) ++count;
	xrudp__peer_capture(p); p->Notifying = true;
	uint64_t now = xrudp__endpoint_now(e); p->NotifyAt = now > UINT64_MAX - 1000 ? UINT64_MAX : now + 1000;
	if (events != 0 && e->Events.Changed != NULL) e->Events.Changed(e, p, events, e->Context);
	for (size_t i = 0; i < count && xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED &&
		xrtRudpEndpointState(e) == XRUDP_ENDPOINT_OPEN; ++i) {
		xrudpstreamstate state;
		if (e->Events.StreamChanged != NULL && xrtRudpSessionStreamState(p->Session, streams[i].Id, &state) == XRUDP_OK) {
			if (state.Transmit.Cancelled || p->Snapshot.Shutdown.Requested) streams[i].Events &= ~XRUDP_STREAM_WRITE_READY;
			if (streams[i].Events != 0) e->Events.StreamChanged(e, p, &streams[i], e->Context);
		}
	}
	p->Notifying = false;
	now = xrudp__endpoint_now(e); p->NotifyAt = now > UINT64_MAX - 1000 ? UINT64_MAX : now + 1000;
}
static void xrudp__peer_pump(xrudppeer* p, uint64_t now)
{
	xrudpendpoint* e = p->Endpoint;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	p->OutputAttempted = false;
	/* A refused packet already passed stream/cwnd admission. Give its owner
	 * the first retry when capacity is observable again, not only at a timer.
	 * Poll still checks pacing/deadlines; no new output means no HOL waiter. */
	if (p->OutputWaiting && p->Session != NULL) p->SessionPoll = now;
	if (p->Handshake != NULL) {
		/* A retained packet waiting for UDP capacity must not bypass the
		 * absolute handshake deadline by skipping HandshakeNext forever. */
		if (xrtRudpPeerState(p) == XRUDP_PEER_NEGOTIATING && now >= p->Info.DeadlineUs) {
			xrudp__peer_close(p, XRUDP_TIMEOUT); return;
		}
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && now >= p->Info.DeadlineUs) {
			xrtRudpHandshakeDestroy(p->Handshake); p->Handshake = NULL; p->NextPoll = UINT64_MAX;
		} else {
			if (p->PendingSize == 0 && now >= p->NextPoll) {
				uint64_t due = p->Info.DeadlineUs;
				xrudpresult result = xrtRudpHandshakeNext(p->Handshake, now, p->Pending, sizeof(p->Pending), &p->PendingSize, &due);
				p->NextPoll = due; (void)xrtRudpHandshakeInfo(p->Handshake, &p->Info);
				if (p->Info.Phase == XRUDP_HANDSHAKE_FAILED || (result != XRUDP_OK && result != XRUDP_TIMEOUT && result != XRUDP_DONE)) {
					xrudp__peer_close(p, p->Info.Phase == XRUDP_HANDSHAKE_FAILED ? p->Info.Failure : result); return;
				}
			}
			/* Allocate the full session before admitting the final server proof
			 * to UDP; Confirmed never exposes a peer lacking receive resources. */
			if (p->Info.Phase == XRUDP_HANDSHAKE_ESTABLISHED && p->Session == NULL) {
				if (xrtRudpHandshakeTakeCipher(p->Handshake, &p->Cipher) != XRUDP_OK) { xrudp__peer_close(p, XRUDP_IO_ERROR); return; }
				(void)xrtRudpHandshakeInfo(p->Handshake, &p->Info);
				xrudpsessionconfig config = e->Config.Session; config.Peer = p->Info.PeerParameters; config.Role = p->Info.Role;
				xrudpresult result = xrtRudpSessionCreate(&config, p->Cipher, now, xrudp__peer_send, p, &p->Session);
				if (result != XRUDP_OK) { if (result == XRUDP_NO_MEMORY) ++e->Stats.CapacityDropped; xrudp__peer_close(p, result); return; }
				xrudpaddress address;
				if (!xrudp__endpoint_address(&p->Address, &address) || xrtRudpSessionEnablePaths(p->Session, &address, xrudp__peer_send_path) != XRUDP_OK) {
					xrudp__peer_close(p, XRUDP_IO_ERROR); return;
				}
				p->HadSession = true; p->SessionPoll = now; (void)xrtRudpSessionState(p->Session, &p->Snapshot);
			}
			if (p->PendingSize != 0) {
				xnetresult sent = xrudp__peer_output(p, p->Pending, p->PendingSize);
				if (sent == XNET_RESULT_OK) {
					if (p->Info.Phase == XRUDP_HANDSHAKE_ESTABLISHED && p->Info.Role == XRUDP_RESPONDER) p->FinalQueued = true;
					p->PendingSize = 0;
				} else if (sent != XNET_RESULT_AGAIN) {
					xrudpresult reason = xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN ? XRUDP_CANCELLED :
						(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR);
					xrudp__peer_close(p, reason); return;
				}
			}
			if (xrtRudpPeerState(p) == XRUDP_PEER_NEGOTIATING && p->Info.Phase == XRUDP_HANDSHAKE_ESTABLISHED &&
				(p->Info.Role == XRUDP_INITIATOR || p->FinalQueued)) {
				--e->HalfOpen;
				xrtAtomic32Store(&p->State, XRUDP_PEER_CONFIRMED, XMEMORY_RELEASE);
				if (e->Events.Confirmed != NULL) e->Events.Confirmed(e, p, e->Context);
				if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
			}
		}
	}
	if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED) {
		if (now >= p->SessionPoll) {
			xrudpresult result = xrtRudpSessionPoll(p->Session, now, &p->SessionPoll);
			if (result != XRUDP_OK) { xrudp__peer_close(p, result); return; }
			xrudp__peer_capture(p);
		}
		if (!p->OutputAttempted) xrudp__peer_waiting(p, false);
		xrudp__async_pump(e); xrudp__peer_notify(p);
	} else if (!p->OutputAttempted) xrudp__peer_waiting(p, false);
}
static void xrudp__endpoint_input_pump(xrudppeer* p, uint64_t now, bool resume)
{
	xrudpendpoint* e = p->Endpoint;
	uint32_t first = resume ? xrudp__endpoint_waiter(e) : e->Config.MaxPeers;
	if (first != e->Config.MaxPeers && e->Slots[first] != p) {
		/* One bounded earlier waiter before the newly active input peer. This
		 * prevents ACK/data receive callbacks from bypassing the shared queue. */
		xrudppeer* q = xrtRudpPeerRef(e->Slots[first]); xrudp__peer_pump(q, now); xrtRudpPeerDestroy(q);
		now = xrudp__endpoint_now(e); /* Earlier callbacks may have queued work. */
	}
	xrudp__peer_pump(p, now);
}
static void xrudp__endpoint_timer(xnetworker* worker, uint64 id, xnetresult result, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)worker;
	if (e->Timer == id) { e->Timer = 0; e->TimerDue = 0; }
	if (result == XNET_RESULT_OK && xrtRudpEndpointState(e) == XRUDP_ENDPOINT_OPEN) {
		uint64_t now = xrudp__endpoint_now(e);
		/* A tiny auxiliary class must not repeatedly steal the only shared
		 * queue slot, nor wait forever behind continuous peer output. Alternate
		 * first opportunity after actual admission; pacing still limits mapping. */
		bool discoveryFirst=e->DiscoveryFirst;
		if(discoveryFirst && xrudp__endpoint_aux_pump(e,now,true))e->DiscoveryFirst=false;
		now=xrudp__endpoint_now(e); /* Auxiliary callbacks can mutate live peers. */
		uint64_t peerSent=e->Stats.Sent;
		uint32_t i = e->ServiceCursor;
		for (uint32_t n = 0; n < e->Config.MaxPeers; ++n) {
			if (e->Slots[i] != NULL) {
				xrudppeer* p = xrtRudpPeerRef(e->Slots[i]); xrudp__peer_pump(p, now); xrtRudpPeerDestroy(p);
			}
			if (++i == e->Config.MaxPeers) i = 0;
		}
		if(!discoveryFirst) {
			bool peerAdmitted=e->Stats.Sent!=peerSent;
			if(peerAdmitted)e->DiscoveryFirst=true;
			if(xrudp__endpoint_aux_pump(e,xrudp__endpoint_now(e),!peerAdmitted))e->DiscoveryFirst=false;
		}
		xrudp__async_pump(e); xrudp__endpoint_schedule(e);
	}
	xrudp__endpoint_finish(e, NULL); xrtRudpEndpointDestroy(e);
}
static void xrudp__endpoint_schedule(xrudpendpoint* e)
{
	if ((e->Peers == 0 && e->DiscoveryCount == 0 && e->PunchCount == 0 && !e->Check.State.Active) || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	uint64_t now = xrudp__endpoint_now(e), due = UINT64_MAX;
	if(e->Check.State.Active && e->Check.Next<due)due=e->Check.Next;
	uint64_t asyncDue = xrudp__async_due(e); if (asyncDue < due) due = asyncDue;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_DISCOVERIES_MAX;++i)if(e->Discoveries[i].Used) {
		xrudp__discoveryjob* j=&e->Discoveries[i];uint64_t next=j->NotifyAt;
		if(j->Core.State.Reason==XRUDP_OK) {
			next=j->Core.State.NextUs;
			if(next<=now && next<e->DiscoveryAt)next=e->DiscoveryAt;
			if(j->Core.State.DeadlineUs<next)next=j->Core.State.DeadlineUs;
		}
		if(next<due)due=next;
	}
	for(uint32_t i=0;i<XRUDP_ENDPOINT_PUNCHES_MAX;++i)if(e->Punches[i].Used) {
		xrudp__punchjob* j=&e->Punches[i];uint64_t next=j->NotifyAt;
		if(j->Core.State.Reason==XRUDP_OK) {
			next=j->Core.State.NextUs;
			if(next<=now && next<e->DiscoveryAt)next=e->DiscoveryAt;
			if(j->Core.State.DeadlineUs<next)next=j->Core.State.DeadlineUs;
		}
		if(next<due)due=next;
	}
	for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
		xrudppeer* p = e->Slots[i]; uint64_t next = p->PendingSize ? now : p->NextPoll;
		if (xrtRudpPeerState(p) == XRUDP_PEER_NEGOTIATING && p->Info.DeadlineUs < next) next = p->Info.DeadlineUs;
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && p->SessionPoll < next) next = p->SessionPoll;
		if (p->PendingEvents != 0 || p->Snapshot.PendingStreamEvents != 0) {
			uint64_t notify = p->NotifyAt > now ? p->NotifyAt : now; if (notify < next) next = notify;
		}
		if (next < due) due = next;
	}
	uint64_t milliseconds = due > now ? (due - now) / 1000 + ((due - now) % 1000 != 0) : 1;
	if (milliseconds == 0) milliseconds = 1;
	if (milliseconds > 1000) milliseconds = 1000;
	due = now > UINT64_MAX - milliseconds * 1000 ? UINT64_MAX : now + milliseconds * 1000;
	if (e->Timer != 0) {
		if (due >= e->TimerDue) return;
		if (!xrtNetEngineTimerCancelCurrent(e->Engine, e->Timer)) return; /* Already queued: it will run next. */
		if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	}
	xrtRudpEndpointRef(e);
	e->TimerDue = due;
	e->Timer = xrtNetEngineAfter(e->Engine, xrtNetWorkerIndex(e->Worker), (int64)milliseconds, xrudp__endpoint_timer, e);
	if (e->Timer == 0) {
		e->LocalError = xrtErrorRef(xrtGetError()); ++e->Stats.IoErrors;
		xrtRudpEndpointDestroy(e); (void)xrtRudpEndpointClose(e);
	}
}
static void xrudp__endpoint_receive(xnetudp* udp, const xnetudpmessage* message, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)udp;
	if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	++e->Stats.Received; xrudpaddress address; xrudpwirepacket packet;
	if ((message->Flags & XNET_UDP_MESSAGE_TRUNCATED) != 0 || !xrudp__endpoint_address(&message->Remote, &address)) { ++e->Stats.Invalid;return; }
	uint64_t now = xrudp__endpoint_now(e);
	if(message->Size>=4 && !memcmp(message->Data,"XRC1",4)) {
		xrudp__check_input(e,message,now);xrudp__endpoint_schedule(e);return;
	}
	if(message->Size>=20 && (message->Data[0]&0xc0)==0 && message->Data[4]==0x21 && message->Data[5]==0x12 && message->Data[6]==0xa4 && message->Data[7]==0x42) {
		xrudp__endpoint_discovery_input(e,message,&address,now);xrudp__endpoint_schedule(e);return;
	}
	if(xrtRudpPacketDecode(message->Data,message->Size,&packet)!=XRUDP_OK) {++e->Stats.Invalid;return;}
	xrudppeer* p = xrudp__endpoint_find(e, packet.Header.ConnectionId);
	if (p != NULL) {
		bool active = xrtNetAddrEqual(&p->Address, &message->Remote);
		bool encrypted = packet.Header.Type == XRUDP_PACKET_DATA || packet.Header.Type == XRUDP_PACKET_PATH;
		if (!active && (xrtRudpPeerState(p) != XRUDP_PEER_CONFIRMED || !encrypted || packet.Header.PacketNumber < 2)) { ++e->Stats.AddressDropped; return; }
		xrtRudpPeerRef(p);
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && encrypted && packet.Header.PacketNumber >= 2) {
			xrudpresult result = xrtRudpSessionPathInput(p->Session, &address, message->Data, message->Size, now);
			if (!active && result != XRUDP_OK) ++e->Stats.AddressDropped;
			if (result == XRUDP_AUTH_FAILED) ++e->Stats.Invalid;
			else if (result != XRUDP_OK && result != XRUDP_DUPLICATE && result != XRUDP_TOO_OLD && result != XRUDP_BACKPRESSURE) xrudp__peer_close(p, result);
			if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED) {
				xrudp__peer_capture(p); p->SessionPoll = now; xrudp__endpoint_input_pump(p, now, result == XRUDP_OK);
			}
		} else if (p->Handshake != NULL) {
			xrudpresult result = xrtRudpHandshakeInput(p->Handshake, message->Data, message->Size, now);
			(void)xrtRudpHandshakeInfo(p->Handshake, &p->Info);
			if (p->Info.Phase == XRUDP_HANDSHAKE_FAILED) xrudp__peer_close(p, p->Info.Failure);
			else if (result == XRUDP_NO_MEMORY || result == XRUDP_IO_ERROR) xrudp__peer_close(p, result);
			else {
				if (result == XRUDP_OK) { p->PendingSize = 0; p->NextPoll = now; }
				if (result == XRUDP_AUTH_FAILED) ++e->Stats.Invalid;
				xrudp__endpoint_input_pump(p, now, result == XRUDP_OK);
			}
		}
		xrtRudpPeerDestroy(p); xrudp__endpoint_schedule(e); return;
	}
	if (!e->Config.Listen) { ++e->Stats.Invalid; return; }
	if (!xrudp__endpoint_rate(e, &address, now)) { ++e->Stats.RateDropped; return; }
	if (packet.Header.Type == XRUDP_PACKET_HELLO) {
		unsigned char response[XRUDP_HANDSHAKE_PACKET_MAX]; size_t size;
		xrudpresult result = xrtRudpHandshakeRetry(e->CookieKey, e->Scope, e->Public, &address, now,
			message->Data, message->Size, response, sizeof(response), &size);
		if (result == XRUDP_OK) (void)xrudp__endpoint_send(e, &message->Remote, response, size);
		else ++e->Stats.Invalid;
		return;
	}
	if (packet.Header.Type != XRUDP_PACKET_HANDSHAKE) { ++e->Stats.Invalid; return; }
	size_t slot;
	if (!xrudp__endpoint_reserve(e, &slot)) { ++e->Stats.CapacityDropped; return; }
	xrudphandshakeconfig c = {0}; c.StaticPrivate = e->Private; c.LocalPublic = e->Public; c.Scope = e->Scope;
	c.PeerPublic = e->Config.PeerPublic; c.ApplicationContext = e->Config.ApplicationContext;
	c.Trust = e->Config.Trust; c.TrustContext = e->Config.TrustContext; c.Parameters = e->Config.Session.Local; c.TimeoutUs = e->Config.HandshakeTimeoutUs;
	xrudphandshake* h = NULL;
	xrudpresult result = xrtRudpHandshakeAccept(&c, e->CookieKey, NULL, &address, now, message->Data, message->Size, &h);
	if (result != XRUDP_OK) {
		xrudp__endpoint_unreserve(e);
		if (result == XRUDP_NO_MEMORY) ++e->Stats.CapacityDropped;
		else if (result == XRUDP_IO_ERROR) ++e->Stats.IoErrors;
		else ++e->Stats.Invalid;
		return;
	}
	p = xrudp__endpoint_peer(e, slot, &message->Remote, h);
	if (p == NULL) { xrtRudpHandshakeDestroy(h); xrudp__endpoint_unreserve(e); ++e->Stats.CapacityDropped; return; }
	xrtRudpPeerRef(p); xrudp__peer_pump(p, now); xrtRudpPeerDestroy(p); xrudp__endpoint_schedule(e);
}
static void xrudp__endpoint_open(xnetudp* udp, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)udp;
	uint32 expected = XRUDP_ENDPOINT_OPENING;
	if (!xrtAtomic32CompareExchange(&e->State, &expected, XRUDP_ENDPOINT_OPEN, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
		(void)xrtNetUdpAbort(e->Udp); return;
	}
	if(e->Config.ConnectivityChecks) {
		e->Check.State.Active=true;e->Check.State.Reason=XRUDP_OK;
		e->Check.Until=xrudp__endpoint_now(e)+UINT64_C(10000000);e->Check.Next=e->Check.Until;
		e->Check.State.DeadlineUs=e->Check.Until;
	}
	if (e->Events.Open != NULL) e->Events.Open(e, e->Context);
	xrudp__endpoint_schedule(e);
}
static void xrudp__endpoint_error(xnetudp* udp, const xerror* error, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)udp; (void)error; ++e->Stats.IoErrors;
}
static void xrudp__endpoint_finish(xrudpendpoint* e, const xerror* error)
{
	if (!e->UdpClosed || e->Timer != 0 || e->CloseNotified || e->DiscoveryNotifying || e->DiscoveryCount || e->PunchNotifying || e->PunchCount) return;
	if (!xrudp__async_idle(e)) return;
	e->CloseNotified = true; xrudp__check_stop(e,XRUDP_CANCELLED); xrtSecureZero(e->Private, 32); xrtSecureZero(e->CookieKey, 32);
	xrtAtomic32Store(&e->State, XRUDP_ENDPOINT_CLOSED, XMEMORY_RELEASE);
	if (error == NULL) error = e->LocalError != NULL ? e->LocalError : xrtNetUdpError(e->Udp);
	if (e->Events.Close != NULL) e->Events.Close(e, e->CloseReason, error, e->Context);
	xrtRudpEndpointDestroy(e); /* Internal I/O lifetime. */
}
static void xrudp__endpoint_closed(xnetudp* udp, xnetresult result, const xerror* error, ptr context)
{
	xrudpendpoint* e = xrtRudpEndpointRef((xrudpendpoint*)context); (void)udp;
	if (e == NULL) return;
	xrudpendpointstate previous = xrtRudpEndpointState(e);
	xrtAtomic32Store(&e->State, XRUDP_ENDPOINT_CLOSING, XMEMORY_RELEASE);
	e->CloseReason = e->LocalError != NULL ? (xrtErrorKind(e->LocalError) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR) :
		(previous == XRUDP_ENDPOINT_CLOSING ? XRUDP_CANCELLED : (result == XNET_RESULT_ERROR ? XRUDP_IO_ERROR : XRUDP_CLOSED));
	xrudp__async_stop(e); xrudp__async_pump(e);
	for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
		xrudppeer* p = xrtRudpPeerRef(e->Slots[i]); xrudp__peer_close(p, e->CloseReason); xrtRudpPeerDestroy(p);
	}
	e->UdpClosed = true;
	uint64_t now=xrudp__endpoint_now(e);
	for(uint32_t i=0;i<XRUDP_ENDPOINT_DISCOVERIES_MAX;++i)if(e->Discoveries[i].Used)
		xrudp__endpoint_discovery_end(&e->Discoveries[i],e->CloseReason,now,error);
	xrudp__endpoint_discovery_notify(e,now,true);
	for(uint32_t i=0;i<XRUDP_ENDPOINT_PUNCHES_MAX;++i)if(e->Punches[i].Used)
		xrudp__endpoint_punch_end(&e->Punches[i],e->CloseReason,now,error);
	xrudp__endpoint_punch_notify(e,now,true);
	if (e->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(e->Engine, e->Timer);
	xrudp__endpoint_finish(e, error); xrtRudpEndpointDestroy(e);
}
void xrtRudpEndpointConfigInit(xrudpendpointconfig* c)
{
	if (c == NULL) return;
	memset(c, 0, sizeof(*c)); xrtRudpSessionConfigInit(&c->Session);
	c->Session.DiscoverMtu = true;
	c->MaxPeers = 64; c->MaxHalfOpen = 16; c->MemoryLimit = 16u * 1024u * 1024u;
	c->HandshakesPerSecond = 128; c->AddressHandshakesPerSecond = 8;
}
xrudpresult xrtRudpEndpointBind(xnetworker* worker, const xnetaddr* local,
	const xrudpendpointconfig* config, const xrudpendpointevents* events, void* context, xrudpendpoint** output)
{
	size_t sessionBytes; xrudpsessionconfig session;
	if (config == NULL) return XRUDP_ARGUMENT;
	session = config->Session; session.Peer = session.Local;
	if (worker == NULL || !xrtNetWorkerIsCurrent(worker) || local == NULL || config == NULL || output == NULL ||
		(local->Family != XNET_FAMILY_IPV4 && local->Family != XNET_FAMILY_IPV6) ||
		!xrudp__endpoint_nonzero(config->StaticPrivate, 32) || (config->Scope != NULL && !xrudp__endpoint_nonzero(config->Scope, 16)) ||
		(config->PeerPublic != NULL && !xrudp__endpoint_nonzero(config->PeerPublic, 32)) ||
		(config->Listen && config->PeerPublic == NULL && config->Trust == NULL) || config->MaxPeers == 0 || config->MaxPeers > XRUDP_ENDPOINT_PEERS_MAX ||
		(config->ConnectivityChecks && (config->PeerPublic==NULL || config->ApplicationContext==NULL)) ||
		config->MaxHalfOpen == 0 || config->MaxHalfOpen > config->MaxPeers || config->HandshakesPerSecond == 0 || config->HandshakesPerSecond > 65536 ||
		config->AddressHandshakesPerSecond == 0 || config->AddressHandshakesPerSecond > config->HandshakesPerSecond ||
		config->MemoryLimit > 1024u * 1024u * 1024u || (config->HandshakeTimeoutUs != 0 && (config->HandshakeTimeoutUs < 1000000 || config->HandshakeTimeoutUs > XRUDP_HANDSHAKE_MAX_TIMEOUT_US)) ||
		xrtRudpSessionAllocationSize(&session, &sessionBytes) != XRUDP_OK) return XRUDP_ARGUMENT;
	size_t size = sizeof(xrudpendpoint) + config->MaxPeers * sizeof(xrudppeer*);
	size_t runtime = xrtRudpHandshakeAllocationSize() + sessionBytes - xrtRudpCipherAllocationSize();
	size_t asyncBytes = xrudp__async_budget(config);
	if (asyncBytes == SIZE_MAX || config->MemoryLimit < size + sizeof(xrudppeer) + runtime + asyncBytes) return XRUDP_ARGUMENT;
	xrudpendpoint* e = (xrudpendpoint*)xrtMalloc(size);
	if (e == NULL) return XRUDP_NO_MEMORY;
	memset(e, 0, size); e->Allocation = size; e->Worker = worker; e->Engine = xrtNetWorkerEngine(worker);
	e->Config = *config; e->Config.Session = session; e->PeerRuntime = runtime;
	e->Context = context; e->Epoch = xrtTimer(); e->RateSecond = UINT64_MAX;
	if (events != NULL) e->Events = *events;
	xrtAtomic32Init(&e->References, 2); xrtAtomic32Init(&e->State, XRUDP_ENDPOINT_OPENING); xrtAtomic64Init(&e->Reserved, size + asyncBytes);
	if (xrudp__async_init(e) != XRUDP_OK) { xrtRudpEndpointDestroy(e); xrtRudpEndpointDestroy(e); return XRUDP_NO_MEMORY; }
	memcpy(e->Private, config->StaticPrivate, 32); e->Config.StaticPrivate = e->Private;
	if (config->Scope != NULL) memcpy(e->Scope, config->Scope, 16);
	else if (!xrtSecureRandom(e->Scope, 16)) goto random_failure;
	if (!xrudp__endpoint_nonzero(e->Scope, 16) || !xrtSecureRandom(e->CookieKey, 32) || !xrudp__endpoint_nonzero(e->CookieKey, 32) ||
		!xrtX25519Public(e->Private, e->Public)) goto random_failure;
	e->Config.Scope = e->Scope;
	if (config->PeerPublic != NULL) { memcpy(e->Pin, config->PeerPublic, 32); e->Config.PeerPublic = e->Pin; }
	if (config->ApplicationContext != NULL) { memcpy(e->Application, config->ApplicationContext, 32); e->Config.ApplicationContext = e->Application; }
	if(config->ConnectivityChecks && !xrudp__check_init(e))goto random_failure;
	xnetudpconfig udp; xrtNetUdpConfigInit(&udp); udp.ReceiveSize = XRUDP_PACKET_MAX; udp.ReceiveBatch = 8;
	udp.ReceiveQueueLimit = 0; udp.ReceiveQueueByteLimit = 0; udp.Truncation = XNET_UDP_TRUNCATE_DROP;
	udp.SendLimit = 65536; udp.SendPacketLimit = 64; udp.SendHighWater = 49152; udp.SendLowWater = 16384;
	udp.ReceiveBuffer = udp.SendBuffer = 262144; udp.IPv6Only = true;
	if (session.DiscoverMtu) udp.PathMtu = XNET_PMTU_PROBE;
	xnetudpevents ev; memset(&ev, 0, sizeof(ev)); ev.Open = xrudp__endpoint_open; ev.Receive = xrudp__endpoint_receive;
	ev.Error = xrudp__endpoint_error; ev.Close = xrudp__endpoint_closed;
	e->Udp = xrtNetUdpBind(e->Engine, local, xrtNetWorkerIndex(worker), &udp, &ev, e);
	if (e->Udp == NULL) {
		xrudpresult result = xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR;
		xrtRudpEndpointDestroy(e); xrtRudpEndpointDestroy(e); return result;
	}
	*output = e; return XRUDP_OK;
random_failure:
	xrtRudpEndpointDestroy(e); xrtRudpEndpointDestroy(e); return XRUDP_IO_ERROR;
}
xrudpresult xrtRudpEndpointCheck(xrudpendpoint* e,const xnetaddr* candidates,size_t count,uint32_t timeout)
{
	if(e==NULL || candidates==NULL || !count || count>XRUDP_ENDPOINT_CHECK_CANDIDATES_MAX || !timeout || timeout>10000)return XRUDP_ARGUMENT;
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return XRUDP_CLOSED;
	if(!xrudp__endpoint_owner(e) || !e->Config.ConnectivityChecks)return XRUDP_ARGUMENT;
	xrudp__check* c=&e->Check;uint64_t now=xrudp__endpoint_now(e);
	if(!c->State.Active || now>=c->Until)return XRUDP_TIMEOUT;
	if(c->Started)return XRUDP_DONE;
	xnetaddr local;if(!xrtNetUdpLocal(e->Udp,&local))return XRUDP_IO_ERROR;
	for(size_t i=0;i<count;++i) {
		xrudpaddress a;if(candidates[i].Family!=local.Family || !xrudp__endpoint_address(&candidates[i],&a))return XRUDP_ARGUMENT;
		for(size_t k=0;k<i;++k)if(xrtNetAddrEqual(&candidates[i],&candidates[k]))return XRUDP_ARGUMENT;
	}
	memcpy(c->Candidates,candidates,count*sizeof(*candidates));c->Count=(uint32_t)count;c->Started=true;
	c->SendUntil=now+(uint64_t)timeout*1000u;if(c->SendUntil>c->Until)c->SendUntil=c->Until;
	for(uint32_t i=0;i<c->Count;++i)c->Due[i]=now;
	c->Next=now;xrudp__endpoint_schedule(e);return XRUDP_OK;
}
xrudpresult xrtRudpEndpointCheckInfo(const xrudpendpoint* e,xrudpcheckstate* state)
{
	if(e==NULL || state==NULL || (xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_CLOSED && !xrudp__endpoint_owner(e)))return XRUDP_ARGUMENT;
	*state=e->Check.State;return XRUDP_OK;
}
xrudpresult xrtRudpEndpointCheckCancel(xrudpendpoint* e)
{
	if(e==NULL)return XRUDP_ARGUMENT;
	if(xrtRudpEndpointState(e)==XRUDP_ENDPOINT_CLOSED)return XRUDP_DONE;
	if(!xrudp__endpoint_owner(e))return XRUDP_ARGUMENT;
	if(!e->Check.State.Active)return XRUDP_DONE;
	xrudp__check_stop(e,XRUDP_CANCELLED);xrudp__endpoint_schedule(e);return XRUDP_OK;
}
xrudpresult xrtRudpEndpointConnect(xrudpendpoint* e, const xnetaddr* remote,
	const xrudpconnectconfig* config, xrudppeer** output)
{
	xrudpaddress address;
	if (!xrudp__endpoint_owner(e) || config == NULL || output == NULL || !xrudp__endpoint_address(remote, &address) ||
		!xrudp__endpoint_nonzero(config->PeerPublic, 32) || !xrudp__endpoint_nonzero(config->Scope, 16)) return XRUDP_ARGUMENT;
	if (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return XRUDP_CLOSED;
	xnetaddr local; if (!xrtNetUdpLocal(e->Udp, &local) || remote->Family != local.Family) return XRUDP_ARGUMENT;
	size_t slot; if (!xrudp__endpoint_reserve(e, &slot)) return XRUDP_BACKPRESSURE;
	xrudphandshakeconfig c = {0}; c.StaticPrivate = e->Private; c.PeerPublic = config->PeerPublic; c.Scope = config->Scope;
	c.ApplicationContext = config->ApplicationContext; c.Parameters = e->Config.Session.Local; c.TimeoutUs = e->Config.HandshakeTimeoutUs;
	xrudphandshake* h = NULL;
	xrudpresult result = xrtRudpHandshakeCreate(&c, xrudp__endpoint_now(e), &h);
	if (result != XRUDP_OK) { xrudp__endpoint_unreserve(e); return result; }
	xrudphandshakeinfo info; (void)xrtRudpHandshakeInfo(h, &info);
	if (xrudp__endpoint_find(e, info.LocalId) != NULL) { xrtRudpHandshakeDestroy(h); xrudp__endpoint_unreserve(e); return XRUDP_IO_ERROR; }
	xrudppeer* p = xrudp__endpoint_peer(e, slot, remote, h);
	if (p == NULL) { xrtRudpHandshakeDestroy(h); xrudp__endpoint_unreserve(e); return XRUDP_NO_MEMORY; }
	*output = xrtRudpPeerRef(p); xrtRudpPeerRef(p); xrudp__peer_pump(p, e->LastNow); xrtRudpPeerDestroy(p);
	xrudp__endpoint_schedule(e); return XRUDP_OK;
}
xrudpresult xrtRudpEndpointInfo(const xrudpendpoint* e, xrudpendpointinfo* output)
{
	if (e == NULL || output == NULL || (xrtRudpEndpointState(e) != XRUDP_ENDPOINT_CLOSED && !xrudp__endpoint_owner(e))) return XRUDP_ARGUMENT;
	xrudpendpointinfo info = e->Stats;
	if (!xrtNetUdpLocal(e->Udp, &info.Local)) return XRUDP_IO_ERROR;
	memcpy(info.Public, e->Public, 32); memcpy(info.Scope, e->Scope, 16); info.Peers = e->Peers; info.HalfOpen = e->HalfOpen;
	info.ReservedBytes = (size_t)xrtAtomic64Load(&e->Reserved, XMEMORY_ACQUIRE); info.MemoryLimit = e->Config.MemoryLimit;
	info.Discoveries=e->DiscoveryCount;
	info.Punches=e->PunchCount;
	*output = info; return XRUDP_OK;
}
xrudpresult xrtRudpEndpointDiscover(xrudpendpoint* e,const xnetaddr* server,uint32_t timeout,bool fingerprint,xrudpdiscoveryproc callback,void* context,uint64_t* id)
{
	if(e==NULL || server==NULL || callback==NULL || id==NULL)return XRUDP_ARGUMENT;
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return XRUDP_CLOSED;
	xrudpaddress address;if(!xrudp__endpoint_owner(e) || !xrudp__endpoint_address(server,&address))return XRUDP_ARGUMENT;
	xnetaddr local;if(!xrtNetUdpLocal(e->Udp,&local) || local.Family!=server->Family)return XRUDP_ARGUMENT;
	if(e->DiscoveryId==UINT64_MAX)return XRUDP_LIMIT;
	uint32_t slot=XRUDP_ENDPOINT_DISCOVERIES_MAX;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_DISCOVERIES_MAX;++i)if(!e->Discoveries[i].Used) {slot=i;break;}
	if(slot==XRUDP_ENDPOINT_DISCOVERIES_MAX)return XRUDP_BACKPRESSURE;
	xrudpdiscovery value;uint64_t now=xrudp__endpoint_now(e);
	xrudpresult r=xrtRudpDiscoveryInit(&value,&address,now,timeout,fingerprint);if(r!=XRUDP_OK)return r;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_DISCOVERIES_MAX;++i)if(e->Discoveries[i].Used && e->Discoveries[i].Core.State.Reason==XRUDP_OK &&
		memcmp(e->Discoveries[i].Core._Transaction,value._Transaction,12)==0) {xrtSecureZero(&value,sizeof(value));return XRUDP_IO_ERROR;}
	xrudp__discoveryjob* j=&e->Discoveries[slot];j->Core=value;j->Server=*server;j->Callback=callback;j->Context=context;j->Id=++e->DiscoveryId;j->Used=true;
	++e->DiscoveryCount;++e->Stats.DiscoveryStarted;*id=j->Id;xrtSecureZero(&value,sizeof(value));
	xrudp__endpoint_schedule(e);return XRUDP_OK;
}
xrudpresult xrtRudpEndpointDiscoveryCancel(xrudpendpoint* e,uint64_t id)
{
	if(e==NULL || !id)return XRUDP_ARGUMENT;
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return XRUDP_CLOSED;
	if(!xrudp__endpoint_owner(e))return XRUDP_ARGUMENT;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_DISCOVERIES_MAX;++i) {
		xrudp__discoveryjob* j=&e->Discoveries[i];if(!j->Used || j->Id!=id)continue;
		if(j->Core.State.Reason!=XRUDP_OK)return XRUDP_DONE;
		unsigned char packet[28];size_t n;uint64_t due;uint64_t now=xrudp__endpoint_now(e);
		(void)xrtRudpDiscoveryNext(&j->Core,now,packet,sizeof(packet),&n,&due);
		bool pending=j->Core.State.Reason==XRUDP_OK;
		if(pending)(void)xrtRudpDiscoveryCancel(&j->Core);
		j->NotifyAt=xrudp__discovery_after(now,1000);xrudp__endpoint_schedule(e);return pending?XRUDP_OK:XRUDP_DONE;
	}
	return XRUDP_DONE;
}
xrudpresult xrtRudpEndpointPunch(xrudpendpoint* e,const xnetaddr* candidates,size_t count,uint32_t timeout,xrudppunchproc callback,void* context,uint64_t* id)
{
	if(e==NULL || candidates==NULL || !count || count>XRUDP_PUNCH_CANDIDATES_MAX || callback==NULL || id==NULL)return XRUDP_ARGUMENT;
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return XRUDP_CLOSED;
	if(!xrudp__endpoint_owner(e))return XRUDP_ARGUMENT;
	xnetaddr local;if(!xrtNetUdpLocal(e->Udp,&local))return XRUDP_IO_ERROR;
	xrudpaddress addresses[XRUDP_PUNCH_CANDIDATES_MAX];
	for(size_t i=0;i<count;++i)if(candidates[i].Family!=local.Family || !xrudp__endpoint_address(&candidates[i],&addresses[i]))return XRUDP_ARGUMENT;
	if(e->PunchId==UINT64_MAX)return XRUDP_LIMIT;
	uint32_t slot=XRUDP_ENDPOINT_PUNCHES_MAX;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_PUNCHES_MAX;++i)if(!e->Punches[i].Used) {slot=i;break;}
	if(slot==XRUDP_ENDPOINT_PUNCHES_MAX)return XRUDP_BACKPRESSURE;
	xrudppunch value;uint64_t now=xrudp__endpoint_now(e);
	xrudpresult r=xrtRudpPunchInit(&value,addresses,count,now,timeout);if(r!=XRUDP_OK)return r;
	xrudp__punchjob* j=&e->Punches[slot];j->Core=value;j->Callback=callback;j->Context=context;j->Id=++e->PunchId;j->Used=true;
	++e->PunchCount;++e->Stats.PunchStarted;*id=j->Id;xrtSecureZero(&value,sizeof(value));
	xrudp__endpoint_schedule(e);return XRUDP_OK;
}
xrudpresult xrtRudpEndpointPunchCancel(xrudpendpoint* e,uint64_t id)
{
	if(e==NULL || !id)return XRUDP_ARGUMENT;
	if(xrtRudpEndpointState(e)!=XRUDP_ENDPOINT_OPEN)return XRUDP_CLOSED;
	if(!xrudp__endpoint_owner(e))return XRUDP_ARGUMENT;
	for(uint32_t i=0;i<XRUDP_ENDPOINT_PUNCHES_MAX;++i) {
		xrudp__punchjob* j=&e->Punches[i];if(!j->Used || j->Id!=id)continue;
		if(j->Core.State.Reason!=XRUDP_OK)return XRUDP_DONE;
		unsigned char packet[28];size_t n;uint64_t due;xrudpaddress target;uint32_t index;uint64_t now=xrudp__endpoint_now(e);
		(void)xrtRudpPunchNext(&j->Core,now,packet,sizeof(packet),&n,&target,&index,&due);
		bool pending=j->Core.State.Reason==XRUDP_OK;
		if(pending)(void)xrtRudpPunchCancel(&j->Core);
		j->NotifyAt=xrudp__discovery_after(now,1000);xrudp__endpoint_schedule(e);return pending?XRUDP_OK:XRUDP_DONE;
	}
	return XRUDP_DONE;
}
xrudpresult xrtRudpPeerInfo(const xrudppeer* p, xrudphandshakeinfo* output, xnetaddr* address)
{
	if (p == NULL || output == NULL || address == NULL ||
		(xrtRudpPeerState(p) != XRUDP_PEER_CLOSED && !xrudp__endpoint_owner(p->Endpoint))) return XRUDP_ARGUMENT;
	*output = p->Info; *address = p->Address; return XRUDP_OK;
}
static xrudpresult xrudp__peer_io(const xrudppeer* p)
{
	if (p == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) return XRUDP_CLOSED;
	if (!xrudp__endpoint_owner(p->Endpoint)) return XRUDP_ARGUMENT;
	if (xrtRudpEndpointState(p->Endpoint) != XRUDP_ENDPOINT_OPEN) return XRUDP_CLOSED;
	return xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED ? XRUDP_OK : XRUDP_BACKPRESSURE;
}
static void xrudp__peer_wake(xrudppeer* p)
{
	xrudp__peer_capture(p); p->SessionPoll = xrudp__endpoint_now(p->Endpoint); xrudp__endpoint_schedule(p->Endpoint);
}
xrudpresult xrtRudpPeerShutdown(xrudppeer* p, uint32_t timeout)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionShutdown(p->Session, timeout);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerProbePath(xrudppeer* p, const xnetaddr* address)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrudpaddress candidate;
	if (!xrudp__endpoint_address(address, &candidate) || address->Family != p->Address.Family) return XRUDP_ARGUMENT;
	xrtRudpPeerRef(p); result = xrtRudpSessionProbePath(p->Session, &candidate, xrudp__endpoint_now(p->Endpoint));
	xrudpsessionstate state;
	if (xrtRudpSessionState(p->Session, &state) == XRUDP_OK) {
		if (state.Reason != XRUDP_OK) xrudp__peer_close(p, state.Reason);
		else if (result == XRUDP_OK || state.Path.Failures > p->Snapshot.Path.Failures) xrudp__peer_wake(p);
	}
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerWrite(xrudppeer* p, const void* data, size_t size, size_t* written)
{
	return xrtRudpPeerStreamWrite(p, 1, data, size, written);
}
xrudpresult xrtRudpPeerStreamWrite(xrudppeer* p, uint32_t id, const void* data, size_t size, size_t* written)
{
	if (written != NULL) *written = 0;
	if (written == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamWrite(p->Session, id, data, size, written);
	if (*written != 0) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerRead(xrudppeer* p, void* output, size_t capacity, size_t* read)
{
	return xrtRudpPeerStreamRead(p, 1, output, capacity, read);
}
xrudpresult xrtRudpPeerStreamRead(xrudppeer* p, uint32_t id, void* output, size_t capacity, size_t* read)
{
	if (read != NULL) *read = 0;
	if (read == NULL || output == NULL || capacity == 0) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamRead(p->Session, id, output, capacity, read);
	if (*read != 0) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerMessageWrite(xrudppeer* p, uint32_t id, const void* data, size_t size)
{
	if (data == NULL && size != 0) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionMessageWrite(p->Session, id, data, size);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerMessagePeek(const xrudppeer* p, uint32_t id, size_t* size)
{
	if (size != NULL) *size = 0;
	if (size == NULL) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	return xrtRudpSessionMessagePeek(p->Session, id, size);
}
xrudpresult xrtRudpPeerMessageRead(xrudppeer* p, uint32_t id, void* output, size_t capacity, size_t* size)
{
	if (size != NULL) *size = 0;
	if (size == NULL || (output == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionMessageRead(p->Session, id, output, capacity, size);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerDatagramConfigure(xrudppeer* p, const xrudpdatagramchannel* channel)
{
	if (channel == NULL) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__peer_io(p); if (r != XRUDP_OK) return r;
	r = xrtRudpSessionDatagramConfigure(p->Session, channel); xrudp__peer_capture(p); return r;
}
xrudpresult xrtRudpPeerDatagramChannelState(const xrudppeer* p, uint32_t id, xrudpdatagramchannelstate* output)
{
	if (output == NULL) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__peer_io(p); if (r != XRUDP_OK) return r;
	return xrtRudpSessionDatagramChannelState(p->Session, id, output);
}
static void xrudp__peer_datagram_after(xrudppeer* p)
{
	xrudpsessionstate state;
	if (xrtRudpSessionState(p->Session, &state) == XRUDP_OK && state.Reason != XRUDP_OK) xrudp__peer_close(p, state.Reason);
	else xrudp__peer_wake(p);
}
static xrudpresult xrudp__peer_datagram_write_at(xrudppeer* p, uint32_t id, const void* data, size_t size,
	uint64_t now, uint64_t deadline, uint64_t* sequence)
{
	xrtRudpPeerRef(p);
	xrudpresult r = xrtRudpSessionDatagramWrite(p->Session, id, data, size, now, deadline, sequence);
	xrudp__peer_datagram_after(p); xrtRudpPeerDestroy(p); return r;
}
xrudpresult xrtRudpPeerDatagramWrite(xrudppeer* p, uint32_t id, const void* data, size_t size, uint64_t lifetime, uint64_t* sequence)
{
	if (sequence == NULL || (data == NULL && size != 0) || lifetime > XRUDP_DATAGRAM_LIFETIME_MAX_US) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__peer_io(p); if (r != XRUDP_OK) return r;
	uint64_t now = xrudp__endpoint_now(p->Endpoint);
	uint64_t deadline = lifetime == 0 ? 0 : (now > UINT64_MAX - lifetime ? UINT64_MAX : now + lifetime);
	return xrudp__peer_datagram_write_at(p, id, data, size, now, deadline, sequence);
}
xrudpresult xrtRudpPeerDatagramPeek(const xrudppeer* p, xrudpdatagraminfo* info)
{
	if (info != NULL) memset(info, 0, sizeof(*info));
	if (info == NULL) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__peer_io(p); if (r != XRUDP_OK) return r;
	return xrtRudpSessionDatagramPeek(p->Session, info);
}
xrudpresult xrtRudpPeerDatagramRead(xrudppeer* p, void* output, size_t capacity, xrudpdatagraminfo* info)
{
	if (info != NULL) memset(info, 0, sizeof(*info));
	if (info == NULL || (output == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__peer_io(p); if (r != XRUDP_OK) return r;
	xrtRudpPeerRef(p); r = xrtRudpSessionDatagramRead(p->Session, output, capacity, xrudp__endpoint_now(p->Endpoint), info);
	xrudp__peer_datagram_after(p);
	if (r == XRUDP_OK && xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && p->Snapshot.Datagrams.ReceiveQueued != 0) {
		p->PendingEvents |= XRUDP_PEER_DATAGRAM_READ_READY; xrudp__endpoint_schedule(p->Endpoint);
	}
	xrtRudpPeerDestroy(p); return r;
}
xrudpresult xrtRudpPeerFinish(xrudppeer* p)
{
	return xrtRudpPeerStreamFinish(p, 1);
}
xrudpresult xrtRudpPeerStreamFinish(xrudppeer* p, uint32_t id)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamFinish(p->Session, id);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerStreamOpen(xrudppeer* p, const xrudpstreamconfig* config, uint32_t* id)
{
	if (config == NULL || id == NULL) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamOpen(p->Session, config, id);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerStreamState(const xrudppeer* p, uint32_t id, xrudpstreamstate* output)
{
	if (output == NULL) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	return xrtRudpSessionStreamState(p->Session, id, output);
}
xrudpresult xrtRudpPeerStreamReset(xrudppeer* p, uint32_t id, uint32_t reason)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamReset(p->Session, id, reason);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerStop(xrudppeer* p, uint32_t reason) { return xrtRudpPeerStreamStop(p, 1, reason); }
xrudpresult xrtRudpPeerStreamStop(xrudppeer* p, uint32_t id, uint32_t reason)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamStop(p->Session, id, reason);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerStreamRetire(xrudppeer* p, uint32_t id)
{
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	xrtRudpPeerRef(p); result = xrtRudpSessionStreamRetire(p->Session, id);
	if (result == XRUDP_OK) xrudp__peer_wake(p);
	xrtRudpPeerDestroy(p); return result;
}
xrudpresult xrtRudpPeerSessionState(const xrudppeer* p, xrudpsessionstate* output)
{
	if (p == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) {
		if (!p->HadSession) return XRUDP_CLOSED;
		*output = p->Snapshot; return XRUDP_OK;
	}
	xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
	return xrtRudpSessionState(p->Session, output);
}
xrudpresult xrtRudpPeerAbort(xrudppeer* p)
{
	if (p == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) return XRUDP_DONE;
	if (!xrudp__endpoint_owner(p->Endpoint)) return XRUDP_ARGUMENT;
	xrtRudpPeerRef(p);
	if (p->Cipher != NULL && xrtRudpEndpointState(p->Endpoint) == XRUDP_ENDPOINT_OPEN) {
		unsigned char frames[8] = {XRUDP_FRAME_CLOSE,0,0,4,0,0,0,XRUDP_CANCELLED}, packet[80]; size_t size;
		if (xrtRudpCipherSeal(p->Cipher, frames, sizeof(frames), packet, sizeof(packet), &size) == XRUDP_OK)
			(void)xrudp__peer_output(p, packet, size);
	}
	xrudp__peer_close(p, XRUDP_CANCELLED); xrtRudpPeerDestroy(p); return XRUDP_OK;
}
xrudpresult xrtRudpEndpointClose(xrudpendpoint* e)
{
	if (e == NULL) return XRUDP_ARGUMENT;
	xrtRudpEndpointRef(e); uint32 state = xrtAtomic32Load(&e->State, XMEMORY_ACQUIRE);
	while (state < XRUDP_ENDPOINT_CLOSING && !xrtAtomic32CompareExchange(&e->State, &state, XRUDP_ENDPOINT_CLOSING, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {}
	if (state < XRUDP_ENDPOINT_CLOSED) { xrudp__async_stop(e); (void)xrtNetUdpAbort(e->Udp); }
	xrtRudpEndpointDestroy(e); return XRUDP_OK;
}

#if defined(XRUDP_FEATURE_RUDP_ENDPOINT_FUTURE)
typedef struct xrudp__asyncop {
	struct xrudp__asyncop* Next;
	xrudppeer* Peer;
	xpromise* Promise;
	xcancel* Cancel;
	xcancelwatch* Watch;
	xrudpoperation Request;
	xrudpoperationresult Result;
	size_t Cost, PayloadSize;
	uint64_t Ticket, Deadline, DatagramDeadline;
	bool Linked, Active, Counted;
	unsigned char Payload[];
} xrudp__asyncop;

struct xrudp__asyncstate {
	xmutex Lock;
	xnetpost Post;
	xrudpendpoint* Endpoint; /* Borrowed; every activation/result retains a peer. */
	xrudp__asyncop *Head, *Tail;
	xrudppeer *ClosedHead, *ClosedTail;
	xrudpasyncstate Info;
	uint64_t Ticket;
	bool Pumping; /* Owner only. */
};

static void xrudp__async_add(uint64_t* value) { if (*value != UINT64_MAX) ++*value; }
static void xrudp__async_dispatch(xnetworker* worker, ptr context);
static bool xrudp__async_kick_locked(xrudp__asyncstate* s)
{
	if (xrtNetPostPending(&s->Post)) return true;
	if (xrtRudpEndpointState(s->Endpoint) == XRUDP_ENDPOINT_CLOSED) return false;
	xrtRudpEndpointRef(s->Endpoint);
	if (xrtNetPost(s->Endpoint->Worker, &s->Post, xrudp__async_dispatch, s->Endpoint)) return true;
	xrtRudpEndpointDestroy(s->Endpoint); return false;
}
static size_t xrudp__async_budget(const xrudpendpointconfig* c)
{
	if (!c->AsyncOperations && !c->AsyncBytes) return 0;
	if (!c->AsyncOperations || c->AsyncOperations > XRUDP_ASYNC_OPERATIONS_MAX ||
		c->AsyncBytes < sizeof(xrudp__asyncop) || c->AsyncBytes > XRUDP_ASYNC_BYTES_MAX) return SIZE_MAX;
	return sizeof(xrudp__asyncstate) + c->AsyncBytes;
}
static xrudpresult xrudp__async_init(xrudpendpoint* e)
{
	if (!e->Config.AsyncOperations) return XRUDP_OK;
	xrudp__asyncstate* s = xrtCalloc(1, sizeof(*s));
	if (s == NULL) return XRUDP_NO_MEMORY;
	if (!xrtMutexInit(&s->Lock)) { xrtFree(s); return XRUDP_NO_MEMORY; }
	if (!xrtNetPostInit(&s->Post)) { (void)xrtMutexUnit(&s->Lock); xrtFree(s); return XRUDP_NO_MEMORY; }
	s->Endpoint = e; s->Info.Limit = e->Config.AsyncOperations; s->Info.ByteLimit = e->Config.AsyncBytes;
	e->Async = s; return XRUDP_OK;
}
static void xrudp__async_destroy(xrudpendpoint* e)
{
	xrudp__asyncstate* s = e->Async;
	if (s != NULL) { (void)xrtMutexUnit(&s->Lock); xrtSecureZero(s, sizeof(*s)); xrtFree(s); }
}
static void xrudp__async_remove_locked(xrudp__asyncstate* s, xrudp__asyncop* op)
{
	xrudp__asyncop* previous = NULL;
	for (xrudp__asyncop* p = s->Head; p != NULL; p = p->Next) {
		if (p == op) {
			if (previous != NULL) previous->Next = p->Next; else s->Head = p->Next;
			if (s->Tail == p) s->Tail = previous;
			op->Linked = false; op->Next = NULL; return;
		}
		previous = p;
	}
}
static void xrudp__async_release(xrudp__asyncop* op)
{
	xrudppeer* p = op->Peer; xrudp__asyncstate* s = p->Endpoint->Async; size_t cost = op->Cost;
	xrtErrorFree((xerror*)op->Result.Error);
	xrtSecureZero(op, cost); xrtFree(op);
	/* The peer keeps s alive until after actual erasure/free. Releasing quota
	 * earlier would let concurrent submissions exceed the live byte budget. */
	xrtMutexLock(&s->Lock); --s->Info.Retained; s->Info.RetainedBytes -= cost; xrtMutexUnlock(&s->Lock);
	xrtRudpPeerDestroy(p);
}
static void xrudp__async_result_free(ptr value, ptr context)
{ (void)value; xrudp__async_release((xrudp__asyncop*)context); }
static void xrudp__async_cancel(ptr context)
{
	xrudp__asyncop* op = context; xrudp__asyncstate* s = op->Peer->Endpoint->Async;
	xrtMutexLock(&s->Lock);
	if (op->Linked && !op->Active && !s->Info.Stopping) (void)xrudp__async_kick_locked(s);
	xrtMutexUnlock(&s->Lock);
}
static bool xrudp__async_read(xrudpoperationkind kind)
{ return kind == XRUDP_OPERATION_READ || kind == XRUDP_OPERATION_MESSAGE_READ || kind == XRUDP_OPERATION_DATAGRAM_READ; }
static bool xrudp__async_write(xrudpoperationkind kind)
{ return kind == XRUDP_OPERATION_WRITE || kind == XRUDP_OPERATION_MESSAGE_WRITE || kind == XRUDP_OPERATION_DATAGRAM_WRITE; }
static void xrudp__async_finish_op(xrudp__asyncop* op, bool cancelled)
{
	xrudp__asyncstate* s = op->Peer->Endpoint->Async; xpromise* promise = op->Promise;
	xrtCancelUnwatch(op->Watch); op->Watch = NULL; xrtCancelDestroy(op->Cancel); op->Cancel = NULL; op->Promise = NULL;
	xrtMutexLock(&s->Lock);
	if (op->Counted) { --s->Info.Pending; op->Counted = false; }
	if (cancelled) xrudp__async_add(&s->Info.Cancelled); else xrudp__async_add(&s->Info.Completed);
	xrtMutexUnlock(&s->Lock);
	if (cancelled) { (void)xrtPromiseCancel(promise); xrudp__async_release(op); }
	else {
		if (op->Result.Result == XRUDP_IO_ERROR || op->Result.Result == XRUDP_NO_MEMORY) {
			const xerror* error = op->Peer->Endpoint->LocalError;
			if (error == NULL) error = xrtNetUdpError(op->Peer->Endpoint->Udp);
			if (error != NULL) op->Result.Error = xrtErrorRef(error);
		}
		if (xrudp__async_write(op->Request.Kind)) xrtSecureZero(op->Payload, op->PayloadSize);
		op->Result.Data = xrudp__async_read(op->Request.Kind) && op->Result.Result == XRUDP_OK ? op->Payload : NULL;
		if (!xrtPromiseResolveOwned(promise, &op->Result, xrudp__async_result_free, op)) xrudp__async_release(op);
	}
	xrtPromiseDestroy(promise);
}
void xrtRudpOperationInit(xrudpoperation* op)
{
	if (op != NULL) { memset(op, 0, sizeof(*op)); op->Kind = XRUDP_OPERATION_READ; op->Stream = 1; op->Capacity = 4096; }
}
xrudpresult xrtRudpEndpointAsyncState(const xrudpendpoint* e, xrudpasyncstate* output)
{
	if (e == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(output, 0, sizeof(*output)); xrudp__asyncstate* s = e->Async;
	if (s != NULL) { xrtMutexLock(&s->Lock); *output = s->Info; xrtMutexUnlock(&s->Lock); }
	return XRUDP_OK;
}
xfuture* xrtRudpPeerSubmitAsync(xrudppeer* p, const xrudpoperation* request)
{
	if (p == NULL || request == NULL || request->Kind < XRUDP_OPERATION_READ || request->Kind > XRUDP_OPERATION_WAIT_WRITE_DONE ||
		request->TimeoutMs > 600000u || (request->Kind == XRUDP_OPERATION_READ && !request->Capacity) ||
		(request->Kind == XRUDP_OPERATION_DATAGRAM_WRITE && request->DatagramLifetimeUs > XRUDP_DATAGRAM_LIFETIME_MAX_US) ||
		(xrudp__async_write(request->Kind) && request->Size && request->Data == NULL)) {
		xrtSetErrorKind(XERR_ARGUMENT); return NULL;
	}
	double submitted = xrtTimer();
	xrudp__asyncstate* s = p->Endpoint->Async;
	if (s == NULL) { xrtSetErrorInfo(XERR_UNSUPPORTED, "xrudp-async", XRUDP_LIMIT, "endpoint Future adapter is disabled"); return NULL; }
	size_t payload = xrudp__async_read(request->Kind) ? request->Capacity : (xrudp__async_write(request->Kind) ? request->Size : 0);
	if (payload > XRUDP_ASYNC_BYTES_MAX || payload > SIZE_MAX - sizeof(xrudp__asyncop)) {
		xrtSetErrorKind(XERR_RANGE); return NULL;
	}
	size_t cost = sizeof(xrudp__asyncop) + payload;
	xrtMutexLock(&s->Lock);
	if (s->Info.Stopping || xrtRudpEndpointState(p->Endpoint) >= XRUDP_ENDPOINT_CLOSING || xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) {
		xrtMutexUnlock(&s->Lock); xrtSetErrorKind(XERR_CLOSED); return NULL;
	}
	if (cost > s->Info.ByteLimit) {
		xrudp__async_add(&s->Info.Rejected); xrtMutexUnlock(&s->Lock);
		xrtSetErrorInfo(XERR_RANGE, "xrudp-async", XRUDP_LIMIT, "operation exceeds the endpoint byte limit even when empty"); return NULL;
	}
	if (s->Ticket == UINT64_MAX || s->Info.Retained == s->Info.Limit || cost > s->Info.ByteLimit - s->Info.RetainedBytes) {
		xrudp__async_add(&s->Info.Rejected); xrtMutexUnlock(&s->Lock);
		xrtSetErrorInfo(XERR_AGAIN, "xrudp-async", XRUDP_BACKPRESSURE, "retained operation/result quota exhausted"); return NULL;
	}
	++s->Info.Retained; s->Info.RetainedBytes += cost; xrtMutexUnlock(&s->Lock);
	xrudp__asyncop* op = xrtCalloc(1, cost);
	if (op == NULL) {
		xrtMutexLock(&s->Lock); --s->Info.Retained; s->Info.RetainedBytes -= cost; xrudp__async_add(&s->Info.Rejected); xrtMutexUnlock(&s->Lock);
		return NULL;
	}
	op->Peer = xrtRudpPeerRef(p); op->Cost = cost; op->PayloadSize = payload; op->Request = *request;
	op->Result.Stream = request->Stream;
	if (xrudp__async_write(request->Kind)) { if (payload) memcpy(op->Payload, request->Data, payload); op->Request.Data = op->Payload; }
	if (request->TimeoutMs || (request->Kind == XRUDP_OPERATION_DATAGRAM_WRITE && request->DatagramLifetimeUs)) {
		double elapsed = submitted - p->Endpoint->Epoch;
		uint64_t now = elapsed > 0 && elapsed < (double)UINT64_MAX / 1000000.0 ? (uint64_t)(elapsed * 1000000.0) : 0;
		if (request->TimeoutMs) {
			uint64_t duration = (uint64_t)request->TimeoutMs * 1000;
			op->Deadline = now > UINT64_MAX - duration ? UINT64_MAX : now + duration;
		}
		if (request->Kind == XRUDP_OPERATION_DATAGRAM_WRITE && request->DatagramLifetimeUs)
			op->DatagramDeadline = now > UINT64_MAX - request->DatagramLifetimeUs ? UINT64_MAX : now + request->DatagramLifetimeUs;
	}
	xfuture* future = NULL; op->Promise = xrtPromiseCreate(&future, NULL);
	if (op->Promise != NULL) op->Cancel = xrtPromiseCancelToken(op->Promise);
	if (op->Cancel != NULL) op->Watch = xrtCancelWatch(op->Cancel, xrudp__async_cancel, op);
	if (op->Watch == NULL) {
		if (op->Promise != NULL) { (void)xrtPromiseCancel(op->Promise); xrtPromiseDestroy(op->Promise); }
		xrtMutexLock(&s->Lock); xrudp__async_add(&s->Info.Rejected); xrtMutexUnlock(&s->Lock);
		xrtFutureDestroy(future); xrtCancelDestroy(op->Cancel); xrudp__async_release(op); xrtSetErrorKind(XERR_MEMORY); return NULL;
	}
	xrtMutexLock(&s->Lock); xrudp__async_add(&s->Info.Submitted);
	if (s->Info.Stopping || xrtRudpEndpointState(p->Endpoint) >= XRUDP_ENDPOINT_CLOSING || xrtRudpPeerState(p) == XRUDP_PEER_CLOSED)
		op->Result.Result = XRUDP_CLOSED;
	else if (s->Ticket == UINT64_MAX) op->Result.Result = XRUDP_LIMIT;
	else {
		op->Ticket = ++s->Ticket; op->Linked = op->Counted = true; ++s->Info.Pending;
		if (s->Tail != NULL) s->Tail->Next = op; else s->Head = op;
		s->Tail = op;
		if (!xrudp__async_kick_locked(s)) { xrudp__async_remove_locked(s, op); op->Result.Result = XRUDP_CLOSED; }
	}
	bool queued = op->Linked; xrtMutexUnlock(&s->Lock);
	if (!queued) xrudp__async_finish_op(op, false);
	return future;
}

static xrudpresult xrudp__async_invoke(xrudp__asyncop* op)
{
	xrudppeer* p = op->Peer; xrudpoperation* r = &op->Request; xrudpoperationresult* out = &op->Result;
	out->Size = 0;
	switch (r->Kind) {
	case XRUDP_OPERATION_READ: return xrtRudpPeerStreamRead(p, r->Stream, op->Payload, r->Capacity, &out->Size);
	case XRUDP_OPERATION_WRITE: return xrtRudpPeerStreamWrite(p, r->Stream, op->Payload, r->Size, &out->Size);
	case XRUDP_OPERATION_MESSAGE_READ: return xrtRudpPeerMessageRead(p, r->Stream, op->Payload, r->Capacity, &out->Size);
	case XRUDP_OPERATION_MESSAGE_WRITE: {
		xrudpresult result = xrtRudpPeerMessageWrite(p, r->Stream, op->Payload, r->Size);
		if (result == XRUDP_OK) out->Size = r->Size;
		return result;
	}
	case XRUDP_OPERATION_DATAGRAM_READ: {
		xrudpresult result = xrtRudpPeerDatagramRead(p, op->Payload, r->Capacity, &out->Datagram);
		out->Size = out->Datagram.Size; return result;
	}
	case XRUDP_OPERATION_DATAGRAM_WRITE: {
		xrudpresult result = xrudp__peer_io(p); if (result != XRUDP_OK) return result;
		uint64_t now = xrudp__endpoint_now(p->Endpoint);
		if (op->DatagramDeadline && now >= op->DatagramDeadline) return XRUDP_TIMEOUT;
		result = xrudp__peer_datagram_write_at(p, r->Channel, op->Payload, r->Size, now, op->DatagramDeadline, &out->Sequence);
		if (result == XRUDP_OK) out->Size = r->Size;
		return result;
	}
	case XRUDP_OPERATION_STREAM_OPEN: return xrtRudpPeerStreamOpen(p, &r->StreamConfig, &out->Stream);
	case XRUDP_OPERATION_STREAM_FINISH: return xrtRudpPeerStreamFinish(p, r->Stream);
	case XRUDP_OPERATION_STREAM_STOP: return xrtRudpPeerStreamStop(p, r->Stream, r->Reason);
	case XRUDP_OPERATION_STREAM_RESET: return xrtRudpPeerStreamReset(p, r->Stream, r->Reason);
	case XRUDP_OPERATION_STREAM_RETIRE: return xrtRudpPeerStreamRetire(p, r->Stream);
	case XRUDP_OPERATION_DATAGRAM_CONFIGURE: return xrtRudpPeerDatagramConfigure(p, &r->ChannelConfig);
	case XRUDP_OPERATION_SHUTDOWN: return xrtRudpPeerShutdown(p, r->DrainTimeoutMs);
	case XRUDP_OPERATION_ABORT: return xrtRudpPeerAbort(p);
	case XRUDP_OPERATION_WAIT_WRITE_DONE: {
		xrudpstreamstate state; xrudpresult result = xrtRudpPeerStreamState(p, r->Stream, &state);
		return result != XRUDP_OK ? result : (state.WriteDone ? XRUDP_OK : XRUDP_BACKPRESSURE);
	}
	default: return XRUDP_ARGUMENT;
	}
}

typedef struct xrudp__asyncdomain { xrudppeer* Peer; uint32_t Stream, Direction; } xrudp__asyncdomain;
static bool xrudp__async_graceful_write(xrudpoperationkind kind)
{
	return kind == XRUDP_OPERATION_WRITE || kind == XRUDP_OPERATION_MESSAGE_WRITE ||
		kind == XRUDP_OPERATION_STREAM_OPEN || kind == XRUDP_OPERATION_STREAM_FINISH;
}
/* A shutdown must not discard writes merely because they have not yet won
 * native buffer space. Later writes must not bypass that same peer barrier.
 * Reads and abortive controls remain runnable so flow/cancellation can finish. */
static bool xrudp__async_fenced_locked(xrudp__asyncstate* s, xrudp__asyncop* target)
{
	xrudpoperationkind kind = target->Request.Kind;
	if (kind != XRUDP_OPERATION_SHUTDOWN && !xrudp__async_graceful_write(kind) && kind != XRUDP_OPERATION_DATAGRAM_WRITE) return false;
	for (xrudp__asyncop* op = s->Head; op != NULL && op != target; op = op->Next) {
		if (op->Peer != target->Peer) continue;
		if (op->Request.Kind == XRUDP_OPERATION_SHUTDOWN ||
			(kind == XRUDP_OPERATION_SHUTDOWN && xrudp__async_graceful_write(op->Request.Kind))) return true;
	}
	return false;
}
static xrudp__asyncdomain xrudp__async_domain(xrudp__asyncop* op)
{
	xrudp__asyncdomain key = {op->Peer, op->Request.Stream, 0};
	switch (op->Request.Kind) {
	case XRUDP_OPERATION_READ: case XRUDP_OPERATION_MESSAGE_READ: key.Direction = 1; break;
	case XRUDP_OPERATION_WRITE: case XRUDP_OPERATION_MESSAGE_WRITE: case XRUDP_OPERATION_STREAM_FINISH: key.Direction = 2; break;
	case XRUDP_OPERATION_DATAGRAM_READ: key.Direction = 3; key.Stream = 0; break;
	case XRUDP_OPERATION_DATAGRAM_WRITE: key.Direction = 4; key.Stream = 0; break;
	case XRUDP_OPERATION_STREAM_OPEN: key.Direction = 5; key.Stream = 0; break;
	default: break;
	}
	return key;
}

static void xrudp__async_pump(xrudpendpoint* e)
{
	xrudp__asyncstate* s = e->Async;
	if (s == NULL || s->Pumping) return;
	xrtRudpEndpointRef(e); s->Pumping = true;
	xrudp__asyncdomain blocked[XRUDP_ASYNC_OPERATIONS_MAX]; size_t blockedCount = 0;
	xrtMutexLock(&s->Lock); uint64_t limit = s->Ticket; xrudp__asyncop* cursor = s->Head; xrtMutexUnlock(&s->Lock);
	unsigned completed = 0; bool remaining = false;
	while (cursor != NULL && cursor->Ticket <= limit) {
		xrudp__asyncop* op = cursor; xrudp__asyncdomain key = xrudp__async_domain(op);
		xrtMutexLock(&s->Lock); cursor = op->Next;
		bool stopping = s->Info.Stopping || xrtRudpEndpointState(e) >= XRUDP_ENDPOINT_CLOSING;
		unsigned burst = stopping || s->ClosedHead != NULL ? XRUDP_ASYNC_OPERATIONS_MAX : XRUDP_ASYNC_COMPLETION_BURST;
		if (completed >= burst) { remaining = true; xrtMutexUnlock(&s->Lock); break; }
		bool cancelled = xrtCancelRequested(op->Cancel);
		bool closed = stopping || xrtRudpPeerState(op->Peer) == XRUDP_PEER_CLOSED;
		uint64_t now = xrudp__endpoint_now(e);
		bool expired = (op->Deadline && now >= op->Deadline) || (op->DatagramDeadline && now >= op->DatagramDeadline);
		bool wait = !cancelled && !closed && !expired &&
			((xrtRudpPeerState(op->Peer) != XRUDP_PEER_CONFIRMED && op->Request.Kind != XRUDP_OPERATION_ABORT) || xrudp__async_fenced_locked(s, op));
		if (!cancelled && !closed && !expired && key.Direction) for (size_t i = 0; i < blockedCount; ++i)
			if (blocked[i].Peer == key.Peer && blocked[i].Stream == key.Stream && blocked[i].Direction == key.Direction) wait = true;
		if (wait) { xrtMutexUnlock(&s->Lock); continue; }
		op->Active = true; xrtMutexUnlock(&s->Lock);
		xrudpresult closeReason = e->CloseReason;
		if (closeReason == XRUDP_OK) closeReason = e->LocalError != NULL ?
			(xrtErrorKind(e->LocalError) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR) : XRUDP_CANCELLED;
		xrudpresult result = closed ? (xrtRudpPeerState(op->Peer) == XRUDP_PEER_CLOSED ?
			op->Peer->AsyncCloseReason : closeReason) :
			(expired ? XRUDP_TIMEOUT : XRUDP_OK);
		if (!cancelled && !closed && !expired) result = xrudp__async_invoke(op);
		xrtMutexLock(&s->Lock); op->Active = false;
		if (result == XRUDP_BACKPRESSURE && !cancelled) {
			/* No native admission occurred. A cancellation arriving during the
			 * failed try may now win; successful admission never takes this path. */
			cancelled = xrtCancelRequested(op->Cancel);
			if (!cancelled) {
				if (key.Direction && blockedCount < XRUDP_ASYNC_OPERATIONS_MAX) blocked[blockedCount++] = key;
				xrtMutexUnlock(&s->Lock); continue;
			}
		}
		xrudp__async_remove_locked(s, op); op->Result.Result = result; xrtMutexUnlock(&s->Lock);
		xrudp__async_finish_op(op, cancelled); ++completed;
	}
	/* PeerClose callbacks cannot overtake a currently admitted operation or
	 * another queued operation on that peer. Reentrant aborts join this list. */
	unsigned closes = 0;
	while (s->ClosedHead != NULL) {
		xrtMutexLock(&s->Lock);
		if (!s->Info.Stopping && closes >= XRUDP_ENDPOINT_PEERS_MAX) { remaining = true; xrtMutexUnlock(&s->Lock); break; }
		xrudppeer* p = s->ClosedHead; bool pending = false;
		for (xrudp__asyncop* op = s->Head; op != NULL; op = op->Next) if (op->Peer == p) { pending = true; break; }
		xrtMutexUnlock(&s->Lock);
		if (pending) { remaining = true; break; }
		s->ClosedHead = p->AsyncCloseNext; if (s->ClosedHead == NULL) s->ClosedTail = NULL;
		p->AsyncCloseNext = NULL;
		if (e->Events.PeerClose != NULL) e->Events.PeerClose(e, p, p->AsyncCloseReason, e->Context);
		xrtRudpPeerDestroy(p); ++closes;
	}
	s->Pumping = false;
	xrtMutexLock(&s->Lock);
	if (remaining || (completed && s->Head != NULL)) (void)xrudp__async_kick_locked(s);
	xrtMutexUnlock(&s->Lock);
	xrudp__endpoint_finish(e, NULL); xrtRudpEndpointDestroy(e);
}
static void xrudp__async_dispatch(xnetworker* worker, ptr context)
{
	xrudpendpoint* e = context; (void)worker;
	xrudp__async_pump(e); xrudp__endpoint_schedule(e); xrudp__endpoint_finish(e, NULL); xrtRudpEndpointDestroy(e);
}
static void xrudp__async_stop(xrudpendpoint* e)
{
	xrudp__asyncstate* s = e->Async; if (s == NULL) return;
	xrtMutexLock(&s->Lock); s->Info.Stopping = true;
	if (s->Info.Pending) (void)xrudp__async_kick_locked(s);
	xrtMutexUnlock(&s->Lock);
}
static bool xrudp__async_peer_closed(xrudppeer* p, xrudpresult reason)
{
	xrudp__asyncstate* s = p->Endpoint->Async; if (s == NULL) return false;
	p->AsyncCloseReason = reason; p->AsyncCloseNext = NULL; xrtRudpPeerRef(p);
	if (s->ClosedTail != NULL) s->ClosedTail->AsyncCloseNext = p; else s->ClosedHead = p;
	s->ClosedTail = p; xrudp__async_pump(p->Endpoint); return true;
}
static uint64_t xrudp__async_due(xrudpendpoint* e)
{
	xrudp__asyncstate* s = e->Async; if (s == NULL) return UINT64_MAX;
	uint64_t due = UINT64_MAX; xrtMutexLock(&s->Lock);
	for (xrudp__asyncop* op = s->Head; op != NULL; op = op->Next) {
		if (op->Deadline && op->Deadline < due) due = op->Deadline;
		if (op->DatagramDeadline && op->DatagramDeadline < due) due = op->DatagramDeadline;
	}
	xrtMutexUnlock(&s->Lock); return due;
}
static bool xrudp__async_idle(xrudpendpoint* e)
{
	xrudp__asyncstate* s = e->Async; if (s == NULL) return true;
	if (s->Pumping || s->ClosedHead != NULL) return false;
	xrtMutexLock(&s->Lock); bool idle = s->Info.Pending == 0; xrtMutexUnlock(&s->Lock); return idle;
}
#endif
