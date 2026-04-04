#ifndef XRT_EXTLIB_XSMTP_H
#define XRT_EXTLIB_XSMTP_H

#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define XSMTP_SECURE_AUTO      0
#define XSMTP_SECURE_NONE      1
#define XSMTP_SECURE_SSL       2
#define XSMTP_SECURE_STARTTLS  3

#define XSMTP_AUTH_AUTO        0
#define XSMTP_AUTH_NONE        1
#define XSMTP_AUTH_PLAIN       2
#define XSMTP_AUTH_LOGIN       3

#define XSMTP_CAP_AUTH_PLAIN   0x0001u
#define XSMTP_CAP_AUTH_LOGIN   0x0002u
#define XSMTP_CAP_STARTTLS     0x0004u

typedef struct {
	const char* sEmail;
	const char* sName;
} xsmtpaddr;

typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iTimeoutMs;
	int iSecureMode;
	bool bAuth;
	int iAuthMode;
	bool bVerifyPeer;
	const char* sUser;
	const char* sPass;
	const char* sHeloName;
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
} xsmtpconfig;

typedef struct {
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
	const xsmtpaddr* arrTo;
	size_t iToCount;
	const xsmtpaddr* arrCc;
	size_t iCcCount;
	const xsmtpaddr* arrBcc;
	size_t iBccCount;
	const char* sSubject;
	const char* sTextBody;
	const char* sHtmlBody;
	const char* const* arrHeaderNames;
	const char* const* arrHeaderValues;
	size_t iHeaderCount;
} xsmtpmessage;

typedef struct {
	bool bSuccess;
	bool bUsedTLS;
	bool bUsedStartTLS;
	int iServerCode;
	int iAuthMode;
	uint32 iCapabilities;
	char sError[256];
	char sLastReply[1024];
} xsmtpresult;

typedef struct {
	char* sData;
	size_t iLen;
	size_t iCap;
} xsmtpbuf;

typedef struct {
	xsocket hSocket;
	xtlssession* pTls;
	bool bTlsReady;
	bool bOwnSocket;
	uint32 iTimeoutMs;
	char aRecv[32768];
	size_t iRecvLen;
} xsmtpsession;

static void xrtSmtpConfigInit(xsmtpconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	memset(pCfg, 0, sizeof(*pCfg));
	pCfg->iPort = 465;
	pCfg->iTimeoutMs = 15000;
	pCfg->iSecureMode = XSMTP_SECURE_AUTO;
	pCfg->bAuth = TRUE;
	pCfg->iAuthMode = XSMTP_AUTH_AUTO;
	pCfg->bVerifyPeer = TRUE;
}

static void xrtSmtpMessageInit(xsmtpmessage* pMsg)
{
	if ( pMsg == NULL ) {
		return;
	}
	memset(pMsg, 0, sizeof(*pMsg));
}

static void xrtSmtpResultInit(xsmtpresult* pRet)
{
	if ( pRet == NULL ) {
		return;
	}
	memset(pRet, 0, sizeof(*pRet));
}

static const char* xsmtp_str_or_empty(const char* sText)
{
	return sText ? sText : "";
}

static void xsmtp_result_error(xsmtpresult* pRet, const char* sError)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s", xsmtp_str_or_empty(sError));
}

static bool xsmtp_starts_with_i(const char* sText, const char* sPrefix)
{
	size_t i;

	if ( sText == NULL || sPrefix == NULL ) {
		return FALSE;
	}
	for ( i = 0; sPrefix[i] != '\0'; i++ ) {
		unsigned char a = (unsigned char)sText[i];
		unsigned char b = (unsigned char)sPrefix[i];
		if ( a == '\0' ) {
			return FALSE;
		}
		if ( tolower(a) != tolower(b) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_find_i(const char* sText, const char* sSub)
{
	size_t i;
	size_t j;

	if ( sText == NULL || sSub == NULL || sSub[0] == '\0' ) {
		return FALSE;
	}
	for ( i = 0; sText[i] != '\0'; i++ ) {
		for ( j = 0; sSub[j] != '\0'; j++ ) {
			if ( sText[i + j] == '\0' ) {
				return FALSE;
			}
			if ( tolower((unsigned char)sText[i + j]) != tolower((unsigned char)sSub[j]) ) {
				break;
			}
		}
		if ( sSub[j] == '\0' ) {
			return TRUE;
		}
	}
	return FALSE;
}

static bool xsmtp_buf_reserve(xsmtpbuf* pBuf, size_t iNeed)
{
	char* sNew;
	size_t iCap;

	if ( pBuf == NULL ) {
		return FALSE;
	}
	if ( pBuf->iLen + iNeed + 1 <= pBuf->iCap ) {
		return TRUE;
	}
	iCap = pBuf->iCap ? pBuf->iCap : 256;
	while ( iCap < pBuf->iLen + iNeed + 1 ) {
		iCap *= 2;
	}
	sNew = (char*)xrtRealloc(pBuf->sData, iCap);
	if ( sNew == NULL ) {
		return FALSE;
	}
	pBuf->sData = sNew;
	pBuf->iCap = iCap;
	return TRUE;
}

static bool xsmtp_buf_append_n(xsmtpbuf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL || sText == NULL ) {
		return FALSE;
	}
	if ( !xsmtp_buf_reserve(pBuf, iLen) ) {
		return FALSE;
	}
	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return TRUE;
}

static bool xsmtp_buf_append(xsmtpbuf* pBuf, const char* sText)
{
	return xsmtp_buf_append_n(pBuf, xsmtp_str_or_empty(sText), strlen(xsmtp_str_or_empty(sText)));
}

static bool xsmtp_buf_appendf(xsmtpbuf* pBuf, const char* sFormat, ...)
{
	va_list ap;
	va_list ap2;
	int iNeed;
	char* sText;
	bool bOK;

	if ( pBuf == NULL || sFormat == NULL ) {
		return FALSE;
	}

	va_start(ap, sFormat);
	va_copy(ap2, ap);
	iNeed = vsnprintf(NULL, 0, sFormat, ap);
	va_end(ap);
	if ( iNeed < 0 ) {
		va_end(ap2);
		return FALSE;
	}
	sText = (char*)xrtMalloc((size_t)iNeed + 1);
	if ( sText == NULL ) {
		va_end(ap2);
		return FALSE;
	}
	vsnprintf(sText, (size_t)iNeed + 1, sFormat, ap2);
	va_end(ap2);
	if ( sText == NULL ) {
		return FALSE;
	}
	bOK = xsmtp_buf_append(pBuf, sText);
	xrtFree(sText);
	return bOK;
}

static void xsmtp_buf_unit(xsmtpbuf* pBuf)
{
	if ( pBuf && pBuf->sData ) {
		xrtFree(pBuf->sData);
		pBuf->sData = NULL;
		pBuf->iLen = 0;
		pBuf->iCap = 0;
	}
}

static bool xsmtp_has_non_ascii(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;
	if ( sText == NULL ) {
		return FALSE;
	}
	while ( *p ) {
		if ( *p >= 0x80 ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

static bool xsmtp_needs_addr_quote(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;
	if ( sText == NULL || sText[0] == '\0' ) {
		return FALSE;
	}
	while ( *p ) {
		if ( *p < 0x20 || *p == '"' || *p == '\\' || *p == ',' || *p == '<' || *p == '>' || *p == '@' ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

static str xsmtp_encode_header_word(const char* sText)
{
	str sBase64;
	str sOut;
	size_t iNeed;

	if ( sText == NULL ) {
		return xrtCopyStr("", 0);
	}
	if ( !xsmtp_has_non_ascii(sText) ) {
		return xrtCopyStr((str)sText, 0);
	}
	sBase64 = xrtBase64Encode((ptr)sText, strlen(sText), NULL);
	if ( sBase64 == NULL ) {
		return NULL;
	}
	iNeed = strlen(sBase64) + 12;
	sOut = (str)xrtMalloc(iNeed);
	if ( sOut ) {
		snprintf(sOut, iNeed, "=?UTF-8?B?%s?=", sBase64);
	}
	xrtFree(sBase64);
	return sOut;
}

static str xsmtp_format_mailbox(const xsmtpaddr* pAddr)
{
	str sName = NULL;
	str sOut = NULL;
	const char* sEmail;

	if ( pAddr == NULL || pAddr->sEmail == NULL || pAddr->sEmail[0] == '\0' ) {
		return xrtCopyStr("", 0);
	}

	sEmail = pAddr->sEmail;
	if ( pAddr->sName == NULL || pAddr->sName[0] == '\0' ) {
		return xrtCopyStr((str)sEmail, 0);
	}

	sName = xsmtp_encode_header_word(pAddr->sName);
	if ( sName == NULL ) {
		return NULL;
	}

	if ( !xsmtp_has_non_ascii(pAddr->sName) && xsmtp_needs_addr_quote(pAddr->sName) ) {
		sOut = xrtFormat("\"%s\" <%s>", pAddr->sName, sEmail);
	} else {
		sOut = xrtFormat("%s <%s>", sName, sEmail);
	}

	xrtFree(sName);
	return sOut;
}

static str xsmtp_join_mailboxes(const xsmtpaddr* arrList, size_t iCount)
{
	xsmtpbuf tBuf;
	size_t i;

	memset(&tBuf, 0, sizeof(tBuf));
	for ( i = 0; i < iCount; i++ ) {
		str sItem = xsmtp_format_mailbox(&arrList[i]);
		if ( sItem == NULL ) {
			xsmtp_buf_unit(&tBuf);
			return NULL;
		}
		if ( i > 0 ) {
			xsmtp_buf_append(&tBuf, ", ");
		}
		xsmtp_buf_append(&tBuf, sItem);
		xrtFree(sItem);
	}
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr("", 0);
	}
	return tBuf.sData;
}

static str xsmtp_base64_wrap(const void* pData, size_t iLen)
{
	xsmtpbuf tBuf;
	str sBase64;
	size_t i;

	memset(&tBuf, 0, sizeof(tBuf));
	sBase64 = xrtBase64Encode((ptr)pData, iLen, NULL);
	if ( sBase64 == NULL ) {
		return NULL;
	}
	for ( i = 0; sBase64[i] != '\0'; i += 76 ) {
		size_t iChunk = strlen(sBase64 + i);
		if ( iChunk > 76 ) {
			iChunk = 76;
		}
		if ( !xsmtp_buf_append_n(&tBuf, sBase64 + i, iChunk) || !xsmtp_buf_append(&tBuf, "\r\n") ) {
			xrtFree(sBase64);
			xsmtp_buf_unit(&tBuf);
			return NULL;
		}
	}
	xrtFree(sBase64);
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr("", 0);
	}
	return tBuf.sData;
}

static str xsmtp_normalize_crlf(const char* sText)
{
	xsmtpbuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( sText == NULL ) {
		return xrtCopyStr("", 0);
	}

	for ( p = sText; *p; p++ ) {
		if ( *p == '\r' ) {
			if ( p[1] == '\n' ) {
				xsmtp_buf_append(&tBuf, "\r\n");
				p++;
			} else {
				xsmtp_buf_append(&tBuf, "\r\n");
			}
		} else if ( *p == '\n' ) {
			xsmtp_buf_append(&tBuf, "\r\n");
		} else {
			xsmtp_buf_append_n(&tBuf, p, 1);
		}
	}

	if ( tBuf.sData == NULL ) {
		return xrtCopyStr("", 0);
	}
	return tBuf.sData;
}

static bool xsmtp_count_recipients(const xsmtpmessage* pMsg, size_t* pCount)
{
	size_t iTotal = 0;
	if ( pMsg == NULL || pCount == NULL ) {
		return FALSE;
	}
	iTotal += pMsg->iToCount;
	iTotal += pMsg->iCcCount;
	iTotal += pMsg->iBccCount;
	*pCount = iTotal;
	return iTotal > 0;
}

static void xsmtp_random_hex(char* sOut, size_t iOutLen, size_t iByteCount)
{
	uint8 aBuf[32];
	static const char sHex[] = "0123456789abcdef";
	size_t i;

	if ( sOut == NULL || iOutLen == 0 ) {
		return;
	}
	if ( iByteCount > sizeof(aBuf) ) {
		iByteCount = sizeof(aBuf);
	}
	xrtRandomBytes(aBuf, iByteCount);
	for ( i = 0; i < iByteCount && (i * 2 + 1) < iOutLen; i++ ) {
		sOut[i * 2] = sHex[(aBuf[i] >> 4) & 0x0F];
		sOut[i * 2 + 1] = sHex[aBuf[i] & 0x0F];
	}
	sOut[(iByteCount * 2 < iOutLen) ? iByteCount * 2 : iOutLen - 1] = '\0';
}

static str xsmtp_rfc2822_date(void)
{
	time_t tNow;
	struct tm tmNow;
	char sBuf[80];

	time(&tNow);
#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&tmNow, &tNow);
#else
	localtime_r(&tNow, &tmNow);
#endif
	if ( strftime(sBuf, sizeof(sBuf), "%a, %d %b %Y %H:%M:%S %z", &tmNow) == 0 ) {
		return xrtCopyStr("", 0);
	}
	return xrtCopyStr(sBuf, 0);
}

static str xsmtp_build_message_id(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg)
{
	char sRand[33];
	const char* sHost = NULL;
	xsmtp_random_hex(sRand, sizeof(sRand), 16);
	if ( pMsg && pMsg->tFrom.sEmail && strchr(pMsg->tFrom.sEmail, '@') ) {
		sHost = strchr(pMsg->tFrom.sEmail, '@') + 1;
	} else if ( pCfg && pCfg->tFrom.sEmail && strchr(pCfg->tFrom.sEmail, '@') ) {
		sHost = strchr(pCfg->tFrom.sEmail, '@') + 1;
	} else if ( pCfg && pCfg->sHost ) {
		sHost = pCfg->sHost;
	} else {
		sHost = "localhost";
	}
	return xrtFormat("<%s@%s>", sRand, sHost);
}

static str xsmtp_build_body_payload(const char* sBody)
{
	str sNorm;
	str sOut;
	sNorm = xsmtp_normalize_crlf(sBody);
	if ( sNorm == NULL ) {
		return NULL;
	}
	sOut = xsmtp_base64_wrap(sNorm, strlen(sNorm));
	xrtFree(sNorm);
	return sOut;
}

static bool xsmtp_append_body_headers(xsmtpbuf* pBuf, const char* sContentType, const char* sPayload)
{
	return xsmtp_buf_appendf(pBuf,
		"Content-Type: %s; charset=UTF-8\r\n"
		"Content-Transfer-Encoding: base64\r\n"
		"\r\n"
		"%s",
		sContentType, xsmtp_str_or_empty(sPayload));
}

static str xsmtp_build_message_data(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg)
{
	xsmtpbuf tBuf;
	str sDate = NULL;
	str sMessageID = NULL;
	str sSubject = NULL;
	str sFrom = NULL;
	str sTo = NULL;
	str sCc = NULL;
	str sReplyTo = NULL;
	str sTextPayload = NULL;
	str sHtmlPayload = NULL;
	bool bOK = FALSE;

	memset(&tBuf, 0, sizeof(tBuf));

	sDate = xsmtp_rfc2822_date();
	sMessageID = xsmtp_build_message_id(pCfg, pMsg);
	sSubject = xsmtp_encode_header_word(xsmtp_str_or_empty(pMsg->sSubject));
	sFrom = xsmtp_format_mailbox((pMsg->tFrom.sEmail && pMsg->tFrom.sEmail[0]) ? &pMsg->tFrom : &pCfg->tFrom);
	sTo = xsmtp_join_mailboxes(pMsg->arrTo, pMsg->iToCount);
	sCc = xsmtp_join_mailboxes(pMsg->arrCc, pMsg->iCcCount);
	if ( pMsg->tReplyTo.sEmail && pMsg->tReplyTo.sEmail[0] ) {
		sReplyTo = xsmtp_format_mailbox(&pMsg->tReplyTo);
	} else if ( pCfg->tReplyTo.sEmail && pCfg->tReplyTo.sEmail[0] ) {
		sReplyTo = xsmtp_format_mailbox(&pCfg->tReplyTo);
	}

	if ( sDate == NULL || sMessageID == NULL || sSubject == NULL || sFrom == NULL || sTo == NULL ) {
		goto cleanup;
	}

	if ( !xsmtp_buf_appendf(&tBuf, "Date: %s\r\n", sDate) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "Message-ID: %s\r\n", sMessageID) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "From: %s\r\n", sFrom) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "To: %s\r\n", sTo) ) goto cleanup;
	if ( sCc && sCc[0] && !xsmtp_buf_appendf(&tBuf, "Cc: %s\r\n", sCc) ) goto cleanup;
	if ( sReplyTo && sReplyTo[0] && !xsmtp_buf_appendf(&tBuf, "Reply-To: %s\r\n", sReplyTo) ) goto cleanup;
	if ( !xsmtp_buf_appendf(&tBuf, "Subject: %s\r\n", sSubject) ) goto cleanup;
	if ( !xsmtp_buf_append(&tBuf, "MIME-Version: 1.0\r\n") ) goto cleanup;

	if ( pMsg->arrHeaderNames && pMsg->arrHeaderValues ) {
		size_t i;
		for ( i = 0; i < pMsg->iHeaderCount; i++ ) {
			const char* sName = pMsg->arrHeaderNames[i];
			const char* sValue = pMsg->arrHeaderValues[i];
			if ( sName && sName[0] && sValue ) {
				if ( !xsmtp_buf_appendf(&tBuf, "%s: %s\r\n", sName, sValue) ) goto cleanup;
			}
		}
	}

	if ( pMsg->sTextBody && pMsg->sTextBody[0] && pMsg->sHtmlBody && pMsg->sHtmlBody[0] ) {
		char sBoundary[49];
		xsmtp_random_hex(sBoundary, sizeof(sBoundary), 24);
		sTextPayload = xsmtp_build_body_payload(pMsg->sTextBody);
		sHtmlPayload = xsmtp_build_body_payload(pMsg->sHtmlBody);
		if ( sTextPayload == NULL || sHtmlPayload == NULL ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "Content-Type: multipart/alternative; boundary=\"%s\"\r\n\r\n", sBoundary) ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "--%s\r\n", sBoundary) ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/plain", sTextPayload) ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "--%s\r\n", sBoundary) ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/html", sHtmlPayload) ) goto cleanup;
		if ( !xsmtp_buf_appendf(&tBuf, "--%s--\r\n", sBoundary) ) goto cleanup;
	} else if ( pMsg->sHtmlBody && pMsg->sHtmlBody[0] ) {
		sHtmlPayload = xsmtp_build_body_payload(pMsg->sHtmlBody);
		if ( sHtmlPayload == NULL ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/html", sHtmlPayload) ) goto cleanup;
	} else {
		sTextPayload = xsmtp_build_body_payload(xsmtp_str_or_empty(pMsg->sTextBody));
		if ( sTextPayload == NULL ) goto cleanup;
		if ( !xsmtp_append_body_headers(&tBuf, "text/plain", sTextPayload) ) goto cleanup;
	}

	bOK = TRUE;

cleanup:
	if ( sDate ) xrtFree(sDate);
	if ( sMessageID ) xrtFree(sMessageID);
	if ( sSubject ) xrtFree(sSubject);
	if ( sFrom ) xrtFree(sFrom);
	if ( sTo ) xrtFree(sTo);
	if ( sCc ) xrtFree(sCc);
	if ( sReplyTo ) xrtFree(sReplyTo);
	if ( sTextPayload ) xrtFree(sTextPayload);
	if ( sHtmlPayload ) xrtFree(sHtmlPayload);
	if ( !bOK ) {
		xsmtp_buf_unit(&tBuf);
		return NULL;
	}
	return tBuf.sData;
}

static int xsmtp_secure_mode_auto(const xsmtpconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return XSMTP_SECURE_SSL;
	}
	if ( pCfg->iSecureMode != XSMTP_SECURE_AUTO ) {
		return pCfg->iSecureMode;
	}
	if ( pCfg->iPort == 465 ) {
		return XSMTP_SECURE_SSL;
	}
	if ( pCfg->iPort == 587 ) {
		return XSMTP_SECURE_STARTTLS;
	}
	return XSMTP_SECURE_NONE;
}

static int xsmtp_auth_mode_auto(const xsmtpconfig* pCfg, uint32 iCaps)
{
	if ( pCfg == NULL ) {
		return XSMTP_AUTH_NONE;
	}
	if ( !pCfg->bAuth || pCfg->sUser == NULL || pCfg->sUser[0] == '\0' ) {
		return XSMTP_AUTH_NONE;
	}
	if ( pCfg->iAuthMode != XSMTP_AUTH_AUTO ) {
		return pCfg->iAuthMode;
	}
	if ( iCaps & XSMTP_CAP_AUTH_PLAIN ) {
		return XSMTP_AUTH_PLAIN;
	}
	if ( iCaps & XSMTP_CAP_AUTH_LOGIN ) {
		return XSMTP_AUTH_LOGIN;
	}
	return XSMTP_AUTH_LOGIN;
}

static bool xsmtp_socket_set_timeout(xsocket hSocket, uint32 iTimeoutMs)
{
#if defined(_WIN32) || defined(_WIN64)
	DWORD dwTimeout = (DWORD)iTimeoutMs;
	if ( setsockopt(hSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&dwTimeout, sizeof(dwTimeout)) != 0 ) {
		return FALSE;
	}
	if ( setsockopt(hSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&dwTimeout, sizeof(dwTimeout)) != 0 ) {
		return FALSE;
	}
#else
	struct timeval tv;
	tv.tv_sec = (int)(iTimeoutMs / 1000);
	tv.tv_usec = (int)((iTimeoutMs % 1000) * 1000);
	if ( setsockopt(hSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0 ) {
		return FALSE;
	}
	if ( setsockopt(hSocket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0 ) {
		return FALSE;
	}
#endif
	return TRUE;
}

static bool xsmtp_addr_to_sockaddr(const xnetaddr* pAddr, struct sockaddr_storage* pStorage, int* pLen)
{
	if ( pAddr == NULL || pStorage == NULL || pLen == NULL ) {
		return FALSE;
	}
	memset(pStorage, 0, sizeof(*pStorage));
	if ( pAddr->iFamily == AF_INET ) {
		struct sockaddr_in* pIPv4 = (struct sockaddr_in*)pStorage;
		pIPv4->sin_family = AF_INET;
		pIPv4->sin_port = htons(pAddr->iPort);
		memcpy(&pIPv4->sin_addr, pAddr->aAddr, 4);
		*pLen = (int)sizeof(struct sockaddr_in);
		return TRUE;
	}
	if ( pAddr->iFamily == AF_INET6 ) {
		struct sockaddr_in6* pIPv6 = (struct sockaddr_in6*)pStorage;
		pIPv6->sin6_family = AF_INET6;
		pIPv6->sin6_port = htons(pAddr->iPort);
		pIPv6->sin6_scope_id = pAddr->iScopeId;
		memcpy(&pIPv6->sin6_addr, pAddr->aAddr, 16);
		*pLen = (int)sizeof(struct sockaddr_in6);
		return TRUE;
	}
	return FALSE;
}

static bool xsmtp_socket_connect(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xnetaddr tAddr;
	struct sockaddr_storage tSockAddr;
	int iSockAddrLen = 0;

	if ( pSess == NULL || pCfg == NULL || pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP host is empty");
		return FALSE;
	}

#if defined(_WIN32) || defined(_WIN64)
	{
		static bool bWSAReady = FALSE;
		if ( !bWSAReady ) {
			WSADATA tWSA;
			if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) {
				xsmtp_result_error(pRet, "WSAStartup failed");
				return FALSE;
			}
			bWSAReady = TRUE;
		}
	}
#endif

	memset(&tAddr, 0, sizeof(tAddr));
	if ( xrtNetResolve(pCfg->sHost, &tAddr) != XRT_NET_OK ) {
		xsmtp_result_error(pRet, "SMTP host resolve failed");
		return FALSE;
	}
	tAddr.iPort = pCfg->iPort;
	if ( !xsmtp_addr_to_sockaddr(&tAddr, &tSockAddr, &iSockAddrLen) ) {
		xsmtp_result_error(pRet, "SMTP address family unsupported");
		return FALSE;
	}

	pSess->hSocket = socket(tAddr.iFamily, SOCK_STREAM, IPPROTO_TCP);
	if ( pSess->hSocket == XSOCKET_INVALID ) {
		xsmtp_result_error(pRet, "SMTP socket create failed");
		return FALSE;
	}
	pSess->bOwnSocket = TRUE;
	pSess->iTimeoutMs = pCfg->iTimeoutMs ? pCfg->iTimeoutMs : 15000;
	xsmtp_socket_set_timeout(pSess->hSocket, pSess->iTimeoutMs);

	if ( connect(pSess->hSocket, (struct sockaddr*)&tSockAddr, iSockAddrLen) != 0 ) {
		xsmtp_result_error(pRet, "SMTP socket connect failed");
		return FALSE;
	}
	return TRUE;
}

static void xsmtp_session_close(xsmtpsession* pSess)
{
	if ( pSess == NULL ) {
		return;
	}
	if ( pSess->pTls ) {
		xrtNetTlsSessionDestroy(pSess->pTls);
		pSess->pTls = NULL;
	}
	if ( pSess->bOwnSocket && pSess->hSocket != XSOCKET_INVALID ) {
#if defined(_WIN32) || defined(_WIN64)
		closesocket(pSess->hSocket);
#else
		close(pSess->hSocket);
#endif
	}
	pSess->hSocket = XSOCKET_INVALID;
	pSess->bOwnSocket = FALSE;
	pSess->bTlsReady = FALSE;
	pSess->iRecvLen = 0;
}

static bool xsmtp_socket_send_all(xsocket hSocket, const void* pData, size_t iLen)
{
	const char* p = (const char*)pData;
	while ( iLen > 0 ) {
		int iSent = send(hSocket, p, (int)iLen, 0);
		if ( iSent <= 0 ) {
			return FALSE;
		}
		p += iSent;
		iLen -= (size_t)iSent;
	}
	return TRUE;
}

static bool xsmtp_socket_recv_some(xsocket hSocket, void* pBuf, size_t iCap, size_t* pRead)
{
	int iRet;
	if ( pRead ) *pRead = 0;
	iRet = recv(hSocket, (char*)pBuf, (int)iCap, 0);
	if ( iRet <= 0 ) {
		return FALSE;
	}
	if ( pRead ) *pRead = (size_t)iRet;
	return TRUE;
}

static bool xsmtp_tls_flush(xsmtpsession* pSess, xsmtpresult* pRet)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL || pSess->pTls == NULL ) {
		return TRUE;
	}

	while ( xrtNetTlsSessionPendingCipher(pSess->pTls) > 0 ) {
		if ( xrtNetTlsSessionPeekCipher(pSess->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0 ) {
			xsmtp_result_error(pRet, "SMTP TLS cipher peek failed");
			return FALSE;
		}
		if ( !xsmtp_socket_send_all(pSess->hSocket, aBuf, iRead) ) {
			xsmtp_result_error(pRet, "SMTP TLS cipher send failed");
			return FALSE;
		}
		xrtNetTlsSessionConsumeCipher(pSess->pTls, iRead);
	}
	return TRUE;
}

static bool xsmtp_tls_handshake(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xtlsconfig tTLS;
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL || pCfg == NULL ) {
		xsmtp_result_error(pRet, "SMTP TLS config invalid");
		return FALSE;
	}

	memset(&tTLS, 0, sizeof(tTLS));
	tTLS.sHostName = pCfg->sHost;
	tTLS.bVerifyPeer = pCfg->bVerifyPeer;
	pSess->pTls = xrtNetTlsSessionCreate(&tTLS, FALSE);
	if ( pSess->pTls == NULL ) {
		xsmtp_result_error(pRet, "SMTP TLS session create failed");
		return FALSE;
	}

	while ( !xrtNetTlsSessionIsReady(pSess->pTls) ) {
		xnet_result iResult = xrtNetTlsSessionDriveHandshake(pSess->pTls);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_result_error(pRet, "SMTP TLS handshake failed");
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionIsReady(pSess->pTls) ) {
			break;
		}
		if ( !xsmtp_socket_recv_some(pSess->hSocket, aBuf, sizeof(aBuf), &iRead) ) {
			xsmtp_result_error(pRet, "SMTP TLS handshake recv failed");
			return FALSE;
		}
		if ( xrtNetTlsSessionFeedCipher(pSess->pTls, aBuf, iRead) != XRT_NET_OK ) {
			xsmtp_result_error(pRet, "SMTP TLS feed failed");
			return FALSE;
		}
	}

	pSess->bTlsReady = TRUE;
	if ( pRet ) {
		pRet->bUsedTLS = TRUE;
	}
	return TRUE;
}

static bool xsmtp_send_bytes(xsmtpsession* pSess, const void* pData, size_t iLen, xsmtpresult* pRet)
{
	const char* p = (const char*)pData;

	if ( pSess == NULL ) {
		xsmtp_result_error(pRet, "SMTP session missing");
		return FALSE;
	}
	if ( pSess->pTls == NULL ) {
		if ( !xsmtp_socket_send_all(pSess->hSocket, pData, iLen) ) {
			xsmtp_result_error(pRet, "SMTP socket send failed");
			return FALSE;
		}
		return TRUE;
	}

	while ( iLen > 0 ) {
		size_t iWritten = 0;
		xnet_result iResult = xrtNetTlsSessionWritePlain(pSess->pTls, p, iLen, &iWritten);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_result_error(pRet, "SMTP TLS write failed");
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
		if ( iWritten == 0 && iLen > 0 ) {
			xsmtp_result_error(pRet, "SMTP TLS write stalled");
			return FALSE;
		}
		p += iWritten;
		iLen -= iWritten;
	}
	return TRUE;
}

static bool xsmtp_append_recv(xsmtpsession* pSess, const char* pData, size_t iLen, xsmtpresult* pRet)
{
	if ( pSess == NULL || pData == NULL ) {
		return FALSE;
	}
	if ( pSess->iRecvLen + iLen >= sizeof(pSess->aRecv) ) {
		xsmtp_result_error(pRet, "SMTP recv buffer overflow");
		return FALSE;
	}
	memcpy(pSess->aRecv + pSess->iRecvLen, pData, iLen);
	pSess->iRecvLen += iLen;
	pSess->aRecv[pSess->iRecvLen] = '\0';
	return TRUE;
}

static bool xsmtp_pump_recv(xsmtpsession* pSess, xsmtpresult* pRet)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL ) {
		return FALSE;
	}

	if ( pSess->pTls == NULL ) {
		if ( !xsmtp_socket_recv_some(pSess->hSocket, aBuf, sizeof(aBuf), &iRead) ) {
			xsmtp_result_error(pRet, "SMTP recv failed");
			return FALSE;
		}
		return xsmtp_append_recv(pSess, aBuf, iRead, pRet);
	}

	while ( xrtNetTlsSessionPendingRecv(pSess->pTls) == 0 ) {
		if ( !xsmtp_socket_recv_some(pSess->hSocket, aBuf, sizeof(aBuf), &iRead) ) {
			xsmtp_result_error(pRet, "SMTP TLS recv failed");
			return FALSE;
		}
		if ( xrtNetTlsSessionFeedCipher(pSess->pTls, aBuf, iRead) != XRT_NET_OK ) {
			xsmtp_result_error(pRet, "SMTP TLS feed failed");
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
	}

	while ( xrtNetTlsSessionPendingRecv(pSess->pTls) > 0 ) {
		size_t iPlain = 0;
		if ( xrtNetTlsSessionReadPlain(pSess->pTls, aBuf, sizeof(aBuf), &iPlain) != XRT_NET_OK ) {
			xsmtp_result_error(pRet, "SMTP TLS read plain failed");
			return FALSE;
		}
		if ( iPlain == 0 ) {
			break;
		}
		if ( !xsmtp_append_recv(pSess, aBuf, iPlain, pRet) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_read_line(xsmtpsession* pSess, char* sOut, size_t iOutCap, xsmtpresult* pRet)
{
	size_t i;

	if ( sOut == NULL || iOutCap == 0 ) {
		xsmtp_result_error(pRet, "SMTP output buffer invalid");
		return FALSE;
	}

	for (;;) {
		for ( i = 0; i + 1 < pSess->iRecvLen; i++ ) {
			if ( pSess->aRecv[i] == '\r' && pSess->aRecv[i + 1] == '\n' ) {
				size_t iCopy = i;
				if ( iCopy >= iOutCap ) {
					iCopy = iOutCap - 1;
				}
				memcpy(sOut, pSess->aRecv, iCopy);
				sOut[iCopy] = '\0';
				memmove(pSess->aRecv, pSess->aRecv + i + 2, pSess->iRecvLen - (i + 2));
				pSess->iRecvLen -= (i + 2);
				pSess->aRecv[pSess->iRecvLen] = '\0';
				return TRUE;
			}
		}

		if ( !xsmtp_pump_recv(pSess, pRet) ) {
			return FALSE;
		}
	}
}

static bool xsmtp_read_reply(xsmtpsession* pSess, xsmtpresult* pRet, int* pCode, uint32* pCaps)
{
	char sLine[1024];
	int iReplyCode = 0;
	bool bFirst = TRUE;
	xsmtpbuf tReply;

	memset(&tReply, 0, sizeof(tReply));
	if ( pCaps ) {
		*pCaps = 0;
	}

	for (;;) {
		if ( !xsmtp_read_line(pSess, sLine, sizeof(sLine), pRet) ) {
			xsmtp_buf_unit(&tReply);
			return FALSE;
		}

		if ( bFirst ) {
			if ( strlen(sLine) < 3 || !isdigit((unsigned char)sLine[0]) || !isdigit((unsigned char)sLine[1]) || !isdigit((unsigned char)sLine[2]) ) {
				xsmtp_result_error(pRet, "SMTP invalid reply line");
				xsmtp_buf_unit(&tReply);
				return FALSE;
			}
			iReplyCode = (sLine[0] - '0') * 100 + (sLine[1] - '0') * 10 + (sLine[2] - '0');
			if ( pRet ) {
				pRet->iServerCode = iReplyCode;
			}
			bFirst = FALSE;
		}

		if ( !xsmtp_buf_append(&tReply, sLine) || !xsmtp_buf_append(&tReply, "\n") ) {
			xsmtp_result_error(pRet, "SMTP reply buffer alloc failed");
			xsmtp_buf_unit(&tReply);
			return FALSE;
		}

		if ( pCaps && strlen(sLine) > 4 && iReplyCode == 250 ) {
			const char* sCap = sLine + 4;
			if ( xsmtp_starts_with_i(sCap, "STARTTLS") ) {
				*pCaps |= XSMTP_CAP_STARTTLS;
			}
			if ( xsmtp_starts_with_i(sCap, "AUTH ") ) {
				if ( xsmtp_find_i(sCap, "PLAIN") ) {
					*pCaps |= XSMTP_CAP_AUTH_PLAIN;
				}
				if ( xsmtp_find_i(sCap, "LOGIN") ) {
					*pCaps |= XSMTP_CAP_AUTH_LOGIN;
				}
			}
		}

		if ( strlen(sLine) < 4 || sLine[3] != '-' ) {
			break;
		}
	}

	if ( pCode ) {
		*pCode = iReplyCode;
	}
	if ( pRet && tReply.sData ) {
		snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", tReply.sData);
	}
	xsmtp_buf_unit(&tReply);
	return TRUE;
}

static bool xsmtp_expect_reply(xsmtpsession* pSess, xsmtpresult* pRet, int iExpect, uint32* pCaps)
{
	int iCode = 0;
	if ( !xsmtp_read_reply(pSess, pRet, &iCode, pCaps) ) {
		return FALSE;
	}
	if ( iCode != iExpect ) {
		xsmtp_result_error(pRet, pRet ? pRet->sLastReply : "SMTP unexpected reply");
		return FALSE;
	}
	return TRUE;
}

static bool xsmtp_send_line(xsmtpsession* pSess, xsmtpresult* pRet, const char* sLine)
{
	xsmtpbuf tBuf;
	bool bOK;
	memset(&tBuf, 0, sizeof(tBuf));
	bOK = xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(sLine)) && xsmtp_buf_append(&tBuf, "\r\n");
	if ( bOK ) {
		bOK = xsmtp_send_bytes(pSess, tBuf.sData, tBuf.iLen, pRet);
	}
	xsmtp_buf_unit(&tBuf);
	return bOK;
}

static bool xsmtp_send_ehlo(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet, uint32* pCaps)
{
	const char* sHelo = (pCfg && pCfg->sHeloName && pCfg->sHeloName[0]) ? pCfg->sHeloName : "localhost";
	str sLine = xrtFormat("EHLO %s", sHelo);
	bool bOK;
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP EHLO alloc failed");
		return FALSE;
	}
	bOK = xsmtp_send_line(pSess, pRet, sLine) && xsmtp_expect_reply(pSess, pRet, 250, pCaps);
	xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_auth_plain(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xsmtpbuf tBuf;
	str sBase64 = NULL;
	str sLine = NULL;
	bool bOK = FALSE;

	memset(&tBuf, 0, sizeof(tBuf));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pCfg->sUser));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pCfg->sPass));
	sBase64 = xrtBase64Encode(tBuf.sData, tBuf.iLen, NULL);
	if ( sBase64 == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH PLAIN base64 failed");
		goto cleanup;
	}
	sLine = xrtFormat("AUTH PLAIN %s", sBase64);
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH PLAIN alloc failed");
		goto cleanup;
	}
	bOK = xsmtp_send_line(pSess, pRet, sLine) && xsmtp_expect_reply(pSess, pRet, 235, NULL);

cleanup:
	xsmtp_buf_unit(&tBuf);
	if ( sBase64 ) xrtFree(sBase64);
	if ( sLine ) xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_auth_login(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	str sUser64 = NULL;
	str sPass64 = NULL;
	bool bOK = FALSE;

	sUser64 = xrtBase64Encode((ptr)xsmtp_str_or_empty(pCfg->sUser), strlen(xsmtp_str_or_empty(pCfg->sUser)), NULL);
	sPass64 = xrtBase64Encode((ptr)xsmtp_str_or_empty(pCfg->sPass), strlen(xsmtp_str_or_empty(pCfg->sPass)), NULL);
	if ( sUser64 == NULL || sPass64 == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH LOGIN base64 failed");
		goto cleanup;
	}

	if ( !xsmtp_send_line(pSess, pRet, "AUTH LOGIN") || !xsmtp_expect_reply(pSess, pRet, 334, NULL) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(pSess, pRet, sUser64) || !xsmtp_expect_reply(pSess, pRet, 334, NULL) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(pSess, pRet, sPass64) || !xsmtp_expect_reply(pSess, pRet, 235, NULL) ) {
		goto cleanup;
	}

	bOK = TRUE;

cleanup:
	if ( sUser64 ) xrtFree(sUser64);
	if ( sPass64 ) xrtFree(sPass64);
	return bOK;
}

static bool xsmtp_send_auth(xsmtpsession* pSess, const xsmtpconfig* pCfg, uint32 iCaps, xsmtpresult* pRet)
{
	int iAuthMode;
	if ( pRet ) {
		pRet->iCapabilities = iCaps;
	}
	iAuthMode = xsmtp_auth_mode_auto(pCfg, iCaps);
	if ( pRet ) {
		pRet->iAuthMode = iAuthMode;
	}

	if ( iAuthMode == XSMTP_AUTH_NONE ) {
		return TRUE;
	}
	if ( iAuthMode == XSMTP_AUTH_PLAIN ) {
		return xsmtp_send_auth_plain(pSess, pCfg, pRet);
	}
	if ( iAuthMode == XSMTP_AUTH_LOGIN ) {
		return xsmtp_send_auth_login(pSess, pCfg, pRet);
	}

	xsmtp_result_error(pRet, "SMTP auth mode unsupported");
	return FALSE;
}

static bool xsmtp_send_mail_from(xsmtpsession* pSess, const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pRet)
{
	const char* sEmail = NULL;
	str sLine = NULL;
	bool bOK;

	if ( pMsg && pMsg->tFrom.sEmail && pMsg->tFrom.sEmail[0] ) {
		sEmail = pMsg->tFrom.sEmail;
	} else if ( pCfg && pCfg->tFrom.sEmail && pCfg->tFrom.sEmail[0] ) {
		sEmail = pCfg->tFrom.sEmail;
	}
	if ( sEmail == NULL || sEmail[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP from email is empty");
		return FALSE;
	}
	sLine = xrtFormat("MAIL FROM:<%s>", sEmail);
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP MAIL FROM alloc failed");
		return FALSE;
	}
	bOK = xsmtp_send_line(pSess, pRet, sLine) && xsmtp_expect_reply(pSess, pRet, 250, NULL);
	xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_rcpt_list(xsmtpsession* pSess, const xsmtpaddr* arrList, size_t iCount, xsmtpresult* pRet)
{
	size_t i;
	for ( i = 0; i < iCount; i++ ) {
		str sLine;
		if ( arrList[i].sEmail == NULL || arrList[i].sEmail[0] == '\0' ) {
			continue;
		}
		sLine = xrtFormat("RCPT TO:<%s>", arrList[i].sEmail);
		if ( sLine == NULL ) {
			xsmtp_result_error(pRet, "SMTP RCPT TO alloc failed");
			return FALSE;
		}
		if ( !xsmtp_send_line(pSess, pRet, sLine) || !xsmtp_expect_reply(pSess, pRet, 250, NULL) ) {
			xrtFree(sLine);
			return FALSE;
		}
		xrtFree(sLine);
	}
	return TRUE;
}

static bool xsmtp_send_data_body(xsmtpsession* pSess, const char* sData, xsmtpresult* pRet)
{
	xsmtpbuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xsmtp_buf_reserve(&tBuf, strlen(xsmtp_str_or_empty(sData)) + 16) ) {
		xsmtp_result_error(pRet, "SMTP DATA buffer alloc failed");
		return FALSE;
	}
	for ( p = xsmtp_str_or_empty(sData); *p; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen;
		if ( sLineEnd ) {
			iLineLen = (size_t)(sLineEnd - p);
		} else {
			iLineLen = strlen(p);
		}
		if ( iLineLen > 0 && p[0] == '.' ) {
			xsmtp_buf_append(&tBuf, ".");
		}
		xsmtp_buf_append_n(&tBuf, p, iLineLen);
		xsmtp_buf_append(&tBuf, "\r\n");
		if ( !sLineEnd ) {
			break;
		}
		p = sLineEnd + 2;
	}
	xsmtp_buf_append(&tBuf, ".\r\n");

	if ( !xsmtp_send_bytes(pSess, tBuf.sData, tBuf.iLen, pRet) ) {
		xsmtp_buf_unit(&tBuf);
		return FALSE;
	}
	xsmtp_buf_unit(&tBuf);
	return xsmtp_expect_reply(pSess, pRet, 250, NULL);
}

static bool xrtSmtpSendMail(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pRet)
{
	xsmtpsession tSess;
	uint32 iCaps = 0;
	int iSecureMode;
	str sData = NULL;
	size_t iRecipientCount = 0;
	bool bOK = FALSE;

	if ( pRet ) {
		xrtSmtpResultInit(pRet);
	}
	memset(&tSess, 0, sizeof(tSess));
	tSess.hSocket = XSOCKET_INVALID;

	if ( pCfg == NULL || pMsg == NULL ) {
		xsmtp_result_error(pRet, "SMTP config or message missing");
		goto cleanup;
	}
	if ( !xsmtp_count_recipients(pMsg, &iRecipientCount) ) {
		xsmtp_result_error(pRet, "SMTP recipients are empty");
		goto cleanup;
	}

	iSecureMode = xsmtp_secure_mode_auto(pCfg);
	if ( !xsmtp_socket_connect(&tSess, pCfg, pRet) ) {
		goto cleanup;
	}

	if ( iSecureMode == XSMTP_SECURE_SSL ) {
		if ( !xsmtp_tls_handshake(&tSess, pCfg, pRet) ) {
			goto cleanup;
		}
	}

	if ( !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
		goto cleanup;
	}

	if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
		goto cleanup;
	}

	if ( iSecureMode == XSMTP_SECURE_STARTTLS ) {
		if ( !(iCaps & XSMTP_CAP_STARTTLS) ) {
			xsmtp_result_error(pRet, "SMTP server does not support STARTTLS");
			goto cleanup;
		}
		if ( !xsmtp_send_line(&tSess, pRet, "STARTTLS") || !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
			goto cleanup;
		}
		if ( !xsmtp_tls_handshake(&tSess, pCfg, pRet) ) {
			goto cleanup;
		}
		if ( pRet ) {
			pRet->bUsedStartTLS = TRUE;
		}
		iCaps = 0;
		if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
			goto cleanup;
		}
	}

	if ( !xsmtp_send_auth(&tSess, pCfg, iCaps, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_mail_from(&tSess, pCfg, pMsg, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrTo, pMsg->iToCount, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrCc, pMsg->iCcCount, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrBcc, pMsg->iBccCount, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(&tSess, pRet, "DATA") || !xsmtp_expect_reply(&tSess, pRet, 354, NULL) ) {
		goto cleanup;
	}

	sData = xsmtp_build_message_data(pCfg, pMsg);
	if ( sData == NULL ) {
		xsmtp_result_error(pRet, "SMTP build message failed");
		goto cleanup;
	}
	if ( !xsmtp_send_data_body(&tSess, sData, pRet) ) {
		goto cleanup;
	}

	xsmtp_send_line(&tSess, pRet, "QUIT");
	xsmtp_expect_reply(&tSess, pRet, 221, NULL);

	bOK = TRUE;
	if ( pRet ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sData ) {
		xrtFree(sData);
	}
	xsmtp_session_close(&tSess);
	return bOK;
}

#endif
