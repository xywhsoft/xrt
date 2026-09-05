#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：tls/messages —— Certificate 消息：编码与条目遍历
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsCertificateSize / Encode   证书链 → 正文（两遍式）
 *   xrtTlsCertificateParse           正文 → 消息视图
 *   xrtTlsCertificateEntries / Read  链条目游标（三态）
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/messages/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   certificate[0]: 3 bytes
 *   certificate[1]: 3 bytes
 *
 * TLS 1.3 的证书按"条目"组织（每条可带 OCSP 扩展），
 *   与 1.2 的裸列表不同——Encode/Parse 统一处理两个版本
 *   （第一参数给版本）。条目是 DER 视图，
 *   交给 x509 模块解析（见 x509/inspect）。
 */


/* 编码并遍历一个两项 TLS 1.3 Certificate 正文。 */
int main(void)
{
	static const uint8 Leaf[] = { 0x30, 0x01, 0x01 };
	static const uint8 Issuer[] = { 0x30, 0x01, 0x02 };
	xtlscertificateentry Chain[2];
	uint8 Body[64];
	xtlscertificatemessage Message;
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;
	xtlsitemresult Result;
	size_t iBodySize;
	size_t iCount = 0;

	memset(Chain, 0, sizeof(Chain));
	Chain[0].Data = (xbytesview) { Leaf, sizeof(Leaf) };
	Chain[1].Data = (xbytesview) { Issuer, sizeof(Issuer) };
	iBodySize = xrtTlsCertificateSize(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2u
	);
	if ( (iBodySize == 0) || !xrtTlsCertificateEncode(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2u,
		Body, sizeof(Body)
	) || !xrtTlsCertificateParse(
		XTLS_VERSION_13, (xbytesview) { Body, iBodySize }, &Message
	) || !xrtTlsCertificateEntries(&Message, &Cursor) ) {
		return 1;
	}
	while ( (Result = xrtTlsCertificatesRead(
		&Cursor, &Entry
	)) == XTLS_ITEM_VALUE ) {
		printf("certificate[%zu]: %zu bytes\n", iCount, Entry.Data.Size);
		iCount++;
	}
	return (Result == XTLS_ITEM_DONE) && (iCount == 2u) ? 0 : 1;
}
