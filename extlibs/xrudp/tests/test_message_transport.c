#include <xrudp.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>

/* Default mode tests a byte-stream adapter. TEST_SESSION_MESSAGES also enables
 * real kind=1 streams in both directions, sharing the same impaired wire. */
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s (tick=%u)\n",__FILE__,__LINE__,#e,tick); return 1; } } while (0)
#define CHANNELS 3u
#define RECORDS 12u
#define QUEUE 128u
typedef struct message_packet { unsigned char Data[1200]; size_t Size; uint32_t Target, Due; bool Used; } message_packet;
static struct {
	xrudpsession* Session[2]; xrudpmessage* Decoder[2][CHANNELS]; message_packet Packet[QUEUE];
	bool Active[2][CHANNELS], Eof[2][CHANNELS], ShortChecked[2][CHANNELS];
	size_t Offset[2][CHANNELS]; uint32_t Written[2][CHANNELS], Read[2][CHANNELS];
	uint32_t Dropped, Blocked, Duplicated, Deferred, Epoch[2], RetiredKeys;
} net;
static uint32_t tick, randomState;
static const uint32_t ids[CHANNELS] = {1,3,2};
static uint32_t random_next(void) { randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5; return randomState; }
static size_t record_size(uint32_t n) { static const size_t sizes[] = {0,1,33,252,508,97}; return sizes[n % 6]; }
static unsigned char pattern(uint32_t side, uint32_t channel, uint32_t n, size_t at)
{
	return (unsigned char)((side * 73 + channel * 31 + n * 17 + at * 13) ^ (at >> 4));
}
static xrudpresult send_packet(const void* data, size_t size, void* context)
{
	uint32_t side = (uint32_t)(uintptr_t)context;
	if (random_next() % 19 == 0) { ++net.Blocked; return XRUDP_BACKPRESSURE; }
	if ((tick >= 80 && tick < 240) || random_next() % 100 < 25) { ++net.Dropped; return XRUDP_OK; }
	unsigned int copies = random_next() % 5 == 0 ? 2 : 1; if (copies == 2) ++net.Duplicated;
	for (unsigned int c = 0; c < copies; ++c) {
		size_t i; for (i = 0; i < QUEUE && net.Packet[i].Used; ++i) {}
		if (i == QUEUE) return XRUDP_IO_ERROR;
		message_packet* p = &net.Packet[i]; p->Used = true; p->Size = size; p->Target = 1 - side; p->Due = tick + 1 + random_next() % 12;
		memcpy(p->Data, data, size);
	}
	return XRUDP_OK;
}
static int cipher_pair(xrudpcipher* cipher[2])
{
	unsigned char key[2][32] = {{3},{11}}, id[2][16] = {{7},{19}}, packet[2][100], plain[100]; size_t size[2], read;
	unsigned char proof[12] = {XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	for (size_t i = 0; i < 2; ++i) {
		CHECK(xrtRudpCipherCreate(key[i], key[1-i], id[i], id[1-i], 0, &cipher[i]) == XRUDP_OK);
		CHECK(xrtRudpCipherSeal(cipher[i], proof, 12, packet[i], 100, &size[i]) == XRUDP_OK);
	}
	for (size_t i = 0; i < 2; ++i) CHECK(xrtRudpCipherOpen(cipher[i], packet[1-i], size[1-i], plain, 100, &read) == XRUDP_OK);
	return 0;
}
static int drain(uint32_t side, size_t k, bool stalled, bool* wake)
{
	xrudpmessage* m = net.Decoder[side][k]; unsigned char buffer[512]; size_t size, consumed;
	for (size_t n = 0; n < 32; ++n) { /* Application callback work is finite. */
		xrudpresult ready = xrtRudpMessagePeek(m, &size);
#ifdef TEST_SESSION_MESSAGES
		if (k != 0) {
			ready = xrtRudpSessionMessagePeek(net.Session[side], ids[k], &size);
			xrudpstreamstate state; CHECK(xrtRudpSessionStreamState(net.Session[side], ids[k], &state) == XRUDP_OK);
			CHECK(state.Kind == XRUDP_STREAM_MESSAGES && state.SendMessageLimit == 508 && state.Message.Buffered <= 508);
			if (ready == XRUDP_BACKPRESSURE) { CHECK(!state.ReadReady); return 0; }
		}
#endif
		if (ready == XRUDP_OK) {
			if (stalled) return 0;
			uint32_t r = net.Read[side][k]; CHECK(r < RECORDS && size == record_size(r));
			if (size != 0 && !net.ShortChecked[side][k]) {
				memset(buffer, 0xa5, sizeof(buffer)); size_t required;
				xrudpresult shortRead;
#ifdef TEST_SESSION_MESSAGES
				if (k != 0) shortRead = xrtRudpSessionMessageRead(net.Session[side], ids[k], buffer, size - 1, &required); else
#endif
				shortRead = xrtRudpMessageRead(m, buffer, size - 1, &required);
				CHECK(shortRead == XRUDP_TRUNCATED && required == size && buffer[0] == 0xa5);
				net.ShortChecked[side][k] = true;
			}
			xrudpresult read;
#ifdef TEST_SESSION_MESSAGES
			if (k != 0) read = xrtRudpSessionMessageRead(net.Session[side], ids[k], buffer, sizeof(buffer), &size); else
#endif
			read = xrtRudpMessageRead(m, buffer, sizeof(buffer), &size);
			CHECK(read == XRUDP_OK); *wake = true;
			for (size_t i = 0; i < size; ++i) CHECK(buffer[i] == pattern(1-side, (uint32_t)k, r, i));
			++net.Read[side][k];
#ifdef TEST_SESSION_MESSAGES
			if (k != 0) continue;
#endif
		} else if (ready == XRUDP_DONE) {
			CHECK(net.Read[side][k] == RECORDS); net.Eof[side][k] = true; return 0;
		} else CHECK(ready == XRUDP_BACKPRESSURE);
		xrudpmessagestate state; CHECK(xrtRudpMessageState(m, &state) == XRUDP_OK && state.Buffered <= 508);
		size_t needed = state.HeaderBytes < 4 ? 4 - state.HeaderBytes : state.Expected - state.Buffered;
		CHECK(needed != 0); if (needed > sizeof(buffer)) needed = sizeof(buffer);
		xrudpresult r = xrtRudpSessionStreamRead(net.Session[side], ids[k], buffer, needed, &size);
		if (r == XRUDP_BACKPRESSURE) return 0;
		if (r == XRUDP_DONE) { CHECK(xrtRudpMessageFinish(m) == XRUDP_OK); continue; }
		CHECK(r == XRUDP_OK && size > 0 && size <= needed);
		CHECK(xrtRudpMessagePush(m, buffer, size, &consumed) == XRUDP_OK && consumed == size);
		*wake = true; /* SessionRead already returned the exact byte credit. */
	}
	return 0;
}
static int scenario(uint32_t seed, bool stall)
{
	memset(&net, 0, sizeof(net)); tick = 0; randomState = seed; net.Epoch[0] = net.Epoch[1] = 1;
	xrudpcipher* cipher[2] = {NULL,NULL}; CHECK(cipher_pair(cipher) == 0);
	for (uint32_t side = 0; side < 2; ++side) {
		xrudpsessionconfig c; xrtRudpSessionConfigInit(&c); c.Role = side ? XRUDP_RESPONDER : XRUDP_INITIATOR;
		c.StreamCapacity = 2; c.Local.MaxStreams = c.Peer.MaxStreams = 2;
		c.ReceiveCapacity = 256; c.TransmitCapacity = 512;
		c.Local.StreamCredit = c.Peer.StreamCredit = 16;
#ifdef TEST_SESSION_MESSAGES
		c.Local.MaxMessageSize = c.Peer.MaxMessageSize = 508;
#endif
		c.Local.ConnectionCredit = c.Peer.ConnectionCredit = stall ? 64 : 16;
		c.Local.DatagramSize = c.Peer.DatagramSize = 512; c.PacketCapacity = 64; c.InitialRttUs = 10000;
		c.Local.MaxAckDelayUs = c.Peer.MaxAckDelayUs = 1000;
		c.Local.IdleTimeoutMs = c.Peer.IdleTimeoutMs = 60000; c.KeyUpdatePackets = 16;
		CHECK(xrtRudpSessionCreate(&c, cipher[side], 0, send_packet, (void*)(uintptr_t)side, &net.Session[side]) == XRUDP_OK);
		xrudpstreamconfig stream; xrtRudpStreamConfigInit(&stream); uint32_t id;
#ifdef TEST_SESSION_MESSAGES
		stream.Kind = XRUDP_STREAM_MESSAGES;
#endif
		CHECK(xrtRudpSessionStreamOpen(net.Session[side], &stream, &id) == XRUDP_OK && id == ids[side ? 2 : 1]);
		net.Active[side][0] = net.Active[side][side ? 2 : 1] = true;
		for (size_t k = 0; k < CHANNELS; ++k) CHECK(xrtRudpMessageCreate(508, &net.Decoder[side][k]) == XRUDP_OK);
	}
	CHECK(xrtMemDebugFailAfter(0)); uint64_t due[2] = {0,0}; uint32_t wake = 3; bool done = false, othersProgress = false;
	for (tick = 1; tick < 300000 && !done; ++tick) {
		uint64_t now = (uint64_t)tick * 1000;
		for (size_t i = 0; i < QUEUE; ++i) if (net.Packet[i].Used && net.Packet[i].Due <= tick) {
			message_packet p = net.Packet[i]; net.Packet[i].Used = false;
			if (random_next() % 11 == 0) {
				unsigned char bad[1200]; memcpy(bad, p.Data, p.Size); bad[p.Size - 1] ^= 1;
				CHECK(xrtRudpSessionInput(net.Session[p.Target], bad, p.Size, now) == XRUDP_AUTH_FAILED);
			}
			xrudpwirepacket header; CHECK(xrtRudpPacketDecode(p.Data, p.Size, &header) == XRUDP_OK);
			xrudpresult r = xrtRudpSessionInput(net.Session[p.Target], p.Data, p.Size, now);
			if (r == XRUDP_AUTH_FAILED && header.Header.KeyEpoch + 1 < net.Epoch[p.Target]) { ++net.RetiredKeys; continue; }
			CHECK(r == XRUDP_OK || r == XRUDP_DUPLICATE || r == XRUDP_TOO_OLD || r == XRUDP_BACKPRESSURE);
			if (header.Header.KeyEpoch > net.Epoch[p.Target]) net.Epoch[p.Target] = header.Header.KeyEpoch;
			if (r == XRUDP_BACKPRESSURE) ++net.Deferred;
			wake |= 1u << p.Target;
		}
		done = true;
		for (uint32_t side = 0; side < 2; ++side) {
			xrudpstreamevent event;
			while (xrtRudpSessionStreamNextEvent(net.Session[side], &event) == XRUDP_OK) if (event.Events & XRUDP_STREAM_OPENED) {
				size_t k; for (k = 0; k < CHANNELS && ids[k] != event.Id; ++k) {}
				CHECK(k < CHANNELS && !net.Active[side][k]); net.Active[side][k] = true;
			}
			for (size_t k = 0; k < CHANNELS; ++k) if (net.Active[side][k]) {
				if (net.Written[side][k] < RECORDS) {
					unsigned char encoded[512]; uint32_t r = net.Written[side][k]; size_t length = record_size(r), total = length + 4;
					for (size_t j = 0; j < 4; ++j) encoded[3-j] = (unsigned char)((uint32_t)length >> (j*8));
					for (size_t j = 0; j < length; ++j) encoded[j+4] = pattern(side, (uint32_t)k, r, j);
					size_t count; xrudpresult result;
#ifdef TEST_SESSION_MESSAGES
					if (k != 0) {
						result = xrtRudpSessionMessageWrite(net.Session[side], ids[k], encoded + 4, length); count = result == XRUDP_OK ? total : 0;
						size_t rejected = 99;
						CHECK(xrtRudpSessionStreamWrite(net.Session[side], ids[k], encoded, 1, &rejected) == XRUDP_ARGUMENT && rejected == 0);
						CHECK(xrtRudpSessionStreamRead(net.Session[side], ids[k], encoded, 1, &rejected) == XRUDP_ARGUMENT && rejected == 0);
					} else
#endif
					result = xrtRudpSessionStreamWrite(net.Session[side], ids[k], encoded + net.Offset[side][k], total - net.Offset[side][k], &count);
					CHECK(result == XRUDP_OK || result == XRUDP_BACKPRESSURE); net.Offset[side][k] += count; if (count) wake |= 1u << side;
					if (net.Offset[side][k] == total) {
						net.Offset[side][k] = 0; ++net.Written[side][k];
						if (net.Written[side][k] == RECORDS) CHECK(xrtRudpSessionStreamFinish(net.Session[side], ids[k]) == XRUDP_OK);
					}
				}
				bool localWake = false; CHECK(drain(side, k, stall && side == 0 && k == 1 && tick < 20000, &localWake) == 0);
				if (localWake) wake |= 1u << side;
				if (stall && side == 0 && k == 2 && net.Read[side][k] > 3 && tick < 20000) othersProgress = true;
				if (!net.Eof[side][k]) done = false;
				xrudpstreamstate state; CHECK(xrtRudpSessionStreamState(net.Session[side], ids[k], &state) == XRUDP_OK);
				if (!state.WriteDone || state.References) done = false;
			} else done = false;
			if ((wake & (1u << side)) || now >= due[side]) { CHECK(xrtRudpSessionPoll(net.Session[side], now, &due[side]) == XRUDP_OK); wake &= ~(1u << side); }
		}
	}
	CHECK(done && (!stall || othersProgress)); CHECK(net.Dropped && net.Duplicated && net.Blocked);
	CHECK(xrtMemDebugFailAfter(-1));
	for (uint32_t side = 0; side < 2; ++side) {
		for (size_t k = 0; k < CHANNELS; ++k) { CHECK(net.Read[side][k] == RECORDS); xrtRudpMessageDestroy(net.Decoder[side][k]); }
		xrtRudpSessionDestroy(net.Session[side]);
	}
	printf("xrudp staged messages seed=%u stall=%u: tick=%u connection=%u stream=16 max-record=512 drop=%u duplicate=%u pressure=%u deferred=%u\n", seed, stall ? 1u : 0u, tick, stall ? 64u : 16u, net.Dropped, net.Duplicated, net.Blocked, net.Deferred);
	return 0;
}
int main(void)
{
	xmemdebugsnapshot before, after; xrtClearError(); xrtMemDebugSnapshot(&before);
	CHECK(scenario(0x73641093, false) == 0); CHECK(scenario(0x6c8215db, true) == 0);
	xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
	CHECK(before.DoubleFreeCount == after.DoubleFreeCount && before.UseAfterFreeCount == after.UseAfterFreeCount);
	puts("xrudp staged messages: three impaired bidirectional streams exceed shared credit without deadlock or partial delivery; fixed memory balanced"); return 0;
}
