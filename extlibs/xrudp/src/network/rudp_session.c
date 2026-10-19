#include <xrudp/session.h>
#include "../internal/xrudp_path.h"
#include <xrt/crypto.h>
#include <xrt/memory.h>
#include <string.h>
#include "../internal/xrudp_loss.h"

typedef struct xrudp__sessionref {
	uint64_t Number, At, Offset, ConnectionGrant, StreamGrant;
	uint32_t Size, Flags, Stream, StreamsGrant;
	bool Used, Fin, Open, Reset, Stop, Datagram, Shutdown, Drained;
} xrudp__sessionref;

#define XRUDP__ORDINAL_MAX UINT32_C(2147483648)
#define XRUDP__NO_STREAM UINT32_MAX
typedef struct xrudp__sessionstream {
	xrudpreceive* Rx;
	xrudptransmit* Tx;
	xrudpmessage* Message;
	uint64_t InitialCredit, GrantAcked, RetryAt, ResetFinal, ResetDeadline, StopRetryAt, StopDeadline;
	uint32_t Id, References, Events, Deficit, ResetReason, PeerStopReason;
	uint16_t Weight;
	uint8_t Priority, Kind;
	bool Local, OpenSent, OpenAcked, ResetQueued, ResetSent, ResetAcked, ResetClockPending;
	bool StopQueued, StopSent, StopComplete, StopClockPending, PeerStopped;
	bool LastRead, LastReadDone, LastDone, LastEof, LastRetire;
	bool StagePending;
} xrudp__sessionstream;

struct xrudpsession {
	xrudpsessionconfig Config;
	xrudpcipher* Cipher;
	xrudpsentwindow* Sent;
	xrudpdatagrams* Datagrams;
	xrudpreceive* Rx;
	xrudptransmit* Tx;
	xrudp__sessionstream* Streams;
	xrudpcongestion Cc;
	xrudp__lossproof Loss;
	xrudprtt Rtt;
	xrudpackset Ack;
	xrudpsessionsendproc Send;
	void* Context;
	xrudpresult Reason;
	xrudpshutdownstate Shutdown;
	xrudp__path Path;
	xrudpmtustate Mtu;
	uint64_t ShutdownLocalDeadline, ShutdownPeerDeadline, ShutdownRetry, PeerDrainedPacket;
	bool ShutdownClockPending;
	bool Busy, Work, WorkStartPending, AckWanted, ConfirmWanted, PreferDatagram;
	size_t Allocation;
	uint32_t Slots, PtoCount, Probes, ElicitingSinceAck;
	uint32_t StreamSlots, ActiveStreams, ScheduleCursor, EventCursor, MessageCursor, StopWaiters, ResetWaiters;
	uint32_t IncomingNext, OutgoingNext, IncomingLimit, OutgoingLimit, StreamsAcked;
	uint64_t Now, Activity, Progress, IdleUs, PtoAt, AckAt, ControlAt, KeepAliveSent;
	uint64_t GrantRetry, ConnectionLimit, ConnectionReceived, ConnectionSent, PeerLimit;
	uint64_t ConnectionAcked, StreamsRetry, PreviousKeyUntil;
	uint64_t Eliciting[XRUDP_REPLAY_BITS];
	uint64_t ReceivedPackets, SentPackets, LostPackets, ProbeTimeouts, KeyUpdates, InvalidPackets, OutputBackpressure;
	uint64_t PersistentCongestion;
	uint64_t DatagramAcknowledged, DatagramLost, DatagramDisabledDropped;
	xrudpsentevent* Events;
	xrudp__sessionref Ref[];
};

static uint64_t xrudp__session_add(uint64_t a, uint64_t b) { return a > UINT64_MAX - b ? UINT64_MAX : a + b; }
static uint32_t xrudp__mtu_next(uint32_t current, uint32_t ceiling)
{
	uint32_t next = current < 768 ? 768u : (current < 1024 ? 1024u : ceiling);
	return next < ceiling ? next : ceiling;
}
static void xrudp__mtu_reset(xrudpsession* s)
{
	xrudpmtustate* m = &s->Mtu;
	m->Enabled = s->Config.DiscoverMtu; m->Ceiling = s->Config.Local.DatagramSize < s->Config.Peer.DatagramSize ? s->Config.Local.DatagramSize : s->Config.Peer.DatagramSize;
	m->Effective = m->Enabled ? 512u : m->Ceiling; m->ProbeSize = 512;
	m->Attempts = m->LargeSize = 0; m->ProbePacket = m->ProbeDeadlineUs = m->LargePacket = m->LargeDeadlineUs = 0;
	m->BaseConfirmed = false; m->Failure = XRUDP_OK;
	m->RaiseAfterUs = 0;
	m->NextProbeUs = m->Enabled ? xrudp__session_add(s->Now, 100000) : UINT64_MAX;
}
static uint64_t xrudp__mtu_interval(const xrudpsession* s)
{
	uint64_t interval = 1000000; (void)xrtRudpRttPto(&s->Rtt, 0, &interval);
	interval = xrudp__session_add(interval, xrudp__session_add(interval, interval));
	return interval < 1000000 ? 1000000 : interval;
}
static void xrudp__mtu_result(xrudpsession* s, const xrudpsentevent* e)
{
	xrudpmtustate* m = &s->Mtu;
	if (!m->Enabled) return;
	if (e->Kind == XRUDP_SENT_ACKED && e->Bytes >= 512 && !m->BaseConfirmed) {
		m->BaseConfirmed = true; m->Failure = XRUDP_OK;
		if (!m->ProbePacket && m->ProbeSize == 512) {
			m->ProbeSize = xrudp__mtu_next(512, m->Ceiling); m->Attempts = 0;
			m->NextProbeUs = m->Ceiling > 512 ? xrudp__session_add(s->Now, 100000) : UINT64_MAX;
			if (m->NextProbeUs < m->RaiseAfterUs) m->NextProbeUs = m->RaiseAfterUs;
		}
	}
	if ((e->Flags & XRUDP_SENT_MTU) && e->PacketNumber == m->ProbePacket) {
		m->ProbePacket = m->ProbeDeadlineUs = 0;
		if (e->Kind == XRUDP_SENT_ACKED) {
			m->ProbesAcknowledged = xrudp__session_add(m->ProbesAcknowledged, 1);
			m->Effective = m->ProbeSize; m->BaseConfirmed = true; m->Attempts = 0;
			m->ProbeSize = xrudp__mtu_next(m->Effective, m->Ceiling);
			m->NextProbeUs = m->Effective < m->Ceiling ? xrudp__session_add(s->Now, 100000) : UINT64_MAX;
			if (m->NextProbeUs < m->RaiseAfterUs) m->NextProbeUs = m->RaiseAfterUs;
		} else {
			m->ProbeFailures = xrudp__session_add(m->ProbeFailures, 1);
			if (m->BaseConfirmed && m->ProbeSize == 512) {
				m->Attempts = 0; m->ProbeSize = xrudp__mtu_next(512, m->Ceiling);
				m->NextProbeUs = m->Ceiling > 512 ? xrudp__session_add(s->Now, 100000) : UINT64_MAX;
				if (m->NextProbeUs < m->RaiseAfterUs) m->NextProbeUs = m->RaiseAfterUs;
			} else if (m->Attempts >= 3) {
				if (!m->BaseConfirmed) { m->Failure = XRUDP_TIMEOUT; m->NextProbeUs = UINT64_MAX; }
				else { m->Attempts = 0; m->NextProbeUs = xrudp__session_add(s->Now, 60000000); }
			} else m->NextProbeUs = xrudp__session_add(s->Now, xrudp__mtu_interval(s));
		}
	}
	/* An ACK for a smaller packet/other stream cannot renew the pending large
	 * size evidence. Retired old-path/old-size ACKs never reach this callback. */
	if (e->Kind == XRUDP_SENT_ACKED && e->PacketNumber >= m->LargePacket && e->Bytes >= m->LargeSize && m->LargeDeadlineUs) {
		m->LargeSize = 0; m->LargePacket = m->LargeDeadlineUs = 0;
	}
}
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
	return xrtRudpReceiveState(m->Rx, &r) == XRUDP_OK && r.HasFinal && !r.Reset && !r.Stopped && r.ReadOffset == r.FinalOffset;
}
static bool xrudp__stream_retire(const xrudp__sessionstream* m)
{
	if (m->Id == 1 || !m->OpenAcked || m->References != 0 || !xrudp__stream_done(m)) return false;
	xrudpreceivestate r;
	return xrtRudpReceiveState(m->Rx, &r) == XRUDP_OK &&
		(r.Reset || (r.Stopped && r.HasFinal) || xrudp__stream_eof(m));
}
static bool xrudp__stream_read_done(const xrudp__sessionstream* m)
{
	xrudpreceivestate r; (void)xrtRudpReceiveState(m->Rx, &r);
	return r.Reset || (r.Stopped && r.HasFinal) || xrudp__stream_eof(m);
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
	bool readDone = xrudp__stream_read_done(m);
	if (read && !m->LastRead) m->Events |= XRUDP_STREAM_READ_READY;
	if (done && !m->LastDone) m->Events |= XRUDP_STREAM_WRITE_DONE;
	if (eof && !m->LastEof) m->Events |= XRUDP_STREAM_READ_EOF;
	if (retire && !m->LastRetire) m->Events |= XRUDP_STREAM_RETIRE_READY;
	if (readDone && !m->LastReadDone) m->Events |= XRUDP_STREAM_READ_DONE;
	if (!retire) m->Events &= ~XRUDP_STREAM_RETIRE_READY;
	m->LastRead = read; m->LastReadDone = readDone; m->LastDone = done; m->LastEof = eof; m->LastRetire = retire;
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
	unsigned char params[32]; size_t rx, tx, sent, message = 0, datagrams = 0; xrudpcongestion cc; xrudprtt rtt;
	if (c == NULL || c->PacketCapacity == 0 || c->PacketCapacity > XRUDP_SESSION_PACKETS_MAX ||
		c->ControlPerSecond < 40 || c->ControlPerSecond > 4096 ||
		(c->Role != XRUDP_INITIATOR && c->Role != XRUDP_RESPONDER) || c->StreamCapacity == 0 ||
		c->StreamCapacity > XRUDP_SESSION_STREAM_CAPACITY_MAX || c->Local.MaxStreams > c->StreamCapacity ||
		c->ProgressTimeoutUs < 1000000 || c->ProgressTimeoutUs > 600000000 ||
		(c->KeepAliveUs && (c->KeepAliveUs < 1000000 || c->KeepAliveUs > UINT64_C(3600000000))) ||
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
	if (c->Datagrams.Channels != 0) {
		if (xrtRudpDatagramAllocationSize(&c->Datagrams, &datagrams) != XRUDP_OK) return XRUDP_ARGUMENT;
	} else if (c->Datagrams.SendCapacity || c->Datagrams.ReceiveCapacity || c->Datagrams.PayloadCapacity) return XRUDP_ARGUMENT;
	*own = sizeof(xrudpsession) + (c->PacketCapacity + XRUDP_SENT_PROBE_RESERVE) *
		(sizeof(xrudp__sessionref) + sizeof(xrudpsentevent) + sizeof(xrudp__lossrecord)) + (2 * c->StreamCapacity - 1) * sizeof(xrudp__sessionstream);
	*total = *own + (2 * c->StreamCapacity - 1) * (rx + tx) + (2 * c->StreamCapacity - 2) * message + sent + datagrams + xrtRudpCipherAllocationSize();
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
	s->Loss.Records = (xrudp__lossrecord*)(s->Events + s->Slots); s->Loss.Capacity = s->Slots; s->Loss.RetiredThrough = 1;
	s->Streams = (xrudp__sessionstream*)(s->Loss.Records + s->Slots); s->StreamSlots = 2 * c->StreamCapacity - 1;
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
	if (result == XRUDP_OK && c->Datagrams.Channels != 0) result = xrtRudpDatagramCreate(&c->Datagrams, &s->Datagrams);
	if (result != XRUDP_OK) {
		xrtRudpDatagramDestroy(s->Datagrams);
		for (uint32_t i = 0; i < s->StreamSlots; ++i) { xrtRudpMessageDestroy(s->Streams[i].Message); xrtRudpTransmitDestroy(s->Streams[i].Tx); xrtRudpReceiveDestroy(s->Streams[i].Rx); }
		xrtRudpSentDestroy(s->Sent);
		xrtSecureZero(s, own); xrtFree(s); return result;
	}
	(void)xrtRudpCongestionInit(&s->Cc, c->MaximumWindow); s->Cc.LargestSent = 1;
	(void)xrtRudpRttInit(&s->Rtt, c->InitialRttUs, c->Peer.MaxAckDelayUs);
	xrtRudpAckSetInit(&s->Ack); (void)xrtRudpAckSetRecord(&s->Ack, 1, now);
	xrudp__stream_activate(s, 0, 1, true, c->Local.StreamCredit, 1, 1, XRUDP_STREAM_BYTES); s->Streams[0].OpenSent = true;
	s->Rx = s->Streams[0].Rx; s->Tx = s->Streams[0].Tx;
	s->Cipher = cipher; xrudp__mtu_reset(s); *output = s; (void)total; return XRUDP_OK;
}
static bool xrudp__session_work(xrudpsession* s)
{
	if (s->Shutdown.Requested || s->Shutdown.PeerRequested) return true;
	if (s->Datagrams != NULL) { xrudpdatagramview view; if (xrtRudpDatagramPeekSend(s->Datagrams, &view) == XRUDP_OK) return true; }
	if (s->ConnectionLimit > s->ConnectionAcked || s->IncomingLimit > s->StreamsAcked) return true;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id != 0) {
		xrudp__sessionstream* m = &s->Streams[i]; xrudptransmitstate tx; xrudpreceivestate rx;
		(void)xrtRudpTransmitState(m->Tx, &tx); (void)xrtRudpReceiveState(m->Rx, &rx);
		if (!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || tx.PendingBytes ||
			(!m->ResetQueued && tx.FinQueued && !tx.FinAcknowledged) || rx.Limit > m->GrantAcked ||
			(m->StopQueued && !rx.HasFinal)) return true;
	}
	return false;
}
static void xrudp__session_syncwork(xrudpsession* s)
{
	bool work = xrudp__session_work(s);
	if (work && !s->Work) s->Progress = s->Now;
	s->Work = work;
}
static uint64_t xrudp__session_keepalive(const xrudpsession* s)
{
	if (!s->Config.KeepAliveUs || s->Work || s->PtoAt || s->Shutdown.Requested || s->Shutdown.PeerRequested) return UINT64_MAX;
	uint64_t interval = s->Config.KeepAliveUs, maximum = s->IdleUs / 3;
	if (interval > maximum) interval = maximum;
	uint64_t base = s->Activity > s->KeepAliveSent ? s->Activity : s->KeepAliveSent;
	return xrudp__session_add(base, interval);
}
static xrudpresult xrudp__session_abort(xrudpsession* s, xrudpresult reason)
{
	if (s->Reason != XRUDP_OK) return XRUDP_DONE;
	xrudptransmitchange tx; xrudpreceivestate rx; xrudpreceivechange reset; size_t count;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) {
		if (!s->Shutdown.Succeeded) (void)xrtRudpTransmitCancel(s->Streams[i].Tx, &tx);
		(void)xrtRudpReceiveState(s->Streams[i].Rx, &rx);
		if (!s->Shutdown.Succeeded) (void)xrtRudpReceiveReset(s->Streams[i].Rx, rx.HighestOffset, (uint32_t)reason, &reset);
		if (!s->Shutdown.Succeeded && s->Streams[i].Message != NULL) (void)xrtRudpMessageReset(s->Streams[i].Message);
		s->Streams[i].References = s->Streams[i].Events = 0;
		s->Streams[i].StopClockPending = false; s->Streams[i].StopDeadline = 0;
		s->Streams[i].ResetClockPending = false; s->Streams[i].ResetDeadline = 0;
	}
	s->StopWaiters = s->ResetWaiters = 0;
	xrudp__path_clear(&s->Path);
	if (s->Datagrams != NULL) (void)xrtRudpDatagramAbort(s->Datagrams);
	(void)xrtRudpSentAbort(s->Sent, s->Events, s->Slots, &count);
	(void)xrtRudpCongestionApply(&s->Cc, s->Events, count, s->Now, true);
	memset(s->Ref, 0, s->Slots * sizeof(*s->Ref)); xrtRudpCipherDestroy(s->Cipher); s->Cipher = NULL;
	s->AckWanted = s->ConfirmWanted = s->Work = s->WorkStartPending = false; s->Probes = 0; s->PtoAt = 0; s->Reason = reason;
	s->Mtu.ProbePacket = s->Mtu.ProbeDeadlineUs = s->Mtu.LargePacket = s->Mtu.LargeDeadlineUs = 0; s->Mtu.NextProbeUs = UINT64_MAX;
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
	xrtRudpDatagramDestroy(s->Datagrams);
	for (uint32_t i = 0; i < s->StreamSlots; ++i) { xrtRudpMessageDestroy(s->Streams[i].Message); xrtRudpTransmitDestroy(s->Streams[i].Tx); xrtRudpReceiveDestroy(s->Streams[i].Rx); }
	size_t size = s->Allocation; xrtSecureZero(s, size); xrtFree(s);
}
xrudpresult xrtRudpSessionState(const xrudpsession* s, xrudpsessionstate* output)
{
	if (s == NULL || output == NULL || s->Busy) return XRUDP_ARGUMENT;
	xrudpsessionstate state; memset(&state, 0, sizeof(state)); state.Reason = s->Reason;
	state.Shutdown = s->Shutdown;
	state.Path = s->Path.State;
	(void)xrtRudpReceiveState(s->Rx, &state.Receive); (void)xrtRudpTransmitState(s->Tx, &state.Transmit);
	(void)xrtRudpSentState(s->Sent, &state.Sent); state.Congestion = s->Cc; state.Rtt = s->Rtt;
	state.ReceivedConnection = s->ConnectionReceived; state.ReceiveLimit = s->ConnectionLimit;
	state.SentConnection = s->ConnectionSent; state.PeerLimit = s->PeerLimit;
	state.LastNowUs = s->Now; state.LastActivityUs = s->Activity; state.ProgressAtUs = s->Progress;
	state.ReceivedPackets = s->ReceivedPackets; state.SentPackets = s->SentPackets; state.LostPackets = s->LostPackets;
	state.ProbeTimeouts = s->ProbeTimeouts; state.KeyUpdates = s->KeyUpdates; state.InvalidPackets = s->InvalidPackets;
	state.PersistentCongestion = s->PersistentCongestion;
	state.OutputBackpressure = s->OutputBackpressure; state.PtoBackoff = s->PtoCount;
	state.Datagrams.NextDeadlineUs = UINT64_MAX;
	state.DatagramAcknowledged = s->DatagramAcknowledged; state.DatagramLost = s->DatagramLost;
	state.DatagramDisabledDropped = s->DatagramDisabledDropped; state.Mtu = s->Mtu;
	if (s->Datagrams != NULL) {
		(void)xrtRudpDatagramState(s->Datagrams, &state.Datagrams);
		uint32_t maximum = s->Mtu.Effective;
		state.DatagramSendLimit = maximum - XRUDP_DATAGRAM_OVERHEAD;
		if (state.DatagramSendLimit > s->Config.Datagrams.PayloadCapacity) state.DatagramSendLimit = s->Config.Datagrams.PayloadCapacity;
	}
	state.OutstandingProbes = s->Probes; state.AckPending = s->AckWanted;
	state.ActiveStreams = s->ActiveStreams; state.IncomingLimit = s->IncomingLimit; state.OutgoingLimit = s->OutgoingLimit;
	state.PendingReceiveStops = s->StopWaiters;
	state.PendingSendResets = s->ResetWaiters;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Events) ++state.PendingStreamEvents;
	state.ReadReady = xrtRudpReceiveReady(s->Rx); state.WriteStopped = s->Streams[0].PeerStopped;
	state.ReadDone = xrudp__stream_read_done(&s->Streams[0]);
	state.WriteDone = xrudp__stream_done(&s->Streams[0]);
	state.WriteStopReason = s->Streams[0].PeerStopReason; *output = state; return XRUDP_OK;
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
	state.ReadDone = xrudp__stream_read_done(m);
	state.ResetReason = m->ResetReason; state.ResetFinal = m->ResetFinal;
	state.StopQueued = m->StopQueued; state.StopSent = m->StopSent; state.StopComplete = m->StopComplete;
	state.ReceiveStopDeadlineUs = m->StopDeadline;
	state.ResetDeadlineUs = m->ResetDeadline;
	state.PeerStopped = m->PeerStopped; state.PeerStopReason = m->PeerStopReason;
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
	if (s->Shutdown.Requested || s->Shutdown.PeerRequested) return XRUDP_CLOSED;
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
	if (s->Shutdown.Requested) return XRUDP_CLOSED;
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
static void xrudp__session_credit(xrudpsession* s, uint64_t amount)
{
	s->ConnectionLimit = xrudp__session_add(s->ConnectionLimit, amount); s->GrantRetry = s->Now;
}
static void xrudp__session_consumed(xrudpsession* s, uint32_t index, uint64_t amount)
{
	xrudp__sessionstream* m = &s->Streams[index]; xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
	if (!rx.Reset && !rx.Stopped) (void)xrtRudpReceiveExtend(m->Rx, xrudp__session_add(rx.ReadOffset, s->Config.Local.StreamCredit));
	m->RetryAt = s->Now;
	xrudp__session_credit(s, amount);
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
	if (state.Ready || state.Finished || rx->Reset || rx->Stopped) return XRUDP_OK;
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
	if (rx.Reset || rx.Stopped) { (void)xrtRudpMessageReset(m->Message); return XRUDP_OK; }
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
	if (s->Shutdown.Requested) return XRUDP_CLOSED;
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
xrudpresult xrtRudpSessionShutdown(xrudpsession* s, uint32_t timeout)
{
	if (s == NULL || s->Busy || timeout < 1000 || timeout > 600000) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	if (s->Shutdown.Requested) return s->Shutdown.LocalTimeoutMs == timeout ? XRUDP_DUPLICATE : XRUDP_ARGUMENT;
	bool prior = s->Work;
	s->Shutdown.Requested = true; s->Shutdown.LocalTimeoutMs = timeout;
	s->Shutdown.LocalFinalOrdinal = s->OutgoingNext - 1; s->ShutdownClockPending = true; s->ShutdownRetry = s->Now;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
		if (!s->Streams[i].ResetQueued) (void)xrtRudpTransmitFinish(s->Streams[i].Tx);
		s->Streams[i].Events &= ~XRUDP_STREAM_WRITE_READY;
	}
	if (s->Datagrams != NULL) (void)xrtRudpDatagramAbort(s->Datagrams);
	xrudp__session_syncwork(s); if (!prior) s->WorkStartPending = true;
	return XRUDP_OK;
}
static void xrudp__stream_reset_send(xrudpsession* s, xrudp__sessionstream* m, uint32_t reason)
{
	xrudptransmitstate tx; xrudptransmitchange change;
	(void)xrtRudpTransmitState(m->Tx, &tx); (void)xrtRudpTransmitCancel(m->Tx, &change);
	m->Events &= ~XRUDP_STREAM_WRITE_READY;
	m->ResetQueued = true; m->ResetReason = reason; m->ResetFinal = tx.HighestSent; m->RetryAt = s->Now;
	m->ResetClockPending = true; ++s->ResetWaiters;
}
xrudpresult xrtRudpSessionStreamReset(xrudpsession* s, uint32_t id, uint32_t reason)
{
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index];
	if (m->ResetQueued) return m->ResetReason == reason ? XRUDP_DUPLICATE : XRUDP_ARGUMENT;
	if (xrudp__stream_done(m)) return XRUDP_DONE;
	bool prior = s->Work; xrudp__stream_reset_send(s, m, reason);
	xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true; return XRUDP_OK;
}
xrudpresult xrtRudpSessionStop(xrudpsession* s, uint32_t reason) { return xrtRudpSessionStreamStop(s, 1, reason); }
xrudpresult xrtRudpSessionStreamStop(xrudpsession* s, uint32_t id, uint32_t reason)
{
	uint32_t index; xrudpresult access = xrudp__stream_access(s, id, &index); if (access != XRUDP_OK) return access;
	xrudp__sessionstream* m = &s->Streams[index]; xrudpreceivechange change;
	xrudpresult result = xrtRudpReceiveStop(m->Rx, reason, &change); if (result != XRUDP_OK) return result;
	bool prior = s->Work; xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
	m->StopQueued = true; m->StopComplete = rx.HasFinal != 0; m->StopRetryAt = s->Now;
	if (!rx.HasFinal) { m->StopClockPending = true; ++s->StopWaiters; }
	if (m->Message != NULL) { (void)xrtRudpMessageReset(m->Message); m->StagePending = false; }
	if (change.ConsumedDelta) xrudp__session_consumed(s, index, change.ConsumedDelta);
	xrudp__stream_note(m); xrudp__session_syncwork(s); if (!prior && s->Work) s->WorkStartPending = true; return XRUDP_OK;
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
	if (!local && !s->Shutdown.Requested && s->IncomingLimit < XRUDP__ORDINAL_MAX) { ++s->IncomingLimit; s->StreamsRetry = s->Now; }
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
	if (s->Path.State.Enabled) xrudp__path_clock(&s->Path, now);
	if (s->ShutdownClockPending) {
		s->ShutdownClockPending = false; s->ShutdownLocalDeadline = xrudp__session_add(now, (uint64_t)s->Shutdown.LocalTimeoutMs * 1000);
	}
	s->Shutdown.DeadlineUs = s->ShutdownLocalDeadline;
	if (s->ShutdownPeerDeadline && (!s->Shutdown.DeadlineUs || s->ShutdownPeerDeadline < s->Shutdown.DeadlineUs))
		s->Shutdown.DeadlineUs = s->ShutdownPeerDeadline;
	/* Clock-free application operations cannot use a stale idle timestamp to
	 * start their deadline. Anchor only this idle->work transition at the next
	 * driver observation; further writes/reads cannot continually rearm it. */
	if (s->WorkStartPending) { s->Progress = now; s->WorkStartPending = false; }
	if (s->StopWaiters || s->ResetWaiters) for (uint32_t i = 0; i < s->StreamSlots; ++i) {
		xrudp__sessionstream* m = &s->Streams[i];
		if (m->StopClockPending) { m->StopClockPending = false; m->StopDeadline = xrudp__session_add(now, s->Config.ProgressTimeoutUs); }
		if (m->ResetClockPending) { m->ResetClockPending = false; m->ResetDeadline = xrudp__session_add(now, s->Config.ProgressTimeoutUs); }
	}
	/* Expired unreliable work must not trigger a useful-progress timeout for
	 * work that no longer exists. This is local maintenance, not peer liveness. */
	if (s->Datagrams != NULL && now != UINT64_MAX) { (void)xrtRudpDatagramMaintain(s->Datagrams, now); xrudp__session_syncwork(s); }
}
static bool xrudp__session_dead(xrudpsession* s, uint64_t now)
{
	if (s->Shutdown.LingerUntilUs) return false; /* Proven drain has its own immutable final timer. */
	if (s->Shutdown.DeadlineUs && now >= s->Shutdown.DeadlineUs) return true;
	if (now == UINT64_MAX || now - s->Activity >= s->IdleUs || (s->Work && now - s->Progress >= s->Config.ProgressTimeoutUs)) return true;
	if (s->StopWaiters || s->ResetWaiters) for (uint32_t i = 0; i < s->StreamSlots; ++i)
		if ((s->Streams[i].StopDeadline && now >= s->Streams[i].StopDeadline) ||
			(s->Streams[i].ResetDeadline && now >= s->Streams[i].ResetDeadline)) return true;
	return false;
}
static void xrudp__shutdown_update(xrudpsession* s)
{
	xrudpshutdownstate* c = &s->Shutdown;
	if (!c->Requested || !c->PeerRequested) return;
	if (!c->Ready && s->IncomingNext - 1 == c->PeerFinalOrdinal) {
		bool ready = true;
		for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
			xrudp__sessionstream* m = &s->Streams[i];
			if (!m->OpenAcked || m->References || !xrudp__stream_done(m) || !xrudp__stream_read_done(m)) { ready = false; break; }
		}
		if (ready) { c->Ready = true; s->ShutdownRetry = s->Now; s->Progress = s->Now; }
	}
	if (c->Ready && c->DrainedAcknowledged && c->PeerDrained && c->PeerDrainedAcknowledged && !c->LingerUntilUs) {
		uint64_t interval; (void)xrtRudpRttPto(&s->Rtt, 0, &interval);
		interval = xrudp__session_add(interval, xrudp__session_add(interval, interval));
		if (interval < 100000) interval = 100000;
		c->LingerUntilUs = xrudp__session_add(s->Now, interval);
		if (c->DeadlineUs && c->LingerUntilUs > c->DeadlineUs) c->LingerUntilUs = c->DeadlineUs;
		s->Probes = 0; s->ConfirmWanted = false;
		for (uint32_t i = 0; i < s->StreamSlots; ++i) s->Streams[i].Events = 0;
	}
}
static bool xrudp__shutdown_complete(xrudpsession* s)
{
	if (!s->Shutdown.LingerUntilUs || s->Now < s->Shutdown.LingerUntilUs) return false;
	s->Shutdown.Succeeded = true; (void)xrudp__session_abort(s, XRUDP_CLOSED); return true;
}
static xrudpresult xrudp__session_datagram_access(const xrudpsession* s)
{
	if (s == NULL || s->Busy) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	if (s->Shutdown.Requested) return XRUDP_CLOSED;
	return s->Datagrams != NULL ? XRUDP_OK : XRUDP_PROTOCOL_MISMATCH;
}
xrudpresult xrtRudpSessionDatagramConfigure(xrudpsession* s, const xrudpdatagramchannel* channel)
{
	xrudpresult r = xrudp__session_datagram_access(s); if (r != XRUDP_OK) return r;
	return xrtRudpDatagramConfigure(s->Datagrams, channel);
}
xrudpresult xrtRudpSessionDatagramChannelState(const xrudpsession* s, uint32_t id, xrudpdatagramchannelstate* output)
{
	xrudpresult r = xrudp__session_datagram_access(s); if (r != XRUDP_OK) return r;
	return xrtRudpDatagramChannelState(s->Datagrams, id, output);
}
static xrudpresult xrudp__session_datagram_clock(xrudpsession* s, uint64_t now)
{
	xrudpresult r = xrudp__session_datagram_access(s); if (r != XRUDP_OK) return r;
	if (now < s->Now) return XRUDP_ARGUMENT;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	return xrtRudpDatagramMaintain(s->Datagrams, now);
}
xrudpresult xrtRudpSessionDatagramWrite(xrudpsession* s, uint32_t id, const void* data, size_t size,
	uint64_t now, uint64_t deadline, uint64_t* sequence)
{
	if (sequence == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__session_datagram_access(s); if (r != XRUDP_OK) return r;
	uint32_t maximum = s->Mtu.Effective;
	if (size > maximum - XRUDP_DATAGRAM_OVERHEAD) return XRUDP_LIMIT;
	r = xrudp__session_datagram_clock(s, now); if (r != XRUDP_OK) return r;
	r = xrtRudpDatagramWrite(s->Datagrams, id, data, size, now, deadline, sequence);
	xrudp__session_syncwork(s); return r;
}
xrudpresult xrtRudpSessionDatagramPeek(const xrudpsession* s, xrudpdatagraminfo* info)
{
	if (info != NULL) memset(info, 0, sizeof(*info));
	if (info == NULL) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__session_datagram_access(s); if (r != XRUDP_OK) return r;
	return xrtRudpDatagramPeek(s->Datagrams, info);
}
xrudpresult xrtRudpSessionDatagramRead(xrudpsession* s, void* output, size_t capacity, uint64_t now, xrudpdatagraminfo* info)
{
	if (info != NULL) memset(info, 0, sizeof(*info));
	if (info == NULL || (output == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	xrudpresult r = xrudp__session_datagram_clock(s, now); if (r != XRUDP_OK) return r;
	r = xrtRudpDatagramRead(s->Datagrams, output, capacity, now, info); xrudp__session_syncwork(s); return r;
}
static bool xrudp__session_limited(xrudpsession* s)
{
	uint64_t backlog = 0;
	if (s->Datagrams != NULL) {
		xrudpdatagramstate state; (void)xrtRudpDatagramState(s->Datagrams, &state);
		/* Sent game payloads have already left the queue. Include in-flight
		 * bytes while more game records wait; reliable flow credit cannot make
		 * this congestion demand application-limited. Empty records cost wire
		 * bytes too. All terms are bounded by queue/cwnd configuration. */
		if (state.SendQueued != 0) backlog = s->Cc.BytesInFlight + state.SendBytes +
			(uint64_t)state.SendQueued * XRUDP_DATAGRAM_OVERHEAD;
	}
	for (uint32_t i = 0; s->ConnectionSent < s->PeerLimit && i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
		xrudptransmitstate tx; (void)xrtRudpTransmitState(s->Streams[i].Tx, &tx);
		if (!tx.Cancelled && tx.HighestSent < tx.PeerLimit) backlog += tx.QueuedEnd - tx.BaseOffset;
	}
	return backlog < s->Cc.Window;
}
static xrudpresult xrudp__session_feedback(xrudpsession* s, size_t count, bool networkLoss)
{
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* event = &s->Events[i];
		if (event->UserTag == 0 || event->UserTag > s->Slots) return XRUDP_IO_ERROR;
		xrudp__sessionref* ref = &s->Ref[event->UserTag - 1];
		if (!ref->Used || ref->Number != event->PacketNumber) return XRUDP_IO_ERROR;
		xrudp__mtu_result(s, event);
		if (ref->Stream != XRUDP__NO_STREAM && ref->Stream >= s->StreamSlots) return XRUDP_IO_ERROR;
		if (ref->Stream == XRUDP__NO_STREAM && (ref->Size || ref->Fin || ref->Open || ref->Reset || ref->Stop || ref->StreamGrant)) return XRUDP_IO_ERROR;
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
			if (ref->Shutdown && !s->Shutdown.DeclarationAcknowledged) { s->Shutdown.DeclarationAcknowledged = true; s->Progress = s->Now; }
			if (ref->Drained && !s->Shutdown.DrainedAcknowledged) { s->Shutdown.DrainedAcknowledged = true; s->Progress = s->Now; }
			if (ref->Datagram) { if (s->DatagramAcknowledged != UINT64_MAX) ++s->DatagramAcknowledged; s->Progress = s->Now; }
			if (ref->ConnectionGrant > s->ConnectionAcked) { s->ConnectionAcked = ref->ConnectionGrant; s->Progress = s->Now; }
			if (ref->StreamsGrant > s->StreamsAcked) { s->StreamsAcked = ref->StreamsGrant; s->Progress = s->Now; }
			if (m != NULL) {
				if (ref->StreamGrant > m->GrantAcked) { m->GrantAcked = ref->StreamGrant; s->Progress = s->Now; }
				if (ref->Open && !m->OpenAcked) { m->OpenAcked = true; s->Progress = s->Now; }
				if (ref->Reset && !m->ResetAcked) {
					m->ResetAcked = true; m->ResetClockPending = false; m->ResetDeadline = 0; --s->ResetWaiters; s->Progress = s->Now;
				}
				if (ref->Stop && !m->StopComplete) { m->StopComplete = true; s->Progress = s->Now; }
			}
		} else if (event->Kind == XRUDP_SENT_LOST) {
			if ((ref->Shutdown && !s->Shutdown.DeclarationAcknowledged) || (ref->Drained && !s->Shutdown.DrainedAcknowledged)) s->ShutdownRetry = s->Now;
			if (ref->Datagram && s->DatagramLost != UINT64_MAX) ++s->DatagramLost;
			if (!(ref->Flags & XRUDP_SENT_MTU)) ++s->LostPackets;
			if (ref->ConnectionGrant || ref->StreamGrant) s->GrantRetry = s->Now;
			if (ref->StreamsGrant) s->StreamsRetry = s->Now;
			if (m != NULL && (ref->StreamGrant || ref->Open || ref->Reset)) m->RetryAt = s->Now;
			if (m != NULL && ref->Stop && !m->StopComplete) m->StopRetryAt = s->Now;
		}
		if (m != NULL) { --m->References; xrudp__stream_note(m); }
		memset(ref, 0, sizeof(*ref));
		if (!xrudp__loss_finish(&s->Loss, event, networkLoss)) return XRUDP_IO_ERROR;
	}
	uint64_t duration = UINT64_MAX;
	if (networkLoss && s->Rtt.HasSample) { (void)xrtRudpRttPto(&s->Rtt, 0, &duration); duration = xrudp__session_add(duration, xrudp__session_add(duration, duration)); }
	uint32_t periods = xrudp__loss_retire(&s->Loss, duration);
	if (periods) {
		if (xrtRudpCongestionCollapse(&s->Cc, s->Now) != XRUDP_OK) return XRUDP_IO_ERROR;
		s->PersistentCongestion = xrudp__session_add(s->PersistentCongestion, periods);
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
	return result == XRUDP_OK ? xrudp__session_feedback(s, before + count, true) : XRUDP_IO_ERROR;
}
static xrudpresult xrudp__mtu_maintain(xrudpsession* s)
{
	xrudpmtustate* m = &s->Mtu;
	if (!m->Enabled || s->Shutdown.Ready) return XRUDP_OK;
	if (m->ProbePacket && s->Now >= m->ProbeDeadlineUs) {
		xrudpresult r = xrtRudpSentDiscardPacket(s->Sent, m->ProbePacket, s->Now, s->Events);
		if (r != XRUDP_OK || xrtRudpCongestionApply(&s->Cc, s->Events, 1, s->Now, true) != XRUDP_OK || xrudp__session_feedback(s, 1, false) != XRUDP_OK) return XRUDP_IO_ERROR;
	}
	if (m->Failure != XRUDP_OK && !s->Shutdown.Requested && !s->Shutdown.PeerRequested) return m->Failure;
	if (m->LargeDeadlineUs && s->Now >= m->LargeDeadlineUs) {
		bool reset = m->Effective > 512 || m->BaseConfirmed;
		size_t count; xrudpresult r = xrtRudpSentDiscardPath(s->Sent, s->Now, s->Events, s->Slots, &count);
		if ((r != XRUDP_OK && r != XRUDP_DONE) || xrtRudpCongestionApply(&s->Cc, s->Events, count, s->Now, true) != XRUDP_OK || xrudp__session_feedback(s, count, false) != XRUDP_OK) return XRUDP_IO_ERROR;
		if (reset) { xrudp__mtu_reset(s); m->NextProbeUs = s->Now; m->RaiseAfterUs = xrudp__session_add(s->Now, 60000000); }
		/* While rechecking BASE, discarding stalled ordinary packets may free
		 * the only normal slot, but must NEVER restart the admitted probe count. */
		m->LargePacket = m->LargeDeadlineUs = 0; m->LargeSize = 0;
		m->Fallbacks = xrudp__session_add(m->Fallbacks, 1);
		if (s->Datagrams != NULL && !s->Shutdown.Requested) {
			uint32_t dropped; if (xrtRudpDatagramTrimSend(s->Datagrams, m->Effective - XRUDP_DATAGRAM_OVERHEAD, &dropped) != XRUDP_OK) return XRUDP_IO_ERROR;
		}
		s->PtoAt = 0; s->PtoCount = s->Probes = 0;
	}
	return XRUDP_OK;
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
	uint32_t Index, Id, ResetReason, StopReason;
	uint16_t Weight;
	uint8_t Priority, Kind;
	bool New, HasFinal, HasReset, HasOpen, HasStop;
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
static xrudpresult xrudp__session_input(xrudpsession* s, const xrudpaddress* source, const void* packet, size_t size, uint64_t now)
{
	if (s == NULL || s->Busy || packet == NULL || now < s->Now) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	if (xrudp__shutdown_complete(s)) return XRUDP_CLOSED;
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
	xrudpshutdownstate shutdown = s->Shutdown; bool peerDrained = false;
	const unsigned char* challenge = NULL;
	for (size_t i = 0; i < count; ++i) if (frames[i].Type == XRUDP_FRAME_SHUTDOWN) {
		uint32_t final = xrudp__session_get32(frames[i].Data), timeout = xrudp__session_get32(frames[i].Data + 4);
		if (final < s->IncomingNext - 1 || final > s->IncomingLimit ||
			(shutdown.PeerRequested && (final != shutdown.PeerFinalOrdinal || timeout != shutdown.PeerTimeoutMs))) goto malformed;
		shutdown.PeerRequested = true; shutdown.PeerFinalOrdinal = final; shutdown.PeerTimeoutMs = timeout;
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
		if (shutdown.PeerRequested && plan->New && plan->Id / 2 + 1 > shutdown.PeerFinalOrdinal) goto malformed;
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
	bool eliciting = false, closing = false, hasDatagrams = false;
	for (size_t i = 0; i < count; ++i) {
		const xrudpwireframe* f = &frames[i]; xrudp__streamplan* plan = NULL;
		if (f->Type != XRUDP_FRAME_ACK && f->Type != XRUDP_FRAME_PADDING && f->Type != XRUDP_FRAME_KEY_CONFIRM) eliciting = true;
		if (f->Type == XRUDP_FRAME_STREAM || f->Type == XRUDP_FRAME_RESET || f->Type == XRUDP_FRAME_MAX_STREAM_DATA || f->Type == XRUDP_FRAME_STOP_SENDING) {
			result = xrudp__plan_get(s, plans, &planCount, xrudp__session_get32(f->Data), false, &plan);
			if (result == XRUDP_BACKPRESSURE) goto deferred;
			if (result != XRUDP_OK) goto malformed;
			if (plan == NULL) continue; /* Late contents cannot reopen a retired ID or grant connection credit. */
		}
		switch (f->Type) {
		case XRUDP_FRAME_PADDING: case XRUDP_FRAME_PING: case XRUDP_FRAME_KEY_CONFIRM: case XRUDP_FRAME_OPEN: break;
		case XRUDP_FRAME_DATAGRAM: hasDatagrams = true; break;
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
		case XRUDP_FRAME_STOP_SENDING: {
			uint32_t reason = xrudp__session_get32(f->Data + 4);
			if ((plan->HasStop && plan->StopReason != reason) ||
				(plan->Stream->PeerStopped && plan->Stream->PeerStopReason != reason)) goto malformed;
			plan->HasStop = true; plan->StopReason = reason; break;
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
		case XRUDP_FRAME_SHUTDOWN: break;
		case XRUDP_FRAME_PATH_CHALLENGE:
			if (!s->Path.State.Enabled || source == NULL || (challenge != NULL && memcmp(challenge, f->Data, 16) != 0)) goto malformed;
			challenge = f->Data; break;
		case XRUDP_FRAME_PATH_RESPONSE:
			if (!s->Path.State.Enabled || source == NULL) goto malformed;
			break;
		case XRUDP_FRAME_DRAINED:
			if (!shutdown.Requested || !shutdown.PeerRequested || s->IncomingNext + opened - 1 != shutdown.PeerFinalOrdinal) goto malformed;
			peerDrained = true; break;
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
	if (challenge != NULL && !s->Shutdown.Ready) xrudp__path_echo(&s->Path, source, challenge);
	if (shutdown.PeerRequested && !s->Shutdown.PeerRequested) {
		s->Shutdown.PeerRequested = true; s->Shutdown.PeerFinalOrdinal = shutdown.PeerFinalOrdinal;
		s->Shutdown.PeerTimeoutMs = shutdown.PeerTimeoutMs;
		s->ShutdownPeerDeadline = xrudp__session_add(now, (uint64_t)shutdown.PeerTimeoutMs * 1000);
		if (!s->Shutdown.DeadlineUs || s->ShutdownPeerDeadline < s->Shutdown.DeadlineUs) s->Shutdown.DeadlineUs = s->ShutdownPeerDeadline;
		s->Progress = now;
	}
	if (peerDrained) {
		if (!s->Shutdown.PeerDrained) s->Progress = now;
		s->Shutdown.PeerDrained = true;
		if (parsed.Header.PacketNumber > s->PeerDrainedPacket) s->PeerDrainedPacket = parsed.Header.PacketNumber;
	}
	for (size_t i = 0; i < planCount; ++i) if (plans[i].New) {
		xrudp__streamplan* p = &plans[i];
		xrudp__stream_activate(s, p->Index, p->Id, false, p->Credit, p->Priority, p->Weight, p->Kind);
		if (s->Shutdown.Requested) (void)xrtRudpTransmitFinish(p->Stream->Tx);
		p->Stream->Events |= XRUDP_STREAM_OPENED;
	}
	s->IncomingNext += (uint32_t)opened; if (opened) s->Progress = now;
	if (streamCount) {
		xrudpreceivebatchchange change;
		if (!xrudp__session_good(xrtRudpReceivePushBatch(streams, streamCount, s->ConnectionLimit - s->ConnectionReceived, &change))) goto internal;
		if (change.NewBytes || change.NewFinals) s->Progress = now;
		if (change.ConsumedDelta) { xrudp__session_credit(s, change.ConsumedDelta); s->Progress = now; }
	}
	s->ConnectionReceived += charge;
	for (size_t i = 0; i < planCount; ++i) {
		xrudp__streamplan* p = &plans[i];
		if (p->HasReset) {
			xrudpreceivechange change;
			if (!xrudp__session_good(xrtRudpReceiveReset(p->Stream->Rx, p->ResetFinal, p->ResetReason, &change))) goto internal;
			if (change.ConsumedDelta) { s->Progress = now; xrudp__session_consumed(s, p->Index, change.ConsumedDelta); }
		}
		xrudpreceivestate rx; (void)xrtRudpReceiveState(p->Stream->Rx, &rx);
		if (p->Stream->StopQueued && rx.HasFinal) {
			p->Stream->StopComplete = true;
			if (p->Stream->StopClockPending || p->Stream->StopDeadline) {
				p->Stream->StopClockPending = false; p->Stream->StopDeadline = 0; --s->StopWaiters;
			}
		}
		if (p->HasStop && !p->Stream->PeerStopped) {
			p->Stream->PeerStopped = true; p->Stream->PeerStopReason = p->StopReason;
			p->Stream->Events |= XRUDP_STREAM_WRITE_STOPPED; s->Progress = now;
			if (!p->Stream->ResetQueued && !xrudp__stream_done(p->Stream)) xrudp__stream_reset_send(s, p->Stream, p->StopReason);
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
		xrudp__loss_ack(&s->Loss, &ack);
		eventCount += events; if (change.NewlyAckedEliciting) progress = true;
		if (change.RttSampleValid && (!sample.RttSampleValid || change.LargestAcked > sample.LargestAcked)) sample = change;
	}
	if (sample.RttSampleValid) (void)xrtRudpRttSample(&s->Rtt, sample.RttSampleUs, sample.PeerAckDelayUs);
	if (xrudp__session_losses(s, eventCount) != XRUDP_OK) goto internal;
	xrudpdatagramstate datagramsBefore;
	if (hasDatagrams && s->Datagrams != NULL && !s->Shutdown.Requested) (void)xrtRudpDatagramState(s->Datagrams, &datagramsBefore);
	for (size_t i = 0; i < count; ++i) if (frames[i].Type == XRUDP_FRAME_DATAGRAM) {
		if (s->Datagrams != NULL && !s->Shutdown.Requested) {
			if (xrtRudpDatagramInput(s->Datagrams, xrudp__session_get32(frames[i].Data),
				(frames[i].Flags & XRUDP_DATAGRAM_SEQUENCED) != 0, xrudp__session_get64(frames[i].Data + 4),
				frames[i].Data + 12, frames[i].Size - 12, now) != XRUDP_OK) goto internal;
		} else if (s->DatagramDisabledDropped != UINT64_MAX) ++s->DatagramDisabledDropped;
	}
	if (hasDatagrams && s->Datagrams != NULL && !s->Shutdown.Requested) {
		xrudpdatagramstate after; (void)xrtRudpDatagramState(s->Datagrams, &after); if (after.Accepted != datagramsBefore.Accepted) s->Progress = now;
	}
	xrudp__session_epoch(s, previous.ReceiveEpoch);
	result = xrtRudpAckSetRecord(&s->Ack, parsed.Header.PacketNumber, now);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE && result != XRUDP_TOO_OLD) goto internal;
	if (eliciting) {
		s->Eliciting[parsed.Header.PacketNumber % XRUDP_REPLAY_BITS] = parsed.Header.PacketNumber;
		if (s->ElicitingSinceAck < 2) ++s->ElicitingSinceAck;
		xrudp__session_ackwant(s, s->ElicitingSinceAck >= 2);
	}
	s->Activity = now; ++s->ReceivedPackets; xrudp__session_pto_sync(s, progress); xrudp__session_syncwork(s);
	xrudp__shutdown_update(s);
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

xrudpresult xrtRudpSessionInput(xrudpsession* s, const void* packet, size_t size, uint64_t now)
{
	if (s != NULL && s->Path.State.Enabled) return XRUDP_ARGUMENT;
	return xrudp__session_input(s, NULL, packet, size, now);
}
xrudpresult xrtRudpSessionEnablePaths(xrudpsession* s, const xrudpaddress* address, xrudppathsendproc send)
{
	if (s == NULL || s->Busy || !xrudp__path_address(address) || send == NULL) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	if (s->Path.State.Enabled || s->ReceivedPackets || s->SentPackets || s->Shutdown.Requested) return XRUDP_ARGUMENT;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher);
	if (cipher.NextPacketNumber != 2 || cipher.LargestReceivedPacket != 1) return XRUDP_ARGUMENT;
	s->Path.State.Enabled = true; s->Path.State.Active = *address; s->Path.Send = send; s->Path.ActiveNumber = 1;
	s->Path.State.NextProbeUs = UINT64_MAX; return XRUDP_OK;
}
xrudpresult xrtRudpSessionProbePath(xrudpsession* s, const xrudpaddress* address, uint64_t now)
{
	if (s == NULL || s->Busy || !xrudp__path_address(address) || now < s->Now || !s->Path.State.Enabled) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	if (s->Shutdown.Requested) return XRUDP_CLOSED;
	xrudp__session_clock(s, now);
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	return xrudp__path_start(&s->Path, address, now, true);
}
static xrudpresult xrudp__session_migrate(xrudpsession* s)
{
	size_t count; xrudpresult r = xrtRudpSentDiscardPath(s->Sent, s->Now, s->Events, s->Slots, &count);
	if (r != XRUDP_OK && r != XRUDP_DONE) return XRUDP_IO_ERROR;
	if (xrtRudpCongestionApply(&s->Cc, s->Events, count, s->Now, true) != XRUDP_OK || xrudp__session_feedback(s, count, false) != XRUDP_OK) return XRUDP_IO_ERROR;
	xrudpsentstate sent; (void)xrtRudpSentState(s->Sent, &sent);
	(void)xrtRudpCongestionInit(&s->Cc, s->Config.MaximumWindow); s->Cc.LargestSent = sent.LargestSent; s->Cc.LastNowUs = s->Now;
	(void)xrtRudpRttInit(&s->Rtt, s->Config.InitialRttUs, s->Config.Peer.MaxAckDelayUs);
	xrudp__loss_restart(&s->Loss);
	s->PtoAt = 0; s->PtoCount = s->Probes = 0; s->GrantRetry = s->StreamsRetry = s->Now; s->Progress = s->Now;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher);
	xrudp__path_promote(&s->Path, cipher.LargestReceivedPacket);
	xrudp__mtu_reset(s);
	if (s->Datagrams != NULL) { uint32_t count; if (xrtRudpDatagramTrimSend(s->Datagrams, s->Mtu.Effective - XRUDP_DATAGRAM_OVERHEAD, &count) != XRUDP_OK && !s->Shutdown.Requested) return XRUDP_IO_ERROR; }
	xrudp__session_ackwant(s, true); return XRUDP_OK;
}
xrudpresult xrtRudpSessionPathInput(xrudpsession* s, const xrudpaddress* address, const void* packet, size_t size, uint64_t now)
{
	if (s == NULL || s->Busy || !xrudp__path_address(address) || packet == NULL || now < s->Now || !s->Path.State.Enabled) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__shutdown_complete(s)) return XRUDP_CLOSED;
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	if (xrudp__path_equal(address, &s->Path.State.Active)) {
		xrudpresult r = xrudp__session_input(s, address, packet, size, now);
		/* Deferred frames still authenticated this active source and committed
		 * its crypto packet number. They cannot be ACKed/delivered yet, but a
		 * lower-numbered passive proof must not override that newer evidence. */
		if (r == XRUDP_OK || r == XRUDP_BACKPRESSURE) { xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher); s->Path.ActiveNumber = cipher.LargestReceivedPacket; }
		return r;
	}
	xrudpwirepacket parsed;
	if (s->Shutdown.Ready) return XRUDP_BACKPRESSURE;
	if (xrtRudpPacketDecode(packet, size, &parsed) != XRUDP_OK) {
		if (s->Path.State.Invalid != UINT64_MAX) ++s->Path.State.Invalid;
		return XRUDP_AUTH_FAILED;
	}
	if (!xrudp__path_inspect(&s->Path, address, parsed.Header.Type == XRUDP_PACKET_PATH, now)) return XRUDP_BACKPRESSURE;
	if (size > s->Config.Local.DatagramSize) { if (s->Path.State.Invalid != UINT64_MAX) ++s->Path.State.Invalid; return XRUDP_BACKPRESSURE; }
	unsigned char plain[XRUDP_PACKET_MAX]; size_t length;
	xrudpresult r = xrtRudpCipherInspect(s->Cipher, packet, size, plain, sizeof(plain), &length);
	if (r != XRUDP_OK) {
		xrtSecureZero(plain, sizeof(plain));
		if (s->Path.State.Invalid != UINT64_MAX) ++s->Path.State.Invalid;
		return r == XRUDP_AUTH_FAILED ? r : XRUDP_BACKPRESSURE;
	}
	bool proof = false, only = true; const unsigned char* challenge = NULL; size_t offset = 0; xrudpwireframe frame;
	while (xrtRudpFrameNext(plain, length, &offset, &frame) == XRUDP_OK) {
		if (frame.Type == XRUDP_FRAME_PATH_RESPONSE && s->Path.State.Validating && s->Path.State.Attempts &&
			xrudp__path_equal(address, &s->Path.State.Candidate) && xrtConstTimeEqual(frame.Data, s->Path.Token, 16)) proof = true;
		else if (frame.Type == XRUDP_FRAME_PATH_CHALLENGE) {
			if (challenge != NULL && memcmp(challenge, frame.Data, 16) != 0) only = false;
			challenge = frame.Data;
		} else if (frame.Type != XRUDP_FRAME_PATH_RESPONSE && frame.Type != XRUDP_FRAME_PADDING && frame.Type != XRUDP_FRAME_KEY_CONFIRM) only = false;
	}
	/* Passive selection follows the newest active-path evidence, not a delayed
	 * matching response. A trusted caller explicitly selecting a reachable
	 * alternative is allowed to prefer it while the current path still works. */
	if (proof && only && (s->Path.Trusted || parsed.Header.PacketNumber > s->Path.ActiveNumber)) {
		r = xrudp__session_input(s, address, packet, size, now);
		if (r == XRUDP_OK) {
			r = xrudp__session_migrate(s);
			if (r != XRUDP_OK) (void)xrudp__session_abort(s, XRUDP_IO_ERROR);
		}
	} else {
		bool fresh = parsed.Header.PacketNumber > s->Path.ActiveNumber && parsed.Header.PacketNumber > s->Path.UnvalidatedNumber;
		if (!s->Path.State.Validating && fresh) {
			xrudpresult start = xrudp__path_start(&s->Path, address, now, false);
			/* An authenticated trigger is spent even if the random source fails;
			 * the same captured packet cannot reopen attempts after cooldown. */
			if (start != XRUDP_OK) s->Path.UnvalidatedNumber = parsed.Header.PacketNumber;
		}
		if (s->Path.State.Validating && xrudp__path_equal(address, &s->Path.State.Candidate)) {
			xrudp__path_credit(&s->Path, parsed.Header.PacketNumber, (uint32_t)size);
			if (fresh && only && challenge != NULL) xrudp__path_echo(&s->Path, address, challenge);
		}
		r = XRUDP_BACKPRESSURE;
	}
	xrtSecureZero(plain, sizeof(plain)); return r;
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
	if (s->Shutdown.Ready) return false;
	xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
	bool grant = rx.Limit > m->GrantAcked && !(s->Shutdown.Requested && xrudp__stream_done(m) && xrudp__stream_read_done(m));
	return ((probe || s->Now >= m->RetryAt) &&
		(!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || grant)) ||
		(m->StopQueued && !m->StopComplete && (probe || s->Now >= m->StopRetryAt));
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
	if (s->Shutdown.Ready) return XRUDP__NO_STREAM;
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
/* One admission/nonce/ledger path for reliable and unreliable payloads. Caller
 * prechecks congestion/capacity; no logical queue is consumed until OK. */
static xrudpresult xrudp__session_emit(xrudpsession* s, const unsigned char* plain, size_t used,
	uint32_t flags, unsigned char* packet, uint64_t* due, size_t* size, const xrudpaddress* destination, bool* routeError, xrudp__sessionref** output)
{
	if (routeError != NULL) *routeError = false;
	size_t slot; for (slot = 0; slot < s->Slots && s->Ref[slot].Used; ++slot) {}
	if (slot == s->Slots) return XRUDP_IO_ERROR;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher);
	if (cipher.PacketsInSendEpoch >= s->Config.KeyUpdatePackets && !cipher.SendKeyPending) {
		if (xrtRudpCipherUpdateKey(s->Cipher) != XRUDP_OK) return XRUDP_IO_ERROR;
		++s->KeyUpdates;
	}
	xrudpresult result = xrtRudpCipherSeal(s->Cipher, plain, used, packet, XRUDP_PACKET_MAX, size);
	if (result != XRUDP_OK) return result == XRUDP_LIMIT ? XRUDP_LIMIT : XRUDP_IO_ERROR;
	result = s->Path.State.Enabled ? s->Path.Send(packet, *size, destination != NULL ? destination : &s->Path.State.Active, s->Context) : s->Send(packet, *size, s->Context);
	if (result == XRUDP_BACKPRESSURE || result == XRUDP_NO_MEMORY) {
		++s->OutputBackpressure; uint64_t at = xrudp__session_add(s->Now, 1000); if (at < *due) *due = at;
		return XRUDP_BACKPRESSURE;
	}
	if (result != XRUDP_OK) { if (routeError != NULL) *routeError = true; return XRUDP_IO_ERROR; }
	(void)xrtRudpCipherState(s->Cipher, &cipher); uint64_t number = cipher.NextPacketNumber - 1;
	if (xrtRudpSentRecord(s->Sent, number, s->Now, (uint32_t)*size, flags, slot + 1) != XRUDP_OK ||
		xrtRudpCongestionOnSent(&s->Cc, number, s->Now, s->Rtt.SmoothedUs, (uint32_t)*size, flags) != XRUDP_OK) return XRUDP_IO_ERROR;
	if (!xrudp__loss_record(&s->Loss, number, s->Now, flags, s->Rtt.HasSample)) return XRUDP_IO_ERROR;
	xrudp__sessionref* ref = &s->Ref[slot]; memset(ref, 0, sizeof(*ref)); ref->Used = true;
	ref->Number = number; ref->At = s->Now; ref->Flags = flags; ref->Stream = XRUDP__NO_STREAM; *output = ref;
	if (s->Mtu.Enabled && !(flags & XRUDP_SENT_MTU) && (flags & XRUDP_SENT_ACK_ELICITING) && *size >= 512) {
		if (!s->Mtu.LargeDeadlineUs) {
			s->Mtu.LargeDeadlineUs = xrudp__session_add(s->Now, xrudp__mtu_interval(s));
			s->Mtu.LargeSize = (uint32_t)*size; s->Mtu.LargePacket = number;
		} else if (*size > s->Mtu.LargeSize) { s->Mtu.LargeSize = (uint32_t)*size; s->Mtu.LargePacket = number; }
	}
	return XRUDP_OK;
}
static xrudpresult xrudp__session_send_inner(xrudpsession* s, uint64_t* due, bool ackOnly,
	unsigned char* plain, unsigned char* body, unsigned char* packet)
{
	size_t used = 0, ackSize = 0, size;
	uint32_t maximum = s->Mtu.Effective;
	size_t limit = maximum - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher);
	bool probe = s->Probes != 0;
	if (s->Shutdown.LingerUntilUs) ackOnly = true;
	uint32_t selected = ackOnly ? XRUDP__NO_STREAM : xrudp__session_select(s, probe);
	xrudp__sessionstream* m = selected != XRUDP__NO_STREAM ? &s->Streams[selected] : NULL;
	xrudpreceivestate rx; memset(&rx, 0, sizeof(rx)); if (m) (void)xrtRudpReceiveState(m->Rx, &rx);
	bool open = m && m->Local && m->Id != 1 && !m->OpenAcked;
	bool reset = m && m->ResetQueued && !m->ResetAcked && (probe || !m->ResetSent || s->Now >= m->RetryAt);
	bool stop = m && m->StopQueued && !m->StopComplete && (probe || !m->StopSent || s->Now >= m->StopRetryAt);
	bool streamGrant = m && rx.Limit > m->GrantAcked && (probe || s->Now >= m->RetryAt) &&
		!(s->Shutdown.Requested && xrudp__stream_done(m) && xrudp__stream_read_done(m));
	bool grant = !ackOnly && !s->Shutdown.Ready && s->ConnectionLimit > s->ConnectionAcked && s->Now >= s->GrantRetry;
	bool streamsGrant = !ackOnly && !s->Shutdown.Ready && s->IncomingLimit > s->StreamsAcked && s->Now >= s->StreamsRetry;
	bool closeTurn = !ackOnly && (probe || s->Now >= s->ShutdownRetry);
	bool drained = closeTurn && s->Shutdown.Ready && !s->Shutdown.DrainedAcknowledged;
	bool shutdown = !ackOnly && s->Shutdown.Requested && !s->Shutdown.DeclarationAcknowledged && (closeTurn || drained);
	/* Make a final ACK recoverable even with a one-slot normal ledger. A
	 * bounded PING elicits its ACK; true PTO may repeat ACK+PING using the
	 * existing two-probe reserve. No reliable data/control is revived in linger. */
	bool finalAck = s->Shutdown.PeerDrained && ((!s->Shutdown.PeerDrainedAcknowledged && s->AckWanted) || (probe && s->Shutdown.LingerUntilUs));
	bool keepalive = !ackOnly && s->Now >= xrudp__session_keepalive(s);
	bool data = false, eliciting = open || reset || stop || streamGrant || grant || streamsGrant || shutdown || drained || finalAck || probe || keepalive || (!ackOnly && s->ConfirmWanted);
	uint32_t quantum = m ? XRUDP_PACKET_MAX * (m->Weight < 16 ? m->Weight : 16u) * (4u - m->Priority) : 0;
	uint32_t deficit = m && m->Deficit ? m->Deficit : quantum;
	if (s->AckWanted || finalAck) {
		/* Pure ACKs cannot elicit their own delivery proof. Keep their portion
		 * within the safe base budget even after a previously larger path
		 * shrinks, including fallback-to-ACK and final drain ACK+PING packets.
		 * Reliable/game payload still uses the current effective packet size. */
		size_t ackLimit = 512u - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE;
		if (ackLimit > limit) ackLimit = limit;
		xrudpackset ack = s->Ack; size_t ranges = (ackLimit - 12 - 128) / 16;
		if (ack.Count > ranges) ack.Count = (uint16_t)ranges;
		if (xrtRudpAckSetEncode(&ack, s->Now, plain, limit, &used) != XRUDP_OK) return XRUDP_IO_ERROR;
		ackSize = used;
	}
	if (grant) {
		xrudp__session_put64(body, s->ConnectionLimit);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_MAX_DATA, 0, body, 8)) return XRUDP_IO_ERROR;
	}
	if (shutdown) {
		xrudp__session_put32(body, s->Shutdown.LocalFinalOrdinal); xrudp__session_put32(body + 4, s->Shutdown.LocalTimeoutMs);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_SHUTDOWN, 0, body, 8)) return XRUDP_IO_ERROR;
	}
	if (drained && !xrudp__session_append(plain, limit, &used, XRUDP_FRAME_DRAINED, 0, NULL, 0)) return XRUDP_IO_ERROR;
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
	if (stop) {
		xrudp__session_put32(body, m->Id); xrudp__session_put32(body + 4, rx.StopReason);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_STOP_SENDING, 0, body, 8)) return XRUDP_IO_ERROR;
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
	if (eliciting && !data && !open && !reset && !stop && !streamGrant && !grant && !streamsGrant && !shutdown && !drained)
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_PING, 0, NULL, 0)) return XRUDP_IO_ERROR;
	if (used == 0) return XRUDP_DONE;
	if (eliciting) {
		xrudp__session_put32(body, cipher.ReceiveEpoch);
		if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4)) return XRUDP_IO_ERROR;
	}
	uint32_t flags = eliciting ? XRUDP_SENT_ACK_ELICITING : XRUDP_SENT_ACK_ONLY;
	if (probe) flags |= XRUDP_SENT_PROBE;
	uint64_t at = s->Now;
	xrudpresult admission = xrtRudpCongestionCanSend(&s->Cc, s->Now, s->Rtt.SmoothedUs,
		(uint32_t)(used + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE), flags, &at);
	if (!(eliciting ? xrtRudpSentCanRecord(s->Sent, probe) : xrtRudpSentCanRecordAck(s->Sent))) admission = XRUDP_BACKPRESSURE;
	if (admission != XRUDP_OK) {
		if (admission == XRUDP_TIMEOUT && at < *due) *due = at;
		if (finalAck) return XRUDP_DONE; /* Keep its recovery PING, never degrade this proof to ACK-only. */
		if (ackSize == 0 || !xrtRudpSentCanRecordAck(s->Sent)) return XRUDP_DONE;
		used = ackSize; data = grant = streamsGrant = streamGrant = open = reset = stop = shutdown = drained = probe = eliciting = false; flags = XRUDP_SENT_ACK_ONLY;
	}
	if (!eliciting) {
		at = s->AckAt > s->ControlAt ? s->AckAt : s->ControlAt;
		if (s->Now < at) { if (at < *due) *due = at; return XRUDP_DONE; }
	}
	xrudp__sessionref* ref; xrudpresult result = xrudp__session_emit(s, plain, used, flags, packet, due, &size, NULL, NULL, &ref);
	if (result == XRUDP_BACKPRESSURE) return XRUDP_DONE;
	if (result != XRUDP_OK) return result;
	if (data || open || reset || stop || streamGrant) { ref->Stream = selected; ++m->References; }
	if (data) {
		xrudptransmitchange change;
		if (xrtRudpTransmitMarkSent(m->Tx, frame.Offset, frame.Size, frame.Fin != 0, &change) != XRUDP_OK) return XRUDP_IO_ERROR;
		s->ConnectionSent += change.HighestDelta; ref->Offset = frame.Offset; ref->Size = frame.Size; ref->Fin = frame.Fin != 0;
	}
	uint64_t interval; (void)xrtRudpRttPto(&s->Rtt, 0, &interval);
	if (shutdown || drained) { ref->Shutdown = shutdown; ref->Drained = drained; s->ShutdownRetry = xrudp__session_add(s->Now, interval); }
	if (open) { ref->Open = true; m->OpenSent = true; }
	if (reset) { ref->Reset = true; m->ResetSent = true; }
	if (stop) { ref->Stop = true; m->StopSent = true; m->StopRetryAt = xrudp__session_add(s->Now, interval); }
	if (streamGrant) ref->StreamGrant = rx.Limit;
	if (open || reset || streamGrant) m->RetryAt = xrudp__session_add(s->Now, interval);
	if (grant) { ref->ConnectionGrant = s->ConnectionLimit; s->GrantRetry = xrudp__session_add(s->Now, interval); }
	if (streamsGrant) { ref->StreamsGrant = s->IncomingLimit; s->StreamsRetry = xrudp__session_add(s->Now, interval); }
	if (ref->Stream != XRUDP__NO_STREAM) {
		m->Deficit = deficit > size ? deficit - (uint32_t)size : 0;
		s->ScheduleCursor = m->Deficit ? selected : (selected + 1) % s->StreamSlots;
		xrudp__stream_note(m);
	}
	if (ackSize) {
		/* Only the ranges ACTUALLY encoded and admitted to UDP prove a final ACK.
		 * The full local ACK history may contain older, truncated-away ranges. */
		if (s->Shutdown.PeerDrained && !s->Shutdown.PeerDrainedAcknowledged) {
			xrudpack ack; if (xrtRudpAckDecode(plain, ackSize, &ack) != XRUDP_OK) return XRUDP_IO_ERROR;
			for (uint16_t i = 0; i < ack.Count; ++i) if (ack.Ranges[i].First <= s->PeerDrainedPacket && s->PeerDrainedPacket <= ack.Ranges[i].Last)
				s->Shutdown.PeerDrainedAcknowledged = true;
		}
		s->AckWanted = false; s->AckAt = UINT64_MAX; s->ElicitingSinceAck = 0;
	}
	if (probe) --s->Probes;
	if (!eliciting) s->ControlAt = xrudp__session_add(s->Now, (1000000u + s->Config.ControlPerSecond - 1) / s->Config.ControlPerSecond);
	if (eliciting) { s->ConfirmWanted = false; s->PreferDatagram = true; s->KeepAliveSent = s->Now; }
	++s->SentPackets; xrudp__session_pto_sync(s, false); return XRUDP_OK;
}

static xrudpresult xrudp__session_send_datagram(xrudpsession* s, uint64_t* due,
	unsigned char* plain, unsigned char* body, unsigned char* packet)
{
	xrudpdatagramview view; if (s->Probes != 0 || s->Datagrams == NULL || xrtRudpDatagramPeekSend(s->Datagrams, &view) != XRUDP_OK) return XRUDP_DONE;
	uint32_t maximum = s->Mtu.Effective;
	size_t limit = maximum - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE, used = 0, ackSize = 0;
	size_t needed = view.Info.Size + 16u + 8u;
	if (needed > limit) return XRUDP_IO_ERROR; /* Write already enforced the effective payload bound. */
	if (s->AckWanted && limit - needed >= 28) {
		xrudpackset ack = s->Ack; size_t ranges = (limit - needed - 12) / 16;
		if (ack.Count > ranges) ack.Count = (uint16_t)ranges;
		if (xrtRudpAckSetEncode(&ack, s->Now, plain, limit - needed, &used) != XRUDP_OK) return XRUDP_IO_ERROR;
		ackSize = used;
	}
	xrudp__session_put32(body, view.Info.Channel); xrudp__session_put64(body + 4, view.Info.Sequence);
	if (view.Info.Size) memcpy(body + 12, view.Data, view.Info.Size);
	if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_DATAGRAM, view.Info.Sequenced ? XRUDP_DATAGRAM_SEQUENCED : 0, body, view.Info.Size + 12)) return XRUDP_IO_ERROR;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher); xrudp__session_put32(body, cipher.ReceiveEpoch);
	if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_KEY_CONFIRM, 0, body, 4)) return XRUDP_IO_ERROR;
	uint64_t at = s->Now; xrudpresult admission = xrtRudpCongestionCanSend(&s->Cc, s->Now, s->Rtt.SmoothedUs,
		(uint32_t)(used + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE), XRUDP_SENT_ACK_ELICITING, &at);
	if (admission != XRUDP_OK || !xrtRudpSentCanRecord(s->Sent, false)) {
		if (admission == XRUDP_TIMEOUT && at < *due) *due = at;
		return XRUDP_DONE;
	}
	size_t size; xrudp__sessionref* ref; xrudpresult r = xrudp__session_emit(s, plain, used, XRUDP_SENT_ACK_ELICITING, packet, due, &size, NULL, NULL, &ref);
	if (r == XRUDP_BACKPRESSURE) return XRUDP_DONE;
	if (r != XRUDP_OK) return r;
	ref->Datagram = true;
	if (xrtRudpDatagramSent(s->Datagrams, view.Info.Ticket, (uint32_t)size) != XRUDP_OK) return XRUDP_IO_ERROR;
	if (ackSize) { s->AckWanted = false; s->AckAt = UINT64_MAX; s->ElicitingSinceAck = 0; }
	s->ConfirmWanted = false; s->PreferDatagram = false;
	++s->SentPackets; xrudp__session_pto_sync(s, false); xrudp__session_syncwork(s); return XRUDP_OK;
}

static xrudpresult xrudp__session_send_path(xrudpsession* s, uint64_t* due)
{
	xrudp__path* p = &s->Path;
	if (!p->State.Enabled || s->Shutdown.Ready || s->Now < p->ResponseAt) return XRUDP_DONE;
	bool echo = false, challenge = false; uint32_t index = 0;
	if (p->Echo[0].Pending) echo = true;
	else if (p->State.Validating) {
		index = 1; echo = p->Echo[1].Pending;
		challenge = p->State.Attempts < 3 && s->Now >= p->State.NextProbeUs;
	}
	if (!echo && !challenge) return XRUDP_DONE;
	const xrudpaddress* address = index ? &p->State.Candidate : &p->State.Active;
	unsigned char plain[64], packet[XRUDP_PACKET_MAX], epoch[4]; size_t used = 0, size;
	if (echo && !xrudp__session_append(plain, sizeof(plain), &used, XRUDP_FRAME_PATH_RESPONSE, 0, p->Echo[index].Token, 16)) return XRUDP_IO_ERROR;
	if (challenge && !xrudp__session_append(plain, sizeof(plain), &used, XRUDP_FRAME_PATH_CHALLENGE, 0, p->Token, 16)) return XRUDP_IO_ERROR;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher); xrudp__session_put32(epoch, cipher.ReceiveEpoch);
	(void)xrudp__session_append(plain, sizeof(plain), &used, XRUDP_FRAME_KEY_CONFIRM, 0, epoch, 4);
	uint32_t bytes = (uint32_t)(used + XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE);
	uint32_t allowance = p->InitialAllowance + 3u * p->State.CandidateReceived; if (allowance > 3600) allowance = 3600;
	if (index && bytes > allowance - p->State.CandidateSent) { xrtSecureZero(plain, sizeof(plain)); return XRUDP_DONE; }
	uint32_t flags = XRUDP_SENT_ACK_ELICITING | (s->Probes ? XRUDP_SENT_PROBE : 0); uint64_t at = s->Now;
	xrudpresult result = xrtRudpCongestionCanSend(&s->Cc, s->Now, s->Rtt.SmoothedUs, bytes, flags, &at);
	if (result != XRUDP_OK || !xrtRudpSentCanRecord(s->Sent, s->Probes != 0)) {
		if (result == XRUDP_TIMEOUT && at < *due) *due = at;
		xrtSecureZero(plain, sizeof(plain)); return XRUDP_DONE;
	}
	bool routeError; xrudp__sessionref* ref; result = xrudp__session_emit(s, plain, used, flags, packet, due, &size, address, &routeError, &ref);
	xrtSecureZero(plain, sizeof(plain)); xrtSecureZero(packet, sizeof(packet));
	if (result == XRUDP_BACKPRESSURE) return XRUDP_DONE;
	/* Only a rejected UDP route to the unvalidated candidate is local to this
	 * attempt. Cipher/ledger/CC errors and the active route remain terminal. */
	if (routeError && index) { xrudp__path_fail(p, s->Now, XRUDP_IO_ERROR); return XRUDP_DONE; }
	if (result != XRUDP_OK) return result;
	(void)ref;
	if (index) {
		p->State.CandidateSent += (uint32_t)size;
		p->State.SentUnvalidated = xrudp__session_add(p->State.SentUnvalidated, size);
	}
	if (echo) xrtSecureZero(&p->Echo[index], sizeof(p->Echo[index]));
	if (challenge) { ++p->State.Attempts; p->State.NextProbeUs = xrudp__session_add(s->Now, UINT64_C(200000) << (p->State.Attempts - 1)); }
	p->ResponseAt = xrudp__session_add(s->Now, 50000);
	if (s->Probes) --s->Probes;
	++s->SentPackets; xrudp__session_pto_sync(s, false); return XRUDP_OK;
}
static xrudpresult xrudp__session_send_mtu(xrudpsession* s, uint64_t* due)
{
	xrudpmtustate* m = &s->Mtu;
	if (!m->Enabled || m->ProbePacket || m->Failure != XRUDP_OK || s->Now < m->NextProbeUs ||
		s->Probes || s->Shutdown.Requested || s->Shutdown.PeerRequested) return XRUDP_DONE;
	const uint32_t flags = XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING | XRUDP_SENT_MTU;
	uint64_t at = s->Now; xrudpresult r = xrtRudpCongestionCanSend(&s->Cc, s->Now, s->Rtt.SmoothedUs, m->ProbeSize, flags, &at);
	if (r != XRUDP_OK || !xrtRudpSentCanRecord(s->Sent, false)) {
		if (r == XRUDP_TIMEOUT && at < *due) *due = at;
		return XRUDP_DONE;
	}
	unsigned char plain[XRUDP_PACKET_MAX] = {0}, packet[XRUDP_PACKET_MAX], epoch[4]; size_t used = 0, size;
	size_t limit = m->ProbeSize - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE;
	xrudpcipherstate cipher; (void)xrtRudpCipherState(s->Cipher, &cipher); xrudp__session_put32(epoch, cipher.ReceiveEpoch);
	if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_PING, 0, NULL, 0) ||
		!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_KEY_CONFIRM, 0, epoch, 4)) return XRUDP_IO_ERROR;
	/* Padding is one bounded all-zero frame. Its packet number, not a peer
	 * reported length/response size, uniquely proves the admitted UDP payload. */
	unsigned char padding[XRUDP_PACKET_MAX] = {0};
	if (!xrudp__session_append(plain, limit, &used, XRUDP_FRAME_PADDING, 0, padding, limit - used - 4)) return XRUDP_IO_ERROR;
	xrudp__sessionref* ref; r = xrudp__session_emit(s, plain, used, flags, packet, due, &size, NULL, NULL, &ref);
	xrtSecureZero(plain, sizeof(plain)); xrtSecureZero(packet, sizeof(packet));
	if (r == XRUDP_BACKPRESSURE) return XRUDP_DONE;
	if (r != XRUDP_OK) return r;
	if (size != m->ProbeSize) return XRUDP_IO_ERROR;
	m->ProbePacket = ref->Number; m->ProbeDeadlineUs = xrudp__session_add(s->Now, xrudp__mtu_interval(s));
	m->NextProbeUs = UINT64_MAX; ++m->Attempts; m->ProbesSent = xrudp__session_add(m->ProbesSent, 1);
	s->ConfirmWanted = false; ++s->SentPackets; xrudp__session_pto_sync(s, false); return XRUDP_OK;
}

static xrudpresult xrudp__session_send(xrudpsession* s, uint64_t* due)
{
	unsigned char plain[XRUDP_PACKET_MAX], body[XRUDP_PACKET_MAX], packet[XRUDP_PACKET_MAX];
	uint64_t pressure = s->OutputBackpressure; xrudpresult result = XRUDP_DONE; bool tried = false, reserveGame = false;
	if (s->Datagrams != NULL && s->PreferDatagram && s->Probes == 0) {
		xrudpdatagramstate state; (void)xrtRudpDatagramState(s->Datagrams, &state); reserveGame = state.SendQueued != 0;
		tried = true; result = xrudp__session_send_datagram(s, due, plain, body, packet);
		if (result != XRUDP_DONE || s->OutputBackpressure != pressure) goto complete;
	}
	/* Keep a blocked class's turn until its full packet fits. Otherwise small
	 * packets of the other class can refill every ACK opening forever. Pure
	 * ACKs never spend the turn; PTO probes retain their recovery priority. */
	result = xrudp__session_send_inner(s, due, reserveGame, plain, body, packet);
	if (result == XRUDP_DONE && s->Datagrams != NULL && !tried && s->Probes == 0 && s->OutputBackpressure == pressure &&
		xrudp__session_select(s, false) == XRUDP__NO_STREAM && !s->ConfirmWanted &&
		!(s->ConnectionLimit > s->ConnectionAcked && s->Now >= s->GrantRetry) &&
		!(s->IncomingLimit > s->StreamsAcked && s->Now >= s->StreamsRetry))
		result = xrudp__session_send_datagram(s, due, plain, body, packet);
complete:
	xrtSecureZero(plain, sizeof(plain)); xrtSecureZero(body, sizeof(body)); xrtSecureZero(packet, sizeof(packet));
	return result;
}

xrudpresult xrtRudpSessionPoll(xrudpsession* s, uint64_t now, uint64_t* output)
{
	if (s == NULL || s->Busy || output == NULL || now < s->Now) return XRUDP_ARGUMENT;
	if (s->Reason != XRUDP_OK) return s->Reason;
	xrudp__session_clock(s, now);
	if (xrudp__shutdown_complete(s)) return XRUDP_CLOSED;
	if (xrudp__session_dead(s, now)) { (void)xrudp__session_abort(s, XRUDP_TIMEOUT); return XRUDP_TIMEOUT; }
	s->Busy = true; xrudpresult result = xrudp__session_losses(s, 0);
	if (result != XRUDP_OK) goto failure;
	result = xrudp__mtu_maintain(s); if (result != XRUDP_OK) goto failure;
	result = xrudp__session_stage(s); if (result != XRUDP_OK) goto failure;
	xrudp__session_pto_sync(s, false); xrudp__session_syncwork(s);
	xrudp__shutdown_update(s);
	if (s->PtoAt && now >= s->PtoAt) {
		if (s->PtoCount < 32) ++s->PtoCount;
		s->Probes = 2; ++s->ProbeTimeouts; uint64_t interval;
		(void)xrtRudpRttPto(&s->Rtt, s->PtoCount, &interval); s->PtoAt = xrudp__session_add(now, interval);
	}
	if (s->PreviousKeyUntil && now >= s->PreviousKeyUntil) { xrtRudpCipherRetirePrevious(s->Cipher); s->PreviousKeyUntil = 0; }
	uint64_t due = xrudp__session_add(s->Activity, s->IdleUs);
	if (s->Work && xrudp__session_add(s->Progress, s->Config.ProgressTimeoutUs) < due) due = xrudp__session_add(s->Progress, s->Config.ProgressTimeoutUs);
	for (size_t i = 0; i < XRUDP_SESSION_POLL_BURST; ++i) {
		uint64_t pressure = s->OutputBackpressure;
		result = i == 0 ? xrudp__session_send_path(s, &due) : XRUDP_DONE;
		if (i == 0 && result == XRUDP_DONE && pressure == s->OutputBackpressure) result = xrudp__session_send_mtu(s, &due);
		if (result == XRUDP_DONE && pressure == s->OutputBackpressure) result = xrudp__session_send(s, &due);
		if (result == XRUDP_DONE) break;
		if (result != XRUDP_OK) goto failure;
		if (i + 1 == XRUDP_SESSION_POLL_BURST && xrudp__session_add(now, 1000) < due) due = xrudp__session_add(now, 1000);
	}
	xrudp__shutdown_update(s);
	if (s->Path.State.Enabled && !s->Shutdown.Ready) {
		uint64_t at = xrudp__path_due(&s->Path, now); if (at < due) due = at;
	}
	if (s->Shutdown.DeadlineUs && s->Shutdown.DeadlineUs < due) due = s->Shutdown.DeadlineUs;
	if (s->Shutdown.Requested && (!s->Shutdown.DeclarationAcknowledged || (s->Shutdown.Ready && !s->Shutdown.DrainedAcknowledged)) && s->ShutdownRetry > now && s->ShutdownRetry < due) due = s->ShutdownRetry;
	if (s->Shutdown.LingerUntilUs) {
		/* Gracefully drained sessions only service bounded ACK work until linger.
		 * Old loss/control timers cannot rearm retransmission or shorten success. */
		due = s->Shutdown.LingerUntilUs;
		if (s->PtoAt && s->PtoAt < due) due = s->PtoAt;
		if (s->AckWanted) {
			uint64_t at = s->AckAt > s->ControlAt ? s->AckAt : s->ControlAt; if (at < due) due = at;
		}
		goto scheduled;
	}
	if (s->PtoAt && s->PtoAt < due) due = s->PtoAt;
	uint64_t keepalive = xrudp__session_keepalive(s); if (keepalive < due) due = keepalive;
	if (s->Mtu.Enabled && !s->Shutdown.Requested && !s->Shutdown.PeerRequested) {
		uint64_t at = s->Mtu.ProbePacket ? s->Mtu.ProbeDeadlineUs : s->Mtu.NextProbeUs;
		if (at < due) due = at;
		if (s->Mtu.LargeDeadlineUs && s->Mtu.LargeDeadlineUs < due) due = s->Mtu.LargeDeadlineUs;
	}
	if (s->PreviousKeyUntil && s->PreviousKeyUntil < due) due = s->PreviousKeyUntil;
	if (s->Datagrams != NULL) {
		xrudpdatagramstate state; (void)xrtRudpDatagramState(s->Datagrams, &state);
		if (state.NextDeadlineUs < due) due = state.NextDeadlineUs;
	}
	if (s->AckWanted && xrtRudpSentCanRecordAck(s->Sent)) {
		uint64_t at = s->AckAt > s->ControlAt ? s->AckAt : s->ControlAt; if (at < due) due = at;
	}
	if (s->ConnectionLimit > s->ConnectionAcked && s->GrantRetry > now && s->GrantRetry < due) due = s->GrantRetry;
	if (s->IncomingLimit > s->StreamsAcked && s->StreamsRetry > now && s->StreamsRetry < due) due = s->StreamsRetry;
	for (uint32_t i = 0; i < s->StreamSlots; ++i) if (s->Streams[i].Id) {
		xrudp__sessionstream* m = &s->Streams[i]; xrudpreceivestate rx; (void)xrtRudpReceiveState(m->Rx, &rx);
		if (m->StagePending && xrudp__session_add(now, 1000) < due) due = xrudp__session_add(now, 1000);
		if ((!m->OpenAcked || (m->ResetQueued && !m->ResetAcked) || rx.Limit > m->GrantAcked) && m->RetryAt > now && m->RetryAt < due) due = m->RetryAt;
		if (m->StopQueued && !m->StopComplete && m->StopRetryAt > now && m->StopRetryAt < due) due = m->StopRetryAt;
		if (m->StopDeadline && m->StopDeadline < due) due = m->StopDeadline;
		if (m->ResetDeadline && m->ResetDeadline < due) due = m->ResetDeadline;
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
scheduled:
	if (due <= now) due = xrudp__session_add(now, 1000);
	s->Busy = false; *output = due; return XRUDP_OK;
failure:
	s->Busy = false; (void)xrudp__session_abort(s, xrudp__session_terminal(result) ? result : XRUDP_IO_ERROR); return s->Reason;
}
