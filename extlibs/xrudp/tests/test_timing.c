#include <xrudp.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/timing_vectors.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

int main(void)
{
	xrudprtt rtt, saved, invalid = {0};
	uint64_t interval, previous, loss;
	CHECK(xrtRudpRttInit(&rtt, 0, XRUDP_ACK_DELAY_MAX_US) == XRUDP_OK);
	CHECK(!rtt.HasSample && rtt.SmoothedUs == XRUDP_INITIAL_RTT_US);
	CHECK(xrtRudpRttPto(&rtt, 0, &interval) == XRUDP_OK && interval == 1024000);
	CHECK(xrtRudpRttLossDelay(&rtt, &loss) == XRUDP_OK && loss == 374625);
	for (size_t i = 0; i < sizeof(timing_vectors) / sizeof(timing_vectors[0]); ++i) {
		const struct timing_vector* v = &timing_vectors[i];
		CHECK(xrtRudpRttSample(&rtt, v->Sample, v->PeerDelay) == XRUDP_OK);
		CHECK(rtt.HasSample && rtt.LatestUs == (v->Sample == 0 ? 1 : v->Sample));
		CHECK(rtt.MinimumUs == v->Minimum && rtt.SmoothedUs == v->Smoothed);
		CHECK(rtt.VariationUs == v->Variation);
		CHECK(xrtRudpRttPto(&rtt, 0, &interval) == XRUDP_OK && interval == v->Pto);
		CHECK(xrtRudpRttLossDelay(&rtt, &loss) == XRUDP_OK && loss == v->Loss);
	}
	CHECK(xrtRudpRttInit(&rtt, 1, 0) == XRUDP_OK);
	CHECK(xrtRudpRttSample(&rtt, 0, UINT32_MAX) == XRUDP_OK);
	CHECK(rtt.SmoothedUs == 1 && rtt.MinimumUs == 1 && rtt.VariationUs == 0);
	CHECK(xrtRudpRttPto(&rtt, 0, &interval) == XRUDP_OK && interval == 1001);
	CHECK(xrtRudpRttLossDelay(&rtt, &loss) == XRUDP_OK && loss == 1000);
	previous = interval;
	for (unsigned int i = 1; i < 70; ++i) {
		CHECK(xrtRudpRttPto(&rtt, i, &interval) == XRUDP_OK && interval >= previous);
		CHECK(interval == (previous > UINT64_MAX / 2 ? UINT64_MAX : previous * 2));
		previous = interval;
	}
	CHECK(xrtRudpRttPto(&rtt, UINT32_MAX, &interval) == XRUDP_OK && interval == UINT64_MAX);
	CHECK(xrtRudpRttInit(&rtt, UINT64_MAX, 0) == XRUDP_OK);
	CHECK(xrtRudpRttPto(&rtt, 0, &interval) == XRUDP_OK && interval == UINT64_MAX);
	CHECK(xrtRudpRttLossDelay(&rtt, &loss) == XRUDP_OK && loss == UINT64_MAX);
	saved = rtt;
	CHECK(xrtRudpRttInit(&rtt, 1000, XRUDP_ACK_DELAY_MAX_US + 1) == XRUDP_ARGUMENT);
	CHECK(memcmp(&rtt, &saved, sizeof(rtt)) == 0);
	interval = 321;
	CHECK(xrtRudpRttPto(&invalid, 0, &interval) == XRUDP_ARGUMENT && interval == 321);
	CHECK(xrtRudpRttLossDelay(NULL, &interval) == XRUDP_ARGUMENT && interval == 321);
	CHECK(xrtRudpRttSample(&invalid, 1, 0) == XRUDP_ARGUMENT);
	puts("xrudp timing: big-integer vectors, bounded ACK delay, zero RTT and saturating deadlines passed");
	return 0;
}
