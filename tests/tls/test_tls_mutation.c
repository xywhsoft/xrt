#include "../test.h"



/* 检查单个变异记录不会破坏失败原子性。 */
static xtlsresult testTlsMutationOne(const uint8* pData, size_t iSize)
{
	xtlsrecord Record;
	xtlsrecord Before;
	size_t iRequired = 0;
	xtlsresult Result;

	memset(&Record, 0xA5, sizeof(Record));
	Before = Record;
	Result = xrtTlsRecordParse(
		(xbytesview) { pData, iSize }, &Record, &iRequired
	);
	if ( Result != XTLS_OK ) {
		testRequire(memcmp(&Record, &Before, sizeof(Record)) == 0,
			"mutated TLS record changed failed output");
	} else {
		testRequire((Record.EncodedSize >= XTLS_RECORD_HEADER_SIZE) &&
			(Record.EncodedSize <= iSize) &&
			(Record.Payload.Data == pData + XTLS_RECORD_HEADER_SIZE),
			"mutated TLS record produced an invalid view");
	}
	(void)iRequired;
	return Result;
}



/* 对完整记录执行单比特与确定性多字节变异。 */
int main(void)
{
	static const uint8 Seed[] = {
		23, 0x03, 0x03, 0x00, 0x0B,
		'h', 'e', 'l', 'l', 'o', '-', 'w', 'o', 'r', 'l', 'd'
	};
	uint8 Mutated[sizeof(Seed)];
	uint32 iState = UINT32_C(0x6D2B79F5);
	size_t iOk = 0;
	size_t iAgain = 0;
	size_t iError = 0;
	size_t iCases = 0;

	for ( size_t i = 0; i < sizeof(Mutated); i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			xtlsresult Result;

			memcpy(Mutated, Seed, sizeof(Mutated));
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			Result = testTlsMutationOne(Mutated, sizeof(Mutated));
			iOk += Result == XTLS_OK;
			iAgain += Result == XTLS_AGAIN;
			iError += Result == XTLS_ERROR;
			iCases++;
		}
	}
	for ( size_t i = 0; i < 2048u; i++ ) {
		xtlsresult Result;

		memcpy(Mutated, Seed, sizeof(Mutated));
		for ( size_t j = 0; j < 3u; j++ ) {
			iState = (iState * UINT32_C(1664525)) + UINT32_C(1013904223);
			Mutated[iState % sizeof(Mutated)] ^=
				(uint8)((iState >> 24u) | 1u);
		}
		Result = testTlsMutationOne(Mutated, sizeof(Mutated));
		iOk += Result == XTLS_OK;
		iAgain += Result == XTLS_AGAIN;
		iError += Result == XTLS_ERROR;
		iCases++;
	}
	testRequire((iOk != 0) && (iAgain != 0) && (iError != 0),
		"TLS mutation corpus missed a result class");
	printf(
		"[PASS] tls_mutation cases=%zu ok=%zu again=%zu errors=%zu\n",
		iCases, iOk, iAgain, iError
	);
	return 0;
}
