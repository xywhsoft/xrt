/*
 * 范例：asn1/decode_tour —— DER 读取器族：类型化转换与游标工具
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDerPeek           窥视下一元素（不消费游标）
 *   xrtDerRemaining      游标剩余字节数
 *   xrtDerIs             类型匹配判定
 *   xrtDerBoolean        BOOLEAN → bool
 *   xrtDerInt64          INTEGER → int64
 *   xrtDerUnsigned       非负 INTEGER → 借用字节（任意精度）
 *   xrtDerOctets         OCTET STRING → 借用视图
 *   xrtDerBitString      BIT STRING → 数据 + 未用位数
 * 模块宏：XRT_MODULE_ASN1
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/asn1/decode_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   peek-tag=1 remaining=3 is-bool=1
 *   bool=true int=12345 unsigned-len=5
 *   octets=hello bitstring=A0 unused=4
 *
 * Peek vs Read：Read 消费游标；Peek 只窥不消费——
 *   "看一眼类型再决定怎么读"的分派入口。
 *   Unsigned 返回原始字节（非负 INTEGER 任意精度安全）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	xbuffer Buf;
	xdercursor Cursor;
	xdervalue Value;
	bool bBool = false;
	int64 iInt = 0;
	xbytesview Bytes;
	uint8 iUnused = 0;

	/* 构造含多类型的序列（编码器族产物）。 */
	xrtBufferInit(&Buf);
	(void)xrtDerAppendBoolean(&Buf, true);
	(void)xrtDerAppendInt64(&Buf, 12345);
	(void)xrtDerAppendOctets(&Buf, XRT_BYTES_LITERAL("hello"));
	(void)xrtDerAppendBitString(&Buf, XRT_BYTES_LITERAL("\xA0"), 4u);

	/* 游标绑定 + Peek / Remaining / Is。 */
	if ( !xrtDerInit(&Cursor, Buf.Data, Buf.Size) ) {
		return 1;
	}
	(void)xrtDerPeek(&Cursor, &Value);
	printf("peek-tag=%u remaining-after-peek=%zu",
		(unsigned)Value.Tag.Number, xrtDerRemaining(&Cursor));
	printf(" is-bool=%d\n",
		xrtDerIs(&Value, XASN1_UNIVERSAL, (uint32)XASN1_BOOLEAN, false) ? 1 : 0);

	/* 逐类型读取。 */
	if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
		(void)xrtDerBoolean(&Value, &bBool);
		printf("bool=%s", bBool ? "true" : "false");
	}
	if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
		(void)xrtDerInt64(&Value, &iInt);
		printf(" int=%lld", (long long)iInt);
		(void)xrtDerUnsigned(&Value, &Bytes);
		printf(" unsigned-len=%zu", Bytes.Size);
	}
	if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
		(void)xrtDerOctets(&Value, &Bytes);
		printf(" octets=%.*s", (int)Bytes.Size, (const char*)Bytes.Data);
	}
	if ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
		(void)xrtDerBitString(&Value, &Bytes, &iUnused);
		printf(" bitstring=%02X unused=%u\n",
			Bytes.Size ? (unsigned)Bytes.Data[0] : 0u, (unsigned)iUnused);
	}

	xrtBufferUnit(&Buf);
	return 0;
}
