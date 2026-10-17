#include "../src/internal/xrudp_noise.h"
#include "fixtures/noise_ik_vector.h"
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static int make_pair(xrudpnoise** init, xrudpnoise** resp, bool deterministic)
{
	unsigned char initPublic[32];
	xrudpnoiseconfig a = {0}, b = {0};
	CHECK(xrtX25519Public(vector_init_static, initPublic));
	a.Role = XRUDP_INITIATOR; a.StaticPrivate = vector_init_static;
	a.PeerPublic = vector_init_remote_static;
	a.Prologue = vector_init_prologue; a.PrologueSize = sizeof(vector_init_prologue);
	b = a; b.Role = XRUDP_RESPONDER; b.StaticPrivate = vector_resp_static;
	b.PeerPublic = initPublic;
	CHECK(xrudp__noise_create(&a, deterministic ? vector_init_ephemeral : NULL, init) == XRUDP_OK);
	CHECK(xrudp__noise_create(&b, deterministic ? vector_resp_ephemeral : NULL, resp) == XRUDP_OK);
	return 0;
}

static int vector_test(void)
{
	xrudpnoise *a, *b;
	xrudpcipher *ca = NULL, *cb = NULL, *again = NULL;
	unsigned char message[XRUDP_NOISE_MESSAGE_MAX], output[XRUDP_NOISE_MESSAGE_MAX];
	unsigned char local[16] = {1}, peer[16] = {2}, zeros[32] = {0}, nonce[12] = {0};
	size_t size, read;
	xrudpnoiseinfo ia, ib;
	const unsigned char* payloads[] = {vector_message2_payload,vector_message3_payload,
		vector_message4_payload,vector_message5_payload};
	const unsigned char* ciphertexts[] = {vector_message2_ciphertext,vector_message3_ciphertext,
		vector_message4_ciphertext,vector_message5_ciphertext};
	const size_t lengths[] = {sizeof(vector_message2_payload),sizeof(vector_message3_payload),
		sizeof(vector_message4_payload),sizeof(vector_message5_payload)};
	static const unsigned char ping[] = {1,0,0,0};
	CHECK(make_pair(&a, &b, true) == 0);
	CHECK(xrtRudpNoiseCreateCipher(a, local, peer, 0, &ca) == XRUDP_BACKPRESSURE && ca == NULL);
	CHECK(xrtRudpNoiseWrite(a, vector_message0_payload, sizeof(vector_message0_payload),
		message, sizeof(message), &size) == XRUDP_OK);
	CHECK(size == sizeof(vector_message0_ciphertext) && memcmp(message, vector_message0_ciphertext, size) == 0);
	CHECK(xrtRudpNoiseWrite(a, NULL, 0, output, sizeof(output), &read) == XRUDP_BACKPRESSURE && read == 0);
	CHECK(xrtRudpNoiseRead(b, message, size) == XRUDP_OK);
	CHECK(xrtRudpNoisePeerParameters(b, output, sizeof(output), &read) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpNoiseRead(b, message, size) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpNoiseWrite(b, vector_message1_payload, sizeof(vector_message1_payload),
		message, sizeof(message), &size) == XRUDP_OK);
	CHECK(size == sizeof(vector_message1_ciphertext) && memcmp(message, vector_message1_ciphertext, size) == 0);
	CHECK(xrtRudpNoiseRead(a, message, size) == XRUDP_OK);
	CHECK(xrtRudpNoiseInfo(a, &ia) == XRUDP_OK && xrtRudpNoiseInfo(b, &ib) == XRUDP_OK);
	CHECK(ia.Complete && ib.Complete && ia.PeerVerified && ib.PeerVerified);
	CHECK(memcmp(ia.HandshakeHash, ib.HandshakeHash, 32) == 0);
	CHECK(memcmp(a->SendKey, b->ReceiveKey, 32) == 0 && memcmp(a->ReceiveKey, b->SendKey, 32) == 0);
	CHECK(memcmp(a->StaticPrivate, zeros, 32) == 0 && memcmp(b->EphemeralPrivate, zeros, 32) == 0);
	/* The published vector's four transport messages independently validate
	 * Split direction and nonce behavior, beyond the two handshake flights. */
	for (size_t i = 0; i < 4; ++i) {
		nonce[4] = (unsigned char)(i / 2);
		CHECK(xrtChaCha20Poly1305Seal(i % 2 == 0 ? a->SendKey : b->SendKey,
			nonce, NULL, 0, payloads[i], lengths[i], output, sizeof(output)));
		CHECK(memcmp(output, ciphertexts[i], lengths[i] + XRUDP_TAG_SIZE) == 0);
	}
	CHECK(xrtRudpNoisePeerParameters(a, output, sizeof(output), &read) == XRUDP_OK);
	CHECK(read == sizeof(vector_message1_payload) && memcmp(output, vector_message1_payload, read) == 0);
	CHECK(xrtRudpNoisePeerParameters(b, output, sizeof(output), &read) == XRUDP_OK);
	CHECK(read == sizeof(vector_message0_payload) && memcmp(output, vector_message0_payload, read) == 0);
	CHECK(xrtRudpNoiseCreateCipher(a, local, peer, 0, &ca) == XRUDP_OK);
	CHECK(xrtRudpNoiseCreateCipher(a, local, peer, 0, &again) == XRUDP_DONE && again == NULL);
	CHECK(memcmp(a->SendKey, zeros, 32) == 0 && memcmp(a->ReceiveKey, zeros, 32) == 0);
	CHECK(xrtRudpNoiseCreateCipher(b, peer, local, 0, &cb) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(ca, ping, sizeof(ping), message, sizeof(message), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(cb, message, size, output, sizeof(output), &read) == XRUDP_OK);
	CHECK(read == sizeof(ping) && memcmp(output, ping, read) == 0);
	xrtRudpCipherDestroy(ca); xrtRudpCipherDestroy(cb);
	xrtRudpNoiseDestroy(a); xrtRudpNoiseDestroy(b);
	return 0;
}

static int transaction_test(void)
{
	xrudpnoise *a, *b, saved;
	unsigned char message[XRUDP_NOISE_MESSAGE_MAX], output[XRUDP_NOISE_MESSAGE_MAX];
	unsigned char before[XRUDP_NOISE_MESSAGE_MAX];
	size_t size, written;
	CHECK(make_pair(&a, &b, true) == 0);
	saved = *a; memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
	CHECK(xrtRudpNoiseWrite(a, NULL, 0, output, 1, &written) == XRUDP_LIMIT && written == 0);
	CHECK(memcmp(a, &saved, sizeof(saved)) == 0 && memcmp(output, before, sizeof(output)) == 0);
	CHECK(xrtRudpNoiseWrite(a, vector_message0_payload, sizeof(vector_message0_payload), message, sizeof(message), &size) == XRUDP_OK);
	saved = *b;
	for (size_t i = 0; i < size; ++i) {
		message[i] ^= 1;
		CHECK(xrtRudpNoiseRead(b, message, size) == XRUDP_AUTH_FAILED);
		CHECK(memcmp(b, &saved, sizeof(saved)) == 0);
		message[i] ^= 1;
	}
	for (size_t i = 0; i < 96; ++i) {
		CHECK(xrtRudpNoiseRead(b, message, i) == XRUDP_TRUNCATED);
		CHECK(memcmp(b, &saved, sizeof(saved)) == 0);
	}
	CHECK(xrtRudpNoiseRead(b, message, size) == XRUDP_OK);
	CHECK(xrtRudpNoiseWrite(b, vector_message1_payload, sizeof(vector_message1_payload), message, sizeof(message), &size) == XRUDP_OK);
	saved = *a;
	for (size_t i = 0; i < size; ++i) {
		message[i] ^= 1;
		CHECK(xrtRudpNoiseRead(a, message, size) == XRUDP_AUTH_FAILED);
		CHECK(memcmp(a, &saved, sizeof(saved)) == 0);
		message[i] ^= 1;
	}
	CHECK(xrtRudpNoiseRead(a, message, size) == XRUDP_OK);
	xrtRudpNoiseDestroy(a); xrtRudpNoiseDestroy(b);
	return 0;
}

static bool trust_allowed = false;
static size_t trust_calls;
static bool trust(const unsigned char* key, void* context)
{
	(void)key; (void)context;
	++trust_calls;
	return trust_allowed;
}

static int policy_and_boundaries(void)
{
	xrudpnoise *a, *b, *invalid = NULL;
	xrudpnoiseconfig config = {0};
	unsigned char message[XRUDP_NOISE_MESSAGE_MAX], output[XRUDP_NOISE_MESSAGE_MAX];
	unsigned char params[XRUDP_NOISE_PARAMETERS_MAX] = {0};
	size_t size, written;
	CHECK(xrtRudpNoiseCreate(&config, &invalid) == XRUDP_ARGUMENT && invalid == NULL);
	config.Role = XRUDP_INITIATOR; config.StaticPrivate = vector_init_static;
	CHECK(xrtRudpNoiseCreate(&config, &invalid) == XRUDP_ARGUMENT && invalid == NULL);
	config.Role = XRUDP_RESPONDER; config.StaticPrivate = vector_resp_static;
	CHECK(xrtRudpNoiseCreate(&config, &invalid) == XRUDP_ARGUMENT && invalid == NULL);
	config.Trust = trust;
	config.Prologue = vector_init_prologue; config.PrologueSize = sizeof(vector_init_prologue);
	CHECK(xrudp__noise_create(&config, vector_resp_ephemeral, &b) == XRUDP_OK);
	config.Role = XRUDP_INITIATOR; config.StaticPrivate = vector_init_static;
	config.PeerPublic = vector_init_remote_static; config.Trust = NULL;
	CHECK(xrudp__noise_create(&config, vector_init_ephemeral, &a) == XRUDP_OK);
	CHECK(xrtRudpNoiseWrite(a, NULL, 0, message, sizeof(message), &size) == XRUDP_OK && size == 96);
	message[size - 1] ^= 1;
	CHECK(xrtRudpNoiseRead(b, message, size) == XRUDP_AUTH_FAILED && trust_calls == 0);
	message[size - 1] ^= 1;
	CHECK(xrtRudpNoiseRead(b, message, size) == XRUDP_AUTH_FAILED && trust_calls == 1);
	trust_allowed = true;
	CHECK(xrtRudpNoiseRead(b, message, size) == XRUDP_OK && trust_calls == 2);
	CHECK(xrtRudpNoiseWrite(b, NULL, 0, message, sizeof(message), &size) == XRUDP_OK && size == 48);
	CHECK(xrtRudpNoiseRead(a, message, size) == XRUDP_OK);
	CHECK(xrtRudpNoisePeerParameters(a, NULL, 0, &written) == XRUDP_OK && written == 0);
	xrtRudpNoiseDestroy(a); xrtRudpNoiseDestroy(b);
	CHECK(make_pair(&a, &b, false) == 0);
	CHECK(xrtRudpNoiseWrite(a, params, sizeof(params), message, sizeof(message), &size) == XRUDP_OK);
	CHECK(size == XRUDP_NOISE_MESSAGE_MAX && xrtRudpNoiseRead(b, message, size) == XRUDP_OK);
	CHECK(xrtRudpNoiseWrite(b, params, sizeof(params), message, sizeof(message), &size) == XRUDP_OK);
	CHECK(xrtRudpNoiseRead(a, message, size) == XRUDP_OK);
	CHECK(xrtRudpNoisePeerParameters(a, output, sizeof(output), &written) == XRUDP_OK && written == sizeof(params));
	CHECK(memcmp(output, params, written) == 0);
	xrtRudpNoiseDestroy(a); xrtRudpNoiseDestroy(b);
	return 0;
}

static int pin_and_prologue(void)
{
	xrudpnoiseconfig config = {0};
	xrudpnoise *b, *a, *second;
	xrudpnoiseinfo info;
	unsigned char pin[32], message[XRUDP_NOISE_MESSAGE_MAX], another[XRUDP_NOISE_MESSAGE_MAX];
	size_t size, otherSize;
	CHECK(xrtX25519Public(vector_init_static, pin));
	config.Role = XRUDP_RESPONDER; config.StaticPrivate = vector_resp_static;
	config.PeerPublic = pin; config.Prologue = vector_init_prologue;
	config.PrologueSize = sizeof(vector_init_prologue);
	pin[0] ^= 1;
	CHECK(xrudp__noise_create(&config, vector_resp_ephemeral, &b) == XRUDP_OK);
	CHECK(xrtRudpNoiseRead(b, vector_message0_ciphertext, sizeof(vector_message0_ciphertext)) == XRUDP_AUTH_FAILED);
	CHECK(xrtRudpNoiseInfo(b, &info) == XRUDP_OK && !info.PeerVerified && info.MessageIndex == 0);
	xrtRudpNoiseDestroy(b);
	pin[0] ^= 1;
	config.Prologue = "wrong session"; config.PrologueSize = 13;
	CHECK(xrudp__noise_create(&config, vector_resp_ephemeral, &b) == XRUDP_OK);
	CHECK(xrtRudpNoiseRead(b, vector_message0_ciphertext, sizeof(vector_message0_ciphertext)) == XRUDP_AUTH_FAILED);
	CHECK(xrtRudpNoiseInfo(b, &info) == XRUDP_OK && !info.PeerVerified && info.MessageIndex == 0);
	xrtRudpNoiseDestroy(b);
	config.Role = XRUDP_INITIATOR; config.StaticPrivate = vector_init_static;
	config.PeerPublic = vector_init_remote_static; config.Prologue = NULL; config.PrologueSize = 0;
	CHECK(xrtRudpNoiseCreate(&config, &a) == XRUDP_OK);
	CHECK(xrtRudpNoiseCreate(&config, &second) == XRUDP_OK);
	CHECK(xrtRudpNoiseWrite(a, NULL, 0, message, sizeof(message), &size) == XRUDP_OK);
	CHECK(xrtRudpNoiseWrite(second, NULL, 0, another, sizeof(another), &otherSize) == XRUDP_OK);
	CHECK(size == otherSize && memcmp(message, another, 32) != 0);
	xrtRudpNoiseDestroy(a); xrtRudpNoiseDestroy(second);
	return 0;
}

int main(void)
{
	if (vector_test() || transaction_test() || policy_and_boundaries() || pin_and_prologue()) return 1;
	puts("xrudp Noise IK: published independent vectors, atomic flights, trust and bounds passed");
	return 0;
}
