#include <xrudp.h>
#include <xrt/crypto.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_SESSION_FAULTS
#include <xrt/memory_debug.h>
#endif

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%s:%d: %s (tick=%u seed=%u)\n", __FILE__, __LINE__, #e, tick, randomState); return 1; } } while (0)
#define TEST_DATA 32768u
#define TEST_QUEUE 256u
static uint32_t tick, randomState;
static uint32_t random_next(void) { randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5; return randomState; }
static void put32(unsigned char* p, uint32_t v) { for (size_t i = 0; i < 4; ++i) p[3 - i] = (unsigned char)(v >> (i * 8)); }
static void put64(unsigned char* p, uint64_t v) { for (size_t i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(v >> (i * 8)); }
static bool good(xrudpresult r) { return r == XRUDP_OK || r == XRUDP_DUPLICATE; }

typedef struct session_packet { unsigned char Data[XRUDP_PACKET_MAX]; size_t Size; uint32_t Due, Target; bool Used; } session_packet;
typedef struct session_output { uint32_t Index; bool Impair, Block, OutOfMemory, Error; size_t Calls; } session_output;
static struct {
	xrudpsession* Session[2]; session_output Output[2]; session_packet Queue[TEST_QUEUE];
	unsigned char Source[2][TEST_DATA]; size_t Written[2], Read[2];
	uint32_t Dropped, Duplicated, Tampered, Blocked; bool Overflow;
} net;

static xrudpresult send_packet(const void* data, size_t size, void* context)
{
	session_output* out = (session_output*)context; ++out->Calls;
	if (out->Error) return XRUDP_IO_ERROR;
	if (out->Block) return XRUDP_BACKPRESSURE;
	if (out->OutOfMemory) return XRUDP_NO_MEMORY;
	if (size > XRUDP_PACKET_MAX || size < XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE + 4) { net.Overflow = true; return XRUDP_IO_ERROR; }
	if (out->Impair && random_next() % 17 == 0) { ++net.Blocked; return XRUDP_BACKPRESSURE; }
	if (out->Impair && ((tick >= 80 && tick <= 240) || random_next() % 100 < 25)) { ++net.Dropped; return XRUDP_OK; }
	uint32_t copies = out->Impair && random_next() % 5 == 0 ? 2 : 1;
	if (copies == 2) ++net.Duplicated;
	for (uint32_t copy = 0; copy < copies; ++copy) {
		size_t slot; for (slot = 0; slot < TEST_QUEUE && net.Queue[slot].Used; ++slot) {}
		if (slot == TEST_QUEUE) { net.Overflow = true; return XRUDP_IO_ERROR; }
		session_packet* p = &net.Queue[slot]; p->Used = true; p->Target = 1 - out->Index; p->Size = size;
		p->Due = tick + (out->Impair ? 1 + random_next() % 25 : 1); memcpy(p->Data, data, size);
	}
	return XRUDP_OK;
}
static int raw_pair(xrudpcipher* cipher[2])
{
	unsigned char key[2][32] = {{3},{11}}, id[2][16] = {{7},{19}}, packet[2][100], plain[100];
	unsigned char proof[12] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	size_t size[2], length;
	for (size_t i = 0; i < 2; ++i) {
		CHECK(xrtRudpCipherCreate(key[i], key[1 - i], id[i], id[1 - i], 0, &cipher[i]) == XRUDP_OK);
		CHECK(xrtRudpCipherSeal(cipher[i], proof, sizeof(proof), packet[i], sizeof(packet[i]), &size[i]) == XRUDP_OK);
	}
	for (size_t i = 0; i < 2; ++i) CHECK(xrtRudpCipherOpen(cipher[i], packet[1 - i], size[1 - i], plain, sizeof(plain), &length) == XRUDP_OK);
	return 0;
}
static void configure(xrudpsessionconfig* cfg, uint32_t capacity, uint32_t datagram)
{
	xrtRudpSessionConfigInit(cfg); cfg->ReceiveCapacity = cfg->TransmitCapacity = capacity;
	cfg->Local.StreamCredit = cfg->Peer.StreamCredit = capacity;
	cfg->Local.ConnectionCredit = cfg->Peer.ConnectionCredit = capacity;
	cfg->Peer.MaxStreams = 1;
	cfg->Local.DatagramSize = cfg->Peer.DatagramSize = datagram;
	cfg->PacketCapacity = 32; cfg->InitialRttUs = 30000; cfg->MaximumWindow = 12000;
	cfg->Local.IdleTimeoutMs = cfg->Peer.IdleTimeoutMs = 60000; cfg->KeyUpdatePackets = 16;
}
static int new_pair(uint32_t capacity, uint32_t datagram, bool impair)
{
	xrudpcipher* cipher[2] = {NULL,NULL}; xrudpsessionconfig cfg;
	memset(&net, 0, sizeof(net)); CHECK(raw_pair(cipher) == 0); configure(&cfg, capacity, datagram);
	for (size_t i = 0; i < 2; ++i) {
		net.Output[i].Index = (uint32_t)i; net.Output[i].Impair = impair;
		CHECK(xrtRudpSessionCreate(&cfg, cipher[i], 0, send_packet, &net.Output[i], &net.Session[i]) == XRUDP_OK);
		for (size_t k = 0; k < TEST_DATA; ++k) net.Source[i][k] = (unsigned char)((k * 37 + i * 91) ^ (k >> 9));
	}
	return 0;
}
static int confirm_pair(xrudpsessionconfig* cfg, xrudpcipher* cipher[2])
{
	unsigned char keys[2][32] = {{3},{11}}, publics[2][32], cookie[32] = {17}, scope[16] = {23};
	unsigned char packet[312], retry[312]; size_t size, retrySize; uint64_t due;
	xrudphandshakeconfig config[2]; xrudphandshake* h[2] = {NULL,NULL};
	xrudpaddress source = {4,42000,0,{192,0,2,1}};
	for (size_t i = 0; i < 2; ++i) CHECK(xrtX25519Public(keys[i], publics[i]));
	for (size_t i = 0; i < 2; ++i) {
		memset(&config[i], 0, sizeof(config[i])); config[i].StaticPrivate = keys[i]; config[i].LocalPublic = publics[i];
		config[i].PeerPublic = publics[1 - i]; config[i].Scope = scope; config[i].Parameters = cfg->Local;
	}
	CHECK(xrtRudpHandshakeCreate(&config[0], 0, &h[0]) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(h[0], 0, packet, sizeof(packet), &size, &due) == XRUDP_OK);
	CHECK(xrtRudpHandshakeRetry(cookie, scope, publics[1], &source, 0, packet, size, retry, sizeof(retry), &retrySize) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInput(h[0], retry, retrySize, 0) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(h[0], 0, packet, sizeof(packet), &size, &due) == XRUDP_OK);
	CHECK(xrtRudpHandshakeAccept(&config[1], cookie, NULL, &source, 0, packet, size, &h[1]) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(h[1], 0, packet, sizeof(packet), &size, &due) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInput(h[0], packet, size, 0) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(h[0], 0, packet, sizeof(packet), &size, &due) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInput(h[1], packet, size, 0) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(h[1], 0, packet, sizeof(packet), &size, &due) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInput(h[0], packet, size, 0) == XRUDP_OK);
	for (size_t i = 0; i < 2; ++i) {
		xrudphandshakeinfo info; CHECK(xrtRudpHandshakeInfo(h[i], &info) == XRUDP_OK && info.Phase == XRUDP_HANDSHAKE_ESTABLISHED);
		CHECK(memcmp(&info.PeerParameters, &cfg->Peer, sizeof(cfg->Peer)) == 0);
		CHECK(xrtRudpHandshakeTakeCipher(h[i], &cipher[i]) == XRUDP_OK); xrtRudpHandshakeDestroy(h[i]);
	}
	return 0;
}
static void destroy_pair(void)
{
	xrtRudpSessionDestroy(net.Session[0]); xrtRudpSessionDestroy(net.Session[1]); net.Session[0] = net.Session[1] = NULL;
}
static int transfer(uint32_t seed, uint32_t capacity, uint32_t datagram)
{
#ifdef TEST_SESSION_FAULTS
	xmemdebugsnapshot baseline, final; xrtMemDebugSnapshot(&baseline);
#endif
	randomState = seed; tick = 0; memset(&net, 0, sizeof(net));
	xrudpsessionconfig cfg; configure(&cfg, capacity, datagram); xrudpcipher* cipher[2] = {NULL,NULL};
	CHECK(confirm_pair(&cfg, cipher) == 0);
	for (size_t i = 0; i < 2; ++i) {
		net.Output[i].Index = (uint32_t)i; net.Output[i].Impair = true;
		CHECK(xrtRudpSessionCreate(&cfg, cipher[i], 0, send_packet, &net.Output[i], &net.Session[i]) == XRUDP_OK);
		for (size_t k = 0; k < TEST_DATA; ++k) net.Source[i][k] = (unsigned char)((k * 37 + i * 91) ^ (k >> 9));
	}
#ifdef TEST_SESSION_FAULTS
	CHECK(xrtMemDebugFailAfter(0));
#endif
	bool done = false; uint64_t due[2] = {0,0}; uint32_t masks = 0;
	for (tick = 1; tick < 150000 && !done; ++tick) {
		uint64_t now = (uint64_t)tick * 1000;
		for (size_t i = 0; i < TEST_QUEUE; ++i) if (net.Queue[i].Used && net.Queue[i].Due <= tick) {
			session_packet p = net.Queue[i]; net.Queue[i].Used = false;
			if (random_next() % 7 == 0) {
				unsigned char bad[XRUDP_PACKET_MAX]; memcpy(bad, p.Data, p.Size); bad[random_next() % p.Size] ^= 1;
				CHECK(xrtRudpSessionInput(net.Session[p.Target], bad, p.Size, now) == XRUDP_AUTH_FAILED); ++net.Tampered;
			}
			xrudpresult result = xrtRudpSessionInput(net.Session[p.Target], p.Data, p.Size, now);
			CHECK(good(result) || result == XRUDP_TOO_OLD); masks |= 1u << p.Target;
		}
		for (size_t i = 0; i < 2; ++i) {
			if (net.Written[i] < TEST_DATA) {
				size_t written; xrudpresult result = xrtRudpSessionWrite(net.Session[i], net.Source[i] + net.Written[i], TEST_DATA - net.Written[i], &written);
				CHECK(result == XRUDP_OK || result == XRUDP_BACKPRESSURE); net.Written[i] += written;
				if (written) masks |= 1u << i;
				if (net.Written[i] == TEST_DATA) CHECK(xrtRudpSessionFinish(net.Session[i]) == XRUDP_OK);
			}
			if (random_next() % 3 == 0) {
				unsigned char output[128]; size_t count;
				xrudpresult result = xrtRudpSessionRead(net.Session[i], output, 1 + random_next() % sizeof(output), &count);
				CHECK(result == XRUDP_OK || result == XRUDP_BACKPRESSURE || result == XRUDP_DONE);
				CHECK(net.Read[i] + count <= TEST_DATA && memcmp(output, net.Source[1 - i] + net.Read[i], count) == 0);
				net.Read[i] += count; if (count) masks |= 1u << i;
			}
			if (now >= due[i] || (masks & (1u << i))) {
				size_t calls = net.Output[i].Calls;
				CHECK(xrtRudpSessionPoll(net.Session[i], now, &due[i]) == XRUDP_OK && due[i] > now);
				CHECK(net.Output[i].Calls - calls <= XRUDP_SESSION_POLL_BURST); masks &= ~(1u << i);
			}
			xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK);
			CHECK(state.Congestion.BytesInFlight == state.Sent.BytesInFlight);
			CHECK(state.ReceivedConnection <= state.ReceiveLimit && state.SentConnection <= state.PeerLimit);
			CHECK(state.Sent.PendingPackets <= 64 && state.OutstandingProbes <= 2);
		}
		done = true;
		for (size_t i = 0; i < 2; ++i) {
			xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK);
			if (net.Read[i] != TEST_DATA || state.Transmit.PendingBytes || !state.Transmit.FinAcknowledged) done = false;
		}
	}
	CHECK(done && !net.Overflow && net.Dropped && net.Duplicated && net.Tampered && net.Blocked);
	uint64_t losses = 0, ptos = 0, updates = 0;
	for (size_t i = 0; i < 2; ++i) {
		xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK);
		losses += state.LostPackets; ptos += state.ProbeTimeouts; updates += state.KeyUpdates;
		CHECK(xrtRudpSessionAbort(net.Session[i], XRUDP_CANCELLED) == XRUDP_OK);
		CHECK(xrtRudpSessionAbort(net.Session[i], XRUDP_CANCELLED) == XRUDP_DONE);
		CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK && state.Sent.PendingPackets == 0 && state.Sent.BytesInFlight == 0);
	}
	CHECK(losses && ptos && updates);
	destroy_pair();
#ifdef TEST_SESSION_FAULTS
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtMemDebugSnapshot(&final);
	CHECK(final.LiveBytes == baseline.LiveBytes && final.LiveCount == baseline.LiveCount);
#endif
	printf("xrudp session seed=%u capacity=%u datagram=%u ticks=%u lost=%llu PTO=%llu keys=%llu\n", seed, capacity, datagram, tick,
		(unsigned long long)losses, (unsigned long long)ptos, (unsigned long long)updates);
	return 0;
}
static int append(unsigned char* out, size_t* size, xrudpframetype type, uint8_t flags, const void* body, size_t length)
{
	size_t written; CHECK(xrtRudpFrameAppend(out + *size, XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE - *size,
		&written, type, flags, body, length) == XRUDP_OK); *size += written; return 0;
}
static int deliver_raw(xrudpsession* s, xrudpcipher* peer, const void* plain, size_t size, uint64_t now, xrudpresult expected)
{
	unsigned char packet[XRUDP_PACKET_MAX]; size_t length;
	CHECK(xrtRudpCipherSeal(peer, plain, size, packet, sizeof(packet), &length) == XRUDP_OK);
	CHECK(xrtRudpSessionInput(s, packet, length, now) == expected); return 0;
}
static int raw_session(xrudpcipher** peer, xrudpsession** session, xrudpsessionconfig* cfg)
{
	xrudpcipher* cipher[2] = {NULL,NULL}; CHECK(raw_pair(cipher) == 0);
	CHECK(xrtRudpSessionCreate(cfg, cipher[0], 0, send_packet, &net.Output[0], session) == XRUDP_OK); *peer = cipher[1]; return 0;
}
static int transaction_cases(void)
{
	/* Each invalid last frame must prevent an earlier valid ACK, STREAM and
	 * credit update from committing. Core cancellation drains but never ACKs. */
	for (unsigned int mode = 0; mode < 5; ++mode) {
		memset(&net, 0, sizeof(net)); xrudpsessionconfig cfg; configure(&cfg, 512, 1200);
		xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &cfg) == 0);
		unsigned char plain[XRUDP_PACKET_MAX], body[32] = {0}; size_t size = 0, written; uint64_t due;
		CHECK(xrtRudpSessionWrite(s, "outgoing", 8, &written) == XRUDP_OK && written == 8);
		CHECK(xrtRudpSessionPoll(s, 1000, &due) == XRUDP_OK);
		/* Actual first output is PN 2; ACK its range twice (overlapping frames). */
		put32(body, 0); body[4] = 0; body[5] = 1; put64(body + 8, 2); put64(body + 16, 2);
		CHECK(append(plain, &size, XRUDP_FRAME_ACK, 0, body, 24) == 0);
		CHECK(append(plain, &size, XRUDP_FRAME_ACK, 0, body, 24) == 0);
		put32(body, 1); put64(body + 4, 0); memcpy(body + 12, "read", 4);
		CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 16) == 0);
		put64(body, 900); CHECK(append(plain, &size, XRUDP_FRAME_MAX_DATA, 0, body, 8) == 0);
		if (mode == 0) { /* Future ACK after valid ACK/data/grant. */
			memset(body, 0, sizeof(body)); body[5] = 1; put64(body + 8, 99); put64(body + 16, 99);
			CHECK(append(plain, &size, XRUDP_FRAME_ACK, 0, body, 24) == 0);
		} else if (mode == 1) {
			put32(body, 1); put64(body + 4, 0); memcpy(body + 12, "evil", 4);
			CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 16) == 0);
		} else if (mode == 2) {
			put32(body, 1); put64(body + 4, 3); put32(body + 12, 9);
			CHECK(append(plain, &size, XRUDP_FRAME_RESET, 0, body, 16) == 0);
		} else if (mode == 3) {
			put32(body, 1); put64(body + 4, 512); body[12] = 3;
			CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 13) == 0);
		} else { /* A disabled additional stream cannot leak partial callbacks. */
			put32(body, 3); put64(body + 4, 0); body[12] = 1;
			CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 13) == 0);
		}
		CHECK(deliver_raw(s, peer, plain, size, 2000, XRUDP_MALFORMED) == 0);
		xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK);
		CHECK(state.Reason == XRUDP_MALFORMED && state.Sent.LargestAcked == 0 && state.ReceivedPackets == 0);
		CHECK(state.Receive.HighestOffset == 0 && state.ReceivedConnection == 0 && state.PeerLimit == 512 && !state.AckPending);
		xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	}
	/* A whole valid packet may admit data and RESET its final size atomically,
	 * charging holes only once. */
	memset(&net, 0, sizeof(net)); xrudpsessionconfig cfg; configure(&cfg, 512, 1200);
	xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &cfg) == 0);
	unsigned char plain[XRUDP_PACKET_MAX], body[32] = {0}; size_t size = 0;
	put32(body, 1); put64(body + 4, 0); memcpy(body + 12, "read", 4); CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 16) == 0);
	put32(body, 1); put64(body + 4, 6); put32(body + 12, 42); CHECK(append(plain, &size, XRUDP_FRAME_RESET, 0, body, 16) == 0);
	CHECK(deliver_raw(s, peer, plain, size, 1000, XRUDP_OK) == 0);
	xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK);
	CHECK(state.Receive.Reset && state.Receive.FinalOffset == 6 && state.Receive.BufferedBytes == 0 && state.ReceivedConnection == 6 && state.ReceiveLimit == 518);
	size_t read; CHECK(xrtRudpSessionRead(s, body, sizeof(body), &read) == XRUDP_CANCELLED && read == 0);
	size = 0; put32(body, 1); put64(body + 4, 6); put32(body + 12, 42); CHECK(append(plain, &size, XRUDP_FRAME_RESET, 0, body, 16) == 0);
	CHECK(deliver_raw(s, peer, plain, size, 2000, XRUDP_OK) == 0);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.ReceiveLimit == 518 && state.ReceivedConnection == 6);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer); return 0;
}
static int admission_cases(void)
{
	/* Local work starts at the next driver clock observation, not the previous
	 * idle maintenance tick. Write has no clock argument and may follow a long
	 * quiet interval shorter than the negotiated idle timeout. */
	memset(&net, 0, sizeof(net)); xrudpsessionconfig cfg; configure(&cfg, 512, 1200);
	cfg.ProgressTimeoutUs = 1000000; xrudpcipher* peer; xrudpsession* s;
	CHECK(raw_session(&peer, &s, &cfg) == 0); uint64_t idleDue; size_t idleWritten;
	CHECK(xrtRudpSessionPoll(s, 5000000, &idleDue) == XRUDP_OK);
	CHECK(xrtRudpSessionWrite(s, "new", 3, &idleWritten) == XRUDP_OK && idleWritten == 3);
	CHECK(xrtRudpSessionPoll(s, 10000000, &idleDue) == XRUDP_OK);
	xrudpsessionstate idleState; CHECK(xrtRudpSessionState(s, &idleState) == XRUDP_OK && idleState.ProgressAtUs == 10000000);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	for (unsigned int mode = 0; mode < 3; ++mode) {
		tick = 0; CHECK(new_pair(512, 1200, false) == 0);
		net.Output[0].Block = mode == 0; net.Output[0].OutOfMemory = mode == 1; net.Output[0].Error = mode == 2;
		size_t written; uint64_t due;
		CHECK(xrtRudpSessionWrite(net.Session[0], "data", 4, &written) == XRUDP_OK && written == 4);
		CHECK(xrtRudpSessionPoll(net.Session[0], 1000, &due) == (mode == 2 ? XRUDP_IO_ERROR : XRUDP_OK));
		xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[0], &state) == XRUDP_OK);
		CHECK(state.Sent.LargestSent == 1 && state.Sent.PendingPackets == 0 && state.SentConnection == 0 && state.Transmit.HighestSent == 0);
		if (mode < 2) {
			CHECK(state.Reason == XRUDP_OK && state.OutputBackpressure == 1 && due > 1000 && due <= 2000);
			net.Output[0].Block = net.Output[0].OutOfMemory = false;
			CHECK(xrtRudpSessionPoll(net.Session[0], due, &due) == XRUDP_OK);
			CHECK(xrtRudpSessionState(net.Session[0], &state) == XRUDP_OK && state.Sent.LargestSent >= 3 && state.Transmit.HighestSent == 4);
		}
		destroy_pair();
	}
	return 0;
}
static int overlapping_ack_commit(void)
{
	memset(&net, 0, sizeof(net)); xrudpsessionconfig cfg; configure(&cfg, 1024, 512);
	xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &cfg) == 0);
	unsigned char data[700] = {17}, plain[XRUDP_PACKET_MAX], body[32] = {0}; size_t size = 0, written; uint64_t due;
	CHECK(xrtRudpSessionWrite(s, data, sizeof(data), &written) == XRUDP_OK && written == sizeof(data));
	CHECK(xrtRudpSessionPoll(s, 1000, &due) == XRUDP_OK);
	xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Sent.LargestSent == 3);
	for (size_t i = 0; i < 3; ++i) {
		memset(body, 0, sizeof(body)); body[5] = 1;
		put64(body + 8, i == 1 ? 3 : 2); put64(body + 16, i == 0 ? 2 : 3);
		CHECK(append(plain, &size, XRUDP_FRAME_ACK, 0, body, 24) == 0);
	}
	put32(body, 1); put64(body + 4, 0); memcpy(body + 12, "read", 4);
	CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 16) == 0);
	put64(body, 2000); CHECK(append(plain, &size, XRUDP_FRAME_MAX_DATA, 0, body, 8) == 0);
	CHECK(deliver_raw(s, peer, plain, size, 2000, XRUDP_OK) == 0);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Sent.LargestAcked == 3 && state.Sent.PendingPackets == 0);
	CHECK(state.Congestion.BytesInFlight == 0 && state.Transmit.BaseOffset == sizeof(data) && !state.Transmit.Cancelled);
	CHECK(state.Receive.HighestOffset == 4 && state.PeerLimit == 2000 && state.ReceivedPackets == 1);
	CHECK(xrtRudpSessionRead(s, data, sizeof(data), &written) == XRUDP_OK && written == 4 && memcmp(data, "read", 4) == 0);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.ReceiveLimit == 1028);
	due = 42; CHECK(xrtRudpSessionPoll(s, 1999, &due) == XRUDP_ARGUMENT && due == 42);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.LastNowUs == 2000 && state.Reason == XRUDP_OK);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer); return 0;
}
static int empty_fin(void)
{
	memset(&net, 0, sizeof(net)); tick = 0; xrudpsessionconfig cfg; configure(&cfg, 256, 512);
	cfg.Local.StreamCredit = cfg.Peer.StreamCredit = 0; cfg.Local.ConnectionCredit = cfg.Peer.ConnectionCredit = 0;
	xrudpcipher* cipher[2] = {NULL,NULL}; CHECK(raw_pair(cipher) == 0);
	for (size_t i = 0; i < 2; ++i) {
		net.Output[i].Index = (uint32_t)i;
		CHECK(xrtRudpSessionCreate(&cfg, cipher[i], 0, send_packet, &net.Output[i], &net.Session[i]) == XRUDP_OK);
		CHECK(xrtRudpSessionFinish(net.Session[i]) == XRUDP_OK);
	}
	for (tick = 1; tick < 100; ++tick) {
		for (size_t i = 0; i < TEST_QUEUE; ++i) if (net.Queue[i].Used && net.Queue[i].Due <= tick) {
			session_packet* p = &net.Queue[i]; p->Used = false;
			CHECK(good(xrtRudpSessionInput(net.Session[p->Target], p->Data, p->Size, tick * 1000)));
		}
		for (size_t i = 0; i < 2; ++i) { uint64_t due; CHECK(xrtRudpSessionPoll(net.Session[i], tick * 1000, &due) == XRUDP_OK); }
	}
	for (size_t i = 0; i < 2; ++i) {
		xrudpsessionstate state; unsigned char data[1]; size_t read;
		CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK && state.Transmit.FinAcknowledged && state.Receive.HasFinal);
		CHECK(state.SentConnection == 0 && state.ReceivedConnection == 0 && state.Sent.BytesInFlight == 0);
		CHECK(xrtRudpSessionRead(net.Session[i], data, 1, &read) == XRUDP_DONE && read == 0);
	}
	destroy_pair(); return 0;
}
static int deadlines_and_control(void)
{
	memset(&net, 0, sizeof(net)); xrudpsessionconfig cfg; configure(&cfg, 512, 512);
	cfg.Peer.ConnectionCredit = cfg.Peer.StreamCredit = 0; cfg.ProgressTimeoutUs = 1000000;
	xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &cfg) == 0);
	size_t written; CHECK(xrtRudpSessionWrite(s, "blocked", 7, &written) == XRUDP_OK && written == 7);
	/* Fresh authenticated PING keeps idle alive but does not extend a blocked
	 * application write's useful-progress deadline. */
	unsigned char ping[4] = {XRUDP_FRAME_PING,0,0,0}; uint64_t due;
	CHECK(xrtRudpSessionPoll(s, 0, &due) == XRUDP_OK);
	for (uint64_t now = 100000; now < 1000000; now += 100000) {
		CHECK(deliver_raw(s, peer, ping, sizeof(ping), now, XRUDP_OK) == 0);
		CHECK(xrtRudpSessionPoll(s, now, &due) == XRUDP_OK);
	}
	CHECK(xrtRudpSessionPoll(s, 1000000, &due) == XRUDP_TIMEOUT);
	xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Reason == XRUDP_TIMEOUT);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	/* Authenticated duplicate and forged packets never refresh activity. */
	memset(&net, 0, sizeof(net)); configure(&cfg, 512, 1200); cfg.Local.IdleTimeoutMs = cfg.Peer.IdleTimeoutMs = 1000;
	CHECK(raw_session(&peer, &s, &cfg) == 0);
	unsigned char packet[100]; size_t size;
	CHECK(xrtRudpCipherSeal(peer, ping, 4, packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpSessionInput(s, packet, size, 1000) == XRUDP_OK);
	CHECK(xrtRudpSessionInput(s, packet, size, 500000) == XRUDP_DUPLICATE);
	packet[size - 1] ^= 1; CHECK(xrtRudpSessionInput(s, packet, size, 700000) == XRUDP_AUTH_FAILED);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.LastActivityUs == 1000);
	CHECK(xrtRudpSessionPoll(s, 1001000, &due) == XRUDP_TIMEOUT);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	/* Control feedback is rate limited even with a new authenticated PING every
	 * millisecond, and ACK-only input must never trigger ACK-of-ACK traffic. */
	memset(&net, 0, sizeof(net)); configure(&cfg, 512, 512); cfg.ControlPerSecond = 40; cfg.KeyUpdatePackets = XRUDP_KEY_PACKETS_MAX;
	CHECK(raw_session(&peer, &s, &cfg) == 0);
	for (uint64_t now = 1000; now <= 900000; now += 1000) {
		CHECK(deliver_raw(s, peer, ping, 4, now, XRUDP_OK) == 0);
		CHECK(xrtRudpSessionPoll(s, now, &due) == XRUDP_OK);
	}
	CHECK(net.Output[0].Calls <= 37 && !net.Overflow);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	/* ACK-only and its exact duplicate do not create an ACK loop. */
	memset(&net, 0, sizeof(net)); configure(&cfg, 512, 1200); CHECK(raw_session(&peer, &s, &cfg) == 0);
	unsigned char ack[28] = {XRUDP_FRAME_ACK,0,0,24,0,0,0,0,0,1,0,0};
	put64(ack + 12, 1); put64(ack + 20, 1);
	CHECK(xrtRudpCipherSeal(peer, ack, sizeof(ack), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpSessionInput(s, packet, size, 1000) == XRUDP_OK);
	CHECK(xrtRudpSessionInput(s, packet, size, 2000) == XRUDP_DUPLICATE);
	CHECK(xrtRudpSessionPoll(s, 100000, &due) == XRUDP_OK && net.Output[0].Calls == 0);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	return 0;
}
static int compact_ack(void)
{
	memset(&net, 0, sizeof(net)); xrudpsessionconfig cfg; configure(&cfg, 1024, 512);
	xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &cfg) == 0);
	unsigned char packet[XRUDP_PACKET_MAX], plain[XRUDP_PACKET_MAX], ping[4] = {XRUDP_FRAME_PING,0,0,0}; size_t size;
	for (unsigned int i = 0; i < 64; ++i) {
		CHECK(xrtRudpCipherSeal(peer, ping, 4, packet, sizeof(packet), &size) == XRUDP_OK);
		if (!(i & 1)) CHECK(xrtRudpSessionInput(s, packet, size, 1000) == XRUDP_OK);
	}
	size_t written; uint64_t due; CHECK(xrtRudpSessionWrite(s, "payload", 7, &written) == XRUDP_OK && written == 7);
	CHECK(xrtRudpSessionPoll(s, 2000, &due) == XRUDP_OK);
	bool found = false;
	for (size_t i = 0; i < TEST_QUEUE; ++i) if (net.Queue[i].Used) {
		session_packet* p = &net.Queue[i]; CHECK(p->Size <= 512);
		CHECK(xrtRudpCipherOpen(peer, p->Data, p->Size, plain, sizeof(plain), &size) == XRUDP_OK);
		size_t cursor = 0; xrudpwireframe f; bool ack = false, stream = false;
		while (xrtRudpFrameNext(plain, size, &cursor, &f) == XRUDP_OK) {
			if (f.Type == XRUDP_FRAME_ACK) { CHECK(f.Data[5] <= 23); ack = true; }
			if (f.Type == XRUDP_FRAME_STREAM) { CHECK(f.Size == 19 && memcmp(f.Data + 12, "payload", 7) == 0); stream = true; }
		}
		if (ack && stream) found = true;
	}
	CHECK(found); xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer); return 0;
}

#ifdef TEST_SESSION_FAULTS
static int allocation_faults(void)
{
	for (uint64_t budget = 0; budget <= 5; ++budget) {
		xrudpcipher* cipher[2] = {NULL,NULL}; CHECK(raw_pair(cipher) == 0);
		xrudpsessionconfig cfg; configure(&cfg, 512, 512); xrudpsession* s = (xrudpsession*)(uintptr_t)1;
		xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
		CHECK(xrtMemDebugFailAfter(budget));
		xrudpresult result = xrtRudpSessionCreate(&cfg, cipher[0], 0, send_packet, &net.Output[0], &s);
		bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (budget < 4) {
			CHECK(result == XRUDP_NO_MEMORY && triggered && s == (xrudpsession*)(uintptr_t)1);
			xrudpcipherstate state; CHECK(xrtRudpCipherState(cipher[0], &state) == XRUDP_OK && state.NextPacketNumber == 2);
			xrtClearError(); xrtMemDebugSnapshot(&after); CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
			xrtRudpCipherDestroy(cipher[0]);
		} else {
			CHECK(result == XRUDP_OK && !triggered); size_t reserved;
			CHECK(xrtRudpSessionAllocationSize(&cfg, &reserved) == XRUDP_OK); xrtMemDebugSnapshot(&after);
			CHECK(after.LiveBytes - before.LiveBytes == reserved - xrtRudpCipherAllocationSize()); xrtRudpSessionDestroy(s);
		}
		xrtRudpCipherDestroy(cipher[1]); xrtClearError();
	}
	return 0;
}
#endif
int main(void)
{
	randomState = 1;
	CHECK(transaction_cases() == 0); CHECK(admission_cases() == 0); CHECK(deadlines_and_control() == 0); CHECK(compact_ack() == 0);
	CHECK(overlapping_ack_commit() == 0); CHECK(empty_fin() == 0);
	CHECK(transfer(0x8734bc17, 512, 1200) == 0);
	CHECK(transfer(0x1357a9cf, 1024, 512) == 0);
	CHECK(transfer(0xe6713569, 4096, 1200) == 0);
	CHECK(transfer(0x912b3891, 65536, 1200) == 0);
#ifdef TEST_SESSION_FAULTS
	CHECK(allocation_faults() == 0);
#endif
	puts("xrudp default session: full packet admission, bounded duplex recovery, finite deadlines and UDP backpressure passed");
	return 0;
}
