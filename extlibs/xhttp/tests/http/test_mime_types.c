#include "../test.h"



/* 比较媒体类型视图与零结尾期望文本。 */
static bool testMimeTypeEqual(
	xstrview Type,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Type.Size == iSize) &&
		(memcmp(Type.Data, sExpected, iSize) == 0);
}



/* 验证旧版常用格式和现代 Web 格式映射。 */
static void testMimeTypesKnown(void)
{
	static const struct {
		cstr Extension;
		cstr Type;
	} Cases[] = {
		{ ".3G2", "video/3gpp2" },
		{ "html", "text/html; charset=utf-8" },
		{ ".CSS", "text/css; charset=utf-8" },
		{ "js", "text/javascript; charset=utf-8" },
		{ "mjs", "text/javascript; charset=utf-8" },
		{ "json", "application/json" },
		{ "txt", "text/plain; charset=utf-8" },
		{ "log", "text/plain; charset=utf-8" },
		{ "png", "image/png" },
		{ "jpeg", "image/jpeg" },
		{ "gif", "image/gif" },
		{ "webp", "image/webp" },
		{ "avif", "image/avif" },
		{ "svg", "image/svg+xml" },
		{ "ico", "image/vnd.microsoft.icon" },
		{ "woff2", "font/woff2" },
		{ "wasm", "application/wasm" },
		{ "yaml", "application/yaml" },
		{ "cbor", "application/cbor" },
		{ "pdf", "application/pdf" },
		{ "zip", "application/zip" },
		{ ".ZST", "application/zstd" }
	};
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		testRequire(testMimeTypeEqual(
			xrtMimeByExt((xstrview){
				Cases[i].Extension,
				strlen(Cases[i].Extension)
			}),
			Cases[i].Type
		), "MIME extension mapping mismatch");
	}
}



/* 验证路径边界、非零结尾输入和未知类型回退。 */
static void testMimeTypesPath(void)
{
	static const char PathData[] = {
		'a', '/', 'b', '.', 'J', 'S', 'x'
	};
	xerror* pPrior;
	xstrview Type;

	Type = xrtMimeByPath((xstrview){
		PathData,
		sizeof(PathData) - 1u
	});
	testRequire(testMimeTypeEqual(
		Type,
		"text/javascript; charset=utf-8"
	), "MIME non-terminated path lookup mismatch");
	testRequire(xrtMimeByPath(
		XRT_STR_LITERAL("dir.with.dot/file")
	).Size == 0, "MIME used a directory dot as extension");
	testRequire(xrtMimeByPath(
		XRT_STR_LITERAL("assets/.env")
	).Size == 0, "MIME treated hidden name as extension");
	testRequire(xrtMimeByPath(
		XRT_STR_LITERAL("assets/.config.json")
	).Size != 0, "MIME lost hidden file real extension");
	testRequire(xrtMimeByPath(
		XRT_STR_LITERAL("assets/file.")
	).Size == 0, "MIME accepted empty path extension");
	testRequire(xrtMimeByPath(
		XRT_STR_LITERAL("assets\\image.PNG")
	).Size != 0, "MIME Windows path lookup failed");

	pPrior = xrtErrorCreate(
		XERR_STATE,
		"test",
		1,
		"preserved error"
	);
	testRequire(pPrior != NULL,
		"MIME prior error creation failed");
	xrtSetError(pPrior);
	testRequire(xrtMimeByExt(
		XRT_STR_LITERAL("unknown")
	).Size == 0 && (xrtGetError() == pPrior),
		"MIME unknown extension changed prior error");
	xrtClearError();
	xrtErrorFree(pPrior);

	testRequire(strcmp(
		xrtMime("site/index.HTML"),
		"text/html; charset=utf-8"
	) == 0, "MIME path helper known type mismatch");
	testRequire(strcmp(
		xrtMime("site/archive.unknown"),
		"application/octet-stream"
	) == 0, "MIME path helper fallback mismatch");
	testRequire(strcmp(
		xrtMime(NULL),
		"application/octet-stream"
	) == 0, "MIME null path helper fallback mismatch");
}



/* 验证不一致视图会进入统一参数错误通道。 */
static void testMimeTypesInvalid(void)
{
	xstrview Type;

	Type = xrtMimeByExt((xstrview){ NULL, 1 });
	testRequire((Type.Size == 0) && (xrtGetError() != NULL),
		"MIME extension accepted invalid view");
	xrtClearError();
	Type = xrtMimeByPath((xstrview){ NULL, 1 });
	testRequire((Type.Size == 0) && (xrtGetError() != NULL),
		"MIME path accepted invalid view");
	xrtClearError();
	Type = xrtMimeByExt((xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4
	});
	testRequire((Type.Size == 0) && (xrtGetError() != NULL),
		"MIME extension accepted wrapping view");
	xrtClearError();
	Type = xrtMimeByPath((xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4
	});
	testRequire((Type.Size == 0) && (xrtGetError() != NULL),
		"MIME path accepted wrapping view");
	xrtClearError();
}



/* 执行内置 MIME 类型映射测试。 */
int main(void)
{
	testMimeTypesKnown();
	testMimeTypesPath();
	testMimeTypesInvalid();
	printf("[PASS] mime_types\n");
	return 0;
}

