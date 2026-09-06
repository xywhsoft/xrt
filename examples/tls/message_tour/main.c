/*
 * 范例：tls/message_tour —— 握手消息编码/解析族 + 流式 Reader
 * ----------------------------------------------------------------
 * 演示 API：
 *   【通用框架】  xrtTlsHandshakeSize / HandshakeEncode /
 *                HandshakeParse（分片感知）
 *                xrtTlsExtensionSize / ExtensionEncode /
 *                ExtensionParse
 *                xrtTlsRecordSize
 *                xrtTlsAlertName / HandshakeName / ExtensionName
 *   【消息族】    xrtTlsAlertEncode / AlertParse
 *                xrtTlsFinishedEncode / FinishedParse
 *                xrtTlsKeyUpdateEncode / KeyUpdateParse
 *                xrtTlsCertificateVerifySize / Encode / Parse
 *                xrtTlsEncryptedExtensionsSize / Encode / Parse
 *                xrtTlsSessionTicketSize / Encode / Parse
 *                xrtTlsCertificateStatusSize / Encode / Parse
 *                xrtTlsCompressedCertificateSize / Encode / Parse
 *                xrtTls12ClientKeyExchangeSize / Encode / Parse
 *                xrtTls12CertificateRequestSize / Encode / Parse
 *                xrtTls13CertificateRequestSize / Encode / Parse
 *                xrtTls13CertificateVerifyContentSize / Encode
 *   【扩展解析】  xrtTlsClientVersions / ServerVersion /
 *                ServerKeyShare / ServerPsk
 *                xrtTlsClientVersionSelect
 *                xrtTlsAuthorities / AuthoritiesRead /
 *                AuthoritiesSize / AuthoritiesEncode
 *   【流式 Reader】 xrtTlsHandshakeReaderConfigInit /
 *                ReaderInit / ReaderRead / ReaderRequired /
 *                ReaderReset / ReaderUnit（跨分片重组）
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/message_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   tls: frame handshake/ext/record ok names ok
 *   tls: messages alert+finished+keyupdate ok
 *   tls: verify+ee+ticket+status+compressed ok
 *   tls: tls12+tls13 cr/cke/cvc ok
 *   tls: ext-parsers versions/keyshare/psk ok
 *   tls: ext-parsers authorities ok
 *   tls: reader split-consume ok
 *
 * 编码→解析闭环：每条消息 Encode 后用对应 Parse 读回核对；
 *   Reader 把一条消息切成 3 分片验证渐进重组。
 * 线路格式要点（本例实测确认）：
 *   - Handshake/Extension Parse 的 Required 是"下次调用前至少
 *     要凑够的字节数"：头(4 字节)未齐时给 4，头已齐给整条大小。
 *   - EE 与 CR13 的扩展入参是裸记录列表（无外层向量长），
 *     CR13 必须含 signature_algorithms 记录。
 *   - Hello.Extensions 视图同样不含外层向量长。
 *   - CertificateStatus 响应用 24 位向量；CompressedCertificate
 *     压缩流无向量前缀；12 CKE 公钥用 8 位向量。
 *   - Authorities 条目用 16 位长度，外框 16 位总长。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

static void examplePut16(uint8* p, uint16 v)
{
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)(v & 0xFFu);
}

static void examplePut24(uint8* p, size_t v)
{
	p[0] = (uint8)(v >> 16);
	p[1] = (uint8)(v >> 8);
	p[2] = (uint8)v;
}

int main(void)
{
	static uint8 arrOut[512];
	uint8 arrBody[16];
	uint8 arrHandshake[32];
	xtlshandshake Handshake;
	xtlshandshake ReaderMsg;
	xtlsextension Extension;
	xtlsalertlevel AlertLevel;
	xtlsalert Alert;
	xtlskeyupdate KeyUpdate;
	xtlscertificateverify Verify;
	xtlscertificateverify VerifyParsed;
	xtlssessionticket Ticket;
	xtlssessionticket TicketParsed;
	xtlscertificatestatusmessage Status;
	xtlscertificatestatusmessage StatusParsed;
	xtlscompressedcertificate Compressed;
	xtlscompressedcertificate CompressedParsed;
	xtls12certificaterequest Req12;
	xtls12certificaterequest Req12Parsed;
	xtls13certificaterequest Req13Parsed;
	xtlsauthoritycursor AuthCursor;
	xtlshandshakereaderconfig ReaderConfig;
	xtlshandshakereader Reader;
	xbytesview VerifyData;
	xbytesview Extensions;
	xbytesview PublicKey;
	size_t iConsumed;
	size_t iSize;
	uint16 iVersion = 0;
	int iResult = 1;

	/* ---- 通用框架：Handshake / Extension / Record / Names。 ---- */
	/* HandshakeSize(4) = 4 字节头 + 4 字节正文。 */
	if ( (xrtTlsHandshakeSize(4u) != 8u) ||
		(xrtTlsHandshakeSize(SIZE_MAX) != 0u) ) {
		goto Cleanup;
	}
	/* HandshakeEncode：client_hello(1) + 4 字节正文。 */
	memset(arrBody, 0x42u, 4u);
	if ( !xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CLIENT_HELLO,
			(xbytesview) { arrBody, 4u }, arrOut, 8u) ) {
		goto Cleanup;
	}
	/* 分片感知 Parse：前 3 字节凑不齐 4 字节头 → AGAIN +
	 * Required=4（头未齐阶段，见下方扩展处注释）。 */
	{
		size_t iRequired = 0;

		if ( (xrtTlsHandshakeParse(
				(xbytesview) { arrOut, 3u },
				&Handshake, &iRequired) != XTLS_AGAIN) ||
			(iRequired != 4u) ) {
			goto Cleanup;
		}
	}
	/* 完整解析：类型与正文视图。 */
	if ( (xrtTlsHandshakeParse((xbytesview) { arrOut, 8u },
			&Handshake, NULL) != XTLS_OK) ||
		(Handshake.Type != XTLS_HANDSHAKE_CLIENT_HELLO) ||
		(Handshake.Body.Size != 4u) ||
		(Handshake.EncodedSize != 8u) ) {
		goto Cleanup;
	}
	/* 分片感知 Parse：Required 是"下次调用前至少要凑够的字节数"
	 * ——头(4字节)未凑齐时给 4；头已齐、整条未齐时给整条大小。 */
	{
		static const uint8 arrData[3] = { 1u, 2u, 3u };
		size_t iRequired = 0;

		if ( (xrtTlsExtensionSize(3u) != 7u) ||
			!xrtTlsExtensionEncode(
				XTLS_EXTENSION_SUPPORTED_GROUPS,
				(xbytesview) { arrData, 3u }, arrOut,
				7u) ||
			(xrtTlsExtensionParse(
				(xbytesview) { arrOut, 2u },
				&Extension, &iRequired) != XTLS_AGAIN) ||
			(iRequired != 4u) ||
			(xrtTlsExtensionParse(
				(xbytesview) { arrOut, 5u },
				&Extension, &iRequired) != XTLS_AGAIN) ||
			(iRequired != 7u) ||
			(xrtTlsExtensionParse(
				(xbytesview) { arrOut, 7u },
				&Extension, NULL) != XTLS_OK) ||
			(Extension.Data.Size != 3u) ) {
			goto Cleanup;
		}
	}
	/* RecordSize(100) = 5 头 + 100 = 105。 */
	if ( (xrtTlsRecordSize(100u) != 105u) ) {
		goto Cleanup;
	}
	/* Names：已知与未知名。 */
	if ( (strcmp(xrtTlsAlertName(XTLS_ALERT_CLOSE_NOTIFY),
			"close_notify") != 0) ||
		(strcmp(xrtTlsHandshakeName((xtlshandshaketype)9999u),
			"unknown_handshake") != 0) ||
		(strcmp(xrtTlsHandshakeName(
			XTLS_HANDSHAKE_CLIENT_HELLO),
			"client_hello") != 0) ||
		(strcmp(xrtTlsExtensionName(
			XTLS_EXTENSION_SUPPORTED_GROUPS),
			"supported_groups") != 0) ) {
		goto Cleanup;
	}
	(void)arrHandshake;
	printf("tls: frame handshake/ext/record ok names ok\n");

	/* ---- 消息族：Alert / Finished / KeyUpdate。 ---- */
	/* Alert：warning(1) + close_notify(0)。 */
	if ( !xrtTlsAlertEncode(XTLS_ALERT_WARNING,
			XTLS_ALERT_CLOSE_NOTIFY, arrOut, 2u) ||
		!xrtTlsAlertParse((xbytesview) { arrOut, 2u },
			&AlertLevel, &Alert) ||
		(AlertLevel != XTLS_ALERT_WARNING) ||
		(Alert != XTLS_ALERT_CLOSE_NOTIFY) ) {
		goto Cleanup;
	}
	/* Finished：12 字节 verify_data。 */
	memset(arrBody, 0xF1u, 12u);
	if ( !xrtTlsFinishedEncode((xbytesview) { arrBody, 12u },
			arrOut, 12u) ||
		!xrtTlsFinishedParse((xbytesview) { arrOut, 12u },
			12u, &VerifyData) ||
		(VerifyData.Size != 12u) ) {
		goto Cleanup;
	}
	/* KeyUpdate：单字节 requested(1)。 */
	if ( !xrtTlsKeyUpdateEncode(XTLS_KEY_UPDATE_REQUESTED,
			arrOut, 1u) ||
		!xrtTlsKeyUpdateParse((xbytesview) { arrOut, 1u },
			&KeyUpdate) ||
		(KeyUpdate != XTLS_KEY_UPDATE_REQUESTED) ) {
		goto Cleanup;
	}
	printf("tls: messages alert+finished+keyupdate ok\n");

	/* ---- verify / EE / ticket / status / compressed。 ---- */
	/* CertificateVerify：scheme(2) + 签名向量长(2) + 签名(4) = 8。 */
	Verify.Scheme = XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;
	Verify.Signature = (xbytesview) { arrBody, 4u };
	memset(arrBody, 0x77u, 4u);
	iSize = xrtTlsCertificateVerifySize(&Verify);
	if ( (iSize != 8u) ||
		!xrtTlsCertificateVerifyEncode(&Verify, arrOut, 8u) ||
		!xrtTlsCertificateVerifyParse(
			(xbytesview) { arrOut, 8u }, &VerifyParsed) ||
		(VerifyParsed.Scheme !=
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ||
		(VerifyParsed.Signature.Size != 4u) ) {
		goto Cleanup;
	}
	/* EncryptedExtensions：入参是裸扩展列表（无外层向量长），
	 * Encode 补上 2 字节外框。空列表 → 2；SNI 空确认
	 * （[00 00][00 00] 记录）→ 6，且解析回 4 字节列表。 */
	{
		static const uint8 arrAck[4] = { 0u, 0u, 0u, 0u };

		if ( (xrtTlsEncryptedExtensionsSize(
				(xbytesview) { NULL, 0u }) != 2u) ||
			!xrtTlsEncryptedExtensionsEncode(
				(xbytesview) { NULL, 0u }, arrOut, 2u) ||
			!xrtTlsEncryptedExtensionsParse(
				(xbytesview) { arrOut, 2u }, &Extensions) ||
			(Extensions.Size != 0u) ||
			(xrtTlsEncryptedExtensionsSize(
				(xbytesview) { arrAck, 4u }) != 6u) ||
			!xrtTlsEncryptedExtensionsEncode(
				(xbytesview) { arrAck, 4u }, arrOut, 6u) ||
			!xrtTlsEncryptedExtensionsParse(
				(xbytesview) { arrOut, 6u }, &Extensions) ||
			(Extensions.Size != 4u) ) {
			goto Cleanup;
		}
	}
	/* SessionTicket：1.3 带寿命/年龄/nonce。 */
	memset(&Ticket, 0, sizeof(Ticket));
	Ticket.Version = XTLS_VERSION_13;
	Ticket.Lifetime = 7200u;
	Ticket.AgeAdd = 100u;
	Ticket.Nonce = (xbytesview) { arrBody, 4u };
	Ticket.Ticket = (xbytesview) { arrBody + 4, 4u };
	Ticket.Extensions = (xbytesview) { NULL, 0u };
	memset(arrBody, 0x33u, 8u);
	iSize = xrtTlsSessionTicketSize(&Ticket);
	if ( (iSize == 0u) ||
		!xrtTlsSessionTicketEncode(&Ticket, arrOut, iSize) ||
		!xrtTlsSessionTicketParse(XTLS_VERSION_13,
			(xbytesview) { arrOut, iSize },
			&TicketParsed) ||
		(TicketParsed.Lifetime != 7200u) ||
		(TicketParsed.Ticket.Size != 4u) ) {
		goto Cleanup;
	}
	/* CertificateStatus：type(1) + 24 位响应向量长(3) + OCSP(4) = 8。 */
	Status.Type = 1u;
	Status.Response = (xbytesview) { arrBody, 4u };
	memset(arrBody, 0x0Cu, 4u);
	iSize = xrtTlsCertificateStatusSize(&Status);
	if ( (iSize != 8u) ||
		!xrtTlsCertificateStatusEncode(&Status, arrOut, 8u) ||
		!xrtTlsCertificateStatusParse(
			(xbytesview) { arrOut, 8u }, &StatusParsed) ||
		(StatusParsed.Type != 1u) ||
		(StatusParsed.Response.Size != 4u) ) {
		goto Cleanup;
	}
	/* CompressedCertificate：algorithm(2) + 原长 24 位(3) +
	 * 裸压缩流(4，无向量前缀) = 9。 */
	Compressed.Algorithm = 1u;
	Compressed.UncompressedSize = 128u;
	Compressed.Data = (xbytesview) { arrBody, 4u };
	iSize = xrtTlsCompressedCertificateSize(&Compressed);
	if ( (iSize != 9u) ||
		!xrtTlsCompressedCertificateEncode(&Compressed,
			arrOut, 9u) ||
		!xrtTlsCompressedCertificateParse(
			(xbytesview) { arrOut, 9u },
			&CompressedParsed) ||
		(CompressedParsed.Algorithm != 1u) ||
		(CompressedParsed.UncompressedSize != 128u) ) {
		goto Cleanup;
	}
	printf("tls: verify+ee+ticket+status+compressed ok\n");

	/* ---- TLS12 / TLS13 专属消息。 ---- */
	/* 12 CKE：ECDHE 公钥用 8 位向量前缀 → 1 + 4 = 5。 */
	{
		static const uint8 arrKey[4] = { 9u, 8u, 7u, 6u };

		if ( (xrtTls12ClientKeyExchangeSize(
				(xbytesview) { arrKey, 4u }) != 5u) ||
			!xrtTls12ClientKeyExchangeEncode(
				(xbytesview) { arrKey, 4u }, arrOut,
				5u) ||
			!xrtTls12ClientKeyExchangeParse(
				(xbytesview) { arrOut, 5u },
				&PublicKey) ||
			(PublicKey.Size != 4u) ) {
			goto Cleanup;
		}
	}
	/* 12 CR：类型 [1] + 签名 [0804] + 颁发者 [0][0]（空）。 */
	{
		static const uint8 arrTypes[2] = { 1u, 1u };
		static const uint8 arrSigs[4] = { 8u, 4u, 8u, 3u };
		static const uint8 arrAuth[2] = { 0u, 0u };

		Req12.CertificateTypes =
			(xbytesview) { arrTypes, 2u };
		Req12.Signatures.Data.Data = arrSigs;
		Req12.Signatures.Data.Size = 4u;
		Req12.AuthorityData = (xbytesview) { arrAuth, 2u };
		iSize = xrtTls12CertificateRequestSize(&Req12);
		if ( (iSize == 0u) ||
			!xrtTls12CertificateRequestEncode(&Req12,
				arrOut, iSize) ||
			!xrtTls12CertificateRequestParse(
				(xbytesview) { arrOut, iSize },
				&Req12Parsed) ||
			(Req12Parsed.CertificateTypes.Size != 2u) ||
			(Req12Parsed.Signatures.Data.Size != 4u) ) {
			goto Cleanup;
		}
	}
	/* 13 CR：request_context(1) + 扩展向量(2) + 裸扩展列表。
	 * 列表必须含 signature_algorithms——记录
	 * [000D][0006][0004][0804][0803] 共 10 字节，总长 3+1+10=14。 */
	{
		static const uint8 arrCtx[1] = { 0u };
		static const uint8 arr13Ext[10] = {
			0u, 13u, 0u, 6u, 0u, 4u, 8u, 4u, 8u, 3u
		};

		iSize = xrtTls13CertificateRequestSize(
			(xbytesview) { arrCtx, 1u },
			(xbytesview) { arr13Ext, 10u });
		if ( (iSize != 14u) ||
			!xrtTls13CertificateRequestEncode(
				(xbytesview) { arrCtx, 1u },
				(xbytesview) { arr13Ext, 10u }, arrOut,
				14u) ||
			!xrtTls13CertificateRequestParse(
				(xbytesview) { arrOut, 14u },
				&Req13Parsed) ||
			(Req13Parsed.RequestContext.Size != 1u) ||
			(Req13Parsed.Extensions.Size != 10u) ) {
			goto Cleanup;
		}
	}
	/* 13 CVC：server + 32 字节脚本哈希。 */
	{
		uint8 arrHash[32];
		size_t iCvc = xrtTls13CertificateVerifyContentSize(
			XTLS_SERVER, 32u);

		memset(arrHash, 0xEEu, sizeof(arrHash));
		if ( (iCvc == 0u) ||
			!xrtTls13CertificateVerifyContentEncode(
				XTLS_SERVER,
				(xbytesview) { arrHash, 32u },
				arrOut, iCvc) ) {
			goto Cleanup;
		}
	}
	printf("tls: tls12+tls13 cr/cke/cvc ok\n");

	/* ---- 扩展数据解析器：versions / keyshare / psk。 ---- */
	{
		/* ServerVersion：服务端扩展数据 = 裸 16 位选定版本。 */
		static const uint8 arrTwo[2] = { 3u, 4u };
		/* ServerKeyShare：group(2) + 公钥 16 位向量(2+4)，无外框。 */
		static const uint8 arrKs[8] = {
			0u, 0x1Du, 0u, 4u, 1u, 2u, 3u, 4u
		};
		/* ServerPsk：16 位选中的身份索引。 */
		static const uint8 arrPsk[2] = { 0u, 0u };
		/* supported_versions 裸记录（Hello.Extensions 不含外层
		 * 向量长）：[002B][0005][字节数4][0304][0303]。 */
		static const uint8 arrSv[9] = {
			0u, 0x2Bu, 0u, 5u, 4u, 3u, 4u, 3u, 3u
		};
		static const uint8 arrRand[32] = { 0u };
		static const xtlsversion arrPref[2] = {
			XTLS_VERSION_13, XTLS_VERSION_12
		};
		xtlskeyshare Share;
		xtlsids Ids;
		xtlsclienthello Hello;
		xtlsversion Selected = (xtlsversion)0u;

		/* ClientVersions：数据 = 8 位字节数 + 版本列表。 */
		static const uint8 arrCv[5] = { 4u, 3u, 4u, 3u, 3u };

		if ( !xrtTlsServerVersion((xbytesview) { arrTwo, 2u },
				&iVersion) ||
			(iVersion != 0x0304u) ) {
			goto Cleanup;
		}
		if ( !xrtTlsServerKeyShare(
				(xbytesview) { arrKs, 8u }, &Share) ||
			(Share.Group != 0x001Du) ||
			(Share.Key.Size != 4u) ) {
			goto Cleanup;
		}
		if ( !xrtTlsServerPsk((xbytesview) { arrPsk, 2u },
				&iVersion) ) {
			goto Cleanup;
		}
		if ( !xrtTlsClientVersions(
				(xbytesview) { arrCv, 5u },
				&Ids) ||
			(xrtTlsIdsCount(&Ids) != 2u) ) {
			goto Cleanup;
		}
		/* ClientVersionSelect：偏好 [1.3, 1.2] 与 supported_versions
		 * [1.3, 1.2] 取交集 → 1.3(0x0304)。 */
		memset(&Hello, 0, sizeof(Hello));
		Hello.LegacyVersion = XTLS_VERSION_12;
		Hello.Random = (xbytesview) { arrRand, 32u };
		Hello.SessionId = (xbytesview) { arrTwo, 1u };
		Hello.CipherSuites.Data.Data = arrTwo;
		Hello.CipherSuites.Data.Size = 2u;
		Hello.CompressionMethods = (xbytesview) { arrTwo, 1u };
		Hello.Extensions = (xbytesview) { arrSv, 9u };
		if ( (xrtTlsClientVersionSelect(&Hello, arrPref, 2u,
				&Selected) != XTLS_ITEM_VALUE) ||
			(Selected != XTLS_VERSION_13) ) {
			goto Cleanup;
		}
	}
	printf("tls: ext-parsers versions/keyshare/psk ok\n");

	/* ---- Authorities 游标 + Size/Encode。 ---- */
	/* 数据 = 16 位总长外框 + 条目[16 位长 + DER]。
	 * 两项(3+4 字节 DER)：外框 11，总 2+(2+3)+(2+4)=13。 */
	{
		static const uint8 arrGood[13] = {
			0u, 11u,
			0u, 3u, 0xA0u, 3u, 1u,
			0u, 4u, 0xB0u, 4u, 5u, 6u
		};
		static const uint8 arrA[3] = { 0xA0u, 3u, 1u };
		static const uint8 arrB[4] = { 0xB0u, 4u, 5u, 6u };
		static const xbytesview arrTwo[2] = {
			{ arrA, 3u }, { arrB, 4u }
		};
		xbytesview Name;
		int iNames = 0;

		if ( !xrtTlsAuthorities(
				(xbytesview) { arrGood, 13u },
				&AuthCursor) ) {
			goto Cleanup;
		}
		while ( xrtTlsAuthoritiesRead(&AuthCursor,
				&Name) == XTLS_ITEM_VALUE ) {
			++iNames;
		}
		if ( iNames != 2u ) {
			goto Cleanup;
		}
		/* Size/Encode：同样布局 → 13 字节，编码后再游标往返。 */
		if ( (xrtTlsAuthoritiesSize(arrTwo, 2u) != 13u) ||
			!xrtTlsAuthoritiesEncode(arrTwo, 2u,
				arrOut, 13u) ||
			!xrtTlsAuthorities(
				(xbytesview) { arrOut, 13u }, &AuthCursor) ) {
			goto Cleanup;
		}
		iNames = 0;
		while ( xrtTlsAuthoritiesRead(&AuthCursor,
				&Name) == XTLS_ITEM_VALUE ) {
			++iNames;
		}
		if ( iNames != 2u ) {
			goto Cleanup;
		}
	}
	printf("tls: ext-parsers authorities ok\n");

	/* ---- HandshakeReader：跨分片重组。 ---- */
	/* 编码一条 server_hello(2) + 4 字节正文 = 8 字节消息。 */
	memset(arrBody, 0x99u, 4u);
	if ( !xrtTlsHandshakeEncode(XTLS_HANDSHAKE_SERVER_HELLO,
			(xbytesview) { arrBody, 4u }, arrHandshake,
			8u) ) {
		goto Cleanup;
	}
	xrtTlsHandshakeReaderConfigInit(&ReaderConfig);
	if ( !xrtTlsHandshakeReaderInit(&Reader, &ReaderConfig) ) {
		goto Cleanup;
	}
	/* 分片一：1 字节 → AGAIN，头未凑齐 → Required=4。 */
	iConsumed = 0;
	if ( (xrtTlsHandshakeReaderRead(&Reader,
			(xbytesview) { arrHandshake, 1u },
			&iConsumed, &ReaderMsg) != XTLS_AGAIN) ||
		(iConsumed != 1u) ||
		(xrtTlsHandshakeReaderRequired(&Reader) != 4u) ) {
		goto Cleanup;
	}
	/* 分片二：4 字节 → 头已齐，Required 变为整条大小 8。 */
	if ( (xrtTlsHandshakeReaderRead(&Reader,
			(xbytesview) { arrHandshake + 1, 4u },
			&iConsumed, &ReaderMsg) != XTLS_AGAIN) ||
		(iConsumed != 4u) ||
		(xrtTlsHandshakeReaderRequired(&Reader) != 8u) ) {
		goto Cleanup;
	}
	/* 分片三：3 字节 → OK + 类型与正文视图。 */
	if ( (xrtTlsHandshakeReaderRead(&Reader,
			(xbytesview) { arrHandshake + 5, 3u },
			&iConsumed, &ReaderMsg) != XTLS_OK) ||
		(iConsumed != 3u) ||
		(ReaderMsg.Type != XTLS_HANDSHAKE_SERVER_HELLO) ||
		(ReaderMsg.Body.Size != 4u) ) {
		goto Cleanup;
	}
	/* Reset 丢弃当前消息并保留缓冲；Unit 归还。 */
	if ( !xrtTlsHandshakeReaderReset(&Reader) ) {
		goto Cleanup;
	}
	xrtTlsHandshakeReaderUnit(&Reader);
	printf("tls: reader split-consume ok\n");
	iResult = 0;

Cleanup:
	xrtTlsHandshakeReaderUnit(&Reader);
	return iResult;
}
