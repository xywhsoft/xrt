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
XXAPI xnet_result xrtTlsSetCertData(xtlsctx *pCtx, const void *pCertData, size_t iCertLen, const void *pKeyData, size_t iKeyLen);

typedef struct {
	char* pBase;
	char* pData;
	size_t iSize;
	size_t iCapacity;
} __xrt_tls_buf;


// 内部函数：初始化 TLS 缓冲区
static void __xrt_tls_secure_zero(void* pData, size_t iLen);
static bool __xrt_tls_buf_init(__xrt_tls_buf* pBuf, size_t iCapacity)
{
	if ( !pBuf || iCapacity == 0 ) { return false; }
	pBuf->pBase = (char*)xrtMalloc(iCapacity);
	if ( !pBuf->pBase ) { return false; }
	pBuf->pData = pBuf->pBase;
	pBuf->iSize = 0;
	pBuf->iCapacity = iCapacity;
	return true;
}


// 内部函数：释放 TLS 缓冲区
static void __xrt_tls_buf_free(__xrt_tls_buf* pBuf)
{
	if ( !pBuf ) { return; }
	if ( pBuf->pBase ) {
		__xrt_tls_secure_zero(pBuf->pBase, pBuf->iCapacity);
		xrtFree(pBuf->pBase);
		pBuf->pBase = NULL;
		pBuf->pData = NULL;
	}
	pBuf->iSize = 0;
	pBuf->iCapacity = 0;
}


// 内部函数：确保 TLS 缓冲区
static bool __xrt_tls_buf_ensure(__xrt_tls_buf* pBuf, size_t iExtra)
{
	size_t iHead;
	size_t iNeed;
	size_t iNewCap;
	char* pNew;
	if ( !pBuf ) { return false; }
	if ( !pBuf->pBase ) { return false; }
	// 计算数据指针前方的空闲头部空间大小
	iHead = (pBuf->pData && pBuf->pData >= pBuf->pBase) ? (size_t)(pBuf->pData - pBuf->pBase) : 0u;
	// 防止整数溢出：确保所需空间不会超过 SIZE_MAX
	if ( iExtra > SIZE_MAX - pBuf->iSize ) { return false; }
	iNeed = pBuf->iSize + iExtra;
	if ( iHead > SIZE_MAX - iNeed ) { return false; }
	// 头部空闲 + 已用 + 额外需求 <= 总容量，空间足够
	if ( iHead + iNeed <= pBuf->iCapacity ) { return true; }
	// 容量已满足需求，但数据未紧凑排列，将数据前移消除头部空闲
	if ( iNeed <= pBuf->iCapacity ) {
		if ( pBuf->iSize > 0u && iHead > 0u ) {
			memmove(pBuf->pBase, pBuf->pData, pBuf->iSize);
		}
		pBuf->pData = pBuf->pBase;
		return true;
	}
	// 容量不足，按翻倍策略扩容；若容量已超过 SIZE_MAX/2 则直接使用需求值
	if ( pBuf->iCapacity > SIZE_MAX / 2u ) {
		iNewCap = iNeed;
	} else {
		iNewCap = pBuf->iCapacity ? (pBuf->iCapacity * 2u) : 256u;
	}
	if ( iNewCap < iNeed ) { iNewCap = iNeed; }
	// 重新分配更大的缓冲区
	pNew = (char*)xrtRealloc(pBuf->pBase, iNewCap);
	if ( !pNew ) { return false; }
	// 扩容后数据可能不在起始位置，将其移到新缓冲区开头以消除头部空闲
	if ( pBuf->iSize > 0u && iHead > 0u ) {
		memmove(pNew, pNew + iHead, pBuf->iSize);
	}
	pBuf->pBase = pNew;
	pBuf->pData = pNew;
	pBuf->iCapacity = iNewCap;
	return true;
}


// 内部函数：追加 TLS 缓冲区
static bool __xrt_tls_buf_append(__xrt_tls_buf* pBuf, const char* pData, size_t iLen)
{
	if ( !pBuf || !pData || iLen == 0 ) { return false; }
	if ( !__xrt_tls_buf_ensure(pBuf, iLen) ) { return false; }
	memcpy(pBuf->pData + pBuf->iSize, pData, iLen);
	pBuf->iSize += iLen;
	return true;
}


// 内部函数：消费 TLS 缓冲区
static void __xrt_tls_buf_consume(__xrt_tls_buf* pBuf, size_t iLen)
{
	if ( !pBuf || iLen == 0 ) { return; }
	if ( iLen >= pBuf->iSize ) {
		pBuf->iSize = 0;
		pBuf->pData = pBuf->pBase;
		return;
	}
	pBuf->pData += iLen;
	pBuf->iSize -= iLen;
}


// 内部函数：__xrt_tls_sock_last_err
static int __xrt_tls_sock_last_err(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}


// 内部函数：__xrt_tls_sock_would_block
static bool __xrt_tls_sock_would_block(int iErr)
{
	#if defined(_WIN32) || defined(_WIN64)
		return iErr == WSAEWOULDBLOCK || iErr == WSAEINPROGRESS || iErr == WSAEINTR;
	#else
		return iErr == EAGAIN || iErr == EWOULDBLOCK || iErr == EINTR;
	#endif
}


// 内部函数：__xrt_tls_sock_send
static xnet_result __xrt_tls_sock_send(xsocket hSocket, const char* pData, size_t iLen, size_t* pSent)
{
	int iRet;
	if ( pSent ) { *pSent = 0; }
	if ( hSocket == XSOCKET_INVALID || !pData || iLen == 0 ) { return XRT_NET_ERROR; }
	// 截断发送长度到 INT_MAX，适配 send() 的 int 参数
	iRet = (int)send(hSocket, pData, (int)((iLen > (size_t)INT_MAX) ? INT_MAX : iLen), 0);
	if ( iRet > 0 ) {
		if ( pSent ) { *pSent = (size_t)iRet; }
		return XRT_NET_OK;
	}
	// 对端已关闭连接
	if ( iRet == 0 ) { return XRT_NET_CLOSED; }
	// 根据错误码判断是非阻塞暂时不可用还是真正的错误
	return __xrt_tls_sock_would_block(__xrt_tls_sock_last_err()) ? XRT_NET_AGAIN : XRT_NET_ERROR;
}


// 内部函数：__xrt_tls_sock_recv
static xnet_result __xrt_tls_sock_recv(xsocket hSocket, char* pBuf, size_t iLen, size_t* pReceived)
{
	int iRet;
	if ( pReceived ) { *pReceived = 0; }
	if ( hSocket == XSOCKET_INVALID || !pBuf || iLen == 0 ) { return XRT_NET_ERROR; }
	// 截断接收长度到 INT_MAX，适配 recv() 的 int 参数
	iRet = (int)recv(hSocket, pBuf, (int)((iLen > (size_t)INT_MAX) ? INT_MAX : iLen), 0);
	if ( iRet > 0 ) {
		if ( pReceived ) { *pReceived = (size_t)iRet; }
		return XRT_NET_OK;
	}
	// 对端已关闭连接
	if ( iRet == 0 ) { return XRT_NET_CLOSED; }
	// 根据错误码判断是非阻塞暂时不可用还是真正的错误
	return __xrt_tls_sock_would_block(__xrt_tls_sock_last_err()) ? XRT_NET_AGAIN : XRT_NET_ERROR;
}


/* ============================== TLS resume state ============================== */

struct xrt_tls_resume {
	uint16 iVersion;
	uint16 iCipherSuite;
	uint8 iSessionIdLen;
	uint8 aSessionId[32];
	uint8 aMasterSecret[48];
	uint8 iIdentityType;
	uint8 iIdentityHashLen;
	uint8 bHasServerName;
	uint8 aIdentityHash[32];
	uint8 aServerNameHash[32];
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

#define __XRT_TLS_RESUME_IDENTITY_NONE     0
#define __XRT_TLS_RESUME_IDENTITY_RSA      1
#define __XRT_TLS_RESUME_IDENTITY_ECDSA    2
#define __XRT_TLS_RESUME_IDENTITY_ED25519  3

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

// 内部函数：__xrt_tls_resume_lock_acquire
static void __xrt_tls_resume_lock_wait(uint32 iSpin)
{
	if ( iSpin < 16 ) {
		xrtThreadYield();
		return;
	}
	if ( iSpin < 32 ) {
		xrtSleep(1);
		return;
	}
	if ( iSpin < 48 ) {
		xrtSleep(2);
		return;
	}
	xrtSleep(4);
}


// 内部函数：__xrt_tls_resume_lock_acquire
static void __xrt_tls_resume_lock_acquire(void)
{
	uint32 iSpin = 0;

	// 自旋锁：通过原子交换操作尝试获取锁，失败则逐步退避，降低高并发下的 CPU 空转
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		while ( __xrtAtomicExchange32(&__xrt_tls_resume_lock, 1) != 0 ) {
			__xrt_tls_resume_lock_wait(iSpin++);
		}
	#elif defined(_WIN32) || defined(_WIN64)
		while ( InterlockedCompareExchange((volatile LONG*)&__xrt_tls_resume_lock, 1, 0) != 0 ) {
			__xrt_tls_resume_lock_wait(iSpin++);
		}
	#else
		while ( __sync_lock_test_and_set(&__xrt_tls_resume_lock, 1) != 0 ) {
			__xrt_tls_resume_lock_wait(iSpin++);
		}
	#endif
}


// 内部函数：__xrt_tls_resume_lock_release
static void __xrt_tls_resume_lock_release(void)
{
	// 释放自旋锁：将锁变量原子地置为 0
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		__xrtAtomicExchange32(&__xrt_tls_resume_lock, 0);
	#elif defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)&__xrt_tls_resume_lock, 0);
	#else
		__sync_lock_release(&__xrt_tls_resume_lock);
	#endif
}


// 内部函数：__xrt_tls_resume_copy
static bool __xrt_tls_resume_copy(struct xrt_tls_resume* pDst, const struct xrt_tls_resume* pSrc)
{
	if ( !pDst || !pSrc ) { return false; }
	if ( pSrc->iSessionIdLen == 0 || pSrc->iSessionIdLen > sizeof(pSrc->aSessionId) ) { return false; }
	if ( pSrc->iVersion != __XRT_TLS_VERSION_1_2 ) { return false; }
	memcpy(pDst, pSrc, sizeof(*pDst));
	return true;
}


// 内部函数：__xrt_tls_resume_cache_lookup
static bool __xrt_tls_resume_cache_lookup(const uint8* pSessionId, uint8 iSessionIdLen, struct xrt_tls_resume* pOut)
{
	__xrt_tls_resume_cache_entry* pEntry;
	bool bFound = false;
	if ( !pSessionId || !pOut || iSessionIdLen == 0 || iSessionIdLen > 32u ) { return false; }

	// 加锁遍历链表，按会话ID查找匹配的缓存条目
	__xrt_tls_resume_lock_acquire();
	for ( pEntry = __xrt_tls_resume_cache; pEntry; pEntry = pEntry->pNext ) {
		if ( pEntry->tResume.iSessionIdLen != iSessionIdLen ) { continue; }
		if ( memcmp(pEntry->tResume.aSessionId, pSessionId, iSessionIdLen) != 0 ) { continue; }
		// 匹配成功，复制恢复数据并更新代数（模拟 LRU 访问标记）
		memcpy(pOut, &pEntry->tResume, sizeof(*pOut));
		pEntry->iGeneration = ++__xrt_tls_resume_cache_gen;
		bFound = true;
		break;
	}
	__xrt_tls_resume_lock_release();
	return bFound;
}


// 内部函数：__xrt_tls_resume_cache_store
static void __xrt_tls_resume_cache_store(const struct xrt_tls_resume* pResume)
{
	__xrt_tls_resume_cache_entry* pEntry;
	__xrt_tls_resume_cache_entry* pPrevOldest = NULL;
	__xrt_tls_resume_cache_entry* pOldest = NULL;
	__xrt_tls_resume_cache_entry* pPrev = NULL;
	__xrt_tls_resume_cache_entry* pCurr = NULL;

	// 验证参数有效性：仅支持 TLS 1.2 且会话ID长度合法
	if ( !pResume || pResume->iVersion != __XRT_TLS_VERSION_1_2
		|| pResume->iSessionIdLen == 0 || pResume->iSessionIdLen > 32u
		|| pResume->iIdentityType == __XRT_TLS_RESUME_IDENTITY_NONE
		|| pResume->iIdentityHashLen != 32 ) return;

	__xrt_tls_resume_lock_acquire();
	// 遍历链表查找是否已存在相同会话ID的条目
	for ( pCurr = __xrt_tls_resume_cache; pCurr; pCurr = pCurr->pNext ) {
		if ( pCurr->tResume.iSessionIdLen == pResume->iSessionIdLen
			&& memcmp(pCurr->tResume.aSessionId, pResume->aSessionId, pResume->iSessionIdLen) == 0 ) {
			// 已存在则更新恢复数据并刷新代数
			memcpy(&pCurr->tResume, pResume, sizeof(*pResume));
			pCurr->iGeneration = ++__xrt_tls_resume_cache_gen;
			__xrt_tls_resume_lock_release();
			return;
		}
	}

	// 分配新的缓存条目并初始化
	pEntry = (__xrt_tls_resume_cache_entry*)xrtCalloc(1, sizeof(*pEntry));
	if ( !pEntry ) {
		__xrt_tls_resume_lock_release();
		return;
	}
	memcpy(&pEntry->tResume, pResume, sizeof(*pResume));
	pEntry->iGeneration = ++__xrt_tls_resume_cache_gen;
	// 头插法：将新条目插入链表头部
	pEntry->pNext = __xrt_tls_resume_cache;
	__xrt_tls_resume_cache = pEntry;
	__xrt_tls_resume_cache_count++;

	// 缓存数量超过上限（128），淘汰代数最小的条目（即最久未被访问的）
	if ( __xrt_tls_resume_cache_count > 128u ) {
		// 遍历链表寻找代数最小的条目及其前驱节点
		for ( pCurr = __xrt_tls_resume_cache; pCurr; pCurr = pCurr->pNext ) {
			if ( !pOldest || pCurr->iGeneration < pOldest->iGeneration ) {
				pOldest = pCurr;
				pPrevOldest = pPrev;
			}
			pPrev = pCurr;
		}
		// 从链表中移除最旧条目并释放内存
		if ( pOldest ) {
			if ( pPrevOldest ) { pPrevOldest->pNext = pOldest->pNext; }
			else { __xrt_tls_resume_cache = pOldest->pNext; }
			xrtFree(pOldest);
			__xrt_tls_resume_cache_count--;
		}
	}

	__xrt_tls_resume_lock_release();
}


// 内部函数：__xrt_tls12_client_offers_suite
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

#define __XRT_TLS_ALERT_WARNING       1
#define __XRT_TLS_ALERT_FATAL         2
#define __XRT_TLS_ALERT_CLOSE_NOTIFY  0

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

#define __XRT_TLS_EXT_SERVER_NAME        0x0000
#define __XRT_TLS_EXT_ALPN               0x0010
#define __XRT_TLS_EXT_SUPPORTED_GROUPS   0x000a
#define __XRT_TLS_EXT_EC_POINT_FORMATS   0x000b
#define __XRT_TLS_EXT_SIGNATURE_ALGS     0x000d
#define __XRT_TLS_EXT_SUPPORTED_VERSIONS 0x002b
#define __XRT_TLS_EXT_KEY_SHARE          0x0033
#define __XRT_TLS_EXT_PSK_MODES          0x002d
#define __XRT_TLS_EXT_RENEGOTIATION_INFO 0xff01

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
#define __XRT_TLS_MAX_RECORD_PAYLOAD  16640



/* ============================== 辅助函数 ============================== */

// 内部函数：加载大端序 16 位整数
static inline uint16 __xrt_tls_load_be16(const uint8 *p)
{
	return (uint16)((uint16)p[0] << 8 | p[1]); // 高字节在前
}


// 内部函数：加载 TLS be 24
static inline uint32 __xrt_tls_load_be24(const uint8 *p)
{
	return ((uint32)p[0] << 16) | ((uint32)p[1] << 8) | p[2];
}


// 内部函数：保存 TLS be 16
static inline void __xrt_tls_store_be16(uint8 *p, uint16 v)
{
	p[0] = (uint8)(v >> 8);
	p[1] = (uint8)(v);
}


// 内部函数：保存 TLS be 24
static inline void __xrt_tls_store_be24(uint8 *p, uint32 v)
{
	p[0] = (uint8)(v >> 16);
	p[1] = (uint8)(v >> 8);
	p[2] = (uint8)(v);
}


// 内部函数：保存 TLS be 64
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


// 安全清零内存，防止编译器优化掉清零操作（ 用于清除密钥等敏感数据 ）
static void __xrt_tls_secure_zero(void* pData, size_t iLen)
{
	volatile uint8* pZero = (volatile uint8*)pData; // volatile 防止编译器优化掉写入操作
	while ( pZero != NULL && iLen-- > 0 ) {
		*pZero++ = 0;
	}
}


// 判断客户端是否支持指定的密码套件
static bool __xrt_tls_client_offers_suite(uint16 iSuite, bool bAllowTls13)
{
	switch ( iSuite ) {
		case __XRT_TLS_AES_128_GCM_SHA256:
		case __XRT_TLS_AES_256_GCM_SHA384:
		case __XRT_TLS_CHACHA20_POLY1305_SHA256:
			return bAllowTls13;
		case __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256:
		case __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256:
			return true;
		default:
			return false;
	}
}


static inline bool __xrt_tls_record_payload_valid(size_t iLen)
{
	return iLen <= __XRT_TLS_MAX_RECORD_PAYLOAD;
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

// 解析一个 DER TLV 节点，返回总消耗字节数（ 含头 ），失败返回 -1
static int __xrt_der_parse(uint8 *pDer, size_t iDerSz, struct __xrt_der_tlv *pTlv)
{
	size_t iHeaderLen = 2; // 默认头长：1 字节类型 + 1 字节短格式长度
	uint32 iLen;

	if ( iDerSz < 2 ) { return -1; }

	// 解析标签字节
	pTlv->iType = pDer[0];
	iLen = pDer[1];

	// 长格式长度：最高位为 1，低 7 位表示后续长度字节数
	if ( iLen > 0x7F ) {  // long-form length
		uint8 iLenBytes = iLen & 0x7F; // 取低 7 位作为长度字段占用字节数
		uint8 k;
		if ( iLenBytes == 0 || iLenBytes > sizeof(uint32) ) { return -1; }
		if ( iDerSz < (size_t)(2 + iLenBytes) ) { return -1; }
		iLen = 0;
		for ( k = 0; k < iLenBytes; k++ ) {
			iLen = (iLen << 8) | pDer[2 + k]; // 大端序拼接长度值
		}
		iHeaderLen += iLenBytes;
	}

	// 检查数据是否足够包含头 + 值
	if ( iDerSz < iHeaderLen + iLen ) { return -1; }

	// 填充解析结果
	pTlv->iLen = iLen;
	pTlv->pValue = pDer + iHeaderLen; // 值起始位置
	pTlv->pRaw = pDer; // 原始数据起始位置
	pTlv->iHeaderLen = (uint32)iHeaderLen;
	pTlv->iTotalLen = (uint32)(iHeaderLen + iLen);
	return (int)(iHeaderLen + iLen);
}

// 遍历下一个子 TLV，从父节点的值区域中顺序解析，自动推进父节点指针
static int __xrt_der_next(struct __xrt_der_tlv *pParent, struct __xrt_der_tlv *pChild)
{
	int iConsumed;
	if ( pParent->iLen == 0 ) { return 0; } // 无剩余数据
	iConsumed = __xrt_der_parse(pParent->pValue, pParent->iLen, pChild); // 从当前位置解析子节点
	if ( iConsumed < 0 ) { return -1; }
	// 推进父节点指针，跳过已解析的子节点
	pParent->pValue += iConsumed;
	pParent->iLen -= (uint32)iConsumed;
	return 1;
}

// 在 DER 结构中递归查找 OID，找到后返回其下一个兄弟节点（ 通常为 OID 对应的值 ）
static int UNUSED_ATTR __xrt_der_find_oid(struct __xrt_der_tlv *pTlv, const uint8 *pOid,
	size_t iOidLen, struct __xrt_der_tlv *pFound)
{
	struct __xrt_der_tlv tParent, tChild;
	tParent = *pTlv;

	while ( __xrt_der_next(&tParent, &tChild) > 0 ) {
		// 匹配 OID 标签（ 0x06 ）且内容一致
		if ( (tChild.iType == 0x06) && (tChild.iLen == iOidLen) &&
			(memcmp(tChild.pValue, pOid, iOidLen) == 0) ) {
			return __xrt_der_next(&tParent, pFound); // 返回 OID 后面的节点
		} else if ( tChild.iType & 0x20 ) { // 构造类型（ bit 5 = 1 ），递归进入子结构
			struct __xrt_der_tlv tSub = tChild;
			if ( __xrt_der_find_oid(&tSub, pOid, iOidLen, pFound) ) { return 1; }
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
	const uint8 *pSerial;
	size_t iSerialLen;
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
	const char *sMatchHostname;
	bool bMatchHostIsIp;
	uint8 aMatchHostIp[16];
	size_t iMatchHostIpLen;
	bool bHasDnsSan;
	bool bDnsSanMatched;
	size_t iDnsNameCount;
	bool bHasIpSan;
	bool bIpSanMatched;
	size_t iIpAddrCount;
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


struct __xrt_x509_crl {
	uint8 *pDer;
	size_t iDerLen;
	uint8 *pTbs;
	size_t iTbsLen;
	uint8 *pIssuerRaw;
	size_t iIssuerRawLen;
	time_t iThisUpdate;
	time_t iNextUpdate;
	bool bHasThisUpdate;
	bool bHasNextUpdate;
	enum __xrt_x509_sig_alg iSigAlg;
	size_t iSigHashLen;
	const uint8 *pSig;
	size_t iSigLen;
	uint8 *pRevokedRaw;
	size_t iRevokedRawLen;
};

enum __xrt_x509_parse_mode {
	__XRT_X509_PARSE_STRICT = 0,
	__XRT_X509_PARSE_ALLOW_UNKNOWN_SIGALG
};


// 内部函数：转为小写 TLS ASCII
static int __xrt_tls_ascii_tolower(int c)
{
	if ( c >= 'A' && c <= 'Z' ) { return c + ('a' - 'A'); }
	return c;
}


// 内部函数：__xrt_tls_ascii_case_equal
static bool __xrt_tls_ascii_case_equal(const char *sA, const char *sB)
{
	size_t i;
	size_t iLenA = sA ? strlen(sA) : 0;
	size_t iLenB = sB ? strlen(sB) : 0;

	if ( iLenA != iLenB ) { return false; }
	for ( i = 0; i < iLenA; i++ ) {
		if ( __xrt_tls_ascii_tolower((uint8)sA[i]) != __xrt_tls_ascii_tolower((uint8)sB[i]) ) {
			return false;
		}
	}
	return true;
}


// 内部函数：__xrt_tls_hostname_matches（ 通配符主机名匹配，支持 *.example.com 形式 ）
static bool __xrt_tls_hostname_matches(const char *sPattern, const char *sHost)
{
	const char *pFirstDot;
	const char *pHostDot;
	size_t iSuffixLen;

	if ( !sPattern || !sHost || !sPattern[0] || !sHost[0] ) { return false; }
	if ( strchr(sHost, '*') ) { return false; } // 主机名不允许包含通配符
	// 非通配符模式：直接做大小写不敏感比较
	if ( sPattern[0] != '*' ) { return __xrt_tls_ascii_case_equal(sPattern, sHost); }
	if ( sPattern[1] != '.' ) { return false; } // 通配符后必须跟点号

	// 查找模式中通配符后的第一个点号，用于提取域名后缀
	pFirstDot = strchr(sPattern + 2, '.');
	if ( !pFirstDot ) { return false; }
	pHostDot = strchr(sHost, '.');
	if ( !pHostDot ) { return false; }
	// 确保主机名至少有两级域名，防止匹配 *.com 这类过宽泛的模式
	if ( strchr(pHostDot + 1, '.') == NULL ) { return false; }

	// 比较模式后缀（ 含点号 ）与主机名的域名后缀
	iSuffixLen = strlen(sPattern + 1);
	if ( strlen(pHostDot) != iSuffixLen ) { return false; }
	return __xrt_tls_ascii_case_equal(pHostDot, sPattern + 1);
}


// 内部函数：十六进制字符值
static int __xrt_tls_hex_value(int c)
{
	if ( c >= '0' && c <= '9' ) { return c - '0'; }
	c = __xrt_tls_ascii_tolower(c);
	if ( c >= 'a' && c <= 'f' ) { return c - 'a' + 10; }
	return -1;
}


// 内部函数：解析 IPv4 文本，输出 4 字节二进制地址
static bool __xrt_tls_parse_ipv4_literal(const char* sHost, uint8* pOut)
{
	size_t i;
	const char* p;

	if ( !sHost || !pOut ) { return false; }
	p = sHost;
	// 逐段解析 4 个十进制数，每段 0-255
	for ( i = 0; i < 4; i++ ) {
		unsigned int iPart = 0;
		size_t iDigits = 0;

		while ( *p >= '0' && *p <= '9' ) {
			iPart = (iPart * 10u) + (unsigned int)(*p - '0');
			if ( iPart > 255u ) { return false; }
			p++;
			iDigits++;
		}
		if ( iDigits == 0 ) { return false; } // 每段至少一个数字
		pOut[i] = (uint8)iPart;
		if ( i == 3 ) { break; } // 最后一段不需要点号
		if ( *p != '.' ) { return false; }
		p++;
	}
	return *p == '\0'; // 确保没有多余字符
}


// 内部函数：解析 IPv6 文本，输出 16 字节二进制地址，支持 :: 缩写和 IPv4 映射
static bool __xrt_tls_parse_ipv6_literal(const char* sHost, uint8* pOut)
{
	uint16 aWords[8]; // IPv6 地址的 8 个 16 位字
	const char* pStart = sHost;
	const char* pEnd;
	const char* p;
	int iWordCount = 0;
	int iDoubleColon = -1; // 记录 :: 出现的位置

	if ( !sHost || !pOut ) { return false; }

	memset(aWords, 0, sizeof(aWords));
	// 处理方括号包围的格式，如 [::1]
	if ( sHost[0] == '[' ) {
		pEnd = strrchr(sHost, ']');
		if ( !pEnd || pEnd[1] != '\0' ) { return false; }
		pStart = sHost + 1;
	} else {
		pEnd = sHost + strlen(sHost);
	}
	if ( pStart >= pEnd ) { return false; }

	p = pStart;
	// 处理开头的 ::（ 表示前面若干字为 0 ）
	if ( *p == ':' ) {
		if ( p + 1 >= pEnd || p[1] != ':' ) { return false; }
		iDoubleColon = iWordCount;
		p += 2;
		if ( p >= pEnd ) {
			memset(pOut, 0, 16); // 纯 "::" 等价于全 0 地址
			return true;
		}
	}

	// 逐个解析地址段
	while ( p < pEnd ) {
		uint8 aIPv4[4];
		uint16 iValue = 0;
		size_t iDigits = 0;
		const char* pNextDot = strchr(p, '.');

		// 检测内嵌的 IPv4 地址（ 如 ::ffff:192.168.1.1 ）
		if ( pNextDot && pNextDot < pEnd ) {
			if ( iWordCount > 6 || !__xrt_tls_parse_ipv4_literal(p, aIPv4) ) { return false; }
			// 将 IPv4 的 4 字节拆为 2 个 16 位字（ 大端序 ）
			aWords[iWordCount++] = (uint16)(((uint16)aIPv4[0] << 8) | aIPv4[1]);
			aWords[iWordCount++] = (uint16)(((uint16)aIPv4[2] << 8) | aIPv4[3]);
			p = pEnd;
			break;
		}

		// 解析十六进制段（ 最多 4 位 ）
		while ( p < pEnd ) {
			int iHex = __xrt_tls_hex_value((uint8)*p);
			if ( iHex < 0 ) { break; }
			iValue = (uint16)((iValue << 4) | (uint16)iHex); // 十六进制转数值
			p++;
			iDigits++;
			if ( iDigits > 4 ) { return false; } // 每段最多 4 个十六进制字符
		}
		if ( iDigits == 0 || iWordCount >= 8 ) { return false; }
		aWords[iWordCount++] = iValue;
		if ( p >= pEnd ) { break; }
		if ( *p != ':' ) { return false; }
		p++;
		// 检测 ::（ 连续冒号 ）
		if ( p < pEnd && *p == ':' ) {
			if ( iDoubleColon >= 0 ) { return false; } // 只允许一个 ::
			iDoubleColon = iWordCount;
			p++;
			if ( p >= pEnd ) { break; }
		}
	}

	// 处理 :: 展开的零填充
	if ( iDoubleColon >= 0 ) {
		int iMove = iWordCount - iDoubleColon; // :: 后面已有的字数
		int iFill = 8 - iWordCount; // 需要填充的零字数
		int i;

		if ( iFill < 0 ) { return false; }
		// 将 :: 后面的字右移，腾出空间填零
		for ( i = iMove - 1; i >= 0; i-- ) {
			aWords[iDoubleColon + iFill + i] = aWords[iDoubleColon + i];
		}
		for ( i = 0; i < iFill; i++ ) {
			aWords[iDoubleColon + i] = 0;
		}
		iWordCount = 8;
	}

	if ( iWordCount != 8 ) { return false; }
	// 将 8 个 16 位字转为 16 字节大端序二进制地址
	for ( int i = 0; i < 8; i++ ) {
		pOut[i * 2] = (uint8)(aWords[i] >> 8);     // 高字节
		pOut[i * 2 + 1] = (uint8)(aWords[i] & 0xff); // 低字节
	}
	return true;
}


// 内部函数：解析 IP 文本
static bool __xrt_tls_parse_ip_literal(const char* sHost, uint8* pOut, size_t* pOutLen)
{
	if ( !sHost || !pOut || !pOutLen ) { return false; }
	if ( __xrt_tls_parse_ipv4_literal(sHost, pOut) ) {
		*pOutLen = 4;
		return true;
	}
	if ( __xrt_tls_parse_ipv6_literal(sHost, pOut) ) {
		*pOutLen = 16;
		return true;
	}
	return false;
}


// 内部函数：__xrt_tls_timegm
static time_t __xrt_tls_timegm(struct tm *pTM)
{
	#if defined(_WIN32) || defined(_WIN64)
		return _mkgmtime(pTM);
	#else
		return timegm(pTM);
	#endif
}


// 内部函数：解析 X.509 时间值（ UTCTime 0x17 或 GeneralizedTime 0x18 ），返回 time_t
static bool __xrt_x509_parse_time_value(const struct __xrt_der_tlv *pTime, time_t *pOut)
{
	struct tm tTM;
	int iYear = 0, iMonth = 0, iDay = 0;
	int iHour = 0, iMinute = 0, iSecond = 0;
	const char *p = (const char*)pTime->pValue;

	if ( !pTime || !pOut ) { return false; }
	memset(&tTM, 0, sizeof(tTM));

	// UTCTime 格式：YYMMDDHHMMSSZ（ 2 位年份，>=50 为 19xx，<50 为 20xx ）
	if ( pTime->iType == 0x17 ) {
		if ( pTime->iLen < 13 || p[pTime->iLen - 1] != 'Z' ) { return false; }
		iYear = (p[0] - '0') * 10 + (p[1] - '0');
		iYear += (iYear >= 50) ? 1900 : 2000; // RFC 5280 规则
		iMonth = (p[2] - '0') * 10 + (p[3] - '0');
		iDay = (p[4] - '0') * 10 + (p[5] - '0');
		iHour = (p[6] - '0') * 10 + (p[7] - '0');
		iMinute = (p[8] - '0') * 10 + (p[9] - '0');
		iSecond = (p[10] - '0') * 10 + (p[11] - '0');
	} else if ( pTime->iType == 0x18 ) { // GeneralizedTime 格式：YYYYMMDDHHMMSSZ（ 4 位年份 ）
		if ( pTime->iLen < 15 || p[pTime->iLen - 1] != 'Z' ) { return false; }
		iYear = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
		iMonth = (p[4] - '0') * 10 + (p[5] - '0');
		iDay = (p[6] - '0') * 10 + (p[7] - '0');
		iHour = (p[8] - '0') * 10 + (p[9] - '0');
		iMinute = (p[10] - '0') * 10 + (p[11] - '0');
		iSecond = (p[12] - '0') * 10 + (p[13] - '0');
	} else {
		return false;
	}

	// 校验时间值合法性
	if ( iMonth < 1 || iMonth > 12 || iDay < 1 || iDay > 31 ||
		iHour < 0 || iHour > 23 || iMinute < 0 || iMinute > 59 ||
		iSecond < 0 || iSecond > 60 ) { // 60 允许闰秒
		return false;
	}

	// 转换为 struct tm 并生成 time_t（ UTC ）
	tTM.tm_year = iYear - 1900; // tm_year 从 1900 开始计数
	tTM.tm_mon = iMonth - 1;    // tm_mon 范围 0-11
	tTM.tm_mday = iDay;
	tTM.tm_hour = iHour;
	tTM.tm_min = iMinute;
	tTM.tm_sec = iSecond;
	*pOut = __xrt_tls_timegm(&tTM);
	return *pOut != (time_t)-1;
}


// 内部函数：复制 x 509 字符串值
static bool __xrt_x509_copy_string_value(char *sOut, size_t iOutSz, const struct __xrt_der_tlv *pValue)
{
	size_t iCopy;

	if ( !sOut || iOutSz == 0 || !pValue ) { return false; }
	if ( pValue->iType != 0x0c && pValue->iType != 0x13 && pValue->iType != 0x16 &&
		pValue->iType != 0x14 ) {
		return false;
	}

	iCopy = pValue->iLen;
	if ( iCopy >= iOutSz ) { iCopy = iOutSz - 1; }
	memcpy(sOut, pValue->pValue, iCopy);
	sOut[iCopy] = '\0';
	return iCopy > 0;
}


// 内部函数：从 X.509 Name 结构中提取 commonName 字段值
static bool __xrt_x509_parse_common_name(const struct __xrt_der_tlv *pName, char *sOut, size_t iOutSz)
{
	struct __xrt_der_tlv tRDNs, tSet, tAttr, tOID, tValue;

	if ( !pName || pName->iType != 0x30 ) { return false; }
	tRDNs = *pName;
	// 遍历 RDN（ Relative Distinguished Name ）集合
	while ( __xrt_der_next(&tRDNs, &tSet) > 0 ) {
		if ( tSet.iType != 0x31 ) { continue; } // SET 标签
		{
			struct __xrt_der_tlv tSetItems = tSet;
			// 遍历 SET 中的属性键值对
			while ( __xrt_der_next(&tSetItems, &tAttr) > 0 ) {
				if ( tAttr.iType != 0x30 ) { continue; } // SEQUENCE 标签
				{
					struct __xrt_der_tlv tAttrItems = tAttr;
					// 解析 OID 和值
					if ( __xrt_der_next(&tAttrItems, &tOID) <= 0 || tOID.iType != 0x06 ) { continue; }
					if ( __xrt_der_next(&tAttrItems, &tValue) <= 0 ) { continue; }
					// 匹配 commonName OID（ 2.5.4.3 ）
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


// 内部函数：根据哈希算法 OID 确定哈希输出长度
static bool __xrt_x509_parse_hash_oid(const struct __xrt_der_tlv *pOID, size_t *pHashLen)
{
	if ( !pOID || pOID->iType != 0x06 || !pHashLen ) { return false; }
	// SHA-256 → 32 字节
	if ( pOID->iLen == sizeof(__xrt_oid_sha256) &&
		memcmp(pOID->pValue, __xrt_oid_sha256, sizeof(__xrt_oid_sha256)) == 0 ) {
		*pHashLen = 32;
		return true;
	}
	// SHA-384 → 48 字节
	if ( pOID->iLen == sizeof(__xrt_oid_sha384) &&
		memcmp(pOID->pValue, __xrt_oid_sha384, sizeof(__xrt_oid_sha384)) == 0 ) {
		*pHashLen = 48;
		return true;
	}
	// SHA-512 → 64 字节
	if ( pOID->iLen == sizeof(__xrt_oid_sha512) &&
		memcmp(pOID->pValue, __xrt_oid_sha512, sizeof(__xrt_oid_sha512)) == 0 ) {
		*pHashLen = 64;
		return true;
	}
	return false;
}


// 内部函数：解析 RSA-PSS 签名参数（ hashAlgorithm、maskGenAlgorithm、saltLength ）
static bool __xrt_x509_parse_pss_params(const struct __xrt_der_tlv *pParams,
	enum __xrt_x509_sig_alg *pSigAlg, size_t *pHashLen)
{
	struct __xrt_der_tlv tSeq, tField;
	size_t iHashLen = 0;
	size_t iMGFHashLen = 0;
	size_t iSaltLen = 0;

	if ( !pParams || pParams->iType != 0x30 ) { return false; }
	tSeq = *pParams;
	// 遍历 PSS 参数序列中的各个隐式标签字段
	while ( __xrt_der_next(&tSeq, &tField) > 0 ) {
		if ( tField.iType == 0xa0 ) { // [0] hashAlgorithm
			struct __xrt_der_tlv tInner, tOID;
			if ( __xrt_der_next(&tField, &tInner) <= 0 || tInner.iType != 0x30 ) { return false; }
			if ( __xrt_der_next(&tInner, &tOID) <= 0 ) { return false; }
			if ( !__xrt_x509_parse_hash_oid(&tOID, &iHashLen) ) { return false; }
		} else if ( tField.iType == 0xa1 ) { // [1] maskGenAlgorithm（ 期望 MGF1 ）
			struct __xrt_der_tlv tInner, tOID, tHashAlg, tHashOID;
			if ( __xrt_der_next(&tField, &tInner) <= 0 || tInner.iType != 0x30 ) { return false; }
			if ( __xrt_der_next(&tInner, &tOID) <= 0 || tOID.iType != 0x06 ) { return false; }
			// 验证掩码生成函数为 MGF1
			if ( tOID.iLen != sizeof(__xrt_oid_mgf1) ||
				memcmp(tOID.pValue, __xrt_oid_mgf1, sizeof(__xrt_oid_mgf1)) != 0 ) {
				return false;
			}
			// 提取 MGF1 内部的哈希算法 OID
			if ( __xrt_der_next(&tInner, &tHashAlg) <= 0 || tHashAlg.iType != 0x30 ) { return false; }
			if ( __xrt_der_next(&tHashAlg, &tHashOID) <= 0 ) { return false; }
			if ( !__xrt_x509_parse_hash_oid(&tHashOID, &iMGFHashLen) ) { return false; }
		} else if ( tField.iType == 0xa2 ) { // [2] saltLength
			struct __xrt_der_tlv tInt;
			size_t i;
			if ( __xrt_der_next(&tField, &tInt) <= 0 || tInt.iType != 0x02 ) { return false; }
			// 大端序解析盐长度整数
			for ( i = 0; i < tInt.iLen; i++ ) {
				iSaltLen = (iSaltLen << 8) | tInt.pValue[i];
			}
		}
	}

	// 验证 PSS 参数一致性：MGF1 哈希和盐长度必须与主哈希算法匹配
	if ( iHashLen == 0 ) { return false; }
	if ( iMGFHashLen == 0 ) { iMGFHashLen = iHashLen; } // 默认 MGF1 使用相同哈希
	if ( iSaltLen == 0 ) { iSaltLen = iHashLen; } // 默认盐长度等于哈希长度
	if ( iMGFHashLen != iHashLen || iSaltLen != iHashLen ) { return false; }

	if ( iHashLen == 32 ) { *pSigAlg = __XRT_X509_SIGALG_RSA_PSS_SHA256; }
	else if ( iHashLen == 48 ) { *pSigAlg = __XRT_X509_SIGALG_RSA_PSS_SHA384; }
	else if ( iHashLen == 64 ) { *pSigAlg = __XRT_X509_SIGALG_RSA_PSS_SHA512; }
	else { return false; }

	if ( pHashLen ) { *pHashLen = iHashLen; }
	return true;
}


// 内部函数：__xrt_x509_parse_signature_algorithm
static bool __xrt_x509_parse_signature_algorithm(const struct __xrt_der_tlv *pAlgSeq,
	enum __xrt_x509_sig_alg *pSigAlg, size_t *pHashLen)
{
	struct __xrt_der_tlv tSeq, tOID, tParams;

	if ( !pAlgSeq || pAlgSeq->iType != 0x30 || !pSigAlg || !pHashLen ) { return false; }
	*pSigAlg = __XRT_X509_SIGALG_UNKNOWN;
	*pHashLen = 0;
	tSeq = *pAlgSeq;

	if ( __xrt_der_next(&tSeq, &tOID) <= 0 || tOID.iType != 0x06 ) { return false; }
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
		if ( __xrt_der_next(&tSeq, &tParams) <= 0 ) { return false; }
		return __xrt_x509_parse_pss_params(&tParams, pSigAlg, pHashLen);
	}
	return false;
}


// 内部函数：__xrt_x509_parse_signature_algorithm_ex
static bool __xrt_x509_parse_signature_algorithm_ex(const struct __xrt_der_tlv *pAlgSeq,
	enum __xrt_x509_sig_alg *pSigAlg, size_t *pHashLen, enum __xrt_x509_parse_mode iMode)
{
	struct __xrt_der_tlv tSeq, tOID;

	if ( __xrt_x509_parse_signature_algorithm(pAlgSeq, pSigAlg, pHashLen) ) { return true; }
	if ( iMode != __XRT_X509_PARSE_ALLOW_UNKNOWN_SIGALG ) { return false; }
	if ( !pAlgSeq || pAlgSeq->iType != 0x30 || !pSigAlg || !pHashLen ) { return false; }

	*pSigAlg = __XRT_X509_SIGALG_UNKNOWN;
	*pHashLen = 0;
	tSeq = *pAlgSeq;
	return __xrt_der_next(&tSeq, &tOID) > 0 && tOID.iType == 0x06;
}


// 内部函数：__xrt_x509_match_dns_san
static void __xrt_x509_match_dns_san(struct __xrt_x509_cert *pCert, const uint8 *pName, size_t iNameLen)
{
	char sName[256];

	if ( !pCert || !pName || iNameLen == 0 ) { return; }
	pCert->bHasDnsSan = true;
	pCert->iDnsNameCount++;
	if ( !pCert->sMatchHostname || pCert->bMatchHostIsIp || pCert->bDnsSanMatched ) { return; }
	if ( iNameLen >= sizeof(sName) ) { return; }
	memcpy(sName, pName, iNameLen);
	sName[iNameLen] = '\0';
	if ( __xrt_tls_hostname_matches(sName, pCert->sMatchHostname) ) {
		pCert->bDnsSanMatched = true;
	}
}


// 内部函数：__xrt_x509_match_ip_san
static void __xrt_x509_match_ip_san(struct __xrt_x509_cert *pCert, const uint8 *pAddr, size_t iAddrLen)
{
	if ( !pCert || !pAddr ) { return; }
	if ( iAddrLen != 4 && iAddrLen != 16 ) { return; }
	pCert->bHasIpSan = true;
	pCert->iIpAddrCount++;
	if ( pCert->bMatchHostIsIp && !pCert->bIpSanMatched && pCert->iMatchHostIpLen == iAddrLen
		&& memcmp(pCert->aMatchHostIp, pAddr, iAddrLen) == 0 ) {
		pCert->bIpSanMatched = true;
	}
}


// 内部函数：解析 subjectAltName 扩展，提取 DNS 名称和 IP 地址
static bool __xrt_x509_parse_subject_alt_name(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tSeq, tName;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) { return false; }
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) { return false; }

	while ( __xrt_der_next(&tSeq, &tName) > 0 ) {
		if ( tName.iType == 0x82 ) { // context [2] = dNSName（ IA5String ）
			__xrt_x509_match_dns_san(pCert, tName.pValue, tName.iLen);
		} else if ( tName.iType == 0x87 ) { // context [7] = iPAddress（ 二进制 ）
			__xrt_x509_match_ip_san(pCert, tName.pValue, tName.iLen);
		}
	}
	return true;
}


// 内部函数：解析 keyUsage 扩展，将 BIT STRING 转为位标志
static bool __xrt_x509_parse_key_usage(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tBitStr;
	size_t iBit;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) { return false; }
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tBitStr) < 0 || tBitStr.iType != 0x03 ) { return false; }
	if ( tBitStr.iLen < 2 ) { return false; }

	pCert->bHasKeyUsage = true;
	pCert->iKeyUsage = 0;
	// 遍历前 9 个关键用途位（ bit 0-8 ），按 X.509 定义
	for ( iBit = 0; iBit < 9; iBit++ ) {
		size_t iByte = 1 + (iBit / 8); // 跳过 BIT STRING 的未使用位数首字节
		uint8 iMask = (uint8)(0x80 >> (iBit % 8)); // 从高位到低位逐位检测
		if ( iByte < tBitStr.iLen && (tBitStr.pValue[iByte] & iMask) ) {
			pCert->iKeyUsage |= (uint16)(1u << iBit); // 将位映射到对应标志
		}
	}
	return true;
}


// 内部函数：解析 basicConstraints 扩展，判断是否为 CA 证书
static bool __xrt_x509_parse_basic_constraints(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tSeq, tField;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) { return false; }
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) { return false; }
	pCert->bHasBasicConstraints = true;
	pCert->bIsCA = false; // 默认非 CA

	// 检查第一个字段是否为 BOOLEAN cA（ 如果存在 ）
	if ( __xrt_der_next(&tSeq, &tField) > 0 ) {
		if ( tField.iType == 0x01 && tField.iLen == 1 ) { // BOOLEAN 类型
			pCert->bIsCA = (tField.pValue[0] != 0); // DER 中 TRUE = 0xFF
		}
	}
	return true;
}


// 内部函数：__xrt_x509_parse_extended_key_usage
static bool __xrt_x509_parse_extended_key_usage(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pOctet)
{
	struct __xrt_der_tlv tSeq, tOID;

	if ( !pCert || !pOctet || pOctet->iType != 0x04 ) { return false; }
	if ( __xrt_der_parse(pOctet->pValue, pOctet->iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) { return false; }
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


// 内部函数：解析 SubjectPublicKeyInfo，提取 RSA/EC/Ed25519 公钥参数
static bool __xrt_x509_parse_spki(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pSPKI)
{
	struct __xrt_der_tlv tSPKI, tAlgId, tBitStr, tOID;

	if ( !pCert || !pSPKI || pSPKI->iType != 0x30 ) { return false; }
	tSPKI = *pSPKI;
	// SPKI = AlgorithmIdentifier + BIT STRING 公钥数据
	if ( __xrt_der_next(&tSPKI, &tAlgId) <= 0 || tAlgId.iType != 0x30 ) { return false; }
	if ( __xrt_der_next(&tSPKI, &tBitStr) <= 0 || tBitStr.iType != 0x03 || tBitStr.iLen < 2 ) { return false; }

	{
		struct __xrt_der_tlv tAlg = tAlgId;
		if ( __xrt_der_next(&tAlg, &tOID) <= 0 || tOID.iType != 0x06 ) { return false; }

		bool bIsRSAPSS = tOID.iLen == sizeof(__xrt_oid_rsassa_pss) &&
			memcmp(tOID.pValue, __xrt_oid_rsassa_pss, sizeof(__xrt_oid_rsassa_pss)) == 0;
		// RSA 公钥：BIT STRING 内容为 DER 编码的 SEQUENCE { modulus, exponent }
		if ( (tOID.iLen == sizeof(__xrt_oid_rsa_encryption) &&
			memcmp(tOID.pValue, __xrt_oid_rsa_encryption, sizeof(__xrt_oid_rsa_encryption)) == 0) ||
			bIsRSAPSS ) {
			struct __xrt_der_tlv tKeySeq, tMod, tExp;
			// 跳过 BIT STRING 首字节（ 未使用位数，通常为 0x00 ）
			if ( __xrt_der_parse(tBitStr.pValue + 1, tBitStr.iLen - 1, &tKeySeq) < 0 || tKeySeq.iType != 0x30 ) {
				return false;
			}
			if ( __xrt_der_next(&tKeySeq, &tMod) <= 0 || tMod.iType != 0x02 ) { return false; }
			if ( __xrt_der_next(&tKeySeq, &tExp) <= 0 || tExp.iType != 0x02 ) { return false; }
			pCert->bIsECPubKey = false;
			pCert->bIsRSAPSSKey = bIsRSAPSS;
			pCert->pMod = tMod.pValue;
			pCert->iModSz = tMod.iLen;
			// 跳过模数前导零字节（ DER 整数编码为正数时，若最高位为 1 则补 0x00 ）
			if ( pCert->iModSz > 0 && pCert->pMod[0] == 0x00 ) {
				pCert->pMod++;
				pCert->iModSz--;
			}
			pCert->pExp = tExp.pValue;
			pCert->iExpSz = tExp.iLen;
			return pCert->iModSz > 0 && pCert->iExpSz > 0;
		}

		// EC 公钥：BIT STRING 内容为未压缩点 04||X||Y
		if ( tOID.iLen == sizeof(__xrt_oid_ec_public_key) &&
			memcmp(tOID.pValue, __xrt_oid_ec_public_key, sizeof(__xrt_oid_ec_public_key)) == 0 ) {
			struct __xrt_der_tlv tCurveOID;
			bool bCurveOK = false;

			// 识别曲线类型：P-256（ 65 字节 ）或 P-384（ 97 字节 ）
			if ( __xrt_der_next(&tAlg, &tCurveOID) > 0 && tCurveOID.iType == 0x06 ) {
				if ( tCurveOID.iLen == sizeof(__xrt_oid_prime256v1) &&
					memcmp(tCurveOID.pValue, __xrt_oid_prime256v1, sizeof(__xrt_oid_prime256v1)) == 0 ) {
					bCurveOK = (tBitStr.iLen - 1) == 65; // P-256：1 + 32 + 32 = 65
				} else if ( tCurveOID.iLen == sizeof(__xrt_oid_secp384r1) &&
					memcmp(tCurveOID.pValue, __xrt_oid_secp384r1, sizeof(__xrt_oid_secp384r1)) == 0 ) {
					bCurveOK = (tBitStr.iLen - 1) == 97; // P-384：1 + 48 + 48 = 97
				}
			} else {
				// 无法解析曲线 OID 时，根据公钥长度猜测
				bCurveOK = (tBitStr.iLen - 1) == 65 || (tBitStr.iLen - 1) == 97;
			}
			if ( !bCurveOK ) { return false; }
			pCert->bIsECPubKey = true;
			pCert->bIsEd25519Key = false;
			pCert->pECPub = tBitStr.pValue + 1; // 跳过未压缩点前缀 0x04
			pCert->iECPubSz = tBitStr.iLen - 1;
			return pCert->iECPubSz > 0;
		}
		// Ed25519 公钥：BIT STRING 内容为 0x00 + 32 字节密钥
		if ( tOID.iLen == sizeof(__xrt_oid_ed25519) &&
			memcmp(tOID.pValue, __xrt_oid_ed25519, sizeof(__xrt_oid_ed25519)) == 0 ) {
			if ( tBitStr.iLen != 33 || tBitStr.pValue[0] != 0x00 ) { return false; }
			pCert->bIsECPubKey = false;
			pCert->bIsEd25519Key = true;
			pCert->bIsRSAPSSKey = false;
			pCert->pEdPub = tBitStr.pValue + 1; // 跳过前导 0x00 字节
			pCert->iEdPubSz = 32;
			return true;
		}
	}
	return false;
}


// 内部函数：遍历 X.509 v3 扩展区域，解析各扩展项
static bool __xrt_x509_parse_extensions(struct __xrt_x509_cert *pCert, const struct __xrt_der_tlv *pExtsField)
{
	struct __xrt_der_tlv tExtField, tExtWrapper, tExtSeq, tExt;

	if ( !pCert || !pExtsField || pExtsField->iType != 0xa3 ) { return false; } // [3] EXPLICIT
	tExtField = *pExtsField;
	if ( __xrt_der_next(&tExtField, &tExtWrapper) <= 0 || tExtWrapper.iType != 0x30 ) {
		return false;
	}

	// 遍历扩展序列中的每个扩展项
	tExtSeq = tExtWrapper;
	while ( __xrt_der_next(&tExtSeq, &tExt) > 0 ) {
		struct __xrt_der_tlv tItems = tExt;
		struct __xrt_der_tlv tOID, tMaybeCrit, tValue;
		bool bHaveValue = false;

		if ( tExt.iType != 0x30 ) { continue; } // 每个 Ext 为 SEQUENCE
		if ( __xrt_der_next(&tItems, &tOID) <= 0 || tOID.iType != 0x06 ) { continue; }
		// 解析可选的 critical 标志（ BOOLEAN ），如果不存在则直接是 OCTET STRING 值
		if ( __xrt_der_next(&tItems, &tMaybeCrit) <= 0 ) { continue; }
		if ( tMaybeCrit.iType == 0x01 ) { // BOOLEAN → 有 critical 标志
			if ( __xrt_der_next(&tItems, &tValue) <= 0 ) { continue; }
			bHaveValue = true;
		} else {
			tValue = tMaybeCrit; // 无 critical 标志，第二项就是值
			bHaveValue = true;
		}
		if ( !bHaveValue || tValue.iType != 0x04 ) { continue; } // 扩展值必须为 OCTET STRING

		// 根据 OID 分发到对应的扩展解析函数
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


// 内部函数：解析 X.509 证书 DER 数据，填充证书结构体（ 支持严格/宽松模式 ）
static bool __xrt_x509_parse_ex(uint8 *pCertDer, size_t iCertLen, struct __xrt_x509_cert *pCert,
	enum __xrt_x509_parse_mode iMode, const char *sMatchHostname)
{
	struct __xrt_der_tlv tRoot, tTbs, tSigAlg, tSigValue;
	struct __xrt_der_tlv tFields, tField, tValidity, tTime;

	if ( !pCertDer || !pCert || iCertLen == 0 ) { return false; }
	memset(pCert, 0, sizeof(*pCert));
	pCert->pDer = pCertDer;
	pCert->iDerLen = iCertLen;
	if ( sMatchHostname && sMatchHostname[0] ) {
		pCert->sMatchHostname = sMatchHostname;
		pCert->bMatchHostIsIp = __xrt_tls_parse_ip_literal(sMatchHostname,
			pCert->aMatchHostIp, &pCert->iMatchHostIpLen);
	}

	// 证书 DER 结构：SEQUENCE { tbsCertificate, signatureAlgorithm, signatureValue }
	if ( __xrt_der_parse(pCertDer, iCertLen, &tRoot) < 0 || tRoot.iType != 0x30 ) { return false; }
	if ( __xrt_der_next(&tRoot, &tTbs) <= 0 || tTbs.iType != 0x30 ) { return false; }
	if ( __xrt_der_next(&tRoot, &tSigAlg) <= 0 || tSigAlg.iType != 0x30 ) { return false; }
	// 签名值为 BIT STRING，首字节为未使用位数（ 通常为 0 ）
	if ( __xrt_der_next(&tRoot, &tSigValue) <= 0 || tSigValue.iType != 0x03 || tSigValue.iLen < 2 ) { return false; }
	if ( !__xrt_x509_parse_signature_algorithm_ex(&tSigAlg, &pCert->iSigAlg, &pCert->iSigHashLen, iMode) ) {
		return false;
	}

	// 记录 TBS 原始数据指针（ 用于签名验证 ）
	pCert->pTbs = tTbs.pRaw;
	pCert->iTbsLen = tTbs.iTotalLen;
	pCert->pSig = tSigValue.pValue + 1;
	pCert->iSigLen = tSigValue.iLen - 1;

	tFields = tTbs;
	if ( __xrt_der_next(&tFields, &tField) <= 0 ) { return false; }
	if ( tField.iType == 0xa0 ) {
		if ( __xrt_der_next(&tFields, &tField) <= 0 ) { return false; }
	}

	// serialNumber
	if ( tField.iType != 0x02 ) { return false; }
	pCert->pSerial = tField.pValue;
	pCert->iSerialLen = tField.iLen;
	// signature
	if ( __xrt_der_next(&tFields, &tField) <= 0 ) { return false; }
	// issuer
	if ( __xrt_der_next(&tFields, &tField) <= 0 || tField.iType != 0x30 ) { return false; }
	pCert->pIssuerRaw = tField.pRaw;
	pCert->iIssuerRawLen = tField.iTotalLen;
	// validity
	if ( __xrt_der_next(&tFields, &tValidity) <= 0 || tValidity.iType != 0x30 ) { return false; }
	if ( __xrt_der_next(&tValidity, &tTime) <= 0 ) { return false; }
	if ( !__xrt_x509_parse_time_value(&tTime, &pCert->iNotBefore) ) { return false; }
	if ( __xrt_der_next(&tValidity, &tTime) <= 0 ) { return false; }
	if ( !__xrt_x509_parse_time_value(&tTime, &pCert->iNotAfter) ) { return false; }
	pCert->bHasValidity = true;
	// subject
	if ( __xrt_der_next(&tFields, &tField) <= 0 || tField.iType != 0x30 ) { return false; }
	pCert->pSubjectRaw = tField.pRaw;
	pCert->iSubjectRawLen = tField.iTotalLen;
	pCert->bHasCommonName = __xrt_x509_parse_common_name(&tField, pCert->sCommonName, sizeof(pCert->sCommonName));
	// subjectPublicKeyInfo
	if ( __xrt_der_next(&tFields, &tField) <= 0 || tField.iType != 0x30 ) { return false; }
	if ( !__xrt_x509_parse_spki(pCert, &tField) ) { return false; }

	while ( __xrt_der_next(&tFields, &tField) > 0 ) {
		if ( tField.iType == 0xa3 ) {
			__xrt_x509_parse_extensions(pCert, &tField);
		}
	}

	return true;
}


// 内部函数：__xrt_x509_parse
static bool __xrt_x509_parse(uint8 *pCertDer, size_t iCertLen, struct __xrt_x509_cert *pCert)
{
	return __xrt_x509_parse_ex(pCertDer, iCertLen, pCert, __XRT_X509_PARSE_STRICT, NULL);
}


// 内部函数：__xrt_x509_parse_for_chain
static bool __xrt_x509_parse_for_chain(uint8 *pCertDer, size_t iCertLen, struct __xrt_x509_cert *pCert)
{
	return __xrt_x509_parse_ex(pCertDer, iCertLen, pCert, __XRT_X509_PARSE_ALLOW_UNKNOWN_SIGALG, NULL);
}


// 内部函数：__xrt_x509_parse_leaf_for_chain
static bool __xrt_x509_parse_leaf_for_chain(uint8 *pCertDer, size_t iCertLen,
	struct __xrt_x509_cert *pCert, const char *sMatchHostname)
{
	return __xrt_x509_parse_ex(pCertDer, iCertLen, pCert,
		__XRT_X509_PARSE_ALLOW_UNKNOWN_SIGALG, sMatchHostname);
}


// 内部函数：解析 CRL（ 证书吊销列表 ）DER 数据
static bool __xrt_x509_parse_crl(uint8 *pCrlDer, size_t iCrlLen, struct __xrt_x509_crl *pCrl)
{
	struct __xrt_der_tlv tRoot, tTbs, tSigAlg, tSigValue;
	struct __xrt_der_tlv tFields, tField, tMaybe;

	if ( !pCrlDer || iCrlLen == 0 || !pCrl ) { return false; }
	memset(pCrl, 0, sizeof(*pCrl));
	pCrl->pDer = pCrlDer;
	pCrl->iDerLen = iCrlLen;

	// CRL 顶层结构：SEQUENCE { tbsCertList, signatureAlgorithm, signatureValue }
	if ( __xrt_der_parse(pCrlDer, iCrlLen, &tRoot) < 0 || tRoot.iType != 0x30 ) { return false; }
	if ( __xrt_der_next(&tRoot, &tTbs) <= 0 || tTbs.iType != 0x30 ) { return false; }
	if ( __xrt_der_next(&tRoot, &tSigAlg) <= 0 || tSigAlg.iType != 0x30 ) { return false; }
	if ( __xrt_der_next(&tRoot, &tSigValue) <= 0 || tSigValue.iType != 0x03 || tSigValue.iLen < 2 ) { return false; }
	if ( !__xrt_x509_parse_signature_algorithm(&tSigAlg, &pCrl->iSigAlg, &pCrl->iSigHashLen) ) { return false; }

	// 记录 TBS 和签名数据
	pCrl->pTbs = tTbs.pRaw;
	pCrl->iTbsLen = tTbs.iTotalLen;
	pCrl->pSig = tSigValue.pValue + 1; // 跳过 BIT STRING 未使用位数
	pCrl->iSigLen = tSigValue.iLen - 1;

	// 解析 TBS 字段
	tFields = tTbs;
	// 可选的版本号（ INTEGER ）
	if ( __xrt_der_next(&tFields, &tField) <= 0 ) { return false; }
	if ( tField.iType == 0x02 ) {
		if ( __xrt_der_next(&tFields, &tField) <= 0 ) { return false; }
	}
	// 签名算法（ 与外层重复，跳过 ）
	if ( tField.iType != 0x30 ) { return false; }
	// 颁发者名称
	if ( __xrt_der_next(&tFields, &tField) <= 0 || tField.iType != 0x30 ) { return false; }
	pCrl->pIssuerRaw = tField.pRaw;
	pCrl->iIssuerRawLen = tField.iTotalLen;
	// thisUpdate 时间
	if ( __xrt_der_next(&tFields, &tField) <= 0 ) { return false; }
	if ( !__xrt_x509_parse_time_value(&tField, &pCrl->iThisUpdate) ) { return false; }
	pCrl->bHasThisUpdate = true;

	// 可选字段：nextUpdate 和 revokedCertificates
	if ( __xrt_der_next(&tFields, &tMaybe) > 0 ) {
		if ( tMaybe.iType == 0x17 || tMaybe.iType == 0x18 ) { // UTCTime 或 GeneralizedTime
			if ( !__xrt_x509_parse_time_value(&tMaybe, &pCrl->iNextUpdate) ) { return false; }
			pCrl->bHasNextUpdate = true;
			// nextUpdate 后面可能是 revokedCertificates
			if ( __xrt_der_next(&tFields, &tMaybe) > 0 ) {
				if ( tMaybe.iType == 0x30 ) { // SEQUENCE OF revoked entries
					pCrl->pRevokedRaw = tMaybe.pRaw;
					pCrl->iRevokedRawLen = tMaybe.iTotalLen;
				}
			}
		} else if ( tMaybe.iType == 0x30 ) { // 无 nextUpdate，直接是 revokedCertificates
			pCrl->pRevokedRaw = tMaybe.pRaw;
			pCrl->iRevokedRawLen = tMaybe.iTotalLen;
		}
	}

	return true;
}


// 内部函数：__xrt_x509_is_time_valid
static bool __xrt_x509_is_time_valid(const struct __xrt_x509_cert *pCert, time_t iNow)
{
	if ( !pCert || !pCert->bHasValidity ) { return false; }
	return iNow >= pCert->iNotBefore && iNow <= pCert->iNotAfter;
}


// 内部函数：序列号相等比较（ 跳过前导零后再比较 ）
static bool __xrt_x509_serial_eq(const uint8 *pA, size_t iALen, const uint8 *pB, size_t iBLen)
{
	if ( !pA || !pB || iALen == 0 || iBLen == 0 ) { return false; }
	// 跳过 DER INTEGER 编码中的前导零字节
	while ( iALen > 1 && *pA == 0 ) {
		pA++;
		iALen--;
	}
	while ( iBLen > 1 && *pB == 0 ) {
		pB++;
		iBLen--;
	}
	return iALen == iBLen && memcmp(pA, pB, iALen) == 0;
}


// 内部函数：CRL 时间有效
static bool __xrt_x509_crl_is_time_valid(const struct __xrt_x509_crl *pCrl, time_t iNow)
{
	if ( !pCrl || !pCrl->bHasThisUpdate ) { return false; }
	if ( iNow < pCrl->iThisUpdate ) { return false; }
	if ( pCrl->bHasNextUpdate && iNow > pCrl->iNextUpdate ) { return false; }
	return true;
}


// 内部函数：x 509 名称相等比较（ 原始 DER 字节 ）
static bool __xrt_x509_name_eq(const uint8 *pA, size_t iALen, const uint8 *pB, size_t iBLen)
{
	return pA && pB && iALen == iBLen && memcmp(pA, pB, iALen) == 0;
}


// 内部函数：比较两个证书的公钥是否相同（ 支持 RSA/EC/Ed25519 ）
static bool __xrt_x509_public_key_eq(const struct __xrt_x509_cert *pA, const struct __xrt_x509_cert *pB)
{
	if ( !pA || !pB ) { return false; }
	// 密钥类型必须一致
	if ( pA->bIsECPubKey != pB->bIsECPubKey ) { return false; }
	if ( pA->bIsEd25519Key != pB->bIsEd25519Key ) { return false; }
	if ( pA->bIsRSAPSSKey != pB->bIsRSAPSSKey ) { return false; }

	// 按密钥类型比较公钥数据
	if ( pA->bIsEd25519Key ) {
		return pA->pEdPub && pB->pEdPub && pA->iEdPubSz == pB->iEdPubSz &&
			memcmp(pA->pEdPub, pB->pEdPub, pA->iEdPubSz) == 0;
	}
	if ( pA->bIsECPubKey ) {
		return pA->pECPub && pB->pECPub && pA->iECPubSz == pB->iECPubSz &&
			memcmp(pA->pECPub, pB->pECPub, pA->iECPubSz) == 0;
	}
	// RSA：比较模数和指数
	return pA->pMod && pB->pMod && pA->pExp && pB->pExp &&
		pA->iModSz == pB->iModSz && pA->iExpSz == pB->iExpSz &&
		memcmp(pA->pMod, pB->pMod, pA->iModSz) == 0 &&
		memcmp(pA->pExp, pB->pExp, pA->iExpSz) == 0;
}


// 内部函数：检查证书是否与信任锚匹配（ 主题名 + 公钥 都相同 ）
static bool __xrt_x509_anchor_matches(const struct __xrt_x509_cert *pCert, const struct __xrt_x509_cert *pAnchor)
{
	if ( !pCert || !pAnchor ) { return false; }
	// 比较主题名称的 DER 编码
	if ( !__xrt_x509_name_eq(pCert->pSubjectRaw, pCert->iSubjectRawLen,
		pAnchor->pSubjectRaw, pAnchor->iSubjectRawLen) ) {
		return false;
	}
	// 比较公钥数据
	return __xrt_x509_public_key_eq(pCert, pAnchor);
}


// 内部函数：验证签名对象，根据签名算法分派到 RSA-PKCS1/RSA-PSS/ECDSA/Ed25519 验证
static bool __xrt_x509_verify_signed_object(const uint8 *pTbs, size_t iTbsLen,
	enum __xrt_x509_sig_alg iSigAlg, size_t iSigHashLen,
	const uint8 *pSig, size_t iSigLen,
	const struct __xrt_x509_cert *pIssuer)
{
	uint8 aHash[64]; // 最大 SHA-512 输出 64 字节

	if ( !pTbs || iTbsLen == 0 || !pSig || iSigLen == 0 || !pIssuer ) { return false; }

	switch ( iSigAlg ) {
		case __XRT_X509_SIGALG_RSA_PKCS1_SHA256:
		case __XRT_X509_SIGALG_RSA_PKCS1_SHA384:
		case __XRT_X509_SIGALG_RSA_PKCS1_SHA512:
			// RSA PKCS#1 v1.5 签名验证：对 TBS 计算哈希后验证
			if ( !__xrt_hash_bytes(aHash, iSigHashLen, pTbs, iTbsLen) ) { return false; }
			if ( pIssuer->bIsECPubKey || !pIssuer->pMod || !pIssuer->pExp ) { return false; }
			return xrtRSAPKCS1Verify(aHash, iSigHashLen, pSig, iSigLen,
				pIssuer->pMod, pIssuer->iModSz, pIssuer->pExp, pIssuer->iExpSz);
		case __XRT_X509_SIGALG_RSA_PSS_SHA256:
		case __XRT_X509_SIGALG_RSA_PSS_SHA384:
		case __XRT_X509_SIGALG_RSA_PSS_SHA512:
			// RSA-PSS 签名验证：概率签名方案，安全性高于 PKCS#1 v1.5
			if ( !__xrt_hash_bytes(aHash, iSigHashLen, pTbs, iTbsLen) ) { return false; }
			if ( pIssuer->bIsECPubKey || !pIssuer->pMod || !pIssuer->pExp ) { return false; }
			return xrtRSAPSSVerify(aHash, iSigHashLen, pSig, iSigLen,
				pIssuer->pMod, pIssuer->iModSz, pIssuer->pExp, pIssuer->iExpSz);
		case __XRT_X509_SIGALG_ECDSA_SHA256:
		case __XRT_X509_SIGALG_ECDSA_SHA384:
		case __XRT_X509_SIGALG_ECDSA_SHA512:
			// ECDSA 签名验证：对 TBS 计算哈希后验证
			if ( !__xrt_hash_bytes(aHash, iSigHashLen, pTbs, iTbsLen) ) { return false; }
			if ( !pIssuer->bIsECPubKey || !pIssuer->pECPub ) { return false; }
			return xrtECDSAVerify(aHash, iSigHashLen, pSig, iSigLen,
				pIssuer->pECPub, pIssuer->iECPubSz);
		case __XRT_X509_SIGALG_ED25519:
			// Ed25519 签名验证：纯签名（ 无哈希预处理 ），直接对原始消息验证
			if ( !pIssuer->bIsEd25519Key || !pIssuer->pEdPub || pIssuer->iEdPubSz != 32 ) { return false; }
			return xrtEd25519Verify(pTbs, iTbsLen, pSig, pIssuer->pEdPub);
		default:
			return false;
	}
}


// 内部函数：__xrt_x509_verify_signature
static bool __xrt_x509_verify_signature(const struct __xrt_x509_cert *pChild, const struct __xrt_x509_cert *pIssuer)
{
	if ( !pChild || !pIssuer ) { return false; }
	return __xrt_x509_verify_signed_object(pChild->pTbs, pChild->iTbsLen,
		pChild->iSigAlg, pChild->iSigHashLen, pChild->pSig, pChild->iSigLen, pIssuer);
}


// 内部函数：验证 CRL 签名
static bool __xrt_x509_verify_crl_signature(const struct __xrt_x509_crl *pCrl, const struct __xrt_x509_cert *pIssuer)
{
	if ( !pCrl || !pIssuer ) { return false; }
	return __xrt_x509_verify_signed_object(pCrl->pTbs, pCrl->iTbsLen,
		pCrl->iSigAlg, pCrl->iSigHashLen, pCrl->pSig, pCrl->iSigLen, pIssuer);
}


// 内部函数：CRL 是否吊销指定证书（ 遍历吊销列表中的序列号 ）
static bool __xrt_x509_crl_revokes_cert(const struct __xrt_x509_crl *pCrl, const struct __xrt_x509_cert *pCert)
{
	struct __xrt_der_tlv tSeq, tEntry, tItems, tSerial;

	if ( !pCrl || !pCert || !pCrl->pRevokedRaw || pCrl->iRevokedRawLen == 0 ) { return false; }
	if ( __xrt_der_parse(pCrl->pRevokedRaw, pCrl->iRevokedRawLen, &tSeq) < 0 || tSeq.iType != 0x30 ) { return false; }
	// 遍历吊销条目序列
	while ( __xrt_der_next(&tSeq, &tEntry) > 0 ) {
		if ( tEntry.iType != 0x30 ) { continue; } // 每个条目为 SEQUENCE
		tItems = tEntry;
		// 提取条目中的序列号并比较
		if ( __xrt_der_next(&tItems, &tSerial) <= 0 || tSerial.iType != 0x02 ) { continue; }
		if ( __xrt_x509_serial_eq(pCert->pSerial, pCert->iSerialLen, tSerial.pValue, tSerial.iLen) ) {
			return true; // 证书序列号在吊销列表中
		}
	}
	return false;
}


// 内部函数：判断证书是否可作为 CA 使用（ 检查 basicConstraints 和 keyUsage ）
static bool __xrt_x509_is_ca_usable(const struct __xrt_x509_cert *pCert)
{
	if ( !pCert ) { return false; }
	if ( pCert->bHasBasicConstraints && !pCert->bIsCA ) { return false; }
	if ( pCert->bHasKeyUsage && (pCert->iKeyUsage & (1u << 5)) == 0 ) { return false; } // bit 5 = keyCertSign
	return true;
}


// 内部函数：__xrt_tls_current_cert_trusted_by_anchor
static bool __xrt_tls_current_cert_trusted_by_anchor(const struct __xrt_x509_cert *pCurrent,
	const struct __xrt_x509_cert *pAnchor, time_t iNow)
{
	if ( !pCurrent || !pAnchor ) { return false; }
	if ( !__xrt_x509_is_ca_usable(pAnchor) ) { return false; }
	if ( !__xrt_x509_is_time_valid(pAnchor, iNow) ) { return false; }
	if ( __xrt_x509_anchor_matches(pCurrent, pAnchor) ) { return true; }
	if ( !__xrt_x509_name_eq(pCurrent->pIssuerRaw, pCurrent->iIssuerRawLen,
		pAnchor->pSubjectRaw, pAnchor->iSubjectRawLen) ) {
		return false;
	}
	return __xrt_x509_verify_signature(pCurrent, pAnchor);
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

// TLS 上下文 （ 不透明结构 ）
struct xrt_tls_context {
	volatile long iApiLock;
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
	char sAlpnProtocols[256];
	char sAlpnSelected[256];
	
	// 服务端 SNI
	char sClientSNI[254];         // 服务端: 客户端请求的 SNI hostname
	void (*OnSNI)(xtlssession *pSession, const char *sHostName, ptr pUserData);
	ptr pSNIUserData;
	xtlsverifyproc OnVerify;
	ptr pVerifyUserData;
	xtlssession* pSession;
	
	// 证书数据
	uint8 *pCertDer;
	size_t iCertDerLen;
	uint8 *pKeyDer;
	size_t iKeyDerLen;
	uint8 *pCaData;
	size_t iCaDataLen;
	uint8 *pCrlData;
	size_t iCrlDataLen;
	uint8 *pPeerCertDer;
	size_t iPeerCertDerLen;
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
	bool bCloseNotifySent;
	bool bCloseNotifyReceived;
	uint8 iPeerAlertLevel;
	uint8 iPeerAlertDesc;
	
	// 服务器证书公钥 （ RSA 或 EC ）
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
	__xrt_tls_buf tHandshakeBuf;        // TLS 1.3 握手消息重组缓冲区 （ 用于跨记录的大消息 ）
	__xrt_tls_buf tPlainBuf;            // 已解密但尚未被应用层消费的明文
	size_t iRecvOffset;
	size_t iRecvMsgLen;
	uint8 iContentType;
};


static bool __xrt_tls_consttime_equal(const uint8 *pA, const uint8 *pB, size_t iLen);


static bool __xrt_tls_alpn_is_trim_char(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}


static int __xrt_tls_alpn_next_config_protocol(const char *sProtocols, size_t *pPos, const char **pStart, size_t *pLen)
{
	size_t i;
	size_t iBegin;
	size_t iEnd;
	size_t iNext;
	size_t j;

	if ( !sProtocols || !pPos || !pStart || !pLen ) { return -1; }
	i = *pPos;
	while ( __xrt_tls_alpn_is_trim_char(sProtocols[i]) ) { i++; }
	if ( sProtocols[i] == '\0' ) { return 0; }
	if ( sProtocols[i] == ',' ) { return -1; }

	iBegin = i;
	while ( sProtocols[i] != '\0' && sProtocols[i] != ',' ) { i++; }
	iEnd = i;
	while ( iEnd > iBegin && __xrt_tls_alpn_is_trim_char(sProtocols[iEnd - 1]) ) { iEnd--; }
	if ( iEnd <= iBegin || iEnd - iBegin > 255u ) { return -1; }
	for ( j = iBegin; j < iEnd; j++ ) {
		unsigned char ch = (unsigned char)sProtocols[j];
		if ( ch <= 0x20u || ch >= 0x7fu || ch == ',' ) { return -1; }
	}

	if ( sProtocols[i] == ',' ) {
		iNext = i + 1u;
		while ( __xrt_tls_alpn_is_trim_char(sProtocols[iNext]) ) { iNext++; }
		if ( sProtocols[iNext] == '\0' ) { return -1; }
		i++;
	}

	*pStart = sProtocols + iBegin;
	*pLen = iEnd - iBegin;
	*pPos = i;
	return 1;
}


static bool __xrt_tls_alpn_config_valid(const char *sProtocols)
{
	size_t iPos = 0;
	const char *sProto = NULL;
	size_t iProtoLen = 0;
	int iRead;
	bool bHave = false;

	if ( !sProtocols || sProtocols[0] == '\0' ) { return true; }
	while ( (iRead = __xrt_tls_alpn_next_config_protocol(sProtocols, &iPos, &sProto, &iProtoLen)) > 0 ) {
		(void)sProto;
		(void)iProtoLen;
		bHave = true;
	}
	return iRead == 0 && bHave;
}


static bool __xrt_tls_alpn_config_contains(const char *sProtocols, const uint8 *pName, size_t iNameLen)
{
	size_t iPos = 0;
	const char *sProto = NULL;
	size_t iProtoLen = 0;
	int iRead;

	if ( !sProtocols || !pName || iNameLen == 0u ) { return false; }
	while ( (iRead = __xrt_tls_alpn_next_config_protocol(sProtocols, &iPos, &sProto, &iProtoLen)) > 0 ) {
		if ( iProtoLen == iNameLen && memcmp(sProto, pName, iNameLen) == 0 ) { return true; }
	}
	return false;
}


static bool __xrt_tls_alpn_wire_list_valid(const uint8 *pData, size_t iLen)
{
	size_t iPos;
	size_t iEnd;

	if ( !pData || iLen < 3u ) { return false; }
	iEnd = 2u + (size_t)__xrt_tls_load_be16(pData);
	if ( iEnd != iLen ) { return false; }
	iPos = 2u;
	while ( iPos < iEnd ) {
		uint8 iNameLen = pData[iPos++];
		if ( iNameLen == 0u || iPos + (size_t)iNameLen > iEnd ) { return false; }
		iPos += (size_t)iNameLen;
	}
	return iPos == iEnd;
}


static bool __xrt_tls_alpn_wire_contains(const uint8 *pData, size_t iLen, const char *sProto, size_t iProtoLen)
{
	size_t iPos;
	size_t iEnd;

	if ( !pData || !sProto || iProtoLen == 0u || iProtoLen > 255u || !__xrt_tls_alpn_wire_list_valid(pData, iLen) ) {
		return false;
	}
	iEnd = iLen;
	iPos = 2u;
	while ( iPos < iEnd ) {
		uint8 iNameLen = pData[iPos++];
		if ( iNameLen == iProtoLen && memcmp(pData + iPos, sProto, iProtoLen) == 0 ) { return true; }
		iPos += (size_t)iNameLen;
	}
	return false;
}


static bool __xrt_tls_alpn_write_extension(uint8 *pBuf, size_t *pPos, size_t iCap, const char *sProtocols)
{
	size_t iPos;
	size_t iExtLenPos;
	size_t iListLenPos;
	size_t iListStart;
	size_t iCfgPos = 0;
	const char *sProto = NULL;
	size_t iProtoLen = 0;
	int iRead;

	if ( !pBuf || !pPos || !sProtocols || sProtocols[0] == '\0' ) { return true; }
	if ( !__xrt_tls_alpn_config_valid(sProtocols) ) { return false; }
	iPos = *pPos;
	if ( iPos + 6u > iCap ) { return false; }
	__xrt_tls_store_be16(pBuf + iPos, __XRT_TLS_EXT_ALPN);
	iPos += 2u;
	iExtLenPos = iPos;
	iPos += 2u;
	iListLenPos = iPos;
	iPos += 2u;
	iListStart = iPos;

	while ( (iRead = __xrt_tls_alpn_next_config_protocol(sProtocols, &iCfgPos, &sProto, &iProtoLen)) > 0 ) {
		if ( iPos + 1u + iProtoLen > iCap ) { return false; }
		pBuf[iPos++] = (uint8)iProtoLen;
		memcpy(pBuf + iPos, sProto, iProtoLen);
		iPos += iProtoLen;
	}
	if ( iRead < 0 || iPos == iListStart || iPos - iListStart > 65535u ) { return false; }
	__xrt_tls_store_be16(pBuf + iListLenPos, (uint16)(iPos - iListStart));
	__xrt_tls_store_be16(pBuf + iExtLenPos, (uint16)(2u + iPos - iListStart));
	*pPos = iPos;
	return true;
}


static bool __xrt_tls_alpn_select_from_client(xtlsctx *pCtx, const uint8 *pData, size_t iLen)
{
	size_t iCfgPos = 0;
	const char *sProto = NULL;
	size_t iProtoLen = 0;
	int iRead;

	if ( !pCtx ) { return false; }
	pCtx->sAlpnSelected[0] = '\0';
	if ( !pData || iLen == 0u ) { return true; }
	if ( !__xrt_tls_alpn_wire_list_valid(pData, iLen) ) { return false; }
	if ( pCtx->sAlpnProtocols[0] == '\0' ) { return true; }

	while ( (iRead = __xrt_tls_alpn_next_config_protocol(pCtx->sAlpnProtocols, &iCfgPos, &sProto, &iProtoLen)) > 0 ) {
		if ( __xrt_tls_alpn_wire_contains(pData, iLen, sProto, iProtoLen) ) {
			memcpy(pCtx->sAlpnSelected, sProto, iProtoLen);
			pCtx->sAlpnSelected[iProtoLen] = '\0';
			return true;
		}
	}
	return false;
}


static bool __xrt_tls_alpn_accept_selected(xtlsctx *pCtx, const uint8 *pData, size_t iLen)
{
	size_t iListLen;
	size_t iNameLen;

	if ( !pCtx || !pData || pCtx->sAlpnProtocols[0] == '\0' ) { return false; }
	if ( iLen < 4u ) { return false; }
	iListLen = (size_t)__xrt_tls_load_be16(pData);
	if ( iListLen + 2u != iLen || iListLen < 2u ) { return false; }
	iNameLen = (size_t)pData[2];
	if ( iNameLen == 0u || 3u + iNameLen != iLen || iNameLen >= sizeof(pCtx->sAlpnSelected) ) { return false; }
	if ( !__xrt_tls_alpn_config_contains(pCtx->sAlpnProtocols, pData + 3, iNameLen) ) { return false; }
	memcpy(pCtx->sAlpnSelected, pData + 3, iNameLen);
	pCtx->sAlpnSelected[iNameLen] = '\0';
	return true;
}


// 内部函数：__xrt_tls_resume_identity_type
static uint8 __xrt_tls_resume_identity_type(const xtlsctx* pCtx)
{
	if ( !pCtx || !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) { return __XRT_TLS_RESUME_IDENTITY_NONE; }
	if ( pCtx->bIsEd25519Key ) { return __XRT_TLS_RESUME_IDENTITY_ED25519; }
	if ( pCtx->bIsECPubKey ) { return __XRT_TLS_RESUME_IDENTITY_ECDSA; }
	return __XRT_TLS_RESUME_IDENTITY_RSA;
}


// 内部函数：__xrt_tls12_suite_matches_identity
static bool __xrt_tls12_suite_matches_identity(uint16 iCipherSuite, uint8 iIdentityType)
{
	switch ( iCipherSuite ) {
		case __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256:
			return iIdentityType == __XRT_TLS_RESUME_IDENTITY_ECDSA
				|| iIdentityType == __XRT_TLS_RESUME_IDENTITY_ED25519;
		case __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256:
		case __XRT_TLS12_RSA_AES128_GCM_SHA256:
		case __XRT_TLS12_RSA_AES256_GCM_SHA384:
			return iIdentityType == __XRT_TLS_RESUME_IDENTITY_RSA;
		default:
			return false;
	}
}


// 内部函数：__xrt_tls_resume_fill_identity
static bool __xrt_tls_resume_fill_identity(xtlsctx* pCtx, struct xrt_tls_resume* pOut)
{
	const char* sServerName;

	if ( !pCtx || !pOut ) { return false; }
	pOut->iIdentityType = __xrt_tls_resume_identity_type(pCtx);
	if ( pOut->iIdentityType == __XRT_TLS_RESUME_IDENTITY_NONE || !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) { return false; }

	xrtSHA256((const ptr)pCtx->pCertDer, pCtx->iCertDerLen, pOut->aIdentityHash);
	pOut->iIdentityHashLen = 32;

	sServerName = pCtx->bIsServer ? pCtx->sClientSNI : pCtx->sHostname;
	if ( sServerName && sServerName[0] ) {
		xrtSHA256((const ptr)sServerName, strlen(sServerName), pOut->aServerNameHash);
		pOut->bHasServerName = true;
	} else {
		pOut->bHasServerName = false;
		memset(pOut->aServerNameHash, 0, sizeof(pOut->aServerNameHash));
	}
	return true;
}


// 内部函数：__xrt_tls_resume_matches_identity
static bool __xrt_tls_resume_matches_identity(xtlsctx* pCtx, const struct xrt_tls_resume* pResume)
{
	struct xrt_tls_resume tCurrent;

	if ( !pCtx || !pResume ) { return false; }
	if ( !__xrt_tls_resume_fill_identity(pCtx, &tCurrent) ) { return false; }
	if ( pResume->iIdentityType != tCurrent.iIdentityType ) { return false; }
	if ( pResume->iIdentityHashLen != 32 || tCurrent.iIdentityHashLen != 32 ) { return false; }
	if ( !__xrt_tls_consttime_equal(pResume->aIdentityHash, tCurrent.aIdentityHash, 32) ) { return false; }
	if ( pResume->bHasServerName != tCurrent.bHasServerName ) { return false; }
	if ( pResume->bHasServerName && !__xrt_tls_consttime_equal(pResume->aServerNameHash, tCurrent.aServerNameHash, 32) ) { return false; }
	return __xrt_tls12_suite_matches_identity(pResume->iCipherSuite, tCurrent.iIdentityType);
}


static bool __xrt_tls_parse_alert_record(const uint8* pAlert, size_t iLen, uint8* pLevel, uint8* pDesc)
{
	if ( pAlert == NULL || iLen < 2 ) { return false; }
	if ( pLevel ) { *pLevel = pAlert[0]; }
	if ( pDesc ) { *pDesc = pAlert[1]; }
	return true;
}


// 内部函数：处理对端发来的 TLS alert 记录
static xnet_result __xrt_tls_handle_alert(xtlsctx* pCtx, const uint8* pAlert, size_t iLen)
{
	uint8 iLevel = 0;
	uint8 iDesc = 0;

	// 解析 alert 记录，提取级别和描述符
	if ( !__xrt_tls_parse_alert_record(pAlert, iLen, &iLevel, &iDesc) ) {
		return XRT_NET_ERROR;
	}

	// 保存对端 alert 信息到上下文
	if ( pCtx ) {
		pCtx->iPeerAlertLevel = iLevel;
		pCtx->iPeerAlertDesc = iDesc;
		if ( iDesc == __XRT_TLS_ALERT_CLOSE_NOTIFY ) {
			pCtx->bCloseNotifyReceived = true;
		}
	}

	// close_notify 表示优雅关闭连接，其他 alert 均视为错误
	if ( iDesc == __XRT_TLS_ALERT_CLOSE_NOTIFY ) {
		return XRT_NET_CLOSED;
	}
	return XRT_NET_ERROR;
}


// 内部函数：__xrt_tls_resume_from_ctx
static bool __xrt_tls_resume_from_ctx(xtlsctx* pCtx, struct xrt_tls_resume* pOut)
{
	if ( !pCtx || !pOut ) { return false; }
	if ( !pCtx->bHandshakeDone || !pCtx->bIsTls12 ) { return false; }
	if ( pCtx->iSessionIdLen == 0 || pCtx->iSessionIdLen > sizeof(pOut->aSessionId) ) { return false; }
	memset(pOut, 0, sizeof(*pOut));
	pOut->iVersion = __XRT_TLS_VERSION_1_2;
	pOut->iCipherSuite = pCtx->iCipherSuite;
	pOut->iSessionIdLen = pCtx->iSessionIdLen;
	memcpy(pOut->aSessionId, pCtx->aSessionId, pCtx->iSessionIdLen);
	memcpy(pOut->aMasterSecret, pCtx->aMasterSecret, sizeof(pOut->aMasterSecret));
	if ( pCtx->bIsServer ) {
		if ( !__xrt_tls_resume_fill_identity(pCtx, pOut) ) { return false; }
		if ( !__xrt_tls12_suite_matches_identity(pOut->iCipherSuite, pOut->iIdentityType) ) { return false; }
	}
	return true;
}

// SHA-256("") 预计算
static const uint8 __xrt_tls_zeros_sha256[32] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
	0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
	0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};


// 内部函数：__xrt_tls_hash_bytes
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


// 内部函数：__xrt_tls_sigalg_hash_len
static size_t __xrt_tls_sigalg_hash_len(uint16 iSigAlg)
{
	// TLS 签名算法标识符格式：高字节 = 哈希算法，低字节 = 签名算法
	// 0x0401 = rsa_pkcs1_sha256, 0x0403 = ecdsa_secp256r1_sha256, 0x0804 = rsa_pss_rsae_sha256, 0x0809 = rsa_pss_pss_sha256
	// 0x0501 = rsa_pkcs1_sha384, 0x0503 = ecdsa_secp384r1_sha384, 0x0805 = rsa_pss_rsae_sha384, 0x080a = rsa_pss_pss_sha384
	// 0x0601 = rsa_pkcs1_sha512, 0x0603 = ecdsa_secp521r1_sha512, 0x0806 = rsa_pss_rsae_sha512, 0x080b = rsa_pss_pss_sha512
	// 0x0807 = ed25519（ 纯签名，无哈希预处理 ）
	switch ( iSigAlg ) {
		case 0x0807:
			return 0;
		case 0x0401:
		case 0x0403:
		case 0x0804:
		case 0x0809:
			return 32; // SHA-256
		case 0x0501:
		case 0x0503:
		case 0x0805:
		case 0x080a:
			return 48; // SHA-384
		case 0x0601:
		case 0x0603:
		case 0x0806:
		case 0x080b:
			return 64; // SHA-512
		default:
			// 兜底：按高字节推断哈希长度（ 4→SHA-256, 5→SHA-384, 6→SHA-512, 8→Ed25519 ）
			switch ( (uint8)(iSigAlg >> 8) ) {
				case 8: return 0;
				case 4: return 32;
				case 5: return 48;
				case 6: return 64;
				default: return 0;
			}
	}
}


// 内部函数：__xrt_tls13_get_hash_len
static size_t __xrt_tls13_get_hash_len(uint16 iCipherSuite)
{
	return (iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384) ? 48 : 32;
}


// 内部函数：__xrt_tls13_get_key_len
static size_t __xrt_tls13_get_key_len(uint16 iCipherSuite)
{
	return (iCipherSuite == __XRT_TLS_AES_128_GCM_SHA256) ? 16 : 32;
}


// 内部函数：__xrt_tls13_is_supported_cipher
static bool __xrt_tls13_is_supported_cipher(uint16 iCipherSuite)
{
	return iCipherSuite == __XRT_TLS_AES_128_GCM_SHA256
		|| iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384
		|| iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256;
}


// 内部函数：__xrt_tls12_set_cipher_params
static bool __xrt_tls12_set_cipher_params(xtlsctx *pCtx, uint16 iCipherSuite)
{
	if ( !pCtx ) { return false; }

	pCtx->iCipherSuite = iCipherSuite;
	pCtx->bIsTls12 = true;
	pCtx->bTls12UseChaCha = false;

	// 根据密码套件设置哈希算法和密钥长度
	switch ( iCipherSuite ) {
		case __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256:
		case __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256:
		case __XRT_TLS12_RSA_AES128_GCM_SHA256:
			pCtx->bUseSHA384 = false; // 使用 SHA-256 伪随机函数
			pCtx->iKeyLen = 16; // AES-128 密钥长度
			break;
		case __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384:
		case __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384:
		case __XRT_TLS12_RSA_AES256_GCM_SHA384:
			pCtx->bUseSHA384 = true; // 使用 SHA-384 伪随机函数
			pCtx->iKeyLen = 32; // AES-256 密钥长度
			break;
		case __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256:
		case __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256:
			pCtx->bUseSHA384 = false; // ChaCha20-Poly1305 固定使用 SHA-256
			pCtx->bTls12UseChaCha = true;
			pCtx->iKeyLen = 32; // ChaCha20 密钥长度固定 32 字节
			break;
		default:
			return false;
	}

	// 根据密码套件判断密钥交换方式是否为 ECDHE
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
			pCtx->bIsECDHE = false; // RSA 静态密钥交换
			break;
	}

	return true;
}


// 内部函数：__xrt_tls12_get_iv_len
static size_t __xrt_tls12_get_iv_len(const xtlsctx *pCtx)
{
	// ChaCha20-Poly1305 使用 12 字节显式 IV；AES-GCM 使用 4 字节隐式 IV（ 显式部分在记录头 ）
	return pCtx->bTls12UseChaCha ? 12 : 4;
}


// 内部函数：常数时间内存比较，防止时序侧信道攻击
static bool __xrt_tls_consttime_equal(const uint8 *pA, const uint8 *pB, size_t iLen)
{
	uint8 iDiff = 0;
	size_t i;
	// 逐字节异或并累积差异，不提前退出，保证执行时间恒定
	for ( i = 0; i < iLen; i++ ) { iDiff |= pA[i] ^ pB[i]; }
	return iDiff == 0;
}


// 内部函数：__xrt_tls_validate_leaf_server_cert
static bool __xrt_tls_validate_leaf_server_cert(xtlsctx *pCtx, const struct __xrt_x509_cert *pLeaf)
{
	time_t iNow = time(NULL);
	bool bHostMatched = false;

	// 基本有效性检查：参数非空且证书在有效期内
	if ( !pCtx || !pLeaf || !__xrt_x509_is_time_valid(pLeaf, iNow) ) { return false; }
	// 叶子证书不能是 CA 证书
	if ( pLeaf->bHasBasicConstraints && pLeaf->bIsCA ) { return false; }
	// keyUsage bit 0 = digitalSignature，TLS 服务器证书必须具备
	if ( pLeaf->bHasKeyUsage && (pLeaf->iKeyUsage & (1u << 0)) == 0 ) { return false; }
	// 扩展密钥用途必须包含 serverAuth（ OID 1.3.6.1.5.5.7.3.1 ）
	if ( pLeaf->bHasExtendedKeyUsage && !pLeaf->bHasServerAuth ) { return false; }

	// 主机名匹配验证
	if ( pCtx->sHostname[0] ) {
		// IP 地址必须匹配 subjectAltName 中的 iPAddress。
		if ( pLeaf->bMatchHostIsIp ) {
			if ( pLeaf->bHasIpSan ) {
				bHostMatched = pLeaf->bIpSanMatched;
			}
		// DNS 名称优先匹配 subjectAltName 中的 dNSName。
		} else if ( pLeaf->bHasDnsSan ) {
			bHostMatched = pLeaf->bDnsSanMatched;
		// 最后尝试与 Common Name 匹配
		} else if ( pLeaf->bHasCommonName ) {
			bHostMatched = __xrt_tls_hostname_matches(pLeaf->sCommonName, pCtx->sHostname);
		} else {
			return false;
		}
		if ( !bHostMatched ) { return false; }
	}

	return true;
}


// 内部函数：__xrt_tls_copy_pubkey_from_cert
static bool __xrt_tls_copy_pubkey_from_cert(xtlsctx *pCtx, const struct __xrt_x509_cert *pCert)
{
	if ( !pCtx || !pCert ) { return false; }

	// 重置公钥状态
	pCtx->iPubKeySz = 0;
	pCtx->iPubKeyModSz = 0;

	// EC 公钥：直接复制原始未压缩公钥点
	if ( pCert->bIsECPubKey ) {
		if ( pCert->iECPubSz == 0 || pCert->iECPubSz > sizeof(pCtx->aPubKey) ) { return false; }
		pCtx->bIsECPubKey = true;
		pCtx->bIsEd25519Key = false;
		pCtx->bIsRSAPSSKey = false;
		memcpy(pCtx->aPubKey, pCert->pECPub, pCert->iECPubSz);
		pCtx->iPubKeySz = pCert->iECPubSz;
		return true;
	}

	// Ed25519 公钥：固定 32 字节
	if ( pCert->bIsEd25519Key ) {
		if ( pCert->iEdPubSz != 32 ) { return false; }
		pCtx->bIsECPubKey = false;
		pCtx->bIsEd25519Key = true;
		pCtx->bIsRSAPSSKey = false;
		memcpy(pCtx->aPubKey, pCert->pEdPub, 32);
		pCtx->iPubKeySz = 32;
		return true;
	}

	// RSA 公钥：连续存放 modulus + exponent
	if ( !pCert->pMod || !pCert->pExp || pCert->iModSz + pCert->iExpSz > sizeof(pCtx->aPubKey) ) { return false; }
	pCtx->bIsECPubKey = false;
	pCtx->bIsEd25519Key = false;
	pCtx->bIsRSAPSSKey = pCert->bIsRSAPSSKey;
	memcpy(pCtx->aPubKey, pCert->pMod, pCert->iModSz);
	memcpy(pCtx->aPubKey + pCert->iModSz, pCert->pExp, pCert->iExpSz);
	pCtx->iPubKeyModSz = pCert->iModSz;
	pCtx->iPubKeySz = pCert->iModSz + pCert->iExpSz;
	return true;
}


// 内部函数：解码 TLS pem 证书块
static bool __xrt_tls_decode_pem_cert_block(const char *pStart, const char *pEnd, uint8 **ppDer, size_t *pDerLen)
{
	size_t iSrcLen;
	size_t iB64Len = 0;
	char *pB64;
	uint8 *pDer;

	if ( !pStart || !pEnd || !ppDer || !pDerLen || pEnd <= pStart ) { return false; }
	iSrcLen = (size_t)(pEnd - pStart);

	// 分配临时缓冲区，用于存放去除空白字符后的 Base64 数据
	pB64 = (char*)xrtMalloc(iSrcLen + 5);
	if ( !pB64 ) { return false; }

	// 去除 PEM 体中的换行符和空白字符，得到纯 Base64 字符串
	for ( size_t i = 0; i < iSrcLen; i++ ) {
		if ( pStart[i] != '\r' && pStart[i] != '\n' && pStart[i] != ' ' && pStart[i] != '\t' ) {
			pB64[iB64Len++] = pStart[i];
		}
	}
	// 补齐 Base64 填充字符 '='，使长度为 4 的倍数
	while ( iB64Len % 4 != 0 ) { pB64[iB64Len++] = '='; }
	pB64[iB64Len] = '\0';

	// Base64 解码为 DER 二进制数据
	pDer = (uint8*)xrtBase64Decode((str)pB64, iB64Len, NULL);
	if ( !pDer || pDer == (uint8*)xCore.sNull ) {
		xrtFree(pB64);
		return false;
	}

	// 计算 DER 数据实际长度（ 扣除 Base64 填充字符对应的字节数 ）
	*pDerLen = (iB64Len / 4) * 3;
	if ( iB64Len > 0 && pB64[iB64Len - 1] == '=' ) { (*pDerLen)--; }
	if ( iB64Len > 1 && pB64[iB64Len - 2] == '=' ) { (*pDerLen)--; }
	*ppDer = pDer;
	xrtFree(pB64);
	return true;
}


// 内部函数：__xrt_tls_ca_bundle_next
static bool __xrt_tls_ca_bundle_next(xtlsctx *pCtx, size_t *pOffset, uint8 **ppDer, size_t *pDerLen, bool *pOwned)
{
	const char *pData;
	const char *pBegin;
	const char *pBody;
	const char *pEnd;

	if ( !pCtx || !pOffset || !ppDer || !pDerLen || !pOwned ) { return false; }
	if ( !pCtx->pCaData || pCtx->iCaDataLen == 0 || *pOffset >= pCtx->iCaDataLen ) { return false; }

	pData = (const char*)pCtx->pCaData;

	// PEM 格式：按 "-----BEGIN CERTIFICATE-----" / "-----END CERTIFICATE-----" 逐个提取
	if ( pCtx->iCaDataLen >= 11 && memcmp(pData, "-----BEGIN ", 11) == 0 ) {
		pBegin = strstr(pData + *pOffset, "-----BEGIN CERTIFICATE-----");
		if ( !pBegin ) { return false; }
		// 跳过 BEGIN 行，找到 Base64 体起始位置
		pBody = strchr(pBegin, '\n');
		if ( !pBody ) { return false; }
		pBody++;
		// 查找 END 标记
		pEnd = strstr(pBody, "-----END CERTIFICATE-----");
		if ( !pEnd ) { return false; }
		if ( !__xrt_tls_decode_pem_cert_block(pBody, pEnd, ppDer, pDerLen) ) { return false; }
		*pOwned = true;
		// 推进偏移量到 END 标记之后
		*pOffset = (size_t)((pEnd - pData) + strlen("-----END CERTIFICATE-----"));
		return true;
	}

	// DER 格式：整个 CA 数据作为一个证书，只返回一次
	if ( *pOffset != 0 ) { return false; }
	*ppDer = pCtx->pCaData;
	*pDerLen = pCtx->iCaDataLen;
	*pOwned = false;
	*pOffset = pCtx->iCaDataLen;
	return true;
}


// 内部函数：__xrt_tls_crl_bundle_next
static bool __xrt_tls_crl_bundle_next(xtlsctx *pCtx, size_t *pOffset, uint8 **ppDer, size_t *pDerLen, bool *pOwned)
{
	const char *pData;
	const char *pBegin;
	const char *pBody;
	const char *pEnd;

	if ( !pCtx || !pOffset || !ppDer || !pDerLen || !pOwned ) { return false; }
	if ( !pCtx->pCrlData || pCtx->iCrlDataLen == 0 || *pOffset >= pCtx->iCrlDataLen ) { return false; }

	pData = (const char*)pCtx->pCrlData;

	// PEM 格式：按 "-----BEGIN X509 CRL-----" / "-----END X509 CRL-----" 逐个提取
	if ( pCtx->iCrlDataLen >= 11 && memcmp(pData, "-----BEGIN ", 11) == 0 ) {
		pBegin = strstr(pData + *pOffset, "-----BEGIN X509 CRL-----");
		if ( !pBegin ) { return false; }
		// 跳过 BEGIN 行，找到 Base64 体起始位置
		pBody = strchr(pBegin, '\n');
		if ( !pBody ) { return false; }
		pBody++;
		// 查找 END 标记
		pEnd = strstr(pBody, "-----END X509 CRL-----");
		if ( !pEnd ) { return false; }
		if ( !__xrt_tls_decode_pem_cert_block(pBody, pEnd, ppDer, pDerLen) ) { return false; }
		*pOwned = true;
		// 推进偏移量到 END 标记之后
		*pOffset = (size_t)((pEnd - pData) + strlen("-----END X509 CRL-----"));
		return true;
	}

	// DER 格式：整个 CRL 数据作为一个 CRL，只返回一次
	if ( *pOffset != 0 ) { return false; }
	*ppDer = pCtx->pCrlData;
	*pDerLen = pCtx->iCrlDataLen;
	*pOwned = false;
	*pOffset = pCtx->iCrlDataLen;
	return true;
}


// 内部函数：检查证书是否被 CRL 吊销
static bool __xrt_tls_check_cert_not_revoked(xtlsctx *pCtx, const struct __xrt_x509_cert *pCert,
	const struct __xrt_x509_cert *pIssuer, time_t iNow)
{
	size_t iOffset = 0;
	bool bMatchedIssuer = false;
	bool bValidCRL = false;

	if ( !pCtx || !pCert || !pIssuer ) { return false; }
	// 如果没有配置 CRL 数据，默认不吊销
	if ( !pCtx->pCrlData || pCtx->iCrlDataLen == 0 ) { return true; }

	// 遍历所有 CRL 条目
	while ( 1 ) {
		uint8 *pCrlDer = NULL;
		size_t iCrlLen = 0;
		bool bOwned = false;
		struct __xrt_x509_crl tCrl;

		if ( !__xrt_tls_crl_bundle_next(pCtx, &iOffset, &pCrlDer, &iCrlLen, &bOwned) ) { break; }

		// 检查 CRL 的颁发者是否匹配证书的颁发者
		if ( __xrt_x509_parse_crl(pCrlDer, iCrlLen, &tCrl)
			&& __xrt_x509_name_eq(tCrl.pIssuerRaw, tCrl.iIssuerRawLen, pIssuer->pSubjectRaw, pIssuer->iSubjectRawLen) ) {
			bMatchedIssuer = true;
			// CRL 必须在有效期内且签名验证通过
			if ( __xrt_x509_crl_is_time_valid(&tCrl, iNow) && __xrt_x509_verify_crl_signature(&tCrl, pIssuer) ) {
				bValidCRL = true;
				// 在有效的 CRL 中查找证书序列号，若存在则证书已被吊销
				if ( __xrt_x509_crl_revokes_cert(&tCrl, pCert) ) {
					if ( bOwned ) { xrtFree(pCrlDer); }
					return false;
				}
			}
		}
		if ( bOwned ) { xrtFree(pCrlDer); }
	}

	// 如果找到了匹配颁发者的 CRL 但没有一个是有效的，视为吊销检查失败
	if ( bMatchedIssuer && !bValidCRL ) { return false; }
	return true;
}


// 内部函数：__xrt_tls_verify_presented_chain
static bool __xrt_tls_verify_presented_chain(xtlsctx *pCtx, uint8 **apCertData, size_t *apCertLen, size_t iCertCount)
{
	struct __xrt_x509_cert aCerts[__XRT_TLS_MAX_CERT_CHAIN];
	bool aUsed[__XRT_TLS_MAX_CERT_CHAIN];
	size_t i;
	size_t iCurrent;
	time_t iNow = time(NULL);
	bool bParsed;

	if ( !pCtx || !apCertData || !apCertLen || iCertCount == 0 || iCertCount > __XRT_TLS_MAX_CERT_CHAIN ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS] verify chain: invalid input count=%d\n", (int)iCertCount);
		#endif
		return false;
	}

	// 解析证书链中的每个证书
	for ( i = 0; i < iCertCount; i++ ) {
		bParsed = (i == 0)
			? __xrt_x509_parse_leaf_for_chain(apCertData[i], apCertLen[i], &aCerts[i], pCtx->sHostname)
			: __xrt_x509_parse_for_chain(apCertData[i], apCertLen[i], &aCerts[i]);
		if ( !bParsed ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS] verify chain: parse cert[%d] failed len=%d\n", (int)i, (int)apCertLen[i]);
			#endif
			return false;
		}
		aUsed[i] = false;
	}

	// 验证叶子证书（ 证书链第一个证书 ）：有效性、主机名匹配
	if ( !__xrt_tls_validate_leaf_server_cert(pCtx, &aCerts[0]) ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS] verify chain: leaf validation failed host=%s cn=%s dns=%d\n",
				pCtx->sHostname, aCerts[0].sCommonName, (int)aCerts[0].iDnsNameCount);
		#endif
		return false;
	}
	// 提取叶子证书公钥保存到上下文，用于后续密钥交换验证
	if ( !__xrt_tls_copy_pubkey_from_cert(pCtx, &aCerts[0]) ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS] verify chain: copy leaf public key failed\n");
		#endif
		return false;
	}

	// 自底向上构建证书链：从叶子证书开始，逐个查找颁发者
	iCurrent = 0;
	aUsed[0] = true;
	for ( i = 0; i < iCertCount; i++ ) {
		size_t j;
		bool bFoundIssuer = false;

		// 自签名证书：issuer == subject，链构建结束
		if ( __xrt_x509_name_eq(aCerts[iCurrent].pIssuerRaw, aCerts[iCurrent].iIssuerRawLen,
			aCerts[iCurrent].pSubjectRaw, aCerts[iCurrent].iSubjectRawLen) ) {
			break;
		}

		// 在已提供的证书链中查找颁发者
		for ( j = 0; j < iCertCount; j++ ) {
			if ( j == iCurrent || aUsed[j] ) { continue; }
			if ( __xrt_x509_name_eq(aCerts[iCurrent].pIssuerRaw, aCerts[iCurrent].iIssuerRawLen,
				aCerts[j].pSubjectRaw, aCerts[j].iSubjectRawLen) ) {
				// 验证颁发者的 CA 属性、有效期、签名和 CRL 吊销状态
				if ( !__xrt_x509_is_ca_usable(&aCerts[j]) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] verify chain: issuer cert[%d] not usable as CA\n", (int)j);
					#endif
					return false;
				}
				if ( !__xrt_x509_is_time_valid(&aCerts[j], iNow) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] verify chain: issuer cert[%d] time invalid\n", (int)j);
					#endif
					return false;
				}
				if ( !__xrt_x509_verify_signature(&aCerts[iCurrent], &aCerts[j]) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] verify chain: cert[%d] signature by cert[%d] failed\n", (int)iCurrent, (int)j);
					#endif
					return false;
				}
				if ( !__xrt_tls_check_cert_not_revoked(pCtx, &aCerts[iCurrent], &aCerts[j], iNow) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] verify chain: cert[%d] revocation check failed\n", (int)iCurrent);
					#endif
					return false;
				}
				iCurrent = j;
				aUsed[j] = true;
				bFoundIssuer = true;
				break;
			}
		}
		if ( bFoundIssuer ) { continue; }

		// 链内未找到颁发者，尝试在 CA 信任锚中查找
		{
			size_t iOffset = 0;
			while ( iOffset < pCtx->iCaDataLen ) {
				uint8 *pAnchorDer = NULL;
				size_t iAnchorLen = 0;
				bool bOwned = false;
				struct __xrt_x509_cert tAnchor;
				bool bOK = false;

				if ( !__xrt_tls_ca_bundle_next(pCtx, &iOffset, &pAnchorDer, &iAnchorLen, &bOwned) ) { break; }
				if ( __xrt_x509_parse_for_chain(pAnchorDer, iAnchorLen, &tAnchor) &&
					__xrt_tls_current_cert_trusted_by_anchor(&aCerts[iCurrent], &tAnchor, iNow) &&
					__xrt_tls_check_cert_not_revoked(pCtx, &aCerts[iCurrent], &tAnchor, iNow) ) {
					bOK = true;
				}
				if ( bOwned ) { xrtFree(pAnchorDer); }
				if ( bOK ) { return true; }
			}
		}
		// CA 信任锚中也未找到颁发者，链验证失败
		#ifdef DEBUG_TRACE
			printf("    [TLS] verify chain: issuer not found in chain or CA bundle\n");
		#endif
		return false;
	}

	// 自签名证书的信任锚查找：在 CA 库中验证当前证书是否被信任
	if ( pCtx->iCaDataLen == 0 ) { return false; }
	{
		size_t iOffset = 0;
		while ( iOffset < pCtx->iCaDataLen ) {
			uint8 *pAnchorDer = NULL;
			size_t iAnchorLen = 0;
			bool bOwned = false;
			struct __xrt_x509_cert tAnchor;
			bool bTrusted = false;

			if ( !__xrt_tls_ca_bundle_next(pCtx, &iOffset, &pAnchorDer, &iAnchorLen, &bOwned) ) { break; }
			if ( __xrt_x509_parse_for_chain(pAnchorDer, iAnchorLen, &tAnchor) &&
				__xrt_tls_current_cert_trusted_by_anchor(&aCerts[iCurrent], &tAnchor, iNow) &&
				__xrt_tls_check_cert_not_revoked(pCtx, &aCerts[iCurrent], &tAnchor, iNow) ) {
				bTrusted = true;
			}
			if ( bOwned ) { xrtFree(pAnchorDer); }
			if ( bTrusted ) { return true; }
		}
	}

	#ifdef DEBUG_TRACE
		printf("    [TLS] verify chain: self-signed anchor not trusted\n");
	#endif
	return false;
}


// 内部函数：__xrt_tls_capture_peer_cert_chain
static bool __xrt_tls_store_peer_leaf_cert(xtlsctx *pCtx, const uint8 *pCertData, size_t iCertLen)
{
	uint8* pCopy;

	if ( !pCtx ) { return false; }
	if ( pCtx->pPeerCertDer ) {
		xrtFree(pCtx->pPeerCertDer);
		pCtx->pPeerCertDer = NULL;
		pCtx->iPeerCertDerLen = 0;
	}
	if ( !pCertData || iCertLen == 0 ) { return true; }
	pCopy = (uint8*)xrtMalloc(iCertLen);
	if ( !pCopy ) { return false; }
	memcpy(pCopy, pCertData, iCertLen);
	pCtx->pPeerCertDer = pCopy;
	pCtx->iPeerCertDerLen = iCertLen;
	return true;
}


static bool __xrt_tls_apply_verify_callback(xtlsctx *pCtx, bool bDefaultOK)
{
	if ( !pCtx || !pCtx->OnVerify ) { return bDefaultOK; }
	return pCtx->OnVerify(
		pCtx->pSession,
		pCtx->pPeerCertDer,
		pCtx->iPeerCertDerLen,
		bDefaultOK,
		pCtx->pVerifyUserData
	);
}


static bool __xrt_tls_capture_peer_cert_chain(xtlsctx *pCtx, uint8 **apCertData, size_t *apCertLen, size_t iCertCount)
{
	struct __xrt_x509_cert tLeaf;
	bool bOK;

	if ( !pCtx || !apCertData || !apCertLen || iCertCount == 0 ) { return false; }
	if ( !__xrt_x509_parse(apCertData[0], apCertLen[0], &tLeaf) ) { return false; }
	if ( !__xrt_tls_copy_pubkey_from_cert(pCtx, &tLeaf) ) { return false; }
	if ( !__xrt_tls_store_peer_leaf_cert(pCtx, apCertData[0], apCertLen[0]) ) { return false; }

	// 跳过验证模式：仍允许应用层回调做 pinning 或拒绝证书
	if ( pCtx->bSkipVerify ) { return __xrt_tls_apply_verify_callback(pCtx, true); }
	// 必须有 CA 数据才能进行完整证书链验证；应用层回调可以接管最终策略
	if ( pCtx->iCaDataLen == 0 ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS] certificate: CA bundle empty\n");
		#endif
		return __xrt_tls_apply_verify_callback(pCtx, false);
	}
	bOK = __xrt_tls_verify_presented_chain(pCtx, apCertData, apCertLen, iCertCount);
	return __xrt_tls_apply_verify_callback(pCtx, bOK);
}


// 内部函数：复制 TLS load 文件
static bool __xrt_tls_load_file_copy(const char *sFile, uint8 **ppData, size_t *pLen)
{
	size_t iFileSize = 0;
	uint8 *pFileData;
	uint8 *pCopy;

	if ( !sFile || !sFile[0] || !ppData || !pLen ) { return false; }
	// 读取文件全部内容到临时缓冲区
	pFileData = (uint8*)xrtFileGetAll((str)sFile, &iFileSize);
	if ( !pFileData || pFileData == (uint8*)xCore.sNull ) { return false; }

	// 复制一份独立内存，因为 xrtFileGetAll 返回的缓冲区可能被内部复用
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


// 内部函数：加载 TLS der 文件
static bool __xrt_tls_load_der_file(const char *sFile, uint8 **ppDer, size_t *pDerLen)
{
	size_t iFileSize = 0;
	uint8 *pFileData;

	if ( !sFile || !sFile[0] || !ppDer || !pDerLen ) { return false; }
	*ppDer = NULL;
	*pDerLen = 0;

	// 读取文件全部内容
	pFileData = (uint8*)xrtFileGetAll((str)sFile, &iFileSize);
	if ( !pFileData || pFileData == (uint8*)xCore.sNull ) { return false; }

	// 判断文件内容是否为 PEM 格式
	if ( iFileSize > 27 && memcmp(pFileData, "-----BEGIN ", 11) == 0 ) {
		const char *pStart = strstr((char*)pFileData, "\n");
		const char *pEnd;
		bool bOK;

		if ( !pStart ) {
			xrtFree(pFileData);
			return false;
		}
		pStart++;
		// 查找 PEM 结束标记
		pEnd = strstr(pStart, "-----END ");
		if ( !pEnd ) {
			xrtFree(pFileData);
			return false;
		}

		// PEM 转 DER
		bOK = __xrt_tls_decode_pem_cert_block(pStart, pEnd, ppDer, pDerLen);
		xrtFree(pFileData);
		return bOK && *ppDer && *pDerLen > 0;
	}

	// DER 格式：直接复制文件内容
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


// 内部函数：在内存块中查找 TLS 文本标记
static const uint8* __xrt_tls_find_mem_token(const uint8* pData, size_t iDataLen, const char* sToken)
{
	size_t iTokenLen;
	size_t i;

	if ( pData == NULL || sToken == NULL ) {
		return NULL;
	}
	iTokenLen = strlen(sToken);
	if ( iTokenLen == 0 || iDataLen < iTokenLen ) {
		return NULL;
	}
	for ( i = 0; i + iTokenLen <= iDataLen; i++ ) {
		if ( memcmp(pData + i, sToken, iTokenLen) == 0 ) {
			return pData + i;
		}
	}

	return NULL;
}


// 内部函数：把内存中的证书/私钥数据归一化为 DER
static bool __xrt_tls_copy_der_data(const void* pData, size_t iLen, uint8** ppDer, size_t* pDerLen)
{
	const uint8* pBytes;
	const uint8* pBody;
	const uint8* pEnd;
	uint8* pCopy;
	size_t iOffset;
	size_t i;

	if ( ppDer == NULL || pDerLen == NULL ) {
		return false;
	}
	*ppDer = NULL;
	*pDerLen = 0;
	if ( pData == NULL || iLen == 0 ) {
		return true;
	}

	pBytes = (const uint8*)pData;

	// 跳过数据开头的空白字符
	iOffset = 0;
	while ( iOffset < iLen ) {
		if ( pBytes[iOffset] != ' ' && pBytes[iOffset] != '\t' && pBytes[iOffset] != '\r' && pBytes[iOffset] != '\n' ) {
			break;
		}
		iOffset++;
	}

	// 检测是否为 PEM 格式（ 以 "-----BEGIN " 开头 ）
	if ( iOffset + 27 <= iLen && memcmp(pBytes + iOffset, "-----BEGIN ", 11) == 0 ) {
		// 跳过 BEGIN 行，定位到 Base64 体
		pBody = NULL;
		for ( i = iOffset; i < iLen; i++ ) {
			if ( pBytes[i] == '\n' ) {
				pBody = pBytes + i + 1;
				break;
			}
		}
		if ( pBody == NULL ) {
			return false;
		}

		// 查找 END 标记
		pEnd = __xrt_tls_find_mem_token(pBody, iLen - (size_t)(pBody - pBytes), "-----END ");
		if ( pEnd == NULL || pEnd <= pBody ) {
			return false;
		}

		// PEM 解码为 DER
		return __xrt_tls_decode_pem_cert_block((const char*)pBody, (const char*)pEnd, ppDer, pDerLen)
			&& *ppDer != NULL && *pDerLen > 0;
	}

	// DER 格式：直接复制原始数据
	pCopy = (uint8*)xrtMalloc(iLen);
	if ( pCopy == NULL ) {
		return false;
	}
	memcpy(pCopy, pData, iLen);
	*ppDer = pCopy;
	*pDerLen = iLen;
	return true;
}


// 内部函数：追加 TLS pem 证书
static bool UNUSED_ATTR __xrt_tls_append_pem_cert(__xrt_tls_buf* pBuf, const uint8* pDer, size_t iDerLen)
{
	static const char sBegin[] = "-----BEGIN CERTIFICATE-----\n";
	static const char sEnd[] = "-----END CERTIFICATE-----\n";
	str sBase64;
	size_t iBase64Len;
	size_t iOffset;

	if ( !pBuf || !pDer || iDerLen == 0 ) { return false; }

	// DER 转 Base64
	sBase64 = xrtBase64Encode((ptr)pDer, iDerLen, NULL);
	if ( !sBase64 || sBase64 == xCore.sNull ) { return false; }

	iBase64Len = strlen(__xrt_cstr(sBase64));

	// 写入 PEM 头部标记
	if ( !__xrt_tls_buf_append(pBuf, sBegin, sizeof(sBegin) - 1) ) {
		xrtFree(sBase64);
		return false;
	}

	// 将 Base64 字符串按每行 64 字符分段写入
	for ( iOffset = 0; iOffset < iBase64Len; iOffset += 64 ) {
		size_t iChunk = iBase64Len - iOffset;
		if ( iChunk > 64 ) { iChunk = 64; }
		if ( !__xrt_tls_buf_append(pBuf, __xrt_cstr(sBase64 + iOffset), iChunk)
			|| !__xrt_tls_buf_append(pBuf, "\n", 1) ) {
			xrtFree(sBase64);
			return false;
		}
	}

	xrtFree(sBase64);
	// 写入 PEM 尾部标记
	return __xrt_tls_buf_append(pBuf, sEnd, sizeof(sEnd) - 1);
}

#if defined(_WIN32) || defined(_WIN64)
// 内部函数：__xrt_tls_load_windows_root_store
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

	if ( !pCtx ) { return false; }

	// 延迟加载 crypt32.dll，避免静态依赖
	if ( !bCrypt32Loaded ) {
		HMODULE hLib = LoadLibraryA("crypt32.dll");
		if ( hLib ) {
			FARPROC pCertOpenStore = GetProcAddress(hLib, "CertOpenStore");
			FARPROC pCertEnumCertificatesInStore = GetProcAddress(hLib, "CertEnumCertificatesInStore");
			FARPROC pCertCloseStore = GetProcAddress(hLib, "CertCloseStore");
			// 通过 memcpy 赋值避免指针类型转换导致的严格别名问题
			memcpy(&procCertOpenStore, &pCertOpenStore, sizeof(procCertOpenStore));
			memcpy(&procCertEnumCertificatesInStore, &pCertEnumCertificatesInStore, sizeof(procCertEnumCertificatesInStore));
			memcpy(&procCertCloseStore, &pCertCloseStore, sizeof(procCertCloseStore));
		}
		bCrypt32Loaded = true;
	}

	if ( !procCertOpenStore || !procCertEnumCertificatesInStore || !procCertCloseStore ) {
		return false;
	}

	// 初始化 PEM 输出缓冲区
	if ( !__xrt_tls_buf_init(&tPemBuf, 4096) ) {
		return false;
	}

	// 遍历 Current User 和 Local Machine 两个 ROOT 证书存储
	for ( i = 0; i < sizeof(arrStoreFlags) / sizeof(arrStoreFlags[0]); i++ ) {
		hStore = procCertOpenStore(
			CERT_STORE_PROV_SYSTEM_A,
			0,
			0,
			CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG | arrStoreFlags[i],
			"ROOT");
		if ( !hStore ) { continue; }

		// 枚举存储中的每个证书，转换为 PEM 格式追加到缓冲区
		pCert = NULL;
		while ( (pCert = procCertEnumCertificatesInStore(hStore, pCert)) != NULL ) {
			if ( !pCert->pbCertEncoded || pCert->cbCertEncoded == 0 ) { continue; }
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

	// 至少加载了一个证书才算成功
	if ( !bAnyLoaded || !__xrt_tls_buf_append(&tPemBuf, "\0", 1) ) {
		__xrt_tls_buf_free(&tPemBuf);
		return false;
	}

	// 将 PEM 缓冲区转移给上下文，注意减去末尾的 '\0'
	pCtx->pCaData = (uint8*)tPemBuf.pBase;
	pCtx->iCaDataLen = tPemBuf.iSize - 1;
	tPemBuf.pBase = NULL;
	tPemBuf.pData = NULL;
	tPemBuf.iSize = 0;
	tPemBuf.iCapacity = 0;
	return true;
}

#endif

// 内部函数：__xrt_tls_load_ca_bundle
static bool __xrt_tls_load_ca_bundle(xtlsctx *pCtx, const char *sCaFile)
{
	const char *sEnvPath = NULL;

	if ( !pCtx ) { return false; }
	if ( pCtx->pCaData ) {
		xrtFree(pCtx->pCaData);
		pCtx->pCaData = NULL;
		pCtx->iCaDataLen = 0;
	}

	// 如果指定了 CA 文件路径，直接加载
	if ( sCaFile && sCaFile[0] ) {
		return __xrt_tls_load_file_copy(sCaFile, &pCtx->pCaData, &pCtx->iCaDataLen);
	}

	// 依次尝试 SSL_CERT_FILE、CURL_CA_BUNDLE、REQUESTS_CA_BUNDLE 环境变量
	sEnvPath = getenv("SSL_CERT_FILE");
	if ( (!sEnvPath || !sEnvPath[0]) ) { sEnvPath = getenv("CURL_CA_BUNDLE"); }
	if ( (!sEnvPath || !sEnvPath[0]) ) { sEnvPath = getenv("REQUESTS_CA_BUNDLE"); }
	if ( sEnvPath && sEnvPath[0] && __xrt_tls_load_file_copy(sEnvPath, &pCtx->pCaData, &pCtx->iCaDataLen) ) {
		return true;
	}

	// 平台特定的 CA 证书加载
	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrt_tls_load_windows_root_store(pCtx) ) {
			return true;
		}
	#else
		static const char *aDefaultPaths[] = {
			"/etc/ssl/certs/ca-certificates.crt",
			"/etc/pki/tls/certs/ca-bundle.crt",
			"/etc/ssl/cert.pem",
			"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem"
		};
		size_t i;
		for ( i = 0; i < sizeof(aDefaultPaths) / sizeof(aDefaultPaths[0]); i++ ) {
			if ( xrtFileExists((str)aDefaultPaths[i]) &&
				__xrt_tls_load_file_copy(aDefaultPaths[i], &pCtx->pCaData, &pCtx->iCaDataLen) ) {
				return true;
			}
		}
	#endif
	return false;
}


// 内部函数：__xrt_tls_load_crl_bundle
static bool __xrt_tls_load_crl_bundle(xtlsctx *pCtx, const char *sCrlFile)
{
	if ( !pCtx ) { return false; }
	if ( pCtx->pCrlData ) {
		xrtFree(pCtx->pCrlData);
		pCtx->pCrlData = NULL;
		pCtx->iCrlDataLen = 0;
	}
	if ( sCrlFile && sCrlFile[0] ) {
		return __xrt_tls_load_file_copy(sCrlFile, &pCtx->pCrlData, &pCtx->iCrlDataLen);
	}
	return true;
}


// 内部函数：更新 TLS 13 hash
static void __xrt_tls13_hash_update(xtlsctx *pCtx, const uint8 *pData, size_t iLen)
{
	xrtSHA256Update(&pCtx->tSHA256, (ptr)pData, iLen);
	xrtSHA512Update(&pCtx->tSHA384, (const ptr)pData, iLen);
}


// 内部函数：__xrt_tls13_get_transcript_hash
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
	if ( pHashLen ) { *pHashLen = iHashLen; }
}


// 内部函数：__xrt_tls13_hkdf_extract
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


// 内部函数：__xrt_tls13_hkdf_expand
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


// 内部函数：TLS 13 HMAC相关处理
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


// 内部函数：__xrt_tls_xor_seq_nonce
static void __xrt_tls_xor_seq_nonce(uint8 *pNonce, const uint8 *pIV, uint64 iSeq)
{
	// 序列号填充为 12 字节：前 4 字节为 0，后 8 字节为大端序序列号
	uint8 aSeq[12] = {0};
	memcpy(pNonce, pIV, 12);  // 复制 IV 作为 nonce 初始值
	__xrt_tls_store_be64(aSeq + 4, iSeq);  // 序列号存入后 8 字节
	// 逐字节异或：nonce = IV XOR padded(seq_num)
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
	if ( iFullLen > sizeof(aFullSeed) ) {
		memset(pOut, 0, iOutLen);
		return;
	}
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
		if ( iCopy > 32 ) { iCopy = 32; }
		memcpy(pOut + iDone, aP, iCopy);
		iDone += iCopy;
		
		// A(i+1) = HMAC(secret, A(i))
		xrtHMAC_SHA256(pSecret, iSecretLen, aA, 32, aA);
	}
}


// 内部函数：__xrt_tls12_prf_sha384
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
	if ( iFullLen > sizeof(aFullSeed) ) {
		memset(pOut, 0, iOutLen);
		return;
	}
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
		if ( iCopy > 48 ) { iCopy = 48; }
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
	
	// 派生 write IV （ 始终 12 字节 ）
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerWriteIV : pEnc->aClientWriteIV,
		12, pSecret, iHashLen, "iv", 2, NULL, 0, iHashLen);
	
	// 派生 finished key （ 长度等于握手哈希长度 ）
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerFinishedKey : pEnc->aClientFinishedKey,
		iHashLen, pSecret, iHashLen, "finished", 8, NULL, 0, iHashLen);
}



/* ============================== TLS 记录层 ============================== */

// 加密并发送一条 TLS 记录 （ 栈缓冲区优化, 消除 heap 分配 ）
static bool __xrt_tls_encrypt_record(xtlsctx *pCtx, uint8 iType,
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
	if ( *pSeq == UINT64_MAX ) { return false; }
	__xrt_tls_xor_seq_nonce(aNonce, pIV, *pSeq);
	
	// 内层 plaintext: data + content_type （ 栈缓冲区, 消除 malloc ）
	size_t iInnerLen = iLen + 1;
	uint8 aInnerStack[__XRT_TLS_MAX_RECORD + 1];
	uint8 *pInner = (iInnerLen <= sizeof(aInnerStack)) ? aInnerStack : (uint8*)xrtMalloc(iInnerLen);
	if ( !pInner ) { return false; }
	memcpy(pInner, pData, iLen);
	pInner[iLen] = iType;
	
	// 记录头 (TLS_APP_DATA 0x17, version 0x0303, length)
	size_t iCipherLen = iInnerLen + 16;  // plaintext + AEAD tag
	if ( iCipherLen > 0xffffu ) {
		if ( pInner != aInnerStack ) { xrtFree(pInner); }
		return false;
	}
	uint8 aHdr[5];
	aHdr[0] = __XRT_TLS_APP_DATA;
	__xrt_tls_store_be16(aHdr + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aHdr + 3, (uint16)iCipherLen);
	
	// AEAD 加密 （ 栈缓冲区, 消除 malloc ）
	uint8 aCipherStack[__XRT_TLS_MAX_RECORD + 17];
	uint8 *pCipher = (iCipherLen <= sizeof(aCipherStack)) ? aCipherStack : (uint8*)xrtMalloc(iCipherLen);
	if ( !pCipher ) {
		if ( pInner != aInnerStack ) { xrtFree(pInner); }
		return false;
	}
	
	if ( pCtx->iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) {
		xrtChaCha20Poly1305Encrypt(pCipher, pKey, aNonce, aHdr, 5, pInner, iInnerLen);
	} else if ( pCtx->iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384 ) {
		xrtAES256GCMEncrypt(pCipher, pKey, aNonce, 12, aHdr, 5, pInner, iInnerLen);
	} else {
		xrtAES128GCMEncrypt(pCipher, pKey, aNonce, 12, aHdr, 5, pInner, iInnerLen);
	}
	
	// 写入发送缓冲区: 预留容量 + 直接写入 （ 消除多次 Append ）
	size_t iTotalLen = 5 + iCipherLen;
	if ( !__xrt_tls_buf_ensure(&pCtx->tSendBuf, iTotalLen) ) {
		if ( pCipher != aCipherStack ) { xrtFree(pCipher); }
		if ( pInner != aInnerStack ) { xrtFree(pInner); }
		return false;
	}
	memcpy(pCtx->tSendBuf.pData + pCtx->tSendBuf.iSize, aHdr, 5);
	memcpy(pCtx->tSendBuf.pData + pCtx->tSendBuf.iSize + 5, pCipher, iCipherLen);
	pCtx->tSendBuf.iSize += iTotalLen;
	
	if ( pCipher != aCipherStack ) { xrtFree(pCipher); }
	if ( pInner != aInnerStack ) { xrtFree(pInner); }
	
	(*pSeq)++;
	return true;
}

// 解密一条 TLS 记录
static bool __xrt_tls_decrypt_record(xtlsctx *pCtx, const uint8 *pRecord,
	size_t iRecordLen, uint8 *pOut, size_t *pOutLen, uint8 *pType,
	bool bUseServerKeys)
{
	if ( iRecordLen < __XRT_TLS_RECHDR_SIZE + 16 ) { return false; }
	
	// 根据握手状态选择加密密钥集
	struct __xrt_tls_enc *pEnc = (pCtx->bHandshakeDone) ? &pCtx->tAppKeys : &pCtx->tEnc;
	
	uint8 *pKey, *pIV;
	uint64 *pSeq;
	
	// 选择对应方向的密钥、IV 和序列号
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
	if ( *pSeq == UINT64_MAX ) { return false; }
	__xrt_tls_xor_seq_nonce(aNonce, pIV, *pSeq);
	
	// 读取密文长度并校验有效性
	uint16 iCipherLen = __xrt_tls_load_be16(pRecord + 3);
	if ( !__xrt_tls_record_payload_valid(iCipherLen)
		|| (size_t)iCipherLen != iRecordLen - __XRT_TLS_RECHDR_SIZE
		|| iCipherLen < 17u ) {
		return false;
	}
	const uint8 *pCipher = pRecord + __XRT_TLS_RECHDR_SIZE;
	
	// AAD = 记录头
	uint8 aAAD[5];
	memcpy(aAAD, pRecord, 5);
	
	// 根据密码套件选择 AEAD 解密算法
	bool bOK;
	if ( pCtx->iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) {
		bOK = xrtChaCha20Poly1305Decrypt(pOut, pKey, aNonce, aAAD, 5, pCipher, iCipherLen);
	} else if ( pCtx->iCipherSuite == __XRT_TLS_AES_256_GCM_SHA384 ) {
		bOK = xrtAES256GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 5, pCipher, iCipherLen);
	} else {
		bOK = xrtAES128GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 5, pCipher, iCipherLen);
	}
	
	if ( !bOK ) { return false; }
	
	// 去除填充，找到 content type（从末尾向前找非零字节）
	size_t iPlainLen = iCipherLen - 16;
	while ( iPlainLen > 0 && pOut[iPlainLen - 1] == 0 ) {
		iPlainLen--;
	}
	if ( iPlainLen == 0 ) { return false; }
	
	*pType = pOut[iPlainLen - 1];
	*pOutLen = iPlainLen - 1;
	
	(*pSeq)++;
	return true;
}

// TLS 1.2 AEAD 加密记录
static bool __xrt_tls12_encrypt_record(xtlsctx *pCtx, uint8 iType,
	const uint8 *pData, size_t iLen)
{
	uint8 aNonce[12];
	// 根据自身角色选择发送密钥、IV 和序列号
	uint8 *pWriteIV = pCtx->bIsServer ? pCtx->aServerWriteIV12 : pCtx->aClientWriteIV12;
	uint8 *pKey = pCtx->bIsServer ? pCtx->aServerWriteKey12 : pCtx->aClientWriteKey12;
	uint64 *pSeq = pCtx->bIsServer ? &pCtx->iServerSeq12 : &pCtx->iClientSeq12;
	// ChaCha20 无显式 nonce，GCM 使用 8 字节显式 nonce
	size_t iExplicitNonceLen = pCtx->bTls12UseChaCha ? 0 : 8;
	// payload = 显式 nonce(可选) + 密文 + AEAD tag(16)
	size_t iPayloadLen = iExplicitNonceLen + iLen + 16;

	if ( *pSeq == UINT64_MAX || iPayloadLen > 0xffffu ) { return false; }
	// 构造 nonce：ChaCha20 用 XOR 模式，GCM 用 implicit+explicit nonce 模式
	if ( pCtx->bTls12UseChaCha ) {
		__xrt_tls_xor_seq_nonce(aNonce, pWriteIV, *pSeq);
	} else {
		memcpy(aNonce, pWriteIV, 4);  // 前 4 字节为 implicit nonce（ 写入 IV ）
		__xrt_tls_store_be64(aNonce + 4, *pSeq);  // 后 8 字节为 explicit nonce（ 序列号 ）
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
	__xrt_tls_store_be16(aHdr + 3, (uint16)iPayloadLen);
	
	// 加密 （ 栈缓冲区, 消除 malloc ）
	uint8 aCipherStack[__XRT_TLS_MAX_RECORD + 16];
	uint8 *pCipher = (iLen + 16 <= sizeof(aCipherStack)) ? aCipherStack : (uint8*)xrtMalloc(iLen + 16);
	if ( !pCipher ) { return false; }
	
	#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
		printf("    [TLS12] Encrypt: type=%d len=%d seq=%d keyLen=%d\n", iType, (int)iLen, *pSeq, pCtx->iKeyLen);
		printf("    [TLS12]   Key: ");
		for (int i = 0; i < (int)pCtx->iKeyLen; i++) { printf("%02x", pKey[i]); }
		printf("\n    [TLS12]   Nonce: ");
		for (int i = 0; i < 12; i++) { printf("%02x", aNonce[i]); }
		printf("\n    [TLS12]   AAD: ");
		for (int i = 0; i < 13; i++) { printf("%02x", aAAD[i]); }
		printf("\n    [TLS12]   Plaintext: ");
		for (int i = 0; i < (int)(iLen < 32 ? iLen : 32); i++) { printf("%02x", pData[i]); }
		printf("\n");
	#endif
	
	// 根据密码套件执行 AEAD 加密
	if ( pCtx->bTls12UseChaCha ) {
		xrtChaCha20Poly1305Encrypt(pCipher, pKey, aNonce, aAAD, 13, pData, iLen);
	} else if ( pCtx->iKeyLen == 32 ) {
		xrtAES256GCMEncrypt(pCipher, pKey, aNonce, 12, aAAD, 13, pData, iLen);
	} else {
		xrtAES128GCMEncrypt(pCipher, pKey, aNonce, 12, aAAD, 13, pData, iLen);
	}
	
	// 写入发送缓冲区: 预留容量 + 直接写入 （ 消除多次 Append ）
	size_t iTotalLen = 5 + iExplicitNonceLen + iLen + 16;
	if ( !__xrt_tls_buf_ensure(&pCtx->tSendBuf, iTotalLen) ) {
		if ( pCipher != aCipherStack ) { xrtFree(pCipher); }
		return false;
	}
	{
		char *pDst = pCtx->tSendBuf.pData + pCtx->tSendBuf.iSize;
		memcpy(pDst, aHdr, 5);
		if ( iExplicitNonceLen > 0 ) {
			__xrt_tls_store_be64((uint8*)pDst + 5, *pSeq);
		}
		memcpy(pDst + 5 + iExplicitNonceLen, pCipher, iLen + 16);
		pCtx->tSendBuf.iSize += iTotalLen;
	}
	
	#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
		printf("    [TLS12]   Ciphertext+Tag: ");
		for (int i = 0; i < (int)(iLen + 16); i++) { printf("%02x", pCipher[i]); }
		printf("\n    [TLS12]   RecHdr: ");
		for (int i = 0; i < 5; i++) { printf("%02x", aHdr[i]); }
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
	
	if ( pCipher != aCipherStack ) { xrtFree(pCipher); }
	(*pSeq)++;
	return true;
}

// TLS 1.2 AEAD 解密记录
// pRecord 指向完整记录 (hdr + payload), pOut 接收明文
static bool __xrt_tls12_decrypt_record(xtlsctx *pCtx, const uint8 *pRecord,
	size_t iRecordLen, uint8 *pOut, size_t *pOutLen, uint8 *pType)
{
	// ChaCha20 无显式 nonce，GCM 使用 8 字节显式 nonce
	size_t iExplicitNonceLen = pCtx->bTls12UseChaCha ? 0 : 8;
	if ( iRecordLen < __XRT_TLS_RECHDR_SIZE + iExplicitNonceLen + 16 ) { return false; }

	// 读取方向：使用对端的写入密钥来解密（ 服务器用客户端密钥，反之亦然 ）
	uint8 *pReadIV = pCtx->bIsServer ? pCtx->aClientWriteIV12 : pCtx->aServerWriteIV12;
	uint8 *pKey = pCtx->bIsServer ? pCtx->aClientWriteKey12 : pCtx->aServerWriteKey12;
	uint64 *pSeq = pCtx->bIsServer ? &pCtx->iClientSeq12 : &pCtx->iServerSeq12;
	
	// 校验密文长度有效性
	uint16 iPayloadLen = __xrt_tls_load_be16(pRecord + 3);
	if ( *pSeq == UINT64_MAX || iPayloadLen < iExplicitNonceLen + 16 ) { return false; }
	if ( !__xrt_tls_record_payload_valid(iPayloadLen)
		|| (size_t)iPayloadLen != iRecordLen - __XRT_TLS_RECHDR_SIZE ) return false;
	
	// 计算 payload 指针和实际密文/明文长度
	const uint8 *pPayload = pRecord + __XRT_TLS_RECHDR_SIZE;
	size_t iCipherLen = iPayloadLen - iExplicitNonceLen;  // 密文 + AEAD tag
	size_t iPlainLen = iCipherLen - 16;  // 去掉 16 字节 AEAD tag
	
	uint8 aNonce[12];
	const uint8 *pCipher = pPayload + iExplicitNonceLen;
	// 构造 nonce：ChaCha20 用 XOR 模式，GCM 用 implicit+explicit nonce 模式
	if ( pCtx->bTls12UseChaCha ) {
		__xrt_tls_xor_seq_nonce(aNonce, pReadIV, *pSeq);
	} else {
		memcpy(aNonce, pReadIV, 4);  // 前 4 字节 implicit nonce
		memcpy(aNonce + 4, pPayload, 8);  // 后 8 字节 explicit nonce 从记录中读取
	}
	
	// 13 字节 AAD
	uint8 aAAD[13];
	__xrt_tls_store_be64(aAAD, *pSeq);
	aAAD[8] = pRecord[0];  // content type
	memcpy(aAAD + 9, pRecord + 1, 2);  // version
	__xrt_tls_store_be16(aAAD + 11, (uint16)iPlainLen);
	
	// 根据密码套件执行 AEAD 解密
	bool bOK;
	if ( pCtx->bTls12UseChaCha ) {
		bOK = xrtChaCha20Poly1305Decrypt(pOut, pKey, aNonce, aAAD, 13, pCipher, iCipherLen);
	} else if ( pCtx->iKeyLen == 32 ) {
		bOK = xrtAES256GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 13, pCipher, iCipherLen);
	} else {
		bOK = xrtAES128GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 13, pCipher, iCipherLen);
	}
	
	if ( !bOK ) { return false; }
	
	*pOutLen = iPlainLen;
	*pType = pRecord[0];  // TLS 1.2: content type 在记录头明文中
	(*pSeq)++;  // 递增接收序列号
	return true;
}



/* ============================== 握手消息构造 ============================== */

// 前向声明: TLS 1.2 握手哈希更新 （ 定义在 TLS 1.2 握手函数区 ）
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
		for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aRandom[i]); }
		printf("\n");
	#endif
	
	// Handshake: ClientHello (type=1)
	aBuf[iPos++] = __XRT_TLS_CLIENT_HELLO;
	size_t iLenPos = iPos;
	iPos += 3;  // 预留长度
	
	// client version = 0x0303 （ TLS 1.2 兼容 ）
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	
	// client random (32 bytes)
	memcpy(aBuf + iPos, pCtx->aRandom, 32);
	iPos += 32;
	
	// session ID
	aBuf[iPos++] = pCtx->iSessionIdLen;
	memcpy(aBuf + iPos, pCtx->aSessionId, pCtx->iSessionIdLen);
	iPos += pCtx->iSessionIdLen;
	
	// cipher suites （ TLS 1.3 + TLS 1.2 现代 AEAD 套件, 9 套件 ）
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
	
	if ( bAllowTls13 ) {
	// Extension: supported_versions (0x002b) — TLS 1.3 preferred, TLS 1.2 fallback
	__xrt_tls_store_be16(aBuf + iPos, 0x002b);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 5);  // ext data length
	iPos += 2;
	aBuf[iPos++] = 4;   // version list length
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_3);
		iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	}
	
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
	__xrt_tls_store_be16(aBuf + iPos, 0x0804);  // RSA-PSS-RSAE-SHA256 （ TLS 1.3 必须 ）
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
	__xrt_tls_store_be16(aBuf + iPos, 0x0401);  // RSA-PKCS1-SHA256 （ TLS 1.2 兼容 ）
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0501);  // RSA-PKCS1-SHA384 （ TLS 1.2 兼容 ）
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0601);  // RSA-PKCS1-SHA512 （ TLS 1.2 兼容 ）
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
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_EXT_SERVER_NAME);  // SNI extension
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

	if ( pCtx->sAlpnProtocols[0] != '\0'
		&& !__xrt_tls_alpn_write_extension(aBuf, &iPos, sizeof(aBuf), pCtx->sAlpnProtocols) ) {
		pCtx->sAlpnProtocols[0] = '\0';
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
		for ( size_t _i = 0; _i < 5; _i++ ) { printf("%02x ", aRec[_i]); }
		printf("\n");
		printf("    [TLS] Full ClientHello hex:\n    ");
		for ( size_t _i = 0; _i < iPos; _i++ ) {
			printf("%02x ", aBuf[_i]);
			if ( (_i + 1) % 32 == 0 ) { printf("\n    "); }
		}
		printf("\n");
	#endif
}

// 解析 ServerHello 消息 （ 支持 TLS 1.3 和 TLS 1.2 ）
static bool __xrt_tls_parse_server_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	// TLS 1.3 降级保护标记（ RFC 8446 Section 4.1.3 ）：服务器在 random 末尾嵌入此值表示支持 TLS 1.3 但协商了更低版本
	static const uint8 aTls13DowngradeSentinel[8] = {
		0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x01  // "DOWNGRD\x01"
	};
	bool bAllowTls13 = (pCtx->iMaxVersion == 0 || pCtx->iMaxVersion >= __XRT_TLS_VERSION_1_3);

	// 校验最小长度: version(2) + random(32) + session_id_len(1)
	if ( iLen < 2 + 32 + 1 ) { return false; }
	
	size_t iPos = 0;
	bool bTls12ResumeAccepted = false;
	pCtx->sAlpnSelected[0] = '\0';
	
	// legacy server version
	uint16 iLegacyVer = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	// server random
	memcpy(pCtx->aServerRandom, pMsg + iPos, 32);
	iPos += 32;
	
	// session ID
	uint8 iSessLen = pMsg[iPos++];
	if ( iSessLen > sizeof(pCtx->aSessionId) || iPos + iSessLen > iLen ) { return false; }
	pCtx->iSessionIdLen = iSessLen;
	if ( iSessLen > 0 ) { memcpy(pCtx->aSessionId, pMsg + iPos, iSessLen); }
	iPos += iSessLen;
	
	// cipher suite
	if ( iPos + 2 > iLen ) { return false; }
	pCtx->iCipherSuite = __xrt_tls_load_be16(pMsg + iPos);
	if ( !__xrt_tls_client_offers_suite(pCtx->iCipherSuite, bAllowTls13) ) { return false; }
	iPos += 2;
	
	// compression
	if ( iPos + 1 > iLen ) { return false; }
	if ( pMsg[iPos] != 0 ) { return false; }
	iPos += 1;
	
	// 解析扩展
	bool bFoundTls13 = false;
	if ( iPos + 2 <= iLen ) {
		uint16 iExtLen = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		
		size_t iExtEnd = iPos + iExtLen;
		if ( iExtEnd > iLen ) { return false; }
		while ( iPos + 4 <= iExtEnd ) {
			uint16 iExtType = __xrt_tls_load_be16(pMsg + iPos);
			iPos += 2;
			uint16 iExtDataLen = __xrt_tls_load_be16(pMsg + iPos);
			iPos += 2;
			if ( iPos + iExtDataLen > iExtEnd ) { return false; }
			
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
					uint16 iGroup = __xrt_tls_load_be16(pMsg + iPos);  // 服务器选择的曲线组
					uint16 iKeyLen = __xrt_tls_load_be16(pMsg + iPos + 2);  // 公钥长度
					if ( iGroup == 0x001d && iKeyLen == 32 && iExtDataLen >= 36 ) {
						// X25519: 32 字节公钥
						xrtX25519SharedSecret(pCtx->aX25519Secret, pCtx->aX25519Priv, pMsg + iPos + 4);
						pCtx->iTls13Group = 0x001d;
						pCtx->iTls13SecretLen = 32;
						#ifdef DEBUG_TRACE
							printf("    [TLS] ServerHello key_share: X25519\n");
						#endif
					} else if ( iGroup == 0x001e && iKeyLen == 56 && iExtDataLen >= 60 ) {
						// X448: 56 字节公钥
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
						// P-384 (secp384r1): 97 字节 uncompressed 公钥
						xrtECDHSecp384r1SharedSecret(pCtx->aX25519Secret, pCtx->aP384Priv, pMsg + iPos + 4);
						pCtx->iTls13Group = 0x0018;
						pCtx->iTls13SecretLen = 48;
						#ifdef DEBUG_TRACE
							printf("    [TLS] ServerHello key_share: P-384\n");
						#endif
					}
				}
			} else if ( iExtType == __XRT_TLS_EXT_ALPN ) {
				if ( !__xrt_tls_alpn_accept_selected(pCtx, pMsg + iPos, iExtDataLen) ) { return false; }
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
		if ( bAllowTls13 && memcmp(pCtx->aServerRandom + 24, aTls13DowngradeSentinel, sizeof(aTls13DowngradeSentinel)) == 0 ) {
			return false;
		}
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
			if ( bTls12ResumeAccepted ) { printf("    [TLS] ServerHello: session resumed\n"); }
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
static bool __xrt_tls_send_finished(xtlsctx *pCtx, bool bAsServer)
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
	if ( !__xrt_tls13_hmac(aVerifyData, pFinKey, iHashLen, aHash, iHashLen, iHashLen) ) { return false; }
	
	// 构造 Finished 握手消息
	aMsg[0] = __XRT_TLS_FINISHED;
	__xrt_tls_store_be24(aMsg + 1, (uint32)iHashLen);
	memcpy(aMsg + 4, aVerifyData, iHashLen);
	
	// 更新握手哈希
	__xrt_tls13_hash_update(pCtx, aMsg, 4 + iHashLen);
	
	// 加密发送
	return __xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, aMsg, 4 + iHashLen, bAsServer);
}


// 内部函数：__xrt_tls_verify_finished
static bool __xrt_tls_verify_finished(xtlsctx *pCtx, const uint8 *pVerifyData,
	size_t iVerifyLen, const uint8 *pTranscriptHash, size_t iHashLen, bool bFromServer)
{
	uint8 aExpected[64];
	uint8 *pFinishedKey;
	if ( iVerifyLen != iHashLen ) { return false; }
	// 根据发送方选择对应的 finished_key
	pFinishedKey = bFromServer ? pCtx->tEnc.aServerFinishedKey : pCtx->tEnc.aClientFinishedKey;
	// 计算期望的 verify_data = HMAC(finished_key, transcript_hash)
	if ( !__xrt_tls13_hmac(aExpected, pFinishedKey, iHashLen,
		pTranscriptHash, iHashLen, iHashLen) ) {
		return false;
	}
	// 常量时间比较，防止时序侧信道攻击
	return __xrt_tls_consttime_equal(aExpected, pVerifyData, iHashLen);
}


// 内部函数：构建 TLS 13 证书 verify 输入
static bool __xrt_tls13_build_cert_verify_input(uint8 *pOut, size_t *pOutLen,
	const uint8 *pTranscriptHash, size_t iTranscriptHashLen, uint16 iSigAlg, bool bAsServer)
{
	size_t iSigHashLen;
	const char *sContext = bAsServer ? "TLS 1.3, server CertificateVerify"
		: "TLS 1.3, client CertificateVerify";
	size_t iContextLen = strlen(sContext);
	uint8 aSigContent[64 + 40 + 1 + 64];
	size_t iContentLen;

	// 根据签名算法确定哈希输出长度
	switch ( iSigAlg ) {
		case 0x0807:  // ed25519 不需要预哈希
			iSigHashLen = 0;
			break;
		case 0x0804:
		case 0x0403:
		case 0x0809:
			iSigHashLen = 32;  // SHA-256 输出 32 字节
			break;
		case 0x0805:
		case 0x0503:
		case 0x080a:
			iSigHashLen = 48;  // SHA-384 输出 48 字节
			break;
		case 0x0806:
		case 0x080b:
			iSigHashLen = 64;  // SHA-512 输出 64 字节
			break;
		default:
			return false;
	}

	if ( iTranscriptHashLen == 0 || iTranscriptHashLen > 64 ) { return false; }

	// 构造签名输入内容: spaces(64) + context + 0x00 + transcript_hash （ RFC 8446 Section 4.4.3 ）
	memset(aSigContent, 0x20, 64);  // 64 字节 0x20（空格）分隔符
	iContentLen = 64;
	memcpy(aSigContent + iContentLen, sContext, iContextLen);  // 上下文字符串
	iContentLen += iContextLen;
	aSigContent[iContentLen++] = 0x00;  // 分隔符字节
	memcpy(aSigContent + iContentLen, pTranscriptHash, iTranscriptHashLen);  // 握手记录哈希
	iContentLen += iTranscriptHashLen;

	// Ed25519 不做预哈希，直接输出原始内容
	if ( iSigAlg == 0x0807 ) {
		memcpy(pOut, aSigContent, iContentLen);
		if ( pOutLen ) { *pOutLen = iContentLen; }
		return true;
	}
	// 其他算法对内容进行哈希后输出
	if ( !__xrt_tls_hash_bytes(pOut, iSigHashLen, aSigContent, iContentLen) ) { return false; }
	if ( pOutLen ) { *pOutLen = iSigHashLen; }
	return true;
}



/* ============================== TLS 密钥调度 ============================== */

// 执行 TLS 1.3 密钥调度 （ 在收到 ServerHello 后 ）
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
	
	// 获取当前握手哈希 （ 包含 ClientHello + ServerHello ）
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

// 更新 TLS 1.2 握手哈希 （ 同时更新 SHA-256 和 SHA-384 ）
static void __xrt_tls12_update_hash(xtlsctx *pCtx, const uint8 *pData, size_t iLen)
{
	xrtSHA256Update(&pCtx->tSHA256_12, (ptr)pData, iLen);
	xrtSHA512Update(&pCtx->tSHA384_12, (const ptr)pData, iLen);
}

// 获取 TLS 1.2 握手哈希快照 （ 根据套件选择 SHA-256 或 SHA-384 ）
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


// 内部函数：__xrt_tls12_collect_certs
static bool __xrt_tls12_collect_certs(const uint8 *pMsg, size_t iLen,
	uint8 **apCertData, size_t *apCertLen, size_t *pCertCount)
{
	size_t iOff = 3;
	size_t iCount = 0;
	uint32 iCertListLen;

	if ( !pMsg || iLen < 3 || !apCertData || !apCertLen || !pCertCount ) { return false; }
	// 读取证书链总长度（ 3 字节大端 ）
	iCertListLen = __xrt_tls_load_be24(pMsg);
	if ( 3 + iCertListLen > iLen ) { return false; }

	// 逐个解析证书条目
	while ( iOff + 3 <= iLen && iCount < __XRT_TLS_MAX_CERT_CHAIN ) {
		uint32 iCertLen = __xrt_tls_load_be24(pMsg + iOff);  // 单个证书长度
		iOff += 3;
		if ( iCertLen == 0 || iOff + iCertLen > iLen ) { return false; }
		apCertData[iCount] = (uint8*)(pMsg + iOff);
		apCertLen[iCount] = iCertLen;
		iCount++;
		iOff += iCertLen;
	}

	*pCertCount = iCount;
	return iCount > 0;
}


// 内部函数：__xrt_tls13_collect_certs
static bool __xrt_tls13_collect_certs(const uint8 *pMsg, size_t iLen,
	uint8 **apCertData, size_t *apCertLen, size_t *pCertCount)
{
	size_t iOff = 0;
	size_t iCount = 0;
	uint8 iReqCtxLen;
	uint32 iCertListLen;
	size_t iCertListEnd;

	if ( !pMsg || !apCertData || !apCertLen || !pCertCount || iLen < 4 ) { return false; }
	// 解析 request_context （ TLS 1.3 Certificate 消息特有字段 ）
	iReqCtxLen = pMsg[iOff++];
	if ( iOff + iReqCtxLen + 3 > iLen ) { return false; }
	iOff += iReqCtxLen;
	// 读取证书链总长度（ 3 字节大端 ）
	iCertListLen = __xrt_tls_load_be24(pMsg + iOff);
	iOff += 3;
	iCertListEnd = iOff + iCertListLen;
	if ( iCertListEnd > iLen ) { return false; }

	// 逐个解析证书条目（ 每个条目包含证书数据 + 扩展 ）
	while ( iOff + 3 <= iCertListEnd && iCount < __XRT_TLS_MAX_CERT_CHAIN ) {
		uint32 iCertLen = __xrt_tls_load_be24(pMsg + iOff);  // 证书数据长度
		uint16 iExtLen;
		iOff += 3;
		if ( iCertLen == 0 || iOff + iCertLen + 2 > iCertListEnd ) { return false; }
		apCertData[iCount] = (uint8*)(pMsg + iOff);  // 保存证书数据指针
		apCertLen[iCount] = iCertLen;
		iCount++;
		iOff += iCertLen;
		// 跳过证书条目的扩展数据
		iExtLen = __xrt_tls_load_be16(pMsg + iOff);
		iOff += 2;
		if ( iOff + iExtLen > iCertListEnd ) { return false; }
		iOff += iExtLen;
	}

	*pCertCount = iCount;
	return iCount > 0;
}

// Step 5: 解析 TLS 1.2 Certificate 消息
// TLS 1.2 格式: cert_list_len(3) + [cert_len(3) + cert]*  （ 无 request_context, 无 extensions ）
static bool __xrt_tls12_parse_certificate(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	uint8 *apCertData[__XRT_TLS_MAX_CERT_CHAIN];
	size_t apCertLen[__XRT_TLS_MAX_CERT_CHAIN];
	size_t iCertCount = 0;

	if ( !__xrt_tls12_collect_certs(pMsg, iLen, apCertData, apCertLen, &iCertCount) ) { return false; }
	if ( !__xrt_tls_capture_peer_cert_chain(pCtx, apCertData, apCertLen, iCertCount) ) { return false; }

	#ifdef DEBUG_TRACE
		printf("    [TLS12] Certificate: chain=%d pubkey=%s\n", (int)iCertCount, pCtx->bIsECPubKey ? "EC" : "RSA");
	#endif
	return true;
}

// Step 6: 解析 ServerKeyExchange （ ECDHE 套件 ）
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
	if ( iOff + 1 > iLen ) { return false; }
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
	
	if ( iOff + iExpectedLen > iLen ) { return false; }
	iOff += iExpectedLen;
	
	// 保存 server_params 的位置和长度 （ 用于签名验证 ）
	size_t iParamsLen = iOff;  // curve_type(1) + named_curve(2) + point_len(1) + point
	
	// 解析签名: sig_hash_alg(2) + sig_len(2) + signature
	if ( iOff + 4 > iLen ) { return false; }
	uint16 iSigAlg = __xrt_tls_load_be16(pMsg + iOff);
	iOff += 2;
	uint16 iSigLen = __xrt_tls_load_be16(pMsg + iOff);
	iOff += 2;
	if ( iOff + iSigLen > iLen ) { return false; }
	const uint8 *pSig = pMsg + iOff;

	#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
		printf("    [TLS12] SKE raw (%d): ", (int)iLen);
		for (int i = 0; i < (int)iLen; i++) { printf("%02x", pMsg[i]); }
		printf("\n");
	#endif
	
	// 验证签名: 对 client_random(32) + server_random(32) + server_params 进行验签
	if ( !pCtx->bSkipVerify ) {
		uint8 aSigBuf[192];
		size_t iSigBufLen = 32 + 32 + iParamsLen;
		uint8 *pSigData = (iSigBufLen <= sizeof(aSigBuf)) ? aSigBuf : (uint8*)xrtMalloc(iSigBufLen);
		bool bVerifyOK = false;
		if ( !pSigData ) { return false; }
		
		memcpy(pSigData, pCtx->aRandom, 32);          // client_random
		memcpy(pSigData + 32, pCtx->aServerRandom, 32); // server_random
		memcpy(pSigData + 64, pMsg, iParamsLen);         // server_params
		
		if ( iSigAlg == 0x0807 ) {
			if ( !pCtx->bIsEd25519Key || pCtx->iPubKeySz != 32 ) {
				if ( pSigData != aSigBuf ) { xrtFree(pSigData); }
				return false;
			}
			bVerifyOK = xrtEd25519Verify(pSigData, iSigBufLen, pSig, pCtx->aPubKey);
		} else {
			uint8 aHash[64];
			size_t iHashLen = __xrt_tls_sigalg_hash_len(iSigAlg);
			uint8 iSigType = iSigAlg & 0xFF;  // signature algorithm type

			if ( iHashLen == 0 || !__xrt_tls_hash_bytes(aHash, iHashLen, pSigData, iSigBufLen) ) {
				if ( pSigData != aSigBuf ) { xrtFree(pSigData); }
				return false;
			}

			#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
				printf("    [TLS12] SKE hash (%d): ", (int)iHashLen);
				for (int i = 0; i < (int)iHashLen; i++) { printf("%02x", aHash[i]); }
				printf("\n    [TLS12] SKE sig (%d): ", (int)iSigLen);
				for (int i = 0; i < (int)iSigLen; i++) { printf("%02x", pSig[i]); }
				printf("\n    [TLS12] SKE pub (%d): ", (int)pCtx->iPubKeySz);
				for (int i = 0; i < (int)pCtx->iPubKeySz; i++) { printf("%02x", pCtx->aPubKey[i]); }
				printf("\n");
			#endif

			if ( iSigAlg == 0x0804 || iSigAlg == 0x0805 || iSigAlg == 0x0806 ||
				iSigAlg == 0x0809 || iSigAlg == 0x080a || iSigAlg == 0x080b ) {
				bool bIsPssPss = (iSigAlg == 0x0809 || iSigAlg == 0x080a || iSigAlg == 0x080b);
				if ( bIsPssPss != pCtx->bIsRSAPSSKey ) {
					if ( pSigData != aSigBuf ) { xrtFree(pSigData); }
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

		if ( pSigData != aSigBuf ) { xrtFree(pSigData); }
		
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
		#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
			printf("    [TLS12] ServerKeyExchange: P-256 ECDH shared secret computed\n");
			printf("    [TLS12]   ServerPub: ");
			for (int i = 0; i < 65; i++) { printf("%02x", pServerPub[i]); }
			printf("\n    [TLS12]   ClientPub: ");
			for (int i = 0; i < 65; i++) { printf("%02x", pCtx->aP256Pub[i]); }
			printf("\n    [TLS12]   SharedSecret: ");
			for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aSharedSecret[i]); }
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
		#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
			printf("    [TLS12] ServerKeyExchange: X25519 ECDH shared secret computed\n");
			printf("    [TLS12]   ServerPub: ");
			for (int i = 0; i < 32; i++) { printf("%02x", pServerPub[i]); }
			printf("\n    [TLS12]   ClientPub: ");
			for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aX25519Pub[i]); }
			printf("\n    [TLS12]   SharedSecret: ");
			for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aSharedSecret[i]); }
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
		if ( !pPadded ) { return; }
		
		pPadded[0] = 0x00;
		pPadded[1] = 0x02;
		size_t iPadLen = iModSz - 3 - 48;  // padding length
		xrtRandomBytes(pPadded + 2, iPadLen);
		// 确保 padding 无零字节
		for ( size_t i = 0; i < iPadLen; i++ ) {
			while ( pPadded[2 + i] == 0 ) { xrtRandomBytes(pPadded + 2 + i, 1); }
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
	
	// 包装为 TLS record （ 明文 ）
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
	
	#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
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
				for (int i = 0; i < 32; i++) { printf("%02x", tOut[i]); }
				printf("\n    [TLS12]     Expected: ");
				for (int i = 0; i < 32; i++) { printf("%02x", tExpected[i]); }
				printf("\n");
			}
		}
		printf("    [TLS12]   PreMaster: ");
		for (int i = 0; i < (int)iPMLen; i++) { printf("%02x", pPreMaster[i]); }
		printf("\n    [TLS12]   MasterSecret: ");
		for (int i = 0; i < 48; i++) { printf("%02x", pCtx->aMasterSecret[i]); }
		printf("\n    [TLS12]   ClientWriteKey: ");
		for (int i = 0; i < (int)pCtx->iKeyLen; i++) { printf("%02x", pCtx->aClientWriteKey12[i]); }
		printf("\n    [TLS12]   ServerWriteKey: ");
		for (int i = 0; i < (int)pCtx->iKeyLen; i++) { printf("%02x", pCtx->aServerWriteKey12[i]); }
		printf("\n    [TLS12]   ClientIV: ");
		for (int i = 0; i < (int)iIVLen; i++) { printf("%02x", pCtx->aClientWriteIV12[i]); }
		printf("\n    [TLS12]   ServerIV: ");
		for (int i = 0; i < (int)iIVLen; i++) { printf("%02x", pCtx->aServerWriteIV12[i]); }
		printf("\n    [TLS12]   ClientRandom: ");
		for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aRandom[i]); }
		printf("\n    [TLS12]   ServerRandom: ");
		for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aServerRandom[i]); }
		printf("\n");
	#endif
	
	// 清零临时数据
	memset(aKeyBlock, 0, sizeof(aKeyBlock));
}

// Step 9: 发送 ChangeCipherSpec + Finished
static bool __xrt_tls12_send_ccs_finished(xtlsctx *pCtx, bool bAsServer)
{
	const char *sLabel = bAsServer ? "server finished" : "client finished";

	// 1. 发送 ChangeCipherSpec 记录 （ 明文, 1字节 0x01 ）
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
	
	// 更新握手哈希 （ 包含客户端 Finished ）
	__xrt_tls12_update_hash(pCtx, aMsg, 16);
	
	#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
		printf("    [TLS12] Finished hash: ");
		for (int i = 0; i < (int)iHashLen; i++) { printf("%02x", aHash[i]); }
		printf("\n    [TLS12] %s VerifyData: ", bAsServer ? "Server" : "Client");
		for (int i = 0; i < 12; i++) { printf("%02x", aVerifyData[i]); }
		printf("\n    [TLS12] FinishedMsg: ");
		for (int i = 0; i < 16; i++) { printf("%02x", aMsg[i]); }
		printf("\n");
	#endif
	
	// 4. 用 TLS 1.2 GCM 加密发送 Finished
	if ( !__xrt_tls12_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, aMsg, 16) ) {
		return false;
	}
	
	#if defined(DEBUG_TRACE) && defined(XRT_TLS_TRACE_SECRETS)
		printf("    [TLS12] Sent CCS + Finished\n");
		// Dump entire send buffer
		printf("    [TLS12] SendBuf (%d bytes):\n    ", (int)pCtx->tSendBuf.iSize);
		for (int i = 0; i < (int)pCtx->tSendBuf.iSize; i++) {
			printf("%02x ", (uint8)pCtx->tSendBuf.pData[i]);
			if ((i+1) % 32 == 0) { printf("\n    "); }
		}
		printf("\n");
	#endif

	return true;
}


// 内部函数：__xrt_tls12_verify_finished
static bool __xrt_tls12_verify_finished(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen, bool bFromServer)
{
	const char *sLabel = bFromServer ? "server finished" : "client finished";

	if ( iLen < 12 ) { return false; }
	
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
	for ( int i = 0; i < 12; i++ ) { iDiff |= aExpected[i] ^ pMsg[i]; }
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Verify %s Finished: %s\n", bFromServer ? "server" : "client",
			iDiff ? "FAILED" : "OK");
	#endif
	return (iDiff == 0);
}


// 内部函数：发送 TLS 12 handshake 消息
static bool __xrt_tls12_send_handshake_message(xtlsctx *pCtx, const uint8 *pMsg, size_t iMsgLen)
{
	uint8 aRec[5];

	if ( !pCtx || !pMsg || iMsgLen == 0 || iMsgLen > 0xffff ) { return false; }

	// 更新握手哈希
	__xrt_tls12_update_hash(pCtx, pMsg, iMsgLen);

	// 构建记录头：类型(1) + 版本(2) + 长度(2)
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aRec + 3, (uint16)iMsgLen);

	// 拼接记录头和消息体到发送缓冲区
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aRec, sizeof(aRec));
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)pMsg, iMsgLen);
	return true;
}

static bool __xrt_tls_sign_server_hash(xtlsctx *pCtx, const uint8 *pHash, size_t iHashLen,
	uint8 *pSig, size_t *pSigLen);


// 内部函数：__xrt_tls12_send_server_hello
static bool __xrt_tls12_send_server_hello(xtlsctx *pCtx)
{
	uint8 aMsg[512];
	size_t iPos = 0;
	size_t iLenPos;

	if ( !pCtx ) { return false; }

	// 生成服务器随机数
	xrtRandomBytes(pCtx->aServerRandom, 32);

	// 构建握手消息头：类型(1) + 长度(3)
	aMsg[iPos++] = __XRT_TLS_SERVER_HELLO;
	iLenPos = iPos;
	iPos += 3;

	// 协议版本 TLS 1.2
	__xrt_tls_store_be16(aMsg + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;

	// 服务器随机数 (32 字节)
	memcpy(aMsg + iPos, pCtx->aServerRandom, 32);
	iPos += 32;

	// 会话 ID
	aMsg[iPos++] = pCtx->iSessionIdLen;
	memcpy(aMsg + iPos, pCtx->aSessionId, pCtx->iSessionIdLen);
	iPos += pCtx->iSessionIdLen;

	// 已协商的密码套件 + 压缩方法 (0 = 无压缩)
	__xrt_tls_store_be16(aMsg + iPos, pCtx->iCipherSuite);
	iPos += 2;
	aMsg[iPos++] = 0;

	// extensions
	{
		size_t iExtStart = iPos;
		iPos += 2;
		if ( pCtx->bPeerSecureReneg ) {
			__xrt_tls_store_be16(aMsg + iPos, __XRT_TLS_EXT_RENEGOTIATION_INFO);
			iPos += 2;
			__xrt_tls_store_be16(aMsg + iPos, 1);
			iPos += 2;
			aMsg[iPos++] = 0;
		}
		if ( pCtx->sAlpnSelected[0] != '\0' ) {
			size_t iProtoLen = strlen(pCtx->sAlpnSelected);
			if ( iProtoLen == 0u || iProtoLen > 255u || iPos + 7u + iProtoLen > sizeof(aMsg) ) { return false; }
			__xrt_tls_store_be16(aMsg + iPos, __XRT_TLS_EXT_ALPN);
			iPos += 2;
			__xrt_tls_store_be16(aMsg + iPos, (uint16)(3u + iProtoLen));
			iPos += 2;
			__xrt_tls_store_be16(aMsg + iPos, (uint16)(1u + iProtoLen));
			iPos += 2;
			aMsg[iPos++] = (uint8)iProtoLen;
			memcpy(aMsg + iPos, pCtx->sAlpnSelected, iProtoLen);
			iPos += iProtoLen;
		}
		__xrt_tls_store_be16(aMsg + iExtStart, (uint16)(iPos - iExtStart - 2));
	}

	__xrt_tls_store_be24(aMsg + iLenPos, (uint32)(iPos - iLenPos - 3));
	return __xrt_tls12_send_handshake_message(pCtx, aMsg, iPos);
}


// 内部函数：发送 TLS 12 certificate
static bool __xrt_tls12_send_certificate(xtlsctx *pCtx)
{
	size_t iBodyLen;
	size_t iMsgLen;
	uint8 *pMsg;

	if ( !pCtx || !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) { return false; }

	// 计算消息体长度：证书列表总长度(3) + 单个证书长度(3) + 证书数据
	iBodyLen = 3 + 3 + pCtx->iCertDerLen;
	iMsgLen = __XRT_TLS_MSGHDR_SIZE + iBodyLen;
	pMsg = (uint8*)xrtMalloc(iMsgLen);
	if ( !pMsg ) { return false; }

	// 构建消息：类型(1) + 体长度(3) + 证书列表长度(3) + 证书长度(3) + 证书数据
	pMsg[0] = __XRT_TLS_CERTIFICATE;
	__xrt_tls_store_be24(pMsg + 1, (uint32)iBodyLen);					// 消息体长度
	__xrt_tls_store_be24(pMsg + 4, (uint32)(3 + pCtx->iCertDerLen));	// 证书列表总长度
	__xrt_tls_store_be24(pMsg + 7, (uint32)pCtx->iCertDerLen);		// 单个证书长度
	memcpy(pMsg + 10, pCtx->pCertDer, pCtx->iCertDerLen);
	if ( !__xrt_tls12_send_handshake_message(pCtx, pMsg, iMsgLen) ) {
		xrtFree(pMsg);
		return false;
	}
	xrtFree(pMsg);
	return true;
}


// 内部函数：__xrt_tls12_send_server_key_exchange
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

	if ( !pCtx || !pCtx->bIsECDHE || pCtx->iServerSigAlg == 0 ) { return false; }

	// 构建消息头和 ECDHE 参数
	aMsg[iPos++] = __XRT_TLS_SERVER_KEY_EXCHANGE;
	iLenPos = iPos;
	iPos += 3;

	// EC 参数：曲线类型(3 = named_curve) + 命名曲线 ID
	aMsg[iPos++] = 3;	// named_curve
	__xrt_tls_store_be16(aMsg + iPos, pCtx->iTls12Curve);
	iPos += 2;

	// 根据曲线类型写入服务器临时公钥
	if ( pCtx->iTls12Curve == 0x001d ) {		// X25519
		iPubLen = 32;
		aMsg[iPos++] = 32;
		memcpy(aMsg + iPos, pCtx->aX25519Pub, 32);
	} else if ( pCtx->iTls12Curve == 0x001e ) {	// X448
		iPubLen = 56;
		aMsg[iPos++] = 56;
		memcpy(aMsg + iPos, pCtx->aX448Pub, 56);
	} else if ( pCtx->iTls12Curve == 0x0017 ) {	// secp256r1
		iPubLen = 65;	// 04 + 32字节X + 32字节Y
		aMsg[iPos++] = 65;
		memcpy(aMsg + iPos, pCtx->aP256Pub, 65);
	} else if ( pCtx->iTls12Curve == 0x0018 ) {	// secp384r1
		iPubLen = 97;	// 04 + 48字节X + 48字节Y
		aMsg[iPos++] = 97;
		memcpy(aMsg + iPos, pCtx->aP384Pub, 97);
	} else {
		return false;
	}
	iPos += iPubLen;
	iParamsLen = iPos - __XRT_TLS_MSGHDR_SIZE;

	// 构建签名输入：client_random(32) + server_random(32) + ECDHE参数
	memcpy(aSigInput, pCtx->aRandom, 32);
	memcpy(aSigInput + 32, pCtx->aServerRandom, 32);
	memcpy(aSigInput + 64, aMsg + __XRT_TLS_MSGHDR_SIZE, iParamsLen);

	// 根据签名算法对参数进行签名
	iHashAlg = (uint16)(pCtx->iServerSigAlg >> 8);	// 高字节为哈希算法标识
	(void)iHashAlg;
	if ( pCtx->iServerSigAlg == 0x0807 ) {
		// Ed25519 签名：直接对原始数据签名，无需预哈希
		if ( !__xrt_tls_sign_server_hash(pCtx, aSigInput, 64 + iParamsLen, aSig, &iSigLen) ) {
			#ifdef DEBUG_TRACE
				printf("    [TLS12] SKE sign FAILED: sigAlg=0x%04x rawLen=%d curve=0x%04x\n",
					pCtx->iServerSigAlg, (int)(64 + iParamsLen), pCtx->iTls12Curve);
			#endif
			return false;
		}
	} else {
		// RSA/ECDSA 签名：先哈希再签名
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

	// 追加签名：算法标识(2) + 签名长度(2) + 签名数据
	__xrt_tls_store_be16(aMsg + iPos, pCtx->iServerSigAlg);
	iPos += 2;
	__xrt_tls_store_be16(aMsg + iPos, (uint16)iSigLen);
	iPos += 2;
	memcpy(aMsg + iPos, aSig, iSigLen);
	iPos += iSigLen;

	#ifdef DEBUG_TRACE
		printf("    [TLS12] SKE send raw (%d): ", (int)(iPos - __XRT_TLS_MSGHDR_SIZE));
		for (int i = (int)__XRT_TLS_MSGHDR_SIZE; i < (int)iPos; i++) { printf("%02x", aMsg[i]); }
		printf("\n");
	#endif

	__xrt_tls_store_be24(aMsg + iLenPos, (uint32)(iPos - iLenPos - 3));
	// 回填消息体长度并发送
	return __xrt_tls12_send_handshake_message(pCtx, aMsg, iPos);
}


// 内部函数：__xrt_tls12_send_server_hello_done
static bool __xrt_tls12_send_server_hello_done(xtlsctx *pCtx)
{
	uint8 aMsg[4];

	if ( !pCtx ) { return false; }
	aMsg[0] = __XRT_TLS_SERVER_HELLO_DONE;
	__xrt_tls_store_be24(aMsg + 1, 0);
	return __xrt_tls12_send_handshake_message(pCtx, aMsg, sizeof(aMsg));
}


// 内部函数：__xrt_tls12_send_server_flight
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


// 内部函数：__xrt_tls12_parse_client_key_exchange
static bool __xrt_tls12_parse_client_key_exchange(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	if ( !pCtx || !pMsg || !pCtx->bIsECDHE || iLen < 1 ) { return false; }

	// 根据协商的曲线解析客户端临时公钥，计算共享密钥
	if ( pCtx->iTls12Curve == 0x001d ) {		// X25519：公钥32字节 + 1字节长度前缀
		if ( iLen != 33 || pMsg[0] != 32 ) { return false; }
		xrtX25519SharedSecret(pCtx->aSharedSecret, pCtx->aX25519Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 32;
		return true;
	}
	if ( pCtx->iTls12Curve == 0x001e ) {		// X448：公钥56字节 + 1字节长度前缀
		if ( iLen != 57 || pMsg[0] != 56 ) { return false; }
		xrtX448SharedSecret(pCtx->aSharedSecret, pCtx->aX448Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 56;
		return true;
	}
	if ( pCtx->iTls12Curve == 0x0017 ) {		// secp256r1：未压缩点 04 + 32字节X + 32字节Y
		if ( iLen != 66 || pMsg[0] != 65 || pMsg[1] != 0x04 ) { return false; }
		xrtECDHSecp256r1SharedSecret(pCtx->aSharedSecret, pCtx->aP256Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 32;
		return true;
	}
	if ( pCtx->iTls12Curve == 0x0018 ) {		// secp384r1：未压缩点 04 + 48字节X + 48字节Y
		if ( iLen != 98 || pMsg[0] != 97 || pMsg[1] != 0x04 ) { return false; }
		xrtECDHSecp384r1SharedSecret(pCtx->aSharedSecret, pCtx->aP384Priv, pMsg + 1);
		pCtx->iSharedSecretLen = 48;
		return true;
	}
	return false;
}



/* ============================== 公共 API ============================== */

static inline bool __xrt_tls_have_record(xtlsctx *pCtx)
{
	if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) { return false; }
	uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
	if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return true; }
	return pCtx->tRecvBuf.iSize >= (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen);
}

static bool __xrt_tls_parse_client_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen);
static bool __xrt_tls13_send_server_hello(xtlsctx *pCtx);
static bool __xrt_tls13_send_server_flight(xtlsctx *pCtx);
static bool __xrt_tls_prepare_server_identity(xtlsctx *pCtx);


// 配置对象自旋锁，用于多线程安全访问配置数据
static void __xrt_tls_config_lock(const xtlsconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	while ( __xrtAtomicCompareExchange32((volatile long*)&pCfg->iDataLock, 1, 0) != 0 ) {
		xrtSleep(1);
	}
}


// 释放配置对象自旋锁
static void __xrt_tls_config_unlock(const xtlsconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	(void)__xrtAtomicExchange32((volatile long*)&pCfg->iDataLock, 0);
}


// 上下文自旋锁，跨平台实现（ TCC/x86_64 用原子交换，Windows 用 Interlocked，其他用 GCC 内建函数 ）
static void __xrt_tls_ctx_lock(xtlsctx* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		while ( __xrtAtomicExchange32(&pCtx->iApiLock, 1) != 0 ) {
			xrtThreadYield();
		}
	#elif defined(_WIN32) || defined(_WIN64)
		while ( InterlockedCompareExchange((volatile LONG*)&pCtx->iApiLock, 1, 0) != 0 ) {
			xrtThreadYield();
		}
	#else
		while ( __sync_lock_test_and_set(&pCtx->iApiLock, 1) != 0 ) {
			xrtThreadYield();
		}
	#endif
}


// 释放上下文自旋锁
static void __xrt_tls_ctx_unlock(xtlsctx* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		(void)__xrtAtomicExchange32(&pCtx->iApiLock, 0);
	#elif defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)&pCtx->iApiLock, 0);
	#else
		__sync_lock_release(&pCtx->iApiLock);
	#endif
}


// 安全清除上下文中拥有的证书和私钥数据
static void __xrt_tls_clear_cert_owned(xtlsctx* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}
	if ( pCtx->pCertDer ) {
		__xrt_tls_secure_zero(pCtx->pCertDer, pCtx->iCertDerLen);
		xrtFree(pCtx->pCertDer);
		pCtx->pCertDer = NULL;
		pCtx->iCertDerLen = 0;
	}
	if ( pCtx->pKeyDer ) {
		__xrt_tls_secure_zero(pCtx->pKeyDer, pCtx->iKeyDerLen);
		xrtFree(pCtx->pKeyDer);
		pCtx->pKeyDer = NULL;
		pCtx->iKeyDerLen = 0;
	}
}


// 设置上下文的证书和私钥（接管所有权），并解析服务器身份信息
static xnet_result __xrt_tls_set_cert_owned(xtlsctx* pCtx, uint8* pCertDer, size_t iCertDerLen, uint8* pKeyDer, size_t iKeyDerLen)
{
	if ( pCtx == NULL ) {
		// 上下文无效时释放传入的证书和私钥，避免内存泄漏
		if ( pCertDer ) { xrtFree(pCertDer); }
		if ( pKeyDer ) { xrtFree(pKeyDer); }
		return XRT_NET_ERROR;
	}

	// 清除旧的证书和私钥
	__xrt_tls_clear_cert_owned(pCtx);

	// 接管新证书和私钥的所有权
	pCtx->pCertDer = pCertDer;
	pCtx->iCertDerLen = iCertDerLen;
	pCtx->pKeyDer = pKeyDer;
	pCtx->iKeyDerLen = iKeyDerLen;

	// 解析证书中的公钥等身份信息（ RSA 模数、EC 密钥等 ）
	if ( !__xrt_tls_prepare_server_identity(pCtx) ) {
		__xrt_tls_clear_cert_owned(pCtx);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 复制 CA 证书数据到上下文
static bool __xrt_tls_set_ca_copy(xtlsctx* pCtx, const void* pCaData, size_t iCaDataLen)
{
	uint8* pCopy;

	if ( pCtx == NULL ) {
		return false;
	}
	if ( pCtx->pCaData ) {
		xrtFree(pCtx->pCaData);
		pCtx->pCaData = NULL;
		pCtx->iCaDataLen = 0;
	}
	if ( pCaData == NULL || iCaDataLen == 0 ) {
		return true;
	}
	pCopy = (uint8*)xrtMalloc(iCaDataLen + 1);
	if ( pCopy == NULL ) {
		return false;
	}
	memcpy(pCopy, pCaData, iCaDataLen);
	pCopy[iCaDataLen] = 0;
	pCtx->pCaData = pCopy;
	pCtx->iCaDataLen = iCaDataLen;
	return true;
}


// 复制 CRL（ 证书吊销列表 ）数据到上下文
static bool __xrt_tls_set_crl_copy(xtlsctx* pCtx, const void* pCrlData, size_t iCrlDataLen)
{
	uint8* pCopy;

	if ( pCtx == NULL ) {
		return false;
	}
	if ( pCtx->pCrlData ) {
		xrtFree(pCtx->pCrlData);
		pCtx->pCrlData = NULL;
		pCtx->iCrlDataLen = 0;
	}
	if ( pCrlData == NULL || iCrlDataLen == 0 ) {
		return true;
	}
	pCopy = (uint8*)xrtMalloc(iCrlDataLen + 1);
	if ( pCopy == NULL ) {
		return false;
	}
	memcpy(pCopy, pCrlData, iCrlDataLen);
	pCopy[iCrlDataLen] = 0;
	pCtx->pCrlData = pCopy;
	pCtx->iCrlDataLen = iCrlDataLen;
	return true;
}


XXAPI xtlsctx* xrtTlsCreate(const xtlsconfig *pConfig, bool bIsServer)
{
	xtlsctx *pCtx = (xtlsctx*)xrtCalloc(1, sizeof(xtlsctx));
	if ( !pCtx ) { return NULL; }
	
	pCtx->bIsServer = bIsServer;
	pCtx->bHandshakeDone = false;
	pCtx->iCipherSuite = __XRT_TLS_CHACHA20_POLY1305_SHA256;  // 默认首选
	pCtx->iState = bIsServer ? XRT_TLS_SERVER_START : XRT_TLS_CLIENT_START;
	
	// 初始化 TLS 1.3 握手哈希上下文
	xrtSHA256Init(&pCtx->tSHA256);
	xrtSHA384Init(&pCtx->tSHA384);
	
	// 初始化 TLS 1.2 哈希上下文 （ 两个都初始化, 待协商后再选择 ）
	xrtSHA256Init(&pCtx->tSHA256_12);
	xrtSHA384Init(&pCtx->tSHA384_12);
	
	// 初始化 IO 缓冲区
	if ( !__xrt_tls_buf_init(&pCtx->tSendBuf, 8192)
		|| !__xrt_tls_buf_init(&pCtx->tRecvBuf, 8192)
		|| !__xrt_tls_buf_init(&pCtx->tHandshakeBuf, 4096)
		|| !__xrt_tls_buf_init(&pCtx->tPlainBuf, 4096) ) {
		xrtTlsDestroy(pCtx);
		return NULL;
	}
	
	if ( pConfig ) {
		pCtx->bSkipVerify = !pConfig->bVerifyPeer;
		pCtx->bAllowTLS12Ed25519 = pConfig->bAllowTLS12Ed25519;
		pCtx->iMaxVersion = pConfig->iMaxVersion;
		if ( pConfig->sHostName ) {
			strncpy(pCtx->sHostname, pConfig->sHostName, sizeof(pCtx->sHostname) - 1);
			pCtx->sHostname[sizeof(pCtx->sHostname) - 1] = '\0';
		}
		if ( pConfig->sAlpnProtocols && __xrt_tls_alpn_config_valid(pConfig->sAlpnProtocols) ) {
			strncpy(pCtx->sAlpnProtocols, pConfig->sAlpnProtocols, sizeof(pCtx->sAlpnProtocols) - 1);
			pCtx->sAlpnProtocols[sizeof(pCtx->sAlpnProtocols) - 1] = '\0';
		}
		pCtx->OnSNI = pConfig->OnSNI;
		pCtx->pSNIUserData = pConfig->pSNIUserData;
		pCtx->OnVerify = pConfig->OnVerify;
		pCtx->pVerifyUserData = pConfig->pVerifyUserData;
		if ( pConfig->pResume && __xrt_tls_resume_copy(&pCtx->tResume, pConfig->pResume) ) {
			pCtx->bHaveResume = true;
			if ( pCtx->iMaxVersion == 0 || pCtx->iMaxVersion > pCtx->tResume.iVersion ) {
				pCtx->iMaxVersion = pCtx->tResume.iVersion;
			}
		}
		__xrt_tls_config_lock(pConfig);
		if ( (pConfig->pCertData && pConfig->iCertDataLen > 0) || (pConfig->pKeyData && pConfig->iKeyDataLen > 0) ) {
			if ( xrtTlsSetCertData(pCtx, pConfig->pCertData, pConfig->iCertDataLen, pConfig->pKeyData, pConfig->iKeyDataLen) != XRT_NET_OK ) {
				__xrt_tls_config_unlock(pConfig);
				xrtTlsDestroy(pCtx);
				return NULL;
			}
		} else if ( (pConfig->sCertFile && pConfig->sCertFile[0]) || (pConfig->sKeyFile && pConfig->sKeyFile[0]) ) {
			if ( xrtTlsSetCert(pCtx, pConfig->sCertFile, pConfig->sKeyFile) != XRT_NET_OK ) {
				__xrt_tls_config_unlock(pConfig);
				xrtTlsDestroy(pCtx);
				return NULL;
			}
		}
		if ( pConfig->bVerifyPeer ) {
			if ( pConfig->pCaData && pConfig->iCaDataLen > 0 ) {
				(void)__xrt_tls_set_ca_copy(pCtx, pConfig->pCaData, pConfig->iCaDataLen);
			} else {
				(void)__xrt_tls_load_ca_bundle(pCtx, pConfig->sCaFile);
			}
			if ( pConfig->pCrlData && pConfig->iCrlDataLen > 0 ) {
				(void)__xrt_tls_set_crl_copy(pCtx, pConfig->pCrlData, pConfig->iCrlDataLen);
			} else {
				(void)__xrt_tls_load_crl_bundle(pCtx, pConfig->sCrlFile);
			}
		}
		__xrt_tls_config_unlock(pConfig);
	} else {
		pCtx->bSkipVerify = true;
	}
	
	return pCtx;
}


// 销毁 TLS
XXAPI void xrtTlsDestroy(xtlsctx *pCtx)
{
	if ( !pCtx ) { return; }
	
	// 释放 IO 缓冲区
	__xrt_tls_buf_free(&pCtx->tSendBuf);
	__xrt_tls_buf_free(&pCtx->tRecvBuf);
	__xrt_tls_buf_free(&pCtx->tHandshakeBuf);
	__xrt_tls_buf_free(&pCtx->tPlainBuf);
	
	// 释放证书和私钥（私钥先安全清零再释放）
	if ( pCtx->pCertDer ) { xrtFree(pCtx->pCertDer); }
	if ( pCtx->pKeyDer ) {
		__xrt_tls_secure_zero(pCtx->pKeyDer, pCtx->iKeyDerLen);
		xrtFree(pCtx->pKeyDer);
	}
	// 释放 CA 和 CRL 数据
	if ( pCtx->pCaData ) { xrtFree(pCtx->pCaData); }
	if ( pCtx->pCrlData ) { xrtFree(pCtx->pCrlData); }
	if ( pCtx->pPeerCertDer ) { xrtFree(pCtx->pPeerCertDer); }

	// 安全清零整个上下文结构后释放，防止密钥残留
	__xrt_tls_secure_zero(pCtx, sizeof(*pCtx));
	xrtFree(pCtx);
}


// 内部函数：__xrt_tls_drive_internal
static xnet_result __xrt_tls_drive_internal(xtlsctx *pCtx, xsocket hSocket, bool bAllowSocketIO)
{
	if ( !pCtx ) { return XRT_NET_ERROR; }
	if ( bAllowSocketIO && hSocket == XSOCKET_INVALID ) { return XRT_NET_ERROR; }
	
	// 如果有待发送数据，先发送
	if ( pCtx->tSendBuf.iSize > 0 ) {
		if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
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
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
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
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) { return XRT_NET_AGAIN; }
			
			uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
			if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return XRT_NET_ERROR; }
			if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { return XRT_NET_AGAIN; }
			
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
				return __xrt_tls_handle_alert(pCtx, pRecData, iRecLen);
			}
			if ( iRecType != __XRT_TLS_HANDSHAKE ) {
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				return XRT_NET_ERROR;
			}
			
			// 更新握手哈希 （ ServerHello 消息 ）
			__xrt_tls13_hash_update(pCtx, pRecData, iRecLen);
			__xrt_tls12_update_hash(pCtx, pRecData, iRecLen);
			
			// 解析 ServerHello （ 跳过握手头 ）
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
					for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aRandom[i]); }
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
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
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
				if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return XRT_NET_ERROR; }
				
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
					// ChangeCipherSpec: 忽略 （ TLS 1.3 兼容 ）
					if ( iRecLen != 1 || pCtx->tRecvBuf.pData[__XRT_TLS_RECHDR_SIZE] != 0x01 ) {
						return XRT_NET_ERROR;
					}
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
						// 将解密后的握手明文追加到重组缓冲区 （ 支持跨记录的大消息 ）
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
								size_t iEEPos;
								size_t iEEEnd;
								if ( iMsgBodyLen < 2u ) { return XRT_NET_ERROR; }
								iEEPos = __XRT_TLS_MSGHDR_SIZE + 2u;
								iEEEnd = iEEPos + (size_t)__xrt_tls_load_be16(pMsg + __XRT_TLS_MSGHDR_SIZE);
								if ( iEEEnd != __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen ) { return XRT_NET_ERROR; }
								while ( iEEPos + 4u <= iEEEnd ) {
									uint16 iExtType = __xrt_tls_load_be16(pMsg + iEEPos);
									uint16 iExtLen;
									iEEPos += 2u;
									iExtLen = __xrt_tls_load_be16(pMsg + iEEPos);
									iEEPos += 2u;
									if ( iEEPos + (size_t)iExtLen > iEEEnd ) { return XRT_NET_ERROR; }
									if ( iExtType == __XRT_TLS_EXT_ALPN
										&& !__xrt_tls_alpn_accept_selected(pCtx, pMsg + iEEPos, iExtLen) ) {
										return XRT_NET_ERROR;
									}
									iEEPos += (size_t)iExtLen;
								}
								if ( iEEPos != iEEEnd ) { return XRT_NET_ERROR; }
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
									if ( 4u + iSigLen > iMsgBodyLen ) { return XRT_NET_ERROR; }
									pSig = pMsg + __XRT_TLS_MSGHDR_SIZE + 4;

									if ( !__xrt_tls13_build_cert_verify_input(aContentHash, &iContentHashLen,
										pCtx->aSigHash, pCtx->iSigHashLen, iSigAlg, true) ) {
										return XRT_NET_ERROR;
									}

									if ( pCtx->bIsEd25519Key ) {
										if ( iSigAlg != 0x0807 || pCtx->iPubKeySz != 32 ) { return XRT_NET_ERROR; }
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
										if ( bIsPssPss != pCtx->bIsRSAPSSKey ) { return XRT_NET_ERROR; }
										bVerifyOK = xrtRSAPSSVerify(aContentHash, iContentHashLen,
											pSig, iSigLen,
											pCtx->aPubKey, pCtx->iPubKeyModSz,
											pCtx->aPubKey + pCtx->iPubKeyModSz,
											pCtx->iPubKeySz - pCtx->iPubKeyModSz);
									}

									if ( !bVerifyOK ) { return XRT_NET_ERROR; }
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
								if ( !__xrt_tls_send_finished(pCtx, false) ) {
									return XRT_NET_ERROR;
								}
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
				for (int i = 0; i < 32; i++) { printf("%02x", pCtx->aRandom[i]); }
				printf("\n");
			#endif
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
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
				// 读取记录头：类型(1字节) + 版本(2字节) + 长度(2字节)
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				const uint8 *pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
				if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return XRT_NET_ERROR; }
				
				// 数据不足时等待更多数据
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { break; }
				
				// 处理 Alert 警告记录
				if ( iRecType == __XRT_TLS_ALERT ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS12] Alert: level=%d desc=%d\n", pRecData[0], (iRecLen >= 2) ? pRecData[1] : -1);
					#endif
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return __xrt_tls_handle_alert(pCtx, pRecData, iRecLen);
				}
				
				// 非握手记录直接丢弃
				if ( iRecType != __XRT_TLS_HANDSHAKE ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					continue;
				}
				
				// 明文握手记录: 可能包含多个握手消息
				size_t iMsgOff = 0;
				
				// 逐个解析记录内的握手消息
				while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= iRecLen ) {
					// 握手消息头：类型(1字节) + 长度(3字节)
					uint8 iMsgType = pRecData[iMsgOff];
					uint32 iMsgBodyLen = __xrt_tls_load_be24(pRecData + iMsgOff + 1);
					size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;
					
					if ( iMsgOff + iTotalMsgLen > iRecLen ) { break; }
					
					const uint8 *pFullMsg = pRecData + iMsgOff;
					const uint8 *pMsgBody = pFullMsg + __XRT_TLS_MSGHDR_SIZE;
					
					// 更新 TLS 1.2 握手哈希
					__xrt_tls12_update_hash(pCtx, pFullMsg, iTotalMsgLen);
					
					switch ( iMsgType ) {
						case __XRT_TLS_CERTIFICATE: {
							#ifdef DEBUG_TRACE
								printf("    [TLS12] Got Certificate, len=%d\n", (int)iMsgBodyLen);
							#endif
							// 解析服务器证书链
							if ( !__xrt_tls12_parse_certificate(pCtx, pMsgBody, iMsgBodyLen) ) {
								return XRT_NET_ERROR;
							}
							// ECDHE 套件需要等待 ServerKeyExchange
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
							// 解析服务器密钥交换参数（ ECDHE 公钥或 DH 参数 ）
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
							if ( !__xrt_tls12_send_ccs_finished(pCtx, false) ) {
								return XRT_NET_ERROR;
							}
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
				if ( pCtx->iState == XRT_TLS12_CLIENT_WAIT_CCS ) { break; }
			}
			
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS12_CLIENT_WAIT_CCS: {
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
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
			
			// 逐个处理接收缓冲区中的记录
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				const uint8 *pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
				if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return XRT_NET_ERROR; }
				
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { break; }
				
				if ( iRecType == __XRT_TLS_CHANGE_CIPHER ) {
					// 验证 CCS 消息格式：长度必须为 1，值为 0x01
					if ( iRecLen != 1 || pRecData[0] != 0x01 ) { return XRT_NET_ERROR; }
					#ifdef DEBUG_TRACE
						printf("    [TLS12] Got server CCS\n");
					#endif
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					pCtx->iState = XRT_TLS12_CLIENT_WAIT_FINISH;
					break;
				} else if ( iRecType == __XRT_TLS_ALERT ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS12] WAIT_CCS: Alert level=%d desc=%d\n", pRecData[0], (iRecLen >= 2) ? pRecData[1] : -1);
					#endif
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return __xrt_tls_handle_alert(pCtx, pRecData, iRecLen);
				}
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			}
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS12_CLIENT_WAIT_FINISH: {
			// 事件驱动优化: 缓冲区已有完整记录时跳过 recv
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}
			
			// 检查是否有完整记录头
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) { return XRT_NET_AGAIN; }
			
			// 读取记录长度和负载数据指针
			uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
			const uint8* pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
			if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return XRT_NET_ERROR; }
			if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { return XRT_NET_AGAIN; }
			
			uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
			
			// Alert 记录直接处理
			if ( iRecType == __XRT_TLS_ALERT ) {
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				return __xrt_tls_handle_alert(pCtx, pRecData, iRecLen);
			}
			
			// Finished 必须以加密后的 Handshake 记录出现
			if ( iRecType == __XRT_TLS_HANDSHAKE ) {
				// 使用服务器写密钥解密记录
				uint8 aPlain[__XRT_TLS_MAX_RECORD];
				size_t iPlainLen = 0;
				uint8 iContentType = 0;
				
				if ( !__xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
					__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType) ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				// 解密后的内容类型必须是握手
				if ( iContentType != __XRT_TLS_HANDSHAKE ) {
					return XRT_NET_ERROR;
				}
				
				// 验证 Finished 消息：类型必须为 Finished，verify_data 至少 12 字节
				if ( iPlainLen >= __XRT_TLS_MSGHDR_SIZE && aPlain[0] == __XRT_TLS_FINISHED ) {
					uint32 iFinLen = __xrt_tls_load_be24(aPlain + 1);
					if ( iFinLen >= 12 ) {
						// 验证服务器的 Finished verify_data
						if ( !__xrt_tls12_verify_finished(pCtx, aPlain + __XRT_TLS_MSGHDR_SIZE, iFinLen, true) ) {
							return XRT_NET_ERROR;
						}
						// 会话恢复时，收到服务器 Finished 后客户端需要发送自己的 CCS + Finished
						if ( pCtx->bSessionResumed ) {
							__xrt_tls12_update_hash(pCtx, aPlain, __XRT_TLS_MSGHDR_SIZE + iFinLen);
							if ( !__xrt_tls12_send_ccs_finished(pCtx, false) ) {
								return XRT_NET_ERROR;
							}
						}
						// 握手完成，进入连接状态
						pCtx->bHandshakeDone = true;
						pCtx->iState = XRT_TLS12_CLIENT_CONNECTED;
						return XRT_NET_AGAIN;
					}
				}
				return XRT_NET_ERROR;
			}

			return XRT_NET_ERROR;
		}
		
		case XRT_TLS_CLIENT_CONNECTED:
		case XRT_TLS12_CLIENT_CONNECTED:
			// 发送剩余数据
			if ( pCtx->tSendBuf.iSize > 0 ) {
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
				size_t iSent = 0;
				__xrt_tls_sock_send(hSocket, pCtx->tSendBuf.pData, pCtx->tSendBuf.iSize, &iSent);
				if ( iSent > 0 ) { __xrt_tls_buf_consume(&pCtx->tSendBuf, iSent); }
				if ( pCtx->tSendBuf.iSize > 0 ) { return XRT_NET_AGAIN; }
			}
			return XRT_NET_OK;
		
		case XRT_TLS_SERVER_START:
		{
			// 尝试从缓冲区或网络读取数据
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}
			// 检查是否收到完整记录头
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) { return XRT_NET_AGAIN; }
			{
				// 读取记录头信息
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				const uint8 *pRecData;
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { return XRT_NET_AGAIN; }
				// 必须是握手记录
				if ( (uint8)pCtx->tRecvBuf.pData[0] != __XRT_TLS_HANDSHAKE ) { return XRT_NET_ERROR; }
				pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
				// 必须是 ClientHello 消息
				if ( iRecLen < __XRT_TLS_MSGHDR_SIZE || pRecData[0] != __XRT_TLS_CLIENT_HELLO ) { return XRT_NET_ERROR; }

				// 解析 ClientHello 消息体（ 跳过握手头 ）
				if ( !__xrt_tls_parse_client_hello(pCtx, pRecData + __XRT_TLS_MSGHDR_SIZE,
					iRecLen - __XRT_TLS_MSGHDR_SIZE) ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] parse ClientHello FAILED\n");
					#endif
					return XRT_NET_ERROR;
				}

				// 根据协商的 TLS 版本走不同的处理分支
				if ( pCtx->bIsTls12 ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS12] server negotiated suite=0x%04x curve=0x%04x sig=0x%04x\n",
							pCtx->iCipherSuite, pCtx->iTls12Curve, pCtx->iServerSigAlg);
					#endif
					// 将 ClientHello 消息加入握手哈希
					__xrt_tls12_update_hash(pCtx, pRecData, iRecLen);
					if ( pCtx->bSessionResumed ) {
						// 会话恢复：发送 ServerHello，派生密钥，发送 CCS + Finished
						if ( !__xrt_tls12_send_server_hello(pCtx) ) { return XRT_NET_ERROR; }
						__xrt_tls12_derive_keys(pCtx);
						if ( !__xrt_tls12_send_ccs_finished(pCtx, true) ) { return XRT_NET_ERROR; }
						pCtx->iState = XRT_TLS12_SERVER_WAIT_CCS;
					} else {
						// 完整握手：发送 ServerHello + Certificate + ServerKeyExchange + ServerHelloDone
						if ( !__xrt_tls12_send_server_flight(pCtx) ) { return XRT_NET_ERROR; }
						pCtx->iState = XRT_TLS12_SERVER_WAIT_CKE;
					}
				} else {
					#ifdef DEBUG_TRACE
						printf("    [TLS13] server negotiated suite=0x%04x group=0x%04x sig=0x%04x\n",
							pCtx->iCipherSuite, pCtx->iTls13Group, pCtx->iServerSigAlg);
					#endif
					// TLS 1.3: 将 ClientHello 加入转录哈希
					__xrt_tls13_hash_update(pCtx, pRecData, iRecLen);
					// 发送 ServerHello（ 含 key_share ）
					if ( !__xrt_tls13_send_server_hello(pCtx) ) { return XRT_NET_ERROR; }
					// 派生握手密钥
					__xrt_tls_derive_handshake_keys(pCtx);
					// 发送加密扩展 + 证书 + 证书验证 + Finished
					if ( !__xrt_tls13_send_server_flight(pCtx) ) { return XRT_NET_ERROR; }
					pCtx->iState = XRT_TLS_SERVER_NEGOTIATED;
				}
				// 消费已处理的记录
				__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			}
			return XRT_NET_AGAIN;
		}

		// ==================== TLS 1.2 服务端等待客户端密钥交换 / CCS / Finished ====================
		case XRT_TLS12_SERVER_WAIT_CKE:
		case XRT_TLS12_SERVER_WAIT_CCS:
		case XRT_TLS12_SERVER_WAIT_FINISH: {
			// 尝试从缓冲区或网络读取数据
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}

			// 逐个处理接收缓冲区中的记录
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				const uint8* pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
				if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return XRT_NET_ERROR; }

				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { break; }
				// Alert 记录直接处理
				if ( iRecType == __XRT_TLS_ALERT ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return __xrt_tls_handle_alert(pCtx, pRecData, iRecLen);
				}

				// 状态: 等待 ClientKeyExchange
				if ( pCtx->iState == XRT_TLS12_SERVER_WAIT_CKE ) {
					const uint8 *pRecData;
					size_t iMsgOff = 0;

					// ClientKeyExchange 必须是明文握手记录
					if ( iRecType != __XRT_TLS_HANDSHAKE ) { return XRT_NET_ERROR; }
					pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
					// 将记录负载追加到握手重组缓冲区
					__xrt_tls_buf_append(&pCtx->tHandshakeBuf, (const char*)pRecData, iRecLen);
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);

					// 从重组缓冲区解析完整的握手消息
					while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= pCtx->tHandshakeBuf.iSize ) {
						const uint8 *pMsg = (const uint8*)pCtx->tHandshakeBuf.pData + iMsgOff;
						uint8 iMsgType = pMsg[0];
						uint32 iMsgBodyLen = __xrt_tls_load_be24(pMsg + 1);
						size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;

						if ( iMsgOff + iTotalMsgLen > pCtx->tHandshakeBuf.iSize ) { break; }
						// 必须是 ClientKeyExchange 消息
						if ( iMsgType != __XRT_TLS_CLIENT_KEY_EXCHANGE ) { return XRT_NET_ERROR; }
						// 解析客户端密钥交换（ 提取预主密钥 ）
						if ( !__xrt_tls12_parse_client_key_exchange(pCtx,
							pMsg + __XRT_TLS_MSGHDR_SIZE, iMsgBodyLen) ) {
							return XRT_NET_ERROR;
						}
						// 更新握手哈希并派生工作密钥
						__xrt_tls12_update_hash(pCtx, pMsg, iTotalMsgLen);
						__xrt_tls12_derive_keys(pCtx);
						__xrt_tls_buf_consume(&pCtx->tHandshakeBuf, iMsgOff + iTotalMsgLen);
						// 进入等待 CCS 状态
						pCtx->iState = XRT_TLS12_SERVER_WAIT_CCS;
						iMsgOff = 0;
					}
					continue;
				}

				// 状态: 等待 ChangeCipherSpec
				if ( pCtx->iState == XRT_TLS12_SERVER_WAIT_CCS ) {
					// CCS 必须是 ChangeCipherSpec 记录类型
					if ( iRecType != __XRT_TLS_CHANGE_CIPHER ) { return XRT_NET_ERROR; }
					// 验证 CCS 格式：长度 1，值为 0x01
					if ( iRecLen != 1 || pRecData[0] != 0x01 ) { return XRT_NET_ERROR; }
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					// 切换到服务器读密钥，等待加密的 Finished
					pCtx->iState = XRT_TLS12_SERVER_WAIT_FINISH;
					continue;
				}

				// 状态: 等待加密的 Finished（ 此时记录已是加密的 ）
				if ( iRecType != __XRT_TLS_HANDSHAKE && iRecType != __XRT_TLS_APP_DATA ) {
					return XRT_NET_ERROR;
				}

				{
					// 使用客户端写密钥解密记录
					uint8 aPlain[__XRT_TLS_MAX_RECORD];
					size_t iPlainLen = 0;
					uint8 iContentType = 0;
					size_t iMsgOff = 0;

					if ( !__xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
						__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType) ) {
						return XRT_NET_ERROR;
					}
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					// 解密后遇到 Alert 则处理
					if ( iContentType == __XRT_TLS_ALERT ) { return __xrt_tls_handle_alert(pCtx, aPlain, iPlainLen); }
					// 内容类型必须是握手
					if ( iContentType != __XRT_TLS_HANDSHAKE ) { return XRT_NET_ERROR; }

					// 将解密后的明文追加到握手重组缓冲区
					__xrt_tls_buf_append(&pCtx->tHandshakeBuf, (const char*)aPlain, iPlainLen);
					// 解析 Finished 消息
					while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= pCtx->tHandshakeBuf.iSize ) {
						const uint8 *pMsg = (const uint8*)pCtx->tHandshakeBuf.pData + iMsgOff;
						uint8 iMsgType = pMsg[0];
						uint32 iMsgBodyLen = __xrt_tls_load_be24(pMsg + 1);
						size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;

						if ( iMsgOff + iTotalMsgLen > pCtx->tHandshakeBuf.iSize ) { break; }
						// 必须是 Finished 消息
						if ( iMsgType != __XRT_TLS_FINISHED ) { return XRT_NET_ERROR; }
						// 验证客户端的 Finished verify_data
						if ( !__xrt_tls12_verify_finished(pCtx,
							pMsg + __XRT_TLS_MSGHDR_SIZE, iMsgBodyLen, false) ) {
							return XRT_NET_ERROR;
						}
						// 将客户端 Finished 加入握手哈希
						__xrt_tls12_update_hash(pCtx, pMsg, iTotalMsgLen);
						__xrt_tls_buf_consume(&pCtx->tHandshakeBuf, iMsgOff + iTotalMsgLen);
						// 非恢复会话：服务端需要发送 CCS + Finished 响应
						if ( !pCtx->bSessionResumed ) {
							if ( !__xrt_tls12_send_ccs_finished(pCtx, true) ) { return XRT_NET_ERROR; }
						}
						pCtx->bHandshakeDone = true;
						// 非恢复会话：保存会话票据用于后续恢复
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
		
		// ==================== TLS 1.3 服务端等待客户端 Finished ====================
		case XRT_TLS_SERVER_NEGOTIATED: {
			// 尝试从缓冲区或网络读取数据
			if ( !__xrt_tls_have_record(pCtx) ) {
				if ( !bAllowSocketIO ) { return XRT_NET_AGAIN; }
				char aBuf[4096];
				size_t iRecvd = 0;
				xnet_result iRes = __xrt_tls_sock_recv(hSocket, aBuf, sizeof(aBuf), &iRecvd);
				if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
					__xrt_tls_buf_append(&pCtx->tRecvBuf, aBuf, iRecvd);
				} else if ( iRes == XRT_NET_CLOSED ) {
					return XRT_NET_ERROR;
				}
			}

			// 逐个处理接收缓冲区中的记录
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				const uint8* pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
				if ( !__xrt_tls_record_payload_valid(iRecLen) ) { return XRT_NET_ERROR; }

				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { break; }
				// 忽略 CCS 记录（ TLS 1.3 中间件兼容性 ）
				if ( iRecType == __XRT_TLS_CHANGE_CIPHER ) {
					if ( iRecLen != 1 || pRecData[0] != 0x01 ) { return XRT_NET_ERROR; }
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					continue;
				}
				// Alert 记录直接处理
				if ( iRecType == __XRT_TLS_ALERT ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return __xrt_tls_handle_alert(pCtx, pRecData, iRecLen);
				}
				// 客户端 Finished 必须以加密的 APP_DATA 记录形式出现
				if ( iRecType != __XRT_TLS_APP_DATA ) {
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}

				{
					// 使用客户端握手密钥解密记录
					uint8 aPlain[__XRT_TLS_MAX_RECORD];
					size_t iPlainLen = 0;
					uint8 iContentType = 0;
					if ( !__xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
						__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, false) ) {
						return XRT_NET_ERROR;
					}
					__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);

					// 解密后遇到 Alert 则处理
					if ( iContentType == __XRT_TLS_ALERT ) { return __xrt_tls_handle_alert(pCtx, aPlain, iPlainLen); }
					// 跳过非握手内容（ 如 NewSessionTicket 等 ）
					if ( iContentType != __XRT_TLS_HANDSHAKE ) { continue; }

					// 将解密后的明文追加到握手重组缓冲区
					__xrt_tls_buf_append(&pCtx->tHandshakeBuf, (const char*)aPlain, iPlainLen);
					{
						uint8 *pHsBuf = (uint8*)pCtx->tHandshakeBuf.pData;
						size_t iHsBufLen = pCtx->tHandshakeBuf.iSize;
						size_t iMsgOff = 0;

						// 解析重组缓冲区中的握手消息
						while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= iHsBufLen ) {
							uint8 *pMsg = pHsBuf + iMsgOff;
							uint8 iMsgType = pMsg[0];
							uint32 iMsgBodyLen = __xrt_tls_load_be24(pMsg + 1);
							size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;
							uint8 aHash[64];
							size_t iHashLen = 0;

							if ( iMsgOff + iTotalMsgLen > iHsBufLen ) { break; }
							// 必须是 Finished 消息
							if ( iMsgType != __XRT_TLS_FINISHED ) { return XRT_NET_ERROR; }
							// 获取当前转录哈希用于验证 Finished
							__xrt_tls13_get_transcript_hash(pCtx, aHash, &iHashLen);
							// 验证客户端的 Finished verify_data
							if ( !__xrt_tls_verify_finished(pCtx, pMsg + __XRT_TLS_MSGHDR_SIZE,
								iMsgBodyLen, aHash, iHashLen, false) ) {
								return XRT_NET_ERROR;
							}
							// 将客户端 Finished 加入转录哈希
							__xrt_tls13_hash_update(pCtx, pMsg, iTotalMsgLen);
							__xrt_tls_buf_consume(&pCtx->tHandshakeBuf, iMsgOff + iTotalMsgLen);
							// 握手完成，进入连接状态
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


// 执行 TLS 握手（ 阻塞模式，允许 socket IO ）
XXAPI xnet_result xrtTlsHandshake(xtlsctx *pCtx, xsocket hSocket)
{
	xnet_result iRes;

	if ( !pCtx ) { return XRT_NET_ERROR; }
	__xrt_tls_ctx_lock(pCtx);
	// 允许 socket IO 的阻塞式握手驱动
	iRes = __xrt_tls_drive_internal(pCtx, hSocket, true);
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// 驱动 TLS 状态机（ 非阻塞模式，仅处理缓冲区数据 ）
XXAPI xnet_result xrtTlsDrive(xtlsctx *pCtx)
{
	xnet_result iRes;

	if ( !pCtx ) { return XRT_NET_ERROR; }
	__xrt_tls_ctx_lock(pCtx);
	// 禁止 socket IO，仅消费缓冲区中已有的数据
	iRes = __xrt_tls_drive_internal(pCtx, XSOCKET_INVALID, false);
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// 读取 TLS 解密后的应用数据
XXAPI xnet_result xrtTlsRead(xtlsctx *pCtx, char *pBuf, size_t iLen, size_t *pRead)
{
	xnet_result iRes = XRT_NET_AGAIN;

	if ( !pCtx || !pBuf || iLen == 0 ) { return XRT_NET_ERROR; }
	__xrt_tls_ctx_lock(pCtx);
	// 握手未完成时返回 AGAIN
	if ( !pCtx->bHandshakeDone ) {
		iRes = XRT_NET_AGAIN;
		goto Exit;
	}
	if ( pRead ) { *pRead = 0; }
	
	// 优先从明文缓冲区读取上次未读完的剩余数据
	if ( pCtx->tPlainBuf.iSize > 0 ) {
		size_t iCopy = pCtx->tPlainBuf.iSize < iLen ? pCtx->tPlainBuf.iSize : iLen;
		memcpy(pBuf, pCtx->tPlainBuf.pData, iCopy);
		__xrt_tls_buf_consume(&pCtx->tPlainBuf, iCopy);
		if ( pRead ) { *pRead = iCopy; }
		iRes = XRT_NET_OK;
		goto Exit;
	}
	// 已收到 close_notify，返回连接关闭
	if ( pCtx->bCloseNotifyReceived ) {
		iRes = XRT_NET_CLOSED;
		goto Exit;
	}
	
	// 逐个解密接收缓冲区中的加密记录
	while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
		uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
		uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
		const uint8* pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
		uint8 aPlain[__XRT_TLS_MAX_RECORD];
		size_t iPlainLen = 0;
		uint8 iContentType = 0;
		bool bOK;
		
		// 验证记录长度是否合法
		if ( !__xrt_tls_record_payload_valid(iRecLen) ) {
			iRes = XRT_NET_ERROR;
			goto Exit;
		}
		// 数据不完整则等待更多数据
		if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) { break; }
		// 明文 Alert 记录直接处理
		if ( iRecType == __XRT_TLS_ALERT ) {
			__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			iRes = __xrt_tls_handle_alert(pCtx, pRecData, iRecLen);
			goto Exit;
		}
		
		// 根据 TLS 版本选择对应的解密函数
		if ( pCtx->bIsTls12 ) {
			bOK = __xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
				__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType);
		} else {
			// TLS 1.3: 客户端用服务器密钥解密，服务端用客户端密钥解密
			bool bUseServerKeys = !pCtx->bIsServer;
			bOK = __xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
				__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, bUseServerKeys);
		}
		
		if ( !bOK ) {
			iRes = XRT_NET_ERROR;
			goto Exit;
		}
		__xrt_tls_buf_consume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
		
		// 解密成功后将应用数据追加到明文缓冲区
		if ( iContentType == __XRT_TLS_APP_DATA ) {
			if ( iPlainLen > 0 ) {
				if ( !__xrt_tls_buf_append(&pCtx->tPlainBuf, (const char*)aPlain, iPlainLen) ) {
					iRes = XRT_NET_ERROR;
					goto Exit;
				}
				break;
			}
			continue;
		}
		// 解密后遇到 Alert 则处理
		if ( iContentType == __XRT_TLS_ALERT ) {
			iRes = __xrt_tls_handle_alert(pCtx, aPlain, iPlainLen);
			goto Exit;
		}
	}
	
	// 从明文缓冲区复制数据到用户缓冲区
	if ( pCtx->tPlainBuf.iSize > 0 ) {
		size_t iCopy = pCtx->tPlainBuf.iSize < iLen ? pCtx->tPlainBuf.iSize : iLen;
		memcpy(pBuf, pCtx->tPlainBuf.pData, iCopy);
		__xrt_tls_buf_consume(&pCtx->tPlainBuf, iCopy);
		if ( pRead ) { *pRead = iCopy; }
		iRes = XRT_NET_OK;
		goto Exit;
	}
	// 检查是否已收到 close_notify
	if ( pCtx->bCloseNotifyReceived ) {
		iRes = XRT_NET_CLOSED;
		goto Exit;
	}
	
	iRes = XRT_NET_AGAIN;

Exit:
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// 写入 TLS：将应用数据分片加密后写入发送缓冲区
XXAPI xnet_result xrtTlsWrite(xtlsctx *pCtx, const char *pData, size_t iLen, size_t *pWritten)
{
	size_t iOffset = 0;
	size_t iChunkLimit;
	xnet_result iRes = XRT_NET_OK;

	if ( !pCtx || !pData || iLen == 0 ) { return XRT_NET_ERROR; }
	__xrt_tls_ctx_lock(pCtx);
	if ( !pCtx->bHandshakeDone ) {
		iRes = XRT_NET_AGAIN;
		goto Exit;
	}

	/*
		TLS record payload has a hard upper bound. Large HTTP bodies must be
		split into multiple records, otherwise the 16-bit record length wraps
		and clients fail while decrypting subsequent application data.
		TLS 1.2 上限 16384 字节，TLS 1.3 因内容类型后缀需减 1
	*/
	iChunkLimit = pCtx->bIsTls12 ? 16384u : 16383u;

	// 按 TLS 记录最大负载分片加密
	while ( iOffset < iLen ) {
		size_t iChunk = iLen - iOffset;

		if ( iChunk > iChunkLimit ) {
			iChunk = iChunkLimit;
		}

		// 根据 TLS 版本选择加密函数
		if ( pCtx->bIsTls12 ) {
			if ( !__xrt_tls12_encrypt_record(pCtx, __XRT_TLS_APP_DATA, (const uint8*)pData + iOffset, iChunk) ) {
				iRes = XRT_NET_ERROR;
				goto Exit;
			}
		} else {
			// TLS 1.3: 客户端用客户端密钥加密，服务端用服务端密钥加密
			bool bUseServerKeys = pCtx->bIsServer;
			if ( !__xrt_tls_encrypt_record(pCtx, __XRT_TLS_APP_DATA, (const uint8*)pData + iOffset, iChunk, bUseServerKeys) ) {
				iRes = XRT_NET_ERROR;
				goto Exit;
			}
		}

		iOffset += iChunk;
	}

	if ( pWritten ) { *pWritten = iLen; }
	iRes = XRT_NET_OK;

Exit:
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// 关闭 TLS 连接：发送 close_notify 警告
XXAPI xnet_result xrtTlsClose(xtlsctx *pCtx)
{
	xnet_result iRes = XRT_NET_OK;

	if ( !pCtx ) { return XRT_NET_ERROR; }
	__xrt_tls_ctx_lock(pCtx);
	if ( !pCtx->bHandshakeDone ) {
		iRes = XRT_NET_AGAIN;
		goto Exit;
	}
	// 已发送过 close_notify，根据是否收到对端响应返回状态
	if ( pCtx->bCloseNotifySent ) {
		iRes = pCtx->bCloseNotifyReceived ? XRT_NET_OK : XRT_NET_AGAIN;
		goto Exit;
	}
	
	// 发送 close_notify alert
	uint8 aAlert[2] = { __XRT_TLS_ALERT_WARNING, __XRT_TLS_ALERT_CLOSE_NOTIFY };
	// 根据 TLS 版本加密 Alert 记录
	if ( pCtx->bIsTls12 ) {
		if ( !__xrt_tls12_encrypt_record(pCtx, __XRT_TLS_ALERT, aAlert, 2) ) {
			iRes = XRT_NET_ERROR;
			goto Exit;
		}
	} else {
		bool bUseServerKeys = pCtx->bIsServer;
		if ( !__xrt_tls_encrypt_record(pCtx, __XRT_TLS_ALERT, aAlert, 2, bUseServerKeys) ) {
			iRes = XRT_NET_ERROR;
			goto Exit;
		}
	}
	pCtx->bCloseNotifySent = true;
	
	iRes = pCtx->bCloseNotifyReceived ? XRT_NET_OK : XRT_NET_AGAIN;

Exit:
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// xrtTlsIsReady 相关处理
XXAPI bool xrtTlsIsReady(xtlsctx *pCtx)
{
	bool bReady;

	if ( !pCtx ) { return false; }
	__xrt_tls_ctx_lock(pCtx);
	bReady = pCtx->bHandshakeDone;
	__xrt_tls_ctx_unlock(pCtx);
	return bReady;
}


// xrtTlsFeed 相关处理
XXAPI xnet_result xrtTlsFeed(xtlsctx *pCtx, const char *pData, size_t iLen)
{
	xnet_result iRes;

	if ( !pCtx || !pData || iLen == 0 ) { return XRT_NET_ERROR; }
	__xrt_tls_ctx_lock(pCtx);
	iRes = __xrt_tls_buf_append(&pCtx->tRecvBuf, pData, iLen) ? XRT_NET_OK : XRT_NET_ERROR;
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// xrtTlsPendingSend 相关处理
XXAPI size_t xrtTlsPendingSend(xtlsctx *pCtx)
{
	size_t iPending;

	if ( !pCtx ) { return 0; }
	__xrt_tls_ctx_lock(pCtx);
	iPending = pCtx->tSendBuf.iSize;
	__xrt_tls_ctx_unlock(pCtx);
	return iPending;
}


// xrtTlsPendingRecv 相关处理
XXAPI size_t xrtTlsPendingRecv(xtlsctx *pCtx)
{
	size_t iPending;

	if ( !pCtx ) { return 0; }
	__xrt_tls_ctx_lock(pCtx);
	iPending = pCtx->tRecvBuf.iSize;
	__xrt_tls_ctx_unlock(pCtx);
	return iPending;
}


// xrtTlsPeekSend 相关处理
XXAPI xnet_result xrtTlsPeekSend(xtlsctx *pCtx, char *pBuf, size_t iLen, size_t *pRead)
{
	size_t iCopy;
	xnet_result iRes;

	if ( pRead ) { *pRead = 0; }
	if ( !pCtx || !pBuf || iLen == 0 ) { return XRT_NET_ERROR; }
	__xrt_tls_ctx_lock(pCtx);
	// 检查发送缓冲区是否有待发送数据
	if ( pCtx->tSendBuf.iSize == 0 ) {
		iRes = XRT_NET_AGAIN;
		goto Exit;
	}
	// 取缓冲区数据和请求长度的较小值
	iCopy = pCtx->tSendBuf.iSize < iLen ? pCtx->tSendBuf.iSize : iLen;
	memcpy(pBuf, pCtx->tSendBuf.pData, iCopy);
	if ( pRead ) { *pRead = iCopy; }
	iRes = XRT_NET_OK;

Exit:
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// xrtTlsConsumeSend 相关处理
XXAPI void xrtTlsConsumeSend(xtlsctx *pCtx, size_t iLen)
{
	if ( !pCtx || iLen == 0 ) { return; }
	__xrt_tls_ctx_lock(pCtx);
	if ( pCtx->tSendBuf.iSize == 0 ) {
		__xrt_tls_ctx_unlock(pCtx);
		return;
	}
	// 请求消费长度 >= 缓冲区大小，清空全部发送缓冲区
	if ( iLen >= pCtx->tSendBuf.iSize ) {
		__xrt_tls_buf_consume(&pCtx->tSendBuf, pCtx->tSendBuf.iSize);
		__xrt_tls_ctx_unlock(pCtx);
		return;
	}
	// 部分消费：仅移除前 iLen 字节
	__xrt_tls_buf_consume(&pCtx->tSendBuf, iLen);
	__xrt_tls_ctx_unlock(pCtx);
}


// xrtTlsExportResume 相关处理
XXAPI xtlsresume* xrtTlsExportResume(xtlsctx *pCtx)
{
	xtlsresume* pResume;
	if ( !pCtx ) { return NULL; }
	__xrt_tls_ctx_lock(pCtx);
	// 分配会话恢复数据结构
	pResume = (xtlsresume*)xrtCalloc(1, sizeof(xtlsresume));
	if ( !pResume ) {
		__xrt_tls_ctx_unlock(pCtx);
		return NULL;
	}
	// 从当前 TLS 上下文导出会话恢复信息
	if ( !__xrt_tls_resume_from_ctx(pCtx, pResume) ) {
		__xrt_tls_ctx_unlock(pCtx);
		xrtFree(pResume);
		return NULL;
	}
	__xrt_tls_ctx_unlock(pCtx);
	return pResume;
}


// xrtTlsResumeDestroy 相关处理
XXAPI void xrtTlsResumeDestroy(xtlsresume* pResume)
{
	if ( !pResume ) { return; }
	memset(pResume, 0, sizeof(*pResume));
	xrtFree(pResume);
}


// xrtTlsWasResumed 相关处理
XXAPI bool xrtTlsWasResumed(xtlsctx *pCtx)
{
	bool bWasResumed;

	if ( !pCtx ) { return false; }
	__xrt_tls_ctx_lock(pCtx);
	bWasResumed = pCtx->bSessionResumed;
	__xrt_tls_ctx_unlock(pCtx);
	return bWasResumed;
}

static bool __xrt_tls_send_finished(xtlsctx *pCtx, bool bAsServer);
static void __xrt_tls_derive_application_keys(xtlsctx *pCtx);

// Step 12: 服务端 ClientHello 解析 （ 提取 SNI ）
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
	bool bHasNullCompression = false;
	bool bPeerSigECDSA256 = false, bPeerSigECDSA384 = false, bPeerSigECDSA512 = false;
	bool bPeerSigRSAPKCS1256 = false, bPeerSigRSAPKCS1384 = false, bPeerSigRSAPKCS1512 = false;
	uint8 aX25519Peer[32];
	uint8 aX448Peer[56];
	uint8 aP256Peer[65];
	uint8 aP384Peer[97];
	bool bHaveX25519 = false, bHaveX448 = false, bHaveP256 = false, bHaveP384 = false;
	struct xrt_tls_resume tCachedResume;
	bool bAllowTls13 = false;

	if ( !pCtx ) { return false; }
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
	pCtx->sAlpnSelected[0] = '\0';
	pCtx->bIsTls12 = false;
	pCtx->bSessionResumed = false;
	bAllowTls13 = (pCtx->iMaxVersion == 0 || pCtx->iMaxVersion >= __XRT_TLS_VERSION_1_3);
	
	// client_version(2)
	if ( iPos + 2 > iLen ) { return false; }
	iLegacyVersion = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	// client_random(32) — 保存到上下文
	if ( iPos + 32 > iLen ) { return false; }
	memcpy(pCtx->aRandom, pMsg + iPos, 32);
	iPos += 32;
	
	// session_id
	if ( iPos + 1 > iLen ) { return false; }
	uint8 iSessLen = pMsg[iPos++];
	if ( iSessLen > 32 || iPos + iSessLen > iLen ) { return false; }
	pCtx->iSessionIdLen = iSessLen;
	if ( iSessLen <= 32 ) {
		memcpy(pCtx->aSessionId, pMsg + iPos, iSessLen);
	}
	iPos += iSessLen;
	
	// cipher_suites
	if ( iPos + 2 > iLen ) { return false; }
	uint16 iSuitesLen = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	if ( iPos + iSuitesLen > iLen ) { return false; }
	for ( size_t i = 0; i + 1 < iSuitesLen; i += 2 ) {
		uint16 iSuite = __xrt_tls_load_be16(pMsg + iPos + i);
		if ( iSuite == __XRT_TLS_AES_128_GCM_SHA256 ) { bOfferTLS13AES128 = true; }
		else if ( iSuite == __XRT_TLS_AES_256_GCM_SHA384 ) { bOfferTLS13AES256 = true; }
		else if ( iSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) { bOfferTLS13ChaCha = true; }
		else if ( iSuite == 0x00ff ) { pCtx->bPeerSecureReneg = true; }
		else if ( iSuite == __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256 ) { bOfferTLS12EcdheEcdsaAES128 = true; }
		else if ( iSuite == __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384 ) { bOfferTLS12EcdheEcdsaAES256 = true; }
		else if ( iSuite == __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256 ) { bOfferTLS12EcdheEcdsaChaCha = true; }
		else if ( iSuite == __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256 ) { bOfferTLS12EcdheRsaAES128 = true; }
		else if ( iSuite == __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384 ) { bOfferTLS12EcdheRsaAES256 = true; }
		else if ( iSuite == __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256 ) { bOfferTLS12EcdheRsaChaCha = true; }
	}
	iPos += iSuitesLen;
	
	// compression_methods
	if ( iPos + 1 > iLen ) { return false; }
	uint8 iCompLen = pMsg[iPos++];
	if ( iPos + iCompLen > iLen ) { return false; }
	for ( size_t i = 0; i < iCompLen; i++ ) {
		if ( pMsg[iPos + i] == 0 ) {
			bHasNullCompression = true;
			break;
		}
	}
	if ( !bHasNullCompression ) { return false; }
	iPos += iCompLen;
	
	// 解析扩展
	size_t iExtEnd = iPos;
	if ( iPos + 2 <= iLen ) {
		uint16 iExtTotalLen = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		iExtEnd = iPos + iExtTotalLen;
		if ( iExtEnd > iLen ) { iExtEnd = iLen; }
	}
	
	while ( iPos + 4 <= iExtEnd ) {
		uint16 iExtType = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		uint16 iExtDataLen = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		
		if ( iPos + iExtDataLen > iExtEnd ) { break; }
		
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
		} else if ( iExtType == __XRT_TLS_EXT_ALPN ) {
			if ( !__xrt_tls_alpn_select_from_client(pCtx, pMsg + iPos, iExtDataLen) ) { return false; }
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
					if ( iGroup == 0x001d ) { bGroupX25519 = true; }
					else if ( iGroup == 0x001e ) { bGroupX448 = true; }
					else if ( iGroup == 0x0017 ) { bGroupP256 = true; }
					else if ( iGroup == 0x0018 ) { bGroupP384 = true; }
					iOff += 2;
				}
			}
		} else if ( iExtType == 0x000b ) {  // ec_point_formats
			size_t iOff = iPos;
			bSawPointFormats = true;
			if ( iExtDataLen >= 1 ) {
				uint8 iFmtLen = pMsg[iOff++];
				while ( iOff < iPos + iExtDataLen && iOff < iPos + 1 + iFmtLen ) {
					if ( pMsg[iOff] == 0 ) { bPointFmtUncompressed = true; }
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
					else if ( iSigAlg == 0x0805 ) { pCtx->bPeerSigRSAPSS384 = true; }
					else if ( iSigAlg == 0x0806 ) { pCtx->bPeerSigRSAPSS512 = true; }
					else if ( iSigAlg == 0x0809 ) { pCtx->bPeerSigRSAPSSPSS256 = true; }
					else if ( iSigAlg == 0x080a ) { pCtx->bPeerSigRSAPSSPSS384 = true; }
					else if ( iSigAlg == 0x080b ) { pCtx->bPeerSigRSAPSSPSS512 = true; }
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
					if ( iOff + iKeyLen > iPos + iExtDataLen ) { break; }
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
	// 触发 SNI 回调，允许应用层根据域名动态切换证书
	if ( pCtx->sClientSNI[0] != '\0' && pCtx->OnSNI ) {
		__xrt_tls_ctx_unlock(pCtx);
		pCtx->OnSNI(pCtx->pSession, pCtx->sClientSNI, pCtx->pSNIUserData);
		__xrt_tls_ctx_lock(pCtx);
	}

	// 优先协商 TLS 1.3
	if ( bAllowTls13 && bTls13Supported ) {
		// 选择密码套件，优先 AES-256，其次 ChaCha20，最后 AES-128
		if ( bOfferTLS13AES256 ) { pCtx->iCipherSuite = __XRT_TLS_AES_256_GCM_SHA384; }
		else if ( bOfferTLS13ChaCha ) { pCtx->iCipherSuite = __XRT_TLS_CHACHA20_POLY1305_SHA256; }
		else if ( bOfferTLS13AES128 ) { pCtx->iCipherSuite = __XRT_TLS_AES_128_GCM_SHA256; }
		else { pCtx->iCipherSuite = 0; }

		if ( pCtx->iCipherSuite != 0 ) {
			// 根据 key_share 扩展中的密钥交换组，完成 ECDHE 密钥协商
			if ( bHaveX25519 ) {
				// X25519 密钥交换
				pCtx->iTls13Group = 0x001d;
				pCtx->iPeerKeyShareLen = 32;
				memcpy(pCtx->aPeerKeyShare, aX25519Peer, 32);
				xrtX25519Keypair(pCtx->aX25519Priv, pCtx->aX25519Pub);
				xrtX25519SharedSecret(pCtx->aX25519Secret, pCtx->aX25519Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 32;
			} else if ( bHaveX448 ) {
				// X448 密钥交换
				pCtx->iTls13Group = 0x001e;
				pCtx->iPeerKeyShareLen = 56;
				memcpy(pCtx->aPeerKeyShare, aX448Peer, 56);
				xrtX448Keypair(pCtx->aX448Priv, pCtx->aX448Pub);
				xrtX448SharedSecret(pCtx->aX25519Secret, pCtx->aX448Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 56;
			} else if ( bHaveP256 ) {
				// P-256 密钥交换
				pCtx->iTls13Group = 0x0017;
				pCtx->iPeerKeyShareLen = 65;
				memcpy(pCtx->aPeerKeyShare, aP256Peer, 65);
				xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
				xrtECDHSecp256r1SharedSecret(pCtx->aX25519Secret, pCtx->aP256Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 32;
			} else if ( bHaveP384 ) {
				// P-384 密钥交换
				pCtx->iTls13Group = 0x0018;
				pCtx->iPeerKeyShareLen = 97;
				memcpy(pCtx->aPeerKeyShare, aP384Peer, 97);
				xrtECDHSecp384r1Keypair(pCtx->aP384Priv, pCtx->aP384Pub);
				xrtECDHSecp384r1SharedSecret(pCtx->aX25519Secret, pCtx->aP384Priv, pCtx->aPeerKeyShare);
				pCtx->iTls13SecretLen = 48;
			}

			// 密钥交换成功，选择与客户端兼容的签名算法
			if ( pCtx->iTls13Group != 0 ) {
				if ( pCtx->bIsEd25519Key ) {
					// Ed25519 签名
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

		// TLS 1.3 协商失败，清理状态准备回退
		pCtx->iTls13Group = 0;
		pCtx->iPeerKeyShareLen = 0;
	}

	// 回退到 TLS 1.2
	if ( iLegacyVersion != __XRT_TLS_VERSION_1_2 ) { return false; }

	// TLS 1.2 中 Ed25519 不是默认互操作路径, 仅在显式允许时作为 ECDHE_ECDSA 认证证书使用。
	if ( pCtx->bIsEd25519Key && !pCtx->bAllowTLS12Ed25519 ) { return false; }

	// 尝试 TLS 1.2 会话恢复：匹配 session_id 和密码套件
	if ( pCtx->iSessionIdLen > 0
		&& __xrt_tls_resume_cache_lookup(pCtx->aSessionId, pCtx->iSessionIdLen, &tCachedResume)
		&& __xrt_tls_resume_matches_identity(pCtx, &tCachedResume)
		&& __xrt_tls12_client_offers_suite(tCachedResume.iCipherSuite,
			bOfferTLS12EcdheEcdsaAES128,
			bOfferTLS12EcdheEcdsaAES256,
			bOfferTLS12EcdheEcdsaChaCha,
			bOfferTLS12EcdheRsaAES128,
			bOfferTLS12EcdheRsaAES256,
			bOfferTLS12EcdheRsaChaCha) ) {
		pCtx->iCipherSuite = tCachedResume.iCipherSuite;
		if ( !__xrt_tls12_set_cipher_params(pCtx, pCtx->iCipherSuite) ) { return false; }
		// 恢复上次会话的 master secret
		memcpy(pCtx->aMasterSecret, tCachedResume.aMasterSecret, sizeof(pCtx->aMasterSecret));
		pCtx->bIsTls12 = true;
		pCtx->bSessionResumed = true;
		return true;
	}

	// 根据证书类型选择 ECDHE_ECDSA 或 ECDHE_RSA 密码套件
	if ( pCtx->bIsECPubKey || pCtx->bIsEd25519Key ) {
		// EC/Ed25519 证书 → 选择 ECDHE_ECDSA 套件
		if ( bOfferTLS12EcdheEcdsaAES256 ) { pCtx->iCipherSuite = __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384; }
		else if ( bOfferTLS12EcdheEcdsaChaCha ) { pCtx->iCipherSuite = __XRT_TLS12_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256; }
		else if ( bOfferTLS12EcdheEcdsaAES128 ) { pCtx->iCipherSuite = __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256; }
		else { return false; }
	} else {
		// RSA 证书 → 选择 ECDHE_RSA 套件
		if ( bOfferTLS12EcdheRsaAES256 ) { pCtx->iCipherSuite = __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384; }
		else if ( bOfferTLS12EcdheRsaChaCha ) { pCtx->iCipherSuite = __XRT_TLS12_ECDHE_RSA_CHACHA20_POLY1305_SHA256; }
		else if ( bOfferTLS12EcdheRsaAES128 ) { pCtx->iCipherSuite = __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256; }
		else { return false; }
	}

	// 设置密码套件对应的哈希和密钥长度参数
	if ( !__xrt_tls12_set_cipher_params(pCtx, pCtx->iCipherSuite) ) { return false; }

	// 选择 ECDHE 密钥交换曲线，按优先级尝试
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
		// 客户端未声明 supported_groups，默认使用 P-256
		pCtx->iTls12Curve = 0x0017;
		xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
	} else {
		return false;
	}

	// 验证私钥存在
	if ( pCtx->bIsEd25519Key ) {
		if ( !pCtx->bHasEd25519Priv ) { return false; }
	} else if ( pCtx->bIsECPubKey ) {
		if ( !pCtx->bHasECPrivKey ) { return false; }
	} else {
		if ( !pCtx->bHasRSAPrivKey ) { return false; }
	}

	// 根据证书类型和客户端支持的签名算法，选择服务端签名方案
	if ( pCtx->bIsEd25519Key ) {
		// Ed25519 签名（ 需要客户端和配置都允许 ）
		if ( !pCtx->bAllowTLS12Ed25519 || !pCtx->bHasEd25519Priv || !pCtx->bPeerSigEd25519 ) { return false; }
		pCtx->iServerSigAlg = 0x0807;
	} else if ( pCtx->bIsECPubKey ) {
		// ECDSA 签名，优先选择与密钥长度匹配的哈希
		if ( bPeerSigECDSA256 ) { pCtx->iServerSigAlg = 0x0403; }
		else if ( pCtx->bUseSHA384 && bPeerSigECDSA384 ) { pCtx->iServerSigAlg = 0x0503; }
		else if ( bPeerSigECDSA384 ) { pCtx->iServerSigAlg = 0x0503; }
		else if ( bPeerSigECDSA512 ) { pCtx->iServerSigAlg = 0x0603; }
		else { return false; }
	} else {
		// RSA 签名，区分 RSASSA-PSS 和 PKCS#1 v1.5
		if ( pCtx->bIsRSAPSSKey ) {
			// RSA-PSS 证书：优先匹配密码套件哈希长度的 PSS 签名
			if ( pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSSPSS384 ) { pCtx->iServerSigAlg = 0x080a; }
			else if ( !pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSSPSS256 ) { pCtx->iServerSigAlg = 0x0809; }
			else if ( pCtx->bPeerSigRSAPSSPSS256 ) { pCtx->iServerSigAlg = 0x0809; }
			else if ( pCtx->bPeerSigRSAPSSPSS384 ) { pCtx->iServerSigAlg = 0x080a; }
			else if ( pCtx->bPeerSigRSAPSSPSS512 ) { pCtx->iServerSigAlg = 0x080b; }
			else { return false; }
		} else {
			// 普通 RSA 证书：优先 PSS，回退到 PKCS#1 v1.5
			if ( pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSS384 ) { pCtx->iServerSigAlg = 0x0805; }
			else if ( !pCtx->bUseSHA384 && pCtx->bPeerSigRSAPSS256 ) { pCtx->iServerSigAlg = 0x0804; }
			else if ( pCtx->bUseSHA384 && bPeerSigRSAPKCS1384 ) { pCtx->iServerSigAlg = 0x0501; }
			else if ( !pCtx->bUseSHA384 && bPeerSigRSAPKCS1256 ) { pCtx->iServerSigAlg = 0x0401; }
			else if ( pCtx->bPeerSigRSAPSS256 ) { pCtx->iServerSigAlg = 0x0804; }
			else if ( pCtx->bPeerSigRSAPSS384 ) { pCtx->iServerSigAlg = 0x0805; }
			else if ( pCtx->bPeerSigRSAPSS512 ) { pCtx->iServerSigAlg = 0x0806; }
			else if ( bPeerSigRSAPKCS1256 ) { pCtx->iServerSigAlg = 0x0401; }
			else if ( bPeerSigRSAPKCS1384 ) { pCtx->iServerSigAlg = 0x0501; }
			else if ( bPeerSigRSAPKCS1512 ) { pCtx->iServerSigAlg = 0x0601; }
			else { return false; }
		}
	}

	return true;
}


// 内部函数：__xrt_tls_sign_server_hash
static bool __xrt_tls_sign_server_hash(xtlsctx *pCtx, const uint8 *pHash, size_t iHashLen,
	uint8 *pSig, size_t *pSigLen)
{
	if ( !pCtx || !pHash || !pSig || !pSigLen ) { return false; }
	// Ed25519 签名
	if ( pCtx->bIsEd25519Key ) {
		if ( !pCtx->bHasEd25519Priv || pCtx->iServerSigAlg != 0x0807 ) { return false; }
		*pSigLen = 64;
		return xrtEd25519Sign(pSig, pHash, iHashLen, pCtx->aEd25519Key);
	}
	// ECDSA 签名（ P-256 或 P-384 ）
	if ( pCtx->bIsECPubKey ) {
		if ( !pCtx->bHasECPrivKey ) { return false; }
		if ( pCtx->iPubKeySz == 65 || pCtx->iECKeyLen == 32 ) {
			// P-256 密钥签名
			if ( pCtx->iServerSigAlg != 0x0403 && pCtx->iServerSigAlg != 0x0503 && pCtx->iServerSigAlg != 0x0603 ) { return false; }
			return __xrt_ecdsa_sign_p256(pHash, iHashLen, pCtx->aECKey, pSig, pSigLen);
		}
		if ( pCtx->iPubKeySz == 97 || pCtx->iECKeyLen == 48 ) {
			// P-384 密钥签名
			if ( pCtx->iServerSigAlg != 0x0403 && pCtx->iServerSigAlg != 0x0503 && pCtx->iServerSigAlg != 0x0603 ) { return false; }
			return __xrt_ecdsa_sign_p384(pHash, iHashLen, pCtx->aECKey, pSig, pSigLen);
		}
		return false;
	}
	// RSA 签名
	if ( !pCtx->bHasRSAPrivKey || pCtx->iPubKeyModSz == 0 ) { return false; }

	*pSigLen = pCtx->iPubKeyModSz;
	// RSASSA-PSS 签名
	if ( pCtx->iServerSigAlg == 0x0804 || pCtx->iServerSigAlg == 0x0805 || pCtx->iServerSigAlg == 0x0806 ||
		pCtx->iServerSigAlg == 0x0809 || pCtx->iServerSigAlg == 0x080a || pCtx->iServerSigAlg == 0x080b ) {
		return __xrt_rsa_pss_sign(pHash, iHashLen, pCtx->aPubKey, pCtx->iPubKeyModSz,
			pCtx->aRSAPrivExp, pCtx->iRSAPrivExpSz, pSig, *pSigLen);
	}
	// PKCS#1 v1.5 签名
	if ( pCtx->iServerSigAlg == 0x0401 || pCtx->iServerSigAlg == 0x0501 || pCtx->iServerSigAlg == 0x0601 ) {
		return __xrt_rsa_pkcs1_sign(pHash, iHashLen, pCtx->aPubKey, pCtx->iPubKeyModSz,
			pCtx->aRSAPrivExp, pCtx->iRSAPrivExpSz, pSig, *pSigLen);
	}
	return false;
}


// 内部函数：__xrt_tls13_send_server_hello
static bool __xrt_tls13_send_server_hello(xtlsctx *pCtx)
{
	uint8 aBuf[256];
	uint8 aRec[5];
	size_t iPos = 0;
	size_t iLenPos;
	size_t iKeyLen;

	if ( !pCtx ) { return false; }
	// 确定密钥交换的公钥长度
	if ( pCtx->iTls13Group == 0x001d ) { iKeyLen = 32; }
	else if ( pCtx->iTls13Group == 0x001e ) { iKeyLen = 56; }
	else if ( pCtx->iTls13Group == 0x0017 ) { iKeyLen = 65; }
	else if ( pCtx->iTls13Group == 0x0018 ) { iKeyLen = 97; }
	else { return false; }

	// 生成服务端随机数
	xrtRandomBytes(pCtx->aServerRandom, 32);
	// 构造 ServerHello 消息体
	aBuf[iPos++] = __XRT_TLS_SERVER_HELLO;
	iLenPos = iPos;
	iPos += 3; // 预留 3 字节消息长度
	// legacy_version: TLS 1.2 （ 兼容性要求 ）
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	// server_random（ 32 字节 ）
	memcpy(aBuf + iPos, pCtx->aServerRandom, 32);
	iPos += 32;
	// legacy_session_id_echo
	aBuf[iPos++] = pCtx->iSessionIdLen;
	memcpy(aBuf + iPos, pCtx->aSessionId, pCtx->iSessionIdLen);
	iPos += pCtx->iSessionIdLen;
	// cipher_suite
	__xrt_tls_store_be16(aBuf + iPos, pCtx->iCipherSuite);
	iPos += 2;
	// legacy_compression_method: null
	aBuf[iPos++] = 0;

	// 构造扩展区域
	{
		size_t iExtPos = iPos;
		iPos += 2; // 预留扩展总长度

		// supported_versions 扩展：声明 TLS 1.3
		__xrt_tls_store_be16(aBuf + iPos, 0x002b);
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, 2); // 扩展数据长度
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_3);
		iPos += 2;

		// key_share 扩展：包含服务端 ECDHE 公钥
		__xrt_tls_store_be16(aBuf + iPos, 0x0033);
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, (uint16)(4 + iKeyLen)); // 扩展数据长度 = group(2) + key_len(2) + pubkey
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, pCtx->iTls13Group);
		iPos += 2;
		__xrt_tls_store_be16(aBuf + iPos, (uint16)iKeyLen);
		iPos += 2;
		// 写入对应曲线的公钥
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

		// 回填扩展总长度
		__xrt_tls_store_be16(aBuf + iExtPos, (uint16)(iPos - iExtPos - 2));
	}

	// 回填消息体长度
	__xrt_tls_store_be24(aBuf + iLenPos, (uint32)(iPos - iLenPos - 3));
	// 更新 TLS 1.3 握手哈希
	__xrt_tls13_hash_update(pCtx, aBuf, iPos);
	// 构造记录层头部并发送
	__xrt_tls_store_be16(aRec + 1, __XRT_TLS_VERSION_1_2); // legacy_record_version
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 3, (uint16)iPos); // 记录长度
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aRec, 5);
	__xrt_tls_buf_append(&pCtx->tSendBuf, (const char*)aBuf, iPos);
	return true;
}


// 内部函数：__xrt_tls13_send_encrypted_extensions
static bool __xrt_tls13_send_encrypted_extensions(xtlsctx *pCtx)
{
	uint8 aMsg[512];
	size_t iPos = 0;
	size_t iLenPos;
	size_t iExtStart;
	if ( !pCtx ) { return false; }

	aMsg[iPos++] = __XRT_TLS_ENCRYPTED_EXTENSIONS;
	iLenPos = iPos;
	iPos += 3;
	iExtStart = iPos;
	iPos += 2;
	if ( pCtx->sAlpnSelected[0] != '\0' ) {
		size_t iProtoLen = strlen(pCtx->sAlpnSelected);
		if ( iProtoLen == 0u || iProtoLen > 255u || iPos + 7u + iProtoLen > sizeof(aMsg) ) { return false; }
		__xrt_tls_store_be16(aMsg + iPos, __XRT_TLS_EXT_ALPN);
		iPos += 2;
		__xrt_tls_store_be16(aMsg + iPos, (uint16)(3u + iProtoLen));
		iPos += 2;
		__xrt_tls_store_be16(aMsg + iPos, (uint16)(1u + iProtoLen));
		iPos += 2;
		aMsg[iPos++] = (uint8)iProtoLen;
		memcpy(aMsg + iPos, pCtx->sAlpnSelected, iProtoLen);
		iPos += iProtoLen;
	}
	__xrt_tls_store_be16(aMsg + iExtStart, (uint16)(iPos - iExtStart - 2u));
	__xrt_tls_store_be24(aMsg + iLenPos, (uint32)(iPos - iLenPos - 3u));
	__xrt_tls13_hash_update(pCtx, aMsg, iPos);
	return __xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, aMsg, iPos, true);
}


static bool __xrt_tls13_send_certificate(xtlsctx *pCtx)
{
	size_t iBodyLen;
	size_t iMsgLen;
	uint8 *pMsg;

	if ( !pCtx || !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) { return false; }
	// 计算消息体长度：request_context(1) + cert_list_length(3) + cert_length(3) + cert_data + extensions_length(2)
	iBodyLen = 1 + 3 + 3 + pCtx->iCertDerLen + 2;
	iMsgLen = __XRT_TLS_MSGHDR_SIZE + iBodyLen;
	pMsg = (uint8*)xrtMalloc(iMsgLen);
	if ( !pMsg ) { return false; }

	// 构造 Certificate 消息
	pMsg[0] = __XRT_TLS_CERTIFICATE;
	__xrt_tls_store_be24(pMsg + 1, (uint32)iBodyLen);
	pMsg[4] = 0; // request_context（ 服务端发送时为空 ）
	__xrt_tls_store_be24(pMsg + 5, (uint32)(3 + pCtx->iCertDerLen + 2)); // certificate_list 长度
	__xrt_tls_store_be24(pMsg + 8, (uint32)pCtx->iCertDerLen); // 单个证书条目长度
	memcpy(pMsg + 11, pCtx->pCertDer, pCtx->iCertDerLen);
	__xrt_tls_store_be16(pMsg + 11 + pCtx->iCertDerLen, 0); // 证书扩展长度为 0

	__xrt_tls13_hash_update(pCtx, pMsg, iMsgLen);
	{
		bool bOK = __xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, pMsg, iMsgLen, true);
		xrtFree(pMsg);
		return bOK;
	}
}


// 内部函数：__xrt_tls13_send_certificate_verify
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

	if ( !pCtx || pCtx->iServerSigAlg == 0 ) { return false; }
	// 计算握手消息的 transcript hash
	__xrt_tls13_get_transcript_hash(pCtx, aTranscriptHash, &iTranscriptHashLen);
	// 构造 CertificateVerify 签名输入数据
	if ( !__xrt_tls13_build_cert_verify_input(aContentHash, &iContentHashLen,
		aTranscriptHash, iTranscriptHashLen, pCtx->iServerSigAlg, true) ) {
		return false;
	}
	// 使用服务端私钥对签名输入进行签名
	if ( !__xrt_tls_sign_server_hash(pCtx, aContentHash, iContentHashLen, aSig, &iSigLen) ) { return false; }

	// 构造 CertificateVerify 消息
	iMsgLen = __XRT_TLS_MSGHDR_SIZE + 2 + 2 + iSigLen;
	pMsg = (uint8*)xrtMalloc(iMsgLen);
	if ( !pMsg ) { return false; }
	pMsg[0] = __XRT_TLS_CERTIFICATE_VERIFY;
	__xrt_tls_store_be24(pMsg + 1, (uint32)(4 + iSigLen)); // 消息体长度
	__xrt_tls_store_be16(pMsg + 4, pCtx->iServerSigAlg); // 签名算法标识
	__xrt_tls_store_be16(pMsg + 6, (uint16)iSigLen); // 签名长度
	memcpy(pMsg + 8, aSig, iSigLen); // 签名数据

	__xrt_tls13_hash_update(pCtx, pMsg, iMsgLen);
	{
		bool bOK = __xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, pMsg, iMsgLen, true);
		xrtFree(pMsg);
		return bOK;
	}
}


// 内部函数：__xrt_tls13_send_server_flight
static bool __xrt_tls13_send_server_flight(xtlsctx *pCtx)
{
	// 发送 EncryptedExtensions 消息
	if ( !__xrt_tls13_send_encrypted_extensions(pCtx) ) { return false; }
	// 发送 Certificate 消息
	if ( !__xrt_tls13_send_certificate(pCtx) ) { return false; }
	// 发送 CertificateVerify 消息
	if ( !__xrt_tls13_send_certificate_verify(pCtx) ) { return false; }
	// 发送 Finished 消息
	if ( !__xrt_tls_send_finished(pCtx, true) ) { return false; }
	// 派生应用数据密钥
	__xrt_tls_derive_application_keys(pCtx);
	return true;
}


// 内部函数：__xrt_tls_parse_rsa_private_key
static bool __xrt_tls_parse_rsa_private_key(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tSeq, tField;

	if ( !pCtx || !pDer ) { return false; }
	if ( __xrt_der_parse(pDer, iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) { return false; }

	// version
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) { return false; }
	// modulus
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) { return false; }
	// publicExponent
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) { return false; }
	// privateExponent
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) { return false; }

	pCtx->iRSAPrivExpSz = tField.iLen;
	// DER 整数编码可能包含前导 0x00 填充字节，跳过它
	if ( pCtx->iRSAPrivExpSz > 0 && tField.pValue[0] == 0x00 ) { pCtx->iRSAPrivExpSz--; }
	if ( pCtx->iRSAPrivExpSz == 0 || pCtx->iRSAPrivExpSz > sizeof(pCtx->aRSAPrivExp) ) { return false; }

	if ( tField.pValue[0] == 0x00 ) {
		memcpy(pCtx->aRSAPrivExp, tField.pValue + 1, pCtx->iRSAPrivExpSz);
	} else {
		memcpy(pCtx->aRSAPrivExp, tField.pValue, pCtx->iRSAPrivExpSz);
	}
	pCtx->bHasRSAPrivKey = true;
	return true;
}


// 内部函数：__xrt_tls_parse_ec_private_key
static bool __xrt_tls_parse_ec_private_key(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tSeq, tField;

	if ( !pCtx || !pDer ) { return false; }
	if ( __xrt_der_parse(pDer, iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) { return false; }
	// version（ INTEGER ）
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x02 ) { return false; }
	// privateKey（ OCTET STRING ），仅支持 P-256（ 32 字节 ）和 P-384（ 48 字节 ）
	if ( __xrt_der_next(&tSeq, &tField) <= 0 || tField.iType != 0x04 ) { return false; }
	if ( tField.iLen != 32 && tField.iLen != 48 ) { return false; }
	memcpy(pCtx->aECKey, tField.pValue, tField.iLen);
	pCtx->iECKeyLen = tField.iLen;
	pCtx->bHasECPrivKey = true;
	return true;
}


// 内部函数：__xrt_tls_parse_ed25519_private_key
static bool __xrt_tls_parse_ed25519_private_key(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tKey, tSeed;

	if ( !pCtx || !pDer ) { return false; }
	// 直接 32 字节原始种子
	if ( iLen == 32 ) {
		memcpy(pCtx->aEd25519Key, pDer, 32);
		pCtx->bHasEd25519Priv = true;
		return true;
	}
	// DER 编码格式解析
	if ( __xrt_der_parse(pDer, iLen, &tKey) < 0 ) { return false; }
	// OCTET STRING 直接包含 32 字节种子
	if ( tKey.iType == 0x04 && tKey.iLen == 32 ) {
		memcpy(pCtx->aEd25519Key, tKey.pValue, 32);
		pCtx->bHasEd25519Priv = true;
		return true;
	}
	// 嵌套 DER 结构：外层 OCTET STRING 包含内层 OCTET STRING（ 种子 ）
	if ( tKey.iType != 0x04 ) { return false; }
	if ( __xrt_der_parse(tKey.pValue, tKey.iLen, &tSeed) < 0 || tSeed.iType != 0x04 || tSeed.iLen != 32 ) { return false; }
	memcpy(pCtx->aEd25519Key, tSeed.pValue, 32);
	pCtx->bHasEd25519Priv = true;
	return true;
}


// 内部函数：__xrt_tls_parse_private_key_info
static bool __xrt_tls_parse_private_key_info(xtlsctx *pCtx, uint8 *pDer, size_t iLen)
{
	struct __xrt_der_tlv tSeq, tVersion, tAlg, tPriv, tOID;

	if ( !pCtx || !pDer ) { return false; }
	// 解析 PKCS#8 PrivateKeyInfo 顶层 SEQUENCE
	if ( __xrt_der_parse(pDer, iLen, &tSeq) < 0 || tSeq.iType != 0x30 ) { return false; }
	// version（ INTEGER ）
	if ( __xrt_der_next(&tSeq, &tVersion) <= 0 || tVersion.iType != 0x02 ) { return false; }
	// privateKeyAlgorithm（ SEQUENCE ）
	if ( __xrt_der_next(&tSeq, &tAlg) <= 0 || tAlg.iType != 0x30 ) { return false; }
	// privateKey（ OCTET STRING ）
	if ( __xrt_der_next(&tSeq, &tPriv) <= 0 || tPriv.iType != 0x04 ) { return false; }

	// 根据算法 OID 分发到对应的密钥解析函数
	{
		struct __xrt_der_tlv tAlgItems = tAlg;
		if ( __xrt_der_next(&tAlgItems, &tOID) <= 0 || tOID.iType != 0x06 ) { return false; }
		// RSA 或 RSASSA-PSS
		if ( (tOID.iLen == sizeof(__xrt_oid_rsa_encryption) &&
			memcmp(tOID.pValue, __xrt_oid_rsa_encryption, sizeof(__xrt_oid_rsa_encryption)) == 0) ||
			(tOID.iLen == sizeof(__xrt_oid_rsassa_pss) &&
			memcmp(tOID.pValue, __xrt_oid_rsassa_pss, sizeof(__xrt_oid_rsassa_pss)) == 0) ) {
			return __xrt_tls_parse_rsa_private_key(pCtx, tPriv.pValue, tPriv.iLen);
		}
		// EC 公钥（ P-256 / P-384 ）
		if ( tOID.iLen == sizeof(__xrt_oid_ec_public_key) &&
			memcmp(tOID.pValue, __xrt_oid_ec_public_key, sizeof(__xrt_oid_ec_public_key)) == 0 ) {
			return __xrt_tls_parse_ec_private_key(pCtx, tPriv.pValue, tPriv.iLen);
		}
		// Ed25519
		if ( tOID.iLen == sizeof(__xrt_oid_ed25519) &&
			memcmp(tOID.pValue, __xrt_oid_ed25519, sizeof(__xrt_oid_ed25519)) == 0 ) {
			return __xrt_tls_parse_ed25519_private_key(pCtx, tPriv.pValue, tPriv.iLen);
		}
	}
	return false;
}


// 内部函数：__xrt_tls_prepare_server_identity
static bool __xrt_tls_prepare_server_identity(xtlsctx *pCtx)
{
	struct __xrt_x509_cert tCert;

	if ( !pCtx ) { return false; }
	// 清除之前的密钥状态
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

	// 没有证书时直接返回成功（ 匿名模式 ）
	if ( !pCtx->pCertDer || pCtx->iCertDerLen == 0 ) { return true; }
	// 解析 X.509 证书，提取公钥信息
	if ( !__xrt_x509_parse(pCtx->pCertDer, pCtx->iCertDerLen, &tCert) ) { return false; }
	if ( !__xrt_tls_copy_pubkey_from_cert(pCtx, &tCert) ) { return false; }

	// 没有私钥时只设置公钥信息
	if ( !pCtx->pKeyDer || pCtx->iKeyDerLen == 0 ) { return true; }
	// 尝试 PKCS#8 格式解析私钥
	if ( __xrt_tls_parse_private_key_info(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		// 验证 EC 公私钥长度匹配
		if ( pCtx->bIsECPubKey && pCtx->bHasECPrivKey ) {
			if ( (pCtx->iPubKeySz == 65 && pCtx->iECKeyLen != 32)
				|| (pCtx->iPubKeySz == 97 && pCtx->iECKeyLen != 48) ) {
				return false;
			}
		}
		if ( pCtx->bIsEd25519Key ) { return pCtx->bHasEd25519Priv; }
		return pCtx->bIsECPubKey ? pCtx->bHasECPrivKey : pCtx->bHasRSAPrivKey;
	}
	// 回退尝试传统 RSA PKCS#1 格式
	if ( __xrt_tls_parse_rsa_private_key(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		return !pCtx->bIsECPubKey && !pCtx->bIsEd25519Key;
	}
	// 回退尝试传统 EC 格式
	if ( __xrt_tls_parse_ec_private_key(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		if ( pCtx->bIsECPubKey ) {
			if ( (pCtx->iPubKeySz == 65 && pCtx->iECKeyLen != 32)
				|| (pCtx->iPubKeySz == 97 && pCtx->iECKeyLen != 48) ) {
				return false;
			}
		}
		return pCtx->bIsECPubKey;
	}
	// 回退尝试 Ed25519 格式
	if ( __xrt_tls_parse_ed25519_private_key(pCtx, pCtx->pKeyDer, pCtx->iKeyDerLen) ) {
		return pCtx->bIsEd25519Key;
	}
	return false;
}

// Step 13: SNI 相关公共 API

XXAPI const char* xrtTlsGetSNI(xtlsctx *pCtx)
{
	const char* sSNI;

	if ( !pCtx ) { return NULL; }
	__xrt_tls_ctx_lock(pCtx);
	sSNI = (pCtx->sClientSNI[0] == '\0') ? NULL : pCtx->sClientSNI;
	__xrt_tls_ctx_unlock(pCtx);
	return sSNI;
}


XXAPI const char* xrtTlsGetALPN(xtlsctx *pCtx)
{
	const char* sALPN;

	if ( !pCtx ) { return NULL; }
	__xrt_tls_ctx_lock(pCtx);
	sALPN = (pCtx->sAlpnSelected[0] == '\0') ? NULL : pCtx->sAlpnSelected;
	__xrt_tls_ctx_unlock(pCtx);
	return sALPN;
}


// 设置 TLS allow TLS 12 ed 25519
XXAPI void xrtTlsSetAllowTLS12Ed25519(xtlsctx *pCtx, bool bAllow)
{
	if ( !pCtx ) { return; }
	__xrt_tls_ctx_lock(pCtx);
	pCtx->bAllowTLS12Ed25519 = bAllow;
	__xrt_tls_ctx_unlock(pCtx);
}


// 设置 TLS 证书
XXAPI xnet_result xrtTlsSetCert(xtlsctx *pCtx, const char *sCertFile, const char *sKeyFile)
{
	uint8 *pCertDer = NULL;
	size_t iCertDerLen = 0;
	uint8 *pKeyDer = NULL;
	size_t iKeyDerLen = 0;
	xnet_result iRes;

	if ( !pCtx ) { return XRT_NET_ERROR; }

	// 加载 DER 格式证书文件
	if ( sCertFile && sCertFile[0] ) {
		if ( !__xrt_tls_load_der_file(sCertFile, &pCertDer, &iCertDerLen) ) {
			return XRT_NET_ERROR;
		}
	}

	// 加载 DER 格式私钥文件
	if ( sKeyFile && sKeyFile[0] ) {
		if ( !__xrt_tls_load_der_file(sKeyFile, &pKeyDer, &iKeyDerLen) ) {
			if ( pCertDer ) { xrtFree(pCertDer); }
			return XRT_NET_ERROR;
		}
	}

	__xrt_tls_ctx_lock(pCtx);
	iRes = __xrt_tls_set_cert_owned(pCtx, pCertDer, iCertDerLen, pKeyDer, iKeyDerLen);
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}


// 设置 TLS 证书（ 内存数据形式 ）
XXAPI xnet_result xrtTlsSetCertData(xtlsctx *pCtx, const void *pCertData, size_t iCertLen, const void *pKeyData, size_t iKeyLen)
{
	uint8 *pCertCopy = NULL;
	uint8 *pKeyCopy = NULL;
	size_t iCertCopyLen = 0;
	size_t iKeyCopyLen = 0;
	xnet_result iRes;

	if ( !pCtx ) {
		return XRT_NET_ERROR;
	}
	// 复制证书 DER 数据
	if ( !__xrt_tls_copy_der_data(pCertData, iCertLen, &pCertCopy, &iCertCopyLen) ) {
		return XRT_NET_ERROR;
	}
	// 复制私钥 DER 数据
	if ( !__xrt_tls_copy_der_data(pKeyData, iKeyLen, &pKeyCopy, &iKeyCopyLen) ) {
		if ( pCertCopy ) {
			xrtFree(pCertCopy);
		}
		return XRT_NET_ERROR;
	}
	__xrt_tls_ctx_lock(pCtx);
	iRes = __xrt_tls_set_cert_owned(pCtx, pCertCopy, iCertCopyLen, pKeyCopy, iKeyCopyLen);
	__xrt_tls_ctx_unlock(pCtx);
	return iRes;
}



struct xrt_tls_session {
	xtlsctx* pCtx;
	bool bIsServer;
};


// 内部函数：__xrtNetTlsSessionCtx
static xtlsctx* __xrtNetTlsSessionCtx(const xtlssession* pSession)
{
	return pSession ? pSession->pCtx : NULL;
}


// 创建网络 TLS session
XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer)
{
	xtlssession* pSession;

	(void)xrtInit();
	// 分配会话结构体
	pSession = (xtlssession*)xrtCalloc(1, sizeof(xtlssession));
	if ( !pSession ) {
		return NULL;
	}
	// 创建底层 TLS 上下文
	pSession->pCtx = xrtTlsCreate(pCfg, bIsServer);
	if ( !pSession->pCtx ) {
		xrtFree(pSession);
		return NULL;
	}
	// 双向关联上下文和会话
	pSession->pCtx->pSession = pSession;
	pSession->bIsServer = bIsServer;
	return pSession;
}


// 销毁网络 TLS session
XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession)
{
	if ( !pSession ) {
		return;
	}
	// 先断开双向关联，再销毁上下文
	if ( pSession->pCtx ) {
		pSession->pCtx->pSession = NULL;
		xrtTlsDestroy(pSession->pCtx);
		pSession->pCtx = NULL;
	}
	xrtFree(pSession);
}


// xrtNetTlsSessionIsReady 相关处理
XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsIsReady(pCtx) : false;
}


// xrtNetTlsSessionDriveHandshake 相关处理
XXAPI xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsDrive(pCtx) : XRT_NET_ERROR;
}


// xrtNetTlsSessionFeedCipher 相关处理
XXAPI xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pData || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsFeed(pCtx, (const char*)pData, iLen);
}


// xrtNetTlsSessionPendingCipher 相关处理
XXAPI size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsPendingSend(pCtx) : 0;
}


// xrtNetTlsSessionPendingRecv 相关处理
XXAPI size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsPendingRecv(pCtx) : 0;
}


// xrtNetTlsSessionPeekCipher 相关处理
XXAPI xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pBuf || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsPeekSend(pCtx, (char*)pBuf, iLen, pRead);
}


// xrtNetTlsSessionConsumeCipher 相关处理
XXAPI void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || iLen == 0 ) {
		return;
	}
	xrtTlsConsumeSend(pCtx, iLen);
}


// xrtNetTlsSessionWritePlain 相关处理
XXAPI xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pData || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsWrite(pCtx, (const char*)pData, iLen, pWritten);
}


// xrtNetTlsSessionReadPlain 相关处理
XXAPI xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	if ( !pCtx || !pBuf || iLen == 0 ) {
		return XRT_NET_ERROR;
	}
	return xrtTlsRead(pCtx, (char*)pBuf, iLen, pRead);
}


// 关闭网络 TLS session 队列
XXAPI xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsClose(pCtx) : XRT_NET_ERROR;
}


// xrtNetTlsSessionExportResume 相关处理
XXAPI xtlsresume* xrtNetTlsSessionExportResume(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsExportResume(pCtx) : NULL;
}


// 销毁网络 TLS resume
XXAPI void xrtNetTlsResumeDestroy(xtlsresume* pResume)
{
	xrtTlsResumeDestroy(pResume);
}


// xrtNetTlsSessionWasResumed 相关处理
XXAPI bool xrtNetTlsSessionWasResumed(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsWasResumed(pCtx) : false;
}


// xrtNetTlsSessionGetSNI 相关处理
XXAPI const char* xrtNetTlsSessionGetSNI(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsGetSNI(pCtx) : NULL;
}


XXAPI const char* xrtNetTlsSessionGetALPN(const xtlssession* pSession)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsGetALPN(pCtx) : NULL;
}


// 获取网络 TLS session 对端叶子证书 DER 数据
XXAPI const void* xrtNetTlsSessionPeerCertDer(const xtlssession* pSession, size_t* pSize)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	const void* pData = NULL;

	if ( pSize ) { *pSize = 0; }
	if ( !pCtx ) { return NULL; }
	__xrt_tls_ctx_lock(pCtx);
	pData = pCtx->pPeerCertDer;
	if ( pSize ) { *pSize = pCtx->iPeerCertDerLen; }
	__xrt_tls_ctx_unlock(pCtx);
	return pData;
}


// 获取网络 TLS session 对端叶子证书 DER 数据大小
XXAPI size_t xrtNetTlsSessionPeerCertSize(const xtlssession* pSession)
{
	size_t iSize = 0;
	(void)xrtNetTlsSessionPeerCertDer(pSession, &iSize);
	return iSize;
}


// 设置网络 TLS session 证书
XXAPI xnet_result xrtNetTlsSessionSetCert(xtlssession* pSession, const char* sCertFile, const char* sKeyFile)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsSetCert(pCtx, sCertFile, sKeyFile) : XRT_NET_ERROR;
}


// 设置网络 TLS session allow TLS 12 ed 25519
XXAPI xnet_result xrtNetTlsSessionSetCertData(xtlssession* pSession, const void* pCertData, size_t iCertLen, const void* pKeyData, size_t iKeyLen)
{
	xtlsctx* pCtx = __xrtNetTlsSessionCtx(pSession);
	return pCtx ? xrtTlsSetCertData(pCtx, pCertData, iCertLen, pKeyData, iKeyLen) : XRT_NET_ERROR;
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
		printf("    got: "); for ( int i=0;i<32;i++) printf("%02x",aR[i]); printf("\n");
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
		printf("    Gy^2:         "); for ( int i=0;i<32;i++) printf("%02x",aL[i]); printf("\n");
		printf("    Gx^3-3Gx+b:  "); for ( int i=0;i<32;i++) printf("%02x",aR[i]); printf("\n");
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
		printf("    y^2:         "); for ( int i=0;i<32;i++) printf("%02x",aL[i]); printf("\n");
		printf("    x^3-3x+b:    "); for ( int i=0;i<32;i++) printf("%02x",aR[i]); printf("\n");
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
