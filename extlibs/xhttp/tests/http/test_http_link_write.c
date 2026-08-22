#include "../test.h"

#include <xrt/http_link.h>



/* 验证标准属性、无值扩展和多元素规范写出。 */
static void testLinkWrite(void)
{
	static const xhttplinkparamvalue Params[] = {
		{
			XRT_STR_INIT("anchor"),
			XRT_STR_INIT("#toc"),
			XHTTP_PARAM_HAS_VALUE
		},
		{
			XRT_STR_INIT("hreflang"),
			XRT_STR_INIT("de"),
			XHTTP_PARAM_HAS_VALUE
		},
		{
			XRT_STR_INIT("title"),
			XRT_STR_INIT("a \"quoted\" title"),
			XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
		},
		{
			XRT_STR_INIT("title*"),
			XRT_STR_INIT("UTF-8'de'letztes%20Kapitel"),
			XHTTP_PARAM_HAS_VALUE
		},
		{
			XRT_STR_INIT("type"),
			XRT_STR_INIT("text/html"),
			XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
		},
		{
			XRT_STR_INIT("crossorigin"),
			XRT_STR_INIT(""),
			XHTTP_PARAM_NONE
		}
	};
	static const xhttplinkvalue Links[] = {
		{
			XRT_STR_INIT("/TheBook/chapter2"),
			XRT_STR_INIT("previous next"),
			Params,
			6u
		},
		{
			XRT_STR_INIT("https://example.test/index"),
			XRT_STR_INIT("canonical"),
			NULL,
			0
		}
	};
	static const char Expected[] =
		"</TheBook/chapter2>; rel=\"previous next\"; "
		"anchor=#toc; hreflang=de; "
		"title=\"a \\\"quoted\\\" title\"; "
		"title*=UTF-8'de'letztes%20Kapitel; "
		"type=\"text/html\"; crossorigin, "
		"<https://example.test/index>; rel=canonical";
	char sOutput[384];
	size_t iSize;
	str sBuilt;

	testRequire(
		xrtHttpLinkWrite(
			Links, 2u, NULL, 0, &iSize
		) && (iSize == (sizeof(Expected) - 1u)),
		"Link writer size query mismatch"
	);
	testRequire(
		xrtHttpLinkWrite(
			Links, 2u, sOutput,
			sizeof(sOutput), &iSize
		) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(sOutput, Expected, iSize) == 0) &&
		xrtHttpLinkValid((xstrview){ sOutput, iSize }),
		"Link writer output mismatch"
	);
	sBuilt = xrtHttpLinkBuild(Links, 2u, &iSize);
	testRequire(
		(sBuilt != NULL) &&
		(iSize == (sizeof(Expected) - 1u)) &&
		(strcmp(sBuilt, Expected) == 0),
		"Link build mismatch"
	);
	xrtFree(sBuilt);
	testRequire(
		xrtHttpLinkWrite(NULL, 0, NULL, 0, &iSize) &&
		(iSize == 0),
		"Link writer rejected an empty list"
	);
}



/* 验证生产侧重复、语义和短缓冲失败原子性。 */
static void testLinkWriteFailure(void)
{
	static const xhttplinkparamvalue Duplicate[] = {
		{
			XRT_STR_INIT("title"),
			XRT_STR_INIT("first"),
			XHTTP_PARAM_HAS_VALUE
		},
		{
			XRT_STR_INIT("TITLE"),
			XRT_STR_INIT("second"),
			XHTTP_PARAM_HAS_VALUE
		}
	};
	static const xhttplinkparamvalue Rel[] = {
		{
			XRT_STR_INIT("rel"),
			XRT_STR_INIT("last"),
			XHTTP_PARAM_HAS_VALUE
		}
	};
	xhttplinkvalue Link;
	union {
		xhttplinkvalue Link;
		size_t Size;
	} Shared;
	char sTarget[] = "/x";
	char sOutput[8];
	char sSaved[8];
	size_t iSize;

	memset(&Link, 0, sizeof(Link));
	Link.Target = XRT_STR_LITERAL("/x");
	Link.Relations = XRT_STR_LITERAL("next");
	Link.Parameters = Duplicate;
	Link.ParameterCount = 2u;
	testRequire(!xrtHttpLinkElementWrite(
		&Link, NULL, 0, &iSize
	), "Link writer accepted duplicate singleton");
	xrtClearError();
	Link.Parameters = Rel;
	Link.ParameterCount = 1u;
	testRequire(!xrtHttpLinkElementWrite(
		&Link, NULL, 0, &iSize
	), "Link writer accepted rel in parameter array");
	xrtClearError();
	Link.Relations = XRT_STR_LITERAL("Next");
	Link.Parameters = NULL;
	Link.ParameterCount = 0;
	testRequire(!xrtHttpLinkElementWrite(
		&Link, NULL, 0, &iSize
	), "Link writer accepted invalid registered relation");
	xrtClearError();
	Link.Relations = XRT_STR_LITERAL("next");
	Link.Parameters = Duplicate;
	Link.ParameterCount = SIZE_MAX;
	iSize = 0xA5A5u;
	testRequire(
		!xrtHttpLinkElementWrite(
			&Link, NULL, 0, &iSize
		) && (iSize == 0xA5A5u),
		"Link writer traversed an overflowing parameter array"
	);
	xrtClearError();
	Link.Parameters = NULL;
	Link.ParameterCount = 0;
	Link.Relations = XRT_STR_LITERAL("next");
	memset(sOutput, 0xA5, sizeof(sOutput));
	memcpy(sSaved, sOutput, sizeof(sSaved));
	testRequire(
		!xrtHttpLinkElementWrite(
			&Link, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 14u) &&
		(memcmp(sOutput, sSaved, sizeof(sOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Link writer short output was not atomic"
	);
	xrtClearError();
	testRequire(!xrtHttpLinkElementWrite(
		&Link, &Link, sizeof(Link), &iSize
	), "Link writer accepted descriptor/output overlap");
	xrtClearError();
	Link.Target = (xstrview){ sTarget, sizeof(sTarget) - 1u };
	testRequire(!xrtHttpLinkElementWrite(
		&Link, sTarget, sizeof(sTarget), &iSize
	), "Link writer accepted target/output overlap");
	xrtClearError();
	Shared.Link = Link;
	testRequire(!xrtHttpLinkElementWrite(
		&Shared.Link, NULL, 0, &Shared.Size
	), "Link writer accepted descriptor/size overlap");
	xrtClearError();
}



/* 执行 RFC 8288 Link writer 测试。 */
int main(void)
{
	testLinkWrite();
	testLinkWriteFailure();
	printf("[PASS] http_link_write\n");
	return 0;
}
