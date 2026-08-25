#ifndef XRT_INTERNAL_TLS_CLIENT_H
#define XRT_INTERNAL_TLS_CLIENT_H

#include "xrt_tls_session.h"



#if defined(XRT_FEATURE_TLS_CLIENT)

#define XTLS_CLIENT_SECRET_MAX_SIZE 48u

/* 客户端内部步骤不进入公开 ABI。 */
typedef enum xtlsclientstep {
	XTLS_CLIENT_WAIT_SERVER_HELLO = 1,
	XTLS_CLIENT_WAIT_ENCRYPTED_EXTENSIONS,
	XTLS_CLIENT_WAIT_CERTIFICATE,
	XTLS_CLIENT_WAIT_CERTIFICATE_VERIFY,
	XTLS_CLIENT_WAIT_FINISHED,
	XTLS_CLIENT_WAIT_SERVER_KEY_EXCHANGE,
	XTLS_CLIENT_WAIT_SERVER_HELLO_DONE,
	XTLS_CLIENT_WAIT_CHANGE_CIPHER_SPEC,
	XTLS_CLIENT_READY
} xtlsclientstep;



#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)

/* 已验证对端证书只保留一份精确分配和指向该分配的解析视图。 */
typedef struct xtlsclientpeer {
	xx509cert* Certificates;
	size_t CertificateCount;
	xx509pubkey PublicKey;
} xtlsclientpeer;

#endif



/* 客户端首航状态与配置快照都位于会话尾部的一次精确分配中。 */
typedef struct xtlsclientstate {
	xtlshandshakereader Reader;
	xtlstranscript Transcript;
	xbytesview ServerName;
	xbytesview SniName;
	xbytesview* Protocols;
	size_t ProtocolCount;
	uint16* Versions;
	size_t VersionCount;
	uint16* Ciphers;
	size_t CipherCount;
	uint16* Groups;
	size_t GroupCount;
	uint16* Signatures;
	size_t SignatureCount;
	bytes PrivateKey;
	size_t PrivateKeySize;
	size_t PrivateKeyCapacity;
	bytes PublicKey;
	size_t PublicKeySize;
	size_t PublicKeyCapacity;
	bytes Workspace;
	size_t WorkspaceSize;
	bytes ClientHello;
	size_t ClientHelloSize;
	bytes RetryStorage;
	bytes HandshakeSecret;
	bytes ClientHandshakeTraffic;
	bytes ServerHandshakeTraffic;
	bytes ClientApplicationTraffic;
	bytes ServerApplicationTraffic;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		const xtlsresume* OfferResume;
		bytes ResumptionMaster;
		xtlsresume** Resumes;
		size_t ResumeLimit;
		size_t ResumeHead;
		size_t ResumeCount;
		uint64 ResumeDropped;
		uint64 ResumePublished;
		uint32 ResumeAge;
		uint8 PeerIdentity[32];
		bool ResumptionReady;
		bool Resumed;
	#endif
	size_t SecretCapacity;
	size_t HashSize;
	size_t RecordOffset;
	uint8 Random[XTLS_RANDOM_SIZE];
	uint8 ServerRandom[XTLS_RANDOM_SIZE];
	uint8 Shared[56u];
	size_t SharedSize;
	uint8 Master[XTLS12_MASTER_SECRET_SIZE];
	xtlsrecordkey PendingReadKey;
	uint8 SessionId[XTLS_SESSION_ID_MAX];
	uint16 Group;
	xtlscipher Cipher;
	xtlsversion Version;
	xtlsclientstep Step;
	bool Offer12;
	bool Offer13;
	bool ExtendedMasterSecret;
	bool ResumeOnly;
	bool RetrySeen;
	bool CompatibilityCcsSeen;
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		xtlsverifier* Verifier;
		xtlsclientpeer* Peer;
	#endif
} xtlsclientstate;



/* 设置客户端角色错误并返回 false，供构造和驱动文件共享。 */
bool __xrtTlsClientError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
);



/* 包装客户端角色底层失败并保留原因链。 */
bool __xrtTlsClientCause(cstr sOperation, cstr sMessage);



/* 设置客户端协议错误并进入失败终态。 */
xtlsresult __xrtTlsClientProtocol(
	xtlssession* pSession,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
);



/* 查询客户端是否实际发送了一个 16 位线路标识。 */
bool __xrtTlsClientOffered(
	const uint16* pValues,
	size_t iCount,
	uint16 iValue
);



/* 根据客户端当前队列和输入需求发布等待方向。 */
bool __xrtTlsClientWait(xtlssession* pSession, bool bInput);



/* 为首航或 HRR 重试构建并排队当前 ClientHello。 */
bool __xrtTlsClientHelloQueue(
	xtlssession* pSession,
	xtlsclientstate* pState,
	xbytesview Cookie,
	bool bRetry
);



/* 在客户端实际发送的 ALPN 列表中查找并发布稳定协议视图。 */
bool __xrtTlsClientProtocolSelect(
	const xtlsclientstate* pState,
	xbytesview Selected,
	xbytesview* pProtocol,
	cstr sOperation
);



#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)

/* 验证并提交当前协商版本的服务端证书链。 */
xtlsresult __xrtTlsClientCertificateCommit(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
);



/* 处理 TLS 1.2 ServerHello 并切换到明文服务端认证航班。 */
xtlsresult __xrtTlsClient12ServerHello(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage,
	const xtlsserverhello* pHello
);



/* 处理 TLS 1.2 证书之后的角色握手消息。 */
xtlsresult __xrtTlsClient12Handshake(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlshandshake* pMessage
);



/* 验证 TLS 1.2 CCS 并原子启用服务端写入的接收 epoch。 */
xtlsresult __xrtTlsClient12ChangeCipherSpec(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlssessionrecord* pRecord
);



/* 在消费 ServerHelloDone 前检查完整客户端 TLS 1.2 航班能否原子排队。 */
xtlsresult __xrtTlsClient12FlightWritable(
	xtlssession* pSession,
	const xtlsclientstate* pState
);

#endif



#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)

/* 发布一张已严格解析的 TLS 1.3 NewSessionTicket。 */
bool __xrtTlsClientResumePublish(
	xtlssession* pSession,
	xtlsclientstate* pState,
	const xtlssessionticket* pTicket
);



/* 返回成功进入客户端恢复队列的累计票据序号。 */
uint64 __xrtTlsClientResumePublished(
	const xtlssession* pSession
);



/* 为已经完整编码且末尾预留 binder 的 ClientHello 写入真实 binder。 */
bool __xrtTlsClientResumeBinder(
	xtlsclientstate* pState,
	const xtlstranscript* pPrefix
);



/* 释放客户端仍持有的全部恢复对象。 */
void __xrtTlsClientResumeClear(xtlsclientstate* pState);

#endif

#endif

#endif
