/*
 * 范例：asn1/der —— 用游标从一个借用 DER SEQUENCE 中读出两个整数
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDerValidate  整体结构校验（长度前缀、嵌套边界一遍过）
 *   xrtDerInit      在借用字节上建立根游标（零拷贝）
 *   xrtDerExpect    断言"当前元素必须是某类型"并取出值视图
 *   xrtDerEnter     进入构造类型（SEQUENCE/SET）的内部，得到子游标
 *   xrtDerRead      顺序读取下一个子元素，三态返回
 *   xrtDerUInt64    把 INTEGER 值视图转换为 uint64
 *   xrtDerDone      断言游标已消费完毕（防尾随垃圾）
 * 模块宏：XRT_MODULE_ASN1
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/asn1/der/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   7 + 42 = 49
 *
 * 游标模型（asn1.h）：
 *   - 游标只借用输入内存，解析全程零分配、零拷贝；
 *   - 读出来的 xdervalue 是"类型 + 字节范围"的视图，
 *     数值转换（UInt64/Int64 等）由调用方按需触发；
 *   - Read 返回三态：XDER_VALUE 取到元素 / XDER_END 到达边界 /
 *     XDER_ERROR 结构非法并设置线程错误。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/*
	 * 手工构造的最小 DER 文档：
	 *   30 06                SEQUENCE，内容 6 字节
	 *     02 01 07             INTEGER 7
	 *     02 01 2A             INTEGER 42
	 * x509/tls 证书解析就是这个模型逐层展开的。
	 */
	static const uint8 Document[] = {
		0x30, 0x06,
		0x02, 0x01, 0x07,
		0x02, 0x01, 0x2A
	};
	xdercursor Root;      /* 根游标：指向整个文档 */
	xdercursor Items;     /* 子游标：SEQUENCE 内部 */
	xdervalue Value;      /* 当前元素的值视图（类型 + 范围） */
	uint64 iLeft;
	uint64 iRight;

	/*
	 * 一步串联整条解析链，任何一环失败都走统一错误分支：
	 *   Validate —— 先整体校验，杜绝"读到一半才发现越界"；
	 *   Init     —— 建立根游标（借用，不复制 Document）；
	 *   Expect   —— 根元素必须是 (UNIVERSAL, SEQUENCE, 构造式)；
	 *   Enter    —— 进入 SEQUENCE 得到子游标 Items。
	 */
	if ( !xrtDerValidate(Document, sizeof(Document)) ||
		!xrtDerInit(&Root, Document, sizeof(Document)) ||
		!xrtDerExpect(
			&Root, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE, true, &Value
		) || !xrtDerEnter(&Value, &Items) ||

		/* 依次读取两个 INTEGER 并转换为 uint64（7 与 42）。 */
		(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
		!xrtDerUInt64(&Value, &iLeft) ||
		(xrtDerRead(&Items, &Value) != XDER_VALUE) ||
		!xrtDerUInt64(&Value, &iRight) ||

		/* Done 断言恰好消费完：多一个字节都算结构非法。 */
		!xrtDerDone(&Items) ) {
		return 1;
	}
	printf("%llu + %llu = %llu\n",
		(unsigned long long)iLeft,
		(unsigned long long)iRight,
		(unsigned long long)(iLeft + iRight));
	return 0;
}
