#include <xrudp.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static bool fail;
static size_t calls;
static void* alloc(void* context, size_t size)
{
	(void)context; ++calls; return fail ? NULL : malloc(size);
}
static void* resize(void* context, void* data, size_t size)
{
	(void)context; (void)data; (void)size; ++calls; return NULL;
}
static void release(void* context, void* data) { (void)context; free(data); }

int main(void)
{
	xallocator allocator = {NULL, alloc, resize, release};
	xrudptransmit* t = NULL;
	xrudptransmitframe frame;
	xrudptransmitchange change;
	unsigned char input[256], output[256];
	size_t before, written;
	memset(input, 0xa7, sizeof(input));
	CHECK(xrtSetAllocator(&allocator));
	fail = true;
	CHECK(xrtRudpTransmitCreate(XRUDP_TRANSMIT_CAPACITY_MAX, 256, &t) == XRUDP_NO_MEMORY && t == NULL);
	fail = false;
	CHECK(xrtRudpTransmitCreate(XRUDP_TRANSMIT_CAPACITY_MAX, 256, &t) == XRUDP_OK);
	fail = true; before = calls;
	CHECK(xrtRudpTransmitWrite(t, input, sizeof(input), &written) == XRUDP_OK && written == sizeof(input));
	CHECK(xrtRudpTransmitFinish(t) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK);
	CHECK(memcmp(output, input, sizeof(input)) == 0 && frame.Size == 256 && frame.Fin);
	CHECK(xrtRudpTransmitMarkSent(t, 0, 256, true, &change) == XRUDP_OK);
	for (size_t i = 0; i < 256; ++i)
		CHECK(xrtRudpTransmitResolve(t, i, 1, false, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK);
	CHECK(xrtRudpTransmitResolve(t, 0, 256, true, true, &change) == XRUDP_OK);
	CHECK(change.ReleasedBytes == 256 && change.NewFinAcknowledged);
	CHECK(xrtRudpTransmitCancel(t, &change) == XRUDP_OK);
	xrtRudpTransmitDestroy(t);
	CHECK(calls == before);
	puts("xrudp transmit memory: constructor failure and allocation-free write/feedback/FIN/cancel passed");
	return 0;
}
