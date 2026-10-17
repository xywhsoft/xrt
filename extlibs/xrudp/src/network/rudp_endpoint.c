#include <xrudp/endpoint.h>
#include <xrt/atomic.h>
#include <xrt/time.h>
#include <xrt/random.h>
#include <xrt/crypto.h>
#include <xrt/memory.h>
#include <string.h>

struct xrudppeer {
	xatomic32 References, State;
	xrudpendpoint* Endpoint;
	xnetaddr Address;
	xrudphandshake* Handshake;
	xrudpcipher* Cipher;
	xrudphandshakeinfo Info;
	uint64_t NextPoll, LastActivity, IdleUs;
	size_t Slot, RuntimeBytes, PendingSize;
	bool FinalQueued;
	unsigned char Pending[XRUDP_HANDSHAKE_PACKET_MAX];
};

typedef struct xrudp__addressrate {
	xrudpaddress Address;
	uint64_t Second;
	uint32_t Count;
	bool Used;
} xrudp__addressrate;

struct xrudpendpoint {
	xatomic32 References, State;
	xatomic64 Reserved;
	xnetworker* Worker;
	xnetengine* Engine;
	xnetudp* Udp; /* Stable owned handle; CLOSED UDP no longer holds Engine. */
	xrudpendpointconfig Config;
	xrudpendpointevents Events;
	void* Context;
	size_t Allocation;
	uint64_t Timer, LastNow, RateSecond;
	double Epoch;
	uint32_t RateCount, Peers, HalfOpen;
	bool UdpClosed, CloseNotified;
	xrudpresult CloseReason;
	xerror* LocalError;
	unsigned char Private[32], Public[32], Scope[16], Pin[32], Application[32], CookieKey[32];
	xrudpendpointinfo Stats;
	xrudp__addressrate Rates[XRUDP_ENDPOINT_ADDRESS_BUCKETS];
	xrudppeer* Slots[];
};

static void xrudp__endpoint_schedule(xrudpendpoint* e);
static void xrudp__endpoint_finish(xrudpendpoint* e, const xerror* error);

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
	xrtAtomic32Store(&p->State, XRUDP_PEER_CLOSED, XMEMORY_RELEASE);
	e->Slots[p->Slot] = NULL; --e->Peers; if (previous == XRUDP_PEER_NEGOTIATING) --e->HalfOpen;
	xrtRudpHandshakeDestroy(p->Handshake); p->Handshake = NULL; xrtRudpCipherDestroy(p->Cipher); p->Cipher = NULL;
	(void)xrtAtomic64FetchSub(&e->Reserved, p->RuntimeBytes, XMEMORY_ACQ_REL); p->RuntimeBytes = 0;
	p->PendingSize = 0;
	if (e->Events.PeerClose != NULL) e->Events.PeerClose(e, p, reason, e->Context);
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
	size_t cost = sizeof(xrudppeer) + xrtRudpHandshakeAllocationSize();
	if (e->Peers >= e->Config.MaxPeers || e->HalfOpen >= e->Config.MaxHalfOpen ||
		cost > e->Config.MemoryLimit - (size_t)xrtAtomic64Load(&e->Reserved, XMEMORY_ACQUIRE)) return false;
	for (*slot = 0; *slot < e->Config.MaxPeers && e->Slots[*slot] != NULL; ++*slot) {}
	if (*slot == e->Config.MaxPeers) return false;
	(void)xrtAtomic64FetchAdd(&e->Reserved, cost, XMEMORY_ACQ_REL); return true;
}
static void xrudp__endpoint_unreserve(xrudpendpoint* e)
{
	(void)xrtAtomic64FetchSub(&e->Reserved, sizeof(xrudppeer) + xrtRudpHandshakeAllocationSize(), XMEMORY_ACQ_REL);
}
static xrudppeer* xrudp__endpoint_peer(xrudpendpoint* e, size_t slot, const xnetaddr* address, xrudphandshake* handshake)
{
	xrudppeer* p = (xrudppeer*)xrtMalloc(sizeof(*p));
	if (p == NULL) return NULL;
	memset(p, 0, sizeof(*p)); xrtAtomic32Init(&p->References, 1); xrtAtomic32Init(&p->State, XRUDP_PEER_NEGOTIATING);
	p->Endpoint = xrtRudpEndpointRef(e); p->Address = *address; p->Handshake = handshake; p->Slot = slot;
	p->RuntimeBytes = xrtRudpHandshakeAllocationSize(); (void)xrtRudpHandshakeInfo(handshake, &p->Info);
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
	else if (result != XNET_RESULT_AGAIN) {
		if (xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY) ++e->Stats.CapacityDropped;
		else ++e->Stats.IoErrors;
	}
	return result;
}
static void xrudp__peer_pump(xrudppeer* p, uint64_t now)
{
	xrudpendpoint* e = p->Endpoint;
	if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
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
			if (p->PendingSize != 0) {
				xnetresult sent = xrudp__endpoint_send(e, &p->Address, p->Pending, p->PendingSize);
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
				if (xrtRudpHandshakeTakeCipher(p->Handshake, &p->Cipher) != XRUDP_OK) { xrudp__peer_close(p, XRUDP_IO_ERROR); return; }
				(void)xrtRudpHandshakeInfo(p->Handshake, &p->Info); p->LastActivity = now;
				uint32_t idle = p->Info.PeerParameters.IdleTimeoutMs < e->Config.Parameters.IdleTimeoutMs ?
					p->Info.PeerParameters.IdleTimeoutMs : e->Config.Parameters.IdleTimeoutMs;
				p->IdleUs = (uint64_t)idle * 1000; --e->HalfOpen;
				xrtAtomic32Store(&p->State, XRUDP_PEER_CONFIRMED, XMEMORY_RELEASE);
				if (e->Events.Confirmed != NULL) e->Events.Confirmed(e, p, e->Context);
				if (xrtRudpPeerState(p) == XRUDP_PEER_CLOSED) return;
			}
		}
	}
	if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && now - p->LastActivity >= p->IdleUs) xrudp__peer_close(p, XRUDP_TIMEOUT);
}
static void xrudp__endpoint_timer(xnetworker* worker, uint64 id, xnetresult result, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)worker;
	if (e->Timer == id) e->Timer = 0;
	if (result == XNET_RESULT_OK && xrtRudpEndpointState(e) == XRUDP_ENDPOINT_OPEN) {
		uint64_t now = xrudp__endpoint_now(e);
		for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
			xrudppeer* p = xrtRudpPeerRef(e->Slots[i]); xrudp__peer_pump(p, now); xrtRudpPeerDestroy(p);
		}
		xrudp__endpoint_schedule(e);
	}
	xrudp__endpoint_finish(e, NULL); xrtRudpEndpointDestroy(e);
}
static void xrudp__endpoint_schedule(xrudpendpoint* e)
{
	if (e->Timer != 0 || e->Peers == 0 || xrtRudpEndpointState(e) != XRUDP_ENDPOINT_OPEN) return;
	uint64_t now = xrudp__endpoint_now(e), due = UINT64_MAX;
	for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
		xrudppeer* p = e->Slots[i]; uint64_t next = p->PendingSize ? now : p->NextPoll;
		uint64_t idle = p->LastActivity > UINT64_MAX - p->IdleUs ? UINT64_MAX : p->LastActivity + p->IdleUs;
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && idle < next) next = idle;
		if (next < due) due = next;
	}
	uint64_t milliseconds = due > now ? (due - now) / 1000 + ((due - now) % 1000 != 0) : 1;
	if (milliseconds == 0) milliseconds = 1;
	if (milliseconds > 1000) milliseconds = 1000;
	xrtRudpEndpointRef(e);
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
	if ((message->Flags & XNET_UDP_MESSAGE_TRUNCATED) != 0 || !xrudp__endpoint_address(&message->Remote, &address) ||
		xrtRudpPacketDecode(message->Data, message->Size, &packet) != XRUDP_OK) { ++e->Stats.Invalid; return; }
	uint64_t now = xrudp__endpoint_now(e); xrudppeer* p = xrudp__endpoint_find(e, packet.Header.ConnectionId);
	if (p != NULL) {
		if (!xrtNetAddrEqual(&p->Address, &message->Remote)) { ++e->Stats.AddressDropped; return; }
		xrtRudpPeerRef(p);
		if (xrtRudpPeerState(p) == XRUDP_PEER_CONFIRMED && packet.Header.Type == XRUDP_PACKET_DATA && packet.Header.PacketNumber >= 2) {
			/* Carrier control only; reliable session dispatch is attached by the
			 * next layer. Never ACK/discard reliable business data here. */
			unsigned char plain[XRUDP_PACKET_MAX]; size_t size, offset = 0; xrudpwireframe frame;
			xrudpresult result = xrtRudpCipherOpen(p->Cipher, message->Data, message->Size, plain, sizeof(plain), &size);
			if (result == XRUDP_OK) {
				while (xrtRudpFrameNext(plain, size, &offset, &frame) == XRUDP_OK)
					if (frame.Type == XRUDP_FRAME_CLOSE) { xrudp__peer_close(p, XRUDP_CLOSED); break; }
			} else if (result == XRUDP_AUTH_FAILED) ++e->Stats.Invalid;
		} else if (p->Handshake != NULL) {
			xrudpresult result = xrtRudpHandshakeInput(p->Handshake, message->Data, message->Size, now);
			(void)xrtRudpHandshakeInfo(p->Handshake, &p->Info);
			if (p->Info.Phase == XRUDP_HANDSHAKE_FAILED) xrudp__peer_close(p, p->Info.Failure);
			else if (result == XRUDP_NO_MEMORY || result == XRUDP_IO_ERROR) xrudp__peer_close(p, result);
			else {
				if (result == XRUDP_OK) { p->PendingSize = 0; p->NextPoll = now; }
				if (result == XRUDP_AUTH_FAILED) ++e->Stats.Invalid;
				xrudp__peer_pump(p, now);
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
	c.Trust = e->Config.Trust; c.TrustContext = e->Config.TrustContext; c.Parameters = e->Config.Parameters; c.TimeoutUs = e->Config.HandshakeTimeoutUs;
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
	if (e->Events.Open != NULL) e->Events.Open(e, e->Context);
}
static void xrudp__endpoint_error(xnetudp* udp, const xerror* error, ptr context)
{
	xrudpendpoint* e = (xrudpendpoint*)context; (void)udp; (void)error; ++e->Stats.IoErrors;
}
static void xrudp__endpoint_finish(xrudpendpoint* e, const xerror* error)
{
	if (!e->UdpClosed || e->Timer != 0 || e->CloseNotified) return;
	e->CloseNotified = true; xrtSecureZero(e->Private, 32); xrtSecureZero(e->CookieKey, 32);
	xrtAtomic32Store(&e->State, XRUDP_ENDPOINT_CLOSED, XMEMORY_RELEASE);
	if (error == NULL) error = e->LocalError != NULL ? e->LocalError : xrtNetUdpError(e->Udp);
	if (e->Events.Close != NULL) e->Events.Close(e, e->CloseReason, error, e->Context);
	xrtRudpEndpointDestroy(e); /* Internal I/O lifetime. */
}
static void xrudp__endpoint_closed(xnetudp* udp, xnetresult result, const xerror* error, ptr context)
{
	xrudpendpoint* e = xrtRudpEndpointRef((xrudpendpoint*)context); (void)udp;
	xrudpendpointstate previous = xrtRudpEndpointState(e);
	xrtAtomic32Store(&e->State, XRUDP_ENDPOINT_CLOSING, XMEMORY_RELEASE);
	e->CloseReason = e->LocalError != NULL ? (xrtErrorKind(e->LocalError) == XERR_MEMORY ? XRUDP_NO_MEMORY : XRUDP_IO_ERROR) :
		(previous == XRUDP_ENDPOINT_CLOSING ? XRUDP_CANCELLED : (result == XNET_RESULT_ERROR ? XRUDP_IO_ERROR : XRUDP_CLOSED));
	for (size_t i = 0; i < e->Config.MaxPeers; ++i) if (e->Slots[i] != NULL) {
		xrudppeer* p = xrtRudpPeerRef(e->Slots[i]); xrudp__peer_close(p, e->CloseReason); xrtRudpPeerDestroy(p);
	}
	e->UdpClosed = true;
	if (e->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(e->Engine, e->Timer);
	xrudp__endpoint_finish(e, error); xrtRudpEndpointDestroy(e);
}
void xrtRudpEndpointConfigInit(xrudpendpointconfig* c)
{
	if (c == NULL) return;
	memset(c, 0, sizeof(*c)); xrtRudpParametersDefault(&c->Parameters);
	c->MaxPeers = 64; c->MaxHalfOpen = 16; c->MemoryLimit = 16u * 1024u * 1024u;
	c->HandshakesPerSecond = 128; c->AddressHandshakesPerSecond = 8;
}
xrudpresult xrtRudpEndpointBind(xnetworker* worker, const xnetaddr* local,
	const xrudpendpointconfig* config, const xrudpendpointevents* events, void* context, xrudpendpoint** output)
{
	unsigned char params[32];
	if (worker == NULL || !xrtNetWorkerIsCurrent(worker) || local == NULL || config == NULL || output == NULL ||
		(local->Family != XNET_FAMILY_IPV4 && local->Family != XNET_FAMILY_IPV6) ||
		!xrudp__endpoint_nonzero(config->StaticPrivate, 32) || (config->Scope != NULL && !xrudp__endpoint_nonzero(config->Scope, 16)) ||
		(config->PeerPublic != NULL && !xrudp__endpoint_nonzero(config->PeerPublic, 32)) ||
		(config->Listen && config->PeerPublic == NULL && config->Trust == NULL) || config->MaxPeers == 0 || config->MaxPeers > XRUDP_ENDPOINT_PEERS_MAX ||
		config->MaxHalfOpen == 0 || config->MaxHalfOpen > config->MaxPeers || config->HandshakesPerSecond == 0 || config->HandshakesPerSecond > 65536 ||
		config->AddressHandshakesPerSecond == 0 || config->AddressHandshakesPerSecond > config->HandshakesPerSecond ||
		config->MemoryLimit > 1024u * 1024u * 1024u || (config->HandshakeTimeoutUs != 0 && (config->HandshakeTimeoutUs < 1000000 || config->HandshakeTimeoutUs > XRUDP_HANDSHAKE_MAX_TIMEOUT_US)) ||
		xrtRudpParametersEncode(&config->Parameters, params, sizeof(params)) != XRUDP_OK) return XRUDP_ARGUMENT;
	size_t size = sizeof(xrudpendpoint) + config->MaxPeers * sizeof(xrudppeer*);
	if (config->MemoryLimit < size + sizeof(xrudppeer) + xrtRudpHandshakeAllocationSize()) return XRUDP_ARGUMENT;
	xrudpendpoint* e = (xrudpendpoint*)xrtMalloc(size);
	if (e == NULL) return XRUDP_NO_MEMORY;
	memset(e, 0, size); e->Allocation = size; e->Worker = worker; e->Engine = xrtNetWorkerEngine(worker);
	e->Config = *config; e->Context = context; e->Epoch = xrtTimer(); e->RateSecond = UINT64_MAX;
	if (events != NULL) e->Events = *events;
	xrtAtomic32Init(&e->References, 2); xrtAtomic32Init(&e->State, XRUDP_ENDPOINT_OPENING); xrtAtomic64Init(&e->Reserved, size);
	memcpy(e->Private, config->StaticPrivate, 32); e->Config.StaticPrivate = e->Private;
	if (config->Scope != NULL) memcpy(e->Scope, config->Scope, 16);
	else if (!xrtSecureRandom(e->Scope, 16)) goto random_failure;
	if (!xrudp__endpoint_nonzero(e->Scope, 16) || !xrtSecureRandom(e->CookieKey, 32) || !xrudp__endpoint_nonzero(e->CookieKey, 32) ||
		!xrtX25519Public(e->Private, e->Public)) goto random_failure;
	e->Config.Scope = e->Scope;
	if (config->PeerPublic != NULL) { memcpy(e->Pin, config->PeerPublic, 32); e->Config.PeerPublic = e->Pin; }
	if (config->ApplicationContext != NULL) { memcpy(e->Application, config->ApplicationContext, 32); e->Config.ApplicationContext = e->Application; }
	xnetudpconfig udp; xrtNetUdpConfigInit(&udp); udp.ReceiveSize = XRUDP_PACKET_MAX; udp.ReceiveBatch = 8;
	udp.ReceiveQueueLimit = 0; udp.ReceiveQueueByteLimit = 0; udp.Truncation = XNET_UDP_TRUNCATE_DROP;
	udp.SendLimit = 65536; udp.SendPacketLimit = 64; udp.SendHighWater = 49152; udp.SendLowWater = 16384;
	udp.ReceiveBuffer = udp.SendBuffer = 262144; udp.IPv6Only = true;
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
	c.ApplicationContext = config->ApplicationContext; c.Parameters = e->Config.Parameters; c.TimeoutUs = e->Config.HandshakeTimeoutUs;
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
	*output = info; return XRUDP_OK;
}
xrudpresult xrtRudpPeerInfo(const xrudppeer* p, xrudphandshakeinfo* output, xnetaddr* address)
{
	if (p == NULL || output == NULL || address == NULL ||
		(xrtRudpPeerState(p) != XRUDP_PEER_CLOSED && !xrudp__endpoint_owner(p->Endpoint))) return XRUDP_ARGUMENT;
	*output = p->Info; *address = p->Address; return XRUDP_OK;
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
			(void)xrudp__endpoint_send(p->Endpoint, &p->Address, packet, size);
	}
	xrudp__peer_close(p, XRUDP_CANCELLED); xrtRudpPeerDestroy(p); return XRUDP_OK;
}
xrudpresult xrtRudpEndpointClose(xrudpendpoint* e)
{
	if (e == NULL) return XRUDP_ARGUMENT;
	xrtRudpEndpointRef(e); uint32 state = xrtAtomic32Load(&e->State, XMEMORY_ACQUIRE);
	while (state < XRUDP_ENDPOINT_CLOSING && !xrtAtomic32CompareExchange(&e->State, &state, XRUDP_ENDPOINT_CLOSING, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {}
	if (state < XRUDP_ENDPOINT_CLOSED) (void)xrtNetUdpAbort(e->Udp);
	xrtRudpEndpointDestroy(e); return XRUDP_OK;
}
