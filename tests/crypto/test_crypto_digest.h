#ifndef XRT_TEST_CRYPTO_DIGEST_H
#define XRT_TEST_CRYPTO_DIGEST_H



/* 把单个十六进制字符转换为半字节。 */
static uint8 testCryptoHexDigit(char iDigit)
{
	if ( (iDigit >= '0') && (iDigit <= '9') ) {
		return (uint8)(iDigit - '0');
	}
	if ( (iDigit >= 'a') && (iDigit <= 'f') ) {
		return (uint8)(iDigit - 'a' + 10);
	}
	return (uint8)(iDigit - 'A' + 10);
}



/* 把紧凑十六进制文本解码到固定长度测试缓冲。 */
static inline void testCryptoDecode(
	uint8* pOutput,
	size_t iSize,
	cstr sHex,
	cstr sMessage
)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		pOutput[i] = (uint8)(
			(testCryptoHexDigit(sHex[i * 2u]) << 4u) |
			testCryptoHexDigit(sHex[(i * 2u) + 1u])
		);
	}
	testRequire(sHex[iSize * 2u] == '\0', sMessage);
}



/* 对照紧凑十六进制字符串验证摘要，避免测试向量重复展开。 */
static inline void testCryptoDigest(
	const uint8* pDigest,
	size_t iSize,
	cstr sExpected,
	cstr sMessage
)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		uint8 iExpected = (uint8)(
			(testCryptoHexDigit(sExpected[i * 2]) << 4u) |
			testCryptoHexDigit(sExpected[(i * 2) + 1])
		);

		testRequire(pDigest[i] == iExpected, sMessage);
	}
	testRequire(sExpected[iSize * 2] == '\0',
		"digest vector has the wrong size");
}

#endif
