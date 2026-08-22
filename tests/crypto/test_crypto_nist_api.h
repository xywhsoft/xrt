#ifndef XRT_TEST_CRYPTO_NIST_API_H
#define XRT_TEST_CRYPTO_NIST_API_H

#include "test_crypto_digest.h"



typedef bool (*test_nist_valid_fn)(const void* pPublic);
typedef bool (*test_nist_binary_fn)(
	const void* pLeft,
	const void* pRight,
	void* pOutput
);
typedef bool (*test_nist_public_fn)(const void* pPrivate, void* pPublic);



/* 验证公开 NIST 曲线 API 的向量、错误、原子性和缓冲重叠契约。 */
static inline void testCryptoNistApi(
	size_t iPrivateSize,
	size_t iPublicSize,
	cstr sOrder,
	cstr sGenerator,
	cstr sDouble,
	cstr sPublicOperation,
	cstr sMultiplyOperation,
	cstr sSharedOperation,
	test_nist_valid_fn pValid,
	test_nist_binary_fn pMultiply,
	test_nist_binary_fn pAdd,
	test_nist_public_fn pPublic,
	test_nist_binary_fn pShared
)
{
	uint8 Zero[48] = { 0 };
	uint8 One[48] = { 0 };
	uint8 Two[48] = { 0 };
	uint8 Order[48];
	uint8 Generator[97];
	uint8 Double[97];
	uint8 Output[97];
	uint8 Before[97];
	uint8 Buffer[224];

	One[iPrivateSize - 1u] = 1;
	Two[iPrivateSize - 1u] = 2;
	testCryptoDecode(Order, iPrivateSize, sOrder, "NIST order size mismatch");
	testCryptoDecode(
		Generator, iPublicSize, sGenerator, "NIST generator size mismatch"
	);
	testCryptoDecode(Double, iPublicSize, sDouble, "NIST double size mismatch");

	testRequire(pValid(Generator), "NIST generator was rejected");
	testRequire(pValid(Double), "NIST doubled point was rejected");
	testRequire(pPublic(One, Output) &&
		xrtConstTimeEqual(Output, Generator, iPublicSize),
		"NIST public key for scalar one mismatch");
	testRequire(pPublic(Two, Output) &&
		xrtConstTimeEqual(Output, Double, iPublicSize),
		"NIST public key for scalar two mismatch");
	testRequire(pMultiply(Two, Generator, Output) &&
		xrtConstTimeEqual(Output, Double, iPublicSize),
		"NIST point multiplication mismatch");
	testRequire(pAdd(Generator, Generator, Output) &&
		xrtConstTimeEqual(Output, Double, iPublicSize),
		"NIST point addition mismatch");
	testRequire(pShared(One, Double, Output) &&
		xrtConstTimeEqual(Output, Double + 1, iPrivateSize),
		"NIST first shared secret mismatch");
	testRequire(pShared(Two, Generator, Output) &&
		xrtConstTimeEqual(Output, Double + 1, iPrivateSize),
		"NIST reciprocal shared secret mismatch");

	memset(Buffer, 0, sizeof(Buffer));
	memcpy(Buffer, Two, iPrivateSize);
	memcpy(Buffer + 96, Generator, iPublicSize);
	testRequire(pMultiply(Buffer, Buffer + 96, Buffer + 32) &&
		xrtConstTimeEqual(Buffer + 32, Double, iPublicSize),
		"NIST partial overlap multiplication mismatch");
	memcpy(Buffer, Two, iPrivateSize);
	testRequire(pPublic(Buffer, Buffer + 8) &&
		xrtConstTimeEqual(Buffer + 8, Double, iPublicSize),
		"NIST partial overlap public derivation mismatch");
	memcpy(Buffer, Two, iPrivateSize);
	memcpy(Buffer + 96, Generator, iPublicSize);
	testRequire(pShared(Buffer, Buffer + 96, Buffer + 16) &&
		xrtConstTimeEqual(Buffer + 16, Double + 1, iPrivateSize),
		"NIST partial overlap shared secret mismatch");

	memset(Output, 0xA5, iPublicSize);
	memcpy(Before, Output, iPublicSize);
	xrtClearError();
	testRequire(!pPublic(Zero, Output) &&
		xrtConstTimeEqual(Output, Before, iPublicSize) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_KEY) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sPublicOperation) == 0),
		"NIST zero scalar public error contract mismatch");
	xrtClearError();
	testRequire(!pPublic(Order, Output) &&
		xrtConstTimeEqual(Output, Before, iPublicSize) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sPublicOperation) == 0),
		"NIST order scalar was accepted");

	memcpy(Buffer, Generator, iPublicSize);
	Buffer[0] = 0x02;
	xrtClearError();
	testRequire(!pMultiply(Two, Buffer, Output) &&
		xrtConstTimeEqual(Output, Before, iPublicSize) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_KEY) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sMultiplyOperation) == 0),
		"NIST invalid point multiplication error contract mismatch");
	memset(Output, 0xA5, iPrivateSize);
	memcpy(Before, Output, iPrivateSize);
	xrtClearError();
	testRequire(!pShared(Two, Buffer, Output) &&
		xrtConstTimeEqual(Output, Before, iPrivateSize) &&
		(xrtErrorCode(xrtGetError()) == XCRYPTO_ERROR_KEY_AGREEMENT) &&
		(strcmp(xrtErrorOperation(xrtGetError()), sSharedOperation) == 0),
		"NIST invalid peer shared error contract mismatch");

	xrtClearError();
	testRequire(!pValid(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"NIST null validation error contract mismatch");
	testRequire(!pMultiply(NULL, Generator, Output),
		"NIST null scalar was accepted");
	testRequire(!pAdd(Generator, NULL, Output),
		"NIST null addend was accepted");
	testRequire(!pPublic(One, NULL), "NIST null public output was accepted");
	testRequire(!pShared(One, Generator, NULL),
		"NIST null shared output was accepted");
}

#endif
