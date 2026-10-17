#include <xrudp/session.h>
#include <xrt/memory.h>
#include <string.h>

typedef struct xrudp__sessionref {
	uint64_t Number, At, Offset, ConnectionGrant, StreamGrant;
	uint32_t Size, Flags;
	bool Used, Fin;
} xrudp__sessionref;

struct xrudpsession {
	xrudpsessionconfig Config;
	xrudpcipher* Cipher;
	xrudpsentwindow* Sent;
	xrudpreceive* Rx;
	xrudptransmit* Tx;
	xrudpcongestion Cc;
	xrudprtt Rtt;
	xrudpackset Ack;
	xrudpsessionsendproc Send;
	void* Context;
	xrudpresult Reason;
	bool Busy, Work, WorkStartPending, AckWanted, ConfirmWanted;
	size_t Allocation;
	uint32_t Slots, PtoCount, Probes, ElicitingSinceAck;
	uint64_t Now, Activity, Progress, IdleUs, PtoAt, AckAt, ControlAt;
	uint64_t GrantRetry, ConnectionLimit, ConnectionReceived, ConnectionSent, PeerLimit;
	uint64_t ConnectionAcked, StreamAcked, PreviousKeyUntil;
	uint64_t Eliciting[XRUDP_REPLAY_BITS];
	uint64_t ReceivedPackets, SentPackets, LostPackets, ProbeTimeouts, KeyUpdates, InvalidPackets, OutputBackpressure;
	xrudpsentevent* Events;
	xrudp__sessionref Ref[];
};

static uint64_t xrudp__session_add(uint64_t a, uint64_t b) { return a > UINT64_MAX - b ? UINT64_MAX : a + b; }
static uint32_t xrudp__session_get32(const unsigned char* p)
{
	uint32_t v = 0; for (size_t i = 0; i < 4; ++i) v = (v << 8) | p[i]; return v;
}
static uint64_t xrudp__session_get64(const unsigned char* p)
{
	uint64_t v = 0; for (size_t i = 0; i < 8; ++i) v = (v << 8) | p[i]; return v;
}
static void xrudp__session_put32(unsigned char* p, uint32_t v)
{
	for (size_t i = 0; i < 4; ++i) p[3 - i] = (unsigned char)(v >> (i * 8));
}
static void xrudp__session_put64(unsigned char* p, uint64_t v)
{
	for (size_t i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(v >> (i * 8));
}
static bool xrudp__session_good(xrudpresult r) { return r == XRUDP_OK || r == XRUDP_DUPLICATE; }
static bool xrudp__session_terminal(xrudpresult r)
{
	return r == XRUDP_CANCELLED || r == XRUDP_TIMEOUT || r == XRUDP_CLOSED || r == XRUDP_MALFORMED ||
		r == XRUDP_IO_ERROR || r == XRUDP_NO_MEMORY || r == XRUDP_LIMIT;
}

void xrtRudpSessionConfigInit(xrudpsessionconfig* c)
{
	if (c == NULL) return;
	memset(c, 0, sizeof(*c)); xrtRudpParametersDefault(&c->Local); xrtRudpParametersDefault(&c->Peer);
	c->Local.MaxStreams = 1; c->ReceiveCapacity = c->TransmitCapacity = 65536;
	c->PacketCapacity = 128; c->ControlPerSecond = 128; c->ProgressTimeoutUs = 30000000;
	c->KeyUpdatePackets = 1048576;
}
static xrudpresult xrudp__session_sizes(const xrudpsessionconfig* c, size_t* own, size_t* total)
{
	unsigned char params[32]; size_t rx, tx, sent; xrudpcongestion cc; xrudprtt rtt;
	if (c == NULL || c->PacketCapacity == 0 || c->PacketCapacity > XRUDP_SESSION_PACKETS_MAX ||
		c->ControlPerSecond < 40 || c->ControlPerSecond > 4096 || c->Local.MaxStreams != 1 ||
		c->ProgressTimeoutUs < 1000000 || c->ProgressTimeoutUs > 600000000 ||
		c->KeyUpdatePackets < 8 || c->KeyUpdatePackets > XRUDP_KEY_PACKETS_MAX ||
		xrtRudpParametersEncode(&c->Local, params, sizeof(params)) != XRUDP_OK ||
		xrtRudpParametersEncode(&c->Peer, params, sizeof(params)) != XRUDP_OK ||
		xrtRudpReceiveAllocationSize(c->ReceiveCapacity, &rx) != XRUDP_OK ||
		xrtRudpTransmitAllocationSize(c->TransmitCapacity, &tx) != XRUDP_OK ||
		c->Local.StreamCredit > (c->ReceiveCapacity ? c->ReceiveCapacity : XRUDP_RECEIVE_CAPACITY_DEFAULT) ||
		xrtRudpSentAllocationSize(c->PacketCapacity, &sent) != XRUDP_OK ||
		xrtRudpCongestionInit(&cc, c->MaximumWindow) != XRUDP_OK ||
		xrtRudpRttInit(&rtt, c->InitialRttUs, c->Peer.MaxAckDelayUs) != XRUDP_OK) return XRUDP_ARGUMENT;
	*own = sizeof(xrudpsession) + (c->PacketCapacity + XRUDP_SENT_PROBE_RESERVE) *
		(sizeof(xrudp__sessionref) + sizeof(xrudpsentevent));
	*total = *own + rx + tx + sent + xrtRudpCipherAllocationSize();
	return XRUDP_OK;
}
xrudpresult xrtRudpSessionAllocationSize(const xrudpsessionconfig* c, size_t* output)
{
	size_t own, total; xrudpresult r;
	if (output == NULL) return XRUDP_ARGUMENT;
	r = xrudp__session_sizes(c, &own, &total); if (r == XRUDP_OK) *output = total; return r;
}
xrudpresult xrtRudpSessionCreate(const xrudpsessionconfig* c, xrudpcipher* cipher,
	uint64_t now, xrudpsessionsendproc send, void* context, xrudpsession** output)
{
	size_t own, total; xrudpcipherstate state; xrudpresult result;
	if (cipher == NULL || send == NULL || output == NULL || xrudp__session_sizes(c, &own, &total) != XRUDP_OK ||
		xrtRudpCipherState(cipher, &state) != XRUDP_OK || state.NextPacketNumber != 2 ||
		state.LargestReceivedPacket != 1 || state.SendEpoch != 1 || state.ReceiveEpoch != 1 || state.SendKeyPending)
		return XRUDP_ARGUMENT;
	xrudpsession* s = (xrudpsession*)xrtMalloc(own);
	if (s == NULL) return XRUDP_NO_MEMORY;
	memset(s, 0, own); s->Allocation = own; s->Slots = c->PacketCapacity + XRUDP_SENT_PROBE_RESERVE;
	s->Events = (xrudpsentevent*)(s->Ref + s->Slots); s->Config = *c; s->Send = send; s->Context = context;
	s->Now = s->Activity = s->Progress = now; s->AckAt = UINT64_MAX; s->GrantRetry = now;
	s->ConnectionLimit = s->ConnectionAcked = c->Local.ConnectionCredit; s->StreamAcked = c->Local.StreamCredit;
	s->PeerLimit = c->Peer.ConnectionCredit;
	s->IdleUs = (uint64_t)(c->Local.IdleTimeoutMs < c->Peer.IdleTimeoutMs ? c->Local.IdleTimeoutMs : c->Peer.IdleTimeoutMs) * 1000;
	result = xrtRudpSentCreateAfter(c->PacketCapacity, 1, &s->Sent);
	if (result == XRUDP_OK) result = xrtRudpReceiveCreate(c->ReceiveCapacity, c->Local.StreamCredit, &s->Rx);
	if (result == XRUDP_OK) result = xrtRudpTransmitCreate(c->TransmitCapacity, c->Peer.StreamCredit, &s->Tx);
	if (result != XRUDP_OK) {
		xrtRudpTransmitDestroy(s->Tx); xrtRudpReceiveDestroy(s->Rx); xrtRudpSentDestroy(s->Sent);
		xrtSecureZero(s, own); xrtFree(s); return result;
	}
	(void)xrtRudpCongestionInit(&s->Cc, c->MaximumWindow); s->Cc.LargestSent = 1;
	(void)xrtRudpRttInit(&s->Rtt, c->InitialRttUs, c->Peer.MaxAckDelayUs);
	xrtRudpAckSetInit(&s->Ack); (void)xrtRudpAckSetRecord(&s->Ack, 1, now);
	s->Cipher = cipher; *output = s; (void)total; return XRUDP_OK;
}
static bool xrudp__session_work(xrudpsession* s)
{
	xrudptransmitstate tx; xrudpreceivestate rx;
	(void)xrtRudpTransmitState(s->Tx, &tx); (void)xrtRudpReceiveState(s->Rx, &rx);
	return tx.PendingBytes != 0 || (tx.FinQueued && !tx.FinAcknowledged) ||
		s->ConnectionLimit > s->ConnectionAcked || rx.Limit > s->StreamAcked;
}
static void xrudp__session_syncwork(xrudpsession* s)
{
	bool work = xrudp__session_work(s);
	if (work && !s->Work) s->Progress = s->Now;
	s->Work = work;
}
static xrudpresult xrudp__session_abort(xrudpsession* s, xrudpresult reason)
{
	if (s->Reason != XRUDP_OK) return XRUDP_DONE;
	xrudptransmitchange tx; xrudpreceivestate rx; xrudpreceivechange reset; size_t count;
	(void)xrtRudpTransmitCancel(s->Tx, &tx);
	(void)xrtRudpReceiveState(s->Rx, &rx); (void)xrtRudpReceiveReset(s->Rx, rx.HighestOffset, (uint32_t)reason, &reset);
	(void)xrtRudpSentAbort(s->Sent, s->Events, s->Slots, &count);
	(void)xrtRudpCongestionApply(&s->Cc, s->Events, count, s->Now, true);
	memset(s->Ref, 0, s->Slots * sizeof(*s->Ref)); xrtRudpCipherDestroy(s->Cipher); s->Cipher = NULL;
	s->AckWanted = s->ConfirmWanted = s->Work = s->WorkStartPending = false; s->Probes = 0; s->PtoAt = 0; s->Reason = reason;
	return XRUDP_OK;
}
xrudpresult xrtRudpSessionAbort(xrudpsession* s, xrudpresult reason)
{
	if (s == NULL || s->Busy || !xrudp__session_terminal(reason)) return XRUDP_ARGUMENT;
	return xrudp__session_abort(s, reason);
}
void xrtRudpSessionDestroy(xrudpsession* s)
{
	if (s == NULL) return;
	(void)xrudp__session_abort(s, XRUDP_CANCELLED); xrtRudpCipherDestroy(s->Cipher);
	xrtRudpSentDestroy(s->Sent); xrtRudpTransmitDestroy(s->Tx); xrtRudpReceiveDestroy(s->Rx);
	size_t size = s->Allocation; xrtSecureZero(s, size); xrtFree(s);
}
xrudpresult xrtRudpSessionState(const xrudpsession* s, xrudpsessionstate* output)
{
	if (s == NULL || output == NULL || s->Busy) return XRUDP_ARGUMENT;
	xrudpsessionstate state; memset(&state, 0, sizeof(state)); state.Reason = s->Reason;
	(void)xrtRudpReceiveState(s->Rx, &state.Receive); (void)xrtRudpTransmitState(s->Tx, &state.Transmit);
	(void)xrtRudpSentState(s->Sent, &state.Sent); state.Congestion = s->Cc; state.Rtt = s->Rtt;
	state.ReceivedConnection = s->ConnectionReceived; state.ReceiveLimit = s->ConnectionLimit;
	state.SentConnection = s->ConnectionSent; state.PeerLimit = s->PeerLimit;
	state.LastNowUs = s->Now; state.LastActivityUs = s->Activity; state.ProgressAtUs = s->Progress;
	state.ReceivedPackets = s->ReceivedPackets; state.SentPackets = s->SentPackets; state.LostPackets = s->LostPackets;
	state.ProbeTimeouts = s->ProbeTimeouts; state.KeyUpdates = s->KeyUpdates; state.InvalidPackets = s->InvalidPackets;
	state.OutputBackpressure = s->OutputBackpressure; state.PtoBackoff = s->PtoCount;
	state.OutstandingProbes = s->Probes; state.AckPending = s->AckWanted;
	state.ReadReady = xrtRudpReceiveReady(s->Rx); *output = state; return XRUDP_OK;
}
xrudpresult xrtRudpSessionWrite(xrudpsession* s, const void* data, size_t size, size_t* written)
{
	if (written != NULL) *written = 0;
	if (s == NULL || s->Busy || written == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	bool prior = s->Work;
	xrudpresult result = xrtRudpTransmitWrite(s->Tx, data, size, written); xrudp__session_syncwork(s);
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
xrudpresult xrtRudpSessionFinish(xrudpsession* s)
{
	if (s == NULL || s->Busy) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	bool prior = s->Work;
	xrudpresult result = xrtRudpTransmitFinish(s->Tx); xrudp__session_syncwork(s);
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
static void xrudp__session_consumed(xrudpsession* s, uint64_t amount)
{
	xrudpreceivestate rx; (void)xrtRudpReceiveState(s->Rx, &rx);
	if (!rx.Reset) (void)xrtRudpReceiveExtend(s->Rx, xrudp__session_add(rx.ReadOffset, s->Config.Local.StreamCredit));
	s->ConnectionLimit = xrudp__session_add(s->ConnectionLimit, amount); s->GrantRetry = s->Now;
	xrudp__session_syncwork(s);
}
xrudpresult xrtRudpSessionRead(xrudpsession* s, void* output, size_t capacity, size_t* read)
{
	if (read != NULL) *read = 0;
	if (s == NULL || s->Busy || output == NULL || capacity == 0 || read == NULL) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	bool prior = s->Work;
	xrudpresult result = xrtRudpReceiveRead(s->Rx, output, capacity, read);
	if (*read != 0) xrudp__session_consumed(s, *read);
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
static void xrudp__session_clock(xrudpsession* s, uint64_t now)
{
	s->Now = now;
	/* Clock-free application operations cannot use a stale idle timestamp to
	 * start their deadline. Anchor only this idle->work transition at the next
	 * driver observation; further writes/reads cannot continually rearm it. */
	if (s->WorkStartPending) { s->Progress = now; s->WorkStartPending = false; }
}
static bool xrudp__session_dead(xrudpsession* s, uint64_t now)
{
	return now == UINT64_MAX || now - s->Activity >= s->IdleUs || (s->Work && now - s->Progress >= s->Config.ProgressTimeoutUs);
}
static bool xrudp__session_limited(xrudpsession* s)
{
	xrudptransmitstate tx; (void)xrtRudpTransmitState(s->Tx, &tx);
	return tx.QueuedEnd - tx.BaseOffset < s->Cc.Window || tx.HighestSent >= tx.PeerLimit || s->ConnectionSent >= s->PeerLimit;
}
static xrudpresult xrudp__session_feedback(xrudpsession* s, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* event = &s->Events[i];
		if (event->UserTag == 0 || event->UserTag > s->Slots) return XRUDP_IO_ERROR;
		xrudp__sessionref* ref = &s->Ref[event->UserTag - 1];
		if (!ref->Used || ref->Number != event->PacketNumber) return XRUDP_IO_ERROR;
		if (ref->Size || ref->Fin) {
			xrudptransmitchange change;
			xrudpresult result = xrtRudpTransmitResolve(s->Tx, ref->Offset, ref->Size, ref->Fin, event->Kind == XRUDP_SENT_ACKED, &change);
			if (!xrudp__session_good(result)) return XRUDP_IO_ERROR;
			if (change.NewlyAckedBytes || change.NewFinAcknowledged) s->Progress = s->Now;
		}
		if (event->Kind == XRUDP_SENT_ACKED) {
			if (ref->ConnectionGrant > s->ConnectionAcked) { s->ConnectionAcked = ref->ConnectionGrant; s->Progress = s->Now; }
			if (ref->StreamGrant > s->StreamAcked) { s->StreamAcked = ref->StreamGrant; s->Progress = s->Now; }
		} else if (event->Kind == XRUDP_SENT_LOST) {
			++s->LostPackets;
			if (ref->ConnectionGrant || ref->StreamGrant) s->GrantRetry = s->Now;
		}
		memset(ref, 0, sizeof(*ref));
	}
	return XRUDP_OK;
}
static xrudpresult xrudp__session_losses(xrudpsession* s, size_t before)
{
	uint64_t delay, age; size_t count;
	(void)xrtRudpRttLossDelay(&s->Rtt, &delay); (void)xrtRudpRttPto(&s->Rtt, 0, &age);
	age = xrudp__session_add(age, xrudp__session_add(age, age)); if (age < 1000000) age = 1000000;
	xrudpresult result = xrtRudpSentDetectLoss(s->Sent, delay, age, s->Now, s->Events + before, s->Slots - before, &count);
	if (result != XRUDP_OK && result != XRUDP_DONE) return XRUDP_IO_ERROR;
	result = xrtRudpCongestionApply(&s->Cc, s->Events, before + count, s->Now, xrudp__session_limited(s));
	return result == XRUDP_OK ? xrudp__session_feedback(s, before + count) : XRUDP_IO_ERROR;
}
static void xrudp__session_ackwant(xrudpsession* s, bool immediate)
{
	uint64_t due = immediate ? s->Now : xrudp__session_add(s->Now, s->Config.Local.MaxAckDelayUs);
	if (!s->AckWanted || due < s->AckAt) s->AckAt = due;
	s->AckWanted = true;
}
static void xrudp__session_pto_sync(xrudpsession* s, bool progress)
{
	xrudpsentstate sent; uint64_t interval; (void)xrtRudpSentState(s->Sent, &sent);
	if (progress) { s->PtoCount = 0; s->Probes = 0; s->PtoAt = 0; }
	if (sent.BytesInFlight == 0) { s->PtoAt = 0; s->Probes = 0; }
	else if (s->PtoAt == 0) {
		(void)xrtRudpRttPto(&s->Rtt, s->PtoCount, &interval); s->PtoAt = xrudp__session_add(s->Now, interval);
	}
}

xrudpresult xrtRudpSessionInput(xrudpsession* s, const void* packet, size_t size, uint64_t now)
{
	if (s == NULL || s->Busy || packet == NULL || now < s->Now) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	unsigned char plain[XRUDP_PACKET_MAX]; size_t length, cursor = 0;
	xrudpcipherstate previous, current; xrudpwirepacket parsed;
	(void)xrtRudpCipherState(s->Cipher, &previous);
	xrudpresult result = xrtRudpCipherOpen(s->Cipher, packet, size, plain, sizeof(plain), &length);
	if (result == XRUDP_AUTH_FAILED) { ++s->InvalidPackets; return result; }
	if (result == XRUDP_DUPLICATE || result == XRUDP_TOO_OLD) {
		if (result == XRUDP_DUPLICATE && xrtRudpPacketDecode(packet, size, &parsed) == XRUDP_OK &&
			xrtRudpAckSetContains(&s->Ack, parsed.Header.PacketNumber) &&
			s->Eliciting[parsed.Header.PacketNumber % XRUDP_REPLAY_BITS] == parsed.Header.PacketNumber)
			xrudp__session_ackwant(s, true);
		return result;
	}
	if (result != XRUDP_OK) {
		(void)xrudp__session_abort(s, result == XRUDP_MALFORMED ? XRUDP_MALFORMED : XRUDP_IO_ERROR); return s->Reason;
	}
	(void)xrtRudpPacketDecode(packet, size, &parsed);
	/* A completed proof already owns PN 1. Negotiated datagram limits apply only
	 * after authentication; an oversized forged UDP packet cannot close us. */
	if (parsed.Header.PacketNumber < 2 || size > s->Config.Local.DatagramSize) goto malformed;
	xrudpwireframe frames[XRUDP_FRAMES_MAX]; size_t offsets[XRUDP_FRAMES_MAX], ends[XRUDP_FRAMES_MAX], count = 0;
	xrudpreceiveframe streams[XRUDP_FRAMES_MAX]; size_t streamCount = 0;
	xrudpreceivestate rx; xrudptransmitstate tx;
	(void)xrtRudpReceiveState(s->Rx, &rx); (void)xrtRudpTransmitState(s->Tx, &tx);
	uint64_t peerLimit = s->PeerLimit, streamLimit = tx.PeerLimit, highest = rx.HighestOffset, final = rx.FinalOffset;
	uint64_t resetFinal = 0; uint32_t resetReason = 0;
	bool eliciting = false, hasFinal = rx.HasFinal != 0, hasReset = false, closing = false;
	while (cursor < length) {
		size_t start = cursor; xrudpwireframe* f = &frames[count];
		if (count == XRUDP_FRAMES_MAX || xrtRudpFrameNext(plain, length, &cursor, f) != XRUDP_OK) goto malformed;
		offsets[count] = start; ends[count] = cursor; ++count;
		if (f->Type != XRUDP_FRAME_ACK && f->Type != XRUDP_FRAME_PADDING && f->Type != XRUDP_FRAME_KEY_CONFIRM) eliciting = true;
		switch (f->Type) {
		case XRUDP_FRAME_PADDING: case XRUDP_FRAME_PING: case XRUDP_FRAME_KEY_CONFIRM: break;
		case XRUDP_FRAME_ACK: {
			xrudpack ack; xrudpackchange change; size_t events;
			if (xrtRudpAckDecode(plain + start, cursor - start, &ack) != XRUDP_OK ||
				xrtRudpSentCheckAcknowledge(s->Sent, &ack, now, &events, &change) != XRUDP_OK) goto malformed;
			break;
		}
		case XRUDP_FRAME_STREAM: {
			if (xrudp__session_get32(f->Data) != 1) goto malformed;
			xrudpreceiveframe* sf = &streams[streamCount++]; sf->Receive = s->Rx;
			sf->Offset = xrudp__session_get64(f->Data + 4); sf->Size = f->Size - 12;
			sf->Data = f->Data + 12; sf->Fin = (f->Flags & XRUDP_STREAM_FIN) != 0;
			uint64_t end = sf->Offset + sf->Size; /* Structure validator proved no overflow. */
			if (end > highest) highest = end;
			if (sf->Fin) { if (hasFinal && end != final) goto malformed; hasFinal = true; final = end; }
			break;
		}
		case XRUDP_FRAME_RESET: {
			uint64_t value = xrudp__session_get64(f->Data + 4); uint32_t reason = xrudp__session_get32(f->Data + 12);
			if (xrudp__session_get32(f->Data) != 1 || (hasReset && (resetFinal != value || resetReason != reason))) goto malformed;
			hasReset = true; resetFinal = value; resetReason = reason; break;
		}
		case XRUDP_FRAME_MAX_DATA: {
			uint64_t value = xrudp__session_get64(f->Data); if (value > peerLimit) peerLimit = value; break;
		}
		case XRUDP_FRAME_MAX_STREAM_DATA: {
			if (xrudp__session_get32(f->Data) != 1) goto malformed;
			uint64_t value = xrudp__session_get64(f->Data + 4); if (value > streamLimit) streamLimit = value; break;
		}
		case XRUDP_FRAME_MAX_STREAMS: break; /* Additional local streams are not opened by this profile. */
		case XRUDP_FRAME_CLOSE: closing = true; break;
		default: goto malformed; /* Not enabled in the default-byte-stream profile. */
		}
	}
	if (hasFinal && highest > final) goto malformed;
	if (hasReset) {
		if (resetFinal < highest || resetFinal > rx.Limit || (hasFinal && resetFinal != final) ||
			(rx.Reset && resetReason != rx.ResetReason)) goto malformed;
		highest = resetFinal;
	}
	if (highest - rx.HighestOffset > s->ConnectionLimit - s->ConnectionReceived) goto malformed;
	if (streamCount) {
		xrudpreceivebatchchange change;
		result = xrtRudpReceiveCheckBatch(streams, streamCount, s->ConnectionLimit - s->ConnectionReceived, &change);
		if (!xrudp__session_good(result)) goto malformed;
	}
	/* Commit is allocation-free. All peer-dependent rejection is above. No
	 * callbacks/application-visible data is delivered until the entire commit. */
	if (streamCount) {
		xrudpreceivebatchchange change;
		if (!xrudp__session_good(xrtRudpReceivePushBatch(streams, streamCount, s->ConnectionLimit - s->ConnectionReceived, &change))) goto internal;
		if (change.NewBytes || change.NewFinals) s->Progress = now;
	}
	s->ConnectionReceived += highest - rx.HighestOffset;
	if (hasReset) {
		xrudpreceivechange change;
		if (!xrudp__session_good(xrtRudpReceiveReset(s->Rx, resetFinal, resetReason, &change))) goto internal;
		if (change.ConsumedDelta) { s->Progress = now; xrudp__session_consumed(s, change.ConsumedDelta); }
	}
	if (peerLimit > s->PeerLimit || streamLimit > tx.PeerLimit) s->Progress = now;
	s->PeerLimit = peerLimit; (void)xrtRudpTransmitSetLimit(s->Tx, streamLimit);
	size_t eventCount = 0; bool progress = false; xrudpackchange sample; memset(&sample, 0, sizeof(sample));
	for (size_t i = 0; i < count; ++i) if (frames[i].Type == XRUDP_FRAME_ACK) {
		xrudpack ack; xrudpackchange change; size_t events;
		(void)xrtRudpAckDecode(plain + offsets[i], ends[i] - offsets[i], &ack);
		if (xrtRudpSentAcknowledge(s->Sent, &ack, now, s->Events + eventCount, s->Slots - eventCount, &events, &change) != XRUDP_OK) goto internal;
		eventCount += events; if (change.NewlyAckedEliciting) progress = true;
		if (change.RttSampleValid && (!sample.RttSampleValid || change.LargestAcked > sample.LargestAcked)) sample = change;
	}
	if (sample.RttSampleValid) (void)xrtRudpRttSample(&s->Rtt, sample.RttSampleUs, sample.PeerAckDelayUs);
	if (xrudp__session_losses(s, eventCount) != XRUDP_OK) goto internal;
	(void)xrtRudpCipherState(s->Cipher, &current);
	if (current.ReceiveEpoch != previous.ReceiveEpoch) {
		uint64_t grace; (void)xrtRudpRttPto(&s->Rtt, 0, &grace);
		grace = xrudp__session_add(grace, xrudp__session_add(grace, grace));
		if (grace < 1000000) grace = 1000000;
		if (grace > 60000000) grace = 60000000;
		s->PreviousKeyUntil = xrudp__session_add(now, grace); s->ConfirmWanted = true;
	}
	result = xrtRudpAckSetRecord(&s->Ack, parsed.Header.PacketNumber, now);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE && result != XRUDP_TOO_OLD) goto internal;
	if (eliciting) {
		s->Eliciting[parsed.Header.PacketNumber % XRUDP_REPLAY_BITS] = parsed.Header.PacketNumber;
		if (s->ElicitingSinceAck < 2) ++s->ElicitingSinceAck;
		xrudp__session_ackwant(s, s->ElicitingSinceAck >= 2);
	}
	s->Activity = now; ++s->ReceivedPackets; xrudp__session_pto_sync(s, progress); xrudp__session_syncwork(s);
	xrtSecureZero(plain, sizeof(plain));
	if (closing) { (void)xrudp__session_abort(s, XRUDP_CLOSED); return XRUDP_CLOSED; }
	return XRUDP_OK;
malformed:
	xrtSecureZero(plain, sizeof(plain)); (void)xrudp__session_abort(s, XRUDP_MALFORMED); return XRUDP_MALFORMED;
internal:
	xrtSecureZero(plain, sizeof(plain)); (void)xrudp__session_abort(s, XRUDP_IO_ERROR); return XRUDP_IO_ERROR;
}

static bool xrudp__session_append(unsigned char* plain, size_t maximum, size_t* used,
	xrudpframetype type, uint8_t flags, const void* data, size_t size)
{
	size_t written;
	if (xrtRudpFrameAppend(plain + *used, maximum - *used, &written, type, flags, data, size) != XRUDP_OK) return false;
	*used += written; return true;
}
static xrudpresult xrudp__session_send_inner(xrudpsession* s, uint64_t* due,
	unsigned char* plain, unsigned char* body, unsigned char* packet)
{
	size_t used = 0, ackSize = 0, size, slot;
	uint32_t maximum = s->Config.Peer.DatagramSize < s->Config.Local.DatagramSize ? s->Config.Peer.DatagramSize : s->Config.Local.DatagramSize;
	size_t limit = maximum - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE;
	xrudpreceivestate rx; xrudptransmitstate tx; xrudpcipherstate cipher; xrudptransmitframe frame;
	(void)xrtRudpReceiveState(s->Rx, &rx); (void)xrtRudpTransmitState(s->Tx, &tx); (void)xrtRudpCipherState(s->Cipher, &cipher);
	bool grant = (s->ConnectionLimit > s->ConnectionAcked || rx.Limit > s->StreamAcked) && s->Now >= s->GrantRetry;
	bool probe = s->Probes != 0, data = false, eliciting = grant || probe || s->ConfirmWanted;
	if (s->AckWanted) {
		xrudpackset ack = s->Ack; size_t ranges = (limit - 12 - 64) / 16;
		if (ack.Count > ranges) ack.Count = (uint16_t)ranges;
		if (xrtRudpAckSetEncode(&ack, s->Now, plain, limit, &used) != XRUDP_OK) return XRUDP_IO_ERROR;
		ackSize = used;
	}
	if (grant) {
		xrudp__session_put64(body, s->ConnectionLimit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_DATA, 0, body, 8)) return XRUDP_IO_ERROR;
		xrudp__session_put32(body, 1); xrudp__session_put64(body + 4, rx.Limit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_STREAM_DATA, 0, body, 12)) return XRUDP_IO_ERROR;
	}
	/* Leave room for KEY_CONFIRM and the STREAM header. ACK range truncation
	 * retains a nonempty data opportunity even at the 512-byte datagram floor. */
	size_t chunk = limit - used - 24;
	xrudpresult peek = xrtRudpTransmitPeek(s->Tx, probe, body + 12, chunk, &frame);
	if (peek == XRUDP_OK) {
		uint64_t remaining = s->PeerLimit - s->ConnectionSent;
		if (frame.HighestDelta > remaining) {
			uint64_t trim = frame.HighestDelta - remaining;
			if (trim >= frame.Size) { frame.Size = 0; frame.Fin = 0; }
			else { frame.Size -= (uint32_t)trim; frame.Fin = 0; }
		}
		if (frame.Size || frame.Fin) {
			xrudp__session_put32(body, 1); xrudp__session_put64(body + 4, frame.Offset);
			if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_STREAM, frame.Fin ? XRUDP_STREAM_FIN : 0, body, frame.Size + 12)) return XRUDP_IO_ERROR;
			data = eliciting = true;
		}
	} else if (peek != XRUDP_BACKPRESSURE && peek != XRUDP_DONE) return XRUDP_IO_ERROR;
	if (eliciting && !data && !grant)
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_PING, 0, NULL, 0)) return XRUDP_IO_ERROR;
	if (used == 0) return XRUDP_DONE;
	if (eliciting) {
		xrudp__session_put32(body, cipher.ReceiveEpoch);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4)) return XRUDP_IO_ERROR;
	}
	uint32_t flags = eliciting ? XRUDP_SENT_ACK_ELICITING : 0;
	if (probe) flags |= XRUDP_SENT_PROBE;
	uint64_t at = s->Now;
	xrudpresult admission = xrtRudpCongestionCanSend(&s->Cc, s->Now, s->Rtt.SmoothedUs,
		(uint32_t)(used + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE), flags, &at);
	if (!xrtRudpSentCanRecord(s->Sent, probe)) admission = XRUDP_BACKPRESSURE;
	if (admission != XRUDP_OK) {
		if (admission == XRUDP_TIMEOUT && at < *due) *due = at;
		if (ackSize == 0 || !xrtRudpSentCanRecord(s->Sent, false)) return XRUDP_DONE;
		used = ackSize; data = grant = probe = eliciting = false; flags = 0;
	}
	if (!eliciting) {
		at = s->AckAt > s->ControlAt ? s->AckAt : s->ControlAt;
		if (s->Now < at) { if (at < *due) *due = at; return XRUDP_DONE; }
	}
	for (slot = 0; slot < s->Slots && s->Ref[slot].Used; ++slot) {}
	if (slot == s->Slots) return XRUDP_IO_ERROR;
	if (cipher.PacketsInSendEpoch >= s->Config.KeyUpdatePackets && !cipher.SendKeyPending) {
		if (xrtRudpCipherUpdateKey(s->Cipher) != XRUDP_OK) return XRUDP_IO_ERROR;
		++s->KeyUpdates;
	}
	xrudpresult result = xrtRudpCipherSeal(s->Cipher, plain, used, packet, XRUDP_PACKET_MAX, &size);
	if (result != XRUDP_OK) return result == XRUDP_LIMIT ? XRUDP_LIMIT : XRUDP_IO_ERROR;
	result = s->Send(packet, size, s->Context);
	if (result == XRUDP_BACKPRESSURE || result == XRUDP_NO_MEMORY) {
		++s->OutputBackpressure; at = xrudp__session_add(s->Now, 1000); if (at < *due) *due = at;
		return XRUDP_DONE; /* Fresh PN consumed; no false sent/stream/ACK commit. */
	}
	if (result != XRUDP_OK) return XRUDP_IO_ERROR;
	(void)xrtRudpCipherState(s->Cipher, &cipher); uint64_t number = cipher.NextPacketNumber - 1;
	if (xrtRudpSentRecord(s->Sent, number, s->Now, (uint32_t)size, flags, slot + 1) != XRUDP_OK ||
		xrtRudpCongestionOnSent(&s->Cc, number, s->Now, s->Rtt.SmoothedUs, (uint32_t)size, flags) != XRUDP_OK) return XRUDP_IO_ERROR;
	xrudp__sessionref* ref = &s->Ref[slot]; memset(ref, 0, sizeof(*ref)); ref->Used = true;
	ref->Number = number; ref->At = s->Now; ref->Flags = flags;
	if (data) {
		xrudptransmitchange change;
		if (xrtRudpTransmitMarkSent(s->Tx, frame.Offset, frame.Size, frame.Fin != 0, &change) != XRUDP_OK) return XRUDP_IO_ERROR;
		s->ConnectionSent += change.HighestDelta; ref->Offset = frame.Offset; ref->Size = frame.Size; ref->Fin = frame.Fin != 0;
	}
	if (grant) {
		ref->ConnectionGrant = s->ConnectionLimit; ref->StreamGrant = rx.Limit;
		uint64_t interval; (void)xrtRudpRttPto(&s->Rtt, 0, &interval); s->GrantRetry = xrudp__session_add(s->Now, interval);
	}
	if (ackSize) { s->AckWanted = false; s->AckAt = UINT64_MAX; s->ElicitingSinceAck = 0; }
	if (probe) --s->Probes;
	if (!eliciting) s->ControlAt = xrudp__session_add(s->Now, (1000000u + s->Config.ControlPerSecond - 1) / s->Config.ControlPerSecond);
	if (eliciting) s->ConfirmWanted = false;
	++s->SentPackets; xrudp__session_pto_sync(s, false); return XRUDP_OK;
}
static xrudpresult xrudp__session_send(xrudpsession* s, uint64_t* due)
{
	unsigned char plain[XRUDP_PACKET_MAX], body[XRUDP_PACKET_MAX], packet[XRUDP_PACKET_MAX];
	xrudpresult result = xrudp__session_send_inner(s, due, plain, body, packet);
	xrtSecureZero(plain, sizeof(plain)); xrtSecureZero(body, sizeof(body)); xrtSecureZero(packet, sizeof(packet));
	return result;
}

xrudpresult xrtRudpSessionPoll(xrudpsession* s, uint64_t now, uint64_t* output)
{
	if (s == NULL || s->Busy || output == NULL || now < s->Now) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	s->Busy = true; xrudpresult result = xrudp__session_losses(s, 0);
	if (result != XRUDP_OK) goto failure;
	xrudp__session_pto_sync(s, false); xrudp__session_syncwork(s);
	if (s->PtoAt && now >= s->PtoAt) {
		if (s->PtoCount < 32) ++s->PtoCount;
		s->Probes = 2; ++s->ProbeTimeouts; uint64_t interval;
		(void)xrtRudpRttPto(&s->Rtt, s->PtoCount, &interval); s->PtoAt = xrudp__session_add(now, interval);
	}
	if (s->PreviousKeyUntil && now >= s->PreviousKeyUntil) { xrtRudpCipherRetirePrevious(s->Cipher); s->PreviousKeyUntil = 0; }
	uint64_t due = xrudp__session_add(s->Activity, s->IdleUs);
	if (s->Work && xrudp__session_add(s->Progress, s->Config.ProgressTimeoutUs) < due) due = xrudp__session_add(s->Progress, s->Config.ProgressTimeoutUs);
	for (size_t i = 0; i < XRUDP_SESSION_POLL_BURST; ++i) {
		result = xrudp__session_send(s, &due);
		if (result == XRUDP_DONE) break;
		if (result != XRUDP_OK) goto failure;
		if (i + 1 == XRUDP_SESSION_POLL_BURST && xrudp__session_add(now, 1000) < due) due = xrudp__session_add(now, 1000);
	}
	if (s->PtoAt && s->PtoAt < due) due = s->PtoAt;
	if (s->PreviousKeyUntil && s->PreviousKeyUntil < due) due = s->PreviousKeyUntil;
	if (s->AckWanted && xrtRudpSentCanRecord(s->Sent, false)) {
		uint64_t at = s->AckAt > s->ControlAt ? s->AckAt : s->ControlAt; if (at < due) due = at;
	}
	xrudpreceivestate rx; (void)xrtRudpReceiveState(s->Rx, &rx);
	if ((s->ConnectionLimit > s->ConnectionAcked || rx.Limit > s->StreamAcked) && s->GrantRetry > now && s->GrantRetry < due) due = s->GrantRetry;
	uint64_t delay, age; xrudpsentstate sent;
	(void)xrtRudpRttLossDelay(&s->Rtt, &delay); (void)xrtRudpRttPto(&s->Rtt, 0, &age);
	age = xrudp__session_add(age, xrudp__session_add(age, age)); if (age < 1000000) age = 1000000;
	(void)xrtRudpSentState(s->Sent, &sent);
	for (size_t i = 0; i < s->Slots; ++i) if (s->Ref[i].Used) {
		xrudp__sessionref* ref = &s->Ref[i]; uint64_t at = UINT64_MAX;
		if (!(ref->Flags & XRUDP_SENT_ACK_ELICITING)) at = xrudp__session_add(ref->At, age);
		else if (ref->Number < sent.LargestAcked) at = xrudp__session_add(ref->At, delay);
		if (at < due) due = at;
	}
	/* Saturating time does not grant an infinite lease. Caller can terminate at
	 * UINT64_MAX; ordinary deadlines always advance by at least 1ms. */
	if (due <= now) due = xrudp__session_add(now, 1000);
	s->Busy = false; *output = due; return XRUDP_OK;
failure:
	s->Busy = false; (void)xrudp__session_abort(s, xrudp__session_terminal(result) ? result : XRUDP_IO_ERROR); return s->Reason;
}
