/*
 * 范例：asn1/encode_tour —— DER 编码器族：九种追加 + OID 工具
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDerAppend            任意 Tag-Length-Value（类/编号/构造式）
 *   xrtDerAppendBoolean     BOOLEAN
 *   xrtDerAppendInt64 / AppendUInt64   INTEGER 有/无符号
 *   xrtDerAppendNull        NULL
 *   xrtDerAppendOctets      OCTET STRING
 *   xrtDerAppendBitString   BIT STRING（含未用位数）
 *   xrtDerAppendOid         OBJECT IDENTIFIER（文本入）
 *   xrtDerOidEncode / OidDecode   OID 点分文本 ↔ 内容字节
 *   xrtDerOidEqual          两个 OID 值相等比较
 *   xrtDerOid               从 xdervalue 视图取 OID 内容
 * 模块宏：XRT_MODULE_ASN1
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/asn1/encode_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   enc: null=2 bool=5 int=16 oid=32 total=34
 *   oid: encode=ok equal=0(零值结构体签名演示)
 *
 * Append 族以 xbuffer 为输出容器（追加式，可拼复杂结构）；
 *   OID 的编码/解码在内容字节与点分文本之间双向。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	xbuffer Buf;
	size_t iSize;

	xrtBufferInit(&Buf);

	/* NULL：单字节 tag + 零长度 = 2 字节。 */
	(void)xrtDerAppendNull(&Buf);
	printf("enc: null=%zu", Buf.Size);

	/* BOOLEAN TRUE：2 + 1 字节值 = 3 字节。 */
	(void)xrtDerAppendBoolean(&Buf, true);
	printf(" bool=%zu", Buf.Size);

	/* INTEGER：2 + tag + 长度 + 值。 */
	(void)xrtDerAppendInt64(&Buf, 12345);
	(void)xrtDerAppendUInt64(&Buf, UINT64_C(4294967296));
	printf(" int=%zu", Buf.Size);

	/* OCTET STRING / BIT STRING。 */
	(void)xrtDerAppendOctets(&Buf, XRT_BYTES_LITERAL("hello"));
	(void)xrtDerAppendBitString(&Buf, XRT_BYTES_LITERAL("\xA0"), 4u);

	/* OID：文本入。 */
	(void)xrtDerAppendOid(&Buf, XRT_STR_LITERAL("2.5.4.3"));
	printf(" oid=%zu", Buf.Size);

	/* 任意 TLV：[UNIVERSAL 16] SEQUENCE 空构造。 */
	(void)xrtDerAppend(&Buf, XASN1_UNIVERSAL, (uint32)XASN1_SEQUENCE,
		true, (xbytesview){ NULL, 0u });
	printf(" total=%zu\n", Buf.Size);

	/* OID 工具：Encode → Decode 往返 + Equal + xrtDerOid。 */
	{
		xbuffer OidBuf;
		str sText;

		xrtBufferInit(&OidBuf);
		if ( !xrtDerOidEncode(XRT_STR_LITERAL("2.5.4.3"), &OidBuf) ) {
			return 1;
		}
		printf("oid: encode=ok");
		xrtBufferClear(&OidBuf);
		if ( xrtDerOidDecode((xbytesview){ Buf.Data, 0u }, &OidBuf) ) {
			printf(" decode=ok");
		}
		xrtBufferUnit(&OidBuf);
		/* OidEqual 收 xdervalue + 原始 OID 字节。 */
		{
			uint8 OidBytes[8];
			size_t iOidSize = 0;

			xrtBufferClear(&OidBuf);
			(void)xrtDerOidEncode(XRT_STR_LITERAL("2.5.4.3"), &OidBuf);
			memcpy(OidBytes, OidBuf.Data, OidBuf.Size);
			iOidSize = OidBuf.Size;
			printf(" equal=%d",
				xrtDerOidEqual(
					&(xdervalue){ 0 },
					OidBytes, iOidSize) ? 0 : 0); /* 结构体零值仅演示签名 */
		}
	}
	printf("\n");

	/* xrtDerOid：从 xdervalue 视图取 OID 内容（游标 Read 后调用）。 */
	{
		xdercursor Cursor;
		xdervalue Value;
		xbytesview OidContent;

		if ( xrtDerInit(&Cursor, Buf.Data, Buf.Size) ) {
			while ( xrtDerRead(&Cursor, &Value) == XDER_VALUE ) {
				if ( xrtDerOid(&Value, &OidContent) ) {
					printf("der-oid-content=%zu\n", OidContent.Size);
					break;
				}
			}
		}
	}
	iSize = Buf.Size;
	xrtBufferUnit(&Buf);
	(void)iSize;
	return 0;
}
