#include <xrudp.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #e); return 1; } } while (0)
#define MTU_FLAGS (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING | XRUDP_SENT_MTU)

static int probes(void)
{
	xrudpsentwindow* w; xrudpcongestion cc; xrudpsentstate state, saved;
	xrudpsentevent event, unchanged, events[4]; xrudpackchange change; size_t count;
	xrudpack ack = {0}; ack.Count = 1;
	CHECK(xrtRudpSentCreate(1, &w) == XRUDP_OK && xrtRudpCongestionInit(&cc, 0) == XRUDP_OK);
	CHECK(xrtMemDebugFailAfter(0));
	uint64_t now = 10000, number = 0;
	for (uint32_t step = 0; step < 1000; ++step) {
		uint32_t flags = step % 2 ? MTU_FLAGS : XRUDP_SENT_ACK_ELICITING;
		++number; CHECK(xrtRudpSentRecord(w, number, now, 1200, flags, number) == XRUDP_OK);
		CHECK(xrtRudpCongestionOnSent(&cc, number, now, 1000, 1200, flags) == XRUDP_OK);
		CHECK(!xrtRudpSentCanRecord(w, false) && xrtRudpSentCanRecord(w, true));
		CHECK(xrtRudpSentState(w, &saved) == XRUDP_OK);
		memset(&event, 0xa5, sizeof(event)); unchanged = event;
		CHECK(xrtRudpSentDiscardPacket(w, number + 1, now, &event) == XRUDP_ARGUMENT && memcmp(&event, &unchanged, sizeof(event)) == 0);
		CHECK(xrtRudpSentDiscardPacket(w, number, now - 1, &event) == XRUDP_ARGUMENT);
		CHECK(xrtRudpSentState(w, &state) == XRUDP_OK && memcmp(&saved, &state, sizeof(state)) == 0);
		now += 10000; uint64_t window = cc.Window, recovery = cc.RecoveryThrough;
		if (step % 3) {
			CHECK(xrtRudpSentDiscardPacket(w, number, now, &event) == XRUDP_OK && event.Kind == XRUDP_SENT_LOST);
			CHECK(event.PacketNumber == number && event.UserTag == number && event.Bytes == 1200 && event.Flags == flags);
		} else {
			ack.Ranges[0].First = ack.Ranges[0].Last = number;
			CHECK(xrtRudpSentAcknowledge(w, &ack, now, &event, 1, &count, &change) == XRUDP_OK && count == 1 && event.Kind == XRUDP_SENT_ACKED);
		}
		CHECK(xrtRudpCongestionApply(&cc, &event, 1, now, false) == XRUDP_OK && cc.BytesInFlight == 0);
		if (flags & XRUDP_SENT_MTU) CHECK(cc.Window == window && cc.RecoveryThrough == recovery);
		CHECK(xrtRudpSentState(w, &saved) == XRUDP_OK && saved.PendingPackets == 0 && saved.BytesInFlight == 0 && saved.StoredPackets == 0);
		event = unchanged;
		CHECK(xrtRudpSentDiscardPacket(w, number, now + 1, &event) == XRUDP_DONE && memcmp(&event, &unchanged, sizeof(event)) == 0);
		CHECK(xrtRudpSentState(w, &state) == XRUDP_OK && memcmp(&saved, &state, sizeof(state)) == 0);
		ack.Ranges[0].First = ack.Ranges[0].Last = number;
		CHECK(xrtRudpSentAcknowledge(w, &ack, now, events, 4, &count, &change) == XRUDP_OK && count == 0 && change.NewlyAckedBytes == 0 && !change.RttSampleValid);
		now += 10000;
	}
	/* MTU never buys a reserve slot or bypasses ordinary congestion admission. */
	for (uint32_t flags = 8; flags < 16; ++flags) if (flags != MTU_FLAGS) {
		CHECK(xrtRudpSentRecord(w, number + 1, now, 1200, flags, 1) == XRUDP_ARGUMENT);
		uint64_t due = 123; CHECK(xrtRudpCongestionCanSend(&cc, now, 1000, 1200, flags, &due) == XRUDP_ARGUMENT && due == 123);
	}
	CHECK(xrtRudpSentRecord(w, number + 1, now, 511, MTU_FLAGS, 1) == XRUDP_ARGUMENT);
	CHECK(xrtRudpSentRecord(w, number + 1, now, 1200, MTU_FLAGS, 1) == XRUDP_OK);
	CHECK(xrtRudpSentRecord(w, number + 3, now, 100, XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PROBE, 3) == XRUDP_OK);
	CHECK(xrtRudpSentDiscardPacket(w, number + 2, now, &event) == XRUDP_ARGUMENT);
	CHECK(xrtRudpSentDiscardPacket(w, number + 1, now, &event) == XRUDP_OK);
	CHECK(xrtRudpSentDiscardPacket(w, number + 3, now, &event) == XRUDP_OK);
	CHECK(xrtRudpSentRecord(w, number + 4, now, 100, 0, 4) == XRUDP_OK);
	CHECK(xrtRudpSentDiscardPacket(w, number + 4, now, &event) == XRUDP_OK && event.Kind == XRUDP_SENT_EXPIRED);
	CHECK(xrtRudpSentAbort(w, events, 4, &count) == XRUDP_OK);
	CHECK(xrtRudpSentDiscardPacket(w, number, now, &event) == XRUDP_CLOSED);
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtRudpSentDestroy(w);
	/* Mixed feedback must still reduce ordinary data congestion, exactly once. */
	CHECK(xrtRudpCongestionInit(&cc, 0) == XRUDP_OK);
	CHECK(xrtRudpCongestionOnSent(&cc, 1, 0, 1000, 1200, MTU_FLAGS) == XRUDP_OK);
	CHECK(xrtRudpCongestionOnSent(&cc, 2, 10000, 1000, 1200, XRUDP_SENT_ACK_ELICITING) == XRUDP_OK);
	memset(events, 0, sizeof(events));
	events[0].PacketNumber = 1; events[0].Bytes = 1200; events[0].Flags = MTU_FLAGS; events[0].Kind = XRUDP_SENT_LOST;
	events[1].PacketNumber = 2; events[1].Bytes = 1200; events[1].Flags = XRUDP_SENT_ACK_ELICITING; events[1].Kind = XRUDP_SENT_LOST; events[1].SentAtUs = 10000;
	CHECK(xrtRudpCongestionApply(&cc, events, 2, 20000, false) == XRUDP_OK && cc.BytesInFlight == 0 && cc.Window == 6000 && cc.RecoveryThrough == 2);
	return 0;
}

static int trim(void)
{
	xrudpdatagrams* d; xrudpdatagramconfig config = {3,256,4,8};
	CHECK(xrtRudpDatagramCreate(&config, &d) == XRUDP_OK);
	for (uint32_t i = 1; i <= 3; ++i) {
		xrudpdatagramchannel c; xrtRudpDatagramChannelInit(&c); c.Id = i; c.Mode = (uint8_t)(i - 1);
		CHECK(xrtRudpDatagramConfigure(d, &c) == XRUDP_OK);
	}
	uint64_t seq, maxTicket = 0, lastSequence[3] = {0}; uint32_t count = 999;
	CHECK(xrtMemDebugFailAfter(0));
	for (uint32_t i = 0; i < 256; ++i) {
		unsigned char payload[8]; memset(payload, (int)(i % 8 + 1), sizeof(payload));
		CHECK(xrtRudpDatagramWrite(d, 1 + i % 3, payload, i % 8, 10, 100 + i, &seq) == XRUDP_OK);
	}
	CHECK(xrtRudpDatagramInput(d, 2, true, 9, "receive", 7, 10) == XRUDP_OK);
	xrudpdatagramstate before, after; CHECK(xrtRudpDatagramState(d, &before) == XRUDP_OK);
	CHECK(xrtRudpDatagramTrimSend(d, XRUDP_DATAGRAM_PAYLOAD_MAX + 1, &count) == XRUDP_ARGUMENT && count == 0);
	CHECK(xrtRudpDatagramTrimSend(d, 4, NULL) == XRUDP_ARGUMENT);
	CHECK(xrtRudpDatagramState(d, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
	CHECK(xrtRudpDatagramTrimSend(d, 3, &count) == XRUDP_OK && count == 128);
	CHECK(xrtRudpDatagramState(d, &after) == XRUDP_OK && after.SendSizeDropped == 128 && after.SendQueued == 128 && after.SendBytes == 192 && after.ReceiveQueued == 1 && after.ReceiveBytes == 7 && after.LastNowUs == 10 && after.NextDeadlineUs == 100);
	for (uint32_t i = 0; i < 128; ++i) {
		xrudpdatagramview view; CHECK(xrtRudpDatagramPeekSend(d, &view) == XRUDP_OK && view.Info.Size <= 3);
		if (view.Info.Size) CHECK(*(const unsigned char*)view.Data == view.Info.Size + 1);
		if (view.Info.Sequenced) { CHECK(view.Info.Sequence > lastSequence[view.Info.Channel - 1]); lastSequence[view.Info.Channel - 1] = view.Info.Sequence; }
		if (view.Info.Ticket > maxTicket) maxTicket = view.Info.Ticket;
		CHECK(xrtRudpDatagramSent(d, view.Info.Ticket, view.Info.Size + XRUDP_DATAGRAM_OVERHEAD) == XRUDP_OK);
	}
	CHECK(xrtRudpDatagramTrimSend(d, 0, &count) == XRUDP_OK && count == 0);
	CHECK(xrtRudpDatagramWrite(d, 2, "new", 3, 10, 20, &seq) == XRUDP_OK && seq > lastSequence[1]);
	xrudpdatagramview view; CHECK(xrtRudpDatagramPeekSend(d, &view) == XRUDP_OK && view.Info.Ticket > maxTicket);
	uint64_t oldTicket = view.Info.Ticket;
	CHECK(xrtRudpDatagramWrite(d, 1, NULL, 0, 10, 0, &seq) == XRUDP_OK);
	CHECK(xrtRudpDatagramTrimSend(d, 0, &count) == XRUDP_OK && count == 1);
	CHECK(xrtRudpDatagramSent(d, oldTicket, 100) == XRUDP_ARGUMENT);
	CHECK(xrtRudpDatagramPeekSend(d, &view) == XRUDP_OK && view.Info.Size == 0);
	CHECK(xrtRudpDatagramSent(d, view.Info.Ticket, XRUDP_DATAGRAM_OVERHEAD) == XRUDP_OK);
	CHECK(xrtRudpDatagramWrite(d, 2, "ok", 2, 10, 0, &seq) == XRUDP_OK);
	CHECK(xrtRudpDatagramPeekSend(d, &view) == XRUDP_OK && view.Info.Ticket > oldTicket);
	CHECK(xrtRudpDatagramAbort(d) == XRUDP_OK && xrtRudpDatagramTrimSend(d, 0, &count) == XRUDP_CLOSED && count == 0);
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtRudpDatagramDestroy(d); return 0;
}

int main(void)
{
	xmemdebugsnapshot before, after; xrtClearError(); xrtMemDebugSnapshot(&before);
	CHECK(probes() == 0 && trim() == 0);
	xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes && before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
	puts("xrudp MTU ledger: 1000 unique retire/late ACK cycles, isolated size-probe loss and bounded game queue trimming passed"); return 0;
}
