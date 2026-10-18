#include <xrudp.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
static int configure(xrudpdatagrams* d, uint32_t id, uint8_t mode, bool replace, uint64_t lifetime)
{
	xrudpdatagramchannel c; xrtRudpDatagramChannelInit(&c); c.Id = id; c.Mode = mode; c.ReplacePending = replace; c.ReceiveLifetimeUs = lifetime;
	CHECK(xrtRudpDatagramConfigure(d, &c) == XRUDP_OK && xrtRudpDatagramConfigure(d, &c) == XRUDP_DUPLICATE);
	c.Weight = 2; CHECK(xrtRudpDatagramConfigure(d, &c) == XRUDP_ARGUMENT); return 0;
}
static int boundaries(void)
{
	xrudpdatagramconfig c; xrtRudpDatagramConfigInit(&c); c.Channels = 3; c.SendCapacity = c.ReceiveCapacity = 2; c.PayloadCapacity = 8;
	size_t allocation = 99; xrudpdatagrams* d = NULL;
	xrudpdatagramconfig bad = c; bad.Channels = 0; CHECK(xrtRudpDatagramAllocationSize(&bad, &allocation) == XRUDP_ARGUMENT && allocation == 99);
	CHECK(xrtRudpDatagramCreate(&c, &d) == XRUDP_OK && configure(d, 1, XRUDP_DATAGRAM_INDEPENDENT, false, 0) == 0);
	CHECK(configure(d, 2, XRUDP_DATAGRAM_ORDERED, false, 0) == 0 && configure(d, 3, XRUDP_DATAGRAM_LATEST, true, 10) == 0);
	xrudpdatagramchannel channel; xrtRudpDatagramChannelInit(&channel); channel.Id = 4; CHECK(xrtRudpDatagramConfigure(d, &channel) == XRUDP_LIMIT);
	uint64_t seq = 99; xrudpdatagramview first, view; xrudpdatagramstate before, after; xrudpdatagraminfo info; unsigned char output[16];
	CHECK(xrtRudpDatagramWrite(d, 2, "a", 1, 1, 20, &seq) == XRUDP_OK && seq == 1);
	CHECK(xrtRudpDatagramWrite(d, 3, "old", 3, 1, 0, &seq) == XRUDP_OK && seq == 1);
	CHECK(xrtRudpDatagramPeekSend(d, &first) == XRUDP_OK && first.Info.Channel == 2);
	seq = 99; CHECK(xrtRudpDatagramWrite(d, 1, "b", 1, 1, 0, &seq) == XRUDP_BACKPRESSURE && seq == 99);
	CHECK(xrtRudpDatagramWrite(d, 3, "new", 3, 2, 30, &seq) == XRUDP_OK && seq == 2);
	CHECK(xrtRudpDatagramSent(d, first.Info.Ticket, XRUDP_DATAGRAM_OVERHEAD + 1) == XRUDP_OK);
	CHECK(xrtRudpDatagramPeekSend(d, &first) == XRUDP_OK && first.Info.Channel == 3 && first.Info.Sequence == 2 && memcmp(first.Data,"new",3) == 0);
	CHECK(xrtRudpDatagramWrite(d, 3, "next", 4, 2, 25, &seq) == XRUDP_OK && seq == 3);
	CHECK(xrtRudpDatagramSent(d, first.Info.Ticket, 100) == XRUDP_ARGUMENT); /* Replaced ticket is never an ABA pop. */
	CHECK(xrtRudpDatagramPeekSend(d, &view) == XRUDP_OK && view.Info.Sequence == 3 && memcmp(view.Data,"next",4) == 0);
	CHECK(xrtRudpDatagramState(d, &before) == XRUDP_OK);
	CHECK(xrtRudpDatagramSent(d, view.Info.Ticket, 1) == XRUDP_ARGUMENT);
	CHECK(xrtRudpDatagramState(d, &after) == XRUDP_OK && memcmp(&before,&after,sizeof(before)) == 0);
	CHECK(xrtRudpDatagramWrite(d, 1, NULL, 0, 2, 5, &seq) == XRUDP_OK && seq == 0);
	CHECK(xrtRudpDatagramMaintain(d, 5) == XRUDP_OK && xrtRudpDatagramState(d, &after) == XRUDP_OK && after.SendExpired == 1 && after.SendQueued == 1 && after.NextDeadlineUs == 25);
	CHECK(xrtRudpDatagramMaintain(d, 25) == XRUDP_OK && xrtRudpDatagramPeekSend(d, &view) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpDatagramWrite(d, 3, "x", 1, 25, 25, &seq) == XRUDP_TIMEOUT);
	CHECK(xrtRudpDatagramWrite(d, 3, "123456789", 9, 25, 0, &seq) == XRUDP_LIMIT);
	/* A full receive queue must advance the seen sequence before dropping a
	 * newer record. The older, delayed record cannot reappear after draining. */
	CHECK(xrtRudpDatagramInput(d, 1, false, 0, NULL, 0, 25) == XRUDP_OK);
	CHECK(xrtRudpDatagramInput(d, 2, true, 5, "five", 4, 25) == XRUDP_OK);
	CHECK(xrtRudpDatagramInput(d, 2, true, 7, "seven", 5, 25) == XRUDP_OK);
	CHECK(xrtRudpDatagramRead(d, NULL, 0, 25, &info) == XRUDP_OK && info.Channel == 1 && info.Size == 0);
	CHECK(xrtRudpDatagramInput(d, 2, true, 6, "six", 3, 25) == XRUDP_OK);
	memset(output,0xa5,sizeof(output)); CHECK(xrtRudpDatagramRead(d, output, 3, 25, &info) == XRUDP_TRUNCATED && info.Size == 4 && output[0] == 0xa5);
	CHECK(xrtRudpDatagramRead(d, output, 16, 25, &info) == XRUDP_OK && info.Sequence == 5 && memcmp(output,"five",4) == 0);
	CHECK(xrtRudpDatagramRead(d, output, 16, 25, &info) == XRUDP_BACKPRESSURE && info.Size == 0);
	CHECK(xrtRudpDatagramInput(d, 3, true, 1, "old", 3, 26) == XRUDP_OK);
	CHECK(xrtRudpDatagramInput(d, 3, true, 3, "new", 3, 27) == XRUDP_OK);
	CHECK(xrtRudpDatagramInput(d, 3, true, 2, "late", 4, 27) == XRUDP_OK);
	CHECK(xrtRudpDatagramPeek(d, &info) == XRUDP_OK && info.Sequence == 3 && info.AtUs == 27 && info.DeadlineUs == 37);
	CHECK(xrtRudpDatagramRead(d, output, 16, 37, &info) == XRUDP_BACKPRESSURE && info.Size == 0);
	CHECK(xrtRudpDatagramInput(d, 9, false, 0, "x", 1, 37) == XRUDP_OK);
	CHECK(xrtRudpDatagramInput(d, 1, true, 1, "x", 1, 37) == XRUDP_OK);
	CHECK(xrtRudpDatagramInput(d, 2, true, UINT64_MAX, "max", 3, 37) == XRUDP_OK);
	CHECK(xrtRudpDatagramInput(d, 2, true, UINT64_MAX-1, "late", 4, 37) == XRUDP_OK);
	CHECK(xrtRudpDatagramState(d, &after) == XRUDP_OK && after.DroppedUnknown == 1 && after.DroppedMode == 1 && after.DroppedStale == 3 && after.DroppedCapacity == 1 && after.ReceiveReplaced == 1 && after.ReceiveExpired == 1);
	CHECK(xrtRudpDatagramWrite(d, 1, "secret", 6, 37, 0, &seq) == XRUDP_OK);
	CHECK(xrtRudpDatagramAbort(d) == XRUDP_OK && xrtRudpDatagramAbort(d) == XRUDP_DONE);
	CHECK(xrtRudpDatagramState(d, &after) == XRUDP_OK && after.Stopped && after.SendQueued == 0 && after.ReceiveQueued == 0 && after.SendBytes == 0 && after.ReceiveBytes == 0);
	CHECK(xrtRudpDatagramWrite(d, 1, NULL, 0, 37, 0, &seq) == XRUDP_CLOSED);
	CHECK(xrtRudpDatagramRead(d, output, 16, 37, &info) == XRUDP_CLOSED);
	xrtRudpDatagramDestroy(d); return 0;
}
typedef struct model_record { uint32_t Channel, Value; uint64_t Sequence, Deadline, At; } model_record;
static uint32_t seed = 0x6b9a3f17;
static uint32_t random_next(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static void remove_record(model_record* records, size_t* count, size_t at) { memmove(records + at, records + at + 1, (--*count - at) * sizeof(*records)); }
static int model(void)
{
	xrudpdatagramconfig c = {3,8,8,4}; xrudpdatagrams* d; CHECK(xrtRudpDatagramCreate(&c, &d) == XRUDP_OK);
	for (uint32_t id = 1; id <= 3; ++id) CHECK(configure(d,id,(uint8_t)(id-1),id==3,id==3?10:0) == 0);
	model_record tx[8], rx[8]; size_t ntx = 0, nrx = 0; uint64_t sequence[4] = {0}, seen[4] = {0};
	CHECK(xrtMemDebugFailAfter(0));
	for (uint32_t step = 1; step <= 100000; ++step) {
		uint64_t now = step / 3; CHECK(xrtRudpDatagramMaintain(d, now) == XRUDP_OK);
		for (size_t i = 0; i < ntx;) if (tx[i].Deadline && tx[i].Deadline <= now) remove_record(tx,&ntx,i); else ++i;
		for (size_t i = 0; i < nrx;) if (rx[i].Deadline && rx[i].Deadline <= now) remove_record(rx,&nrx,i); else ++i;
		uint32_t word = random_next(), id = 1 + word % 4, value = step;
		if (word % 10 < 4 && id <= 3) {
			uint64_t deadline = word & 32 ? now + 1 + word % 20 : 0, resultSequence = 999;
			size_t replace = ntx; if (id == 3) for (size_t i = 0; i < ntx; ++i) if (tx[i].Channel == id) replace = i;
			xrudpresult r = xrtRudpDatagramWrite(d,id,&value,4,now,deadline,&resultSequence);
			if (replace == ntx && ntx == 8) CHECK(r == XRUDP_BACKPRESSURE && resultSequence == 999);
			else {
				CHECK(r == XRUDP_OK); uint64_t expected = id == 1 ? 0 : ++sequence[id]; CHECK(resultSequence == expected);
				model_record rec = {id,value,expected,deadline,now}; if (replace == ntx) tx[ntx++] = rec; else tx[replace] = rec;
			}
		} else if (word % 10 < 8) {
			bool seq = id != 1, mismatch = (word & 255) == 0; if (mismatch) seq = !seq;
			uint64_t number = seq ? step + 1 + word % 7 : 0;
			CHECK(xrtRudpDatagramInput(d,id,seq,number,&value,4,now) == XRUDP_OK);
			if (id <= 3 && !mismatch && (!seq || number > seen[id])) {
				if (seq) seen[id] = number;
				size_t replace = nrx; if (id == 3) for (size_t i = 0; i < nrx; ++i) if (rx[i].Channel == id) replace = i;
				model_record rec = {id,value,number,id==3?now+10:0,now};
				if (replace != nrx) rx[replace] = rec; else if (nrx != 8) rx[nrx++] = rec;
			}
		} else if (word % 10 == 8) {
			xrudpdatagraminfo info; uint32_t data = 0; xrudpresult r = xrtRudpDatagramRead(d,&data,4,now,&info);
			if (nrx == 0) CHECK(r == XRUDP_BACKPRESSURE && info.Size == 0);
			else { CHECK(r == XRUDP_OK && info.Channel == rx[0].Channel && info.Sequence == rx[0].Sequence && info.AtUs == rx[0].At && data == rx[0].Value); remove_record(rx,&nrx,0); }
		} else {
			xrudpdatagramview view; xrudpresult r = xrtRudpDatagramPeekSend(d,&view);
			if (ntx == 0) CHECK(r == XRUDP_BACKPRESSURE);
			else {
				CHECK(r == XRUDP_OK); size_t i; for (i = 0; i < ntx && tx[i].Channel != view.Info.Channel; ++i) {}
				CHECK(i < ntx && view.Info.Sequence == tx[i].Sequence && view.Info.DeadlineUs == tx[i].Deadline && memcmp(view.Data,&tx[i].Value,4) == 0);
				if (!(word & 32)) { CHECK(xrtRudpDatagramSent(d,view.Info.Ticket,92) == XRUDP_OK); remove_record(tx,&ntx,i); }
			}
		}
		xrudpdatagramstate state; CHECK(xrtRudpDatagramState(d,&state) == XRUDP_OK && state.SendQueued == ntx && state.ReceiveQueued == nrx && state.SendBytes == ntx*4 && state.ReceiveBytes == nrx*4);
		uint64_t deadline = UINT64_MAX;
		for (size_t i=0;i<ntx;++i) if (tx[i].Deadline && tx[i].Deadline < deadline) deadline = tx[i].Deadline;
		for (size_t i=0;i<nrx;++i) if (rx[i].Deadline && rx[i].Deadline < deadline) deadline = rx[i].Deadline;
		CHECK(state.LastNowUs == now && state.NextDeadlineUs == deadline);
		for (uint32_t ch = 1; ch <= 3; ++ch) { xrudpdatagramchannelstate s; CHECK(xrtRudpDatagramChannelState(d,ch,&s) == XRUDP_OK && s.SendSequence == sequence[ch] && s.ReceiveSequence == seen[ch]); }
	}
	CHECK(xrtRudpDatagramAbort(d) == XRUDP_OK && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtRudpDatagramDestroy(d); return 0;
}
static int fairness(void)
{
	xrudpdatagramconfig c = {2,64,1,64}; xrudpdatagrams* d; CHECK(xrtRudpDatagramCreate(&c,&d) == XRUDP_OK);
	for (uint32_t id = 1; id <= 2; ++id) {
		xrudpdatagramchannel channel; xrtRudpDatagramChannelInit(&channel); channel.Id = id; channel.Weight = id == 1 ? 4 : 1;
		CHECK(xrtRudpDatagramConfigure(d,&channel) == XRUDP_OK);
	}
	unsigned char bytes[64] = {0}; uint64_t seq; uint32_t count[2] = {0};
	for (uint32_t i = 0; i < 64; ++i) CHECK(xrtRudpDatagramWrite(d,1+i%2,bytes,64,0,0,&seq) == XRUDP_OK);
	for (uint32_t i = 0; i < 20000; ++i) {
		xrudpdatagramview view; CHECK(xrtRudpDatagramPeekSend(d,&view) == XRUDP_OK);
		++count[view.Info.Channel-1]; CHECK(xrtRudpDatagramSent(d,view.Info.Ticket,152) == XRUDP_OK);
		CHECK(xrtRudpDatagramWrite(d,view.Info.Channel,bytes,64,0,0,&seq) == XRUDP_OK);
	}
	CHECK(count[1] > 0 && count[0]*100 > count[1]*380 && count[0]*100 < count[1]*420);
	printf("xrudp datagram weighted wire service 4:1: %u:%u packets\n",count[0],count[1]); xrtRudpDatagramDestroy(d); return 0;
}
int main(void)
{
	xrtClearError(); xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
	xrudpdatagramconfig c; xrtRudpDatagramConfigInit(&c); xrudpdatagrams* d = (xrudpdatagrams*)(uintptr_t)1;
	CHECK(xrtMemDebugFailAfter(0)); CHECK(xrtRudpDatagramCreate(&c,&d) == XRUDP_NO_MEMORY && d == (xrudpdatagrams*)(uintptr_t)1);
	xrtMemDebugFailClear(); xrtClearError(); xrtMemDebugSnapshot(&after); CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
	size_t reserved; CHECK(xrtRudpDatagramAllocationSize(&c,&reserved) == XRUDP_OK && xrtRudpDatagramCreate(&c,&d) == XRUDP_OK);
	xrtMemDebugSnapshot(&after); CHECK(after.LiveBytes - before.LiveBytes == reserved); xrtRudpDatagramDestroy(d);
	CHECK(boundaries() == 0 && model() == 0 && fairness() == 0);
	xrtClearError(); xrtMemDebugSnapshot(&after); CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
	CHECK(before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
	puts("xrudp datagrams: expiry, stale suppression, replacement tickets, bounded queues, 100000-step model and fixed allocation passed"); return 0;
}
