#include <xrudp.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/congestion_vectors.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static int send_packet(xrudpcongestion* s, uint64_t* now, uint32_t bytes,
	uint32_t flags, xrudpsentevent* event)
{
	uint64_t due;
	xrudpresult result = xrtRudpCongestionCanSend(s, *now, 100000, bytes, flags, &due);
	CHECK(result == XRUDP_OK || result == XRUDP_TIMEOUT);
	if (result == XRUDP_TIMEOUT) *now = due;
	memset(event, 0, sizeof(*event));
	event->PacketNumber = s->LargestSent + 1; event->SentAtUs = *now;
	event->Bytes = bytes; event->Flags = flags;
	CHECK(xrtRudpCongestionOnSent(s, event->PacketNumber, *now, 100000, bytes, flags) == XRUDP_OK);
	return 0;
}

static int growth_and_recovery(void)
{
	xrudpcongestion s, saved;
	xrudpsentevent e[8], next;
	uint64_t now = 0;
	CHECK(xrtRudpCongestionInit(&s, 0) == XRUDP_OK);
	CHECK(send_packet(&s, &now, 1200, 1, &e[0]) == 0);
	e[0].Kind = XRUDP_SENT_ACKED;
	CHECK(xrtRudpCongestionApply(&s, e, 1, now, true) == XRUDP_OK && s.Window == 12000);
	CHECK(send_packet(&s, &now, 1200, 1, &e[0]) == 0);
	e[0].Kind = XRUDP_SENT_ACKED;
	CHECK(xrtRudpCongestionApply(&s, e, 1, now, false) == XRUDP_OK && s.Window == 13200);
	saved = s;
	CHECK(xrtRudpCongestionApply(&s, e, 1, now, false) == XRUDP_ARGUMENT);
	CHECK(memcmp(&s, &saved, sizeof(s)) == 0); /* Cannot remove bytes twice. */
	CHECK(xrtRudpCongestionInit(&s, 0) == XRUDP_OK); now = 0;
	for (size_t i = 0; i < 5; ++i) CHECK(send_packet(&s, &now, 1200, 1, &e[i]) == 0);
	e[0].Kind = e[1].Kind = XRUDP_SENT_LOST;
	CHECK(xrtRudpCongestionApply(&s, e, 2, now, false) == XRUDP_OK);
	CHECK(s.Window == 6000 && s.SlowStartThreshold == 6000 && s.RecoveryThrough == 5);
	e[2].Kind = XRUDP_SENT_LOST;
	CHECK(xrtRudpCongestionApply(&s, &e[2], 1, now, false) == XRUDP_OK && s.Window == 6000);
	e[3].Kind = XRUDP_SENT_ACKED;
	CHECK(xrtRudpCongestionApply(&s, &e[3], 1, now, false) == XRUDP_OK && s.Window == 6000);
	for (size_t i = 0; i < 5; ++i) {
		CHECK(send_packet(&s, &now, 1200, 1, &next) == 0);
		next.Kind = XRUDP_SENT_ACKED;
		CHECK(xrtRudpCongestionApply(&s, &next, 1, now, false) == XRUDP_OK);
		CHECK(s.Window == (i < 4 ? 6000u : 7200u));
	}
	CHECK(s.AvoidanceCredit == 0 && s.BytesInFlight == 1200);
	CHECK(xrtRudpCongestionCollapse(&s, now) == XRUDP_OK);
	CHECK(s.Window == 2400 && s.SlowStartThreshold == 3600 && s.RecoveryThrough == 10);
	e[4].Kind = XRUDP_SENT_CANCELLED;
	CHECK(xrtRudpCongestionApply(&s, &e[4], 1, now, false) == XRUDP_OK && s.BytesInFlight == 0);
	CHECK(send_packet(&s, &now, 1200, XRUDP_SENT_PADDING, &next) == 0);
	next.Kind = XRUDP_SENT_LOST;
	CHECK(xrtRudpCongestionApply(&s, &next, 1, now, false) == XRUDP_OK && s.Window == 2400);
	CHECK(send_packet(&s, &now, 1200, 1, &e[0]) == 0);
	CHECK(send_packet(&s, &now, 1200, 1, &e[1]) == 0);
	/* A new loss and ACK in the same batch must reduce first, independent of order. */
	e[0].Kind = XRUDP_SENT_ACKED; e[1].Kind = XRUDP_SENT_LOST;
	CHECK(xrtRudpCongestionApply(&s, e, 2, now, false) == XRUDP_OK);
	CHECK(s.Window == 2400 && s.AvoidanceCredit == 0 && s.BytesInFlight == 0);
	CHECK(xrtRudpCongestionInit(&s, 12000) == XRUDP_OK); now = 0;
	for (size_t i = 0; i < 100; ++i) {
		CHECK(send_packet(&s, &now, 1200, 1, &next) == 0);
		next.Kind = XRUDP_SENT_ACKED;
		CHECK(xrtRudpCongestionApply(&s, &next, 1, now, false) == XRUDP_OK && s.Window == 12000);
	}
	return 0;
}

static int pacing_and_bounds(void)
{
	xrudpcongestion s, saved, invalid = {0};
	xrudpsentevent e[48];
	uint64_t due = 987, now = 0;
	CHECK(xrtRudpCongestionInit(&s, 12000) == XRUDP_OK);
	CHECK(xrtRudpCongestionOnSent(&s, 1, 0, 100000, 1200, 1) == XRUDP_OK);
	CHECK(xrtRudpCongestionOnSent(&s, 2, 0, 100000, 1200, 1) == XRUDP_OK);
	saved = s;
	CHECK(xrtRudpCongestionCanSend(&s, 0, 100000, 1200, 1, &due) == XRUDP_TIMEOUT && due == 10000);
	CHECK(xrtRudpCongestionOnSent(&s, 3, 0, 100000, 1200, 1) == XRUDP_TIMEOUT);
	CHECK(memcmp(&s, &saved, sizeof(s)) == 0);
	CHECK(xrtRudpCongestionCanSend(&s, 0, 100000, 100, 0, &due) == XRUDP_OK && due == 0);
	CHECK(xrtRudpCongestionOnSent(&s, 3, 0, 100000, 100, 0) == XRUDP_OK);
	CHECK(s.BytesInFlight == 2400 && s.PacerNextUs == saved.PacerNextUs);
	/* Time spent idle cannot buy an unbounded burst. */
	now = 1000000;
	CHECK(xrtRudpCongestionOnSent(&s, 4, now, 100000, 1200, 1) == XRUDP_OK);
	CHECK(xrtRudpCongestionOnSent(&s, 5, now, 100000, 1200, 1) == XRUDP_OK);
	CHECK(xrtRudpCongestionOnSent(&s, 6, now, 100000, 1200, 1) == XRUDP_TIMEOUT);
	CHECK(xrtRudpCongestionInit(&s, 12000) == XRUDP_OK); now = 0;
	for (size_t i = 0; i < 10; ++i) CHECK(send_packet(&s, &now, 1200, 1, &e[i]) == 0);
	saved = s; due = 987;
	CHECK(xrtRudpCongestionCanSend(&s, now, 100000, 1200, 1, &due) == XRUDP_BACKPRESSURE && due == 987);
	CHECK(xrtRudpCongestionOnSent(&s, 11, now, 100000, 1200, 1) == XRUDP_BACKPRESSURE);
	CHECK(memcmp(&s, &saved, sizeof(s)) == 0);
	for (size_t i = 10; i < 42; ++i) CHECK(send_packet(&s, &now, 1200, 3, &e[i]) == 0);
	CHECK(s.BytesInFlight == 50400);
	CHECK(xrtRudpCongestionCanSend(&s, now, 100000, 1200, 3, &due) == XRUDP_BACKPRESSURE);
	for (size_t i = 0; i < 42; ++i) e[i].Kind = XRUDP_SENT_CANCELLED;
	CHECK(xrtRudpCongestionApply(&s, e, 42, now, false) == XRUDP_OK && s.BytesInFlight == 0 && s.Window == 12000);
	saved = s;
	CHECK(xrtRudpCongestionInit(&s, 11999) == XRUDP_ARGUMENT && memcmp(&s, &saved, sizeof(s)) == 0);
	CHECK(xrtRudpCongestionInit(&s, XRUDP_CC_MAXIMUM_LIMIT + 1) == XRUDP_ARGUMENT);
	due = 987;
	CHECK(xrtRudpCongestionCanSend(&invalid, 0, 1, 100, 1, &due) == XRUDP_ARGUMENT && due == 987);
	CHECK(xrtRudpCongestionCanSend(&s, now, 0, 100, 1, &due) == XRUDP_ARGUMENT && due == 987);
	CHECK(xrtRudpCongestionCanSend(&s, now - 1, 100, 100, 1, &due) == XRUDP_ARGUMENT);
	CHECK(xrtRudpCongestionOnSent(&s, 43, now, 100, 100, 2) == XRUDP_ARGUMENT);
	CHECK(xrtRudpCongestionOnSent(&s, UINT64_MAX, now, 100, 100, 1) == XRUDP_ARGUMENT);
	e[0].Kind = XRUDP_SENT_EXPIRED;
	CHECK(xrtRudpCongestionApply(&s, e, 1, now, false) == XRUDP_ARGUMENT);
	CHECK(memcmp(&s, &saved, sizeof(s)) == 0);
	for (size_t i = 0; i < sizeof(congestion_vectors) / sizeof(congestion_vectors[0]); ++i) {
		const struct congestion_vector* v = &congestion_vectors[i];
		CHECK(xrtRudpCongestionInit(&s, XRUDP_CC_MAXIMUM_LIMIT) == XRUDP_OK);
		s.Window = v->Window;
		CHECK(xrtRudpCongestionOnSent(&s, 1, 1, v->Rtt, v->Bytes, 1) == XRUDP_OK);
		CHECK(s.PacerNextUs == v->FirstNext);
		CHECK(xrtRudpCongestionCanSend(&s, 1, v->Rtt, v->Bytes, 1, &due) == v->Result);
		CHECK(due == v->Due);
	}
	return 0;
}

static uint32_t seed = 0x1af89657;
static uint32_t random_word(void)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}

static int ledger_integration(void)
{
	xrudpcongestion cc;
	xrudpsentwindow* w = NULL;
	xrudpsentstate state;
	xrudpsentevent acked[64], final[128];
	xrudpackchange change;
	uint64_t now = 0, largest = 0, due;
	size_t ackCount, lossCount;
	CHECK(xrtRudpCongestionInit(&cc, 0) == XRUDP_OK);
	CHECK(xrtRudpSentCreate(32, &w) == XRUDP_OK);
	for (size_t i = 0; i < 20000; ++i) {
		uint32_t word = random_word();
		now += word % 1000;
		if (word % 3 == 0 && largest) {
			xrudpack ack = {0};
			ack.Count = 1; ack.Ranges[0].First = ack.Ranges[0].Last = 1 + random_word() % largest;
			CHECK(xrtRudpSentAcknowledge(w, &ack, now, acked, 64, &ackCount, &change) == XRUDP_OK);
			CHECK(xrtRudpSentDetectLoss(w, 20000, 30000, now, final, 128, &lossCount) <= XRUDP_DONE);
			memcpy(final + lossCount, acked, ackCount * sizeof(*acked));
			CHECK(xrtRudpCongestionApply(&cc, final, lossCount + ackCount, now, (word & 7) == 0) == XRUDP_OK);
		} else {
			uint32_t flags = (word & 15) == 0 ? 0 : ((word & 7) == 0 ? 4 : ((word & 3) == 0 ? 3 : 1));
			uint32_t bytes = 68 + random_word() % 1133;
			xrudpresult ready = xrtRudpCongestionCanSend(&cc, now, 10000, bytes, flags, &due);
			CHECK(ready == XRUDP_OK || ready == XRUDP_TIMEOUT || ready == XRUDP_BACKPRESSURE);
			if (ready == XRUDP_OK && xrtRudpSentCanRecord(w, (flags & 2) != 0)) {
				++largest;
				CHECK(xrtRudpSentRecord(w, largest, now, bytes, flags, largest) == XRUDP_OK);
				CHECK(xrtRudpCongestionOnSent(&cc, largest, now, 10000, bytes, flags) == XRUDP_OK);
			}
		}
		CHECK(xrtRudpSentState(w, &state) == XRUDP_OK);
		CHECK(state.BytesInFlight == cc.BytesInFlight && state.LargestSent == cc.LargestSent);
		CHECK(cc.Window >= XRUDP_CC_MINIMUM_WINDOW && cc.Window <= cc.MaximumWindow);
		CHECK(cc.AvoidanceCredit < cc.Window);
	}
	CHECK(xrtRudpSentAbort(w, final, 128, &lossCount) == XRUDP_OK);
	CHECK(xrtRudpCongestionApply(&cc, final, lossCount, now, true) == XRUDP_OK && cc.BytesInFlight == 0);
	xrtRudpSentDestroy(w);
	return 0;
}

int main(void)
{
	CHECK(growth_and_recovery() == 0);
	CHECK(pacing_and_bounds() == 0);
	CHECK(ledger_integration() == 0);
	puts("xrudp congestion: Reno epochs, bounded pacing/probes, exact integer vectors and 20,000-step ledger integration passed");
	return 0;
}
