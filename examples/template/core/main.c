/*
 * 范例：template/core —— 模板核心：编译一次、动态路径、类型化格式化
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTemplateCompile  编译模板为不可变对象（引用计数，可复用）
 *   xrtTemplateRender   数据对象 + 模板 → 拥有式输出字符串
 *   xrtTemplateRelease  释放编译对象
 *   数据侧：xrtValueObject/SetNew + String/Int/Time 构造值树
 * 模块宏：XRT_MODULE_TEMPLATE（依赖 VALUE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/template/core/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   xrt handled 42 requests on 2026-04-02
 *
 * 模板语法三种插值前缀：
 *   {$path}   直接输出（字符串原样）；
 *   {%path:fmt} 数字格式化（":,d" 千分位整数，语法同 number 模块）；
 *   {&path:fmt} 时间格式化（":%F" ISO 日期）。
 * 编译/渲染分离的意义：模板只解析一次，
 *   高频渲染只走 Render——配置模板、报表头的标准用法。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xvalue* pData = xrtValueObject();   /* 渲染数据：一个对象 */
	xtemplate* pTemplate;
	str sOutput;
	xtime Time;

	/*
	 * 组装数据：name(字符串)、requests(整数)、created(时间)。
	 * xrtDateTime 按年月日时分秒+纳秒构造 xtime；
	 * ObjectSetNew 接管右侧新值的引用（函数名里的 New 即此意）。
	 */
	if ( (pData == NULL) || !xrtDateTime(
		2026,
		4,
		2,
		12,
		30,
		0,
		0,
		&Time
	) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("name"),
		xrtValueString(XRT_STR_LITERAL("xrt"))
	) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("requests"),
		xrtValueInt(42)
	) || !xrtValueObjectSetNew(
		pData,
		XRT_STR_LITERAL("created"),
		xrtValueTime(Time)
	) ) {
		xrtValueRelease(pData);
		return 1;
	}

	/* 编译：一次解析成不可变对象；模板语法错在此步报错。 */
	pTemplate = xrtTemplateCompile(
		XRT_STR_LITERAL(
			"{$name} handled {%requests:,d} requests on {&created:%F}"
		)
	);
	if ( pTemplate == NULL ) {
		xrtValueRelease(pData);
		return 2;
	}

	/* 渲染：NULL 配置 = 默认预算（防失控输出的上限仍生效）。 */
	sOutput = xrtTemplateRender(pTemplate, pData, NULL);
	if ( sOutput == NULL ) {
		xrtTemplateRelease(pTemplate);
		xrtValueRelease(pData);
		return 3;
	}
	puts(sOutput);

	/* 三个拥有式资源逆序释放。 */
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pData);
	return 0;
}
