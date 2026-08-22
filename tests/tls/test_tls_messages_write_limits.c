#include "../test.h"



/* 证书编码不得保留旧版固定链数量或小证书缓冲限制。 */
static void testTlsCertificateScale(void)
{
	static const uint8 Byte = 0xA5;
	xtlscertificateentry Entries[40];
	uint8* pLargeCertificate;
	uint8* pBody;
	xtlscertificatemessage Message;
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;
	xtlsitemresult Result;
	size_t iRequired;
	size_t iCount = 0;

	memset(Entries, 0, sizeof(Entries));
	for ( size_t i = 0; i < 40u; i++ ) {
		Entries[i].Data = (xbytesview) { &Byte, 1u };
	}
	iRequired = xrtTlsCertificateSize(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Entries, 40u
	);
	pBody = (uint8*)malloc(iRequired);
	testRequire((pBody != NULL) && xrtTlsCertificateEncode(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Entries, 40u,
		pBody, iRequired
	) && xrtTlsCertificateParse(
		XTLS_VERSION_13, (xbytesview) { pBody, iRequired }, &Message
	) && xrtTlsCertificateEntries(&Message, &Cursor),
		"TLS Certificate writer rejected a forty-entry chain");
	while ( (Result = xrtTlsCertificatesRead(
		&Cursor, &Entry
	)) == XTLS_ITEM_VALUE ) {
		iCount++;
	}
	testRequire((Result == XTLS_ITEM_DONE) && (iCount == 40u),
		"TLS Certificate cursor imposed a hidden chain count limit");
	free(pBody);

	pLargeCertificate = (uint8*)malloc(70000u);
	testRequire(pLargeCertificate != NULL,
		"large TLS Certificate test allocation failed");
	memset(pLargeCertificate, 0x5A, 70000u);
	Entries[0].Data = (xbytesview) { pLargeCertificate, 70000u };
	iRequired = xrtTlsCertificateSize(
		XTLS_VERSION_12, (xbytesview) { NULL, 0 }, Entries, 1u
	);
	pBody = (uint8*)malloc(iRequired);
	testRequire((pBody != NULL) && xrtTlsCertificateEncode(
		XTLS_VERSION_12, (xbytesview) { NULL, 0 }, Entries, 1u,
		pBody, iRequired
	) && xrtTlsCertificateParse(
		XTLS_VERSION_12, (xbytesview) { pBody, iRequired }, &Message
	), "TLS Certificate writer retained a 16-bit certificate limit");
	free(pBody);
	free(pLargeCertificate);
}



/* 所有多字段编码失败必须保持目标缓冲不变。 */
static void testTlsMessageWriteAtomicity(void)
{
	static const uint8 Certificate[] = { 1, 2, 3 };
	static const uint8 TicketData[] = { 4, 5, 6 };
	xtlscertificateentry Entry;
	xtlssessionticket Ticket;
	uint8 Output[64];
	uint8 Before[64];
	size_t iRequired;

	memset(&Entry, 0, sizeof(Entry));
	Entry.Data = (xbytesview) { Certificate, sizeof(Certificate) };
	iRequired = xrtTlsCertificateSize(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, &Entry, 1u
	);
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtTlsCertificateEncode(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, &Entry, 1u,
		Output, iRequired - 1u
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"short TLS Certificate output changed its buffer");

	memset(&Ticket, 0, sizeof(Ticket));
	Ticket.Version = XTLS_VERSION_13;
	Ticket.Lifetime = 1u;
	Ticket.Ticket = (xbytesview) { TicketData, sizeof(TicketData) };
	iRequired = xrtTlsSessionTicketSize(&Ticket);
	testRequire(!xrtTlsSessionTicketEncode(
		&Ticket, Output, iRequired - 1u
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"short TLS NewSessionTicket output changed its buffer");

	Entry.Data = (xbytesview) { Output + 8u, sizeof(Certificate) };
	testRequire(!xrtTlsCertificateEncode(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, &Entry, 1u,
		Output, sizeof(Output)
	) && (memcmp(Output, Before, sizeof(Output)) == 0),
		"overlapping TLS Certificate input changed its output");
}



/* 描述结构位于输出区域时必须先快照元数据再写线路前缀。 */
static void testTlsMessageDescriptorAlias(void)
{
	static const uint8 Signature[] = { 1, 2, 3 };
	static const uint8 TicketData[] = { 4, 5, 6 };
	union {
		xtlscertificateverify Verify;
		uint8 Data[64];
	} VerifyStorage;
	union {
		xtlssessionticket Ticket;
		uint8 Data[96];
	} TicketStorage;
	xtlscertificateverify ParsedVerify;
	xtlssessionticket ParsedTicket;
	size_t iRequired;

	memset(&VerifyStorage, 0, sizeof(VerifyStorage));
	VerifyStorage.Verify.Scheme = XTLS_SIGNATURE_ED25519;
	VerifyStorage.Verify.Signature = (xbytesview) {
		Signature, sizeof(Signature)
	};
	iRequired = xrtTlsCertificateVerifySize(&VerifyStorage.Verify);
	testRequire(xrtTlsCertificateVerifyEncode(
		&VerifyStorage.Verify, VerifyStorage.Data, sizeof(VerifyStorage.Data)
	) && xrtTlsCertificateVerifyParse(
		(xbytesview) { VerifyStorage.Data, iRequired }, &ParsedVerify
	) && (ParsedVerify.Scheme == XTLS_SIGNATURE_ED25519) &&
		(ParsedVerify.Signature.Size == sizeof(Signature)),
		"TLS CertificateVerify descriptor alias corrupted metadata");

	memset(&TicketStorage, 0, sizeof(TicketStorage));
	TicketStorage.Ticket.Version = XTLS_VERSION_13;
	TicketStorage.Ticket.Lifetime = 1u;
	TicketStorage.Ticket.AgeAdd = 2u;
	TicketStorage.Ticket.Ticket = (xbytesview) {
		TicketData, sizeof(TicketData)
	};
	iRequired = xrtTlsSessionTicketSize(&TicketStorage.Ticket);
	testRequire(xrtTlsSessionTicketEncode(
		&TicketStorage.Ticket, TicketStorage.Data, sizeof(TicketStorage.Data)
	) && xrtTlsSessionTicketParse(
		XTLS_VERSION_13,
		(xbytesview) { TicketStorage.Data, iRequired }, &ParsedTicket
	) && (ParsedTicket.Lifetime == 1u) && (ParsedTicket.AgeAdd == 2u) &&
		(ParsedTicket.Ticket.Size == sizeof(TicketData)),
		"TLS NewSessionTicket descriptor alias corrupted metadata");
}



/* 票据必须支持完整一字节 nonce 上限并拒绝七天以上寿命。 */
static void testTlsTicketLimits(void)
{
	uint8 Nonce[UINT8_MAX];
	uint8 TicketData = 1;
	uint8 Body[300];
	xtlssessionticket Ticket;
	xtlssessionticket Parsed;
	size_t iRequired;

	memset(Nonce, 0x5A, sizeof(Nonce));
	memset(&Ticket, 0, sizeof(Ticket));
	Ticket.Version = XTLS_VERSION_13;
	Ticket.Lifetime = XTLS13_TICKET_LIFETIME_MAX;
	Ticket.Nonce = (xbytesview) { Nonce, sizeof(Nonce) };
	Ticket.Ticket = (xbytesview) { &TicketData, 1u };
	iRequired = xrtTlsSessionTicketSize(&Ticket);
	testRequire((iRequired == 269u) && xrtTlsSessionTicketEncode(
		&Ticket, Body, sizeof(Body)
	) && xrtTlsSessionTicketParse(
		XTLS_VERSION_13, (xbytesview) { Body, iRequired }, &Parsed
	) && (Parsed.Nonce.Size == UINT8_MAX),
		"TLS NewSessionTicket rejected its exact nonce or lifetime limit");
	Ticket.Lifetime++;
	testRequire(xrtTlsSessionTicketSize(&Ticket) == 0,
		"TLS NewSessionTicket accepted a lifetime above seven days");
}



/* 执行 TLS 消息 writer 上限与失败原子性回归。 */
int main(void)
{
	testTlsCertificateScale();
	testTlsMessageWriteAtomicity();
	testTlsMessageDescriptorAlias();
	testTlsTicketLimits();
	return 0;
}
