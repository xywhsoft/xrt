#ifndef XRT_INTERNAL_TLS_H
#define XRT_INTERNAL_TLS_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_TLS)

/* 设置带可选输入偏移的 TLS 结构化错误。 */
void __xrtTlsError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
);



/* 设置带原因链和可选输入偏移的 TLS 结构化错误。 */
void __xrtTlsErrorCause(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset,
	const xerror* pCause
);



/* 读取网络字节序 16 位整数。 */
uint16 __xrtTlsRead16(const uint8* pData);



/* 写入网络字节序 16 位整数。 */
void __xrtTlsWrite16(uint8* pData, uint16 iValue);



/* 读取网络字节序 32 位整数。 */
uint32 __xrtTlsRead32(const uint8* pData);



/* 写入网络字节序 32 位整数。 */
void __xrtTlsWrite32(uint8* pData, uint32 iValue);



/* 检查借用字节视图是否具有一致的空值语义。 */
bool __xrtTlsViewValid(xbytesview View);



/* 判断借用输入是否与给定输出区域重叠。 */
bool __xrtTlsViewOverlap(
	const void* pOutput,
	size_t iOutputSize,
	xbytesview Input
);



/* 判断版本是否属于 XRT 协商的 TLS 1.2 或 TLS 1.3。 */
bool __xrtTlsVersionSupported(xtlsversion Version);



/* 检查是否是 XRT 接受的 TLS 记录类型。 */
bool __xrtTlsRecordTypeValid(xtlsrecordtype Type);



#if defined(XRT_FEATURE_TLS_HANDSHAKE)

/* 读取网络字节序 24 位整数。 */
uint32 __xrtTlsRead24(const uint8* pData);



/* 写入网络字节序 24 位整数。 */
void __xrtTlsWrite24(uint8* pData, uint32 iValue);

#endif



#if defined(XRT_FEATURE_TLS_HELLO)

/* 设置 Hello 语义层协议错误并返回 false。 */
bool __xrtTlsHelloError(
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
);



/* 返回 256 个精确桶中一个 16 位标识对应的位。 */
uint64 __xrtTlsHelloSeenBit(uint16 iValue);



/* 验证去除线路长度前缀后的非空偶数标识列表。 */
bool __xrtTlsIdsDataValid(
	xbytesview Data,
	cstr sOperation,
	xerrkind Kind
);



/* 验证 ClientHello 或 ServerHello 中已知核心扩展的局部语义。 */
bool __xrtTlsHelloExtensions(
	xbytesview Extensions,
	bool bServer,
	bool bRetry
);



/* 验证 ClientHello 的 TLS 1.3 兼容压缩字段。 */
bool __xrtTlsClientCompressionValid(
	const xtlsclienthello* pHello
);



/* 判断 random 是否是 HelloRetryRequest 固定标记。 */
bool __xrtTlsHelloRetry(xbytesview Random);

#endif



#if defined(XRT_FEATURE_TLS_MESSAGES)

/* 验证 EncryptedExtensions 中已知扩展的局部线路语义。 */
bool __xrtTlsEncryptedExtensionsValid(xbytesview Extensions);



/* 验证 TLS 1.3 NewSessionTicket 扩展的局部线路语义。 */
bool __xrtTlsTicketExtensionsValid(xbytesview Extensions);

#endif



#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES)

/* 验证完整证书颁发者向量并可选发布去除前缀后的游标。 */
bool __xrtTlsAuthoritiesValid(
	xbytesview Data,
	xtlsauthoritycursor* pCursor,
	cstr sOperation,
	xerrkind Kind
);



/* 验证 TLS 1.2 CertificateRequest 的全部借用字段。 */
bool __xrtTls12CertificateRequestValid(
	const xtls12certificaterequest* pRequest,
	cstr sOperation,
	xerrkind Kind
);



/* 验证 TLS 1.3 CertificateRequest 扩展并发布常用认证字段。 */
bool __xrtTls13CertificateRequestExtensions(
	xbytesview Extensions,
	xtlsids* pSignatures,
	xtlsids* pCertificateSignatures,
	xbytesview* pAuthorityData,
	cstr sOperation,
	xerrkind Kind
);



/* 验证 TLS 1.2 ECDHE ServerKeyExchange 的可编码字段。 */
bool __xrtTls12ServerKeyExchangeValid(
	uint16 iGroup,
	xbytesview PublicKey,
	const xtlscertificateverify* pVerify,
	cstr sOperation,
	xerrkind Kind
);



/* 验证 OCSP CertificateStatus 的可编码字段。 */
bool __xrtTlsCertificateStatusValid(
	const xtlscertificatestatusmessage* pStatus,
	cstr sOperation,
	xerrkind Kind
);



/* 验证 CompressedCertificate 的可编码字段。 */
bool __xrtTlsCompressedCertificateValid(
	const xtlscompressedcertificate* pCertificate,
	cstr sOperation,
	xerrkind Kind
);

#endif



#if defined(XRT_FEATURE_TLS_SCHEDULE)

/* TLS 1.2 与 TLS 1.3 当前允许的最大握手摘要长度。 */
#define XTLS_TRANSCRIPT_HASH_MAX_SIZE 48u

/* TLS 1.2 PRF 单次派生采用明确的 16 位输出上限。 */
#define XTLS12_PRF_MAX_SIZE 65535u



/* 握手 transcript 只保存协商密码套件实际使用的一种摘要状态。 */
typedef struct xtlstranscript {
	xcryptohash Hash;
	bool Ready;
	union {
		#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
			xsha256 Sha256;
		#endif
		#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
			xsha384 Sha384;
		#endif
		uint64 Align;
	} State;
} xtlstranscript;



/* 返回当前构建是否提供指定 TLS 摘要后端。 */
bool __xrtTlsScheduleHashSupported(xcryptohash Hash);



/* 把 TLS 套件摘要映射到密码底座摘要标识，未知值返回零。 */
xcryptohash __xrtTlsHash(xtlshash Hash);



/* 返回 TLS 调度允许的摘要长度，不支持的算法返回零。 */
size_t __xrtTlsScheduleHashSize(xcryptohash Hash);



/* 初始化只跟踪一种已协商摘要的握手 transcript。 */
bool __xrtTlsTranscriptInit(xtlstranscript* pTranscript, xcryptohash Hash);



/* 清除 transcript 的全部摘要中间状态。 */
void __xrtTlsTranscriptClear(xtlstranscript* pTranscript);



/* 原子地向 transcript 追加一段已编码握手消息。 */
bool __xrtTlsTranscriptUpdate(xtlstranscript* pTranscript, xbytesview Message);



/* 从 transcript 快照输出摘要，不结束或修改原状态。 */
bool __xrtTlsTranscriptDigest(
	const xtlstranscript* pTranscript,
	void* pDigest,
	size_t iDigestSize
);



/* 按 TLS 1.3 message_hash 规则重建 HelloRetryRequest transcript。 */
bool __xrtTlsTranscriptRetry(xtlstranscript* pTranscript);



/* 执行 TLS 1.3 HKDF-Extract，输出必须恰好容纳摘要长度。 */
bool __xrtTls13Extract(
	xcryptohash Hash,
	xbytesview Salt,
	xbytesview Ikm,
	void* pSecret,
	size_t iSecretSize
);



/* 编码完整 HkdfLabel 并执行 TLS 1.3 HKDF-Expand-Label。 */
bool __xrtTls13ExpandLabel(
	xcryptohash Hash,
	xbytesview Secret,
	xstrview Label,
	xbytesview Context,
	void* pOutput,
	size_t iOutputSize
);



/* 使用 transcript 摘要派生一个 TLS 1.3 固定长度 secret。 */
bool __xrtTls13DeriveSecret(
	xcryptohash Hash,
	xbytesview Secret,
	xstrview Label,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
);



/* 输出当前摘要算法的空 transcript 摘要。 */
bool __xrtTls13EmptyHash(
	xcryptohash Hash,
	void* pDigest,
	size_t iDigestSize
);



/* 从可选 PSK、共享秘密和 ServerHello transcript 派生握手秘密。 */
bool __xrtTls13HandshakeSchedule(
	xcryptohash Hash,
	xbytesview Psk,
	xbytesview Shared,
	xbytesview TranscriptHash,
	void* pHandshake,
	void* pClientTraffic,
	void* pServerTraffic,
	size_t iSecretSize
);



/* 从握手秘密和服务端 Finished transcript 派生主秘密及应用秘密。 */
bool __xrtTls13ApplicationSchedule(
	xcryptohash Hash,
	xbytesview Handshake,
	xbytesview TranscriptHash,
	void* pMaster,
	void* pClientTraffic,
	void* pServerTraffic,
	size_t iSecretSize
);



/* 计算 TLS 1.3 外部会话票据的 resumption binder。 */
bool __xrtTls13ResumptionBinder(
	xcryptohash Hash,
	xbytesview Psk,
	xbytesview ClientHelloPartial,
	void* pOutput,
	size_t iOutputSize
);



/* 计算 TLS 1.3 Finished.verify_data。 */
bool __xrtTls13Finished(
	xcryptohash Hash,
	xbytesview FinishedKey,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
);



/* 流式执行 TLS 1.2 P_hash，不拼接 label 与 seed 临时副本。 */
bool __xrtTls12Prf(
	xcryptohash Hash,
	xbytesview Secret,
	xstrview Label,
	xbytesview Seed,
	void* pOutput,
	size_t iOutputSize
);



#define XTLS12_MASTER_SECRET_SIZE 48u
#define XTLS12_FINISHED_SIZE 12u
#define XTLS12_RANDOM_SIZE 32u
#define XTLS12_KEY_MAX_SIZE 32u
#define XTLS12_IV_MAX_SIZE 12u



/* TLS 1.2 完整握手只在临时对象中保存主密钥和双向记录材料。 */
typedef struct xtls12keymaterial {
	uint8 Master[XTLS12_MASTER_SECRET_SIZE];
	uint8 ClientKey[XTLS12_KEY_MAX_SIZE];
	uint8 ServerKey[XTLS12_KEY_MAX_SIZE];
	uint8 ClientIv[XTLS12_IV_MAX_SIZE];
	uint8 ServerIv[XTLS12_IV_MAX_SIZE];
	size_t KeySize;
	size_t IvSize;
	size_t HashSize;
	xcryptohash Hash;
} xtls12keymaterial;



/* 使用扩展主密钥派生 TLS 1.2 主密钥和双向 AEAD 记录材料。 */
bool __xrtTls12KeyMaterial(
	xtlscipher Cipher,
	xbytesview Shared,
	xbytesview SessionHash,
	xbytesview ClientRandom,
	xbytesview ServerRandom,
	xtls12keymaterial* pMaterial
);



/* 从当前 transcript 摘要计算固定 12 字节 TLS 1.2 Finished。 */
bool __xrtTls12Finished(
	xcryptohash Hash,
	xbytesview Master,
	bool bServer,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
);



#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)

/* SHA-256 调度后端由公共层验证参数后调用。 */
void __xrtTlsScheduleSha256Init(xtlstranscript* pTranscript);
bool __xrtTlsScheduleSha256Update(
	xtlstranscript* pTranscript,
	xbytesview Message
);
bool __xrtTlsScheduleSha256Digest(
	const xtlstranscript* pTranscript,
	void* pDigest
);
bool __xrtTlsScheduleSha256Extract(
	xbytesview Salt,
	xbytesview Ikm,
	void* pSecret
);
bool __xrtTlsScheduleSha256Expand(
	xbytesview Secret,
	xbytesview Info,
	void* pOutput,
	size_t iOutputSize
);

#endif



#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)

/* SHA-384 调度后端由公共层验证参数后调用。 */
void __xrtTlsScheduleSha384Init(xtlstranscript* pTranscript);
bool __xrtTlsScheduleSha384Update(
	xtlstranscript* pTranscript,
	xbytesview Message
);
bool __xrtTlsScheduleSha384Digest(
	const xtlstranscript* pTranscript,
	void* pDigest
);
bool __xrtTlsScheduleSha384Extract(
	xbytesview Salt,
	xbytesview Ikm,
	void* pSecret
);
bool __xrtTlsScheduleSha384Expand(
	xbytesview Secret,
	xbytesview Info,
	void* pOutput,
	size_t iOutputSize
);

#endif

#endif



#if defined(XRT_FEATURE_TLS_RECORD)

/* 写入网络字节序 64 位整数。 */
void __xrtTlsWrite64(uint8* pData, uint64 iValue);



/* 以 TLS 规定的方式把序列号异或进 12 字节静态 IV。 */
void __xrtTlsRecordNonce(uint8* pNonce, const uint8* pIv, uint64 iSequence);

/* 一个 record-key 只管理单向密钥、静态 IV 和该方向的序列号。 */
typedef struct xtlsrecordkey {
	xtlsversion Version;
	xtlscipher Cipher;
	xtlsaead Aead;
	uint8 Iv[12];
	uint64 Sequence;
	uint8 IvSize;
	uint8 ExplicitNonceSize;
	uint8 TagSize;
	bool Ready;
	#if defined(XRT_FEATURE_TLS_RECORD_AES)
		xaesgcm Aes;
	#endif
	#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)
		uint8 ChaChaKey[32];
	#endif
} xtlsrecordkey;



/* 初始化单向记录密钥；成功后序列号从零开始。 */
bool __xrtTlsRecordKeyInit(
	xtlsrecordkey* pKey,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview Key,
	xbytesview Iv
);



/* 清除记录密钥、IV 和序列号。 */
void __xrtTlsRecordKeyClear(xtlsrecordkey* pKey);



/* 返回当前构建中是否支持指定版本与密码套件。 */
bool __xrtTlsRecordCipherSupported(xtlsversion Version, xtlscipher Cipher);



/* 返回单组记录密钥允许处理的记录数量。 */
uint64 __xrtTlsRecordKeyLimit(const xtlsrecordkey* pKey);



/* 返回记录密钥是否必须更新或关闭。 */
bool __xrtTlsRecordKeyExhausted(const xtlsrecordkey* pKey);



/* 返回保护一条明文记录所需的完整线路长度。 */
size_t __xrtTlsRecordSealSize(
	const xtlsrecordkey* pKey,
	size_t iPlainSize,
	size_t iPadding
);



/* 保护一条记录并在成功后递增发送序列号。 */
bool __xrtTlsRecordSeal(
	xtlsrecordkey* pKey,
	xtlsrecordtype Type,
	xbytesview Plain,
	size_t iPadding,
	void* pOutput,
	size_t iOutputSize,
	size_t* pWritten
);



/* 打开一条完整记录并在成功后递增接收序列号。 */
bool __xrtTlsRecordOpen(
	xtlsrecordkey* pKey,
	const xtlsrecord* pRecord,
	void* pOutput,
	size_t iOutputSize,
	xtlsrecordtype* pType,
	size_t* pWritten
);



#if defined(XRT_FEATURE_TLS_RECORD_AES)

/* 初始化、清除并调用 AES-GCM 记录后端。 */
bool __xrtTlsRecordAesInit(
	xtlsrecordkey* pKey,
	const void* pData,
	size_t iSize
);
void __xrtTlsRecordAesClear(xtlsrecordkey* pKey);
bool __xrtTlsRecordAesSeal(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
);
bool __xrtTlsRecordAesOpen(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
);

#endif



#if defined(XRT_FEATURE_TLS_RECORD_CHACHA)

/* 初始化、清除并调用 ChaCha20-Poly1305 记录后端。 */
bool __xrtTlsRecordChaChaInit(
	xtlsrecordkey* pKey,
	const void* pData,
	size_t iSize
);
void __xrtTlsRecordChaChaClear(xtlsrecordkey* pKey);
bool __xrtTlsRecordChaChaSeal(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
);
bool __xrtTlsRecordChaChaOpen(
	const xtlsrecordkey* pKey,
	const uint8* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
);

#endif

#endif

#endif

#endif
