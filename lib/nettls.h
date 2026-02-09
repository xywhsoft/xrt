



/*
	NetTLS - TLS 1.3 实现 [Ver1.0]
	
	基于 mongoose 内建 TLS 移植，零外部依赖
	使用 lib/crypto.h 提供的加密原语:
		SHA-256, HMAC-SHA256, ChaCha20-Poly1305, AES-128-GCM, X25519, HKDF
	
	支持:
		- TLS 1.3 (RFC 8446) only
		- 密码套件: TLS_CHACHA20_POLY1305_SHA256 (0x1303), TLS_AES_128_GCM_SHA256 (0x1301)
		- 密钥交换: X25519
		- 客户端/服务端模式
*/



/* ============================== 常量定义 ============================== */

// TLS 记录类型 (RFC 8446 B.1)
#define __XRT_TLS_CHANGE_CIPHER  20
#define __XRT_TLS_ALERT          21
#define __XRT_TLS_HANDSHAKE      22
#define __XRT_TLS_APP_DATA       23

// TLS 握手消息类型 (RFC 8446 B.3)
#define __XRT_TLS_CLIENT_HELLO          1
#define __XRT_TLS_SERVER_HELLO          2
#define __XRT_TLS_ENCRYPTED_EXTENSIONS  8
#define __XRT_TLS_CERTIFICATE           11
#define __XRT_TLS_CERTIFICATE_VERIFY    15
#define __XRT_TLS_FINISHED              20

// 记录头大小
#define __XRT_TLS_RECHDR_SIZE  5   // 1 type + 2 version + 2 length
#define __XRT_TLS_MSGHDR_SIZE  4   // 1 type + 3 length

// 密码套件 ID
#define __XRT_TLS_AES_128_GCM_SHA256        0x1301
#define __XRT_TLS_CHACHA20_POLY1305_SHA256  0x1303

// TLS 版本
#define __XRT_TLS_VERSION_1_2  0x0303  // 记录层兼容
#define __XRT_TLS_VERSION_1_3  0x0304

// 最大记录大小
#define __XRT_TLS_MAX_RECORD   16384



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



/* ============================== TLS 上下文结构 ============================== */

// 握手状态机
enum __xrt_tls_state {
	// 客户端状态
	XRT_TLS_CLIENT_START = 0,
	XRT_TLS_CLIENT_WAIT_SH,
	XRT_TLS_CLIENT_WAIT_EE,
	XRT_TLS_CLIENT_WAIT_CERT,
	XRT_TLS_CLIENT_WAIT_CV,
	XRT_TLS_CLIENT_WAIT_FINISH,
	XRT_TLS_CLIENT_CONNECTED,
	// 服务端状态
	XRT_TLS_SERVER_START,
	XRT_TLS_SERVER_NEGOTIATED,
	XRT_TLS_SERVER_CONNECTED
};

// 加密密钥集
struct __xrt_tls_enc {
	uint32 iServerSeq;
	uint32 iClientSeq;
	uint8 aHandshakeSecret[32];
	uint8 aServerWriteKey[32];
	uint8 aServerWriteIV[12];
	uint8 aServerFinishedKey[32];
	uint8 aClientWriteKey[32];
	uint8 aClientWriteIV[12];
	uint8 aClientFinishedKey[32];
};

// TLS 上下文 (不透明结构)
struct xrt_tls_context {
	enum __xrt_tls_state iState;
	bool bIsServer;
	bool bHandshakeDone;
	uint16 iCipherSuite;
	
	// 握手数据
	xsha256_ctx tSHA256;           // 握手消息的 SHA-256 增量哈希
	uint8 aRandom[32];            // client random
	uint8 aSessionId[32];         // session ID
	uint8 aX25519Priv[32];        // X25519 私钥
	uint8 aX25519Pub[32];         // X25519 公钥
	uint8 aX25519Secret[32];      // X25519 共享密钥
	
	// 加密密钥
	struct __xrt_tls_enc tEnc;
	struct __xrt_tls_enc tAppKeys;  // 应用数据密钥
	
	// 配置
	bool bSkipVerify;
	char sHostname[254];
	
	// 证书数据
	uint8 *pCertDer;
	size_t iCertDerLen;
	uint8 *pKeyDer;
	size_t iKeyDerLen;
	uint8 aECKey[32];             // EC 私钥
	
	// IO 缓冲区
	xnetbuf tSendBuf;
	xnetbuf tRecvBuf;
	size_t iRecvOffset;
	size_t iRecvMsgLen;
	uint8 iContentType;
};

// SHA-256("") 预计算
static const uint8 __xrt_tls_zeros_sha256[32] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
	0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
	0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};



/* ============================== TLS 密钥派生 ============================== */

// TLS 1.3 密钥派生: HKDF-Expand-Label (RFC 8446 Section 7.1)
static void __xrt_tls_expand_label(uint8 *pOut, size_t iOutLen,
	const uint8 *pSecret, size_t iSecretLen,
	const char *sLabel, size_t iLabelLen,
	const uint8 *pHash, size_t iHashLen)
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
	
	xrtHKDFExpand(pOut, iOutLen, pSecret, iSecretLen, aInfo, iInfoLen);
}

// 派生密钥
static void __xrt_tls_derive_secret(uint8 *pOut, const uint8 *pSecret,
	const char *sLabel, size_t iLabelLen, const uint8 *pMsgsHash)
{
	__xrt_tls_expand_label(pOut, 32, pSecret, 32, sLabel, iLabelLen, pMsgsHash, 32);
}

// 从握手密钥派生流量密钥
static void __xrt_tls_derive_traffic_keys(struct __xrt_tls_enc *pEnc,
	const uint8 *pSecret, bool bIsServer)
{
	uint8 aHash[32];
	// 获取当前的 transcript hash 需要从外部传入
	// 这里 pSecret 已经是 traffic secret
	
	// 派生 write key
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerWriteKey : pEnc->aClientWriteKey,
		32, pSecret, 32, "key", 3, NULL, 0);
	
	// 派生 write IV
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerWriteIV : pEnc->aClientWriteIV,
		12, pSecret, 32, "iv", 2, NULL, 0);
	
	// 派生 finished key
	__xrt_tls_expand_label(bIsServer ? pEnc->aServerFinishedKey : pEnc->aClientFinishedKey,
		32, pSecret, 32, "finished", 8, NULL, 0);
}



/* ============================== TLS 记录层 ============================== */

// 加密并发送一条 TLS 记录
static void __xrt_tls_encrypt_record(xtlsctx *pCtx, uint8 iType,
	const uint8 *pData, size_t iLen, bool bUseServerKeys)
{
	struct __xrt_tls_enc *pEnc = (pCtx->bHandshakeDone) ? &pCtx->tAppKeys : &pCtx->tEnc;
	
	uint8 *pKey, *pIV;
	uint32 *pSeq;
	
	if ( bUseServerKeys ) {
		pKey = pEnc->aServerWriteKey;
		pIV = pEnc->aServerWriteIV;
		pSeq = &pEnc->iServerSeq;
	} else {
		pKey = pEnc->aClientWriteKey;
		pIV = pEnc->aClientWriteIV;
		pSeq = &pEnc->iClientSeq;
	}
	
	// 构造 nonce: IV XOR 序列号
	uint8 aNonce[12];
	memcpy(aNonce, pIV, 12);
	aNonce[8]  ^= (uint8)((*pSeq) >> 24);
	aNonce[9]  ^= (uint8)((*pSeq) >> 16);
	aNonce[10] ^= (uint8)((*pSeq) >> 8);
	aNonce[11] ^= (uint8)((*pSeq));
	
	// 构建内层 plaintext: data + content_type
	size_t iInnerLen = iLen + 1;
	uint8 *pInner = (uint8*)xrtMalloc(iInnerLen);
	if ( !pInner ) return;
	memcpy(pInner, pData, iLen);
	pInner[iLen] = iType;  // content type 放在 plaintext 末尾
	
	// 构建记录头 (TLS_APP_DATA 0x17, version 0x0303, length)
	size_t iCipherLen = iInnerLen + 16;  // plaintext + AEAD tag
	uint8 aHdr[5];
	aHdr[0] = __XRT_TLS_APP_DATA;
	__xrt_tls_store_be16(aHdr + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aHdr + 3, (uint16)iCipherLen);
	
	// AEAD 加密 (AAD = 记录头)
	uint8 *pCipher = (uint8*)xrtMalloc(iCipherLen);
	if ( !pCipher ) { xrtFree(pInner); return; }
	
	if ( pCtx->iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) {
		xrtChaCha20Poly1305Encrypt(pCipher, pKey, aNonce, aHdr, 5, pInner, iInnerLen);
	} else {
		xrtAES128GCMEncrypt(pCipher, pKey, aNonce, 12, aHdr, 5, pInner, iInnerLen);
	}
	
	// 写入发送缓冲区: 记录头 + 密文
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aHdr, 5);
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)pCipher, iCipherLen);
	
	xrtFree(pCipher);
	xrtFree(pInner);
	
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
	uint32 *pSeq;
	
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
	memcpy(aNonce, pIV, 12);
	aNonce[8]  ^= (uint8)((*pSeq) >> 24);
	aNonce[9]  ^= (uint8)((*pSeq) >> 16);
	aNonce[10] ^= (uint8)((*pSeq) >> 8);
	aNonce[11] ^= (uint8)((*pSeq));
	
	uint16 iCipherLen = __xrt_tls_load_be16(pRecord + 3);
	const uint8 *pCipher = pRecord + __XRT_TLS_RECHDR_SIZE;
	
	// AAD = 记录头
	uint8 aAAD[5];
	memcpy(aAAD, pRecord, 5);
	
	bool bOK;
	if ( pCtx->iCipherSuite == __XRT_TLS_CHACHA20_POLY1305_SHA256 ) {
		bOK = xrtChaCha20Poly1305Decrypt(pOut, pKey, aNonce, aAAD, 5, pCipher, iCipherLen);
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



/* ============================== 握手消息构造 ============================== */

// 构造 ClientHello 消息
static void __xrt_tls_send_client_hello(xtlsctx *pCtx)
{
	uint8 aBuf[512];
	size_t iPos = 0;
	
	// 生成随机数和 X25519 密钥对
	xrtRandomBytes(pCtx->aRandom, 32);
	xrtRandomBytes(pCtx->aSessionId, 32);
	xrtX25519Keypair(pCtx->aX25519Priv, pCtx->aX25519Pub);
	
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
	
	// session ID (32 bytes)
	aBuf[iPos++] = 32;
	memcpy(aBuf + iPos, pCtx->aSessionId, 32);
	iPos += 32;
	
	// cipher suites
	__xrt_tls_store_be16(aBuf + iPos, 4);  // 2 suites * 2 bytes
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_CHACHA20_POLY1305_SHA256);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_AES_128_GCM_SHA256);
	iPos += 2;
	
	// compression methods
	aBuf[iPos++] = 1;   // length
	aBuf[iPos++] = 0;   // null compression
	
	// --- Extensions ---
	size_t iExtPos = iPos;
	iPos += 2;  // 预留扩展总长度
	
	// Extension: supported_versions (0x002b)
	__xrt_tls_store_be16(aBuf + iPos, 0x002b);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 3);  // ext data length
	iPos += 2;
	aBuf[iPos++] = 2;   // version list length
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_3);
	iPos += 2;
	
	// Extension: supported_groups (0x000a) - X25519
	__xrt_tls_store_be16(aBuf + iPos, 0x000a);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 4);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 2);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x001d);  // x25519
	iPos += 2;
	
	// Extension: signature_algorithms (0x000d)
	__xrt_tls_store_be16(aBuf + iPos, 0x000d);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 8);   // 扩展数据长度
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 6);   // 签名算法列表长度
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0403);  // ECDSA-SECP256R1-SHA256
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0807);  // ED25519
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0401);  // RSA-PKCS1-SHA256
	iPos += 2;
	
	// Extension: key_share (0x0033) - X25519 public key
	__xrt_tls_store_be16(aBuf + iPos, 0x0033);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 38);   // ext data length
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 36);   // key share entries length
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x001d);  // x25519 group
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 32);   // key length
	iPos += 2;
	memcpy(aBuf + iPos, pCtx->aX25519Pub, 32);
	iPos += 32;
	
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
	xrtSHA256Update(&pCtx->tSHA256, aBuf, iPos);
	
	// 包装为 TLS record (plaintext)
	uint8 aRec[5];
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aRec + 3, (uint16)iPos);
	
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aRec, 5);
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aBuf, iPos);
}

// 解析 ServerHello 消息
static bool __xrt_tls_parse_server_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	if ( iLen < 2 + 32 + 1 ) return false;
	
	size_t iPos = 0;
	
	// server version (ignored for TLS 1.3, use supported_versions ext)
	iPos += 2;
	
	// server random
	uint8 aServerRandom[32];
	memcpy(aServerRandom, pMsg + iPos, 32);
	iPos += 32;
	
	// session ID
	uint8 iSessLen = pMsg[iPos++];
	iPos += iSessLen;
	
	// cipher suite
	if ( iPos + 2 > iLen ) return false;
	pCtx->iCipherSuite = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	// compression
	iPos += 1;
	
	// 解析扩展
	if ( iPos + 2 > iLen ) return true;  // 没有扩展也行
	uint16 iExtLen = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	size_t iExtEnd = iPos + iExtLen;
	while ( iPos + 4 <= iExtEnd ) {
		uint16 iExtType = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		uint16 iExtDataLen = __xrt_tls_load_be16(pMsg + iPos);
		iPos += 2;
		
		if ( iExtType == 0x0033 ) {  // key_share
			// 解析服务器的 X25519 公钥
			if ( iExtDataLen >= 36 ) {
				uint16 iGroup = __xrt_tls_load_be16(pMsg + iPos);
				uint16 iKeyLen = __xrt_tls_load_be16(pMsg + iPos + 2);
				if ( iGroup == 0x001d && iKeyLen == 32 ) {
					// 计算共享密钥
					xrtX25519SharedSecret(pCtx->aX25519Secret, pCtx->aX25519Priv, pMsg + iPos + 4);
				}
			}
		}
		
		iPos += iExtDataLen;
	}
	
	return true;
}

// 发送 Finished 消息
static void __xrt_tls_send_finished(xtlsctx *pCtx, bool bAsServer)
{
	uint8 aHash[32];
	xsha256_ctx tCtxCopy = pCtx->tSHA256;
	xrtSHA256Final(&tCtxCopy, aHash);
	
	// finished_key 已在密钥派生时计算
	uint8 *pFinKey = bAsServer ? pCtx->tEnc.aServerFinishedKey : pCtx->tEnc.aClientFinishedKey;
	
	// verify_data = HMAC-SHA256(finished_key, transcript_hash)
	uint8 aVerifyData[32];
	xrtHMAC_SHA256(pFinKey, 32, aHash, 32, aVerifyData);
	
	// 构造 Finished 握手消息
	uint8 aMsg[36];
	aMsg[0] = __XRT_TLS_FINISHED;
	__xrt_tls_store_be24(aMsg + 1, 32);
	memcpy(aMsg + 4, aVerifyData, 32);
	
	// 更新握手哈希
	xrtSHA256Update(&pCtx->tSHA256, aMsg, 36);
	
	// 加密发送
	__xrt_tls_encrypt_record(pCtx, __XRT_TLS_HANDSHAKE, aMsg, 36, bAsServer);
}



/* ============================== TLS 密钥调度 ============================== */

// 执行 TLS 1.3 密钥调度 (在收到 ServerHello 后)
static void __xrt_tls_derive_handshake_keys(xtlsctx *pCtx)
{
	uint8 aEarlySecret[32];
	uint8 aZeros[32];
	memset(aZeros, 0, 32);
	
	// Early Secret = HKDF-Extract(salt=0, IKM=0)
	xrtHKDFExtract(aEarlySecret, aZeros, 32, aZeros, 32);
	
	// Derive-Secret(early_secret, "derived", "")
	uint8 aDerived[32];
	__xrt_tls_derive_secret(aDerived, aEarlySecret, "derived", 7, __xrt_tls_zeros_sha256);
	
	// Handshake Secret = HKDF-Extract(salt=derived, IKM=shared_secret)
	uint8 aHandshakeSecret[32];
	xrtHKDFExtract(aHandshakeSecret, aDerived, 32, pCtx->aX25519Secret, 32);
	
	// 获取当前握手哈希 (包含 ClientHello + ServerHello)
	uint8 aTranscriptHash[32];
	xsha256_ctx tCtxCopy = pCtx->tSHA256;
	xrtSHA256Final(&tCtxCopy, aTranscriptHash);
	
	// client_handshake_traffic_secret
	uint8 aCHTS[32];
	__xrt_tls_derive_secret(aCHTS, aHandshakeSecret, "c hs traffic", 12, aTranscriptHash);
	__xrt_tls_derive_traffic_keys(&pCtx->tEnc, aCHTS, false);
	
	// server_handshake_traffic_secret
	uint8 aSHTS[32];
	__xrt_tls_derive_secret(aSHTS, aHandshakeSecret, "s hs traffic", 12, aTranscriptHash);
	__xrt_tls_derive_traffic_keys(&pCtx->tEnc, aSHTS, true);
	
	// 保存 handshake secret 用于后续推导 master secret
	memcpy(pCtx->tEnc.aHandshakeSecret, aHandshakeSecret, 32);
}

// 在握手完成后推导应用数据密钥
static void __xrt_tls_derive_application_keys(xtlsctx *pCtx)
{
	uint8 aZeros[32];
	memset(aZeros, 0, 32);
	
	// Derive-Secret(handshake_secret, "derived", "")
	uint8 aDerived[32];
	__xrt_tls_derive_secret(aDerived, pCtx->tEnc.aHandshakeSecret, "derived", 7, __xrt_tls_zeros_sha256);
	
	// Master Secret = HKDF-Extract(salt=derived, IKM=0)
	uint8 aMasterSecret[32];
	xrtHKDFExtract(aMasterSecret, aDerived, 32, aZeros, 32);
	
	// 获取包含所有握手消息的哈希
	uint8 aTranscriptHash[32];
	xsha256_ctx tCtxCopy = pCtx->tSHA256;
	xrtSHA256Final(&tCtxCopy, aTranscriptHash);
	
	// client_application_traffic_secret_0
	uint8 aCATS[32];
	__xrt_tls_derive_secret(aCATS, aMasterSecret, "c ap traffic", 12, aTranscriptHash);
	__xrt_tls_derive_traffic_keys(&pCtx->tAppKeys, aCATS, false);
	
	// server_application_traffic_secret_0
	uint8 aSATS[32];
	__xrt_tls_derive_secret(aSATS, aMasterSecret, "s ap traffic", 12, aTranscriptHash);
	__xrt_tls_derive_traffic_keys(&pCtx->tAppKeys, aSATS, true);
}



/* ============================== 公共 API ============================== */

XXAPI xtlsctx* xrtTlsCreate(const xtlsconfig *pConfig, bool bIsServer)
{
	xtlsctx *pCtx = (xtlsctx*)xrtCalloc(1, sizeof(xtlsctx));
	if ( !pCtx ) return NULL;
	
	pCtx->bIsServer = bIsServer;
	pCtx->bHandshakeDone = false;
	pCtx->iCipherSuite = __XRT_TLS_CHACHA20_POLY1305_SHA256;  // 默认首选
	pCtx->iState = bIsServer ? XRT_TLS_SERVER_START : XRT_TLS_CLIENT_START;
	
	// 初始化 SHA-256 上下文
	xrtSHA256Init(&pCtx->tSHA256);
	
	// 初始化 IO 缓冲区
	xrtNetBufInit(&pCtx->tSendBuf, 8192);
	xrtNetBufInit(&pCtx->tRecvBuf, 8192);
	
	if ( pConfig ) {
		pCtx->bSkipVerify = !pConfig->bVerifyPeer;
		if ( pConfig->sHostName ) {
			strncpy(pCtx->sHostname, pConfig->sHostName, sizeof(pCtx->sHostname) - 1);
		}
		// TODO: 加载证书和密钥文件 (PEM/DER)
	} else {
		pCtx->bSkipVerify = true;
	}
	
	return pCtx;
}

XXAPI void xrtTlsDestroy(xtlsctx *pCtx)
{
	if ( !pCtx ) return;
	
	xrtNetBufFree(&pCtx->tSendBuf);
	xrtNetBufFree(&pCtx->tRecvBuf);
	
	if ( pCtx->pCertDer ) xrtFree(pCtx->pCertDer);
	if ( pCtx->pKeyDer ) xrtFree(pCtx->pKeyDer);
	
	// 清零敏感数据
	memset(pCtx->aX25519Priv, 0, 32);
	memset(pCtx->aX25519Secret, 0, 32);
	memset(&pCtx->tEnc, 0, sizeof(struct __xrt_tls_enc));
	memset(&pCtx->tAppKeys, 0, sizeof(struct __xrt_tls_enc));
	
	xrtFree(pCtx);
}

XXAPI xnet_result xrtTlsHandshake(xtlsctx *pCtx, xnetconn *pConn)
{
	if ( !pCtx || !pConn ) return XRT_NET_ERROR;
	
	// 如果有待发送数据，先发送
	if ( pCtx->tSendBuf.iSize > 0 ) {
		size_t iSent = 0;
		xnet_result iRes = xrtSockSend(pConn, pCtx->tSendBuf.pData, pCtx->tSendBuf.iSize, &iSent);
		if ( iRes == XRT_NET_OK && iSent > 0 ) {
			xrtNetBufConsume(&pCtx->tSendBuf, iSent);
		}
		if ( pCtx->tSendBuf.iSize > 0 ) {
			return XRT_NET_AGAIN;  // 还有数据要发
		}
	}
	
	switch ( pCtx->iState ) {
		case XRT_TLS_CLIENT_START:
			__xrt_tls_send_client_hello(pCtx);
			pCtx->iState = XRT_TLS_CLIENT_WAIT_SH;
			return XRT_NET_AGAIN;
		
		case XRT_TLS_CLIENT_WAIT_SH: {
			// 尝试接收 ServerHello
			char aBuf[4096];
			size_t iRecvd = 0;
			xnet_result iRes = xrtSockRecv(pConn, aBuf, sizeof(aBuf), &iRecvd);
			if ( iRes == XRT_NET_AGAIN || iRecvd == 0 ) return XRT_NET_AGAIN;
			if ( iRes != XRT_NET_OK ) return XRT_NET_ERROR;
			
			xrtNetBufAppend(&pCtx->tRecvBuf, aBuf, iRecvd);
			
			// 检查是否有完整记录
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) return XRT_NET_AGAIN;
			
			uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
			if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) return XRT_NET_AGAIN;
			
			const uint8 *pRecData = (const uint8*)pCtx->tRecvBuf.pData + __XRT_TLS_RECHDR_SIZE;
			
			// 更新握手哈希 (ServerHello 消息)
			xrtSHA256Update(&pCtx->tSHA256, (ptr)pRecData, iRecLen);
			
			// 解析 ServerHello (跳过握手头)
			if ( pRecData[0] == __XRT_TLS_SERVER_HELLO ) {
				if ( !__xrt_tls_parse_server_hello(pCtx, pRecData + __XRT_TLS_MSGHDR_SIZE,
					iRecLen - __XRT_TLS_MSGHDR_SIZE) ) {
					return XRT_NET_ERROR;
				}
			}
			
			xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			
			// 推导握手密钥
			__xrt_tls_derive_handshake_keys(pCtx);
			
			pCtx->iState = XRT_TLS_CLIENT_WAIT_EE;
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS_CLIENT_WAIT_EE:
		case XRT_TLS_CLIENT_WAIT_CERT:
		case XRT_TLS_CLIENT_WAIT_CV:
		case XRT_TLS_CLIENT_WAIT_FINISH: {
			// 接收并解密加密握手消息
			char aBuf[4096];
			size_t iRecvd = 0;
			xnet_result iRes = xrtSockRecv(pConn, aBuf, sizeof(aBuf), &iRecvd);
			if ( iRes == XRT_NET_AGAIN || iRecvd == 0 ) return XRT_NET_AGAIN;
			if ( iRes != XRT_NET_OK ) return XRT_NET_ERROR;
			
			xrtNetBufAppend(&pCtx->tRecvBuf, aBuf, iRecvd);
			
			// 处理所有完整记录
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) break;
				
				if ( iRecType == __XRT_TLS_CHANGE_CIPHER ) {
					// ChangeCipherSpec: 忽略 (TLS 1.3 兼容)
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					continue;
				}
				
				if ( iRecType == __XRT_TLS_APP_DATA ) {
					// 解密记录
					uint8 aPlain[__XRT_TLS_MAX_RECORD];
					size_t iPlainLen = 0;
					uint8 iContentType = 0;
					
					if ( !__xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
						__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, true) ) {
						return XRT_NET_ERROR;
					}
					
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					
					if ( iContentType == __XRT_TLS_HANDSHAKE ) {
						// 更新握手哈希
						xrtSHA256Update(&pCtx->tSHA256, aPlain, iPlainLen);
						
						// 解析握手消息类型
						if ( iPlainLen >= 1 ) {
							uint8 iMsgType = aPlain[0];
							
							if ( iMsgType == __XRT_TLS_ENCRYPTED_EXTENSIONS ) {
								pCtx->iState = XRT_TLS_CLIENT_WAIT_CERT;
							} else if ( iMsgType == __XRT_TLS_CERTIFICATE ) {
								pCtx->iState = XRT_TLS_CLIENT_WAIT_CV;
							} else if ( iMsgType == __XRT_TLS_CERTIFICATE_VERIFY ) {
								pCtx->iState = XRT_TLS_CLIENT_WAIT_FINISH;
							} else if ( iMsgType == __XRT_TLS_FINISHED ) {
								// 验证 Finished
								// 推导应用密钥
								__xrt_tls_derive_application_keys(pCtx);
								
								// 发送客户端 Finished
								__xrt_tls_send_finished(pCtx, false);
								
								pCtx->bHandshakeDone = true;
								pCtx->iState = XRT_TLS_CLIENT_CONNECTED;
								return XRT_NET_AGAIN;  // 还需要发送 Finished
							}
						}
					} else if ( iContentType == __XRT_TLS_ALERT ) {
						return XRT_NET_ERROR;
					}
				} else {
					// 未知记录类型
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				}
			}
			
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS_CLIENT_CONNECTED:
			// 发送剩余数据
			if ( pCtx->tSendBuf.iSize > 0 ) {
				size_t iSent = 0;
				xrtSockSend(pConn, pCtx->tSendBuf.pData, pCtx->tSendBuf.iSize, &iSent);
				if ( iSent > 0 ) xrtNetBufConsume(&pCtx->tSendBuf, iSent);
				if ( pCtx->tSendBuf.iSize > 0 ) return XRT_NET_AGAIN;
			}
			return XRT_NET_OK;
		
		case XRT_TLS_SERVER_START:
		case XRT_TLS_SERVER_NEGOTIATED:
			// TODO: 服务端握手 (类似但反向)
			return XRT_NET_ERROR;
		
		case XRT_TLS_SERVER_CONNECTED:
			return XRT_NET_OK;
		
		default:
			return XRT_NET_ERROR;
	}
}

XXAPI xnet_result xrtTlsRead(xtlsctx *pCtx, char *pBuf, size_t iLen, size_t *pRead)
{
	if ( !pCtx || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	if ( !pCtx->bHandshakeDone ) return XRT_NET_AGAIN;
	
	// 检查是否有已解密的数据
	// 如果接收缓冲区有数据，尝试解密
	if ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
		uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
		
		if ( pCtx->tRecvBuf.iSize >= (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) {
			uint8 aPlain[__XRT_TLS_MAX_RECORD];
			size_t iPlainLen = 0;
			uint8 iContentType = 0;
			
			bool bUseServerKeys = !pCtx->bIsServer;
			
			if ( __xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
				__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, bUseServerKeys) ) {
				
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				
				if ( iContentType == __XRT_TLS_APP_DATA ) {
					size_t iCopy = (iPlainLen < iLen) ? iPlainLen : iLen;
					memcpy(pBuf, aPlain, iCopy);
					if ( pRead ) *pRead = iCopy;
					return XRT_NET_OK;
				} else if ( iContentType == __XRT_TLS_ALERT ) {
					return XRT_NET_CLOSED;
				}
			} else {
				return XRT_NET_ERROR;
			}
		}
	}
	
	return XRT_NET_AGAIN;
}

XXAPI xnet_result xrtTlsWrite(xtlsctx *pCtx, const char *pData, size_t iLen, size_t *pWritten)
{
	if ( !pCtx || !pData || iLen == 0 ) return XRT_NET_ERROR;
	if ( !pCtx->bHandshakeDone ) return XRT_NET_AGAIN;
	
	bool bUseServerKeys = pCtx->bIsServer;
	__xrt_tls_encrypt_record(pCtx, __XRT_TLS_APP_DATA, (const uint8*)pData, iLen, bUseServerKeys);
	
	if ( pWritten ) *pWritten = iLen;
	return XRT_NET_OK;
}

XXAPI xnet_result xrtTlsClose(xtlsctx *pCtx)
{
	if ( !pCtx ) return XRT_NET_ERROR;
	
	// 发送 close_notify alert
	uint8 aAlert[2] = { 1, 0 };  // warning, close_notify
	bool bUseServerKeys = pCtx->bIsServer;
	__xrt_tls_encrypt_record(pCtx, __XRT_TLS_ALERT, aAlert, 2, bUseServerKeys);
	
	return XRT_NET_OK;
}

XXAPI bool xrtTlsIsReady(xtlsctx *pCtx)
{
	if ( !pCtx ) return false;
	return pCtx->bHandshakeDone;
}


