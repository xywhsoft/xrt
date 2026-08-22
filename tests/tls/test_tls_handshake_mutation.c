#include "../test.h"



/* 变异握手 framing 只能发布输入范围内的借用视图。 */
static xtlsresult testTlsHandshakeMutationOne(
	const uint8* pData,
	size_t iSize
)
{
	xtlshandshake Handshake;
	xtlshandshake Before;
	xtlsresult Result;

	memset(&Handshake, 0xA5, sizeof(Handshake));
	Before = Handshake;
	Result = xrtTlsHandshakeParse(
		(xbytesview) { pData, iSize }, &Handshake, NULL
	);
	if ( Result == XTLS_OK ) {
		testRequire((Handshake.EncodedSize >= XTLS_HANDSHAKE_HEADER_SIZE) &&
			(Handshake.EncodedSize <= iSize) &&
			(Handshake.Body.Data == pData + XTLS_HANDSHAKE_HEADER_SIZE) &&
			(Handshake.Body.Size ==
				Handshake.EncodedSize - XTLS_HANDSHAKE_HEADER_SIZE),
			"mutated TLS handshake produced an invalid view");
	} else {
		testRequire((Result == XTLS_AGAIN) &&
			(memcmp(&Handshake, &Before, sizeof(Handshake)) == 0),
			"mutated TLS handshake violated failure atomicity");
	}
	return Result;
}



/* 变异扩展 framing 只能发布输入范围内的借用视图。 */
static xtlsresult testTlsExtensionMutationOne(
	const uint8* pData,
	size_t iSize
)
{
	xtlsextension Extension;
	xtlsextension Before;
	xtlsresult Result;

	memset(&Extension, 0xA5, sizeof(Extension));
	Before = Extension;
	Result = xrtTlsExtensionParse(
		(xbytesview) { pData, iSize }, &Extension, NULL
	);
	if ( Result == XTLS_OK ) {
		testRequire((Extension.EncodedSize >= XTLS_EXTENSION_HEADER_SIZE) &&
			(Extension.EncodedSize <= iSize) &&
			(Extension.Data.Data == pData + XTLS_EXTENSION_HEADER_SIZE) &&
			(Extension.Data.Size ==
				Extension.EncodedSize - XTLS_EXTENSION_HEADER_SIZE),
			"mutated TLS extension produced an invalid view");
	} else {
		testRequire((Result == XTLS_AGAIN) &&
			(memcmp(&Extension, &Before, sizeof(Extension)) == 0),
			"mutated TLS extension violated failure atomicity");
	}
	return Result;
}



/* 对握手与扩展头执行单比特和确定性多字节变异。 */
int main(void)
{
	static const uint8 HandshakeSeed[] = {
		1, 0, 0, 11, 'c', 'l', 'i', 'e', 'n', 't', '-', 'h', 'e', 'l', 'l'
	};
	static const uint8 ExtensionSeed[] = {
		0, 16, 0, 7, 0, 5, 4, 'h', 't', 't', 'p'
	};
	uint8 Mutated[sizeof(HandshakeSeed)];
	uint32 iState = UINT32_C(0x9E3779B9);
	size_t iOk = 0;
	size_t iAgain = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < sizeof(HandshakeSeed); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			xtlsresult Result;

			memcpy(Mutated, HandshakeSeed, sizeof(HandshakeSeed));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			Result = testTlsHandshakeMutationOne(
				Mutated, sizeof(HandshakeSeed)
			);
			iOk += Result == XTLS_OK;
			iAgain += Result == XTLS_AGAIN;
			iCases++;
		}
	}
	for ( size_t i = 0; i < 2048u; i++ ) {
		xtlsresult Result;

		memcpy(Mutated, HandshakeSeed, sizeof(HandshakeSeed));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Mutated[iState % sizeof(HandshakeSeed)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		Result = testTlsHandshakeMutationOne(
			Mutated, sizeof(HandshakeSeed)
		);
		iOk += Result == XTLS_OK;
		iAgain += Result == XTLS_AGAIN;
		iCases++;
	}
	for ( size_t i = 0; i < sizeof(ExtensionSeed); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			xtlsresult Result;

			memcpy(Mutated, ExtensionSeed, sizeof(ExtensionSeed));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			Result = testTlsExtensionMutationOne(
				Mutated, sizeof(ExtensionSeed)
			);
			iOk += Result == XTLS_OK;
			iAgain += Result == XTLS_AGAIN;
			iCases++;
		}
	}
	testRequire((iOk != 0) && (iAgain != 0),
		"TLS handshake mutation corpus missed a result class");
	printf(
		"[PASS] tls_handshake_mutation cases=%zu ok=%zu again=%zu\n",
		iCases, iOk, iAgain
	);
	return 0;
}
