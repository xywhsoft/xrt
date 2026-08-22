#include <stdio.h>

#include <xrt.h>



/* 展示解析客户端 offer 并构造最小合规服务端响应。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"permessage-deflate; server_max_window_bits=10; "
		"client_max_window_bits"
	);
	xwsextension Extension;
	xwsdeflate Offer;
	xwsdeflate Response;
	char Output[XWS_DEFLATE_MAX_SIZE + 1u];
	size_t iOffset = 0;
	size_t iSize;

	if ( xrtWsExtensionNext(
		Text,
		&iOffset,
		&Extension
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	if ( !xrtWsDeflateOfferParse(
		&Extension,
		&Offer
	) || !xrtWsDeflateAccept(
		&Offer,
		&Response
	) || !xrtWsDeflateResponseWrite(
		&Response,
		Output,
		XWS_DEFLATE_MAX_SIZE,
		&iSize
	) ) {
		return 2;
	}
	Output[iSize] = '\0';
	printf("response=%s\n", Output);
	return 0;
}
