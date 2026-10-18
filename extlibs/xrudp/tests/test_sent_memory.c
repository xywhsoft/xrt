#include <xrudp.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static bool fail;
static size_t calls;
static void* alloc(void* context, size_t size)
{
	(void)context; ++calls;
	return fail ? NULL : malloc(size);
}
static void* resize(void* context, void* data, size_t size)
{
	(void)context; (void)data; (void)size; ++calls;
	return NULL;
}
static void release(void* context, void* data) { (void)context; free(data); }

int main(void)
{
	xallocator allocator = {NULL, alloc, resize, release};
	xrudpsentwindow* w = NULL;
	xrudpsentevent event[2];
	xrudpack ack = {0};
	xrudpackchange change;
	size_t before, count;
	CHECK(xrtSetAllocator(&allocator));
	fail = true;
	CHECK(xrtRudpSentCreate(XRUDP_SENT_CAPACITY_MAX, &w) == XRUDP_NO_MEMORY && w == NULL);
	fail = false;
	CHECK(xrtRudpSentCreate(XRUDP_SENT_CAPACITY_MAX, &w) == XRUDP_OK);
	before = calls; fail = true;
	CHECK(xrtRudpSentRecord(w, 1, 100, 100, 1, 10) == XRUDP_OK);
	CHECK(xrtRudpSentRecord(w, 2, 100, 100, 1, 20) == XRUDP_OK);
	ack.Count = 1; ack.Ranges[0].First = ack.Ranges[0].Last = 2;
	CHECK(xrtRudpSentCheckAcknowledge(w, &ack, 200, &count, &change) == XRUDP_OK && count == 1);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 200, event, 2, &count, &change) == XRUDP_OK);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1000, 1200, event, 2, &count) == XRUDP_OK);
	CHECK(xrtRudpSentRecord(w, 3, 1200, 100, 1, 30) == XRUDP_OK);
	CHECK(xrtRudpSentDiscardPath(w, 1300, event, 2, &count) == XRUDP_OK && count == 1 && event[0].Kind == XRUDP_SENT_LOST);
	CHECK(xrtRudpSentRecord(w, 4, 1400, 100, 1, 40) == XRUDP_OK);
	CHECK(xrtRudpSentAbort(w, event, 2, &count) == XRUDP_OK);
	xrtRudpSentDestroy(w);
	CHECK(calls == before);
	puts("xrudp sent memory: constructor failure and allocation-free ACK/loss/cancel passed");
	return 0;
}
