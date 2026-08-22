#ifndef XRT_TLS_VERIFY_H
#define XRT_TLS_VERIFY_H

#include <xrt/tls.h>
#include <xrt/x509.h>



#if defined(XRT_FEATURE_TLS_VERIFY) && \
	(!defined(XRT_FEATURE_TLS_NEGOTIATE) || \
	 !defined(XRT_FEATURE_X509_STORE) || \
	 !defined(XRT_FEATURE_X509_IDENTITY) || \
	 !defined(XRT_FEATURE_X509_VERIFY) || \
	 !defined(XRT_FEATURE_ATOMIC) || \
	 !defined(XRT_FEATURE_CRYPTO_CORE))
	#error "XRT_FEATURE_TLS_VERIFY requires TLS negotiation, complete X.509 verification and atomic references"
#endif



#if defined(XRT_FEATURE_TLS_VERIFY)

typedef struct xtlsverifier xtlsverifier;



/* 自定义验证过程可以接管信任决策，也可以回退到不可变信任库。 */
typedef enum xtlsverifydecision {
	XTLS_VERIFY_ERROR = -1,
	XTLS_VERIFY_DEFAULT = 0,
	XTLS_VERIFY_ACCEPT,
	XTLS_VERIFY_REJECT
} xtlsverifydecision;



/* 对端证书视图仅在验证调用期间有效，证书按叶到根的线路顺序排列。 */
typedef struct xtlspeer {
	xtlsrole Role;
	xstrview Name;
	xtime Time;
	const xx509cert* Certificates;
	size_t CertificateCount;
} xtlspeer;



/* 已验证路径按叶到根排列但不包含独立 trust anchor，全部视图只在策略回调期间有效。 */
typedef struct xtlsverifiedpeer {
	const xtlspeer* Peer;
	const xx509cert* const* Path;
	size_t PathCount;
	const xx509anchor* Anchor;
} xtlsverifiedpeer;



/* 自定义验证过程必须允许并发调用，不负责 TLS 握手签名验证。 */
typedef xtlsverifydecision (*xtlsverifyproc)(
	const xtlspeer* pPeer,
	ptr pContext
);



/* 默认链和身份验证成功后执行附加策略；返回 false 时可以设置结构化原因。 */
typedef bool (*xtlsverifypolicyproc)(
	const xtlsverifiedpeer* pPeer,
	ptr pContext
);



/* 自定义时间源用于确定性测试、重放验证和受控时钟环境。 */
typedef xtime (*xtlsverifytimeproc)(ptr pContext);



/* 最后一个验证器引用释放时清理调用方上下文。 */
typedef void (*xtlsverifyreleaseproc)(ptr pContext);



/* 创建时深复制可选信任库并借用回调；成功后接管上下文。 */
typedef struct xtlsverifierconfig {
	const xx509store* Store;
	xtlsverifyproc Verify;
	xtlsverifypolicyproc Policy;
	xtlsverifytimeproc Time;
	xtlsverifyreleaseproc Release;
	ptr Context;
} xtlsverifierconfig;



XRT_EXTERN_C_BEGIN



/* 初始化尚未绑定信任库、回调或自定义时钟的验证器配置。 */
XRT_API void xrtTlsVerifierConfigInit(xtlsverifierconfig* pConfig);



/* 创建可跨线程共享的验证器；成功后接管自定义上下文。 */
XRT_API xtlsverifier* xrtTlsVerifierCreate(
	const xtlsverifierconfig* pConfig
);



/* 增加不可变验证器引用。 */
XRT_API xtlsverifier* xrtTlsVerifierRetain(
	const xtlsverifier* pVerifier
);



/* 释放验证器引用；空指针无操作。 */
XRT_API void xrtTlsVerifierRelease(xtlsverifier* pVerifier);



/* 使用显式时间、证书和借用信任库执行默认路径与身份验证。 */
XRT_API bool xrtTlsPeerVerify(
	const xtlspeer* pPeer,
	const xx509store* pStore,
	xtlsverifypolicyproc pPolicy,
	ptr pContext
);



/* 执行自定义或默认信任决策；证书和名称只在调用期间借用。 */
XRT_API bool xrtTlsVerifierVerify(
	const xtlsverifier* pVerifier,
	xtlsrole Role,
	xstrview Name,
	const xx509cert* pCertificates,
	size_t iCertificateCount
);



/* 验证 TLS 1.2 ECDHE ServerKeyExchange；签名覆盖双方随机数和原始参数。 */
XRT_API bool xrtTls12ServerKeyExchangeVerify(
	xtlssignature Scheme,
	xbytesview ClientRandom,
	xbytesview ServerRandom,
	xbytesview Parameters,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
);



/* 验证 TLS 1.3 对端 CertificateVerify；信任回调不能绕过此步骤。 */
XRT_API bool xrtTls13CertificateVerifySignature(
	xtlsrole Signer,
	xtlssignature Scheme,
	xbytesview TranscriptHash,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
);



XRT_EXTERN_C_END

#endif

#endif
