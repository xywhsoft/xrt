/*
 * 范例：string/builder —— 字符串构建器：追加链与所有权移交
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrBufInit          初始化栈上构建器句柄
 *   xrtStrBufAppend        追加一段视图
 *   xrtStrBufAppendRepeat  重复追加 n 次（分隔线/缩进生成）
 *   xrtStrBufTake          移交缓冲为拥有式结果（构建器归零可复用）
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/string/builder/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   items=ababab
 *
 * 构建器 vs 反复 Concat：Concat 每次产生新的中间串
 *   （O(n²) 拷贝）；构建器内部按倍增扩容，追加链 O(n) 总拷贝。
 *   拼三段以上一律用构建器。
 * Take 的移交语义：缓冲区所有权直接转交给调用方，
 *   构建器变回"空"状态——不复制、不共享。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xstrbuf tBuffer;
	str sResult;

	xrtStrBufInit(&tBuffer);

	/* 前缀 + 模式重复三次："ab" × 3 = ababab。 */
	if ( !xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("items=")) ||
		 !xrtStrBufAppendRepeat(&tBuffer, XRT_STR_LITERAL("ab"), 3) ) {
		xrtStrBufFree(&tBuffer);
		return 1;
	}

	/* 移交：产物由 xrtFree 释放，构建器可继续复用。 */
	sResult = xrtStrBufTake(&tBuffer);
	if ( sResult == NULL ) {
		return 2;
	}
	printf("%s\n", sResult);
	xrtFree(sResult);
	return 0;
}
