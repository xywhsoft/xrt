#include <xrudp.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_MESSAGE_MEMORY
#include <xrt/memory_debug.h>
#endif

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
static unsigned char payload[XRUDP_MESSAGE_SIZE_MAX], output[XRUDP_MESSAGE_SIZE_MAX];
static uint32_t seed = 0xa4680539;
static uint32_t random_next(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }

static int boundaries(void)
{
	xrudpmessage* m = NULL; size_t size = 177, consumed = 177, allocation;
	xrudpmessagestate before, state;
	CHECK(xrtRudpMessageAllocationSize(XRUDP_MESSAGE_SIZE_MAX + 1, &size) == XRUDP_ARGUMENT && size == 177);
	CHECK(xrtRudpMessageAllocationSize(0, &allocation) == XRUDP_OK && allocation > 0);
	CHECK(xrtRudpMessageCreate(XRUDP_MESSAGE_SIZE_MAX + 1, &m) == XRUDP_ARGUMENT && m == NULL);
	CHECK(xrtRudpMessageCreate(12, &m) == XRUDP_OK);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	CHECK(xrtRudpMessagePeek(m, &size) == XRUDP_BACKPRESSURE && size == 0);
	CHECK(xrtRudpMessagePush(m, NULL, 1, &consumed) == XRUDP_ARGUMENT && consumed == 0);
	CHECK(xrtRudpMessagePush(m, NULL, 0, &consumed) == XRUDP_OK && consumed == 0);
	/* Two records in one input, including an empty first record: suffix remains
	 * with the caller. A complete empty record is distinct from no record. */
	unsigned char records[] = {0,0,0,0, 0,0,0,3, 'a','b','c'};
	CHECK(xrtRudpMessagePush(m, records, sizeof(records), &consumed) == XRUDP_OK && consumed == 4);
	CHECK(xrtRudpMessageState(m, &before) == XRUDP_OK && before.Ready && before.Expected == 0);
	CHECK(xrtRudpMessagePush(m, records, sizeof(records), &consumed) == XRUDP_BACKPRESSURE && consumed == 0);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpMessageState(m, &state) == XRUDP_OK && memcmp(&before, &state, sizeof(state)) == 0);
	CHECK(xrtRudpMessageRead(m, NULL, 0, &size) == XRUDP_OK && size == 0);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_BACKPRESSURE); /* Live boundary is not a new stream. */
	CHECK(xrtRudpMessagePush(m, records + 4, 2, &consumed) == XRUDP_OK && consumed == 2);
	CHECK(xrtRudpMessagePeek(m, &size) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpMessagePush(m, records + 6, 5, &consumed) == XRUDP_OK && consumed == 5);
	CHECK(xrtRudpMessagePeek(m, &size) == XRUDP_OK && size == 3);
	memset(output, 0xa5, 8);
	CHECK(xrtRudpMessageState(m, &before) == XRUDP_OK);
	CHECK(xrtRudpMessageRead(m, NULL, 0, &size) == XRUDP_TRUNCATED && size == 3);
	CHECK(xrtRudpMessageRead(m, output, 2, &size) == XRUDP_TRUNCATED && size == 3 && output[0] == 0xa5);
	CHECK(xrtRudpMessageRead(m, NULL, 3, &size) == XRUDP_ARGUMENT && size == 0);
	CHECK(xrtRudpMessageState(m, &state) == XRUDP_OK && memcmp(&before, &state, sizeof(state)) == 0);
	CHECK(xrtRudpMessageFinish(m) == XRUDP_OK);
	CHECK(xrtRudpMessageFinish(m) == XRUDP_DUPLICATE);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpMessagePush(m, records, 1, &consumed) == XRUDP_CLOSED && consumed == 0);
	CHECK(xrtRudpMessageRead(m, output, 8, &size) == XRUDP_OK && size == 3 && memcmp(output, "abc", 3) == 0);
	CHECK(xrtRudpMessageRead(m, output, 8, &size) == XRUDP_DONE && size == 0);
	CHECK(xrtRudpMessageReset(m) == XRUDP_DONE);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	/* Every incomplete header prefix and body prefix at FIN is malformed. */
	for (size_t n = 1; n < 7; ++n) {
		CHECK(xrtRudpMessagePush(m, records + 4, n, &consumed) == XRUDP_OK && consumed == n);
		CHECK(xrtRudpMessageFinish(m) == XRUDP_MALFORMED);
		CHECK(xrtRudpMessageRead(m, output, 8, &size) == XRUDP_MALFORMED && size == 0);
		CHECK(xrtRudpMessageState(m, &state) == XRUDP_OK && state.Buffered == 0 && state.Expected == 0);
		CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	}
	unsigned char oversized[] = {0xff,0xff,0xff,0xff, 0,0,0,0};
	CHECK(xrtRudpMessagePush(m, oversized, sizeof(oversized), &consumed) == XRUDP_MALFORMED && consumed == 4);
	CHECK(xrtRudpMessagePush(m, records, 4, &consumed) == XRUDP_MALFORMED && consumed == 0);
	CHECK(xrtRudpMessageReset(m) == XRUDP_MALFORMED);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	CHECK(xrtRudpMessagePush(m, records + 4, 6, &consumed) == XRUDP_OK);
	CHECK(xrtRudpMessageReset(m) == XRUDP_OK);
	CHECK(xrtRudpMessageReset(m) == XRUDP_DUPLICATE);
	CHECK(xrtRudpMessageRead(m, output, 8, &size) == XRUDP_CANCELLED && size == 0);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	CHECK(xrtRudpMessageFinish(m) == XRUDP_OK && xrtRudpMessageRead(m, NULL, 0, &size) == XRUDP_DONE);
	xrtRudpMessageDestroy(m);
	CHECK(xrtRudpMessageCreate(0, &m) == XRUDP_OK);
	CHECK(xrtRudpMessagePush(m, records, 4, &consumed) == XRUDP_OK && consumed == 4);
	CHECK(xrtRudpMessageRead(m, NULL, 0, &size) == XRUDP_OK && size == 0);
	CHECK(xrtRudpMessagePush(m, records + 4, 7, &consumed) == XRUDP_MALFORMED && consumed == 4);
	xrtRudpMessageDestroy(m);
	return 0;
}

static int atomic_write(void)
{
	xrudptransmit* t; xrudptransmitstate before, state; xrudptransmitframe frame; xrudptransmitchange change; size_t written;
	CHECK(xrtRudpTransmitCreate(256, UINT64_MAX, &t) == XRUDP_OK);
#ifdef TEST_MESSAGE_MEMORY
	CHECK(xrtMemDebugFailAfter(0));
#endif
	CHECK(xrtRudpTransmitMessageWrite(t, NULL, 1) == XRUDP_ARGUMENT);
	CHECK(xrtRudpTransmitMessageWrite(t, payload, 253) == XRUDP_LIMIT);
	CHECK(xrtRudpTransmitMessageWrite(t, payload, SIZE_MAX) == XRUDP_LIMIT);
	CHECK(xrtRudpTransmitMessageWrite(t, payload, 251) == XRUDP_OK);
	CHECK(xrtRudpTransmitState(t, &before) == XRUDP_OK && before.QueuedEnd == 255);
	CHECK(xrtRudpTransmitMessageWrite(t, NULL, 0) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpTransmitState(t, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpTransmitPeek(t, false, output, 512, &frame) == XRUDP_OK && frame.Size == 255);
	CHECK(output[0] == 0 && output[1] == 0 && output[2] == 0 && output[3] == 251 && memcmp(output + 4, payload, 251) == 0);
	CHECK(xrtRudpTransmitMarkSent(t, 0, 255, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitResolve(t, 0, 255, false, true, &change) == XRUDP_OK);
	/* A wrapping four-byte prefix must be retained intact for retransmission. */
	CHECK(xrtRudpTransmitMessageWrite(t, NULL, 0) == XRUDP_OK);
	CHECK(xrtRudpTransmitMessageWrite(t, payload, 248) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, 512, &frame) == XRUDP_OK && frame.Offset == 255 && frame.Size == 256);
	CHECK(output[0] == 0 && output[3] == 0 && output[7] == 248 && memcmp(output + 8, payload, 248) == 0);
	CHECK(xrtRudpTransmitMarkSent(t, 255, 256, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitResolve(t, 255, 256, false, false, &change) == XRUDP_OK);
	memset(output, 0xa5, 256);
	CHECK(xrtRudpTransmitPeek(t, false, output, 512, &frame) == XRUDP_OK && frame.Offset == 255 && frame.Size == 256 && output[7] == 248);
	CHECK(xrtRudpTransmitFinish(t) == XRUDP_OK);
	CHECK(xrtRudpTransmitMessageWrite(t, NULL, 0) == XRUDP_CLOSED);
	xrtRudpTransmitDestroy(t);
#ifdef TEST_MESSAGE_MEMORY
	CHECK(xrtMemDebugFailAfter(-1));
#endif
	CHECK(xrtRudpTransmitCreate(256, UINT64_MAX, &t) == XRUDP_OK);
	CHECK(xrtRudpTransmitWrite(t, payload, 1, &written) == XRUDP_OK && written == 1);
	CHECK(xrtRudpTransmitCancel(t, &change) == XRUDP_OK && xrtRudpTransmitMessageWrite(t, NULL, 0) == XRUDP_CLOSED);
	xrtRudpTransmitDestroy(t); return 0;
}

static int randomized(uint32_t maximum)
{
	xrudpmessage* m; CHECK(xrtRudpMessageCreate(maximum, &m) == XRUDP_OK);
#ifdef TEST_MESSAGE_MEMORY
	CHECK(xrtMemDebugFailAfter(0));
#endif
	for (size_t n = 0; n < 200; ++n) {
		uint32_t length = n == 0 ? maximum : random_next() % (maximum + 1);
		unsigned char header[4]; for (size_t i = 0; i < 4; ++i) header[3 - i] = (unsigned char)(length >> (i * 8));
		size_t size, consumed;
		for (size_t i = 0; i < 4; ++i) CHECK(xrtRudpMessagePush(m, header + i, 1, &consumed) == XRUDP_OK && consumed == 1);
		for (size_t at = 0; at < length;) {
			size_t count = 1 + random_next() % 4096; if (count > length - at) count = length - at;
			CHECK(xrtRudpMessagePeek(m, &size) == XRUDP_BACKPRESSURE && size == 0);
			CHECK(xrtRudpMessagePush(m, payload + at, count, &consumed) == XRUDP_OK && consumed == count); at += count;
		}
		CHECK(xrtRudpMessagePeek(m, &size) == XRUDP_OK && size == length);
		CHECK(xrtRudpMessageRead(m, output, sizeof(output), &size) == XRUDP_OK && size == length && memcmp(payload, output, length) == 0);
	}
	CHECK(xrtRudpMessageFinish(m) == XRUDP_OK); size_t size; CHECK(xrtRudpMessagePeek(m, &size) == XRUDP_DONE);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	/* The terminal/error paths also run with every further allocation denied. */
	unsigned char bad[4] = {0xff,0xff,0xff,0xff}, empty[4] = {0}; size_t consumed;
	CHECK(xrtRudpMessagePush(m, bad, 4, &consumed) == XRUDP_MALFORMED && consumed == 4);
	CHECK(xrtRudpMessageFinish(m) == XRUDP_MALFORMED && xrtRudpMessageReset(m) == XRUDP_MALFORMED);
	CHECK(xrtRudpMessagePeek(m, &size) == XRUDP_MALFORMED);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	CHECK(xrtRudpMessagePush(m, empty, 1, &consumed) == XRUDP_OK && xrtRudpMessageFinish(m) == XRUDP_MALFORMED);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
	CHECK(xrtRudpMessagePush(m, empty, 4, &consumed) == XRUDP_OK && xrtRudpMessageReset(m) == XRUDP_OK);
	CHECK(xrtRudpMessageRead(m, NULL, 0, &size) == XRUDP_CANCELLED && size == 0);
	CHECK(xrtRudpMessageRestart(m) == XRUDP_OK);
#ifdef TEST_MESSAGE_MEMORY
	CHECK(xrtMemDebugFailAfter(-1));
#endif
	xrtRudpMessageDestroy(m); return 0;
}

int main(void)
{
	for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = (unsigned char)(i * 19 + (i >> 7));
#ifdef TEST_MESSAGE_MEMORY
	xmemdebugsnapshot before, after; xrtClearError(); xrtMemDebugSnapshot(&before);
	xrudpmessage* m = NULL; CHECK(xrtMemDebugFailAfter(0));
	CHECK(xrtRudpMessageCreate(123, &m) == XRUDP_NO_MEMORY && m == NULL);
	CHECK(xrtMemDebugFailAfter(-1)); xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
#endif
	CHECK(boundaries() == 0); CHECK(atomic_write() == 0);
	CHECK(randomized(0) == 0); CHECK(randomized(252) == 0); CHECK(randomized(XRUDP_MESSAGE_SIZE_MAX) == 0);
#ifdef TEST_MESSAGE_MEMORY
	xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
	CHECK(before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
#endif
	puts("xrudp messages: bounded framing, atomic output, fragmented records, truncation, EOF/reset and fixed allocation passed"); return 0;
}
