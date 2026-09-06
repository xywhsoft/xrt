/*
 * 范例：text/pattern_tour —— 字段模式引擎全接口（编译/Builder/错误）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【一次性提取】  xrtPatternExtract / ExtractConfig（免编译整串匹配，
 *                   容量不足时报告所需捕获数）
 *   【单条编译】    xrtPatternCompile / CompileConfig / Ref /
 *                   ConfigInit（自定义分隔符与预算）
 *   【批量编译】    xrtPatternCompileMany / CompileManyConfig
 *   【编译自省】    Count / CompiledBytes / Separators / Source /
 *                   Id / Value / CaptureCount / MaxCaptureCount /
 *                   CaptureName / CaptureIndex
 *   【匹配三态】    Lookup（只选最优）/ Match（含捕获）/ Test（只判命中）
 *   【错误机器数据】 xrtPatternErrorOffset（模式内字节位置）/
 *                   xrtPatternErrorPattern（批量编译失败索引）
 *   【Builder 族】  Create / CreateConfig / Add / AddMany / Set /
 *                   Remove / Reserve / Count / Version / Dirty /
 *                   Clear / Free
 * 模块宏：XRT_MODULE_PATTERN
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/text/pattern_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   pattern: extract "jane/example.org" -> jane / example.org
 *   pattern: single compiled captures=2 name=user
 *   pattern: compile count=2 max-captures=2 id-nonzero=1
 *   pattern: match id->value->first="Jane"
 *   pattern: lookup/test = 1/0
 *   pattern: builder version=5 dirty=0 count=2
 *   pattern: error offset=3 pattern-index=1
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xpattern* pSingle = NULL;
	xpattern* pMulti = NULL;
	xpattern* pRef = NULL;
	xpattern* pBuilt = NULL;
	xpatternbuilder* pBuilder = NULL;
	xpatternconfig Config;
	xpatternspec Specs[2];
	xpatternspec Extra;
	xpatternmatch Match;
	xstrview Captures[4];
	xerror* pError;
	xpatternid Ids[2];
	xpatternid IdMail = 0;
	xpatternid IdUser = 0;
	size_t iNeeded = 0;
	size_t iOffset = 0;
	size_t iBadIndex = 99;
	uint64 iVersion = 0;
	int iResult = 1;

	/* ---- 一次性提取：{user}@{host} ---- */
	iNeeded = 2;
	if ( (xrtPatternExtract(SV("{user}/{host}"),
			SV("jane/example.org"), Captures, 4u,
			&iNeeded) != XPATTERN_MATCH) ||
		(iNeeded != 2u) ||
		(Captures[0].Size != 4u) ||
		(memcmp(Captures[0].Data, "jane", 4u) != 0) ||
		(Captures[1].Size != 11u) ||
		(memcmp(Captures[1].Data, "example.org", 11u) != 0) ) {
		goto Cleanup;
	}
	printf("pattern: extract \"jane/example.org\" -> ");
	printf("%.*s / %.*s\n",
		(int)Captures[0].Size, Captures[0].Data,
		(int)Captures[1].Size, Captures[1].Data);
	/* 容量不足：返回错误并报告所需数量，不产生部分捕获。 */
	iNeeded = 0;
	if ( xrtPatternExtract(SV("{a}/{b}"), SV("x/y"), Captures, 1u,
			&iNeeded) != XPATTERN_ERROR ) {
		goto Cleanup;
	}
	/* 默认分隔符集合只有 '/'；把 '@' 加入后邮件模式才合法。 */
	xrtPatternConfigInit(&Config);
	Config.Separators = SV("@/");
	iNeeded = 2;
	if ( xrtPatternExtractConfig(SV("{u}@{h}"),
			SV("bob@x.io"), &Config, Captures, 4u,
			&iNeeded) != XPATTERN_MATCH ) {
		goto Cleanup;
	}

	/* ---- 单条编译 + 自省 ---- */
	pSingle = xrtPatternCompileConfig(SV("{user}@{host}"), &Config);
	if ( (pSingle == NULL) ||
		(xrtPatternRef(pSingle) != pSingle) ||
		(xrtPatternCount(pSingle) != 1u) ||
		(xrtPatternCompiledBytes(pSingle) == 0u) ||
		(xrtPatternSeparators(pSingle).Size != 2u) ||
		(xrtPatternSource(pSingle, 0u).Size != 13u) ||
		(xrtPatternId(pSingle, 0u) == XPATTERN_ID_INVALID) ||
		(xrtPatternCaptureCount(pSingle, 0u) != 2u) ||
		(xrtPatternMaxCaptureCount(pSingle) != 2u) ||
		!xrtPatternCaptureName(pSingle, 0u, 0u, &Captures[2]) ||
		(Captures[2].Size != 4u) ||
		(memcmp(Captures[2].Data, "user", 4u) != 0) ||
		(xrtPatternCaptureIndex(pSingle, 0u, SV("host")) != 1u) ) {
		goto Cleanup;
	}
	printf("pattern: single compiled captures=2 name=user\n");

	/* ---- 批量编译：邮件全捕获 + 前缀捕获，值与优先级 ---- */
	Specs[0].Pattern = SV("{user}@{host}");
	Specs[0].Value = (ptr)"Jane";
	Specs[0].Priority = 0;
	Specs[0].Flags = 0;
	Specs[1].Pattern = SV("mail/{*rest}");
	Specs[1].Value = (ptr)"Mailbox";
	Specs[1].Priority = 1;
	Specs[1].Flags = 0;
	pMulti = xrtPatternCompileManyConfig(Specs, 2u, &Config);
	if ( (pMulti == NULL) ||
		(xrtPatternCount(pMulti) != 2u) ||
		(xrtPatternValue(pMulti, 0u) != (ptr)"Jane") ||
		(xrtPatternId(pMulti, 1u) == XPATTERN_ID_INVALID) ) {
		goto Cleanup;
	}
	printf("pattern: compile count=2 max-captures=%zu id-nonzero=1\n",
		xrtPatternMaxCaptureCount(pMulti));

	/* ---- 匹配三态：Match / Lookup / Test ---- */
	if ( (xrtPatternMatch(pMulti, SV("amy@corp.io"), Captures, 4u,
			&Match) != XPATTERN_MATCH) ||
		(Match.PatternIndex != 0u) ||
		(Match.Value != (ptr)"Jane") ||
		(Match.CaptureCount != 2u) ||
		(Captures[0].Size != 3u) ||
		(memcmp(Captures[0].Data, "amy", 3u) != 0) ) {
		goto Cleanup;
	}
	printf("pattern: match id->value->first=\"%s\"\n",
		(const char*)Match.Value);
	/* Lookup：高优先级的 mail: 前缀模式胜出。 */
	if ( (xrtPatternLookup(pMulti, SV("mail/hello"),
			&Match) != XPATTERN_MATCH) ||
		(Match.PatternIndex != 1u) ||
		(Match.Value != (ptr)"Mailbox") ||
		(xrtPatternLookup(pMulti, SV("nothing"),
			&Match) != XPATTERN_NONE) ) {
		goto Cleanup;
	}
	if ( (xrtPatternTest(pMulti, SV("a@b")) != XPATTERN_MATCH) ||
		(xrtPatternTest(pMulti, SV("-")) != XPATTERN_NONE) ) {
		goto Cleanup;
	}
	printf("pattern: lookup/test = 1/0\n");

	/* ---- Builder 族：增量构建 + 修改 + 版本 ---- */
	pBuilder = xrtPatternBuilderCreateConfig(&Config);
	Extra.Pattern = SV("{who}@{where}");
	Extra.Value = (ptr)"Builder";
	Extra.Priority = 0;
	Extra.Flags = 0;
	if ( (pBuilder == NULL) ||
		!xrtPatternBuilderReserve(pBuilder, 8u) ||
		((IdMail = xrtPatternBuilderAdd(pBuilder,
			&Extra)) == XPATTERN_ID_INVALID) ||
		(xrtPatternBuilderCount(pBuilder) != 1u) ||
		!xrtPatternBuilderDirty(pBuilder) ) {
		goto Cleanup;
	}
	/* AddMany 原子追加两条（含后来的 Set/Remove 靶子）。 */
	{
		xpatternspec More[2];

		More[0] = Extra;
		More[0].Pattern = SV("v1/{*a}");
		More[1] = Extra;
		More[1].Pattern = SV("v2/{*b}");
		if ( !xrtPatternBuilderAddMany(pBuilder, More, 2u,
				Ids) ||
			(Ids[0] == XPATTERN_ID_INVALID) ||
			(Ids[1] == XPATTERN_ID_INVALID) ||
			(xrtPatternBuilderCount(pBuilder) != 3u) ) {
			goto Cleanup;
		}
		IdUser = Ids[0];
	}
	/* Set：替换 IdUser 的模式，保留 ID 与顺序。 */
	Extra.Pattern = SV("v1x/{*a}");
	if ( !xrtPatternBuilderSet(pBuilder, IdUser, &Extra) ||
		!xrtPatternBuilderRemove(pBuilder, Ids[1]) ||
		(xrtPatternBuilderCount(pBuilder) != 2u) ||
		xrtPatternBuilderRemove(pBuilder, Ids[1]) ) {
		goto Cleanup;
	}
	iVersion = xrtPatternBuilderVersion(pBuilder);
	pBuilt = xrtPatternBuilderCompile(pBuilder);
	if ( (pBuilt == NULL) ||
		(xrtPatternCount(pBuilt) != 2u) ||
		xrtPatternBuilderDirty(pBuilder) ||
		(xrtPatternBuilderCompile(pBuilder) == NULL) ) {
		goto Cleanup;  /* 二次编译复用缓存（同引用） */
	}
	printf("pattern: builder version=%llu dirty=%d count=%zu\n",
		(unsigned long long)iVersion,
		xrtPatternBuilderDirty(pBuilder) ? 1 : 0,
		xrtPatternBuilderCount(pBuilder));
	/* Clear 清空后再编译出空程序。 */
	xrtPatternBuilderClear(pBuilder);
	if ( (xrtPatternBuilderCount(pBuilder) != 0u) ||
		((pRef = xrtPatternBuilderCompile(pBuilder)) == NULL) ||
		(xrtPatternCount(pRef) != 0u) ) {
		goto Cleanup;
	}

	/* ---- 错误机器数据：Offset（单条）与 Pattern（批量索引） ---- */
	{
		xpattern* pBad = xrtPatternCompile(SV("{a}{b}"));

		if ( (pBad != NULL) ||
			((pError = xrtTakeError()) == NULL) ||
			!xrtPatternErrorOffset(pError, &iOffset) ) {
			goto Cleanup;
		}
		xrtErrorFree(pError);
	}
	{
		static xpatternspec BadSpecs[2] = {
			{ XRT_STR_INIT("{ok}"), NULL, 0, 0 },
			{ XRT_STR_INIT("{bad}{x}"), NULL, 0, 0 }
		};
		xpattern* pBadMany = xrtPatternCompileMany(BadSpecs, 2u);

		if ( (pBadMany != NULL) ||
			((pError = xrtTakeError()) == NULL) ||
			!xrtPatternErrorPattern(pError, &iBadIndex) ||
			(iBadIndex != 1u) ) {
			goto Cleanup;
		}
		xrtErrorFree(pError);
	}
	printf("pattern: error offset=%zu pattern-index=%zu\n",
		iOffset, iBadIndex);
	iResult = 0;

Cleanup:
	xrtPatternRelease(pBuilt);
	xrtPatternRelease(pRef);
	xrtPatternBuilderFree(pBuilder);
	xrtPatternRelease(pMulti);
	xrtPatternRelease(pSingle);
	xrtPatternRelease(pSingle);  /* Ref 那份 */
	return iResult;
}
