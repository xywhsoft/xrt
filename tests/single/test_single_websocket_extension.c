#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留严格扩展字段迭代和写出能力。 */
int main(void)
{
	xwsextension Extension;
	char Output[64];
	size_t iOffset = 0;
	size_t iSize;

	if ( xrtWsExtensionNext(
		XRT_STR_LITERAL(
			"permessage-deflate; server_no_context_takeover"
		),
		&iOffset,
		&Extension
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	if ( !xrtWsExtensionWrite(
		Extension.Name,
		Extension.Parameters,
		Output,
		sizeof(Output),
		&iSize
	) || (iSize != 46u) ||
		(memcmp(
			Output,
			"permessage-deflate; server_no_context_takeover",
			iSize
		) != 0) ) {
		return 2;
	}
	return 0;
}
