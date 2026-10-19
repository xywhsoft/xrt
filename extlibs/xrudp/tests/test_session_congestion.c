/* Public-session cases use real packet numbers and authenticated ACKs. Fixed
 * unit vectors additionally test the private bounded evidence classifier. */
#define main session_congestion_legacy_main
#include "test_session.c"
#undef main
#include "../src/internal/xrudp_loss.h"

typedef struct congestion_output {
	uint64_t Number[128], At[128], Now;
	size_t Count;
	size_t Rejected;
	bool Block;
} congestion_output;
static xrudpresult congestion_send(const void* data, size_t size, void* context)
{
	congestion_output* out = context; xrudpwirepacket packet;
	if (out->Block) { ++out->Rejected; return XRUDP_BACKPRESSURE; }
	if (out->Count == 128 || xrtRudpPacketDecode(data, size, &packet) != XRUDP_OK) return XRUDP_IO_ERROR;
	out->Number[out->Count] = packet.Header.PacketNumber; out->At[out->Count++] = out->Now;
	return XRUDP_OK;
}
static int congestion_write(xrudpsession* s, congestion_output* out, uint64_t now)
{
	size_t written, before = out->Count; uint64_t due;
	out->Now = now;
	CHECK(xrtRudpSessionWrite(s, "x", 1, &written) == XRUDP_OK && written == 1);
	CHECK(xrtRudpSessionPoll(s, now, &due) == XRUDP_OK);
	CHECK(out->Block ? out->Count == before : out->Count > before);
	return 0;
}
static int congestion_ack(xrudpsession* s, xrudpcipher* peer, uint64_t number, uint64_t now)
{
	unsigned char plain[64], body[24] = {0}; size_t size = 0;
	body[5] = 1; put64(body + 8, number); put64(body + 16, number);
	CHECK(append(plain, &size, XRUDP_FRAME_ACK, 0, body, sizeof(body)) == 0);
	CHECK(deliver_raw(s, peer, plain, size, now, XRUDP_OK) == 0);
	return 0;
}
static int congestion_period(unsigned int mode)
{
	xrudpcipher* cipher[2] = {NULL,NULL}; xrudpsession* s; xrudpsessionconfig config;
	congestion_output out; memset(&out, 0, sizeof(out));
	CHECK(raw_pair(cipher) == 0); configure(&config, 65536, 1200); config.KeyUpdatePackets = 4096;
	CHECK(xrtRudpSessionCreate(&config, cipher[0], 0, congestion_send, &out, &s) == XRUDP_OK);
	CHECK(congestion_write(s, &out, 1000) == 0);
	if (mode != 2) CHECK(congestion_ack(s, cipher[1], out.Number[out.Count - 1], 11000) == 0);
	xrudpsessionstate before; CHECK(xrtRudpSessionState(s, &before) == XRUDP_OK && before.Rtt.HasSample == (mode != 2));
	size_t first = out.Count;
	out.Block = mode == 4;
	CHECK(congestion_write(s, &out, 20000) == 0);
	CHECK(congestion_write(s, &out, mode == 1 ? 30000 : 100000) == 0);
	if (mode == 3) CHECK(congestion_ack(s, cipher[1], out.Number[out.Count - 1], 110000) == 0);
	CHECK(congestion_write(s, &out, mode == 1 ? 40000 : 200000) == 0);
	out.Block = false;
	CHECK(congestion_write(s, &out, mode == 1 ? 50000 : 250000) == 0);
	if (mode != 5) CHECK(congestion_ack(s, cipher[1], out.Number[out.Count - 1], mode == 1 ? 60000 : 260000) == 0);
	xrudpsessionstate after; CHECK(xrtRudpSessionState(s, &after) == XRUDP_OK);
	uint64_t base; CHECK(xrtRudpRttPto(&after.Rtt, 0, &base) == XRUDP_OK);
	printf("recovery period %u: window=%llu minimum=%u pto=%llu lost=%llu packets=%zu proven=%llu\n", mode,
		(unsigned long long)after.Congestion.Window, XRUDP_CC_MINIMUM_WINDOW,
		(unsigned long long)base, (unsigned long long)after.LostPackets, out.Count, (unsigned long long)after.PersistentCongestion);
	if (mode == 0) CHECK(out.Count > first + 2 && out.At[out.Count - 2] - out.At[first] > 3 * base);
	if (mode == 4) CHECK(out.Rejected && after.OutputBackpressure && out.Number[out.Count - 1] > out.Count + 1);
	if (mode == 5) CHECK(after.LostPackets == 0 && after.ProbeTimeouts > 0);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(cipher[1]);
	CHECK(after.PersistentCongestion == (mode == 0 ? 1u : 0u));
	if (mode == 0) CHECK(after.Congestion.Window == XRUDP_CC_MINIMUM_WINDOW);
	return 0;
}

static int proof_done(xrudp__lossproof* p, uint64_t number, uint64_t at, uint32_t flags, uint32_t kind, bool network)
{
	xrudpsentevent event; memset(&event, 0, sizeof(event));
	event.PacketNumber = number; event.SentAtUs = at; event.Flags = flags; event.Kind = kind;
	CHECK(xrudp__loss_finish(p, &event, network)); return 0;
}
static int proof_one(xrudp__lossproof* p, uint64_t number, uint64_t at, uint32_t flags,
	bool sampled, uint32_t kind, bool network, uint64_t duration, uint32_t expected)
{
	CHECK(xrudp__loss_record(p, number, at, flags, sampled));
	CHECK(proof_done(p, number, at, flags, kind, network) == 0);
	CHECK(xrudp__loss_retire(p, duration) == expected && p->Count == 0); return 0;
}
static void proof_ack(xrudp__lossproof* p, uint64_t number)
{
	xrudpack ack; memset(&ack, 0, sizeof(ack)); ack.Count = 1;
	ack.Ranges[0].First = ack.Ranges[0].Last = number; xrudp__loss_ack(p, &ack);
}
static int proof_vectors(void)
{
	xrudp__lossrecord records[8]; xrudp__lossproof p;
	const uint32_t e = XRUDP_SENT_ACK_ELICITING, mtu = XRUDP_SENT_MTU | XRUDP_SENT_PADDING | XRUDP_SENT_ACK_ELICITING;
#define PROOF_RESET() do { memset(&p, 0, sizeof(p)); memset(records, 0, sizeof(records)); p.Records = records; p.Capacity = 8; } while (0)
	PROOF_RESET();
	CHECK(proof_one(&p, 1, 10, e, true, XRUDP_SENT_LOST, true, 10, 0) == 0);
	CHECK(proof_one(&p, 2, 20, e, true, XRUDP_SENT_LOST, true, 10, 0) == 0); /* strict > */
	CHECK(proof_one(&p, 3, 21, e, true, XRUDP_SENT_LOST, true, 10, 1) == 0);
	CHECK(proof_one(&p, 4, 1000, e, true, XRUDP_SENT_LOST, true, 10, 0) == 0); /* once */
	proof_ack(&p, 2); CHECK(p.FirstNumber == 0);
	CHECK(proof_one(&p, 5, 1010, e, true, XRUDP_SENT_LOST, true, 10, 0) == 0);
	CHECK(proof_one(&p, 6, 1040, e, true, XRUDP_SENT_LOST, true, 10, 1) == 0);
	PROOF_RESET();
	for (uint64_t n = 1; n <= 3; ++n) CHECK(xrudp__loss_record(&p, n, n == 3 ? 100 : n * 10, e, true));
	CHECK(proof_done(&p, 3, 100, e, XRUDP_SENT_LOST, true) == 0);
	CHECK(xrudp__loss_retire(&p, 30) == 0 && p.Count == 3);
	proof_ack(&p, 3); /* Late ACK while a prior record is still pending. */
	CHECK(proof_done(&p, 1, 10, e, XRUDP_SENT_LOST, true) == 0 && proof_done(&p, 2, 20, e, XRUDP_SENT_LOST, true) == 0);
	CHECK(xrudp__loss_retire(&p, 30) == 0 && p.Count == 0 && p.FirstNumber == 0);
	for (unsigned int type = 0; type < 2; ++type) {
		PROOF_RESET(); uint32_t flags = type ? mtu : XRUDP_SENT_ACK_ONLY;
		CHECK(xrudp__loss_record(&p, 1, 0, flags, true)); /* Not yet ACKed/expired. */
		CHECK(xrudp__loss_record(&p, 2, 10, e, true) && xrudp__loss_record(&p, 3, 100, e, true));
		CHECK(proof_done(&p, 2, 10, e, XRUDP_SENT_LOST, true) == 0 && proof_done(&p, 3, 100, e, XRUDP_SENT_LOST, true) == 0);
		CHECK(xrudp__loss_retire(&p, 30) == 1 && p.Count == 0); /* No ACK-only head blocking. */
		CHECK(proof_done(&p, 1, 0, flags, XRUDP_SENT_ACKED, false) == 0); /* Ownership finishes later. */
	}
	for (unsigned int late = 0; late < 2; ++late) {
		PROOF_RESET();
		CHECK(proof_one(&p, 1, 10, e, true, XRUDP_SENT_LOST, true, 30, 0) == 0);
		CHECK(proof_one(&p, 2, 20, XRUDP_SENT_ACK_ONLY, true, XRUDP_SENT_EXPIRED, false, 30, 0) == 0);
		if (late) proof_ack(&p, 2); /* Retired ACK-only remains a real barrier. */
		CHECK(proof_one(&p, 3, 100, e, true, XRUDP_SENT_LOST, true, 30, late ? 0 : 1) == 0);
	}
	PROOF_RESET();
	CHECK(proof_one(&p, 1, 0, mtu, true, XRUDP_SENT_LOST, true, 10, 0) == 0);
	CHECK(proof_one(&p, 2, 100, mtu, true, XRUDP_SENT_LOST, true, 10, 0) == 0 && p.FirstNumber == 0);
	for (unsigned int reason = 0; reason < 3; ++reason) {
		PROOF_RESET();
		CHECK(proof_one(&p, 1, 10, e, true, XRUDP_SENT_LOST, true, 10, 0) == 0);
		CHECK(proof_one(&p, 2, 20, reason == 2 ? mtu : e, reason != 1,
			reason == 2 ? XRUDP_SENT_ACKED : XRUDP_SENT_LOST, reason != 0, 10, 0) == 0);
		CHECK(proof_one(&p, 3, 100, e, true, XRUDP_SENT_LOST, true, 10, 0) == 0);
	}
	PROOF_RESET();
	CHECK(proof_one(&p, 1, 1, e, true, XRUDP_SENT_LOST, true, 200, 0) == 0);
	CHECK(proof_one(&p, 2, 101, e, true, XRUDP_SENT_LOST, true, 200, 0) == 0);
	CHECK(xrudp__loss_retire(&p, 100) == 0 && xrudp__loss_retire(&p, 99) == 1); /* New RTT threshold. */
	PROOF_RESET();
	CHECK(proof_one(&p, 1, 0, e, true, XRUDP_SENT_LOST, true, UINT64_MAX, 0) == 0);
	CHECK(proof_one(&p, 2, UINT64_MAX - 1, e, true, XRUDP_SENT_LOST, true, UINT64_MAX, 0) == 0);
	PROOF_RESET(); uint32_t periods = 0;
	for (uint64_t n = 1; n <= 7000; ++n) {
		CHECK(xrudp__loss_record(&p, n * 2, n * 5, e, true)); /* Unsent gaps do not add evidence. */
		CHECK(proof_done(&p, n * 2, n * 5, e, n % 7 == 0 ? XRUDP_SENT_ACKED : XRUDP_SENT_LOST, true) == 0);
		periods += xrudp__loss_retire(&p, 5); CHECK(p.Count == 0 && p.Head < p.Capacity);
	}
	CHECK(periods == 1000);
#undef PROOF_RESET
	puts("xrudp loss evidence: exact threshold, late ACKs, pending holes, local/MTU/no-RTT barriers, overflow and 7000 ring retirements passed");
	return 0;
}
int main(void)
{
	CHECK(proof_vectors() == 0);
	for (unsigned int mode = 0; mode < 6; ++mode) CHECK(congestion_period(mode) == 0);
	CHECK(session_congestion_legacy_main() == 0);
	puts("xrudp session persistent congestion passed"); return 0;
}
