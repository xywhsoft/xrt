#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

int main(void)
{
	xrudpreplay window, saved;
	unsigned char seen[20000] = {0};
	uint64_t largest = 0;
	uint32_t random = 0x12345678;
	xrtRudpReplayInit(&window);
	CHECK(xrtRudpReplayCheck(&window, 0) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReplayAccept(NULL, 1) == XRUDP_ARGUMENT);
	for (size_t i = 0; i < 100000; ++i) {
		uint64_t number;
		xrudpresult expected;
		random ^= random << 13; random ^= random >> 17; random ^= random << 5;
		number = 1 + random % 19999;
		expected = number <= largest && largest - number >= XRUDP_REPLAY_BITS ? XRUDP_TOO_OLD :
			(seen[number] ? XRUDP_DUPLICATE : XRUDP_OK);
		saved = window;
		CHECK(xrtRudpReplayCheck(&window, number) == expected);
		CHECK(memcmp(&saved, &window, sizeof(window)) == 0);
		CHECK(xrtRudpReplayAccept(&window, number) == expected);
		if (expected == XRUDP_OK) {
			seen[number] = 1;
			if (number > largest) largest = number;
		} else CHECK(memcmp(&saved, &window, sizeof(window)) == 0);
	}
	/* Exercise shifts across all word/bit boundaries against a simple oracle. */
	for (uint64_t shift = 1; shift <= XRUDP_REPLAY_BITS + 1; ++shift) {
		xrtRudpReplayInit(&window);
		for (uint64_t n = 1; n <= XRUDP_REPLAY_BITS; n += 3)
			CHECK(xrtRudpReplayAccept(&window, n) == XRUDP_OK);
		largest = window.Largest;
		CHECK(xrtRudpReplayAccept(&window, largest + shift) == XRUDP_OK);
		for (uint64_t n = 1; n <= largest; ++n) {
			xrudpresult expected = largest + shift - n >= XRUDP_REPLAY_BITS ? XRUDP_TOO_OLD :
				(n % 3 == 1 ? XRUDP_DUPLICATE : XRUDP_OK);
			CHECK(xrtRudpReplayCheck(&window, n) == expected);
		}
	}
	xrtRudpReplayInit(&window);
	CHECK(xrtRudpReplayAccept(&window, UINT64_MAX) == XRUDP_OK);
	CHECK(xrtRudpReplayAccept(&window, UINT64_MAX - 1023) == XRUDP_OK);
	CHECK(xrtRudpReplayAccept(&window, UINT64_MAX - 1024) == XRUDP_TOO_OLD);
	CHECK(xrtRudpReplayAccept(&window, UINT64_MAX) == XRUDP_DUPLICATE);
	puts("xrudp replay: oracle, word boundaries, maximum sequence passed");
	return 0;
}
