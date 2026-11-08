#ifndef XRT_TLS_SERVER_H
#define XRT_TLS_SERVER_H

#include <xrt/tls_session.h>
#include <xrt/tls_identity.h>
#include <xrt/temp.h>

#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
	#include <xrt/tls_resume.h>
#endif



/* 关闭会话恢复实现时仍保留公开配置使用的不透明类型。 */
#if !defined(XRT_FEATURE_TLS_SERVER_RESUME)
typedef struct xtlsresume xtlsresume;
#endif



#if defined(XRT_FEATURE_TLS_SERVER) && \
	(!defined(XRT_FEATURE_TLS_SESSION) || \
	 !defined(XRT_FEATURE_TLS_HELLO_WRITE) || \
	 !defined(XRT_FEATURE_TLS_HANDSHAKE_READER) || \
	 !defined(XRT_FEATURE_TLS_MESSAGES_WRITE) || \
	 !defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE) || \
	 !defined(XRT_FEATURE_TLS_SCHEDULE) || \
	 !defined(XRT_FEATURE_TLS_KEY_EXCHANGE) || \
	 !defined(XRT_FEATURE_TLS_IDENTITY) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE) || \
	 !defined(XRT_FEATURE_TEMP_MEMORY))
	#error "XRT_FEATURE_TLS_SERVER requires TLS session, Hello writer," \
		"handshake reader, message writers, schedule, key exchange," \
		"identity, secure random and temporary memory"
#endif



#if defined(XRT_FEATURE_TLS_SERVER_RESUME) && \
	(!defined(XRT_FEATURE_TLS_SERVER) || \
	 !defined(XRT_FEATURE_TLS_RESUME) || \
	 !defined(XRT_FEATURE_TLS_PSK) || \
	 !defined(XRT_FEATURE_TLS_PSK_WRITE))
	#error "XRT_FEATURE_TLS_SERVER_RESUME requires TLS server, resume and PSK"
#endif



#if defined(XRT_FEATURE_TLS_SERVER)

/* SIZE_MAX 表示不协商 ALPN；其他值是服务器配置协议数组的下标。 */
#define XTLS_SERVER_PROTOCOL_NONE SIZE_MAX



/* 选择请求中的视图只在回调期间借用，Protocols 是完整 ALPN 扩展负载。 */
typedef struct xtlsserverrequest {
	xbytesview ServerName;
	xbytesview Protocols;
} xtlsserverrequest;



/* 选择结果默认带入静态身份、ALPN 和零 Cookie，回调可替换这些结果。 */
typedef struct xtlsserverchoice {
	const xtlsidentity* Identity;
	size_t Protocol;
	uint64 Cookie;
	/* 框架私有的回调期身份引用，只由 SetIdentityV1 和框架管理。
	 * 不可直接修改、复制或在回调返回后保存本结构。 */
	xtlsidentity* IdentityOwnerV1;
} xtlsserverchoice;



/* 同步选择器用于 SNI、多身份和租户路由；返回 false 会拒绝握手。 */
typedef bool (*xtlsserverselectproc)(
	ptr pContext,
	const xtlsserverrequest* pRequest,
	xtlsserverchoice* pChoice
);



#define XTLS_SERVER_RESUME_AGE_TOLERANCE_DEFAULT 10000u
#define XTLS_SERVER_TICKET_LIFETIME_DEFAULT 86400u
#define XTLS_SERVER_TICKET_SIZE_DEFAULT 32u



/* 票据查找请求中的全部视图仅在回调期间借用。 */
typedef struct xtlsserverresumerequest {
	xbytesview ServerName;
	xbytesview Protocols;
	xbytesview Ticket;
	uint32 Age;
} xtlsserverresumerequest;



/* 返回借用恢复对象；服务器会在回调返回后立即 retain，再读取其不可变快照。 */
typedef const xtlsresume* (*xtlsserverresumeproc)(
	ptr pContext,
	const xtlsserverresumerequest* pRequest
);



/* 拥有式查找：调用前 *ppResume 为 NULL；true + NULL 表示缓存未命中，
 * false 表示查找失败（诊断可为 NULL）。写入的独立引用在所有返回路径上
 * 都转交框架，框架使用后或失败时恰好 release 一次。回调不得再释放它。
 * 允许临时语言结果；不需要回调返回后的借用存活窗口。 */
typedef bool (*xtlsserverresumeownedprocV1)(
	ptr pContext,
	const xtlsserverresumerequest* pRequest,
	xtlsresume** ppResume
);



/* 创建期间借用配置；会话持有身份、深复制协议，两个回调上下文借用到首航结束。 */
typedef struct xtlsserverconfig {
	const xtlscontext* Context;
	const xtlsidentity* Identity;
	const xstrview* Protocols;
	size_t ProtocolCount;
	xtlsserverselectproc Select;
	ptr SelectContext;
	bool RequireProtocol;
	xtlsserverresumeproc Resume;
	ptr ResumeContext;
	uint32 ResumeAgeTolerance;
	/* 与 Resume 互斥，共用 ResumeContext；结构布局变化需重编译 C 单元。 */
	xtlsserverresumeownedprocV1 ResumeOwnedV1;
} xtlsserverconfig;



XRT_EXTERN_C_BEGIN



/* 初始化默认上下文、无身份、无 ALPN 和无动态选择器的服务端配置。 */
XRT_API void xrtTlsServerConfigInit(xtlsserverconfig* pConfig);

/* 仅在 Select 回调中使用：独立 retain 身份，允许调用方随后立即释放
 * 自己的引用。替换时释放前次设置的引用；回调成功或失败后，框架统一
 * 释放最后的临时引用。失败保留原选择（旧身份析构报错时替换已提交）。
 * 直接赋 Identity 仍是原有借用合同，不隐式转移引用。 */
XRT_API bool xrtTlsServerChoiceSetIdentityV1(
	xtlsserverchoice* pChoice,
	const xtlsidentity* pIdentity
);



/* 创建等待 ClientHello 的服务端会话；必须提供静态身份或选择器。 */
XRT_API xtlssession* xrtTlsServerCreate(
	const xtlsserverconfig* pConfig,
	xnetbufpool* pPool
);



/* 在公平性预算内消费输入并推进服务端握手和后握手状态。 */
XRT_API xtlsresult xrtTlsServerDrive(xtlssession* pSession);



/* 用当前 TLS 1.3 写 epoch 排队 KeyUpdate；TLS 1.2 会话返回不支持。 */
XRT_API xtlsresult xrtTlsServerKeyUpdate(
	xtlssession* pSession,
	xtlskeyupdate Request
);



/* 借用服务端从 ClientHello 深复制的 SNI；尚未收到名称时返回 false。 */
XRT_API bool xrtTlsServerName(
	const xtlssession* pSession,
	xbytesview* pServerName
);



/* 返回选择器为本次握手保存的不透明宿主 Cookie；未设置时返回零。 */
XRT_API bool xrtTlsServerCookie(
	const xtlssession* pSession,
	uint64* pCookie
);



/* 返回本次连接是否接受了客户端提供的 TLS 1.3 会话票据。
 * 这是基础角色查询；未编译恢复能力时，合法服务端会话返回 false。 */
XRT_API bool xrtTlsServerResumed(const xtlssession* pSession);



#if defined(XRT_FEATURE_TLS_SERVER_RESUME)

/* 用调用方票据签发 TLS 1.3 NewSessionTicket；TLS 1.2 会话返回不支持。 */
XRT_API xtlsresult xrtTlsServerTicket(
	xtlssession* pSession,
	xbytesview Ticket,
	uint32 iLifetime,
	xtlsresume** ppResume
);



/* 使用默认随机票据和有效期完成一次 TLS 1.3 签发。 */
XRT_API xtlsresult xrtTlsServerTicketNew(
	xtlssession* pSession,
	xtlsresume** ppResume
);

#endif



XRT_EXTERN_C_END

#endif

#endif
