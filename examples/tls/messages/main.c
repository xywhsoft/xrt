#include <stdio.h>
#include <string.h>

#include <xrt.h>



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
