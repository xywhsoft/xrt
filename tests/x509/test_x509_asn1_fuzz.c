#include "../test.h"
#include "../fixtures/x509_vectors.h"

#include "../../fuzz/x509_asn1.c"



#ifndef XRT_X509_FUZZ_ROUNDS
	#define XRT_X509_FUZZ_ROUNDS 2000u
#endif

#define XRT_X509_FUZZ_TEST_MAX 4096u



/* 生成可重复的 X.509/ASN.1 协议噪声。 */
static uint32 testX509FuzzNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 执行真实证书、DER 边界和确定性随机输入。 */
int main(void)
{
	static const uint8 Der[] = {
		0x30, 0x06, 0x02, 0x01, 0x01, 0x01, 0x01, 0xFF
	};
	static const uint8 NonCanonical[] = {
		0x04, 0x81, 0x01, 0x00
	};
	uint8 Data[XRT_X509_FUZZ_TEST_MAX];
	uint32 iState = UINT32_C(0x85A308D3);

	testRequire(xrtX509FuzzerTestOneInput(NULL, 0) == 0,
		"X.509 empty fuzz seed failed");
	testRequire(xrtX509FuzzerTestOneInput(
		Der, sizeof(Der)
	) == 0, "DER structured fuzz seed failed");
	testRequire(xrtX509FuzzerTestOneInput(
		NonCanonical, sizeof(NonCanonical)
	) == 0, "DER non-canonical fuzz seed failed");
	testRequire(xrtX509FuzzerTestOneInput(
		X509_VALID_ED25519, sizeof(X509_VALID_ED25519)
	) == 0, "X.509 valid certificate fuzz seed failed");
	for ( size_t iRound = 0; iRound < XRT_X509_FUZZ_ROUNDS; iRound++ ) {
		size_t iSize = (size_t)(
			testX509FuzzNext(&iState) %
			(XRT_X509_FUZZ_TEST_MAX + 1u)
		);

		for ( size_t i = 0; i < iSize; i++ ) {
			Data[i] = (uint8)(testX509FuzzNext(&iState) >> 24u);
		}
		testRequire(xrtX509FuzzerTestOneInput(
			Data, iSize
		) == 0, "X.509 deterministic fuzz round failed");
	}
	printf(
		"[PASS] X.509/ASN.1 fuzz (%u rounds)\n",
		(unsigned int)XRT_X509_FUZZ_ROUNDS
	);
	return 0;
}
