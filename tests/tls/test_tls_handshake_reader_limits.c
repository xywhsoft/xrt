#include "../test.h"



/* 配置必须限制完整编码长度并允许显式零保留。 */
static void testTlsHandshakeReaderConfig(void)
{
	xtlshandshakereaderconfig Config;
	xtlshandshakereader Reader;

	xrtTlsHandshakeReaderConfigInit(&Config);
	testRequire((Config.Limit == XTLS_HANDSHAKE_LIMIT_DEFAULT) &&
		(Config.Retain == XTLS_HANDSHAKE_RETAIN_DEFAULT),
		"TLS handshake reader default config mismatch");
	Config.Limit = 3u;
	testRequire(!xrtTlsHandshakeReaderInit(&Reader, &Config),
		"TLS handshake reader accepted a limit below its header");
	Config.Limit = 64u;
	Config.Retain = 65u;
	testRequire(!xrtTlsHandshakeReaderInit(&Reader, &Config),
		"TLS handshake reader accepted retain above limit");
	Config.Retain = 0;
	testRequire(xrtTlsHandshakeReaderInit(&Reader, &Config),
		"TLS handshake reader rejected explicit zero retain");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 超限头必须零消费、保持 reader 和消息输出不变。 */
static void testTlsHandshakeReaderLimit(void)
{
	static const uint8 Header[] = { 11, 0, 0, 100 };
	static const uint8 Exact[64] = {
		11, 0, 0, 60
	};
	xtlshandshakereaderconfig Config = { 64u, 16u };
	xtlshandshakereader Reader;
	xtlshandshakereader BeforeReader;
	xtlshandshake Message;
	xtlshandshake BeforeMessage;
	size_t iConsumed = SIZE_MAX;

	testRequire(xrtTlsHandshakeReaderInit(&Reader, &Config),
		"TLS limit reader initialization failed");
	memset(&Message, 0xA5, sizeof(Message));
	BeforeMessage = Message;
	BeforeReader = Reader;
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Header, sizeof(Header) },
		&iConsumed, &Message
	) == XTLS_ERROR && (iConsumed == 0) &&
		(memcmp(&Reader, &BeforeReader, sizeof(Reader)) == 0) &&
		(memcmp(&Message, &BeforeMessage, sizeof(Message)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_LIMIT),
		"TLS direct over-limit header was not rejected atomically");

	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Header, 2u }, &iConsumed, &Message
	) == XTLS_AGAIN && (iConsumed == 2u),
		"TLS split over-limit setup failed");
	BeforeReader = Reader;
	BeforeMessage = Message;
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Header + 2u, 2u },
		&iConsumed, &Message
	) == XTLS_ERROR && (iConsumed == 0) &&
		(memcmp(&Reader, &BeforeReader, sizeof(Reader)) == 0) &&
		(memcmp(&Message, &BeforeMessage, sizeof(Message)) == 0),
		"TLS split over-limit header changed state");

	testRequire(xrtTlsHandshakeReaderReset(&Reader),
		"TLS reader reset after limit failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Exact, sizeof(Exact) },
		&iConsumed, &Message
	) == XTLS_OK && (iConsumed == sizeof(Exact)) &&
		(Message.Body.Size == 60u),
		"TLS reader rejected a message exactly at its limit");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 大消息必须随已接收字节增长，而不是按声明长度立即分配。 */
static void testTlsHandshakeReaderAdaptive(void)
{
	enum { BODY_SIZE = 100000, MESSAGE_SIZE = BODY_SIZE + 4 };
	xtlshandshakereaderconfig Config = { 200000u, 128u };
	xtlshandshakereader Reader;
	xtlshandshake Message;
	uint8* pData = (uint8*)xrtMalloc(MESSAGE_SIZE);
	size_t iOffset = 0;
	size_t iConsumed = 0;

	testRequire(pData != NULL,
		"TLS adaptive reader test allocation failed");
	pData[0] = XTLS_HANDSHAKE_CERTIFICATE;
	pData[1] = 0x01;
	pData[2] = 0x86;
	pData[3] = 0xA0;
	memset(pData + 4u, 0x5A, BODY_SIZE);
	testRequire(xrtTlsHandshakeReaderInit(&Reader, &Config),
		"TLS adaptive reader initialization failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { pData, 4u },
		&iConsumed, &Message
	) == XTLS_AGAIN && (iConsumed == 4u) &&
		(Reader.Required == MESSAGE_SIZE) &&
		(Reader.Capacity < Reader.Required) &&
		(Reader.Capacity <= 64u),
		"TLS reader allocated the declared large message eagerly");
	iOffset = 4u;
	while ( iOffset < MESSAGE_SIZE ) {
		size_t iChunk = MESSAGE_SIZE - iOffset;
		xtlsresult Result;

		if ( iChunk > 4096u ) {
			iChunk = 4096u;
		}
		Result = xrtTlsHandshakeReaderRead(
			&Reader, (xbytesview) { pData + iOffset, iChunk },
			&iConsumed, &Message
		);
		testRequire(iConsumed == iChunk,
			"TLS adaptive reader did not consume its input chunk");
		iOffset += iConsumed;
		testRequire(Result == (iOffset == MESSAGE_SIZE ?
			XTLS_OK : XTLS_AGAIN),
			"TLS adaptive reader completed at the wrong boundary");
	}
	testRequire((Message.Body.Size == BODY_SIZE) &&
		(Message.Body.Data == Reader.Data + 4u) &&
		(Reader.Capacity >= MESSAGE_SIZE),
		"TLS adaptive reader produced the wrong large message");

	memset(&Message, 0xA5, sizeof(Message));
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { NULL, 0 }, &iConsumed, &Message
	) == XTLS_AGAIN && (Reader.Data == NULL) &&
		(Reader.Capacity == 0) && (Reader.Size == 0),
		"TLS reader retained an oversized completed buffer");
	xrtTlsHandshakeReaderUnit(&Reader);
	xrtFree(pData);
}



/* Reader 自身可移动缓冲不得作为下一段输入。 */
static void testTlsHandshakeReaderAlias(void)
{
	static const uint8 Data[] = {
		11, 0, 0, 8, 1, 2, 3, 4, 5, 6, 7, 8
	};
	xtlshandshakereader Reader;
	xtlshandshakereader Before;
	xtlshandshake Message;
	size_t iConsumed = 0;

	testRequire(xrtTlsHandshakeReaderInit(&Reader, NULL),
		"TLS alias reader initialization failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data, 5u }, &iConsumed, &Message
	) == XTLS_AGAIN && (Reader.Data != NULL),
		"TLS alias reader setup failed");
	Before = Reader;
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Reader.Data, 1u },
		&iConsumed, &Message
	) == XTLS_ERROR && (iConsumed == 0) &&
		(memcmp(&Reader, &Before, sizeof(Reader)) == 0),
		"TLS reader accepted input aliased to its movable buffer");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 小缓冲应在 Reset 后保留以复用。 */
static void testTlsHandshakeReaderRetain(void)
{
	static const uint8 Data[] = {
		11, 0, 0, 20,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	xtlshandshakereaderconfig Config = { 128u, 64u };
	xtlshandshakereader Reader;
	xtlshandshake Message;
	size_t iConsumed;
	size_t iCapacity;

	testRequire(xrtTlsHandshakeReaderInit(&Reader, &Config),
		"TLS retain reader initialization failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data, 5u }, &iConsumed, &Message
	) == XTLS_AGAIN && xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data + 5u, sizeof(Data) - 5u },
		&iConsumed, &Message
	) == XTLS_OK, "TLS retain reader setup failed");
	iCapacity = Reader.Capacity;
	testRequire((iCapacity != 0) && (iCapacity <= Config.Retain) &&
		xrtTlsHandshakeReaderReset(&Reader) &&
		(Reader.Capacity == iCapacity) && (Reader.Data != NULL),
		"TLS reader did not retain its small buffer");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 执行 TLS 握手 reader 限制与内存回归。 */
int main(void)
{
	testTlsHandshakeReaderConfig();
	testTlsHandshakeReaderLimit();
	testTlsHandshakeReaderAdaptive();
	testTlsHandshakeReaderAlias();
	testTlsHandshakeReaderRetain();
	return 0;
}
