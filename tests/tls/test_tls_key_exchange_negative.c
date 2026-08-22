#include "../test.h"



/* 返回当前测试构建中第一个可用命名组。 */
static const xtlsgroupinfo* testTlsAvailableGroup(void)
{
	static const uint16 Groups[] = {
		XTLS_GROUP_X25519,
		XTLS_GROUP_X448,
		XTLS_GROUP_SECP256R1,
		XTLS_GROUP_SECP384R1
	};

	for ( size_t i = 0; i < (sizeof(Groups) / sizeof(Groups[0])); i++ ) {
		if ( xrtTlsGroupAvailable(Groups[i]) ) {
			return xrtTlsGroupInfo(Groups[i]);
		}
	}
	return NULL;
}



/* 验证未知组查询无副作用，操作路径则返回结构化 unsupported。 */
static void testTlsUnknownGroup(void)
{
	uint8 Buffer[97];

	xrtClearError();
	testRequire((xrtTlsGroupInfo(UINT16_C(0xFE01)) == NULL) &&
		!xrtTlsGroupAvailable(UINT16_C(0xFE01)) &&
		(xrtGetError() == NULL),
		"unknown TLS named-group query set an error");
	memset(Buffer, 0x5A, sizeof(Buffer));
	testRequire(!xrtTlsKeyShareGenerate(
		UINT16_C(0xFE01), Buffer, sizeof(Buffer),
		Buffer + 56u, sizeof(Buffer) - 56u
	) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == XTLS_ERROR_KEY_EXCHANGE),
		"unknown TLS named-group operation was not structured");
}



/* 验证裁掉的已知后端不会伪装成协议未知组。 */
static void testTlsTrimmedGroups(void)
{
	static const uint16 Groups[] = {
		XTLS_GROUP_X25519,
		XTLS_GROUP_X448,
		XTLS_GROUP_SECP256R1,
		XTLS_GROUP_SECP384R1
	};
	uint8 Private[56];
	uint8 Public[97];

	for ( size_t i = 0; i < (sizeof(Groups) / sizeof(Groups[0])); i++ ) {
		const xtlsgroupinfo* pInfo = xrtTlsGroupInfo(Groups[i]);

		if ( xrtTlsGroupAvailable(Groups[i]) ) {
			continue;
		}
		memset(Private, 0x61, sizeof(Private));
		memset(Public, 0x62, sizeof(Public));
		testRequire((pInfo != NULL) && !xrtTlsKeyShareGenerate(
			Groups[i], Private, sizeof(Private), Public, sizeof(Public)
		) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
			(Private[0] == 0x61) && (Public[0] == 0x62),
			"trimmed TLS named-group backend changed output");
	}
}



/* 验证生成 API 在指针、容量和重叠错误上保持输出不变。 */
static void testTlsGenerateBoundaries(const xtlsgroupinfo* pInfo)
{
	uint8 Buffer[160];
	uint8 Before[160];

	memset(Buffer, 0x73, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	testRequire(!xrtTlsKeyShareGenerate(
		pInfo->Group, NULL, pInfo->PrivateSize,
		Buffer + 56u, pInfo->PublicSize
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"TLS key generation accepted a null private output");
	testRequire(!xrtTlsKeyShareGenerate(
		pInfo->Group, Buffer, pInfo->PrivateSize - 1u,
		Buffer + 56u, pInfo->PublicSize
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"TLS key generation accepted a short private buffer");
	testRequire(!xrtTlsKeyShareGenerate(
		pInfo->Group, Buffer, pInfo->PrivateSize,
		Buffer + 56u, pInfo->PublicSize - 1u
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"TLS key generation accepted a short public buffer");
	testRequire(!xrtTlsKeyShareGenerate(
		pInfo->Group, Buffer, pInfo->PrivateSize,
		Buffer + 1u, pInfo->PublicSize
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"TLS key generation accepted overlapping outputs");
}



/* 验证派生 API 严格区分本地参数、对端线路错误和输出容量。 */
static void testTlsDeriveBoundaries(const xtlsgroupinfo* pInfo)
{
	uint8 Private[56];
	uint8 Public[97];
	uint8 Shared[56];
	uint8 Before[56];

	testRequire(xrtTlsKeyShareGenerate(
		pInfo->Group, Private, sizeof(Private), Public, sizeof(Public)
	), "TLS key share for negative tests was not generated");
	memset(Shared, 0x84, sizeof(Shared));
	memcpy(Before, Shared, sizeof(Shared));
	testRequire(!xrtTlsKeyShareDerive(
		pInfo->Group,
		(xbytesview) { Private, pInfo->PrivateSize - 1u },
		(xbytesview) { Public, pInfo->PublicSize },
		Shared, sizeof(Shared)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(Shared, Before, sizeof(Shared)) == 0),
		"TLS key derivation accepted the wrong private-key size");
	testRequire(!xrtTlsKeyShareDerive(
		pInfo->Group,
		(xbytesview) { Private, pInfo->PrivateSize },
		(xbytesview) { Public, pInfo->PublicSize - 1u },
		Shared, sizeof(Shared)
	) && (xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(memcmp(Shared, Before, sizeof(Shared)) == 0),
		"TLS key derivation accepted the wrong peer-key size");
	testRequire(!xrtTlsKeyShareDerive(
		pInfo->Group,
		(xbytesview) { Private, pInfo->PrivateSize },
		(xbytesview) { Public, pInfo->PublicSize },
		Shared, pInfo->SharedSize - 1u
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(memcmp(Shared, Before, sizeof(Shared)) == 0),
		"TLS key derivation accepted a short output buffer");

	memset(Public, 0, pInfo->PublicSize);
	testRequire(!xrtTlsKeyShareDerive(
		pInfo->Group,
		(xbytesview) { Private, pInfo->PrivateSize },
		(xbytesview) { Public, pInfo->PublicSize },
		Shared, sizeof(Shared)
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_KEY_EXCHANGE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.tls") == 0) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())), "xrt.crypto"
		) == 0) && (memcmp(Shared, Before, sizeof(Shared)) == 0),
		"invalid peer key was not preserved as a TLS crypto cause");
}



/* 执行 TLS 密钥交换错误、裁剪和失败原子边界回归。 */
int main(void)
{
	const xtlsgroupinfo* pInfo;

	testTlsUnknownGroup();
	testTlsTrimmedGroups();
	pInfo = testTlsAvailableGroup();
	if ( pInfo != NULL ) {
		testTlsGenerateBoundaries(pInfo);
		testTlsDeriveBoundaries(pInfo);
	}
	return 0;
}
