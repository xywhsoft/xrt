#include "../test.h"



static bool testPatternViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 一次性解析器与编译扫描器必须给出完全相同的命中和顺序捕获。 */
static void testPatternSingleDifferential(
	xstrview Separators,
	xstrview Pattern,
	const xstrview* arrText,
	size_t iTextCount
)
{
	xpatternconfig Config;
	xpattern* pPattern;

	xrtPatternConfigInit(&Config);
	Config.Separators = Separators;
	pPattern = xrtPatternCompileConfig(Pattern, &Config);
	testRequire(pPattern != NULL, "differential pattern compile failed");
	for ( size_t i = 0; i < iTextCount; i++ ) {
		xstrview arrDirect[8];
		xstrview arrCompiled[8];
		xpatternmatch Match;
		size_t iDirectCount = 0;
		xpatternresult iDirect = xrtPatternExtractConfig(
			Pattern,
			arrText[i],
			&Config,
			arrDirect,
			8u,
			&iDirectCount
		);
		xpatternresult iCompiled = xrtPatternMatch(
			pPattern,
			arrText[i],
			arrCompiled,
			8u,
			&Match
		);

		testRequire(iDirect == iCompiled, "direct and compiled results differ");
		if ( iDirect == XPATTERN_MATCH ) {
			testRequire(
				iDirectCount == Match.CaptureCount,
				"direct and compiled capture counts differ"
			);
			for ( size_t j = 0; j < iDirectCount; j++ ) {
				testRequire(
					testPatternViewEqual(arrDirect[j], arrCompiled[j]),
					"direct and compiled captures differ"
				);
			}
		}
	}
	xrtPatternRelease(pPattern);
}



/* 覆盖无分隔符、专用 1～4 字节扫描器以及 bitmap 扫描器。 */
static void testPatternScanners(void)
{
	const xstrview arrGeneralText[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("a"),
		XRT_STR_INIT("a/b.c;d:e,f"),
		XRT_STR_INIT("a/b.c;d:e/f"),
		XRT_STR_INIT("a//b.c;d:e,f"),
		XRT_STR_INIT("a/b.c;d:e,")
	};
	const xstrview arrNoneText[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("whole"),
		XRT_STR_INIT("whole/including.everything")
	};
	const xstrview arrSlashText[] = {
		XRT_STR_INIT("/a/b"),
		XRT_STR_INIT("/a/"),
		XRT_STR_INIT("/a/b/c"),
		XRT_STR_INIT("a/b")
	};
	const xstrview arrAffixNoneText[] = {
		XRT_STR_INIT("preXsuf"), XRT_STR_INIT("presuf"),
		XRT_STR_INIT("preXYZsuf"), XRT_STR_INIT("xpreXsuf"),
		XRT_STR_INIT("preXsufz")
	};
	const xstrview arrAffixSlashText[] = {
		XRT_STR_INIT("/preXsuf"), XRT_STR_INIT("/presuf"),
		XRT_STR_INIT("/preXYZsuf"), XRT_STR_INIT("/preX/suf"),
		XRT_STR_INIT("preXsuf")
	};
	const xstrview arrAffixBitmapText[] = {
		XRT_STR_INIT("a/preXsuf.endYtail"),
		XRT_STR_INIT("a/preXYZsuf.endZtail"),
		XRT_STR_INIT("a/presuf.endYtail"),
		XRT_STR_INIT("a/preXsuf/endYtail"),
		XRT_STR_INIT("a/preXsuf.endtail")
	};

	testPatternSingleDifferential(
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("{value}"),
		arrNoneText,
		sizeof(arrNoneText) / sizeof(arrNoneText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("pre{value}suf"),
		arrAffixNoneText,
		sizeof(arrAffixNoneText) / sizeof(arrAffixNoneText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("/"),
		XRT_STR_LITERAL("/{left}/{right}"),
		arrSlashText,
		sizeof(arrSlashText) / sizeof(arrSlashText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("/"),
		XRT_STR_LITERAL("/pre{value}suf"),
		arrAffixSlashText,
		sizeof(arrAffixSlashText) / sizeof(arrAffixSlashText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("."),
		XRT_STR_LITERAL("{a}.{b}"),
		arrGeneralText,
		sizeof(arrGeneralText) / sizeof(arrGeneralText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("/."),
		XRT_STR_LITERAL("{a}/{b}.{c}"),
		arrGeneralText,
		sizeof(arrGeneralText) / sizeof(arrGeneralText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("/.;"),
		XRT_STR_LITERAL("{a}/{b}.{c};{d}"),
		arrGeneralText,
		sizeof(arrGeneralText) / sizeof(arrGeneralText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("/.;:"),
		XRT_STR_LITERAL("{a}/{b}.{c};{d}:{e}"),
		arrGeneralText,
		sizeof(arrGeneralText) / sizeof(arrGeneralText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("/.;:,"),
		XRT_STR_LITERAL("{a}/{b}.{c};{d}:{e},{f}"),
		arrGeneralText,
		sizeof(arrGeneralText) / sizeof(arrGeneralText[0])
	);
	testPatternSingleDifferential(
		XRT_STR_LITERAL("/.;:,"),
		XRT_STR_LITERAL("a/pre{x}suf.end{y}tail"),
		arrAffixBitmapText,
		sizeof(arrAffixBitmapText) / sizeof(arrAffixBitmapText[0])
	);
}



/*
	逐条编译充当独立成员资格判定；批量结果必须选择预先按结构特异度
	排列的第一个命中项，从而覆盖多层 literal/capture/tail 回退。
*/
static void testPatternBatchDifferential(void)
{
	static const xstrview arrSource[] = {
		XRT_STR_INIT("/a/b/c"),
		XRT_STR_INIT("/a/b{x}c/d"),
		XRT_STR_INIT("/a/{x}c/d"),
		XRT_STR_INIT("/a/{x}/d"),
		XRT_STR_INIT("/a/{x}/{*tail}"),
		XRT_STR_INIT("/{x}/b/e"),
		XRT_STR_INIT("/{x}/{y}/f"),
		XRT_STR_INIT("/{x}/{y}/{z}"),
		XRT_STR_INIT("/{x}/{*tail}"),
		XRT_STR_INIT("/{*all}")
	};
	static const xstrview arrText[] = {
		XRT_STR_INIT(""), XRT_STR_INIT("/"), XRT_STR_INIT("/a"),
		XRT_STR_INIT("/a/"), XRT_STR_INIT("/a/b"),
		XRT_STR_INIT("/a/b/c"), XRT_STR_INIT("/a/b/d"),
		XRT_STR_INIT("/a/bQc/d"), XRT_STR_INIT("/a/qc/d"),
		XRT_STR_INIT("/a/bc/d"),
		XRT_STR_INIT("/a/b/e"), XRT_STR_INIT("/a/b/f"),
		XRT_STR_INIT("/a/b/z"), XRT_STR_INIT("/a/b/c/d"),
		XRT_STR_INIT("/q/b/e"), XRT_STR_INIT("/q/r/f"),
		XRT_STR_INIT("/q/r/z"), XRT_STR_INIT("/q/r/z/t"),
		XRT_STR_INIT("//"), XRT_STR_INIT("/a//d"),
		XRT_STR_INIT("plain")
	};
	xpatternspec arrSpec[sizeof(arrSource) / sizeof(arrSource[0])];
	xpattern* arrSingle[sizeof(arrSource) / sizeof(arrSource[0])];
	xpattern* pBatch;

	memset(arrSpec, 0, sizeof(arrSpec));
	for ( size_t i = 0; i < sizeof(arrSource) / sizeof(arrSource[0]); i++ ) {
		arrSpec[i].Pattern = arrSource[i];
		arrSpec[i].Value = (ptr)(uintptr_t)(i + 1u);
		arrSingle[i] = xrtPatternCompile(arrSource[i]);
		testRequire(arrSingle[i] != NULL, "single membership compile failed");
	}
	pBatch = xrtPatternCompileMany(
		arrSpec,
		sizeof(arrSpec) / sizeof(arrSpec[0])
	);
	testRequire(pBatch != NULL, "batch differential compile failed");
	for ( size_t i = 0; i < sizeof(arrText) / sizeof(arrText[0]); i++ ) {
		size_t iExpected = XRT_NPOS;
		xpatternmatch Match;
		xpatternresult iResult;

		for ( size_t j = 0; j < sizeof(arrSingle) / sizeof(arrSingle[0]); j++ ) {
			if ( xrtPatternTest(arrSingle[j], arrText[i]) == XPATTERN_MATCH ) {
				iExpected = j;
				break;
			}
		}
		iResult = xrtPatternLookup(pBatch, arrText[i], &Match);
		if ( iExpected == XRT_NPOS ) {
			testRequire(iResult == XPATTERN_NONE, "batch produced a false hit");
		} else {
			testRequire(iResult == XPATTERN_MATCH, "batch lost a member hit");
			testRequire(
				Match.Value == (ptr)(uintptr_t)(iExpected + 1u),
				"batch selected the wrong structural winner"
			);
		}
	}
	for ( size_t i = 0; i < sizeof(arrSingle) / sizeof(arrSingle[0]); i++ ) {
		xrtPatternRelease(arrSingle[i]);
	}
	xrtPatternRelease(pBatch);
}



int main(void)
{
	testPatternScanners();
	testPatternBatchDifferential();
	printf("[PASS] pattern differential\n");
	return 0;
}
