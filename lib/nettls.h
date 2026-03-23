/*
    NetTLS - builtin TLS engine for xrt

    This file implements the in-tree TLS stack used by the modern xrt network
    path. It does not depend on the archived v1 network layer and only relies
    on:
      - lib/crypto.h for cryptographic primitives
      - platform sockets when the blocking handshake helper is used
      - transport-fed TLS byte buffers owned by xtlsctx

    Current scope:
      - TLS 1.3 as the primary path
      - TLS 1.2 compatibility where already supported by the engine
      - socketless transport-owned drive/feed integration via xrtTlsDrive and
        xrtTlsFeed
      - builtin client/server operation with no external TLS dependency
*/

#if defined(_WIN32) || defined(_WIN64)
	#ifdef __TINYC__
		#include <winapi/wincrypt.h>
	#else
		#include <wincrypt.h>
	#endif
#endif

typedef struct xrt_tls_context xtlsctx;

XXAPI void xrtTlsDestroy(xtlsctx *pCtx);
XXAPI xnet_result xrtTlsSetCert(xtlsctx *pCtx, const char *sCertFile, const char *sKeyFile);

typedef struct {
	char* pBase;
	char* pData;
	size_t iSize;
	size_t iCapacity;
} __xrt_tls_buf;

static bool __xrt_tls_buf_init(__xrt_tls_buf* pBuf, size_t iCapacity)
{
	if ( !pBuf ) return false;
	pBuf->pBase = (char*)xrtMalloc(iCapacity);
	if ( !pBuf->pBase ) return false;
	pBuf->pData = pBuf->pBase;
	pBuf->iSize = 0;
	pBuf->iCapacity = iCapacity;
	return true;
}

static void __xrt_tls_buf_free(__xrt_tls_buf* pBuf)
{
	if ( !pBuf ) return;
	if ( pBuf->pBase ) {
		xrtFree(pBuf->pBase);
		pBuf->pBase = NULL;
		pBuf->pData = NULL;
	}
	pBuf->iSize = 0;
	pBuf->iCapacity = 0;
}

static bool __xrt_tls_buf_ensure(__xrt_tls_buf* pBuf, size_t iExtra)
{
	size_t iHead;
	size_t iNeed;
	size_t iNewCap;
	char* pNew;
	if ( !pBuf ) return false;
	if ( !pBuf->pBase ) return false;
	iHead = (pBuf->pData && pBuf->pData >= pBuf->pBase) ? (size_t)(pBuf->pData - pBuf->pBase) : 0u;
	iNeed = pBuf->iSize + iExtra;
	if ( iHead + iNeed <= pBuf->iCapacity ) return true;
	if ( iNeed <= pBuf->iCapacity ) {
		if ( pBuf->iSize > 0u && iHead > 0u ) {
			memmove(pBuf->pBase, pBuf->pData, pBuf->iSize);
		}
		pBuf->pData = pBuf->pBase;
		return true;
	}
	iNewCap = pBuf->iCapacity ? (pBuf->iCapacity * 2) : 256;
	if ( iNewCap < iNeed ) iNewCap = iNeed;
	pNew = (char*)xrtRealloc(pBuf->pBase, iNewCap);
	if ( !pNew ) return false;
	if ( pBuf->iSize > 0u && iHead > 0u ) {
		memmove(pNew, pNew + iHead, pBuf->iSize);
	}
	pBuf->pBase = pNew;
	pBuf->pData = pNew;
	pBuf->iCapacity = iNewCap;
	return true;
}

static bool __xrt_tls_buf_append(__xrt_tls_buf* pBuf, const char* pData, size_t iLen)
{
	if ( !pBuf || !pData || iLen == 0 ) return false;
	if ( !__xrt_tls_buf_ensure(pBuf, iLen) ) return false;
	memcpy(pBuf->pData + pBuf->iSize, pData, iLen);
	pBuf->iSize += iLen;
	return true;
}

static void __xrt_tls_buf_consume(__xrt_tls_buf* pBuf, size_t iLen)
{
	if ( !pBuf || iLen == 0 ) return;
	if ( iLen >= pBuf->iSize ) {
		pBuf->iSize = 0;
		pBuf->pData = pBuf->pBase;
		return;
	}
	pBuf->pData += iLen;
	pBuf->iSize -= iLen;
}

static int __xrt_tls_sock_last_err(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}

static bool __xrt_tls_sock_would_block(int iErr)
{
	#if defined(_WIN32) || defined(_WIN64)
		return iErr == WSAEWOULDBLOCK || iErr == WSAEINPROGRESS || iErr == WSAEINTR;
	#else
		return iErr == EAGAIN || iErr == EWOULDBLOCK || iErr == EINTR;
	#endif
}

static xnet_result __xrt_tls_sock_send(xsocket hSocket, const char* pData, size_t iLen, size_t* pSent)
{
	int iRet;
	if ( pSent ) *pSent = 0;
	if ( hSocket == XSOCKET_INVALID || !pData || iLen == 0 ) return XRT_NET_ERROR;
	iRet = (int)send(hSocket, pData, (int)((iLen > (size_t)INT_MAX) ? INT_MAX : iLen), 0);
	if ( iRet > 0 ) {
		if ( pSent ) *pSent = (size_t)iRet;
		return XRT_NET_OK;
	}
	if ( iRet == 0 ) return XRT_NET_CLOSED;
	return __xrt_tls_sock_would_block(__xrt_tls_sock_last_err()) ? XRT_NET_AGAIN : XRT_NET_ERROR;
}

static xnet_result __xrt_tls_sock_recv(xsocket hSocket, char* pBuf, size_t iLen, size_t* pReceived)
{
	int iRet;
	if ( pReceived ) *pReceived = 0;
	if ( hSocket == XSOCKET_INVALID || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	iRet = (int)recv(hSocket, pBuf, (int)((iLen > (size_t)INT_MAX) ? INT_MAX : iLen), 0);
	if ( iRet > 0 ) {
		if ( pReceived ) *pReceived = (size_t)iRet;
		return XRT_NET_OK;
	}
	if ( iRet == 0 ) return XRT_NET_CLOSED;
	return __xrt_tls_sock_would_block(__xrt_tls_sock_last_err()) ? XRT_NET_AGAIN : XRT_NET_ERROR;
}


/* ============================== TLS resume state ============================== */

struct xrt_tls_resume {
	uint16 iVersion;
	uint16 iCipherSuite;
	uint8 iSessionIdLen;
	uint8 aSessionId[32];
	uint8 aMasterSecret[48];
};

typedef struct __xrt_tls_resume_cache_entry {
	struct __xrt_tls_resume_cache_entry* pNext;
	uint64 iGeneration;
	struct xrt_tls_resume tResume;
} __xrt_tls_resume_cache_entry;

static volatile long __xrt_tls_resume_lock = 0;
static __xrt_tls_resume_cache_entry* __xrt_tls_resume_cache = NULL;
static uint32 __xrt_tls_resume_cache_count = 0;
static uint64 __xrt_tls_resume_cache_gen = 0;

#ifndef __XRT_TLS_VERSION_1_2
	#define __XRT_TLS_VERSION_1_2  0x0303
#endif
#ifndef __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256
	#define __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256  0xC02B
	#define __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256    0xC02F
	#define __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384  0xC02C
	#define __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384    0xC030
	#define __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256    0xCCA8
	#define __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256  0xCCA9
#endif

static void __xrt_tls_resume_lock_acquire(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		while ( InterlockedCompareExchange((volatile LONG*)&__xrt_tls_resume_lock, 1, 0) != 0 ) {
			xrtThreadYield();
		}
	#else
		while ( __sync_lock_test_and_set(&__xrt_tls_resume_lock, 1) != 0 ) {
			xrtThreadYield();
		}
	#endif
}

static void __xrt_tls_resume_lock_release(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)&__xrt_tls_resume_lock, 0);
	#else
		__sync_lock_release(&__xrt_tls_resume_lock);
	#endif
}

static bool __xrt_tls_resume_copy(struct xrt_tls_resume* pDst, const struct xrt_tls_resume* pSrc)
{
	if ( !pDst || !pSrc ) return false;
	if ( pSrc->iSessionIdLen == 0 || pSrc->iSessionIdLen > sizeof(pSrc->aSessionId) ) return false;
	if ( pSrc->iVersion != __XRT_TLS_VERSION_1_2 ) return false;
	memcpy(pDst, pSrc, sizeof(*pDst));
	return true;
}

static bool __xrt_tls_resume_cache_lookup(const uint8* pSessionId, uint8 iSessionIdLen, struct xrt_tls_resume* pOut)
{
	__xrt_tls_resume_cache_entry* pEntry;
	bool bFound = false;
	if ( !pSessionId || !pOut || iSessionIdLen == 0 ) return false;

	__xrt_tls_resume_lock_acquire();
	for ( pEntry = __xrt_tls_resume_cache; pEntry; pEntry = pEntry->pNext ) {
		if ( pEntry->tResume.iSessionIdLen != iSessionIdLen ) continue;
		if ( memcmp(pEntry->tResume.aSessionId, pSessionId, iSessionIdLen) != 0 ) continue;
		memcpy(pOut, &pEntry->tResume, sizeof(*pOut));
		pEntry->iGeneration = ++__xrt_tls_resume_cache_gen;
		bFound = true;
		break;
	}
	__xrt_tls_resume_lock_release();
	return bFound;
}

static void __xrt_tls_resume_cache_store(const struct xrt_tls_resume* pResume)
{
	__xrt_tls_resume_cache_entry* pEntry;
	__xrt_tls_resume_cache_entry* pPrevOldest = NULL;
	__xrt_tls_resume_cache_entry* pOldest = NULL;
	__xrt_tls_resume_cache_entry* pPrev = NULL;
	__xrt_tls_resume_cache_entry* pCurr = NULL;

	if ( !pResume || pResume->iVersion != __XRT_TLS_VERSION_1_2 || pResume->iSessionIdLen == 0 ) return;

	__xrt_tls_resume_lock_acquire();
	for ( pCurr = __xrt_tls_resume_cache; pCurr; pCurr = pCurr->pNext ) {
		if ( pCurr->tResume.iSessionIdLen == pResume->iSessionIdLen
			&& memcmp(pCurr->tResume.aSessionId, pResume->aSessionId, pResume->iSessionIdLen) == 0 ) {
			memcpy(&pCurr->tResume, pResume, sizeof(*pResume));
			pCurr->iGeneration = ++__xrt_tls_resume_cache_gen;
			__xrt_tls_resume_lock_release();
			return;
		}
	}

	pEntry = (__xrt_tls_resume_cache_entry*)xrtCalloc(1, sizeof(*pEntry));
	if ( !pEntry ) {
		__xrt_tls_resume_lock_release();
		return;
	}
	memcpy(&pEntry->tResume, pResume, sizeof(*pResume));
	pEntry->iGeneration = ++__xrt_tls_resume_cache_gen;
	pEntry->pNext = __xrt_tls_resume_cache;
	__xrt_tls_resume_cache = pEntry;
	__xrt_tls_resume_cache_count++;

	if ( __xrt_tls_resume_cache_count > 128u ) {
		for ( pCurr = __xrt_tls_resume_cache; pCurr; pCurr = pCurr->pNext ) {
			if ( !pOldest || pCurr->iGeneration < pOldest->iGeneration ) {
				pOldest = pCurr;
				pPrevOldest = pPrev;
			}
			pPrev = pCurr;
		}
		if ( pOldest ) {
			if ( pPrevOldest ) pPrevOldest->pNext = pOldest->pNext;
			else __xrt_tls_resume_cache = pOldest->pNext;
			xrtFree(pOldest);
			__xrt_tls_resume_cache_count--;
		}
	}

	__xrt_tls_resume_lock_release();
}

static bool __xrt_tls12_client_offers_suite(uint16 iSuite,
	bool bOfferTLS12EcdheEcdsaAES128,
	bool bOfferTLS12EcdheEcdsaAES256,
	bool bOfferTLS12EcdheEcdsaChaCha,
	bool bOfferTLS12EcdheRsaAES128,
	bool bOfferTLS12EcdheRsaAES256,
	bool bOfferTLS12EcdheRsaChaCha)
{
	switch ( iSuite ) {
		case __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256: return bOfferTLS12EcdheEcdsaAES128;
		case __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384: return bOfferTLS12EcdheEcdsaAES256;
		case __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256: return bOfferTLS12EcdheEcdsaChaCha;
		case __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256: return bOfferTLS12EcdheRsaAES128;
		case __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384: return bOfferTLS12EcdheRsaAES256;
		case __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256: return bOfferTLS12EcdheRsaChaCha;
		default: return false;
	}
}


/* ============================== 常量定义 ============================== */

// TLS 记录类型 (RFC 8446 B.1)
#define __XRT_TLS_CHANGE_CIPHER  20
#define __XRT_TLS_ALERT          21
#define __XRT_TLS_HANDSHAKE      22
#define __XRT_TLS_APP_DATA       23

// TLS 握手消息类型 (RFC 8446 B.3 + RFC 5246)
#define __XRT_TLS_CLIENT_HELLO          1
#define __XRT_TLS_SERVER_HELLO          2
#define __XRT_TLS_ENCRYPTED_EXTENSIONS  8
#define __XRT_TLS_CERTIFICATE           11
#define __XRT_TLS_SERVER_KEY_EXCHANGE   12   // TLS 1.2 only
#define __XRT_TLS_SERVER_HELLO_DONE     14   // TLS 1.2 only
#define __XRT_TLS_CERTIFICATE_VERIFY    15
#define __XRT_TLS_CLIENT_KEY_EXCHANGE   16   // TLS 1.2 only
#define __XRT_TLS_FINISHED              20

// 记录头大小
#define __XRT_TLS_RECHDR_SIZE  5   // 1 type + 2 version + 2 length
#define __XRT_TLS_MSGHDR_SIZE  4   // 1 type + 3 length

// TLS 1.3 密码套件 ID
#define __XRT_TLS_AES_128_GCM_SHA256        0x1301
#define __XRT_TLS_AES_256_GCM_SHA384        0x1302
#define __XRT_TLS_CHACHA20_POLY1305_SHA256  0x1303

// TLS 1.2 密码套件 ID
#define __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256  0xC02B
#define __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256    0xC02F
#define __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384  0xC02C
#define __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384    0xC030
#define __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256    0xCCA8
#define __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256  0xCCA9
#define __XRT_TLS12_RSA_AES128_GCM_SHA256          0x009C
#define __XRT_TLS12_RSA_AES256_GCM_SHA384          0x009D

// TLS 版本
#define __XRT_TLS_VERSION_1_2  0x0303  // 记录层兼容
#define __XRT_TLS_VERSION_1_3  0x0304

// 最大记录大小
#define __XRT_TLS_MAX_RECORD   32768



/* ============================== 辅助函数 ============================== */

static inline uint16 __xrt_tls_load_be16(const uint8 *p)
{
	return (uint16)((uint16)p[0] << 8 | p[1]);
}

static inline uint32 __xrt_tls_load_be24(const uint8 *p)
{
	return ((uint32)p[0] << 16) | ((uint32)p[1] << 8) | p[2];
}

static inline void __xrt_tls_store_be16(uint8 *p, uint16 v)
{
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)(v);
}

static inline void __xrt_tls_store_be24(uint8 *p, uint32 v)
{
	p[0] = (uint8)(v >> 16);
	p[1] = (uint8)(v >> 8);
	p[2] = (uint8)(v);
}

static inline void __xrt_tls_store_be64(uint8 *p, uint64 v)
{
	p[0] = (uint8)(v >> 56);
	p[1] = (uint8)(v >> 48);
	p[2] = (uint8)(v >> 40);
	p[3] = (uint8)(v >> 32);
	p[4] = (uint8)(v >> 24);
	p[5] = (uint8)(v >> 16);
	p[6] = (uint8)(v >> 8);
	p[7] = (uint8)(v);
}



/* ============================== ASN.1/DER 解析 ============================== */

// DER TLV 节点
struct __xrt_der_tlv {
	uint8 iType;
	uint32 iLen;
	uint8 *pValue;
	uint8 *pRaw;
	uint32 iHeaderLen;
	uint32 iTotalLen;
};

// 解析一个 DER TLV 节点
static int __xrt_der_parse(uint8 *pDer, size_t iDerSz, struct __xrt_der_tlv *pTlv)
{
	size_t iHeaderLen = 2;
	uint32 iLen;

	if ( iDerSz < 2 ) return -1;

	pTlv->iType = pDer[0];
	iLen = pDer[1];

	if ( iLen > 0x7F ) {  // long-form length
		uint8 iLenBytes = iLen & 0x7F;
		uint8 k;
		if ( iDerSz < (size_t)(2 + iLenBytes) ) return -1;
		iLen = 0;
		for ( k = 0; k < iLenBytes; k++ ) {
			iLen = (iLen << 8) | pDer[2 + k];
		}
		iHeaderLen += iLenBytes;
	}

	if ( iDerSz < iHeaderLen + iLen ) return -1;

	pTlv->iLen = iLen;
	pTlv->pValue = pDer + iHeaderLen;
	pTlv->pRaw = pDer;
	pTlv->iHeaderLen = (uint32)iHeaderLen;
	pTlv->iTotalLen = (uint32)(iHeaderLen + iLen);
	return (int)(iHeaderLen + iLen);
}

// 遍历下一个子 TLV
static int __xrt_der_next(struct __xrt_der_tlv *pParent, struct __xrt_der_tlv *pChild)
{
	int iConsumed;
	if ( pParent->iLen == 0 ) return 0;
	iConsumed = __xrt_der_parse(pParent->pValue, pParent->iLen, pChild);
	if ( iConsumed < 0 ) return -1;
	pParent->pValue += iConsumed;
	pParent->iLen -= (uint32)iConsumed;
	return 1;
}

// 在 DER 结构中递归查找 OID
static int __xrt_der_find_oid(struct __xrt_der_tlv *pTlv, const uint8 *pOid,
	size_t iOidLen, struct __xrt_der_tlv *pFound)
{
	struct __xrt_der_tlv tParent, tChild;
	tParent = *pTlv;

	while ( __xrt_der_next(&tParent, &tChild) > 0 ) {
		if ( (tChild.iType == 0x06) && (tChild.iLen == iOidLen) &&
			(memcmp(tChild.pValue, pOid, iOidLen) == 0) ) {
			return __xrt_der_next(&tParent, pFound);
		} else if ( tChild.iType & 0x20 ) {
			struct __xrt_der_tlv tSub = tChild;
			if ( __xrt_der_find_oid(&tSub, pOid, iOidLen, pFound) ) return 1;
		}
	}
	return 0;
}

// rsaEncryption OID: 1.2.840.113549.1.1.1
static const uint8 __xrt_oid_rsa_encryption[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
};

// ecPublicKey OID: 1.2.840.10045.2.1
static const uint8 __xrt_oid_ec_public_key[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01
};

// prime256v1 OID: 1.2.840.10045.3.1.7
static const uint8 __xrt_oid_prime256v1[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
};

// secp384r1 OID: 1.3.132.0.34
static const uint8 __xrt_oid_secp384r1[] = {
	0x2b, 0x81, 0x04, 0x00, 0x22
};

// Ed25519 OID: 1.3.101.112
static const uint8 __xrt_oid_ed25519[] = {
	0x2b, 0x65, 0x70
};

// x509 OID: commonName 2.5.4.3
static const uint8 __xrt_oid_common_name[] = {
        0x55, 0x04, 0x03
};

// x509 OID: subjectAltName 2.5.29.17
static const uint8 __xrt_oid_subject_alt_name[] = {
	0x55, 0x1d, 0x11
};

// x509 OID: keyUsage 2.5.29.15
static const uint8 __xrt_oid_key_usage[] = {
	0x55, 0x1d, 0x0f
};

// x509 OID: basicConstraints 2.5.29.19
static const uint8 __xrt_oid_basic_constraints[] = {
	0x55, 0x1d, 0x13
};

// x509 OID: extendedKeyUsage 2.5.29.37
static const uint8 __xrt_oid_extended_key_usage[] = {
	0x55, 0x1d, 0x25
};

// x509 OID: id-kp-serverAuth 1.3.6.1.5.5.7.3.1
static const uint8 __xrt_oid_server_auth[] = {
	0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
};

// x509 OID: sha256WithRSAEncryption 1.2.840.113549.1.1.11
static const uint8 __xrt_oid_sha256_with_rsa[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b
};

// x509 OID: sha384WithRSAEncryption 1.2.840.113549.1.1.12
static const uint8 __xrt_oid_sha384_with_rsa[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c
};

// x509 OID: sha512WithRSAEncryption 1.2.840.113549.1.1.13
static const uint8 __xrt_oid_sha512_with_rsa[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d
};

// x509 OID: rsassaPss 1.2.840.113549.1.1.10
static const uint8 __xrt_oid_rsassa_pss[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a
};

// x509 OID: id-mgf1 1.2.840.113549.1.1.8
static const uint8 __xrt_oid_mgf1[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x08
};

// x509 OID: sha256 2.16.840.1.101.3.4.2.1
static const uint8 __xrt_oid_sha256[] = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01
};

// x509 OID: sha384 2.16.840.1.101.3.4.2.2
static const uint8 __xrt_oid_sha384[] = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02
};

// x509 OID: sha512 2.16.840.1.101.3.4.2.3
static const uint8 __xrt_oid_sha512[] = {
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03
};

// x509 OID: ecdsa-with-SHA256 1.2.840.10045.4.3.2
static const uint8 __xrt_oid_ecdsa_sha256[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02
};

// x509 OID: ecdsa-with-SHA384 1.2.840.10045.4.3.3
static const uint8 __xrt_oid_ecdsa_sha384[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03
};

// x509 OID: ecdsa-with-SHA512 1.2.840.10045.4.3.4
static const uint8 __xrt_oid_ecdsa_sha512[] = {
	0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04
};

#define __XRT_TLS_MAX_CERT_CHAIN  8
#define __XRT_TLS_MAX_DNS_NAMES   8

enum __xrt_x509_sig_alg {
	__XRT_X509_SIGALG_UNKNOWN = 0,
	__XRT_X509_SIGALG_RSA_PKCS1_SHA256,
	__XRT_X509_SIGALG_RSA_PKCS1_SHA384,
	__XRT_X509_SIGALG_RSA_PKCS1_SHA512,
	__XRT_X509_SIGALG_RSA_PSS_SHA256,
	__XRT_X509_SIGALG_RSA_PSS_SHA384,
	__XRT_X509_SIGALG_RSA_PSS_SHA512,
	__XRT_X509_SIGALG_ECDSA_SHA256,
	__XRT_X509_SIGALG_ECDSA_SHA384,
	__XRT_X509_SIGALG_ECDSA_SHA512,
	__XRT_X509_SIGALG_ED25519
};

struct __xrt_x509_cert {
	uint8 *pDer;
	size_t iDerLen;
	uint8 *pTbs;
	size_t iTbsLen;
	uint8 *pIssuerRaw;
	size_t iIssuerRawLen;
	uint8 *pSubjectRaw;
	size_t iSubjectRawLen;
	time_t iNotBefore;
	time_t iNotAfter;
	bool bHasValidity;
	bool bHasBasicConstraints;
	bool bIsCA;
	bool bHasKeyUsage;
	uint16 iKeyUsage;
	bool bHasExtendedKeyUsage;
	bool bHasServerAuth;
	size_t iDnsNameCount;
	char aDnsNames[__XRT_TLS_MAX_DNS_NAMES][256];
	bool bHasCommonName;
	char sCommonName[256];
	enum __xrt_x509_sig_alg iSigAlg;
	size_t iSigHashLen;
	const uint8 *pSig;
	size_t iSigLen;
	bool bIsECPubKey;
	bool bIsEd25519Key;
	bool bIsRSAPSSKey;
	uint8 *pMod;
	size_t iModSz;
	uint8 *pExp;
	size_t iExpSz;
	uint8 *pECPub;
	size_t iECPubSz;
	uint8 *pEdPub;
	size_t iEdPubSz;
};

static int __xrt_tls_ascii_tolower(int c)
{
	if ( c >= 'A' && c <= 'Z' ) return c + ('a' - 'A');
	return c;
}

static bool __xrt_tls_ascii_case_equal(const char *sA, const char *sB)
{
	size_t i;
	size_t iLenA = sA ? strlen(sA) : 0;
	size_t iLenB = sB ? strlen(sB) : 0;

	if ( iLenA != iLenB ) return false;
	for ( i = 0; i < iLenA; i++ ) {
		if ( __xrt_tls_ascii_tolower((uint8)sA[i]) != __xrt_tls_ascii_tolower((uint8)sB[i]) ) {
			return false;
		}
	}
	return true;
}

static bool __xrt_tls_hostname_matches(const char *sPattern, const char *sHost)
{
	const char *pFirstDot;
	const char *pHostDot;
	size_t iSuffixLen;

	if ( !sPattern || !sHost || !sPattern[0] || !sHost[0] ) return false;
	if ( strchr(sHost, '*') ) return false;
	if ( sPattern[0] != '*' ) return __xrt_tls_ascii_case_equal(sPattern, sHost);
	if ( sPattern[1] != '.' ) return false;

	pFirstDot = strchr(sPattern + 2, '.');
	if ( !pFirstDot ) return false;
	pHostDot = strchr(sHost, '.');
	if ( !pHostDot ) return false;
	if ( strchr(pHostDot + 1, '.') == NULL ) return false;

	iSuffixLen = strlen(sPattern + 1);
	if ( strlen(pHostDot) != iSuffixLen ) return false;
	return __xrt_tls_ascii_case_equal(pHostDot, sPattern + 1);
}

static time_t __xrt_tls_timegm(struct tm *pTM)
{
	#if defined(_WIN32) || defined(_WIN64)
		return _mkgmtime(pTM);
	#else
		return timegm(pTM);
	#endif
}

static bool __xrt_x509_parse_time_value(const struct __xrt_der_tlv *pTime, time_t *pOut)
{
	struct tm tTM;
	int iYear = 0, iMonth = 0, iDay = 0;
	int iHour = 0, iMinute = 0, iSecond = 0;
	const char *p = (const char*)pTime->pValue;

	if ( !pTime || !pOut ) return false;
	memset(&tTM, 0, sizeof(tTM));

	if ( pTime->iType == 0x17 ) {
		if ( pTime->iLen < 13 || p[pTime->iLen - 1] != 'Z' ) return false;
		iYear = (p[0] - '0') * 10 + (p[1] - '0');
		iYear += (iYear >= 50) ? 1900 : 2000;
		iMonth = (p[2] - '0') * 10 + (p[3] - '0');
		iDay = (p[4] - '0') * 10 + (p[5] - '0');
		iHour = (p[6] - '0') * 10 + (p[7] - '0');
		iMinute = (p[8] - '0') * 10 + (p[9] - '0');
		iSecond = (p[10] - '0') * 10 + (p[11] - '0');
	} else if ( pTime->iType == 0x18 ) {
		if ( pTime->iLen < 15 || p[pTime->iLen - 1] != 'Z' ) return false;
		iYear = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
		iMonth = (p[4] - '0') * 10 + (p[5] - '0');
		iDay = (p[6] - '0') * 10 + (p[7] - '0');
		iHour = (p[8] - '0') * 10 + (p[9] - '0');
		iMinute = (p[10] - '0') * 10 + (p[11] - '0');
		iSecond = (p[12] - '0') * 10 + (p[13] - '0');
	} else {
		return false;
	}

	if ( iMonth < 1 || iMonth > 12 || iDay < 1 || iDay > 31 ||
		iHour < 0 || iHour > 23 || iMinute < 0 || iMinute > 59 ||
		iSecond < 0 || iSecond > 60 ) {
		return false;
	}

	tTM.tm_year = iYear - 1900;
	tTM.tm_mon = iMonth - 1;
	tTM.tm_mday = iDay;
	tTM.tm_hour = iHour;
	tTM.tm_min = iMinute;
	tTM.tm_sec = iSecond;
	*pOut = __xrt_tls_timegm(&tTM);
	return *pOut != (time_t)-1;
}

static bool __xrt_x509_copy_string_value(char *sOut, size_t iOutSz, const struct __xrt_der_tlv *pValue)
{
	size_t iCopy;

	if ( !sOut || iOutSz == 0 || !pValue ) return false;
	if ( pValue->iType != 0x0c && pValue->iType != 0x13 && pValue->iType != 0x16 &&
		pValue->iType != 0x14 ) {
		return false;
	}

	iCopy = pValue->iLen;
	if ( iCopy >= iOutSz ) iCopy = iOutSz - 1;
	memcpy(sOut, pValue->pValue, iCopy);
	sOut[iCopy] = '\0';
	return iCopy > 0;
}

static bool __xrt_x509_parse_common_name(const struct __xrt_der_tlv *pName, char *sOut, size_t iOutSz)
{
	struct __xrt_der_tlv tRDNs, tSet, tAttr, tOID, tValue;

	if ( !pName || pName->iType != 0x30 ) return false;
	tRDNs = *pName;
	while ( __xrt_der_next(&tRDNs, &tSet) > 0 ) {
		if ( tSet.iType != 0x31 ) continue;
		{
			struct __xrt_der_tlv tSetItems = tSet;
			while ( __xrt_der_next(&tSetItems, &tAttr) > 0 ) {
				if ( tAttr.iType != 0x30 ) continue;
				{
					struct __xrt_der_tlv tAttrItems = tAttr;
					if ( __xrt_der_next(&tAttrItems, &tOID) <= 0 || tOID.iType != 0x06 ) continue;
					if ( __xrt_der_next(&tAttrItems, &tValue) <= 0 ) continue;
					if ( tOID.iLen == sizeof(__xrt_oid_common_name) &&
						memcmp(tOID.pValue, __xrt_oid_common_name, sizeof(__xrt_oid_common_name)) == 0 ) {
						return __xrt_x509_copy_string_value(sOut, iOutSz, &tValue);
					}
				}
			}
		}
	}
	return false;
}

static bool __xrt_x509_parse_hash_oid(const struct __xrt_der_tlv *pOID, size_t *pHashLen)
{
	if ( !pOID || pOID->iType != 0x06 || !pHashLen ) return false;
	if ( pOID->iLen == sizeof(__xrt_oid_sha256) &&
		memcmp(pOID->pValue, __xrt_oid_sha256, sizeof(__xrt_oid_sha256)) == 0 ) {
		*pHashLen = 32;
		return true;
	}
	if ( pOID->iLen == sizeof(__xrt_oid_sha384) &&
		memcmp(pOID->pValue, __xrt_oid_sha384, sizeof(__xrt_oid_sha384)) == 0 ) {
		*pHashLen = 48;
		return true;
	}
	if ( pOID->iLen == sizeof(__xrt_oid_sha512) &&
		memcmp(pOID->pValue, __xrt_oid_sha512, sizeof(__xrt_oid_sha512)) == 0 ) {
		*pHashLen = 64;
		return true;
	}
	return false;
}

static bool __xrt_x509_parse_pss_params(const struct __xrt_der_tlv *pParams,
	enum __xrt_x509_sig_alg *pSigAlg, size_t *pHashLen)
{
	struct __xrt_der_tlv tSeq, tField;
	size_t iHashLen = 0;
	size_t iMGFHashLen = 0;
	size_t iSaltLen = 0;

	if ( !pParams || pParams->iType != 0x30 ) return false;
	tSeq = *pParams;
	while ( __xrt_der_next(&tSeq, &tField) > 0 ) {
		if ( tField.iType == 0xa0 ) {
			struct __xrt_der_tlv tInner, tOID;
			if ( __xrt_der_next(&tField, &tInner) <= 0 || tInner.iType != 0x30 ) return false;
			if ( __xrt_der_next(&tInner, &tOID) <= 0 ) return false;
			if ( !__xrt_x509_parse_hash_oid(&tOID, &iHashLen) ) return false;
		} else if ( tField.iType == 0xa1 ) {
			struct __xrt_der_tlv tInner, tOID, tHashAlg, tHashOID;
			if ( __xrt_der_next(&tField, &tInner) <= 0 || tInner.iType != 0x30 ) return false;
			if ( __xrt_der_next(&tInner, &tOID) <= 0 || tOID.iType != 0x06 ) return false;
			if ( tOID.iLen != sizeof(__xrt_oid_mgf1) ||
				memcmp(tOID.pValue, __xrt_oid_mgf1, sizeof(__xrt_oid_mgf1)) != 0 ) {
				return false;
			}
			if ( __xrt_der_next(&tInner, &tHashAlg) <= 0 || tHashAlg.iType != 0x30 ) return false;
			if ( __xrt_der_next(&tHashAlg, &tHashOID) <= 0 ) return false;
			if ( !__xrt_x509_parse_hash_oid(&tHashOID, &iMGFHashLen) ) return false;
		} else if ( tField.iType == 0xa2 ) {
			struct __xrt_der_tlv tInt;
			size_t i;
			if ( __xrt_der_next(&tField, &tInt) <= 0 || tInt.iType != 0x02 ) return false;
			for ( i = 0; i < tInt.iLen; i++ ) {
				iSaltLen = (iSaltLen << 8) | tInt.pValue[i];
			}
		}
	}

	if ( iHashLen == 0 ) return false;
	if ( iMGFHashLen == 0 ) iMGFHashLen = iHashLen;
	if ( iSaltLen == 0 ) iSaltLen = iHashLen;
	if ( iMGFHashLen != iHashLen || iSaltLen != iHashLen ) return false;

	if ( iHashLen == 32 ) *pSigAlg = __XRT_X509_SIGALG_RSA_PSS_SHA256;
	else if ( iHashLen == 48 ) *pSigAlg = __XRT_X509_SIGALG_RSA_PSS_SHA384;
	else if ( iHashLen == 64 ) *pSigAlg = __XRT_X509_SIGALG_RSA_PSS_SHA512;
	else return false;

	if ( pHashLen ) *pHashLen = iHashLen;
	return true;
}

static bool __xrt_x509_parse_signature_algorithm(const struct __xrt_der_tlv *pAlgSeq,
	enum __xrt_x509_sig_alg *pSigAlg, size_t *pHashLen)
{
	struct __xrt_der_tlv tSeq, tOID, tParams;

	if ( !pAlgSeq || pAlgSeq->iType != 0x30 || !pSigAlg || !pHashLen ) return false;
	*pSigAlg = __XRT_X509_SIGALG_UNKNOWN;
	*pHashLen = 0;
	tSeq = *pAlgSeq;

	if ( __xrt_der_next(&tSeq, &tOID) <= 0 || tOID.iType != 0x06 ) return false;
	if ( tOID.iLen == sizeof(__xrt_oid_sha256_with_rsa) &&
		memcmp(tOID.pValue, __xrt_oid_sha256_with_rsa, sizeof(__xrt_oid_sha256_with_rsa)) == 0 ) {
		*pSigAlg = __XRT_X509_SIGALG_RSA_PKCS1_SHA256;
		*pHashLen = 32;
		return true;
	}
	if ( tOID.iLen == sizeof(__xrt_oid_sha384_with_rsa) &&
		memcmp(tOID.pValue, __xrt_oid_sha384_with_rsa, sizeof(__xrt_oid_sha384_with_rsa)) == 0 ) {
		*pSigAlg = __XRT_X509_SIGALG_RSA_PKCS1_SHA384;
		*pHashLen = 48;
		return true;
	}
	if ( tOID.iLen == sizeof(__xrt_oid_sha512_with_rsa) &&
		memcmp(tOID.pValue, __xrt_oid_sha512_with_rsa, sizeof(__xrt_oid_sha512_with_rsa)) == 0 ) {
		*pSigAlg = __XRT_X509_SIGALG_RSA_PKCS1_SHA512;
		*pHashLen = 64;
		return true;
	}
	if ( tOID.iLen == sizeof(__xrt_oid_ecdsa_sha256) &&
		memcmp(tOID.pValue, __xrt_oid_ecdsa_sha256, sizeof(__xrt_oid_ecdsa_sha256)) == 0 ) {
		*pSigAlg = __XRT_X509_SIGALG_ECDSA_SHA256;
		*pHashLen = 32;
		return true;
	}
	if ( tOID.iLen == sizeof(__xrt_oid_ecdsa_sha384) &&
		memcmp(tOID.pValue, __xrt_oid_ecdsa_sha384, sizeof(__xrt_oid_ecdsa_sha384)) == 0 ) {
		*pSigAlg = __XRT_X509_SIGALG_ECDSA_SHA384;
		*pHashLen = 48;
		return true;
	}
	if ( tOID.iLen == sizeof(__xrt_oid_ecdsa_sha512) &&
		memcmp(tOID.pValue, __xrt_oid_ecdsa_sha512, sizeof(__xrt_oid_ecdsa_sha512)) == 0 ) {
		*pSigAlg = __XRT_X509_SIGALG_ECDSA_SHA512;
		*pHashLen = 64;
		return true;
	}
	if ( tOID.iLen == sizeof(__xrt_oid_ed25519) &&
		memcmp(tOID.pValue, __xrt_oid_ed25519, sizeof(__xrt_oid_ed25519)) == 0 ) {
		*pSigAlg = __XRT_X509_SIGALG_ED25519;
		*pHashLen = 0;
		return true;
	}
	if ( tOID.iLen == sizeof(__xrt_oid_rsassa_pss) &&
		memcmp(tOID.pValue, __xrt_oid_rsassa_pss, sizeof(__xrt_oid_rsassa_pss)) == 0 ) {
		if ( __xrt_der_next(&tSeq, &tParams) <= 0 ) return false;
		return __xrt_x509_parse_pss_params(&tParams, pSigAlg, pHashLen);
	}
	return false;
}

static void __xrt_x509_add_dns_name(struct __xrt_x509_cert *pCert, const uint8 *pName, size_t iNameLen)
{
	size_t iCopy;
	if ( !pCert || !pName || iNameLen == 0 ) return;
	if ( pCert->iDnsNameCount >= __XRT_TLS_MAX_DNS_NAMES ) return;
	iCopy = iNameLen;
	if ( iCopy >= sizeof(pCert->aDnsNames[0]) ) iCopy = sizeof(pCert->aDnsNames[0]) - 1;
	memcpy(pCert->aDnsNames[pCert->iDnsNameCount], pName, iCopy);
	pCert->aDnsNames[pCert->iDnsNameCount][iCopy] = '\0';
	pCert->iDnsNameCount++;
}

static bool __xrt_x509_parse_subject_alt_name(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tSeq, tName;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) return false;
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) return false;

	while ( __xrt_der_next(&tSeq, &tName) > 0 ) {
		if ( tName.iType == 0x82 ) {
			__xrt_x509_add_dns_name(pCert, tName.pValue, tName.iLen);
		}
	}
	return true;
}

static bool __xrt_x509_parse_key_usage(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tBitStr;
	size_t iBit;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) return false;
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tBitStr) < 0 || tBitStr.iType != 0x03 ) return false;
	if ( tBitStr.iLen < 2 ) return false;

	pCert->bHasKeyUsage = true;
	pCert->iKeyUsage = 0;
	for ( iBit = 0; iBit < 9; iBit++ ) {
		size_t iByte = 1 + (iBit / 8);
		uint8 iMask = (uint8)(0x80 >> (iBit % 8));
		if ( iByte < tBitStr.iLen && (tBitStr.pValue[iByte] & iMask) ) {
			pCert->iKeyUsage |= (uint16)(1u << iBit);
		}
	}
	return true;
}

static bool __xrt_x509_parse_basic_constraints(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tSeq, tField;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) return false;
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) return false;
	pCert->bHasBasicConstraints = true;
	pCert->bIsCA = false;

	if ( __xrt_der_next(&tSeq, &tField) > 0 ) {
		if ( tField.iType == 0x01 && tField.iLen == 1 ) {
			pCert->bIsCA = (tField.pValue[0] != 0);
		}
	}
	return true;
}

static bool __xrt_x509_parse_extended_key_usage(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tSeq, tOID;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) return false;
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) return false;
	pCert->bHasExtendedKeyUsage = true;
	pCert->bHasServerAuth = false;

	while ( __xrt_der_next(&tSeq, &tOID) > 0 ) {
		if ( tOID.iType == 0x06 && tOID.iLen == sizeof(__xrt_oid_server_auth) &&
			memcmp(tOID.pValue, __xrt_oid_server_auth, sizeof(__xrt_oid_server_auth)) == 0 ) {
			pCert->bHasServerAuth = true;
		}
	}
	return true;
}

static bool __xrt_x509_parse_spki(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pSPKI)
{
	struct __xrt_der_tlv tSPKI, tAlgId, tBitStr, tOID;

	if ( !pCert || !pSPKI || pSPKI->iType != 0x30 ) return false;
	tSPKI = *pSPKI;
	if ( __xrt_der_next(&tSPKI, &tAlgId) <= 0 || tAlgId.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tSPKI, &tBitStr) <= 0 || tBitStr.iType != 0x03 || tBitStr.iLen < 2 ) return false;

	{
		struct __xrt_der_tlv tAlg = tAlgId;
		if ( __xrt_der_next(&tAlg, &tOID) <= 0 || tOID.iType != 0x06 ) return false;

		bool bIsRSAPSS = tOID.iLen == sizeof(__xrt_oid_rsassa_pss) &&
			memcmp(tOID.pValue, __xrt_oid_rsassa_pss, sizeof(__xrt_oid_rsassa_pss)) == 0;
		if ( (tOID.iLen == sizeof(__xrt_oid_rsa_encryption) &&
			memcmp(tOID.pValue, __xrt_oid_rsa_encryption, sizeof(__xrt_oid_rsa_encryption)) == 0) ||
			bIsRSAPSS ) {
			struct __xrt_der_tlv tKeySeq, tMod, tExp;
			if ( __xrt_der_parse(tBitStr.pValue + 1, tBitStr.iLen - 1, &tKeySeq) < 0 || tKeySeq.iType != 0x30 ) {
				return false;
			}
			if ( __xrt_der_next(&tKeySeq, &tMod) <= 0 || tMod.iType != 0x02 ) return false;
			if ( __xrt_der_next(&tKeySeq, &tExp) <= 0 || tExp.iType != 0x02 ) return false;
			pCert->bIsECPubKey = false;
			pCert->bIsRSAPSSKey = bIsRSAPSS;
			pCert->pMod = tMod.pValue;
			pCert->iModSz = tMod.iLen;
			if ( pCert->iModSz > 0 && pCert->pMod[0] == 0x00 ) {
				pCert->pMod++;
				pCert->iModSz--;
			}
			pCert->pExp = tExp.pValue;
			pCert->iExpSz = tExp.iLen;
			return pCert->iModSz > 0 && pCert->iExpSz > 0;
		}

		if ( tOID.iLen == sizeof(__xrt_oid_ec_public_key) &&
			memcmp(tOID.pValue, __xrt_oid_ec_public_key, sizeof(__xrt_oid_ec_public_key)) == 0 ) {
			struct __xrt_der_tlv tCurveOID;
			bool bCurveOK = false;

			if ( __xrt_der_next(&tAlg, &tCurveOID) > 0 && tCurveOID.iType == 0x06 ) {
				if ( tCurveOID.iLen == sizeof(__xrt_oid_prime256v1) &&
					memcmp(tCurveOID.pValue, __xrt_oid_prime256v1, sizeof(__xrt_oid_prime256v1)) == 0 ) {
					bCurveOK = (tBitStr.iLen - 1) == 65;
				} else if ( tCurveOID.iLen == sizeof(__xrt_oid_secp384r1) &&
					memcmp(tCurveOID.pValue, __xrt_oid_secp384r1, sizeof(__xrt_oid_secp384r1)) == 0 ) {
					bCurveOK = (tBitStr.iLen - 1) == 97;
				}
			} else {
				bCurveOK = (tBitStr.iLen - 1) == 65 || (tBitStr.iLen - 1) == 97;
			}
			if ( !bCurveOK ) return false;
			pCert->bIsECPubKey = true;
			pCert->bIsEd25519Key = false;
			pCert->pECPub = tBitStr.pValue + 1;
			pCert->iECPubSz = tBitStr.iLen - 1;
			return pCert->iECPubSz > 0;
		}
		if ( tOID.iLen == sizeof(__xrt_oid_ed25519) &&
			memcmp(tOID.pValue, __xrt_oid_ed25519, sizeof(__xrt_oid_ed25519)) == 0 ) {
			if ( tBitStr.iLen != 33 || tBitStr.pValue[0] != 0x00 ) return false;
			pCert->bIsECPubKey = false;
			pCert->bIsEd25519Key = true;
			pCert->bIsRSAPSSKey = false;
			pCert->pEdPub = tBitStr.pValue + 1;
			pCert->iEdPubSz = 32;
			return true;
		}
	}
	return false;
}

static bool __xrt_x509_parse_extensions(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pExtsField)
{
	struct __xrt_der_tlv tExtField, tExtWrapper, tExtSeq, tExt;

	if ( !pCert || !pExtsField || pExtsField->iType != 0xa3 ) return false;
	tExtField = *pExtsField;
	if ( __xrt_der_next(&tExtField, &tExtWrapper) <= 0 || tExtWrapper.iType != 0x30 ) {
		return false;
	}

	tExtSeq = tExtWrapper;
	while ( __xrt_der_next(&tExtSeq, &tExt) > 0 ) {
		struct __xrt_der_tlv tItems = tExt;
		struct __xrt_der_tlv tOID, tMaybeCrit, tValue;
		bool bHaveValue = false;

		if ( tExt.iType != 0x30 ) continue;
		if ( __xrt_der_next(&tItems, &tOID) <= 0 || tOID.iType != 0x06 ) continue;
		if ( __xrt_der_next(&tItems, &tMaybeCrit) <= 0 ) continue;
		if ( tMaybeCrit.iType == 0x01 ) {
			if ( __xrt_der_next(&tItems, &tValue) <= 0 ) continue;
			bHaveValue = true;
		} else {
			tValue = tMaybeCrit;
			bHaveValue = true;
		}
		if ( !bHaveValue || tValue.iType != 0x04 ) continue;

		if ( tOID.iLen == sizeof(__xrt_oid_subject_alt_name) &&
			memcmp(tOID.pValue, __xrt_oid_subject_alt_name, sizeof(__xrt_oid_subject_alt_name)) == 0 ) {
			__xrt_x509_parse_subject_alt_name(pCert, &tValue);
		} else if ( tOID.iLen == sizeof(__xrt_oid_key_usage) &&
			memcmp(tOID.pValue, __xrt_oid_key_usage, sizeof(__xrt_oid_key_usage)) == 0 ) {
			__xrt_x509_parse_key_usage(pCert, &tValue);
		} else if ( tOID.iLen == sizeof(__xrt_oid_basic_constraints) &&
			memcmp(tOID.pValue, __xrt_oid_basic_constraints, sizeof(__xrt_oid_basic_constraints)) == 0 ) {
			__xrt_x509_parse_basic_constraints(pCert, &tValue);
		} else if ( tOID.iLen == sizeof(__xrt_oid_extended_key_usage) &&
			memcmp(tOID.pValue, __xrt_oid_extended_key_usage, sizeof(__xrt_oid_extended_key_usage)) == 0 ) {
			__xrt_x509_parse_extended_key_usage(pCert, &tValue);
		}
	}
	return true;
}

static bool __xrt_x509_parse(uint8 *pCertDer, size_t iCertLen, struct __xrt_x509_cert *pCert)
{
	struct __xrt_der_tlv tRoot, tTbs, tSigAlg, tSigValue;
	struct __xrt_der_tlv tFields, tField, tValidity, tTime;

	if ( !pCertDer || !pCert || iCertLen == 0 ) return false;
	memset(pCert, 0, sizeof(*pCert));
	pCert->pDer = pCertDer;
	pCert->iDerLen = iCertLen;

	if ( __xrt_der_parse(pCertDer, iCertLen, &tRoot) < 0 || tRoot.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tRoot, &tTbs) <= 0 || tTbs.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tRoot, &tSigAlg) <= 0 || tSigAlg.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tRoot, &tSigValue) <= 0 || tSigValue.iType != 0x03 || tSigValue.iLen < 2 ) return false;
	if ( !__xrt_x509_parse_signature_algorithm(&tSigAlg, &pCert->iSigAlg, &pCert->iSigHashLen) ) return false;

	pCert->pTbs = tTbs.pRaw;
	pCert->iTbsLen = tTbs.iTotalLen;
	pCert->pSig = tSigValue.pValue + 1;
	pCert->iSigLen = tSigValue.iLen - 1;

	tFields = tTbs;
	if ( __xrt_der_next(&tFields, &tField) <= 0 ) return false;
	if ( tField.iType == 0xa0 ) {
		if ( __xrt_der_next(&tFields, &tField) <= 0 ) return false;
	}

	// serialNumber
	if ( tField.iType != 0x02 ) return false;
	// signature
	if ( __xrt_der_next(&tFields, &tField) <= 0 ) return false;
	// issuer
	if ( __xrt_der_next(&tFields, &tField) <= 0 || tField.iType != 0x30 ) return false;
	pCert->pIssuerRaw = tField.pRaw;
	pCert->iIssuerRawLen = tField.iTotalLen;
	// validity
	if ( __xrt_der_next(&tFields, &tValidity) <= 0 || tValidity.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tValidity, &tTime) <= 0 ) return false;
	if ( !__xrt_x509_parse_time_value(&tTime, &pCert->iNotBefore) ) return false;
	if ( __xrt_der_next(&tValidity, &tTime) <= 0 ) return false;
	if ( !__xrt_x509_parse_time_value(&tTime, &pCert->iNotAfter) ) return false;
	pCert->bHasValidity = true;
	// subject
	if ( __xrt_der_next(&tFields, &tField) <= 0 || tField.iType != 0x30 ) return false;
	pCert->pSubjectRaw = tField.pRaw;
	pCert->iSubjectRawLen = tField.iTotalLen;
	pCert->bHasCommonName = __xrt_x509_parse_common_name(&tField, pCert->sCommonName, sizeof(pCert->sCommonName));
	// subjectPublicKeyInfo
	if ( __xrt_der_next(&tFields, &tField) <= 0 || tField.iType != 0x30 ) return false;
	if ( !__xrt_x509_parse_spki(pCert, &tField) ) return false;

	while ( __xrt_der_next(&tFields, &tField) > 0 ) {
		if ( tField.iType == 0xa3 ) {
			__xrt_x509_parse_extensions(pCert, &tField);
		}
	}

	return true;
}

static bool __xrt_x509_is_time_valid(const struct __xrt_x509_cert *pCert, time_t iNow)
{
	if ( !pCert || !pCert->bHasValidity ) return false;
	return iNow >= pCert->iNotBefore && iNow <= pCert->iNotAfter;
}

static bool __xrt_x509_name_eq(const uint8 *pA, size_t iALen, const uint8 *pB, size_t iBLen)
{
	return pA && pB && iALen == iBLen && memcmp(pA, pB, iALen) == 0;
}

static bool __xrt_x509_verify_signature(const struct __xrt_x509_cert *pChild, const struct __xrt_x509_cert *pIssuer)
{
	uint8 aHash[64];

	if ( !pChild || !pIssuer ) return false;

	switch ( pChild->iSigAlg ) {
		case __XRT_X509_SIGALG_RSA_PKCS1_SHA256:
		case __XRT_X509_SIGALG_RSA_PKCS1_SHA384:
		case __XRT_X509_SIGALG_RSA_PKCS1_SHA512:
			if ( !__xrt_hash_bytes(aHash, pChild->iSigHashLen, pChild->pTbs, pChild->iTbsLen) ) return false;
			if ( pIssuer->bIsECPubKey || !pIssuer->pMod || !pIssuer->pExp ) return false;
			return xrtRSAPKCS1Verify(aHash, pChild->iSigHashLen, pChild->pSig, pChild->iSigLen,
				pIssuer->pMod, pIssuer->iModSz, pIssuer->pExp, pIssuer->iExpSz);
		case __XRT_X509_SIGALG_RSA_PSS_SHA256:
		case __XRT_X509_SIGALG_RSA_PSS_SHA384:
		case __XRT_X509_SIGALG_RSA_PSS_SHA512:
			if ( !__xrt_hash_bytes(aHash, pChild->iSigHashLen, pChild->pTbs, pChild->iTbsLen) ) return false;
			if ( pIssuer->bIsECPubKey || !pIssuer->pMod || !pIssuer->pExp ) return false;
			return xrtRSAPSSVerify(aHash, pChild->iSigHashLen, pChild->pSig, pChild->iSigLen,
				pIssuer->pMod, pIssuer->iModSz, pIssuer->pExp, pIssuer->iExpSz);
		case __XRT_X509_SIGALG_ECDSA_SHA256:
		case __XRT_X509_SIGALG_ECDSA_SHA384:
		case __XRT_X509_SIGALG_ECDSA_SHA512:
			if ( !__xrt_hash_bytes(aHash, pChild->iSigHashLen, pChild->pTbs, pChild->iTbsLen) ) return false;
			if ( !pIssuer->bIsECPubKey || !pIssuer->pECPub ) return false;
			return xrtECDSAVerify(aHash, pChild->iSigHashLen, pChild->pSig, pChild->iSigLen,
				pIssuer->pECPub, pIssuer->iECPubSz);
		case __XRT_X509_SIGALG_ED25519:
			if ( !pIssuer->bIsEd25519Key || !pIssuer->pEdPub || pIssuer->iEdPubSz != 32 ) return false;
			return xrtEd25519Verify(pChild->pTbs, pChild->iTbsLen, pChild->pSig, pIssuer->pEdPub);
		default:
			return false;
	}
}

static bool __xrt_x509_is_ca_usable(const struct __xrt_x509_cert *pCert)
{
	if ( !pCert ) return false;
	if ( pCert->bHasBasicConstraints && !pCert->bIsCA ) return false;
	if ( pCert->bHasKeyUsage && (pCert->iKeyUsage & (1u << 5)) == 0 ) return false;
	return true;
}



/* ============================== TLS 上下文结构 ============================== */

// 握手状态机
enum __xrt_tls_state {
	// TLS 1.3 客户端状态
	XRT_TLS_CLIENT_START = 0,
	XRT_TLS_CLIENT_WAIT_SH,
	XRT_TLS_CLIENT_WAIT_EE,
	XRT_TLS_CLIENT_WAIT_CERT,
	XRT_TLS_CLIENT_WAIT_CV,
	XRT_TLS_CLIENT_WAIT_FINISH,
	XRT_TLS_CLIENT_CONNECTED,
	// TLS 1.2 客户端状态
	XRT_TLS12_CLIENT_WAIT_CERT,
	XRT_TLS12_CLIENT_WAIT_SKE,
	XRT_TLS12_CLIENT_WAIT_SHD,
	XRT_TLS12_CLIENT_WAIT_CCS,
	XRT_TLS12_CLIENT_WAIT_FINISH,
	XRT_TLS12_CLIENT_CONNECTED,
	// TLS 1.2 服务端状态
	XRT_TLS12_SERVER_WAIT_CKE,
	XRT_TLS12_SERVER_WAIT_CCS,
	XRT_TLS12_SERVER_WAIT_FINISH,
	// 服务端状态
	XRT_TLS_SERVER_START,
	XRT_TLS_SERVER_NEGOTIATED,
	XRT_TLS_SERVER_CONNECTED
};

// 加密密钥集
struct __xrt_tls_enc {
	uint64 iServerSeq;
	uint64 iClientSeq;
	uint8 aHandshakeSecret[64];
	uint8 aServerWriteKey[32];
	uint8 aServerWriteIV[12];
	uint8 aServerFinishedKey[64];
	uint8 aClientWriteKey[32];
	uint8 aClientWriteIV[12];
	uint8 aClientFinishedKey[64];
};

// TLS 上下文 (不透明结构)
struct xrt_tls_context {
	enum __xrt_tls_state iState;
	bool bIsServer;
	bool bIsTls12;             // 标记 TLS 1.2 模式
	bool bHandshakeDone;
	uint16 iCipherSuite;
	
	// TLS 1.3 握手数据
	xsha256_ctx tSHA256;           // TLS 1.3 握手 SHA-256 增量哈希
	xsha512_ctx tSHA384;           // TLS 1.3 握手 SHA-384 增量哈希
	uint8 aRandom[32];            // client random
	uint8 aSessionId[32];         // session ID
	uint8 aX25519Priv[32];        // X25519 私钥
	uint8 aX25519Pub[32];         // X25519 公钥
	uint8 aX448Priv[56];          // X448 私钥
	uint8 aX448Pub[56];           // X448 公钥
	uint8 aX25519Secret[56];      // TLS 1.3 共享密钥 (X25519 / P-256 / P-384 / X448)
	size_t iTls13SecretLen;
	uint8 aPeerKeyShare[97];      // 服务端: 客户端的 key_share
	size_t iPeerKeyShareLen;
	uint16 iTls13Group;           // TLS 1.3 协商的密钥组
	uint8 iSessionIdLen;
	
	// TLS 1.3 加密密钥
	uint16 iMaxVersion;
	bool bHaveResume;
	bool bSessionResumed;
	struct xrt_tls_resume tResume;
	struct __xrt_tls_enc tEnc;
	struct __xrt_tls_enc tAppKeys;  // 应用数据密钥
	
	// TLS 1.2 特有字段
	uint8 aServerRandom[32];      // 服务器随机数
	uint8 aMasterSecret[48];      // 主密钥
	uint8 aP256Priv[32];          // ECDHE P-256 私钥
	uint8 aP256Pub[65];           // ECDHE P-256 公钥
	uint8 aP384Priv[48];          // ECDHE P-384 私钥
	uint8 aP384Pub[97];           // ECDHE P-384 公钥
	uint8 aPreMaster[48];         // RSA 模式 pre_master_secret
	uint8 aSharedSecret[56];      // ECDHE 共享密钥
	size_t iSharedSecretLen;
	uint8 aClientWriteKey12[32];  // 客户端写密钥 (16 or 32)
	uint8 aServerWriteKey12[32];  // 服务端写密钥 (16 or 32)
	uint8 aClientWriteIV12[12];   // 客户端隐式 IV (GCM:4, ChaCha20:12)
	uint8 aServerWriteIV12[12];   // 服务端隐式 IV (GCM:4, ChaCha20:12)
	uint16 iKeyLen;               // 密钥长度 (16=AES128, 32=AES256)
	uint64 iClientSeq12;          // TLS 1.2 客户端序列号
	uint64 iServerSeq12;          // TLS 1.2 服务端序列号
	xsha256_ctx tSHA256_12;       // TLS 1.2 握手 SHA-256 哈希
	xsha512_ctx tSHA384_12;       // TLS 1.2 握手 SHA-384 哈希
	bool bUseSHA384;              // 当前套件是否使用 SHA-384
	bool bTls12UseChaCha;         // TLS 1.2 是否使用 ChaCha20-Poly1305
	bool bIsECDHE;                // 是否为 ECDHE 密钥交换
	uint16 iTls12Curve;           // TLS 1.2 ECDHE 曲线类型
	
	// 配置
	bool bSkipVerify;
	bool bAllowTLS12Ed25519;
	char sHostname[254];
	
	// 服务端 SNI
	char sClientSNI[254];         // 服务端: 客户端请求的 SNI hostname
	void (*OnSNI)(xtlssession *pSession, const char *sHostName, ptr pUserData);
	ptr pSNIUserData;
	xtlssession* pSession;
	
	// 证书数据
	uint8 *pCertDer;
	size_t iCertDerLen;
	uint8 *pKeyDer;
	size_t iKeyDerLen;
	uint8 *pCaData;
	size_t iCaDataLen;
	uint8 aECKey[48];             // EC 私钥
	uint8 aEd25519Key[32];        // Ed25519 seed
	size_t iECKeyLen;
	uint8 aRSAPrivExp[512];       // RSA 私钥指数 d
	size_t iRSAPrivExpSz;
	bool bHasECPrivKey;
	bool bHasEd25519Priv;
	bool bHasRSAPrivKey;
	uint16 iServerSigAlg;
	bool bPeerSigECDSAP256;
	bool bPeerSigECDSAP384;
	bool bPeerSigEd25519;
	bool bPeerSigRSAPSS256;
	bool bPeerSigRSAPSS384;
	bool bPeerSigRSAPSS512;
	bool bPeerSigRSAPSSPSS256;
	bool bPeerSigRSAPSSPSS384;
	bool bPeerSigRSAPSSPSS512;
	bool bPeerSecureReneg;
	
	// 服务器证书公钥 (RSA 或 EC)
	bool bIsECPubKey;             // true=EC, false=RSA
	bool bIsEd25519Key;
	bool bIsRSAPSSKey;
	uint8 aPubKey[512 + 16];      // RSA 公钥 (modulus + exp) 或 EC 公钥
	size_t iPubKeySz;             // 公钥总大小
	size_t iPubKeyModSz;          // RSA modulus 大小
	uint8 aSigHash[64];           // CertificateVerify 使用的 transcript hash
	size_t iSigHashLen;
	
	// IO 缓冲区
	__xrt_tls_buf tSendBuf;
	__xrt_tls_buf tRecvBuf;
	__xrt_tls_buf tHandshakeBuf;        // TLS 1.3 握手消息重组缓冲区 (用于跨记录的大消息)
	__xrt_tls_buf tPlainBuf;            // 已解密但尚未被应用层消费的明文
	size_t iRecvOffset;
	size_t iRecvMsgLen;
	uint8 iContentType;
};

static bool __xrt_tls_resume_from_ctx(xtlsctx* pCtx, struct xrt_tls_resume* pOut)
{
	if ( !pCtx || !pOut ) return false;
	if ( !pCtx->bHandshakeDone || !pCtx->bIsTls12 ) return false;
	if ( pCtx->iSessionIdLen == 0 || pCtx->iSessionIdLen > sizeof(pOut->aSessionId) ) return false;
	memset(pOut, 0, sizeof(*pOut));
	pOut->iVersion = __XRT_TLS_VERSION_1_2;
	pOut->iCipherSuite = pCtx->iCipherSuite;
	pOut->iSessionIdLen = pCtx->iSessionIdLen;
	memcpy(pOut->aSessionId, pCtx->aSessionId, pCtx->iSessionIdLen);
	memcpy(pOut->aMasterSecret, pCtx->aMasterSecret, sizeof(pOut->aMasterSecret));
	return true;
}

// SHA-256("") 预计算
static const uint8 __xrt_tls_zeros_sha256[32] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
	0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
	0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

static bool __xrt_tls_hash_bytes(uint8 *pOut, size_t iHashLen, const uint8 *pData, size_t iLen)
{
	if ( iHashLen == 32 ) {
		xrtSHA256((const ptr)pData, iLen, pOut);
		return true;
	}
	if ( iHashLen == 48 ) {
		xrtSHA384((const ptr)pData, iLen, pOut);
		return true;
	}
	if ( iHashLen == 64 ) {
		xrtSHA512((const ptr)pData, iLen, pOut);
		return true;
	}
	return false;
}

static size_t __xrt_tls_sigalg_hash_len(uint16 iSigAlg)
{
	switch ( iSigAlg ) {
		case 0x0807:
			return 0;
		case 0x0401:
		case 0x0403:
		case 0x0804:
		case 0x0809:
			return 32;
		case 0x0501:
		case 0x0503:
		case 0x0805:
		case 0x080a:
			return 48;
		case 0x0601:
		case 0x0603:
		case 0x0806:
		case 0x080b:
			return 64;
		default:
			switch ( (uint8)(iSigAlg >> 8) ) {
				case 8: return 0;
				case 4: return 32;
				case 5: return 48;
				case 6: return 64;
				default: return 0;
			}
	}
}

static size_t __xrt_tls13_get_hash_len(uint16 iCipherSuite)
{
	return (iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384) ? 48 : 32;
}

static size_t __xrt_tls13_get_key_len(uint16 iCipherSuite)
{
	return (iCipherSuite == __XRT_TLS_AES_128_GCM_SHA256) ? 16 : 32;
}

static bool __xrt_tls13_is_supported_cipher(uint16 iCipherSuite)
{
	return iCipherSuite == __XRT_TLS_AES_128_GCM_SHA256
		|| iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384
		|| iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256;
}

static bool __xrt_tls12_set_cipher_params(xtlsctx *pCtx, uint16 iCipherSuite)
{
	if ( !pCtx ) return false;

	pCtx->iCipherSuite = iCipherSuite;
	pCtx->bIsTls12 = true;
	pCtx->bTls12UseChaCha = false;

	switch ( iCipherSuite ) {
		case __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256:
		case __XRT_TLS12_RSA_AES128_GCM_SHA256:
			pCtx->bUseSHA384 = false;
			pCtx->iKeyLen = 16;
			break;
		case __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384:
		case __XRT_TLS12_RSA_AES256_GCM_SHA384:
			pCtx->bUseSHA384 = true;
			pCtx->iKeyLen = 32;
			break;
		case __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256:
		case __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256:
			pCtx->bUseSHA384 = false;
			pCtx->bTls12UseChaCha = true;
			pCtx->iKeyLen = 32;
			break;
		default:
			return false;
	}

	switch ( iCipherSuite ) {
		case __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256:
		case __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256:
			pCtx->bIsECDHE = true;
			break;
		default:
			pCtx->bIsECDHE = false;
			break;
	}

	return true;
}

static size_t __xrt_tls12_get_iv_len(const xtlsctx *pCtx)
{
	return pCtx->bTls12UseChaCha ? 12 : 4;
}

static bool __xrt_tls_consttime_equal(const uint8 *pA, const uint8 *pB, size_t iLen)
{
	uint8 iDiff = 0;
	size_t i;
	for ( i = 0; i < iLen; i++ ) iDiff |= pA[i] ^ pB[i];
	return iDiff == 0;
}

static bool __xrt_tls_validate_leaf_server_cert(xtlsctx *pCtx, const struct __xrt_x509_cert *pLeaf)
{
	size_t i;
	time_t iNow = time(NULL);
	bool bHostMatched = false;

	if ( !pCtx || !pLeaf || !__xrt_x509_is_time_valid(pLeaf, iNow) ) return false;
	if ( pLeaf->bHasBasicConstraints && pLeaf->bIsCA ) return false;
	if ( pLeaf->bHasKeyUsage && (pLeaf->iKeyUsage & (1u << 0)) == 0 ) return false;
	if ( pLeaf->bHasExtendedKeyUsage && !pLeaf->bHasServerAuth ) return false;

	if ( pCtx->sHostname[0] ) {
		if ( pLeaf->iDnsNameCount > 0 ) {
			for ( i = 0; i < pLeaf->iDnsNameCount; i++ ) {
				if ( __xrt_tls_hostname_matches(pLeaf->aDnsNames[i], pCtx->sHostname) ) {
					bHostMatched = true;
					break;
				}
			}
		} else if ( pLeaf->bHasCommonName ) {
			bHostMatched = __xrt_tls_hostname_matches(pLeaf->sCommonName, pCtx->sHostname);
		} else {
			return false;
		}
		if ( !bHostMatched ) return false;
	}

	return true;
}

static bool __xrt_tls_copy_pubkey_from_cert(xtlsctx *pCtx, const struct __xrt_x509_cert *pCert)
{
	if ( !pCtx || !pCert ) return false;

	pCtx->iPubKeySz = 0;
	pCtx->iPubKeyModSz = 0;
	if ( pCert->bIsECPubKey ) {
		if ( pCert->iECPubSz == 0 || pCert->iECPubSz > sizeof(pCtx->aPubKey) ) return false;
		pCtx->bIsECPubKey = true;
		pCtx->bIsEd25519Key = false;
		pCtx->bIsRSAPSSKey = false;
		memcpy(pCtx->aPubKey, pCert->pECPub, pCert->iECPubSz);
		pCtx->iPubKeySz = pCert->iECPubSz;
		return true;
	}
	if ( pCert->bIsEd25519Key ) {
		if ( pCert->iEdPubSz != 32 ) return false;
		pCtx->bIsECPubKey = false;
		pCtx->bIsEd25519Key = true;
		pCtx->bIsRSAPSSKey = false;
		memcpy(pCtx->aPubKey, pCert->pEdPub, 32);
		pCtx->iPubKeySz = 32;
		return true;
	}

	if ( !pCert->pMod || !pCert->pExp || pCert->iModSz + pCert->iExpSz > sizeof(pCtx->aPubKey) ) return false;
	pCtx->bIsECPubKey = false;
	pCtx->bIsEd25519Key = false;
	pCtx->bIsRSAPSSKey = pCert->bIsRSAPSSKey;
	memcpy(pCtx->aPubKey, pCert->pMod, pCert->iModSz);
	memcpy(pCtx->aPubKey + pCert->iModSz, pCert->pExp, pCert->iExpSz);
	pCtx->iPubKeyModSz = pCert->iModSz;
	pCtx->iPubKeySz = pCert->iModSz + pCert->iExpSz;
	return true;
}

static bool __xrt_tls_decode_pem_cert_block(const char *pStart, const char *pEnd, uint8 **ppDer, size_t *pDerLen)
{
	size_t iSrcLen;
	size_t iB64Len = 0;
	char *pB64;
	uint8 *pDer;

	if ( !pStart || !pEnd || !ppDer || !pDerLen || pEnd <= pStart ) return false;
	iSrcLen = (size_t)(pEnd - pStart);
	pB64 = (char*)xrtMalloc(iSrcLen + 5);
	if ( !pB64 ) return false;

	for ( size_t i = 0; i < iSrcLen; i++ ) {
		if ( pStart[i] != '\r' && pStart[i] != '\n' && pStart[i] != ' ' && pStart[i] != '\t' ) {
			pB64[iB64Len++] = pStart[i];
		}
	}
	while ( iB64Len % 4 != 0 ) pB64[iB64Len++] = '=';
	pB64[iB64Len] = '\0';

	pDer = (uint8*)xrtBase64Decode(pB64, iB64Len, NULL);
	if ( !pDer || pDer == (uint8*)xCore.sNull ) {
		xrtFree(pB64);
		return false;
	}

	*pDerLen = (iB64Len / 4) * 3;
	if ( iB64Len > 0 && pB64[iB64Len - 1] == '=' ) (*pDerLen)--;
	if ( iB64Len > 1 && pB64[iB64Len - 2] == '=' ) (*pDerLen)--;
	*ppDer = pDer;
	xrtFree(pB64);
	return true;
}

static bool __xrt_tls_ca_bundle_next(xtlsctx *pCtx, size_t *pOffset, uint8 **ppDer, size_t *pDerLen, bool *pOwned)
{
	const char *pData;
	const char *pBegin;
	const char *pBody;
	const char *pEnd;

	if ( !pCtx || !pOffset || !ppDer || !pDerLen || !pOwned ) return false;
	if ( !pCtx->pCaData || pCtx->iCaDataLen == 0 || *pOffset >= pCtx->iCaDataLen ) return false;

	pData = (const char*)pCtx->pCaData;
	if ( pCtx->iCaDataLen >= 11 && memcmp(pData, "-----BEGIN ", 11) == 0 ) {
		pBegin = strstr(pData + *pOffset, "-----BEGIN CERTIFICATE-----");
		if ( !pBegin ) return false;
		pBody = strchr(pBegin, '\n');
		if ( !pBody ) return false;
		pBody++;
		pEnd = strstr(pBody, "-----END CERTIFICATE-----");
		if ( !pEnd ) return false;
		if ( !__xrt_tls_decode_pem_cert_block(pBody, pEnd, ppDer, pDerLen) ) return false;
		*pOwned = true;
		*pOffset = (size_t)((pEnd - pData) + strlen("-----END CERTIFICATE-----"));
		return true;
	}

	if ( *pOffset != 0 ) return false;
	*ppDer = pCtx->pCaData;
	*pDerLen = pCtx->iCaDataLen;
	*pOwned = false;
	*pOffset = pCtx->iCaDataLen;
	return true;
}

static bool __xrt_tls_verify_presented_chain(xtlsctx *pCtx, uint8 **apCertData, size_t *apCertLen, size_t iCertCount)
{
	struct __xrt_x509_cert aCerts[__XRT_TLS_MAX_CERT_CHAIN];
	size_t i;
	size_t iCurrent;
	time_t iNow = time(NULL);

	if ( !pCtx || !apCertData || !apCertLen || iCertCount == 0 || iCertCount > __XRT_TLS_MAX_CERT_CHAIN ) {
		return false;
	}

	for ( i = 0; i < iCertCount; i++ ) {
		if ( !__xrt_x509_parse(apCertData[i], apCertLen[i], &aCerts[i]) ) return false;
	}
	if ( !__xrt_tls_validate_leaf_server_cert(pCtx, &aCerts[0]) ) return false;
	if ( !__xrt_tls_copy_pubkey_from_cert(pCtx, &aCerts[0]) ) return false;

	iCurrent = 0;
	for ( i = 0; i < iCertCount; i++ ) {
		size_t j;
		bool bFoundIssuer = false;

		if ( __xrt_x509_name_eq(aCerts[iCurrent].pIssuerRaw, aCerts[iCurrent].iIssuerRawLen,
			aCerts[iCurrent].pSubjectRaw, aCerts[iCurrent].iSubjectRawLen) ) {
			break;
		}

		for ( j = iCurrent + 1; j < iCertCount; j++ ) {
			if ( __xrt_x509_name_eq(aCerts[iCurrent].pIssuerRaw, aCerts[iCurrent].iIssuerRawLen,
				aCerts[j].pSubjectRaw, aCerts[j].iSubjectRawLen) ) {
				if ( !__xrt_x509_is_ca_usable(&aCerts[j]) ) return false;
				if ( !__xrt_x509_is_time_valid(&aCerts[j], iNow) ) return false;
				if ( !__xrt_x509_verify_signature(&aCerts[iCurrent], &aCerts[j]) ) return false;
				iCurrent = j;
				bFoundIssuer = true;
				break;
			}
		}
		if ( bFoundIssuer ) continue;

		{
			size_t iOffset = 0;
			while ( iOffset < pCtx->iCaDataLen ) {
				uint8 *pAnchorDer = NULL;
				size_t iAnchorLen = 0;
				bool bOwned = false;
				struct __xrt_x509_cert tAnchor;
				bool bOK = false;

				if ( !__xrt_tls_ca_bundle_next(pCtx, &iOffset, &pAnchorDer, &iAnchorLen, &bOwned) ) break;
				if ( __xrt_x509_parse(pAnchorDer, iAnchorLen, &tAnchor) &&
					__xrt_x509_name_eq(aCerts[iCurrent].pIssuerRaw, aCerts[iCurrent].iIssuerRawLen,
						tAnchor.pSubjectRaw, tAnchor.iSubjectRawLen) &&
					__xrt_x509_is_ca_usable(&tAnchor) &&
					__xrt_x509_is_time_valid(&tAnchor, iNow) &&
					__xrt_x509_verify_signature(&aCerts[iCurrent], &tAnchor) ) {
					bOK = true;
				}
				if ( bOwned ) xrtFree(pAnchorDer);
				if ( bOK ) return true;
			}
		}
		return false;
	}

	if ( pCtx->iCaDataLen == 0 ) return false;
	{
		size_t iOffset = 0;
		while ( iOffset < pCtx->iCaDataLen ) {
			uint8 *pAnchorDer = NULL;
			size_t iAnchorLen = 0;
			bool bOwned = false;
			struct __xrt_x509_cert tAnchor;
			bool bTrusted = false;

			if ( !__xrt_tls_ca_bundle_next(pCtx, &iOffset, &pAnchorDer, &iAnchorLen, &bOwned) ) break;
			if ( __xrt_x509_parse(pAnchorDer, iAnchorLen, &tAnchor) &&
				__xrt_x509_is_ca_usable(&tAnchor) &&
				__xrt_x509_is_time_valid(&tAnchor, iNow) &&
				__xrt_x509_name_eq(aCerts[iCurrent].pSubjectRaw, aCerts[iCurrent].iSubjectRawLen,
					tAnchor.pSubjectRaw, tAnchor.iSubjectRawLen) &&
				__xrt_x509_name_eq(aCerts[iCurrent].pIssuerRaw, aCerts[iCurrent].iIssuerRawLen,
					tAnchor.pIssuerRaw, tAnchor.iIssuerRawLen) &&
				__xrt_x509_verify_signature(&aCerts[iCurrent], &tAnchor) ) {
				bTrusted = true;
			}
			if ( bOwned ) xrtFree(pAnchorDer);
			if ( bTrusted ) return true;
		}
	}

	return false;
}

static bool __xrt_tls_capture_peer_cert_chain(xtlsctx *pCtx, uint8 **apCertData, size_t *apCertLen, size_t iCertCount)
{
	struct __xrt_x509_cert tLeaf;

	if ( !pCtx || !apCertData || !apCertLen || iCertCount == 0 ) return false;
	if ( pCtx->bSkipVerify ) {
		if ( !__xrt_x509_parse(apCertData[0], apCertLen[0], &tLeaf) ) return false;
		return __xrt_tls_copy_pubkey_from_cert(pCtx, &tLeaf);
	}
	if ( pCtx->iCaDataLen == 0 ) return false;
	return __xrt_tls_verify_presented_chain(pCtx, apCertData, apCertLen, iCertCount);
}

static bool __xrt_tls_load_file_copy(const char *sFile, uint8 **ppData, size_t *pLen)
{
	size_t iFileSize = 0;
	uint8 *pFileData;
	uint8 *pCopy;

	if ( !sFile || !sFile[0] || !ppData || !pLen ) return false;
	pFileData = (uint8*)xrtFileGetAll((str)sFile, &iFileSize);
	if ( !pFileData || pFileData == (uint8*)xCore.sNull ) return false;

	pCopy = (uint8*)xrtMalloc(iFileSize);
	if ( !pCopy ) {
		xrtFree(pFileData);
		return false;
	}
	memcpy(pCopy, pFileData, iFileSize);
	xrtFree(pFileData);
	*ppData = pCopy;
	*pLen = iFileSize;
	return true;
}

static bool __xrt_tls_load_der_file(const char *sFile, uint8 **ppDer, size_t *pDerLen)
{
	size_t iFileSize = 0;
	uint8 *pFileData;

	if ( !sFile || !sFile[0] || !ppDer || !pDerLen ) return false;
	*ppDer = NULL;
	*pDerLen = 0;

	pFileData = (uint8*)xrtFileGetAll((str)sFile, &iFileSize);
	if ( !pFileData || pFileData == (uint8*)xCore.sNull ) return false;

	if ( iFileSize > 27 && memcmp(pFileData, "-----BEGIN ", 11) == 0 ) {
		const char *pStart = strstr((char*)pFileData, "\n");
		const char *pEnd;
		bool bOK;

		if ( !pStart ) {
			xrtFree(pFileData);
			return false;
		}
		pStart++;
		pEnd = strstr(pStart, "-----END ");
		if ( !pEnd ) {
			xrtFree(pFileData);
			return false;
		}

		bOK = __xrt_tls_decode_pem_cert_block(pStart, pEnd, ppDer, pDerLen);
		xrtFree(pFileData);
		return bOK && *ppDer && *pDerLen > 0;
	}

	{
		uint8 *pCopy = (uint8*)xrtMalloc(iFileSize);
		if ( !pCopy ) {
			xrtFree(pFileData);
			return false;
		}
		memcpy(pCopy, pFileData, iFileSize);
		xrtFree(pFileData);
		*ppDer = pCopy;
		*pDerLen = iFileSize;
		return true;
	}
}

static bool __xrt_tls_append_pem_cert(__xrt_tls_buf* pBuf, const uint8* pDer, size_t iDerLen)
{
	static const char sBegin[] = "-----BEGIN CERTIFICATE-----\n";
	static const char sEnd[] = "-----END CERTIFICATE-----\n";
	str sBase64;
	size_t iBase64Len;
	size_t iOffset;

	if ( !pBuf || !pDer || iDerLen == 0 ) return false;

	sBase64 = xrtBase64Encode((ptr)pDer, iDerLen, NULL);
	if ( !sBase64 || sBase64 == xCore.sNull ) return false;

	iBase64Len = strlen(sBase64);
	if ( !__xrt_tls_buf_append(pBuf, sBegin, sizeof(sBegin) - 1) ) {
		xrtFree(sBase64);
		return false;
	}

	for ( iOffset = 0; iOffset < iBase64Len; iOffset += 64 ) {
		size_t iChunk = iBase64Len - iOffset;
		if ( iChunk > 64 ) iChunk = 64;
		if ( !__xrt_tls_buf_append(pBuf, sBase64 + iOffset, iChunk)
			|| !__xrt_tls_buf_append(pBuf, "\n", 1) ) {
			xrtFree(sBase64);
			return false;
		}
	}

	xrtFree(sBase64);
	return __xrt_tls_buf_append(pBuf, sEnd, sizeof(sEnd) - 1);
}

#if defined(_WIN32) || defined(_WIN64)
static bool __xrt_tls_load_windows_root_store(xtlsctx *pCtx)
{
	typedef HCERTSTORE (WINAPI *procCertOpenStore_t)(LPCSTR, DWORD, ULONG_PTR, DWORD, const void*);
	typedef PCCERT_CONTEXT (WINAPI *procCertEnumCertificatesInStore_t)(HCERTSTORE, PCCERT_CONTEXT);
	typedef BOOL (WINAPI *procCertCloseStore_t)(HCERTSTORE, DWORD);

	static procCertOpenStore_t procCertOpenStore = NULL;
	static procCertEnumCertificatesInStore_t procCertEnumCertificatesInStore = NULL;
	static procCertCloseStore_t procCertCloseStore = NULL;
	static bool bCrypt32Loaded = false;
	static const DWORD arrStoreFlags[] = {
		CERT_SYSTEM_STORE_CURRENT_USER,
		CERT_SYSTEM_STORE_LOCAL_MACHINE
	};

	__xrt_tls_buf tPemBuf;
	HCERTSTORE hStore = NULL;
	PCCERT_CONTEXT pCert = NULL;
	size_t i;
	bool bAnyLoaded = false;

	if ( !pCtx ) return false;

	if ( !bCrypt32Loaded ) {
		HMODULE hLib = LoadLibraryA("crypt32.dll");
		if ( hLib ) {
			procCertOpenStore = (procCertOpenStore_t)GetProcAddress(hLib, "CertOpenStore");
			procCertEnumCertificatesInStore =
				(procCertEnumCertificatesInStore_t)GetProcAddress(hLib, "CertEnumCertificatesInStore");
			procCertCloseStore = (procCertCloseStore_t)GetProcAddress(hLib, "CertCloseStore");
		}
		bCrypt32Loaded = true;
	}

	if ( !procCertOpenStore || !procCertEnumCertificatesInStore || !procCertCloseStore ) {
		return false;
	}

	if ( !__xrt_tls_buf_init(&tPemBuf, 4096) ) {
		return false;
	}

	for ( i = 0; i < sizeof(arrStoreFlags) / sizeof(arrStoreFlags[0]); i++ ) {
		hStore = procCertOpenStore(
			CERT_STORE_PROV_SYSTEM_A,
			0,
			0,
			CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG | arrStoreFlags[i],
			"ROOT");
		if ( !hStore ) continue;

		pCert = NULL;
		while ( (pCert = procCertEnumCertificatesInStore(hStore, pCert)) != NULL ) {
			if ( !pCert->pbCertEncoded || pCert->cbCertEncoded == 0 ) continue;
			if ( !__xrt_tls_append_pem_cert(&tPemBuf, pCert->pbCertEncoded, pCert->cbCertEncoded) ) {
				procCertCloseStore(hStore, 0);
				__xrt_tls_buf_free(&tPemBuf);
				return false;
			}
			bAnyLoaded = true;
		}

		procCertCloseStore(hStore, 0);
		hStore = NULL;
	}

	if ( !bAnyLoaded || !__xrt_tls_buf_append(&tPemBuf, "\0", 1) ) {
		__xrt_tls_buf_free(&tPemBuf);
		return false;
	}

	pCtx->pCaData = (uint8*)tPemBuf.pBase;
	pCtx->iCaDataLen = tPemBuf.iSize - 1;
	tPemBuf.pBase = NULL;
	tPemBuf.pData = NULL;
	tPemBuf.iSize = 0;
	tPemBuf.iCapacity = 0;
	return true;
}
#endif

static bool __xrt_tls_load_ca_bundle(xtlsctx *pCtx, const char *sCaFile)
{
	static const char *aDefaultPaths[] = {
		"/etc/ssl/certs/ca-certificates.crt",
		"/etc/pki/tls/certs/ca-bundle.crt",
		"/etc/ssl/cert.pem",
		"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem"
	};
	const char *sEnvPath = NULL;
	size_t i;

	if ( !pCtx ) return false;
	if ( pCtx->pCaData ) {
		xrtFree(pCtx->pCaData);
		pCtx->pCaData = NULL;
		pCtx->iCaDataLen = 0;
	}

	if ( sCaFile && sCaFile[0] ) {
		return __xrt_tls_load_file_copy(sCaFile, &pCtx->pCaData, &pCtx->iCaDataLen);
	}

	sEnvPath = getenv("SSL_CERT_FILE");
	if ( (!sEnvPath || !sEnvPath[0]) ) sEnvPath = getenv("CURL_CA_BUNDLE");
	if ( (!sEnvPath || !sEnvPath[0]) ) sEnvPath = getenv("REQUESTS_CA_BUNDLE");
	if ( sEnvPath && sEnvPath[0] && __xrt_tls_load_file_copy(sEnvPath, &pCtx->pCaData, &pCtx->iCaDataLen) ) {
		return true;
	}

	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrt_tls_load_windows_root_store(pCtx) ) {
			return true;
		}
	#else
		for ( i = 0; i < sizeof(aDefaultPaths) / sizeof(aDefaultPaths[0]); i++ ) {
			if ( xrtFileExists((str)aDefaultPaths[i]) &&
				__xrt_tls_load_file_copy(aDefaultPaths[i], &pCtx->pCaData, &pCtx->iCaDataLen) ) {
				return true;
			}
		}
	#endif
	return false;
}

static void __xrt_tls13_hash_update(xtlsctx *pCtx, const uint8 *pData, size_t iLen)
{
	xrtSHA256Update(&pCtx->tSHA256, (ptr)pData, iLen);
	xrtSHA512Update(&pCtx->tSHA384, (const ptr)pData, iLen);
}

static void __xrt_tls13_get_transcript_hash(xtlsctx *pCtx, uint8 *pOut, size_t *pHashLen)
{
	size_t iHashLen = __xrt_tls13_get_hash_len(pCtx->iCipherSuite);
	if ( iHashLen == 48 ) {
		xsha512_ctx tCopy = pCtx->tSHA384;
		xrtSHA384Final(&tCopy, pOut);
	} else {
		xsha256_ctx tCopy = pCtx->tSHA256;
		xrtSHA256Final(&tCopy, pOut);
	}
	if ( pHashLen ) *pHashLen = iHashLen;
}

static bool __xrt_tls13_hkdf_extract(uint8 *pOut, const uint8 *pSalt, size_t iSaltLen,
	const uint8 *pIKM, size_t iIKMLen, size_t iHashLen)
{
	if ( iHashLen == 48 ) {
		xrtHKDFExtract_SHA384(pOut, pSalt, iSaltLen, pIKM, iIKMLen);
		return true;
	}
	if ( iHashLen == 32 ) {
		xrtHKDFExtract(pOut, pSalt, iSaltLen, pIKM, iIKMLen);
		return true;
	}
	return false;
}

static bool __xrt_tls13_hkdf_expand(uint8 *pOut, size_t iOutLen, const uint8 *pPRK,
	size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen, size_t iHashLen)
{
	if ( iHashLen == 48 ) {
		xrtHKDFExpand_SHA384(pOut, iOutLen, pPRK, iPRKLen, pInfo, iInfoLen);
		return true;
	}
	if ( iHashLen == 32 ) {
		xrtHKDFExpand(pOut, iOutLen, pPRK, iPRKLen, pInfo, iInfoLen);
		return true;
	}
	return false;
}

static bool __xrt_tls13_hmac(uint8 *pOut, const uint8 *pKey, size_t iKeyLen,
	const uint8 *pMsg, size_t iMsgLen, size_t iHashLen)
{
	if ( iHashLen == 48 ) {
		xrtHMAC_SHA384(pKey, iKeyLen, pMsg, iMsgLen, pOut);
		return true;
	}
	if ( iHashLen == 32 ) {
		xrtHMAC_SHA256(pKey, iKeyLen, pMsg, iMsgLen, pOut);
		return true;
	}
	return false;
}

static void __xrt_tls_xor_seq_nonce(uint8 *pNonce, const uint8 *pIV, uint64 iSeq)
{
	uint8 aSeq[12] = {0};
	memcpy(pNonce, pIV, 12);
	__xrt_tls_store_be64(aSeq + 4, iSeq);
	pNonce[0] ^= aSeq[0];
	pNonce[1] ^= aSeq[1];
	pNonce[2] ^= aSeq[2];
	pNonce[3] ^= aSeq[3];
	pNonce[4] ^= aSeq[4];
	pNonce[5] ^= aSeq[5];
	pNonce[6] ^= aSeq[6];
	pNonce[7] ^= aSeq[7];
	pNonce[8] ^= aSeq[8];
	pNonce[9] ^= aSeq[9];
	pNonce[10] ^= aSeq[10];
	pNonce[11] ^= aSeq[11];
}



/* ============================== TLS 1.2 PRF ============================== */

// TLS 1.2 PRF (RFC 5246 Section 5):
// A(0) = seed, A(i) = HMAC(secret, A(i-1))
// P_hash = HMAC(secret, A(1)+seed) || HMAC(secret, A(2)+seed) || ...

static void __xrt_tls12_prf_sha256(uint8 *pOut, size_t iOutLen,
	const uint8 *pSecret, size_t iSecretLen,
	const char *pLabel, size_t iLabelLen,
	const uint8 *pSeed, size_t iSeedLen)
{
	uint8 aA[32];  // A(i), SHA-256 输出 32 字节
	uint8 aP[32];  // P_hash 当前块
	
	// 组合 seed = label + seed
	uint8 aFullSeed[256];
	size_t iFullLen = iLabelLen + iSeedLen;
	memcpy(aFullSeed, pLabel, iLabelLen);
	memcpy(aFullSeed + iLabelLen, pSeed, iSeedLen);
	
	// A(1) = HMAC(secret, A(0)) = HMAC(secret, full_seed)
	xrtHMAC_SHA256(pSecret, iSecretLen, aFullSeed, iFullLen, aA);
	
	size_t iDone = 0;
	while ( iDone < iOutLen ) {
		// HMAC(secret, A(i) + full_seed)
		uint8 aBuf[32 + 256];  // A(i)(32) + full_seed
		memcpy(aBuf, aA, 32);
		memcpy(aBuf + 32, aFullSeed, iFullLen);
		xrtHMAC_SHA256(pSecret, iSecretLen, aBuf, 32 + iFullLen, aP);
		
		size_t iCopy = iOutLen - iDone;
		if ( iCopy > 32 ) iCopy = 32;
		memcpy(pOut + iDone, aP, iCopy);
		iDone += iCopy;
		
		// A(i+1) = HMAC(secret, A(i))
		xrtHMAC_SHA256(pSecret, iSecretLen, aA, 32, aA);
	}
}

static void __xrt_tls12_prf_sha384(uint8 *pOut, size_t iOutLen,
	const uint8 *pSecret, size_t iSecretLen,
	const char *pLabel, size_t iLabelLen,
	const uint8 *pSeed, size_t iSeedLen)
{
	uint8 aA[48];  // A(i), SHA-384 输出 48 字节
	uint8 aP[48];  // P_hash 当前块
	
	// 组合 seed = label + seed
	uint8 aFullSeed[256];
	size_t iFullLen = iLabelLen + iSeedLen;
	memcpy(aFullSeed, pLabel, iLabelLen);
	memcpy(aFullSeed + iLabelLen, pSeed, iSeedLen);
	
	// A(1) = HMAC(secret, full_seed)
	xrtHMAC_SHA384(pSecret, iSecretLen, aFullSeed, iFullLen, aA);
	
	size_t iDone = 0;
	while ( iDone < iOutLen ) {
		uint8 aBuf[48 + 256];  // A(i)(48) + full_seed
		memcpy(aBuf, aA, 48);
		memcpy(aBuf + 48, aFullSeed, iFullLen);
		xrtHMAC_SHA384(pSecret, iSecretLen, aBuf, 48 + iFullLen, aP);
		
		size_t iCopy = iOutLen - iDone;
		if ( iCopy > 48 ) iCopy = 48;
		memcpy(pOut + iDone, aP, iCopy);
		iDone += iCopy;
		
		// A(i+1) = HMAC(secret, A(i))
		xrtHMAC_SHA384(pSecret, iSecretLen, aA, 48, aA);
	}
}

// 统一 PRF 入口: 根据 bUseSHA384 选择 SHA-256 或 SHA-384
static void __xrt_tls12_prf(xtlsctx *pCtx, uint8 *pOut, size_t iOutLen,
	const uint8 *pSecret, size_t iSecretLen,
	const char *pLabel, size_t iLabelLen,
	const uint8 *pSeed, size_t iSeedLen)
{
	if ( pCtx->bUseSHA384 ) {
		__xrt_tls12_prf_sha384(pOut, iOutLen, pSecret, iSecretLen, pLabel, iLabelLen, pSeed, iSeedLen);
	} else {
		__xrt_tls12_prf_sha256(pOut, iOutLen, pSecret, iSecretLen, pLabel, iLabelLen, pSeed, iSeedLen);
	}
}



/* ============================== TLS 密钥派生 ============================== */

// TLS 1.3 密钥派生: HKDF-Expand-Label (RFC 8446 Section 7.1)
static void __xrt_tls_expand_label(uint8 *pOut, size_t iOutLen,
	const uint8 *pSecret, size_t iSecretLen,
	const char *sLabel, size_t iLabelLen,
	const uint8 *pHash, size_t iHashLen, size_t iSuiteHashLen)
{
	// HkdfLabel 结构:
	//   uint16 length
	//   opaque label<7..255> = "tls13 " + Label
	//   opaque context<0..255> = Hash
	uint8 aInfo[256];
	size_t iInfoLen = 0;
	
	aInfo[iInfoLen++] = (uint8)(iOutLen >> 8);
	aInfo[iInfoLen++] = (uint8)(iOutLen);
	
	size_t iFullLabelLen = 6 + iLabelLen;  // "tls13 " + label
	aInfo[iInfoLen++] = (uint8)iFullLabelLen;
	memcpy(aInfo + iInfoLen, "tls13 ", 6);
	iInfoLen += 6;
	memcpy(aInfo + iInfoLen, sLabel, iLabelLen);
	iInfoLen += iLabelLen;
	
	aInfo[iInfoLen++] = (uint8)iHashLen;
	if ( iHashLen > 0 && pHash ) {
		memcpy(aInfo + iInfoLen, pHash, iHashLen);
		iInfoLen += iHashLen;
	}
	
	__xrt_tls13_hkdf_expand(pOut, iOutLen, pSecret, iSecretLen, aInfo, iInfoLen, iSuiteHashLen);
}

// 派生密钥
static void __xrt_tls_derive_secret(uint8 *pOut, const uint8 *pSecret,
	const char *sLabel, size_t iLabelLen, const uint8 *pMsgsHash, size_t iHashLen)
{
	__xrt_tls_expand_label(pOut, iHashLen, pSecret, iHashLen, sLabel, iLabelLen,
		pMsgsHash, iHashLen, iHashLen);
}

// 从握手密钥派生流量密钥
static void __xrt_tls_derive_traffic_keys(struct __xrt_tls_enc *pEnc,
	const uint8 *pSecret, bool bIsServer, uint16 iCipherSuite)
{
	size_t iKeyLen = __xrt_tls13_get_key_len(iCipherSuite);
	size_t iHashLen = __xrt_tls13_get_hash_len(iCipherSuite);
	
	// 派生 write key
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerWriteKey : pEnc->aClientWriteKey,
		iKeyLen, pSecret, iHashLen, "key", 3, NULL, 0, iHashLen);
	
	// 派生 write IV (始终 12 字节)
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerWriteIV : pEnc->aClientWriteIV,
		12, pSecret, iHashLen, "iv", 2, NULL, 0, iHashLen);
	
	// 派生 finished key (长度等于握手哈希长度)
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerFinishedKey : pEnc->aClientFinishedKey,
		iHashLen, pSecret, iHashLen, "finished", 8, NULL, 0, iHashLen);
}



/* ============================== TLS 记录层 ============================== */

// 加密并发送一条 TLS 记录 (栈缓冲区优化, 消除 heap 分配)
static void __xrt_tls_encrypt_record(xtlsctx *pCtx, uint8 iType,
	const uint8 *pData, size_t iLen, bool bUseServerKeys)
{
	struct __xrt_tls_enc *pEnc = (pCtx->bHandshakeDone) ? &pCtx->tAppKeys : &pCtx->tEnc;
	
	uint8 *pKey, *pIV;
	uint64 *pSeq;
	
	if ( bUseServerKeys ) {
		pKey = pEnc->aServerWriteKey;
		pIV = pEnc->aServerWriteIV;
		pSeq = &pEnc->iServerSeq;
	} else {
		pKey = pEnc->aClientWriteKey;
		pIV = pEnc->aClientWriteIV;
		pSeq = &pEnc->iClientSeq;
	}
	
	// 构造 nonce: IV XOR padded(seq_num)
	uint8 aNonce[12];
	__xrt_tls_xor_seq_nonce(aNonce, pIV, *pSeq);
	
	// 内层 plaintext: data + content_type (栈缓冲区, 消除 malloc)
	size_t iInnerLen = iLen + 1;
	uint8 aInnerStack[__XRT_TLS_MAX_RECORD + 1];
	uint8 *pInner = (iInnerLen <= sizeof(aInnerStack)) ? aInnerStack : (uint8*)xrtMalloc(iInnerLen);
	if ( !pInner ) return;
	memcpy(pInner, pData, iLen);
	pInner[iLen] = iType;
	
	// 记录头 (TLS_APP_DATA 0x17, version 0x0303, length)
	size_t iCipherLen = iInnerLen + 16;  // plaintext + AEAD tag
	uint8 aHdr[5];
	aHdr[0] = __XRT_TLS_APP_DATA;
	__xrt_tls_store_be16(aHdr + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aHdr + 3, (uint16)iCipherLen);
	
	// AEAD 加密 (栈缓冲区, 消除 malloc)
	uint8 aCipherStack[__XRT_TLS_MAX_RECORD + 17];
	uint8 *pCipher = (iCipherLen <= sizeof(aCipherStack)) ? aCipherStack : (uint8*)xrtMalloc(iCipherLen);
	if ( !pCipher ) {
		if ( pInner != aInnerStack ) xrtFree(pInner);
		return;
	}
	
	if ( pCtx->iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) {
		xrtChaCha20Poly1305Encrypt(pCipher, pKey, aNonce, aHdr, 5, pInner, iInnerLen);
	} else if ( pCtx->iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384 ) {
		xrtAES256GCMEncrypt(pCipher, pKey, aNonce, 12, aHdr, 5, pInner, iInnerLen);
	} else {
		xrtAES128GCMEncrypt(pCipher, pKey, aNonce, 12, aHdr, 5, pInner, iInnerLen);
	}
	
	// 写入发送缓冲区: 预留容量 + 直接写入 (消除多次 Append)
	size_t iTotalLen = 5 + iCipherLen;
	if ( __xrt_tls_buf_ensure(&pCtx->tSendBuf, iTotalLen) ) {
		memcpy(pCtx->tSendBuf.pData + pCtx->tSendBuf.iSize, aHdr, 5);
		memcpy(pCtx->tSendBuf.pData + pCtx->tSendBuf.iSize + 5, pCipher, iCipherLen);
		pCtx->tSendBuf.iSize += iTotalLen;
	}
	
	if ( pCipher != aCipherStack ) xrtFree(pCipher);
	if ( pInner != aInnerStack ) xrtFree(pInner);
	
	(*pSeq)++;
}

// 解密一条 TLS 记录
static bool __xrt_tls_decrypt_record(xtlsctx *pCtx, const uint8 *pRecord,
	size_t iRecordLen, uint8 *pOut, size_t *pOutLen, uint8 *pType,
	bool bUseServerKeys)
{
	if ( iRecordLen < __XRT_TLS_RECHDR_SIZE + 16 ) return false;
	
	struct __xrt_tls_enc *pEnc = (pCtx->bHandshakeDone) ? &pCtx->tAppKeys : &pCtx->tEnc;
	
	uint8 *pKey, *pIV;
	uint64 *pSeq;
	
	if ( bUseServerKeys ) {
		pKey = pEnc->aServerWriteKey;
		pIV = pEnc->aServerWriteIV;
		pSeq = &pEnc->iServerSeq;
	} else {
		pKey = pEnc->aClientWriteKey;
		pIV = pEnc->aClientWriteIV;
		pSeq = &pEnc->iClientSeq;
	}
	
	// 构造 nonce
	uint8 aNonce[12];
	__xrt_tls_xor_seq_nonce(aNonce, pIV, *pSeq);
	
	uint16 iCipherLen = __xrt_tls_load_be16(pRecord + 3);
	const uint8 *pCipher = pRecord + __XRT_TLS_RECHDR_SIZE;
	
	// AAD = 记录头
	uint8 aAAD[5];
	memcpy(aAAD, pRecord, 5);
	
	bool bOK;
	if ( pCtx->iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) {
		bOK = xrtChaCha20Poly1305Decrypt(pOut, pKey, aNonce, aAAD, 5, pCipher, iCipherLen);
	} else if ( pCtx->iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384 ) {
		bOK = xrtAES256GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 5, pCipher, iCipherLen);
	} else {
		bOK = xrtAES128GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 5, pCipher, iCipherLen);
	}
	
	if ( !bOK ) return false;
	
	// 去除填充，找到 content type（从末尾向前找非零字节）
	size_t iPlainLen = iCipherLen - 16;
	while ( iPlainLen > 0 && pOut[iPlainLen - 1] == 0 ) {
		iPlainLen--;
	}
	if ( iPlainLen == 0 ) return false;
	
	*pType = pOut[iPlainLen - 1];
	*pOutLen = iPlainLen - 1;
	
	(*pSeq)++;
	return true;
}

// TLS 1.2 AEAD 加密记录
static void __xrt_tls12_encrypt_record(xtlsctx *pCtx, uint8 iType,
	const uint8 *pData, size_t iLen)
{
	uint8 aNonce[12];
	uint8 *pWriteIV = pCtx->bIsServer ? pCtx->aServerWriteIV12 : pCtx->aClientWriteIV12;
	uint8 *pKey = pCtx->bIsServer ? pCtx->aServerWriteKey12 : pCtx->aClientWriteKey12;
	uint64 *pSeq = pCtx->bIsServer ? &pCtx->iServerSeq12 : &pCtx->iClientSeq12;
	size_t iExplicitNonceLen = pCtx->bTls12UseChaCha ? 0 : 8;

	if ( pCtx->bTls12UseChaCha ) {
		__xrt_tls_xor_seq_nonce(aNonce, pWriteIV, *pSeq);
	} else {
		memcpy(aNonce, pWriteIV, 4);
		__xrt_tls_store_be64(aNonce + 4, *pSeq);
	}
	
	// 13 字节 AAD = seq_num(8) + type(1) + version(2) + plaintext_length(2)
	uint8 aAAD[13];
	__xrt_tls_store_be64(aAAD, *pSeq);
	aAAD[8] = iType;
	__xrt_tls_store_be16(aAAD + 9, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aAAD + 11, (uint16)iLen);
	
	// 记录头: type(1) + version(2) + length(2)
	// GCM: length = explicit_nonce(8) + ciphertext + tag
	// ChaCha20-Poly1305: length = ciphertext + tag
	uint8 aHdr[5];
	aHdr[0] = iType;
	__xrt_tls_store_be16(aHdr + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aHdr + 3, (uint16)(iExplicitNonceLen + iLen + 16));
	
	// 加密 (栈缓冲区, 消除 malloc)
	uint8 aCipherStack[__XRT_TLS_MAX_RECORD + 16];
	uint8 *pCipher = (iLen + 16 <= sizeof(aCipherStack)) ? aCipherStack : (uint8*)xrtMalloc(iLen + 16);
	if ( !pCipher ) return;
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Encrypt: type=%d len=%d seq=%d keyLen=%d\n", iType, (int)iLen, *pSeq, pCtx->iKeyLen);
		printf("    [TLS12]   Key: ");
		for (int i = 0; i < (int)pCtx->iKeyLen; i++) printf("%02x", pKey[i]);
		printf("\n    [TLS12]   Nonce: ");
		for (int i = 0; i < 12; i++) printf("%02x", aNonce[i]);
		printf("\n    [TLS12]   AAD: ");
		for (int i = 0; i < 13; i++) printf("%02x", aAAD[i]);
		printf("\n    [TLS12]   Plaintext: ");
		for (int i = 0; i < (int)(iLen < 32 ? iLen : 32); i++) printf("%02x", pData[i]);
		printf("\n");
	#endif
	
	if ( pCtx->bTls12UseChaCha ) {
		xrtChaCha20Poly1305Encrypt(pCipher, pKey, aNonce, aAAD, 13, pData, iLen);
	} else if ( pCtx->iKeyLen == 32 ) {
		xrtAES256GCMEncrypt(pCipher, pKey, aNonce, 12, aAAD, 13, pData, iLen);
	} else {
		xrtAES128GCMEncrypt(pCipher, pKey, aNonce, 12, aAAD, 13, pData, iLen);
	}
	
	// 写入发送缓冲区: 预留容量 + 直接写入 (消除多次 Append)
	size_t iTotalLen = 5 + iExplicitNonceLen + iLen + 16;
	if ( __xrt_tls_buf_ensure(&pCtx->tSendBuf, iTotalLen) ) {
		char *pDst = pCtx->tSendBuf.pData + pCtx->tSendBuf.iSize;
		memcpy(pDst, aHdr, 5);
		if ( iExplicitNonceLen > 0 ) {
			__xrt_tls_store_be64((uint8*)pDst + 5, *pSeq);
		}
		memcpy(pDst + 5 + iExplicitNonceLen, pCipher, iLen + 16);
		pCtx->tSendBuf.iSize += iTotalLen;
	}
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12]   Ciphertext+Tag: ");
		for (int i = 0; i < (int)(iLen + 16); i++) printf("%02x", pCipher[i]);
		printf("\n    [TLS12]   RecHdr: ");
		for (int i = 0; i < 5; i++) printf("%02x", aHdr[i]);
		printf("\n");
		// Self-decrypt test
		{
			uint8 *aDecrypt = (uint8*)xrtMalloc(iLen);
			if ( aDecrypt ) {
				bool bOK;
				if ( pCtx->bTls12UseChaCha ) {
					bOK = xrtChaCha20Poly1305Decrypt(aDecrypt, pKey, aNonce, aAAD, 13, pCipher, iLen + 16);
				} else if ( pCtx->iKeyLen == 32 ) {
					bOK = xrtAES256GCMDecrypt(aDecrypt, pKey, aNonce, 12, aAAD, 13, pCipher, iLen + 16);
				} else {
					bOK = xrtAES128GCMDecrypt(aDecrypt, pKey, aNonce, 12, aAAD, 13, pCipher, iLen + 16);
				}
				printf("    [TLS12]   SelfDecrypt: %s\n", bOK ? "OK" : "FAIL");
				if (bOK) {
					int match = (memcmp(aDecrypt, pData, iLen) == 0);
					printf("    [TLS12]   PlaintextMatch: %s\n", match ? "OK" : "MISMATCH");
				}
				xrtFree(aDecrypt);
			}
		}
	#endif
	
	if ( pCipher != aCipherStack ) xrtFree(pCipher);
	(*pSeq)++;
}

// TLS 1.2 AEAD 解密记录
// pRecord 指向完整记录 (hdr + payload), pOut 接收明文
static bool __xrt_tls12_decrypt_record(xtlsctx *pCtx, const uint8 *pRecord,
	size_t iRecordLen, uint8 *pOut, size_t *pOutLen, uint8 *pType)
{
	size_t iExplicitNonceLen = pCtx->bTls12UseChaCha ? 0 : 8;
	if ( iRecordLen < __XRT_TLS_RECHDR_SIZE + iExplicitNonceLen + 16 ) return false;

	uint8 *pReadIV = pCtx->bIsServer ? pCtx->aClientWriteIV12 : pCtx->aServerWriteIV12;
	uint8 *pKey = pCtx->bIsServer ? pCtx->aClientWriteKey12 : pCtx->aServerWriteKey12;
	uint64 *pSeq = pCtx->bIsServer ? &pCtx->iClientSeq12 : &pCtx->iServerSeq12;
	
	uint16 iPayloadLen = __xrt_tls_load_be16(pRecord + 3);
	if ( iPayloadLen < iExplicitNonceLen + 16 ) return false;
	
	const uint8 *pPayload = pRecord + __XRT_TLS_RECHDR_SIZE;
	size_t iCipherLen = iPayloadLen - iExplicitNonceLen;
	size_t iPlainLen = iCipherLen - 16;
	
	uint8 aNonce[12];
	const uint8 *pCipher = pPayload + iExplicitNonceLen;
	if ( pCtx->bTls12UseChaCha ) {
		__xrt_tls_xor_seq_nonce(aNonce, pReadIV, *pSeq);
	} else {
		memcpy(aNonce, pReadIV, 4);
		memcpy(aNonce + 4, pPayload, 8);
	}
	
	// 13 字节 AAD
	uint8 aAAD[13];
	__xrt_tls_store_be64(aAAD, *pSeq);
	aAAD[8] = pRecord[0];  // content type
	memcpy(aAAD + 9, pRecord + 1, 2);  // version
	__xrt_tls_store_be16(aAAD + 11, (uint16)iPlainLen);
	
	bool bOK;
	if ( pCtx->bTls12UseChaCha ) {
		bOK = xrtChaCha20Poly1305Decrypt(pOut, pKey, aNonce, aAAD, 13, pCipher, iCipherLen);
	} else if ( pCtx->iKeyLen == 32 ) {
		bOK = xrtAES256GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 13, pCipher, iCipherLen);
	} else {
		bOK = xrtAES128GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 13, pCipher, iCipherLen);
	}
	
	if ( !bOK ) return false;
	
	*pOutLen = iPlainLen;
	*pType = pRecord[0];  // TLS 1.2: content type 在记录头明文中
	(*pSeq)++;
	return true;
}



/* ============================== 握手消息构造 ============================== */

// 前向声明: TLS 1.2 握手哈希更新 (定义在 TLS 1.2 握手函数区)
static void __xrt_tls12_update_hash(xtlsctx *pCtx, const uint8 *pData, size_t iLen);

// 构造 ClientHello 消息
static void __xrt_tls_send_client_hello(xtlsctx *pCtx)
{
	uint8 aBuf[1024];
	size_t iPos = 0;
	size_t iSuitesPos;
	size_t iSuitesStart;
	size_t iExtPos;
	bool bAllowTls13 = (pCtx->iMaxVersion == 0 || pCtx->iMaxVersion >= __XRT_TLS_VERSION_1_3);
	
	// 生成随机数和密钥对 (X25519 + X448 + P-256 + P-384)
	xrtRandomBytes(pCtx->aRandom, 32);
	if ( pCtx->bHaveResume && pCtx->tResume.iVersion == __XRT_TLS_VERSION_1_2 && pCtx->tResume.iSessionIdLen > 0 ) {
		pCtx->iSessionIdLen = pCtx->tResume.iSessionIdLen;
		memcpy(pCtx->aSessionId, pCtx->tResume.aSessionId, pCtx->iSessionIdLen);
	} else {
		pCtx->iSessionIdLen = 32;
		xrtRandomBytes(pCtx->aSessionId, 32);
	}
	xrtX25519Keypair(pCtx->aX25519Priv, pCtx->aX25519Pub);
	xrtX448Keypair(pCtx->aX448Priv, pCtx->aX448Pub);
	xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
	xrtECDHSecp384r1Keypair(pCtx->aP384Priv, pCtx->aP384Pub);
	
	#ifdef DEBUG_TRACE
		printf("    [TLS] ClientHello: aRandom generated: ");
		for (int i = 0; i < 32; i++) printf("%02x", pCtx->aRandom[i]);
		printf("\n");
	#endif
	
	// Handshake: ClientHello (type=1)
	aBuf[iPos++] = __XRT_TLS_CLIENT_HELLO;
	size_t iLenPos = iPos;
	iPos += 3;  // 预留长度
	
	// client version = 0x0303 (TLS 1.2 兼容)
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	
	// client random (32 bytes)
	memcpy(aBuf + iPos, pCtx->aRandom, 32);
	iPos += 32;
	
	// session ID
	aBuf[iPos++] = pCtx->iSessionIdLen;
	memcpy(aBuf + iPos, pCtx->aSessionId, pCtx->iSessionIdLen);
	iPos += pCtx->iSessionIdLen;
	
	// cipher suites (TLS 1.3 + TLS 1.2 现代 AEAD 套件, 9 套件)
	iSuitesPos = iPos;
	iPos += 2;
	iSuitesStart = iPos;
	if ( bAllowTls13 ) {
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_CHACHA20_POLY1305_SHA256);  // TLS 1.3
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_AES_128_GCM_SHA256);        // TLS 1.3
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_AES_256_GCM_SHA384);        // TLS 1.3
		iPos += 2;
	}
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256);  // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256);    // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384);  // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384);    // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256);  // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256);    // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iSuitesPos, (uint16)(iPos - iSuitesStart));
	
	// compression methods
	aBuf[iPos++] = 1;   // length
	aBuf[iPos++] = 0;   // null compression
	
	// --- Extensions ---
	iExtPos = iPos;
	iPos += 2;  // 预留扩展总长度
	
	// Extension: supported_versions (0x002b) — TLS 1.3 preferred, TLS 1.2 fallback
	__xrt_tls_store_be16(aBuf + iPos, 0x002b);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, bAllowTls13 ? 5 : 3);  // ext data length
	iPos += 2;
	aBuf[iPos++] = bAllowTls13 ? 4 : 2;   // version list length
	if ( bAllowTls13 ) {
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_3);
		iPos += 2;
	}
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	
	// Extension: supported_groups (0x000a) - X25519 + X448 + secp256r1 + secp384r1
	__xrt_tls_store_be16(aBuf + iPos, 0x000a);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 10);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 8);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x001d);  // x25519
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x001e);  // x448
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0017);  // secp256r1
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0018);  // secp384r1
	iPos += 2;
	
	// Extension: signature_algorithms (0x000d)
	__xrt_tls_store_be16(aBuf + iPos, 0x000d);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 26);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 24);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0804);  // RSA-PSS-RSAE-SHA256 (TLS 1.3 必须)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0805);  // RSA-PSS-RSAE-SHA384
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0806);  // RSA-PSS-RSAE-SHA512
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0809);  // RSA-PSS-PSS-SHA256
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x080a);  // RSA-PSS-PSS-SHA384
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x080b);  // RSA-PSS-PSS-SHA512
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0807);  // Ed25519
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0403);  // ECDSA-SECP256R1-SHA256
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0503);  // ECDSA-SECP384R1-SHA384
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0401);  // RSA-PKCS1-SHA256 (TLS 1.2 兼容)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0501);  // RSA-PKCS1-SHA384 (TLS 1.2 兼容)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0601);  // RSA-PKCS1-SHA512 (TLS 1.2 兼容)
	iPos += 2;
	
	if ( bAllowTls13 ) {
	// Extension: key_share (0x0033) - X25519 + X448 + P-256 + P-384
	__xrt_tls_store_be16(aBuf + iPos, 0x0033);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 268);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 266);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x001d);  // x25519 group
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 32);   // key length
	iPos += 2;
	memcpy(aBuf + iPos, pCtx->aX25519Pub, 32);
	iPos += 32;
	__xrt_tls_store_be16(aBuf + iPos, 0x001e);  // x448 group
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 56);
	iPos += 2;
	memcpy(aBuf + iPos, pCtx->aX448Pub, 56);
	iPos += 56;
	__xrt_tls_store_be16(aBuf + iPos, 0x0017);  // secp256r1 group
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 65);   // key length (uncompressed point)
	iPos += 2;
	memcpy(aBuf + iPos, pCtx->aP256Pub, 65);
	iPos += 65;
	__xrt_tls_store_be16(aBuf + iPos, 0x0018);  // secp384r1 group
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 97);
	iPos += 2;
	memcpy(aBuf + iPos, pCtx->aP384Pub, 97);
	iPos += 97;
	
	// Extension: psk_key_exchange_modes (0x002d) — 许多服务器要求此扩展
	__xrt_tls_store_be16(aBuf + iPos, 0x002d);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 2);  // ext data length
	iPos += 2;
	aBuf[iPos++] = 1;   // modes list length
	aBuf[iPos++] = 1;   // psk_dhe_ke
	}
	
	// Extension: ec_point_formats (0x000b) — TLS 1.2 兼容
	__xrt_tls_store_be16(aBuf + iPos, 0x000b);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 2);  // ext data length
	iPos += 2;
	aBuf[iPos++] = 1;   // formats length
	aBuf[iPos++] = 0;   // uncompressed
	
	// Extension: server_name (SNI) - 如果设置了主机名
	if ( pCtx->sHostname[0] ) {
		size_t iHostLen = strlen(pCtx->sHostname);
		__xrt_tls_store_be16(aBuf + iPos, 0x0000);  // SNI extension
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, (uint16)(iHostLen + 5));
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, (uint16)(iHostLen + 3));
		iPos += 2;
		aBuf[iPos++] = 0;  // host_name type
		__xrt_tls_store_be16(aBuf + iPos, (uint16)iHostLen);
		iPos += 2;
		memcpy(aBuf + iPos, pCtx->sHostname, iHostLen);
		iPos += iHostLen;
	}
	
	// 填充扩展总长度
	__xrt_tls_store_be16(aBuf + iExtPos, (uint16)(iPos - iExtPos - 2));
	
	// 填充握手消息长度
	__xrt_tls_store_be24(aBuf + iLenPos, (uint32)(iPos - iLenPos - 3));
	
	// 更新握手哈希
	__xrt_tls13_hash_update(pCtx, aBuf, iPos);
	__xrt_tls12_update_hash(pCtx, aBuf, iPos);
	
	// 包装为 TLS record (plaintext)
	// RFC 8446: initial ClientHello record version SHOULD be 0x0301 for compatibility
	uint8 aRec[5];
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 1, 0x0303);  // TLS 1.2 record version for compatibility
	__xrt_tls_store_be16(aRec + 3, (uint16)iPos);
	
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aRec, 5);
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aBuf, iPos);
	
	#ifdef DEBUG_TRACE
		printf("    [TLS] ClientHello: total=%d bytes, msg=%d bytes\n",
			(int)(5 + iPos), (int)iPos);
		printf("    [TLS] Record: ");
		for ( size_t _i = 0; _i < 5; _i++ ) printf("%02x ", aRec[_i]);
		printf("\n");
		printf("    [TLS] Full ClientHello hex:\n    ");
		for ( size_t _i = 0; _i < iPos; _i++ ) {
			printf("%02x ", aBuf[_i]);
			if ( (_i + 1) % 32 == 0 ) printf("\n    ");
		}
		printf("\n");
	#endif
}

// 解析 ServerHello 消息 (支持 TLS 1.3 和 TLS 1.2)
static bool __xrt_tls_parse_server_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	if ( iLen < 2 + 32 + 1 ) return false;
	
	size_t iPos = 0;
	bool bTls12ResumeAccepted = false;
	
	// legacy server version
	uint16 iLegacyVer = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	// server random
	memcpy(pCtx->aServerRandom, pMsg + iPos, 32);
	iPos += 32;
	
	// session ID
	uint8 iSessLen = pMsg[iPos++];
	if ( iSessLen > sizeof(pCtx->aSessionId) || iPos + iSessLen > iLen ) return false;
	pCtx->iSessionIdLen = iSessLen;
	if ( iSessLen > 0 ) memcpy(pCtx->aSessionId, pMsg + iPos, iSessLen);
	iPos += iSessLen;
	
	// cipher suite
	if ( iPos + 2 > iLen ) return false;
	pCtx->iCipherSuite = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	// compression
	iPos += 1;
	
	// 解析扩展
	bool bFoundTls13 = false;
	if ( iPos + 2 <= iLen ) {
		uint16 iExtLen = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		
		size_t iExtEnd = iPos + iExtLen;
		while ( iPos + 4 <= iExtEnd ) {
			uint16 iExtType = __xrt_tls_load_be16(pMsg + iPos);
			iPos += 2;
			uint16 iExtDataLen = __xrt_tls_load_be16(pMsg + iPos);
			iPos += 2;
			
			if ( iExtType == 0x002b ) {  // supported_versions
				if ( iExtDataLen >= 2 ) {
					uint16 iSelectedVer = __xrt_tls_load_be16(pMsg + iPos);
					if ( iSelectedVer == __XRT_TLS_VERSION_1_3 ) {
						bFoundTls13 = true;
					}
				}
			} else if ( iExtType == 0x0033 ) {  // key_share
				// 解析服务器的密钥共享 (TLS 1.3)
				if ( iExtDataLen >= 4 ) {
					uint16 iGroup = __xrt_tls_load_be16(pMsg + iPos);
					uint16 iKeyLen = __xrt_tls_load_be16(pMsg + iPos + 2);
					if ( iGroup == 0x001d && iKeyLen == 32 ) {
						// X25519: 32 字节公钥
						xrtX25519SharedSecret(pCtx->aX25519Secret, pCtx->aX25519Priv, pMsg + iPos + 4);
						pCtx->iTls13Group = 0x001d;
						pCtx->iTls13SecretLen = 32;
						#ifdef DEBUG_TRACE
							printf("    [TLS] ServerHello key_share: X25519\n");
						#endif
					} else if ( iGroup == 0x001e && iKeyLen == 56 && iExtDataLen >= 60 ) {
						xrtX448SharedSecret(pCtx->aX25519Secret, pCtx->aX448Priv, pMsg + iPos + 4);
						pCtx->iTls13Group = 0x001e;
						pCtx->iTls13SecretLen = 56;
						#ifdef DEBUG_TRACE
							printf("    [TLS] ServerHello key_share: X448\n");
						#endif
					} else if ( iGroup == 0x0017 && iKeyLen == 65 && iExtDataLen >= 69 ) {
						// P-256 (secp256r1): 65 字节 uncompressed 公钥
						xrtECDHSecp256r1SharedSecret(pCtx->aX25519Secret, pCtx->aP256Priv, pMsg + iPos + 4);
						pCtx->iTls13Group = 0x0017;
						pCtx->iTls13SecretLen = 32;
						#ifdef DEBUG_TRACE
							printf("    [TLS] ServerHello key_share: P-256\n");
						#endif
					} else if ( iGroup == 0x0018 && iKeyLen == 97 && iExtDataLen >= 101 ) {
						xrtECDHSecp384r1SharedSecret(pCtx->aX25519Secret, pCtx->aP384Priv, pMsg + iPos + 4);
						pCtx->iTls13Group = 0x0018;
						pCtx->iTls13SecretLen = 48;
						#ifdef DEBUG_TRACE
							printf("    [TLS] ServerHello key_share: P-384\n");
						#endif
					}
				}
			}
			
			iPos += iExtDataLen;
		}
	}
	
	if ( bFoundTls13 ) {
		// TLS 1.3 协商成功
		if ( !__xrt_tls13_is_supported_cipher(pCtx->iCipherSuite) || pCtx->iTls13Group == 0 ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS] ServerHello: unsupported TLS 1.3 cipher/group (cipher=0x%04x group=0x%04x)\n",
					pCtx->iCipherSuite, pCtx->iTls13Group);
			#endif
			return false;
		}
		pCtx->bIsTls12 = false;
		#ifdef DEBUG_TRACE
			printf("    [TLS] ServerHello: TLS 1.3, cipher=0x%04x\n", pCtx->iCipherSuite);
		#endif
		return true;
	}
	
	// 没有 supported_versions 扩展 + legacy version == 0x0303 → TLS 1.2
	if ( iLegacyVer == __XRT_TLS_VERSION_1_2 ) {
		if ( !__xrt_tls12_set_cipher_params(pCtx, pCtx->iCipherSuite) ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS] ServerHello: unsupported TLS 1.2 cipher 0x%04x\n", pCtx->iCipherSuite);
			#endif
			return false;
		}
		if ( pCtx->bHaveResume
			&& pCtx->tResume.iVersion == __XRT_TLS_VERSION_1_2
			&& pCtx->tResume.iSessionIdLen == pCtx->iSessionIdLen
			&& pCtx->tResume.iCipherSuite == pCtx->iCipherSuite
			&& pCtx->iSessionIdLen > 0
			&& memcmp(pCtx->tResume.aSessionId, pCtx->aSessionId, pCtx->iSessionIdLen) == 0 ) {
			memcpy(pCtx->aMasterSecret, pCtx->tResume.aMasterSecret, sizeof(pCtx->aMasterSecret));
			bTls12ResumeAccepted = true;
		}
		
		#ifdef DEBUG_TRACE
			printf("    [TLS] ServerHello: TLS 1.2, cipher=0x%04x, sha384=%d, ecdhe=%d, keyLen=%d\n",
				pCtx->iCipherSuite, pCtx->bUseSHA384, pCtx->bIsECDHE, pCtx->iKeyLen);
			if ( bTls12ResumeAccepted ) printf("    [TLS] ServerHello: session resumed\n");
		#endif
		pCtx->bSessionResumed = bTls12ResumeAccepted;
		return true;
	}
	
	#ifdef DEBUG_TRACE
		printf("    [TLS] ServerHello: unsupported version 0x%04x\n", iLegacyVer);
	#endif
	return false;
}

// 发送 Finished 消息
static void __xrt_tls_send_finished(xtlsctx *pCtx, bool bAsServer)
{
	size_t iHashLen = __xrt_tls13_get_hash_len(pCtx->iCipherSuite);
	uint8 aHash[64];
	uint8 aVerifyData[64];
	uint8 aMsg[68];
	uint8 *pFinKey;
	
	__xrt_tls13_get_transcript_hash(pCtx, aHash, NULL);

	// finished_key 已在密钥派生时计算
	pFinKey = bAsServer ? pCtx->tEnc.aServerFinishedKey : pCtx->tEnc.aClientFinishedKey;
	
	// verify_data = HMAC(finished_key, transcript_hash)
	if ( !__xrt_tls13_hmac(aVerifyData, pFinKey, iHashLen, aHash, iHashLen, iHashLen) ) return;
	
	// 构造 Finished 握手消息
	aMsg[0] = __XRT_TLS_FINISHED;
	__xrt_tls_store_be24(aMsg + 1, (uint32)iHashLen);
	memcpy(aMsg + 4, aVerifyData, iHashLen);
	
	// 更新握手哈希
	__xrt_tls13_hash_update(pCtx, aMsg, 4 + iHashLen);
	
	// 加密发送
	__xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, aMsg, 4 + iHashLen, bAsServer);
}

static bool __xrt_tls_verify_finished(xtlsctx *pCtx, const uint8 *pVerifyData,
	size_t iVerifyLen, const uint8 *pTranscriptHash, size_t iHashLen, bool bFromServer)
{
	uint8 aExpected[64];
	uint8 *pFinishedKey;
	if ( iVerifyLen != iHashLen ) return false;
	pFinishedKey = bFromServer ? pCtx->tEnc.aServerFinishedKey : pCtx->tEnc.aClientFinishedKey;
	if ( !__xrt_tls13_hmac(aExpected, pFinishedKey, iHashLen,
		pTranscriptHash, iHashLen, iHashLen) ) {
		return false;
	}
	return __xrt_tls_consttime_equal(aExpected, pVerifyData, iHashLen);
}

static bool __xrt_tls13_build_cert_verify_input(uint8 *pOut, size_t *pOutLen,
	const uint8 *pTranscriptHash, size_t iTranscriptHashLen, uint16 iSigAlg, bool bAsServer)
{
	size_t iSigHashLen;
	const char *sContext = bAsServer ? "TLS 1.3, server CertificateVerify"
		: "TLS 1.3, client CertificateVerify";
	size_t iContextLen = strlen(sContext);
	uint8 aSigContent[64 + 40 + 1 + 64];
	size_t iContentLen;

	switch ( iSigAlg ) {
		case 0x0807:
			iSigHashLen = 0;
			break;
		case 0x0804:
		case 0x0403:
		case 0x0809:
			iSigHashLen = 32;
			break;
		case 0x0805:
		case 0x0503:
		case 0x080a:
			iSigHashLen = 48;
			break;
		case 0x0806:
		case 0x080b:
			iSigHashLen = 64;
			break;
		default:
			return false;
	}

	if ( iTranscriptHashLen == 0 || iTranscriptHashLen > 64 ) return false;

	memset(aSigContent, 0x20, 64);
	iContentLen = 64;
	memcpy(aSigContent + iContentLen, sContext, iContextLen);
	iContentLen += iContextLen;
	aSigContent[iContentLen++] = 0x00;
	memcpy(aSigContent + iContentLen, pTranscriptHash, iTranscriptHashLen);
	iContentLen += iTranscriptHashLen;

	if ( iSigAlg == 0x0807 ) {
		memcpy(pOut, aSigContent, iContentLen);
		if ( pOutLen ) *pOutLen = iContentLen;
		return true;
	}
	if ( !__xrt_tls_hash_bytes(pOut, iSigHashLen, aSigContent, iContentLen) ) return false;
	if ( pOutLen ) *pOutLen = iSigHashLen;
	return true;
}



/* ============================== TLS 密钥调度 ============================== */

// 执行 TLS 1.3 密钥调度 (在收到 ServerHello 后)
static void __xrt_tls_derive_handshake_keys(xtlsctx *pCtx)
{
	size_t iHashLen = __xrt_tls13_get_hash_len(pCtx->iCipherSuite);
	uint8 aEarlySecret[64];
	uint8 aZeros[64];
	uint8 aDerived[64];
	uint8 aHandshakeSecret[64];
	uint8 aTranscriptHash[64];
	uint8 aCHTS[64];
	uint8 aSHTS[64];
	uint8 aEmptyHash[64];
	memset(aZeros, 0, iHashLen);
	
	// Early Secret = HKDF-Extract(salt=0, IKM=0)
	__xrt_tls13_hkdf_extract(aEarlySecret, aZeros, iHashLen, aZeros, iHashLen, iHashLen);
	
	// Derive-Secret(early_secret, "derived", "")
	__xrt_tls_hash_bytes(aEmptyHash, iHashLen, (const uint8 *)"", 0);
	__xrt_tls_derive_secret(aDerived, aEarlySecret, "derived", 7, aEmptyHash, iHashLen);
	
	// Handshake Secret = HKDF-Extract(salt=derived, IKM=shared_secret)
	__xrt_tls13_hkdf_extract(aHandshakeSecret, aDerived, iHashLen,
		pCtx->aX25519Secret, pCtx->iTls13SecretLen, iHashLen);
	
	// 获取当前握手哈希 (包含 ClientHello + ServerHello)
	__xrt_tls13_get_transcript_hash(pCtx, aTranscriptHash, NULL);
	
	// client_handshake_traffic_secret
	__xrt_tls_derive_secret(aCHTS, aHandshakeSecret, "c hs traffic", 12, aTranscriptHash, iHashLen);
	__xrt_tls_derive_traffic_keys(&pCtx->tEnc, aCHTS, false, pCtx->iCipherSuite);
	
	// server_handshake_traffic_secret
	__xrt_tls_derive_secret(aSHTS, aHandshakeSecret, "s hs traffic", 12, aTranscriptHash, iHashLen);
	__xrt_tls_derive_traffic_keys(&pCtx->tEnc, aSHTS, true, pCtx->iCipherSuite);
	
	// 保存 handshake secret 用于后续推导 master secret
	memcpy(pCtx->tEnc.aHandshakeSecret, aHandshakeSecret, iHashLen);
}

// 在握手完成后推导应用数据密钥
static void __xrt_tls_derive_application_keys(xtlsctx *pCtx)
{
	size_t iHashLen = __xrt_tls13_get_hash_len(pCtx->iCipherSuite);
	uint8 aZeros[64];
	uint8 aDerived[64];
	uint8 aMasterSecret[64];
	uint8 aTranscriptHash[64];
	uint8 aCATS[64];
	uint8 aSATS[64];
	uint8 aEmptyHash[64];
	memset(aZeros, 0, iHashLen);
	
	// Derive-Secret(handshake_secret, "derived", "")
	__xrt_tls_hash_bytes(aEmptyHash, iHashLen, (const uint8 *)"", 0);
	__xrt_tls_derive_secret(aDerived, pCtx->tEnc.aHandshakeSecret, "derived", 7, aEmptyHash, iHashLen);
	
	// Master Secret = HKDF-Extract(salt=derived, IKM=0)
	__xrt_tls13_hkdf_extract(aMasterSecret, aDerived, iHashLen, aZeros, iHashLen, iHashLen);
	
	// 获取包含所有握手消息的哈希
	__xrt_tls13_get_transcript_hash(pCtx, aTranscriptHash, NULL);
	
	// client_application_traffic_secret_0
	__xrt_tls_derive_secret(aCATS, aMasterSecret, "c ap traffic", 12, aTranscriptHash, iHashLen);
	__xrt_tls_derive_traffic_keys(&pCtx->tAppKeys, aCATS, false, pCtx->iCipherSuite);
	
	// server_application_traffic_secret_0
	__xrt_tls_derive_secret(aSATS, aMasterSecret, "s ap traffic", 12, aTranscriptHash, iHashLen);
	__xrt_tls_derive_traffic_keys(&pCtx->tAppKeys, aSATS, true, pCtx->iCipherSuite);
}



/* ============================== TLS 1.2 握手函数 ============================== */

// 更新 TLS 1.2 握手哈希 (同时更新 SHA-256 和 SHA-384)
static void __xrt_tls12_update_hash(xtlsctx *pCtx, const uint8 *pData, size_t iLen)
{
	xrtSHA256Update(&pCtx->tSHA256_12, (ptr)pData, iLen);
	xrtSHA512Update(&pCtx->tSHA384_12, (const ptr)pData, iLen);
}

// 获取 TLS 1.2 握手哈希快照 (根据套件选择 SHA-256 或 SHA-384)
static void __xrt_tls12_get_hash(xtlsctx *pCtx, uint8 *pOut)
{
	if ( pCtx->bUseSHA384 ) {
		xsha512_ctx tCopy = pCtx->tSHA384_12;
		xrtSHA384Final(&tCopy, pOut);
	} else {
		xsha256_ctx tCopy = pCtx->tSHA256_12;
		xrtSHA256Final(&tCopy, pOut);
	}
}

static bool __xrt_tls12_collect_certs(const uint8 *pMsg, size_t iLen,
	uint8 **apCertData, size_t *apCertLen, size_t *pCertCount)
{
	size_t iOff = 3;
	size_t iCount = 0;
	uint32 iCertListLen;

	if ( !pMsg || iLen < 3 || !apCertData || !apCertLen || !pCertCount ) return false;
	iCertListLen = __xrt_tls_load_be24(pMsg);
	if ( 3 + iCertListLen > iLen ) return false;

	while ( iOff + 3 <= iLen && iCount < __XRT_TLS_MAX_CERT_CHAIN ) {
		uint32 iCertLen = __xrt_tls_load_be24(pMsg + iOff);
		iOff += 3;
		if ( iCertLen == 0 || iOff + iCertLen > iLen ) return false;
		apCertData[iCount] = (uint8*)(pMsg + iOff);
		apCertLen[iCount] = iCertLen;
		iCount++;
		iOff += iCertLen;
	}

	*pCertCount = iCount;
	return iCount > 0;
}

static bool __xrt_tls13_collect_certs(const uint8 *pMsg, size_t iLen,
	uint8 **apCertData, size_t *apCertLen, size_t *pCertCount)
{
	size_t iOff = 0;
	size_t iCount = 0;
	uint8 iReqCtxLen;
	uint32 iCertListLen;
	size_t iCertListEnd;

	if ( !pMsg || !apCertData || !apCertLen || !pCertCount || iLen < 4 ) return false;
	iReqCtxLen = pMsg[iOff++];
	if ( iOff + iReqCtxLen + 3 > iLen ) return false;
	iOff += iReqCtxLen;
	iCertListLen = __xrt_tls_load_be24(pMsg + iOff);
	iOff += 3;
	iCertListEnd = iOff + iCertListLen;
	if ( iCertListEnd > iLen ) return false;

	while ( iOff + 3 <= iCertListEnd && iCount < __XRT_TLS_MAX_CERT_CHAIN ) {
		uint32 iCertLen = __xrt_tls_load_be24(pMsg + iOff);
		uint16 iExtLen;
		iOff += 3;
		if ( iCertLen == 0 || iOff + iCertLen + 2 > iCertListEnd ) return false;
		apCertData[iCount] = (uint8*)(pMsg + iOff);
		apCertLen[iCount] = iCertLen;
		iCount++;
		iOff += iCertLen;
		iExtLen = __xrt_tls_load_be16(pMsg + iOff);
		iOff += 2;
		if ( iOff + iExtLen > iCertListEnd ) return false;
		iOff += iExtLen;
	}

	*pCertCount = iCount;
	return iCount > 0;
}

// Step 5: 解析 TLS 1.2 Certificate 消息
// TLS 1.2 格式: cert_list_len(3) + [cert_len(3) + cert]*  (无 request_context, 无 extensions)
static bool __xrt_tls12_parse_certificate(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	uint8 *apCertData[__XRT_TLS_MAX_CERT_CHAIN];
	size_t apCertLen[__XRT_TLS_MAX_CERT_CHAIN];
	size_t iCertCount = 0;

	if ( !__xrt_tls12_collect_certs(pMsg, iLen, apCertData, apCertLen, &iCertCount) ) return false;
	if ( !__xrt_tls_capture_peer_cert_chain(pCtx, apCertData, apCertLen, iCertCount) ) return false;

	#ifdef DEBUG_TRACE
		printf("    [TLS12] Certificate: chain=%d pubkey=%s\n", (int)iCertCount, pCtx->bIsECPubKey ? "EC" : "RSA");
	#endif
	return true;
}

// Step 6: 解析 ServerKeyExchange (ECDHE 套件)
static bool __xrt_tls12_parse_server_key_exchange(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	size_t iOff = 0;
	
	// ECParameters: curve_type(1) + named_curve(2)
	if ( iOff + 3 > iLen ) {
		#ifdef DEBUG_TRACE
		printf("    [TLS12] SKE: too short for ECParameters\n");
		#endif
		return false;
	}
	if ( pMsg[iOff] != 3 ) {
		#ifdef DEBUG_TRACE
		printf("    [TLS12] SKE: curve_type=%d (expected 3)\n", pMsg[iOff]);
		#endif
		return false;  // named_curve
	}
	uint16 iCurve = __xrt_tls_load_be16(pMsg + iOff + 1);
	#ifdef DEBUG_TRACE
	printf("    [TLS12] SKE: curve=0x%04x\n", iCurve);
	#endif
	if ( iCurve != 0x0017 && iCurve != 0x0018 && iCurve != 0x001d && iCurve != 0x001e ) {
		#ifdef DEBUG_TRACE
		printf("    [TLS12] SKE: unsupported curve 0x%04x\n", iCurve);
		#endif
		return false;
	}
	pCtx->iTls12Curve = iCurve;
	iOff += 3;
	
	// EC point: point_len(1) + point
	// P-256: 65 bytes, P-384: 97 bytes, X25519: 32 bytes, X448: 56 bytes
	if ( iOff + 1 > iLen ) return false;
	uint8 iPointLen = pMsg[iOff++];
	
	const uint8 *pServerPub = pMsg + iOff;
	size_t iExpectedLen;
	
	if ( iCurve == 0x0017 ) {  // P-256
		iExpectedLen = 65;
		if ( iPointLen != 65 || pMsg[iOff] != 0x04 ) {
			#ifdef DEBUG_TRACE
			printf("    [TLS12] SKE: P-256 invalid point (len=%d, format=0x%02x)\n", iPointLen, pMsg[iOff]);
			#endif
			return false;  // uncompressed format required
		}
	} else if ( iCurve == 0x0018 ) {
		iExpectedLen = 97;
		if ( iPointLen != 97 || pMsg[iOff] != 0x04 ) {
			#ifdef DEBUG_TRACE
			printf("    [TLS12] SKE: P-384 invalid point (len=%d, format=0x%02x)\n", iPointLen, pMsg[iOff]);
			#endif
			return false;
		}
	} else if ( iCurve == 0x001d ) {  // X25519
		iExpectedLen = 32;
		if ( iPointLen != 32 ) {
			#ifdef DEBUG_TRACE
			printf("    [TLS12] SKE: X25519 invalid point (len=%d, expected 32)\n", iPointLen);
			#endif
			return false;
		}
	} else {  // X448
		iExpectedLen = 56;
		if ( iPointLen != 56 ) {
			#ifdef DEBUG_TRACE
			printf("    [TLS12] SKE: X448 invalid point (len=%d, expected 56)\n", iPointLen);
			#endif
			return false;
		}
	}
	
	if ( iOff + iExpectedLen > iLen ) return false;
	iOff += iExpectedLen;
	
	// 保存 server_params 的位置和长度 (用于签名验证)
	size_t iParamsLen = iOff;  // curve_type(1) + named_curve(2) + point_len(1) + point
	
	// 解析签名: sig_hash_alg(2) + sig_len(2) + signature
	if ( iOff + 4 > iLen ) return false;
	uint16 iSigAlg = __xrt_tls_load_be16(pMsg + iOff);
	iOff += 2;
	uint16 iSigLen = __xrt_tls_load_be16(pMsg + iOff);
	iOff += 2;
	if ( iOff + iSigLen > iLen ) return false;
	const uint8 *pSig = pMsg + iOff;

	#ifdef DEBUG_TRACE
		printf("    [TLS12] SKE raw (%d): ", (int)iLen);
		for (int i = 0; i < (int)iLen; i++) printf("%02x", pMsg[i]);
		printf("\n");
	#endif
	
	// 验证签名: 对 client_random(32) + server_random(32) + server_params 进行验签
	if ( !pCtx->bSkipVerify ) {
		uint8 aSigBuf[192];
		size_t iSigBufLen = 32 + 32 + iParamsLen;
		uint8 *pSigData = (iSigBufLen <= sizeof(aSigBuf)) ? aSigBuf : (uint8*)xrtMalloc(iSigBufLen);
		bool bVerifyOK = false;
		if ( !pSigData ) return false;
		
		memcpy(pSigData, pCtx->aRandom, 32);          // client_random
		memcpy(pSigData + 32, pCtx->aServerRandom, 32); // server_random
		memcpy(pSigData + 64, pMsg, iParamsLen);         // server_params
		
		if ( iSigAlg == 0x0807 ) {
			if ( !pCtx->bIsEd25519Key || pCtx->iPubKeySz != 32 ) {
				if ( pSigData != aSigBuf ) xrtFree(pSigData);
				return false;
			}
			bVerifyOK = xrtEd25519Verify(pSigData, iSigBufLen, pSig, pCtx->aPubKey);
		} else {
			uint8 aHash[64];
			size_t iHashLen = __xrt_tls_sigalg_hash_len(iSigAlg);
			uint8 iSigType = iSigAlg & 0xFF;  // signature algorithm type

			if ( iHashLen == 0 || !__xrt_tls_hash_bytes(aHash, iHashLen, pSigData, iSigBufLen) ) {
				if ( pSigData != aSigBuf ) xrtFree(pSigData);
				return false;
			}

			#ifdef DEBUG_TRACE
				printf("    [TLS12] SKE hash (%d): ", (int)iHashLen);
				for (int i = 0; i < (int)iHashLen; i++) printf("%02x", aHash[i]);
				printf("\n    [TLS12] SKE sig (%d): ", (int)iSigLen);
				for (int i = 0; i < (int)iSigLen; i++) printf("%02x", pSig[i]);
				printf("\n    [TLS12] SKE pub (%d): ", (int)pCtx->iPubKeySz);
				for (int i = 0; i < (int)pCtx->iPubKeySz; i++) printf("%02x", pCtx->aPubKey[i]);
				printf("\n");
			#endif

			if ( iSigAlg == 0x0804 || iSigAlg == 0x0805 || iSigAlg == 0x0806 ||
				iSigAlg == 0x0809 || iSigAlg == 0x080a || iSigAlg == 0x080b ) {
				bool bIsPssPss = (iSigAlg == 0x0809 || iSigAlg == 0x080a || iSigAlg == 0x080b);
				if ( bIsPssPss != pCtx->bIsRSAPSSKey ) {
					if ( pSigData != aSigBuf ) xrtFree(pSigData);
					return false;
				}
				// RSA-PSS (TLS 1.3 style sig algs used in TLS 1.2)
				bVerifyOK = xrtRSAPSSVerify(aHash, iHashLen, pSig, iSigLen,
					pCtx->aPubKey, pCtx->iPubKeyModSz,
					pCtx->aPubKey + pCtx->iPubKeyModSz,
					pCtx->iPubKeySz - pCtx->iPubKeyModSz);
			} else if ( iSigType == 1 ) {  // RSA-PKCS1
				bVerifyOK = xrtRSAPKCS1Verify(aHash, iHashLen, pSig, iSigLen,
					pCtx->aPubKey, pCtx->iPubKeyModSz,
					pCtx->aPubKey + pCtx->iPubKeyModSz,
					pCtx->iPubKeySz - pCtx->iPubKeyModSz);
			} else if ( iSigType == 3 ) {  // ECDSA
				bVerifyOK = xrtECDSAVerify(aHash, iHashLen, pSig, iSigLen,
					pCtx->aPubKey, pCtx->iPubKeySz);
			}
		}

		if ( pSigData != aSigBuf ) xrtFree(pSigData);
		
		if ( !bVerifyOK ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS12] ServerKeyExchange: signature verify FAILED (alg=0x%04x)\n", iSigAlg);
			#endif
			return false;
		}
		#ifdef DEBUG_TRACE
			printf("    [TLS12] ServerKeyExchange: signature verify OK (alg=0x%04x)\n", iSigAlg);
		#endif
	}
	
	// 根据曲线类型生成密钥对并计算共享密钥
	if ( iCurve == 0x0017 ) {  // P-256
		// 生成 ECDH P-256 密钥对
		xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
		// 计算共享密钥
		xrtECDHSecp256r1SharedSecret(pCtx->aSharedSecret, pCtx->aP256Priv, pServerPub);
		pCtx->iSharedSecretLen = 32;
		#ifdef DEBUG_TRACE
			printf("    [TLS12] ServerKeyExchange: P-256 ECDH shared secret computed\n");
			printf("    [TLS12]   ServerPub: ");
			for (int i = 0; i < 65; i++) printf("%02x", pServerPub[i]);
			printf("\n    [TLS12]   ClientPub: ");
			for (int i = 0; i < 65; i++) printf("%02x", pCtx->aP256Pub[i]);
			printf("\n    [TLS12]   SharedSecret: ");
			for (int i = 0; i < 32; i++) printf("%02x", pCtx->aSharedSecret[i]);
			printf("\n");
		#endif
	} else if ( iCurve == 0x0018 ) {
		xrtECDHSecp384r1Keypair(pCtx->aP384Priv, pCtx->aP384Pub);
		xrtECDHSecp384r1SharedSecret(pCtx->aSharedSecret, pCtx->aP384Priv, pServerPub);
		pCtx->iSharedSecretLen = 48;
		#ifdef DEBUG_TRACE
			printf("    [TLS12] ServerKeyExchange: P-384 ECDH shared secret computed\n");
		#endif
	} else if ( iCurve == 0x001d ) {  // X25519
		// 生成 X25519 密钥对
		xrtX25519Keypair(pCtx->aX25519Priv, pCtx->aX25519Pub);
		// 计算共享密钥
		xrtX25519SharedSecret(pCtx->aSharedSecret, pCtx->aX25519Priv, pServerPub);
		pCtx->iSharedSecretLen = 32;
		#ifdef DEBUG_TRACE
			printf("    [TLS12] ServerKeyExchange: X25519 ECDH shared secret computed\n");
			printf("    [TLS12]   ServerPub: ");
			for (int i = 0; i < 32; i++) printf("%02x", pServerPub[i]);
			printf("\n    [TLS12]   ClientPub: ");
			for (int i = 0; i < 32; i++) printf("%02x", pCtx->aX25519Pub[i]);
			printf("\n    [TLS12]   SharedSecret: ");
			for (int i = 0; i < 32; i++) printf("%02x", pCtx->aSharedSecret[i]);
			printf("\n");
		#endif
	} else {  // X448
		xrtX448Keypair(pCtx->aX448Priv, pCtx->aX448Pub);
		xrtX448SharedSecret(pCtx->aSharedSecret, pCtx->aX448Priv, pServerPub);
		pCtx->iSharedSecretLen = 56;
		#ifdef DEBUG_TRACE
			printf("    [TLS12] ServerKeyExchange: X448 ECDH shared secret computed\n");
		#endif
	}
	
	return true;
}

// Step 7: 构造并发送 ClientKeyExchange
static void __xrt_tls12_send_client_key_exchange(xtlsctx *pCtx)
{
	uint8 aBuf[512];
	size_t iPos = 0;
	
	// 握手消息头: type=16(ClientKeyExchange)
	aBuf[iPos++] = __XRT_TLS_CLIENT_KEY_EXCHANGE;
	size_t iLenPos = iPos;
	iPos += 3;  // 预留长度
	
	if ( pCtx->bIsECDHE ) {
		// ECDHE: 发送客户端公钥
		if ( pCtx->iTls12Curve == 0x0017 ) {  // P-256
			aBuf[iPos++] = 65;  // point length
			memcpy(aBuf + iPos, pCtx->aP256Pub, 65);
			iPos += 65;
		} else if ( pCtx->iTls12Curve == 0x0018 ) {
			aBuf[iPos++] = 97;
			memcpy(aBuf + iPos, pCtx->aP384Pub, 97);
			iPos += 97;
		} else if ( pCtx->iTls12Curve == 0x001d ) {  // X25519
			aBuf[iPos++] = 32;  // point length
			memcpy(aBuf + iPos, pCtx->aX25519Pub, 32);
			iPos += 32;
		} else {  // X448
			aBuf[iPos++] = 56;
			memcpy(aBuf + iPos, pCtx->aX448Pub, 56);
			iPos += 56;
		}
	} else {
		// RSA: 生成 pre_master_secret, PKCS#1 v1.5 加密后发送
		// pre_master_secret = version(2) + random(46)
		pCtx->aPreMaster[0] = 0x03;
		pCtx->aPreMaster[1] = 0x03;  // TLS 1.2 version
		xrtRandomBytes(pCtx->aPreMaster + 2, 46);
		
		// PKCS#1 v1.5 加密: 0x00 0x02 [random non-zero padding] 0x00 [data]
		size_t iModSz = pCtx->iPubKeyModSz;
		uint8 *pPadded = (uint8*)xrtMalloc(iModSz);
		if ( !pPadded ) return;
		
		pPadded[0] = 0x00;
		pPadded[1] = 0x02;
		size_t iPadLen = iModSz - 3 - 48;  // padding length
		xrtRandomBytes(pPadded + 2, iPadLen);
		// 确保 padding 无零字节
		for ( size_t i = 0; i < iPadLen; i++ ) {
			while ( pPadded[2 + i] == 0 ) xrtRandomBytes(pPadded + 2 + i, 1);
		}
		pPadded[2 + iPadLen] = 0x00;
		memcpy(pPadded + 3 + iPadLen, pCtx->aPreMaster, 48);
		
		// RSA 幂运算
		uint8 *pEncrypted = (uint8*)xrtMalloc(iModSz);
		if ( !pEncrypted ) { xrtFree(pPadded); return; }
		
		xrtRSAModPow(pCtx->aPubKey, iModSz,
			pCtx->aPubKey + pCtx->iPubKeyModSz,
			pCtx->iPubKeySz - pCtx->iPubKeyModSz,
			pPadded, iModSz,
			pEncrypted, iModSz);
		
		// 写入: encrypted_len(2) + encrypted_data
		__xrt_tls_store_be16(aBuf + iPos, (uint16)iModSz);
		iPos += 2;
		memcpy(aBuf + iPos, pEncrypted, iModSz);
		iPos += iModSz;
		
		xrtFree(pPadded);
		xrtFree(pEncrypted);
	}
	
	// 填充消息长度
	__xrt_tls_store_be24(aBuf + iLenPos, (uint32)(iPos - iLenPos - 3));
	
	// 更新握手哈希
	__xrt_tls12_update_hash(pCtx, aBuf, iPos);
	
	// 包装为 TLS record (明文)
	uint8 aRec[5];
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aRec + 3, (uint16)iPos);
	
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aRec, 5);
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aBuf, iPos);
}

// Step 8: TLS 1.2 密钥派生
static void __xrt_tls12_derive_keys(xtlsctx *pCtx)
{
	const uint8 *pPreMaster;
	size_t iPMLen;
	
	if ( pCtx->bIsECDHE ) {
		pPreMaster = pCtx->aSharedSecret;
		iPMLen = pCtx->iSharedSecretLen;
	} else {
		pPreMaster = pCtx->aPreMaster;
		iPMLen = 48;
	}
	
	// master_secret = PRF(pre_master, "master secret", client_random + server_random)[0..47]
	uint8 aSeed[64];
	memcpy(aSeed, pCtx->aRandom, 32);          // client_random
	memcpy(aSeed + 32, pCtx->aServerRandom, 32); // server_random
	
	__xrt_tls12_prf(pCtx, pCtx->aMasterSecret, 48,
		pPreMaster, iPMLen,
		"master secret", 13, aSeed, 64);
	
	// key_block = PRF(master, "key expansion", server_random + client_random)
	uint8 aSeed2[64];
	memcpy(aSeed2, pCtx->aServerRandom, 32);     // server_random 先
	memcpy(aSeed2 + 32, pCtx->aRandom, 32);      // client_random 后
	
	// AEAD key_block = client_write_key + server_write_key + client_IV + server_IV
	size_t iIVLen = __xrt_tls12_get_iv_len(pCtx);
	size_t iBlockLen = pCtx->iKeyLen * 2 + iIVLen * 2;
	uint8 aKeyBlock[88];
	
	__xrt_tls12_prf(pCtx, aKeyBlock, iBlockLen,
		pCtx->aMasterSecret, 48,
		"key expansion", 13, aSeed2, 64);
	
	// 拆分 key_block
	size_t iOff = 0;
	memcpy(pCtx->aClientWriteKey12, aKeyBlock + iOff, pCtx->iKeyLen); iOff += pCtx->iKeyLen;
	memcpy(pCtx->aServerWriteKey12, aKeyBlock + iOff, pCtx->iKeyLen); iOff += pCtx->iKeyLen;
	memset(pCtx->aClientWriteIV12, 0, sizeof(pCtx->aClientWriteIV12));
	memset(pCtx->aServerWriteIV12, 0, sizeof(pCtx->aServerWriteIV12));
	memcpy(pCtx->aClientWriteIV12, aKeyBlock + iOff, iIVLen); iOff += iIVLen;
	memcpy(pCtx->aServerWriteIV12, aKeyBlock + iOff, iIVLen);
	
	// 重置序列号
	pCtx->iClientSeq12 = 0;
	pCtx->iServerSeq12 = 0;
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Keys derived: keyLen=%d\n", pCtx->iKeyLen);
		
		// PRF test vector verification
		{
			uint8 tSecret[] = {0x9b,0xbe,0x43,0x6b,0xa9,0x40,0xf0,0x17,0xb1,0x76,0x52,0x84,0x9a,0x71,0xdb,0x35};
			uint8 tSeed[] = {0xa0,0xba,0x9f,0x93,0x6c,0xda,0x31,0x18,0x27,0xa6,0xf7,0x96,0xff,0xd5,0x19,0x8c};
			uint8 tExpected[] = {0xe3,0xf2,0x29,0xba,0x72,0x7b,0xe1,0x7b,0x8d,0x12,0x26,0x20,0x55,0x7c,0xd4,0x53,
				0xc2,0xaa,0xb2,0x1d,0x07,0xc3,0xd4,0x95,0x32,0x9b,0x52,0xd4,0xe6,0x1e,0xdb,0x5a};
			uint8 tOut[32];
			__xrt_tls12_prf_sha256(tOut, 32, tSecret, 16, "test label", 10, tSeed, 16);
			printf("    [TLS12]   PRF test vector: %s\n", memcmp(tOut, tExpected, 32) == 0 ? "PASS" : "FAIL");
			if (memcmp(tOut, tExpected, 32) != 0) {
				printf("    [TLS12]     Got:      ");
				for (int i = 0; i < 32; i++) printf("%02x", tOut[i]);
				printf("\n    [TLS12]     Expected: ");
				for (int i = 0; i < 32; i++) printf("%02x", tExpected[i]);
				printf("\n");
			}
		}
		printf("    [TLS12]   PreMaster: ");
		for (int i = 0; i < (int)iPMLen; i++) printf("%02x", pPreMaster[i]);
		printf("\n    [TLS12]   MasterSecret: ");
		for (int i = 0; i < 48; i++) printf("%02x", pCtx->aMasterSecret[i]);
		printf("\n    [TLS12]   ClientWriteKey: ");
		for (int i = 0; i < (int)pCtx->iKeyLen; i++) printf("%02x", pCtx->aClientWriteKey12[i]);
		printf("\n    [TLS12]   ServerWriteKey: ");
		for (int i = 0; i < (int)pCtx->iKeyLen; i++) printf("%02x", pCtx->aServerWriteKey12[i]);
		printf("\n    [TLS12]   ClientIV: ");
		for (int i = 0; i < (int)iIVLen; i++) printf("%02x", pCtx->aClientWriteIV12[i]);
		printf("\n    [TLS12]   ServerIV: ");
		for (int i = 0; i < (int)iIVLen; i++) printf("%02x", pCtx->aServerWriteIV12[i]);
		printf("\n    [TLS12]   ClientRandom: ");
		for (int i = 0; i < 32; i++) printf("%02x", pCtx->aRandom[i]);
		printf("\n    [TLS12]   ServerRandom: ");
		for (int i = 0; i < 32; i++) printf("%02x", pCtx->aServerRandom[i]);
		printf("\n");
	#endif
	
	// 清零临时数据
	memset(aKeyBlock, 0, sizeof(aKeyBlock));
}

// Step 9: 发送 ChangeCipherSpec + Finished
static void __xrt_tls12_send_ccs_finished(xtlsctx *pCtx, bool bAsServer)
{
	const char *sLabel = bAsServer ? "server finished" : "client finished";

	// 1. 发送 ChangeCipherSpec 记录 (明文, 1字节 0x01)
	uint8 aCCS[6];
	aCCS[0] = __XRT_TLS_CHANGE_CIPHER;
	__xrt_tls_store_be16(aCCS + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aCCS + 3, 1);
	aCCS[5] = 0x01;
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aCCS, 6);
	
	// 2. 计算 verify_data = PRF(master_secret, label, Hash(handshake_messages))[0..11]
	uint8 aHash[48];
	__xrt_tls12_get_hash(pCtx, aHash);
	
	uint8 aVerifyData[12];
	size_t iHashLen = pCtx->bUseSHA384 ? 48 : 32;
	__xrt_tls12_prf(pCtx, aVerifyData, 12,
		pCtx->aMasterSecret, 48,
		sLabel, 15, aHash, iHashLen);
	
	// 3. 构造 Finished 握手消息
	uint8 aMsg[16];
	aMsg[0] = __XRT_TLS_FINISHED;
	__xrt_tls_store_be24(aMsg + 1, 12);
	memcpy(aMsg + 4, aVerifyData, 12);
	
	// 更新握手哈希 (包含客户端 Finished)
	__xrt_tls12_update_hash(pCtx, aMsg, 16);
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Finished hash: ");
		for (int i = 0; i < (int)iHashLen; i++) printf("%02x", aHash[i]);
		printf("\n    [TLS12] %s VerifyData: ", bAsServer ? "Server" : "Client");
		for (int i = 0; i < 12; i++) printf("%02x", aVerifyData[i]);
		printf("\n    [TLS12] FinishedMsg: ");
		for (int i = 0; i < 16; i++) printf("%02x", aMsg[i]);
		printf("\n");
	#endif
	
	// 4. 用 TLS 1.2 GCM 加密发送 Finished
	__xrt_tls12_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, aMsg, 16);
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Sent CCS + Finished\n");
		// Dump entire send buffer
		printf("    [TLS12] SendBuf (%d bytes):\n    ", (int)pCtx->tSendBuf.iSize);
		for (int i = 0; i < (int)pCtx->tSendBuf.iSize; i++) {
			printf("%02x ", (uint8)pCtx->tSendBuf.pData[i]);
			if ((i+1) % 32 == 0) printf("\n    ");
		}
		printf("\n");
	#endif
}

static bool __xrt_tls12_verify_finished(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen, bool bFromServer)
{
	const char *sLabel = bFromServer ? "server finished" : "client finished";

	if ( iLen < 12 ) return false;
	
	// 计算 expected = PRF(master_secret, label, Hash(handshake_messages))[0..11]
	uint8 aHash[48];
	__xrt_tls12_get_hash(pCtx, aHash);
	
	uint8 aExpected[12];
	size_t iHashLen = pCtx->bUseSHA384 ? 48 : 32;
	__xrt_tls12_prf(pCtx, aExpected, 12,
		pCtx->aMasterSecret, 48,
		sLabel, 15, aHash, iHashLen);
	
	// 安全比较
	uint8 iDiff = 0;
	for ( int i = 0; i < 12; i++ ) iDiff |= aExpected[i] ^ pMsg[i];
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Verify %s Finished: %s\n", bFromServer ? "server" : "client",
			iDiff ? "FAILED" : "OK");
	#endif
	return (iDiff == 0);
}

static bool __xrt_tls12_send_handshake_message(xtlsctx *pCtx, const uint8 *pMsg, size_t iMsgLen)
{
	uint8 aRec[5];

	if ( !pCtx || !pMsg || iMsgLen == 0 || iMsgLen > 0xffff ) return false;
	__xrt_tls12_update_hash(pCtx, pMsg, iMsgLen);
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aRec + 3, (uint16)iMsgLen);
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aRec, sizeof(aRec));
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)pMsg, iMsgLen);
	return true;
}

static bool __xrt_tls_sign_server_hash(xtlsctx *pCtx, const uint8 *pHash, size_t iHashLen,
	uint8 *pSig, size_t *pSigLen);

static bool __xrt_tls12_send_server_hello(xtlsctx *pCtx)
{
	uint8 aMsg[128];
	size_t iPos = 0;
	size_t iLenPos;

	if ( !pCtx ) return false;

	xrtRandomBytes(pCtx->aServerRandom, 32);
	aMsg[iPos++] = __XRT_TLS_SERVER_HELLO;
	iLenPos = iPos;
	iPos += 3;
	__xrt_tls_store_be16(aMsg + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	memcpy(aMsg + iPos, pCtx->aServerRandom, 32);
	iPos += 32;
	aMsg[iPos++] = pCtx->iSessionIdLen;
	memcpy(aMsg + iPos, pCtx->aSessionId, pCtx->iSessionIdLen);
	iPos += pCtx->iSessionIdLen;
	__xrt_tls_store_be16(aMsg + iPos, pCtx->iCipherSuite);
	iPos += 2;
	aMsg[iPos++] = 0;
	if ( pCtx->bPeerSecureReneg ) {
		size_t iExtPos = iPos;
		iPos += 2;
		__xrt_tls_store_be16(aMsg + iPos, 0xff01);
		iPos += 2;
		__xrt_tls_store_be16(aMsg + iPos, 1);
		iPos += 2;
		aMsg[iPos++] = 0;
		__xrt_tls_store_be16(aMsg + iExtPos, (uint16)(iPos - iExtPos - 2));
	} else {
		__xrt_tls_store_be16(aMsg + iPos, 0);
		iPos += 2;
	}
	__xrt_tls_store_be24(aMsg + iLenPos, (uint32)(iPos - iLenPos - 3));
	return __xrt_tls12_send_handshake_message(pCtx, aMsg, iPos);
}

static bool __xrt_tls12_send_certificate(xtlsctx *pCtx)
{
	size_t iBodyLen;
	size_t iMsgLen;
	uint8 *pMsg;

	if ( !pCtx || !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) return false;

	iBodyLen = 3 + 3 + pCtx->iCertDerLen;
	iMsgLen = __XRT_TLS_MSGHDR_SIZE + iBodyLen;
	pMsg = (uint8*)xrtMalloc(iMsgLen);
	if ( !pMsg ) return false;

	pMsg[0] = __XRT_TLS_CERTIFICATE;
	__xrt_tls_store_be24(pMsg + 1, (uint32)iBodyLen);
	__xrt_tls_store_be24(pMsg + 4, (uint32)(3 + pCtx->iCertDerLen));
	__xrt_tls_store_be24(pMsg + 7, (uint32)pCtx->iCertDerLen);
	memcpy(pMsg + 10, pCtx->pCertDer, pCtx->iCertDerLen);
	if ( !__xrt_tls12_send_handshake_message(pCtx, pMsg, iMsgLen) ) {
		xrtFree(pMsg);
		return false;
	}
	xrtFree(pMsg);
	return true;
}

static bool __xrt_tls12_send_server_key_exchange(xtlsctx *pCtx)
{
	uint8 aMsg[1024];
	uint8 aSigInput[256];
	uint8 aHash[64];
	uint8 aSig[512];
	size_t iPos = 0;
	size_t iLenPos;
	size_t iPubLen;
	size_t iParamsLen;
	size_t iHashLen = 0;
	size_t iSigLen = 0;
	uint16 iHashAlg;

	if ( !pCtx || !pCtx->bIsECDHE || pCtx->iServerSigAlg == 0 ) return false;

	aMsg[iPos++] = __XRT_TLS_SERVER_KEY_EXCHANGE;
	iLenPos = iPos;
	iPos += 3;
	aMsg[iPos++] = 3;
	__xrt_tls_store_be16(aMsg + iPos, pCtx->iTls12Curve);
	iPos += 2;

	if ( pCtx->iTls12Curve == 0x001d ) {
		iPubLen = 32;
		aMsg[iPos++] = 32;
		memcpy(aMsg + iPos, pCtx->aX25519Pub, 32);
	} else if ( pCtx->iTls12Curve == 0x001e ) {
		iPubLen = 56;
		aMsg[iPos++] = 56;
		memcpy(aMsg + iPos, pCtx->aX448Pub, 56);
	} else if ( pCtx->iTls12Curve == 0x0017 ) {
		iPubLen = 65;
		aMsg[iPos++] = 65;
		memcpy(aMsg + iPos, pCtx->aP256Pub, 65);
	} else if ( pCtx->iTls12Curve == 0x0018 ) {
		iPubLen = 97;
		aMsg[iPos++] = 97;
		memcpy(aMsg + iPos, pCtx->aP384Pub, 97);
	} else {
		return false;
	}
	iPos += iPubLen;
	iParamsLen = iPos - __XRT_TLS_MSGHDR_SIZE;

	memcpy(aSigInput, pCtx->aRandom, 32);
	memcpy(aSigInput + 32, pCtx->aServerRandom, 32);
	memcpy(aSigInput + 64, aMsg + __XRT_TLS_MSGHDR_SIZE, iParamsLen);

	iHashAlg = (uint16)(pCtx->iServerSigAlg >> 8);
	if ( pCtx->iServerSigAlg == 0x0807 ) {
		if ( !__xrt_tls_sign_server_hash(pCtx, aSigInput, 64 + iParamsLen, aSig, &iSigLen) ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS12] SKE sign FAILED: sigAlg=0x%04x rawLen=%d curve=0x%04x\n",
					pCtx->iServerSigAlg, (int)(64 + iParamsLen), pCtx->iTls12Curve);
			#endif
			return false;
		}
	} else {
		iHashLen = __xrt_tls_sigalg_hash_len(pCtx->iServerSigAlg);
		if ( iHashLen == 0 || !__xrt_tls_hash_bytes(aHash, iHashLen, aSigInput, 64 + iParamsLen) ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS12] SKE sign: unsupported hash alg 0x%02x\n", iHashAlg);
			#endif
			return false;
		}
		if ( !__xrt_tls_sign_server_hash(pCtx, aHash, iHashLen, aSig, &iSigLen) ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS12] SKE sign FAILED: sigAlg=0x%04x hashLen=%d modSz=%d privSz=%d curve=0x%04x\n",
					pCtx->iServerSigAlg, (int)iHashLen, (int)pCtx->iPubKeyModSz,
					(int)pCtx->iRSAPrivExpSz, pCtx->iTls12Curve);
			#endif
			return false;
		}
	}

	if ( pCtx->iServerSigAlg != 0x0807 && !__xrt_tls_sign_server_hash ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS12] SKE sign: impossible state\n");
		#endif
		return false;
	}

	__xrt_tls_store_be16(aMsg + iPos, pCtx->iServerSigAlg);
	iPos += 2;
	__xrt_tls_store_be16(aMsg + iPos, (uint16)iSigLen);
	iPos += 2;
	memcpy(aMsg + iPos, aSig, iSigLen);
	iPos += iSigLen;

	#ifdef DEBUG_TRACE
		printf("    [TLS12] SKE send raw (%d): ", (int)(iPos - __XRT_TLS_MSGHDR_SIZE));
		for (int i = (int)__XRT_TLS_MSGHDR_SIZE; i < (int)iPos; i++) printf("%02x", aMsg[i]);
		printf("\n");
	#endif

	__xrt_tls_store_be24(aMsg + iLenPos, (uint32)(iPos - iLenPos - 3));
	return __xrt_tls12_send_handshake_message(pCtx, aMsg, iPos);
}

static bool __xrt_tls12_send_server_hello_done(xtlsctx *pCtx)
{
	uint8 aMsg[4];

	if ( !pCtx ) return false;
	aMsg[0] = __XRT_TLS_SERVER_HELLO_DONE;
	__xrt_tls_store_be24(aMsg + 1, 0);
	return __xrt_tls12_send_handshake_message(pCtx, aMsg, sizeof(aMsg));
}

static bool __xrt_tls12_send_server_flight(xtlsctx *pCtx)
{
	if ( !__xrt_tls12_send_server_hello(pCtx) ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS12] send ServerHello FAILED\n");
		#endif
		return false;
	}
	if ( !__xrt_tls12_send_certificate(pCtx) ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS12] send Certificate FAILED (certLen=%d)\n", (int)pCtx->iCertDerLen);
		#endif
		return false;
	}
	if ( pCtx->bIsECDHE && !__xrt_tls12_send_server_key_exchange(pCtx) ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS12] send ServerKeyExchange FAILED\n");
		#endif
		return false;
	}
	return __xrt_tls12_send_server_hello_done(pCtx);
}

static bool __xrt_tls12_parse_client_key_exchange(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	if ( !pCtx || !pMsg || !pCtx->bIsECDHE || iLen < 1 ) return false;

	if ( pCtx->iTls12Curve == 0x001d ) {
		if ( iLen != 33 || pMsg[0] != 32 ) return false;
		xrtX25519SharedSecret(pCtx->aSharedSecret, pCtx->aX25519Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 32;
		return true;
	}
	if ( pCtx->iTls12Curve == 0x001e ) {
		if ( iLen != 57 || pMsg[0] != 56 ) return false;
		xrtX448SharedSecret(pCtx->aSharedSecret, pCtx->aX448Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 56;
		return true;
	}
	if ( pCtx->iTls12Curve == 0x0017 ) {
		if ( iLen != 66 || pMsg[0] != 65 || pMsg[1] != 0x04 ) return false;
		xrtECDHSecp256r1SharedSecret(pCtx->aSharedSecret, pCtx->aP256Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 32;
		return true;
	}
	if ( pCtx->iTls12Curve == 0x0018 ) {
		if ( iLen != 98 || pMsg[0] != 97 || pMsg[1] != 0x04 ) return false;
		xrtECDHSecp384r1SharedSecret(pCtx->aSharedSecret, pCtx->aP384Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 48;
		return true;
	}
	return false;
}



/* ============================== 公共 API ============================== */

static inline bool __xrt_tls_have_record(xtlsctx *pCtx)
{
	if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) return false;
	uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
	return pCtx->tRecvBuf.iSize >= (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen);
}

static bool __xrt_tls_parse_client_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen);
static bool __xrt_tls13_send_server_hello(xtlsctx *pCtx);
static bool __xrt_tls13_send_server_flight(xtlsctx *pCtx);

XXAPI xtlsctx* xrtTlsCreate(const xtlsconfig *pConfig, bool bIsServer)
{
	xtlsctx *pCtx = (xtlsctx*)xrtCalloc(1, sizeof(xtlsctx));
	if ( !pCtx ) return NULL;
	
	pCtx->bIsServer = bIsServer;
	pCtx->bHandshakeDone = false;
	pCtx->iCipherSuite = __XRT_TLS_CHACHA20_POLY1305_SHA256;  // 默认首选
	pCtx->iState = bIsServer ? XRT_TLS_SERVER_START : XRT_TLS_CLIENT_START;
	
	// 初始化 TLS 1.3 握手哈希上下文
	xrtSHA256Init(&pCtx->tSHA256);
	xrtSHA384Init(&pCtx->tSHA384);
	
	// 初始化 TLS 1.2 哈希上下文 (两个都初始化, 待协商后再选择)
	xrtSHA256Init(&pCtx->tSHA256_12);
	xrtSHA384Init(&pCtx->tSHA384_12);
	
	// 初始化 IO 缓冲区
	__xrt_tls_buf_init(&pCtx->tSendBuf, 8192);
	__xrt_tls_buf_init(&pCtx->tRecvBuf, 8192);
	__xrt_tls_buf_init(&pCtx->tHandshakeBuf, 4096);
	__xrt_tls_buf_init(&pCtx->tPlainBuf, 4096);
	
	if ( pConfig ) {
		pCtx->bSkipVerify = !pConfig->bVerifyPeer;
		pCtx->bAllowTLS12Ed25519 = pConfig->bAllowTLS12Ed25519;
		pCtx->iMaxVersion = pConfig->iMaxVersion;
		if ( pConfig->sHostName ) {
			strncpy(pCtx->sHostname, pConfig->sHostName, sizeof(pCtx->sHostname) - 1);
		}
		pCtx->OnSNI = pConfig->OnSNI;
		pCtx->pSNIUserData = pConfig->pSNIUserData;
		if ( pConfig->pResume && __xrt_tls_resume_copy(&pCtx->tResume, pConfig->pResume) ) {
			pCtx->bHaveResume = true;
			if ( pCtx->iMaxVersion == 0 || pCtx->iMaxVersion > pCtx->tResume.iVersion ) {
				pCtx->iMaxVersion = pCtx->tResume.iVersion;
			}
		}
		if ( (pConfig->sCertFile && pConfig->sCertFile[0]) || (pConfig->sKeyFile && pConfig->sKeyFile[0]) ) {
			if ( xrtTlsSetCert(pCtx, pConfig->sCertFile, pConfig->sKeyFile) != XRT_NET_OK ) {
				xrtTlsDestroy(pCtx);
				return NULL;
			}
		}
		if ( pConfig->bVerifyPeer ) {
			(void)__xrt_tls_load_ca_bundle(pCtx, pConfig->sCaFile);
		}
	} else {
		pCtx->bSkipVerify = true;
	}
	
	return pCtx;
}

XXAPI void xrtTlsDestroy(xtlsctx *pCtx)
{
	if ( !pCtx ) return;
	
	__xrt_tls_buf_free(&pCtx->tSendBuf);
	__xrt_tls_buf_free(&pCtx->tRecvBuf);
	__xrt_tls_buf_free(&pCtx->tHandshakeBuf);
	__xrt_tls_buf_free(&pCtx->tPlainBuf);
	
	if ( pCtx->pCertDer ) xrtFree(pCtx->pCertDer);
	if ( pCtx->pKeyDer ) xrtFree(pCtx->pKeyDer);
	if ( pCtx->pCaData ) xrtFree(pCtx->pCaData);
	
	// 清零敏感数据
	memset(pCtx->aX25519Priv, 0, sizeof(pCtx->aX25519Priv));
	memset(pCtx->aX448Priv, 0, sizeof(pCtx->aX448Priv));
	memset(pCtx->aX25519Secret, 0, sizeof(pCtx->aX25519Secret));
	memset(&pCtx->tEnc, 0, sizeof(struct __xrt_tls_enc));
	memset(&pCtx->tAppKeys, 0, sizeof(struct __xrt_tls_enc));
	memset(&pCtx->tResume, 0, sizeof(pCtx->tResume));
	memset(pCtx->aMasterSecret, 0, 48);
	memset(pCtx->aP256Priv, 0, sizeof(pCtx->aP256Priv));
	memset(pCtx->aP384Priv, 0, sizeof(pCtx->aP384Priv));
	memset(pCtx->aECKey, 0, sizeof(pCtx->aECKey));
	memset(pCtx->aSharedSecret, 0, sizeof(pCtx->aSharedSecret));
	memset(pCtx->aClientWriteKey12, 0, 32);
	memset(pCtx->aServerWriteKey12, 0, 32);
	
	xrtFree(pCtx);
}

static xnet_result __xrt_tls_drive_internal(xtlsctx *pCtx, xsocket hSocket, bool bAllowSocketIO)
{
	if ( !pCtx ) return XRT_NET_ERROR;
	if ( bAllowSocketIO && hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	// 如果有待发送数据，先发送
	if ( pCtx->tSendBuf.iSize > 0 ) {
		if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
		size_t iSent = 0;
		xnet_result iRes = __xrt_tls_sock_send(hSocket, pCtx->tSendBuf.pData, pCtx->tSendBuf.iSize, &iSent);
		if ( iRes == XRT_NET_OK && iSent > 0 ) {
			__xrt_tls_buf_consume(&pCtx->tSendBuf, iSent);
		}
		if ( pCtx->tSendBuf.iSize > 0 ) {
			return XRT_NET_AGAIN;  // 还有数据要发
		}
	}
	
	#ifdef DEBUG_TRACE
		printf("    [TLS] state=%d recvBuf=%d sendBuf=%d\n",
			pCtx->iState, (int)pCtx->tRecvBuf.iSize, (int)pCtx->tSendBuf.iSize);
	#endif
	
	switch ( pCtx->iState ) {
		case XRT_TLS_CLIENT_START:
			__xrt_tls_send_client_hello(pCtx);
			pCtx->iState = XRT_TLS_CLIENT_WAIT_SH;
			return XRT_NET_AGAIN;
		
		case XRT_TLS_CLIENT_WAIT_SH: {
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] WAIT_SH: connection closed\n");
					#endif
					return XRT_NET_ERROR;
				}
			}
			
			// 检查是否有完整记录
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) return XRT_NET_AGAIN;
			
			uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
			if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) return XRT_NET_AGAIN;
			
			const uint8 *pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
			
			#ifdef DEBUG_TRACE
				printf("    [TLS] WAIT_SH: got record type=0x%02x len=%d, msg_type=0x%02x\n",
					(uint8)pCtx->tRecvBuf.pData[0], (int)iRecLen, pRecData[0]);
			#endif
			
			// 检查记录类型
			uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
			if ( iRecType == __XRT_TLS_ALERT ) {
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_SH: server alert level=%d desc=%d\n",
						pRecData[0], (iRecLen >= 2) ? pRecData[1] : -1);
				#endif
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				return XRT_NET_ERROR;
			}
			if ( iRecType != __XRT_TLS_HANDSHAKE ) {
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				return XRT_NET_ERROR;
			}
			
			// 更新握手哈希 (ServerHello 消息)
			__xrt_tls13_hash_update(pCtx, pRecData, iRecLen);
			__xrt_tls12_update_hash(pCtx, pRecData, iRecLen);
			
			// 解析 ServerHello (跳过握手头)
			if ( pRecData[0] == __XRT_TLS_SERVER_HELLO ) {
				if ( !__xrt_tls_parse_server_hello(pCtx, pRecData + __XRT_TLS_MSGHDR_SIZE,
					iRecLen - __XRT_TLS_MSGHDR_SIZE) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] WAIT_SH: parse_server_hello FAILED\n");
					#endif
					return XRT_NET_ERROR;
				}
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_SH: ServerHello parsed, cipher=0x%04x, tls12=%d\n",
						pCtx->iCipherSuite, pCtx->bIsTls12);
				#endif
			}
			
			__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			
			if ( pCtx->bIsTls12 ) {
				// TLS 1.2: 不派生握手密钥, 后续消息仍为明文
				if ( pCtx->bSessionResumed ) {
					__xrt_tls12_derive_keys(pCtx);
					pCtx->iState = XRT_TLS12_CLIENT_WAIT_CCS;
				} else {
					pCtx->iState = XRT_TLS12_CLIENT_WAIT_CERT;
				}
				#ifdef DEBUG_TRACE
					printf("    [TLS] After ServerHello: aRandom: ");
					for (int i = 0; i < 32; i++) printf("%02x", pCtx->aRandom[i]);
					printf("\n");
				#endif
			} else {
				// TLS 1.3: 推导握手密钥
				__xrt_tls_derive_handshake_keys(pCtx);
				pCtx->iState = XRT_TLS_CLIENT_WAIT_EE;
			}
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS_CLIENT_WAIT_EE:
		case XRT_TLS_CLIENT_WAIT_CERT:
		case XRT_TLS_CLIENT_WAIT_CV:
		case XRT_TLS_CLIENT_WAIT_FINISH: {
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_EE+: socket recv returned res=%d, recvd=%d\n", iRes, (int)iRecvd);
				#endif
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
					#ifdef DEBUG_TRACE
						printf("    [TLS] WAIT_EE+: recv %d bytes, recvBuf=%d\n", (int)iRecvd, (int)pCtx->tRecvBuf.iSize);
					#endif
				} else if ( iRes == XRT_NET_CLOSED ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] WAIT_EE+: connection closed\n");
					#endif
					return XRT_NET_ERROR;
				}
			}
			
			// 检查缓冲区是否有可处理的记录
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) {
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_EE+: recvBuf too small (%d), need recv\n", (int)pCtx->tRecvBuf.iSize);
				#endif
				return XRT_NET_AGAIN;
			}
			
			// 处理所有完整记录
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_EE+: check record type=0x%02x len=%d, have=%d\n", 
						iRecType, iRecLen, (int)pCtx->tRecvBuf.iSize);
				#endif
				
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] WAIT_EE+: incomplete record, need %d more bytes\n", 
							(int)(__XRT_TLS_RECHDR_SIZE + iRecLen - pCtx->tRecvBuf.iSize));
					#endif
					break;
				}
				
				if ( iRecType == __XRT_TLS_CHANGE_CIPHER ) {
					// ChangeCipherSpec: 忽略 (TLS 1.3 兼容)
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					continue;
				}
				
				if ( iRecType == __XRT_TLS_APP_DATA ) {
					// 解密记录
					uint8 aPlain[__XRT_TLS_MAX_RECORD];
					size_t iPlainLen = 0;
					uint8 iContentType = 0;
					
					#ifdef DEBUG_TRACE
						printf("    [TLS] WAIT_EE+: decrypting APP_DATA record len=%d\n", iRecLen);
					#endif
					
					if ( !__xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
						__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, true) ) {
						#ifdef DEBUG_TRACE
							printf("    [TLS] WAIT_EE+: decrypt_record FAILED (recLen=%d, cipher=0x%04x)\n",
								(int)iRecLen, pCtx->iCipherSuite);
						#endif
						return XRT_NET_ERROR;
					}
					
					#ifdef DEBUG_TRACE
						printf("    [TLS] WAIT_EE+: decrypt OK, plainLen=%d, contentType=0x%02x\n", 
							(int)iPlainLen, iContentType);
					#endif
					
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					
					if ( iContentType == __XRT_TLS_HANDSHAKE ) {
						// 将解密后的握手明文追加到重组缓冲区 (支持跨记录的大消息)
						__xrt_tls_buf_append(&pCtx->tHandshakeBuf, (const char*)aPlain, iPlainLen);
						
						#ifdef DEBUG_TRACE
							printf("    [TLS] WAIT_EE+: appended %d bytes to handshakeBuf, total=%d\n",
								(int)iPlainLen, (int)pCtx->tHandshakeBuf.iSize);
						#endif
						
						// 从重组缓冲区中解析完整的握手消息
						uint8 *pHsBuf = (uint8*)pCtx->tHandshakeBuf.pData;
						size_t iHsBufLen = pCtx->tHandshakeBuf.iSize;
						size_t iMsgOff = 0;
						
						while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= iHsBufLen ) {
							uint8 iMsgType = pHsBuf[iMsgOff];
							uint32 iMsgBodyLen = __xrt_tls_load_be24(pHsBuf + iMsgOff + 1);
							size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;
							
							#ifdef DEBUG_TRACE
								printf("    [TLS] WAIT_EE+: msg type=%d, bodyLen=%d, totalLen=%d, available=%d\n",
									iMsgType, (int)iMsgBodyLen, (int)iTotalMsgLen, (int)(iHsBufLen - iMsgOff));
							#endif
							
							if ( iMsgOff + iTotalMsgLen > iHsBufLen ) {
								#ifdef DEBUG_TRACE
									printf("    [TLS] WAIT_EE+: message incomplete, need %d more bytes\n",
										(int)(iMsgOff + iTotalMsgLen - iHsBufLen));
								#endif
								break;
							}
							
							uint8 *pMsg = pHsBuf + iMsgOff;
							uint8 aPreMsgHash[64];
							size_t iPreMsgHashLen = 0;
							bool bDelayHashUpdate = false;
							
							if ( iMsgType == __XRT_TLS_CERTIFICATE_VERIFY || iMsgType == __XRT_TLS_FINISHED ) {
								__xrt_tls13_get_transcript_hash(pCtx, aPreMsgHash, &iPreMsgHashLen);
								if ( iMsgType == __XRT_TLS_CERTIFICATE_VERIFY ) {
									memcpy(pCtx->aSigHash, aPreMsgHash, iPreMsgHashLen);
									pCtx->iSigHashLen = iPreMsgHashLen;
								} else {
									bDelayHashUpdate = true;
								}
							}
							
							if ( !bDelayHashUpdate ) {
								__xrt_tls13_hash_update(pCtx, pMsg, iTotalMsgLen);
							}
							
							if ( iMsgType == __XRT_TLS_ENCRYPTED_EXTENSIONS ) {
								pCtx->iState = XRT_TLS_CLIENT_WAIT_CERT;
							} else if ( iMsgType == __XRT_TLS_CERTIFICATE ) {
								uint8 *apCertData[__XRT_TLS_MAX_CERT_CHAIN];
								size_t apCertLen[__XRT_TLS_MAX_CERT_CHAIN];
								size_t iCertCount = 0;

								if ( !__xrt_tls13_collect_certs(pMsg + __XRT_TLS_MSGHDR_SIZE, iMsgBodyLen,
									apCertData, apCertLen, &iCertCount) ) {
									return XRT_NET_ERROR;
								}
								if ( !__xrt_tls_capture_peer_cert_chain(pCtx, apCertData, apCertLen, iCertCount) ) {
									return XRT_NET_ERROR;
								}
								
								pCtx->iState = XRT_TLS_CLIENT_WAIT_CV;
							} else if ( iMsgType == __XRT_TLS_CERTIFICATE_VERIFY ) {
								// 验证 CertificateVerify 签名
								if ( !pCtx->bSkipVerify ) {
									uint16 iSigAlg;
									uint16 iSigLen;
									const uint8 *pSig;
									uint8 aContentHash[192];
									size_t iContentHashLen = 0;
									bool bVerifyOK = false;

									if ( iMsgBodyLen < 4 || pCtx->iSigHashLen == 0 || pCtx->iPubKeySz == 0 ) {
										return XRT_NET_ERROR;
									}

									iSigAlg = __xrt_tls_load_be16(pMsg + __XRT_TLS_MSGHDR_SIZE);
									iSigLen = __xrt_tls_load_be16(pMsg + __XRT_TLS_MSGHDR_SIZE + 2);
									if ( 4 + iSigLen > iMsgBodyLen ) return XRT_NET_ERROR;
									pSig = pMsg + __XRT_TLS_MSGHDR_SIZE + 4;

									if ( !__xrt_tls13_build_cert_verify_input(aContentHash, &iContentHashLen,
										pCtx->aSigHash, pCtx->iSigHashLen, iSigAlg, true) ) {
										return XRT_NET_ERROR;
									}

									if ( pCtx->bIsEd25519Key ) {
										if ( iSigAlg != 0x0807 || pCtx->iPubKeySz != 32 ) return XRT_NET_ERROR;
										bVerifyOK = xrtEd25519Verify(aContentHash, iContentHashLen, pSig, pCtx->aPubKey);
									} else if ( pCtx->bIsECPubKey ) {
										if ( (pCtx->iPubKeySz == 65 && iSigAlg != 0x0403)
											|| (pCtx->iPubKeySz == 97 && iSigAlg != 0x0503)
											|| (pCtx->iPubKeySz != 65 && pCtx->iPubKeySz != 97) ) {
											return XRT_NET_ERROR;
										}
										bVerifyOK = xrtECDSAVerify(aContentHash, iContentHashLen,
											pSig, iSigLen, pCtx->aPubKey, pCtx->iPubKeySz);
									} else if ( pCtx->iPubKeyModSz > 0 ) {
										bool bIsPssPss = (iSigAlg == 0x0809 || iSigAlg == 0x080a || iSigAlg == 0x080b);
										if ( iSigAlg != 0x0804 && iSigAlg != 0x0805 && iSigAlg != 0x0806 &&
											iSigAlg != 0x0809 && iSigAlg != 0x080a && iSigAlg != 0x080b ) {
											return XRT_NET_ERROR;
										}
										if ( bIsPssPss != pCtx->bIsRSAPSSKey ) return XRT_NET_ERROR;
										bVerifyOK = xrtRSAPSSVerify(aContentHash, iContentHashLen,
											pSig, iSigLen,
											pCtx->aPubKey, pCtx->iPubKeyModSz,
											pCtx->aPubKey + pCtx->iPubKeyModSz,
											pCtx->iPubKeySz - pCtx->iPubKeyModSz);
									}

									if ( !bVerifyOK ) return XRT_NET_ERROR;
								} else {
									#ifdef DEBUG_TRACE
										printf("    [TLS] CertificateVerify: skip verify\n");
									#endif
								}
								
								pCtx->iState = XRT_TLS_CLIENT_WAIT_FINISH;
							} else if ( iMsgType == __XRT_TLS_FINISHED ) {
								if ( !__xrt_tls_verify_finished(pCtx,
									pMsg + __XRT_TLS_MSGHDR_SIZE, iMsgBodyLen,
									aPreMsgHash, iPreMsgHashLen, true) ) {
									return XRT_NET_ERROR;
								}
								__xrt_tls13_hash_update(pCtx, pMsg, iTotalMsgLen);
								__xrt_tls_derive_application_keys(pCtx);
								__xrt_tls_send_finished(pCtx, false);
								pCtx->bHandshakeDone = true;
								pCtx->iState = XRT_TLS_CLIENT_CONNECTED;
								return XRT_NET_AGAIN;
							}
							
							iMsgOff += iTotalMsgLen;
						}
						
						// 消费已处理的握手消息
						if ( iMsgOff > 0 ) {
							__xrt_tls_buf_consume(&pCtx->tHandshakeBuf, iMsgOff);
							#ifdef DEBUG_TRACE
								printf("    [TLS] WAIT_EE+: consumed %d bytes from handshakeBuf, remaining=%d\n",
									(int)iMsgOff, (int)pCtx->tHandshakeBuf.iSize);
							#endif
						}
					} else if ( iContentType == __XRT_TLS_ALERT ) {
						return XRT_NET_ERROR;
					}
				} else {
					// 未知记录类型
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				}
			}
			
			return XRT_NET_AGAIN;
		}
		
		// ==================== TLS 1.2 客户端状态处理 ====================
		case XRT_TLS12_CLIENT_WAIT_CERT:
		case XRT_TLS12_CLIENT_WAIT_SKE:
		case XRT_TLS12_CLIENT_WAIT_SHD: {
			#ifdef DEBUG_TRACE
				printf("    [TLS12] CERT/SKE/SHD entry: aRandom: ");
				for (int i = 0; i < 32; i++) printf("%02x", pCtx->aRandom[i]);
				printf("\n");
			#endif
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}
			
			// 处理所有完整记录
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) break;
				
				if ( iRecType == __XRT_TLS_ALERT ) {
					#ifdef DEBUG_TRACE
						const uint8 *pA = (const uint8*)pCtx->tRecvBuf.pData + 5;
						printf("    [TLS12] Alert: level=%d desc=%d\n", pA[0], (iRecLen >= 2) ? pA[1] : -1);
					#endif
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				
				if ( iRecType != __XRT_TLS_HANDSHAKE ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					continue;
				}
				
				// 明文握手记录: 可能包含多个握手消息
				const uint8 *pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
				size_t iMsgOff = 0;
				
				while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= iRecLen ) {
					uint8 iMsgType = pRecData[iMsgOff];
					uint32 iMsgBodyLen = __xrt_tls_load_be24(pRecData + iMsgOff + 1);
					size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;
					
					if ( iMsgOff + iTotalMsgLen > iRecLen ) break;
					
					const uint8 *pFullMsg = pRecData + iMsgOff;
					const uint8 *pMsgBody = pFullMsg + __XRT_TLS_MSGHDR_SIZE;
					
					// 更新 TLS 1.2 握手哈希
					__xrt_tls12_update_hash(pCtx, pFullMsg, iTotalMsgLen);
					
					switch ( iMsgType ) {
						case __XRT_TLS_CERTIFICATE: {
							#ifdef DEBUG_TRACE
								printf("    [TLS12] Got Certificate, len=%d\n", (int)iMsgBodyLen);
							#endif
							if ( !__xrt_tls12_parse_certificate(pCtx, pMsgBody, iMsgBodyLen) ) {
								return XRT_NET_ERROR;
							}
							if ( pCtx->bIsECDHE ) {
								pCtx->iState = XRT_TLS12_CLIENT_WAIT_SKE;
							} else {
								// RSA 套件: 无 ServerKeyExchange, 直接等 ServerHelloDone
								pCtx->iState = XRT_TLS12_CLIENT_WAIT_SHD;
							}
							break;
						}
						case __XRT_TLS_SERVER_KEY_EXCHANGE: {
							#ifdef DEBUG_TRACE
								printf("    [TLS12] Got ServerKeyExchange, len=%d\n", (int)iMsgBodyLen);
							#endif
							if ( !__xrt_tls12_parse_server_key_exchange(pCtx, pMsgBody, iMsgBodyLen) ) {
								return XRT_NET_ERROR;
							}
							pCtx->iState = XRT_TLS12_CLIENT_WAIT_SHD;
							break;
						}
						case __XRT_TLS_SERVER_HELLO_DONE: {
							#ifdef DEBUG_TRACE
								printf("    [TLS12] Got ServerHelloDone\n");
							#endif
							// 发送 ClientKeyExchange + CCS + Finished
							__xrt_tls12_send_client_key_exchange(pCtx);
							__xrt_tls12_derive_keys(pCtx);
							__xrt_tls12_send_ccs_finished(pCtx, false);
							pCtx->iState = XRT_TLS12_CLIENT_WAIT_CCS;
							break;
						}
						default:
							#ifdef DEBUG_TRACE
								printf("    [TLS12] Unknown handshake msg type=%d\n", iMsgType);
							#endif
							break;
					}
					
					iMsgOff += iTotalMsgLen;
				}
				
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				
				// 如果状态已过渡到 WAIT_CCS, 停止处理更多记录
				if ( pCtx->iState == XRT_TLS12_CLIENT_WAIT_CCS ) break;
			}
			
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS12_CLIENT_WAIT_CCS: {
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				#ifdef DEBUG_TRACE
					printf("    [TLS12] WAIT_CCS: recv res=%d recvd=%d\n", (int)iRes, (int)iRecvd);
				#endif
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}
			
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) break;
				
				if ( iRecType == __XRT_TLS_CHANGE_CIPHER ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS12] Got server CCS\n");
					#endif
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					pCtx->iState = XRT_TLS12_CLIENT_WAIT_FINISH;
					break;
				} else if ( iRecType == __XRT_TLS_ALERT ) {
					#ifdef DEBUG_TRACE
						const uint8 *pA = (const uint8*)pCtx->tRecvBuf.pData + 5;
						printf("    [TLS12] WAIT_CCS: Alert level=%d desc=%d\n", pA[0], (iRecLen >= 2) ? pA[1] : -1);
					#endif
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			}
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS12_CLIENT_WAIT_FINISH: {
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}
			
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) return XRT_NET_AGAIN;
			
			uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
			if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) return XRT_NET_AGAIN;
			
			uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
			
			if ( iRecType == __XRT_TLS_ALERT ) {
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				return XRT_NET_ERROR;
			}
			
			// 服务端 Finished 是加密的 (在 CCS 之后)
			if ( iRecType == __XRT_TLS_APP_DATA ) {
				// TLS 1.2 GCM 解密
				uint8 aPlain[__XRT_TLS_MAX_RECORD];
				size_t iPlainLen = 0;
				uint8 iContentType = 0;
				
				if ( !__xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
					__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS12] Failed to decrypt server Finished\n");
					#endif
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				
				// 解析 Finished 握手消息
				if ( iPlainLen >= __XRT_TLS_MSGHDR_SIZE && aPlain[0] == __XRT_TLS_FINISHED ) {
					uint32 iFinLen = __xrt_tls_load_be24(aPlain + 1);
					if ( iFinLen >= 12 ) {
						if ( !__xrt_tls12_verify_finished(pCtx, aPlain + __XRT_TLS_MSGHDR_SIZE, iFinLen, true) ) {
							#ifdef DEBUG_TRACE
								printf("    [TLS12] Server Finished verify FAILED\n");
							#endif
							return XRT_NET_ERROR;
						}
						
						if ( pCtx->bSessionResumed ) {
							__xrt_tls12_update_hash(pCtx, aPlain, __XRT_TLS_MSGHDR_SIZE + iFinLen);
							__xrt_tls12_send_ccs_finished(pCtx, false);
						}
						pCtx->bHandshakeDone = true;
						pCtx->iState = XRT_TLS12_CLIENT_CONNECTED;
						#ifdef DEBUG_TRACE
							printf("    [TLS12] Handshake complete!\n");
						#endif
						return XRT_NET_AGAIN;
					}
				}
				return XRT_NET_ERROR;
			}
			
			// 也可能是 Handshake 类型 (某些服务器发送加密后的 Handshake 记录)
			if ( iRecType == __XRT_TLS_HANDSHAKE ) {
				// 解密
				uint8 aPlain[__XRT_TLS_MAX_RECORD];
				size_t iPlainLen = 0;
				uint8 iContentType = 0;
				
				if ( !__xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
					__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType) ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				
				if ( iPlainLen >= __XRT_TLS_MSGHDR_SIZE && aPlain[0] == __XRT_TLS_FINISHED ) {
					uint32 iFinLen = __xrt_tls_load_be24(aPlain + 1);
					if ( iFinLen >= 12 ) {
						if ( !__xrt_tls12_verify_finished(pCtx, aPlain + __XRT_TLS_MSGHDR_SIZE, iFinLen, true) ) {
							return XRT_NET_ERROR;
						}
						if ( pCtx->bSessionResumed ) {
							__xrt_tls12_update_hash(pCtx, aPlain, __XRT_TLS_MSGHDR_SIZE + iFinLen);
							__xrt_tls12_send_ccs_finished(pCtx, false);
						}
						pCtx->bHandshakeDone = true;
						pCtx->iState = XRT_TLS12_CLIENT_CONNECTED;
						return XRT_NET_AGAIN;
					}
				}
				return XRT_NET_ERROR;
			}
			
			__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS_CLIENT_CONNECTED:
		case XRT_TLS12_CLIENT_CONNECTED:
			// 发送剩余数据
			if ( pCtx->tSendBuf.iSize > 0 ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				size_t iSent = 0;
				__xrt_tls_sock_send(hSocket, pCtx->tSendBuf.pData, pCtx->tSendBuf.iSize, &iSent);
				if ( iSent > 0 ) __xrt_tls_buf_consume(&pCtx->tSendBuf, iSent);
				if ( pCtx->tSendBuf.iSize > 0 ) return XRT_NET_AGAIN;
			}
			return XRT_NET_OK;
		
		case XRT_TLS_SERVER_START:
		{
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) return XRT_NET_AGAIN;
			{
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				const uint8 *pRecData;
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) return XRT_NET_AGAIN;
				if ( (uint8)pCtx->tRecvBuf.pData[0] != __XRT_TLS_HANDSHAKE ) return XRT_NET_ERROR;
				pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
				if ( iRecLen < __XRT_TLS_MSGHDR_SIZE || pRecData[0] != __XRT_TLS_CLIENT_HELLO ) return XRT_NET_ERROR;

				if ( !__xrt_tls_parse_client_hello(pCtx, pRecData + __XRT_TLS_MSGHDR_SIZE,
					iRecLen - __XRT_TLS_MSGHDR_SIZE) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] parse ClientHello FAILED\n");
					#endif
					return XRT_NET_ERROR;
				}

				if ( pCtx->bIsTls12 ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS12] server negotiated suite=0x%04x curve=0x%04x sig=0x%04x\n",
							pCtx->iCipherSuite, pCtx->iTls12Curve, pCtx->iServerSigAlg);
					#endif
					__xrt_tls12_update_hash(pCtx, pRecData, iRecLen);
					if ( pCtx->bSessionResumed ) {
						if ( !__xrt_tls12_send_server_hello(pCtx) ) return XRT_NET_ERROR;
						__xrt_tls12_derive_keys(pCtx);
						__xrt_tls12_send_ccs_finished(pCtx, true);
						pCtx->iState = XRT_TLS12_SERVER_WAIT_CCS;
					} else {
						if ( !__xrt_tls12_send_server_flight(pCtx) ) return XRT_NET_ERROR;
						pCtx->iState = XRT_TLS12_SERVER_WAIT_CKE;
					}
				} else {
					#ifdef DEBUG_TRACE
						printf("    [TLS13] server negotiated suite=0x%04x group=0x%04x sig=0x%04x\n",
							pCtx->iCipherSuite, pCtx->iTls13Group, pCtx->iServerSigAlg);
					#endif
					__xrt_tls13_hash_update(pCtx, pRecData, iRecLen);
					if ( !__xrt_tls13_send_server_hello(pCtx) ) return XRT_NET_ERROR;
					__xrt_tls_derive_handshake_keys(pCtx);
					if ( !__xrt_tls13_send_server_flight(pCtx) ) return XRT_NET_ERROR;
					pCtx->iState = XRT_TLS_SERVER_NEGOTIATED;
				}
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			}
			return XRT_NET_AGAIN;
		}

		case XRT_TLS12_SERVER_WAIT_CKE:
		case XRT_TLS12_SERVER_WAIT_CCS:
		case XRT_TLS12_SERVER_WAIT_FINISH: {
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}

			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);

				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) break;
				if ( iRecType == __XRT_TLS_ALERT ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}

				if ( pCtx->iState == XRT_TLS12_SERVER_WAIT_CKE ) {
					const uint8 *pRecData;
					size_t iMsgOff = 0;

					if ( iRecType != __XRT_TLS_HANDSHAKE ) return XRT_NET_ERROR;
					pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
					__xrt_tls_buf_append(&pCtx->tHandshakeBuf, (const char*)pRecData, iRecLen);
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);

					while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= pCtx->tHandshakeBuf.iSize ) {
						const uint8 *pMsg = (const uint8*)pCtx->tHandshakeBuf.pData + iMsgOff;
						uint8 iMsgType = pMsg[0];
						uint32 iMsgBodyLen = __xrt_tls_load_be24(pMsg + 1);
						size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;

						if ( iMsgOff + iTotalMsgLen > pCtx->tHandshakeBuf.iSize ) break;
						if ( iMsgType != __XRT_TLS_CLIENT_KEY_EXCHANGE ) return XRT_NET_ERROR;
						if ( !__xrt_tls12_parse_client_key_exchange(pCtx,
							pMsg + __XRT_TLS_MSGHDR_SIZE, iMsgBodyLen) ) {
							return XRT_NET_ERROR;
						}
						__xrt_tls12_update_hash(pCtx, pMsg, iTotalMsgLen);
						__xrt_tls12_derive_keys(pCtx);
						__xrt_tls_buf_consume(&pCtx->tHandshakeBuf, iMsgOff + iTotalMsgLen);
						pCtx->iState = XRT_TLS12_SERVER_WAIT_CCS;
						iMsgOff = 0;
					}
					continue;
				}

				if ( pCtx->iState == XRT_TLS12_SERVER_WAIT_CCS ) {
					if ( iRecType != __XRT_TLS_CHANGE_CIPHER ) return XRT_NET_ERROR;
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					pCtx->iState = XRT_TLS12_SERVER_WAIT_FINISH;
					continue;
				}

				if ( iRecType != __XRT_TLS_HANDSHAKE && iRecType != __XRT_TLS_APP_DATA ) {
					return XRT_NET_ERROR;
				}

				{
					uint8 aPlain[__XRT_TLS_MAX_RECORD];
					size_t iPlainLen = 0;
					uint8 iContentType = 0;
					size_t iMsgOff = 0;

					if ( !__xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
						__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType) ) {
						return XRT_NET_ERROR;
					}
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					if ( iContentType == __XRT_TLS_ALERT ) return XRT_NET_ERROR;
					if ( iContentType != __XRT_TLS_HANDSHAKE ) continue;

					__xrt_tls_buf_append(&pCtx->tHandshakeBuf, (const char*)aPlain, iPlainLen);
					while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= pCtx->tHandshakeBuf.iSize ) {
						const uint8 *pMsg = (const uint8*)pCtx->tHandshakeBuf.pData + iMsgOff;
						uint8 iMsgType = pMsg[0];
						uint32 iMsgBodyLen = __xrt_tls_load_be24(pMsg + 1);
						size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;

						if ( iMsgOff + iTotalMsgLen > pCtx->tHandshakeBuf.iSize ) break;
						if ( iMsgType != __XRT_TLS_FINISHED ) return XRT_NET_ERROR;
						if ( !__xrt_tls12_verify_finished(pCtx,
							pMsg + __XRT_TLS_MSGHDR_SIZE, iMsgBodyLen, false) ) {
							return XRT_NET_ERROR;
						}
						__xrt_tls12_update_hash(pCtx, pMsg, iTotalMsgLen);
						__xrt_tls_buf_consume(&pCtx->tHandshakeBuf, iMsgOff + iTotalMsgLen);
						if ( !pCtx->bSessionResumed ) {
							__xrt_tls12_send_ccs_finished(pCtx, true);
						}
						pCtx->bHandshakeDone = true;
						if ( !pCtx->bSessionResumed ) {
							struct xrt_tls_resume tResume;
							if ( __xrt_tls_resume_from_ctx(pCtx, &tResume) ) {
								__xrt_tls_resume_cache_store(&tResume);
							}
						}
						pCtx->iState = XRT_TLS_SERVER_CONNECTED;
						return XRT_NET_AGAIN;
					}
				}
			}
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS_SERVER_NEGOTIATED: {
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) return XRT_NET_AGAIN;
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}

			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);

				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) break;
				if ( iRecType == __XRT_TLS_CHANGE_CIPHER ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					continue;
				}
				if ( iRecType == __XRT_TLS_ALERT ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				if ( iRecType != __XRT_TLS_APP_DATA ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}

				{
					uint8 aPlain[__XRT_TLS_MAX_RECORD];
					size_t iPlainLen = 0;
					uint8 iContentType = 0;
					if ( !__xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
						__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, false) ) {
						return XRT_NET_ERROR;
					}
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);

					if ( iContentType == __XRT_TLS_ALERT ) return XRT_NET_ERROR;
					if ( iContentType != __XRT_TLS_HANDSHAKE ) continue;

					__xrt_tls_buf_append(&pCtx->tHandshakeBuf, (const char*)aPlain, iPlainLen);
					{
						uint8 *pHsBuf = (uint8*)pCtx->tHandshakeBuf.pData;
						size_t iHsBufLen = pCtx->tHandshakeBuf.iSize;
						size_t iMsgOff = 0;

						while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= iHsBufLen ) {
							uint8 *pMsg = pHsBuf + iMsgOff;
							uint8 iMsgType = pMsg[0];
							uint32 iMsgBodyLen = __xrt_tls_load_be24(pMsg + 1);
							size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;
							uint8 aHash[64];
							size_t iHashLen = 0;

							if ( iMsgOff + iTotalMsgLen > iHsBufLen ) break;
							if ( iMsgType != __XRT_TLS_FINISHED ) return XRT_NET_ERROR;
							__xrt_tls13_get_transcript_hash(pCtx, aHash, &iHashLen);
							if ( !__xrt_tls_verify_finished(pCtx, pMsg + __XRT_TLS_MSGHDR_SIZE,
								iMsgBodyLen, aHash, iHashLen, false) ) {
								return XRT_NET_ERROR;
							}
							__xrt_tls13_hash_update(pCtx, pMsg, iTotalMsgLen);
							__xrt_tls_buf_consume(&pCtx->tHandshakeBuf, iMsgOff + iTotalMsgLen);
							pCtx->bHandshakeDone = true;
							pCtx->iState = XRT_TLS_SERVER_CONNECTED;
							return XRT_NET_OK;
						}
					}
				}
			}
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS_SERVER_CONNECTED:
			return XRT_NET_OK;
		
		default:
			return XRT_NET_ERROR;
	}
}

XXAPI xnet_result xrtTlsHandshake(xtlsctx *pCtx, xsocket hSocket)
{
	return __xrt_tls_drive_internal(pCtx, hSocket, true);
}

XXAPI xnet_result xrtTlsDrive(xtlsctx *pCtx)
{
	return __xrt_tls_drive_internal(pCtx, XSOCKET_INVALID, false);
}

XXAPI xnet_result xrtTlsRead(xtlsctx *pCtx, char *pBuf, size_t iLen, size_t *pRead)
{
	if ( !pCtx || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	if ( !pCtx->bHandshakeDone ) return XRT_NET_AGAIN;
	if ( pRead ) *pRead = 0;
	
	if ( pCtx->tPlainBuf.iSize > 0 ) {
		size_t iCopy = pCtx->tPlainBuf.iSize < iLen ? pCtx->tPlainBuf.iSize : iLen;
		memcpy(pBuf, pCtx->tPlainBuf.pData, iCopy);
		__xrt_tls_buf_consume(&pCtx->tPlainBuf, iCopy);
		if ( pRead ) *pRead = iCopy;
		return XRT_NET_OK;
	}
	
	while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
		uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
		uint8 aPlain[__XRT_TLS_MAX_RECORD];
		size_t iPlainLen = 0;
		uint8 iContentType = 0;
		bool bOK;
		
		if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) break;
		
		if ( pCtx->bIsTls12 ) {
			bOK = __xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
				__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType);
		} else {
			bool bUseServerKeys = !pCtx->bIsServer;
			bOK = __xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
				__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, bUseServerKeys);
		}
		
		if ( !bOK ) return XRT_NET_ERROR;
		__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
		
		if ( iContentType == __XRT_TLS_APP_DATA ) {
			if ( iPlainLen > 0 ) {
				if ( !__xrt_tls_buf_append(&pCtx->tPlainBuf, (const char*)aPlain, iPlainLen) ) return XRT_NET_ERROR;
				break;
			}
			continue;
		}
		if ( iContentType == __XRT_TLS_ALERT ) return XRT_NET_CLOSED;
	}
	
	if ( pCtx->tPlainBuf.iSize > 0 ) {
		size_t iCopy = pCtx->tPlainBuf.iSize < iLen ? pCtx->tPlainBuf.iSize : iLen;
		memcpy(pBuf, pCtx->tPlainBuf.pData, iCopy);
		__xrt_tls_buf_consume(&pCtx->tPlainBuf, iCopy);
		if ( pRead ) *pRead = iCopy;
		return XRT_NET_OK;
	}
	
	return XRT_NET_AGAIN;
}

XXAPI xnet_result xrtTlsWrite(xtlsctx *pCtx, const char *pData, size_t iLen, size_t *pWritten)
{
	if ( !pCtx || !pData || iLen == 0 ) return XRT_NET_ERROR;
	if ( !pCtx->bHandshakeDone ) return XRT_NET_AGAIN;
	
	if ( pCtx->bIsTls12 ) {
		__xrt_tls12_encrypt_record(pCtx, __XRT_TLS_APP_DATA, (const uint8*)pData, iLen);
	} else {
		bool bUseServerKeys = pCtx->bIsServer;
		__xrt_tls_encrypt_record(pCtx, __XRT_TLS_APP_DATA, (const uint8*)pData, iLen, bUseServerKeys);
	}
	
	if ( pWritten ) *pWritten = iLen;
	return XRT_NET_OK;
}

XXAPI xnet_result xrtTlsClose(xtlsctx *pCtx)
{
	if ( !pCtx ) return XRT_NET_ERROR;
	
	// 发送 close_notify alert
	uint8 aAlert[2] = { 1, 0 };  // warning, close_notify
	if ( pCtx->bIsTls12 ) {
		__xrt_tls12_encrypt_record(pCtx, __XRT_TLS_ALERT, aAlert, 2);
	} else {
		bool bUseServerKeys = pCtx->bIsServer;
		__xrt_tls_encrypt_record(pCtx, __XRT_TLS_ALERT, aAlert, 2, bUseServerKeys);
	}
	
	return XRT_NET_OK;
}

XXAPI bool xrtTlsIsReady(xtlsctx *pCtx)
{
	if ( !pCtx ) return false;
	return pCtx->bHandshakeDone;
}

XXAPI xnet_result xrtTlsFeed(xtlsctx *pCtx, const char *pData, size_t iLen)
{
	if ( !pCtx || !pData || iLen == 0 ) return XRT_NET_ERROR;
	return __xrt_tls_buf_append(&pCtx->tRecvBuf, pData, iLen) ? XRT_NET_OK : XRT_NET_ERROR;
}

XXAPI size_t xrtTlsPendingSend(xtlsctx *pCtx)
{
	return pCtx ? pCtx->tSendBuf.iSize : 0;
}

XXAPI size_t xrtTlsPendingRecv(xtlsctx *pCtx)
{
	return pCtx ? pCtx->tRecvBuf.iSize : 0;
}

XXAPI xnet_result xrtTlsPeekSend(xtlsctx *pCtx, char *pBuf, size_t iLen, size_t *pRead)
{
	size_t iCopy;
	if ( pRead ) *pRead = 0;
	if ( !pCtx || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	if ( pCtx->tSendBuf.iSize == 0 ) return XRT_NET_AGAIN;
	iCopy = pCtx->tSendBuf.iSize < iLen ? pCtx->tSendBuf.iSize : iLen;
	memcpy(pBuf, pCtx->tSendBuf.pData, iCopy);
	if ( pRead ) *pRead = iCopy;
	return XRT_NET_OK;
}

XXAPI void xrtTlsConsumeSend(xtlsctx *pCtx, size_t iLen)
{
	if ( !pCtx || iLen == 0 || pCtx->tSendBuf.iSize == 0 ) return;
	if ( iLen >= pCtx->tSendBuf.iSize ) {
		__xrt_tls_buf_consume(&pCtx->tSendBuf, pCtx->tSendBuf.iSize);
		return;
	}
	__xrt_tls_buf_consume(&pCtx->tSendBuf, iLen);
}

XXAPI xtlsresume* xrtTlsExportResume(xtlsctx *pCtx)
{
	xtlsresume* pResume;
	if ( !pCtx ) return NULL;
	pResume = (xtlsresume*)xrtCalloc(1, sizeof(xtlsresume));
	if ( !pResume ) return NULL;
	if ( !__xrt_tls_resume_from_ctx(pCtx, pResume) ) {
		xrtFree(pResume);
		return NULL;
	}
	return pResume;
}

XXAPI void xrtTlsResumeDestroy(xtlsresume* pResume)
{
	if ( !pResume ) return;
	memset(pResume, 0, sizeof(*pResume));
	xrtFree(pResume);
}

XXAPI bool xrtTlsWasResumed(xtlsctx *pCtx)
{
	return pCtx ? pCtx->bSessionResumed : false;
}

static void __xrt_tls_send_finished(xtlsctx *pCtx, bool bAsServer);
static void __xrt_tls_derive_application_keys(xtlsctx *pCtx);

// Step 12: 服务端 ClientHello 解析 (提取 SNI)
static bool __xrt_tls_parse_client_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	size_t iPos = 0;
	uint16 iLegacyVersion = 0;
	bool bTls13Supported = false;
	bool bOfferTLS13AES128 = false, bOfferTLS13AES256 = false, bOfferTLS13ChaCha = false;
	bool bOfferTLS12EcdheEcdsaAES128 = false, bOfferTLS12EcdheEcdsaAES256 = false;
	bool bOfferTLS12EcdheEcdsaChaCha = false;
	bool bOfferTLS12EcdheRsaAES128 = false, bOfferTLS12EcdheRsaAES256 = false;
	bool bOfferTLS12EcdheRsaChaCha = false;
	bool bGroupX25519 = false, bGroupX448 = false, bGroupP256 = false, bGroupP384 = false;
	bool bSawPointFormats = false, bPointFmtUncompressed = false;
	bool bPeerSigECDSA256 = false, bPeerSigECDSA384 = false, bPeerSigECDSA512 = false;
	bool bPeerSigRSAPKCS1256 = false, bPeerSigRSAPKCS1384 = false, bPeerSigRSAPKCS1512 = false;
	uint8 aX25519Peer[32];
	uint8 aX448Peer[56];
	uint8 aP256Peer[65];
	uint8 aP384Peer[97];
	bool bHaveX25519 = false, bHaveX448 = false, bHaveP256 = false, bHaveP384 = false;
	struct xrt_tls_resume tCachedResume;
	bool bHaveCachedResume = false;
	bool bAllowTls13 = false;

	if ( !pCtx ) return false;
	pCtx->bPeerSigECDSAP256 = false;
	pCtx->bPeerSigECDSAP384 = false;
	pCtx->bPeerSigEd25519 = false;
	pCtx->bPeerSigRSAPSS256 = false;
	pCtx->bPeerSigRSAPSS384 = false;
	pCtx->bPeerSigRSAPSS512 = false;
	pCtx->bPeerSigRSAPSSPSS256 = false;
	pCtx->bPeerSigRSAPSSPSS384 = false;
	pCtx->bPeerSigRSAPSSPSS512 = false;
	pCtx->bPeerSecureReneg = false;
	pCtx->iPeerKeyShareLen = 0;
	pCtx->iTls13Group = 0;
	pCtx->iSessionIdLen = 0;
	pCtx->iServerSigAlg = 0;
	pCtx->sClientSNI[0] = '\0';
	pCtx->bIsTls12 = false;
	pCtx->bSessionResumed = false;
	bAllowTls13 = (pCtx->iMaxVersion == 0 || pCtx->iMaxVersion >= __XRT_TLS_VERSION_1_3);
	
	// client_version(2)
	if ( iPos + 2 > iLen ) return false;
	iLegacyVersion = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	// client_random(32) — 保存到上下文
	if ( iPos + 32 > iLen ) return false;
	memcpy(pCtx->aRandom, pMsg + iPos, 32);
	iPos += 32;
	
	// session_id
	if ( iPos + 1 > iLen ) return false;
	uint8 iSessLen = pMsg[iPos++];
	if ( iSessLen > 32 || iPos + iSessLen > iLen ) return false;
	pCtx->iSessionIdLen = iSessLen;
	if ( iSessLen <= 32 ) {
		memcpy(pCtx->aSessionId, pMsg + iPos, iSessLen);
	}
	iPos += iSessLen;
	
	// cipher_suites
	if ( iPos + 2 > iLen ) return false;
	uint16 iSuitesLen = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	if ( iPos + iSuitesLen > iLen ) return false;
	for ( size_t i = 0; i + 1 < iSuitesLen; i += 2 ) {
		uint16 iSuite = __xrt_tls_load_be16(pMsg + iPos + i);
		if ( iSuite == __XRT_TLS_AES_128_GCM_SHA256 ) bOfferTLS13AES128 = true;
		else if ( iSuite == __XRT_TLS_AES_256_GCM_SHA384 ) bOfferTLS13AES256 = true;
		else if ( iSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) bOfferTLS13ChaCha = true;
		else if ( iSuite == 0x00ff ) pCtx->bPeerSecureReneg = true;
		else if ( iSuite == __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256 ) bOfferTLS12EcdheEcdsaAES128 = true;
		else if ( iSuite == __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384 ) bOfferTLS12EcdheEcdsaAES256 = true;
		else if ( iSuite == __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256 ) bOfferTLS12EcdheEcdsaChaCha = true;
		else if ( iSuite == __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256 ) bOfferTLS12EcdheRsaAES128 = true;
		else if ( iSuite == __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384 ) bOfferTLS12EcdheRsaAES256 = true;
		else if ( iSuite == __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256 ) bOfferTLS12EcdheRsaChaCha = true;
	}
	iPos += iSuitesLen;
	
	// compression_methods
	if ( iPos + 1 > iLen ) return false;
	uint8 iCompLen = pMsg[iPos++];
	if ( iPos + iCompLen > iLen ) return false;
	iPos += iCompLen;
	
	// 解析扩展
	size_t iExtEnd = iPos;
	if ( iPos + 2 <= iLen ) {
		uint16 iExtTotalLen = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		iExtEnd = iPos + iExtTotalLen;
		if ( iExtEnd > iLen ) iExtEnd = iLen;
	}
	
	while ( iPos + 4 <= iExtEnd ) {
		uint16 iExtType = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		uint16 iExtDataLen = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		
		if ( iPos + iExtDataLen > iExtEnd ) break;
		
		if ( iExtType == 0x0000 ) {  // server_name (SNI)
			// server_name_list_length(2) + server_name_type(1) + host_name_length(2) + host_name
			if ( iExtDataLen >= 5 ) {
				size_t iSNIOff = iPos;
				uint16 iNameListLen = __xrt_tls_load_be16(pMsg + iSNIOff);
				iSNIOff += 2;
				(void)iNameListLen;
				
				uint8 iNameType = pMsg[iSNIOff++];
				if ( iNameType == 0 ) {  // host_name
					uint16 iHostLen = __xrt_tls_load_be16(pMsg + iSNIOff);
					iSNIOff += 2;
					
					if ( iHostLen > 0 && iHostLen < sizeof(pCtx->sClientSNI) && iSNIOff + iHostLen <= iExtEnd ) {
						memcpy(pCtx->sClientSNI, pMsg + iSNIOff, iHostLen);
						pCtx->sClientSNI[iHostLen] = '\0';
						#ifdef DEBUG_TRACE
							printf("    [TLS] ClientHello SNI: %s\n", pCtx->sClientSNI);
						#endif
					}
				}
			}
		} else if ( iExtType == 0xff01 ) {  // renegotiation_info
			pCtx->bPeerSecureReneg = true;
		} else if ( iExtType == 0x002b ) {  // supported_versions
			size_t iOff = iPos;
			if ( iExtDataLen >= 1 ) {
				uint8 iVerLen = pMsg[iOff++];
				while ( iOff + 1 < iPos + iExtDataLen && iOff < iPos + 1 + iVerLen ) {
					if ( __xrt_tls_load_be16(pMsg + iOff) == __XRT_TLS_VERSION_1_3 ) {
						bTls13Supported = true;
					}
					iOff += 2;
				}
			}
		} else if ( iExtType == 0x000a ) {  // supported_groups
			size_t iOff = iPos;
			if ( iExtDataLen >= 2 ) {
				uint16 iGroupListLen = __xrt_tls_load_be16(pMsg + iOff);
				iOff += 2;
				while ( iOff + 1 < iPos + iExtDataLen && iOff < iPos + 2 + iGroupListLen ) {
					uint16 iGroup = __xrt_tls_load_be16(pMsg + iOff);
					if ( iGroup == 0x001d ) bGroupX25519 = true;
					else if ( iGroup == 0x001e ) bGroupX448 = true;
					else if ( iGroup == 0x0017 ) bGroupP256 = true;
					else if ( iGroup == 0x0018 ) bGroupP384 = true;
					iOff += 2;
				}
			}
		} else if ( iExtType == 0x000b ) {  // ec_point_formats
			size_t iOff = iPos;
			bSawPointFormats = true;
			if ( iExtDataLen >= 1 ) {
				uint8 iFmtLen = pMsg[iOff++];
				while ( iOff < iPos + iExtDataLen && iOff < iPos + 1 + iFmtLen ) {
					if ( pMsg[iOff] == 0 ) bPointFmtUncompressed = true;
					iOff++;
				}
			}
		} else if ( iExtType == 0x000d ) {  // signature_algorithms
			size_t iOff = iPos;
			if ( iExtDataLen >= 2 ) {
				uint16 iSigListLen = __xrt_tls_load_be16(pMsg + iOff);
				iOff += 2;
				while ( iOff + 1 < iPos + iExtDataLen && iOff < iPos + 2 + iSigListLen ) {
					uint16 iSigAlg = __xrt_tls_load_be16(pMsg + iOff);
					if ( iSigAlg == 0x0403 ) {
						pCtx->bPeerSigECDSAP256 = true;
						bPeerSigECDSA256 = true;
					} else if ( iSigAlg == 0x0503 ) {
						pCtx->bPeerSigECDSAP384 = true;
						bPeerSigECDSA384 = true;
					} else if ( iSigAlg == 0x0807 ) {
						pCtx->bPeerSigEd25519 = true;
					} else if ( iSigAlg == 0x0603 ) {
						bPeerSigECDSA512 = true;
					} else if ( iSigAlg == 0x0401 ) {
						bPeerSigRSAPKCS1256 = true;
					} else if ( iSigAlg == 0x0501 ) {
						bPeerSigRSAPKCS1384 = true;
					} else if ( iSigAlg == 0x0601 ) {
						bPeerSigRSAPKCS1512 = true;
					} else if ( iSigAlg == 0x0804 ) pCtx->bPeerSigRSAPSS256 = true;
					else if ( iSigAlg == 0x0805 ) pCtx->bPeerSigRSAPSS384 = true;
					else if ( iSigAlg == 0x0806 ) pCtx->bPeerSigRSAPSS512 = true;
					else if ( iSigAlg == 0x0809 ) pCtx->bPeerSigRSAPSSPSS256 = true;
					else if ( iSigAlg == 0x080a ) pCtx->bPeerSigRSAPSSPSS384 = true;
					else if ( iSigAlg == 0x080b ) pCtx->bPeerSigRSAPSSPSS512 = true;
					iOff += 2;
				}
			}
		} else if ( iExtType == 0x0033 ) {  // key_share
			size_t iOff = iPos;
			if ( iExtDataLen >= 2 ) {
				uint16 iShareListLen = __xrt_tls_load_be16(pMsg + iOff);
				iOff += 2;
				while ( iOff + 4 <= iPos + iExtDataLen && iOff < iPos + 2 + iShareListLen ) {
					uint16 iGroup = __xrt_tls_load_be16(pMsg + iOff);
					uint16 iKeyLen;
					iOff += 2;
					iKeyLen = __xrt_tls_load_be16(pMsg + iOff);
					iOff += 2;
					if ( iOff + iKeyLen > iPos + iExtDataLen ) break;
					if ( iGroup == 0x001d && iKeyLen == 32 ) {
						memcpy(aX25519Peer, pMsg + iOff, 32);
						bHaveX25519 = true;
					} else if ( iGroup == 0x001e && iKeyLen == 56 ) {
						memcpy(aX448Peer, pMsg + iOff, 56);
						bHaveX448 = true;
					} else if ( iGroup == 0x0017 && iKeyLen == 65 && pMsg[iOff] == 0x04 ) {
						memcpy(aP256Peer, pMsg + iOff, 65);
						bHaveP256 = true;
					} else if ( iGroup == 0x0018 && iKeyLen == 97 && pMsg[iOff] == 0x04 ) {
						memcpy(aP384Peer, pMsg + iOff, 97);
						bHaveP384 = true;
					}
					iOff += iKeyLen;
				}
			}
		}
		
		iPos += iExtDataLen;
	}

	// 根据 SNI 动态切换证书后，再选择签名算法和握手参数
	if ( pCtx->sClientSNI[0] != '\0' && pCtx->OnSNI ) {
		pCtx->OnSNI(pCtx->pSession, pCtx->sClientSNI, pCtx->pSNIUserData);
	}

	// 优先协商 TLS 1.3
	if ( bAllowTls13 && bTls13Supported ) {
		if ( bOfferTLS13AES256 ) pCtx->iCipherSuite = __XRT_TLS_AES_256_GCM_SHA384;
		else if ( bOfferTLS13ChaCha ) pCtx->iCipherSuite = __XRT_TLS_CHACHA20_POLY1305_SHA256;
		else if ( bOfferTLS13AES128 ) pCtx->iCipherSuite = __XRT_TLS_AES_128_GCM_SHA256;
		else pCtx->iCipherSuite = 0;

		if ( pCtx->iCipherSuite != 0 ) {
			if ( bHaveX25519 ) {
				pCtx->iTls13Group = 0x001d;
				pCtx->iPeerKeyShareLen = 32;
				memcpy(pCtx->aPeerKeyShare, aX25519Peer, 32);
				xrtX25519Keypair(pCtx->aX25519Priv, pCtx->aX25519Pub);
				xrtX25519SharedSecret(pCtx->aX25519Secret, pCtx->aX25519Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 32;
			} else if ( bHaveX448 ) {
				pCtx->iTls13Group = 0x001e;
				pCtx->iPeerKeyShareLen = 56;
				memcpy(pCtx->aPeerKeyShare, aX448Peer, 56);
				xrtX448Keypair(pCtx->aX448Priv, pCtx->aX448Pub);
				xrtX448SharedSecret(pCtx->aX25519Secret, pCtx->aX448Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 56;
			} else if ( bHaveP256 ) {
				pCtx->iTls13Group = 0x0017;
				pCtx->iPeerKeyShareLen = 65;
				memcpy(pCtx->aPeerKeyShare, aP256Peer, 65);
				xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
				xrtECDHSecp256r1SharedSecret(pCtx->aX25519Secret, pCtx->aP256Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 32;
			} else if ( bHaveP384 ) {
				pCtx->iTls13Group = 0x0018;
				pCtx->iPeerKeyShareLen = 97;
				memcpy(pCtx->aPeerKeyShare, aP384Peer, 97);
				xrtECDHSecp384r1Keypair(pCtx->aP384Priv, pCtx->aP384Pub);
				xrtECDHSecp384r1SharedSecret(pCtx->aX25519Secret, pCtx->aP384Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 48;
			}

			if ( pCtx->iTls13Group != 0 ) {
				if ( pCtx->bIsEd25519Key ) {
					if ( pCtx->bHasEd25519Priv && pCtx->bPeerSigEd25519 ) {
						pCtx->iServerSigAlg = 0x0807;
						return true;
					}
				} else if ( pCtx->bIsECPubKey ) {
					if ( pCtx->bHasECPrivKey && pCtx->iPubKeySz == 65 && pCtx->bPeerSigECDSAP256 ) {
						pCtx->iServerSigAlg = 0x0403;
						return true;
					}
					if ( pCtx->bHasECPrivKey && pCtx->iPubKeySz == 97 && pCtx->bPeerSigECDSAP384 ) {
						pCtx->iServerSigAlg = 0x0503;
						return true;
					}
				} else if ( pCtx->bHasRSAPrivKey ) {
					if ( pCtx->bIsRSAPSSKey ) {
						if ( pCtx->iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384 && pCtx->bPeerSigRSAPSSPSS384 ) {
							pCtx->iServerSigAlg = 0x080a;
							return true;
						}
						if ( pCtx->bPeerSigRSAPSSPSS256 ) {
							pCtx->iServerSigAlg = 0x0809;
							return true;
						}
						if ( pCtx->bPeerSigRSAPSSPSS384 ) {
							pCtx->iServerSigAlg = 0x080a;
							return true;
						}
						if ( pCtx->bPeerSigRSAPSSPSS512 ) {
							pCtx->iServerSigAlg = 0x080b;
							return true;
						}
					} else {
						if ( pCtx->iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384 && pCtx->bPeerSigRSAPSS384 ) {
							pCtx->iServerSigAlg = 0x0805;
							return true;
						}
						if ( pCtx->bPeerSigRSAPSS256 ) {
							pCtx->iServerSigAlg = 0x0804;
							return true;
						}
						if ( pCtx->bPeerSigRSAPSS384 ) {
							pCtx->iServerSigAlg = 0x0805;
							return true;
						}
						if ( pCtx->bPeerSigRSAPSS512 ) {
							pCtx->iServerSigAlg = 0x0806;
							return true;
						}
					}
				}
			}
		}

		pCtx->iTls13Group = 0;
		pCtx->iPeerKeyShareLen = 0;
	}

	// 回退到 TLS 1.2
	if ( iLegacyVersion != __XRT_TLS_VERSION_1_2 ) return false;

	// TLS 1.2 中 Ed25519 不是默认互操作路径, 仅在显式允许时作为 ECDHE_ECDSA 认证证书使用。
	if ( pCtx->bIsEd25519Key && !pCtx->bAllowTLS12Ed25519 ) return false;

	if ( pCtx->iSessionIdLen > 0
		&& __xrt_tls_resume_cache_lookup(pCtx->aSessionId, pCtx->iSessionIdLen, &tCachedResume)
		&& __xrt_tls12_client_offers_suite(tCachedResume.iCipherSuite,
			bOfferTLS12EcdheEcdsaAES128,
			bOfferTLS12EcdheEcdsaAES256,
			bOfferTLS12EcdheEcdsaChaCha,
			bOfferTLS12EcdheRsaAES128,
			bOfferTLS12EcdheRsaAES256,
			bOfferTLS12EcdheRsaChaCha) ) {
		pCtx->iCipherSuite = tCachedResume.iCipherSuite;
		if ( !__xrt_tls12_set_cipher_params(pCtx, pCtx->iCipherSuite) ) return false;
		memcpy(pCtx->aMasterSecret, tCachedResume.aMasterSecret, sizeof(pCtx->aMasterSecret));
		pCtx->bIsTls12 = true;
		pCtx->bSessionResumed = true;
		return true;
	}

	if ( pCtx->bIsECPubKey || pCtx->bIsEd25519Key ) {
		if ( bOfferTLS12EcdheEcdsaAES256 ) pCtx->iCipherSuite = __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384;
		else if ( bOfferTLS12EcdheEcdsaChaCha ) pCtx->iCipherSuite = __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256;
		else if ( bOfferTLS12EcdheEcdsaAES128 ) pCtx->iCipherSuite = __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256;
		else return false;
	} else {
		if ( bOfferTLS12EcdheRsaAES256 ) pCtx->iCipherSuite = __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384;
		else if ( bOfferTLS12EcdheRsaChaCha ) pCtx->iCipherSuite = __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256;
		else if ( bOfferTLS12EcdheRsaAES128 ) pCtx->iCipherSuite = __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256;
		else return false;
	}

	if ( !__xrt_tls12_set_cipher_params(pCtx, pCtx->iCipherSuite) ) return false;

	if ( bGroupX25519 ) {
		pCtx->iTls12Curve = 0x001d;
		xrtX25519Keypair(pCtx->aX25519Priv, pCtx->aX25519Pub);
	} else if ( bGroupX448 ) {
		pCtx->iTls12Curve = 0x001e;
		xrtX448Keypair(pCtx->aX448Priv, pCtx->aX448Pub);
	} else if ( bGroupP256 && (!bSawPointFormats || bPointFmtUncompressed) ) {
		pCtx->iTls12Curve = 0x0017;
		xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
	} else if ( bGroupP384 && (!bSawPointFormats || bPointFmtUncompressed) ) {
		pCtx->iTls12Curve = 0x0018;
		xrtECDHSecp384r1Keypair(pCtx->aP384Priv, pCtx->aP384Pub);
	} else if ( !bGroupX25519 && !bGroupX448 && !bGroupP256 && !bGroupP384 ) {
		pCtx->iTls12Curve = 0x0017;
		xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
	} else {
		return false;
	}

	if ( pCtx->bIsEd25519Key ) {
		if ( !pCtx->bHasEd25519Priv ) return false;
	} else if ( pCtx->bIsECPubKey ) {
		if ( !pCtx->bHasECPrivKey ) return false;
	} else {
		if ( !pCtx->bHasRSAPrivKey ) return false;
	}

	if ( pCtx->bIsEd25519Key ) {
		if ( !pCtx->bAllowTLS12Ed25519 || !pCtx->bHasEd25519Priv || !pCtx->bPeerSigEd25519 ) return false;
		pCtx->iServerSigAlg = 0x0807;
	} else if ( pCtx->bIsECPubKey ) {
		if ( bPeerSigECDSA256 ) pCtx->iServerSigAlg = 0x0403;
		else if ( pCtx->bUseSHA384 && bPeerSigECDSA384 ) pCtx->iServerSigAlg = 0x0503;
		else if ( bPeerSigECDSA384 ) pCtx->iServerSigAlg = 0x0503;
		else if ( bPeerSigECDSA512 ) pCtx->iServerSigAlg = 0x0603;
		else return false;
	} else {
		if ( pCtx->bIsRSAPSSKey ) {
			if ( pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSSPSS384 ) pCtx->iServerSigAlg = 0x080a;
			else if ( !pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSSPSS256 ) pCtx->iServerSigAlg = 0x0809;
			else if ( pCtx->bPeerSigRSAPSSPSS256 ) pCtx->iServerSigAlg = 0x0809;
			else if ( pCtx->bPeerSigRSAPSSPSS384 ) pCtx->iServerSigAlg = 0x080a;
			else if ( pCtx->bPeerSigRSAPSSPSS512 ) pCtx->iServerSigAlg = 0x080b;
			else return false;
		} else {
			if ( pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSS384 ) pCtx->iServerSigAlg = 0x0805;
			else if ( !pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSS256 ) pCtx->iServerSigAlg = 0x0804;
			else if ( pCtx->bUseSHA384 && bPeerSigRSAPKCS1384 ) pCtx->iServerSigAlg = 0x0501;
			else if ( !pCtx->bUseSHA384 && bPeerSigRSAPKCS1256 ) pCtx->iServerSigAlg = 0x0401;
			else if ( pCtx->bPeerSigRSAPSS256 ) pCtx->iServerSigAlg = 0x0804;
			else if ( pCtx->bPeerSigRSAPSS384 ) pCtx->iServerSigAlg = 0x0805;
			else if ( pCtx->bPeerSigRSAPSS512 ) pCtx->iServerSigAlg = 0x0806;
			else if ( bPeerSigRSAPKCS1256 ) pCtx->iServerSigAlg = 0x0401;
			else if ( bPeerSigRSAPKCS1384 ) pCtx->iServerSigAlg = 0x0501;
			else if ( bPeerSigRSAPKCS1512 ) pCtx->iServerSigAlg = 0x0601;
			else return false;
		}
	}

	return true;
}

static bool __xrt_tls_sign_server_hash(xtlsctx *pCtx, const uint8 *pHash, size_t iHashLen,
	uint8 *pSig, size_t *pSigLen)
{
	if ( !pCtx || !pHash || !pSig || !pSigLen ) return false;
	if ( pCtx->bIsEd25519Key ) {
		if ( !pCtx->bHasEd25519Priv || pCtx->iServerSigAlg != 0x0807 ) return false;
		*pSigLen = 64;
		return xrtEd25519Sign(pSig, pHash, iHashLen, pCtx->aEd25519Key);
	}
	if ( pCtx->bIsECPubKey ) {
		if ( !pCtx->bHasECPrivKey ) return false;
		if ( pCtx->iPubKeySz == 65 || pCtx->iECKeyLen == 32 ) {
			if ( pCtx->iServerSigAlg != 0x0403 && pCtx->iServerSigAlg != 0x0503 && pCtx->iServerSigAlg != 0x0603 ) return false;
			return __xrt_ecdsa_sign_p256(pHash, iHashLen, pCtx->aECKey, pSig, pSigLen);
		}
		if ( pCtx->iPubKeySz == 97 || pCtx->iECKeyLen == 48 ) {
			if ( pCtx->iServerSigAlg != 0x0403 && pCtx->iServerSigAlg != 0x0503 && pCtx->iServerSigAlg != 0x0603 ) return false;
			return __xrt_ecdsa_sign_p384(pHash, iHashLen, pCtx->aECKey, pSig, pSigLen);
		}
		return false;
	}
	if ( !pCtx->bHasRSAPrivKey || pCtx->iPubKeyModSz == 0 ) return false;

	*pSigLen = pCtx->iPubKeyModSz;
	if ( pCtx->iServerSigAlg == 0x0804 || pCtx->iServerSigAlg == 0x0805 || pCtx->iServerSigAlg == 0x0806 ||
		pCtx->iServerSigAlg == 0x0809 || pCtx->iServerSigAlg == 0x080a || pCtx->iServerSigAlg == 0x080b ) {
		return __xrt_rsa_pss_sign(pHash, iHashLen, pCtx->aPubKey, pCtx->iPubKeyModSz,
			pCtx->aRSAPrivExp, pCtx->iRSAPrivExpSz, pSig, *pSigLen);
	}
	if ( pCtx->iServerSigAlg == 0x0401 || pCtx->iServerSigAlg == 0x0501 || pCtx->iServerSigAlg == 0x0601 ) {
		return __xrt_rsa_pkcs1_sign(pHash, iHashLen, pCtx->aPubKey, pCtx->iPubKeyModSz,
			pCtx->aRSAPrivExp, pCtx->iRSAPrivExpSz, pSig, *pSigLen);
	}
	return false;
}

static bool __xrt_tls13_send_server_hello(xtlsctx *pCtx)
{
	uint8 aBuf[256];
	uint8 aRec[5];
	size_t iPos = 0;
	size_t iLenPos;
	size_t iKeyLen;

	if ( !pCtx ) return false;
	if ( pCtx->iTls13Group == 0x001d ) iKeyLen = 32;
	else if ( pCtx->iTls13Group == 0x001e ) iKeyLen = 56;
	else if ( pCtx->iTls13Group == 0x0017 ) iKeyLen = 65;
	else if ( pCtx->iTls13Group == 0x0018 ) iKeyLen = 97;
	else return false;

	xrtRandomBytes(pCtx->aServerRandom, 32);
	aBuf[iPos++] = __XRT_TLS_SERVER_HELLO;
	iLenPos = iPos;
	iPos += 3;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	memcpy(aBuf + iPos, pCtx->aServerRandom, 32);
	iPos += 32;
	aBuf[iPos++] = pCtx->iSessionIdLen;
	memcpy(aBuf + iPos, pCtx->aSessionId, pCtx->iSessionIdLen);
	iPos += pCtx->iSessionIdLen;
	__xrt_tls_store_be16(aBuf + iPos, pCtx->iCipherSuite);
	iPos += 2;
	aBuf[iPos++] = 0;

	{
		size_t iExtPos = iPos;
		iPos += 2;

		__xrt_tls_store_be16(aBuf + iPos, 0x002b);
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, 2);
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_3);
		iPos += 2;

		__xrt_tls_store_be16(aBuf + iPos, 0x0033);
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, (uint16)(4 + iKeyLen));
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, pCtx->iTls13Group);
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, (uint16)iKeyLen);
		iPos += 2;
		if ( pCtx->iTls13Group == 0x001d ) {
			memcpy(aBuf + iPos, pCtx->aX25519Pub, 32);
		} else if ( pCtx->iTls13Group == 0x001e ) {
			memcpy(aBuf + iPos, pCtx->aX448Pub, 56);
		} else if ( pCtx->iTls13Group == 0x0017 ) {
			memcpy(aBuf + iPos, pCtx->aP256Pub, 65);
		} else {
			memcpy(aBuf + iPos, pCtx->aP384Pub, 97);
		}
		iPos += iKeyLen;

		__xrt_tls_store_be16(aBuf + iExtPos, (uint16)(iPos - iExtPos - 2));
	}

	__xrt_tls_store_be24(aBuf + iLenPos, (uint32)(iPos - iLenPos - 3));
	__xrt_tls13_hash_update(pCtx, aBuf, iPos);
	__xrt_tls_store_be16(aRec + 1, __XRT_TLS_VERSION_1_2);
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 3, (uint16)iPos);
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aRec, 5);
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aBuf, iPos);
	return true;
}

static bool __xrt_tls13_send_encrypted_extensions(xtlsctx *pCtx)
{
	uint8 aMsg[6];
	if ( !pCtx ) return false;
	aMsg[0] = __XRT_TLS_ENCRYPTED_EXTENSIONS;
	__xrt_tls_store_be24(aMsg + 1, 2);
	__xrt_tls_store_be16(aMsg + 4, 0);
	__xrt_tls13_hash_update(pCtx, aMsg, sizeof(aMsg));
	__xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, aMsg, sizeof(aMsg), true);
	return true;
}

static bool __xrt_tls13_send_certificate(xtlsctx *pCtx)
{
	size_t iBodyLen;
	size_t iMsgLen;
	uint8 *pMsg;

	if ( !pCtx || !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) return false;
	iBodyLen = 1 + 3 + 3 + pCtx->iCertDerLen + 2;
	iMsgLen = __XRT_TLS_MSGHDR_SIZE + iBodyLen;
	pMsg = (uint8*)xrtMalloc(iMsgLen);
	if ( !pMsg ) return false;

	pMsg[0] = __XRT_TLS_CERTIFICATE;
	__xrt_tls_store_be24(pMsg + 1, (uint32)iBodyLen);
	pMsg[4] = 0;
	__xrt_tls_store_be24(pMsg + 5, (uint32)(3 + pCtx->iCertDerLen + 2));
	__xrt_tls_store_be24(pMsg + 8, (uint32)pCtx->iCertDerLen);
	memcpy(pMsg + 11, pCtx->pCertDer, pCtx->iCertDerLen);
	__xrt_tls_store_be16(pMsg + 11 + pCtx->iCertDerLen, 0);

	__xrt_tls13_hash_update(pCtx, pMsg, iMsgLen);
	__xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, pMsg, iMsgLen, true);
	xrtFree(pMsg);
	return true;
}

static bool __xrt_tls13_send_certificate_verify(xtlsctx *pCtx)
{
	uint8 aTranscriptHash[64];
	size_t iTranscriptHashLen = 0;
	uint8 aContentHash[192];
	size_t iContentHashLen = 0;
	uint8 aSig[512];
	size_t iSigLen = 0;
	size_t iMsgLen;
	uint8 *pMsg;

	if ( !pCtx || pCtx->iServerSigAlg == 0 ) return false;
	__xrt_tls13_get_transcript_hash(pCtx, aTranscriptHash, &iTranscriptHashLen);
	if ( !__xrt_tls13_build_cert_verify_input(aContentHash, &iContentHashLen,
		aTranscriptHash, iTranscriptHashLen, pCtx->iServerSigAlg, true) ) {
		return false;
	}
	if ( !__xrt_tls_sign_server_hash(pCtx, aContentHash, iContentHashLen, aSig, &iSigLen) ) return false;

	iMsgLen = __XRT_TLS_MSGHDR_SIZE + 2 + 2 + iSigLen;
	pMsg = (uint8*)xrtMalloc(iMsgLen);
	if ( !pMsg ) return false;
	pMsg[0] = __XRT_TLS_CERTIFICATE_VERIFY;
	__xrt_tls_store_be24(pMsg + 1, (uint32)(4 + iSigLen));
	__xrt_tls_store_be16(pMsg + 4, pCtx->iServerSigAlg);
	__xrt_tls_store_be16(pMsg + 6, (uint16)iSigLen);
	memcpy(pMsg + 8, aSig, iSigLen);

	__xrt_tls13_hash_update(pCtx, pMsg, iMsgLen);
	__xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, pMsg, iMsgLen, true);
	xrtFree(pMsg);
	return true;
}

static bool __xrt_tls13_send_server_flight(xtlsctx *pCtx)
{
	if ( !__xrt_tls13_send_encrypted_extensions(pCtx) ) return false;
	if ( !__xrt_tls13_send_certificate(pCtx) ) return false;
	if ( !__xrt_tls13_send_certificate_verify(pCtx) ) return false;
	__xrt_tls_send_finished(pCtx, true);
	__xrt_tls_derive_application_keys(pCtx);
	return true;
}

static bool __xrt_tls_parse_rsa_private_key(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tSeq, tField;

	if ( !pCtx || !pDer ) return false;
	if ( __xrt_der_parse(pDer, iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) return false;

	// version
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) return false;
	// modulus
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) return false;
	// publicExponent
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) return false;
	// privateExponent
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) return false;

	pCtx->iRSAPrivExpSz = tField.iLen;
	if ( pCtx->iRSAPrivExpSz > 0 && tField.pValue[0] == 0x00 ) pCtx->iRSAPrivExpSz--;
	if ( pCtx->iRSAPrivExpSz == 0 || pCtx->iRSAPrivExpSz > sizeof(pCtx->aRSAPrivExp) ) return false;

	if ( tField.pValue[0] == 0x00 ) {
		memcpy(pCtx->aRSAPrivExp, tField.pValue + 1, pCtx->iRSAPrivExpSz);
	} else {
		memcpy(pCtx->aRSAPrivExp, tField.pValue, pCtx->iRSAPrivExpSz);
	}
	pCtx->bHasRSAPrivKey = true;
	return true;
}

static bool __xrt_tls_parse_ec_private_key(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tSeq, tField;

	if ( !pCtx || !pDer ) return false;
	if ( __xrt_der_parse(pDer, iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) return false;
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x04 ) return false;
	if ( tField.iLen != 32 && tField.iLen != 48 ) return false;
	memcpy(pCtx->aECKey, tField.pValue, tField.iLen);
	pCtx->iECKeyLen = tField.iLen;
	pCtx->bHasECPrivKey = true;
	return true;
}

static bool __xrt_tls_parse_ed25519_private_key(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tKey, tSeed;

	if ( !pCtx || !pDer ) return false;
	if ( iLen == 32 ) {
		memcpy(pCtx->aEd25519Key, pDer, 32);
		pCtx->bHasEd25519Priv = true;
		return true;
	}
	if ( __xrt_der_parse(pDer, iLen, &tKey) < 0 ) return false;
	if ( tKey.iType == 0x04 && tKey.iLen == 32 ) {
		memcpy(pCtx->aEd25519Key, tKey.pValue, 32);
		pCtx->bHasEd25519Priv = true;
		return true;
	}
	if ( tKey.iType != 0x04 ) return false;
	if ( __xrt_der_parse(tKey.pValue, tKey.iLen, &tSeed) < 0 || tSeed.iType != 0x04 || tSeed.iLen != 32 ) return false;
	memcpy(pCtx->aEd25519Key, tSeed.pValue, 32);
	pCtx->bHasEd25519Priv = true;
	return true;
}

static bool __xrt_tls_parse_private_key_info(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tSeq, tVersion, tAlg, tPriv, tOID;

	if ( !pCtx || !pDer ) return false;
	if ( __xrt_der_parse(pDer, iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tSeq, &tVersion) <= 0 || tVersion.iType != 0x02 ) return false;
	if ( __xrt_der_next(&tSeq, &tAlg) <= 0 || tAlg.iType != 0x30 ) return false;
	if ( __xrt_der_next(&tSeq, &tPriv) <= 0 || tPriv.iType != 0x04 ) return false;

	{
		struct __xrt_der_tlv tAlgItems = tAlg;
		if ( __xrt_der_next(&tAlgItems, &tOID) <= 0 || tOID.iType != 0x06 ) return false;
		if ( (tOID.iLen == sizeof(__xrt_oid_rsa_encryption) &&
			memcmp(tOID.pValue, __xrt_oid_rsa_encryption, sizeof(__xrt_oid_rsa_encryption)) == 0) ||
			(tOID.iLen == sizeof(__xrt_oid_rsassa_pss) &&
			memcmp(tOID.pValue, __xrt_oid_rsassa_pss, sizeof(__xrt_oid_rsassa_pss)) == 0) ) {
			return __xrt_tls_parse_rsa_private_key(pCtx, tPriv.pValue, tPriv.iLen);
		}
		if ( tOID.iLen == sizeof(__xrt_oid_ec_public_key) &&
			memcmp(tOID.pValue, __xrt_oid_ec_public_key, sizeof(__xrt_oid_ec_public_key)) == 0 ) {
			return __xrt_tls_parse_ec_private_key(pCtx, tPriv.pValue, tPriv.iLen);
		}
		if ( tOID.iLen == sizeof(__xrt_oid_ed25519) &&
			memcmp(tOID.pValue, __xrt_oid_ed25519, sizeof(__xrt_oid_ed25519)) == 0 ) {
			return __xrt_tls_parse_ed25519_private_key(pCtx, tPriv.pValue, tPriv.iLen);
		}
	}
	return false;
}

static bool __xrt_tls_prepare_server_identity(xtlsctx *pCtx)
{
	struct __xrt_x509_cert tCert;

	if ( !pCtx ) return false;
	pCtx->bHasECPrivKey = false;
	pCtx->bHasEd25519Priv = false;
	pCtx->bHasRSAPrivKey = false;
	pCtx->bIsEd25519Key = false;
	pCtx->bIsRSAPSSKey = false;
	pCtx->iRSAPrivExpSz = 0;
	pCtx->iECKeyLen = 0;
	memset(pCtx->aECKey, 0, sizeof(pCtx->aECKey));
	memset(pCtx->aEd25519Key, 0, sizeof(pCtx->aEd25519Key));
	memset(pCtx->aRSAPrivExp, 0, sizeof(pCtx->aRSAPrivExp));

	if ( !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) return true;
	if ( !__xrt_x509_parse(pCtx->pCertDer, pCtx->iCertDerLen, &tCert) ) return false;
	if ( !__xrt_tls_copy_pubkey_from_cert(pCtx, &tCert) ) return false;

	if ( !pCtx->pKeyDer || pCtx->iKeyDerLen == 0 ) return true;
	if ( __xrt_tls_parse_private_key_info(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		if ( pCtx->bIsECPubKey && pCtx->bHasECPrivKey ) {
			if ( (pCtx->iPubKeySz == 65 && pCtx->iECKeyLen != 32)
				|| (pCtx->iPubKeySz == 97 && pCtx->iECKeyLen != 48) ) {
				return false;
			}
		}
		if ( pCtx->bIsEd25519Key ) return pCtx->bHasEd25519Priv;
		return pCtx->bIsECPubKey ? pCtx->bHasECPrivKey : pCtx->bHasRSAPrivKey;
	}
	if ( __xrt_tls_parse_rsa_private_key(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		return !pCtx->bIsECPubKey && !pCtx->bIsEd25519Key;
	}
	if ( __xrt_tls_parse_ec_private_key(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		if ( pCtx->bIsECPubKey ) {
			if ( (pCtx->iPubKeySz == 65 && pCtx->iECKeyLen != 32)
				|| (pCtx->iPubKeySz == 97 && pCtx->iECKeyLen != 48) ) {
				return false;
			}
		}
		return pCtx->bIsECPubKey;
	}
	if ( __xrt_tls_parse_ed25519_private_key(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		return pCtx->bIsEd25519Key;
	}
	return false;
}

// Step 13: SNI 相关公共 API

XXAPI const char* xrtTlsGetSNI(xtlsctx *pCtx)
{
	if ( !pCtx ) return NULL;
	if ( pCtx->sClientSNI[0] == '\0' ) return NULL;
	return pCtx->sClientSNI;
}

XXAPI void xrtTlsSetAllowTLS12Ed25519(xtlsctx *pCtx, bool bAllow)
{
	if ( !pCtx ) return;
	pCtx->bAllowTLS12Ed25519 = bAllow;
}

XXAPI xnet_result xrtTlsSetCert(xtlsctx *pCtx, const char *sCertFile, const char *sKeyFile)
{
	uint8 *pCertDer = NULL;
	size_t iCertDerLen = 0;
	uint8 *pKeyDer = NULL;
	size_t iKeyDerLen = 0;

	if ( !pCtx ) return XRT_NET_ERROR;

	if ( pCtx->pCertDer ) {
		xrtFree(pCtx->pCertDer);
		pCtx->pCertDer = NULL;
		pCtx->iCertDerLen = 0;
	}
	if ( pCtx->pKeyDer ) {
		xrtFree(pCtx->pKeyDer);
		pCtx->pKeyDer = NULL;
		pCtx->iKeyDerLen = 0;
	}

	if ( sCertFile && sCertFile[0] ) {
		if ( !__xrt_tls_load_der_file(sCertFile, &pCertDer, &iCertDerLen) ) {
			return XRT_NET_ERROR;
		}
	}

	if ( sKeyFile && sKeyFile[0] ) {
		if ( !__xrt_tls_load_der_file(sKeyFile, &pKeyDer, &iKeyDerLen) ) {
			if ( pCertDer ) xrtFree(pCertDer);
			return XRT_NET_ERROR;
		}
	}

	pCtx->pCertDer = pCertDer;
	pCtx->iCertDerLen = iCertDerLen;
	pCtx->pKeyDer = pKeyDer;
	pCtx->iKeyDerLen = iKeyDerLen;

	if ( !__xrt_tls_prepare_server_identity(pCtx) ) {
		if ( pCtx->pCertDer ) {
			xrtFree(pCtx->pCertDer);
			pCtx->pCertDer = NULL;
			pCtx->iCertDerLen = 0;
		}
		if ( pCtx->pKeyDer ) {
			xrtFree(pCtx->pKeyDer);
			pCtx->pKeyDer = NULL;
			pCtx->iKeyDerLen = 0;
		}
		return XRT_NET_ERROR;
	}

	return XRT_NET_OK;
}



struct xrt_tls_session {
	xtlsctx* pCtx;
	bool bIsServer;
};

static xtlsctx* __xrtNetTlsSessionCtx(const xtlssession* pSession)
{
	return pSession ? pSession->pCtx : NULL;
}

XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer)
{
	xtlssession* pSession;

	(void)xrtInit();
	pSession = (xtlssession*)xrtCalloc(1, sizeof(xtlssession));
	if ( !pSession ) {
		return NULL;
	}
	pSession->pCtx = xrtTlsCreate(pCfg, bIsServer);
	if ( !pSession->pCtx ) {
		xrtFree(pSession);
		return NULL;
	}
	pSession->pCtx->pSession = pSession;
	pSession->bIsServer = bIsServer;
	return pSession;
}

XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession)
{
	if ( !pSession ) {
		return;
	}
	if ( pSession->pCtx ) {
		pSession->pCtx->pSession = NULL;
		xrtTlsDestroy(pSession->pCtx);
		pSession->pCtx = NULL;
	}
	xrtFree(pSession);
}

XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsIsReady(pCtx) : false;
}

XXAPI xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsDrive(pCtx) : XRT_NET_ERROR;
}

XXAPI xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pData || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsFeed(pCtx, (const char*)pData, iLen);
}

XXAPI size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsPendingSend(pCtx) : 0;
}

XXAPI size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsPendingRecv(pCtx) : 0;
}

XXAPI xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pBuf || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsPeekSend(pCtx, (char*)pBuf, iLen, pRead);
}

XXAPI void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || iLen == 0 ) {
		return;
	}
	xrtTlsConsumeSend(pCtx, iLen);
}

XXAPI xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pData || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsWrite(pCtx, (const char*)pData, iLen, pWritten);
}

XXAPI xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pBuf || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsRead(pCtx, (char*)pBuf, iLen, pRead);
}

XXAPI xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsClose(pCtx) : XRT_NET_ERROR;
}

XXAPI xtlsresume* xrtNetTlsSessionExportResume(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsExportResume(pCtx) : NULL;
}

XXAPI void xrtNetTlsResumeDestroy(xtlsresume* pResume)
{
	xrtTlsResumeDestroy(pResume);
}

XXAPI bool xrtNetTlsSessionWasResumed(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsWasResumed(pCtx) : false;
}

XXAPI const char* xrtNetTlsSessionGetSNI(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsGetSNI(pCtx) : NULL;
}

XXAPI xnet_result xrtNetTlsSessionSetCert(xtlssession* pSession, const char* sCertFile, const char* sKeyFile)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsSetCert(pCtx, sCertFile, sKeyFile) : XRT_NET_ERROR;
}

XXAPI void xrtNetTlsSessionSetAllowTLS12Ed25519(xtlssession* pSession, bool bAllow)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx ) {
		return;
	}
	xrtTlsSetAllowTLS12Ed25519(pCtx, bAllow);
}

#ifdef DEBUG_TRACE
// Debug: P-256 arithmetic self-tests
XXAPI void xrtP256ArithTest(void)
{
	printf("  --- P-256 Arithmetic Self-Tests ---\n");
	uint32 r[8], t1[8], t2[8];
	uint32 one[8] = {1,0,0,0,0,0,0,0};
	uint32 pm1[8], pm2[8];
	__xrt_u256_copy(pm1, __xrt_p256_P);
	pm1[0] -= 1;  // p-1
	__xrt_u256_copy(pm2, __xrt_p256_P);
	pm2[0] -= 2;  // p-2
	
	// Test 1: (p-1)^2 mod p should == 1
	__xrt_p256_mul_mod_p(r, pm1, pm1);
	printf("  (p-1)^2 mod p == 1: %s\n", memcmp(r, one, 32) == 0 ? "PASS" : "FAIL");
	if (memcmp(r, one, 32) != 0) {
		uint8 aR[32]; __xrt_u256_to_be(aR, r);
		printf("    got: "); for(int i=0;i<32;i++) printf("%02x",aR[i]); printf("\n");
	}
	
	// Test 2: (p-1) * 2 mod p should == p-2
	uint32 two[8] = {2,0,0,0,0,0,0,0};
	__xrt_p256_mul_mod_p(r, pm1, two);
	printf("  (p-1)*2 mod p == p-2: %s\n", memcmp(r, pm2, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 3: sqr_mod_p(p-1) should == 1
	__xrt_p256_sqr_mod_p(r, pm1);
	printf("  sqr(p-1) mod p == 1: %s\n", memcmp(r, one, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 4: add_mod_p(p-1, 1) should == 0
	uint32 zero[8] = {0};
	__xrt_p256_add_mod_p(r, pm1, one);
	printf("  (p-1)+1 mod p == 0: %s\n", memcmp(r, zero, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 5: sub_mod_p(0, 1) should == p-1
	__xrt_p256_sub_mod_p(r, zero, one);
	printf("  0-1 mod p == p-1: %s\n", memcmp(r, pm1, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 6: inv(2) * 2 mod p should == 1
	uint32 inv2[8];
	__xrt_p256_inv_mod_p(inv2, two);
	__xrt_p256_mul_mod_p(r, inv2, two);
	printf("  inv(2)*2 mod p == 1: %s\n", memcmp(r, one, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 7: mul(Gx, Gx) == sqr(Gx)
	__xrt_p256_mul_mod_p(t1, __xrt_p256_Gx, __xrt_p256_Gx);
	__xrt_p256_sqr_mod_p(t2, __xrt_p256_Gx);
	printf("  mul(Gx,Gx)==sqr(Gx): %s\n", memcmp(t1, t2, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 8: verify (a+b)^2 = a^2 + 2ab + b^2 for Gx, Gy
	uint32 apb[8], apb2[8], a2[8], b2[8], ab[8], t[8];
	__xrt_p256_add_mod_p(apb, __xrt_p256_Gx, __xrt_p256_Gy);
	__xrt_p256_sqr_mod_p(apb2, apb);
	__xrt_p256_sqr_mod_p(a2, __xrt_p256_Gx);
	__xrt_p256_sqr_mod_p(b2, __xrt_p256_Gy);
	__xrt_p256_mul_mod_p(ab, __xrt_p256_Gx, __xrt_p256_Gy);
	__xrt_p256_add_mod_p(t, a2, b2);
	__xrt_p256_add_mod_p(r, ab, ab);
	__xrt_p256_add_mod_p(r, r, t);
	printf("  (Gx+Gy)^2 == Gx^2+2*Gx*Gy+Gy^2: %s\n", memcmp(apb2, r, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 9: Compute Gy^2 and Gx^3-3*Gx+b, print raw values  
	static const uint32 p256_b[8] = {
		0x27D2604B, 0x3BCE3C3E, 0xCC53B0F6, 0x651D06B0,
		0x769886BC, 0xB3EBBD55, 0xAA3A93E7, 0x5AC635D8
	};
	__xrt_p256_sqr_mod_p(t1, __xrt_p256_Gy); // Gy^2
	__xrt_p256_sqr_mod_p(t2, __xrt_p256_Gx); // Gx^2
	__xrt_p256_mul_mod_p(t2, t2, __xrt_p256_Gx); // Gx^3
	uint32 three_gx[8];
	__xrt_p256_add_mod_p(three_gx, __xrt_p256_Gx, __xrt_p256_Gx);
	__xrt_p256_add_mod_p(three_gx, three_gx, __xrt_p256_Gx);
	__xrt_p256_sub_mod_p(r, t2, three_gx);
	__xrt_p256_add_mod_p(r, r, p256_b);
	printf("  Gy^2 == Gx^3-3Gx+b: %s\n", memcmp(t1, r, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 10: Use 256x256 multiply to verify small muls
	// 3 * 5 = 15
	uint32 three[8] = {3,0,0,0,0,0,0,0};
	uint32 five[8] = {5,0,0,0,0,0,0,0};
	uint32 fifteen[8] = {15,0,0,0,0,0,0,0};
	__xrt_p256_mul_mod_p(r, three, five);
	printf("  3*5 mod p == 15: %s\n", memcmp(r, fifteen, 32) == 0 ? "PASS" : "FAIL");
	
	// Test 11: Large multiply: C * C mod p (where C = 2^256 - p)
	// C^2 mod p
	uint32 c_sq[8];
	__xrt_p256_mul_mod_p(c_sq, __xrt_p256_C, __xrt_p256_C);
	// Since C = 2^256 mod p, C^2 = 2^512 mod p = (2^256)^2 mod p = C^2 mod p
	// But we can verify: C * (C+1) = C^2 + C vs (C+1)^2 - (2*C+1)
	uint32 cp1[8];
	__xrt_p256_add_mod_p(cp1, __xrt_p256_C, one);
	__xrt_p256_mul_mod_p(t1, __xrt_p256_C, cp1); // C*(C+1)
	__xrt_p256_add_mod_p(t2, c_sq, __xrt_p256_C); // C^2 + C
	printf("  C*(C+1) == C^2+C: %s\n", memcmp(t1, t2, 32) == 0 ? "PASS" : "FAIL");
}

// Debug: P-256 ECDH diagnostic test
XXAPI void xrtP256DebugTest(const uint8 *pPriv, const uint8 *pPub65, const uint8 *pExpected32)
{
	uint32 k[8], px[8], py[8];
	__xrt_u256_from_be(k, pPriv);
	__xrt_u256_from_be(px, pPub65 + 1);
	__xrt_u256_from_be(py, pPub65 + 33);
	
	// Verify point is on curve: y^2 == x^3 - 3x + b (mod p)
	uint32 lhs[8], rhs[8], t1[8], t2[8];
	static const uint32 p256_b[8] = {
		0x27D2604B, 0x3BCE3C3E, 0xCC53B0F6, 0x651D06B0,
		0x769886BC, 0xB3EBBD55, 0xAA3A93E7, 0x5AC635D8
	};
	
	// First verify G is on curve
	__xrt_p256_sqr_mod_p(lhs, __xrt_p256_Gy);
	__xrt_p256_sqr_mod_p(t1, __xrt_p256_Gx);
	__xrt_p256_mul_mod_p(t1, t1, __xrt_p256_Gx);
	__xrt_p256_add_mod_p(t2, __xrt_p256_Gx, __xrt_p256_Gx);
	__xrt_p256_add_mod_p(t2, t2, __xrt_p256_Gx);
	__xrt_p256_sub_mod_p(rhs, t1, t2);
	__xrt_p256_add_mod_p(rhs, rhs, p256_b);
	printf("  G on curve: %s\n", memcmp(lhs, rhs, 32) == 0 ? "PASS" : "FAIL");
	if (memcmp(lhs, rhs, 32) != 0) {
		uint8 aL[32], aR[32];
		__xrt_u256_to_be(aL, lhs); __xrt_u256_to_be(aR, rhs);
		printf("    Gy^2:         "); for(int i=0;i<32;i++) printf("%02x",aL[i]); printf("\n");
		printf("    Gx^3-3Gx+b:  "); for(int i=0;i<32;i++) printf("%02x",aR[i]); printf("\n");
	}
	
	// Now check input point
	__xrt_p256_sqr_mod_p(lhs, py);
	__xrt_p256_sqr_mod_p(t1, px);
	__xrt_p256_mul_mod_p(t1, t1, px);
	__xrt_p256_add_mod_p(t2, px, px);
	__xrt_p256_add_mod_p(t2, t2, px);
	__xrt_p256_sub_mod_p(rhs, t1, t2);
	__xrt_p256_add_mod_p(rhs, rhs, p256_b);
	printf("  Point on curve: %s\n", memcmp(lhs, rhs, 32) == 0 ? "PASS" : "FAIL");
	if (memcmp(lhs, rhs, 32) != 0) {
		uint8 aL[32], aR[32];
		__xrt_u256_to_be(aL, lhs); __xrt_u256_to_be(aR, rhs);
		printf("    y^2:         "); for(int i=0;i<32;i++) printf("%02x",aL[i]); printf("\n");
		printf("    x^3-3x+b:    "); for(int i=0;i<32;i++) printf("%02x",aR[i]); printf("\n");
	}
	
	// Compute k*G and verify against provided point
	// This comparison only makes sense when the input point IS k*G (i.e., verifying public key generation)
	// For ECDH (k*P where P != G), this comparison is expected to fail and is skipped
	bool bInputIsG = (memcmp(px, __xrt_p256_Gx, 32) == 0) && (memcmp(py, __xrt_p256_Gy, 32) == 0);
	if ( bInputIsG ) {
		// Input point is G, so we expect k*G = input point only if this is a degenerate case
		printf("  (Input point is G - skipping k*G comparison)\n");
	} else {
		// Compute k*G to verify if the input point is the public key for k
		__xrt_p256_jpt tIG;
		__xrt_p256_scalar_mult(&tIG, k, __xrt_p256_Gx, __xrt_p256_Gy);
		uint32 ig_ax[8], ig_ay[8];
		__xrt_p256_to_affine(ig_ax, ig_ay, &tIG);
		bool bXMatch = memcmp(ig_ax, px, 32) == 0;
		bool bYMatch = memcmp(ig_ay, py, 32) == 0;
		printf("  k*G.x matches input pub.x: %s\n", bXMatch ? "PASS" : "(N/A for ECDH)");
		printf("  k*G.y matches input pub.y: %s\n", bYMatch ? "PASS" : "(N/A for ECDH)");
		if ( !bYMatch ) {
			// Only show details if there's a mismatch (for debugging public key generation)
			printf("    (k*G != input point is expected for ECDH where input is peer's public key)\n");
		}
	}
	
	// Scalar mult
	__xrt_p256_jpt tR;
	__xrt_p256_scalar_mult(&tR, k, px, py);
	printf("  Result Z non-zero: %s\n", !__xrt_u256_is_zero(tR.z) ? "PASS" : "FAIL");
	
	// Verify inverse
	uint32 zinv[8], zinv2[8], zinv3[8], check[8];
	__xrt_p256_inv_mod_p(zinv, tR.z);
	__xrt_p256_mul_mod_p(check, tR.z, zinv);
	uint32 one[8] = {1,0,0,0,0,0,0,0};
	printf("  Z * Z^(-1) == 1: %s\n", memcmp(check, one, 32) == 0 ? "PASS" : "FAIL");
	
	// Affine conversion
	uint32 ax[8], ay[8];
	__xrt_p256_sqr_mod_p(zinv2, zinv);
	__xrt_p256_mul_mod_p(zinv3, zinv2, zinv);
	__xrt_p256_mul_mod_p(ax, tR.x, zinv2);
	__xrt_p256_mul_mod_p(ay, tR.y, zinv3);
	
	// Result on curve check
	__xrt_p256_sqr_mod_p(lhs, ay);
	__xrt_p256_sqr_mod_p(t1, ax);
	__xrt_p256_mul_mod_p(t1, t1, ax);
	__xrt_p256_add_mod_p(t2, ax, ax);
	__xrt_p256_add_mod_p(t2, t2, ax);
	__xrt_p256_sub_mod_p(rhs, t1, t2);
	__xrt_p256_add_mod_p(rhs, rhs, p256_b);
	printf("  Result on curve: %s\n", memcmp(lhs, rhs, 32) == 0 ? "PASS" : "FAIL");
	
	uint8 aRes[32];
	__xrt_u256_to_be(aRes, ax);
	printf("  Result X: "); for (int i=0;i<32;i++) printf("%02x",aRes[i]); printf("\n");
	if ( pExpected32 ) {
		printf("  Expect X: "); for (int i=0;i<32;i++) printf("%02x",pExpected32[i]); printf("\n");
		printf("  Match: %s\n", memcmp(aRes, pExpected32, 32) == 0 ? "PASS" : "FAIL");
	}
}
#endif
