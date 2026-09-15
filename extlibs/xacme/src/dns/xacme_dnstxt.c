#include "../internal/xacme_dnstxt.h"

#if defined(XACME_FEATURE_ACME_DNS)

#include <xrt/buffer.h>
#include <xrt/math.h>
#include <xrt/net.h>
#include <xrt/random.h>
#include <xrt/time.h>
#include <xrt/udp.h>

#include <string.h>

static void xacmeTxtError(
	xerrkind Kind, xacmednstxterror Code, cstr sMessage)
{
	xrtSetErrorInfo(Kind, "xrt.acme.dns.txt", (int32)Code, sMessage);
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
		if((pDns->pEngine == NULL) || !xrtNetEngineStart(pDns->pEngine))
		{
			pDns->pEngine = NULL;
			xacmeTxtError(
				XERR_STATE, XACME_TXT_ERROR_NETWORK,
				"acme dns engine start failed");
			return false;
		}
		pDns->bEngineOwned = true;
	}
	return true;
}

void xacmeDnsUnit(xacmedns* pDns)
{
	if(pDns == NULL)
	{
		return;
	}
	if(pDns->bEngineOwned && (pDns->pEngine != NULL))
	{
		(void)xrtNetEngineStop(pDns->pEngine);
		(void)xrtNetEngineDestroy(pDns->pEngine);
	}
	memset(pDns, 0, sizeof(*pDns));
}

/* 组装 QNAME：点分文本 → DNS 标签序列（含末尾根零）。 */
static bool xacmeTxtEncodeName(xbuffer* pOut, cstr sFqdn)
{
	const char* s = sFqdn;
	if((sFqdn == NULL) || (sFqdn[0] == '\0'))
	{
		return false;
	}
	while(*s != '\0')
	{
		const char* sDot = strchr(s, '.');
		size_t iLabel =
			(sDot != NULL) ? (size_t)(sDot - s) : strlen(s);
		if((iLabel == 0u) || (iLabel > 63u))
		{
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
	size_t i = iAt;
	size_t iGuard = 0u;
	for(;;)
	{
		uint8 iLen;
		if(i >= iSize)
		{
			return 0u;
		}
		iLen = p[i];
		if((iLen & 0xC0u) == 0xC0u)
		{
			return (i + 2u <= iSize) ? (i + 2u - iAt) : 0u;
		}
		if((iLen & 0xC0u) != 0u)
		{
			return 0u;
		}
		i += 1u + iLen;
		if(iLen == 0u)
		{
			return i - iAt;
		}
		if((++iGuard) > 128u)
		{
			return 0u;
		}
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
bool xacmeTxtParseResponse(
	const uint8* p, size_t iSize, uint16 uExpectId,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity,
	size_t* pOutCount)
{
	size_t iAt;
	uint16 iQuestions;
	uint16 iAnswers;
	uint16 iFlags;

	*pOutCount = 0u;
	if((p == NULL) || (iSize < 12u) || (sOutRecords == NULL) ||
		(pOutCount == NULL) || (iCapacity == 0u))
	{
		return false;
	}
	if((p[0] != (uint8)(uExpectId >> 8u)) ||
		(p[1] != (uint8)uExpectId))
	{
		return false;
	}
	iFlags = (uint16)(((uint16)p[2] << 8u) | p[3]);
	if((iFlags & 0x8000u) == 0u)
	{
		return false;
	}
	if((iFlags & 0x000Fu) != 0u)
	{
		return true; /* NXDOMAIN 等：无记录，成功返回零。 */
	}
	iAt = 4u;
	if(!xacmeTxtReadU16(p, iSize, &iAt, &iQuestions) ||
		!xacmeTxtReadU16(p, iSize, &iAt, &iAnswers))
	{
		return false;
	}
	iAt = 12u;
	for(; iQuestions > 0u; iQuestions--)
	{
		size_t iSkip = xacmeTxtSkipName(p, iSize, iAt);
		if((iSkip == 0u) || ((iAt + iSkip + 4u) > iSize))
		{
			return false;
		}
		iAt += iSkip + 4u;
	}
	for(; iAnswers > 0u; iAnswers--)
	{
		uint16 iType;
		uint16 iClass;
		uint16 iRdLength;
		size_t iRdAt;
		size_t iSkip = xacmeTxtSkipName(p, iSize, iAt);
		if(iSkip == 0u)
		{
			return false;
		}
		iAt += iSkip;
		if(!xacmeTxtReadU16(p, iSize, &iAt, &iType) ||
			!xacmeTxtReadU16(p, iSize, &iAt, &iClass) ||
			(iAt + 4u > iSize))
		{
			return false;
		}
		iAt += 4u; /* TTL */
		if(!xacmeTxtReadU16(p, iSize, &iAt, &iRdLength) ||
			((iAt + iRdLength) > iSize))
		{
			return false;
		}
		iRdAt = iAt;
		iAt += iRdLength;
		if((iType != 0x0010u) || (iClass != 0x0001u) ||
			(*pOutCount >= iCapacity))
		{
			continue;
		}
		/* TXT rdata = 若干 character-string，拼接为一条记录值。 */
		{
			size_t iUsed = 0u;
			char* sRecord = sOutRecords[*pOutCount];
			while(iRdAt < iAt)
			{
				uint8 iStrLen = p[iRdAt];
				iRdAt += 1u;
				if((iStrLen == 0u) || ((iRdAt + iStrLen) > iAt) ||
					((iUsed + iStrLen) >= XACME_TXT_RECORD_MAX))
				{
					return false;
				}
				memcpy(sRecord + iUsed, p + iRdAt, iStrLen);
				iUsed += iStrLen;
				iRdAt += iStrLen;
			}
			sRecord[iUsed] = '\0';
			(*pOutCount)++;
		}
	}
	return true;
}

bool xacmeDnsTxtQuery(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX], size_t iCapacity,
	size_t* pOutCount)
{
	xbuffer Query;
	xnetaddr Peer;
	xnetudp* pUdp = NULL;
	xnetudppacket* pPacket = NULL;
	uint16 iId;
	const uint8* p;
	size_t iSize;
	bool bOk = false;

	if((pDns == NULL) || (sResolver == NULL) || (sFqdn == NULL) ||
		(sOutRecords == NULL) || (pOutCount == NULL) || (iCapacity == 0u))
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt query requires dns, resolver, fqdn and outputs");
		return false;
	}
	*pOutCount = 0u;

	xrtBufferInit(&Query);
	/* 探测仅咨询性（失败不阻断），但事务 ID 仍用密码学随机，
	   降低在路径攻击者伪造应答提前放行传播门的概率。 */
	{
		uint8 uSecure[2];
		if(!xrtSecureRandom(uSecure, sizeof(uSecure)))
		{
			xacmeTxtError(
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
			xacmeTxtError(
				XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
				"acme dns txt query fqdn invalid");
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
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt resolver address invalid");
		goto Done;
	}
	pUdp = xrtNetUdpConnect(pDns->pEngine, &Peer, 0u, NULL, NULL, NULL);
	if(pUdp == NULL)
	{
		xacmeTxtError(
			XERR_IO, XACME_TXT_ERROR_NETWORK,
			"acme dns txt udp open failed");
		goto Done;
	}
	if(xrtNetUdpSend(pUdp, Query.Data, Query.Size) != XNET_RESULT_OK)
	{
		xacmeTxtError(
			XERR_IO, XACME_TXT_ERROR_NETWORK,
			"acme dns txt udp send failed");
		goto Done;
	}
	{
		int iAttempt;
		bool bGot = false;
		for(iAttempt = 0; (iAttempt < 2) && !bGot; iAttempt++)
		{
			pPacket = xrtNetUdpReceiveWait(
				pUdp, xrtClock() + UINT64_C(2000000), NULL);
			if(pPacket == NULL)
			{
				if(iAttempt == 1)
				{
					xacmeTxtError(
						XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
						"acme dns txt udp receive timeout");
					goto Done;
				}
				(void)xrtNetUdpSend(pUdp, Query.Data, Query.Size);
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
	if(!xacmeTxtParseResponse(p, iSize, iId, sOutRecords, iCapacity,
			pOutCount))
	{
		if(xrtGetError() == NULL)
		{
			goto Protocol;
		}
		goto Done;
	}
	bOk = true;

Done:
	if(pPacket != NULL)
	{
		xrtNetUdpPacketDestroy(pPacket);
	}
	if(pUdp != NULL)
	{
		xrtNetUdpDestroy(pUdp);
	}
	xrtBufferUnit(&Query);
	return bOk;

Protocol:
	xacmeTxtError(
		XERR_PROTOCOL, XACME_TXT_ERROR_PROTOCOL,
		"acme dns txt response malformed");
	goto Done;
}

bool xacmeDnsTxtWait(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	cstr sExpected, uint64 uTimeoutMs)
{
	uint64 uDeadline = xrtClock() + uTimeoutMs * 1000u;
	if((sExpected == NULL) || (sExpected[0] == '\0'))
	{
		xacmeTxtError(
			XERR_ARGUMENT, XACME_TXT_ERROR_ARGUMENT,
			"acme dns txt wait requires expected value");
		return false;
	}
	while(xrtClock() < uDeadline)
	{
		char sRecords[4][XACME_TXT_RECORD_MAX];
		size_t iCount = 0u;
		size_t i;
		if(!xacmeDnsTxtQuery(
			pDns, sResolver, iPort, sFqdn, sRecords, 4u, &iCount))
		{
			return false;
		}
		for(i = 0; i < iCount; i++)
		{
			if(strcmp(sRecords[i], sExpected) == 0)
			{
				return true;
			}
		}
		xrtSleep(1000u);
	}
	xacmeTxtError(
		XERR_TIMEOUT, XACME_TXT_ERROR_TIMEOUT,
		"acme dns txt wait exhausted");
	return false;
}

#endif
