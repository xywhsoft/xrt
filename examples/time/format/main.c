/*
 * 范例：time/format —— strftime 风格格式化：缓冲版与分配版
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDateTime   由日历字段构造 xtime（含微秒）
 *   xrtTimeWrite  格式化到调用方缓冲（零分配；成功返回长度）
 *   xrtTimeFormat 格式化为拥有式字符串（xrtFree 释放）
 *   XRT_NPOS      Write 失败（容量不足/格式错）的返回哨兵
 * 模块宏：XRT_MODULE_TIME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/time/format/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   buffer: 2024-03-01 07:58:57.654321 +08:00
 *   allocated: Thursday, February 29, 2024
 *
 * 读数玄机：输入是 2024-02-29 23:58:57（UTC），按 +8 偏移展示
 *   跨过午夜与闰日 → 2024-03-01 07:58:57——格式化的是
 *   "同一时刻的另一种表达"，不是同一组字段。
 * 格式符：strftime 兼容（%F 日期 %T 时刻 %f 微秒 %:z 带冒号偏移、
 *   %A/%B 英文星期/月份），另支持本地化文本注入。
 */

#include <xrt.h>

#include <stdio.h>



int main(void)
{
	char arrText[64];
	xtime iTime;
	str sAllocated;

	/* 闰日 23:58:57.654321 —— 刻意选跨日场景（见文件头）。 */
	if ( !xrtDateTime(2024, 2, 29, 23, 58, 57, 654321, &iTime) ) {
		return 1;
	}

	/* 缓冲版：热路径零分配；NPOS 表示失败（容量或格式非法）。 */
	if ( xrtTimeWrite(arrText, sizeof(arrText), iTime, 8 * 3600,
		XRT_STR_LITERAL("%F %T.%f %:z")) == XRT_NPOS ) {
		return 1;
	}
	printf("buffer: %s\n", arrText);

	/* 分配版：偏移 0 = UTC；%A/%B 输出英文星期与月份全名。 */
	sAllocated = xrtTimeFormat(iTime, 0,
		XRT_STR_LITERAL("%A, %B %d, %Y"));
	if ( sAllocated == NULL ) {
		return 1;
	}
	printf("allocated: %s\n", sAllocated);
	xrtFree(sAllocated);
	return 0;
}
