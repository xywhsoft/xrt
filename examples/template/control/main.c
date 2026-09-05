/*
 * 范例：template/control —— 流程控制：条件/循环/元数据/continue/break
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTemplateCompile / Render / Release   编译-渲染-释放主线
 *   数据侧：xrtValueArray/AppendNew + ObjectSetTake（移交所有权）
 * 模块宏：XRT_MODULE_TEMPLATE（依赖 VALUE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/template/control/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Users: Alice,Bob; range: 13
 *
 * 模板语法速查（本例全覆盖）：
 *   {#if:path}...{#else}...{#end}      条件（对象非空/数组非空为真）
 *   {#foreach:users}...{#end}          容器循环
 *   {?loop.first::, }                  行内条件：首元素前缀 ", "
 *   {$loop.value}                      循环元数据（value/index/first/last）
 *   {#for:1:5}...{#end}                整数范围循环（含端点）
 *   {#if:loop.value = 2}{#continue}    命中 2 跳过本次
 *   {#if:loop.value = 4}{#break}       命中 4 提前终止
 * range 输出 "13" 的原因：1..5 中 2 被 continue 跳过、
 *   4 触发 break（4、5 都不输出），剩下 1 和 3 直接拼接。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xvalue* pRoot = xrtValueObject();
	xvalue* pUsers = xrtValueArray();
	xtemplate* pTemplate;
	str sOutput;

	/*
	 * 数据：{users: ["Alice","Bob"]}。
	 * ObjectSetTake 移交数组所有权（pUsers 被清空防双重释放），
	 * 与 SetNew 的区别：New 接管"新建值"，Take 接管"已有变量"。
	 */
	if ( (pRoot == NULL) || (pUsers == NULL) ||
		 !xrtValueArrayAppendNew(
			pUsers,
			xrtValueString(XRT_STR_LITERAL("Alice"))
		) || !xrtValueArrayAppendNew(
			pUsers,
			xrtValueString(XRT_STR_LITERAL("Bob"))
		) || !xrtValueObjectSetTake(
			pRoot,
			XRT_STR_LITERAL("users"),
			&pUsers
		) ) {
		xrtValueRelease(pUsers);
		xrtValueRelease(pRoot);
		return 1;
	}

	/* 一条模板串演示全部控制语句（见文件头语法速查）。 */
	pTemplate = xrtTemplateCompile(XRT_STR_LITERAL(
		"{#if:users}Users: {#foreach:users}"
		"{?loop.first::, }{$loop.value}{#end}{#else}No users{#end}; "
		"range: {#for:1:5}"
		"{#if:loop.value = 2}{#continue}{#end}"
		"{#if:loop.value = 4}{#break}{#end}"
		"{%loop.value}{#end}"
	));
	if ( pTemplate == NULL ) {
		xrtValueRelease(pRoot);
		return 2;
	}
	sOutput = xrtTemplateRender(pTemplate, pRoot, NULL);
	if ( sOutput == NULL ) {
		xrtTemplateRelease(pTemplate);
		xrtValueRelease(pRoot);
		return 3;
	}
	puts(sOutput);
	xrtFree(sOutput);
	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pRoot);
	return 0;
}
