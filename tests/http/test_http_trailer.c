#include "../test.h"

#include <xrt/http_trailer.h>



/* 验证重复 Trailer 字段的完整校验与名称查询。 */
static void testHttpTrailerDeclarations(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Trailer"),
			XRT_STR_INIT("Digest, X-Meta")
		},
		{
			XRT_STR_INIT("trailer"),
			XRT_STR_INIT("X-Checksum")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Trailer"),
			XRT_STR_INIT("Digest")
		},
		{
			XRT_STR_INIT("Trailer"),
			XRT_STR_INIT("Content-Length")
		}
	};
	static const xhttpfield Empty = {
		XRT_STR_INIT("Trailer"),
		XRT_STR_INIT("")
	};
	size_t iNames;

	testRequire(
		xrtHttpTrailerCount(Fields, 2u, &iNames) &&
		(iNames == 3u) &&
		(xrtHttpTrailerFind(
			Fields, 2u, XRT_STR_LITERAL("digest")
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpTrailerFind(
			Fields, 2u, XRT_STR_LITERAL("X-Missing")
		) == XHTTP_NEXT_END),
		"HTTP Trailer declaration parsing mismatch"
	);
	testRequire(
		!xrtHttpTrailerCount(Invalid, 2u, &iNames) &&
		(iNames == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Trailer accepted a forbidden declaration"
	);
	xrtClearError();
	testRequire(
		(xrtHttpTrailerFind(
			Invalid, 2u, XRT_STR_LITERAL("Digest")
		) == XHTTP_NEXT_ERROR),
		"HTTP Trailer lookup published before full validation"
	);
	xrtClearError();
	testRequire(
		xrtHttpTrailerCount(&Empty, 1u, &iNames) &&
		(iNames == 0) &&
		(xrtHttpTrailerFind(
			&Empty, 1u, XRT_STR_LITERAL("Digest")
		) == XHTTP_NEXT_END),
		"HTTP Trailer empty declaration mismatch"
	);
}



/* 验证实际 trailer section 的名称和值使用同一套严格策略。 */
static void testHttpTrailerSection(void)
{
	static const xhttpfield Valid[] = {
		{
			XRT_STR_INIT("Content-Digest"),
			XRT_STR_INIT("sha-256=:YWJj:")
		},
		{
			XRT_STR_INIT("X-Result"),
			XRT_STR_INIT("complete")
		}
	};
	static const xhttpfield Forbidden = {
		XRT_STR_INIT("Content-Length"),
		XRT_STR_INIT("3")
	};
	static const xhttpfield InvalidValue = {
		XRT_STR_INIT("X-Result"),
		XRT_STR_INIT("bad\rvalue")
	};

	testRequire(
		xrtHttpTrailerSectionValid(Valid, 2u),
		"HTTP Trailer rejected a valid trailer section"
	);
	testRequire(
		!xrtHttpTrailerSectionValid(&Forbidden, 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Trailer accepted a forbidden trailer section"
	);
	xrtClearError();
	testRequire(
		!xrtHttpTrailerSectionValid(&InvalidValue, 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Trailer accepted an invalid field value"
	);
	xrtClearError();
	testRequire(
		xrtHttpTrailerSectionValid(NULL, 0),
		"HTTP Trailer rejected an empty trailer section"
	);
}



/* 验证写入器去重、精确测量、短缓冲和一分配构建。 */
static void testHttpTrailerWriter(void)
{
	static const xhttpfield Trailers[] = {
		{
			XRT_STR_INIT("Digest"),
			XRT_STR_INIT("one")
		},
		{
			XRT_STR_INIT("digest"),
			XRT_STR_INIT("two")
		},
		{
			XRT_STR_INIT("X-Meta"),
			XRT_STR_INIT("three")
		}
	};
	static const xhttpfield Invalid = {
		XRT_STR_INIT("X-Meta"),
		XRT_STR_INIT("bad\rvalue")
	};
	unsigned char Output[32];
	str sBuilt;
	size_t iSize;

	testRequire(
		xrtHttpTrailerNamesWrite(
			Trailers, 3u, NULL, 0, &iSize
		) && (iSize == 14u),
		"HTTP Trailer declaration measure mismatch"
	);
	memset(Output, 0xA5, sizeof(Output));
	testRequire(
		xrtHttpTrailerNamesWrite(
			Trailers, 3u, Output, sizeof(Output), &iSize
		) && (iSize == 14u) &&
		(memcmp(Output, "Digest, X-Meta", 14u) == 0),
		"HTTP Trailer declaration write mismatch"
	);
	memset(Output, 0xA5, sizeof(Output));
	testRequire(
		!xrtHttpTrailerNamesWrite(
			Trailers, 3u, Output, 13u, &iSize
		) && (iSize == 14u) && (Output[0] == 0xA5),
		"HTTP Trailer declaration accepted a short output"
	);
	xrtClearError();
	sBuilt = xrtHttpTrailerNamesBuild(Trailers, 3u, &iSize);
	testRequire(
		(sBuilt != NULL) && (iSize == 14u) &&
		(strcmp(sBuilt, "Digest, X-Meta") == 0),
		"HTTP Trailer declaration Build mismatch"
	);
	xrtFree(sBuilt);
	sBuilt = xrtHttpTrailerNamesBuild(&Invalid, 1u, &iSize);
	testRequire(
		(sBuilt == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP Trailer declaration Build accepted an invalid field value"
	);
	xrtClearError();
}



/* 验证未对齐描述符、重叠输出和空集合边界。 */
static void testHttpTrailerMemory(void)
{
	static const xhttpfield Field = {
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("value")
	};
	static const xhttpfield Declaration = {
		XRT_STR_INIT("Trailer"),
		XRT_STR_INIT("Digest, X-Meta")
	};
	uint8 Storage[sizeof(Field) + 32u];
	uint8 CountStorage[sizeof(size_t) + 1u];
	xhttpfield* pField =
		(xhttpfield*)(void*)(Storage + 1u);
	size_t* pUnalignedCount =
		(size_t*)(void*)(CountStorage + 1u);
	size_t iNames;
	size_t iSize;
	str sEmpty;

	memcpy(pField, &Field, sizeof(Field));
	testRequire(
		xrtHttpTrailerNamesWrite(
			pField, 1u,
			Storage + sizeof(Field) + 1u,
			7u,
			&iSize
		) && (iSize == 6u),
		"HTTP Trailer unaligned descriptor failed"
	);
	testRequire(
		xrtHttpTrailerCount(
			&Declaration, 1u, pUnalignedCount
		),
		"HTTP Trailer rejected an unaligned count output"
	);
	memcpy(&iNames, pUnalignedCount, sizeof(iNames));
	testRequire(
		iNames == 2u,
		"HTTP Trailer published the wrong unaligned count"
	);
	testRequire(
		!xrtHttpTrailerNamesWrite(
			pField, 1u, Storage + 1u, 6u, &iSize
		),
		"HTTP Trailer accepted overlapping output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpTrailerNamesWrite(
			NULL, 1u, Storage, sizeof(Storage), &iSize
		),
		"HTTP Trailer accepted a null non-empty field array"
	);
	xrtClearError();
	sEmpty = xrtHttpTrailerNamesBuild(NULL, 0, &iSize);
	testRequire(
		(sEmpty != NULL) && (iSize == 0) && (sEmpty[0] == 0),
		"HTTP Trailer empty Build mismatch"
	);
	xrtFree(sEmpty);
}



/* 验证远超常见 Header 快速路径的声明不会被固定缓冲截断。 */
static void testHttpTrailerLarge(void)
{
	enum { TEST_TRAILER_COUNT = 128 };
	xhttpfield Fields[TEST_TRAILER_COUNT];
	char Names[TEST_TRAILER_COUNT][24];
	str sBuilt;
	size_t iSize;
	size_t i;

	for ( i = 0; i < TEST_TRAILER_COUNT; i++ ) {
		int iWritten = snprintf(
			Names[i], sizeof(Names[i]),
			"X-Meta-%03u", (unsigned)i
		);

		testRequire(
			(iWritten == 10),
			"HTTP Trailer large name formatting failed"
		);
		Fields[i].Name = (xstrview){
			Names[i], (size_t)iWritten
		};
		Fields[i].Value = XRT_STR_LITERAL("value");
	}
	sBuilt = xrtHttpTrailerNamesBuild(
		Fields, TEST_TRAILER_COUNT, &iSize
	);
	testRequire(
		(sBuilt != NULL) && (iSize > 1024u) &&
		(strstr(sBuilt, "X-Meta-000") == sBuilt) &&
		(strstr(sBuilt, "X-Meta-127") != NULL),
		"HTTP Trailer large declaration was truncated"
	);
	xrtFree(sBuilt);
}



int main(void)
{
	testHttpTrailerDeclarations();
	testHttpTrailerSection();
	testHttpTrailerWriter();
	testHttpTrailerMemory();
	testHttpTrailerLarge();
	printf("[PASS] http_trailer\n");
	return 0;
}
