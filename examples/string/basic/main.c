/*
 * 范例：string/basic —— 视图管线：Trim → Cut → Filter → Concat
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrTrim      首尾空白裁剪（零分配：只调两个指针）
 *   xrtStrCut       按首处分隔符切成两段（前后视图都可选）
 *   xrtStrFilterTo  删除集合内字节，其余写入调用方缓冲
 *   xrtStrConcat    两视图拼接为拥有式结果
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/string/basic/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   alpha.txt
 *
 * 视图管线的意义：前几步全部零分配——Trim/Cut 只搬运
 *   (指针, 长度) 对，直到确实需要"新内存"（Concat）才分配。
 *   对热路径文本处理，这比每步 strdup 一份快一个数量级。
 * 本例数据流："  alpha/beta  " → 裁剪 → 切出 "alpha"
 *   → 过滤（无字符被删，直通）→ 拼 ".txt"。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* Trim 返回原字符串的子视图：不改字节、不分配。 */
	xstrview Text = xrtStrTrim(XRT_STR_LITERAL("  alpha/beta  "));
	xstrview Name;
	char arrName[32];
	size_t iNameSize;
	str sResult;

	/*
	 * Cut 按首个 "/" 切分：Name 收前段 "alpha"，
	 * 后段不需要 → 第二个出参传 NULL。
	 * FilterTo 剔除 "_-" 集合内的字节，其余写入本地缓冲——
	 * "alpha" 不含这两者，原样通过（语义：删除集合字符）。
	 */
	if ( !xrtStrCut(Text, XRT_STR_LITERAL("/"), &Name, NULL) ||
		 !xrtStrFilterTo(Name, XRT_STR_LITERAL("_-"), arrName,
			sizeof(arrName), &iNameSize) ) {
		return 1;
	}

	/* 第一次分配发生在拼接：产物拥有式，xrtFree 释放。 */
	sResult = xrtStrConcat((xstrview){ arrName, iNameSize },
		XRT_STR_LITERAL(".txt"));
	if ( sResult == NULL ) {
		return 2;
	}
	printf("%s\n", sResult);
	xrtFree(sResult);
	return 0;
}
