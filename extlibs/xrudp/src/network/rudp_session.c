#include <xrudp/session.h>
#include <xrt/memory.h>
#include <string.h>

typedef struct xrudp__sessionref {
	uint64_t Number, At, Offset, ConnectionGrant, StreamGrant;
	uint32_t Size, Flags, Stream, StreamsGrant;
	bool Used, Fin, Open, Reset;
} xrudp__sessionref;

#define XRUDP__ORDINAL_MAX UINT32_C(2147483648)
#define XRUDP__NO_STREAM UINT32_MAX
typedef struct xrudp__sessionstream {
	xrudpreceive* Rx;
	xrudptransmit* Tx;
	xrudpmessage* Message;
	uint64_t InitialCredit, GrantAcked, RetryAt, ResetFinal;
	uint32_t Id, References, Events, Deficit, ResetReason;
	uint16_t Weight;
	uint8_t Priority, Kind;
	bool Local, OpenSent, OpenAcked, ResetQueued, ResetSent, ResetAcked;
	bool LastRead, LastDone, LastEof, LastRetire;
	bool StagePending;
} xrudp__sessionstream;

struct xrudpsession {
	xrudpsessionconfig Config;
	xrudpcipher* Cipher;
	xrudpsentwindow* Sent;
	xrudpreceive* Rx;
	xrudptransmit* Tx;
	xrudp__sessionstream* Streams;
	xrudpcongestion Cc;
	xrudprtt Rtt;
	xrudpackset Ack;
	xrudpsessionsendproc Send;
	void* Context;
	xrudpresult Reason;
	bool Busy, Work, WorkStartPending, AckWanted, ConfirmWanted;
	size_t Allocation;
	uint32_t Slots, PtoCount, Probes, ElicitingSinceAck;
	uint32_t StreamSlots, ActiveStreams, ScheduleCursor, EventCursor, MessageCursor;
	uint32_t IncomingNext, OutgoingNext, IncomingLimit, OutgoingLimit, StreamsAcked;
	uint64_t Now, Activity, Progress, IdleUs, PtoAt, AckAt, ControlAt;
	uint64_t GrantRetry, ConnectionLimit, ConnectionReceived, ConnectionSent, PeerLimit;
	uint64_t ConnectionAcked, StreamsRetry, PreviousKeyUntil;
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
static bool xrudp__stream_done(const xrudp__sessionstream* m)
{
	if (m->ResetQueued) return m->ResetAcked;
	xrudptransmitstate t;
	return xrtRudpTransmitState(m->Tx, &t) == XRUDP_OK && t.FinAcknowledged && t.PendingBytes == 0;
}
static bool xrudp__stream_eof(const xrudp__sessionstream* m)
{
	if (m->Kind == XRUDP_STREAM_MESSAGES) { size_t size; return xrtRudpMessagePeek(m->Message, &size) == XRUDP_DONE; }
	xrudpreceivestate r;
	return xrtRudpReceiveState(m->Rx, &r) == XRUDP_OK && r.HasFinal && !r.Reset && r.ReadOffset == r.FinalOffset;
}
static bool xrudp__stream_retire(const xrudp__sessionstream* m)
{
	if (m->Id == 1 || !m->OpenAcked || m->References != 0 || !xrudp__stream_done(m)) return false;
	xrudpreceivestate r;
	return xrtRudpReceiveState(m->Rx, &r) == XRUDP_OK &&
		(r.Reset || xrudp__stream_eof(m));
}
static bool xrudp__stream_read_ready(const xrudp__sessionstream* m)
{
	if (m->Kind == XRUDP_STREAM_BYTES) return xrtRudpReceiveReady(m->Rx);
	size_t size; xrudpresult r = xrtRudpMessagePeek(m->Message, &size);
	return r == XRUDP_OK || r == XRUDP_DONE || r == XRUDP_CANCELLED;
}
static void xrudp__stream_note(xrudp__sessionstream* m)
{
	bool read = xrudp__stream_read_ready(m), done = xrudp__stream_done(m), eof = xrudp__stream_eof(m);
	bool retire = xrudp__stream_retire(m);
	if (read && !m->LastRead) m->Events |= XRUDP_STREAM_READ_READY;
	if (done && !m->LastDone) m->Events |= XRUDP_STREAM_WRITE_DONE;
	if (eof && !m->LastEof) m->Events |= XRUDP_STREAM_READ_EOF;
	if (retire && !m->LastRetire) m->Events |= XRUDP_STREAM_RETIRE_READY;
	if (!retire) m->Events &= ~XRUDP_STREAM_RETIRE_READY;
	m->LastRead = read; m->LastDone = done; m->LastEof = eof; m->LastRetire = retire;
}
static bool xrudp__stream_local(const xrudpsession* s, uint32_t id)
{
	return (id & 1u) == (s->Config.Role == XRUDP_INITIATOR ? 1u : 0u);
}
static uint32_t xrudp__stream_find(const xrudpsession* s, uint32_t id)
{
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id == id && id != 0) return i;
	return XRUDP__NO_STREAM;
}
static xrudpresult xrudp__stream_access(const xrudpsession* s, uint32_t id, uint32_t* index)
{
	if (s == NULL || s->Busy || id == 0) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	*index = xrudp__stream_find(s, id); if (*index != XRUDP__NO_STREAM) return XRUDP_OK;
	uint32_t next = xrudp__stream_local(s, id) ? s->OutgoingNext : s->IncomingNext;
	return (id / 2 + 1) < next ? XRUDP_CLOSED : XRUDP_ARGUMENT;
}
static void xrudp__stream_activate(xrudpsession* s, uint32_t index, uint32_t id,
	bool local, uint64_t credit, uint8_t priority, uint16_t weight, uint8_t kind)
{
	xrudp__sessionstream* m = &s->Streams[index]; m->Id = id; m->Local = local;
	m->InitialCredit = credit; m->Priority = priority; m->Weight = weight; m->Kind = kind;
	m->GrantAcked = s->Config.Local.StreamCredit; m->OpenAcked = !local || id == 1; m->RetryAt = s->Now;
	(void)xrtRudpTransmitSetLimit(m->Tx, local ? s->Config.Peer.StreamCredit : credit); ++s->ActiveStreams;
}

void xrtRudpSessionConfigInit(xrudpsessionconfig* c)
{
	if (c == NULL) return;
	memset(c, 0, sizeof(*c)); xrtRudpParametersDefault(&c->Local); xrtRudpParametersDefault(&c->Peer);
	c->Local.MaxStreams = 1; c->StreamCapacity = 1; c->Role = XRUDP_INITIATOR;
	c->ReceiveCapacity = c->TransmitCapacity = 65536;
	c->PacketCapacity = 128; c->ControlPerSecond = 128; c->ProgressTimeoutUs = 30000000;
	c->KeyUpdatePackets = 1048576;
}
void xrtRudpStreamConfigInit(xrudpstreamconfig* c)
{
	if (c != NULL) { memset(c, 0, sizeof(*c)); c->Priority = 1; c->Weight = 1; }
}
static xrudpresult xrudp__session_sizes(const xrudpsessionconfig* c, size_t* own, size_t* total)
{
	unsigned char params[32]; size_t rx, tx, sent, message = 0; xrudpcongestion cc; xrudprtt rtt;
	if (c == NULL || c->PacketCapacity == 0 || c->PacketCapacity > XRUDP_SESSION_PACKETS_MAX ||
		c->ControlPerSecond < 40 || c->ControlPerSecond > 4096 ||
		(c->Role != XRUDP_INITIATOR && c->Role != XRUDP_RESPONDER) || c->StreamCapacity == 0 ||
		c->StreamCapacity > XRUDP_SESSION_STREAM_CAPACITY_MAX || c->Local.MaxStreams > c->StreamCapacity ||
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
	if (c->Local.MaxMessageSize != 0 && xrtRudpMessageAllocationSize(c->Local.MaxMessageSize, &message) != XRUDP_OK) return XRUDP_ARGUMENT;
	*own = sizeof(xrudpsession) + (c->PacketCapacity + XRUDP_SENT_PROBE_RESERVE) *
		(sizeof(xrudp__sessionref) + sizeof(xrudpsentevent)) + (2 * c->StreamCapacity - 1) * sizeof(xrudp__sessionstream);
	*total = *own + (2 * c->StreamCapacity - 1) * (rx + tx) + (2 * c->StreamCapacity - 2) * message + sent + xrtRudpCipherAllocationSize();
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
	s->Streams = (xrudp__sessionstream*)(s->Events + s->Slots); s->StreamSlots = 2 * c->StreamCapacity - 1;
	s->IncomingNext = s->OutgoingNext = 2; s->IncomingLimit = s->StreamsAcked = c->Local.MaxStreams; s->OutgoingLimit = c->Peer.MaxStreams;
	s->Now = s->Activity = s->Progress = now; s->AckAt = UINT64_MAX; s->GrantRetry = now;
	s->ConnectionLimit = s->ConnectionAcked = c->Local.ConnectionCredit;
	s->PeerLimit = c->Peer.ConnectionCredit;
	s->IdleUs = (uint64_t)(c->Local.IdleTimeoutMs < c->Peer.IdleTimeoutMs ? c->Local.IdleTimeoutMs : c->Peer.IdleTimeoutMs) * 1000;
	result = xrtRudpSentCreateAfter(c->PacketCapacity, 1, &s->Sent);
	for (uint32_t i = 0; i < s->StreamSlots && result == XRUDP_OK; ++i) {
		result = xrtRudpReceiveCreate(c->ReceiveCapacity, c->Local.StreamCredit, &s->Streams[i].Rx);
		if (result == XRUDP_OK) result = xrtRudpTransmitCreate(c->TransmitCapacity, 0, &s->Streams[i].Tx);
		if (result == XRUDP_OK && i != 0 && c->Local.MaxMessageSize != 0)
			result = xrtRudpMessageCreate(c->Local.MaxMessageSize, &s->Streams[i].Message);
	}
	if (result != XRUDP_OK) {
		for (uint32_t i = 0; i < s->StreamSlots; ++i) { xrtRudpMessageDestroy(s->Streams[i].Message); xrtRudpTransmitDestroy(s->Streams[i].Tx); xrtRudpReceiveDestroy(s->Streams[i].Rx); }
		xrtRudpSentDestroy(s->Sent);
		xrtSecureZero(s, own); xrtFree(s); return result;
	}
	(void)xrtRudpCongestionInit(&s->Cc, c->MaximumWindow); s->Cc.LargestSent = 1;
	(void)xrtRudpRttInit(&s->Rtt, c->InitialRttUs, c->Peer.MaxAckDelayUs);
	xrtRudpAckSetInit(&s->Ack); (void)xrtRudpAckSetRecord(&s->Ack, 1, now);
	xrudp__stream_activate(s, 0, 1, true, c->Local.StreamCredit, 1, 1, XRUDP_STREAM_BYTES); s->Streams[0].OpenSent = true;
	s->Rx = s->Streams[0].Rx; s->Tx = s->Streams[0].Tx;
	s->Cipher = cipher; *output = s; (void)total; return XRUDP_OK;
}
static bool xrudp__session_work(xrudpsession* s)
{
	if (s->ConnectionLimit > s->ConnectionAcked || s->IncomingLimit > s->StreamsAcked) return true;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id != 0) {
		xrudp__sessionstream* m = &s->Streams[i]; xrudptransmitstate tx; xrudpreceivestate rx;
		(void)xrtRudpTransmitState(m->Tx, &tx); (void)xrtRudpReceiveState(m->Rx, &rx);
		if (!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || tx.PendingBytes ||
			(!m->ResetQueued && tx.FinQueued && !tx.FinAcknowledged) || rx.Limit > m->GrantAcked) return true;
	}
	return false;
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
	for (uint32_t i = 0; i < s->StreamSlots; ++i) {
		(void)xrtRudpTransmitCancel(s->Streams[i].Tx, &tx);
		(void)xrtRudpReceiveState(s->Streams[i].Rx, &rx);
		(void)xrtRudpReceiveReset(s->Streams[i].Rx, rx.HighestOffset, (uint32_t)reason, &reset);
		if (s->Streams[i].Message != NULL) (void)xrtRudpMessageReset(s->Streams[i].Message);
		s->Streams[i].References = s->Streams[i].Events = 0;
	}
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
	xrtRudpSentDestroy(s->Sent);
	for (uint32_t i = 0; i < s->StreamSlots; ++i) { xrtRudpMessageDestroy(s->Streams[i].Message); xrtRudpTransmitDestroy(s->Streams[i].Tx); xrtRudpReceiveDestroy(s->Streams[i].Rx); }
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
	state.ActiveStreams = s->ActiveStreams; state.IncomingLimit = s->IncomingLimit; state.OutgoingLimit = s->OutgoingLimit;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Events) ++state.PendingStreamEvents;
	state.ReadReady = xrtRudpReceiveReady(s->Rx); *output = state; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamState(const xrudpsession* s, uint32_t id, xrudpstreamstate* output)
{
	uint32_t index; if (output == NULL) return XRUDP_ARGUMENT;
	xrudpresult result = xrudp__stream_access(s, id, &index); if (result != XRUDP_OK) return result;
	const xrudp__sessionstream* m = &s->Streams[index]; xrudpstreamstate state; memset(&state, 0, sizeof(state));
	state.Id = id; state.References = m->References; state.Weight = m->Weight; state.Priority = m->Priority; state.Kind = m->Kind;
	state.Local = m->Local; state.OpenAcknowledged = m->OpenAcked; state.ReadReady = xrudp__stream_read_ready(m);
	state.WriteDone = xrudp__stream_done(m); state.ResetQueued = m->ResetQueued; state.ResetAcknowledged = m->ResetAcked;
	state.RetireReady = xrudp__stream_retire(m);
	state.ResetReason = m->ResetReason; state.ResetFinal = m->ResetFinal;
	(void)xrtRudpReceiveState(m->Rx, &state.Receive); (void)xrtRudpTransmitState(m->Tx, &state.Transmit);
	if (m->Kind == XRUDP_STREAM_MESSAGES) {
		state.SendMessageLimit = s->Config.Peer.MaxMessageSize;
		if (state.SendMessageLimit > state.Transmit.Capacity - 4u) state.SendMessageLimit = state.Transmit.Capacity - 4u;
		(void)xrtRudpMessageState(m->Message, &state.Message);
	}
	*output = state; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamOpen(xrudpsession* s, const xrudpstreamconfig* c, uint32_t* output)
{
	if (s == NULL || s->Busy || c == NULL || output == NULL || c->Priority > 3 || c->Weight == 0 || c->Weight > 16 || c->Kind > XRUDP_STREAM_MESSAGES) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	if (c->Kind == XRUDP_STREAM_MESSAGES && (s->Config.Local.MaxMessageSize == 0 || s->Config.Peer.MaxMessageSize == 0)) return XRUDP_PROTOCOL_MISMATCH;
	if (s->OutgoingNext > XRUDP__ORDINAL_MAX) return XRUDP_LIMIT;
	if (s->OutgoingNext > s->OutgoingLimit) return XRUDP_BACKPRESSURE;
	uint32_t index; for (index = 1; index < s->Config.StreamCapacity && s->Streams[index].Id != 0; ++index) {}
	if (index == s->Config.StreamCapacity) return XRUDP_BACKPRESSURE;
	bool prior = s->Work; uint32_t id = (s->OutgoingNext - 1) * 2 + (s->Config.Role == XRUDP_INITIATOR ? 1u : 0u);
	xrudp__stream_activate(s, index, id, true, s->Config.Local.StreamCredit, c->Priority, c->Weight, c->Kind); ++s->OutgoingNext;
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true;
	*output = id; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamWrite(xrudpsession* s, uint32_t id, const void* data, size_t size, size_t* written)
{
	if (written != NULL) *written = 0;
	if (written == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	if (s->Streams[index].Kind != XRUDP_STREAM_BYTES) return XRUDP_ARGUMENT;
	bool prior = s->Work;
	xrudpresult result = xrtRudpTransmitWrite(s->Streams[index].Tx, data, size, written); xrudp__session_syncwork(s);
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
xrudpresult xrtRudpSessionStreamFinish(xrudpsession* s, uint32_t id)
{
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	bool prior = s->Work;
	xrudpresult result = xrtRudpTransmitFinish(s->Streams[index].Tx); xrudp__session_syncwork(s);
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
static void xrudp__session_consumed(xrudpsession* s, uint32_t index, uint64_t amount)
{
	xrudp__sessionstream* m = &s->Streams[index]; xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
	if (!rx.Reset) (void)xrtRudpReceiveExtend(m->Rx, xrudp__session_add(rx.ReadOffset, s->Config.Local.StreamCredit));
	m->RetryAt = s->Now;
	s->ConnectionLimit = xrudp__session_add(s->ConnectionLimit, amount); s->GrantRetry = s->Now;
	xrudp__session_syncwork(s);
}
/* Only the current undecoded head is examined. At most four bytes, read from
 * existing reassembly overlaid with this already-validated STREAM batch.
 * Thus an arriving header/FIN cannot commit a useful ACK before rejection. */
static xrudpresult xrudp__message_check(const xrudp__sessionstream* m, const xrudpreceivestate* rx,
	const xrudpreceiveframe* frames, size_t count, bool hasFinal, uint64_t final)
{
	xrudpmessagestate state;
	if (xrtRudpMessageState(m->Message, &state) != XRUDP_OK) return XRUDP_IO_ERROR;
	if (state.Ready || state.Finished || rx->Reset) return XRUDP_OK;
	if (state.Reason != XRUDP_OK) return state.Reason;
	if (hasFinal && final < rx->ReadOffset) return XRUDP_MALFORMED;
	uint64_t remaining = (hasFinal ? final : UINT64_MAX) - rx->ReadOffset;
	if (hasFinal && remaining == 0 && state.HeaderBytes == 0) return XRUDP_OK; /* Clean boundary. */
	uint32_t needed = 4 - state.HeaderBytes, length = state.Expected;
	if (remaining < needed) return XRUDP_MALFORMED;
	for (uint32_t j = 0; j < needed; ++j) {
		if (rx->ReadOffset > UINT64_MAX - j) return XRUDP_MALFORMED;
		uint64_t at = rx->ReadOffset + j; unsigned char byte = 0; size_t read;
		bool found = xrtRudpReceivePeek(m->Rx, at, &byte, 1, &read) == XRUDP_OK;
		if (!found) for (size_t i = 0; i < count; ++i) if (frames[i].Receive == m->Rx && at >= frames[i].Offset && at - frames[i].Offset < frames[i].Size) {
			byte = ((const unsigned char*)frames[i].Data)[(size_t)(at - frames[i].Offset)]; found = true; break;
		}
		if (!found) return XRUDP_OK; /* Header hole; no false malformed result. */
		length = (length << 8) | byte;
	}
	if (length > state.Maximum || remaining - needed < length - state.Buffered) return XRUDP_MALFORMED;
	return XRUDP_OK;
}
static xrudpresult xrudp__message_stage_inner(xrudpsession* s, uint32_t index, size_t* budget, unsigned char* buffer)
{
	xrudp__sessionstream* m = &s->Streams[index]; xrudpreceivestate rx;
	m->StagePending = false; (void)xrtRudpReceiveState(m->Rx, &rx);
	if (rx.Reset) { (void)xrtRudpMessageReset(m->Message); return XRUDP_OK; }
	xrudpresult result = xrudp__message_check(m, &rx, NULL, 0, rx.HasFinal != 0, rx.FinalOffset);
	if (result != XRUDP_OK) return result;
	while (*budget != 0) {
		size_t size; result = xrtRudpMessagePeek(m->Message, &size);
		if (result == XRUDP_OK || result == XRUDP_DONE) break;
		if (result != XRUDP_BACKPRESSURE) return result;
		xrudpmessagestate state; (void)xrtRudpMessageState(m->Message, &state);
		size_t needed = state.HeaderBytes < 4 ? 4 - state.HeaderBytes : state.Expected - state.Buffered;
		if (needed == 0) return XRUDP_IO_ERROR;
		if (needed > 512) needed = 512;
		if (needed > *budget) needed = *budget;
		result = xrtRudpReceiveRead(m->Rx, buffer, needed, &size);
		if (result == XRUDP_DONE) return xrtRudpMessageFinish(m->Message);
		if (result == XRUDP_BACKPRESSURE) return XRUDP_OK;
		if (result != XRUDP_OK) return XRUDP_IO_ERROR;
		size_t consumed;
		result = xrtRudpMessagePush(m->Message, buffer, size, &consumed);
		if (result != XRUDP_OK || consumed != size) return result == XRUDP_MALFORMED ? result : XRUDP_IO_ERROR;
		*budget -= size; xrudp__session_consumed(s, index, size);
	}
	(void)xrtRudpReceiveState(m->Rx, &rx);
	if (rx.HasFinal && rx.ReadOffset == rx.FinalOffset) {
		result = xrtRudpMessageFinish(m->Message);
		if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
	}
	size_t size;
	m->StagePending = xrtRudpMessagePeek(m->Message, &size) == XRUDP_BACKPRESSURE && xrtRudpReceiveReady(m->Rx);
	return XRUDP_OK;
}
static xrudpresult xrudp__session_stage(xrudpsession* s)
{
	if (s->Config.Local.MaxMessageSize == 0) return XRUDP_OK;
	size_t budget = XRUDP_SESSION_MESSAGE_STAGE_BYTES; unsigned char buffer[512]; xrudpresult result = XRUDP_OK;
	uint32_t start = s->MessageCursor;
	for (uint32_t n = 0; n < s->StreamSlots && budget != 0; ++n) {
		uint32_t i = (start + n) % s->StreamSlots; xrudp__sessionstream* m = &s->Streams[i];
		if (m->Id && m->Kind == XRUDP_STREAM_MESSAGES && m->StagePending) {
			result = xrudp__message_stage_inner(s, i, &budget, buffer);
			if (result != XRUDP_OK) break;
			xrudp__stream_note(m); s->MessageCursor = (i + 1) % s->StreamSlots;
		}
	}
	xrtSecureZero(buffer, sizeof(buffer)); return result;
}
xrudpresult xrtRudpSessionStreamRead(xrudpsession* s, uint32_t id, void* output, size_t capacity, size_t* read)
{
	if (read != NULL) *read = 0;
	if (output == NULL || capacity == 0 || read == NULL) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	if (s->Streams[index].Kind != XRUDP_STREAM_BYTES) return XRUDP_ARGUMENT;
	bool prior = s->Work;
	xrudpresult result = xrtRudpReceiveRead(s->Streams[index].Rx, output, capacity, read);
	if (*read != 0) { xrudp__session_consumed(s, index, *read); xrudp__stream_note(&s->Streams[index]); }
	if (!prior && s->Work) s->WorkStartPending = true;
	return result;
}
xrudpresult xrtRudpSessionMessageWrite(xrudpsession* s, uint32_t id, const void* data, size_t size)
{
	if (data == NULL && size != 0) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index]; if (m->Kind != XRUDP_STREAM_MESSAGES) return XRUDP_ARGUMENT;
	if (size > s->Config.Peer.MaxMessageSize) return XRUDP_LIMIT;
	bool prior = s->Work; xrudpresult result = xrtRudpTransmitMessageWrite(m->Tx, data, size);
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true; return result;
}
xrudpresult xrtRudpSessionMessagePeek(const xrudpsession* s, uint32_t id, size_t* size)
{
	if (size != NULL) *size = 0;
	if (size == NULL) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	return s->Streams[index].Kind == XRUDP_STREAM_MESSAGES ? xrtRudpMessagePeek(s->Streams[index].Message, size) : XRUDP_ARGUMENT;
}
xrudpresult xrtRudpSessionMessageRead(xrudpsession* s, uint32_t id, void* output, size_t capacity, size_t* size)
{
	if (size != NULL) *size = 0;
	if (size == NULL || (output == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index]; if (m->Kind != XRUDP_STREAM_MESSAGES) return XRUDP_ARGUMENT;
	xrudpresult result = xrtRudpMessageRead(m->Message, output, capacity, size);
	if (result == XRUDP_OK) { m->StagePending = true; xrudp__stream_note(m); }
	return result;
}
xrudpresult xrtRudpSessionWrite(xrudpsession* s, const void* data, size_t size, size_t* written)
{
	return xrtRudpSessionStreamWrite(s, 1, data, size, written);
}
xrudpresult xrtRudpSessionRead(xrudpsession* s, void* output, size_t capacity, size_t* read)
{
	return xrtRudpSessionStreamRead(s, 1, output, capacity, read);
}
xrudpresult xrtRudpSessionFinish(xrudpsession* s) { return xrtRudpSessionStreamFinish(s, 1); }
xrudpresult xrtRudpSessionStreamReset(xrudpsession* s, uint32_t id, uint32_t reason)
{
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index];
	if (m->ResetQueued) return m->ResetReason == reason ? XRUDP_DUPLICATE : XRUDP_ARGUMENT;
	if (xrudp__stream_done(m)) return XRUDP_DONE;
	bool prior = s->Work; xrudptransmitstate tx; xrudptransmitchange change;
	(void)xrtRudpTransmitState(m->Tx, &tx); (void)xrtRudpTransmitCancel(m->Tx, &change);
	m->ResetQueued = true; m->ResetReason = reason; m->ResetFinal = tx.HighestSent; m->RetryAt = s->Now;
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamRetire(xrudpsession* s, uint32_t id)
{
	if (id == 1) return XRUDP_ARGUMENT;
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index];
	if (!xrudp__stream_retire(m)) return XRUDP_BACKPRESSURE;
	bool prior = s->Work, local = m->Local; xrudpreceive* r = m->Rx; xrudptransmit* t = m->Tx; xrudpmessage* message = m->Message;
	if (message != NULL && xrtRudpMessageRestart(message) != XRUDP_OK) return XRUDP_IO_ERROR;
	if (xrtRudpReceiveRestart(r, s->Config.Local.StreamCredit) != XRUDP_OK || xrtRudpTransmitRestart(t, 0) != XRUDP_OK) return XRUDP_IO_ERROR;
	memset(m, 0, sizeof(*m)); m->Rx = r; m->Tx = t; m->Message = message; --s->ActiveStreams;
	if (!local && s->IncomingLimit < XRUDP__ORDINAL_MAX) { ++s->IncomingLimit; s->StreamsRetry = s->Now; }
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStreamNextEvent(xrudpsession* s, xrudpstreamevent* output)
{
	if (s == NULL || s->Busy || output == NULL) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	for (uint32_t n = 0; n < s->StreamSlots; ++n) {
		uint32_t i = (s->EventCursor + n) % s->StreamSlots; xrudp__sessionstream* m = &s->Streams[i];
		if (m->Id && m->Events) { output->Id = m->Id; output->Events = m->Events; m->Events = 0; s->EventCursor = (i + 1) % s->StreamSlots; return XRUDP_OK; }
	}
	return XRUDP_DONE;
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
	if (s->ConnectionSent >= s->PeerLimit) return true;
	uint64_t backlog = 0;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
		xrudptransmitstate tx; (void)xrtRudpTransmitState(s->Streams[i].Tx, &tx);
		if (!tx.Cancelled && tx.HighestSent < tx.PeerLimit) backlog += tx.QueuedEnd - tx.BaseOffset;
	}
	return backlog < s->Cc.Window;
}
static xrudpresult xrudp__session_feedback(xrudpsession* s, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* event = &s->Events[i];
		if (event->UserTag == 0 || event->UserTag > s->Slots) return XRUDP_IO_ERROR;
		xrudp__sessionref* ref = &s->Ref[event->UserTag - 1];
		if (!ref->Used || ref->Number != event->PacketNumber) return XRUDP_IO_ERROR;
		if (ref->Stream != XRUDP__NO_STREAM && ref->Stream >= s->StreamSlots) return XRUDP_IO_ERROR;
		if (ref->Stream == XRUDP__NO_STREAM && (ref->Size || ref->Fin || ref->Open || ref->Reset || ref->StreamGrant)) return XRUDP_IO_ERROR;
		xrudp__sessionstream* m = ref->Stream < s->StreamSlots ? &s->Streams[ref->Stream] : NULL;
		if (m != NULL && (m->Id == 0 || m->References == 0)) return XRUDP_IO_ERROR;
		if (m != NULL && !m->ResetQueued && (ref->Size || ref->Fin)) {
			xrudptransmitchange change;
			xrudpresult result = xrtRudpTransmitResolve(m->Tx, ref->Offset, ref->Size, ref->Fin, event->Kind == XRUDP_SENT_ACKED, &change);
			if (!xrudp__session_good(result)) return XRUDP_IO_ERROR;
			if (change.NewlyAckedBytes || change.NewFinAcknowledged) s->Progress = s->Now;
			xrudptransmitstate tx; (void)xrtRudpTransmitState(m->Tx, &tx);
			if (change.ReleasedBytes && !tx.FinQueued) m->Events |= XRUDP_STREAM_WRITE_READY;
		}
		if (event->Kind == XRUDP_SENT_ACKED) {
			if (ref->ConnectionGrant > s->ConnectionAcked) { s->ConnectionAcked = ref->ConnectionGrant; s->Progress = s->Now; }
			if (ref->StreamsGrant > s->StreamsAcked) { s->StreamsAcked = ref->StreamsGrant; s->Progress = s->Now; }
			if (m != NULL) {
				if (ref->StreamGrant > m->GrantAcked) { m->GrantAcked = ref->StreamGrant; s->Progress = s->Now; }
				if (ref->Open && !m->OpenAcked) { m->OpenAcked = true; s->Progress = s->Now; }
				if (ref->Reset && !m->ResetAcked) { m->ResetAcked = true; s->Progress = s->Now; }
			}
		} else if (event->Kind == XRUDP_SENT_LOST) {
			++s->LostPackets;
			if (ref->ConnectionGrant || ref->StreamGrant) s->GrantRetry = s->Now;
			if (ref->StreamsGrant) s->StreamsRetry = s->Now;
			if (m != NULL && (ref->StreamGrant || ref->Open || ref->Reset)) m->RetryAt = s->Now;
		}
		if (m != NULL) { --m->References; xrudp__stream_note(m); }
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

typedef struct xrudp__streamplan {
	xrudp__sessionstream* Stream;
	xrudpreceivestate Rx;
	uint64_t Highest, Final, TxLimit, ResetFinal, Credit;
	uint32_t Index, Id, ResetReason;
	uint16_t Weight;
	uint8_t Priority, Kind;
	bool New, HasFinal, HasReset, HasOpen;
} xrudp__streamplan;
static xrudpresult xrudp__plan_get(xrudpsession* s, xrudp__streamplan* plans, size_t* count,
	uint32_t id, bool opening, xrudp__streamplan** output)
{
	*output = NULL;
	if (opening && (id == 1 || xrudp__stream_local(s, id))) return XRUDP_MALFORMED;
	for (size_t i = 0; i < *count; ++i) if (plans[i].Id == id) { *output = &plans[i]; return XRUDP_OK; }
	uint32_t index = xrudp__stream_find(s, id); bool fresh = index == XRUDP__NO_STREAM;
	if (fresh) {
		bool local = xrudp__stream_local(s, id); uint32_t ordinal = id / 2 + 1;
		if (ordinal < (local ? s->OutgoingNext : s->IncomingNext)) return XRUDP_OK; /* Retired, never reopened. */
		if (local || ordinal > s->IncomingLimit) return XRUDP_MALFORMED;
		if (!opening) return XRUDP_BACKPRESSURE; /* OPEN was reordered/lost. */
		for (index = s->Config.StreamCapacity; index < s->StreamSlots; ++index) {
			bool chosen = s->Streams[index].Id != 0;
			for (size_t i = 0; i < *count; ++i) if (plans[i].Index == index) chosen = true;
			if (!chosen) break;
		}
		if (index == s->StreamSlots) return XRUDP_BACKPRESSURE;
	}
	if (*count == XRUDP_FRAMES_MAX) return XRUDP_MALFORMED;
	xrudp__streamplan* plan = &plans[(*count)++]; memset(plan, 0, sizeof(*plan));
	plan->Index = index; plan->Id = id; plan->Stream = &s->Streams[index]; plan->New = fresh;
	(void)xrtRudpReceiveState(plan->Stream->Rx, &plan->Rx);
	xrudptransmitstate tx; (void)xrtRudpTransmitState(plan->Stream->Tx, &tx); plan->TxLimit = tx.PeerLimit;
	plan->Highest = plan->Rx.HighestOffset; plan->HasFinal = plan->Rx.HasFinal != 0; plan->Final = plan->Rx.FinalOffset;
	*output = plan; return XRUDP_OK;
}
static void xrudp__session_epoch(xrudpsession* s, uint32_t previous)
{
	xrudpcipherstate current; (void)xrtRudpCipherState(s->Cipher, &current);
	if (current.ReceiveEpoch == previous) return;
	uint64_t grace; (void)xrtRudpRttPto(&s->Rtt, 0, &grace);
	grace = xrudp__session_add(grace, xrudp__session_add(grace, grace));
	if (grace < 1000000) grace = 1000000;
	if (grace > 60000000) grace = 60000000;
	s->PreviousKeyUntil = xrudp__session_add(s->Now, grace); s->ConfirmWanted = true;
}
xrudpresult xrtRudpSessionInput(xrudpsession* s, const void* packet, size_t size, uint64_t now)
{
	if (s == NULL || s->Busy || packet == NULL || now < s->Now) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	unsigned char plain[XRUDP_PACKET_MAX]; size_t length, cursor = 0;
	xrudpcipherstate previous; xrudpwirepacket parsed;
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
	if (parsed.Header.PacketNumber < 2 || size > s->Config.Local.DatagramSize) goto malformed;
	xrudpwireframe frames[XRUDP_FRAMES_MAX]; size_t offsets[XRUDP_FRAMES_MAX], ends[XRUDP_FRAMES_MAX], count = 0;
	while (cursor < length) {
		if (count == XRUDP_FRAMES_MAX) goto malformed;
		offsets[count] = cursor;
		if (xrtRudpFrameNext(plain, length, &cursor, &frames[count]) != XRUDP_OK) goto malformed;
		ends[count++] = cursor;
	}
	xrudp__streamplan plans[XRUDP_FRAMES_MAX]; size_t planCount = 0, opened = 0;
	/* OPEN may follow its STREAM in the same packet. Reserve candidate slots
	 * without exposing any IDs, data, metadata or events until ALL preflight. */
	for (size_t i = 0; i < count; ++i) if (frames[i].Type == XRUDP_FRAME_OPEN) {
		const unsigned char* d = frames[i].Data; xrudp__streamplan* plan;
		result = xrudp__plan_get(s, plans, &planCount, xrudp__session_get32(d), true, &plan);
		if (result == XRUDP_BACKPRESSURE) goto deferred;
		if (result != XRUDP_OK) goto malformed;
		if (plan == NULL) continue;
		uint64_t credit = xrudp__session_get64(d + 4); uint16_t weight = (uint16_t)(((uint16_t)d[14] << 8) | d[15]);
		if (credit > XRUDP_RECEIVE_CAPACITY_MAX || (d[12] == XRUDP_STREAM_MESSAGES &&
			(s->Config.Local.MaxMessageSize == 0 || s->Config.Peer.MaxMessageSize == 0))) goto malformed;
		if (plan->HasOpen) {
			if (plan->Credit != credit || plan->Priority != d[13] || plan->Weight != weight || plan->Kind != d[12]) goto malformed;
		} else {
			if (!plan->New && (plan->Stream->InitialCredit != credit || plan->Stream->Priority != d[13] || plan->Stream->Weight != weight || plan->Stream->Kind != d[12])) goto malformed;
			plan->HasOpen = true; plan->Credit = credit; plan->Priority = d[13]; plan->Weight = weight; plan->Kind = d[12];
			if (plan->New) { ++opened; plan->TxLimit = credit; }
		}
	}
	/* Ordered admission bounds lifetime history: every lower ordinal was
	 * already opened, so absent older IDs are known retired with no tombstones.
	 * Future reordered OPENs are dropped without ACK, never called malformed. */
	for (uint32_t n = 0; n < opened; ++n) {
		bool found = false;
		for (size_t j = 0; j < planCount; ++j) if (plans[j].New && plans[j].Id / 2 + 1 == s->IncomingNext + n) found = true;
		if (!found) goto deferred;
	}
	xrudpreceiveframe streams[XRUDP_FRAMES_MAX]; size_t streamCount = 0;
	uint64_t peerLimit = s->PeerLimit; uint32_t outgoingLimit = s->OutgoingLimit;
	bool eliciting = false, closing = false;
	for (size_t i = 0; i < count; ++i) {
		const xrudpwireframe* f = &frames[i]; xrudp__streamplan* plan = NULL;
		if (f->Type != XRUDP_FRAME_ACK && f->Type != XRUDP_FRAME_PADDING && f->Type != XRUDP_FRAME_KEY_CONFIRM) eliciting = true;
		if (f->Type == XRUDP_FRAME_STREAM || f->Type == XRUDP_FRAME_RESET || f->Type == XRUDP_FRAME_MAX_STREAM_DATA) {
			result = xrudp__plan_get(s, plans, &planCount, xrudp__session_get32(f->Data), false, &plan);
			if (result == XRUDP_BACKPRESSURE) goto deferred;
			if (result != XRUDP_OK) goto malformed;
			if (plan == NULL) continue; /* Late contents cannot reopen a retired ID or grant connection credit. */
		}
		switch (f->Type) {
		case XRUDP_FRAME_PADDING: case XRUDP_FRAME_PING: case XRUDP_FRAME_KEY_CONFIRM: case XRUDP_FRAME_OPEN: break;
		case XRUDP_FRAME_ACK: {
			xrudpack ack; xrudpackchange change; size_t events;
			if (xrtRudpAckDecode(plain + offsets[i], ends[i] - offsets[i], &ack) != XRUDP_OK ||
				xrtRudpSentCheckAcknowledge(s->Sent, &ack, now, &events, &change) != XRUDP_OK) goto malformed;
			break;
		}
		case XRUDP_FRAME_STREAM: {
			xrudpreceiveframe* sf = &streams[streamCount++]; sf->Receive = plan->Stream->Rx;
			sf->Offset = xrudp__session_get64(f->Data + 4); sf->Size = f->Size - 12;
			sf->Data = f->Data + 12; sf->Fin = (f->Flags & XRUDP_STREAM_FIN) != 0;
			uint64_t end = sf->Offset + sf->Size;
			if (end > plan->Highest) plan->Highest = end;
			if (sf->Fin) { if (plan->HasFinal && end != plan->Final) goto malformed; plan->HasFinal = true; plan->Final = end; }
			break;
		}
		case XRUDP_FRAME_RESET: {
			uint64_t value = xrudp__session_get64(f->Data + 4); uint32_t reason = xrudp__session_get32(f->Data + 12);
			if (plan->HasReset && (plan->ResetFinal != value || plan->ResetReason != reason)) goto malformed;
			plan->HasReset = true; plan->ResetFinal = value; plan->ResetReason = reason; break;
		}
		case XRUDP_FRAME_MAX_DATA: {
			uint64_t value = xrudp__session_get64(f->Data); if (value > peerLimit) peerLimit = value; break;
		}
		case XRUDP_FRAME_MAX_STREAM_DATA: {
			uint64_t value = xrudp__session_get64(f->Data + 4); if (value > plan->TxLimit) plan->TxLimit = value; break;
		}
		case XRUDP_FRAME_MAX_STREAMS: {
			uint32_t value = xrudp__session_get32(f->Data); if (value > XRUDP__ORDINAL_MAX) goto malformed;
			if (value > outgoingLimit) outgoingLimit = value;
			break;
		}
		case XRUDP_FRAME_CLOSE: closing = true; break;
		default: goto malformed;
		}
	}
	uint64_t charge = 0;
	for (size_t i = 0; i < planCount; ++i) {
		xrudp__streamplan* p = &plans[i];
		if (p->HasFinal && p->Highest > p->Final) goto malformed;
		if (p->HasReset) {
			if (p->ResetFinal < p->Highest || p->ResetFinal > p->Rx.Limit || (p->HasFinal && p->ResetFinal != p->Final) ||
				(p->Rx.Reset && p->ResetReason != p->Rx.ResetReason)) goto malformed;
			p->Highest = p->ResetFinal;
		}
		uint64_t delta = p->Highest - p->Rx.HighestOffset;
		if (delta > s->ConnectionLimit - s->ConnectionReceived - charge) goto malformed;
		charge += delta;
	}
	if (streamCount) {
		xrudpreceivebatchchange change;
		if (!xrudp__session_good(xrtRudpReceiveCheckBatch(streams, streamCount, s->ConnectionLimit - s->ConnectionReceived, &change))) goto malformed;
	}
	if (s->Config.Local.MaxMessageSize != 0) for (uint32_t i = 1; i < s->StreamSlots; ++i) {
		xrudp__sessionstream* m = &s->Streams[i]; xrudp__streamplan* plan = NULL;
		for (size_t j = 0; j < planCount; ++j) if (plans[j].Index == i) { plan = &plans[j]; break; }
		uint8_t kind = plan != NULL && plan->New ? plan->Kind : m->Kind;
		if (kind != XRUDP_STREAM_MESSAGES || (plan == NULL && !m->StagePending)) continue;
		if (plan != NULL && plan->HasReset) continue;
		xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
		bool final = plan != NULL ? plan->HasFinal : rx.HasFinal != 0;
		uint64_t end = plan != NULL ? plan->Final : rx.FinalOffset;
		if (xrudp__message_check(m, &rx, streams, streamCount, final, end) != XRUDP_OK) goto malformed;
	}
	/* No peer-dependent rejection, allocation or callback after this point. */
	for (size_t i = 0; i < planCount; ++i) if (plans[i].New) {
		xrudp__streamplan* p = &plans[i];
		xrudp__stream_activate(s, p->Index, p->Id, false, p->Credit, p->Priority, p->Weight, p->Kind);
		p->Stream->Events |= XRUDP_STREAM_OPENED;
	}
	s->IncomingNext += (uint32_t)opened; if (opened) s->Progress = now;
	if (streamCount) {
		xrudpreceivebatchchange change;
		if (!xrudp__session_good(xrtRudpReceivePushBatch(streams, streamCount, s->ConnectionLimit - s->ConnectionReceived, &change))) goto internal;
		if (change.NewBytes || change.NewFinals) s->Progress = now;
	}
	s->ConnectionReceived += charge;
	for (size_t i = 0; i < planCount; ++i) {
		xrudp__streamplan* p = &plans[i];
		if (p->HasReset) {
			xrudpreceivechange change;
			if (!xrudp__session_good(xrtRudpReceiveReset(p->Stream->Rx, p->ResetFinal, p->ResetReason, &change))) goto internal;
			if (change.ConsumedDelta) { s->Progress = now; xrudp__session_consumed(s, p->Index, change.ConsumedDelta); }
		}
		xrudptransmitstate tx; (void)xrtRudpTransmitState(p->Stream->Tx, &tx);
		if (!tx.Cancelled) {
			if (p->TxLimit > tx.PeerLimit) s->Progress = now;
			(void)xrtRudpTransmitSetLimit(p->Stream->Tx, p->TxLimit);
		}
		if (p->Stream->Kind == XRUDP_STREAM_MESSAGES) p->Stream->StagePending = true;
		xrudp__stream_note(p->Stream);
	}
	if (xrudp__session_stage(s) != XRUDP_OK) goto internal;
	if (peerLimit > s->PeerLimit || outgoingLimit > s->OutgoingLimit) s->Progress = now;
	s->PeerLimit = peerLimit; s->OutgoingLimit = outgoingLimit;
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
	xrudp__session_epoch(s, previous.ReceiveEpoch);
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
deferred:
	/* Crypto authenticated a possible key update, but no connection frames
	 * were admitted. Confirm that epoch without falsely ACKing this packet. */
	xrudp__session_epoch(s, previous.ReceiveEpoch); xrtSecureZero(plain, sizeof(plain)); return XRUDP_BACKPRESSURE;
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
static bool xrudp__stream_control(xrudpsession* s, xrudp__sessionstream* m, bool probe)
{
	xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
	return (probe || s->Now >= m->RetryAt) &&
		(!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || rx.Limit > m->GrantAcked);
}
static bool xrudp__stream_data(xrudpsession* s, xrudp__sessionstream* m, bool probe)
{
	if (m->ResetQueued) return false;
	unsigned char byte; xrudptransmitframe frame;
	return xrtRudpTransmitPeek(m->Tx, probe, &byte, 1, &frame) == XRUDP_OK &&
		frame.HighestDelta <= s->PeerLimit - s->ConnectionSent;
}
static uint32_t xrudp__session_select(xrudpsession* s, bool probe)
{
	uint32_t first = XRUDP__NO_STREAM;
	for (uint32_t i = 1; i < s->Config.StreamCapacity; ++i) {
		xrudp__sessionstream* m = &s->Streams[i];
		if (m->Id && !m->OpenAcked && (!m->OpenSent || probe) &&
			(first == XRUDP__NO_STREAM || m->Id < s->Streams[first].Id)) first = i;
	}
	if (first != XRUDP__NO_STREAM) return first;
	for (uint32_t n = 0; n < s->StreamSlots; ++n) {
		uint32_t i = (s->ScheduleCursor + n) % s->StreamSlots; xrudp__sessionstream* m = &s->Streams[i];
		if (m->Id && (xrudp__stream_control(s, m, probe) || xrudp__stream_data(s, m, probe))) return i;
	}
	return XRUDP__NO_STREAM;
}
static xrudpresult xrudp__session_send_inner(xrudpsession* s, uint64_t* due,
	unsigned char* plain, unsigned char* body, unsigned char* packet)
{
	size_t used = 0, ackSize = 0, size, slot;
	uint32_t maximum = s->Config.Peer.DatagramSize < s->Config.Local.DatagramSize ? s->Config.Peer.DatagramSize : s->Config.Local.DatagramSize;
	size_t limit = maximum - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher);
	bool probe = s->Probes != 0;
	uint32_t selected = xrudp__session_select(s, probe);
	xrudp__sessionstream* m = selected != XRUDP__NO_STREAM ? &s->Streams[selected] : NULL;
	xrudpreceivestate rx; memset(&rx, 0, sizeof(rx)); if (m) (void)xrtRudpReceiveState(m->Rx, &rx);
	bool open = m && m->Local && m->Id != 1 && !m->OpenAcked;
	bool reset = m && m->ResetQueued && !m->ResetAcked && (probe || !m->ResetSent || s->Now >= m->RetryAt);
	bool streamGrant = m && rx.Limit > m->GrantAcked && (probe || s->Now >= m->RetryAt);
	bool grant = s->ConnectionLimit > s->ConnectionAcked && s->Now >= s->GrantRetry;
	bool streamsGrant = s->IncomingLimit > s->StreamsAcked && s->Now >= s->StreamsRetry;
	bool data = false, eliciting = open || reset || streamGrant || grant || streamsGrant || probe || s->ConfirmWanted;
	uint32_t quantum = m ? XRUDP_PACKET_MAX * (m->Weight < 16 ? m->Weight : 16u) * (4u - m->Priority) : 0;
	uint32_t deficit = m && m->Deficit ? m->Deficit : quantum;
	if (s->AckWanted) {
		xrudpackset ack = s->Ack; size_t ranges = (limit - 12 - 128) / 16;
		if (ack.Count > ranges) ack.Count = (uint16_t)ranges;
		if (xrtRudpAckSetEncode(&ack, s->Now, plain, limit, &used) != XRUDP_OK) return XRUDP_IO_ERROR;
		ackSize = used;
	}
	if (grant) {
		xrudp__session_put64(body, s->ConnectionLimit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_DATA, 0, body, 8)) return XRUDP_IO_ERROR;
	}
	if (streamsGrant) {
		xrudp__session_put32(body, s->IncomingLimit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_STREAMS, 0, body, 4)) return XRUDP_IO_ERROR;
	}
	if (open) {
		xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, m->InitialCredit);
		body[12] = m->Kind; body[13] = m->Priority; body[14] = (unsigned char)(m->Weight >> 8); body[15] = (unsigned char)m->Weight;
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_OPEN, 0, body, 16)) return XRUDP_IO_ERROR;
	}
	if (streamGrant) {
		xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, rx.Limit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_STREAM_DATA, 0, body, 12)) return XRUDP_IO_ERROR;
	}
	if (reset) {
		xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, m->ResetFinal); xrudp__session_put32(body + 12, m->ResetReason);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_RESET, 0, body, 16)) return XRUDP_IO_ERROR;
	}
	xrudptransmitframe frame; memset(&frame, 0, sizeof(frame));
	if (m && !m->ResetQueued) {
		size_t chunk = limit - used - 24;
		uint32_t overhead = (uint32_t)(used + 24 + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE);
		size_t service = deficit > overhead ? deficit - overhead : 1;
		if (chunk > service) chunk = service;
		xrudpresult peek = xrtRudpTransmitPeek(m->Tx, probe, body + 12, chunk, &frame);
		if (peek == XRUDP_OK) {
			uint64_t remaining = s->PeerLimit - s->ConnectionSent;
			if (frame.HighestDelta > remaining) {
				uint64_t trim = frame.HighestDelta - remaining;
				if (trim >= frame.Size) { frame.Size = 0; frame.Fin = 0; }
				else { frame.Size -= (uint32_t)trim; frame.Fin = 0; }
			}
			if (frame.Size || frame.Fin) {
				xrudp__session_put32(body, m->Id); xrudp__session_put64(body + 4, frame.Offset);
				if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_STREAM, frame.Fin ? XRUDP_STREAM_FIN : 0, body, frame.Size + 12)) return XRUDP_IO_ERROR;
				data = eliciting = true;
			}
		} else if (peek != XRUDP_BACKPRESSURE && peek != XRUDP_DONE) return XRUDP_IO_ERROR;
	}
	if (eliciting && !data && !open && !reset && !streamGrant && !grant && !streamsGrant)
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
		used = ackSize; data = grant = streamsGrant = streamGrant = open = reset = probe = eliciting = false; flags = 0;
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
		return XRUDP_DONE;
	}
	if (result != XRUDP_OK) return XRUDP_IO_ERROR;
	(void)xrtRudpCipherState(s->Cipher, &cipher); uint64_t number = cipher.NextPacketNumber - 1;
	if (xrtRudpSentRecord(s->Sent, number, s->Now, (uint32_t)size, flags, slot + 1) != XRUDP_OK ||
		xrtRudpCongestionOnSent(&s->Cc, number, s->Now, s->Rtt.SmoothedUs, (uint32_t)size, flags) != XRUDP_OK) return XRUDP_IO_ERROR;
	xrudp__sessionref* ref = &s->Ref[slot]; memset(ref, 0, sizeof(*ref)); ref->Used = true;
	ref->Number = number; ref->At = s->Now; ref->Flags = flags; ref->Stream = XRUDP__NO_STREAM;
	if (data || open || reset || streamGrant) { ref->Stream = selected; ++m->References; }
	if (data) {
		xrudptransmitchange change;
		if (xrtRudpTransmitMarkSent(m->Tx, frame.Offset, frame.Size, frame.Fin != 0, &change) != XRUDP_OK) return XRUDP_IO_ERROR;
		s->ConnectionSent += change.HighestDelta; ref->Offset = frame.Offset; ref->Size = frame.Size; ref->Fin = frame.Fin != 0;
	}
	uint64_t interval; (void)xrtRudpRttPto(&s->Rtt, 0, &interval);
	if (open) { ref->Open = true; m->OpenSent = true; }
	if (reset) { ref->Reset = true; m->ResetSent = true; }
	if (streamGrant) ref->StreamGrant = rx.Limit;
	if (open || reset || streamGrant) m->RetryAt = xrudp__session_add(s->Now, interval);
	if (grant) { ref->ConnectionGrant = s->ConnectionLimit; s->GrantRetry = xrudp__session_add(s->Now, interval); }
	if (streamsGrant) { ref->StreamsGrant = s->IncomingLimit; s->StreamsRetry = xrudp__session_add(s->Now, interval); }
	if (ref->Stream != XRUDP__NO_STREAM) {
		m->Deficit = deficit > size ? deficit - (uint32_t)size : 0;
		s->ScheduleCursor = m->Deficit ? selected : (selected + 1) % s->StreamSlots;
		xrudp__stream_note(m);
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
	result = xrudp__session_stage(s); if (result != XRUDP_OK) goto failure;
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
	if (s->ConnectionLimit > s->ConnectionAcked && s->GrantRetry > now && s->GrantRetry < due) due = s->GrantRetry;
	if (s->IncomingLimit > s->StreamsAcked && s->StreamsRetry > now && s->StreamsRetry < due) due = s->StreamsRetry;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
		xrudp__sessionstream* m = &s->Streams[i]; xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
		if (m->StagePending && xrudp__session_add(now, 1000) < due) due = xrudp__session_add(now, 1000);
		if ((!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || rx.Limit > m->GrantAcked) && m->RetryAt > now && m->RetryAt < due) due = m->RetryAt;
	}
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
