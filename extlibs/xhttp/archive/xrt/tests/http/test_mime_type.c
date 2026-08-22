#include "../test.h"



/* 验证媒体类型的参数、结构化后缀、规范写出和重复项边界。 */
static void testMimeMediaType(void)
{
	static const char Expected[] =
		"application/problem+json; charset=UTF-8; note=\"a;b\"";
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT(" \t"),
		XRT_STR_INIT("text"),
		XRT_STR_INIT("text/"),
		XRT_STR_INIT("text/plain;"),
		XRT_STR_INIT("text/plain; charset"),
		XRT_STR_INIT("text/plain; charset=utf-8; CHARSET=ascii"),
		XRT_STR_INIT("text/plain; note=\"unterminated")
	};
	xmediatype Type;
	union {
		xmediatype Type;
		size_t Size;
	} Shared;
	xhttpparam Param;
	xstrview Suffix;
	char Text[128];
	size_t iSize;
	size_t i;

	testRequire(xrtHttpMediaTypeParse(
		XRT_STR_LITERAL(
			"application/problem+json; charset=UTF-8; note=\"a;b\""
		), &Type
	), "HTTP media type parse failed");
	testRequire(xrtHttpMediaTypeEqual(
		&Type, XRT_STR_LITERAL("APPLICATION"),
		XRT_STR_LITERAL("problem+json")
	), "HTTP media type comparison mismatch");
	Suffix = xrtHttpMediaTypeSuffix(&Type);
	testRequire((Suffix.Size == 4) &&
		(memcmp(Suffix.Data, "json", 4) == 0),
		"HTTP media type suffix mismatch");
	testRequire(xrtHttpMediaTypeParam(
		&Type, XRT_STR_LITERAL("charset"), &Param
	) == XHTTP_NEXT_ITEM &&
		xrtHttpTokenEqual(Param.Value, XRT_STR_LITERAL("UTF-8")),
		"HTTP media type parameter mismatch");
	testRequire(xrtHttpMediaTypeWrite(
		&Type, Text, sizeof(Text), &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Text, Expected, sizeof(Expected) - 1u) == 0),
		"HTTP media type write mismatch");
	Shared.Type = Type;
	testRequire(!xrtHttpMediaTypeWrite(
		&Shared.Type, NULL, 0, &Shared.Size
	), "HTTP media type size output overlapped metadata");
	xrtClearError();
	testRequire(xrtHttpMediaTypeParse(
		XRT_STR_LITERAL("application/vnd.a+gzip+json"), &Type
	), "HTTP media type multiple plus parse failed");
	Suffix = xrtHttpMediaTypeSuffix(&Type);
	testRequire((Suffix.Size == 4) &&
		(memcmp(Suffix.Data, "json", 4) == 0),
		"HTTP media type did not use the final suffix");
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpMediaTypeParse(Invalid[i], &Type),
			"HTTP media type accepted malformed syntax");
		xrtClearError();
	}
}



/* 验证压缩判断覆盖文本、结构化后缀和预压缩格式边界。 */
static void testMimeCompressible(void)
{
	static const xstrview Compressible[] = {
		XRT_STR_INIT("text/plain"),
		XRT_STR_INIT("TEXT/CSS; charset=UTF-8"),
		XRT_STR_INIT("application/problem+json"),
		XRT_STR_INIT("image/svg+xml"),
		XRT_STR_INIT("application/javascript"),
		XRT_STR_INIT("application/wasm"),
		XRT_STR_INIT("application/toml"),
		XRT_STR_INIT("application/vnd.ms-fontobject"),
		XRT_STR_INIT("font/ttf"),
		XRT_STR_INIT("font/otf")
	};
	static const xstrview Incompressible[] = {
		XRT_STR_INIT("application/octet-stream"),
		XRT_STR_INIT("application/zip"),
		XRT_STR_INIT("application/gzip"),
		XRT_STR_INIT("image/png"),
		XRT_STR_INIT("audio/mpeg"),
		XRT_STR_INIT("video/mp4"),
		XRT_STR_INIT("font/woff"),
		XRT_STR_INIT("font/woff2")
	};
	xmediatype Type;
	xerror* pPrior;
	size_t i;

	for ( i = 0; i <
		(sizeof(Compressible) / sizeof(Compressible[0])); i++ ) {
		testRequire(xrtHttpContentTypeCompressible(Compressible[i]),
			"compressible media type was rejected");
	}
	pPrior = xrtErrorCreate(
		XERR_STATE, "test", 2, "preserved error"
	);
	testRequire(pPrior != NULL,
		"MIME compressibility prior error creation failed");
	xrtSetError(pPrior);
	for ( i = 0; i <
		(sizeof(Incompressible) / sizeof(Incompressible[0])); i++ ) {
		testRequire(
			!xrtHttpContentTypeCompressible(Incompressible[i]) &&
			(xrtGetError() == pPrior),
			"incompressible media type changed the prior error"
		);
	}
	xrtClearError();
	xrtErrorFree(pPrior);

	testRequire(!xrtHttpContentTypeCompressible(
		XRT_STR_LITERAL("text/plain;")
	) && (xrtGetError() != NULL),
		"malformed Content-Type did not report an error");
	xrtClearError();
	memset(&Type, 0, sizeof(Type));
	testRequire(!xrtHttpMediaTypeCompressible(&Type) &&
		(xrtGetError() != NULL),
		"invalid parsed media type did not report an error");
	xrtClearError();
}



/* 执行媒体类型测试。 */
int main(void)
{
	testMimeMediaType();
	testMimeCompressible();
	printf("[PASS] mime_type\n");
	return 0;
}
