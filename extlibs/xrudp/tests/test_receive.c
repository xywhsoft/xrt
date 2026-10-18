#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static unsigned char source[8192];

static int boundaries(void)
{
	xrudpreceive* r = NULL;
	xrudpreceivestate state, before;
	xrudpreceivechange change, saved;
	unsigned char output[512], bad[40];
	size_t count = 9, allocation = 987;
	CHECK(xrtRudpReceiveAllocationSize(255, &allocation) == XRUDP_ARGUMENT && allocation == 987);
	CHECK(xrtRudpReceiveAllocationSize(0, &allocation) == XRUDP_OK && allocation > 65536 + 8192);
	CHECK(xrtRudpReceiveCreate(257, 0, &r) == XRUDP_ARGUMENT && r == NULL);
	CHECK(xrtRudpReceiveCreate(XRUDP_RECEIVE_CAPACITY_MAX * 2, 0, &r) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceiveCreate(256, 257, &r) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceiveCreate(256, 0, &r) == XRUDP_OK);
	memset(&change, 0xa5, sizeof(change)); saved = change;
	CHECK(xrtRudpReceivePush(r, 0, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(memcmp(&change, &saved, sizeof(change)) == 0);
	CHECK(xrtRudpReceiveExtend(r, 256) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 1, source + 1, 255, false, &change) == XRUDP_OK);
	CHECK(change.NewBytes == 255 && change.HighestDelta == 256);
	CHECK(xrtRudpReceivePeek(r, 0, output, sizeof(output), &count) == XRUDP_BACKPRESSURE && count == 0);
	CHECK(xrtRudpReceiveState(r, &before) == XRUDP_OK);
	CHECK(xrtRudpReceivePeek(r, 1, output, sizeof(output), &count) == XRUDP_OK && count == 255 && memcmp(output, source + 1, count) == 0);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpReceivePeek(r, 257, output, sizeof(output), &count) == XRUDP_ARGUMENT && count == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_BACKPRESSURE && count == 0);
	CHECK(xrtRudpReceiveState(r, &before) == XRUDP_OK);
	CHECK(xrtRudpReceiveCheck(r, 0, source, 1, false, &change) == XRUDP_OK && change.NewBytes == 1);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpReceivePush(r, 0, source, 1, false, &change) == XRUDP_OK && change.HighestDelta == 0);
	CHECK(xrtRudpReceiveRead(r, output, 17, &count) == XRUDP_OK && count == 17 && memcmp(output, source, 17) == 0);
	CHECK(xrtRudpReceivePeek(r, 16, output, sizeof(output), &count) == XRUDP_ARGUMENT && count == 0);
	CHECK(xrtRudpReceivePush(r, 256, source + 256, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveExtend(r, 274) == XRUDP_ARGUMENT); /* Physical window is only 273. */
	CHECK(xrtRudpReceiveExtend(r, 273) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 256, source + 256, 17, false, &change) == XRUDP_OK && change.NewBytes == 17);
	CHECK(xrtRudpReceivePeek(r, 250, output, sizeof(output), &count) == XRUDP_OK && count == 23 && memcmp(output, source + 250, count) == 0);
	CHECK(xrtRudpReceivePush(r, 0, source, 17, false, &change) == XRUDP_DUPLICATE);
	CHECK(change.NewBytes == 0 && change.HighestDelta == 0);
	memcpy(bad, source + 10, sizeof(bad)); bad[20] ^= 1;
	CHECK(xrtRudpReceiveState(r, &before) == XRUDP_OK);
	memset(&change, 0xa5, sizeof(change)); saved = change;
	CHECK(xrtRudpReceivePush(r, 10, bad, sizeof(bad), false, &change) == XRUDP_MALFORMED);
	CHECK(memcmp(&change, &saved, sizeof(change)) == 0);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpReceivePush(r, UINT64_MAX, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 0, NULL, 1, false, &change) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceivePush(r, 0, source, XRUDP_STREAM_CHUNK_MAX + 1, false, &change) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceivePush(r, 272, NULL, 0, true, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 273, NULL, 0, true, &change) == XRUDP_OK && change.NewFinal == 1);
	CHECK(xrtRudpReceivePush(r, 272, NULL, 0, true, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 273, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 273, NULL, 0, true, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_OK && count == 256);
	CHECK(memcmp(output, source + 17, count) == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_DONE && count == 0);
	CHECK(xrtRudpReceivePeek(r, 273, output, sizeof(output), &count) == XRUDP_DONE && count == 0);
	CHECK(xrtRudpReceiveReset(r, 273, 9, &change) == XRUDP_DUPLICATE && change.ConsumedDelta == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_DONE);
	xrtRudpReceiveDestroy(r);
	CHECK(xrtRudpReceiveCreate(256, 256, &r) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 10, source + 10, 10, false, &change) == XRUDP_OK);
	memcpy(bad, source, 20); bad[15] ^= 1;
	CHECK(xrtRudpReceiveState(r, &before) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 0, bad, 20, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_BACKPRESSURE && count == 0);
	CHECK(xrtRudpReceivePush(r, 0, source, 20, false, &change) == XRUDP_OK && change.NewBytes == 10);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_OK && count == 20);
	CHECK(memcmp(output, source, count) == 0);
	xrtRudpReceiveDestroy(r);
	CHECK(xrtRudpReceiveCreate(256, 256, &r) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 100, source + 100, 10, false, &change) == XRUDP_OK);
	CHECK(xrtRudpReceiveReset(r, 109, 7, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 200, 7, &change) == XRUDP_OK);
	CHECK(change.HighestDelta == 90 && change.ConsumedDelta == 200 && change.NewFinal == 1);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && state.Reset && state.ResetReason == 7);
	CHECK(state.ReadOffset == 200 && state.HighestOffset == 200 && state.BufferedBytes == 0);
	CHECK(xrtRudpReceivePeek(r, 200, output, sizeof(output), &count) == XRUDP_CANCELLED && count == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_CANCELLED && count == 0);
	CHECK(xrtRudpReceivePush(r, 0, source, 200, false, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpReceivePush(r, 200, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 200, 8, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 199, 7, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 200, 7, &change) == XRUDP_DUPLICATE && change.ConsumedDelta == 0);
	CHECK(xrtRudpReceiveExtend(r, 300) == XRUDP_CLOSED);
	xrtRudpReceiveDestroy(r);
	CHECK(xrtRudpReceiveCreate(256, 0, &r) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 0, NULL, 0, true, &change) == XRUDP_OK);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_DONE);
	xrtRudpReceiveDestroy(r);
	return 0;
}

static uint32_t seed = 0x4f8e9a31;
static uint32_t random_word(void)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}

static int batches(void)
{
	xrudpreceive *a = NULL, *b = NULL;
	xrudpreceiveframe frames[XRUDP_FRAMES_MAX] = {{0}};
	xrudpreceivebatchchange batch, saved;
	xrudpreceivechange single;
	xrudpreceivestate beforeA, beforeB, state;
	unsigned char output[256], bad[10];
	size_t read;
	CHECK(xrtRudpReceiveCreate(256, 256, &a) == XRUDP_OK);
	CHECK(xrtRudpReceiveCreate(256, 256, &b) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(a, 10, source + 10, 10, false, &single) == XRUDP_OK);
	frames[0] = (xrudpreceiveframe){a, 0, source, 10, false};
	frames[1] = (xrudpreceiveframe){b, 0, source, 40, false};
	frames[2] = (xrudpreceiveframe){a, 15, source + 15, 10, false};
	frames[3] = (xrudpreceiveframe){a, 25, NULL, 0, true};
	frames[4] = (xrudpreceiveframe){b, 40, NULL, 0, true};
	CHECK(xrtRudpReceiveState(a, &beforeA) == XRUDP_OK);
	CHECK(xrtRudpReceiveState(b, &beforeB) == XRUDP_OK);
	memset(&batch, 0xa5, sizeof(batch)); saved = batch;
	CHECK(xrtRudpReceivePushBatch(frames, 5, 44, &batch) == XRUDP_MALFORMED);
	CHECK(memcmp(&batch, &saved, sizeof(batch)) == 0);
	CHECK(xrtRudpReceiveState(a, &state) == XRUDP_OK && memcmp(&state, &beforeA, sizeof(state)) == 0);
	CHECK(xrtRudpReceiveState(b, &state) == XRUDP_OK && memcmp(&state, &beforeB, sizeof(state)) == 0);
	frames[4].Offset = 39; /* Late final-size error cannot commit earlier streams. */
	CHECK(xrtRudpReceivePushBatch(frames, 5, 1000, &batch) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveRead(b, output, sizeof(output), &read) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpReceiveState(a, &state) == XRUDP_OK && memcmp(&state, &beforeA, sizeof(state)) == 0);
	frames[4].Offset = 40;
	memcpy(bad, source + 15, 10); bad[2] ^= 1; frames[2].Data = bad;
	CHECK(xrtRudpReceivePushBatch(frames, 5, 1000, &batch) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveState(a, &state) == XRUDP_OK && memcmp(&state, &beforeA, sizeof(state)) == 0);
	frames[2].Data = source + 15;
	CHECK(xrtRudpReceiveCheckBatch(frames, 5, 45, &batch) == XRUDP_OK);
	CHECK(batch.HighestDelta == 45 && batch.NewBytes == 55 && batch.NewFinals == 2 && batch.Streams == 2);
	CHECK(xrtRudpReceiveState(b, &state) == XRUDP_OK && memcmp(&state, &beforeB, sizeof(state)) == 0);
	CHECK(xrtRudpReceivePushBatch(frames, 5, 45, &batch) == XRUDP_OK);
	CHECK(batch.HighestDelta == 45 && batch.NewBytes == 55 && batch.NewFinals == 2);
	CHECK(xrtRudpReceiveRead(a, output, 256, &read) == XRUDP_OK && read == 25 && memcmp(output, source, read) == 0);
	CHECK(xrtRudpReceiveRead(b, output, 256, &read) == XRUDP_OK && read == 40 && memcmp(output, source, read) == 0);
	CHECK(xrtRudpReceiveRead(a, output, 256, &read) == XRUDP_DONE);
	CHECK(xrtRudpReceiveRead(b, output, 256, &read) == XRUDP_DONE);
	CHECK(xrtRudpReceivePushBatch(frames, 5, 0, &batch) == XRUDP_DUPLICATE && batch.HighestDelta == 0 && batch.NewBytes == 0);
	xrtRudpReceiveDestroy(a); xrtRudpReceiveDestroy(b);
	CHECK(xrtRudpReceiveCreate(256, 256, &a) == XRUDP_OK);
	/* New-vs-new overlap conflicts, absent from the original receive buffer. */
	frames[0] = (xrudpreceiveframe){a, 0, source, 20, false};
	frames[1] = (xrudpreceiveframe){a, 15, bad, 10, false};
	CHECK(xrtRudpReceivePushBatch(frames, 2, 1000, &batch) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveRead(a, output, 256, &read) == XRUDP_BACKPRESSURE);
	frames[0] = (xrudpreceiveframe){a, 200, NULL, 0, true};
	frames[1] = (xrudpreceiveframe){a, 200, source, 1, false};
	CHECK(xrtRudpReceivePushBatch(frames, 2, 1000, &batch) == XRUDP_MALFORMED);
	frames[1] = (xrudpreceiveframe){a, 150, source, 10, false};
	CHECK(xrtRudpReceivePushBatch(frames, 2, 200, &batch) == XRUDP_OK && batch.HighestDelta == 200 && batch.NewBytes == 10);
	xrtRudpReceiveDestroy(a);
	CHECK(xrtRudpReceiveCreate(256, 256, &a) == XRUDP_OK);
	for (size_t i = 0; i < XRUDP_FRAMES_MAX; ++i) frames[i] = (xrudpreceiveframe){a, i, NULL, 0, false};
	CHECK(xrtRudpReceivePushBatch(frames, XRUDP_FRAMES_MAX, 63, &batch) == XRUDP_OK);
	CHECK(batch.NewBytes == 0 && batch.HighestDelta == 63 && batch.Streams == 1);
	CHECK(xrtRudpReceiveCheckBatch(frames, 0, 1000, &batch) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceiveCheckBatch(frames, XRUDP_FRAMES_MAX + 1, 1000, &batch) == XRUDP_ARGUMENT);
	xrtRudpReceiveDestroy(a);
	CHECK(xrtRudpReceiveCreate(2048, 2048, &a) == XRUDP_OK);
	frames[0] = frames[1] = (xrudpreceiveframe){a, 0, source, XRUDP_STREAM_CHUNK_MAX, false};
	CHECK(xrtRudpReceivePushBatch(frames, 2, 2048, &batch) == XRUDP_LIMIT);
	CHECK(xrtRudpReceiveState(a, &state) == XRUDP_OK && state.BufferedBytes == 0 && state.HighestOffset == 0);
	xrtRudpReceiveDestroy(a);
	return 0;
}

static int model(void)
{
	/* Full-history reference bitmap uses no modulo/ring indexing. */
	for (size_t round = 0; round < 25; ++round) {
		xrudpreceive* r = NULL;
		xrudpreceivestate state;
		xrudpreceivechange change;
		unsigned char seen[8192] = {0}, output[256];
		size_t consumed = 0, highest = 0, buffered = 0, limit = 256, count;
		bool final = false;
		CHECK(xrtRudpReceiveCreate(256, 256, &r) == XRUDP_OK);
		for (size_t step = 0; step < 4000; ++step) {
			uint32_t word = random_word();
			if (word % 3 != 0) {
				size_t offset = word % (limit + 1), size = random_word() % 81;
				size_t newBytes = 0, end;
				if (size > limit - offset) size = limit - offset;
				end = offset + size;
				bool fin = end == sizeof(source);
				for (size_t i = offset; i < end; ++i) if (!seen[i]) ++newBytes;
				xrudpresult expected = newBytes || end > highest || (fin && !final) ? XRUDP_OK : XRUDP_DUPLICATE;
				CHECK(xrtRudpReceiveCheck(r, offset, source + offset, size, fin, &change) == expected);
				CHECK(change.HighestDelta == (end > highest ? end - highest : 0) && change.NewBytes == newBytes);
				CHECK(xrtRudpReceivePush(r, offset, source + offset, size, fin, &change) == expected);
				for (size_t i = offset; i < end; ++i) seen[i] = 1;
				if (end > highest) highest = end;
				buffered += newBytes; final = final || fin;
			} else {
				size_t capacity = 1 + random_word() % 256, wanted = 0;
				while (wanted < capacity && consumed + wanted < sizeof(source) && seen[consumed + wanted]) ++wanted;
				xrudpresult expected = wanted ? XRUDP_OK : (final && consumed == sizeof(source) ? XRUDP_DONE : XRUDP_BACKPRESSURE);
				CHECK(xrtRudpReceiveRead(r, output, capacity, &count) == expected && count == wanted);
				CHECK(memcmp(output, source + consumed, count) == 0);
				consumed += count; buffered -= count;
				limit = consumed + 256 < sizeof(source) ? consumed + 256 : sizeof(source);
				xrudpresult extended = xrtRudpReceiveExtend(r, limit);
				CHECK(extended == XRUDP_OK || extended == XRUDP_DUPLICATE);
			}
			CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK);
			CHECK(state.ReadOffset == consumed && state.HighestOffset == highest && state.Limit == limit);
			CHECK(state.BufferedBytes == buffered && state.HasFinal == (final ? 1u : 0u));
		}
		/* Force remaining holes to arrive: exact delivery and ring reuse across 8KiB. */
		while (consumed < sizeof(source)) {
			size_t amount = limit - consumed;
			CHECK(amount != 0 && amount <= 256);
			xrudpresult result = xrtRudpReceivePush(r, consumed, source + consumed, amount,
				consumed + amount == sizeof(source), &change);
			CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE);
			CHECK(xrtRudpReceiveRead(r, output, 256, &count) == XRUDP_OK && count == amount);
			CHECK(memcmp(output, source + consumed, count) == 0); consumed += count;
			limit = consumed + 256 < sizeof(source) ? consumed + 256 : sizeof(source);
			result = xrtRudpReceiveExtend(r, limit);
			CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE);
		}
		CHECK(xrtRudpReceiveRead(r, output, 256, &count) == XRUDP_DONE);
		xrtRudpReceiveDestroy(r);
	}
	return 0;
}

static int readiness(void)
{
	xrudpreceive* r; xrudpreceivechange change; unsigned char byte=7,out; size_t count;
	CHECK(!xrtRudpReceiveReady(NULL));
	CHECK(xrtRudpReceiveCreate(256,256,&r)==XRUDP_OK && !xrtRudpReceiveReady(r));
	CHECK(xrtRudpReceivePush(r,100,&byte,1,false,&change)==XRUDP_OK && !xrtRudpReceiveReady(r));
	CHECK(xrtRudpReceivePush(r,0,&byte,1,false,&change)==XRUDP_OK && xrtRudpReceiveReady(r));
	CHECK(xrtRudpReceiveRead(r,&out,1,&count)==XRUDP_OK && count==1 && !xrtRudpReceiveReady(r));
	CHECK(xrtRudpReceiveReset(r,101,42,&change)==XRUDP_OK && xrtRudpReceiveReady(r));
	xrtRudpReceiveDestroy(r);
	CHECK(xrtRudpReceiveCreate(256,0,&r)==XRUDP_OK);
	CHECK(xrtRudpReceivePush(r,0,NULL,0,true,&change)==XRUDP_OK && xrtRudpReceiveReady(r));
	CHECK(xrtRudpReceiveRead(r,&out,1,&count)==XRUDP_DONE && xrtRudpReceiveReady(r));
	xrtRudpReceiveDestroy(r); return 0;
}
int main(void)
{
	for (size_t i = 0; i < sizeof(source); ++i) source[i] = (unsigned char)((i * 37) ^ (i >> 5));
	CHECK(boundaries() == 0);
	CHECK(batches() == 0);
	CHECK(readiness() == 0);
	CHECK(model() == 0);
	puts("xrudp receive: bounded ring/bitmap, flow/final/overlap errors and 100,000-step reassembly model passed");
	return 0;
}
