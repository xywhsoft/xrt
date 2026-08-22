#ifndef XRT_TEST_TLS_HELLO_VECTORS_H
#define XRT_TEST_TLS_HELLO_VECTORS_H

#include "../test.h"



/* 测试向量使用独立的大端序写入，避免复用待测内部帮助函数。 */
static void testTlsHelloWrite16(uint8* pData, uint16 iValue)
{
	pData[0] = (uint8)(iValue >> 8u);
	pData[1] = (uint8)iValue;
}



/* 向测试 Hello 追加一个已经独立准备好负载的扩展。 */
static void testTlsHelloAppendExtension(
	uint8* pData,
	size_t iCapacity,
	size_t* pOffset,
	uint16 iType,
	const uint8* pValue,
	size_t iValueSize
)
{
	size_t iSize = 4u + iValueSize;

	testRequire((*pOffset <= iCapacity) &&
		(iSize <= iCapacity - *pOffset),
		"TLS hello test vector capacity is too small");
	testTlsHelloWrite16(pData + *pOffset, iType);
	testTlsHelloWrite16(pData + *pOffset + 2u, (uint16)iValueSize);
	if ( iValueSize != 0 ) {
		memcpy(pData + *pOffset + 4u, pValue, iValueSize);
	}
	*pOffset += iSize;
}



/* 构造同时覆盖 SNI、ALPN、版本、组、签名和 key_share 的 ClientHello。 */
static size_t testTlsClientHelloVector(
	uint8* pData,
	size_t iCapacity,
	size_t* pExtensionsOffset
)
{
	static const uint8 Sni[] = {
		0, 14, 0, 0, 11,
		'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'
	};
	static const uint8 Alpn[] = {
		0, 12, 2, 'h', '2', 8,
		'h', 't', 't', 'p', '/', '1', '.', '1'
	};
	static const uint8 Versions[] = { 4, 3, 4, 3, 3 };
	static const uint8 Groups[] = { 0, 4, 0, 29, 0, 23 };
	static const uint8 Signatures[] = {
		0, 6, 8, 7, 4, 3, 8, 4
	};
	static const uint8 PointFormats[] = { 1, 0 };
	static const uint8 PskModes[] = { 1, 1 };
	static const uint8 Unknown[] = { 0xA5 };
	uint8 KeyShare[38];
	size_t iOffset = 0;
	size_t iLengthOffset;
	size_t iStart;

	testRequire(iCapacity >= 256u,
		"TLS ClientHello test vector capacity is too small");
	testTlsHelloWrite16(pData + iOffset, 0x0303);
	iOffset += 2u;
	for ( uint8 i = 0; i < 32u; i++ ) {
		pData[iOffset++] = i;
	}
	pData[iOffset++] = 0;
	testTlsHelloWrite16(pData + iOffset, 6);
	iOffset += 2u;
	testTlsHelloWrite16(pData + iOffset, 0x1301);
	iOffset += 2u;
	testTlsHelloWrite16(pData + iOffset, 0x1302);
	iOffset += 2u;
	testTlsHelloWrite16(pData + iOffset, 0xC02F);
	iOffset += 2u;
	pData[iOffset++] = 1;
	pData[iOffset++] = 0;

	iLengthOffset = iOffset;
	iOffset += 2u;
	iStart = iOffset;
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 0, Sni, sizeof(Sni)
	);
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 16, Alpn, sizeof(Alpn)
	);
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 43, Versions, sizeof(Versions)
	);
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 10, Groups, sizeof(Groups)
	);
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 13, Signatures, sizeof(Signatures)
	);

	testTlsHelloWrite16(KeyShare, 36);
	testTlsHelloWrite16(KeyShare + 2u, 29);
	testTlsHelloWrite16(KeyShare + 4u, 32);
	for ( uint8 i = 0; i < 32u; i++ ) {
		KeyShare[6u + i] = (uint8)(0x80u + i);
	}
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 51, KeyShare, sizeof(KeyShare)
	);
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 45, PskModes, sizeof(PskModes)
	);
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 11, PointFormats,
		sizeof(PointFormats)
	);
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 0x1234, Unknown, sizeof(Unknown)
	);
	testTlsHelloWrite16(
		pData + iLengthOffset, (uint16)(iOffset - iStart)
	);
	if ( pExtensionsOffset != NULL ) {
		*pExtensionsOffset = iLengthOffset;
	}
	return iOffset;
}



/* 构造普通 ServerHello 或使用固定 random 的 HelloRetryRequest。 */
static size_t testTlsServerHelloVector(
	uint8* pData,
	size_t iCapacity,
	bool bRetry,
	size_t* pExtensionsOffset
)
{
	static const uint8 RetryRandom[32] = {
		0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11,
		0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
		0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E,
		0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
	};
	static const uint8 Version[] = { 3, 4 };
	static const uint8 Alpn[] = { 0, 3, 2, 'h', '2' };
	static const uint8 RetryGroup[] = { 0, 29 };
	uint8 KeyShare[36];
	size_t iOffset = 0;
	size_t iLengthOffset;
	size_t iStart;

	testRequire(iCapacity >= 128u,
		"TLS ServerHello test vector capacity is too small");
	testTlsHelloWrite16(pData + iOffset, 0x0303);
	iOffset += 2u;
	if ( bRetry ) {
		memcpy(pData + iOffset, RetryRandom, sizeof(RetryRandom));
		iOffset += sizeof(RetryRandom);
	} else {
		for ( uint8 i = 0; i < 32u; i++ ) {
			pData[iOffset++] = (uint8)(0x40u + i);
		}
	}
	pData[iOffset++] = 0;
	testTlsHelloWrite16(pData + iOffset, 0x1301);
	iOffset += 2u;
	pData[iOffset++] = 0;

	iLengthOffset = iOffset;
	iOffset += 2u;
	iStart = iOffset;
	testTlsHelloAppendExtension(
		pData, iCapacity, &iOffset, 43, Version, sizeof(Version)
	);
	if ( bRetry ) {
		testTlsHelloAppendExtension(
			pData, iCapacity, &iOffset, 51,
			RetryGroup, sizeof(RetryGroup)
		);
	} else {
		testTlsHelloWrite16(KeyShare, 29);
		testTlsHelloWrite16(KeyShare + 2u, 32);
		for ( uint8 i = 0; i < 32u; i++ ) {
			KeyShare[4u + i] = (uint8)(0xC0u + i);
		}
		testTlsHelloAppendExtension(
			pData, iCapacity, &iOffset, 51, KeyShare, sizeof(KeyShare)
		);
		testTlsHelloAppendExtension(
			pData, iCapacity, &iOffset, 16, Alpn, sizeof(Alpn)
		);
	}
	testTlsHelloWrite16(
		pData + iLengthOffset, (uint16)(iOffset - iStart)
	);
	if ( pExtensionsOffset != NULL ) {
		*pExtensionsOffset = iLengthOffset;
	}
	return iOffset;
}

#endif
