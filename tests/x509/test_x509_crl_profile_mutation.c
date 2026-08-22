#include "../test.h"
#include "../fixtures/x509_crl_profile_vectors.h"



/* 变异目标标识四类相互独立的 CRL profile 正文。 */
typedef enum testx509crlprofilekind {
	TEST_X509_CRL_PROFILE_NUMBER,
	TEST_X509_CRL_PROFILE_ISSUING_POINT,
	TEST_X509_CRL_PROFILE_REASON,
	TEST_X509_CRL_PROFILE_INVALIDITY
} testx509crlprofilekind;



/* 检查一次变异输入上的终止性和失败输出原子性。 */
static void testX509CrlProfileMutationOne(
	testx509crlprofilekind Kind,
	const uint8* pDer,
	size_t iSize,
	size_t* pAccepted,
	size_t* pRejected
)
{
	if ( Kind == TEST_X509_CRL_PROFILE_NUMBER ) {
		xbytesview Number;
		xbytesview Before;

		memset(&Number, 0xA5, sizeof(Number));
		Before = Number;
		if ( xrtX509CrlNumberParse(
			(xbytesview) { pDer, iSize }, &Number
		) ) {
			(*pAccepted)++;
		} else {
			testRequire(memcmp(&Number, &Before, sizeof(Number)) == 0,
				"mutated CRL number changed failed output");
			(*pRejected)++;
		}
	} else if ( Kind == TEST_X509_CRL_PROFILE_ISSUING_POINT ) {
		xx509issuingpoint Point;
		xx509issuingpoint Before;

		memset(&Point, 0xA5, sizeof(Point));
		Before = Point;
		if ( xrtX509IssuingPointParse(
			(xbytesview) { pDer, iSize }, &Point
		) ) {
			(*pAccepted)++;
		} else {
			testRequire(memcmp(&Point, &Before, sizeof(Point)) == 0,
				"mutated issuing point changed failed output");
			(*pRejected)++;
		}
	} else if ( Kind == TEST_X509_CRL_PROFILE_REASON ) {
		xx509crlreason Reason = X509_CRL_REASON_AA_COMPROMISE;

		if ( xrtX509CrlReasonParse(
			(xbytesview) { pDer, iSize }, &Reason
		) ) {
			(*pAccepted)++;
		} else {
			testRequire(Reason == X509_CRL_REASON_AA_COMPROMISE,
				"mutated CRL reason changed failed output");
			(*pRejected)++;
		}
	} else {
		const xtime iBefore = INT64_C(0x1122334455667788);
		xtime iTime = iBefore;

		if ( xrtX509CrlInvalidityDateParse(
			(xbytesview) { pDer, iSize }, &iTime
		) ) {
			(*pAccepted)++;
		} else {
			testRequire(iTime == iBefore,
				"mutated InvalidityDate changed failed output");
			(*pRejected)++;
		}
	}
}



/* 对一类 profile 正文执行全字节单比特翻转。 */
static void testX509CrlProfileMutationVector(
	testx509crlprofilekind Kind,
	const uint8* pDer,
	size_t iSize,
	size_t* pAccepted,
	size_t* pRejected,
	size_t* pCases
)
{
	uint8 Mutated[64];

	testRequire(iSize <= sizeof(Mutated),
		"CRL profile mutation vector exceeds fixed buffer");
	for ( size_t i = 0; i < iSize; i++ ) {
		for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
			memcpy(Mutated, pDer, iSize);
			Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
			testX509CrlProfileMutationOne(
				Kind, Mutated, iSize, pAccepted, pRejected
			);
			(*pCases)++;
		}
	}
}



/* 覆盖编号、IDP、Reason 和 InvalidityDate 的全字节变异语料。 */
int main(void)
{
	size_t iAccepted = 0;
	size_t iRejected = 0;
	size_t iCases = 0;

	testX509CrlProfileMutationVector(
		TEST_X509_CRL_PROFILE_NUMBER, X509_CRL_PROFILE_LARGE_NUMBER,
		sizeof(X509_CRL_PROFILE_LARGE_NUMBER), &iAccepted, &iRejected, &iCases
	);
	testX509CrlProfileMutationVector(
		TEST_X509_CRL_PROFILE_ISSUING_POINT,
		X509_CRL_PROFILE_ISSUING_POINT,
		sizeof(X509_CRL_PROFILE_ISSUING_POINT),
		&iAccepted, &iRejected, &iCases
	);
	testX509CrlProfileMutationVector(
		TEST_X509_CRL_PROFILE_REASON, X509_CRL_PROFILE_REASON,
		sizeof(X509_CRL_PROFILE_REASON), &iAccepted, &iRejected, &iCases
	);
	testX509CrlProfileMutationVector(
		TEST_X509_CRL_PROFILE_INVALIDITY, X509_CRL_PROFILE_INVALIDITY,
		sizeof(X509_CRL_PROFILE_INVALIDITY), &iAccepted, &iRejected, &iCases
	);
	testRequire((iAccepted != 0) && (iRejected != 0),
		"CRL profile mutation corpus missed a result class");
	printf(
		"[PASS] x509_crl_profile_mutation cases=%zu accepted=%zu rejected=%zu\n",
		iCases, iAccepted, iRejected
	);
	return 0;
}
