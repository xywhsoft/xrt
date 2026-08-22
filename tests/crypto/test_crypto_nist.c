#include "../test.h"
#include "test_crypto_digest.h"
#include "../../src/internal/xrt_crypto_nist.h"



/* 验证一条 NIST 曲线的生成元、倍点、点加和非法点拒绝。 */
static void testNistCurve(
	int iCurve,
	size_t iScalarSize,
	size_t iPointSize,
	cstr sGenerator,
	cstr sDouble
)
{
	uint8 Scalar[48] = { 0 };
	uint8 One[48] = { 0 };
	uint8 Generator[97];
	uint8 Expected[97];
	uint8 Point[97];

	One[iScalarSize - 1u] = 1;
	Scalar[iScalarSize - 1u] = 2;
	testCryptoDecode(
		Generator, iPointSize, sGenerator, "NIST generator has the wrong size"
	);
	testCryptoDecode(
		Expected, iPointSize, sDouble, "NIST double vector has the wrong size"
	);

	testRequire(__xrtNistPointValid(iCurve, Generator, iPointSize) == 1,
		"NIST generator validation failed");
	testRequire(__xrtNistPointMultiplyBase(
		iCurve, Point, One, iScalarSize
	) == iPointSize, "NIST base multiplication length mismatch");
	testRequire(xrtConstTimeEqual(Point, Generator, iPointSize),
		"NIST one times generator mismatch");
	testRequire(__xrtNistPointMultiplyBase(
		iCurve, Point, Scalar, iScalarSize
	) == iPointSize, "NIST double base multiplication failed");
	testRequire(xrtConstTimeEqual(Point, Expected, iPointSize),
		"NIST two times generator mismatch");

	memcpy(Point, Generator, iPointSize);
	testRequire(__xrtNistPointMultiply(
		iCurve, Point, iPointSize, Scalar, iScalarSize
	) == 1, "NIST arbitrary point multiplication failed");
	testRequire(xrtConstTimeEqual(Point, Expected, iPointSize),
		"NIST arbitrary point multiplication mismatch");

	memcpy(Point, Generator, iPointSize);
	testRequire(__xrtNistPointMultiplyAdd(
		iCurve,
		Point,
		Generator,
		iPointSize,
		One,
		iScalarSize,
		One,
		iScalarSize
	) == 1, "NIST point addition failed");
	testRequire(xrtConstTimeEqual(Point, Expected, iPointSize),
		"NIST point addition mismatch");

	Generator[0] = 0x02;
	testRequire(__xrtNistPointValid(iCurve, Generator, iPointSize) == 0,
		"NIST compressed point was accepted");
	memset(Generator, 0, iPointSize);
	Generator[0] = 0x04;
	testRequire(__xrtNistPointValid(iCurve, Generator, iPointSize) == 0,
		"NIST off-curve zero point was accepted");
}



int main(void)
{
	testNistCurve(
		XRT_NIST_P256,
		32,
		65,
		"046b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
		"4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5",
		"047cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978"
		"07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1"
	);
	testNistCurve(
		XRT_NIST_P384,
		48,
		97,
		"04aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a38"
		"5502f25dbf55296c3a545e3872760ab73617de4a96262c6f5d9e98bf9292dc29f8"
		"f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f",
		"0408d999057ba3d2d969260045c55b97f089025959a6f434d651d207d19fb96e9e"
		"4fe0e86ebe0e64f85b96a9c75295df618e80f1fa5b1b3cedb7bfe8dffd6dba74"
		"b275d875bc6cc43e904e505f256ab4255ffd43e94d39e22d61501e700a940e80"
	);
	printf("[PASS] crypto_nist\n");
	return 0;
}
