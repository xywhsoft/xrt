#include "../test.h"
#include "rsa_fixture.h"
#include "../../src/internal/xrt_crypto_rsa.h"

static int TestBlindMode;
static size_t TestBlindCalls;
static size_t TestBlindCoreCalls;
static __xrt_test_rsa_private_fixture TestBlindFixture;
static uint8 TestBlindInput[XRT_RSA_MAX_MODULUS_SIZE];

/* 测试专用编译替身：不在生产接口中加入随机注入或全局可变钩子。 */
static bool testBlindRandom(void* pData, size_t iSize)
{
	uint8* pBytes = (uint8*)pData;

	TestBlindCalls++;
	if ( TestBlindMode == 1 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	memset(pBytes, 0, iSize);
	if ( TestBlindMode == 2 ) {
		return true;
	}
	if ( TestBlindMode == 3 ) {
		pBytes[iSize - 1u] = 1u;
	} else if ( TestBlindMode == 4 ) {
		memset(pBytes, 0xFF, iSize);
	} else if ( (TestBlindMode == 5) ||
		((TestBlindMode == 6) && (TestBlindCalls == 1u)) ) {
		memcpy(pBytes + iSize - TestBlindFixture.Key.Prime1Size,
			TestBlindFixture.Key.Prime1, TestBlindFixture.Key.Prime1Size);
	} else {
		pBytes[iSize - 1u] = (uint8)(TestBlindCalls + 1u);
	}
	return true;
}

static __xrt_rsa_result testBlindCore(const xrsaprivatekey* pKey,
	const void* pInput, size_t iSize, void* pOutput)
{
	TestBlindCoreCalls++;
	memcpy(TestBlindInput, pInput, iSize);
	return __xrtRsaPrivateCore(pKey, pInput, iSize, pOutput);
}

#define xrtSecureRandom testBlindRandom
#define __xrtRsaPrivateCore testBlindCore
#define __xrtRsaPrivatePower testBlindPower
#define __xrtRsaModDivide testBlindDivide
#include "../../src/crypto/rsa_blinding.c"
#undef xrtSecureRandom
#undef __xrtRsaPrivateCore
#undef __xrtRsaPrivatePower
#undef __xrtRsaModDivide

/* 小模数穷举与普通整数乘法独立比较，包括不可逆数与零分子。 */
static void testBlindDivision(void)
{
	for ( uint32 m = 3u; m <= 257u; m += 2u ) {
		uint32 iBits = 0;
		uint32 Modulus[2];

		for ( uint32 v = m; v != 0; v >>= 1u ) {
			iBits++;
		}
		Modulus[0] = iBits;
		Modulus[1] = m;
		for ( uint32 y = 0; y < m; y++ ) {
			uint32 iInverse = 0;
			uint32 Divisor[2] = { iBits, y };
			uint32 Work[3];
			uint32 x = (y * 17u) % m;
			uint32 Value[2] = { iBits, x };
			uint32 iResult;

			for ( uint32 k = 1u; k < m; k++ ) {
				if ( ((k * y) % m) == 1u ) {
					iInverse = k;
					break;
				}
			}
			iResult = testBlindDivide(Value, Divisor, Modulus,
				__xrtI31NegativeInverse(m), Work);
			testRequire((iResult != 0) == (iInverse != 0),
				"blinding division gcd differs");
			if ( iResult != 0 ) {
				testRequire(Value[1] == ((x * iInverse) % m),
					"blinding division result differs");
			}
		}
	}
}

int main(void)
{
	uint8 Input[128];
	uint8 Output[128];
	uint8 Plain[128] = { 0 };
	uint8 Previous[128] = { 0 };

	testBlindDivision();
	testRequire(__xrtTestRsaPrivateFixture(&TestBlindFixture) &&
		__xrtTestRsaHex(__xrtTestRsaRawHex, Input, sizeof(Input)),
		"RSA blinding fixture failed");
	Plain[127] = 2u;
	for ( int i = 1; i <= 5; i++ ) {
		TestBlindMode = i;
		TestBlindCalls = TestBlindCoreCalls = 0;
		memset(Output, 0xA5, sizeof(Output));
		memcpy(Previous, Output, sizeof(Output));
		xrtClearError();
		testRequire((testBlindPower(&TestBlindFixture.Key, Input,
			sizeof(Input), Output) == XRT_RSA_RESULT_RANDOM) &&
			(TestBlindCoreCalls == 0) &&
			(TestBlindCalls == (i == 1 ? 1u : 64u)) &&
			(memcmp(Output, Previous, sizeof(Output)) == 0) &&
			(xrtGetError() != NULL), "blinding failure did not fail closed");
		if ( i == 1 ) {
			testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
				"blinding discarded its random-source error");
		}
	}
	xrtClearError();
	TestBlindMode = 6;
	TestBlindCalls = TestBlindCoreCalls = 0;
	testRequire((testBlindPower(&TestBlindFixture.Key, Input,
		sizeof(Input), Output) == XRT_RSA_RESULT_OK) &&
		(TestBlindCalls == 2u) && (TestBlindCoreCalls == 1u) &&
		(memcmp(Output, Plain, sizeof(Output)) == 0),
		"noninvertible blinding factor did not retry");
	TestBlindMode = 0;
	TestBlindCalls = 0;
	for ( size_t i = 0; i < 4u; i++ ) {
		testRequire((testBlindPower(&TestBlindFixture.Key, Input,
			sizeof(Input), Output) == XRT_RSA_RESULT_OK) &&
			(memcmp(Output, Plain, sizeof(Output)) == 0) &&
			(memcmp(TestBlindInput, Input, sizeof(Input)) != 0) &&
			(memcmp(TestBlindInput, Previous, sizeof(Input)) != 0),
			"RSA private core did not receive a fresh blinded representative");
		memcpy(Previous, TestBlindInput, sizeof(Previous));
	}
	xrtSecureZero(&TestBlindFixture, sizeof(TestBlindFixture));
	xrtSecureZero(TestBlindInput, sizeof(TestBlindInput));
	printf("[PASS] crypto_rsa_blinding\n");
	return 0;
}
