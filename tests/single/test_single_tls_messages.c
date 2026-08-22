#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供严格 TLS 证书消息解析。 */
int main(void)
{
	static const uint8 Body[] = { 0, 0, 4, 0, 0, 1, 0xAA };
	xtlscertificatemessage Message;
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;

	return xrtTlsCertificateParse(
		XTLS_VERSION_12, (xbytesview) { Body, sizeof(Body) }, &Message
	) && xrtTlsCertificateEntries(&Message, &Cursor) &&
		(xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE) &&
		(Entry.Data.Size == 1u) ? 0 : 1;
}
