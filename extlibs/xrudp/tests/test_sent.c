#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static xrudpack one_ack(uint64_t first, uint64_t last)
{
	xrudpack ack;
	memset(&ack, 0, sizeof(ack));
	ack.Count = 1; ack.DelayUs = 25;
	ack.Ranges[0].First = first; ack.Ranges[0].Last = last;
	return ack;
}

static int boundaries(void)
{
	xrudpsentwindow* w = NULL;
	xrudpsentstate s, saved;
	xrudpsentevent events[40], untouched[40];
	xrudpackchange change, before;
	xrudpack ack;
	size_t count = 9;
	memset(events, 0xa5, sizeof(events)); memcpy(untouched, events, sizeof(events));
	memset(&change, 0x5a, sizeof(change)); before = change;
	CHECK(xrtRudpSentCreate(XRUDP_SENT_CAPACITY_MAX + 1, &w) == XRUDP_ARGUMENT && w == NULL);
	CHECK(xrtRudpSentCreate(2, &w) == XRUDP_OK);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && s.TotalCapacity == 34);
	CHECK(xrtRudpSentRecord(w, 1, 100, 100, XRUDP_SENT_ACK_ELICITING, 901) == XRUDP_OK);
	CHECK(xrtRudpSentRecord(w, 3, 101, 90, 0, 903) == XRUDP_OK);
	CHECK(!xrtRudpSentCanRecord(w, false) && xrtRudpSentCanRecord(w, true));
	CHECK(xrtRudpSentRecord(w, 4, 102, 100, XRUDP_SENT_ACK_ELICITING, 904) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSentState(w, &saved) == XRUDP_OK);
	ack = one_ack(1, 3); /* Includes unsent #2: no part may complete. */
	CHECK(xrtRudpSentAcknowledge(w, &ack, 200, events, 40, &count, &change) == XRUDP_MALFORMED);
	CHECK(count == 0 && memcmp(events, untouched, sizeof(events)) == 0);
	CHECK(memcmp(&before, &change, sizeof(change)) == 0);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && memcmp(&s, &saved, sizeof(s)) == 0);
	ack = one_ack(4, 4);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 200, events, 40, &count, &change) == XRUDP_MALFORMED);
	ack = one_ack(3, 3); ack.Count = 2; ack.Ranges[1].First = 1; ack.Ranges[1].Last = 1;
	CHECK(xrtRudpSentCheckAcknowledge(w, &ack, 200, &count, &change) == XRUDP_OK && count == 2);
	CHECK(change.NewlyAckedPackets == 2 && change.NewlyAckedBytes == 100 && change.RttSampleUs == 99);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && memcmp(&s, &saved, sizeof(s)) == 0);
	change = before;
	CHECK(xrtRudpSentAcknowledge(w, &ack, 200, events, 1, &count, &change) == XRUDP_BACKPRESSURE);
	CHECK(count == 0 && memcmp(events, untouched, sizeof(events)) == 0);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && memcmp(&s, &saved, sizeof(s)) == 0);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 200, events, 40, &count, &change) == XRUDP_OK);
	CHECK(count == 2 && events[0].PacketNumber == 3 && events[1].PacketNumber == 1);
	CHECK(events[0].UserTag == 903 && events[1].UserTag == 901 && events[0].Kind == XRUDP_SENT_ACKED);
	CHECK(change.NewlyAckedBytes == 100 && change.NewlyAckedPackets == 2 && change.NewlyAckedEliciting == 1);
	CHECK(change.RttSampleValid == 1 && change.RttSampleUs == 99 && change.PeerAckDelayUs == 25);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && s.StoredPackets == 0 && s.BytesInFlight == 0);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 201, NULL, 0, &count, &change) == XRUDP_OK);
	CHECK(count == 0 && change.NewlyAckedPackets == 0 && !change.RttSampleValid);
	/* Entirely retired ranges, including holes, cannot grant bytes or progress. */
	ack = one_ack(1, 3);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 202, NULL, 0, &count, &change) == XRUDP_OK);
	CHECK(change.NewlyAckedBytes == 0 && change.NewlyAckedEliciting == 0);
	CHECK(xrtRudpSentRecord(w, UINT64_MAX - 1, 203, 68, XRUDP_SENT_ACK_ELICITING, 7) == XRUDP_OK);
	CHECK(xrtRudpSentRecord(w, UINT64_MAX, 204, 68, XRUDP_SENT_ACK_ELICITING, 8) == XRUDP_ARGUMENT);
	ack = one_ack(UINT64_MAX - 1, UINT64_MAX - 1);
	CHECK(xrtRudpSentAcknowledge(w, &ack, UINT64_MAX, events, 40, &count, &change) == XRUDP_OK);
	CHECK(count == 1 && change.RttSampleUs == UINT64_MAX - 203);
	CHECK(xrtRudpSentAbort(w, NULL, 0, &count) == XRUDP_OK && count == 0);
	CHECK(xrtRudpSentAbort(w, NULL, 0, &count) == XRUDP_DONE);
	CHECK(!xrtRudpSentCanRecord(w, true));
	xrtRudpSentDestroy(w);
	CHECK(xrtRudpSentCreate(1, &w) == XRUDP_OK);
	CHECK(xrtRudpSentRecord(w, 1, 0, 1200, XRUDP_SENT_PADDING, 1) == XRUDP_OK);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && s.BytesInFlight == 1200);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1000, 1000, events, 40, &count) == XRUDP_DONE);
	ack = one_ack(1, 1);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 1000, events, 40, &count, &change) == XRUDP_OK);
	CHECK(change.NewlyAckedBytes == 1200 && change.NewlyAckedEliciting == 0 && !change.RttSampleValid);
	xrtRudpSentDestroy(w);
	w = NULL;
	CHECK(xrtRudpSentCreateAfter(2, UINT64_MAX, &w) == XRUDP_ARGUMENT && w == NULL);
	CHECK(xrtRudpSentCreateAfter(2, 1, &w) == XRUDP_OK);
	ack = one_ack(1, 1);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 0, NULL, 0, &count, &change) == XRUDP_OK);
	CHECK(count == 0 && change.NewlyAckedBytes == 0 && !change.RttSampleValid && change.NewlyAckedEliciting == 0);
	CHECK(xrtRudpSentRecord(w, 1, 0, 100, 1, 1) == XRUDP_ARGUMENT);
	CHECK(xrtRudpSentRecord(w, 2, 0, 100, 1, 2) == XRUDP_OK);
	ack = one_ack(1, 2);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 1000, events, 40, &count, &change) == XRUDP_OK);
	CHECK(count == 1 && events[0].PacketNumber == 2 && change.NewlyAckedBytes == 100 && change.RttSampleValid);
	xrtRudpSentDestroy(w);
	return 0;
}

static int loss_and_reserve(void)
{
	xrudpsentwindow* w = NULL;
	xrudpsentevent e[40];
	xrudpsentstate s, saved;
	xrudpackchange change;
	xrudpack ack;
	size_t n;
	CHECK(xrtRudpSentCreate(2, &w) == XRUDP_OK);
	for (uint64_t i = 1; i <= 34; ++i) {
		uint32_t flags = XRUDP_SENT_ACK_ELICITING | (i > 2 ? XRUDP_SENT_PROBE : 0);
		CHECK(xrtRudpSentRecord(w, i, 100 + i, 100, flags, i + 900) == XRUDP_OK);
	}
	CHECK(!xrtRudpSentCanRecord(w, true));
	CHECK(xrtRudpSentRecord(w, 35, 135, 100, 3, 935) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1500, 10000, e, 40, &n) == XRUDP_DONE && n == 0);
	/* PTO alone cannot discard data with no ACK evidence. */
	ack = one_ack(34, 34);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 10001, e, 40, &n, &change) == XRUDP_OK && n == 1);
	CHECK(xrtRudpSentState(w, &saved) == XRUDP_OK);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1500, 10002, e, 32, &n) == XRUDP_BACKPRESSURE && n == 0);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && memcmp(&s, &saved, sizeof(s)) == 0);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1500, 10002, e, 40, &n) == XRUDP_OK && n == 33);
	for (size_t i = 0; i < n; ++i) CHECK(e[i].Kind == XRUDP_SENT_LOST && e[i].PacketNumber == i + 1);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && s.BytesInFlight == 0 && s.StoredPackets == 0);
	ack = one_ack(1, 34);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 10003, NULL, 0, &n, &change) == XRUDP_OK && n == 0);
	CHECK(change.NewlyAckedBytes == 0); /* No second final event for late ACK. */
	xrtRudpSentDestroy(w);
	CHECK(xrtRudpSentCreate(8, &w) == XRUDP_OK);
	for (uint64_t i = 1; i <= 5; ++i)
		CHECK(xrtRudpSentRecord(w, i, 100, 100, i == 2 ? 0 : 1, i) == XRUDP_OK);
	ack = one_ack(4, 4);
	CHECK(xrtRudpSentAcknowledge(w, &ack, 101, e, 40, &n, &change) == XRUDP_OK);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1500, 102, e, 40, &n) == XRUDP_OK && n == 1);
	CHECK(e[0].PacketNumber == 1 && e[0].Kind == XRUDP_SENT_LOST);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1500, 1100, e, 40, &n) == XRUDP_OK && n == 1);
	CHECK(e[0].PacketNumber == 3 && e[0].Kind == XRUDP_SENT_LOST);
	CHECK(xrtRudpSentDetectLoss(w, 1000, 1500, 1600, e, 40, &n) == XRUDP_OK && n == 1);
	CHECK(e[0].PacketNumber == 2 && e[0].Kind == XRUDP_SENT_EXPIRED);
	CHECK(xrtRudpSentState(w, &saved) == XRUDP_OK && saved.PendingPackets == 1 && saved.BytesInFlight == 100);
	CHECK(xrtRudpSentAbort(w, NULL, 0, &n) == XRUDP_BACKPRESSURE && n == 0);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && memcmp(&s, &saved, sizeof(s)) == 0);
	CHECK(xrtRudpSentAbort(w, e, 40, &n) == XRUDP_OK && n == 1 && e[0].Kind == XRUDP_SENT_CANCELLED);
	CHECK(e[0].PacketNumber == 5 && e[0].UserTag == 5);
	CHECK(xrtRudpSentRecord(w, 6, 1601, 100, 1, 6) == XRUDP_CLOSED);
	CHECK(xrtRudpSentState(w, &s) == XRUDP_OK && s.BytesInFlight == 0 && s.StoredPackets == 0 && s.Stopped);
	xrtRudpSentDestroy(w);
	return 0;
}

static uint32_t seed = 0x7f34c6a1;
static uint32_t random_word(void)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
	return seed;
}

static int model(void)
{
	/* Independent flat all-time history; implementation uses a bounded ring. */
	for (unsigned int round = 0; round < 100; ++round) {
		struct packet { uint64_t At; uint32_t Flags, Status; } history[512] = {{0}};
		xrudpsentwindow* w = NULL;
		xrudpsentevent events[40];
		xrudpsentstate state;
		xrudpackchange change;
		uint64_t now = 0, largest = 0, largestAcked = 0, oldest = 1, flight = 0;
		size_t pending = 0, count;
		CHECK(xrtRudpSentCreate(8, &w) == XRUDP_OK);
		for (unsigned int step = 0; step < 500; ++step) {
			uint32_t operation = random_word() % 4;
			now += random_word() % 400;
			if (operation <= 1 && largest < 511) {
				bool probe = (random_word() & 3) == 0;
				uint32_t flags = probe ? 3 : (random_word() & 1);
				bool room = largest - oldest + 1 < (probe ? 40u : 8u);
				CHECK(xrtRudpSentCanRecord(w, probe) == room);
				CHECK(xrtRudpSentRecord(w, largest + 1, now, 100, flags, largest + 901) ==
					(room ? XRUDP_OK : XRUDP_BACKPRESSURE));
				if (room) {
					++largest; ++pending; history[largest].At = now; history[largest].Flags = flags;
					if (flags & 1) flight += 100;
				}
			} else if (operation == 2 && largest != 0) {
				uint64_t number = 1 + random_word() % largest;
				xrudpack ack = one_ack(number, number);
				bool fresh = history[number].Status == 0;
				CHECK(xrtRudpSentAcknowledge(w, &ack, now, events, 40, &count, &change) == XRUDP_OK);
				CHECK(count == (fresh ? 1u : 0u) && change.NewlyAckedPackets == count);
				CHECK(change.NewlyAckedBytes == (fresh && (history[number].Flags & 1) ? 100u : 0u));
				if (fresh) {
					CHECK(events[0].PacketNumber == number && events[0].Kind == XRUDP_SENT_ACKED);
					CHECK(events[0].UserTag == number + 900);
					CHECK(change.RttSampleValid == ((history[number].Flags & 1) != 0));
					if (change.RttSampleValid) CHECK(change.RttSampleUs == now - history[number].At);
					history[number].Status = XRUDP_SENT_ACKED; --pending;
					if (history[number].Flags & 1) flight -= 100;
				} else CHECK(!change.RttSampleValid);
				if (number > largestAcked) largestAcked = number;
			} else {
				size_t wanted = 0;
				for (uint64_t i = 1; i <= largest; ++i) {
					uint32_t kind = 0;
					if (history[i].Status) continue;
					if (!(history[i].Flags & 1)) {
						if (now - history[i].At >= 1500) kind = XRUDP_SENT_EXPIRED;
					} else if (i < largestAcked && (largestAcked - i >= 3 || now - history[i].At >= 1000))
						kind = XRUDP_SENT_LOST;
					if (kind) { history[i].Status = kind; ++wanted; }
				}
				CHECK(xrtRudpSentDetectLoss(w, 1000, 1500, now, events, 40, &count) ==
					(wanted ? XRUDP_OK : XRUDP_DONE));
				CHECK(count == wanted);
				for (size_t i = 0; i < count; ++i) {
					uint64_t number = events[i].PacketNumber;
					CHECK(number <= largest && events[i].Kind == history[number].Status);
					CHECK(i == 0 || events[i - 1].PacketNumber < number);
					CHECK(events[i].UserTag == number + 900 && events[i].SentAtUs == history[number].At);
					if (history[number].Flags & 1) flight -= 100;
					--pending;
				}
			}
			while (oldest <= largest && history[oldest].Status) ++oldest;
			CHECK(xrtRudpSentState(w, &state) == XRUDP_OK);
			CHECK(state.LargestSent == largest && state.LargestAcked == largestAcked);
			CHECK(state.StoredPackets == largest - oldest + 1 && state.PendingPackets == pending);
			CHECK(state.BytesInFlight == flight);
		}
		CHECK(xrtRudpSentAbort(w, events, 40, &count) == XRUDP_OK && count == pending);
		xrtRudpSentDestroy(w);
	}
	return 0;
}

int main(void)
{
	CHECK(boundaries() == 0);
	CHECK(loss_and_reserve() == 0);
	CHECK(model() == 0);
	puts("xrudp sent: gap/future ACK rejection, atomic events, reserved probes and 50,000-step history model passed");
	return 0;
}
