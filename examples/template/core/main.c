#include <stdio.h>

#include <xrt.h>



/* 演示一次编译、动态值路径和直接字符串渲染。 */
int main(void)
{
	xvalue* pData = xrtValueObject();
	xtemplate* pTemplate;
	str sOutput;
	xtime Time;

	/* 核心层直接承载字符串、数字和时间三种格式化输出。 */
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
	pTemplate = xrtTemplateCompile(
		XRT_STR_LITERAL(
			"{$name} handled {%requests:,d} requests on {&created:%F}"
		)
	);
	if ( pTemplate == NULL ) {
		xrtValueRelease(pData);
		return 2;
	}
	sOutput = xrtTemplateRender(pTemplate, pData, NULL);
	if ( sOutput == NULL ) {
		xrtTemplateRelease(pTemplate);
		xrtValueRelease(pData);
		return 3;
	}
	puts(sOutput);
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pData);
	return 0;
}
