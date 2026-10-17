#include <xrudp.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static unsigned char sendKey[32], receiveKey[32];
static struct {
	bool Fail, SecretFound;
	struct { void* Data; size_t Size; } Blocks[64];
	size_t AllocCalls, Frees;
} memory;

static void* test_alloc(void* context, size_t size)
{
	(void)context;
	++memory.AllocCalls;
	if (memory.Fail) return NULL;
	for (size_t i = 0; i < 64; ++i) {
		if (memory.Blocks[i].Data == NULL) {
			memory.Blocks[i].Data = malloc(size);
			memory.Blocks[i].Size = size;
			return memory.Blocks[i].Data;
		}
	}
	return NULL;
}

static void* test_realloc(void* context, void* data, size_t size)
{
	(void)context; (void)data; (void)size;
	/* No cipher operation is allowed to resize its fixed allocation. */
	++memory.AllocCalls;
	return NULL;
}

static void scan_secrets(const void* data, size_t size)
{
	const unsigned char* bytes = (const unsigned char*)data;
	for (size_t i = 0; i + 32 <= size; ++i) {
		if (memcmp(bytes + i, sendKey, 32) == 0 || memcmp(bytes + i, receiveKey, 32) == 0)
			memory.SecretFound = true;
	}
}

static void test_free(void* context, void* data)
{
	(void)context;
	for (size_t i = 0; i < 64; ++i) {
		if (data == memory.Blocks[i].Data) {
			scan_secrets(data, memory.Blocks[i].Size);
			free(data);
			memory.Blocks[i].Data = NULL;
			++memory.Frees;
			return;
		}
	}
	memory.SecretFound = true;
}

int main(void)
{
	xallocator allocator = {NULL, test_alloc, test_realloc, test_free};
	xrudpcipher *cipher = NULL;
	unsigned char local[16] = {1}, peer[16] = {2};
	unsigned char packet[XRUDP_PACKET_MAX], ping[] = {1,0,0,0};
	size_t size, calls;
	for (size_t i = 0; i < 32; ++i) { sendKey[i] = (unsigned char)(i + 20); receiveKey[i] = (unsigned char)(i + 70); }
	CHECK(xrtSetAllocator(&allocator));
	memory.Fail = true;
	CHECK(xrtRudpCipherCreate(sendKey, receiveKey, local, peer, 0, &cipher) == XRUDP_NO_MEMORY);
	CHECK(cipher == NULL);
	memory.Fail = false;
	CHECK(xrtRudpCipherCreate(sendKey, receiveKey, local, peer, 0, &cipher) == XRUDP_OK);
	CHECK(cipher != NULL);
	calls = memory.AllocCalls;
	memory.Fail = true;
	CHECK(xrtRudpCipherSeal(cipher, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpCipherUpdateKey(cipher) == XRUDP_OK);
	CHECK(xrtRudpCipherSeal(cipher, ping, sizeof(ping), packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(memory.AllocCalls == calls);
	xrtRudpCipherDestroy(cipher);
	/* XRT retains pool spans/caches: inspect those still-live backing blocks,
	 * as well as blocks released through our Free hook. */
	for (size_t i = 0; i < 64; ++i)
		if (memory.Blocks[i].Data != NULL)
			scan_secrets(memory.Blocks[i].Data, memory.Blocks[i].Size);
	CHECK(memory.AllocCalls == calls && !memory.SecretFound);
	puts("xrudp cipher memory: allocation failure, fixed working set, secret clearing passed");
	return 0;
}
