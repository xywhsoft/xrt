



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
#define __XRT_TLS_CHACHA20_POLY1305_SHA256  0x1303

// TLS 1.2 密码套件 ID
#define __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256  0xC02B
#define __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256    0xC02F
#define __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384  0xC02C
#define __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384    0xC030
#define __XRT_TLS12_RSA_AES128_GCM_SHA256          0x009C
#define __XRT_TLS12_RSA_AES256_GCM_SHA384          0x009D

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

// 从 X.509 证书 DER 中提取 EC 公钥 (uncompressed P-256, 65 字节)
static bool __xrt_tls_extract_ec_pubkey(uint8 *pCert, size_t iCertLen,
	uint8 **ppPubKey, size_t *pPubKeySz)
{
	struct __xrt_der_tlv tRoot, tRoot2, tCert, tTbsCert, tChild;
	
	if ( __xrt_der_parse(pCert, iCertLen, &tRoot) < 0 ) return false;
	
	// 查找 ecPublicKey OID
	struct __xrt_der_tlv tFound;
	if ( !__xrt_der_find_oid(&tRoot, __xrt_oid_ec_public_key,
		sizeof(__xrt_oid_ec_public_key), &tFound) ) {
		return false;
	}
	
	// 重新遍历找 SubjectPublicKeyInfo 中的 BIT STRING
	if ( __xrt_der_parse(pCert, iCertLen, &tRoot2) < 0 ) return false;
	if ( __xrt_der_next(&tRoot2, &tCert) <= 0 ) return false;
	tTbsCert = tCert;
	
	while ( __xrt_der_next(&tTbsCert, &tChild) > 0 ) {
		if ( tChild.iType == 0x30 ) {  // SEQUENCE
			struct __xrt_der_tlv tSeq = tChild;
			if ( __xrt_der_find_oid(&tSeq, __xrt_oid_ec_public_key,
				sizeof(__xrt_oid_ec_public_key), &tFound) ) {
				// 找到 SubjectPublicKeyInfo
				struct __xrt_der_tlv tSPKI = tChild;
				struct __xrt_der_tlv tAlgId, tBitStr;
				
				if ( __xrt_der_next(&tSPKI, &tAlgId) <= 0 ) return false;
				if ( __xrt_der_next(&tSPKI, &tBitStr) <= 0 ) return false;
				if ( tBitStr.iType != 0x03 ) return false;
				if ( tBitStr.iLen < 2 ) return false;  // unused bits + at least 1 byte
				
				// BIT STRING 第一个字节是 unused bits (should be 0)
				*ppPubKey = tBitStr.pValue + 1;
				*pPubKeySz = tBitStr.iLen - 1;
				return true;
			}
		}
	}
	
	return false;
}

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
	bool bIsTls12;             // 标记 TLS 1.2 模式
	bool bHandshakeDone;
	uint16 iCipherSuite;
	
	// TLS 1.3 握手数据
	xsha256_ctx tSHA256;           // 握手消息的 SHA-256 增量哈希
	uint8 aRandom[32];            // client random
	uint8 aSessionId[32];         // session ID
	uint8 aX25519Priv[32];        // X25519 私钥
	uint8 aX25519Pub[32];         // X25519 公钥
	uint8 aX25519Secret[32];      // X25519 共享密钥
	
	// TLS 1.3 加密密钥
	struct __xrt_tls_enc tEnc;
	struct __xrt_tls_enc tAppKeys;  // 应用数据密钥
	
	// TLS 1.2 特有字段
	uint8 aServerRandom[32];      // 服务器随机数
	uint8 aMasterSecret[48];      // 主密钥
	uint8 aP256Priv[32];          // ECDHE P-256 私钥
	uint8 aP256Pub[65];           // ECDHE P-256 公钥
	uint8 aPreMaster[48];         // RSA 模式 pre_master_secret
	uint8 aSharedSecret[32];      // ECDHE 共享密钥
	uint8 aClientWriteKey12[32];  // 客户端写密钥 (16 or 32)
	uint8 aServerWriteKey12[32];  // 服务端写密钥 (16 or 32)
	uint8 aClientWriteIV12[4];    // 客户端隐式 IV (GCM)
	uint8 aServerWriteIV12[4];    // 服务端隐式 IV (GCM)
	uint16 iKeyLen;               // 密钥长度 (16=AES128, 32=AES256)
	uint32 iClientSeq12;          // TLS 1.2 客户端序列号
	uint32 iServerSeq12;          // TLS 1.2 服务端序列号
	xsha256_ctx tSHA256_12;       // TLS 1.2 握手 SHA-256 哈希
	xsha512_ctx tSHA384_12;       // TLS 1.2 握手 SHA-384 哈希
	bool bUseSHA384;              // 当前套件是否使用 SHA-384
	bool bIsECDHE;                // 是否为 ECDHE 密钥交换
	
	// 配置
	bool bSkipVerify;
	char sHostname[254];
	
	// 服务端 SNI
	char sClientSNI[254];         // 服务端: 客户端请求的 SNI hostname
	void (*OnSNI)(xtlsctx *pCtx, const char *sHostName, ptr pUserData);
	ptr pSNIUserData;
	
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

// TLS 1.2 GCM 加密记录
// Nonce = implicit_iv(4) + explicit_nonce(8), AAD = seq(8)+type(1)+ver(2)+len(2)
// 输出: record_hdr(5) + explicit_nonce(8) + ciphertext + tag(16)
static void __xrt_tls12_encrypt_record(xtlsctx *pCtx, uint8 iType,
	const uint8 *pData, size_t iLen)
{
	// 12 字节 nonce = implicit_iv(4) + explicit_nonce(8)
	uint8 aNonce[12];
	uint8 *pImplicitIV = pCtx->bIsServer ? pCtx->aServerWriteIV12 : pCtx->aClientWriteIV12;
	uint8 *pKey = pCtx->bIsServer ? pCtx->aServerWriteKey12 : pCtx->aClientWriteKey12;
	uint32 *pSeq = pCtx->bIsServer ? &pCtx->iServerSeq12 : &pCtx->iClientSeq12;
	
	memcpy(aNonce, pImplicitIV, 4);
	// explicit nonce = 序列号 (8 字节 big-endian)
	memset(aNonce + 4, 0, 4);
	aNonce[8]  = (uint8)((*pSeq) >> 24);
	aNonce[9]  = (uint8)((*pSeq) >> 16);
	aNonce[10] = (uint8)((*pSeq) >> 8);
	aNonce[11] = (uint8)((*pSeq));
	
	// 13 字节 AAD = seq_num(8) + type(1) + version(2) + plaintext_length(2)
	uint8 aAAD[13];
	memset(aAAD, 0, 8);
	aAAD[4] = (uint8)((*pSeq) >> 24);
	aAAD[5] = (uint8)((*pSeq) >> 16);
	aAAD[6] = (uint8)((*pSeq) >> 8);
	aAAD[7] = (uint8)((*pSeq));
	aAAD[8] = iType;
	__xrt_tls_store_be16(aAAD + 9, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aAAD + 11, (uint16)iLen);
	
	// 记录头: type(1) + version(2) + length(2)
	// length = explicit_nonce(8) + ciphertext(iLen) + tag(16)
	uint8 aHdr[5];
	aHdr[0] = iType;
	__xrt_tls_store_be16(aHdr + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aHdr + 3, (uint16)(8 + iLen + 16));
	
	// 加密
	uint8 *pCipher = (uint8*)xrtMalloc(iLen + 16);
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
	
	if ( pCtx->iKeyLen == 32 ) {
		xrtAES256GCMEncrypt(pCipher, pKey, aNonce, 12, aAAD, 13, pData, iLen);
	} else {
		xrtAES128GCMEncrypt(pCipher, pKey, aNonce, 12, aAAD, 13, pData, iLen);
	}
	
	// 写入发送缓冲区: hdr + explicit_nonce + ciphertext + tag
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aHdr, 5);
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)(aNonce + 4), 8);  // explicit nonce
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)pCipher, iLen + 16);
	
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
				if ( pCtx->iKeyLen == 32 ) {
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
	
	xrtFree(pCipher);
	(*pSeq)++;
}

// TLS 1.2 GCM 解密记录
// pRecord 指向完整记录 (hdr + payload), pOut 接收明文
static bool __xrt_tls12_decrypt_record(xtlsctx *pCtx, const uint8 *pRecord,
	size_t iRecordLen, uint8 *pOut, size_t *pOutLen, uint8 *pType)
{
	if ( iRecordLen < __XRT_TLS_RECHDR_SIZE + 8 + 16 ) return false;  // hdr + nonce + tag
	
	uint8 *pImplicitIV = pCtx->bIsServer ? pCtx->aClientWriteIV12 : pCtx->aServerWriteIV12;
	uint8 *pKey = pCtx->bIsServer ? pCtx->aClientWriteKey12 : pCtx->aServerWriteKey12;
	uint32 *pSeq = pCtx->bIsServer ? &pCtx->iClientSeq12 : &pCtx->iServerSeq12;
	
	uint16 iPayloadLen = __xrt_tls_load_be16(pRecord + 3);
	if ( iPayloadLen < 8 + 16 ) return false;  // 至少 explicit_nonce + tag
	
	const uint8 *pPayload = pRecord + __XRT_TLS_RECHDR_SIZE;
	size_t iCipherLen = iPayloadLen - 8;  // ciphertext + tag (GCM 包含 tag)
	size_t iPlainLen = iCipherLen - 16;   // 减去 tag
	
	// 12 字节 nonce = implicit_iv(4) + explicit_nonce(8)
	uint8 aNonce[12];
	memcpy(aNonce, pImplicitIV, 4);
	memcpy(aNonce + 4, pPayload, 8);  // explicit nonce from record
	
	// 13 字节 AAD
	uint8 aAAD[13];
	memset(aAAD, 0, 8);
	aAAD[4] = (uint8)((*pSeq) >> 24);
	aAAD[5] = (uint8)((*pSeq) >> 16);
	aAAD[6] = (uint8)((*pSeq) >> 8);
	aAAD[7] = (uint8)((*pSeq));
	aAAD[8] = pRecord[0];  // content type
	memcpy(aAAD + 9, pRecord + 1, 2);  // version
	__xrt_tls_store_be16(aAAD + 11, (uint16)iPlainLen);
	
	bool bOK;
	if ( pCtx->iKeyLen == 32 ) {
		bOK = xrtAES256GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 13, pPayload + 8, iCipherLen);
	} else {
		bOK = xrtAES128GCMDecrypt(pOut, pKey, aNonce, 12, aAAD, 13, pPayload + 8, iCipherLen);
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
	uint8 aBuf[640];
	size_t iPos = 0;
	
	// 生成随机数和 X25519 密钥对
	xrtRandomBytes(pCtx->aRandom, 32);
	xrtRandomBytes(pCtx->aSessionId, 32);
	xrtX25519Keypair(pCtx->aX25519Priv, pCtx->aX25519Pub);
	
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
	
	// session ID (32 bytes)
	aBuf[iPos++] = 32;
	memcpy(aBuf + iPos, pCtx->aSessionId, 32);
	iPos += 32;
	
	// cipher suites (TLS 1.3 + TLS 1.2, 8 套件)
	__xrt_tls_store_be16(aBuf + iPos, 16);  // 8 suites * 2 bytes
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_CHACHA20_POLY1305_SHA256);  // TLS 1.3
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS_AES_128_GCM_SHA256);        // TLS 1.3
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256);  // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256);    // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384);  // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384);    // TLS 1.2
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_RSA_AES128_GCM_SHA256);           // TLS 1.2 RSA
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, __XRT_TLS12_RSA_AES256_GCM_SHA384);           // TLS 1.2 RSA
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
	__xrt_tls_store_be16(aBuf + iPos, 18);   // 扩展数据长度 (8 algorithms * 2 + 2)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 16);   // 签名算法列表长度 (8 * 2)
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
	__xrt_tls_store_be16(aBuf + iPos, 0x0401);  // RSA-PKCS1-SHA256 (TLS 1.2 兼容)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0501);  // RSA-PKCS1-SHA384 (TLS 1.2 兼容)
	iPos += 2;
	__xrt_tls_store_be16(aBuf + iPos, 0x0503);  // ECDSA-SECP384R1-SHA384 (TLS 1.2 兼容)
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
	__xrt_tls12_update_hash(pCtx, aBuf, iPos);
	
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

// 解析 ServerHello 消息 (支持 TLS 1.3 和 TLS 1.2)
static bool __xrt_tls_parse_server_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	if ( iLen < 2 + 32 + 1 ) return false;
	
	size_t iPos = 0;
	
	// legacy server version
	uint16 iLegacyVer = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	// server random
	memcpy(pCtx->aServerRandom, pMsg + iPos, 32);
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
				// 解析服务器的 X25519 公钥 (TLS 1.3)
				if ( iExtDataLen >= 36 ) {
					uint16 iGroup = __xrt_tls_load_be16(pMsg + iPos);
					uint16 iKeyLen = __xrt_tls_load_be16(pMsg + iPos + 2);
					if ( iGroup == 0x001d && iKeyLen == 32 ) {
						xrtX25519SharedSecret(pCtx->aX25519Secret, pCtx->aX25519Priv, pMsg + iPos + 4);
					}
				}
			}
			
			iPos += iExtDataLen;
		}
	}
	
	if ( bFoundTls13 ) {
		// TLS 1.3 协商成功
		pCtx->bIsTls12 = false;
		#ifdef DEBUG_TRACE
			printf("    [TLS] ServerHello: TLS 1.3, cipher=0x%04x\n", pCtx->iCipherSuite);
		#endif
		return true;
	}
	
	// 没有 supported_versions 扩展 + legacy version == 0x0303 → TLS 1.2
	if ( iLegacyVer == __XRT_TLS_VERSION_1_2 ) {
		pCtx->bIsTls12 = true;
		
		// 验证密码套件是否受支持
		switch ( pCtx->iCipherSuite ) {
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
			default:
				#ifdef DEBUG_TRACE
					printf("    [TLS] ServerHello: unsupported TLS 1.2 cipher 0x%04x\n", pCtx->iCipherSuite);
				#endif
				return false;
		}
		
		// 确定密钥交换类型
		switch ( pCtx->iCipherSuite ) {
			case __XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256:
			case __XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256:
			case __XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384:
			case __XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384:
				pCtx->bIsECDHE = true;
				break;
			default:
				pCtx->bIsECDHE = false;
				break;
		}
		
		#ifdef DEBUG_TRACE
			printf("    [TLS] ServerHello: TLS 1.2, cipher=0x%04x, sha384=%d, ecdhe=%d, keyLen=%d\n",
				pCtx->iCipherSuite, pCtx->bUseSHA384, pCtx->bIsECDHE, pCtx->iKeyLen);
		#endif
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

// Step 5: 解析 TLS 1.2 Certificate 消息
// TLS 1.2 格式: cert_list_len(3) + [cert_len(3) + cert]*  (无 request_context, 无 extensions)
static bool __xrt_tls12_parse_certificate(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	if ( iLen < 3 ) return false;
	
	uint32 iCertListLen = __xrt_tls_load_be24(pMsg);
	size_t iOff = 3;
	(void)iCertListLen;
	
	// 解析第一个证书 (叶子证书)
	if ( iOff + 3 > iLen ) return false;
	uint32 iCertLen = __xrt_tls_load_be24(pMsg + iOff);
	iOff += 3;
	
	if ( iOff + iCertLen > iLen || iCertLen == 0 ) return false;
	
	uint8 *pCertData = (uint8*)(pMsg + iOff);
	
	// 尝试提取 RSA 公钥
	uint8 *pMod = NULL, *pExp = NULL;
	size_t iModSz = 0, iExpSz = 0;
	
	if ( __xrt_tls_extract_rsa_pubkey(pCertData, iCertLen, &pMod, &iModSz, &pExp, &iExpSz) ) {
		pCtx->bIsECPubKey = false;
		if ( iModSz + iExpSz <= sizeof(pCtx->aPubKey) ) {
			memcpy(pCtx->aPubKey, pMod, iModSz);
			memcpy(pCtx->aPubKey + iModSz, pExp, iExpSz);
			pCtx->iPubKeyModSz = iModSz;
			pCtx->iPubKeySz = iModSz + iExpSz;
			#ifdef DEBUG_TRACE
				printf("    [TLS12] Certificate: RSA key, mod=%d exp=%d\n", (int)iModSz, (int)iExpSz);
			#endif
		}
		return true;
	}
	
	// 尝试提取 EC 公钥
	uint8 *pECPub = NULL;
	size_t iECPubSz = 0;
	if ( __xrt_tls_extract_ec_pubkey(pCertData, iCertLen, &pECPub, &iECPubSz) ) {
		pCtx->bIsECPubKey = true;
		if ( iECPubSz <= sizeof(pCtx->aPubKey) ) {
			memcpy(pCtx->aPubKey, pECPub, iECPubSz);
			pCtx->iPubKeySz = iECPubSz;
			#ifdef DEBUG_TRACE
				printf("    [TLS12] Certificate: EC key, size=%d\n", (int)iECPubSz);
			#endif
		}
		return true;
	}
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Certificate: failed to extract public key\n");
	#endif
	return false;
}

// Step 6: 解析 ServerKeyExchange (ECDHE 套件)
static bool __xrt_tls12_parse_server_key_exchange(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	size_t iOff = 0;
	
	// ECParameters: curve_type(1) + named_curve(2)
	if ( iOff + 3 > iLen ) return false;
	if ( pMsg[iOff] != 3 ) return false;  // named_curve
	uint16 iCurve = __xrt_tls_load_be16(pMsg + iOff + 1);
	if ( iCurve != 0x0017 ) return false;  // secp256r1
	iOff += 3;
	
	// EC point: point_len(1) + point(65)
	if ( iOff + 1 > iLen ) return false;
	uint8 iPointLen = pMsg[iOff++];
	if ( iPointLen != 65 || pMsg[iOff] != 0x04 ) return false;  // uncompressed
	if ( iOff + 65 > iLen ) return false;
	
	const uint8 *pServerPub = pMsg + iOff;  // 服务器 P-256 公钥
	iOff += 65;
	
	// 保存 server_params 的位置和长度 (用于签名验证)
	size_t iParamsLen = iOff;  // curve_type(1) + named_curve(2) + point_len(1) + point(65)
	
	// 解析签名: sig_hash_alg(2) + sig_len(2) + signature
	if ( iOff + 4 > iLen ) return false;
	uint16 iSigAlg = __xrt_tls_load_be16(pMsg + iOff);
	iOff += 2;
	uint16 iSigLen = __xrt_tls_load_be16(pMsg + iOff);
	iOff += 2;
	if ( iOff + iSigLen > iLen ) return false;
	const uint8 *pSig = pMsg + iOff;
	
	// 验证签名: 对 client_random(32) + server_random(32) + server_params 进行验签
	if ( !pCtx->bSkipVerify ) {
		uint8 aSigBuf[128];
		size_t iSigBufLen = 32 + 32 + iParamsLen;
		uint8 *pSigData = (iSigBufLen <= sizeof(aSigBuf)) ? aSigBuf : (uint8*)xrtMalloc(iSigBufLen);
		if ( !pSigData ) return false;
		
		memcpy(pSigData, pCtx->aRandom, 32);          // client_random
		memcpy(pSigData + 32, pCtx->aServerRandom, 32); // server_random
		memcpy(pSigData + 64, pMsg, iParamsLen);         // server_params
		
		// 计算哈希
		uint8 aHash[48];
		size_t iHashLen;
		uint16 iHashAlg = (iSigAlg >> 8) & 0xFF;  // hash algorithm
		
		if ( iHashAlg == 4 ) {  // SHA-256
			xrtSHA256((const ptr)pSigData, iSigBufLen, aHash);
			iHashLen = 32;
		} else if ( iHashAlg == 5 ) {  // SHA-384
			xrtSHA384((const ptr)pSigData, iSigBufLen, aHash);
			iHashLen = 48;
		} else {
			// 默认使用 SHA-256
			xrtSHA256((const ptr)pSigData, iSigBufLen, aHash);
			iHashLen = 32;
		}
		
		if ( pSigData != aSigBuf ) xrtFree(pSigData);
		
		// 根据密码套件类型验证签名
		bool bVerifyOK = false;
		
		uint8 iSigType = iSigAlg & 0xFF;  // signature algorithm type
		
		if ( iSigAlg == 0x0804 || iSigAlg == 0x0805 || iSigAlg == 0x0806 ) {
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
	
	// 生成 ECDH P-256 密钥对
	xrtECDHSecp256r1Keypair(pCtx->aP256Priv, pCtx->aP256Pub);
	
	// 计算共享密钥
	xrtECDHSecp256r1SharedSecret(pCtx->aSharedSecret, pCtx->aP256Priv, pServerPub);
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] ServerKeyExchange: ECDH shared secret computed\n");
		printf("    [TLS12]   ServerPub: ");
		for (int i = 0; i < 65; i++) printf("%02x", pServerPub[i]);
		printf("\n    [TLS12]   ClientPub: ");
		for (int i = 0; i < 65; i++) printf("%02x", pCtx->aP256Pub[i]);
		printf("\n    [TLS12]   SharedSecret: ");
		for (int i = 0; i < 32; i++) printf("%02x", pCtx->aSharedSecret[i]);
		printf("\n");
		// Self-test: verify our own pub key round-trip
		{
			uint8 aTestPriv[32], aTestPub[65], aTestPriv2[32], aTestPub2[65];
			uint8 aSecret1[32], aSecret2[32];
			xrtECDHSecp256r1Keypair(aTestPriv, aTestPub);
			xrtECDHSecp256r1Keypair(aTestPriv2, aTestPub2);
			xrtECDHSecp256r1SharedSecret(aSecret1, aTestPriv, aTestPub2);
			xrtECDHSecp256r1SharedSecret(aSecret2, aTestPriv2, aTestPub);
			printf("    [TLS12]   P256 ECDH self-test: %s\n", 
				memcmp(aSecret1, aSecret2, 32) == 0 ? "PASS" : "FAIL");
		}
		// NIST ECDH test vector
		{
			uint8 nPrivA[] = {0x7d,0x7d,0xc5,0xf7,0x1e,0xb2,0x9d,0xda,0xf8,0x0d,0x62,0x14,0x63,0x2e,0xea,0xe0,
				0x3d,0x90,0x58,0xaf,0x1f,0xb6,0xd2,0x2e,0xd8,0x0b,0xad,0xb6,0x2b,0xc1,0xa5,0x34};
			uint8 nPubB[65] = {0x04,
				0x80,0x9f,0x04,0x28,0x9c,0x64,0x34,0x8c,0x01,0x51,0x5e,0xb0,0x3d,0x5c,0xe7,0xac,
				0x1a,0x8c,0xb9,0x49,0x8f,0x5c,0xaa,0x50,0x19,0x7e,0x58,0xd4,0x3a,0x86,0xa7,0xae,
				0xb2,0x9d,0x84,0xe8,0x11,0x19,0x7f,0x25,0xeb,0xa8,0xf5,0x19,0x40,0x92,0xcb,0x6f,
				0xf4,0x40,0xe2,0x6d,0x44,0x21,0x01,0x13,0x72,0x46,0x1f,0x57,0x92,0x71,0xcd,0xa3};
			uint8 nExpected[] = {0xc4,0x24,0xdb,0x57,0xe2,0xc1,0x53,0x55,0xb3,0xc9,0xe1,0x00,0x00,0x9b,0x6f,0xf4,
				0x05,0xe0,0x03,0x90,0x69,0x1b,0xbd,0xba,0xf3,0xe6,0xab,0xd7,0x45,0xaf,0x99,0x34};
			uint8 nOut[32];
			xrtECDHSecp256r1SharedSecret(nOut, nPrivA, nPubB);
			printf("    [TLS12]   NIST P256 ECDH test: %s\n", memcmp(nOut, nExpected, 32) == 0 ? "PASS" : "FAIL");
			if (memcmp(nOut, nExpected, 32) != 0) {
				printf("    [TLS12]     Got:      ");
				for (int i = 0; i < 32; i++) printf("%02x", nOut[i]);
				printf("\n    [TLS12]     Expected: ");
				for (int i = 0; i < 32; i++) printf("%02x", nExpected[i]);
				printf("\n");
			}
		}
	#endif
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
		// ECDHE: 发送客户端 P-256 公钥 (1 + 65 字节)
		aBuf[iPos++] = 65;  // point length
		memcpy(aBuf + iPos, pCtx->aP256Pub, 65);
		iPos += 65;
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
	
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aRec, 5);
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aBuf, iPos);
}

// Step 8: TLS 1.2 密钥派生
static void __xrt_tls12_derive_keys(xtlsctx *pCtx)
{
	const uint8 *pPreMaster;
	size_t iPMLen;
	
	if ( pCtx->bIsECDHE ) {
		pPreMaster = pCtx->aSharedSecret;
		iPMLen = 32;
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
	
	// AES-GCM: key_block = client_write_key + server_write_key + client_IV + server_IV
	size_t iBlockLen = pCtx->iKeyLen * 2 + 4 * 2;  // 2 keys + 2 IVs (4 bytes each)
	uint8 aKeyBlock[72];  // max: 32*2 + 4*2 = 72
	
	__xrt_tls12_prf(pCtx, aKeyBlock, iBlockLen,
		pCtx->aMasterSecret, 48,
		"key expansion", 13, aSeed2, 64);
	
	// 拆分 key_block
	size_t iOff = 0;
	memcpy(pCtx->aClientWriteKey12, aKeyBlock + iOff, pCtx->iKeyLen); iOff += pCtx->iKeyLen;
	memcpy(pCtx->aServerWriteKey12, aKeyBlock + iOff, pCtx->iKeyLen); iOff += pCtx->iKeyLen;
	memcpy(pCtx->aClientWriteIV12, aKeyBlock + iOff, 4); iOff += 4;
	memcpy(pCtx->aServerWriteIV12, aKeyBlock + iOff, 4);
	
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
		for (int i = 0; i < 4; i++) printf("%02x", pCtx->aClientWriteIV12[i]);
		printf("\n    [TLS12]   ServerIV: ");
		for (int i = 0; i < 4; i++) printf("%02x", pCtx->aServerWriteIV12[i]);
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
static void __xrt_tls12_send_ccs_finished(xtlsctx *pCtx)
{
	// 1. 发送 ChangeCipherSpec 记录 (明文, 1字节 0x01)
	uint8 aCCS[6];
	aCCS[0] = __XRT_TLS_CHANGE_CIPHER;
	__xrt_tls_store_be16(aCCS + 1, __XRT_TLS_VERSION_1_2);
	__xrt_tls_store_be16(aCCS + 3, 1);
	aCCS[5] = 0x01;
	xrtNetBufAppend(&pCtx->tSendBuf, (const char*)aCCS, 6);
	
	// 2. 计算 verify_data = PRF(master_secret, "client finished", Hash(handshake_messages))[0..11]
	uint8 aHash[48];
	__xrt_tls12_get_hash(pCtx, aHash);
	
	uint8 aVerifyData[12];
	size_t iHashLen = pCtx->bUseSHA384 ? 48 : 32;
	__xrt_tls12_prf(pCtx, aVerifyData, 12,
		pCtx->aMasterSecret, 48,
		"client finished", 15, aHash, iHashLen);
	
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
		printf("\n    [TLS12] VerifyData: ");
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

// 验证服务端 Finished
static bool __xrt_tls12_verify_finished(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	if ( iLen < 12 ) return false;
	
	// 计算 expected = PRF(master_secret, "server finished", Hash(handshake_messages))[0..11]
	uint8 aHash[48];
	__xrt_tls12_get_hash(pCtx, aHash);
	
	uint8 aExpected[12];
	size_t iHashLen = pCtx->bUseSHA384 ? 48 : 32;
	__xrt_tls12_prf(pCtx, aExpected, 12,
		pCtx->aMasterSecret, 48,
		"server finished", 15, aHash, iHashLen);
	
	// 安全比较
	uint8 iDiff = 0;
	for ( int i = 0; i < 12; i++ ) iDiff |= aExpected[i] ^ pMsg[i];
	
	#ifdef DEBUG_TRACE
		printf("    [TLS12] Verify server Finished: %s\n", iDiff ? "FAILED" : "OK");
	#endif
	return (iDiff == 0);
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
	
	// 初始化 TLS 1.2 哈希上下文 (两个都初始化, 待协商后再选择)
	xrtSHA256Init(&pCtx->tSHA256_12);
	xrtSHA384Init(&pCtx->tSHA384_12);
	
	// 初始化 IO 缓冲区
	xrtNetBufInit(&pCtx->tSendBuf, 8192);
	xrtNetBufInit(&pCtx->tRecvBuf, 8192);
	
	if ( pConfig ) {
		pCtx->bSkipVerify = !pConfig->bVerifyPeer;
		if ( pConfig->sHostName ) {
			strncpy(pCtx->sHostname, pConfig->sHostName, sizeof(pCtx->sHostname) - 1);
		}
		pCtx->OnSNI = pConfig->OnSNI;
		pCtx->pSNIUserData = pConfig->pSNIUserData;
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
	memset(pCtx->aMasterSecret, 0, 48);
	memset(pCtx->aP256Priv, 0, 32);
	memset(pCtx->aSharedSecret, 0, 32);
	memset(pCtx->aClientWriteKey12, 0, 32);
	memset(pCtx->aServerWriteKey12, 0, 32);
	
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
			
			xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			
			if ( pCtx->bIsTls12 ) {
				// TLS 1.2: 不派生握手密钥, 后续消息仍为明文
				pCtx->iState = XRT_TLS12_CLIENT_WAIT_CERT;
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
		
		// ==================== TLS 1.2 客户端状态处理 ====================
		case XRT_TLS12_CLIENT_WAIT_CERT:
		case XRT_TLS12_CLIENT_WAIT_SKE:
		case XRT_TLS12_CLIENT_WAIT_SHD: {
			#ifdef DEBUG_TRACE
				printf("    [TLS12] CERT/SKE/SHD entry: aRandom: ");
				for (int i = 0; i < 32; i++) printf("%02x", pCtx->aRandom[i]);
				printf("\n");
			#endif
			// TLS 1.2 握手消息在 CCS 前是明文传输
			char aBuf[4096];
			size_t iRecvd = 0;
			xnet_result iRes = xrtSockRecv(pConn, aBuf, sizeof(aBuf), &iRecvd);
			if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
				xrtNetBufAppend(&pCtx->tRecvBuf, aBuf, iRecvd);
			} else if ( iRes == XRT_NET_CLOSED ) {
				return XRT_NET_ERROR;
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
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				
				if ( iRecType != __XRT_TLS_HANDSHAKE ) {
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
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
							__xrt_tls12_send_ccs_finished(pCtx);
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
				
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				
				// 如果状态已过渡到 WAIT_CCS, 停止处理更多记录
				if ( pCtx->iState == XRT_TLS12_CLIENT_WAIT_CCS ) break;
			}
			
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS12_CLIENT_WAIT_CCS: {
			// 等待服务端 ChangeCipherSpec
			char aBuf[4096];
			size_t iRecvd = 0;
			xnet_result iRes = xrtSockRecv(pConn, aBuf, sizeof(aBuf), &iRecvd);
			#ifdef DEBUG_TRACE
				printf("    [TLS12] WAIT_CCS: recv res=%d recvd=%d\n", (int)iRes, (int)iRecvd);
			#endif
			if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
				xrtNetBufAppend(&pCtx->tRecvBuf, aBuf, iRecvd);
			} else if ( iRes == XRT_NET_CLOSED ) {
				return XRT_NET_ERROR;
			}
			
			while ( pCtx->tRecvBuf.iSize >= __XRT_TLS_RECHDR_SIZE ) {
				uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
				uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
				
				if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) break;
				
				if ( iRecType == __XRT_TLS_CHANGE_CIPHER ) {
					#ifdef DEBUG_TRACE
						printf("    [TLS12] Got server CCS\n");
					#endif
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					pCtx->iState = XRT_TLS12_CLIENT_WAIT_FINISH;
					break;
				} else if ( iRecType == __XRT_TLS_ALERT ) {
					#ifdef DEBUG_TRACE
						const uint8 *pA = (const uint8*)pCtx->tRecvBuf.pData + 5;
						printf("    [TLS12] WAIT_CCS: Alert level=%d desc=%d\n", pA[0], (iRecLen >= 2) ? pA[1] : -1);
					#endif
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			}
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS12_CLIENT_WAIT_FINISH: {
			// 解密验证服务端 Finished
			char aBuf[4096];
			size_t iRecvd = 0;
			xnet_result iRes = xrtSockRecv(pConn, aBuf, sizeof(aBuf), &iRecvd);
			if ( iRes == XRT_NET_OK && iRecvd > 0 ) {
				xrtNetBufAppend(&pCtx->tRecvBuf, aBuf, iRecvd);
			} else if ( iRes == XRT_NET_CLOSED ) {
				return XRT_NET_ERROR;
			}
			
			if ( pCtx->tRecvBuf.iSize < __XRT_TLS_RECHDR_SIZE ) return XRT_NET_AGAIN;
			
			uint16 iRecLen = __xrt_tls_load_be16((const uint8*)pCtx->tRecvBuf.pData + 3);
			if ( pCtx->tRecvBuf.iSize < (size_t)(__XRT_TLS_RECHDR_SIZE + iRecLen) ) return XRT_NET_AGAIN;
			
			uint8 iRecType = (uint8)pCtx->tRecvBuf.pData[0];
			
			if ( iRecType == __XRT_TLS_ALERT ) {
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
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
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				
				// 解析 Finished 握手消息
				if ( iPlainLen >= __XRT_TLS_MSGHDR_SIZE && aPlain[0] == __XRT_TLS_FINISHED ) {
					uint32 iFinLen = __xrt_tls_load_be24(aPlain + 1);
					if ( iFinLen >= 12 ) {
						if ( !__xrt_tls12_verify_finished(pCtx, aPlain + __XRT_TLS_MSGHDR_SIZE, iFinLen) ) {
							#ifdef DEBUG_TRACE
								printf("    [TLS12] Server Finished verify FAILED\n");
							#endif
							return XRT_NET_ERROR;
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
					xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
					return XRT_NET_ERROR;
				}
				
				xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
				
				if ( iPlainLen >= __XRT_TLS_MSGHDR_SIZE && aPlain[0] == __XRT_TLS_FINISHED ) {
					uint32 iFinLen = __xrt_tls_load_be24(aPlain + 1);
					if ( iFinLen >= 12 ) {
						if ( !__xrt_tls12_verify_finished(pCtx, aPlain + __XRT_TLS_MSGHDR_SIZE, iFinLen) ) {
							return XRT_NET_ERROR;
						}
						pCtx->bHandshakeDone = true;
						pCtx->iState = XRT_TLS12_CLIENT_CONNECTED;
						return XRT_NET_AGAIN;
					}
				}
				return XRT_NET_ERROR;
			}
			
			xrtNetBufConsume(&pCtx->tRecvBuf, __XRT_TLS_RECHDR_SIZE + iRecLen);
			return XRT_NET_AGAIN;
		}
		
		case XRT_TLS_CLIENT_CONNECTED:
		case XRT_TLS12_CLIENT_CONNECTED:
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
			
			bool bOK;
			if ( pCtx->bIsTls12 ) {
				bOK = __xrt_tls12_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
					__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType);
			} else {
				bool bUseServerKeys = !pCtx->bIsServer;
				bOK = __xrt_tls_decrypt_record(pCtx, (const uint8*)pCtx->tRecvBuf.pData,
					__XRT_TLS_RECHDR_SIZE + iRecLen, aPlain, &iPlainLen, &iContentType, bUseServerKeys);
			}
			
			if ( bOK ) {
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

// Step 12: 服务端 ClientHello 解析 (提取 SNI)
static bool __xrt_tls_parse_client_hello(xtlsctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	size_t iPos = 0;
	
	// client_version(2)
	if ( iPos + 2 > iLen ) return false;
	iPos += 2;
	
	// client_random(32) — 保存到上下文
	if ( iPos + 32 > iLen ) return false;
	memcpy(pCtx->aRandom, pMsg + iPos, 32);
	iPos += 32;
	
	// session_id
	if ( iPos + 1 > iLen ) return false;
	uint8 iSessLen = pMsg[iPos++];
	if ( iPos + iSessLen > iLen ) return false;
	if ( iSessLen <= 32 ) {
		memcpy(pCtx->aSessionId, pMsg + iPos, iSessLen);
	}
	iPos += iSessLen;
	
	// cipher_suites
	if ( iPos + 2 > iLen ) return false;
	uint16 iSuitesLen = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	if ( iPos + iSuitesLen > iLen ) return false;
	iPos += iSuitesLen;
	
	// compression_methods
	if ( iPos + 1 > iLen ) return false;
	uint8 iCompLen = pMsg[iPos++];
	if ( iPos + iCompLen > iLen ) return false;
	iPos += iCompLen;
	
	// 解析扩展
	if ( iPos + 2 > iLen ) return true;  // 无扩展也可以
	uint16 iExtTotalLen = __xrt_tls_load_be16(pMsg + iPos);
	iPos += 2;
	
	size_t iExtEnd = iPos + iExtTotalLen;
	if ( iExtEnd > iLen ) iExtEnd = iLen;
	
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
		}
		
		iPos += iExtDataLen;
	}
	
	return true;
}

// Step 13: SNI 相关公共 API

XXAPI const char* xrtTlsGetSNI(xtlsctx *pCtx)
{
	if ( !pCtx ) return NULL;
	if ( pCtx->sClientSNI[0] == '\0' ) return NULL;
	return pCtx->sClientSNI;
}

XXAPI xnet_result xrtTlsSetCert(xtlsctx *pCtx, const char *sCertFile, const char *sKeyFile)
{
	if ( !pCtx ) return XRT_NET_ERROR;
	// TODO: 加载 PEM/DER 证书和私钥文件到 pCtx->pCertDer / pCtx->pKeyDer
	// 这个 API 主要用于 SNI 回调中根据域名动态配置证书
	(void)sCertFile;
	(void)sKeyFile;
	return XRT_NET_OK;
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
	
	// Compute i*G and verify Y against the provided point's Y
	// (verifying if the Y in the test vector is correct)
	__xrt_p256_jpt tIG;
	__xrt_p256_scalar_mult(&tIG, k, __xrt_p256_Gx, __xrt_p256_Gy);
	uint32 ig_ax[8], ig_ay[8];
	__xrt_p256_to_affine(ig_ax, ig_ay, &tIG);
	printf("  k*G.x matches input pub.x: %s\n", memcmp(ig_ax, px, 32) == 0 ? "PASS" : "FAIL");
	printf("  k*G.y matches input pub.y: %s\n", memcmp(ig_ay, py, 32) == 0 ? "PASS" : "FAIL");
	if (memcmp(ig_ay, py, 32) != 0) {
		uint8 aActY[32], aExpY[32];
		__xrt_u256_to_be(aActY, ig_ay); __xrt_u256_to_be(aExpY, py);
		printf("    Computed Y: "); for(int i=0;i<32;i++) printf("%02x",aActY[i]); printf("\n");
		printf("    Expected Y: "); for(int i=0;i<32;i++) printf("%02x",aExpY[i]); printf("\n");
		// Check if computed point is on curve
		__xrt_p256_sqr_mod_p(lhs, ig_ay);
		__xrt_p256_sqr_mod_p(t1, ig_ax);
		__xrt_p256_mul_mod_p(t1, t1, ig_ax);
		__xrt_p256_add_mod_p(t2, ig_ax, ig_ax);
		__xrt_p256_add_mod_p(t2, t2, ig_ax);
		__xrt_p256_sub_mod_p(rhs, t1, t2);
		__xrt_p256_add_mod_p(rhs, rhs, p256_b);
		printf("  k*G on curve: %s\n", memcmp(lhs, rhs, 32) == 0 ? "PASS" : "FAIL");
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

