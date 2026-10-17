#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static int oracle_test(void)
{
	bool seen[65] = {false};
	xrudpackset set, saved;
	xrudpack ack;
	unsigned char frame[600];
	uint32_t random = 0x13579bdf;
	uint64_t largest = 0, largestAt = 0;
	size_t size;
	xrtRudpAckSetInit(&set);
	for (uint64_t i = 1; i <= 20000; ++i) {
		uint64_t number;
		size_t count = 0;
		random ^= random << 13; random ^= random >> 17; random ^= random << 5;
		number = 1 + random % 64;
		saved = set;
		CHECK(xrtRudpAckSetRecord(&set, number, i) == (seen[number] ? XRUDP_DUPLICATE : XRUDP_OK));
		if (seen[number]) CHECK(memcmp(&set, &saved, sizeof(set)) == 0);
		seen[number] = true;
		if (number > largest) { largest = number; largestAt = i; }
		CHECK(xrtRudpAckSetEncode(&set, i + 10, frame, sizeof(frame), &size) == XRUDP_OK);
		CHECK(xrtRudpAckDecode(frame, size, &ack) == XRUDP_OK);
		CHECK(ack.DelayUs == i + 10 - largestAt);
		/* Independent bitmap oracle, including holes, descending range order
		 * and coalescing. At most 32 isolated points fit this 64-packet domain. */
		for (uint64_t n = 64; n != 0;) {
			uint64_t last, first;
			if (!seen[n]) { --n; continue; }
			last = n;
			while (n > 1 && seen[n - 1]) --n;
			first = n--;
			CHECK(count < ack.Count);
			CHECK(ack.Ranges[count].First == first && ack.Ranges[count].Last == last);
			++count;
		}
		CHECK(count == ack.Count);
		for (uint64_t n = 1; n <= 64; ++n)
			CHECK(xrtRudpAckSetContains(&set, n) == seen[n]);
	}
	return 0;
}

static int bounded_history(void)
{
	xrudpackset set, saved;
	xrudpack ack;
	unsigned char frame[600], before[600];
	size_t size;
	xrtRudpAckSetInit(&set);
	CHECK(xrtRudpAckSetEncode(&set, 0, frame, sizeof(frame), &size) == XRUDP_DONE && size == 0);
	CHECK(xrtRudpAckSetRecord(&set, 0, 0) == XRUDP_ARGUMENT);
	for (uint64_t i = 0; i < 100; ++i) CHECK(xrtRudpAckSetRecord(&set, i * 2 + 1, i + 1) == XRUDP_OK);
	CHECK(set.Count == XRUDP_ACK_RANGES_MAX);
	CHECK(!xrtRudpAckSetContains(&set, 1) && xrtRudpAckSetContains(&set, 137));
	for (size_t i = 0; i < set.Count; ++i)
		CHECK(set.Ranges[i].First == 199u - i * 2u && set.Ranges[i].Last == 199u - i * 2u);
	saved = set;
	CHECK(xrtRudpAckSetRecord(&set, 2, 101) == XRUDP_TOO_OLD);
	CHECK(memcmp(&set, &saved, sizeof(set)) == 0);
	CHECK(xrtRudpAckSetRecord(&set, 198, 102) == XRUDP_OK && set.Count == XRUDP_ACK_RANGES_MAX - 1);
	CHECK(xrtRudpAckSetRecord(&set, 1, 103) == XRUDP_OK && set.Count == XRUDP_ACK_RANGES_MAX);
	memset(frame, 0x5a, sizeof(frame)); memcpy(before, frame, sizeof(frame));
	CHECK(xrtRudpAckSetEncode(&set, 103, frame, 1, &size) == XRUDP_LIMIT && size == 0);
	CHECK(memcmp(frame, before, sizeof(frame)) == 0);
	CHECK(xrtRudpAckSetEncode(&set, 99, frame, sizeof(frame), &size) == XRUDP_ARGUMENT);
	CHECK(xrtRudpAckSetEncode(&set, UINT64_MAX, frame, sizeof(frame), &size) == XRUDP_OK);
	CHECK(xrtRudpAckDecode(frame, size, &ack) == XRUDP_OK && ack.DelayUs == UINT32_MAX);
	xrtRudpAckSetInit(&set);
	CHECK(xrtRudpAckSetRecord(&set, UINT64_MAX, UINT64_MAX) == XRUDP_OK);
	CHECK(xrtRudpAckSetRecord(&set, UINT64_MAX - 1, UINT64_MAX) == XRUDP_OK);
	CHECK(set.Count == 1 && set.Ranges[0].First == UINT64_MAX - 1 && set.Ranges[0].Last == UINT64_MAX);
	CHECK(xrtRudpAckSetEncode(&set, UINT64_MAX, frame, sizeof(frame), &size) == XRUDP_OK);
	CHECK(xrtRudpAckDecode(frame, size, &ack) == XRUDP_OK && ack.DelayUs == 0);
	return 0;
}

static int golden_and_failures(void)
{
	static const unsigned char golden[] = {
		2,0,0,40, 0,0,0,10, 0,2,0,0,
		0,0,0,0,0,0,0,5, 0,0,0,0,0,0,0,7,
		0,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,3
	};
	xrudpackset set;
	xrudpack ack, before;
	unsigned char output[600];
	size_t size;
	xrtRudpAckSetInit(&set);
	for (uint64_t n = 1; n <= 7; ++n) if (n != 4)
		CHECK(xrtRudpAckSetRecord(&set, n, 105) == XRUDP_OK);
	CHECK(xrtRudpAckSetEncode(&set, 115, output, sizeof(output), &size) == XRUDP_OK);
	CHECK(size == sizeof(golden) && memcmp(output, golden, sizeof(golden)) == 0);
	memset(&ack, 0xa5, sizeof(ack)); before = ack;
	for (size_t i = 0; i < sizeof(golden); ++i) {
		CHECK(xrtRudpAckDecode(golden, i, &ack) != XRUDP_OK);
		CHECK(memcmp(&ack, &before, sizeof(ack)) == 0);
	}
	output[size] = 0;
	CHECK(xrtRudpAckDecode(output, size + 1, &ack) == XRUDP_MALFORMED);
	CHECK(memcmp(&ack, &before, sizeof(ack)) == 0);
	CHECK(xrtRudpAckDecode(golden, sizeof(golden), &ack) == XRUDP_OK && ack.Count == 2 && ack.DelayUs == 10);
	return 0;
}

int main(void)
{
	if (oracle_test() || bounded_history() || golden_and_failures()) return 1;
	puts("xrudp ACK: independent bitmap oracle, bounded history, canonical vectors and time bounds passed");
	return 0;
}
