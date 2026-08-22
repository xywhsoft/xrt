#include "../test.h"



/* 两个版本的证书链编码必须与严格解析器完整往返。 */
static void testTlsCertificateWrite(void)
{
	static const uint8 CertA[] = { 1, 2, 3 };
	static const uint8 CertB[] = { 4, 5 };
	static const uint8 EntryExtensions[] = { 0, 5, 0, 0 };
	static const uint8 Context[] = { 0xAA, 0xBB };
	xtlscertificateentry Entries[2];
	uint8 Body[64];
	xtlscertificatemessage Message;
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;
	size_t iRequired;

	memset(Entries, 0, sizeof(Entries));
	Entries[0].Data = (xbytesview) { CertA, sizeof(CertA) };
	Entries[1].Data = (xbytesview) { CertB, sizeof(CertB) };
	iRequired = xrtTlsCertificateSize(
		XTLS_VERSION_12, (xbytesview) { NULL, 0 }, Entries, 2u
	);
	testRequire((iRequired == 14u) && xrtTlsCertificateEncode(
		XTLS_VERSION_12, (xbytesview) { NULL, 0 }, Entries, 2u,
		Body, sizeof(Body)
	) && xrtTlsCertificateParse(
		XTLS_VERSION_12, (xbytesview) { Body, iRequired }, &Message
	) && xrtTlsCertificateEntries(&Message, &Cursor) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Data.Size == sizeof(CertA)) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Data.Size == sizeof(CertB)) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_DONE),
		"TLS 1.2 Certificate writer did not round trip");

	Entries[0].Extensions = (xbytesview) {
		EntryExtensions, sizeof(EntryExtensions)
	};
	iRequired = xrtTlsCertificateSize(
		XTLS_VERSION_13,
		(xbytesview) { Context, sizeof(Context) }, Entries, 2u
	);
	testRequire((iRequired == 25u) && xrtTlsCertificateEncode(
		XTLS_VERSION_13,
		(xbytesview) { Context, sizeof(Context) }, Entries, 2u,
		Body, sizeof(Body)
	) && xrtTlsCertificateParse(
		XTLS_VERSION_13, (xbytesview) { Body, iRequired }, &Message
	) && (Message.RequestContext.Size == sizeof(Context)) &&
		xrtTlsCertificateEntries(&Message, &Cursor) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Extensions.Size == sizeof(EntryExtensions)),
		"TLS 1.3 Certificate writer did not round trip");
}



/* 常用服务端握手消息编码必须支持原位负载并完整往返。 */
static void testTlsCommonMessageWrite(void)
{
	uint8 Buffer[64] = {
		0, 16, 0, 5, 0, 3, 2, 'h', '2'
	};
	xtlscertificateverify Verify;
	xtlscertificateverify Parsed;
	xbytesview Extensions;
	xbytesview Finished;
	xtlskeyupdate Request;
	size_t iRequired;

	iRequired = xrtTlsEncryptedExtensionsSize(
		(xbytesview) { Buffer, 9u }
	);
	testRequire((iRequired == 11u) && xrtTlsEncryptedExtensionsEncode(
		(xbytesview) { Buffer, 9u }, Buffer, sizeof(Buffer)
	) && xrtTlsEncryptedExtensionsParse(
		(xbytesview) { Buffer, iRequired }, &Extensions
	) && (Extensions.Size == 9u),
		"TLS EncryptedExtensions writer did not support overlap");

	memcpy(Buffer, "signature", 9u);
	Verify.Scheme = XTLS_SIGNATURE_ED25519;
	Verify.Signature = (xbytesview) { Buffer, 9u };
	iRequired = xrtTlsCertificateVerifySize(&Verify);
	testRequire((iRequired == 13u) && xrtTlsCertificateVerifyEncode(
		&Verify, Buffer, sizeof(Buffer)
	) && xrtTlsCertificateVerifyParse(
		(xbytesview) { Buffer, iRequired }, &Parsed
	) && (Parsed.Scheme == XTLS_SIGNATURE_ED25519) &&
		(Parsed.Signature.Size == 9u) &&
		(memcmp(Parsed.Signature.Data, "signature", 9u) == 0),
		"TLS CertificateVerify writer did not support overlap");

	memcpy(Buffer, "finished", 8u);
	testRequire(xrtTlsFinishedEncode(
		(xbytesview) { Buffer, 8u }, Buffer + 2u, sizeof(Buffer) - 2u
	) && xrtTlsFinishedParse(
		(xbytesview) { Buffer + 2u, 8u }, 8u, &Finished
	) && (memcmp(Finished.Data, "finished", 8u) == 0),
		"TLS Finished writer did not support overlap");
	testRequire(xrtTlsKeyUpdateEncode(
		XTLS_KEY_UPDATE_REQUESTED, Buffer, sizeof(Buffer)
	) && xrtTlsKeyUpdateParse(
		(xbytesview) { Buffer, 1u }, &Request
	) && (Request == XTLS_KEY_UPDATE_REQUESTED),
		"TLS KeyUpdate writer did not round trip");
}



/* TLS 1.2 与 TLS 1.3 票据编码必须保留版本专用字段。 */
static void testTlsSessionTicketWrite(void)
{
	static const uint8 TicketData[] = { 1, 2, 3 };
	static const uint8 Nonce[] = { 4, 5 };
	static const uint8 Extensions[] = {
		0, 42, 0, 4, 0, 0, 0, 16
	};
	uint8 Body[64];
	xtlssessionticket Ticket;
	xtlssessionticket Parsed;
	size_t iRequired;

	memset(&Ticket, 0, sizeof(Ticket));
	Ticket.Version = XTLS_VERSION_12;
	Ticket.Lifetime = 3600u;
	Ticket.Ticket = (xbytesview) { TicketData, sizeof(TicketData) };
	iRequired = xrtTlsSessionTicketSize(&Ticket);
	testRequire((iRequired == 9u) && xrtTlsSessionTicketEncode(
		&Ticket, Body, sizeof(Body)
	) && xrtTlsSessionTicketParse(
		XTLS_VERSION_12, (xbytesview) { Body, iRequired }, &Parsed
	) && (Parsed.Lifetime == 3600u) &&
		(Parsed.Ticket.Size == sizeof(TicketData)),
		"TLS 1.2 NewSessionTicket writer did not round trip");

	Ticket.Version = XTLS_VERSION_13;
	Ticket.AgeAdd = UINT32_C(0x01020304);
	Ticket.Nonce = (xbytesview) { Nonce, sizeof(Nonce) };
	Ticket.Extensions = (xbytesview) { Extensions, sizeof(Extensions) };
	iRequired = xrtTlsSessionTicketSize(&Ticket);
	testRequire((iRequired == 26u) && xrtTlsSessionTicketEncode(
		&Ticket, Body, sizeof(Body)
	) && xrtTlsSessionTicketParse(
		XTLS_VERSION_13, (xbytesview) { Body, iRequired }, &Parsed
	) && (Parsed.AgeAdd == Ticket.AgeAdd) &&
		(Parsed.Nonce.Size == sizeof(Nonce)) &&
		(Parsed.Extensions.Size == sizeof(Extensions)),
		"TLS 1.3 NewSessionTicket writer did not round trip");
}



/* 执行 TLS 握手语义消息 writer 正向回归。 */
int main(void)
{
	testTlsCertificateWrite();
	testTlsCommonMessageWrite();
	testTlsSessionTicketWrite();
	return 0;
}
