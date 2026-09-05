/*
 * 范例：string/format —— 构建器格式化追加：前缀 + printf 一次成型
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrBufAppendFormat  printf 风格格式化直接追加进构建器
 * 模块宏：XRT_MODULE_STRING（FORMAT 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/string/format/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   result: 000000FF / 3.50
 *
 * 与 sprintf 的区别：不需要预先猜测缓冲大小
 *   （构建器自动扩容）；与 xrtFormat 的区别：
 *   Format 生成独立字符串，AppendFormat 在已有内容尾部续写——
 *   "前缀 + 数据 + 后缀"三段式组装零中间串。
 * 格式走 xrtFormatV（xrt 自研渲染，非标准 vsnprintf）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xstrbuf tBuffer;
	str sResult;

	xrtStrBufInit(&tBuffer);

	/* 文本前缀 + 格式化数据一次追加到位。 */
	if ( !xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("result: ")) ||
		 !xrtStrBufAppendFormat(&tBuffer, "%08X / %.2f", 255u, 3.5) ) {
		xrtStrBufFree(&tBuffer);
		return 1;
	}

	sResult = xrtStrBufTake(&tBuffer);
	if ( sResult == NULL ) {
		return 2;
	}
	printf("%s\n", sResult);
	xrtFree(sResult);
	return 0;
}
