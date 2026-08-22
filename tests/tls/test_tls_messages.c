#include "../test.h"



/* TLS 1.2 与 TLS 1.3 证书链必须零拷贝遍历且不限制条目数量。 */
static void testTlsCertificateMessages(void)
{
	static const uint8 Tls12[] = {
		0, 0, 10,
		0, 0, 3, 1, 2, 3,
		0, 0, 1, 4
	};
	static const uint8 Tls13[] = {
		2, 0xAA, 0xBB, 0, 0, 18,
		0, 0, 3, 1, 2, 3,
		0, 4, 0, 5, 0, 0,
		0, 0, 1, 4, 0, 0
	};
	static const uint8 Empty12[] = { 0, 0, 0 };
	static const uint8 Empty13[] = { 0, 0, 0, 0 };
	xtlscertificatemessage Message;
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;

	testRequire(xrtTlsCertificateParse(
		XTLS_VERSION_12, (xbytesview) { Tls12, sizeof(Tls12) }, &Message
	) && (Message.RequestContext.Size == 0) &&
		xrtTlsCertificateEntries(&Message, &Cursor) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Data.Data == Tls12 + 6u) && (Entry.Data.Size == 3u) &&
		(Entry.Extensions.Size == 0) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Data.Size == 1u) && (Entry.Data.Data[0] == 4u) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_DONE),
		"TLS 1.2 Certificate chain view mismatch");

	testRequire(xrtTlsCertificateParse(
		XTLS_VERSION_13, (xbytesview) { Tls13, sizeof(Tls13) }, &Message
	) && (Message.RequestContext.Size == 2u) &&
		(Message.RequestContext.Data[0] == 0xAAu) &&
		xrtTlsCertificateEntries(&Message, &Cursor) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Data.Size == 3u) && (Entry.Extensions.Size == 4u) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Data.Size == 1u) && (Entry.Extensions.Size == 0) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_DONE),
		"TLS 1.3 Certificate chain view mismatch");

	testRequire(xrtTlsCertificateParse(
		XTLS_VERSION_12, (xbytesview) { Empty12, sizeof(Empty12) }, &Message
	) && xrtTlsCertificateEntries(&Message, &Cursor) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_DONE) &&
		xrtTlsCertificateParse(
			XTLS_VERSION_13,
			(xbytesview) { Empty13, sizeof(Empty13) }, &Message
		) && xrtTlsCertificateEntries(&Message, &Cursor) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_DONE),
		"TLS Certificate parser rejected a structurally empty chain");
}



/* EncryptedExtensions 必须保留扩展并验证服务端 ALPN 与确认字段。 */
static void testTlsEncryptedExtensions(void)
{
	static const uint8 Body[] = {
		0, 13,
		0, 16, 0, 5, 0, 3, 2, 'h', '2',
		0, 0, 0, 0
	};
	xbytesview Extensions;
	xtlsextension Extension;
	xbytesview Protocol;

	testRequire(xrtTlsEncryptedExtensionsParse(
		(xbytesview) { Body, sizeof(Body) }, &Extensions
	) && (Extensions.Data == Body + 2u) &&
		(xrtTlsExtensionsFind(
			Extensions, XTLS_EXTENSION_ALPN, &Extension
		) == XTLS_ITEM_VALUE) &&
		xrtTlsProtocolSelected(Extension.Data, &Protocol) &&
		(Protocol.Size == 2u) &&
		(memcmp(Protocol.Data, "h2", 2u) == 0),
		"TLS EncryptedExtensions view mismatch");
}



/* CertificateVerify、Finished 与 KeyUpdate 必须采用精确长度规则。 */
static void testTlsVerifyMessages(void)
{
	static const uint8 VerifyBody[] = {
		0x08, 0x04, 0, 3, 0xA1, 0xA2, 0xA3
	};
	static const uint8 FinishedBody[] = { 1, 2, 3, 4 };
	static const uint8 UpdateBody[] = { 1 };
	xtlscertificateverify Verify;
	xbytesview Finished;
	xtlskeyupdate Request;

	testRequire(xrtTlsCertificateVerifyParse(
		(xbytesview) { VerifyBody, sizeof(VerifyBody) }, &Verify
	) && (Verify.Scheme == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) &&
		(Verify.Signature.Size == 3u) &&
		(Verify.Signature.Data[2] == 0xA3u),
		"TLS CertificateVerify view mismatch");
	testRequire(xrtTlsFinishedParse(
		(xbytesview) { FinishedBody, sizeof(FinishedBody) },
		sizeof(FinishedBody), &Finished
	) && (Finished.Data == FinishedBody) &&
		(Finished.Size == sizeof(FinishedBody)),
		"TLS Finished view mismatch");
	testRequire(xrtTlsKeyUpdateParse(
		(xbytesview) { UpdateBody, sizeof(UpdateBody) }, &Request
	) && (Request == XTLS_KEY_UPDATE_REQUESTED),
		"TLS KeyUpdate request mismatch");
}



/* 两个 TLS 版本的票据字段必须按各自线路格式发布。 */
static void testTlsSessionTickets(void)
{
	static const uint8 Tls12[] = {
		0, 0, 0x0E, 0x10, 0, 3, 'o', 'l', 'd'
	};
	static const uint8 Tls13[] = {
		0, 0, 0x0E, 0x10,
		1, 2, 3, 4,
		2, 0xAA, 0xBB,
		0, 3, 'n', 'e', 'w',
		0, 8, 0, 42, 0, 4, 0, 0, 0x10, 0
	};
	xtlssessionticket Ticket;

	testRequire(xrtTlsSessionTicketParse(
		XTLS_VERSION_12, (xbytesview) { Tls12, sizeof(Tls12) }, &Ticket
	) && (Ticket.Lifetime == 3600u) && (Ticket.AgeAdd == 0) &&
		(Ticket.Nonce.Size == 0) && (Ticket.Ticket.Size == 3u) &&
		(Ticket.Extensions.Size == 0),
		"TLS 1.2 NewSessionTicket view mismatch");
	testRequire(xrtTlsSessionTicketParse(
		XTLS_VERSION_13, (xbytesview) { Tls13, sizeof(Tls13) }, &Ticket
	) && (Ticket.Lifetime == 3600u) &&
		(Ticket.AgeAdd == UINT32_C(0x01020304)) &&
		(Ticket.Nonce.Size == 2u) && (Ticket.Nonce.Data[1] == 0xBBu) &&
		(Ticket.Ticket.Size == 3u) && (Ticket.Extensions.Size == 8u),
		"TLS 1.3 NewSessionTicket view mismatch");
}



/* 执行 TLS 握手语义消息正向回归。 */
int main(void)
{
	testTlsCertificateMessages();
	testTlsEncryptedExtensions();
	testTlsVerifyMessages();
	testTlsSessionTickets();
	return 0;
}
