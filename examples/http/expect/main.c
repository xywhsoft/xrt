#include <xrt.h>

#include <stdio.h>



/*
 * 范例：http/expect —— Expect 字段：期望枚举与 100-continue 判定
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpExpectFieldCursorInit / FieldNext   跨字段迭代期望
 *   xrtHttpExpectFields       一票判定：是否要求 100-continue
 *   XHTTP_EXPECT_CONTINUE 等   标准期望枚举（扩展期望有名无类）
 * 模块宏：XRT_MODULE_HTTP_EXPECT
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/expect/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   expectation: 100-continue
 *   expectation: feature
 *   supported: no
 *
 * supported=no 的含义：字段里混有非标准期望（feature=on）——
 *   服务器不认识就必须拒绝整个请求（417），不能静默忽略；
 *   只有"全部是可处理的标准期望"时 ExpectFields 才返回 CONTINUE。
 */


/* 迭代标准和扩展 expectation，并执行服务器支持分类。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") },
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("feature=on") }
	};
	xhttpexpectfieldcursor Cursor;
	xhttpexpectation Expectation;

	xrtHttpExpectFieldCursorInit(&Cursor);
	while ( xrtHttpExpectFieldNext(
		Fields, 2u, &Cursor, &Expectation
	) == XHTTP_NEXT_ITEM ) {
		printf(
			"expectation: %.*s\n",
			(int)Expectation.Name.Size,
			Expectation.Name.Data
		);
	}
	printf(
		"supported: %s\n",
		xrtHttpExpectFields(Fields, 2u) ==
			XHTTP_EXPECT_CONTINUE ? "yes" : "no"
	);
	return 0;
}
