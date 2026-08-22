#include "../test.h"



/* 验证协议元数据不随密码后端裁剪状态改变。 */
static void testTlsGroupMetadata(void)
{
	static const struct {
		uint16 Group;
		xtlsgroupkind Kind;
		uint16 PrivateSize;
		uint16 PublicSize;
		uint16 SharedSize;
	} Expected[] = {
		{ XTLS_GROUP_X25519, XTLS_GROUP_KIND_XDH, 32, 32, 32 },
		{ XTLS_GROUP_X448, XTLS_GROUP_KIND_XDH, 56, 56, 56 },
		{ XTLS_GROUP_SECP256R1, XTLS_GROUP_KIND_ECDH, 32, 65, 32 },
		{ XTLS_GROUP_SECP384R1, XTLS_GROUP_KIND_ECDH, 48, 97, 48 }
	};

	for ( size_t i = 0; i < (sizeof(Expected) / sizeof(Expected[0])); i++ ) {
		const xtlsgroupinfo* pInfo = xrtTlsGroupInfo(Expected[i].Group);

		testRequire((pInfo != NULL) &&
			(pInfo->Group == Expected[i].Group) &&
			(pInfo->Kind == Expected[i].Kind) &&
			(pInfo->PrivateSize == Expected[i].PrivateSize) &&
			(pInfo->PublicSize == Expected[i].PublicSize) &&
			(pInfo->SharedSize == Expected[i].SharedSize),
			"TLS named-group metadata mismatch");
	}
}



#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || \
	defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X448) || \
	defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || \
	defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384)

/* 验证每个已编译命名组只写精确尺寸，并产生双方一致的秘密。 */
static void testTlsGroupAgreement(uint16 iGroup)
{
	const xtlsgroupinfo* pInfo = xrtTlsGroupInfo(iGroup);
	uint8 PrivateA[56];
	uint8 PrivateB[56];
	uint8 PublicA[97];
	uint8 PublicB[97];
	uint8 SharedA[56];
	uint8 SharedB[56];

	testRequire((pInfo != NULL) && xrtTlsGroupAvailable(iGroup),
		"TLS named-group backend is unavailable");
	memset(PrivateA, 0xA1, sizeof(PrivateA));
	memset(PrivateB, 0xB2, sizeof(PrivateB));
	memset(PublicA, 0xC3, sizeof(PublicA));
	memset(PublicB, 0xD4, sizeof(PublicB));
	testRequire(xrtTlsKeyShareGenerate(
		iGroup, PrivateA, sizeof(PrivateA), PublicA, sizeof(PublicA)
	), "first TLS key share was not generated");
	testRequire(xrtTlsKeyShareGenerate(
		iGroup, PrivateB, sizeof(PrivateB), PublicB, sizeof(PublicB)
	), "second TLS key share was not generated");
	for ( size_t i = pInfo->PrivateSize; i < sizeof(PrivateA); i++ ) {
		testRequire((PrivateA[i] == 0xA1) && (PrivateB[i] == 0xB2),
			"TLS key generation wrote past the private-key size");
	}
	for ( size_t i = pInfo->PublicSize; i < sizeof(PublicA); i++ ) {
		testRequire((PublicA[i] == 0xC3) && (PublicB[i] == 0xD4),
			"TLS key generation wrote past the public-key size");
	}
	if ( pInfo->Kind == XTLS_GROUP_KIND_ECDH ) {
		testRequire((PublicA[0] == 0x04) && (PublicB[0] == 0x04),
			"TLS ECDH key share is not an uncompressed point");
	}

	memset(SharedA, 0xE5, sizeof(SharedA));
	memset(SharedB, 0xF6, sizeof(SharedB));
	testRequire(xrtTlsKeyShareDerive(
		iGroup,
		(xbytesview) { PrivateA, pInfo->PrivateSize },
		(xbytesview) { PublicB, pInfo->PublicSize },
		SharedA, sizeof(SharedA)
	), "first TLS shared secret was not derived");
	testRequire(xrtTlsKeyShareDerive(
		iGroup,
		(xbytesview) { PrivateB, pInfo->PrivateSize },
		(xbytesview) { PublicA, pInfo->PublicSize },
		SharedB, sizeof(SharedB)
	), "second TLS shared secret was not derived");
	testRequire(memcmp(SharedA, SharedB, pInfo->SharedSize) == 0,
		"TLS key exchange produced different shared secrets");
	for ( size_t i = pInfo->SharedSize; i < sizeof(SharedA); i++ ) {
		testRequire((SharedA[i] == 0xE5) && (SharedB[i] == 0xF6),
			"TLS key exchange wrote past the shared-secret size");
	}

	testRequire(xrtTlsKeyShareDerive(
		iGroup,
		(xbytesview) { PrivateA, pInfo->PrivateSize },
		(xbytesview) { PublicB, pInfo->PublicSize },
		PrivateA, sizeof(PrivateA)
	) && (memcmp(PrivateA, SharedA, pInfo->SharedSize) == 0),
		"TLS shared secret could not replace the local private key");
	testRequire(xrtTlsKeyShareDerive(
		iGroup,
		(xbytesview) { PrivateB, pInfo->PrivateSize },
		(xbytesview) { PublicA, pInfo->PublicSize },
		PublicA, sizeof(PublicA)
	) && (memcmp(PublicA, SharedB, pInfo->SharedSize) == 0),
		"TLS shared secret could not replace the peer public key");
}

#endif



/* 验证编译期后端集合与运行时可用性查询完全一致。 */
static void testTlsGroupAvailability(void)
{
	#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)
		testRequire(xrtTlsGroupAvailable(XTLS_GROUP_X25519),
			"compiled X25519 TLS backend is hidden");
		testTlsGroupAgreement(XTLS_GROUP_X25519);
	#else
		testRequire(!xrtTlsGroupAvailable(XTLS_GROUP_X25519),
			"trimmed X25519 TLS backend is visible");
	#endif

	#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X448)
		testRequire(xrtTlsGroupAvailable(XTLS_GROUP_X448),
			"compiled X448 TLS backend is hidden");
		testTlsGroupAgreement(XTLS_GROUP_X448);
	#else
		testRequire(!xrtTlsGroupAvailable(XTLS_GROUP_X448),
			"trimmed X448 TLS backend is visible");
	#endif

	#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256)
		testRequire(xrtTlsGroupAvailable(XTLS_GROUP_SECP256R1),
			"compiled P-256 TLS backend is hidden");
		testTlsGroupAgreement(XTLS_GROUP_SECP256R1);
	#else
		testRequire(!xrtTlsGroupAvailable(XTLS_GROUP_SECP256R1),
			"trimmed P-256 TLS backend is visible");
	#endif

	#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384)
		testRequire(xrtTlsGroupAvailable(XTLS_GROUP_SECP384R1),
			"compiled P-384 TLS backend is hidden");
		testTlsGroupAgreement(XTLS_GROUP_SECP384R1);
	#else
		testRequire(!xrtTlsGroupAvailable(XTLS_GROUP_SECP384R1),
			"trimmed P-384 TLS backend is visible");
	#endif
}



/* 执行 TLS 命名组和密钥交换正常路径回归。 */
int main(void)
{
	testTlsGroupMetadata();
	testTlsGroupAvailability();
	return 0;
}
