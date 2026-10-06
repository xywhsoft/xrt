/*
 * 范例：data/xlon —— XLON 扩展格式：bytes/time/set/intmap 全类型往返
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtXlonParse / xrtXlonStringify   DOM 解析与美化序列化
 *   xrtXlonVisit / xxlonevent         SAX 事件流（区分扩展类型）
 *   xrtXlonWriterCreate/Set/End/Finish/Take  增量构建（含 set 容器）
 * 模块宏：XRT_MODULE_XLON（依赖 VALUE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/data/xlon/main.c -lws2_32 -liphlpapi
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
 * XLON 是 JSON 的严格超集，多出四种一等类型：
 *   bytes("...")   二进制（内联 Base64，免外部引用）
 *   time("...")    时间戳（解析时归一为 UTC；事件里给毫秒整数）
 *   set[...]       去重集合
 *   intmap{...}    整数键映射
 * JSON 文档天然是合法 XLON——解析器同一套，配置互不干扰。
 * 事件回调按 Type 字段区分类型，扩展类型不再退化为字符串。
 */

#include <inttypes.h>
#include <stdio.h>

#include <xrt.h>



/*
 * SAX 回调：只关心扩展类型——
 *   BYTES 事件带字节视图（本例打印长度 4）；
 *   TIME   事件带 公元 UTC 毫秒整数（时区在解析时已归一）。
 */
static xxlonvisitaction printXlonEvent(
	const xxlonevent* pEvent,
	ptr pUserData
)
{
	(void)pUserData;
	if ( pEvent->Type == XXLON_EVENT_BYTES ) {
		printf("bytes = %u\n", (unsigned)pEvent->Value.Bytes.Size);
	} else if ( pEvent->Type == XXLON_EVENT_TIME ) {
		printf("time = %" PRId64 "\n", (int64)pEvent->Value.Time);
	}
	return XXLON_VISIT_NEXT;
}



int main(void)
{
	/* 四种扩展类型各出现一次的输入文档。 */
	static const char sInput[] =
		"{\"blob\":bytes(\"AAEC/w==\"),"
		"\"updated\":time(\"2026-07-31T08:00:00+08:00\"),"
		"\"roles\":set[\"reader\",\"writer\"],"
		"\"ports\":intmap{80:\"http\",443:\"https\"}}";
	xxlonreadconfig ReadConfig;
	xxlonwriteconfig WriteConfig;
	xxlonwriter* pWriter;
	xvalue* pRoot;
	str sText;
	size_t iSize;

	/* ---- 1) DOM 往返：解析 → 美化输出（time 已归一为 UTC Z 后缀）---- */
	pRoot = xrtXlonParse((xstrview){ sInput, sizeof(sInput) - 1u });
	if ( pRoot == NULL ) {
		return 1;
	}
	sText = xrtXlonStringify(pRoot, true, &iSize);
	xrtValueRelease(pRoot);
	if ( sText == NULL ) {
		return 2;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);

	/* ---- 2) SAX：同一输入走事件流，只处理扩展类型 ---- */
	xrtXlonReadConfigInit(&ReadConfig);
	if (
		xrtXlonVisit(
			(xstrview){ sInput, sizeof(sInput) - 1u },
			&ReadConfig,
			printXlonEvent,
			NULL
		) != XXLON_VISIT_DONE
	) {
		return 3;
	}

	/*
	 * ---- 3) Writer 直接构建（含嵌套 set 容器）----
	 * 结构：{"code":200, "tags":set["xrt"]}
	 * 注意 End 要写两次：先闭 set，再闭外层对象——
	 * 容器开-闭严格配对，Finish 校验整体完整性。
	 */
	xrtXlonWriteConfigInit(&WriteConfig);
	pWriter = xrtXlonWriterCreate(&WriteConfig);
	if (
		(pWriter == NULL) ||
		!xrtXlonWriterObject(pWriter) ||
		!xrtXlonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
		!xrtXlonWriterInt(pWriter, 200) ||
		!xrtXlonWriterName(pWriter, XRT_STR_LITERAL("tags")) ||
		!xrtXlonWriterSet(pWriter) ||                     /* 开 set */
		!xrtXlonWriterString(pWriter, XRT_STR_LITERAL("xrt")) ||
		!xrtXlonWriterEnd(pWriter) ||                     /* 闭 set */
		!xrtXlonWriterEnd(pWriter) ||                     /* 闭对象 */
		!xrtXlonWriterFinish(pWriter)
	) {
		xrtXlonWriterFree(pWriter);
		return 4;
	}
	sText = xrtXlonWriterTake(pWriter, &iSize);
	xrtXlonWriterFree(pWriter);
	if ( sText == NULL ) {
		return 5;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);
	return 0;
}
