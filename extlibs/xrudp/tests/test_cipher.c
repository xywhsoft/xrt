#include <xrudp.h>
#include <xrt/crypto.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static const unsigned char keyA[32] = {1}, keyB[32] = {2};
static const unsigned char idA[16] = {3}, idB[16] = {4};
static const unsigned char ping[] = {1,0,0,0};
/* Independent Python cryptography/OpenSSL ChaCha20Poly1305 fixture; synthetic
 * key 01 followed by 31 zero bytes, nonce LE64(1), header from protocol.md.
 * No Python or OpenSSL dependency is linked into the product or this C test. */
static const unsigned char goldenCipher[] = {
	0x17,0x56,0x10,0x45,0xf7,0xa6,0x7e,0x5d,0x9d,0xe0,
	0x71,0x63,0x3d,0x6b,0xd1,0x12,0x30,0xc8,0x19,0xdd
};
static const unsigned char goldenRekeyCipher[] = {
	0x92,0x56,0x60,0x34,0x1f,0x4e,0x37,0x2a,0x1c,0xf1,
	0x5f,0xca,0x4d,0xd0,0x74,0x4e,0xd6,0x80,0x5f,0xf1
};

static int pair(xrudpcipher** a, xrudpcipher** b, uint64_t limit)
{
	CHECK(xrtRudpCipherCreate(keyA, keyB, idA, idB, limit, a) == XRUDP_OK);
	CHECK(xrtRudpCipherCreate(keyB, keyA, idB, idA, limit, b) == XRUDP_OK);
	return 0;
}

static int basic_tests(void)
{
	xrudpcipher *a, *b, *invalid = NULL;
	xrudpcipherstate before, after;
	unsigned char packet[XRUDP_PACKET_MAX], original[XRUDP_PACKET_MAX];
	unsigned char plain[XRUDP_PACKET_MAX], saved[XRUDP_PACKET_MAX];
	size_t size, read;
	CHECK(xrtRudpCipherCreate(keyA, keyA, idA, idB, 0, &invalid) == XRUDP_ARGUMENT && invalid == NULL);
	CHECK(xrtRudpCipherCreate(keyA, keyB, idA, idB, UINT64_MAX, &invalid) == XRUDP_ARGUMENT && invalid == NULL);
	CHECK(pair(&a, &b, 0) == 0);
	CHECK(xrtRudpCipherState(a, &before) == XRUDP_OK);
	memset(packet, 0xa5, sizeof(packet)); memcpy(original, packet, sizeof(original));
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, 1, &size) == XRUDP_LIMIT);
	CHECK(size == 0 && memcmp(packet, original, sizeof(packet)) == 0);
	CHECK(xrtRudpCipherState(a, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(size == XRUDP_HEADER_SIZE + sizeof(ping) + XRUDP_TAG_SIZE);
	CHECK(memcmp(packet + XRUDP_HEADER_SIZE, goldenCipher, sizeof(goldenCipher)) == 0);
	memcpy(original, packet, size);
	CHECK(xrtRudpCipherState(b, &before) == XRUDP_OK);
	memset(plain, 0x5a, sizeof(plain)); memcpy(saved, plain, sizeof(saved));
	/* Every bit of every header/body byte is protected or structurally rejected.
	 * Rejections must not consume a packet number or alter output. */
	for (size_t i = 0; i < size; ++i) {
		for (unsigned int bit = 0; bit < 8; ++bit) {
			packet[i] ^= (unsigned char)(1u << bit);
			CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) != XRUDP_OK);
			CHECK(read == 0 && memcmp(plain, saved, sizeof(plain)) == 0);
			CHECK(xrtRudpCipherState(b, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
			packet[i] ^= (unsigned char)(1u << bit);
		}
	}
	CHECK(memcmp(packet, original, size) == 0);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, 1, &read) == XRUDP_LIMIT && read == 0);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(read == sizeof(ping) && memcmp(plain, ping, sizeof(ping)) == 0);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_DUPLICATE);
	/* Other direction has independent keys and packet numbers. */
	CHECK(xrtRudpCipherSeal(b, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(memcmp(packet + XRUDP_HEADER_SIZE, original + XRUDP_HEADER_SIZE,
		size - XRUDP_HEADER_SIZE) != 0);
	CHECK(xrtRudpCipherOpen(a, packet, size, packet, sizeof(packet), &read) == XRUDP_OK);
	CHECK(read == sizeof(ping) && memcmp(packet, ping, sizeof(ping)) == 0);
	xrtRudpCipherDestroy(a); xrtRudpCipherDestroy(b);
	xrtRudpCipherDestroy(NULL);
	return 0;
}

static int key_update_tests(void)
{
	xrudpcipher *a, *b;
	xrudpcipherstate state, before, after;
	unsigned char old[XRUDP_PACKET_MAX], packet[XRUDP_PACKET_MAX], plain[XRUDP_PACKET_MAX];
	unsigned char confirm[16], body[4] = {0,0,0,2};
	size_t oldSize, size, read, confirmSize;
	CHECK(pair(&a, &b, 3) == 0);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), old, sizeof(old), &oldSize) == XRUDP_OK);
	CHECK(xrtRudpCipherUpdateKey(a) == XRUDP_OK);
	CHECK(xrtRudpCipherUpdateKey(a) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(memcmp(packet + XRUDP_HEADER_SIZE, goldenRekeyCipher, sizeof(goldenRekeyCipher)) == 0);
	CHECK(xrtRudpCipherState(b, &before) == XRUDP_OK);
	/* A forged next-key packet cannot trigger an epoch transition. */
	packet[size - 1] ^= 1;
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_AUTH_FAILED);
	CHECK(xrtRudpCipherState(b, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
	packet[size - 1] ^= 1;
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(xrtRudpCipherState(b, &state) == XRUDP_OK && state.ReceiveEpoch == 2 && state.PreviousReceiveEpoch == 1);
	CHECK(xrtRudpCipherOpen(b, old, oldSize, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(xrtRudpFrameAppend(confirm, sizeof(confirm), &confirmSize, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(b, confirm, confirmSize, packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(a, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(xrtRudpCipherState(a, &state) == XRUDP_OK && !state.SendKeyPending && state.NextPacketNumber == 3);
	CHECK(xrtRudpCipherUpdateKey(a) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(xrtRudpCipherState(b, &state) == XRUDP_OK && state.ReceiveEpoch == 3 && state.PreviousReceiveEpoch == 2);
	xrtRudpCipherRetirePrevious(b);
	CHECK(xrtRudpCipherState(b, &state) == XRUDP_OK && state.PreviousReceiveEpoch == 0);
	/* Key usage limits force rotation; failed sealing does not consume sequence. */
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherState(a, &before) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_LIMIT && size == 0);
	CHECK(xrtRudpCipherState(a, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
	xrtRudpCipherDestroy(a); xrtRudpCipherDestroy(b);
	return 0;
}

static int confirmation_tests(void)
{
	xrudpcipher *a, *b;
	unsigned char frame[16], packet[XRUDP_PACKET_MAX], plain[XRUDP_PACKET_MAX];
	unsigned char body[4] = {0,0,0,3};
	size_t frameSize, size, read;
	xrudpcipherstate before, after;
	CHECK(pair(&a, &b, 0) == 0);
	CHECK(xrtRudpCipherUpdateKey(a) == XRUDP_OK);
	CHECK(xrtRudpFrameAppend(frame, sizeof(frame), &frameSize, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(b, frame, frameSize, packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherState(a, &before) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(a, packet, size, plain, sizeof(plain), &read) == XRUDP_MALFORMED);
	CHECK(xrtRudpCipherState(a, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
	/* A stale confirmation cannot release a pending newer send key. */
	body[3] = 1;
	CHECK(xrtRudpFrameAppend(frame, sizeof(frame), &frameSize, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(b, frame, frameSize, packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(a, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(xrtRudpCipherState(a, &after) == XRUDP_OK && after.SendKeyPending);
	xrtRudpCipherDestroy(a); xrtRudpCipherDestroy(b);
	return 0;
}

static int authenticated_invalid_payload(void)
{
	xrudpcipher *a, *b;
	xrudpwireheader header;
	xrudpcipherstate state;
	unsigned char packet[XRUDP_PACKET_MAX] = {0}, plain[16], saved[16];
	unsigned char nonce[12] = {0,0,0,0,1,0,0,0,0,0,0,0};
	unsigned char invalid[4] = {255,0,0,0};
	size_t size, read;
	CHECK(pair(&a, &b, 0) == 0);
	memset(&header, 0, sizeof(header));
	header.Type = XRUDP_PACKET_DATA; header.KeyEpoch = 1;
	header.PacketNumber = 1; header.PayloadSize = sizeof(invalid) + XRUDP_TAG_SIZE;
	memcpy(header.ConnectionId, idB, 16);
	CHECK(xrtRudpPacketEncode(packet, sizeof(packet), &size, &header,
		packet + XRUDP_HEADER_SIZE, header.PayloadSize) == XRUDP_OK);
	CHECK(xrtChaCha20Poly1305Seal(keyA, nonce, packet, XRUDP_HEADER_SIZE,
		invalid, sizeof(invalid), packet + XRUDP_HEADER_SIZE, header.PayloadSize));
	memset(plain, 0xa5, sizeof(plain)); memcpy(saved, plain, sizeof(saved));
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_MALFORMED);
	CHECK(read == 0 && memcmp(plain, saved, sizeof(saved)) == 0);
	CHECK(xrtRudpCipherState(b, &state) == XRUDP_OK && state.LargestReceivedPacket == 0);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	xrtRudpCipherDestroy(a); xrtRudpCipherDestroy(b);
	return 0;
}

static int retirement_and_size(void)
{
	xrudpcipher *a, *b;
	unsigned char old[XRUDP_PACKET_MAX], packet[XRUDP_PACKET_MAX], plain[XRUDP_PACKET_MAX];
	unsigned char frame[XRUDP_PACKET_MAX], data[XRUDP_PACKET_MAX] = {0};
	size_t size, oldSize, read, frameSize;
	CHECK(pair(&a, &b, 0) == 0);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), old, sizeof(old), &oldSize) == XRUDP_OK);
	CHECK(xrtRudpCipherUpdateKey(a) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(a, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	xrtRudpCipherRetirePrevious(b);
	CHECK(xrtRudpCipherOpen(b, old, oldSize, plain, sizeof(plain), &read) == XRUDP_AUTH_FAILED);
	/* Largest possible frame payload still fits one protected datagram. */
	CHECK(xrtRudpFrameAppend(frame, sizeof(frame), &frameSize, XRUDP_FRAME_PADDING, 0,
		data, XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE - XRUDP_FRAME_HEADER_SIZE) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(a, frame, frameSize, packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(size == XRUDP_PACKET_MAX);
	CHECK(xrtRudpCipherOpen(b, packet, size, plain, sizeof(plain), &read) == XRUDP_OK);
	CHECK(read == frameSize && memcmp(plain, frame, frameSize) == 0);
	xrtRudpCipherDestroy(a); xrtRudpCipherDestroy(b);
	return 0;
}

int main(void)
{
	if (basic_tests() || key_update_tests() || confirmation_tests() ||
		authenticated_invalid_payload() || retirement_and_size()) return 1;
	puts("xrudp cipher: all-bit tamper, no poisoning, independent directions, rotation and limits passed");
	return 0;
}
