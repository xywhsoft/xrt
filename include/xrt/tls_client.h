#ifndef XRT_TLS_CLIENT_H
#define XRT_TLS_CLIENT_H

#include <xrt/tls_session.h>

#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
	#include <xrt/tls_verify.h>
#endif

#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
	#include <xrt/tls_resume.h>
#endif



/*
	公开配置的布局不能随裁剪宏变化；关闭对应实现时只保留不透明类型声明。
	这样独立编译的扩展库可以安全调用功能更完整的 XRT 运行库。
*/
#if !defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
typedef struct xtlsverifier xtlsverifier;
#endif

#if !defined(XRT_FEATURE_TLS_CLIENT_RESUME)
typedef struct xtlsresume xtlsresume;
#endif



#if defined(XRT_FEATURE_TLS_CLIENT) && \
	(!defined(XRT_FEATURE_TLS_SESSION) || \
	 !defined(XRT_FEATURE_TLS_HELLO_WRITE) || \
	 !defined(XRT_FEATURE_TLS_HANDSHAKE_READER) || \
	 !defined(XRT_FEATURE_TLS_MESSAGES) || \
	 !defined(XRT_FEATURE_TLS_SCHEDULE) || \
	 !defined(XRT_FEATURE_TLS_KEY_EXCHANGE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT_FEATURE_TLS_CLIENT requires TLS session, Hello writer," \
		"handshake reader, messages, schedule, key exchange and secure random"
#endif

#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
	(!defined(XRT_FEATURE_TLS_CLIENT) || \
	 !defined(XRT_FEATURE_TLS_VERIFY) || \
	 !defined(XRT_FEATURE_TLS_AUTH_MESSAGES) || \
	 !defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE) || \
	 !defined(XRT_FEATURE_X509_VERIFY_RSA) || \
	 !defined(XRT_FEATURE_X509_VERIFY_ECDSA) || \
	 !defined(XRT_FEATURE_X509_VERIFY_ED25519))
	#error "XRT_FEATURE_TLS_CLIENT_VERIFY requires TLS client, verifier," \
		" authentication messages and RSA, ECDSA and Ed25519 verification"
#endif

#if defined(XRT_FEATURE_TLS_CLIENT_RESUME) && \
	(!defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	 !defined(XRT_FEATURE_TLS_RESUME) || \
	 !defined(XRT_FEATURE_TLS_PSK_WRITE) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256))
	#error "XRT_FEATURE_TLS_CLIENT_RESUME requires verified client, resume, PSK writer and SHA-256"
#endif



#if defined(XRT_FEATURE_TLS_CLIENT)

/* 客户端默认保留四张票据，兼顾并行恢复与每连接常驻内存。 */
#define XTLS_CLIENT_RESUME_LIMIT_DEFAULT 4u

/* 显式队列上限避免不可信服务端用连续票据放大内存占用。 */
#define XTLS_CLIENT_RESUME_LIMIT_MAX 64u

/*
	配置在创建期间借用全部对象和视图；成功会话持有共享对象并深拷贝视图。
	ServerName 只用于线路 SNI，VerifyName 用于证书身份验证和恢复票据绑定。
	VerifyName 为空时继承 ServerName，允许 DNS 名称保持一字段常用写法；
	连接 IP 字面量时应只设置 VerifyName，避免发送协议不允许的 IP SNI。
*/
typedef struct xtlsclientconfig {
	const xtlscontext* Context;
	xstrview ServerName;
	xstrview VerifyName;
	const xstrview* Protocols;
	size_t ProtocolCount;
	const xtlsverifier* Verifier;
	const xtlsresume* Resume;
	size_t ResumeLimit;
	/* 必须接受所给票据；禁止回退完整证书握手。 */
	bool ResumeOnly;
} xtlsclientconfig;



XRT_EXTERN_C_BEGIN



/* 初始化使用默认共享策略、无 SNI 和无 ALPN 的客户端配置。 */
XRT_API void xrtTlsClientConfigInit(xtlsclientconfig* pConfig);



/* 创建客户端会话；默认要求 Verifier，无验证器时必须启用 ResumeOnly。 */
XRT_API xtlssession* xrtTlsClientCreate(
	const xtlsclientconfig* pConfig,
	xnetbufpool* pPool
);



/* 在公平性预算内消费已喂入记录并推进客户端握手状态。 */
XRT_API xtlsresult xrtTlsClientDrive(xtlssession* pSession);



#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)

/* 返回完整握手中已经深复制并验证的对端证书数量。 */
XRT_API size_t xrtTlsClientCertificateCount(
	const xtlssession* pSession
);



/* 借用一张已解析对端证书，视图稳定到客户端会话销毁。 */
XRT_API const xx509cert* xrtTlsClientCertificate(
	const xtlssession* pSession,
	size_t iIndex
);

#endif



/* 用当前 TLS 1.3 写 epoch 排队 KeyUpdate；TLS 1.2 会话返回不支持。 */
XRT_API xtlsresult xrtTlsClientKeyUpdate(
	xtlssession* pSession,
	xtlskeyupdate Request
);



#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)

/* 返回服务端是否接受了本次创建时提供的恢复对象。 */
XRT_API bool xrtTlsClientResumed(const xtlssession* pSession);



/* 返回等待调用方接管的恢复对象数量。 */
XRT_API size_t xrtTlsClientResumeCount(const xtlssession* pSession);



/* 返回因队列关闭、容量淘汰或可选缓存 OOM 而未保留的有效票据总数。 */
XRT_API uint64 xrtTlsClientResumeDropped(const xtlssession* pSession);



/* 从队首取出一张恢复票据并把唯一会话引用转移给调用方。 */
XRT_API xtlsresume* xrtTlsClientTakeResume(xtlssession* pSession);

#endif



XRT_EXTERN_C_END

#endif

#endif
