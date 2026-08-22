#include "../internal/xrt_websocket.h"



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

#define XRT_WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define XRT_WS_GUID_SIZE 36u



/* 验证借用字符串视图的空值一致性。 */
static bool __xrtWsViewValid(xstrview Text)
{
	return __xrtRangeValid(Text.Data, Text.Size);
}



/* 删除字段值两端的可选横向空白。 */
static xstrview __xrtWsOwsTrim(xstrview Text)
{
	while ( (Text.Size != 0) &&
		((Text.Data[0] == ' ') || (Text.Data[0] == '\t')) ) {
		Text.Data++;
		Text.Size--;
	}
	while ( (Text.Size != 0) &&
		((Text.Data[Text.Size - 1u] == ' ') ||
		 (Text.Data[Text.Size - 1u] == '\t')) ) {
		Text.Size--;
	}
	return Text;
}



/* 返回标准 Base64 字符值，填充和其他字节返回负值。 */
static int __xrtWsBase64Value(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z') ) {
		return (int)(iByte - (unsigned char)'A');
	}
	if ( (iByte >= (unsigned char)'a') &&
		(iByte <= (unsigned char)'z') ) {
		return (int)(iByte - (unsigned char)'a') + 26;
	}
	if ( (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0') + 52;
	}
	if ( iByte == (unsigned char)'+' ) {
		return 62;
	}
	if ( iByte == (unsigned char)'/' ) {
		return 63;
	}
	return -1;
}



/* 验证规范 Base64 文本恰好表示十六字节 nonce。 */
static bool __xrtWsKeyValidRaw(xstrview Key)
{
	int iLast;

	if ( !__xrtWsViewValid(Key) ) {
		return false;
	}
	Key = __xrtWsOwsTrim(Key);
	if ( (Key.Size != XWS_KEY_SIZE) ||
		(Key.Data[22] != '=') || (Key.Data[23] != '=') ) {
		return false;
	}
	for ( size_t i = 0; i < 22u; i++ ) {
		if ( __xrtWsBase64Value(
			(unsigned char)Key.Data[i]
		) < 0 ) {
			return false;
		}
	}
	iLast = __xrtWsBase64Value((unsigned char)Key.Data[21]);
	return (iLast & 0x0F) == 0;
}



/* 计算规范 Accept 文本，调用方已经验证 Key 并提供固定输出空间。 */
static bool __xrtWsAcceptCompute(
	xstrview Key,
	char sAccept[XWS_ACCEPT_CAPACITY]
)
{
	xsha1 State;
	uint8 Digest[XRT_SHA1_SIZE];
	size_t iOutputSize = 0;
	bool bResult;

	Key = __xrtWsOwsTrim(Key);
	xrtSha1Init(&State);
	bResult =
		xrtSha1Update(&State, Key.Data, Key.Size) &&
		xrtSha1Update(&State, XRT_WS_GUID, XRT_WS_GUID_SIZE) &&
		xrtSha1Final(&State, Digest) &&
		xrtBase64Encode(
			Digest,
			sizeof(Digest),
			sAccept,
			XWS_ACCEPT_CAPACITY,
			&iOutputSize,
			NULL
		) &&
		(iOutputSize == XWS_ACCEPT_SIZE);
	xrtSecureZero(Digest, sizeof(Digest));
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 按固定字节数比较两个 Accept 文本。 */
static bool __xrtWsAcceptEqual(
	const char* sLeft,
	const char* sRight
)
{
	unsigned char iDifference = 0;

	for ( size_t i = 0; i < XWS_ACCEPT_SIZE; i++ ) {
		iDifference |=
			(unsigned char)sLeft[i] ^ (unsigned char)sRight[i];
	}
	return iDifference == 0;
}



/*
	严格解析一个子协议条目。
	内部函数只返回状态，不修改线程错误，便于纯谓词和公开迭代器共用。
*/
static xhttpnext __xrtWsProtocolParse(
	xstrview Protocols,
	size_t iOffset,
	size_t* pNext,
	xstrview* pProtocol
)
{
	xstrview Protocol;
	size_t i;

	if ( iOffset == Protocols.Size ) {
		memset(pProtocol, 0, sizeof(*pProtocol));
		*pNext = iOffset;
		return XHTTP_NEXT_END;
	}
	i = iOffset;
	while ( (i < Protocols.Size) &&
		((Protocols.Data[i] == ' ') ||
		 (Protocols.Data[i] == '\t')) ) {
		i++;
	}
	if ( i == Protocols.Size ) {
		return XHTTP_NEXT_ERROR;
	}
	Protocol.Data = Protocols.Data + i;
	while ( (i < Protocols.Size) &&
		(Protocols.Data[i] != ',') &&
		(Protocols.Data[i] != ' ') &&
		(Protocols.Data[i] != '\t') ) {
		i++;
	}
	Protocol.Size = (size_t)((Protocols.Data + i) - Protocol.Data);
	if ( !xrtHttpTokenValid(Protocol) ) {
		return XHTTP_NEXT_ERROR;
	}
	while ( (i < Protocols.Size) &&
		((Protocols.Data[i] == ' ') ||
		 (Protocols.Data[i] == '\t')) ) {
		i++;
	}
	if ( i < Protocols.Size ) {
		if ( Protocols.Data[i] != ',' ) {
			return XHTTP_NEXT_ERROR;
		}
		i++;
		while ( (i < Protocols.Size) &&
			((Protocols.Data[i] == ' ') ||
			 (Protocols.Data[i] == '\t')) ) {
			i++;
		}
		if ( (i == Protocols.Size) ||
			(Protocols.Data[i] == ',') ) {
			return XHTTP_NEXT_ERROR;
		}
	}
	*pNext = i;
	*pProtocol = Protocol;
	return XHTTP_NEXT_ITEM;
}



/* 精确比较大小写敏感的子协议名称。 */
static bool __xrtWsProtocolEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证完整列表，并以无分配的前缀扫描拒绝重复名称。 */
static bool __xrtWsProtocolsValidRaw(xstrview Protocols)
{
	xstrview Current;
	size_t iOffset = 0;

	if ( !__xrtWsViewValid(Protocols) ) {
		return false;
	}
	if ( Protocols.Size == 0 ) {
		return true;
	}
	for ( ;; ) {
		xhttpnext Next;
		size_t iNext;

		Next = __xrtWsProtocolParse(
			Protocols, iOffset, &iNext, &Current
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		{
			xstrview Previous;
			size_t iPrevious = 0;

			while ( iPrevious < iOffset ) {
				size_t iPreviousNext;

				if ( __xrtWsProtocolParse(
					Protocols,
					iPrevious,
					&iPreviousNext,
					&Previous
				) != XHTTP_NEXT_ITEM ) {
					return false;
				}
				if ( __xrtWsProtocolEqual(Previous, Current) ) {
					return false;
				}
				iPrevious = iPreviousNext;
			}
		}
		iOffset = iNext;
	}
}



/* 在已验证列表中执行大小写敏感查找。 */
static bool __xrtWsProtocolsHasRaw(
	xstrview Protocols,
	xstrview Protocol
)
{
	xstrview Current;
	size_t iOffset = 0;

	for ( ;; ) {
		size_t iNext;
		xhttpnext Next = __xrtWsProtocolParse(
			Protocols, iOffset, &iNext, &Current
		);

		if ( Next != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( __xrtWsProtocolEqual(Current, Protocol) ) {
			return true;
		}
		iOffset = iNext;
	}
}



/* 验证去除两端 OWS 后的值是规范编码的十六字节 WebSocket nonce。 */
XRT_API bool xrtWsKeyValid(xstrview Key)
{
	return __xrtWsKeyValidRaw(Key);
}



/* 在栈上完成摘要和编码，最后一次复制保证重叠安全和失败原子性。 */
XRT_API bool xrtWsAccept(
	xstrview Key,
	char* sAccept,
	size_t iCapacity
)
{
	char Accept[XWS_ACCEPT_CAPACITY];

	if ( !__xrtWsViewValid(Key) ||
		!__xrtRangeValid(sAccept, XWS_ACCEPT_CAPACITY) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"compute-websocket-accept",
			"invalid WebSocket accept argument"
		);
		return false;
	}
	if ( !__xrtWsKeyValidRaw(Key) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_KEY,
			"compute-websocket-accept",
			"invalid Sec-WebSocket-Key"
		);
		return false;
	}
	if ( iCapacity < XWS_ACCEPT_CAPACITY ) {
		__xrtWsHandshakeError(
			XERR_RANGE,
			XWS_HANDSHAKE_ERROR_OUTPUT,
			"compute-websocket-accept",
			"WebSocket accept output is too small"
		);
		return false;
	}
	if ( !__xrtWsAcceptCompute(Key, Accept) ) {
		__xrtWsHandshakeWrap(
			XERR_INTERNAL,
			XWS_HANDSHAKE_ERROR_ACCEPT,
			"compute-websocket-accept",
			"failed to compute Sec-WebSocket-Accept"
		);
		xrtSecureZero(Accept, sizeof(Accept));
		return false;
	}
	memmove(sAccept, Accept, sizeof(Accept));
	xrtSecureZero(Accept, sizeof(Accept));
	return true;
}



/* 计算预期值后以固定工作量比较线路 Accept 字段。 */
XRT_API bool xrtWsAcceptValid(
	xstrview Key,
	xstrview Accept
)
{
	char Expected[XWS_ACCEPT_CAPACITY];
	bool bResult;

	if ( !__xrtWsViewValid(Accept) ||
		!__xrtWsKeyValidRaw(Key) ) {
		return false;
	}
	Accept = __xrtWsOwsTrim(Accept);
	if ( Accept.Size != XWS_ACCEPT_SIZE ) {
		return false;
	}
	if ( !__xrtWsAcceptCompute(Key, Expected) ) {
		xrtSecureZero(Expected, sizeof(Expected));
		return false;
	}
	bResult = __xrtWsAcceptEqual(Expected, Accept.Data);
	xrtSecureZero(Expected, sizeof(Expected));
	return bResult;
}



/* 严格迭代子协议 token，并只在成功或正常结束时提交输出。 */
XRT_API xhttpnext xrtWsProtocolNext(
	xstrview Protocols,
	size_t* pOffset,
	xstrview* pProtocol
)
{
	xstrview Protocol;
	xhttpnext Next;
	size_t iOffset;
	size_t iNext;

	if ( !__xrtWsViewValid(Protocols) ||
		!__xrtRangeValid(pOffset, sizeof(iOffset)) ||
		!__xrtRangeValid(pProtocol, sizeof(Protocol)) ||
		__xrtRangesOverlap(
			Protocols.Data, Protocols.Size,
			pOffset, sizeof(iOffset)
		) || __xrtRangesOverlap(
			Protocols.Data, Protocols.Size,
			pProtocol, sizeof(Protocol)
		) || __xrtRangesOverlap(
			pOffset, sizeof(iOffset),
			pProtocol, sizeof(Protocol)
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"iterate-websocket-protocol",
			"invalid WebSocket protocol iterator argument"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Protocols.Size ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"iterate-websocket-protocol",
			"WebSocket protocol iterator offset is out of range"
		);
		return XHTTP_NEXT_ERROR;
	}
	Next = __xrtWsProtocolParse(
		Protocols, iOffset, &iNext, &Protocol
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"iterate-websocket-protocol",
			"invalid Sec-WebSocket-Protocol list"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pOffset, &iNext, sizeof(iNext));
	memcpy(pProtocol, &Protocol, sizeof(Protocol));
	return Next;
}



/* 验证完整子协议列表的语法与名称唯一性。 */
XRT_API bool xrtWsProtocolsValid(xstrview Protocols)
{
	return __xrtWsProtocolsValidRaw(Protocols);
}



/* 完整验证后执行大小写敏感的子协议查找。 */
XRT_API bool xrtWsProtocolsHas(
	xstrview Protocols,
	xstrview Protocol
)
{
	if ( !__xrtWsViewValid(Protocol) ||
		!xrtHttpTokenValid(Protocol) ||
		!__xrtWsProtocolsValidRaw(Protocols) ) {
		return false;
	}
	return __xrtWsProtocolsHasRaw(Protocols, Protocol);
}



/* 先验证完整输入，再按客户端偏好顺序选择服务端支持项。 */
XRT_API bool xrtWsProtocolSelect(
	xstrview ClientProtocols,
	xstrview ServerProtocols,
	xstrview* pSelected
)
{
	xstrview Selected;
	xstrview Client;
	size_t iOffset = 0;

	memset(&Selected, 0, sizeof(Selected));
	if ( !__xrtRangeValid(pSelected, sizeof(Selected)) ||
		!__xrtWsViewValid(ClientProtocols) ||
		!__xrtWsViewValid(ServerProtocols) ||
		__xrtRangesOverlap(
			ClientProtocols.Data, ClientProtocols.Size,
			pSelected, sizeof(Selected)
		) || __xrtRangesOverlap(
			ServerProtocols.Data, ServerProtocols.Size,
			pSelected, sizeof(Selected)
		) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"select-websocket-protocol",
			"invalid WebSocket protocol selection argument"
		);
		return false;
	}
	if ( !__xrtWsProtocolsValidRaw(ClientProtocols) ||
		!__xrtWsProtocolsValidRaw(ServerProtocols) ) {
		__xrtWsHandshakeError(
			XERR_PROTOCOL,
			XWS_HANDSHAKE_ERROR_PROTOCOL,
			"select-websocket-protocol",
			"invalid Sec-WebSocket-Protocol list"
		);
		return false;
	}
	for ( ;; ) {
		size_t iNext;
		xhttpnext Next = __xrtWsProtocolParse(
			ClientProtocols, iOffset, &iNext, &Client
		);

		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( __xrtWsProtocolsHasRaw(ServerProtocols, Client) ) {
			Selected = Client;
			break;
		}
		iOffset = iNext;
	}
	memcpy(pSelected, &Selected, sizeof(Selected));
	return true;
}



#undef XRT_WS_GUID
#undef XRT_WS_GUID_SIZE

#endif
