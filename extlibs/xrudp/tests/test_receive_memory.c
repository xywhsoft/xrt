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
	xrudpreceive* r = NULL;
	xrudpreceivechange change;
	unsigned char input[1024], output[1024];
	size_t before, count;
	memset(input, 0xa7, sizeof(input));
	CHECK(xrtSetAllocator(&allocator));
	fail = true;
	CHECK(xrtRudpReceiveCreate(XRUDP_RECEIVE_CAPACITY_MAX, 1024, &r) == XRUDP_NO_MEMORY && r == NULL);
	fail = false;
	CHECK(xrtRudpReceiveCreate(XRUDP_RECEIVE_CAPACITY_MAX, 1024, &r) == XRUDP_OK);
	fail = true; before = calls;
	/* Maximum fragmentation cannot create metadata allocations. */
	for (size_t i = sizeof(input); i != 0; --i)
		CHECK(xrtRudpReceivePush(r, i - 1, input + i - 1, 1, false, &change) == XRUDP_OK);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_OK && count == sizeof(output));
	CHECK(memcmp(input, output, sizeof(input)) == 0);
	CHECK(xrtRudpReceiveExtend(r, 2048) == XRUDP_OK);
	CHECK(xrtRudpReceiveReset(r, 2048, 5, &change) == XRUDP_OK && change.ConsumedDelta == 1024);
	xrtRudpReceiveDestroy(r);
	CHECK(calls == before);
	puts("xrudp receive memory: allocation failure and allocation-free worst-case fragmentation/read/reset passed");
	return 0;
}
