#include "../test_allocator.h"



/* 验证握手计算、验证和协商成功路径完全不依赖堆分配。 */
int main(void)
{
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Selected;

	testRequire(
		testInstallFailAllocator(),
		"WebSocket handshake failure allocator install failed"
	);
	testRequire(
		xrtWsKeyValid(XRT_STR_LITERAL(
			"dGhlIHNhbXBsZSBub25jZQ=="
		)) &&
		xrtWsAccept(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			Accept,
			sizeof(Accept)
		) &&
		xrtWsAcceptValid(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			(xstrview){ Accept, XWS_ACCEPT_SIZE }
		),
		"WebSocket key or accept path unexpectedly allocated"
	);
	testRequire(
		xrtWsProtocolsValid(XRT_STR_LITERAL(
			"chat, superchat, binary.v2"
		)) &&
		xrtWsProtocolsHas(
			XRT_STR_LITERAL("chat, superchat, binary.v2"),
			XRT_STR_LITERAL("superchat")
		) &&
		xrtWsProtocolSelect(
			XRT_STR_LITERAL("chat, superchat"),
			XRT_STR_LITERAL("superchat, chat"),
			&Selected
		) &&
		(Selected.Size == 4u),
		"WebSocket protocol path unexpectedly allocated"
	);
	printf("[PASS] websocket_handshake_noalloc\n");
	return 0;
}
