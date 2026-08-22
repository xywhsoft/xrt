#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_HANDSHAKE_READER)

/* 检查 reader 的公开状态是否满足内部不变量。 */
static bool __xrtTlsHandshakeReaderValid(
	const xtlshandshakereader* pReader
)
{
	size_t iWireMax = XTLS_HANDSHAKE_HEADER_SIZE +
		XTLS_HANDSHAKE_BODY_MAX;

	if ( (pReader == NULL) ||
		((pReader->Data == NULL) && (pReader->Capacity != 0)) ||
		(pReader->Size > pReader->Capacity) ||
		(pReader->HeaderSize > XTLS_HANDSHAKE_HEADER_SIZE) ||
		(pReader->Limit < XTLS_HANDSHAKE_HEADER_SIZE) ||
		(pReader->Limit > iWireMax) ||
		(pReader->Retain > pReader->Limit) ) {
		return false;
	}
	if ( pReader->Size != 0 ) {
		if ( (pReader->HeaderSize != 0) ||
			(pReader->Size < XTLS_HANDSHAKE_HEADER_SIZE) ||
			(pReader->Required < XTLS_HANDSHAKE_HEADER_SIZE) ||
			(pReader->Size > pReader->Required) ||
			(pReader->Required != XTLS_HANDSHAKE_HEADER_SIZE +
				__xrtTlsRead24(pReader->Data + 1u)) ) {
			return false;
		}
	} else if ( pReader->HeaderSize != 0 ) {
		if ( (pReader->Required != 0) &&
			(pReader->Required != XTLS_HANDSHAKE_HEADER_SIZE) ) {
			return false;
		}
	}
	if ( pReader->Ready ) {
		return ((pReader->Size != 0) &&
			(pReader->Size == pReader->Required)) ||
			((pReader->Size == 0) &&
			 (pReader->HeaderSize == XTLS_HANDSHAKE_HEADER_SIZE) &&
			 (pReader->Required == XTLS_HANDSHAKE_HEADER_SIZE) &&
			 (__xrtTlsRead24(pReader->Header + 1u) == 0));
	}
	return true;
}



/* 拒绝把 reader 自己的可移动分配区再次作为输入。 */
static bool __xrtTlsHandshakeReaderOverlap(
	const xtlshandshakereader* pReader,
	xbytesview Input
)
{
	uintptr_t iBuffer;
	uintptr_t iInput;

	if ( (pReader->Capacity == 0) || (Input.Size == 0) ) {
		return false;
	}
	iBuffer = (uintptr_t)pReader->Data;
	iInput = (uintptr_t)Input.Data;
	if ( (iBuffer > UINTPTR_MAX - pReader->Capacity) ||
		(iInput > UINTPTR_MAX - Input.Size) ) {
		return true;
	}
	return (iBuffer < iInput + Input.Size) &&
		(iInput < iBuffer + pReader->Capacity);
}



/* 丢弃逻辑消息，并释放超过保留阈值的大缓冲。 */
static void __xrtTlsHandshakeReaderDrop(
	xtlshandshakereader* pReader
)
{
	if ( pReader->Capacity > pReader->Retain ) {
		xrtFree(pReader->Data);
		pReader->Data = NULL;
		pReader->Capacity = 0;
	}
	pReader->Size = 0;
	pReader->Required = 0;
	pReader->HeaderSize = 0;
	pReader->Ready = false;
	memset(pReader->Header, 0, sizeof(pReader->Header));
}



/* 渐进扩展跨分片消息缓冲，不按声明总长立即分配。 */
static bool __xrtTlsHandshakeReaderReserve(
	xtlshandshakereader* pReader,
	size_t iRequired
)
{
	bytes pData;
	size_t iCapacity;
	const xerror* pCause;

	if ( pReader->Capacity >= iRequired ) {
		return true;
	}
	iCapacity = pReader->Capacity;
	if ( iCapacity == 0 ) {
		iCapacity = 64u;
	}
	if ( iCapacity > pReader->Required ) {
		iCapacity = pReader->Required;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity + (iCapacity / 2u);

		if ( (iNext <= iCapacity) || (iNext > pReader->Required) ) {
			iNext = pReader->Required;
		}
		iCapacity = iNext;
	}
	pData = (bytes)xrtRealloc(pReader->Data, iCapacity);
	if ( pData == NULL ) {
		pCause = xrtGetError();
		__xrtTlsErrorCause(
			XERR_MEMORY, XTLS_ERROR_HANDSHAKE, "buffer-handshake",
			"TLS handshake reassembly buffer allocation failed",
			pReader->Size, pCause
		);
		return false;
	}
	pReader->Data = pData;
	pReader->Capacity = iCapacity;
	return true;
}



/* 从 reader 已完成的连续存储发布借用消息。 */
static void __xrtTlsHandshakeReaderPublish(
	xtlshandshakereader* pReader,
	xtlshandshake* pMessage
)
{
	xbytesview Data;
	xtlshandshake Message;

	if ( pReader->Size != 0 ) {
		Data.Data = pReader->Data;
		Data.Size = pReader->Size;
	} else {
		Data.Data = pReader->Header;
		Data.Size = pReader->HeaderSize;
	}
	(void)xrtTlsHandshakeParse(Data, &Message, NULL);
	*pMessage = Message;
}



/* 填充默认 reader 配置。 */
XRT_API void xrtTlsHandshakeReaderConfigInit(
	xtlshandshakereaderconfig* pConfig
)
{
	if ( pConfig == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"init-handshake-reader-config",
			"TLS handshake reader config is null", SIZE_MAX
		);
		return;
	}
	pConfig->Limit = XTLS_HANDSHAKE_LIMIT_DEFAULT;
	pConfig->Retain = XTLS_HANDSHAKE_RETAIN_DEFAULT;
}



/* 初始化一个空的自适应握手 reader。 */
XRT_API bool xrtTlsHandshakeReaderInit(
	xtlshandshakereader* pReader,
	const xtlshandshakereaderconfig* pConfig
)
{
	xtlshandshakereader Reader;
	xtlshandshakereaderconfig Config;
	size_t iWireMax = XTLS_HANDSHAKE_HEADER_SIZE +
		XTLS_HANDSHAKE_BODY_MAX;

	if ( pReader == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "init-handshake-reader",
			"TLS handshake reader is null", SIZE_MAX
		);
		return false;
	}
	if ( pConfig == NULL ) {
		xrtTlsHandshakeReaderConfigInit(&Config);
		pConfig = &Config;
	}
	if ( (pConfig->Limit < XTLS_HANDSHAKE_HEADER_SIZE) ||
		(pConfig->Limit > iWireMax) ||
		(pConfig->Retain > pConfig->Limit) ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "init-handshake-reader",
			"TLS handshake reader limits are invalid", SIZE_MAX
		);
		return false;
	}
	memset(&Reader, 0, sizeof(Reader));
	Reader.Limit = pConfig->Limit;
	Reader.Retain = pConfig->Retain;
	*pReader = Reader;
	return true;
}



/* 释放 reader 持有的缓冲。 */
XRT_API void xrtTlsHandshakeReaderUnit(
	xtlshandshakereader* pReader
)
{
	if ( pReader == NULL ) {
		return;
	}
	xrtFree(pReader->Data);
	memset(pReader, 0, sizeof(*pReader));
}



/* 丢弃当前消息并按阈值保留缓冲。 */
XRT_API bool xrtTlsHandshakeReaderReset(
	xtlshandshakereader* pReader
)
{
	if ( !__xrtTlsHandshakeReaderValid(pReader) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "reset-handshake-reader",
			"TLS handshake reader state is invalid", SIZE_MAX
		);
		return false;
	}
	__xrtTlsHandshakeReaderDrop(pReader);
	return true;
}



/* 返回完成当前消息所需的完整编码长度。 */
XRT_API size_t xrtTlsHandshakeReaderRequired(
	const xtlshandshakereader* pReader
)
{
	if ( !__xrtTlsHandshakeReaderValid(pReader) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"required-handshake-reader",
			"TLS handshake reader state is invalid", SIZE_MAX
		);
		return 0;
	}
	return pReader->Required != 0 ?
		pReader->Required : XTLS_HANDSHAKE_HEADER_SIZE;
}



/* 继续复制一个已经拥有完整头的跨分片消息。 */
static xtlsresult __xrtTlsHandshakeReaderBody(
	xtlshandshakereader* pReader,
	xbytesview Input,
	size_t* pConsumed,
	xtlshandshake* pMessage
)
{
	xtlshandshakereader Reader = *pReader;
	size_t iNeed = Reader.Required - Reader.Size;
	size_t iCopy = Input.Size < iNeed ? Input.Size : iNeed;

	if ( !__xrtTlsHandshakeReaderReserve(
		&Reader, Reader.Size + iCopy
	) ) {
		return XTLS_ERROR;
	}
	if ( iCopy != 0 ) {
		memcpy(Reader.Data + Reader.Size, Input.Data, iCopy);
		Reader.Size += iCopy;
	}
	*pConsumed = iCopy;
	if ( Reader.Size != Reader.Required ) {
		*pReader = Reader;
		return XTLS_AGAIN;
	}
	Reader.Ready = true;
	*pReader = Reader;
	__xrtTlsHandshakeReaderPublish(pReader, pMessage);
	return XTLS_OK;
}



/* 读取至多一条握手消息，并在分片时渐进重组。 */
XRT_API xtlsresult xrtTlsHandshakeReaderRead(
	xtlshandshakereader* pReader,
	xbytesview Input,
	size_t* pConsumed,
	xtlshandshake* pMessage
)
{
	xtlshandshakereader Reader;
	uint8 Header[XTLS_HANDSHAKE_HEADER_SIZE];
	size_t iHeaderNeed;
	size_t iBodyCopy;
	size_t iRequired;
	size_t iCopy;

	if ( pConsumed != NULL ) {
		*pConsumed = 0;
	}
	if ( !__xrtTlsHandshakeReaderValid(pReader) ||
		(pConsumed == NULL) || (pMessage == NULL) ||
		((Input.Data == NULL) && (Input.Size != 0)) ||
		__xrtTlsHandshakeReaderOverlap(pReader, Input) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "read-handshake",
			"TLS handshake reader input or output is invalid", SIZE_MAX
		);
		return XTLS_ERROR;
	}
	if ( pReader->Ready && !xrtTlsHandshakeReaderReset(pReader) ) {
		return XTLS_ERROR;
	}
	Reader = *pReader;
	if ( Reader.Size != 0 ) {
		return __xrtTlsHandshakeReaderBody(
			pReader, Input, pConsumed, pMessage
		);
	}

	if ( (Reader.HeaderSize == 0) &&
		(Input.Size >= XTLS_HANDSHAKE_HEADER_SIZE) ) {
		iRequired = XTLS_HANDSHAKE_HEADER_SIZE +
			__xrtTlsRead24(Input.Data + 1u);
		if ( iRequired > Reader.Limit ) {
			__xrtTlsError(
				XERR_RANGE, XTLS_ERROR_LIMIT, "read-handshake",
				"TLS handshake message exceeds the configured limit", 1
			);
			return XTLS_ERROR;
		}
		if ( Input.Size >= iRequired ) {
			xtlshandshake Message;

			(void)xrtTlsHandshakeParse(
				(xbytesview) { Input.Data, iRequired }, &Message, NULL
			);
			*pReader = Reader;
			*pConsumed = iRequired;
			*pMessage = Message;
			return XTLS_OK;
		}
		Reader.Required = iRequired;
		if ( !__xrtTlsHandshakeReaderReserve(
			&Reader, Input.Size
		) ) {
			return XTLS_ERROR;
		}
		memcpy(Reader.Data, Input.Data, Input.Size);
		Reader.Size = Input.Size;
		*pReader = Reader;
		*pConsumed = Input.Size;
		return XTLS_AGAIN;
	}

	iHeaderNeed = XTLS_HANDSHAKE_HEADER_SIZE - Reader.HeaderSize;
	if ( Input.Size < iHeaderNeed ) {
		if ( Input.Size != 0 ) {
			memcpy(
				Reader.Header + Reader.HeaderSize,
				Input.Data,
				Input.Size
			);
			Reader.HeaderSize += (uint8)Input.Size;
		}
		*pReader = Reader;
		*pConsumed = Input.Size;
		return XTLS_AGAIN;
	}
	memcpy(Header, Reader.Header, Reader.HeaderSize);
	memcpy(
		Header + Reader.HeaderSize, Input.Data, iHeaderNeed
	);
	iRequired = XTLS_HANDSHAKE_HEADER_SIZE + __xrtTlsRead24(Header + 1u);
	if ( iRequired > Reader.Limit ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "read-handshake",
			"TLS handshake message exceeds the configured limit", 1
		);
		return XTLS_ERROR;
	}
	if ( iRequired == XTLS_HANDSHAKE_HEADER_SIZE ) {
		memcpy(Reader.Header, Header, sizeof(Header));
		Reader.HeaderSize = XTLS_HANDSHAKE_HEADER_SIZE;
		Reader.Required = iRequired;
		Reader.Ready = true;
		*pReader = Reader;
		*pConsumed = iHeaderNeed;
		__xrtTlsHandshakeReaderPublish(pReader, pMessage);
		return XTLS_OK;
	}

	iBodyCopy = Input.Size - iHeaderNeed;
	if ( iBodyCopy > iRequired - XTLS_HANDSHAKE_HEADER_SIZE ) {
		iBodyCopy = iRequired - XTLS_HANDSHAKE_HEADER_SIZE;
	}
	iCopy = XTLS_HANDSHAKE_HEADER_SIZE + iBodyCopy;
	Reader.Required = iRequired;
	if ( !__xrtTlsHandshakeReaderReserve(&Reader, iCopy) ) {
		return XTLS_ERROR;
	}
	memcpy(Reader.Data, Header, sizeof(Header));
	if ( iBodyCopy != 0 ) {
		memcpy(
			Reader.Data + XTLS_HANDSHAKE_HEADER_SIZE,
			Input.Data + iHeaderNeed,
			iBodyCopy
		);
	}
	Reader.HeaderSize = 0;
	Reader.Size = iCopy;
	*pConsumed = iHeaderNeed + iBodyCopy;
	if ( Reader.Size != Reader.Required ) {
		*pReader = Reader;
		return XTLS_AGAIN;
	}
	Reader.Ready = true;
	*pReader = Reader;
	__xrtTlsHandshakeReaderPublish(pReader, pMessage);
	return XTLS_OK;
}

#endif
