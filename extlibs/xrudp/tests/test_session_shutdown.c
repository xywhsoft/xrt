#define TEST_SESSION_FAULTS
#define main xrudp_shutdown_helpers_main
#include "test_session.c"
#undef main

static int shutdown_frame(unsigned char* plain, size_t* size, uint32_t final, uint32_t timeout)
{
	unsigned char body[8]; put32(body, final); put32(body + 4, timeout);
	return append(plain, size, XRUDP_FRAME_SHUTDOWN, 0, body, sizeof(body));
}
static int shutdown_fin(unsigned char* plain, size_t* size)
{
	unsigned char body[12] = {0}; put32(body, 1);
	return append(plain, size, XRUDP_FRAME_STREAM, XRUDP_STREAM_FIN, body, sizeof(body));
}
static int shutdown_ack(unsigned char* plain, size_t* size, uint64_t pn)
{
	unsigned char body[24] = {0}; body[5] = 1; put64(body + 8, pn); put64(body + 16, pn);
	return append(plain, size, XRUDP_FRAME_ACK, 0, body, sizeof(body));
}
static int shutdown_raw(xrudpcipher** peer, xrudpsession** s)
{
	memset(&net, 0, sizeof(net)); xrudpsessionconfig c; configure(&c, 256, 512);
	c.KeyUpdatePackets = XRUDP_KEY_PACKETS_MAX;
	return raw_session(peer, s, &c);
}
static int shutdown_transaction(void)
{
	for (uint32_t bad = 0; bad < 4; ++bad) {
		xrudpcipher* peer; xrudpsession* s; CHECK(shutdown_raw(&peer, &s) == 0);
		CHECK(xrtRudpSessionShutdown(s, 5000) == XRUDP_OK);
		uint64_t due; CHECK(xrtRudpSessionPoll(s, 0, &due) == XRUDP_OK);
		unsigned char plain[1200], body[13] = {0}; size_t size = 0; put32(body, 1); body[12] = 'x';
		CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, sizeof(body)) == 0);
		if (bad != 2) CHECK(shutdown_frame(plain, &size, bad == 3 ? 2 : 1, 5000) == 0);
		if (bad == 1) CHECK(shutdown_frame(plain, &size, 1, 6000) == 0);
		if (bad == 2) CHECK(append(plain, &size, XRUDP_FRAME_DRAINED, 0, NULL, 0) == 0);
		CHECK(shutdown_ack(plain, &size, 2) == 0);
		CHECK(deliver_raw(s, peer, plain, size, 1000, bad ? XRUDP_MALFORMED : XRUDP_OK) == 0);
		xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK);
		CHECK(state.Shutdown.PeerRequested == (bad == 0) && state.ReceivedConnection == (bad ? 0u : 1u));
		CHECK(state.Shutdown.DeclarationAcknowledged == (bad == 0) && !state.Shutdown.Succeeded);
		xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	}
	return 0;
}
static int shutdown_proof(bool truncate)
{
	xrudpcipher* peer; xrudpsession* s; CHECK(shutdown_raw(&peer, &s) == 0);
	CHECK(xrtRudpSessionShutdown(s, 5000) == XRUDP_OK);
	CHECK(xrtRudpSessionShutdown(s, 5000) == XRUDP_DUPLICATE && xrtRudpSessionShutdown(s, 6000) == XRUDP_ARGUMENT);
	size_t written; uint32_t id = 73; xrudpstreamconfig config; xrtRudpStreamConfigInit(&config);
	CHECK(xrtRudpSessionWrite(s, "x", 1, &written) == XRUDP_CLOSED && written == 0);
	CHECK(xrtRudpSessionStreamOpen(s, &config, &id) == XRUDP_CLOSED && id == 73);
	uint64_t due; CHECK(xrtRudpSessionPoll(s, 0, &due) == XRUDP_OK);
	unsigned char plain[1200]; size_t size = 0;
	CHECK(shutdown_frame(plain, &size, 1, 5000) == 0 && shutdown_fin(plain, &size) == 0 && shutdown_ack(plain, &size, 2) == 0);
	CHECK(append(plain, &size, XRUDP_FRAME_DRAINED, 0, NULL, 0) == 0 && deliver_raw(s, peer, plain, size, 1000, XRUDP_OK) == 0);
	xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.Ready && state.Shutdown.PeerDrained && !state.Shutdown.PeerDrainedAcknowledged);
	if (truncate) {
		/* Keep the DRAINED PN in the local ACK set, but outside the 19 ranges
		 * that actually fit the negotiated 512-byte output. */
		for (uint32_t i = 0; i < 21; ++i) {
			unsigned char ping[4] = {1,0,0,0}, ignored[1200]; size_t n;
			CHECK(xrtRudpCipherSeal(peer, ping, sizeof(ping), ignored, sizeof(ignored), &n) == XRUDP_OK);
			CHECK(deliver_raw(s, peer, ping, sizeof(ping), 2000 + i, XRUDP_OK) == 0);
		}
	}
	net.Output[0].Block = true; CHECK(xrtRudpSessionPoll(s, 10000, &due) == XRUDP_OK);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && !state.Shutdown.PeerDrainedAcknowledged && !state.Shutdown.DrainedAcknowledged && !state.Shutdown.LingerUntilUs);
	net.Output[0].Block = false; CHECK(xrtRudpSessionPoll(s, 11000, &due) == XRUDP_OK);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.PeerDrainedAcknowledged == !truncate && !state.Shutdown.LingerUntilUs);
	uint64_t pn = 0;
	for (size_t i = 0; i < TEST_QUEUE; ++i) if (net.Queue[i].Used) {
		xrudpwirepacket p; CHECK(xrtRudpPacketDecode(net.Queue[i].Data, net.Queue[i].Size, &p) == XRUDP_OK);
		if (p.Header.PacketNumber > pn) pn = p.Header.PacketNumber;
	}
	size = 0; CHECK(shutdown_ack(plain, &size, pn) == 0);
	CHECK(deliver_raw(s, peer, plain, size, 12000, XRUDP_OK) == 0);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.DrainedAcknowledged && !!state.Shutdown.LingerUntilUs == !truncate);
	if (truncate) {
		size = 0; CHECK(append(plain, &size, XRUDP_FRAME_DRAINED, 0, NULL, 0) == 0 && deliver_raw(s, peer, plain, size, 13000, XRUDP_OK) == 0);
		CHECK(xrtRudpSessionPoll(s, 40000, &due) == XRUDP_OK);
		CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.LingerUntilUs && state.Shutdown.PeerDrainedAcknowledged);
	}
	uint64_t linger = state.Shutdown.LingerUntilUs;
	size = 0; CHECK(append(plain, &size, XRUDP_FRAME_DRAINED, 0, NULL, 0) == 0 && deliver_raw(s, peer, plain, size, 50000, XRUDP_OK) == 0);
	CHECK(xrtRudpSessionPoll(s, 80000, &due) == XRUDP_OK);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.LingerUntilUs == linger && !state.Shutdown.Succeeded);
	if (!truncate) CHECK(state.Sent.BytesInFlight == 0); /* Answer retries with ACK, not a fresh PING/ACK cycle. */
	CHECK(xrtRudpSessionPoll(s, linger, &due) == XRUDP_CLOSED);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.Succeeded && state.ReadDone && state.WriteDone && !state.Receive.Reset && !state.Transmit.Cancelled);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer); return 0;
}
static int shutdown_lost_final_ack(void)
{
	memset(&net, 0, sizeof(net)); xrudpsessionconfig config; configure(&config, 256, 512); config.PacketCapacity = 1; config.KeyUpdatePackets = XRUDP_KEY_PACKETS_MAX;
	xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &config) == 0);
	CHECK(xrtRudpSessionShutdown(s, 5000) == XRUDP_OK); uint64_t due; CHECK(xrtRudpSessionPoll(s, 0, &due) == XRUDP_OK);
	unsigned char plain[1200]; size_t size = 0;
	CHECK(shutdown_frame(plain, &size, 1, 5000) == 0 && shutdown_fin(plain, &size) == 0 && shutdown_ack(plain, &size, 2) == 0);
	CHECK(deliver_raw(s, peer, plain, size, 1000, XRUDP_OK) == 0 && xrtRudpSessionPoll(s, 1000, &due) == XRUDP_OK);
	size = 0; CHECK(shutdown_ack(plain, &size, 3) == 0 && deliver_raw(s, peer, plain, size, 2000, XRUDP_OK) == 0);
	size = 0; CHECK(append(plain, &size, XRUDP_FRAME_DRAINED, 0, NULL, 0) == 0 && deliver_raw(s, peer, plain, size, 3000, XRUDP_OK) == 0);
	CHECK(xrtRudpSessionPoll(s, 30000, &due) == XRUDP_OK); xrudpsessionstate state;
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.LingerUntilUs && state.Shutdown.PeerDrainedAcknowledged && state.Sent.StoredPackets == 1 && state.Sent.BytesInFlight != 0);
	uint64_t linger = state.Shutdown.LingerUntilUs, interval; CHECK(xrtRudpRttPto(&state.Rtt, 0, &interval) == XRUDP_OK);
	CHECK(30000 + interval < linger && xrtRudpSessionPoll(s, 30000 + interval, &due) == XRUDP_OK);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.ProbeTimeouts == 1 && state.OutstandingProbes == 0 && state.Sent.StoredPackets == 3 && state.Shutdown.LingerUntilUs == linger);
	uint32_t retries = 0;
	for (size_t k = 0; k < TEST_QUEUE; ++k) if (net.Queue[k].Used) {
		size_t length, offset = 0; xrudpwireframe f; xrudpwirepacket p;
		CHECK(xrtRudpPacketDecode(net.Queue[k].Data, net.Queue[k].Size, &p) == XRUDP_OK);
		CHECK(xrtRudpCipherOpen(peer, net.Queue[k].Data, net.Queue[k].Size, plain, sizeof(plain), &length) == XRUDP_OK);
		bool ping = false, covered = false;
		while (offset < length) {
			CHECK(xrtRudpFrameNext(plain, length, &offset, &f) == XRUDP_OK);
			if (f.Type == XRUDP_FRAME_PING) ping = true;
			if (f.Type == XRUDP_FRAME_ACK) {
				xrudpack ack; CHECK(xrtRudpAckDecode(plain + offset - f.Size - 4, f.Size + 4, &ack) == XRUDP_OK);
				for (uint16_t i = 0; i < ack.Count; ++i) if (ack.Ranges[i].First <= 4 && ack.Ranges[i].Last >= 4) covered = true;
			}
		}
		if (p.Header.PacketNumber >= 5) { CHECK(ping && covered); ++retries; }
	}
	CHECK(retries == 2 && xrtRudpSessionPoll(s, linger, &due) == XRUDP_CLOSED);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.Succeeded);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer); return 0;
}
static int shutdown_deadline(void)
{
	xrudpcipher* peer; xrudpsession* s; CHECK(shutdown_raw(&peer, &s) == 0);
	CHECK(xrtRudpSessionShutdown(s, 1000) == XRUDP_OK); uint64_t due;
	CHECK(xrtRudpSessionPoll(s, 2000000, &due) == XRUDP_OK);
	xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.DeadlineUs == 3000000);
	for (uint32_t i = 0; i < 4; ++i) {
		unsigned char plain[100]; size_t size = 0;
		CHECK(shutdown_ack(plain, &size, 2) == 0 && deliver_raw(s, peer, plain, size, 2200000 + i * 200000, XRUDP_OK) == 0);
		CHECK(xrtRudpSessionShutdown(s, 1000) == XRUDP_DUPLICATE);
	}
	CHECK(xrtRudpSessionPoll(s, 3000000, &due) == XRUDP_TIMEOUT);
	CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && !state.Shutdown.Succeeded);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer); return 0;
}
static int shutdown_remote_and_late_open(void)
{
	for (uint32_t local = 0; local < 2; ++local) {
		memset(&net, 0, sizeof(net)); xrudpsessionconfig c; configure(&c, 256, 512); c.StreamCapacity = c.Local.MaxStreams = c.Peer.MaxStreams = 3;
		c.KeyUpdatePackets = XRUDP_KEY_PACKETS_MAX; xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &c) == 0);
		if (local) CHECK(xrtRudpSessionShutdown(s, 5000) == XRUDP_OK);
		unsigned char plain[1200], body[16] = {0}; size_t size = 0; uint64_t due;
		CHECK(shutdown_frame(plain, &size, local ? 2 : 1, local ? 5000 : 1000) == 0 && shutdown_fin(plain, &size) == 0);
		CHECK(deliver_raw(s, peer, plain, size, 2000000, XRUDP_OK) == 0);
		xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.PeerRequested && !state.Shutdown.Ready);
		xrudpstreamconfig sc; xrtRudpStreamConfigInit(&sc); uint32_t id = 999;
		CHECK(xrtRudpSessionStreamOpen(s, &sc, &id) == XRUDP_CLOSED && id == 999);
		if (!local) {
			size_t written; CHECK(xrtRudpSessionWrite(s, "reply", 5, &written) == XRUDP_OK && written == 5);
			size = 0; CHECK(shutdown_frame(plain, &size, 1, 1000) == 0 && deliver_raw(s, peer, plain, size, 2800000, XRUDP_OK) == 0);
			CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.DeadlineUs == 3000000);
			CHECK(xrtRudpSessionPoll(s, 3000000, &due) == XRUDP_TIMEOUT);
		} else {
			CHECK(xrtRudpSessionPoll(s, 2000000, &due) == XRUDP_OK); size = 0;
			CHECK(shutdown_ack(plain, &size, 2) == 0 && deliver_raw(s, peer, plain, size, 2001000, XRUDP_OK) == 0);
			CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.ReadDone && state.WriteDone && !state.Shutdown.Ready);
			/* Declaration arrived before the in-flight OPEN. It must neither
			 * prematurely finish the drain nor reject this already-admitted ID. */
			size = 0; put32(body, 2); put64(body + 4, 256); body[13] = body[15] = 1;
			CHECK(append(plain, &size, XRUDP_FRAME_OPEN, 0, body, 16) == 0); memset(body + 4, 0, 12);
			CHECK(append(plain, &size, XRUDP_FRAME_STREAM, XRUDP_STREAM_FIN, body, 12) == 0 && deliver_raw(s, peer, plain, size, 2002000, XRUDP_OK) == 0);
			xrudpstreamstate stream; CHECK(xrtRudpSessionStreamState(s, 2, &stream) == XRUDP_OK && stream.Transmit.FinQueued && stream.ReadDone && !stream.WriteDone);
			size = 0; put32(body, 4); put64(body + 4, 256); body[13] = body[15] = 1;
			CHECK(append(plain, &size, XRUDP_FRAME_OPEN, 0, body, 16) == 0 && deliver_raw(s, peer, plain, size, 2003000, XRUDP_MALFORMED) == 0);
			CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.ActiveStreams == 2 && !state.Shutdown.Succeeded);
		}
		xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	}
	return 0;
}
static int shutdown_datagrams_and_abort(void)
{
	for (uint32_t phase = 0; phase < 4; ++phase) {
		memset(&net, 0, sizeof(net)); xrudpsessionconfig c; configure(&c, 256, 512); c.KeyUpdatePackets = XRUDP_KEY_PACKETS_MAX;
		c.Datagrams = (xrudpdatagramconfig){1,4,4,64}; xrudpcipher* cipher[2]; CHECK(raw_pair(cipher) == 0); xrudpsession* s;
		CHECK(xrtRudpSessionCreate(&c, cipher[0], 0, send_packet, &net.Output[0], &s) == XRUDP_OK);
		xrudpdatagramchannel channel; xrtRudpDatagramChannelInit(&channel); channel.Id = 9;
		CHECK(xrtRudpSessionDatagramConfigure(s, &channel) == XRUDP_OK); uint64_t seq;
		CHECK(xrtRudpSessionDatagramWrite(s, 9, "queued", 6, 0, 0, &seq) == XRUDP_OK);
		unsigned char plain[1200], body[14] = {0}; size_t size = 0; put32(body, 9); body[12] = 'x'; body[13] = 'y';
		CHECK(append(plain, &size, XRUDP_FRAME_DATAGRAM, 0, body, sizeof(body)) == 0 && deliver_raw(s, cipher[1], plain, size, 0, XRUDP_OK) == 0);
		CHECK(xrtMemDebugFailAfter(0) && xrtRudpSessionShutdown(s, 5000) == XRUDP_OK);
		xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Datagrams.Stopped && state.Datagrams.SendCancelled == 1 && state.Datagrams.ReceiveCancelled == 1 && !state.Datagrams.SendQueued && !state.Datagrams.ReceiveQueued);
		/* A late datagram is a policy drop, never an internal-error disconnect
		 * from trying to insert it into the already stopped receive pool. */
		CHECK(deliver_raw(s, cipher[1], plain, size, 1000, XRUDP_OK) == 0);
		CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.DatagramDisabledDropped == 1);
		uint64_t due;
		if (phase != 0) {
			CHECK(xrtRudpSessionPoll(s, 2000, &due) == XRUDP_OK); size = 0;
			CHECK(shutdown_frame(plain, &size, 1, 5000) == 0 && shutdown_fin(plain, &size) == 0 && shutdown_ack(plain, &size, 2) == 0);
			if (phase >= 2) CHECK(append(plain, &size, XRUDP_FRAME_DRAINED, 0, NULL, 0) == 0);
			CHECK(deliver_raw(s, cipher[1], plain, size, 3000, XRUDP_OK) == 0);
			CHECK(xrtRudpSessionPoll(s, 30000, &due) == XRUDP_OK);
			if (phase == 3) { size = 0; CHECK(shutdown_ack(plain, &size, 3) == 0 && deliver_raw(s, cipher[1], plain, size, 31000, XRUDP_OK) == 0); CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.Shutdown.LingerUntilUs); }
		}
		CHECK(xrtRudpSessionAbort(s, XRUDP_CANCELLED) == XRUDP_OK && xrtRudpSessionState(s, &state) == XRUDP_OK && state.Reason == XRUDP_CANCELLED && !state.Shutdown.Succeeded && !state.Sent.PendingPackets && !state.Sent.BytesInFlight);
		CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(cipher[1]);
	}
	return 0;
}
static int shutdown_transfer(uint32_t seed, uint32_t packets)
{
	xmemdebugsnapshot baseline, after; xrtMemDebugSnapshot(&baseline);
	memset(&net, 0, sizeof(net)); randomState = seed; tick = 0;
	xrudpcipher* cipher[2]; CHECK(raw_pair(cipher) == 0); uint32_t id[2];
	for (uint32_t i = 0; i < 2; ++i) {
		xrudpsessionconfig c; configure(&c, 256, 512); c.StreamCapacity = c.Local.MaxStreams = c.Peer.MaxStreams = 2;
		c.Role = i ? XRUDP_RESPONDER : XRUDP_INITIATOR; c.Local.MaxMessageSize = c.Peer.MaxMessageSize = 256; c.PacketCapacity = packets;
		net.Output[i].Index = i; net.Output[i].Impair = true;
		CHECK(xrtRudpSessionCreate(&c, cipher[i], 0, send_packet, &net.Output[i], &net.Session[i]) == XRUDP_OK);
		xrudpstreamconfig sc; xrtRudpStreamConfigInit(&sc); sc.Kind = XRUDP_STREAM_MESSAGES;
		CHECK(xrtRudpSessionStreamOpen(net.Session[i], &sc, &id[i]) == XRUDP_OK);
	}
	unsigned char data[200]; memset(data, 'x', sizeof(data)); size_t n;
	CHECK(xrtRudpSessionWrite(net.Session[0], data, sizeof(data), &n) == XRUDP_OK && n == sizeof(data));
	CHECK(xrtRudpSessionMessageWrite(net.Session[0], id[0], data, 128) == XRUDP_OK && xrtRudpSessionShutdown(net.Session[0], 60000) == XRUDP_OK);
	bool requested = false, staged = false, replied = false, done = false; bool closed[2] = {false,false}; size_t read[2] = {0,0}; bool message[2] = {false,false};
	CHECK(xrtMemDebugFailAfter(0));
	for (tick = 1; tick < 65000 && !done; ++tick) {
		uint64_t now = (uint64_t)tick * 1000, due;
		for (size_t k = 0; k < TEST_QUEUE; ++k) if (net.Queue[k].Used && net.Queue[k].Due <= tick) {
			session_packet* p = &net.Queue[k];
			if (!closed[p->Target]) {
				xrudpresult r = xrtRudpSessionInput(net.Session[p->Target], p->Data, p->Size, now);
				CHECK(r == XRUDP_OK || r == XRUDP_DUPLICATE || r == XRUDP_TOO_OLD || r == XRUDP_BACKPRESSURE || r == XRUDP_CLOSED);
				if (r == XRUDP_CLOSED) closed[p->Target] = true;
			}
			p->Used = false;
		}
		for (uint32_t i = 0; i < 2; ++i) if (!closed[i]) {
			xrudpsessionstate state; CHECK(xrtRudpSessionState(net.Session[i], &state) == XRUDP_OK);
			if (i == 1 && state.Shutdown.PeerRequested) {
				requested = true;
				if (tick >= 500 && !replied) {
					/* A delayed business response remains legal until our explicit
					 * shutdown, despite the other direction already requesting it. */
					memset(data, 'x', sizeof(data)); CHECK(xrtRudpSessionWrite(net.Session[i], data, sizeof(data), &n) == XRUDP_OK && n == sizeof(data));
					CHECK(xrtRudpSessionMessageWrite(net.Session[i], id[i], data, 128) == XRUDP_OK && xrtRudpSessionShutdown(net.Session[i], 60000) == XRUDP_OK); replied = true;
				}
			}
			xrudpstreamstate ms;
			if (xrtRudpSessionStreamState(net.Session[i], id[1-i], &ms) == XRUDP_OK && ms.Message.Ready && tick < 800) { staged = true; CHECK(!state.Shutdown.Ready); }
			if (tick >= 800) {
				xrudpresult r = xrtRudpSessionRead(net.Session[i], data, 32, &n);
				CHECK(r == XRUDP_OK || r == XRUDP_BACKPRESSURE || r == XRUDP_DONE); read[i] += n;
				for (size_t k = 0; k < n; ++k) CHECK(data[k] == 'x');
				if (xrtRudpSessionStreamState(net.Session[i], id[1-i], &ms) == XRUDP_OK) {
					r = xrtRudpSessionMessageRead(net.Session[i], id[1-i], data, sizeof(data), &n);
					CHECK(r == XRUDP_OK || r == XRUDP_BACKPRESSURE || r == XRUDP_DONE);
					if (r == XRUDP_OK) { CHECK(!message[i] && n == 128); for (size_t k = 0; k < n; ++k) CHECK(data[k] == 'x'); message[i] = true; }
				}
			}

			xrudpresult r = xrtRudpSessionPoll(net.Session[i], now, &due);
			if (r != XRUDP_OK && r != XRUDP_CLOSED) {
				fprintf(stderr, "shutdown failure side=%u reason=%u read=%zu/%zu messages=%u/%u\n", i, (unsigned)r, read[0], read[1], message[0], message[1]);
				for (uint32_t side = 0; side < 2; ++side) {
					xrudpsessionstate q; (void)xrtRudpSessionState(net.Session[side], &q);
					fprintf(stderr, "side=%u requested=%u/%u ready=%u ack=%u/%u peerdrain=%u/%u refs=%u progress=%llu\n", side, q.Shutdown.Requested, q.Shutdown.PeerRequested, q.Shutdown.Ready, q.Shutdown.DeclarationAcknowledged, q.Shutdown.DrainedAcknowledged, q.Shutdown.PeerDrained, q.Shutdown.PeerDrainedAcknowledged, q.Sent.StoredPackets, (unsigned long long)q.ProgressAtUs);
					for (uint32_t sid = 1; sid <= 3; ++sid) { xrudpstreamstate t; if (xrtRudpSessionStreamState(net.Session[side], sid, &t) == XRUDP_OK) fprintf(stderr, "stream=%u refs=%u done=%u/%u rx=%llu/%llu final=%u open=%u\n", sid, t.References, t.ReadDone, t.WriteDone, (unsigned long long)t.Receive.ReadOffset, (unsigned long long)t.Receive.FinalOffset, t.Receive.HasFinal, t.OpenAcknowledged); }
				}
			}
			CHECK(r == XRUDP_OK || r == XRUDP_CLOSED);
			if (r == XRUDP_CLOSED) closed[i] = true;
		}
		done = closed[0] && closed[1];
	}
	CHECK(done && requested && replied && staged && message[0] && message[1] && read[0] == 200 && read[1] == 200 && !net.Overflow);
	for (uint32_t i = 0; i < 2; ++i) {
		xrudpsessionstate s; CHECK(xrtRudpSessionState(net.Session[i], &s) == XRUDP_OK && s.Reason == XRUDP_CLOSED && s.Shutdown.Succeeded && s.Shutdown.Ready && s.Shutdown.PeerDrainedAcknowledged);
	}
	printf("xrudp shutdown impairment: seed=%u packets=%u elapsed_ms=%u dropped=%u pressure=%u\n", seed, packets, tick, net.Dropped, net.Blocked);
	CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); destroy_pair(); xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(baseline.LiveCount == after.LiveCount && baseline.LiveBytes == after.LiveBytes); return 0;
}
int main(void)
{
	CHECK(shutdown_transaction() == 0 && shutdown_proof(false) == 0 && shutdown_proof(true) == 0 && shutdown_lost_final_ack() == 0 && shutdown_deadline() == 0 && shutdown_remote_and_late_open() == 0 && shutdown_datagrams_and_abort() == 0);
	CHECK(shutdown_transfer(0x6716a71, 32) == 0 && shutdown_transfer(0x5729b7, 1) == 0);
	puts("xrudp shutdown: atomic declarations, two-sided drain, bounded linger, encoded ACK proof and impaired message/byte drain passed"); return 0;
}
