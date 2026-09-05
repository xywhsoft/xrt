/*
 * 范例：string/builder_tour —— 构建器全接口巡礼（补充 builder/format 未覆盖项）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrBufAppendByte        追加单字节
 *   xrtStrBufAppendFormatV     va_list 版格式化追加
 *   xrtStrBufReserve           预留容量（避免后续扩容）
 *   xrtStrBufResize            直接设定长度（增长部分未定义内容）
 *   xrtStrBufClear             清空（保留容量）
 *   xrtStrBufValid             句柄健康检查
 *   xrtStrBufView              取当前内容视图（零拷贝）
 *   xrtStrBufAlias             判断视图是否别名构建器内部缓冲
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/builder_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   valid=1 size=19 alias=1
 *   content=prefix: FF=255 42=4
 *
 * Alias 的用途：拿到构建器视图后，后续判断该视图是否仍
 *   指向构建器内部（决定能否安全继续 Append 或必须先复制）。
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 包装示范：命名参数直接 Append，仅数值走 va_list——格式串只消费可变部分。 */
static bool appendTag(xstrbuf* pBuffer, cstr sTag, ...)
{
	va_list Args;

	if ( !xrtStrBufAppendByte(pBuffer, ' ') ||
		!xrtStrBufAppend(pBuffer, (xstrview){ sTag, strlen(sTag) }) ) {
		return false;
	}
	va_start(Args, sTag);
	bool bOk = xrtStrBufAppendFormatV(pBuffer, "=%d", Args);
	va_end(Args);
	return bOk;
}

int main(void)
{
	xstrbuf Buffer;

	xrtStrBufInit(&Buffer);

	/* 预留容量：预知总量时一次到位，追加链零扩容。 */
	if ( !xrtStrBufReserve(&Buffer, 64u) || !xrtStrBufValid(&Buffer) ) {
		xrtStrBufFree(&Buffer);
		return 1;
	}

	/* 字节级追加 + 格式化追加（经 va_list 版）。 */
	(void)xrtStrBufAppend(&Buffer, SV("prefix"));
	(void)xrtStrBufAppendByte(&Buffer, ':');
	if ( !appendTag(&Buffer, "FF", 255) || !appendTag(&Buffer, "42", 42) ) {
		xrtStrBufFree(&Buffer);
		return 2;
	}

	/* Resize：直接改长度（这里截断 1 字节演示）；Clear 后可复用。 */
	(void)xrtStrBufResize(&Buffer, xrtStrBufView(&Buffer).Size - 1u);

	xstrview View = xrtStrBufView(&Buffer);
	bool bAlias = false;
	size_t iAliasOffset = 0;

	/* Alias：View 确实借用构建器内部缓冲 → true + 偏移 0。 */
	(void)xrtStrBufAlias(&Buffer, View, &bAlias, &iAliasOffset);
	printf("valid=%d size=%zu alias=%d\n",
		xrtStrBufValid(&Buffer) ? 1 : 0, View.Size, bAlias ? 1 : 0);
	printf("content=%.*s\n", (int)View.Size, View.Data);

	xrtStrBufClear(&Buffer);
	(void)xrtStrBufAppend(&Buffer, SV("reuse"));
	printf("cleared-size=%zu\n", xrtStrBufView(&Buffer).Size);
	xrtStrBufFree(&Buffer);
	return 0;
}
