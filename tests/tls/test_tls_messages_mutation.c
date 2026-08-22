#include "../test.h"



/* 所有证书消息截断点都必须失败且保持输出不变。 */
static void testTlsCertificateTruncation(void)
{
	static const uint8 Body[] = {
		1, 0xAA, 0, 0, 11,
		0, 0, 2, 1, 2,
		0, 4, 0, 5, 0, 0
	};
	xtlscertificatemessage Message;
	xtlscertificatemessage Before;

	for ( size_t i = 0; i < sizeof(Body); i++ ) {
		memset(&Message, 0xA5, sizeof(Message));
		Before = Message;
		testRequire(!xrtTlsCertificateParse(
			XTLS_VERSION_13, (xbytesview) { Body, i }, &Message
		) && (memcmp(&Message, &Before, sizeof(Message)) == 0),
			"truncated TLS Certificate changed output or parsed");
	}
	testRequire(xrtTlsCertificateParse(
		XTLS_VERSION_13, (xbytesview) { Body, sizeof(Body) }, &Message
	), "complete TLS Certificate mutation vector did not parse");
}



/* 所有票据截断点都必须失败且保持输出不变。 */
static void testTlsTicketTruncation(void)
{
	static const uint8 Body[] = {
		0, 0, 0, 1, 1, 2, 3, 4,
		1, 0xAA, 0, 2, 0xBB, 0xCC,
		0, 8, 0, 42, 0, 4, 0, 0, 0, 1
	};
	xtlssessionticket Ticket;
	xtlssessionticket Before;

	for ( size_t i = 0; i < sizeof(Body); i++ ) {
		memset(&Ticket, 0xA5, sizeof(Ticket));
		Before = Ticket;
		testRequire(!xrtTlsSessionTicketParse(
			XTLS_VERSION_13, (xbytesview) { Body, i }, &Ticket
		) && (memcmp(&Ticket, &Before, sizeof(Ticket)) == 0),
			"truncated TLS NewSessionTicket changed output or parsed");
	}
	testRequire(xrtTlsSessionTicketParse(
		XTLS_VERSION_13, (xbytesview) { Body, sizeof(Body) }, &Ticket
	), "complete TLS NewSessionTicket mutation vector did not parse");
}



/* 长度字段的单字节变异不得被宽松的大于等于检查吞掉。 */
static void testTlsLengthMutation(void)
{
	uint8 Verify[] = { 8, 4, 0, 3, 1, 2, 3 };
	uint8 Extensions[] = { 0, 4, 0x12, 0x34, 0, 0 };
	xtlscertificateverify ParsedVerify;
	xbytesview ParsedExtensions;

	for ( size_t i = 2u; i < 4u; i++ ) {
		uint8 Saved = Verify[i];

		Verify[i] ^= 1u;
		testRequire(!xrtTlsCertificateVerifyParse(
			(xbytesview) { Verify, sizeof(Verify) }, &ParsedVerify
		), "mutated CertificateVerify length parsed");
		Verify[i] = Saved;
	}
	for ( size_t i = 0; i < 2u; i++ ) {
		uint8 Saved = Extensions[i];

		Extensions[i] ^= 1u;
		testRequire(!xrtTlsEncryptedExtensionsParse(
			(xbytesview) { Extensions, sizeof(Extensions) }, &ParsedExtensions
		), "mutated EncryptedExtensions length parsed");
		Extensions[i] = Saved;
	}
}



/* 执行 TLS 消息确定性变异回归。 */
int main(void)
{
	testTlsCertificateTruncation();
	testTlsTicketTruncation();
	testTlsLengthMutation();
	return 0;
}
