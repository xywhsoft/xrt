#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/crypto.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)
#ifndef TEST_RUDP_BACKEND
#define TEST_RUDP_BACKEND XNET_PORT_SELECT
#endif

typedef struct test_context test_context;
typedef struct test_side { test_context* Test; unsigned int Index; } test_side;
struct test_context {
	xnetengine* Engine;
	xrudpendpoint* Endpoint[2];
	xrudppeer* Peer[2];
	xrudpendpointinfo Identity[2];
	xrudphandshakeinfo Handshake[2];
	xatomic32 Ready, Dialed, Confirmed, Closed, PeerClosed, Failure;
	xatomic32 SnapshotReady;
	xrudpendpointinfo Snapshot;
	unsigned int CloseCount[2], PeerCloseCount[2], Mode;
	xrudpresult CloseReason[2];
	xnetfamily Family;
	unsigned char Private[2][32], Public[2][32], WrongPublic[32];
	test_side Sides[2];
};

static void expect(test_context* t, bool condition, const char* message)
{
	if (!condition) { fprintf(stderr, "endpoint callback: %s\n", message); xrtAtomic32Store(&t->Failure, 1, XMEMORY_RELEASE); }
}
static bool wait_bits(test_context* t, const xatomic32* word, uint32 bits)
{
	double deadline = xrtTimer() + 5.0;
	while ((xrtAtomic32Load(word, XMEMORY_ACQUIRE) & bits) != bits) {
		if (xrtAtomic32Load(&t->Failure, XMEMORY_ACQUIRE) || xrtTimer() >= deadline) return false;
		xrtThreadYield();
	}
	return true;
}
static void opened(xrudpendpoint* e, void* context)
{
	test_side* s = (test_side*)context; test_context* t = s->Test;
	expect(t, xrtRudpEndpointInfo(e, &t->Identity[s->Index]) == XRUDP_OK, "identity query failed");
	expect(t, t->Identity[s->Index].Local.Port != 0, "bound port missing");
	(void)xrtAtomic32FetchOr(&t->Ready, 1u << s->Index, XMEMORY_RELEASE);
}
static void confirmed(xrudpendpoint* e, xrudppeer* p, void* context)
{
	test_side* s = (test_side*)context; test_context* t = s->Test; xnetaddr address; (void)e;
	expect(t, xrtRudpPeerInfo(p, &t->Handshake[s->Index], &address) == XRUDP_OK, "peer info missing");
	expect(t, t->Handshake[s->Index].Phase == XRUDP_HANDSHAKE_ESTABLISHED && t->Handshake[s->Index].KeysTaken, "peer exposed before proof/key transfer");
	if (s->Index == 0) t->Peer[0] = xrtRudpPeerRef(p);
	(void)xrtAtomic32FetchOr(&t->Confirmed, 1u << s->Index, XMEMORY_RELEASE);
	if (t->Mode == 1 && s->Index == 1) {
		/* Native pump and callback must retain their own references. */
		xrtRudpPeerDestroy(t->Peer[1]); t->Peer[1] = NULL;
		expect(t, xrtRudpPeerAbort(p) == XRUDP_OK, "callback reentrant peer abort failed");
	}
}
static void peer_closed(xrudpendpoint* e, xrudppeer* p, xrudpresult reason, void* context)
{
	test_side* s = (test_side*)context; test_context* t = s->Test; (void)e; (void)reason;
	expect(t, xrtRudpPeerState(p) == XRUDP_PEER_CLOSED, "peer close state not published");
	expect(t, ++t->PeerCloseCount[s->Index] == 1, "duplicate peer terminal callback");
	expect(t, xrtRudpPeerAbort(p) == XRUDP_DONE, "terminal peer abort not idempotent");
	(void)xrtAtomic32FetchOr(&t->PeerClosed, 1u << s->Index, XMEMORY_RELEASE);
}
static void closed(xrudpendpoint* e, xrudpresult reason, const xerror* error, void* context)
{
	test_side* s = (test_side*)context; test_context* t = s->Test; (void)reason; (void)error;
	t->CloseReason[s->Index] = reason;
	if (t->Mode == 4 && s->Index == 1) expect(t, reason == XRUDP_IO_ERROR && error != NULL, "timer admission failure lost its structured cause");
	expect(t, xrtRudpEndpointState(e) == XRUDP_ENDPOINT_CLOSED, "endpoint close state not published");
	expect(t, ++t->CloseCount[s->Index] == 1, "duplicate endpoint close callback");
	if (t->Mode == 1) xrtRudpEndpointDestroy(e); /* Drop the constructor's user reference. */
	(void)xrtAtomic32FetchOr(&t->Closed, 1u << s->Index, XMEMORY_RELEASE);
}
static void bind_endpoint(xnetworker* worker, ptr context)
{
	test_side* s = (test_side*)context; test_context* t = s->Test;
	xrudpendpointconfig cfg; xrtRudpEndpointConfigInit(&cfg);
	cfg.StaticPrivate = t->Private[s->Index]; cfg.PeerPublic = t->Public[1 - s->Index]; cfg.Listen = s->Index == 0;
	if (t->Mode >= 2) { cfg.MaxPeers = cfg.MaxHalfOpen = 1; cfg.HandshakeTimeoutUs = t->Mode == 2 ? 1000000 : 10000000; }
	xnetaddr local; expect(t, xrtNetAddrLoopback(&local, t->Family, 0), "loopback address failed");
	xrudpendpointevents ev = {opened, confirmed, peer_closed, closed, NULL, NULL};
	expect(t, xrtRudpEndpointBind(worker, &local, &cfg, &ev, s, &t->Endpoint[s->Index]) == XRUDP_OK, "endpoint bind failed");
}
static void dial(xnetworker* worker, ptr context)
{
	test_context* t = (test_context*)context; (void)worker;
	xrudpconnectconfig cfg = {t->Mode >= 2 ? t->WrongPublic : t->Identity[0].Public, t->Identity[0].Scope, NULL};
	if (t->Mode == 4) {
		extern void endpoint_test_blocker(xnetworker*, uint64, xnetresult, ptr);
		expect(t, xrtNetEngineAfter(t->Engine, 1, 10000, endpoint_test_blocker, t) != 0, "timer blocker rejected");
	}
	expect(t, xrtRudpEndpointConnect(t->Endpoint[1], &t->Identity[0].Local, &cfg, &t->Peer[1]) == XRUDP_OK, "dial failed");
	if (t->Mode == 2 || t->Mode == 3) {
		xrudppeer* extra = NULL;
		expect(t, xrtRudpEndpointConnect(t->Endpoint[1], &t->Identity[0].Local, &cfg, &extra) == XRUDP_BACKPRESSURE && extra == NULL,
			"half-open capacity was not enforced");
	}
	(void)xrtAtomic32FetchOr(&t->Dialed, 1, XMEMORY_RELEASE);
}
void endpoint_test_blocker(xnetworker* worker, uint64 id, xnetresult result, ptr context)
{
	(void)worker; (void)id; (void)result; (void)context;
}

static void snapshot_endpoint(xnetworker* worker, ptr context)
{
	test_context* t = (test_context*)context; (void)worker;
	expect(t, xrtRudpEndpointInfo(t->Endpoint[0], &t->Snapshot) == XRUDP_OK, "security snapshot failed");
	xrtAtomic32Store(&t->SnapshotReady, 1, XMEMORY_RELEASE);
}
static int snapshot(test_context* t)
{
	xrtAtomic32Store(&t->SnapshotReady, 0, XMEMORY_RELEASE);
	CHECK(xrtNetEnginePost(t->Engine, 0, snapshot_endpoint, t)); CHECK(wait_bits(t, &t->SnapshotReady, 1));
	return 0;
}
static int untrusted_packets(test_context* t)
{
	CHECK(snapshot(t) == 0); xrudpendpointinfo before = t->Snapshot;
	xnetaddr local; CHECK(xrtNetAddrLoopback(&local, XNET_FAMILY_IPV4, 0));
	xnetudp* attacker = xrtNetUdpBind(t->Engine, &local, 1, NULL, NULL, NULL); CHECK(attacker != NULL);
	unsigned char body[96] = {0}, hello[144], forged[96], junk[24] = {0}; size_t helloSize, forgedSize;
	body[0] = 0x9a; memcpy(body + 16, t->Identity[0].Public, 32); body[48] = 0x6d;
	xrudpwireheader header; memset(&header, 0, sizeof(header)); header.Type = XRUDP_PACKET_HELLO;
	header.PacketNumber = 1; header.PayloadSize = 96; memcpy(header.ConnectionId, t->Identity[0].Scope, 16);
	CHECK(xrtRudpPacketEncode(hello, sizeof(hello), &helloSize, &header, body, sizeof(body)) == XRUDP_OK);
	header.Type = XRUDP_PACKET_DATA; header.KeyEpoch = 1; header.PacketNumber = 2; header.PayloadSize = sizeof(junk);
	memcpy(header.ConnectionId, t->Handshake[0].LocalId, 16);
	CHECK(xrtRudpPacketEncode(forged, sizeof(forged), &forgedSize, &header, junk, sizeof(junk)) == XRUDP_OK);
	double started = xrtTimer();
	for (size_t i = 0; i < 100; ++i) CHECK(xrtNetUdpSendTo(attacker, &t->Identity[0].Local, hello, helloSize) == XNET_RESULT_OK);
	for (size_t i = 0; i < 16; ++i) CHECK(xrtNetUdpSendTo(attacker, &t->Identity[0].Local, forged, forgedSize) == XNET_RESULT_OK);
	double deadline = xrtTimer() + 3.0;
	for (;;) {
		CHECK(snapshot(t) == 0);
		if (t->Snapshot.Received >= before.Received + 116 && t->Snapshot.RateDropped > before.RateDropped &&
			t->Snapshot.AddressDropped > before.AddressDropped) break;
		CHECK(xrtTimer() < deadline);
		xrtThreadYield();
	}
	CHECK(t->Snapshot.Peers == before.Peers && t->Snapshot.HalfOpen == 0 && t->Snapshot.ReservedBytes == before.ReservedBytes);
	CHECK(xrtRudpPeerState(t->Peer[0]) == XRUDP_PEER_CONFIRMED && xrtRudpPeerState(t->Peer[1]) == XRUDP_PEER_CONFIRMED);
	CHECK(xrtNetUdpClose(attacker));
	while (xrtNetUdpState(attacker) != XNET_UDP_CLOSED) { CHECK(xrtTimer() < deadline); xrtThreadYield(); }
	unsigned int retries = 0; xnetudppacket* received;
	while ((received = xrtNetUdpReceive(attacker)) != NULL) {
		xrudpwirepacket parsed;
		if (xrtRudpPacketDecode(xrtNetUdpPacketData(received), xrtNetUdpPacketSize(received), &parsed) == XRUDP_OK && parsed.Header.Type == XRUDP_PACKET_RETRY) ++retries;
		xrtNetUdpPacketDestroy(received);
	}
	CHECK(retries <= 8u * ((unsigned int)(xrtTimer() - started) + 2u));
	xrtNetUdpDestroy(attacker); return 0;
}

static int run_case(xnetfamily family, unsigned int mode)
{
	test_context t; memset(&t, 0, sizeof(t)); t.Mode = mode; t.Family = family;
	xrtAtomic32Init(&t.Ready, 0); xrtAtomic32Init(&t.Dialed, 0); xrtAtomic32Init(&t.Confirmed, 0);
	xrtAtomic32Init(&t.Closed, 0); xrtAtomic32Init(&t.PeerClosed, 0); xrtAtomic32Init(&t.Failure, 0);
	xrtAtomic32Init(&t.SnapshotReady, 0);
	for (size_t i = 0; i < 32; ++i) { t.Private[0][i] = (unsigned char)(i + 11); t.Private[1][i] = (unsigned char)(i + 97); t.WrongPublic[i] = (unsigned char)(i + 171); }
	CHECK(xrtX25519Public(t.Private[0], t.Public[0]) && xrtX25519Public(t.Private[1], t.Public[1]));
	xnetengineconfig cfg; xrtNetEngineConfigInit(&cfg); cfg.Workers = 2; cfg.Backend = TEST_RUDP_BACKEND;
	if (mode == 4) cfg.TimerLimit = 1;
	t.Engine = xrtNetEngineCreate(&cfg); CHECK(t.Engine != NULL && xrtNetEngineStart(t.Engine));
	for (unsigned int side = 0; side < 2; ++side) {
		t.Sides[side].Test = &t; t.Sides[side].Index = side;
		CHECK(xrtNetEnginePost(t.Engine, side, bind_endpoint, &t.Sides[side]));
	}
	CHECK(wait_bits(&t, &t.Ready, 3));
	CHECK(xrtNetEnginePost(t.Engine, 1, dial, &t)); CHECK(wait_bits(&t, &t.Dialed, 1));
	if (mode < 2) {
		CHECK(wait_bits(&t, &t.Confirmed, 3));
		CHECK(memcmp(t.Handshake[0].LocalId, t.Handshake[1].PeerId, 16) == 0);
		CHECK(memcmp(t.Handshake[1].LocalId, t.Handshake[0].PeerId, 16) == 0);
		if (mode == 0 && family == XNET_FAMILY_IPV4) CHECK(untrusted_packets(&t) == 0);
		if (mode == 1) CHECK(wait_bits(&t, &t.PeerClosed, 3));
	} else if (mode == 2) {
		CHECK(wait_bits(&t, &t.PeerClosed, 2)); CHECK(xrtAtomic32Load(&t.Confirmed, XMEMORY_ACQUIRE) == 0);
	} else if (mode == 4) {
		CHECK(wait_bits(&t, &t.Closed, 2) && t.CloseReason[1] == XRUDP_IO_ERROR);
	}
	if (mode == 3) {
		/* The existing engine rejects stopping with live UDP owners. Its
		 * lifecycle guard must remain intact; close the in-flight handshakes. */
		CHECK(!xrtNetEngineStop(t.Engine) && xrtNetEngineState(t.Engine) == XNET_ENGINE_RUNNING);
		xrtClearError();
		CHECK(xrtRudpEndpointClose(t.Endpoint[0]) == XRUDP_OK && xrtRudpEndpointClose(t.Endpoint[1]) == XRUDP_OK);
		CHECK(wait_bits(&t, &t.Closed, 3)); CHECK(xrtNetEngineStop(t.Engine));
	} else {
		/* Hold temporaries so callback destruction can race the close request. */
		xrudpendpoint* a = xrtRudpEndpointRef(t.Endpoint[0]); xrudpendpoint* b = xrtRudpEndpointRef(t.Endpoint[1]);
		CHECK(xrtRudpEndpointClose(a) == XRUDP_OK && xrtRudpEndpointClose(b) == XRUDP_OK);
		CHECK(xrtRudpEndpointClose(a) == XRUDP_OK); CHECK(wait_bits(&t, &t.Closed, 3));
		xrtRudpEndpointDestroy(a); xrtRudpEndpointDestroy(b);
		CHECK(xrtNetEngineStop(t.Engine));
	}
	CHECK(xrtAtomic32Load(&t.Failure, XMEMORY_ACQUIRE) == 0);
	CHECK(xrtNetEngineDestroy(t.Engine)); /* Closed handles must not retain Engine. */
	for (size_t i = 0; i < 2; ++i) {
		if (mode != 1) {
			xrudpendpointinfo snapshot;
			CHECK(xrtRudpEndpointInfo(t.Endpoint[i], &snapshot) == XRUDP_OK && snapshot.Peers == 0 && snapshot.HalfOpen == 0);
			if (t.Peer[i] != NULL) {
				xrudphandshakeinfo info; xnetaddr address;
				CHECK(xrtRudpPeerInfo(t.Peer[i], &info, &address) == XRUDP_OK);
			}
		}
		xrtRudpPeerDestroy(t.Peer[i]); if (mode != 1) xrtRudpEndpointDestroy(t.Endpoint[i]);
	}
	printf("xrudp native endpoint: backend=%d family=%d case=%u passed\n", (int)TEST_RUDP_BACKEND, (int)family, mode);
	return 0;
}
int main(void)
{
	CHECK(run_case(XNET_FAMILY_IPV4, 0) == 0);
	CHECK(run_case(XNET_FAMILY_IPV6, 0) == 0);
	CHECK(run_case(XNET_FAMILY_IPV4, 1) == 0);
	CHECK(run_case(XNET_FAMILY_IPV4, 2) == 0);
	CHECK(run_case(XNET_FAMILY_IPV4, 3) == 0);
	CHECK(run_case(XNET_FAMILY_IPV4, 4) == 0);
	return 0;
}
