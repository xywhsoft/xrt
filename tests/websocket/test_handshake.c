#include "../test.h"



/* 检查最近一次错误属于稳定 WebSocket 握手错误域。 */
static void testHandshakeError(
	xwshandshakeerror Code,
	cstr sMessage
)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.handshake"
		) == 0) &&
		(xrtErrorCode(pError) == (int32)Code),
		sMessage
	);
	xrtClearError();
}



/* 迁移 RFC 向量并压实规范 Base64、OWS 和输出边界。 */
static void testHandshakeKeyAndAccept(void)
{
	static const char Key[] = "dGhlIHNhbXBsZSBub25jZQ==";
	static const char Accept[] = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
	char Buffer[64];
	char Small[XWS_ACCEPT_CAPACITY];
	xstrview Invalid;

	testRequire(
		(XWS_VERSION == 13u) &&
		(XWS_KEY_BYTES == 16u) &&
		(XWS_KEY_SIZE == 24u) &&
		(XWS_KEY_CAPACITY == 25u) &&
		(XWS_ACCEPT_SIZE == 28u) &&
		(XWS_ACCEPT_CAPACITY == 29u),
		"WebSocket handshake constants mismatch"
	);
	testRequire(
		xrtWsKeyValid(XRT_STR_LITERAL(
			"dGhlIHNhbXBsZSBub25jZQ=="
		)) &&
		xrtWsKeyValid(XRT_STR_LITERAL(
			"\t dGhlIHNhbXBsZSBub25jZQ== \t"
		)),
		"WebSocket RFC key was rejected"
	);
	testRequire(
		!xrtWsKeyValid(XRT_STR_LITERAL(
			"dGhlIHNhbXBsZSBub25jZQ=A"
		)) &&
		!xrtWsKeyValid(XRT_STR_LITERAL(
			"AAAAAAAAAAAAAAAAAAAAAB=="
		)) &&
		!xrtWsKeyValid(XRT_STR_LITERAL(
			"dGhlIHNhbXBs ZSBub25jZQ=="
		)) &&
		!xrtWsKeyValid(XRT_STR_LITERAL(
			"dGhlIHNhbXBsZSBub25jZQ"
		)),
		"WebSocket malformed key was accepted"
	);

	memset(Buffer, 0xA5, sizeof(Buffer));
	testRequire(
		xrtWsAccept(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			Buffer,
			sizeof(Buffer)
		) &&
		(strcmp(Buffer, Accept) == 0),
		"WebSocket RFC accept vector mismatch"
	);
	testRequire(
		xrtWsAcceptValid(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			XRT_STR_LITERAL(
				"\t s3pPLMBiTxaQ9kYGzzhZRbK+xOo= \t"
			)
		) &&
		!xrtWsAcceptValid(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			XRT_STR_LITERAL("s3pPLMBiTxaQ9kYGzzhZRbK+xOA=")
		),
		"WebSocket accept validation mismatch"
	);

	memcpy(Buffer, Key, sizeof(Key));
	testRequire(
		xrtWsAccept(
			(xstrview){ Buffer, XWS_KEY_SIZE },
			Buffer,
			sizeof(Buffer)
		) &&
		(strcmp(Buffer, Accept) == 0),
		"WebSocket overlapping accept output failed"
	);

	memset(Small, 0xA5, sizeof(Small));
	testRequire(
		!xrtWsAccept(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			Small,
			XWS_ACCEPT_CAPACITY - 1u
		) &&
		((unsigned char)Small[0] == UINT8_C(0xA5)),
		"WebSocket accept capacity failure was not atomic"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_OUTPUT,
		"WebSocket accept capacity error mismatch"
	);

	memset(Buffer, 0xA5, sizeof(Buffer));
	testRequire(
		!xrtWsAccept(
			XRT_STR_LITERAL("invalid"),
			Buffer,
			sizeof(Buffer)
		) &&
		((unsigned char)Buffer[0] == UINT8_C(0xA5)),
		"WebSocket invalid key failure was not atomic"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_KEY,
		"WebSocket key error mismatch"
	);

	Invalid.Data = NULL;
	Invalid.Size = 1;
	testRequire(
		!xrtWsKeyValid(Invalid) &&
		!xrtWsAcceptValid(
			Invalid,
			XRT_STR_LITERAL("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")
		) &&
		!xrtWsAccept(Invalid, Buffer, sizeof(Buffer)),
		"WebSocket invalid key view was accepted"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket key argument error mismatch"
	);
}



/* 验证严格迭代、唯一性、大小写和失败时输出保持不变。 */
static void testHandshakeProtocols(void)
{
	union {
		size_t Offset;
		xstrview Protocol;
	} Alias;
	union {
		char Text[64];
		xstrview Selected;
	} Overlap;
	xstrview Protocol;
	xstrview Selected;
	size_t iOffset = 0;
	size_t iBefore;

	testRequire(
		xrtWsProtocolsValid((xstrview){ NULL, 0 }) &&
		xrtWsProtocolsValid(XRT_STR_LITERAL(
			"\tchat , superchat\t, Chat.v2 "
		)) &&
		xrtWsProtocolsHas(
			XRT_STR_LITERAL("chat, superchat, Chat.v2"),
			XRT_STR_LITERAL("superchat")
		) &&
		!xrtWsProtocolsHas(
			XRT_STR_LITERAL("chat, superchat, Chat.v2"),
			XRT_STR_LITERAL("CHAT")
		),
		"WebSocket protocol validation or lookup mismatch"
	);
	testRequire(
		!xrtWsProtocolsValid(XRT_STR_LITERAL(" ")) &&
		!xrtWsProtocolsValid(XRT_STR_LITERAL(",chat")) &&
		!xrtWsProtocolsValid(XRT_STR_LITERAL("chat,")) &&
		!xrtWsProtocolsValid(XRT_STR_LITERAL("chat,,superchat")) &&
		!xrtWsProtocolsValid(XRT_STR_LITERAL("chat, chat")) &&
		!xrtWsProtocolsValid(XRT_STR_LITERAL("bad protocol")) &&
		!xrtWsProtocolsValid(XRT_STR_LITERAL("chat\r\nInjected")) &&
		xrtWsProtocolsValid(XRT_STR_LITERAL("chat, Chat")),
		"WebSocket malformed or duplicate protocol list mismatch"
	);

	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("\tchat , superchat\t, Chat.v2 "),
			&iOffset,
			&Protocol
		) == XHTTP_NEXT_ITEM &&
		(Protocol.Size == 4u) &&
		(memcmp(Protocol.Data, "chat", 4u) == 0),
		"WebSocket first protocol iteration mismatch"
	);
	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("\tchat , superchat\t, Chat.v2 "),
			&iOffset,
			&Protocol
		) == XHTTP_NEXT_ITEM &&
		(Protocol.Size == 9u) &&
		(memcmp(Protocol.Data, "superchat", 9u) == 0),
		"WebSocket second protocol iteration mismatch"
	);
	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("\tchat , superchat\t, Chat.v2 "),
			&iOffset,
			&Protocol
		) == XHTTP_NEXT_ITEM &&
		(Protocol.Size == 7u) &&
		(memcmp(Protocol.Data, "Chat.v2", 7u) == 0),
		"WebSocket third protocol iteration mismatch"
	);
	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("\tchat , superchat\t, Chat.v2 "),
			&iOffset,
			&Protocol
		) == XHTTP_NEXT_END &&
		(Protocol.Data == NULL) && (Protocol.Size == 0),
		"WebSocket protocol iteration end mismatch"
	);

	iOffset = 0;
	Protocol = XRT_STR_LITERAL("unchanged");
	iBefore = iOffset;
	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("chat,,bad"),
			&iOffset,
			&Protocol
		) == XHTTP_NEXT_ERROR &&
		(iOffset == iBefore) &&
		(Protocol.Size == sizeof("unchanged") - 1u),
		"WebSocket protocol syntax failure was not atomic"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_PROTOCOL,
		"WebSocket protocol syntax error mismatch"
	);

	Alias.Offset = 0;
	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("chat"),
			&Alias.Offset,
			&Alias.Protocol
		) == XHTTP_NEXT_ERROR,
		"WebSocket protocol iterator accepted overlapping outputs"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket protocol iterator alias error mismatch"
	);

	testRequire(
		xrtWsProtocolSelect(
			XRT_STR_LITERAL("chat, superchat"),
			XRT_STR_LITERAL("superchat, chat"),
			&Selected
		) &&
		(Selected.Size == 4u) &&
		(memcmp(Selected.Data, "chat", 4u) == 0),
		"WebSocket protocol selection ignored client preference"
	);
	testRequire(
		xrtWsProtocolSelect(
			XRT_STR_LITERAL("chat, superchat"),
			XRT_STR_LITERAL("binary"),
			&Selected
		) &&
		(Selected.Data == NULL) && (Selected.Size == 0),
		"WebSocket no-match selection must succeed without a protocol"
	);

	Selected = XRT_STR_LITERAL("unchanged");
	testRequire(
		!xrtWsProtocolSelect(
			XRT_STR_LITERAL("chat,"),
			XRT_STR_LITERAL("chat"),
			&Selected
		) &&
		(Selected.Size == sizeof("unchanged") - 1u),
		"WebSocket selection accepted malformed client tail"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_PROTOCOL,
		"WebSocket selection client error mismatch"
	);

	Selected = XRT_STR_LITERAL("unchanged");
	testRequire(
		!xrtWsProtocolSelect(
			XRT_STR_LITERAL("chat"),
			XRT_STR_LITERAL("chat,"),
			&Selected
		) &&
		(Selected.Size == sizeof("unchanged") - 1u),
		"WebSocket selection accepted malformed server tail"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_PROTOCOL,
		"WebSocket selection server error mismatch"
	);

	memcpy(
		Overlap.Text,
		"chat, superchat",
		sizeof("chat, superchat")
	);
	testRequire(
		!xrtWsProtocolSelect(
			(xstrview){
				Overlap.Text,
				sizeof("chat, superchat") - 1u
			},
			XRT_STR_LITERAL("chat"),
			&Overlap.Selected
		),
		"WebSocket protocol selection accepted overlapping output"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket protocol selection alias error mismatch"
	);
}



/* 验证握手视图拒绝地址回绕，定长迭代输出支持未对齐存储。 */
static void testHandshakeMemoryContracts(void)
{
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 ProtocolStorage[sizeof(xstrview) + 2u];
	uint8 SelectedStorage[sizeof(xstrview) + 2u];
	char Output[XWS_ACCEPT_CAPACITY];
	xstrview Wrapping = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};
	xstrview Protocol;
	xstrview Selected;
	xstrview Unchanged = XRT_STR_LITERAL("unchanged");
	size_t iOffset = 0;

	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(ProtocolStorage, 0xA5, sizeof(ProtocolStorage));
	memcpy(OffsetStorage + 1u, &iOffset, sizeof(iOffset));
	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("chat"),
			(size_t*)(void*)(OffsetStorage + 1u),
			(xstrview*)(void*)(ProtocolStorage + 1u)
		) == XHTTP_NEXT_ITEM,
		"WebSocket protocol iterator rejected unaligned outputs"
	);
	memcpy(&iOffset, OffsetStorage + 1u, sizeof(iOffset));
	memcpy(&Protocol, ProtocolStorage + 1u, sizeof(Protocol));
	testRequire(
		(OffsetStorage[0] == 0xA5) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] == 0xA5) &&
		(ProtocolStorage[0] == 0xA5) &&
		(ProtocolStorage[sizeof(ProtocolStorage) - 1u] == 0xA5) &&
		(iOffset == 4u) && (Protocol.Size == 4u) &&
		(memcmp(Protocol.Data, "chat", 4u) == 0),
		"WebSocket protocol iterator published wrong unaligned outputs"
	);

	memset(SelectedStorage, 0xA5, sizeof(SelectedStorage));
	testRequire(
		xrtWsProtocolSelect(
			XRT_STR_LITERAL("chat, binary"),
			XRT_STR_LITERAL("binary, chat"),
			(xstrview*)(void*)(SelectedStorage + 1u)
		),
		"WebSocket protocol selection rejected unaligned output"
	);
	memcpy(&Selected, SelectedStorage + 1u, sizeof(Selected));
	testRequire(
		(SelectedStorage[0] == 0xA5) &&
		(SelectedStorage[sizeof(SelectedStorage) - 1u] == 0xA5) &&
		(Selected.Size == 4u) &&
		(memcmp(Selected.Data, "chat", 4u) == 0),
		"WebSocket protocol selection published wrong unaligned output"
	);

	memset(Output, 0xA5, sizeof(Output));
	testRequire(
		!xrtWsKeyValid(Wrapping) &&
		!xrtWsAcceptValid(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			Wrapping
		) &&
		!xrtWsProtocolsValid(Wrapping) &&
		!xrtWsProtocolsHas(Wrapping, XRT_STR_LITERAL("chat")) &&
		!xrtWsProtocolsHas(XRT_STR_LITERAL("chat"), Wrapping),
		"WebSocket handshake predicate accepted a wrapping view"
	);
	xrtClearError();
	testRequire(
		!xrtWsAccept(
			Wrapping,
			Output,
			sizeof(Output)
		) && ((unsigned char)Output[0] == UINT8_C(0xA5)),
		"WebSocket Accept accepted a wrapping Key"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket wrapping Key error mismatch"
	);
	testRequire(
		!xrtWsAccept(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			(char*)(uintptr_t)(UINTPTR_MAX - 1u),
			XWS_ACCEPT_CAPACITY
		),
		"WebSocket Accept accepted a wrapping output"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket wrapping Accept output error mismatch"
	);

	iOffset = 0;
	Protocol = Unchanged;
	testRequire(
		xrtWsProtocolNext(
			Wrapping,
			&iOffset,
			&Protocol
		) == XHTTP_NEXT_ERROR &&
		(iOffset == 0) &&
		(memcmp(&Protocol, &Unchanged, sizeof(Protocol)) == 0),
		"WebSocket iterator changed outputs for a wrapping list"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket wrapping protocol list error mismatch"
	);
	testRequire(
		xrtWsProtocolNext(
			XRT_STR_LITERAL("chat"),
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u),
			&Protocol
		) == XHTTP_NEXT_ERROR &&
		xrtWsProtocolNext(
			XRT_STR_LITERAL("chat"),
			&iOffset,
			(xstrview*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == XHTTP_NEXT_ERROR,
		"WebSocket iterator accepted a wrapping output"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket wrapping iterator output error mismatch"
	);

	Selected = Unchanged;
	testRequire(
		!xrtWsProtocolSelect(
			Wrapping,
			XRT_STR_LITERAL("chat"),
			&Selected
		) &&
		(memcmp(&Selected, &Unchanged, sizeof(Selected)) == 0) &&
		!xrtWsProtocolSelect(
			XRT_STR_LITERAL("chat"),
			XRT_STR_LITERAL("chat"),
			(xstrview*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket protocol selection accepted a wrapping range"
	);
	testHandshakeError(
		XWS_HANDSHAKE_ERROR_ARGUMENT,
		"WebSocket wrapping selection range error mismatch"
	);
}



/* 纯握手谓词不得清除或替换调用方已有错误。 */
static void testHandshakePredicateErrors(void)
{
	char Output[XWS_ACCEPT_CAPACITY];
	const xerror* pBefore;

	testRequire(
		!xrtWsAccept(
			XRT_STR_LITERAL("invalid"),
			Output,
			sizeof(Output)
		),
		"WebSocket predicate error fixture failed"
	);
	pBefore = xrtGetError();
	testRequire(
		(pBefore != NULL) &&
		!xrtWsKeyValid(XRT_STR_LITERAL("invalid")) &&
		!xrtWsAcceptValid(
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			XRT_STR_LITERAL("s3pPLMBiTxaQ9kYGzzhZRbK+xOA=")
		) &&
		!xrtWsProtocolsValid(XRT_STR_LITERAL("chat,,bad")) &&
		!xrtWsProtocolsHas(
			XRT_STR_LITERAL("chat,,bad"),
			XRT_STR_LITERAL("chat")
		) &&
		(xrtGetError() == pBefore),
		"WebSocket predicate changed the current error"
	);
	xrtClearError();
}



/* 运行 WebSocket 纯握手协议回归。 */
int main(void)
{
	testHandshakeKeyAndAccept();
	testHandshakeProtocols();
	testHandshakeMemoryContracts();
	testHandshakePredicateErrors();
	printf("[PASS] websocket_handshake\n");
	return 0;
}
