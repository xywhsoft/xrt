#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 RFC Accept 与无分配子协议选择能力。 */
int main(void)
{
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Selected;

	if ( !xrtWsAccept(
		XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
		Accept,
		sizeof(Accept)
	) || (strcmp(Accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != 0) ) {
		return 1;
	}
	if ( !xrtWsProtocolSelect(
		XRT_STR_LITERAL("chat, superchat"),
		XRT_STR_LITERAL("superchat, chat"),
		&Selected
	) || (Selected.Size != 4u) ||
		(memcmp(Selected.Data, "chat", 4u) != 0) ) {
		return 2;
	}
	return 0;
}
