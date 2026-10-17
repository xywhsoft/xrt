/* Test-only single-stream driver. Not the production endpoint or its packet
 * transaction, authorization, resource admission, timer or close policy. */
#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s (seed=%u tick=%u)\n", __FILE__, __LINE__, #expr, seed, tick); return 1; \
} } while (0)
#define DATA_SIZE 32768u
#define REF_COUNT 64u
#define NET_COUNT 256u
#define PLAIN_SIZE (XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE)

static uint32_t seed, tick;
static uint64_t now;
static uint32_t random_word(void)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}
static void put32(unsigned char* p, uint32_t v)
{
	for (size_t i = 0; i < 4; ++i) p[3 - i] = (unsigned char)(v >> (i * 8));
}
static void put64(unsigned char* p, uint64_t v)
{
	for (size_t i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(v >> (i * 8));
}
static uint64_t get64(const unsigned char* p)
{
	uint64_t v = 0; for (size_t i = 0; i < 8; ++i) v = (v << 8) | p[i]; return v;
}

struct reference {
	uint64_t Number, Offset, Grant;
	uint32_t Size;
	bool Used, Fin;
};
struct endpoint {
	xrudpcipher* Cipher;
	xrudpsentwindow* Sent;
	xrudptransmit* Tx;
	xrudpreceive* Rx;
	xrudpcongestion Cc;
	xrudprtt Rtt;
	xrudpackset Ack;
	struct reference Ref[REF_COUNT];
	uint64_t Eliciting[XRUDP_REPLAY_BITS];
	uint64_t PtoAt, GrantAcked, GrantRetry;
	uint32_t PtoCount, Probes;
	size_t Queued, Consumed;
	bool AckWanted, Eof;
};
struct network {
	unsigned char Bytes[XRUDP_PACKET_MAX];
	size_t Size;
	uint32_t Target, Due;
	bool Used;
};
static struct {
	struct endpoint Ends[2];
	struct network Net[NET_COUNT];
	unsigned char Input[2][DATA_SIZE];
	uint32_t Drops, Duplicates, Tampered, Ptos, Delivered, Rotations;
} sim;

static int append(unsigned char* output, size_t* size, xrudpframetype type,
	uint8_t flags, const void* data, size_t length)
{
	size_t written;
	CHECK(xrtRudpFrameAppend(output + *size, PLAIN_SIZE - *size, &written, type, flags, data, length) == XRUDP_OK);
	*size += written;
	return 0;
}

static int feedback(struct endpoint* e, const xrudpsentevent* events, size_t count)
{
	xrudptransmitchange change;
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* p = &events[i];
		CHECK(p->UserTag > 0 && p->UserTag <= REF_COUNT);
		struct reference* r = &e->Ref[p->UserTag - 1];
		CHECK(r->Used && r->Number == p->PacketNumber);
		if (p->Kind == XRUDP_SENT_CANCELLED) {
			xrudptransmitstate tx;
			CHECK(xrtRudpTransmitState(e->Tx, &tx) == XRUDP_OK && tx.Cancelled);
		} else if (r->Size != 0 || r->Fin) {
			CHECK(p->Kind == XRUDP_SENT_ACKED || p->Kind == XRUDP_SENT_LOST);
			xrudpresult result = xrtRudpTransmitResolve(e->Tx, r->Offset, r->Size, r->Fin,
				p->Kind == XRUDP_SENT_ACKED, &change);
			CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE);
		}
		if (p->Kind == XRUDP_SENT_ACKED && r->Grant > e->GrantAcked) e->GrantAcked = r->Grant;
		if (p->Kind == XRUDP_SENT_LOST && r->Grant != 0) e->GrantRetry = 0;
		r->Used = false;
	}
	return 0;
}

static int acknowledge(struct endpoint* e, const void* frame, size_t size)
{
	xrudpack ack;
	xrudpackchange change;
	xrudpsentevent acked[REF_COUNT], final[REF_COUNT * 2];
	size_t ackCount, lossCount;
	uint64_t delay, interval;
	CHECK(xrtRudpAckDecode(frame, size, &ack) == XRUDP_OK);
	CHECK(xrtRudpSentAcknowledge(e->Sent, &ack, now, acked, REF_COUNT, &ackCount, &change) == XRUDP_OK);
	if (change.RttSampleValid) CHECK(xrtRudpRttSample(&e->Rtt, change.RttSampleUs, change.PeerAckDelayUs) == XRUDP_OK);
	CHECK(xrtRudpRttLossDelay(&e->Rtt, &delay) == XRUDP_OK);
	xrudpresult result = xrtRudpSentDetectLoss(e->Sent, delay, 300000, now, final, REF_COUNT * 2, &lossCount);
	CHECK(result == XRUDP_OK || result == XRUDP_DONE);
	memcpy(final + lossCount, acked, ackCount * sizeof(*acked));
	CHECK(xrtRudpCongestionApply(&e->Cc, final, lossCount + ackCount, now, false) == XRUDP_OK);
	CHECK(feedback(e, final, lossCount + ackCount) == 0);
	if (change.NewlyAckedEliciting) {
		e->PtoCount = 0; e->Probes = 0;
		CHECK(xrtRudpRttPto(&e->Rtt, 0, &interval) == XRUDP_OK);
		e->PtoAt = now + interval;
	}
	return 0;
}

static int receive_packet(uint32_t target, const unsigned char* packet, size_t size)
{
	struct endpoint* e = &sim.Ends[target];
	unsigned char plain[PLAIN_SIZE];
	xrudpwirepacket parsed;
	xrudpwireframe frame;
	xrudpreceivechange change;
	size_t length, cursor = 0;
	bool eliciting = false;
	xrudpresult result = xrtRudpCipherOpen(e->Cipher, packet, size, plain, sizeof(plain), &length);
	if (result == XRUDP_AUTH_FAILED || result == XRUDP_TOO_OLD) return 0;
	CHECK(xrtRudpPacketDecode(packet, size, &parsed) == XRUDP_OK);
	uint64_t number = parsed.Header.PacketNumber;
	if (result == XRUDP_DUPLICATE) {
		if (xrtRudpAckSetContains(&e->Ack, number) && e->Eliciting[number % XRUDP_REPLAY_BITS] == number) e->AckWanted = true;
		return 0;
	}
	CHECK(result == XRUDP_OK);
	while (cursor < length) {
		size_t start = cursor;
		CHECK(xrtRudpFrameNext(plain, length, &cursor, &frame) == XRUDP_OK);
		if (frame.Type == XRUDP_FRAME_ACK) {
			CHECK(acknowledge(e, plain + start, cursor - start) == 0);
		} else if (frame.Type == XRUDP_FRAME_STREAM) {
			result = xrtRudpReceivePush(e->Rx, get64(frame.Data + 4), frame.Data + 12,
				frame.Size - 12, (frame.Flags & XRUDP_STREAM_FIN) != 0, &change);
			CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE); eliciting = true;
		} else if (frame.Type == XRUDP_FRAME_MAX_STREAM_DATA) {
			result = xrtRudpTransmitSetLimit(e->Tx, get64(frame.Data + 4));
			CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE); eliciting = true;
		} else if (frame.Type == XRUDP_FRAME_PING) eliciting = true;
		else CHECK(frame.Type == XRUDP_FRAME_KEY_CONFIRM);
	}
	result = xrtRudpAckSetRecord(&e->Ack, number, now);
	CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE || result == XRUDP_TOO_OLD);
	if (eliciting) { e->AckWanted = true; e->Eliciting[number % XRUDP_REPLAY_BITS] = number; }
	++sim.Delivered;
	return 0;
}

static int queue_packet(uint32_t target, const unsigned char* packet, size_t size)
{
	/* Symmetric burst outage plus independent packet and ACK loss. */
	if ((tick >= 300 && tick < 450) || random_word() % 100 < 25) { ++sim.Drops; return 0; }
	uint32_t copies = random_word() % 5 == 0 ? 2 : 1;
	if (copies == 2) ++sim.Duplicates;
	for (uint32_t copy = 0; copy < copies; ++copy) {
		size_t slot;
		for (slot = 0; slot < NET_COUNT && sim.Net[slot].Used; ++slot) {}
		CHECK(slot < NET_COUNT);
		struct network* n = &sim.Net[slot];
		memcpy(n->Bytes, packet, size); n->Size = size; n->Target = target;
		n->Due = tick + 1 + random_word() % 25; n->Used = true;
	}
	if (random_word() % 7 == 0) {
		unsigned char bad[XRUDP_PACKET_MAX], untouched[PLAIN_SIZE]; size_t read;
		memcpy(bad, packet, size); bad[random_word() % size] ^= 1;
		CHECK(xrtRudpCipherOpen(sim.Ends[target].Cipher, bad, size, untouched, sizeof(untouched), &read) == XRUDP_AUTH_FAILED);
		++sim.Tampered;
	}
	return 0;
}

static int send_packet(uint32_t index)
{
	struct endpoint* e = &sim.Ends[index];
	unsigned char plain[PLAIN_SIZE], body[XRUDP_STREAM_CHUNK_MAX + 12], packet[XRUDP_PACKET_MAX];
	xrudptransmitframe frame;
	xrudptransmitchange change;
	xrudpreceivestate rx;
	xrudpcipherstate cipher;
	xrudpwirepacket parsed;
	size_t length = 0, size, extra, slot;
	uint64_t due, interval;
	bool probe = e->Probes != 0, data = false, grant = false, eliciting = false;
	CHECK(xrtRudpReceiveState(e->Rx, &rx) == XRUDP_OK);
	grant = rx.Limit > e->GrantAcked && now >= e->GrantRetry;
	if (e->AckWanted && e->Ack.Count) {
		CHECK(xrtRudpAckSetEncode(&e->Ack, now, plain, sizeof(plain), &length) == XRUDP_OK);
	}
	if (grant) { put32(body, 1); put64(body + 4, rx.Limit); CHECK(append(plain, &length, XRUDP_FRAME_MAX_STREAM_DATA, 0, body, 12) == 0); eliciting = true; }
	/* Always reserve KEY_CONFIRM overhead before choosing a stream chunk. */
	extra = sizeof(plain) - length - 8 - 16;
	xrudpresult peek = xrtRudpTransmitPeek(e->Tx, probe, body + 12, extra, &frame);
	CHECK(peek == XRUDP_OK || peek == XRUDP_BACKPRESSURE);
	if (peek == XRUDP_OK) {
		put32(body, 1); put64(body + 4, frame.Offset);
		CHECK(append(plain, &length, XRUDP_FRAME_STREAM, frame.Fin ? 1 : 0, body, frame.Size + 12) == 0);
		data = eliciting = true;
	} else if (probe) { CHECK(append(plain, &length, XRUDP_FRAME_PING, 0, NULL, 0) == 0); eliciting = true; }
	if (!length) return 0;
	CHECK(xrtRudpCipherState(e->Cipher, &cipher) == XRUDP_OK);
	if (eliciting) { put32(body, cipher.ReceiveEpoch); CHECK(append(plain, &length, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4) == 0); }
	uint32_t flags = eliciting ? XRUDP_SENT_ACK_ELICITING : 0;
	if (probe) flags |= XRUDP_SENT_PROBE;
	xrudpresult ready = xrtRudpCongestionCanSend(&e->Cc, now, e->Rtt.SmoothedUs,
		(uint32_t)(length + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE), flags, &due);
	CHECK(ready == XRUDP_OK || ready == XRUDP_TIMEOUT || ready == XRUDP_BACKPRESSURE);
	if (ready != XRUDP_OK || !xrtRudpSentCanRecord(e->Sent, probe)) return 0;
	for (slot = 0; slot < REF_COUNT && e->Ref[slot].Used; ++slot) {}
	CHECK(slot < REF_COUNT);
	if (cipher.PacketsInSendEpoch >= 64 && !cipher.SendKeyPending) {
		CHECK(xrtRudpCipherUpdateKey(e->Cipher) == XRUDP_OK); ++sim.Rotations;
	}
	CHECK(xrtRudpCipherSeal(e->Cipher, plain, length, packet, sizeof(packet), &size) == XRUDP_OK);
	CHECK(xrtRudpPacketDecode(packet, size, &parsed) == XRUDP_OK);
	uint64_t number = parsed.Header.PacketNumber;
	CHECK(xrtRudpSentRecord(e->Sent, number, now, (uint32_t)size, flags, slot + 1) == XRUDP_OK);
	CHECK(xrtRudpCongestionOnSent(&e->Cc, number, now, e->Rtt.SmoothedUs, (uint32_t)size, flags) == XRUDP_OK);
	struct reference* ref = &e->Ref[slot]; memset(ref, 0, sizeof(*ref));
	ref->Used = true; ref->Number = number; ref->Grant = grant ? rx.Limit : 0;
	if (data) {
		CHECK(xrtRudpTransmitMarkSent(e->Tx, frame.Offset, frame.Size, frame.Fin != 0, &change) == XRUDP_OK);
		ref->Offset = frame.Offset; ref->Size = frame.Size; ref->Fin = frame.Fin != 0;
	}
	if (e->AckWanted) e->AckWanted = false;
	if (probe) --e->Probes;
	if (eliciting && !e->PtoAt) { CHECK(xrtRudpRttPto(&e->Rtt, e->PtoCount, &interval) == XRUDP_OK); e->PtoAt = now + interval; }
	if (grant) { CHECK(xrtRudpRttPto(&e->Rtt, 0, &interval) == XRUDP_OK); e->GrantRetry = now + interval; }
	CHECK(queue_packet(1 - index, packet, size) == 0);
	return 0;
}

static int maintain(uint32_t index)
{
	struct endpoint* e = &sim.Ends[index];
	xrudpsentevent lost[REF_COUNT];
	xrudpsentstate sent;
	xrudpreceivestate rx;
	unsigned char output[128];
	size_t count, written;
	uint64_t delay, interval;
	CHECK(xrtRudpRttLossDelay(&e->Rtt, &delay) == XRUDP_OK);
	xrudpresult result = xrtRudpSentDetectLoss(e->Sent, delay, 300000, now, lost, REF_COUNT, &count);
	CHECK(result == XRUDP_OK || result == XRUDP_DONE);
	CHECK(xrtRudpCongestionApply(&e->Cc, lost, count, now, false) == XRUDP_OK);
	CHECK(feedback(e, lost, count) == 0);
	if (e->Queued < DATA_SIZE) {
		result = xrtRudpTransmitWrite(e->Tx, sim.Input[index] + e->Queued, DATA_SIZE - e->Queued, &written);
		CHECK(result == XRUDP_OK || result == XRUDP_BACKPRESSURE); e->Queued += written;
		if (e->Queued == DATA_SIZE) CHECK(xrtRudpTransmitFinish(e->Tx) == XRUDP_OK);
	}
	if (random_word() % 3 == 0) {
		result = xrtRudpReceiveRead(e->Rx, output, 1 + random_word() % sizeof(output), &count);
		CHECK(result == XRUDP_OK || result == XRUDP_BACKPRESSURE || result == XRUDP_DONE);
		CHECK(e->Consumed + count <= DATA_SIZE && memcmp(output, sim.Input[1 - index] + e->Consumed, count) == 0);
		e->Consumed += count;
		if (result == XRUDP_DONE) { CHECK(e->Consumed == DATA_SIZE); e->Eof = true; }
		if (count) {
			CHECK(xrtRudpReceiveState(e->Rx, &rx) == XRUDP_OK);
			CHECK(xrtRudpReceiveExtend(e->Rx, rx.ReadOffset + rx.Capacity) == XRUDP_OK); e->GrantRetry = 0;
		}
	}
	CHECK(xrtRudpSentState(e->Sent, &sent) == XRUDP_OK);
	CHECK(sent.BytesInFlight == e->Cc.BytesInFlight);
	if (sent.BytesInFlight == 0) { e->PtoAt = 0; e->Probes = 0; }
	else if (e->PtoAt && now >= e->PtoAt && e->Probes == 0) {
		++e->PtoCount; e->Probes = 2; ++sim.Ptos;
		CHECK(xrtRudpRttPto(&e->Rtt, e->PtoCount, &interval) == XRUDP_OK);
		CHECK(interval < UINT64_MAX - now); e->PtoAt = now + interval;
	}
	return 0;
}

static int scenario(uint32_t initialSeed)
{
	unsigned char keys[2][32], ids[2][16] = {{1},{2}};
	bool done = false;
	memset(&sim, 0, sizeof(sim)); seed = initialSeed; tick = 0; now = 0;
	for (size_t i = 0; i < 32; ++i) { keys[0][i] = (unsigned char)i; keys[1][i] = (unsigned char)(i + 100); }
	for (uint32_t index = 0; index < 2; ++index) {
		struct endpoint* e = &sim.Ends[index];
		for (size_t i = 0; i < DATA_SIZE; ++i) sim.Input[index][i] = (unsigned char)((i * 37 + index * 73) ^ (i >> 9));
		CHECK(xrtRudpCipherCreate(keys[index], keys[1 - index], ids[index], ids[1 - index], 0, &e->Cipher) == XRUDP_OK);
		CHECK(xrtRudpSentCreate(32, &e->Sent) == XRUDP_OK);
		CHECK(xrtRudpTransmitCreate(512, 512, &e->Tx) == XRUDP_OK);
		CHECK(xrtRudpReceiveCreate(512, 512, &e->Rx) == XRUDP_OK);
		CHECK(xrtRudpCongestionInit(&e->Cc, 12000) == XRUDP_OK);
		CHECK(xrtRudpRttInit(&e->Rtt, 30000, 25000) == XRUDP_OK);
		xrtRudpAckSetInit(&e->Ack); e->GrantAcked = 512;
	}
	for (tick = 1; tick <= 200000 && !done; ++tick) {
		now = (uint64_t)tick * 1000;
		for (size_t i = 0; i < NET_COUNT; ++i) if (sim.Net[i].Used && sim.Net[i].Due <= tick) {
			sim.Net[i].Used = false; CHECK(receive_packet(sim.Net[i].Target, sim.Net[i].Bytes, sim.Net[i].Size) == 0);
		}
		for (uint32_t i = 0; i < 2; ++i) {
			CHECK(maintain(i) == 0);
			for (size_t packet = 0; packet < 2; ++packet) CHECK(send_packet(i) == 0);
		}
		done = true;
		for (size_t i = 0; i < 2; ++i) {
			xrudptransmitstate tx;
			CHECK(xrtRudpTransmitState(sim.Ends[i].Tx, &tx) == XRUDP_OK);
			if (!tx.FinAcknowledged || tx.PendingBytes || !sim.Ends[i].Eof) done = false;
		}
	}
	CHECK(done && sim.Drops && sim.Duplicates && sim.Tampered && sim.Ptos && sim.Rotations);
	for (size_t i = 0; i < 2; ++i) {
		struct endpoint* e = &sim.Ends[i]; xrudpsentevent final[REF_COUNT]; size_t count;
		xrudptransmitchange cancelled;
		CHECK(xrtRudpTransmitCancel(e->Tx, &cancelled) == XRUDP_OK && cancelled.ReleasedBytes == 0);
		CHECK(xrtRudpSentAbort(e->Sent, final, REF_COUNT, &count) == XRUDP_OK);
		CHECK(xrtRudpCongestionApply(&e->Cc, final, count, now, true) == XRUDP_OK);
		CHECK(feedback(e, final, count) == 0 && e->Cc.BytesInFlight == 0);
		for (size_t slot = 0; slot < REF_COUNT; ++slot) CHECK(!e->Ref[slot].Used);
		xrtRudpSentDestroy(e->Sent); xrtRudpTransmitDestroy(e->Tx);
		xrtRudpReceiveDestroy(e->Rx); xrtRudpCipherDestroy(e->Cipher);
	}
	printf("xrudp encrypted simulation seed=%u: ticks=%u drop=%u duplicate=%u tamper=%u PTO=%u key-updates=%u\n",
		initialSeed, tick, sim.Drops, sim.Duplicates, sim.Tampered, sim.Ptos, sim.Rotations);
	return 0;
}

int main(void)
{
	CHECK(scenario(0x8734bc17) == 0);
	CHECK(scenario(0x1357a9cf) == 0);
	CHECK(scenario(0xe6713569) == 0);
	puts("xrudp simulated full-duplex delivery: exact bytes, bounded buffers and complete FIN/reference cleanup passed");
	return 0;
}
