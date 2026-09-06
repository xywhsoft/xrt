/*
 * 范例：tls/negotiate_tour —— 协商与扩展解析族（选择器/游标/兼容性）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Ids 工具】  xrtTlsIdsCount / Get / Contain / Select
 *   【扩展向量】  xrtTlsExtensionsInit / Read / Validate / Find
 *   【SNI/ALPN】  xrtTlsServerNames / ServerNamesRead / HostName /
 *                Protocols / ProtocolsRead / ProtocolSelected /
 *                ProtocolFind / ProtocolSelect
 *   【组/签名】   xrtTlsGroups / Signatures / SignatureInfo /
 *                SignatureCompatible / SignatureSelect /
 *                CipherCompatible
 *   【key_share】 xrtTlsClientKeyShares / KeySharesRead /
 *                KeyShareFind / KeyShareSelect
 *   【PSK 族】    xrtTlsPskModes / ClientPsks / PsksRead
 *   【杂项】      xrtTlsLimitsInit / Valid / ContextRetain /
 *                RetryGroup / RetryCookie
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/extension_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   tls: ids count/get/contain/select ok (pick=17)
 *   tls: extensions validate/find/read ok
 *   tls: sni+alpn roundtrip ok
 *   tls: groups/sigs/cipher compatible ok
 *   tls: key-share find/select ok
 *   tls: psk modes+empty-identities ok
 *   tls: limits+context-retain+retry ok
 *
 * 输入字节序列全部手构（16 位向量 + 扩展 TLV + SNI/ALPN
 *   列表），断言"严格解析 + 偏好选择"的每一步。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 网络字节序（大端）16 位写入——TLS 线路格式。 */
static void examplePut16(uint8* p, uint16 v)
{
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)(v & 0xFFu);
}

int main(void)
{
	/* supported_groups 数据：3 个组 0x001d 0x0017 0x0304。 */
	uint8 arrGroups[6];
	/* 扩展向量：supported_groups(10) + ALPN(16)。 */
	uint8 arrExtBlock[32];
	/* SNI server_name_list：一个 host_name "api"。 */
	uint8 arrSni[11];
	/* ALPN ProtocolNameList：["h2", "http/1.1"]。 */
	uint8 arrAlpn[15];
	xtlsids Ids;
	xtlsextensioncursor ExtCursor;
	xtlsextension Extension;
	xtlsservernamecursor SniCursor;
	xtlsservername ServerName;
	xtlsprotocolcursor AlpnCursor;
	xbytesview Protocol;
	xbytesview Host;
	const xtlssignatureinfo* pSigInfo;
	xtlskeysharecursor KsCursor;
	xtlskeyshare Share;
	xtlslimits Limits;
	xtlscontextconfig CtxConfig;
	xtlscontext* pContext = NULL;
	xtlscontext* pRetained = NULL;
	uint16 iValue = 0;
	uint16 iGroup = 0;
	size_t iExtRead = 0;
	size_t iAlpnRead = 0;
	uint16 arrPreferred[2];
	int iResult = 1;

	/* ---- Ids 工具族 ---- */
	examplePut16(arrGroups, 0x001Du);
	examplePut16(arrGroups + 2, 0x0017u);
	examplePut16(arrGroups + 4, 0x0304u);
	Ids.Data = (xbytesview) { arrGroups, 6u };
	if ( (xrtTlsIdsCount(&Ids) != 3u) ||
		!xrtTlsIdsGet(&Ids, 2u, &iValue) ||
		(iValue != 0x0304u) ||
		xrtTlsIdsGet(&Ids, 3u, &iValue) ||
		!xrtTlsIdsContain(&Ids, 0x0017u) ||
		xrtTlsIdsContain(&Ids, 0x00FFu) ) {
		goto Cleanup;
	}
	/* Select：偏好 [0xFFFE(未知), 0x0017] → 选 0x0017。 */
	arrPreferred[0] = 0xFFFEu;
	arrPreferred[1] = 0x0017u;
	{
		uint16 iPicked = 0;

		if ( (xrtTlsIdsSelect(&Ids, arrPreferred, 2u,
				&iPicked) != XTLS_ITEM_VALUE) ||
			(iPicked != 0x0017u) ) {
			goto Cleanup;
		}
		printf("tls: ids count/get/contain/select ok (pick=%X)\n", (unsigned)iPicked);
	}

	/* ---- 扩展向量：组 TLV + ALPN TLV ---- */
	arrAlpn[0] = 0u;   /* 列表长度 13（16 位）：3+10 */
	arrAlpn[1] = 13u;
	arrAlpn[2] = 2u;   /* "h2" */
	arrAlpn[3] = 'h';
	arrAlpn[4] = '2';
	arrAlpn[5] = 9u;   /* "http/1.1" */
	memcpy(arrAlpn + 6, "http/1.1", 9u);
	arrSni[0] = 0u;    /* 列表长度 6（16 位）：1+2+3 */
	arrSni[1] = 6u;
	arrSni[2] = 0u;    /* host_name 类型 */
	arrSni[3] = 0u;    /* 名长 3（16 位） */
	arrSni[4] = 3u;
	memcpy(arrSni + 5, "api", 3u);
	{
		uint8* p = arrExtBlock;

		examplePut16(p, 10u);
		examplePut16(p + 2, 8u);
		examplePut16(p + 4, 6u);
		memcpy(p + 6, arrGroups, 6u);
		p += 12;
		examplePut16(p, 16u);
		examplePut16(p + 2, 15u);
		memcpy(p + 4, arrAlpn, 15u);
	}
	if ( !xrtTlsExtensionsValidate(
			(xbytesview) { arrExtBlock, 31u }) ||
		(xrtTlsExtensionsFind((xbytesview) { arrExtBlock, 31u },
			XTLS_EXTENSION_SUPPORTED_GROUPS,
			&Extension) != XTLS_ITEM_VALUE) ||
		(Extension.Data.Size != 8u) ) {
		goto Cleanup;
	}
	if ( !xrtTlsExtensionsInit(&ExtCursor,
			(xbytesview) { arrExtBlock, 31u }) ) {
		goto Cleanup;
	}
	while ( xrtTlsExtensionsRead(&ExtCursor, &Extension) ==
		XTLS_ITEM_VALUE ) {
		++iExtRead;
	}
	if ( iExtRead != 2u ) {
		goto Cleanup;
	}
	printf("tls: extensions validate/find/read ok\n");

	/* ---- SNI / ALPN 族 ---- */
	if ( !xrtTlsServerNames((xbytesview) { arrSni, 8u },
			&SniCursor) ||
		(xrtTlsServerNamesRead(&SniCursor, &ServerName) !=
			XTLS_ITEM_VALUE) ||
		(ServerName.Type != 0u) ||
		(ServerName.Name.Size != 3u) ||
		(memcmp(ServerName.Name.Data, "api", 3u) != 0) ||
		(xrtTlsServerNamesRead(&SniCursor, &ServerName) !=
			XTLS_ITEM_DONE) ||
		(xrtTlsHostName((xbytesview) { arrSni, 8u },
			&Host) != XTLS_ITEM_VALUE) ||
		(Host.Size != 3u) ) {
		goto Cleanup;
	}
	if ( !xrtTlsProtocols((xbytesview) { arrAlpn, 15u },
			&AlpnCursor) ) {
		goto Cleanup;
	}
	while ( xrtTlsProtocolsRead(&AlpnCursor, &Protocol) ==
		XTLS_ITEM_VALUE ) {
		++iAlpnRead;
	}
	if ( (iAlpnRead != 2u) ||
		(xrtTlsProtocolFind((xbytesview) { arrAlpn, 15u },
			(xbytesview) { (cbytes)"h2", 2u }) !=
			XTLS_ITEM_VALUE) ) {
		goto Cleanup;
	}
	/* ProtocolSelected 输入仍是完整 ProtocolNameList 外框
	 * [16 位列表长 + 条目]——单选 ["x"] 即 {0,2, 1,'x'}。 */
	{
		uint8 arrOne[4] = { 0u, 2u, 1u, 'x' };

		if ( !xrtTlsProtocolSelected(
				(xbytesview) { arrOne, 4u },
				&Protocol) ||
			(Protocol.Size != 1u) ) {
			goto Cleanup;
		}
	}
	/* ProtocolSelect：Offered[h2,http/1.1] × Pref[zz,h2] → h2。 */
	{
		{
			uint8 arrFull[8];

			arrFull[0] = 0u;
			arrFull[1] = 6u;
			arrFull[2] = 2u;
			arrFull[3] = 'z';
			arrFull[4] = 'z';
			arrFull[5] = 2u;
			arrFull[6] = 'h';
			arrFull[7] = '2';
			if ( (xrtTlsProtocolSelect(
					(xbytesview) { arrAlpn, 15u },
					(xbytesview) { arrFull, 8u },
					&Protocol) != XTLS_ITEM_VALUE) ||
				(Protocol.Size != 2u) ||
				(memcmp(Protocol.Data, "h2", 2u) != 0) ) {
				goto Cleanup;
			}
		}
	}
	printf("tls: sni+alpn roundtrip ok\n");

	/* ---- 组/签名/密码套件兼容性 ---- */
	/* Groups/Signatures 的输入是"扩展数据"形态：
	 * [16 位向量长 + 条目]——重取 supported_groups 扩展负载。 */
	if ( (xrtTlsExtensionsFind(
			(xbytesview) { arrExtBlock, 31u },
			XTLS_EXTENSION_SUPPORTED_GROUPS,
			&Extension) != XTLS_ITEM_VALUE) ||
		!xrtTlsGroups(Extension.Data, &Ids) ||
		(xrtTlsIdsCount(&Ids) != 3u) ||
		!xrtTlsIdsContain(&Ids, 0x001Du) ) {
		goto Cleanup;
	}
	{
		uint8 arrSigData[6];

		examplePut16(arrSigData, 4u);      /* 向量长 4 */
		examplePut16(arrSigData + 2, 0x0804u);
		examplePut16(arrSigData + 4, 0x0803u);
		if ( !xrtTlsSignatures((xbytesview) { arrSigData, 6u },
				&Ids) ||
			(xrtTlsIdsCount(&Ids) != 2u) ) {
			goto Cleanup;
		}
	}
	/* SignatureInfo/Compatible：rsa_pss_rsae_sha256 = 0x0804。 */
	pSigInfo = xrtTlsSignatureInfo(XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256);
	if ( (pSigInfo == NULL) ||
		!xrtTlsSignatureCompatible(XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
			XTLS_IDENTITY_RSA) ) {
		goto Cleanup;
	}
	/* SignatureSelect：版本在前、偏好列表是签名枚举数组。 */
	{
		uint8 arrRaw[4];
		static const xtlssignature arrPref[2] = {
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384,
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
		};
		xtlssignature Picked;

		examplePut16(arrRaw, 0x0804u);
		examplePut16(arrRaw + 2, 0x0803u);
		Ids.Data = (xbytesview) { arrRaw, 4u };
		if ( (xrtTlsSignatureSelect(XTLS_VERSION_13, &Ids,
				XTLS_IDENTITY_RSA, arrPref, 2u,
				&Picked) != XTLS_ITEM_VALUE) ||
			(Picked !=
				XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ) {
			goto Cleanup;
		}
	}
	/* CipherCompatible：1.3 套件（0x1301）对任意身份真；
	 * 1.2 的 ECDHE_RSA 套件做身份判定（配 RSA 真、配 ECDSA 假、
	 * 配错版本直接假——套件的版本字段先于身份检查）。 */
	if ( !xrtTlsCipherCompatible(XTLS_VERSION_13,
			XTLS_AES_128_GCM_SHA256,
			XTLS_IDENTITY_ECDSA_P256) ||
		!xrtTlsCipherCompatible((xtlsversion)0x0303u,
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
			XTLS_IDENTITY_RSA) ||
		xrtTlsCipherCompatible((xtlsversion)0x0303u,
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
			XTLS_IDENTITY_ECDSA_P256) ||
		xrtTlsCipherCompatible(XTLS_VERSION_13,
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
			XTLS_IDENTITY_RSA) ) {
		goto Cleanup;
	}
	printf("tls: groups/sigs/cipher compatible ok\n");

	/* ---- key_share 族：空列表路径 + Find 未命中 ---- */
	{
		uint8 arrKsEmpty[2];

		examplePut16(arrKsEmpty, 0u);  /* client_shares 空 */
		if ( !xrtTlsClientKeyShares(
				(xbytesview) { arrKsEmpty, 2u },
				&KsCursor) ||
			(xrtTlsKeySharesRead(&KsCursor, &Share) !=
				XTLS_ITEM_DONE) ) {
			goto Cleanup;
		}
		if ( xrtTlsKeyShareFind(
				(xbytesview) { arrKsEmpty, 2u },
				0x001Du, &Share) != XTLS_ITEM_DONE ) {
			goto Cleanup;
		}
		/* KeyShareSelect：空共享 + 组偏好 → 无共同组（DONE）。 */
		{
			xtlskeyshareselection Selection;

			if ( xrtTlsKeyShareSelect(&Ids,
					(xbytesview) { arrKsEmpty, 2u },
					arrPreferred, 2u,
					XTLS_KEY_SHARE_PREFER_READY,
					&Selection) == XTLS_ITEM_ERROR ) {
				goto Cleanup;
			}
			(void)Selection.Retry;
		}
	}
	printf("tls: key-share find/select ok\n");

	/* ---- PSK 族：psk_key_exchange_modes 是 [1 字节向量长 + 模式]。 */
	{
		uint8 arrModes[2] = { 1u, 1u };  /* 长 1，模式 1 */

		if ( !xrtTlsPskModes((xbytesview) { arrModes, 2u },
				&Protocol) ||
			(Protocol.Size != 1u) ||
			(Protocol.Data[0] != 1u) ) {
			goto Cleanup;
		}
	}
	/* ClientPsks/PsksRead：空 identities + 空 binders 列表。
	 * identities 向量最少 7 字节（一个最小 identity）——空列表
	 * 直接用最小合法 identity + 对应 binder 构造一条 PSK。 */
	{
		/* 50 字节布局：identities 向量 13 = [id-len(2)+id(7)+age(4)]，
		 * binders 向量最少 33 = [binder-len(1)+32 零]。 */
		uint8 arrPsk[50];
		xtlspskcursor PskCursor;
		xtlspsk Psk;

		memset(arrPsk, 0, sizeof(arrPsk));
		examplePut16(arrPsk, 13u);           /* identities 向量长 13 */
		examplePut16(arrPsk + 2, 7u);        /* identity 长 7 */
		examplePut16(arrPsk + 15, 33u);      /* binders 向量长 33 */
		arrPsk[17] = 32u;  /* binder 条目：1 字节长 + 32 零 */
		if ( !xrtTlsClientPsks((xbytesview) { arrPsk, 50u },
				&PskCursor) ) {
			goto Cleanup;
		}
		if ( (xrtTlsPsksRead(&PskCursor, &Psk) !=
				XTLS_ITEM_VALUE) ||
			(Psk.Identity.Size != 7u) ||
			(Psk.Binder.Size != 32u) ||
			(Psk.Binder.Size != 32u) ||
			(xrtTlsPsksRead(&PskCursor, &Psk) !=
				XTLS_ITEM_DONE) ) {
			goto Cleanup;
		}
	}
	printf("tls: psk modes+empty-identities ok\n");

	/* ---- 杂项：Limits + ContextRetain + Retry ---- */
	xrtTlsLimitsInit(&Limits);
	if ( !xrtTlsLimitsValid(&Limits) ) {
		goto Cleanup;
	}
	Limits.FeedLimit = 1u;
	if ( xrtTlsLimitsValid(&Limits) ) {
		goto Cleanup;  /* 预算不足一个记录 → 非法 */
	}
	xrtTlsContextConfigInit(&CtxConfig);
	pContext = xrtTlsContextCreate(&CtxConfig);
	if ( (pContext == NULL) ||
		((pRetained = xrtTlsContextRetain(pContext)) ==
			NULL) ) {
		goto Cleanup;
	}
	/* RetryGroup/Cookie：HRR 扩展数据 [group, cookie_len, cookie]。 */
	{
		uint8 arrRetry[5];

		examplePut16(arrRetry, 0x001Du);
		examplePut16(arrRetry + 2, 1u);
		arrRetry[4] = 0xABu;
		if ( !xrtTlsRetryGroup((xbytesview) { arrRetry, 2u },
				&iGroup) ||
			(iGroup != 0x001Du) ||
			!xrtTlsRetryCookie((xbytesview) { arrRetry + 2,
				3u }, &Protocol) ||
			(Protocol.Size != 1u) ) {
			goto Cleanup;
		}
	}
	printf("tls: limits+context-retain+retry ok\n");
	iResult = 0;

Cleanup:
	if ( pRetained != NULL ) {
		xrtTlsContextRelease(pRetained);
	}
	if ( pContext != NULL ) {
		xrtTlsContextRelease(pContext);
	}
	return iResult;
}
