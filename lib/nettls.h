



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



/* ============================== ASN.1/DER 解析 ============================== */

// DER TLV 节点
struct __xrt_der_tlv {
	uint8 iType;
	uint32 iLen;
	uint8 *pValue;
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

// 从 X.509 证书 DER 中提取 RSA 公钥 (modulus n, exponent e)
// 成功返回 true, pMod/pExp 指向证书数据内部 (零拷贝)
static bool __xrt_tls_extract_rsa_pubkey(uint8 *pCert, size_t iCertLen,
	uint8 **ppMod, size_t *pModSz, uint8 **ppExp, size_t *pExpSz)
{
	struct __xrt_der_tlv tRoot, tFound;

	// 解析证书根 SEQUENCE
	if ( __xrt_der_parse(pCert, iCertLen, &tRoot) < 0 ) return false;

	// 查找 rsaEncryption OID
	if ( !__xrt_der_find_oid(&tRoot, __xrt_oid_rsa_encryption,
		sizeof(__xrt_oid_rsa_encryption), &tFound) ) {
		return false;
	}

	// tFound 现在指向 OID 后的下一个元素
	// 需要重新遍历找到 SubjectPublicKeyInfo -> BIT STRING -> RSA key SEQUENCE
	{
		struct __xrt_der_tlv tRoot2, tCert, tTbsCert, tChild;
		int iRet;

		if ( __xrt_der_parse(pCert, iCertLen, &tRoot2) < 0 ) return false;

		// Certificate -> SEQUENCE
		if ( __xrt_der_next(&tRoot2, &tCert) <= 0 ) return false;

		// TBSCertificate -> first child SEQUENCE
		tTbsCert = tCert;

		// 遍历 TBSCertificate 的子元素找 SubjectPublicKeyInfo
		// 它是一个 SEQUENCE, 其第一个子元素是 AlgorithmIdentifier SEQUENCE
		// 包含 rsaEncryption OID
		while ( __xrt_der_next(&tTbsCert, &tChild) > 0 ) {
			if ( tChild.iType == 0x30 ) {  // SEQUENCE
				struct __xrt_der_tlv tSeq = tChild;
				struct __xrt_der_tlv tFirst;

				// 检查这个 SEQUENCE 是否包含 rsaEncryption OID
				if ( __xrt_der_find_oid(&tSeq, __xrt_oid_rsa_encryption,
					sizeof(__xrt_oid_rsa_encryption), &tFirst) ) {
					// 找到 SubjectPublicKeyInfo
					// 结构: SEQUENCE { AlgorithmIdentifier, BIT STRING { SEQUENCE { INTEGER(n), INTEGER(e) } } }
					struct __xrt_der_tlv tSPKI = tChild;
					struct __xrt_der_tlv tAlgId, tBitStr, tKeySeq, tMod, tExp;

					// 跳过 AlgorithmIdentifier
					if ( __xrt_der_next(&tSPKI, &tAlgId) <= 0 ) return false;

					// BIT STRING
					if ( __xrt_der_next(&tSPKI, &tBitStr) <= 0 ) return false;
					if ( tBitStr.iType != 0x03 ) return false;

					// BIT STRING 第一个字节是 unused bits (should be 0)
					if ( tBitStr.iLen < 1 ) return false;
					{
						uint8 *pKeyData = tBitStr.pValue + 1;
						size_t iKeyDataLen = tBitStr.iLen - 1;

						// 解析 RSA 公钥 SEQUENCE { INTEGER(n), INTEGER(e) }
						if ( __xrt_der_parse(pKeyData, iKeyDataLen, &tKeySeq) < 0 ) return false;
						if ( tKeySeq.iType != 0x30 ) return false;

						// modulus (n)
						if ( __xrt_der_next(&tKeySeq, &tMod) <= 0 ) return false;
						if ( tMod.iType != 0x02 ) return false;

						// 跳过前导零字节
						*ppMod = tMod.pValue;
						*pModSz = tMod.iLen;
						if ( (*pModSz > 0) && ((*ppMod)[0] == 0x00) ) {
							(*ppMod)++;
							(*pModSz)--;
						}

						// exponent (e)
						if ( __xrt_der_next(&tKeySeq, &tExp) <= 0 ) return false;
						if ( tExp.iType != 0x02 ) return false;

						*ppExp = tExp.pValue;
						*pExpSz = tExp.iLen;

						return true;
					}
				}
			}
		}
	}

	return false;
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
	
	// 服务器证书公钥 (RSA 或 EC)
	bool bIsECPubKey;             // true=EC, false=RSA
	uint8 aPubKey[512 + 16];      // RSA 公钥 (modulus + exp) 或 EC 公钥
	size_t iPubKeySz;             // 公钥总大小
	size_t iPubKeyModSz;          // RSA modulus 大小
	uint8 aSigHash[32];           // CertificateVerify 签名哈希
	
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
	
	// cipher suites (TLS 1.3 + TLS 1.2 兼容, RFC 8446 Appendix D.1)
	__xrt_tls_store_be16(aBuf + iPos, 10);  // 5 suites * 2 bytes
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_CHACHA20_POLY1305_SHA256);  // TLS 1.3
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_AES_128_GCM_SHA256);        // TLS 1.3
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0xc02c);  // ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 (TLS 1.2 compat)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0xc02f);  // ECDHE_RSA_WITH_AES_128_GCM_SHA256 (TLS 1.2 compat)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0xc02b);  // ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 (TLS 1.2 compat)
	iPos += 2;
	
	// compression methods
	aBuf[iPos++] = 1;   // length
	aBuf[iPos++] = 0;   // null compression
	
	// --- Extensions ---
	size_t iExtPos = iPos;
	iPos += 2;  // 预留扩展总长度
	
	// Extension: supported_versions (0x002b) — TLS 1.3 preferred, TLS 1.2 fallback
	__xrt_tls_store_be16(aBuf + iPos, 0x002b);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 5);  // ext data length
	iPos += 2;
	aBuf[iPos++] = 4;   // version list length (2 versions * 2 bytes)
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_3);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_VERSION_1_2);
	iPos += 2;
	
	// Extension: supported_groups (0x000a) - X25519 + secp256r1
	__xrt_tls_store_be16(aBuf + iPos, 0x000a);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 6);   // ext data length
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 4);   // named group list length
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x001d);  // x25519
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0017);  // secp256r1
	iPos += 2;
	
	// Extension: signature_algorithms (0x000d)
	__xrt_tls_store_be16(aBuf + iPos, 0x000d);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 14);   // 扩展数据长度 (6 algorithms * 2 + 2)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 12);   // 签名算法列表长度 (6 * 2)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0804);  // RSA-PSS-RSAE-SHA256 (TLS 1.3 必须)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0403);  // ECDSA-SECP256R1-SHA256
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0807);  // ED25519
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0805);  // RSA-PSS-RSAE-SHA384
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0806);  // RSA-PSS-RSAE-SHA512
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0401);  // RSA-PKCS1-SHA256 (兼容)
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
	
	// Extension: psk_key_exchange_modes (0x002d) — 许多服务器要求此扩展
	__xrt_tls_store_be16(aBuf + iPos, 0x002d);
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 2);  // ext data length
	iPos += 2;
	aBuf[iPos++] = 1;   // modes list length
	aBuf[iPos++] = 1;   // psk_dhe_ke
	
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
	xrtSHA256Update(&pCtx->tSHA256, aBuf, iPos);
	
	// 包装为 TLS record (plaintext)
	// RFC 8446: initial ClientHello record version SHOULD be 0x0301 for compatibility
	uint8 aRec[5];
	aRec[0] = __XRT_TLS_HANDSHAKE;
	__xrt_tls_store_be16(aRec + 1, 0x0303);  // TLS 1.2 record version for compatibility
	__xrt_tls_store_be16(aRec + 3, (uint16)iPos);
	
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aRec, 5);
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aBuf, iPos);
	
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
	if ( iPos + 2 > iLen ) return false;  // TLS 1.3 必须有扩展
	{
	uint16 iExtLen = __xrt_tls_load_be16(pMsg + iPos);
	bool bFoundTls13 = false;
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
				if ( iSelectedVer != __XRT_TLS_VERSION_1_3 ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS] ServerHello: server selected version 0x%04x, not TLS 1.3\n", iSelectedVer);
					#endif
					return false;  // 服务器未协商 TLS 1.3
				}
				bFoundTls13 = true;
			}
		} else if ( iExtType == 0x0033 ) {  // key_share
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
	
	if ( !bFoundTls13 ) {
		#ifdef DEBUG_TRACE
			printf("    [TLS] ServerHello: server negotiated TLS 1.2 (TLS 1.3 not supported), fallback not implemented\n");
		#endif
		return false;
	}
	
	return true;
	}
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
			// 尝试接收更多数据
			char aBuf[4096];
			size_t iRecvd = 0;
			xnet_result iRes = xrtSockRecv(pConn, aBuf, sizeof(aBuf), &iRecvd);
			if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
				xrtNetBufAppend(&pCtx->tRecvBuf, aBuf, iRecvd);
			} else if ( iRes == XRT_NET_CLOSED ) {
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_SH: connection closed\n");
				#endif
				return XRT_NET_ERROR;
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
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				return XRT_NET_ERROR;
			}
			if ( iRecType != __XRT_TLS_HANDSHAKE ) {
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				return XRT_NET_ERROR;
			}
			
			// 更新握手哈希 (ServerHello 消息)
			xrtSHA256Update(&pCtx->tSHA256, (ptr)pRecData, iRecLen);
			
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
					printf("    [TLS] WAIT_SH: ServerHello parsed, cipher=0x%04x\n", pCtx->iCipherSuite);
				#endif
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
			// 尝试接收更多数据（非阻塞，可能无新数据）
			char aBuf[4096];
			size_t iRecvd = 0;
			xnet_result iRes = xrtSockRecv(pConn, aBuf, sizeof(aBuf), &iRecvd);
			if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
				xrtNetBufAppend(&pCtx->tRecvBuf, aBuf, iRecvd);
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_EE+: recv %d bytes, recvBuf=%d\n", (int)iRecvd, (int)pCtx->tRecvBuf.iSize);
				#endif
			} else if ( iRes == XRT_NET_CLOSED ) {
				#ifdef DEBUG_TRACE
					printf("    [TLS] WAIT_EE+: connection closed\n");
				#endif
				return XRT_NET_ERROR;
			}
			
			// 检查缓冲区是否有可处理的记录
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) return XRT_NET_AGAIN;
			
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
						#ifdef DEBUG_TRACE
							printf("    [TLS] WAIT_EE+: decrypt_record FAILED (recLen=%d, cipher=0x%04x)\n",
								(int)iRecLen, pCtx->iCipherSuite);
						#endif
						return XRT_NET_ERROR;
					}
					
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					
					if ( iContentType == __XRT_TLS_HANDSHAKE ) {
						// 遍历解密后明文中所有握手消息 (TLS 1.3 允许单条记录含多个消息)
						size_t iMsgOff = 0;
						while ( iMsgOff + __XRT_TLS_MSGHDR_SIZE <= iPlainLen ) {
							uint8 iMsgType = aPlain[iMsgOff];
							uint32 iMsgBodyLen = __xrt_tls_load_be24(aPlain + iMsgOff + 1);
							size_t iTotalMsgLen = __XRT_TLS_MSGHDR_SIZE + iMsgBodyLen;
							
							if ( iMsgOff + iTotalMsgLen > iPlainLen ) break;
							
							uint8 *pMsg = aPlain + iMsgOff;
							
							// 对于 CertificateVerify, 在 update SHA256 之前保存 transcript hash
							if ( iMsgType == __XRT_TLS_CERTIFICATE_VERIFY ) {
								xsha256_ctx tCopy = pCtx->tSHA256;
								xrtSHA256Final(&tCopy, pCtx->aSigHash);
							}
							
							// 更新握手哈希 (仅当前消息)
							xrtSHA256Update(&pCtx->tSHA256, pMsg, iTotalMsgLen);
							
							if ( iMsgType == __XRT_TLS_ENCRYPTED_EXTENSIONS ) {
								pCtx->iState = XRT_TLS_CLIENT_WAIT_CERT;
							} else if ( iMsgType == __XRT_TLS_CERTIFICATE ) {
								// 解析证书, 提取 RSA 公钥
								if ( iMsgBodyLen >= 1 ) {
									size_t iCertOff = __XRT_TLS_MSGHDR_SIZE;
									uint8 iReqCtxLen = pMsg[iCertOff];
									iCertOff += 1 + iReqCtxLen;
									
									if ( iCertOff + 3 <= iTotalMsgLen ) {
										uint32 iCertListLen = __xrt_tls_load_be24(pMsg + iCertOff);
										iCertOff += 3;
										(void)iCertListLen;
										
										if ( iCertOff + 3 <= iTotalMsgLen ) {
											uint32 iCertLen = __xrt_tls_load_be24(pMsg + iCertOff);
											iCertOff += 3;
											
											if ( (iCertOff + iCertLen <= iTotalMsgLen) && (iCertLen > 0) ) {
												uint8 *pCertData = pMsg + iCertOff;
												uint8 *pMod = NULL, *pExp = NULL;
												size_t iModSz = 0, iExpSz = 0;
												
												if ( __xrt_tls_extract_rsa_pubkey(pCertData, iCertLen,
													&pMod, &iModSz, &pExp, &iExpSz) ) {
													pCtx->bIsECPubKey = false;
													if ( iModSz + iExpSz <= sizeof(pCtx->aPubKey) ) {
														memcpy(pCtx->aPubKey, pMod, iModSz);
														memcpy(pCtx->aPubKey + iModSz, pExp, iExpSz);
														pCtx->iPubKeyModSz = iModSz;
														pCtx->iPubKeySz = iModSz + iExpSz;
														#ifdef DEBUG_TRACE
															printf("    [TLS] Certificate: RSA key, mod=%d exp=%d\n",
																(int)iModSz, (int)iExpSz);
														#endif
													}
												} else {
													pCtx->bIsECPubKey = true;
													#ifdef DEBUG_TRACE
														printf("    [TLS] Certificate: EC key (not RSA)\n");
													#endif
												}
											}
										}
									}
								}
								
								pCtx->iState = XRT_TLS_CLIENT_WAIT_CV;
							} else if ( iMsgType == __XRT_TLS_CERTIFICATE_VERIFY ) {
								// 验证 CertificateVerify 签名
								if ( !pCtx->bSkipVerify && !pCtx->bIsECPubKey && pCtx->iPubKeyModSz > 0 ) {
									if ( iMsgBodyLen >= 4 ) {
										uint16 iSigAlg = __xrt_tls_load_be16(pMsg + __XRT_TLS_MSGHDR_SIZE);
										uint16 iSigLen = __xrt_tls_load_be16(pMsg + __XRT_TLS_MSGHDR_SIZE + 2);
										const uint8 *pSig = pMsg + __XRT_TLS_MSGHDR_SIZE + 4;
										
										if ( (iSigAlg == 0x0804) && (4 + iSigLen <= iMsgBodyLen) ) {
											uint8 aSigContent[130];
											xsha256_ctx tHash;
											uint8 aContentHash[32];
											size_t iContentLen = 0;
											
											memset(aSigContent, 0x20, 64);
											iContentLen = 64;
											memcpy(aSigContent + iContentLen, "TLS 1.3, server CertificateVerify", 34);
											iContentLen += 34;
											memcpy(aSigContent + iContentLen, pCtx->aSigHash, 32);
											iContentLen += 32;
											
											xrtSHA256Init(&tHash);
											xrtSHA256Update(&tHash, aSigContent, iContentLen);
											xrtSHA256Final(&tHash, aContentHash);
											
											#ifdef DEBUG_TRACE
												printf("    [TLS] CertificateVerify: RSA-PSS sig_alg=0x%04x sig_len=%d\n",
													iSigAlg, iSigLen);
											#endif
											
											if ( !xrtRSAPSSVerify(aContentHash, 32, pSig, iSigLen,
												pCtx->aPubKey, pCtx->iPubKeyModSz,
												pCtx->aPubKey + pCtx->iPubKeyModSz,
												pCtx->iPubKeySz - pCtx->iPubKeyModSz) ) {
												#ifdef DEBUG_TRACE
													printf("    [TLS] CertificateVerify: RSA-PSS verify FAILED\n");
												#endif
												return XRT_NET_ERROR;
											}
											#ifdef DEBUG_TRACE
												printf("    [TLS] CertificateVerify: RSA-PSS verify OK\n");
											#endif
										}
									}
								} else {
									#ifdef DEBUG_TRACE
										printf("    [TLS] CertificateVerify: skip verify\n");
									#endif
								}
								
								pCtx->iState = XRT_TLS_CLIENT_WAIT_FINISH;
							} else if ( iMsgType == __XRT_TLS_FINISHED ) {
								__xrt_tls_derive_application_keys(pCtx);
								__xrt_tls_send_finished(pCtx, false);
								pCtx->bHandshakeDone = true;
								pCtx->iState = XRT_TLS_CLIENT_CONNECTED;
								return XRT_NET_AGAIN;
							}
							
							iMsgOff += iTotalMsgLen;
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


