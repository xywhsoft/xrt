#include "tls_hello_vectors.h"



/* ClientHello 失败必须保留调用方输出并设置结构化错误。 */
static void testTlsClientHelloReject(const uint8* pData, size_t iSize)
{
	xtlsclienthello Hello;
	xtlsclienthello Before;

	memset(&Hello, 0xA5, sizeof(Hello));
	Before = Hello;
	xrtClearError();
	testRequire(!xrtTlsClientHelloParse(
		(xbytesview) { pData, iSize }, &Hello
	), "malformed TLS ClientHello was accepted");
	testRequire(memcmp(&Hello, &Before, sizeof(Hello)) == 0,
		"failed TLS ClientHello parse changed output");
	testRequire(xrtGetError() != NULL,
		"failed TLS ClientHello parse did not set an error");
}



/* ServerHello 失败必须保留调用方输出并设置结构化错误。 */
static void testTlsServerHelloReject(const uint8* pData, size_t iSize)
{
	xtlsserverhello Hello;
	xtlsserverhello Before;

	memset(&Hello, 0xA5, sizeof(Hello));
	Before = Hello;
	xrtClearError();
	testRequire(!xrtTlsServerHelloParse(
		(xbytesview) { pData, iSize }, &Hello
	), "malformed TLS ServerHello was accepted");
	testRequire(memcmp(&Hello, &Before, sizeof(Hello)) == 0,
		"failed TLS ServerHello parse changed output");
	testRequire(xrtGetError() != NULL,
		"failed TLS ServerHello parse did not set an error");
}



/* 扩展向量必须拒绝截断项和已知或未知的重复类型。 */
static void testTlsExtensionRejects(void)
{
	static const uint8 Truncated[] = { 0, 16, 0, 2, 0 };
	static const uint8 Duplicate[] = {
		0x12, 0x34, 0, 0,
		0x12, 0x34, 0, 0
	};
	xtlsextensioncursor Cursor;
	xtlsextensioncursor BeforeCursor;
	xtlsextension Extension;
	xtlsextension BeforeExtension;

	testRequire(!xrtTlsExtensionsValidate(
		(xbytesview) { Truncated, sizeof(Truncated) }
	), "truncated TLS extension vector was accepted");
	testRequire(!xrtTlsExtensionsValidate(
		(xbytesview) { Duplicate, sizeof(Duplicate) }
	), "duplicate unknown TLS extension was accepted");

	testRequire(xrtTlsExtensionsInit(
		&Cursor, (xbytesview) { Duplicate, sizeof(Duplicate) }
	) && (xrtTlsExtensionsRead(
		&Cursor, &Extension
	) == XTLS_ITEM_VALUE),
		"TLS duplicate-extension setup failed");
	BeforeCursor = Cursor;
	memset(&Extension, 0xA5, sizeof(Extension));
	BeforeExtension = Extension;
	testRequire(xrtTlsExtensionsRead(
		&Cursor, &Extension
	) == XTLS_ITEM_ERROR, "duplicate TLS extension was not rejected");
	testRequire((memcmp(&Cursor, &BeforeCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Extension, &BeforeExtension, sizeof(Extension)) == 0),
		"duplicate TLS extension violated failure atomicity");
}



/* SNI、ALPN 和 16 位列表必须执行精确线路长度检查。 */
static void testTlsListRejects(void)
{
	static const uint8 EmptyName[] = { 0, 3, 0, 0, 0 };
	static const uint8 DuplicateName[] = {
		0, 8, 0, 0, 1, 'a', 0, 0, 1, 'b'
	};
	static const uint8 EmptyAlpn[] = { 0, 1, 0 };
	static const uint8 MultiSelected[] = {
		0, 6, 2, 'h', '2', 2, 'h', '3'
	};
	static const uint8 DuplicateGroups[] = {
		0, 4, 0, 29, 0, 29
	};
	static const uint8 OddGroups[] = { 0, 3, 0, 29, 0 };
	static const uint8 DuplicateVersions[] = { 4, 3, 4, 3, 4 };
	xtlsservernamecursor Names;
	xtlsprotocolcursor Protocols;
	xbytesview Protocol;
	xtlsids Ids;

	testRequire(!xrtTlsServerNames(
		(xbytesview) { EmptyName, sizeof(EmptyName) }, &Names
	), "empty TLS SNI name was accepted");
	testRequire(!xrtTlsServerNames(
		(xbytesview) { DuplicateName, sizeof(DuplicateName) }, &Names
	), "duplicate TLS SNI name type was accepted");
	testRequire(!xrtTlsProtocols(
		(xbytesview) { EmptyAlpn, sizeof(EmptyAlpn) }, &Protocols
	), "empty TLS ALPN protocol was accepted");
	testRequire(!xrtTlsProtocolSelected(
		(xbytesview) { MultiSelected, sizeof(MultiSelected) }, &Protocol
	), "multiple server ALPN protocols were accepted");
	testRequire(!xrtTlsGroups(
		(xbytesview) { DuplicateGroups, sizeof(DuplicateGroups) }, &Ids
	), "duplicate TLS named group was accepted");
	testRequire(!xrtTlsGroups(
		(xbytesview) { OddGroups, sizeof(OddGroups) }, &Ids
	), "odd TLS named-group vector was accepted");
	testRequire(!xrtTlsClientVersions(
		(xbytesview) { DuplicateVersions, sizeof(DuplicateVersions) }, &Ids
	), "duplicate TLS supported version was accepted");
}



/* key_share 必须拒绝重复组、空密钥和尾随字节。 */
static void testTlsKeyShareRejects(void)
{
	static const uint8 Duplicate[] = {
		0, 10,
		0, 29, 0, 1, 1,
		0, 29, 0, 1, 2
	};
	static const uint8 Empty[] = { 0, 4, 0, 29, 0, 0 };
	static const uint8 ServerTrailing[] = {
		0, 29, 0, 1, 0xA5, 0x5A
	};
	static const uint8 RetryTrailing[] = { 0, 29, 0 };
	xtlskeysharecursor Shares;
	xtlskeyshare Share;
	uint16 iGroup;

	testRequire(!xrtTlsClientKeyShares(
		(xbytesview) { Duplicate, sizeof(Duplicate) }, &Shares
	), "duplicate TLS client key-share group was accepted");
	testRequire(!xrtTlsClientKeyShares(
		(xbytesview) { Empty, sizeof(Empty) }, &Shares
	), "empty TLS client key share was accepted");
	testRequire(!xrtTlsServerKeyShare(
		(xbytesview) { ServerTrailing, sizeof(ServerTrailing) }, &Share
	), "TLS server key share with trailing bytes was accepted");
	testRequire(!xrtTlsRetryGroup(
		(xbytesview) { RetryTrailing, sizeof(RetryTrailing) }, &iGroup
	), "TLS retry group with trailing bytes was accepted");
}



/* ClientHello 必须拒绝所有非完整前缀和 TLS 1.3 兼容字段错误。 */
static void testTlsClientHelloRejects(void)
{
	uint8 Data[272];
	uint8 Mutated[272];
	size_t iExtensionsOffset;
	size_t iSize = testTlsClientHelloVector(
		Data, sizeof(Data), &iExtensionsOffset
	);
	size_t iExtensionSize;

	for ( size_t i = 0; i < iSize; i++ ) {
		if ( i != iExtensionsOffset ) {
			testTlsClientHelloReject(Data, i);
		}
	}

	memcpy(Mutated, Data, iSize);
	iExtensionSize = ((size_t)Mutated[iExtensionsOffset] << 8u) |
		Mutated[iExtensionsOffset + 1u];
	testTlsHelloWrite16(
		Mutated + iExtensionsOffset, (uint16)(iExtensionSize + 1u)
	);
	testTlsClientHelloReject(Mutated, iSize);

	memcpy(Mutated, Data, iSize);
	Mutated[36] = 5;
	testTlsClientHelloReject(Mutated, iSize);

	memcpy(Mutated, Data, iSize);
	Mutated[44] = 1;
	testTlsClientHelloReject(Mutated, iSize);

	memcpy(Mutated, Data, iSize);
	memmove(
		Mutated + iExtensionsOffset + 1u,
		Mutated + iExtensionsOffset,
		iSize - iExtensionsOffset
	);
	Mutated[43] = 2;
	Mutated[44] = 0;
	Mutated[45] = 1;
	testTlsClientHelloReject(Mutated, iSize + 1u);

	memcpy(Mutated, Data, iSize);
	testTlsHelloAppendExtension(
		Mutated, sizeof(Mutated), &iSize, 0x1234, NULL, 0
	);
	testTlsHelloWrite16(
		Mutated + iExtensionsOffset, (uint16)(iExtensionSize + 4u)
	);
	testTlsClientHelloReject(Mutated, iSize);
}



/* ServerHello 必须拒绝截断、宽松 key_share 和错误 Retry 形态。 */
static void testTlsServerHelloRejects(void)
{
	uint8 Data[128];
	uint8 Retry[128];
	uint8 Mutated[128];
	size_t iExtensionsOffset;
	size_t iSize = testTlsServerHelloVector(
		Data, sizeof(Data), false, &iExtensionsOffset
	);
	size_t iRetrySize = testTlsServerHelloVector(
		Retry, sizeof(Retry), true, NULL
	);
	size_t iExtensionSize;

	for ( size_t i = 0; i < iSize; i++ ) {
		if ( i != iExtensionsOffset ) {
			testTlsServerHelloReject(Data, i);
		}
	}

	memcpy(Mutated, Data, iSize);
	iExtensionSize = ((size_t)Mutated[iExtensionsOffset] << 8u) |
		Mutated[iExtensionsOffset + 1u];
	testTlsHelloWrite16(
		Mutated + iExtensionsOffset, (uint16)(iExtensionSize + 1u)
	);
	testTlsServerHelloReject(Mutated, iSize);

	memcpy(Mutated, Data, iSize);
	Mutated[37] = 1;
	testTlsServerHelloReject(Mutated, iSize);

	memcpy(Mutated, Data, iSize);
	memcpy(Mutated + 2u, Retry + 2u, 32u);
	testTlsServerHelloReject(Mutated, iSize);

	memcpy(Mutated, Retry, iRetrySize);
	memcpy(Mutated + 2u, Data + 2u, 32u);
	testTlsServerHelloReject(Mutated, iRetrySize);
}



/* 执行 TLS Hello 与核心扩展负向回归。 */
int main(void)
{
	testTlsExtensionRejects();
	testTlsListRejects();
	testTlsKeyShareRejects();
	testTlsClientHelloRejects();
	testTlsServerHelloRejects();
	return 0;
}
