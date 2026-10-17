#include <xrudp.h>
#include <xrt/crypto.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/handshake_vector.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static unsigned char privateKeys[2][32], publicKeys[2][32], cookieKey[32], scope[16] = {7}, context[32] = {9};
static xrudpaddress source = {4, 42000, 0, {198,51,100,2}};
static unsigned int trustCalls;
static bool trust(const unsigned char* publicKey, void* ignored)
{
	(void)ignored; ++trustCalls; return memcmp(publicKey, publicKeys[0], 32) == 0;
}
static void config(xrudphandshakeconfig* c, unsigned int role)
{
	memset(c, 0, sizeof(*c)); c->StaticPrivate = privateKeys[role]; c->PeerPublic = publicKeys[1 - role];
	c->LocalPublic = publicKeys[role]; c->Scope = scope; c->ApplicationContext = context;
	xrtRudpParametersDefault(&c->Parameters);
	if (role) { c->Parameters.StreamCredit = 512; c->Parameters.MaxStreams = 8; c->Trust = trust; }
}
static int flight(xrudphandshake* h, uint64_t now, unsigned char* packet, size_t* size)
{
	uint64_t due;
	CHECK(xrtRudpHandshakeNext(h, now, packet, XRUDP_HANDSHAKE_PACKET_MAX, size, &due) == XRUDP_OK);
	return 0;
}
static int tamper(xrudphandshake* h, const unsigned char* packet, size_t size, uint64_t now)
{
	unsigned char bad[XRUDP_HANDSHAKE_PACKET_MAX];
	xrudphandshakeinfo before, after;
	CHECK(xrtRudpHandshakeInfo(h, &before) == XRUDP_OK);
	for (size_t i = 0; i < size; ++i) {
		memcpy(bad, packet, size); bad[i] ^= 1;
		CHECK(xrtRudpHandshakeInput(h, bad, size, now) == XRUDP_AUTH_FAILED);
		CHECK(xrtRudpHandshakeInfo(h, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
	}
	return 0;
}

static int exchange(void)
{
	xrudphandshakeconfig a, b;
	xrudphandshake *client = NULL, *server = NULL, *invalid = NULL;
	xrudphandshakeinfo info, peer, saved;
	xrudpcipher *ca = NULL, *cb = NULL, *again = NULL;
	unsigned char hello[312], retry[312], first[312], second[312], proof[312], reply[312], copy[312], plain[64];
	size_t helloSize, retrySize, firstSize, secondSize, proofSize, replySize, size, read;
	uint64_t due;
	config(&a, 0); config(&b, 1); trustCalls = 0;
	CHECK(xrtRudpHandshakeCreate(&a, 0, &client) == XRUDP_OK);
	CHECK(xrtRudpHandshakeTakeCipher(client, &ca) == XRUDP_BACKPRESSURE && ca == NULL);
	CHECK(flight(client, 0, hello, &helloSize) == 0 && helloSize == 144);
	CHECK(xrtRudpHandshakeInfo(client, &saved) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(client, 1, copy, sizeof(copy), &size, &due) == XRUDP_TIMEOUT && size == 0 && due == 250000);
	CHECK(xrtRudpHandshakeNext(client, due, copy, 1, &size, &due) == XRUDP_LIMIT && size == 0);
	CHECK(xrtRudpHandshakeInfo(client, &info) == XRUDP_OK && memcmp(&saved, &info, sizeof(info)) == 0);
	CHECK(flight(client, 250000, copy, &size) == 0 && size == helloSize && memcmp(copy, hello, size) == 0);
	CHECK(xrtRudpHandshakeRetry(cookieKey, scope, publicKeys[1], &source, 250000,
		hello, helloSize, retry, sizeof(retry), &retrySize) == XRUDP_OK && retrySize == 136 && retrySize <= helloSize);
	CHECK(tamper(client, retry, retrySize, 250001) == 0);
	CHECK(xrtRudpHandshakeInput(client, retry, retrySize, 250001) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInput(client, retry, retrySize, 250001) == XRUDP_DUPLICATE);
	CHECK(flight(client, 250001, first, &firstSize) == 0 && firstSize == 312);
	CHECK(xrtRudpHandshakeInfo(client, &saved) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(client, 500001, copy, sizeof(copy), &size, &due) == XRUDP_OK);
	CHECK(size == firstSize && memcmp(copy, first, size) == 0);
	xrudpaddress moved = source; moved.Port++;
	CHECK(xrtRudpHandshakeAccept(&b, cookieKey, NULL, &moved, 500002, first, firstSize, &invalid) == XRUDP_AUTH_FAILED);
	CHECK(invalid == NULL && trustCalls == 0);
	unsigned char changedContext[32] = {8}; b.ApplicationContext = changedContext;
	CHECK(xrtRudpHandshakeAccept(&b, cookieKey, NULL, &source, 500002, first, firstSize, &invalid) == XRUDP_AUTH_FAILED);
	b.ApplicationContext = context;
	for (size_t i = 0; i < firstSize; ++i) {
		memcpy(copy, first, firstSize); copy[i] ^= 1;
		CHECK(xrtRudpHandshakeAccept(&b, cookieKey, NULL, &source, 500002, copy, firstSize, &invalid) == XRUDP_AUTH_FAILED);
		CHECK(invalid == NULL && trustCalls == 0);
	}
	CHECK(xrtRudpHandshakeAccept(&b, cookieKey, NULL, &source, 500002, first, firstSize, &server) == XRUDP_OK);
	CHECK(trustCalls == 1);
	CHECK(xrtRudpHandshakeTakeCipher(server, &cb) == XRUDP_BACKPRESSURE && cb == NULL);
	CHECK(flight(server, 500002, second, &secondSize) == 0 && secondSize == 128);
	CHECK(tamper(client, second, secondSize, 500003) == 0);
	CHECK(xrtRudpHandshakeInput(client, second, secondSize, 500003) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInfo(client, &info) == XRUDP_OK && info.Phase == XRUDP_HANDSHAKE_PROOF && info.PeerParameters.StreamCredit == 512);
	CHECK(xrtRudpHandshakeTakeCipher(client, &ca) == XRUDP_BACKPRESSURE && ca == NULL);
	CHECK(flight(client, 500003, proof, &proofSize) == 0 && proofSize == 76);
	/* Unauthenticated size fields must not turn a local scratch limit into a
	 * fatal authenticated protocol error. */
	xrudpwirepacket forged; unsigned char garbage[80] = {0};
	CHECK(xrtRudpPacketDecode(proof, proofSize, &forged) == XRUDP_OK);
	forged.Header.PayloadSize = sizeof(garbage);
	CHECK(xrtRudpPacketEncode(copy, sizeof(copy), &size, &forged.Header, garbage, sizeof(garbage)) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInfo(server, &saved) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInput(server, copy, size, 500004) == XRUDP_AUTH_FAILED);
	CHECK(xrtRudpHandshakeInfo(server, &info) == XRUDP_OK && memcmp(&saved, &info, sizeof(info)) == 0);
	CHECK(tamper(server, proof, proofSize, 500004) == 0);
	CHECK(xrtRudpHandshakeInput(server, proof, proofSize, 500004) == XRUDP_OK);
	CHECK(flight(server, 500004, reply, &replySize) == 0 && replySize == 104);
	CHECK(xrtRudpHandshakeTakeCipher(server, &cb) == XRUDP_OK);
	CHECK(xrtRudpHandshakeTakeCipher(server, &again) == XRUDP_BACKPRESSURE && again == NULL);
	CHECK(xrtRudpHandshakeInput(server, proof, proofSize, 500005) == XRUDP_DUPLICATE);
	CHECK(xrtRudpHandshakeNext(server, 750004, copy, sizeof(copy), &size, &due) == XRUDP_OK && size == replySize && memcmp(copy, reply, size) == 0);
	CHECK(tamper(client, reply, replySize, 750005) == 0);
	CHECK(xrtRudpHandshakeInput(client, reply, replySize, 750005) == XRUDP_OK);
	CHECK(xrtRudpHandshakeTakeCipher(client, &ca) == XRUDP_OK);
	CHECK(xrtRudpHandshakeInfo(client, &info) == XRUDP_OK && info.Phase == XRUDP_HANDSHAKE_ESTABLISHED && info.KeysTaken);
	CHECK(xrtRudpHandshakeInfo(server, &peer) == XRUDP_OK && peer.Phase == XRUDP_HANDSHAKE_ESTABLISHED && peer.KeysTaken);
	CHECK(memcmp(info.LocalId, peer.PeerId, 16) == 0 && memcmp(info.PeerId, peer.LocalId, 16) == 0);
	CHECK(memcmp(info.PeerPublic, publicKeys[1], 32) == 0 && memcmp(peer.PeerPublic, publicKeys[0], 32) == 0);
	CHECK(xrtRudpHandshakeNext(client, 750006, copy, sizeof(copy), &size, &due) == XRUDP_DONE);
	xrudpcipherstate state;
	CHECK(xrtRudpCipherState(ca, &state) == XRUDP_OK && state.NextPacketNumber == 2 && state.LargestReceivedPacket == 1);
	CHECK(xrtRudpCipherState(cb, &state) == XRUDP_OK && state.NextPacketNumber == 2 && state.LargestReceivedPacket == 1);
	static const unsigned char ping[] = {XRUDP_FRAME_PING,0,0,0};
	CHECK(xrtRudpCipherSeal(ca, ping, sizeof(ping), copy, sizeof(copy), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(cb, copy, size, plain, sizeof(plain), &read) == XRUDP_OK && read == sizeof(ping));
	xrtRudpHandshakeDestroy(client); xrtRudpHandshakeDestroy(server);
	CHECK(xrtRudpCipherSeal(cb, ping, sizeof(ping), copy, sizeof(copy), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(ca, copy, size, plain, sizeof(plain), &read) == XRUDP_OK);
	xrtRudpCipherDestroy(ca); xrtRudpCipherDestroy(cb);
	return 0;
}

static int parameters_and_time(void)
{
	xrudpparameters p, decoded, saved;
	unsigned char wire[32], before[32];
	static const unsigned char golden[32] = {0,0,0,0,0,16,0,0,0,1,0,0,0,0,0,16,0,0,117,48,0,0,97,168,0,0,4,176,0,0,0,0};
	xrtRudpParametersDefault(&p);
	CHECK(xrtRudpParametersEncode(&p, wire, sizeof(wire)) == XRUDP_OK && memcmp(wire, golden, 32) == 0);
	CHECK(xrtRudpParametersDecode(wire, sizeof(wire), &decoded) == XRUDP_OK && memcmp(&p, &decoded, sizeof(p)) == 0);
	saved = decoded; wire[31] = 1;
	CHECK(xrtRudpParametersDecode(wire, 32, &decoded) == XRUDP_MALFORMED && memcmp(&decoded, &saved, sizeof(saved)) == 0);
	for (size_t i = 0; i < 32; ++i) CHECK(xrtRudpParametersDecode(golden, i, &decoded) == XRUDP_MALFORMED);
	memset(wire, 0xa5, 32); memcpy(before, wire, 32); p.MaxStreams = 0;
	CHECK(xrtRudpParametersEncode(&p, wire, 32) == XRUDP_ARGUMENT && memcmp(wire, before, 32) == 0);
	xrudphandshakeconfig c; xrudphandshake* h = NULL; xrudphandshakeinfo info;
	config(&c, 0); c.TimeoutUs = 1000000;
	CHECK(xrtRudpHandshakeCreate(&c, UINT64_MAX, &h) == XRUDP_ARGUMENT && h == NULL);
	CHECK(xrtRudpHandshakeCreate(&c, 0, &h) == XRUDP_OK);
	unsigned char packet[312]; size_t size; uint64_t due;
	CHECK(flight(h, 0, packet, &size) == 0);
	CHECK(xrtRudpHandshakeNext(h, 1000000, packet, 312, &size, &due) == XRUDP_TIMEOUT && size == 0);
	CHECK(xrtRudpHandshakeInfo(h, &info) == XRUDP_OK && info.Phase == XRUDP_HANDSHAKE_FAILED && info.Failure == XRUDP_TIMEOUT);
	xrtRudpHandshakeDestroy(h);
	return 0;
}

static int authenticated_violations(void)
{
	/* A custom authenticated peer can bypass the profile's local parameter
	 * encoder. Verify actual peer policy, rather than only our own constructor. */
	for (size_t invalid = 0; invalid < 2; ++invalid) {
		xrudphandshakeconfig serverConfig; config(&serverConfig, 1);
		xrudphandshake* server = NULL; xrudpnoise* noise = NULL; xrudpcipher* cipher = NULL;
		unsigned char hello[96] = {0}, helloPacket[312], retry[312], body[264], packet[312], response[312];
		unsigned char parameters[32], prologue[192]; size_t size, retrySize, noiseSize, responseSize, cursor;
		uint64_t due; xrudpwireheader header; xrudpwirepacket decoded;
		hello[0] = 41; memcpy(hello + 16, publicKeys[1], 32); hello[48] = 51; memcpy(hello + 64, context, 32);
		memset(&header, 0, sizeof(header)); header.Type = XRUDP_PACKET_HELLO; header.PayloadSize = 96;
		header.PacketNumber = 1; memcpy(header.ConnectionId, scope, 16);
		CHECK(xrtRudpPacketEncode(helloPacket, 312, &size, &header, hello, 96) == XRUDP_OK);
		CHECK(xrtRudpHandshakeRetry(cookieKey, scope, publicKeys[1], &source, 0, helloPacket, size, retry, 312, &retrySize) == XRUDP_OK);
		CHECK(retrySize == sizeof(handshake_retry_vector) && memcmp(retry, handshake_retry_vector, retrySize) == 0);
		CHECK(xrtRudpPacketDecode(retry, retrySize, &decoded) == XRUDP_OK);
		static const char domain[] = "XRUDP001/IK/prologue";
		cursor = sizeof(domain) - 1; memcpy(prologue, domain, cursor); memcpy(prologue + cursor, scope, 16); cursor += 16;
		memcpy(prologue + cursor, hello, 96); cursor += 96; memcpy(prologue + cursor, decoded.Payload, 16); cursor += 16;
		unsigned char transcript[32];
		CHECK(xrtSha256(prologue, cursor, transcript) && memcmp(transcript, handshake_prologue_hash, 32) == 0);
		xrudpnoiseconfig n = {0}; n.Role = XRUDP_INITIATOR; n.StaticPrivate = privateKeys[0]; n.PeerPublic = publicKeys[1];
		n.Prologue = prologue; n.PrologueSize = cursor;
		CHECK(xrtRudpNoiseCreate(&n, &noise) == XRUDP_OK);
		CHECK(xrtRudpParametersEncode(&serverConfig.Parameters, parameters, 32) == XRUDP_OK);
		if (invalid == 0) parameters[31] = 1;
		memcpy(body, hello, 96); memcpy(body + 96, decoded.Payload + 48, 40);
		CHECK(xrtRudpNoiseWrite(noise, parameters, 32, body + 136, 128, &noiseSize) == XRUDP_OK);
		header.Type = XRUDP_PACKET_HANDSHAKE; header.PayloadSize = 264; memcpy(header.ConnectionId, decoded.Payload, 16);
		CHECK(xrtRudpPacketEncode(packet, 312, &size, &header, body, 264) == XRUDP_OK);
		if (invalid == 0) {
			CHECK(xrtRudpHandshakeAccept(&serverConfig, cookieKey, NULL, &source, 0, packet, size, &server) == XRUDP_MALFORMED && server == NULL);
		} else {
			CHECK(xrtRudpHandshakeAccept(&serverConfig, cookieKey, NULL, &source, 0, packet, size, &server) == XRUDP_OK);
			CHECK(xrtRudpHandshakeNext(server, 0, response, 312, &responseSize, &due) == XRUDP_OK);
			CHECK(xrtRudpPacketDecode(response, responseSize, &decoded) == XRUDP_OK);
			CHECK(xrtRudpNoiseRead(noise, decoded.Payload, decoded.Header.PayloadSize) == XRUDP_OK);
			CHECK(xrtRudpNoiseCreateCipher(noise, hello, header.ConnectionId, 0, &cipher) == XRUDP_OK);
			unsigned char earlyStream[16] = {XRUDP_FRAME_STREAM,0,0,12,0,0,0,1};
			CHECK(xrtRudpCipherSeal(cipher, earlyStream, sizeof(earlyStream), packet, 312, &size) == XRUDP_OK);
			CHECK(xrtRudpHandshakeInput(server, packet, size, 0) == XRUDP_MALFORMED);
			xrudphandshakeinfo info;
			CHECK(xrtRudpHandshakeInfo(server, &info) == XRUDP_OK && info.Phase == XRUDP_HANDSHAKE_FAILED);
		}
		xrtRudpNoiseDestroy(noise); xrtRudpCipherDestroy(cipher); xrtRudpHandshakeDestroy(server);
	}
	return 0;
}

static uint32_t seed = 0x73618b49;
static uint32_t random_word(void)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}
struct datagram { unsigned char Data[312]; size_t Size; unsigned int Due, Target; bool Used; };
static int lossy(void)
{
	for (unsigned int round = 0; round < 5; ++round) {
		xrudphandshakeconfig cfg[2]; xrudphandshake* h[2] = {NULL,NULL};
		struct datagram queue[32] = {0}; xrudphandshakeinfo info[2];
		unsigned int drops = 0, packets = 0, tick;
		config(&cfg[0], 0); config(&cfg[1], 1); cfg[0].TimeoutUs = cfg[1].TimeoutUs = 30000000;
		CHECK(xrtRudpHandshakeCreate(&cfg[0], 0, &h[0]) == XRUDP_OK);
		bool done = false;
		for (tick = 1; tick < 1200 && !done; ++tick) {
			uint64_t now = (uint64_t)tick * 25000;
			for (size_t i = 0; i < 32; ++i) if (queue[i].Used && queue[i].Due <= tick) {
				struct datagram* d = &queue[i]; d->Used = false;
				xrudpresult result;
				if (d->Target == 1 && h[1] == NULL) {
					xrudpwirepacket parsed; CHECK(xrtRudpPacketDecode(d->Data, d->Size, &parsed) == XRUDP_OK);
					if (parsed.Header.Type == XRUDP_PACKET_HELLO) {
						unsigned char reply[312]; size_t size;
						CHECK(xrtRudpHandshakeRetry(cookieKey, scope, publicKeys[1], &source, now, d->Data, d->Size, reply, 312, &size) == XRUDP_OK);
						if (random_word() % 100 < 35) { ++drops; continue; }
						memcpy(d->Data, reply, size); d->Size = size; d->Target = 0; d->Due = tick + 1 + random_word() % 4; d->Used = true;
					} else {
						CHECK(xrtRudpHandshakeAccept(&cfg[1], cookieKey, NULL, &source, now, d->Data, d->Size, &h[1]) == XRUDP_OK);
					}
				} else {
					result = xrtRudpHandshakeInput(h[d->Target], d->Data, d->Size, now);
					CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE || result == XRUDP_AUTH_FAILED);
				}
			}
			for (unsigned int side = 0; side < 2; ++side) if (h[side] != NULL) {
				unsigned char packet[312]; size_t size; uint64_t due;
				xrudpresult result = xrtRudpHandshakeNext(h[side], now, packet, sizeof(packet), &size, &due);
				CHECK(result == XRUDP_OK || result == XRUDP_TIMEOUT || result == XRUDP_DONE);
				if (result != XRUDP_OK) continue;
				++packets;
				if ((tick >= 8 && tick < 20) || random_word() % 100 < 35) { ++drops; continue; }
				unsigned int copies = random_word() % 3 == 0 ? 2 : 1;
				for (unsigned int copy = 0; copy < copies; ++copy) {
					size_t slot; for (slot = 0; slot < 32 && queue[slot].Used; ++slot) {}
					CHECK(slot < 32);
					memcpy(queue[slot].Data, packet, size); queue[slot].Size = size; queue[slot].Target = 1 - side;
					queue[slot].Due = tick + 1 + random_word() % 4; queue[slot].Used = true;
				}
			}
			done = h[1] != NULL;
			for (size_t side = 0; side < 2 && done; ++side) {
				CHECK(xrtRudpHandshakeInfo(h[side], &info[side]) == XRUDP_OK);
				done = info[side].Phase == XRUDP_HANDSHAKE_ESTABLISHED;
			}
		}
		CHECK(done && drops && packets < 100);
		for (size_t side = 0; side < 2; ++side) xrtRudpHandshakeDestroy(h[side]);
	}
	return 0;
}

int main(void)
{
	for (size_t i = 0; i < 32; ++i) { privateKeys[0][i] = (unsigned char)(i + 7); privateKeys[1][i] = (unsigned char)(i + 71); cookieKey[i] = (unsigned char)(i + 135); }
	CHECK(xrtX25519Public(privateKeys[0], publicKeys[0]) && xrtX25519Public(privateKeys[1], publicKeys[1]));
	CHECK(parameters_and_time() == 0);
	CHECK(exchange() == 0);
	CHECK(authenticated_violations() == 0);
	CHECK(lossy() == 0);
	puts("xrudp handshake: bound Noise/cookie/IDs, all-byte tamper, final DATA proof and lossy cached retransmission passed");
	return 0;
}
