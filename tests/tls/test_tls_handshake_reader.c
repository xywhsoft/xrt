#include "../test.h"



/* 完整单片消息必须直接借用输入且只消费第一条。 */
static void testTlsHandshakeReaderDirect(void)
{
	static const uint8 Data[] = {
		1, 0, 0, 5, 'h', 'e', 'l', 'l', 'o',
		20, 0, 0, 0
	};
	xtlshandshakereader Reader;
	xtlshandshake Message;
	size_t iConsumed = 0;

	testRequire(xrtTlsHandshakeReaderInit(&Reader, NULL),
		"TLS handshake reader initialization failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data, sizeof(Data) },
		&iConsumed, &Message
	) == XTLS_OK, "direct TLS handshake read failed");
	testRequire((iConsumed == 9u) &&
		(Message.Type == XTLS_HANDSHAKE_CLIENT_HELLO) &&
		(Message.Body.Data == Data + 4u) && (Message.Body.Size == 5u) &&
		(Reader.Data == NULL) && (Reader.Capacity == 0),
		"direct TLS handshake read did not stay zero-copy");

	testRequire(xrtTlsHandshakeReaderRead(
		&Reader,
		(xbytesview) { Data + iConsumed, sizeof(Data) - iConsumed },
		&iConsumed, &Message
	) == XTLS_OK && (iConsumed == 4u) &&
		(Message.Type == XTLS_HANDSHAKE_FINISHED) &&
		(Message.Body.Size == 0),
		"second aggregated TLS handshake read failed");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 每个二段切分都必须恰好重组同一条消息。 */
static void testTlsHandshakeReaderSplits(void)
{
	static const uint8 Data[] = {
		11, 0, 0, 5, 'c', 'e', 'r', 't', 's'
	};

	for ( size_t iSplit = 0; iSplit < sizeof(Data); iSplit++ ) {
		xtlshandshakereader Reader;
		xtlshandshake Message;
		size_t iConsumed = SIZE_MAX;

		testRequire(xrtTlsHandshakeReaderInit(&Reader, NULL),
			"TLS split reader initialization failed");
		testRequire(xrtTlsHandshakeReaderRead(
			&Reader, (xbytesview) { Data, iSplit },
			&iConsumed, &Message
		) == XTLS_AGAIN && (iConsumed == iSplit),
			"TLS first split did not request more input");
		if ( iSplit < XTLS_HANDSHAKE_HEADER_SIZE ) {
			testRequire((Reader.Data == NULL) && (Reader.Capacity == 0),
				"partial TLS handshake header allocated memory");
		}
		testRequire(xrtTlsHandshakeReaderRead(
			&Reader,
			(xbytesview) { Data + iSplit, sizeof(Data) - iSplit },
			&iConsumed, &Message
		) == XTLS_OK && (iConsumed == sizeof(Data) - iSplit) &&
			(Message.Type == XTLS_HANDSHAKE_CERTIFICATE) &&
			(Message.Body.Size == 5u) &&
			(memcmp(Message.Body.Data, "certs", 5u) == 0),
			"TLS two-part handshake reassembly failed");
		xrtTlsHandshakeReaderUnit(&Reader);
	}
}



/* 字节级分片不得丢失正文或提前发布消息。 */
static void testTlsHandshakeReaderBytes(void)
{
	static const uint8 Data[] = {
		15, 0, 0, 7, 'v', 'e', 'r', 'i', 'f', 'y', '!'
	};
	xtlshandshakereader Reader;
	xtlshandshake Message;

	testRequire(xrtTlsHandshakeReaderInit(&Reader, NULL),
		"TLS byte reader initialization failed");
	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		size_t iConsumed = 0;
		xtlsresult Result = xrtTlsHandshakeReaderRead(
			&Reader, (xbytesview) { Data + i, 1u },
			&iConsumed, &Message
		);

		testRequire(iConsumed == 1u,
			"TLS byte reader consumed the wrong amount");
		testRequire(Result == (i + 1u == sizeof(Data) ?
			XTLS_OK : XTLS_AGAIN),
			"TLS byte reader completed at the wrong boundary");
	}
	testRequire((Message.Type == XTLS_HANDSHAKE_CERTIFICATE_VERIFY) &&
		(Message.Body.Size == 7u) &&
		(memcmp(Message.Body.Data, "verify!", 7u) == 0),
		"TLS byte reader changed the reassembled body");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 第二个输入含后续消息时只能消费当前消息缺少的部分。 */
static void testTlsHandshakeReaderAggregation(void)
{
	static const uint8 Data[] = {
		1, 0, 0, 5, 'h', 'e', 'l', 'l', 'o',
		20, 0, 0, 0
	};
	xtlshandshakereader Reader;
	xtlshandshake Message;
	size_t iConsumed = 0;

	testRequire(xrtTlsHandshakeReaderInit(&Reader, NULL),
		"TLS aggregate reader initialization failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data, 5u }, &iConsumed, &Message
	) == XTLS_AGAIN && (iConsumed == 5u),
		"TLS aggregate reader setup failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data + 5u, sizeof(Data) - 5u },
		&iConsumed, &Message
	) == XTLS_OK && (iConsumed == 4u) &&
		(Message.Body.Size == 5u),
		"TLS aggregate reader consumed the following message");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data + 9u, 4u },
		&iConsumed, &Message
	) == XTLS_OK && (iConsumed == 4u) &&
		(Message.Type == XTLS_HANDSHAKE_FINISHED),
		"TLS aggregate reader did not continue with the following message");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 分片空正文消息必须只使用内联头并保持借用期。 */
static void testTlsHandshakeReaderEmpty(void)
{
	static const uint8 Data[] = { 20, 0, 0, 0 };
	xtlshandshakereader Reader;
	xtlshandshake Message;
	size_t iConsumed = 0;

	testRequire(xrtTlsHandshakeReaderInit(&Reader, NULL),
		"TLS empty reader initialization failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data, 2u }, &iConsumed, &Message
	) == XTLS_AGAIN && (Reader.Data == NULL),
		"TLS empty message partial header failed");
	testRequire(xrtTlsHandshakeReaderRead(
		&Reader, (xbytesview) { Data + 2u, 2u },
		&iConsumed, &Message
	) == XTLS_OK && (Message.Body.Size == 0) &&
		(Reader.Data == NULL) && Reader.Ready,
		"TLS empty message did not use inline header storage");
	xrtTlsHandshakeReaderUnit(&Reader);
}



/* 执行 TLS 握手 reader 正向回归。 */
int main(void)
{
	testTlsHandshakeReaderDirect();
	testTlsHandshakeReaderSplits();
	testTlsHandshakeReaderBytes();
	testTlsHandshakeReaderAggregation();
	testTlsHandshakeReaderEmpty();
	return 0;
}
