#include "../test.h"



/* 证书消息必须拒绝宽松总长、空条目和截断条目扩展。 */
static void testTlsCertificateRejects(void)
{
	static const uint8 WrongList[] = { 0, 0, 0, 0xAA };
	static const uint8 EmptyEntry[] = { 0, 0, 3, 0, 0, 0 };
	static const uint8 MissingExtensions[] = {
		0, 0, 0, 4, 0, 0, 1, 0xAA
	};
	static const uint8 DuplicateExtensions[] = {
		0, 0, 0, 14,
		0, 0, 1, 0xAA, 0, 8,
		0, 5, 0, 0, 0, 5, 0, 0
	};
	xtlscertificatemessage Message;
	xtlscertificatemessage Before;

	memset(&Message, 0xA5, sizeof(Message));
	Before = Message;
	testRequire(!xrtTlsCertificateParse(
		XTLS_VERSION_12,
		(xbytesview) { WrongList, sizeof(WrongList) }, &Message
	) && (memcmp(&Message, &Before, sizeof(Message)) == 0),
		"TLS Certificate parser accepted trailing body data");
	testRequire(!xrtTlsCertificateParse(
		XTLS_VERSION_12,
		(xbytesview) { EmptyEntry, sizeof(EmptyEntry) }, &Message
	) && !xrtTlsCertificateParse(
		XTLS_VERSION_13,
		(xbytesview) { MissingExtensions, sizeof(MissingExtensions) }, &Message
	) && !xrtTlsCertificateParse(
		XTLS_VERSION_13,
		(xbytesview) { DuplicateExtensions, sizeof(DuplicateExtensions) },
		&Message
	), "TLS Certificate parser accepted an invalid entry");
}



/* EncryptedExtensions 必须拒绝尾随、重复和畸形已知扩展。 */
static void testTlsEncryptedExtensionsRejects(void)
{
	static const uint8 Trailing[] = { 0, 0, 0 };
	static const uint8 Duplicate[] = {
		0, 8, 0, 0, 0, 0, 0, 0, 0, 0
	};
	static const uint8 BadAlpn[] = {
		0, 8, 0, 16, 0, 4, 0, 3, 1, 'h'
	};
	static const uint8 BadAck[] = {
		0, 5, 0, 0, 0, 1, 1
	};
	xbytesview Output;
	xbytesview Before;

	memset(&Output, 0xA5, sizeof(Output));
	Before = Output;
	testRequire(!xrtTlsEncryptedExtensionsParse(
		(xbytesview) { Trailing, sizeof(Trailing) }, &Output
	) && (memcmp(&Output, &Before, sizeof(Output)) == 0) &&
		!xrtTlsEncryptedExtensionsParse(
			(xbytesview) { Duplicate, sizeof(Duplicate) }, &Output
		) && !xrtTlsEncryptedExtensionsParse(
			(xbytesview) { BadAlpn, sizeof(BadAlpn) }, &Output
		) && !xrtTlsEncryptedExtensionsParse(
			(xbytesview) { BadAck, sizeof(BadAck) }, &Output
		), "TLS EncryptedExtensions parser accepted invalid fields");
}



/* 验证类消息必须拒绝空签名、尾随数据和非法请求值。 */
static void testTlsVerifyRejects(void)
{
	static const uint8 EmptySignature[] = { 8, 4, 0, 0 };
	static const uint8 TrailingSignature[] = { 8, 4, 0, 1, 1, 2 };
	static const uint8 Finished[] = { 1, 2, 3, 4 };
	static const uint8 BadUpdate[] = { 2 };
	xtlscertificateverify Verify;
	xbytesview Data;
	xtlskeyupdate Request;

	testRequire(!xrtTlsCertificateVerifyParse(
		(xbytesview) { EmptySignature, sizeof(EmptySignature) }, &Verify
	) && !xrtTlsCertificateVerifyParse(
		(xbytesview) { TrailingSignature, sizeof(TrailingSignature) }, &Verify
	) && !xrtTlsFinishedParse(
		(xbytesview) { Finished, sizeof(Finished) }, 3u, &Data
	) && !xrtTlsKeyUpdateParse(
		(xbytesview) { BadUpdate, sizeof(BadUpdate) }, &Request
	), "TLS verification message parser accepted invalid data");
}



/* TLS 1.3 票据必须拒绝超长寿命、空票据和畸形 early_data。 */
static void testTlsSessionTicketRejects(void)
{
	static const uint8 LongLifetime[] = {
		0, 9, 0x3A, 0x81, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0
	};
	static const uint8 EmptyTicket[] = {
		0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	static const uint8 BadEarlyData[] = {
		0, 0, 0, 1, 0, 0, 0, 0, 0,
		0, 1, 1,
		0, 7, 0, 42, 0, 3, 0, 0, 1
	};
	static const uint8 Tls12Trailing[] = {
		0, 0, 0, 1, 0, 1, 1, 2
	};
	xtlssessionticket Ticket;

	testRequire(!xrtTlsSessionTicketParse(
		XTLS_VERSION_13,
		(xbytesview) { LongLifetime, sizeof(LongLifetime) }, &Ticket
	) && !xrtTlsSessionTicketParse(
		XTLS_VERSION_13,
		(xbytesview) { EmptyTicket, sizeof(EmptyTicket) }, &Ticket
	) && !xrtTlsSessionTicketParse(
		XTLS_VERSION_13,
		(xbytesview) { BadEarlyData, sizeof(BadEarlyData) }, &Ticket
	) && !xrtTlsSessionTicketParse(
		XTLS_VERSION_12,
		(xbytesview) { Tls12Trailing, sizeof(Tls12Trailing) }, &Ticket
	), "TLS NewSessionTicket parser accepted invalid fields");
}



/* 执行 TLS 握手语义消息负向回归。 */
int main(void)
{
	testTlsCertificateRejects();
	testTlsEncryptedExtensionsRejects();
	testTlsVerifyRejects();
	testTlsSessionTicketRejects();
	return 0;
}
