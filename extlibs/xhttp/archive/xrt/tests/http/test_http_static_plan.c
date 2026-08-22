#include "../test.h"



/* 构造带强 ETag 和强修改时间的静态表示。 */
static xhttprepresentation testHttpStaticCurrent(void)
{
	xhttprepresentation Current;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	Current.HasETag = true;
	Current.HasLastModified = true;
	Current.LastModifiedStrong = true;
	Current.ETag.Opaque = XRT_STR_LITERAL("asset-7");
	Current.LastModified = INT64_C(784111777000000);
	return Current;
}



/* 使用默认范围容量构建计划。 */
static bool testHttpStaticPlan(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent,
	uint64 iLength,
	xhttpbyterange* pRanges,
	xhttpstaticplan* pPlan
)
{
	xhttpstaticplanconfig Config;

	xrtHttpStaticPlanConfigInit(&Config);
	return xrtHttpStaticPlanBuild(
		Method,
		pFields,
		iFieldCount,
		pCurrent,
		iLength,
		pRanges,
		16,
		&Config,
		pPlan
	);
}



/* 验证完整 GET、HEAD 和不受支持方法的基础状态。 */
static void testHttpStaticMethods(void)
{
	xhttprepresentation Current = testHttpStaticCurrent();
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK) &&
		Plan.SendBody && Plan.AcceptRanges &&
		(Plan.CompleteLength == 100) &&
		(Plan.SelectedLength == 100) &&
		(Plan.RangeCount == 0),
		"HTTP static full GET plan mismatch");
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("HEAD"),
		NULL,
		0,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK) &&
		!Plan.SendBody && Plan.AcceptRanges,
		"HTTP static HEAD plan mismatch");
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("POST"),
		NULL,
		0,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_METHOD_NOT_ALLOWED) &&
		!Plan.SendBody && !Plan.AcceptRanges &&
		(Plan.SelectedLength == 0),
		"HTTP static method rejection mismatch");
}



/* 验证条件请求状态先于 Range 计算。 */
static void testHttpStaticConditions(void)
{
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("If-None-Match"),
			XRT_STR_INIT("\"asset-7\"")
		},
		{
			XRT_STR_INIT("Range"),
			XRT_STR_INIT("bytes=0-9")
		}
	};
	xhttprepresentation Current = testHttpStaticCurrent();
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_NOT_MODIFIED) &&
		!Plan.SendBody && (Plan.RangeCount == 0),
		"HTTP static If-None-Match did not precede Range");
	Fields[0].Name = XRT_STR_LITERAL("If-Match");
	Fields[0].Value = XRT_STR_LITERAL("\"other\"");
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_PRECONDITION_FAILED) &&
		!Plan.SendBody,
		"HTTP static failed precondition plan mismatch");
}



/* 验证多范围会排序、裁剪并合并重叠或相邻区间。 */
static void testHttpStaticRanges(void)
{
	xhttpfield Field = {
		XRT_STR_INIT("Range"),
		XRT_STR_INIT("bytes=80-120, 0-9, 8-19, 30-39, 40-49")
	};
	xhttprepresentation Current = testHttpStaticCurrent();
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		&Field,
		1,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		Plan.SendBody && (Plan.RangeCount == 3) &&
		(Plan.SelectedLength == 60) &&
		(Ranges[0].First == 0) && (Ranges[0].Last == 19) &&
		(Ranges[1].First == 30) && (Ranges[1].Last == 49) &&
		(Ranges[2].First == 80) && (Ranges[2].Last == 99),
		"HTTP static multi-range normalization mismatch");

	Field.Value = XRT_STR_LITERAL("bytes=100-199, -0");
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		&Field,
		1,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_RANGE_NOT_SATISFIABLE) &&
		!Plan.SendBody && (Plan.SelectedLength == 0),
		"HTTP static unsatisfied range plan mismatch");

	Field.Value = XRT_STR_LITERAL("bytes=-1");
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		&Field,
		1,
		&Current,
		0,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK) &&
		(Plan.SelectedLength == 0),
		"HTTP static empty suffix policy mismatch");
}



/* 验证非法、未知、重复和过量 Range 都安全退化为完整响应。 */
static void testHttpStaticIgnoredRanges(void)
{
	xhttpfield Fields[2];
	xhttprepresentation Current = testHttpStaticCurrent();
	xhttpstaticplanconfig Config;
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	Fields[0] = (xhttpfield){
		XRT_STR_INIT("Range"),
		XRT_STR_INIT("bytes=broken")
	};
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		Fields,
		1,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK) &&
		(xrtGetError() == NULL),
		"HTTP static malformed Range was not ignored cleanly");

	Fields[0].Value = XRT_STR_LITERAL("items=0-1");
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		Fields,
		1,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK),
		"HTTP static unknown range unit was not ignored");

	Fields[0].Value = XRT_STR_LITERAL("bytes=0-1");
	Fields[1] = Fields[0];
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK),
		"HTTP static repeated Range fields were not ignored");

	xrtHttpStaticPlanConfigInit(&Config);
	Config.MaxRanges = 2;
	Fields[0].Value = XRT_STR_LITERAL("bytes=0-0, 2-2, 4-4");
	testRequire(xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		Fields,
		1,
		&Current,
		100,
		Ranges,
		16,
		&Config,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK),
		"HTTP static excessive Range was not ignored");
}



/* 验证 If-Range 只有唯一强匹配时才保留范围请求。 */
static void testHttpStaticIfRange(void)
{
	xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Range"),
			XRT_STR_INIT("bytes=10-19")
		},
		{
			XRT_STR_INIT("If-Range"),
			XRT_STR_INIT("\"asset-7\"")
		}
	};
	xhttprepresentation Current = testHttpStaticCurrent();
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		(Plan.RangeCount == 1),
		"HTTP static matching If-Range was ignored");
	Fields[1].Value = XRT_STR_LITERAL("W/\"asset-7\"");
	testRequire(testHttpStaticPlan(
		XRT_STR_LITERAL("GET"),
		Fields,
		2,
		&Current,
		100,
		Ranges,
		&Plan
	) && (Plan.Status == XHTTP_STATUS_OK),
		"HTTP static weak If-Range was accepted");
}



/* 验证输出容量和结构别名在发布任何计划前失败。 */
static void testHttpStaticContracts(void)
{
	union {
		xhttpstaticplan Plan;
		xhttpbyterange Ranges[16];
	} Output;
	xhttprepresentation Current = testHttpStaticCurrent();
	xhttpstaticplanconfig Config;
	xhttpbyterange Ranges[16];
	xhttpstaticplan Plan;

	xrtHttpStaticPlanConfigInit(&Config);
	testRequire(!xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		&Current,
		100,
		Ranges,
		15,
		&Config,
		&Plan
	), "HTTP static plan accepted insufficient range capacity");
	xrtClearError();

	testRequire(!xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		&Current,
		100,
		Output.Ranges,
		16,
		&Config,
		&Output.Plan
	), "HTTP static plan accepted overlapping outputs");
	xrtClearError();
}



/* 执行静态 HTTP 协议计划测试。 */
/* 验证计划配置、请求描述符和输出结构支持未对齐存储并拒绝回绕区间。 */
static void testHttpStaticPlanMemoryContract(void)
{
	uint8 ConfigStorage[sizeof(xhttpstaticplanconfig) + 2u];
	uint8 FieldStorage[sizeof(xhttpfield) + 2u];
	uint8 CurrentStorage[sizeof(xhttprepresentation) + 2u];
	uint8 RangeStorage[sizeof(xhttpbyterange) + 2u];
	uint8 PlanStorage[sizeof(xhttpstaticplan) + 2u];
	xhttpstaticplanconfig Config;
	xhttprepresentation Current;
	xhttpstaticplan Plan;
	xhttpbyterange Range;
	xhttpfield Field = {
		XRT_STR_INIT("Range"),
		XRT_STR_INIT("bytes=2-4")
	};

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpStaticPlanConfigInit(
		(xhttpstaticplanconfig*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	Config.MaxRanges = 1u;
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	testRequire((ConfigStorage[0] == 0xA5u) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5u),
		"HTTP static plan unaligned config initialization failed");

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	memcpy(FieldStorage + 1u, &Field, sizeof(Field));
	memcpy(CurrentStorage + 1u, &Current, sizeof(Current));
	memset(RangeStorage, 0xB6, sizeof(RangeStorage));
	memset(PlanStorage, 0xC7, sizeof(PlanStorage));
	testRequire(xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		(const xhttpfield*)(FieldStorage + 1u),
		1u,
		(const xhttprepresentation*)(CurrentStorage + 1u),
		10u,
		(xhttpbyterange*)(RangeStorage + 1u),
		1u,
		(const xhttpstaticplanconfig*)(ConfigStorage + 1u),
		(xhttpstaticplan*)(PlanStorage + 1u)
	), "HTTP static plan rejected unaligned fixed storage");
	memcpy(&Range, RangeStorage + 1u, sizeof(Range));
	memcpy(&Plan, PlanStorage + 1u, sizeof(Plan));
	testRequire((Plan.Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		Plan.SendBody && (Plan.RangeCount == 1u) &&
		(Plan.SelectedLength == 3u) &&
		(Range.First == 2u) && (Range.Last == 4u) &&
		(RangeStorage[0] == 0xB6u) &&
		(RangeStorage[sizeof(RangeStorage) - 1u] == 0xB6u) &&
		(PlanStorage[0] == 0xC7u) &&
		(PlanStorage[sizeof(PlanStorage) - 1u] == 0xC7u),
		"HTTP static plan unaligned output mismatch");

	xrtClearError();
	xrtHttpStaticPlanConfigInit(
		(xhttpstaticplanconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(xrtGetError() != NULL,
		"HTTP static plan accepted a wrapping config output");
	xrtClearError();
	testRequire(!xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		(const xhttpfield*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		&Current,
		10u,
		&Range,
		1u,
		&Config,
		&Plan
	), "HTTP static plan accepted a wrapping field array");
	xrtClearError();
	testRequire(!xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		(const xhttprepresentation*)(uintptr_t)(UINTPTR_MAX - 1u),
		10u,
		&Range,
		1u,
		&Config,
		&Plan
	), "HTTP static plan accepted a wrapping representation");
	xrtClearError();
	testRequire(!xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		&Current,
		10u,
		(xhttpbyterange*)(uintptr_t)(UINTPTR_MAX - 1u),
		1u,
		&Config,
		&Plan
	), "HTTP static plan accepted a wrapping range output");
	xrtClearError();
	testRequire(!xrtHttpStaticPlanBuild(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		&Current,
		10u,
		&Range,
		1u,
		&Config,
		(xhttpstaticplan*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "HTTP static plan accepted a wrapping plan output");
	xrtClearError();
}



int main(void)
{
	testHttpStaticMethods();
	testHttpStaticConditions();
	testHttpStaticRanges();
	testHttpStaticIgnoredRanges();
	testHttpStaticIfRange();
	testHttpStaticContracts();
	testHttpStaticPlanMemoryContract();
	printf("[PASS] http_static_plan\n");
	return 0;
}
