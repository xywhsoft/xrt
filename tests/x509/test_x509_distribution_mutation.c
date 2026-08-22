#include "../test.h"
#include "../fixtures/x509_distribution_vectors.h"



/* 对单项分发点执行一次变异解析并检查失败原子性。 */
static void testX509DistributionMutationPoint(
	const uint8* pDer,
	size_t iSize,
	size_t* pAccepted,
	size_t* pRejected
)
{
	xx509distributionpoint Point;
	xx509distributionpoint Before;

	memset(&Point, 0xA5, sizeof(Point));
	Before = Point;
	if ( xrtX509DistributionPointParse(
		(xbytesview) { pDer, iSize }, &Point
	) ) {
		(*pAccepted)++;
	} else {
		testRequire(memcmp(&Point, &Before, sizeof(Point)) == 0,
			"mutated DistributionPoint changed failed output");
		(*pRejected)++;
	}
}



/* 对分发点列表执行一次变异初始化并检查失败原子性。 */
static void testX509DistributionMutationList(
	const uint8* pDer,
	size_t iSize,
	size_t* pAccepted,
	size_t* pRejected
)
{
	xx509distributioncursor Cursor;
	xx509distributioncursor Before;

	memset(&Cursor, 0xA5, sizeof(Cursor));
	Before = Cursor;
	if ( xrtX509DistributionInit(
		(xbytesview) { pDer, iSize }, &Cursor
	) ) {
		(*pAccepted)++;
	} else {
		testRequire(memcmp(&Cursor, &Before, sizeof(Cursor)) == 0,
			"mutated distribution list changed failed output");
		(*pRejected)++;
	}
}



/* 覆盖单项和多项 DER 的全字节单比特翻转。 */
int main(void)
{
	uint8 Mutated[64];
	size_t iAccepted = 0;
	size_t iRejected = 0;
	size_t iCases = 0;

	for ( size_t iKind = 0; iKind < 2u; iKind++ ) {
		const uint8* pSource = iKind == 0 ? X509_DISTRIBUTION_POINT :
			X509_DISTRIBUTION_POINTS;
		size_t iSize = iKind == 0 ? sizeof(X509_DISTRIBUTION_POINT) :
			sizeof(X509_DISTRIBUTION_POINTS);

		testRequire(iSize <= sizeof(Mutated),
			"distribution mutation vector exceeds fixed buffer");
		for ( size_t i = 0; i < iSize; i++ ) {
			for ( uint8 iBit = 0; iBit < 8u; iBit++ ) {
				memcpy(Mutated, pSource, iSize);
				Mutated[i] ^= (uint8)(UINT8_C(1) << iBit);
				if ( iKind == 0 ) {
					testX509DistributionMutationPoint(
						Mutated, iSize, &iAccepted, &iRejected
					);
				} else {
					testX509DistributionMutationList(
						Mutated, iSize, &iAccepted, &iRejected
					);
				}
				iCases++;
			}
		}
	}
	testRequire((iAccepted != 0) && (iRejected != 0),
		"distribution mutation corpus missed a result class");
	printf(
		"[PASS] x509_distribution_mutation cases=%zu accepted=%zu rejected=%zu\n",
		iCases, iAccepted, iRejected
	);
	return 0;
}
