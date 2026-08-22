#include "../test_allocator.h"



/* offer/response 解析、检查、接受和写出不得依赖堆分配。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"permessage-deflate; server_no_context_takeover; "
		"server_max_window_bits=10; client_max_window_bits"
	);
	xwsextension Extension;
	xwsdeflate Offer;
	xwsdeflate Response;
	char Output[XWS_DEFLATE_MAX_SIZE];
	size_t iOffset = 0;
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"permessage-deflate failure allocator install failed"
	);
	testRequire(
		xrtWsExtensionNext(
			Text,
			&iOffset,
			&Extension
		) == XHTTP_NEXT_ITEM,
		"permessage-deflate extension parse allocated"
	);
	testRequire(
		xrtWsDeflateOfferParse(
			&Extension,
			&Offer
		),
		"permessage-deflate offer parse allocated"
	);
	testRequire(
		xrtWsDeflateAccept(
			&Offer,
			&Response
		) &&
		xrtWsDeflateResponseCheck(
			&Offer,
			&Response
		),
		"permessage-deflate negotiation allocated"
	);
	testRequire(
		xrtWsDeflateResponseWrite(
			&Response,
			Output,
			sizeof(Output),
			&iSize
		),
		"permessage-deflate response write allocated"
	);
	printf("[PASS] websocket_deflate_noalloc\n");
	return 0;
}
