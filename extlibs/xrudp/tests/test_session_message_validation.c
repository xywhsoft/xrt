/* Reuse the existing packet-forging helpers; its normal main remains a
 * separate test target. This target adds message-specific transaction proofs. */
#define TEST_SESSION_FAULTS
#define main xrudp_existing_session_test_main
#include "test_session.c"
#undef main

static int message_open_frame(unsigned char* plain, size_t* size, uint8_t kind)
{
	unsigned char body[16] = {0}; put32(body, 2); put64(body + 4, 16); body[12] = kind; body[13] = 1; body[15] = 1;
	return append(plain, size, XRUDP_FRAME_OPEN, 0, body, sizeof(body));
}
static int message_chunk(unsigned char* plain, size_t* size, uint64_t offset, const void* bytes, size_t count, bool fin)
{
	unsigned char body[64] = {0}; put32(body, 2); put64(body + 4, offset); if (count) memcpy(body + 12, bytes, count);
	return append(plain, size, XRUDP_FRAME_STREAM, fin ? XRUDP_STREAM_FIN : 0, body, count + 12);
}
static int message_transactions(void)
{
	for (unsigned int mode = 0; mode < 8; ++mode) {
		memset(&net, 0, sizeof(net)); xrudpsessionconfig cfg; configure(&cfg, 256, 512);
		cfg.StreamCapacity = cfg.Local.MaxStreams = cfg.Peer.MaxStreams = 2;
		cfg.Local.MaxMessageSize = cfg.Peer.MaxMessageSize = 64;
		xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &cfg) == 0);
		size_t written, size = 0; uint64_t due; unsigned char plain[1200], body[32] = {0};
		CHECK(xrtRudpSessionWrite(s, "outgoing", 8, &written) == XRUDP_OK && xrtRudpSessionPoll(s, 1000, &due) == XRUDP_OK);
		if (mode >= 4 && mode <= 6) {
			CHECK(message_open_frame(plain, &size, XRUDP_STREAM_MESSAGES) == 0);
			unsigned char header[4] = {0,0,0,32};
			if (mode == 4) CHECK(message_chunk(plain, &size, 0, header, 3, false) == 0);
			if (mode == 5) {
				header[3] = 65;
				CHECK(message_chunk(plain, &size, 0, header, 1, false) == 0);
				CHECK(message_chunk(plain, &size, 3, header + 3, 1, false) == 0);
			}
			if (mode == 6) CHECK(message_chunk(plain, &size, 0, header, 4, false) == 0);
			CHECK(deliver_raw(s, peer, plain, size, 1000, XRUDP_OK) == 0); size = 0;
		}
		xrudpsessionstate before; CHECK(xrtRudpSessionState(s, &before) == XRUDP_OK);
		/* A useful ACK, default-stream data and flow grant must all remain
		 * uncommitted if the current message header or FIN is invalid. */
		body[5] = 1; put64(body + 8, 2); put64(body + 16, 2);
		CHECK(append(plain, &size, XRUDP_FRAME_ACK, 0, body, 24) == 0);
		memset(body, 0, sizeof(body)); put32(body, 1); memcpy(body + 12, "safe", 4);
		CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 16) == 0);
		put64(body, 999); CHECK(append(plain, &size, XRUDP_FRAME_MAX_DATA, 0, body, 8) == 0);
		if (mode < 4 || mode == 7) CHECK(message_open_frame(plain, &size, XRUDP_STREAM_MESSAGES) == 0);
		unsigned char data[8] = {0,0,0,65,1,2,3,4};
		if (mode == 0 || mode == 7) CHECK(message_chunk(plain, &size, 0, data, 4, false) == 0);
		if (mode == 1) CHECK(message_chunk(plain, &size, 0, data, 3, true) == 0);
		if (mode == 2) { data[3] = 32; CHECK(message_chunk(plain, &size, 0, data, 8, true) == 0); }
		if (mode == 3) CHECK(message_open_frame(plain, &size, XRUDP_STREAM_BYTES) == 0);
		if (mode == 4) CHECK(message_chunk(plain, &size, 3, data + 3, 1, false) == 0);
		if (mode == 5) CHECK(message_chunk(plain, &size, 1, data, 2, false) == 0);
		if (mode == 6) CHECK(message_chunk(plain, &size, 4, data + 4, 4, true) == 0);
		if (mode == 7) {
			put32(body, 2); put64(body + 4, 4); put32(body + 12, 73);
			CHECK(append(plain, &size, XRUDP_FRAME_RESET, 0, body, 16) == 0);
		}
		CHECK(xrtMemDebugFailAfter(0));
		CHECK(deliver_raw(s, peer, plain, size, 2000, mode == 7 ? XRUDP_OK : XRUDP_MALFORMED) == 0);
		xrudpsessionstate after; CHECK(xrtRudpSessionState(s, &after) == XRUDP_OK);
		if (mode != 7) {
			CHECK(after.Reason == XRUDP_MALFORMED && after.Sent.LargestAcked == before.Sent.LargestAcked && after.PeerLimit == before.PeerLimit);
			CHECK(after.Receive.HighestOffset == 0 && after.ReceivedConnection == before.ReceivedConnection && after.ReceivedPackets == before.ReceivedPackets);
			CHECK(after.ActiveStreams == before.ActiveStreams && !after.AckPending);
		} else {
			CHECK(after.Reason == XRUDP_OK && after.Sent.LargestAcked == 2 && after.Receive.HighestOffset == 4);
			CHECK(xrtRudpSessionMessageRead(s, 2, body, sizeof(body), &written) == XRUDP_CANCELLED && written == 0);
		}
		CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	}
	return 0;
}
static int message_allocation(void)
{
	unsigned char body[65] = {0};
	for (uint64_t budget = 0; budget <= 16; ++budget) {
		memset(&net, 0, sizeof(net)); xrudpcipher* cipher[2] = {NULL,NULL}; CHECK(raw_pair(cipher) == 0);
		xrudpsessionconfig c; configure(&c, 256, 512); c.StreamCapacity = c.Local.MaxStreams = c.Peer.MaxStreams = 3;
		c.Local.MaxMessageSize = c.Peer.MaxMessageSize = 64;
		xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before); xrudpsession* s = (xrudpsession*)(uintptr_t)1;
		CHECK(xrtMemDebugFailAfter(budget)); xrudpresult r = xrtRudpSessionCreate(&c, cipher[0], 0, send_packet, &net.Output[0], &s);
		bool failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (budget < 16) {
			CHECK(r == XRUDP_NO_MEMORY && failed && s == (xrudpsession*)(uintptr_t)1);
			xrtClearError(); xrtMemDebugSnapshot(&after); CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
			xrudpcipherstate state; CHECK(xrtRudpCipherState(cipher[0], &state) == XRUDP_OK && state.NextPacketNumber == 2);
			xrtRudpCipherDestroy(cipher[0]);
		} else {
			CHECK(r == XRUDP_OK && !failed); size_t reserved; CHECK(xrtRudpSessionAllocationSize(&c, &reserved) == XRUDP_OK);
			xrtMemDebugSnapshot(&after); CHECK(after.LiveBytes - before.LiveBytes == reserved - xrtRudpCipherAllocationSize());
			xrudpstreamconfig stream; xrtRudpStreamConfigInit(&stream); stream.Kind = XRUDP_STREAM_MESSAGES; uint32_t id;
			CHECK(xrtMemDebugFailAfter(0)); CHECK(xrtRudpSessionStreamOpen(s, &stream, &id) == XRUDP_OK);
			CHECK(xrtRudpSessionMessageWrite(s, id, NULL, 0) == XRUDP_OK && xrtRudpSessionMessageWrite(s, id, "x", 1) == XRUDP_OK);
			CHECK(xrtRudpSessionMessageWrite(s, id, body, 65) == XRUDP_LIMIT);
			CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtRudpSessionDestroy(s);
		}
		xrtRudpCipherDestroy(cipher[1]); xrtClearError();
	}
	return 0;
}
static int message_limits(void)
{
	for (unsigned int mode = 0; mode < 3; ++mode) {
		memset(&net, 0, sizeof(net)); xrudpsessionconfig c; configure(&c, 256, 512);
		c.StreamCapacity = c.Local.MaxStreams = c.Peer.MaxStreams = 2;
		c.Local.MaxMessageSize = mode == 0 ? 0 : 64; c.Peer.MaxMessageSize = mode == 1 ? 0 : 128;
		xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &c) == 0);
		xrudpstreamconfig stream; xrtRudpStreamConfigInit(&stream); stream.Kind = XRUDP_STREAM_MESSAGES; uint32_t id = 99;
		xrudpresult result = xrtRudpSessionStreamOpen(s, &stream, &id);
		if (mode < 2) CHECK(result == XRUDP_PROTOCOL_MISMATCH && id == 99);
		else {
			CHECK(result == XRUDP_OK && id == 3); xrudpstreamstate before, after;
			CHECK(xrtRudpSessionStreamState(s, id, &before) == XRUDP_OK && before.SendMessageLimit == 128 && before.Message.Maximum == 64);
			unsigned char data[129] = {0}; size_t count = 99;
			CHECK(xrtRudpSessionMessageWrite(s, id, data, 129) == XRUDP_LIMIT);
			CHECK(xrtRudpSessionMessageWrite(s, id, data, 128) == XRUDP_OK);
			CHECK(xrtRudpSessionStreamState(s, id, &before) == XRUDP_OK && before.Transmit.QueuedEnd == 132);
			CHECK(xrtRudpSessionMessageWrite(s, id, data, 128) == XRUDP_BACKPRESSURE);
			CHECK(xrtRudpSessionStreamState(s, id, &after) == XRUDP_OK && memcmp(&before, &after, sizeof(before)) == 0);
			CHECK(xrtRudpSessionMessagePeek(s, 1, &count) == XRUDP_ARGUMENT && count == 0);
			CHECK(xrtRudpSessionMessageRead(s, 1, data, 129, &count) == XRUDP_ARGUMENT && count == 0);
			CHECK(xrtRudpSessionStreamFinish(s, id) == XRUDP_OK && xrtRudpSessionMessageWrite(s, id, NULL, 0) == XRUDP_CLOSED);
		}
		xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer);
	}
	return 0;
}
static int message_stage_budget(void)
{
	memset(&net, 0, sizeof(net)); xrudpsessionconfig c; configure(&c, 8192, 1200);
	c.StreamCapacity = c.Local.MaxStreams = c.Peer.MaxStreams = 3;
	c.Local.ConnectionCredit = c.Peer.ConnectionCredit = 32768;
	c.Local.MaxMessageSize = c.Peer.MaxMessageSize = 8192;
	xrudpcipher* peer; xrudpsession* s; CHECK(raw_session(&peer, &s, &c) == 0);
	unsigned char plain[1200], body[1120], record[8004] = {0}, output[8000]; size_t size = 0, count; uint64_t now = 1000, due;
	put32(record, 8000); for (size_t i = 4; i < sizeof(record); ++i) record[i] = (unsigned char)(i * 29);
	for (uint32_t id = 2; id <= 4; id += 2) {
		memset(body, 0, 16); put32(body, id); put64(body + 4, 8192); body[12] = XRUDP_STREAM_MESSAGES; body[13] = 1; body[15] = 1;
		CHECK(append(plain, &size, XRUDP_FRAME_OPEN, 0, body, 16) == 0);
		memset(body, 0, 16); put32(body, id);
		CHECK(append(plain, &size, XRUDP_FRAME_STREAM, 0, body, 16) == 0);
	}
	CHECK(deliver_raw(s, peer, plain, size, now++, XRUDP_OK) == 0);
	for (uint32_t id = 2; id <= 4; id += 2) for (size_t at = 0; at < sizeof(record);) {
		size_t chunk = sizeof(record) - at; if (chunk > 1000) chunk = 1000;
		size = 0; put32(body, id); put64(body + 4, at + 4); memcpy(body + 12, record + at, chunk);
		CHECK(append(plain, &size, XRUDP_FRAME_STREAM, at + chunk == sizeof(record) ? XRUDP_STREAM_FIN : 0, body, 12 + chunk) == 0);
		CHECK(deliver_raw(s, peer, plain, size, now++, XRUDP_OK) == 0); at += chunk;
	}
	for (uint32_t id = 2; id <= 4; id += 2) {
		xrudpstreamstate state; CHECK(xrtRudpSessionStreamState(s, id, &state) == XRUDP_OK && state.Message.Ready && state.Message.Expected == 0 && state.Receive.BufferedBytes == 8004);
		CHECK(xrtRudpSessionMessageRead(s, id, NULL, 0, &count) == XRUDP_OK && count == 0);
	}
	uint64_t previous = 8; bool done = false;
	for (unsigned int turn = 0; turn < 5 && !done; ++turn) {
		CHECK(xrtRudpSessionPoll(s, 10000 + turn * 1000, &due) == XRUDP_OK); uint64_t offsets = 0; done = true;
		for (uint32_t id = 2; id <= 4; id += 2) {
			xrudpstreamstate state; CHECK(xrtRudpSessionStreamState(s, id, &state) == XRUDP_OK); offsets += state.Receive.ReadOffset;
			if (!state.Message.Ready) {
				CHECK(!state.ReadReady && xrtRudpSessionMessageRead(s, id, output, sizeof(output), &count) == XRUDP_BACKPRESSURE && count == 0); done = false;
			}
			if (turn == 1) CHECK(state.Message.Buffered != 0); /* Round-robin, not first-slot monopolization. */
		}
		CHECK(offsets > previous && offsets - previous <= XRUDP_SESSION_MESSAGE_STAGE_BYTES); previous = offsets;
		if (!done) CHECK(due <= 11000 + turn * 1000);
	}
	CHECK(done);
	for (uint32_t id = 2; id <= 4; id += 2) {
		CHECK(xrtRudpSessionMessageRead(s, id, output, sizeof(output), &count) == XRUDP_OK && count == sizeof(output) && memcmp(output, record + 4, sizeof(output)) == 0);
		CHECK(xrtRudpSessionMessagePeek(s, id, &count) == XRUDP_DONE && count == 0);
	}
	xrudpsessionstate state; CHECK(xrtRudpSessionState(s, &state) == XRUDP_OK && state.ReceivedConnection == 16016 && state.ReceiveLimit == 32768 + 16016);
	xrtRudpSessionDestroy(s); xrtRudpCipherDestroy(peer); return 0;
}
int main(void)
{
	randomState = 1; CHECK(message_transactions() == 0); CHECK(message_allocation() == 0); CHECK(message_limits() == 0);
	CHECK(message_stage_budget() == 0);
	puts("xrudp messages: authenticated head validation is atomic across ACK/data/grants; reset wins and 16 allocation positions balance"); return 0;
}
