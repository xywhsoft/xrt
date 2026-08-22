#include "tls_hello_vectors.h"



/* 检查借用视图是否完全落在原始输入内。 */
static bool testTlsHelloViewWithin(
	xbytesview View,
	const uint8* pData,
	size_t iSize
)
{
	if ( View.Size == 0 ) {
		return (View.Data == NULL) ||
			((View.Data >= pData) && (View.Data <= pData + iSize));
	}
	return (View.Data >= pData) && (View.Data < pData + iSize) &&
		(View.Size <= iSize - (size_t)(View.Data - pData));
}



/* 单个 ClientHello 变异只能成功发布有界视图或原子失败。 */
static bool testTlsClientHelloMutationOne(
	const uint8* pData,
	size_t iSize
)
{
	xtlsclienthello Hello;
	xtlsclienthello Before;

	memset(&Hello, 0xA5, sizeof(Hello));
	Before = Hello;
	xrtClearError();
	if ( xrtTlsClientHelloParse(
		(xbytesview) { pData, iSize }, &Hello
	) ) {
		testRequire(testTlsHelloViewWithin(
			Hello.Random, pData, iSize
		) && testTlsHelloViewWithin(
			Hello.SessionId, pData, iSize
		) && testTlsHelloViewWithin(
			Hello.CipherSuites.Data, pData, iSize
		) && testTlsHelloViewWithin(
			Hello.CompressionMethods, pData, iSize
		) && testTlsHelloViewWithin(
			Hello.Extensions, pData, iSize
		) && xrtTlsExtensionsValidate(Hello.Extensions),
			"mutated TLS ClientHello published an invalid view");
		return true;
	}
	testRequire(memcmp(&Hello, &Before, sizeof(Hello)) == 0,
		"mutated TLS ClientHello violated failure atomicity");
	testRequire(xrtGetError() != NULL,
		"mutated TLS ClientHello failed without an error");
	return false;
}



/* 单个 ServerHello 变异只能成功发布有界视图或原子失败。 */
static bool testTlsServerHelloMutationOne(
	const uint8* pData,
	size_t iSize
)
{
	xtlsserverhello Hello;
	xtlsserverhello Before;

	memset(&Hello, 0xA5, sizeof(Hello));
	Before = Hello;
	xrtClearError();
	if ( xrtTlsServerHelloParse(
		(xbytesview) { pData, iSize }, &Hello
	) ) {
		testRequire(testTlsHelloViewWithin(
			Hello.Random, pData, iSize
		) && testTlsHelloViewWithin(
			Hello.SessionId, pData, iSize
		) && testTlsHelloViewWithin(
			Hello.Extensions, pData, iSize
		) && xrtTlsExtensionsValidate(Hello.Extensions),
			"mutated TLS ServerHello published an invalid view");
		return true;
	}
	testRequire(memcmp(&Hello, &Before, sizeof(Hello)) == 0,
		"mutated TLS ServerHello violated failure atomicity");
	testRequire(xrtGetError() != NULL,
		"mutated TLS ServerHello failed without an error");
	return false;
}



/* 对 Hello 正文执行单比特和确定性多字节变异。 */
int main(void)
{
	uint8 Client[256];
	uint8 Server[128];
	uint8 Mutated[256];
	size_t iClientSize = testTlsClientHelloVector(
		Client, sizeof(Client), NULL
	);
	size_t iServerSize = testTlsServerHelloVector(
		Server, sizeof(Server), false, NULL
	);
	uint32 iState = UINT32_C(0xD1B54A35);
	size_t iAccepted = 0;
	size_t iRejected = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < iClientSize; i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Mutated, Client, iClientSize);
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			if ( testTlsClientHelloMutationOne(
				Mutated, iClientSize
			) ) {
				iAccepted++;
			} else {
				iRejected++;
			}
			iCases++;
		}
	}
	for ( size_t i = 0; i < iServerSize; i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Mutated, Server, iServerSize);
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			if ( testTlsServerHelloMutationOne(
				Mutated, iServerSize
			) ) {
				iAccepted++;
			} else {
				iRejected++;
			}
			iCases++;
		}
	}
	for ( size_t i = 0; i < 4096u; i++ ) {
		bool bClient = (i & 1u) == 0;
		const uint8* pSeed = bClient ? Client : Server;
		size_t iSize = bClient ? iClientSize : iServerSize;

		memcpy(Mutated, pSeed, iSize);
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) +
				UINT32_C(1013904223);
			Mutated[iState % iSize] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		if ( bClient ? testTlsClientHelloMutationOne(
			Mutated, iSize
		) : testTlsServerHelloMutationOne(Mutated, iSize) ) {
			iAccepted++;
		} else {
			iRejected++;
		}
		iCases++;
	}
	testRequire((iAccepted != 0) && (iRejected != 0),
		"TLS Hello mutation corpus missed a result class");
	printf(
		"[PASS] tls_hello_mutation cases=%zu accepted=%zu rejected=%zu\n",
		iCases, iAccepted, iRejected
	);
	return 0;
}
