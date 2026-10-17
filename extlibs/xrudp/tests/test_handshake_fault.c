#include <xrudp.h>
#include <xrt/crypto.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static unsigned char privateKeys[2][32], publicKeys[2][32], key[32] = {3}, scope[16] = {4};
static xrudpaddress address = {4, 40000, 0, {192,0,2,1}};

static void configure(xrudphandshakeconfig* c, size_t side)
{
	memset(c, 0, sizeof(*c)); c->StaticPrivate = privateKeys[side]; c->LocalPublic = publicKeys[side];
	c->PeerPublic = publicKeys[1 - side]; c->Scope = scope; xrtRudpParametersDefault(&c->Parameters);
}

static int exercise(unsigned int budget)
{
	xrudphandshakeconfig c[2]; xrudphandshake *client = NULL, *server = NULL;
	xrudpcipher *ca = NULL, *cb = NULL;
	xrudphandshakeinfo info;
	xmemdebugsnapshot before, after, live;
	unsigned char hello[312], retry[312], first[312], second[312], proof[312], reply[312], bad[312];
	size_t helloSize, retrySize, firstSize, secondSize, proofSize, replySize;
	uint64_t due;
	configure(&c[0], 0); configure(&c[1], 1);
	xrtMemDebugSnapshot(&before);
	CHECK(xrtRudpHandshakeCreate(&c[0], 0, &client) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(client, 0, hello, 312, &helloSize, &due) == XRUDP_OK);
	CHECK(xrtMemDebugFailAfter(0));
	CHECK(xrtRudpHandshakeRetry(key, scope, publicKeys[1], &address, 0, hello, helloSize, retry, 312, &retrySize) == XRUDP_OK);
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
	CHECK(xrtRudpHandshakeInput(client, retry, retrySize, 0) == XRUDP_OK);
	CHECK(xrtRudpHandshakeNext(client, 0, first, 312, &firstSize, &due) == XRUDP_OK);
	memcpy(bad, first, firstSize); bad[160] ^= 1;
	CHECK(xrtMemDebugFailAfter(0));
	CHECK(xrtRudpHandshakeAccept(&c[1], key, NULL, &address, 0, bad, firstSize, &server) == XRUDP_AUTH_FAILED);
	CHECK(server == NULL && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
	CHECK(xrtMemDebugFailAfter(budget));
	xrudpresult result = xrtRudpHandshakeAccept(&c[1], key, NULL, &address, 0, first, firstSize, &server);
	if (budget < 3) {
		CHECK(result == XRUDP_NO_MEMORY && server == NULL && xrtMemDebugFailTriggered());
		xrtMemDebugFailClear(); xrtRudpHandshakeDestroy(client);
	} else {
		CHECK(result == XRUDP_OK && server != NULL); xrtMemDebugFailClear();
		CHECK(xrtRudpHandshakeNext(server, 0, second, 312, &secondSize, &due) == XRUDP_OK);
		memcpy(bad, second, secondSize); bad[secondSize - 1] ^= 1;
		CHECK(xrtMemDebugFailAfter(0));
		CHECK(xrtRudpHandshakeInput(client, bad, secondSize, 0) == XRUDP_AUTH_FAILED && !xrtMemDebugFailTriggered());
		xrtMemDebugFailClear();
		CHECK(xrtMemDebugFailAfter(0));
		CHECK(xrtRudpHandshakeInput(client, second, secondSize, 0) == XRUDP_NO_MEMORY && xrtMemDebugFailTriggered());
		xrtMemDebugFailClear();
		CHECK(xrtRudpHandshakeInfo(client, &info) == XRUDP_OK && info.Phase == XRUDP_HANDSHAKE_PROOF);
		CHECK(xrtRudpHandshakeNext(client, 0, proof, 312, &proofSize, &due) == XRUDP_OK);
		xrtMemDebugSnapshot(&live);
		CHECK(live.LiveBytes - before.LiveBytes <= 2 * xrtRudpHandshakeAllocationSize());
		CHECK(xrtMemDebugFailAfter(0));
		CHECK(xrtRudpHandshakeInput(server, proof, proofSize, 0) == XRUDP_OK);
		CHECK(xrtRudpHandshakeNext(server, 0, reply, 312, &replySize, &due) == XRUDP_OK);
		CHECK(xrtRudpHandshakeInput(client, reply, replySize, 0) == XRUDP_OK);
		CHECK(xrtRudpHandshakeTakeCipher(client, &ca) == XRUDP_OK && xrtRudpHandshakeTakeCipher(server, &cb) == XRUDP_OK);
		CHECK(xrtRudpHandshakeNext(server, 250000, reply, 312, &replySize, &due) == XRUDP_OK);
		CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
		xrtRudpHandshakeDestroy(client); xrtRudpHandshakeDestroy(server);
		xrtRudpCipherDestroy(ca); xrtRudpCipherDestroy(cb);
	}
	xrtMemDebugSnapshot(&after);
	CHECK(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
	return 0;
}

static int invalid_cipher_no_allocation(void)
{
	unsigned char keys[2][32] = {{3},{11}}, ids[2][16] = {{7},{19}};
	unsigned char packet[80], plain[80], ping[4] = {XRUDP_FRAME_PING,0,0,0};
	xrudpcipher *a = NULL, *b = NULL; size_t size, read;
	CHECK(xrtRudpCipherCreate(keys[0], keys[1], ids[0], ids[1], 0, &a) == XRUDP_OK);
	CHECK(xrtRudpCipherCreate(keys[1], keys[0], ids[1], ids[0], 0, &b) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	xrtClearError(); CHECK(xrtMemDebugFailAfter(0));
	for (size_t i = 0; i < size; ++i) for (unsigned int bit = 0; bit < 8; ++bit) {
		packet[i] ^= (unsigned char)(1u << bit);
		CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_AUTH_FAILED && read == 0);
		packet[i] ^= (unsigned char)(1u << bit);
		CHECK(!xrtMemDebugFailTriggered() && xrtGetError() == NULL);
	}
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_DUPLICATE);
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
	xrtRudpCipherDestroy(a); xrtRudpCipherDestroy(b); return 0;
}

int main(void)
{
	for (size_t i = 0; i < 32; ++i) { privateKeys[0][i] = (unsigned char)(i + 17); privateKeys[1][i] = (unsigned char)(i + 83); }
	CHECK(xrtX25519Public(privateKeys[0], publicKeys[0]) && xrtX25519Public(privateKeys[1], publicKeys[1]));
	xrudphandshakeconfig c; xrudphandshake* h = NULL; configure(&c, 0);
	CHECK(xrtMemDebugFailAfter(0));
	CHECK(xrtRudpHandshakeCreate(&c, 0, &h) == XRUDP_NO_MEMORY && h == NULL && xrtMemDebugFailTriggered());
	xrtMemDebugFailClear();
	for (unsigned int budget = 0; budget <= 3; ++budget) CHECK(exercise(budget) == 0);
	CHECK(invalid_cipher_no_allocation() == 0);
	puts("xrudp handshake faults: no allocation before cookie, every constructor failure, retained authenticated progress and balanced lifetimes passed");
	return 0;
}
