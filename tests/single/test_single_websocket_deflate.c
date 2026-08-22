#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 permessage-deflate 的纯协议协商能力。 */
int main(void)
{
	xwsextension Extension;
	xwsdeflate Offer;
	xwsdeflate Response;
	char Output[XWS_DEFLATE_MAX_SIZE];
	size_t iOffset = 0;
	size_t iSize;

	if ( xrtWsExtensionNext(
		XRT_STR_LITERAL(
			"permessage-deflate; server_max_window_bits=10"
		),
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
		sizeof(Output),
		&iSize
	) || (iSize == 0) ) {
		return 2;
	}
	return 0;
}
