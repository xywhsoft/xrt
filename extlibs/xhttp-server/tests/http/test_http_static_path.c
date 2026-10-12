#include "../test.h"



/* 查询并写出一个命中的静态路径。 */
static bool testHttpStaticPathWrite(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pTrailingSlash
)
{
	size_t iRequired;
	bool bTrailingSlash;

	if ( xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		NULL,
		0,
		&iRequired,
		&bTrailingSlash
	) != XHTTP_STATIC_PATH_MATCH ) {
		return false;
	}
	if ( xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		sOutput,
		iCapacity,
		&iRequired,
		&bTrailingSlash
	) != XHTTP_STATIC_PATH_MATCH ) {
		return false;
	}
	*pSize = iRequired;
	*pTrailingSlash = bTrailingSlash;
	return true;
}



/* 验证根挂载会严格解码 UTF-8，并保留普通加号。 */
static void testHttpStaticPathRoot(void)
{
	char Output[128];
	size_t iSize;
	bool bTrailingSlash;

	testRequire(testHttpStaticPathWrite(
		XRT_STR_LITERAL(
			"/assets/%E6%96%87%E4%BB%B6+one.txt"
		),
		NULL,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) && (strcmp(
		Output,
		"assets/\xE6\x96\x87\xE4\xBB\xB6+one.txt"
	) == 0) && (iSize == strlen(Output)) &&
		!bTrailingSlash,
		"HTTP static root path mapping mismatch");
	testRequire(testHttpStaticPathWrite(
		XRT_STR_LITERAL("/"),
		NULL,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) && (strcmp(Output, ".") == 0) &&
		(iSize == 1u) && bTrailingSlash,
		"HTTP static root directory mapping mismatch");
}



/* 验证挂载点使用解码后的精确段边界，并正常区分未命中。 */
static void testHttpStaticPathMount(void)
{
	xhttpstaticpathconfig Config;
	char Output[64];
	size_t iSize;
	bool bTrailingSlash;

	xrtHttpStaticPathConfigInit(&Config);
	Config.Mount = XRT_STR_LITERAL("/assets");
	testRequire(testHttpStaticPathWrite(
		XRT_STR_LITERAL("/%61ssets/css/app.css"),
		&Config,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) && (strcmp(Output, "css/app.css") == 0) &&
		!bTrailingSlash,
		"HTTP static decoded mount match failed");
	testRequire(testHttpStaticPathWrite(
		XRT_STR_LITERAL("/assets"),
		&Config,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) && (strcmp(Output, ".") == 0) &&
		!bTrailingSlash,
		"HTTP static exact mount mapping mismatch");
	testRequire(testHttpStaticPathWrite(
		XRT_STR_LITERAL("/assets/"),
		&Config,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) && (strcmp(Output, ".") == 0) &&
		bTrailingSlash,
		"HTTP static mount directory mapping mismatch");

	xrtClearError();
	testRequire(xrtHttpStaticPathWrite(
		XRT_STR_LITERAL("/assets-old/app.js"),
		&Config,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_NO_MATCH,
		"HTTP static mount accepted a partial segment");
	testRequire((iSize == 0) && !bTrailingSlash &&
		(xrtGetError() == NULL),
		"HTTP static route miss left an error or metadata");
}



/* 验证默认安全策略与显式 POSIX 放宽路径。 */
static void testHttpStaticPathPolicy(void)
{
	static const cstr arrRejected[] = {
		"/.env",
		"/a/.hidden",
		"/CON.txt",
		"/dir/trailing.",
		"/bad%3Fname",
		"/bad%3Aname"
	};
	xhttpstaticpathconfig Config;
	char Output[64];
	size_t iSize;
	bool bTrailingSlash;
	size_t i;

	for ( i = 0;
		i < (sizeof(arrRejected) / sizeof(arrRejected[0]));
		i++ ) {
		testRequire(xrtHttpStaticPathWrite(
			xrtStrView(arrRejected[i]),
			NULL,
			Output,
			sizeof(Output),
			&iSize,
			&bTrailingSlash
		) == XHTTP_STATIC_PATH_ERROR,
			"HTTP static default path policy accepted an unsafe name");
		xrtClearError();
	}

	xrtHttpStaticPathConfigInit(&Config);
	Config.Flags = XHTTP_STATIC_PATH_ALLOW_HIDDEN;
	testRequire(testHttpStaticPathWrite(
		XRT_STR_LITERAL("/.env/CON.txt/a%3Fb/trailing."),
		&Config,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_MATCH,
		"HTTP static POSIX path policy did not release portable restrictions");
	testRequire(strcmp(
		Output,
		".env/CON.txt/a?b/trailing."
	) == 0,
		"HTTP static POSIX path output mismatch");
}



/* 验证结构、percent、UTF-8 和路径穿越输入全部失败。 */
static void testHttpStaticPathInvalid(void)
{
	static const cstr arrInvalid[] = {
		"",
		"relative",
		"/%",
		"/%2",
		"/%GG",
		"/a/%2F/b",
		"/a/%5C/b",
		"/a/%00/b",
		"/a//b",
		"/a/./b",
		"/a/%2e%2e/b",
		"/a?query",
		"/a#fragment",
		"/%C0%AF"
	};
	char Output[32] = "unchanged";
	size_t iSize = 0;
	bool bTrailingSlash = false;
	size_t i;

	for ( i = 0;
		i < (sizeof(arrInvalid) / sizeof(arrInvalid[0]));
		i++ ) {
		testRequire(xrtHttpStaticPathWrite(
			xrtStrView(arrInvalid[i]),
			NULL,
			Output,
			sizeof(Output),
			&iSize,
			&bTrailingSlash
		) == XHTTP_STATIC_PATH_ERROR,
			"HTTP static invalid URL path was accepted");
		xrtClearError();
	}
}



/* 验证容量失败与输入输出别名都保持文本输出不变。 */
static void testHttpStaticPathAtomic(void)
{
	char Small[8] = "guard";
	char Alias[32] = "/assets/app.js";
	size_t iSize = 0;
	bool bTrailingSlash = false;

	testRequire(xrtHttpStaticPathWrite(
		XRT_STR_LITERAL("/assets/app.js"),
		NULL,
		Small,
		sizeof(Small),
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_ERROR,
		"HTTP static short output capacity was accepted");
	testRequire((iSize == 13u) && (strcmp(Small, "guard") == 0),
		"HTTP static short output modified data or lost required size");
	xrtClearError();

	testRequire(xrtHttpStaticPathWrite(
		(xstrview){ Alias, strlen(Alias) },
		NULL,
		Alias,
		sizeof(Alias),
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_ERROR,
		"HTTP static overlapping output was accepted");
	testRequire(strcmp(Alias, "/assets/app.js") == 0,
		"HTTP static alias rejection modified input");
}



/* 验证拥有型便捷结果覆盖命中、未命中、释放和重用前置条件。 */
static void testHttpStaticPathOwned(void)
{
	xhttpstaticpathconfig Config;
	xhttpstaticpath Path;

	xrtHttpStaticPathConfigInit(&Config);
	Config.Mount = XRT_STR_LITERAL("/public");
	xrtHttpStaticPathInit(&Path);
	testRequire(xrtHttpStaticPathMap(
		XRT_STR_LITERAL("/public/icons/logo.png"),
		&Config,
		&Path
	) && Path.Matched &&
		(strcmp(Path.Path, "icons/logo.png") == 0) &&
		(Path.Size == 14u) &&
		!Path.TrailingSlash,
		"HTTP static owned path mapping mismatch");
	xrtHttpStaticPathFree(&Path);

	testRequire(xrtHttpStaticPathMap(
		XRT_STR_LITERAL("/private/logo.png"),
		&Config,
		&Path
	) && !Path.Matched && (Path.Path == NULL),
		"HTTP static owned route miss mismatch");
	xrtHttpStaticPathFree(&Path);
}



/* 运行静态路径映射的完整公开契约。 */
/* 验证固定结构、标量输出和拥有型结果支持未对齐存储并拒绝回绕区间。 */
static void testHttpStaticPathMemoryContract(void)
{
	uint8 ConfigStorage[sizeof(xhttpstaticpathconfig) + 2u];
	uint8 PathStorage[sizeof(xhttpstaticpath) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 TrailingStorage[sizeof(bool) + 2u];
	xhttpstaticpathconfig Config;
	xhttpstaticpathconfig LoadedConfig;
	xhttpstaticpath LoadedPath;
	xhttpstaticpath* pPath = (xhttpstaticpath*)(PathStorage + 1u);
	size_t iSize;
	bool bTrailingSlash;
	char Output[32];

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpStaticPathConfigInit(
		(xhttpstaticpathconfig*)(ConfigStorage + 1u)
	);
	memcpy(
		&LoadedConfig,
		ConfigStorage + 1u,
		sizeof(LoadedConfig)
	);
	testRequire((ConfigStorage[0] == 0xA5u) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5u) &&
		(LoadedConfig.Mount.Size == 1u) &&
		(LoadedConfig.Mount.Data[0] == '/') &&
		(LoadedConfig.Flags == XHTTP_STATIC_PATH_PORTABLE),
		"HTTP static path unaligned config initialization failed");

	Config = LoadedConfig;
	Config.Mount = XRT_STR_LITERAL("/assets");
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	memset(SizeStorage, 0x5A, sizeof(SizeStorage));
	memset(TrailingStorage, 0x5A, sizeof(TrailingStorage));
	testRequire(xrtHttpStaticPathWrite(
		XRT_STR_LITERAL("/assets/app.js"),
		(const xhttpstaticpathconfig*)(ConfigStorage + 1u),
		Output,
		sizeof(Output),
		(size_t*)(SizeStorage + 1u),
		(bool*)(TrailingStorage + 1u)
	) == XHTTP_STATIC_PATH_MATCH,
		"HTTP static path rejected unaligned fixed storage");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	memcpy(
		&bTrailingSlash,
		TrailingStorage + 1u,
		sizeof(bTrailingSlash)
	);
	testRequire((iSize == 6u) && !bTrailingSlash &&
		(strcmp(Output, "app.js") == 0) &&
		(SizeStorage[0] == 0x5Au) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0x5Au) &&
		(TrailingStorage[0] == 0x5Au) &&
		(TrailingStorage[sizeof(TrailingStorage) - 1u] == 0x5Au),
		"HTTP static path unaligned metadata output failed");

	memset(PathStorage, 0xC3, sizeof(PathStorage));
	xrtHttpStaticPathInit(pPath);
	testRequire(xrtHttpStaticPathMap(
		XRT_STR_LITERAL("/assets/icons/logo.png"),
		(const xhttpstaticpathconfig*)(ConfigStorage + 1u),
		pPath
	), "HTTP static path rejected an unaligned owned result");
	memcpy(&LoadedPath, PathStorage + 1u, sizeof(LoadedPath));
	testRequire(LoadedPath.Matched &&
		(LoadedPath.Size == 14u) &&
		(strcmp(LoadedPath.Path, "icons/logo.png") == 0) &&
		(PathStorage[0] == 0xC3u) &&
		(PathStorage[sizeof(PathStorage) - 1u] == 0xC3u),
		"HTTP static path unaligned owned result mismatch");
	xrtHttpStaticPathFree(pPath);
	memcpy(&LoadedPath, PathStorage + 1u, sizeof(LoadedPath));
	testRequire((LoadedPath.Path == NULL) &&
		(LoadedPath.Size == 0) && !LoadedPath.Matched &&
		!LoadedPath.TrailingSlash,
		"HTTP static path unaligned free did not clear the result");

	xrtClearError();
	xrtHttpStaticPathConfigInit(
		(xhttpstaticpathconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtGetError() != NULL,
		"HTTP static path accepted a wrapping config output");
	xrtClearError();
	testRequire(xrtHttpStaticPathWrite(
		XRT_STR_LITERAL("/asset"),
		(const xhttpstaticpathconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_ERROR,
		"HTTP static path accepted a wrapping config input");
	xrtClearError();
	testRequire(xrtHttpStaticPathWrite(
		(xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
			4u
		},
		NULL,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_ERROR,
		"HTTP static path accepted a wrapping text input");
	xrtClearError();
	testRequire(!xrtHttpStaticPathMap(
		XRT_STR_LITERAL("/asset"),
		NULL,
		(xhttpstaticpath*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP static path accepted a wrapping owned result");
	xrtClearError();
}



int main(void)
{
	testHttpStaticPathRoot();
	testHttpStaticPathMount();
	testHttpStaticPathPolicy();
	testHttpStaticPathInvalid();
	testHttpStaticPathAtomic();
	testHttpStaticPathOwned();
	testHttpStaticPathMemoryContract();
	printf("[PASS] http_static_path\n");
	return 0;
}
