/*
 * 范例：number/variants —— 数值补遗：UInt 族与 To/Write 缓冲变体
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtUIntString / xrtUIntParse     无符号字符串往返
 *   xrtIntFormatTo / xrtUIntFormatTo  格式化到调用方缓冲
 *   xrtNumFormatTo / xrtNumWrite      浮点格式化：缓冲 / 长度查询
 *   xrtIntWrite                       整数写出（查询 + 写入两用）
 * 模块宏：XRT_MODULE_NUMBER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/number/variants/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   uint-roundtrip=4294967296
 *   format-to=0XFF -1,234,567
 *   num-write=3.50 num-w=2.5
 *   int-write-need=3 int-write=-42 uint-w=12c
 *
 */

#include <stdio.h>
#include <xrt.h>

int main(void)
{
	char Buffer[32];
	size_t iNeed = 0;
	uint64 iValue = 0;

	/* UInt 往返：String 出拥有式文本，Parse 精确读回。 */
	str sText = xrtUIntString(UINT64_C(4294967296), 10u, 0u);
	if ( sText == NULL || !xrtUIntParse(XRT_STR_LITERAL("4294967296"),
		10u, 0u, &iValue) ) {
		xrtFree(sText);
		return 1;
	}
	printf("uint-roundtrip=%llu\n", (unsigned long long)iValue);
	xrtFree(sText);

	/* 格式化缓冲版：十六进制带前缀大写 + 十进制。 */
	if ( xrtUIntFormatTo(UINT64_C(255), XRT_STR_LITERAL("#X"),
		Buffer, sizeof(Buffer), &iNeed) ) {
		printf("format-to=%s ", Buffer);
	}
	if ( xrtIntFormatTo(-1234567, XRT_STR_LITERAL(",d"),
		Buffer, sizeof(Buffer), &iNeed) ) {
		printf("%s\n", Buffer);
	}

	/* 浮点缓冲版（NumWrite 补零）+ 查询模式演示。 */
	if ( xrtNumFormatTo(3.5, XRT_STR_LITERAL(".2f"),
		Buffer, sizeof(Buffer), &iNeed) ) {
		printf("num-write=%s\n", Buffer);
	}
	/* NumWrite/UIntWrite：直接补零写出的底层入口（容量含零）。 */
	if ( xrtNumWrite(2.5, Buffer, sizeof(Buffer), &iNeed, 0u) ) {
		printf("num-w=%s\n", Buffer);
	}
	if ( xrtUIntWrite(UINT64_C(300), 16u, Buffer, sizeof(Buffer), &iNeed, 0u) ) {
		printf("uint-w=%s\n", Buffer);
	}
	/* IntWrite 两用：先查询长度（缓冲 NULL + 容量 0），再写入。 */

	(void)xrtIntWrite(-42, 10u, NULL, 0u, &iNeed, 0u);
	printf("int-write-need=%zu\n", iNeed);


	if ( xrtIntWrite(-42, 10u, Buffer, sizeof(Buffer), &iNeed, 0u) ) {
		printf("int-write=%s\n", Buffer);
	}
	return 0;
}
