#include <xrt/detail/wait.h>
#include "../internal/xacme_dnstxt.h"

#if defined(XACME_FEATURE_ACME_DNS)

#include <xrt/buffer.h>
#include <xrt/math.h>
#include <xrt/net.h>
#include <xrt/random.h>
#include <xrt/time.h>
#include <xrt/thread.h>
#include <xrt/udp.h>

#include <string.h>

static void xacmeTxtError(
	xerrkind Kind, xacmednstxterror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns.txt", (int32)Code, sMessage);
}

/* Resource exhaustion is terminal and retains the actual allocator cause.
 * Other failures keep the DNS probe's stable protocol/network diagnostics. */
static void xacmeTxtFailure(
	xerrkind Kind, xacmednstxterror Code, cstr sMessage)
{
	if(xrtErrorKind(xrtGetError()) != XERR_MEMORY)
		xacmeTxtError(Kind, Code, sMessage);
}

bool xacmeDnsInit(xacmedns* pDns, struct xnetengine* pBorrowedEngine)
{
	if(pDns == NULL)
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns init requires dns");
		return false;
	}
	memset(pDns, 0, sizeof(*pDns));
	if(pBorrowedEngine != NULL)
	{
		pDns->pEngine = pBorrowedEngine;
		return true;
	}
	{
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pDns->pEngine = xrtNetEngineCreate(&Engine);
		if(pDns->pEngine == NULL) return false;
		pDns->bEngineOwned = true;
		if(!xrtNetEngineStart(pDns->pEngine))
		{
			(void)xacmeDnsUnit(pDns);
			return false;
		}
	}
	return true;
}

bool xacmeDnsUnit(xacmedns* pDns)
{
	xerror* pPrevious;
	bool bReady = true;
	if(pDns == NULL)
	{
		return true;
	}
	pPrevious = xrtErrorRef(xrtGetError());
	if(pDns->bEngineOwned && (pDns->pEngine != NULL))
	{
		double Deadline = __xrtWaitAfter(INT64_C(30000));
		for(;;)
		{
			xnetretireresult Result = xrtNetEngineTryDestroy(pDns->pEngine);
			if(Result == XNET_RETIRE_READY) break;
			if(Result == XNET_RETIRE_ERROR) { bReady = false; break; }
			if(__xrtWaitExpired(Deadline))
			{
				xacmeTxtError(XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
					"acme dns engine still has live objects during cleanup");
				bReady = false;
				break;
			}
			xrtSleep(1u);
		}
	}
	if(bReady) memset(pDns, 0, sizeof(*pDns));
	if(pPrevious != NULL) xrtSetErrorTake(pPrevious);
	return bReady;
}

/* 组装 QNAME：点分文本 → DNS 标签序列（含末尾根零）。 */
static bool xacmeTxtEncodeName(xbuffer* pOut, cstr sFqdn)
{
	const char* s = sFqdn;
	if((sFqdn == NULL) || (sFqdn[0] == '\0'))
	{
		xacmeTxtError(XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt query fqdn invalid");
		return false;
	}
	while(*s != '\0')
	{
		const char* sDot = strchr(s, '.');
		size_t iLabel =
			(sDot != NULL) ? (size_t)(sDot - s) : strlen(s);
		if((iLabel == 0u) || (iLabel > 63u))
		{
			xacmeTxtError(XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
				"acme dns txt query fqdn invalid");
			return false;
		}
		if(!xrtBufferAppendByte(pOut, (uint8)iLabel) ||
			!xrtBufferAppend(
				pOut, (xbytesview){ (const uint8*)s, iLabel }))
		{
			return false;
		}
		s += iLabel + ((sDot != NULL) ? 1u : 0u);
	}
	return xrtBufferAppendByte(pOut, 0u);
}

/* 跳过响应中的（可能压缩的）name；返回消费字节数，0 表示非法。 */
static size_t xacmeTxtSkipName(const uint8* p, size_t iSize, size_t iAt)
{
	size_t i = iAt, iConsumed = 0u, iExpanded = 0u, iGuard = 0u;
	bool bJumped = false;
	for(;;)
	{
		uint8 iLen;
		if(i >= iSize || ++iGuard > iSize) return 0u;
		iLen = p[i];
		if((iLen & 0xC0u) == 0xC0u)
		{
			size_t iTarget;
			if(iSize - i < 2u) return 0u;
			iTarget = ((size_t)(iLen & 0x3Fu) << 8u) | p[i + 1u];
			/* Compression refers to a previous name, never this pointer,
			 * the header or an out-of-packet/forward offset. */
			if(iTarget < 12u || iTarget >= i) return 0u;
			if(!bJumped) iConsumed = i + 2u - iAt;
			bJumped = true;
			i = iTarget;
			continue;
		}
		if((iLen & 0xC0u) != 0u || iExpanded > 255u - 1u - iLen ||
			(size_t)iLen > iSize - i - 1u) return 0u;
		iExpanded += 1u + iLen;
		i += 1u + iLen;
		if(iLen == 0u) return bJumped ? iConsumed : i - iAt;
	}
}

static bool xacmeTxtReadU16(
	const uint8* p, size_t iSize, size_t* pAt, uint16* pOut)
{
	if((*pAt + 2u) > iSize)
	{
		return false;
	}
	*pOut = (uint16)(((uint16)p[*pAt] << 8u) | p[*pAt + 1]);
	*pAt += 2u;
	return true;
}

/*
	解析完整 DNS 响应报文为 TXT 记录集合（不可信网络输入的唯一
	消化口，fuzz 目标）。QR 位缺失/结构损坏 → false；RCODE 非零
	（NXDOMAIN 等）→ true 且零记录。记录值拼接 character-string，
	每条以零结尾且严格小于 XACME_TXT_RECORD_MAX。
*/
static bool xacmeTxtParse(
	const uint8* p, size_t iSize, uint16 uExpectId,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity,
	size_t* pOutCount, xbytesview Expected, bool* pFound)
{
	size_t iAt, iCount = 0u;
	uint16 iQuestions, iAnswers, iAuthority, iAdditional, iFlags;
	uint32 iRecords, iRecord;
	bool bFound = false, bPositive;
	if(pOutCount != NULL) *pOutCount = 0u;
	if(pFound != NULL) *pFound = false;
	if(p == NULL || iSize < 12u || (pOutCount == NULL && pFound == NULL) ||
		(pOutCount != NULL && (sOutRecords == NULL || iCapacity == 0u)) ||
		(pFound != NULL && Expected.Data == NULL && Expected.Size != 0u)) return false;
	if(p[0] != (uint8)(uExpectId >> 8u) || p[1] != (uint8)uExpectId) return false;
	iFlags = (uint16)(((uint16)p[2] << 8u) | p[3]);
	if((iFlags & 0x8000u) == 0u || (iFlags & 0x7800u) != 0u ||
		(iFlags & 0x0200u) != 0u) return false;
	bPositive = (iFlags & 0x000Fu) == 0u;
	iAt = 4u;
	if(!xacmeTxtReadU16(p, iSize, &iAt, &iQuestions) ||
		!xacmeTxtReadU16(p, iSize, &iAt, &iAnswers) ||
		!xacmeTxtReadU16(p, iSize, &iAt, &iAuthority) ||
		!xacmeTxtReadU16(p, iSize, &iAt, &iAdditional)) return false;
	for(; iQuestions > 0u; iQuestions--)
	{
		size_t iSkip = xacmeTxtSkipName(p, iSize, iAt);
		if(iSkip == 0u || iSkip > iSize - iAt || iSize - iAt - iSkip < 4u) return false;
		iAt += iSkip + 4u;
	}
	iRecords = (uint32)iAnswers + (uint32)iAuthority + (uint32)iAdditional;
	for(iRecord = 0u; iRecord < iRecords; iRecord++)
	{
		uint16 iType, iClass, iRdLength;
		size_t iRdAt, iSkip = xacmeTxtSkipName(p, iSize, iAt);
		if(iSkip == 0u) return false;
		iAt += iSkip;
		if(!xacmeTxtReadU16(p, iSize, &iAt, &iType) ||
			!xacmeTxtReadU16(p, iSize, &iAt, &iClass) || iSize - iAt < 4u) return false;
		iAt += 4u;
		if(!xacmeTxtReadU16(p, iSize, &iAt, &iRdLength) || (size_t)iRdLength > iSize - iAt) return false;
		iRdAt = iAt;
		iAt += iRdLength;
		if(iType != 0x0010u) continue;
		/* TXT contains one or more binary character-strings. Empty strings
		 * are legal; an empty RDATA or an overrun is not. Validate tails even
		 * when the text output is full or a previous record already matched. */
		{
			size_t iUsed = 0u;
			bool bAnswer = bPositive && iRecord < iAnswers && iClass == 0x0001u;
			bool bCopy = bAnswer && pOutCount != NULL && iCount < iCapacity;
			bool bEqual = bAnswer && pFound != NULL;
			if(iRdLength == 0u) return false;
			while(iRdAt < iAt)
			{
				uint8 iStrLen = p[iRdAt++];
				if((size_t)iStrLen > iAt - iRdAt) return false;
				if(bCopy && ((size_t)iStrLen >= XACME_TXT_RECORD_MAX - iUsed ||
					memchr(p + iRdAt, 0, iStrLen) != NULL)) return false;
				if(bEqual && (iUsed > Expected.Size || (size_t)iStrLen > Expected.Size - iUsed ||
					(iStrLen != 0u && memcmp(Expected.Data + iUsed, p + iRdAt, iStrLen) != 0))) bEqual = false;
				if(bCopy && iStrLen != 0u) memcpy(sOutRecords[iCount] + iUsed, p + iRdAt, iStrLen);
				iUsed += iStrLen;
				iRdAt += iStrLen;
			}
			if(bCopy) { sOutRecords[iCount][iUsed] = '\0'; iCount++; }
			if(bEqual && iUsed == Expected.Size) bFound = true;
		}
	}
	if(iAt != iSize) return false;
	if(pOutCount != NULL) *pOutCount = iCount;
	if(pFound != NULL) *pFound = bFound;
	return true;
}

bool xacmeTxtParseResponse(
	const uint8* p, size_t iSize, uint16 uExpectId,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity, size_t* pOutCount)
{
	if(pOutCount == NULL) return false;
	return xacmeTxtParse(p, iSize, uExpectId, sOutRecords, iCapacity, pOutCount,
		(xbytesview){ NULL, 0u }, NULL);
}

bool xacmeTxtResponseContains(
	const uint8* p, size_t iSize, uint16 uExpectId, xbytesview Expected, bool* pFound)
{
	if(pFound == NULL) return false;
	return xacmeTxtParse(p, iSize, uExpectId, NULL, 0u, NULL, Expected, pFound);
}

static bool xacmeDnsTxtExchange(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity,
	size_t* pOutCount, cstr sExpected, bool* pFound)
{
	xbuffer Query;
	xnetaddr Peer;
	xnetudp* pUdp = NULL;
	xnetudppacket* pPacket = NULL;
	uint16 iId;
	const uint8* p;
	size_t iSize;
	bool bOk = false;

	if(pOutCount != NULL) *pOutCount = 0u;
	if(pFound != NULL) *pFound = false;
	if((pDns == NULL) || (pDns->pEngine == NULL) || (sResolver == NULL) || (sFqdn == NULL) ||
		(pFound == NULL && (sOutRecords == NULL || pOutCount == NULL || iCapacity == 0u)) ||
		(pFound != NULL && (sExpected == NULL || sExpected[0] == '\0')))
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt query requires dns, resolver, fqdn and outputs");
		return false;
	}
	xrtBufferInit(&Query);
	/* 探测仅咨询性（失败不阻断），但事务 ID 仍用密码学随机，
	   降低在路径攻击者伪造应答提前放行传播门的概率。 */
	{
		uint8 uSecure[2];
		if(!xrtSecureRandom(uSecure, sizeof(uSecure)))
		{
			xacmeTxtFailure(
				XERR_INTERNAL, XACME_TXT_ERROR_NETWORK,
				"acme dns txt secure random failed");
			goto Done;
		}
		iId = (uint16)(((uint16)uSecure[0] << 8u) | uSecure[1]);
	}
	{
		uint8 Head[12] = {
			(uint8)(iId >> 8u), (uint8)iId,
			0x01, 0x00, /* RD=1 */
			0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
		if(!xrtBufferAppend(&Query, (xbytesview){ Head, 12u }) ||
			!xacmeTxtEncodeName(&Query, sFqdn))
		{
			goto Done;
		}
		{
			uint8 Tail[4] = { 0x00, 0x10, 0x00, 0x01 };
			if(!xrtBufferAppend(&Query, (xbytesview){ Tail, 4u }))
			{
				goto Done;
			}
		}
	}

	if(!xrtNetAddrParse(&Peer, sResolver, (iPort != 0u) ? iPort : 53u))
	{
		xacmeTxtFailure(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt resolver address invalid");
		goto Done;
	}
	pUdp = xrtNetUdpConnect(pDns->pEngine, &Peer, 0u, NULL, NULL, NULL);
	if(pUdp == NULL)
	{
		xacmeTxtFailure(
			XERR_IO, XACME_TXT_ERROR_NETWORK,
			"acme dns txt udp open failed");
		goto Done;
	}
	if(xrtNetUdpSend(pUdp, Query.Data, Query.Size) != XNET_RESULT_OK)
	{
		xacmeTxtFailure(
			XERR_IO, XACME_TXT_ERROR_NETWORK,
			"acme dns txt udp send failed");
		goto Done;
	}
	{
		int iAttempt;
		bool bGot = false;
		for(iAttempt = 0; (iAttempt < 2) && !bGot; iAttempt++)
		{
			pPacket = __xrtNetUdpReceiveWait(
				pUdp, xrtTimer() + 2, NULL);
			if(pPacket == NULL)
			{
				if(xrtErrorKind(xrtGetError()) == XERR_MEMORY) goto Done;
				if(iAttempt == 1)
				{
					xacmeTxtError(
						XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
						"acme dns txt udp receive timeout");
					goto Done;
				}
				if(xrtNetUdpSend(pUdp, Query.Data, Query.Size) != XNET_RESULT_OK)
				{
					xacmeTxtFailure(XERR_IO, XACME_TXT_ERROR_NETWORK,
						"acme dns txt udp resend failed");
					goto Done;
				}
				continue;
			}
			if((xrtNetUdpPacketSize(pPacket) >= 12u) &&
				(xrtNetUdpPacketData(pPacket)[0] ==
					(uint8)(iId >> 8u)) &&
				(xrtNetUdpPacketData(pPacket)[1] == (uint8)iId))
			{
				bGot = true;
			}
			else
			{
				xrtNetUdpPacketDestroy(pPacket);
				pPacket = NULL;
			}
		}
		if(!bGot)
		{
			xacmeTxtError(
				XERR_PROTOCOL, XACME_TXT_ERROR_PROTOCOL,
				"acme dns txt response id mismatch");
			goto Done;
		}
	}

	p = xrtNetUdpPacketData(pPacket);
	iSize = xrtNetUdpPacketSize(pPacket);
	if(pFound != NULL ? !xacmeTxtResponseContains(p, iSize, iId,
		(xbytesview){ (const uint8*)sExpected, strlen(sExpected) }, pFound) :
		!xacmeTxtParseResponse(p, iSize, iId, sOutRecords, iCapacity, pOutCount))
	{
		goto Protocol;
	}
	bOk = true;

Done:
	{
		xerror* pPrevious = xrtErrorRef(xrtGetError());
		if(!bOk)
		{
			if(pOutCount != NULL) *pOutCount = 0u;
			if(pFound != NULL) *pFound = false;
		}
		if(pPacket != NULL)
		{
			xrtNetUdpPacketDestroy(pPacket);
		}
		if(pUdp != NULL)
		{
			/* Destroy 只释放调用方引用；不关闭的 UDP 会继续占用引擎。 */
			if(!bOk || !xrtNetUdpClose(pUdp)) (void)xrtNetUdpAbort(pUdp);
			xrtNetUdpDestroy(pUdp);
		}
		xrtBufferUnit(&Query);
		if(pPrevious != NULL) xrtSetErrorTake(pPrevious);
	}
	return bOk;

Protocol:
	xacmeTxtError(
		XERR_PROTOCOL, XACME_TXT_ERROR_PROTOCOL,
		"acme dns txt response malformed");
	goto Done;
}

bool xacmeDnsTxtQuery(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity, size_t* pOutCount)
{
	return xacmeDnsTxtExchange(pDns, sResolver, iPort, sFqdn,
		sOutRecords, iCapacity, pOutCount, NULL, NULL);
}

bool xacmeDnsTxtContains(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	cstr sExpected, bool* pFound)
{
	if(pFound == NULL)
	{
		xacmeTxtError(XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt match requires output");
		return false;
	}
	return xacmeDnsTxtExchange(pDns, sResolver, iPort, sFqdn,
		NULL, 0u, NULL, sExpected, pFound);
}

bool xacmeDnsTxtWait(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	cstr sExpected, int64 uTimeoutMs)
{
	double uDeadline = __xrtWaitAfter(uTimeoutMs);
	if((sExpected == NULL) || (sExpected[0] == '\0'))
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt wait requires expected value");
		return false;
	}
	if (!__xrtWaitValid(uDeadline)) return false;
	while(!__xrtWaitExpired(uDeadline))
	{
		bool bFound = false;
		if(!xacmeDnsTxtContains(
			pDns, sResolver, iPort, sFqdn, sExpected, &bFound))
		{
			return false;
		}
		if(bFound) return true;
		xrtSleep(1000u);
	}
	xacmeTxtError(
		XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
		"acme dns txt wait exhausted");
	return false;
}

#endif
