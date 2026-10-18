#include <xrudp/handshake.h>
#include <xrt/crypto.h>
#include <xrt/random.h>
#include <xrt/memory.h>
#include <string.h>

struct xrudphandshake {
	xrudprole Role;
	xrudphandshakephase Phase;
	xrudpresult Failure;
	xrudpnoise* Noise;
	xrudpcipher* Cipher;
	xrudpparameters Local, Peer;
	uint64_t Deadline, Next, LastNow;
	uint32_t Attempts;
	bool Taken;
	unsigned char Static[32], Pin[32], Scope[16], Client[16], Server[16], Hello[96], Cookie[40], Public[32];
	unsigned char LastInput[XRUDP_HANDSHAKE_PACKET_MAX], Output[XRUDP_HANDSHAKE_PACKET_MAX];
	size_t LastSize, OutputSize;
	xrudpnoisetrustproc Trust;
	void* TrustContext;
};

static bool xrudp__hs_nonzero(const void* data, size_t size)
{
	const unsigned char* p = (const unsigned char*)data; unsigned char bits = 0;
	if (p == NULL) return false;
	for (size_t i = 0; i < size; ++i) bits |= p[i];
	return bits != 0;
}
static void xrudp__hs_put(unsigned char* p, uint64_t value, size_t size)
{
	for (size_t i = 0; i < size; ++i) p[size - 1 - i] = (unsigned char)(value >> (i * 8));
}
static uint64_t xrudp__hs_get(const unsigned char* p, size_t size)
{
	uint64_t value = 0; for (size_t i = 0; i < size; ++i) value = (value << 8) | p[i]; return value;
}
static bool xrudp__hs_parameters(const xrudpparameters* p)
{
	return p != NULL && p->ConnectionCredit <= UINT64_C(67108864) && p->StreamCredit <= 1048576 &&
		p->MaxStreams >= 1 && p->MaxStreams <= 1024 && p->IdleTimeoutMs >= 1000 && p->IdleTimeoutMs <= 3600000 &&
		p->MaxAckDelayUs <= 25000 && p->DatagramSize >= 512 && p->DatagramSize <= XRUDP_PACKET_MAX &&
		p->MaxMessageSize <= XRUDP_MESSAGE_SIZE_MAX;
}
void xrtRudpParametersDefault(xrudpparameters* p)
{
	if (p != NULL) {
		memset(p, 0, sizeof(*p)); p->ConnectionCredit = 1048576; p->StreamCredit = 65536;
		p->MaxStreams = 16; p->IdleTimeoutMs = 30000; p->MaxAckDelayUs = 25000; p->DatagramSize = 1200;
	}
}
xrudpresult xrtRudpParametersEncode(const xrudpparameters* p, void* output, size_t capacity)
{
	unsigned char wire[32];
	if (!xrudp__hs_parameters(p) || output == NULL) return XRUDP_ARGUMENT;
	if (capacity < sizeof(wire)) return XRUDP_LIMIT;
	xrudp__hs_put(wire, p->ConnectionCredit, 8); xrudp__hs_put(wire + 8, p->StreamCredit, 4);
	xrudp__hs_put(wire + 12, p->MaxStreams, 4); xrudp__hs_put(wire + 16, p->IdleTimeoutMs, 4);
	xrudp__hs_put(wire + 20, p->MaxAckDelayUs, 4); xrudp__hs_put(wire + 24, p->DatagramSize, 4);
	xrudp__hs_put(wire + 28, p->MaxMessageSize, 4); memmove(output, wire, sizeof(wire)); return XRUDP_OK;
}
xrudpresult xrtRudpParametersDecode(const void* data, size_t size, xrudpparameters* output)
{
	const unsigned char* p = (const unsigned char*)data; xrudpparameters next;
	if (data == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (size != 32) return XRUDP_MALFORMED;
	memset(&next, 0, sizeof(next)); next.ConnectionCredit = xrudp__hs_get(p, 8);
	next.StreamCredit = (uint32_t)xrudp__hs_get(p + 8, 4); next.MaxStreams = (uint32_t)xrudp__hs_get(p + 12, 4);
	next.IdleTimeoutMs = (uint32_t)xrudp__hs_get(p + 16, 4); next.MaxAckDelayUs = (uint32_t)xrudp__hs_get(p + 20, 4);
	next.DatagramSize = (uint32_t)xrudp__hs_get(p + 24, 4); next.MaxMessageSize = (uint32_t)xrudp__hs_get(p + 28, 4);
	if (!xrudp__hs_parameters(&next)) return XRUDP_MALFORMED;
	*output = next; return XRUDP_OK;
}
size_t xrtRudpHandshakeAllocationSize(void)
{
	return sizeof(xrudphandshake) + xrtRudpNoiseAllocationSize() + xrtRudpCipherAllocationSize();
}
static xrudpresult xrudp__hs_packet(xrudppackettype type, uint64_t number, const void* id,
	const void* data, size_t size, void* output, size_t capacity, size_t* written)
{
	xrudpwireheader h; memset(&h, 0, sizeof(h)); h.Type = type; h.PacketNumber = number;
	h.PayloadSize = (uint16_t)size; memcpy(h.ConnectionId, id, 16);
	return xrtRudpPacketEncode(output, capacity, written, &h, data, size);
}
static bool xrudp__hs_hello(const unsigned char* hello)
{
	return xrudp__hs_nonzero(hello, 16) && xrudp__hs_nonzero(hello + 16, 32) && xrudp__hs_nonzero(hello + 48, 16);
}
static bool xrudp__hs_hash(const void* scope, const unsigned char hello[96], unsigned char digest[32])
{
	unsigned char packet[144]; size_t size;
	return xrudp__hs_packet(XRUDP_PACKET_HELLO, 1, scope, hello, 96, packet, sizeof(packet), &size) == XRUDP_OK &&
		xrtSha256(packet, size, digest);
}
static bool xrudp__hs_id(const unsigned char digest[32], const unsigned char cookie[40], unsigned char id[16])
{
	static const char domain[] = "XRUDP001/server-id";
	xsha256 hash; unsigned char output[32];
	xrtSha256Init(&hash);
	bool ok = xrtSha256Update(&hash, domain, sizeof(domain) - 1) && xrtSha256Update(&hash, digest, 32) &&
		xrtSha256Update(&hash, cookie, 40) && xrtSha256Final(&hash, output);
	if (ok) memcpy(id, output, 16);
	xrtSecureZero(&hash, sizeof(hash)); xrtSecureZero(output, sizeof(output)); return ok && xrudp__hs_nonzero(id, 16);
}
static size_t xrudp__hs_prologue(const xrudphandshake* h, unsigned char output[192])
{
	static const char domain[] = "XRUDP001/IK/prologue";
	size_t size = sizeof(domain) - 1;
	memcpy(output, domain, size); memcpy(output + size, h->Scope, 16); size += 16;
	memcpy(output + size, h->Hello, 96); size += 96; memcpy(output + size, h->Server, 16); return size + 16;
}
static xrudpresult xrudp__hs_config(const xrudphandshakeconfig* c, bool responder, uint64_t now, uint64_t* deadline)
{
	uint64_t timeout;
	if (c == NULL || !xrudp__hs_nonzero(c->StaticPrivate, 32) || !xrudp__hs_nonzero(c->Scope, 16) ||
		(!responder && !xrudp__hs_nonzero(c->PeerPublic, 32)) ||
		(responder && (!xrudp__hs_nonzero(c->LocalPublic, 32) || (c->PeerPublic == NULL && c->Trust == NULL))) ||
		!xrudp__hs_parameters(&c->Parameters)) return XRUDP_ARGUMENT;
	timeout = c->TimeoutUs ? c->TimeoutUs : XRUDP_HANDSHAKE_DEFAULT_TIMEOUT_US;
	if (timeout < 1000000 || timeout > XRUDP_HANDSHAKE_MAX_TIMEOUT_US || now > UINT64_MAX - timeout) return XRUDP_ARGUMENT;
	*deadline = now + timeout; return XRUDP_OK;
}
static xrudphandshake* xrudp__hs_alloc(const xrudphandshakeconfig* c, xrudprole role, uint64_t now, uint64_t deadline)
{
	xrudphandshake* h = (xrudphandshake*)xrtMalloc(sizeof(*h));
	if (h == NULL) return NULL;
	memset(h, 0, sizeof(*h)); h->Role = role; h->Local = c->Parameters;
	h->Deadline = deadline; h->Next = h->LastNow = now; h->Trust = c->Trust; h->TrustContext = c->TrustContext;
	memcpy(h->Static, c->StaticPrivate, 32); memcpy(h->Scope, c->Scope, 16);
	if (c->PeerPublic != NULL) memcpy(h->Pin, c->PeerPublic, 32);
	return h;
}
void xrtRudpHandshakeDestroy(xrudphandshake* h)
{
	if (h != NULL) { xrtRudpNoiseDestroy(h->Noise); xrtRudpCipherDestroy(h->Cipher); xrtSecureZero(h, sizeof(*h)); xrtFree(h); }
}
static xrudpresult xrudp__hs_fail(xrudphandshake* h, xrudpresult result)
{
	h->Phase = XRUDP_HANDSHAKE_FAILED; h->Failure = result;
	xrtRudpNoiseDestroy(h->Noise); h->Noise = NULL; xrtRudpCipherDestroy(h->Cipher); h->Cipher = NULL;
	xrtSecureZero(h->Static, sizeof(h->Static)); return result;
}
xrudpresult xrtRudpHandshakeCreate(const xrudphandshakeconfig* c, uint64_t now, xrudphandshake** output)
{
	uint64_t deadline;
	xrudpresult result = xrudp__hs_config(c, false, now, &deadline);
	if (result != XRUDP_OK || output == NULL) return result != XRUDP_OK ? result : XRUDP_ARGUMENT;
	xrudphandshake* h = xrudp__hs_alloc(c, XRUDP_INITIATOR, now, deadline);
	if (h == NULL) return XRUDP_NO_MEMORY;
	if (!xrtSecureRandom(h->Client, 16) || !xrtSecureRandom(h->Hello + 48, 16) ||
		!xrudp__hs_nonzero(h->Client, 16) || !xrudp__hs_nonzero(h->Hello + 48, 16) ||
		xrtConstTimeEqual(h->Client, h->Scope, 16)) { xrtRudpHandshakeDestroy(h); return XRUDP_IO_ERROR; }
	memcpy(h->Hello, h->Client, 16); memcpy(h->Hello + 16, h->Pin, 32);
	if (c->ApplicationContext != NULL) memcpy(h->Hello + 64, c->ApplicationContext, 32);
	result = xrudp__hs_packet(XRUDP_PACKET_HELLO, 1, h->Scope, h->Hello, 96, h->Output, sizeof(h->Output), &h->OutputSize);
	if (result != XRUDP_OK) { xrtRudpHandshakeDestroy(h); return result; }
	h->Phase = XRUDP_HANDSHAKE_RETRY; *output = h; return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeRetry(const void* key, const void* scope, const void* publicKey,
	const xrudpaddress* source, uint64_t now, const void* hello, size_t size,
	void* output, size_t capacity, size_t* written)
{
	xrudpwirepacket packet; unsigned char body[88], digest[32], cookie[40]; size_t resultSize;
	if (written != NULL) *written = 0;
	if (!xrudp__hs_nonzero(key, 32) || !xrudp__hs_nonzero(scope, 16) || !xrudp__hs_nonzero(publicKey, 32) ||
		output == NULL || written == NULL || source == NULL || hello == NULL) return XRUDP_ARGUMENT;
	if (xrtRudpPacketDecode(hello, size, &packet) != XRUDP_OK || packet.Header.Type != XRUDP_PACKET_HELLO ||
		packet.Header.PacketNumber != 1 || packet.Header.PayloadSize != 96 || !xrudp__hs_hello(packet.Payload) ||
		!xrtConstTimeEqual(packet.Header.ConnectionId, scope, 16) || !xrtConstTimeEqual(packet.Payload + 16, publicKey, 32) ||
		xrtConstTimeEqual(packet.Payload, scope, 16)) return XRUDP_AUTH_FAILED;
	if (!xrtSha256(hello, size, digest)) return XRUDP_IO_ERROR;
	xrudpresult result = xrtRudpCookieCreate(key, scope, digest, source, now, cookie, sizeof(cookie));
	if (result != XRUDP_OK) return result;
	if (!xrudp__hs_id(digest, cookie, body)) return XRUDP_IO_ERROR;
	if (xrtConstTimeEqual(body, packet.Payload, 16) || xrtConstTimeEqual(body, scope, 16)) return XRUDP_AUTH_FAILED;
	memcpy(body + 16, digest, 32); memcpy(body + 48, cookie, 40);
	result = xrudp__hs_packet(XRUDP_PACKET_RETRY, 1, packet.Payload, body, sizeof(body), output, capacity, &resultSize);
	if (result == XRUDP_OK) *written = resultSize;
	return result;
}
static xrudpresult xrudp__hs_noise(xrudphandshake* h)
{
	unsigned char prologue[192];
	xrudpnoiseconfig c; memset(&c, 0, sizeof(c)); c.Role = h->Role; c.StaticPrivate = h->Static;
	c.PeerPublic = xrudp__hs_nonzero(h->Pin, 32) ? h->Pin : NULL;
	c.PrologueSize = xrudp__hs_prologue(h, prologue); c.Prologue = prologue;
	c.Trust = h->Trust; c.TrustContext = h->TrustContext;
	return xrtRudpNoiseCreate(&c, &h->Noise);
}
static xrudpresult xrudp__hs_peer(xrudphandshake* h)
{
	unsigned char wire[XRUDP_NOISE_PARAMETERS_MAX]; size_t size;
	xrudpnoiseinfo info;
	xrudpresult result = xrtRudpNoisePeerParameters(h->Noise, wire, sizeof(wire), &size);
	if (result == XRUDP_OK) result = xrtRudpParametersDecode(wire, size, &h->Peer);
	if (result != XRUDP_OK) return result;
	result = xrtRudpNoiseInfo(h->Noise, &info);
	if (result == XRUDP_OK) memcpy(h->Public, info.PeerPublic, 32);
	return result;
}
static xrudpresult xrudp__hs_prepare(xrudphandshake* h)
{
	unsigned char plain[40] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	xrudpresult result;
	if (h->Cipher == NULL) {
		result = xrtRudpNoiseCreateCipher(h->Noise,
			h->Role == XRUDP_INITIATOR ? h->Client : h->Server,
			h->Role == XRUDP_INITIATOR ? h->Server : h->Client, 0, &h->Cipher);
		if (result != XRUDP_OK) return result;
		xrtRudpNoiseDestroy(h->Noise); h->Noise = NULL; xrtSecureZero(h->Static, sizeof(h->Static));
	}
	if (h->Role == XRUDP_INITIATOR && h->OutputSize == 0)
		return xrtRudpCipherSeal(h->Cipher, plain, 12, h->Output, sizeof(h->Output), &h->OutputSize);
	return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeAccept(const xrudphandshakeconfig* c, const void* key, const void* previous,
	const xrudpaddress* source, uint64_t now, const void* data, size_t size, xrudphandshake** output)
{
	uint64_t deadline; unsigned char digest[32], id[16], body[80], params[32];
	xrudpwirepacket packet; size_t noiseSize;
	xrudpresult result = xrudp__hs_config(c, true, now, &deadline);
	if (result != XRUDP_OK || output == NULL || data == NULL || source == NULL || !xrudp__hs_nonzero(key, 32) ||
		(previous != NULL && !xrudp__hs_nonzero(previous, 32)))
		return result != XRUDP_OK ? result : XRUDP_ARGUMENT;
	if (xrtRudpPacketDecode(data, size, &packet) != XRUDP_OK || packet.Header.Type != XRUDP_PACKET_HANDSHAKE ||
		packet.Header.PacketNumber != 1 || packet.Header.PayloadSize != 264 || !xrudp__hs_hello(packet.Payload) ||
		!xrtConstTimeEqual(packet.Payload + 16, c->LocalPublic, 32) ||
		(c->ApplicationContext != NULL && !xrtConstTimeEqual(c->ApplicationContext, packet.Payload + 64, 32))) return XRUDP_AUTH_FAILED;
	if (!xrudp__hs_hash(c->Scope, packet.Payload, digest)) return XRUDP_IO_ERROR;
	result = xrtRudpCookieVerify(key, previous, c->Scope, digest, source, now, 0, packet.Payload + 96, 40);
	if (result != XRUDP_OK) return result;
	if (!xrudp__hs_id(digest, packet.Payload + 96, id)) return XRUDP_IO_ERROR;
	if (!xrtConstTimeEqual(id, packet.Header.ConnectionId, 16) || xrtConstTimeEqual(id, c->Scope, 16) ||
		xrtConstTimeEqual(id, packet.Payload, 16)) return XRUDP_AUTH_FAILED;
	xrudphandshake* h = xrudp__hs_alloc(c, XRUDP_RESPONDER, now, deadline);
	if (h == NULL) return XRUDP_NO_MEMORY;
	memcpy(h->Server, id, 16); memcpy(h->Client, packet.Payload, 16); memcpy(h->Hello, packet.Payload, 96);
	result = xrudp__hs_noise(h);
	if (result == XRUDP_OK) result = xrtRudpNoiseRead(h->Noise, packet.Payload + 136, 128);
	if (result == XRUDP_OK) result = xrtRudpParametersEncode(&h->Local, params, sizeof(params));
	if (result == XRUDP_OK) result = xrtRudpNoiseWrite(h->Noise, params, sizeof(params), body, sizeof(body), &noiseSize);
	if (result == XRUDP_OK) result = xrudp__hs_peer(h);
	if (result == XRUDP_OK) result = xrudp__hs_packet(XRUDP_PACKET_HANDSHAKE, 2, h->Client, body, noiseSize,
		h->Output, sizeof(h->Output), &h->OutputSize);
	if (result == XRUDP_OK) result = xrudp__hs_prepare(h);
	if (result != XRUDP_OK) { xrtRudpHandshakeDestroy(h); return result; }
	memcpy(h->LastInput, data, size); h->LastSize = size; h->Phase = XRUDP_HANDSHAKE_PROOF;
	*output = h; return XRUDP_OK;
}
static xrudpresult xrudp__hs_retry_input(xrudphandshake* h, const xrudpwirepacket* packet, uint64_t now)
{
	unsigned char digest[32], id[16], params[32], body[264]; size_t noiseSize, packetSize;
	xrudphandshake candidate = *h;
	if (packet->Header.Type != XRUDP_PACKET_RETRY || packet->Header.PacketNumber != 1 || packet->Header.PayloadSize != 88 ||
		!xrtConstTimeEqual(packet->Header.ConnectionId, h->Client, 16)) return XRUDP_AUTH_FAILED;
	if (!xrudp__hs_hash(h->Scope, h->Hello, digest) || !xrudp__hs_id(digest, packet->Payload + 48, id)) return XRUDP_IO_ERROR;
	if (!xrtConstTimeEqual(digest, packet->Payload + 16, 32) || !xrtConstTimeEqual(id, packet->Payload, 16) ||
		xrtConstTimeEqual(id, h->Client, 16) || xrtConstTimeEqual(id, h->Scope, 16)) return XRUDP_AUTH_FAILED;
	memcpy(candidate.Server, id, 16); memcpy(candidate.Cookie, packet->Payload + 48, 40);
	xrudpresult result = xrudp__hs_noise(&candidate);
	if (result == XRUDP_OK) result = xrtRudpParametersEncode(&h->Local, params, sizeof(params));
	if (result == XRUDP_OK) result = xrtRudpNoiseWrite(candidate.Noise, params, sizeof(params), body + 136, 128, &noiseSize);
	if (result == XRUDP_OK) {
		memcpy(body, h->Hello, 96); memcpy(body + 96, candidate.Cookie, 40);
		result = xrudp__hs_packet(XRUDP_PACKET_HANDSHAKE, 1, candidate.Server, body, 136 + noiseSize,
			candidate.Output, sizeof(candidate.Output), &packetSize);
	}
	if (result == XRUDP_OK) {
		candidate.OutputSize = packetSize; candidate.Next = now; candidate.Attempts = 0; candidate.Phase = XRUDP_HANDSHAKE_NOISE;
		xrtSecureZero(candidate.Static, sizeof(candidate.Static)); *h = candidate;
	} else xrtRudpNoiseDestroy(candidate.Noise);
	xrtSecureZero(&candidate, sizeof(candidate)); return result;
}
static bool xrudp__hs_proof(const unsigned char* plain, size_t size, bool reply)
{
	static const unsigned char prefix[12] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	if (size != (reply ? 40u : 12u) || memcmp(plain, prefix, 12) != 0) return false;
	if (!reply) return true;
	static const unsigned char ackHead[12] = {XRUDP_FRAME_ACK,0,0,24,0,0,0,0,0,1,0,0};
	return memcmp(plain + 12, ackHead, 12) == 0 && xrudp__hs_get(plain + 24, 8) == 1 && xrudp__hs_get(plain + 32, 8) == 1;
}
xrudpresult xrtRudpHandshakeInput(xrudphandshake* h, const void* data, size_t size, uint64_t now)
{
	xrudpwirepacket packet; xrudpresult result;
	if (h == NULL || data == NULL || now < h->LastNow) return XRUDP_ARGUMENT;
	if (h->Phase == XRUDP_HANDSHAKE_FAILED) return h->Failure;
	if (h->Phase != XRUDP_HANDSHAKE_ESTABLISHED && now >= h->Deadline) return xrudp__hs_fail(h, XRUDP_TIMEOUT);
	if (size == h->LastSize && memcmp(data, h->LastInput, size) == 0) return XRUDP_DUPLICATE;
	if (size > sizeof(h->LastInput) || xrtRudpPacketDecode(data, size, &packet) != XRUDP_OK) return XRUDP_AUTH_FAILED;
	if (h->Phase == XRUDP_HANDSHAKE_ESTABLISHED) return XRUDP_AUTH_FAILED;
	if (h->Phase == XRUDP_HANDSHAKE_RETRY) result = xrudp__hs_retry_input(h, &packet, now);
	else if (h->Phase == XRUDP_HANDSHAKE_NOISE) {
		if (packet.Header.Type != XRUDP_PACKET_HANDSHAKE || packet.Header.PacketNumber != 2 || packet.Header.PayloadSize != 80 ||
			!xrtConstTimeEqual(packet.Header.ConnectionId, h->Client, 16)) return XRUDP_AUTH_FAILED;
		result = xrtRudpNoiseRead(h->Noise, packet.Payload, 80);
		if (result == XRUDP_OK) {
			result = xrudp__hs_peer(h);
			if (result != XRUDP_OK) return xrudp__hs_fail(h, result);
			h->Phase = XRUDP_HANDSHAKE_PROOF; h->OutputSize = 0; h->Attempts = 0; h->Next = now;
			result = xrudp__hs_prepare(h);
		}
	} else {
		/* Capacity/length is not authenticated until Open verifies the tag. Never
		 * classify a caller scratch LIMIT as authenticated peer misconduct. */
		unsigned char plain[XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE]; size_t length;
		if (h->Cipher == NULL) { result = xrudp__hs_prepare(h); if (result != XRUDP_OK) return result; }
		if (packet.Header.Type != XRUDP_PACKET_DATA || packet.Header.PacketNumber != 1 || packet.Header.KeyEpoch != 1)
			return XRUDP_AUTH_FAILED;
		result = xrtRudpCipherOpen(h->Cipher, data, size, plain, sizeof(plain), &length);
		if (result == XRUDP_MALFORMED) return xrudp__hs_fail(h, XRUDP_MALFORMED);
		if (result != XRUDP_OK) return result;
		if (!xrudp__hs_proof(plain, length, h->Role == XRUDP_INITIATOR)) return xrudp__hs_fail(h, XRUDP_MALFORMED);
		if (h->Role == XRUDP_RESPONDER) {
			unsigned char reply[40] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1,
				XRUDP_FRAME_ACK,0,0,24,0,0,0,0,0,1,0,0};
			xrudp__hs_put(reply + 24, 1, 8); xrudp__hs_put(reply + 32, 1, 8);
			result = xrtRudpCipherSeal(h->Cipher, reply, sizeof(reply), h->Output, sizeof(h->Output), &h->OutputSize);
			if (result != XRUDP_OK) return xrudp__hs_fail(h, result);
			h->Next = now; h->Attempts = 0;
		} else h->OutputSize = 0;
		h->Phase = XRUDP_HANDSHAKE_ESTABLISHED;
	}
	if (result == XRUDP_OK) { memcpy(h->LastInput, data, size); h->LastSize = size; h->LastNow = now; }
	return result;
}
xrudpresult xrtRudpHandshakeNext(xrudphandshake* h, uint64_t now, void* output,
	size_t capacity, size_t* written, uint64_t* due)
{
	if (written != NULL) *written = 0;
	if (h == NULL || output == NULL || written == NULL || due == NULL || now < h->LastNow) return XRUDP_ARGUMENT;
	if (h->Phase == XRUDP_HANDSHAKE_FAILED) return h->Failure;
	if (now >= h->Deadline) {
		*due = h->Deadline;
		return h->Phase == XRUDP_HANDSHAKE_ESTABLISHED ? XRUDP_DONE : xrudp__hs_fail(h, XRUDP_TIMEOUT);
	}
	if (h->Phase == XRUDP_HANDSHAKE_PROOF && h->Cipher == NULL) {
		xrudpresult result = xrudp__hs_prepare(h); if (result != XRUDP_OK) return result;
	}
	if (h->OutputSize == 0 || h->Attempts >= 16) {
		*due = h->Deadline;
		return h->Phase == XRUDP_HANDSHAKE_ESTABLISHED ? XRUDP_DONE : XRUDP_TIMEOUT;
	}
	*due = h->Next;
	if (now < h->Next) return XRUDP_TIMEOUT;
	if (capacity < h->OutputSize) return XRUDP_LIMIT;
	memmove(output, h->Output, h->OutputSize); *written = h->OutputSize;
	uint64_t delay = UINT64_C(250000) << (h->Attempts < 3 ? h->Attempts : 3);
	++h->Attempts; h->Next = delay > h->Deadline - now ? h->Deadline : now + delay; h->LastNow = now;
	*due = h->Next; return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeInfo(const xrudphandshake* h, xrudphandshakeinfo* output)
{
	xrudphandshakeinfo info;
	if (h == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&info, 0, sizeof(info)); info.Role = h->Role; info.Phase = h->Phase; info.Failure = h->Failure;
	info.Attempts = h->Attempts; info.DeadlineUs = h->Deadline; info.NextSendUs = h->Next; info.KeysTaken = h->Taken;
	memcpy(info.LocalId, h->Role == XRUDP_INITIATOR ? h->Client : h->Server, 16);
	memcpy(info.PeerId, h->Role == XRUDP_INITIATOR ? h->Server : h->Client, 16);
	memcpy(info.ApplicationContext, h->Hello + 64, 32);
	if (h->Phase == XRUDP_HANDSHAKE_PROOF || h->Phase == XRUDP_HANDSHAKE_ESTABLISHED) {
		info.PeerParameters = h->Peer; memcpy(info.PeerPublic, h->Public, 32);
	}
	*output = info; return XRUDP_OK;
}
xrudpresult xrtRudpHandshakeTakeCipher(xrudphandshake* h, xrudpcipher** output)
{
	if (h == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (h->Phase != XRUDP_HANDSHAKE_ESTABLISHED || h->Taken) return XRUDP_BACKPRESSURE;
	*output = h->Cipher; h->Cipher = NULL; h->Taken = true; return XRUDP_OK;
}
