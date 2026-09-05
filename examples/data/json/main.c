/*
 * 范例：data/json —— JSON 三种用法：DOM、SAX 事件流、增量 Writer
 * ----------------------------------------------------------------
 * 演示 API：
 *   【DOM 路径】
 *   xrtJsonParse         解析为 xvalue 树（拥有式根，Release 释放）
 *   xrtValueObjectGet    按名取对象成员（借用，不转移所有权）
 *   xrtValueGetString    精确读取字符串成员为视图
 *   xrtJsonStringify     序列化（bPretty=true 缩进美化）
 *   【SAX 路径】
 *   xrtJsonVisit         事件回调式遍历，零中间树
 *   xjsonevent           事件结构（HasName/Name/类型/值）
 *   【Writer 路径】
 *   xrtJsonWriterCreate/ Object/Name/Int/String/End/Finish/Free/Take
 *                        增量构建：容器开-闭配对，Finish 校验完整
 * 模块宏：XRT_MODULE_JSON（依赖 VALUE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/data/json/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   name = xrt
 *   {
 *     "name": "xrt",
 *     "features": [
 *       "json",
 *       "http"
 *     ]
 *   }
 *   member: code
 *   member: ok
 *   {"code":200,"message":"OK"}
 *
 * 三种用法怎么选：
 *   DOM  —— 需要随机访问/修改字段（配置、小文档）；
 *   SAX  —— 大文档/网络流只扫一遍（统计、提取、校验），零树分配；
 *   Writer—— 从程序状态直接产出 JSON，不必先构造 Value 树。
 * Writer 的原子性：任一步失败或 Finish 校验不完整都返回 false，
 *   不会产出半个合法文档。
 */

#include <stdio.h>

#include <xrt.h>



/*
 * SAX 回调：每个 JSON 值触发一次事件。
 * 事件按出现顺序到达；对象成员带名字（HasName + Name）。
 * 返回 NEXT 继续遍历（也可返回 STOP 提前终止，ABORT 报错）。
 */
static xjsonvisitaction printJsonEvent(
	const xjsonevent* pEvent,
	ptr pUserData
)
{
	(void)pUserData;
	if ( pEvent->HasName ) {
		printf("member: %.*s\n", (int)pEvent->Name.Size, pEvent->Name.Data);
	}
	return XJSON_VISIT_NEXT;
}



int main(void)
{
	xjsonreadconfig ReadConfig;
	xjsonwriteconfig WriteConfig;
	xjsonwriter* pWriter;
	xvalue* pRoot;
	xvalue* pName;
	xstrview Name;
	str sText;
	size_t iSize;

	/* ---- 1) DOM：解析 → 随机访问 → 美化序列化 ---- */
	pRoot = xrtJsonParse(XRT_STR_LITERAL(
		"{\"name\":\"xrt\",\"features\":[\"json\",\"http\"]}"
	));
	if ( pRoot == NULL ) {
		return 1;
	}

	/* ObjectGet 返回借用指针（树拥有它）；GetString 取出视图。 */
	pName = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name"));
	if ( !xrtValueGetString(pName, &Name) ) {
		xrtValueRelease(pRoot);
		return 2;
	}
	printf("name = %.*s\n", (int)Name.Size, Name.Data);

	/* Stringify 产物是拥有式字符串（xrtFree）；之后树即可释放。 */
	sText = xrtJsonStringify(pRoot, true, &iSize);
	xrtValueRelease(pRoot);
	if ( sText == NULL ) {
		return 3;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);

	/* ---- 2) SAX：同一份输入按事件流走一遍，不建树 ---- */
	xrtJsonReadConfigInit(&ReadConfig);
	if (
		xrtJsonVisit(
			XRT_STR_LITERAL("{\"code\":200,\"ok\":true}"),
			&ReadConfig,
			printJsonEvent,
			NULL
		) != XJSON_VISIT_DONE
	) {
		return 4;
	}

	/* ---- 3) Writer：增量构建 {"code":200,"message":"OK"} ---- */
	xrtJsonWriteConfigInit(&WriteConfig);
	pWriter = xrtJsonWriterCreate(&WriteConfig);
	if (
		(pWriter == NULL) ||
		!xrtJsonWriterObject(pWriter) ||                     /* 开对象 */
		!xrtJsonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
		!xrtJsonWriterInt(pWriter, 200) ||
		!xrtJsonWriterName(pWriter, XRT_STR_LITERAL("message")) ||
		!xrtJsonWriterString(pWriter, XRT_STR_LITERAL("OK")) ||
		!xrtJsonWriterEnd(pWriter) ||                       /* 闭对象 */
		!xrtJsonWriterFinish(pWriter)                       /* 完整性校验 */
	) {
		xrtJsonWriterFree(pWriter);
		return 5;
	}

	/* Take 移交文本（xrtFree 释放）；Take 后 writer 仍需 Free。 */
	sText = xrtJsonWriterTake(pWriter, &iSize);
	xrtJsonWriterFree(pWriter);
	if ( sText == NULL ) {
		return 6;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);
	return 0;
}
