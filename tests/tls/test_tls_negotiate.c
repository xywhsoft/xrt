#include "../test.h"



/* 通用标识选择必须遵循本地偏好并保留私有线路值。 */
static void testTlsIdsSelect(void)
{
	static const uint8 OfferedData[] = {
		0x00, 0x17, 0xFE, 0x01, 0x00, 0x1D
	};
	static const uint16 Preferred[] = { 0xFE01, 0x001D };
	xtlsids Offered = { { OfferedData, sizeof(OfferedData) } };
	uint16 iSelected = 0;

	testRequire(xrtTlsIdsSelect(
		&Offered, Preferred,
		sizeof(Preferred) / sizeof(Preferred[0]), &iSelected
	) == XTLS_ITEM_VALUE, "TLS identifier intersection was not found");
	testRequire(iSelected == UINT16_C(0xFE01),
		"TLS identifier selection ignored local preference");
}



/* 版本选择必须支持扩展优先级和缺失扩展的 TLS 1.2 回退。 */
static void testTlsVersionSelect(void)
{
	static const uint8 OfferedData[] = {
		0x03, 0x04, 0x7A, 0x7A, 0x03, 0x03
	};
	static const uint8 VersionExtension[] = {
		0x00, 0x2B, 0x00, 0x07,
		0x06, 0x03, 0x04, 0x7A, 0x7A, 0x03, 0x03
	};
	static const xtlsversion Prefer12[] = {
		XTLS_VERSION_12, XTLS_VERSION_13
	};
	static const xtlsversion Prefer13[] = {
		XTLS_VERSION_13, XTLS_VERSION_12
	};
	xtlsids Offered = { { OfferedData, sizeof(OfferedData) } };
	xtlsclienthello Hello;
	xtlsversion Version = (xtlsversion)0;

	testRequire(xrtTlsVersionSelect(
		&Offered, Prefer12, 2, &Version
	) == XTLS_ITEM_VALUE, "TLS version selection failed");
	testRequire(Version == XTLS_VERSION_12,
		"TLS version selection ignored local preference");

	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Extensions.Data = VersionExtension;
	Hello.Extensions.Size = sizeof(VersionExtension);
	testRequire(xrtTlsClientVersionSelect(
		&Hello, Prefer13, 2, &Version
	) == XTLS_ITEM_VALUE, "ClientHello version selection failed");
	testRequire(Version == XTLS_VERSION_13,
		"ClientHello supported_versions was not used");

	Hello.Extensions = (xbytesview) { NULL, 0 };
	testRequire(xrtTlsClientVersionSelect(
		&Hello, Prefer13, 2, &Version
	) == XTLS_ITEM_VALUE, "legacy ClientHello version selection failed");
	testRequire(Version == XTLS_VERSION_12,
		"missing supported_versions negotiated TLS 1.3");
}



/* 套件选择必须区分版本和 TLS 1.2 身份认证类型。 */
static void testTlsCipherSelect(void)
{
	static const uint8 OfferedData[] = {
		0x13, 0x01, 0xC0, 0x2F, 0xC0, 0x2B, 0xCC, 0xA9
	};
	static const xtlscipher Preferred[] = {
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256,
		XTLS_AES_128_GCM_SHA256,
		XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256
	};
	xtlsids Offered = { { OfferedData, sizeof(OfferedData) } };
	xtlscipher Cipher = (xtlscipher)0;

	testRequire(xrtTlsCipherSelect(
		XTLS_VERSION_12, &Offered, XTLS_IDENTITY_ECDSA_P256,
		Preferred, sizeof(Preferred) / sizeof(Preferred[0]), &Cipher
	) == XTLS_ITEM_VALUE, "TLS 1.2 ECDSA cipher selection failed");
	testRequire(Cipher ==
		XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256,
		"TLS 1.2 cipher selection ignored identity compatibility");

	testRequire(xrtTlsCipherSelect(
		XTLS_VERSION_12, &Offered, XTLS_IDENTITY_RSA,
		Preferred, sizeof(Preferred) / sizeof(Preferred[0]), &Cipher
	) == XTLS_ITEM_VALUE, "TLS 1.2 RSA cipher selection failed");
	testRequire(Cipher == XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		"TLS 1.2 RSA cipher mismatch");

	testRequire(xrtTlsCipherSelect(
		XTLS_VERSION_13, &Offered, XTLS_IDENTITY_NONE,
		Preferred, sizeof(Preferred) / sizeof(Preferred[0]), &Cipher
	) == XTLS_ITEM_VALUE, "TLS 1.3 PSK cipher selection failed");
	testRequire(Cipher == XTLS_AES_128_GCM_SHA256,
		"TLS 1.3 cipher was incorrectly tied to an identity");

	testRequire(xrtTlsCipherCompatible(
		XTLS_VERSION_12,
		XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256,
		XTLS_IDENTITY_ED25519
	), "TLS 1.2 ECDHE_ECDSA rejected an EdDSA identity");
	testRequire(xrtTlsCipherCompatible(
		XTLS_VERSION_12,
		XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256,
		XTLS_IDENTITY_ED448
	), "TLS 1.2 ECDHE_ECDSA rejected an Ed448 identity");
	testRequire(!xrtTlsCipherCompatible(
		XTLS_VERSION_12,
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
		XTLS_IDENTITY_ED25519
	), "TLS 1.2 ECDHE_RSA accepted an EdDSA identity");
}



/* 签名元数据必须完整覆盖公开线路方案并保留版本与摘要差异。 */
static void testTlsSignatureInfo(void)
{
	static const struct {
		xtlssignature Signature;
		xtlsidentitytype Identity;
		uint8 HashSize;
		xtlsversion Maximum;
	} Cases[] = {
		{
			XTLS_SIGNATURE_RSA_PKCS1_SHA256,
			XTLS_IDENTITY_RSA, 32u, XTLS_VERSION_12
		},
		{
			XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
			XTLS_IDENTITY_ECDSA_P256, 32u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PKCS1_SHA384,
			XTLS_IDENTITY_RSA, 48u, XTLS_VERSION_12
		},
		{
			XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,
			XTLS_IDENTITY_ECDSA_P384, 48u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PKCS1_SHA512,
			XTLS_IDENTITY_RSA, 64u, XTLS_VERSION_12
		},
		{
			XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512,
			XTLS_IDENTITY_ECDSA_P521, 64u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
			XTLS_IDENTITY_RSA, 32u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384,
			XTLS_IDENTITY_RSA, 48u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512,
			XTLS_IDENTITY_RSA, 64u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_ED25519,
			XTLS_IDENTITY_ED25519, 0u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_ED448,
			XTLS_IDENTITY_ED448, 0u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA256,
			XTLS_IDENTITY_RSA_PSS, 32u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA384,
			XTLS_IDENTITY_RSA_PSS, 48u, XTLS_VERSION_13
		},
		{
			XTLS_SIGNATURE_RSA_PSS_PSS_SHA512,
			XTLS_IDENTITY_RSA_PSS, 64u, XTLS_VERSION_13
		}
	};

	for ( size_t i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		const xtlssignatureinfo* pInfo = xrtTlsSignatureInfo(Cases[i].Signature);

		testRequire((pInfo != NULL) &&
			(pInfo->Signature == Cases[i].Signature) &&
			(pInfo->Identity == Cases[i].Identity) &&
			(pInfo->HashSize == Cases[i].HashSize) &&
			(pInfo->Minimum == XTLS_VERSION_12) &&
			(pInfo->Maximum == Cases[i].Maximum),
			"TLS signature metadata mismatch");
	}

	xrtClearError();
	testRequire(xrtTlsSignatureInfo((xtlssignature)0x7777) == NULL,
		"unknown TLS signature metadata was published");
	testRequire(xrtGetError() == NULL,
		"unknown TLS signature metadata query set an error");
}



/* 签名选择必须应用 TLS 1.2 与 TLS 1.3 不同的身份规则。 */
static void testTlsSignatureSelect(void)
{
	static const uint8 OfferedData[] = {
		0x04, 0x01, 0x08, 0x04, 0x04, 0x03,
		0x05, 0x03, 0x08, 0x07, 0x08, 0x09
	};
	static const xtlssignature Preferred[] = {
		XTLS_SIGNATURE_RSA_PKCS1_SHA256,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
		XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,
		XTLS_SIGNATURE_ED25519,
		XTLS_SIGNATURE_RSA_PSS_PSS_SHA256
	};
	xtlsids Offered = { { OfferedData, sizeof(OfferedData) } };
	xtlssignature Signature = (xtlssignature)0;

	testRequire(xrtTlsSignatureSelect(
		XTLS_VERSION_13, &Offered, XTLS_IDENTITY_RSA,
		Preferred, sizeof(Preferred) / sizeof(Preferred[0]), &Signature
	) == XTLS_ITEM_VALUE, "TLS 1.3 RSA signature selection failed");
	testRequire(Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		"TLS 1.3 selected forbidden RSA PKCS#1 handshake signing");

	testRequire(xrtTlsSignatureSelect(
		XTLS_VERSION_12, &Offered, XTLS_IDENTITY_ECDSA_P384,
		Preferred, sizeof(Preferred) / sizeof(Preferred[0]), &Signature
	) == XTLS_ITEM_VALUE, "TLS 1.2 ECDSA signature selection failed");
	testRequire(Signature == XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
		"TLS 1.2 incorrectly tied the signature pair to the key curve");

	testRequire(xrtTlsSignatureSelect(
		XTLS_VERSION_12, &Offered, XTLS_IDENTITY_ED25519,
		Preferred, sizeof(Preferred) / sizeof(Preferred[0]), &Signature
	) == XTLS_ITEM_VALUE, "TLS 1.2 Ed25519 signature selection failed");
	testRequire(Signature == XTLS_SIGNATURE_ED25519,
		"TLS 1.2 Ed25519 signature mismatch");

	testRequire(!xrtTlsSignatureCompatible(
		XTLS_VERSION_13, XTLS_SIGNATURE_RSA_PKCS1_SHA256,
		XTLS_IDENTITY_RSA
	), "TLS 1.3 accepted RSA PKCS#1 handshake signing");
	testRequire(!xrtTlsSignatureCompatible(
		XTLS_VERSION_13, XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
		XTLS_IDENTITY_ECDSA_P384
	), "TLS 1.3 accepted an ECDSA curve mismatch");
}



/* 密钥共享策略必须明确区分直接选择和 HelloRetryRequest。 */
static void testTlsKeyShareSelect(void)
{
	static const uint8 GroupData[] = {
		0x00, 0x1D, 0x00, 0x17, 0x00, 0x18
	};
	static const uint8 KeyShares[] = {
		0x00, 0x06,
		0x00, 0x17, 0x00, 0x02, 0x04, 0x42
	};
	static const uint16 Preferred[] = {
		XTLS_GROUP_X25519, XTLS_GROUP_SECP256R1,
		XTLS_GROUP_SECP384R1
	};
	xtlsids Groups = { { GroupData, sizeof(GroupData) } };
	xtlskeyshareselection Selection;
	xtlskeyshare Found;

	testRequire(xrtTlsKeyShareFind(
		(xbytesview) { KeyShares, sizeof(KeyShares) },
		XTLS_GROUP_SECP256R1, &Found
	) == XTLS_ITEM_VALUE, "TLS key-share lookup failed");
	testRequire((Found.Group == XTLS_GROUP_SECP256R1) &&
		(Found.Key.Size == 2u) && (Found.Key.Data[1] == 0x42),
		"TLS key-share lookup returned the wrong borrowed view");

	testRequire(xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { KeyShares, sizeof(KeyShares) },
		Preferred, 3, XTLS_KEY_SHARE_PREFER_GROUP, &Selection
	) == XTLS_ITEM_VALUE, "group-first TLS key-share selection failed");
	testRequire(Selection.Retry &&
		(Selection.Share.Group == XTLS_GROUP_X25519) &&
		(Selection.Share.Key.Size == 0u),
		"group-first TLS key-share selection did not request retry");

	testRequire(xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { KeyShares, sizeof(KeyShares) },
		Preferred, 3, XTLS_KEY_SHARE_PREFER_READY, &Selection
	) == XTLS_ITEM_VALUE, "ready-first TLS key-share selection failed");
	testRequire(!Selection.Retry &&
		(Selection.Share.Group == XTLS_GROUP_SECP256R1) &&
		(Selection.Share.Key.Data == KeyShares + 6u),
		"ready-first TLS key-share selection chose the wrong share");

	testRequire(xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { NULL, 0 },
		Preferred, 3, XTLS_KEY_SHARE_PREFER_READY, &Selection
	) == XTLS_ITEM_VALUE, "absent TLS key-share selection failed");
	testRequire(Selection.Retry &&
		(Selection.Share.Group == XTLS_GROUP_X25519),
		"absent TLS key-share did not request the preferred group");
}



/* 执行 TLS 无状态协商回归。 */
int main(void)
{
	testTlsIdsSelect();
	testTlsVersionSelect();
	testTlsCipherSelect();
	testTlsSignatureInfo();
	testTlsSignatureSelect();
	testTlsKeyShareSelect();
	return 0;
}
