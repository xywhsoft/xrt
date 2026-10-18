#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static const unsigned char golden[] = {
	0x58,0x52,0x55,0x44,0x50,0x30,0x30,0x31,
	0x01,0x00,0x00,0x03,0x00,0x00,0x00,0x00,
	0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
	0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,
	0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x61,0x62,0x63
};

static int packet_tests(void)
{
	unsigned char output[XRUDP_PACKET_MAX + 1];
	unsigned char before[sizeof(output)];
	xrudpwirepacket packet, saved;
	xrudpwireheader header;
	size_t written;
	memset(&packet, 0x5a, sizeof(packet));
	CHECK(xrtRudpPacketDecode(golden, sizeof(golden), &packet) == XRUDP_OK);
	CHECK(packet.Header.Type == XRUDP_PACKET_HELLO && packet.Header.PayloadSize == 3);
	CHECK(packet.Header.KeyEpoch == 0);
	CHECK(packet.Header.PacketNumber == UINT64_C(0x0102030405060708));
	CHECK(packet.Payload == golden + XRUDP_HEADER_SIZE);
	header = packet.Header;
	CHECK(xrtRudpPacketEncode(output, sizeof(output), &written, &header, "abc", 3) == XRUDP_OK);
	CHECK(written == sizeof(golden) && memcmp(output, golden, sizeof(golden)) == 0);
	/* Moving an overlapping borrowed payload must preserve its bytes. */
	memcpy(output, "abc", 3);
	CHECK(xrtRudpPacketEncode(output, sizeof(output), &written, &header, output, 3) == XRUDP_OK);
	CHECK(memcmp(output, golden, sizeof(golden)) == 0);
	for (size_t i = 0; i < sizeof(golden); ++i) {
		memset(&packet, 0x5a, sizeof(packet)); saved = packet;
		CHECK(xrtRudpPacketDecode(golden, i, &packet) == XRUDP_TRUNCATED);
		CHECK(memcmp(&packet, &saved, sizeof(packet)) == 0);
	}
	for (size_t i = 0; i < XRUDP_HEADER_SIZE; ++i) {
		if (i >= 16 && i < 40) continue;
		memcpy(output, golden, sizeof(golden)); output[i] ^= 0x80;
		memset(&packet, 0x5a, sizeof(packet)); saved = packet;
		CHECK(xrtRudpPacketDecode(output, sizeof(golden), &packet) != XRUDP_OK);
		CHECK(memcmp(&packet, &saved, sizeof(packet)) == 0);
	}
	memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(before));
	CHECK(xrtRudpPacketEncode(output, sizeof(golden) - 1, &written, &header, "abc", 3) == XRUDP_LIMIT);
	CHECK(written == 0 && memcmp(output, before, sizeof(before)) == 0);
	CHECK(xrtRudpPacketEncode(output, sizeof(output), &written, &header, "abc", SIZE_MAX) == XRUDP_LIMIT);
	CHECK(memcmp(output, before, sizeof(before)) == 0);
	CHECK(xrtRudpPacketEncode(output, sizeof(output), &written, &header, "abc", 2) == XRUDP_ARGUMENT);
	CHECK(xrtRudpPacketEncode(NULL, 0, &written, &header, "abc", 3) == XRUDP_ARGUMENT);
	CHECK(xrtRudpPacketDecode(output, sizeof(output), &packet) == XRUDP_LIMIT);
	header.Type = XRUDP_PACKET_DATA; header.KeyEpoch = 1;
	header.PayloadSize = XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE;
	memset(before, 0, sizeof(before));
	CHECK(xrtRudpPacketEncode(output, sizeof(output), &written, &header, before, header.PayloadSize) == XRUDP_OK);
	CHECK(written == XRUDP_PACKET_MAX);
	CHECK(xrtRudpPacketDecode(output, written, &packet) == XRUDP_OK);
	header.KeyEpoch = 0;
	CHECK(xrtRudpPacketEncode(output, sizeof(output), &written, &header, before, header.PayloadSize) == XRUDP_MALFORMED);
	header.Type=XRUDP_PACKET_PATH;header.KeyEpoch=1;header.PacketNumber=2;header.PayloadSize=36;
	CHECK(xrtRudpPacketEncode(output,sizeof(output),&written,&header,before,36)==XRUDP_OK && written==84);
	CHECK(xrtRudpPacketDecode(output,written,&packet)==XRUDP_OK && packet.Header.Type==XRUDP_PACKET_PATH);
	header.PayloadSize=80;CHECK(xrtRudpPacketEncode(output,sizeof(output),&written,&header,before,80)==XRUDP_OK && written==128);
	header.PayloadSize=81;CHECK(xrtRudpPacketEncode(output,sizeof(output),&written,&header,before,81)==XRUDP_MALFORMED);
	header.PayloadSize=35;CHECK(xrtRudpPacketEncode(output,sizeof(output),&written,&header,before,35)==XRUDP_MALFORMED);
	header.PayloadSize=36;header.PacketNumber=1;CHECK(xrtRudpPacketEncode(output,sizeof(output),&written,&header,before,36)==XRUDP_MALFORMED);
	return 0;
}

static void put64(unsigned char* p, uint64_t n)
{
	for (size_t i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(n >> (i * 8));
}

static int frame_tests(void)
{
	unsigned char output[XRUDP_PACKET_MAX], saved[XRUDP_PACKET_MAX];
	unsigned char body[32] = {0};
	xrudpwireframe frame, previous;
	size_t written, offset, count;
	static const unsigned char ping[] = {1, 0, 0, 0};
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_PING, 0, NULL, 0) == XRUDP_OK);
	CHECK(written == sizeof(ping) && memcmp(output, ping, sizeof(ping)) == 0);
	CHECK(xrtRudpFramesValidate(output, written, &count) == XRUDP_OK && count == 1);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_PING, 1, NULL, 0) == XRUDP_MALFORMED);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, (xrudpframetype)255, 0, NULL, 0) == XRUDP_MALFORMED);
	/* Independent canonical STOP_SENDING vector, fixed reason and stream id. */
	static const unsigned char stop[]={14,0,0,8,0,0,0,3,0x12,0x34,0x56,0x78};
	CHECK(xrtRudpFrameAppend(output,sizeof(output),&written,XRUDP_FRAME_STOP_SENDING,0,stop+4,8)==XRUDP_OK && written==sizeof(stop) && memcmp(output,stop,sizeof(stop))==0);
	CHECK(xrtRudpFramesValidate(stop,sizeof(stop),&count)==XRUDP_OK && count==1);
	for(size_t n=0;n<sizeof(stop);++n) { offset=0;CHECK(xrtRudpFrameNext(stop,n,&offset,&frame)==(n==0?XRUDP_DONE:XRUDP_TRUNCATED) && offset==0); }
	for(size_t n=0;n<=12;++n)if(n!=8)CHECK(xrtRudpFrameAppend(output,sizeof(output),&written,XRUDP_FRAME_STOP_SENDING,0,body,n)==XRUDP_MALFORMED);
	CHECK(xrtRudpFrameAppend(output,sizeof(output),&written,XRUDP_FRAME_STOP_SENDING,0,body,8)==XRUDP_MALFORMED);
	CHECK(xrtRudpFrameAppend(output,sizeof(output),&written,XRUDP_FRAME_STOP_SENDING,1,stop+4,8)==XRUDP_MALFORMED);
	count = 87;
	CHECK(xrtRudpFramesValidate(NULL, 0, &count) == XRUDP_MALFORMED && count == 87);
	for (size_t i = 0; i < sizeof(ping); ++i) {
		offset = 0; memset(&frame, 0xa5, sizeof(frame)); previous = frame;
		CHECK(xrtRudpFrameNext(ping, i, &offset, &frame) == (i == 0 ? XRUDP_DONE : XRUDP_TRUNCATED));
		CHECK(offset == 0 && memcmp(&frame, &previous, sizeof(frame)) == 0);
	}
	/* Stream offsets must not wrap and failures must not modify output. */
	body[3] = 1; put64(body + 4, UINT64_MAX); body[12] = 'x';
	memset(output, 0xa5, sizeof(output)); memcpy(saved, output, sizeof(saved));
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_STREAM, 0, body, 13) == XRUDP_MALFORMED);
	CHECK(written == 0 && memcmp(output, saved, sizeof(saved)) == 0);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_STREAM, XRUDP_STREAM_FIN, body, 12) == XRUDP_OK);
	put64(body + 4, UINT64_MAX - 1);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_STREAM, 0, body, 13) == XRUDP_OK);
	CHECK(xrtRudpFramesValidate(output, written, &count) == XRUDP_OK && count == 1);
	/* Unsequenced messages require sequence zero; sequenced messages require >0. */
	put64(body + 4, 0);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_DATAGRAM, 0, body, 12) == XRUDP_OK);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_DATAGRAM, XRUDP_DATAGRAM_SEQUENCED, body, 12) == XRUDP_MALFORMED);
	put64(body + 4, 1);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_DATAGRAM, 0, body, 12) == XRUDP_MALFORMED);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_DATAGRAM, XRUDP_DATAGRAM_SEQUENCED, body, 12) == XRUDP_OK);
	/* A valid prefix must not hide an invalid/truncated suffix. */
	memcpy(output, ping, 4); output[4] = 255; output[5] = output[6] = output[7] = 0;
	count = 87;
	CHECK(xrtRudpFramesValidate(output, 8, &count) == XRUDP_MALFORMED && count == 87);
	CHECK(xrtRudpFramesValidate(output, 5, &count) == XRUDP_TRUNCATED && count == 87);
	for (size_t i = 0; i < XRUDP_FRAMES_MAX + 1; ++i) memcpy(output + i * 4, ping, 4);
	CHECK(xrtRudpFramesValidate(output, XRUDP_FRAMES_MAX * 4, &count) == XRUDP_OK && count == XRUDP_FRAMES_MAX);
	count = 87;
	CHECK(xrtRudpFramesValidate(output, (XRUDP_FRAMES_MAX + 1) * 4, &count) == XRUDP_LIMIT && count == 87);
	/* Frame appending is safe when application data begins at the destination. */
	memset(body, 0, sizeof(body)); body[3] = 1; put64(body + 4, 2); body[12] = 0x5a;
	memcpy(output, body, 13);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_STREAM, 0, output, 13) == XRUDP_OK);
	CHECK(memcmp(output + 4, body, 13) == 0);
	return 0;
}

static int ack_tests(void)
{
	unsigned char body[8 + XRUDP_ACK_RANGES_MAX * 16] = {0};
	unsigned char output[XRUDP_PACKET_MAX];
	size_t written, count;
	body[5] = 2;
	put64(body + 8, 10); put64(body + 16, 15);
	put64(body + 24, 1); put64(body + 32, 8);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_ACK, 0, body, 40) == XRUDP_OK);
	CHECK(xrtRudpFramesValidate(output, written, &count) == XRUDP_OK && count == 1);
	put64(body + 32, 9);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_ACK, 0, body, 40) == XRUDP_MALFORMED);
	put64(body + 32, 10);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_ACK, 0, body, 40) == XRUDP_MALFORMED);
	put64(body + 32, UINT64_MAX);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_ACK, 0, body, 40) == XRUDP_MALFORMED);
	put64(body + 8, 0);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_ACK, 0, body, 40) == XRUDP_MALFORMED);
	memset(body, 0, sizeof(body)); body[5] = XRUDP_ACK_RANGES_MAX;
	for (size_t i = 0; i < XRUDP_ACK_RANGES_MAX; ++i) {
		uint64_t number = UINT64_MAX - i * 3;
		put64(body + 8 + i * 16, number); put64(body + 16 + i * 16, number);
	}
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_ACK, 0, body, sizeof(body)) == XRUDP_OK);
	body[7] = 1;
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &written, XRUDP_FRAME_ACK, 0, body, sizeof(body)) == XRUDP_MALFORMED);
	return 0;
}

static int shutdown_frames(void)
{
	static const unsigned char goldenShutdown[] = {15,0,0,8,0,0,0,3,0,0,0x13,0x88};
	static const unsigned char goldenDrained[] = {16,0,0,0};
	unsigned char output[64], body[12] = {0}; size_t size, offset, count;
	xrudpwireframe frame;
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &size, XRUDP_FRAME_SHUTDOWN, 0, goldenShutdown + 4, 8) == XRUDP_OK && size == sizeof(goldenShutdown) && memcmp(output, goldenShutdown, size) == 0);
	CHECK(xrtRudpFramesValidate(goldenShutdown, sizeof(goldenShutdown), &count) == XRUDP_OK && count == 1);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &size, XRUDP_FRAME_DRAINED, 0, NULL, 0) == XRUDP_OK && size == sizeof(goldenDrained) && memcmp(output, goldenDrained, size) == 0);
	for (size_t n = 0; n < sizeof(goldenShutdown); ++n) { offset = 0; CHECK(xrtRudpFrameNext(goldenShutdown, n, &offset, &frame) == (n ? XRUDP_TRUNCATED : XRUDP_DONE) && offset == 0); }
	for (size_t n = 0; n < sizeof(goldenDrained); ++n) { offset = 0; CHECK(xrtRudpFrameNext(goldenDrained, n, &offset, &frame) == (n ? XRUDP_TRUNCATED : XRUDP_DONE) && offset == 0); }
	for (size_t n = 0; n <= sizeof(body); ++n) if (n != 8) CHECK(xrtRudpFrameAppend(output, sizeof(output), &size, XRUDP_FRAME_SHUTDOWN, 0, body, n) == XRUDP_MALFORMED);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &size, XRUDP_FRAME_DRAINED, 0, body, 1) == XRUDP_MALFORMED);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &size, XRUDP_FRAME_DRAINED, 1, NULL, 0) == XRUDP_MALFORMED);
	CHECK(xrtRudpFrameAppend(output, sizeof(output), &size, XRUDP_FRAME_SHUTDOWN, 1, goldenShutdown + 4, 8) == XRUDP_MALFORMED);
	uint32_t finals[] = {0,1,UINT32_C(2147483648),UINT32_C(2147483649)}, times[] = {0,999,1000,600000,600001};
	for (size_t i = 0; i < 4; ++i) for (size_t j = 0; j < 5; ++j) {
		for (size_t k = 0; k < 4; ++k) { body[3-k] = (unsigned char)(finals[i] >> (8*k)); body[7-k] = (unsigned char)(times[j] >> (8*k)); }
		CHECK(xrtRudpFrameAppend(output, sizeof(output), &size, XRUDP_FRAME_SHUTDOWN, 0, body, 8) ==
			((i == 1 || i == 2) && (j == 2 || j == 3) ? XRUDP_OK : XRUDP_MALFORMED));
	}
	return 0;
}
static int malformed_corpus(void)
{
	unsigned char data[XRUDP_PACKET_MAX + 1];
	uint32_t random = 0x58525544u;
	for (size_t iteration = 0; iteration < 50000; ++iteration) {
		xrudpwirepacket packet, before;
		xrudpwireframe frame;
		size_t length = iteration % sizeof(data), count = 321, offset = 0;
		xrudpresult result;
		for (size_t i = 0; i < length; ++i) {
			random ^= random << 13; random ^= random >> 17; random ^= random << 5;
			data[i] = (unsigned char)random;
		}
		if (iteration % 2 == 0 && length >= sizeof(golden))
			memcpy(data, golden, sizeof(golden));
		memset(&packet, 0xa5, sizeof(packet)); before = packet;
		result = xrtRudpPacketDecode(data, length, &packet);
		if (result == XRUDP_OK) {
			CHECK(packet.Payload == data + XRUDP_HEADER_SIZE);
			CHECK((size_t)packet.Header.PayloadSize + XRUDP_HEADER_SIZE == length);
		} else CHECK(memcmp(&packet, &before, sizeof(packet)) == 0);
		result = xrtRudpFramesValidate(data, length, &count);
		if (result == XRUDP_OK) CHECK(count > 0 && count <= XRUDP_FRAMES_MAX);
		else CHECK(count == 321);
		result = xrtRudpFrameNext(data, length, &offset, &frame);
		if (result == XRUDP_OK) CHECK(offset <= length && offset >= XRUDP_FRAME_HEADER_SIZE);
		else CHECK(offset == 0);
	}
	return 0;
}

int main(void)
{
	if (packet_tests() || frame_tests() || ack_tests() || shutdown_frames() || malformed_corpus()) return 1;
	puts("xrudp wire: golden, truncation, overflow, canonical ACK, bounded corpus passed");
	return 0;
}
