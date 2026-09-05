/*
 * 范例：text/pattern —— 结构化模式匹配：路由风格的 {name} 捕获
 * ----------------------------------------------------------------
 * 演示 API：
 *   xpatternspec / XRT_STR_INIT   模式规格：模板 + 业务标识（kind）
 *   xrtPatternBuilderCreate/Add/Compile/Free
 *                                 构建器：增量注册多条模式后一次编译
 *   xrtPatternMatch               匹配并写出捕获数组 + 命中信息
 *   xrtPatternRelease             释放不可变匹配对象
 * 模块宏：XRT_MODULE_PATTERN
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/text/pattern/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   kind=user group=admin id=42
 *
 * 与正则（regex）的分工：pattern 是"路由/URL/配置键"专用的
 *   受限匹配——{name} 捕获段 + 字面段，没有元语言；
 *   因此它更快、天然无灾难回溯（非回溯实现）、
 *   且命中的 Match.Value 直接给业务标识（"user"），
 *   不需要再解析捕获组语义——Web 路由表的标准选择。
 * 捕获按模板中出现顺序写入数组（group 在前、id 在后）。
 */

#define XRT_MODULE_PATTERN
#include <xrt.h>

#include <stdio.h>



int main(void)
{
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();

	/* 规格四元组：模板 / 业务标识 / 预算标志（0=默认）。 */
	xpatternspec Spec = {
		XRT_STR_INIT("/users/{group}/item-{id}.json"),
		(ptr)"user",
		0,
		0
	};
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview arrCapture[2];

	/* 注册一条模式（真实路由表会注册几十条再一次编译）。 */
	if ( (pBuilder == NULL) ||
		 (xrtPatternBuilderAdd(pBuilder, &Spec) == XPATTERN_ID_INVALID) ) {
		xrtPatternBuilderFree(pBuilder);
		return 1;
	}

	/* 编译产物不可变、可长期持有；构建器即可释放。 */
	pPattern = xrtPatternBuilderCompile(pBuilder);
	xrtPatternBuilderFree(pBuilder);
	if ( pPattern == NULL ) {
		return 1;
	}

	/*
	 * 匹配：捕获按声明顺序写入调用方数组；
	 * Match.Value 是命中模式的业务标识（本例 "user"），
	 * Match.Index 给模式编号——多模式分发的两个键。
	 */
	if ( xrtPatternMatch(
		pPattern,
		XRT_STR_LITERAL("/users/admin/item-42.json"),
		arrCapture,
		2u,
		&Match
	) == XPATTERN_MATCH ) {
		printf(
			"kind=%s group=%.*s id=%.*s\n",
			(cstr)Match.Value,
			(int)arrCapture[0].Size,
			arrCapture[0].Data,
			(int)arrCapture[1].Size,
			arrCapture[1].Data
		);
	}
	xrtPatternRelease(pPattern);
	return 0;
}
