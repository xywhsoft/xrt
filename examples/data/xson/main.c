/*
 * 范例：data/xson —— XSON 扩展格式：bytes/time/set/intmap 全类型往返
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtXsonParse / xrtXsonStringify   DOM 解析与美化序列化
 *   xrtXsonVisit / xxsonevent         SAX 事件流（区分扩展类型）
 *   xrtXsonWriterCreate/Set/End/Finish/Take  增量构建（含 set 容器）
 * 模块宏：XRT_MODULE_XSON（依赖 VALUE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/data/xson/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   {
 *     "blob": bytes("AAEC/w=="),
 *     "updated": time("2026-07-31T00:00:00Z"),
 *     "roles": set[ ... ],
 *     "ports": intmap{ 80: "http", 443: "https" }
 *   }
 *   bytes = 4
 *   time = 1785456000000000
 *   {"code":200,"tags":set["xrt"]}
 *
 * XSON 是 JSON 的严格超集，多出四种一等类型：
 *   bytes("...")   二进制（内联 Base64，免外部引用）
 *   time("...")    时间戳（解析时归一为 UTC；事件里给微秒整数）
 *   set[...]       去重集合
 *   intmap{...}    整数键映射
 * JSON 文档天然是合法 XSON——解析器同一套，配置互不干扰。
 * 事件回调按 Type 字段区分类型，扩展类型不再退化为字符串。
 */

#include <inttypes.h>
#include <stdio.h>

#include <xrt.h>



/*
 * SAX 回调：只关心扩展类型——
 *   BYTES 事件带字节视图（本例打印长度 4）；
 *   TIME   事件带 Unix 微秒整数（时区在解析时已归一）。
 */
static xxsonvisitaction printXsonEvent(
	const xxsonevent* pEvent,
	ptr pUserData
)
{
	(void)pUserData;
	if ( pEvent->Type == XXSON_EVENT_BYTES ) {
		printf("bytes = %u\n", (unsigned)pEvent->Value.Bytes.Size);
	} else if ( pEvent->Type == XXSON_EVENT_TIME ) {
		printf("time = %" PRId64 "\n", (int64)pEvent->Value.Time);
	}
	return XXSON_VISIT_NEXT;
}



int main(void)
{
	/* 四种扩展类型各出现一次的输入文档。 */
	static const char sInput[] =
		"{\"blob\":bytes(\"AAEC/w==\"),"
		"\"updated\":time(\"2026-07-31T08:00:00+08:00\"),"
		"\"roles\":set[\"reader\",\"writer\"],"
		"\"ports\":intmap{80:\"http\",443:\"https\"}}";
	xxsonreadconfig ReadConfig;
	xxsonwriteconfig WriteConfig;
	xxsonwriter* pWriter;
	xvalue* pRoot;
	str sText;
	size_t iSize;

	/* ---- 1) DOM 往返：解析 → 美化输出（time 已归一为 UTC Z 后缀）---- */
	pRoot = xrtXsonParse((xstrview){ sInput, sizeof(sInput) - 1u });
	if ( pRoot == NULL ) {
		return 1;
	}
	sText = xrtXsonStringify(pRoot, true, &iSize);
	xrtValueRelease(pRoot);
	if ( sText == NULL ) {
		return 2;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);

	/* ---- 2) SAX：同一输入走事件流，只处理扩展类型 ---- */
	xrtXsonReadConfigInit(&ReadConfig);
	if (
		xrtXsonVisit(
			(xstrview){ sInput, sizeof(sInput) - 1u },
			&ReadConfig,
			printXsonEvent,
			NULL
		) != XXSON_VISIT_DONE
	) {
		return 3;
	}

	/*
	 * ---- 3) Writer 直接构建（含嵌套 set 容器）----
	 * 结构：{"code":200, "tags":set["xrt"]}
	 * 注意 End 要写两次：先闭 set，再闭外层对象——
	 * 容器开-闭严格配对，Finish 校验整体完整性。
	 */
	xrtXsonWriteConfigInit(&WriteConfig);
	pWriter = xrtXsonWriterCreate(&WriteConfig);
	if (
		(pWriter == NULL) ||
		!xrtXsonWriterObject(pWriter) ||
		!xrtXsonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
		!xrtXsonWriterInt(pWriter, 200) ||
		!xrtXsonWriterName(pWriter, XRT_STR_LITERAL("tags")) ||
		!xrtXsonWriterSet(pWriter) ||                     /* 开 set */
		!xrtXsonWriterString(pWriter, XRT_STR_LITERAL("xrt")) ||
		!xrtXsonWriterEnd(pWriter) ||                     /* 闭 set */
		!xrtXsonWriterEnd(pWriter) ||                     /* 闭对象 */
		!xrtXsonWriterFinish(pWriter)
	) {
		xrtXsonWriterFree(pWriter);
		return 4;
	}
	sText = xrtXsonWriterTake(pWriter, &iSize);
	xrtXsonWriterFree(pWriter);
	if ( sText == NULL ) {
		return 5;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);
	return 0;
}
