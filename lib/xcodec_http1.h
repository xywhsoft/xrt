#ifndef XRT_XCODEC_HTTP1_H
#define XRT_XCODEC_HTTP1_H

/*
	XRT mainline HTTP/1.1 codec.

	This header provides:
	  - request/response start-line and header parsing over xnetchain
	  - content-length, chunked, keep-alive, and upgrade metadata extraction
	  - whole-message delimiting for fixed-length and chunked bodies
	  - chunk decode helpers used by xhttp and xhttpd
*/


/* ============================== HTTP/1 public model ============================== */

#if !defined(XRT_BUILD_CORE)

#define XCODEC_HTTP1_MAX_HEADERS 32u
#define XCODEC_HTTP1_TOKEN_CAP   32u
#define XCODEC_HTTP1_TARGET_CAP  256u
#define XCODEC_HTTP1_VALUE_CAP   256u
#define XCODEC_HTTP1_REASON_CAP  128u

#define XCODEC_HTTP1_F_NONE       0x00000000u
#define XCODEC_HTTP1_F_REQUEST    0x00000001u
#define XCODEC_HTTP1_F_RESPONSE   0x00000002u
#define XCODEC_HTTP1_F_CHUNKED    0x00000004u
#define XCODEC_HTTP1_F_KEEPALIVE  0x00000008u
#define XCODEC_HTTP1_F_UPGRADE    0x00000010u

typedef struct {
	char sName[XCODEC_HTTP1_TOKEN_CAP];
	char sValue[XCODEC_HTTP1_VALUE_CAP];
} xcodechttp1header;

typedef struct {
	uint32 iFlags;
	uint32 iHeaderCount;
	uint32 iStatusCode;
	int64_t iContentLength;
	size_t iHeadBytes;
	char sMethod[XCODEC_HTTP1_TOKEN_CAP];
	char sTarget[XCODEC_HTTP1_TARGET_CAP];
	char sVersion[XCODEC_HTTP1_TOKEN_CAP];
	char sReason[XCODEC_HTTP1_REASON_CAP];
	xcodechttp1header arrHeaders[XCODEC_HTTP1_MAX_HEADERS];
} xcodechttp1msg;

#endif /* !XRT_BUILD_CORE */



/* ============================== Internal string helpers ============================== */

static char __xcodecHttpToLower(char ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) { return (char)(ch + 32); }
	return ch;
}


// 内部函数：HTTP 字符串相等忽略大小写相关处理
static bool __xcodecHttpStrEqNoCase(const char* sA, const char* sB)
{
	size_t i = 0;
	if ( !sA || !sB ) { return false; }
	while ( sA[i] && sB[i] ) {
		if ( __xcodecHttpToLower(sA[i]) != __xcodecHttpToLower(sB[i]) ) { return false; }
		i++;
	}
	return sA[i] == '\0' && sB[i] == '\0';
}


// 内部函数：判断是否包含 HTTP Token 忽略大小写
static bool __xcodecHttpContainsTokenNoCase(const char* sText, const char* sToken)
{
	size_t iLenText;
	size_t iLenToken;
	if ( !sText || !sToken ) { return false; }
	iLenText = strlen(__xrt_cstr(sText));
	iLenToken = strlen(__xrt_cstr(sToken));
	if ( iLenToken == 0 || iLenToken > iLenText ) { return false; }
	for ( size_t i = 0; i + iLenToken <= iLenText; ++i ) {
		size_t j;
		for ( j = 0; j < iLenToken; ++j ) {
			if ( __xcodecHttpToLower(sText[i + j]) != __xcodecHttpToLower(sToken[j]) ) { break; }
		}
		if ( j == iLenToken ) { return true; }
	}
	return false;
}


// 内部函数：复制 HTTP Token
static void __xcodecHttpCopyToken(char* sDst, size_t iDstCap, const char* sSrc, size_t iLen)
{
	size_t iCopyLen;
	if ( !sDst || iDstCap == 0 ) { return; }
	if ( !sSrc ) {
		sDst[0] = '\0';
		return;
	}
	iCopyLen = iLen;
	if ( iCopyLen >= iDstCap ) { iCopyLen = iDstCap - 1u; }
	memcpy(sDst, sSrc, iCopyLen);
	sDst[iCopyLen] = '\0';
}


// 内部函数：裁剪 HTTP 视图
static void __xcodecHttpTrimView(const char** ppText, size_t* pLen)
{
	const char* sText = ppText ? *ppText : NULL;
	size_t iLen = pLen ? *pLen : 0;
	if ( !ppText || !pLen || !sText ) { return; }
	while ( iLen > 0 && (*sText == ' ' || *sText == '\t') ) {
		sText++;
		iLen--;
	}
	while ( iLen > 0 && (sText[iLen - 1] == ' ' || sText[iLen - 1] == '\t') ) {
		iLen--;
	}
	*ppText = sText;
	*pLen = iLen;
}


// 内部函数：解析 HTTP 整数 64
static bool __xcodecHttpParseInt64(const char* sText, int64_t* pValue)
{
	int64_t iValue = 0;
	size_t i = 0;
	if ( !sText || !sText[0] || !pValue ) { return false; }
	while ( sText[i] ) {
		if ( sText[i] < '0' || sText[i] > '9' ) { return false; }
		if ( iValue > ((INT64_MAX - (int64_t)(sText[i] - '0')) / 10) ) { return false; }
		iValue = (iValue * 10) + (int64_t)(sText[i] - '0');
		i++;
	}
	*pValue = iValue;
	return true;
}


// 内部函数：__xcodecHttpParseHexU64
static bool __xcodecHttpParseHexU64(const char* sText, size_t iLen, uint64* pValue)
{
	uint64 iValue = 0;
	bool bHasDigit = false;
	size_t i = 0;
	if ( !sText || !pValue ) { return false; }
	// 跳过前导空白
	while ( i < iLen && (sText[i] == ' ' || sText[i] == '\t') ) { i++; }
	// 跳过尾部空白
	while ( iLen > i && (sText[iLen - 1u] == ' ' || sText[iLen - 1u] == '\t') ) { iLen--; }
	if ( i >= iLen ) { return false; }
	for ( ; i < iLen; ++i ) {
		uint32 iDigit;
		char ch = sText[i];
		// 分号后为 chunk 扩展参数，停止解析
		if ( ch == ';' ) { break; }
		if ( ch >= '0' && ch <= '9' ) { iDigit = (uint32)(ch - '0'); }
		else if ( ch >= 'a' && ch <= 'f' ) { iDigit = 10u + (uint32)(ch - 'a'); }
		else if ( ch >= 'A' && ch <= 'F' ) { iDigit = 10u + (uint32)(ch - 'A'); }
		// 空白后只允许分号或结尾
		else if ( ch == ' ' || ch == '\t' ) {
			while ( i < iLen && (sText[i] == ' ' || sText[i] == '\t') ) { i++; }
			if ( i < iLen && sText[i] != ';' ) { return false; }
			break;
		} else {
			return false;
		}
		// 溢出检查
		if ( iValue > (UINT64_MAX - iDigit) / 16u ) { return false; }
		iValue = (iValue * 16u) + iDigit;
		bHasDigit = true;
	}
	if ( !bHasDigit ) { return false; }
	*pValue = iValue;
	return true;
}


// 获取编解码器 HTTP/1 头部
XXAPI const char* xrtCodecHttp1GetHeader(const xcodechttp1msg* pMsg, const char* sName)
{
	if ( !pMsg || !sName ) { return NULL; }
	for ( uint32 i = 0; i < pMsg->iHeaderCount; ++i ) {
		if ( __xcodecHttpStrEqNoCase(pMsg->arrHeaders[i].sName, sName) ) {
			return pMsg->arrHeaders[i].sValue;
		}
	}
	return NULL;
}


// 初始化编解码器 HTTP/1 消息
XXAPI void xrtCodecHttp1MessageInit(xcodechttp1msg* pMsg)
{
	if ( !pMsg ) { return; }
	memset(pMsg, 0, sizeof(xcodechttp1msg));
	pMsg->iContentLength = -1;
}


// 内部函数：读取 HTTP chunk 头部
static xcodecstatus __xcodecHttpReadChunkHeader(const xnetchain* pInput, size_t iOffset, uint64* pChunkSize, size_t* pDataOffset)
{
	static const uint8 aCrlf[] = { '\r', '\n' };
	size_t iLineEnd;
	size_t iLineLen;
	char* sLine;
	bool bOk;
	if ( !pInput || !pChunkSize ) { return XCODEC_STATUS_ERROR; }
	iLineEnd = __xcodecChainFindPattern(pInput, aCrlf, sizeof(aCrlf), iOffset);
	if ( iLineEnd == (size_t)-1 ) { return XCODEC_STATUS_NEED_MORE; }
	iLineLen = iLineEnd - iOffset;
	sLine = (char*)XNET_ALLOC(iLineLen + 1u);
	if ( !sLine ) { return XCODEC_STATUS_ERROR; }
	if ( __xcodecChainPeekAt(pInput, iOffset, sLine, iLineLen) != iLineLen ) {
		XNET_FREE(sLine);
		return XCODEC_STATUS_ERROR;
	}
	sLine[iLineLen] = '\0';
	bOk = __xcodecHttpParseHexU64(sLine, iLineLen, pChunkSize);
	XNET_FREE(sLine);
	if ( !bOk ) { return XCODEC_STATUS_ERROR; }
	if ( pDataOffset ) { *pDataOffset = iLineEnd + sizeof(aCrlf); }
	return XCODEC_STATUS_FRAME;
}


// 内部函数：__xcodecHttpMeasureChunkedBody
static xcodecstatus __xcodecHttpMeasureChunkedBody(const xnetchain* pInput, size_t iBodyOffset, size_t* pChunkBodyBytes, size_t* pDecodedBytes)
{
	static const uint8 aCrlf[] = { '\r', '\n' };
	static const uint8 aTrailerEnd[] = { '\r', '\n', '\r', '\n' };
	size_t iPos;
	size_t iDecoded = 0u;
	if ( !pInput || !pChunkBodyBytes || !pDecodedBytes ) { return XCODEC_STATUS_ERROR; }
	iPos = iBodyOffset;
	// 逐个解析 chunk 直到遇到终止 chunk（大小为 0）
	for ( ;; ) {
		uint64 iChunkSize = 0u;
		size_t iDataOffset = 0u;
		size_t iChunkBytes;
		xcodecstatus iChunkHdr = __xcodecHttpReadChunkHeader(pInput, iPos, &iChunkSize, &iDataOffset);
		size_t iAvailBytes;
		if ( iChunkHdr != XCODEC_STATUS_FRAME ) { return iChunkHdr; }
		// 终止 chunk：检查后续的 trailer 或空行
		if ( iChunkSize == 0u ) {
			if ( __xcodecChainMatchAt(pInput, iDataOffset, aCrlf, sizeof(aCrlf)) ) {
				*pChunkBodyBytes = (iDataOffset + sizeof(aCrlf)) - iBodyOffset;
				*pDecodedBytes = iDecoded;
				return XCODEC_STATUS_FRAME;
			}
			// 有 trailer 时查找 trailer 结束标记
			iPos = __xcodecChainFindPattern(pInput, aTrailerEnd, sizeof(aTrailerEnd), iDataOffset);
			if ( iPos == (size_t)-1 ) { return XCODEC_STATUS_NEED_MORE; }
			*pChunkBodyBytes = (iPos + sizeof(aTrailerEnd)) - iBodyOffset;
			*pDecodedBytes = iDecoded;
			return XCODEC_STATUS_FRAME;
		}
		// 溢出安全检查
		if ( iChunkSize > (uint64)(SIZE_MAX - iDecoded) ) { return XCODEC_STATUS_ERROR; }
		if ( iChunkSize > (uint64)(SIZE_MAX - iDataOffset - sizeof(aCrlf)) ) { return XCODEC_STATUS_ERROR; }
		// 验证 chunk 数据后有 CRLF
		iChunkBytes = (size_t)iChunkSize;
		iAvailBytes = xrtNetChainBytes(pInput);
		if ( iAvailBytes < iDataOffset + iChunkBytes + sizeof(aCrlf) ) { return XCODEC_STATUS_NEED_MORE; }
		if ( !__xcodecChainMatchAt(pInput, iDataOffset + iChunkBytes, aCrlf, sizeof(aCrlf)) ) { return XCODEC_STATUS_ERROR; }
		// 累加解码字节数并前进到下一个 chunk
		iDecoded += iChunkBytes;
		iPos = iDataOffset + iChunkBytes + sizeof(aCrlf);
	}
}


// 编解码器 HTTP/1 正文 bytes相关处理
XXAPI size_t xrtCodecHttp1BodyBytes(const xcodecframe* pFrame)
{
	if ( !pFrame ) { return 0u; }
	if ( (pFrame->iFlags & XCODEC_FRAME_F_CHUNKED) != 0u ) { return (size_t)pFrame->iMeta0; }
	return pFrame->iPayloadBytes;
}


// 复制编解码器 HTTP/1 正文
XXAPI size_t xrtCodecHttp1CopyBody(const xnetchain* pInput, const xcodecframe* pFrame, ptr pOut, size_t iLen)
{
	uint8* pDst = (uint8*)pOut;
	size_t iWant;
	size_t iCopied = 0u;
	size_t iPos;
	if ( !pInput || !pFrame || !pOut || iLen == 0u ) { return 0u; }
	// 非 chunked 传输直接拷贝
	if ( (pFrame->iFlags & XCODEC_FRAME_F_CHUNKED) == 0u ) {
		return xrtCodecFramePeek(pInput, pFrame, pOut, iLen);
	}
	// chunked 传输需逐个 chunk 解码拷贝
	iWant = xrtCodecHttp1BodyBytes(pFrame);
	if ( iWant > iLen ) { iWant = iLen; }
	iPos = pFrame->iPayloadOffset;
	while ( iCopied < iWant ) {
		uint64 iChunkSize = 0u;
		size_t iDataOffset = 0u;
		size_t iChunkBytes;
		// 读取 chunk 头部获取数据大小和偏移
		if ( __xcodecHttpReadChunkHeader(pInput, iPos, &iChunkSize, &iDataOffset) != XCODEC_STATUS_FRAME ) { break; }
		if ( iChunkSize == 0u ) { break; }
		if ( iChunkSize > (uint64)(SIZE_MAX - iDataOffset - 2u) ) { break; }
		iChunkBytes = (size_t)iChunkSize;
		// 最后一个 chunk 可能只需要部分数据
		if ( iCopied + iChunkBytes > iWant ) { iChunkBytes = iWant - iCopied; }
		// 从链中拷贝 chunk 数据
		if ( iChunkBytes > 0u && __xcodecChainPeekAt(pInput, iDataOffset, pDst + iCopied, iChunkBytes) != iChunkBytes ) { break; }
		iCopied += iChunkBytes;
		if ( iChunkSize > (uint64)(SIZE_MAX - iDataOffset - 2u) ) { break; }
		// 跳过 chunk 数据和尾部 CRLF
		iPos = iDataOffset + (size_t)iChunkSize + 2u;
	}
	return iCopied;
}



/* ============================== HTTP/1 parser ============================== */

XXAPI xcodecstatus xrtCodecHttp1Parse(const xnetchain* pInput, xcodecframe* pFrame, xcodechttp1msg* pMsg)
{
	static const uint8 aHeadEnd[] = { '\r', '\n', '\r', '\n' };
	char* sHeadBuf;
	size_t iHeadEndPos;
	size_t iHeadBytes;
	size_t iBodyBytes = 0;
	char* sCursor;
	char* sLineEnd;
	char* sNextLine;

	if ( !pInput || !pFrame || !pMsg ) { return XCODEC_STATUS_ERROR; }
	xrtCodecFrameInit(pFrame);
	xrtCodecHttp1MessageInit(pMsg);

	// 查找头部结束标记 "\r\n\r\n"
	iHeadEndPos = __xcodecChainFindPattern(pInput, aHeadEnd, sizeof(aHeadEnd), 0);
	if ( iHeadEndPos == (size_t)-1 ) { return XCODEC_STATUS_NEED_MORE; }
	iHeadBytes = iHeadEndPos + sizeof(aHeadEnd);
	// 将整个头部读入连续内存
	sHeadBuf = (char*)XNET_ALLOC(iHeadBytes + 1u);
	if ( !sHeadBuf ) { return XCODEC_STATUS_ERROR; }
	if ( __xcodecChainPeekAt(pInput, 0, sHeadBuf, iHeadBytes) != iHeadBytes ) {
		XNET_FREE(sHeadBuf);
		return XCODEC_STATUS_ERROR;
	}
	sHeadBuf[iHeadBytes] = '\0';

	// 提取首行（请求行或状态行）
	sLineEnd = strstr(sHeadBuf, "\r\n");
	if ( !sLineEnd ) {
		XNET_FREE(sHeadBuf);
		return XCODEC_STATUS_ERROR;
	}
	*sLineEnd = '\0';

	// 判断是响应还是请求
	if ( strncmp(sHeadBuf, "HTTP/", 5) == 0 ) {
		// 解析响应状态行：HTTP/版本 状态码 原因短语
		char* sVersion = sHeadBuf;
		char* sStatus = strchr(sVersion, ' ');
		char* sReason = NULL;
		if ( !sStatus ) {
			XNET_FREE(sHeadBuf);
			return XCODEC_STATUS_ERROR;
		}
		*sStatus++ = '\0';
		while ( *sStatus == ' ' ) { sStatus++; }
		sReason = strchr(sStatus, ' ');
		if ( sReason ) {
			*sReason++ = '\0';
			while ( *sReason == ' ' ) { sReason++; }
		}
		// 解析并验证状态码
		{
			char* pStatusEnd = NULL;
			long iStatusCode = strtol(sStatus, &pStatusEnd, 10);
			if ( pStatusEnd == sStatus || *pStatusEnd != '\0' || iStatusCode < 100 || iStatusCode > 999 ) {
				XNET_FREE(sHeadBuf);
				return XCODEC_STATUS_ERROR;
			}
			pMsg->iStatusCode = (uint32)iStatusCode;
		}
		pMsg->iFlags |= XCODEC_HTTP1_F_RESPONSE;
		pFrame->iFlags |= XCODEC_FRAME_F_RESPONSE;
		__xcodecHttpCopyToken(pMsg->sVersion, sizeof(pMsg->sVersion), sVersion, strlen(sVersion));
		if ( sReason ) { __xcodecHttpCopyToken(pMsg->sReason, sizeof(pMsg->sReason), sReason, strlen(sReason)); }
	} else {
		// 解析请求行：方法 目标 HTTP/版本
		char* sMethod = sHeadBuf;
		char* sTarget = strchr(sMethod, ' ');
		char* sVersion = NULL;
		if ( !sTarget ) {
			XNET_FREE(sHeadBuf);
			return XCODEC_STATUS_ERROR;
		}
		*sTarget++ = '\0';
		while ( *sTarget == ' ' ) { sTarget++; }
		sVersion = strchr(sTarget, ' ');
		if ( !sVersion ) {
			XNET_FREE(sHeadBuf);
			return XCODEC_STATUS_ERROR;
		}
		*sVersion++ = '\0';
		while ( *sVersion == ' ' ) { sVersion++; }
		pMsg->iFlags |= XCODEC_HTTP1_F_REQUEST;
		pFrame->iFlags |= XCODEC_FRAME_F_REQUEST;
		__xcodecHttpCopyToken(pMsg->sMethod, sizeof(pMsg->sMethod), sMethod, strlen(sMethod));
		__xcodecHttpCopyToken(pMsg->sTarget, sizeof(pMsg->sTarget), sTarget, strlen(sTarget));
		__xcodecHttpCopyToken(pMsg->sVersion, sizeof(pMsg->sVersion), sVersion, strlen(sVersion));
	}

	// 逐行解析头部字段
	sCursor = sLineEnd + 2;
	while ( sCursor < (sHeadBuf + iHeadBytes - 2u) ) {
		char* sColon;
		const char* sName;
		const char* sValue;
		size_t iNameLen;
		size_t iValueLen;

		if ( sCursor[0] == '\r' && sCursor[1] == '\n' ) { break; }
		sNextLine = strstr(sCursor, "\r\n");
		if ( !sNextLine ) { break; }
		*sNextLine = '\0';
		// 以冒号分隔头部名称和值
		sColon = strchr(sCursor, ':');
		if ( !sColon ) {
			XNET_FREE(sHeadBuf);
			return XCODEC_STATUS_ERROR;
		}
		*sColon = '\0';
		sName = sCursor;
		sValue = sColon + 1;
		iNameLen = strlen(__xrt_cstr(sName));
		iValueLen = strlen(sValue);
		// 去除头部名称和值的前后空白
		__xcodecHttpTrimView(&sName, &iNameLen);
		__xcodecHttpTrimView(&sValue, &iValueLen);

		if ( pMsg->iHeaderCount < XCODEC_HTTP1_MAX_HEADERS ) {
			xcodechttp1header* pHeader = &pMsg->arrHeaders[pMsg->iHeaderCount++];
			__xcodecHttpCopyToken(pHeader->sName, sizeof(pHeader->sName), sName, iNameLen);
			__xcodecHttpCopyToken(pHeader->sValue, sizeof(pHeader->sValue), sValue, iValueLen);

			// 提取关键头部语义信息
			if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Content-Length") ) {
				(void)__xcodecHttpParseInt64(pHeader->sValue, &pMsg->iContentLength);
			} else if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Transfer-Encoding") ) {
				if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "chunked") ) {
					pMsg->iFlags |= XCODEC_HTTP1_F_CHUNKED;
					pFrame->iFlags |= XCODEC_FRAME_F_CHUNKED;
				}
			} else if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Connection") ) {
				if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "upgrade") ) {
					pMsg->iFlags |= XCODEC_HTTP1_F_UPGRADE;
					pFrame->iFlags |= XCODEC_FRAME_F_UPGRADE;
				}
				if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "close") ) {
					pMsg->iFlags &= ~XCODEC_HTTP1_F_KEEPALIVE;
					pFrame->iFlags &= ~XCODEC_FRAME_F_KEEPALIVE;
				} else if ( __xcodecHttpContainsTokenNoCase(pHeader->sValue, "keep-alive") ) {
					pMsg->iFlags |= XCODEC_HTTP1_F_KEEPALIVE;
					pFrame->iFlags |= XCODEC_FRAME_F_KEEPALIVE;
				}
			} else if ( __xcodecHttpStrEqNoCase(pHeader->sName, "Upgrade") ) {
				if ( pHeader->sValue[0] != '\0' ) {
					pMsg->iFlags |= XCODEC_HTTP1_F_UPGRADE;
					pFrame->iFlags |= XCODEC_FRAME_F_UPGRADE;
				}
			}
		}

		sCursor = sNextLine + 2;
	}

	// HTTP/1.1 默认 keep-alive
	if ( __xcodecHttpContainsTokenNoCase(pMsg->sVersion, "HTTP/1.1") &&
		((pFrame->iFlags & XCODEC_FRAME_F_KEEPALIVE) == 0) ) {
		const char* sConn = xrtCodecHttp1GetHeader(pMsg, "Connection");
		if ( !sConn || !__xcodecHttpContainsTokenNoCase(sConn, "close") ) {
			pMsg->iFlags |= XCODEC_HTTP1_F_KEEPALIVE;
			pFrame->iFlags |= XCODEC_FRAME_F_KEEPALIVE;
		}
	}

	// 记录头部字节数
	pMsg->iHeadBytes = iHeadBytes;
	pFrame->iKind = XCODEC_KIND_HTTP1;
	pFrame->iHeaderBytes = iHeadBytes;
	pFrame->iPayloadOffset = iHeadBytes;

	// 处理 chunked 传输编码的消息体
	if ( pMsg->iFlags & XCODEC_HTTP1_F_CHUNKED ) {
		size_t iChunkBodyBytes = 0u;
		size_t iDecodedBytes = 0u;
		xcodecstatus iChunkParse = __xcodecHttpMeasureChunkedBody(pInput, iHeadBytes, &iChunkBodyBytes, &iDecodedBytes);
		if ( iChunkParse != XCODEC_STATUS_FRAME ) {
			XNET_FREE(sHeadBuf);
			return iChunkParse;
		}
		pFrame->iPayloadBytes = 0;
		pFrame->iFrameBytes = iHeadBytes + iChunkBodyBytes;
		pFrame->iMeta0 = (uint64)iDecodedBytes;
		pFrame->iMeta1 = (uint64)iChunkBodyBytes;
		XNET_FREE(sHeadBuf);
		return XCODEC_STATUS_FRAME;
	}

	// 处理 Content-Length 定长消息体
	if ( pMsg->iContentLength > 0 ) {
		iBodyBytes = (size_t)pMsg->iContentLength;
	}
	pFrame->iPayloadBytes = iBodyBytes;
	pFrame->iFrameBytes = iHeadBytes + iBodyBytes;
	// 检查消息体是否已完整接收
	if ( xrtNetChainBytes(pInput) < pFrame->iFrameBytes ) {
		XNET_FREE(sHeadBuf);
		return XCODEC_STATUS_NEED_MORE;
	}

	XNET_FREE(sHeadBuf);
	return XCODEC_STATUS_FRAME;
}


#endif
