#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 TLS 消息调用方缓冲编码。 */
int main(void)
{
	static const uint8 Data[] = { 1, 2, 3 };
	uint8 Body[16];
	xtlscertificateentry Entry;
	xtlscertificatemessage Message;
	size_t iSize;

	memset(&Entry, 0, sizeof(Entry));
	Entry.Data = (xbytesview) { Data, sizeof(Data) };
	iSize = xrtTlsCertificateSize(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, &Entry, 1u
	);
	return (iSize != 0) && xrtTlsCertificateEncode(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, &Entry, 1u,
		Body, sizeof(Body)
	) && xrtTlsCertificateParse(
		XTLS_VERSION_13, (xbytesview) { Body, iSize }, &Message
	) ? 0 : 1;
}
