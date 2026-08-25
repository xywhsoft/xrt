#ifndef XRT_INTERNAL_TLS_SERVER_H
#define XRT_INTERNAL_TLS_SERVER_H

#include <xrt/tls_server.h>
#include <xrt/temp.h>
#include "xrt_tls_session.h"



#if defined(XRT_FEATURE_TLS_SERVER)

#define XTLS_SERVER_SECRET_MAX_SIZE 48u



#if defined(XRT_FEATURE_TLS_SERVER_RESUME)

/* 服务端只复制恢复握手所需的 PSK，不持有票据查找回调返回的对象。 */
typedef struct xtlsserverresumeoffer {
	uint8 Secret[XTLS_SERVER_SECRET_MAX_SIZE];
	uint16 Selected;
	bool Resumed;
} xtlsserverresumeoffer;

#endif



/* 服务端内部步骤不进入公开 ABI。 */
typedef enum xtlsserverstep {
	XTLS_SERVER_WAIT_CLIENT_HELLO = 1,
	XTLS_SERVER_WAIT_CLIENT_KEY_EXCHANGE,
	XTLS_SERVER_WAIT_CHANGE_CIPHER_SPEC,
	XTLS_SERVER_WAIT_CLIENT_FINISHED,
	XTLS_SERVER_READY
} xtlsserverstep;



/* 首航选择结果借用 ClientHello，并独立持有最终身份引用。 */
typedef struct xtlsserverselection {
	xtlsclienthello Hello;
	xtlskeyshare Share;
	xbytesview ServerName;
	xbytesview Protocols;
	const xtlsidentity* Identity;
	size_t Protocol;
	xtlsversion Version;
	xtlscipher Cipher;
	xtlssignature Signature;
	uint16 Group;
	bool Retry;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		xtlsserverresumeoffer Resume;
	#endif
} xtlsserverselection;



/* 服务端配置快照和流量秘密位于会话尾部，SNI 只在收到后精确分配。 */
typedef struct xtlsserverstate {
	xtlshandshakereader Reader;
	xtlstranscript Transcript;
	xtemparena HandshakeArena;
	xbytesview* Protocols;
	size_t ProtocolCount;
	xbytesview ServerName;
	bytes ServerNameStorage;
	bytes RetryClientHello;
	size_t RetryClientHelloSize;
	bytes ClientHandshakeTraffic;
	bytes ClientApplicationTraffic;
	bytes ServerApplicationTraffic;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		bytes MasterSecret;
		bytes ResumptionMaster;
	#endif
	size_t SecretCapacity;
	size_t HashSize;
	size_t RecordOffset;
	const xtlsidentity* Identity;
	xtlsserverselectproc Select;
	ptr SelectContext;
	xtlsrecordkey PendingReadKey;
	xtlsrecordkey PendingWriteKey;
	uint8 ClientRandom[XTLS12_RANDOM_SIZE];
	uint8 ServerRandom[XTLS12_RANDOM_SIZE];
	uint8 PrivateKey[56u];
	size_t PrivateKeySize;
	uint8 Master[XTLS12_MASTER_SECRET_SIZE];
	xtlsversion Version;
	xtlscipher Cipher;
	xtlssignature Signature;
	xtlscipher RetryCipher;
	uint16 Group;
	uint16 RetryGroup;
	xtlsserverstep Step;
	bool RequireProtocol;
	bool RetrySeen;
	bool CompatibilityCcsSeen;
	#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
		xtlsserverresumeproc Resume;
		ptr ResumeContext;
		uint32 ResumeAgeTolerance;
		bool ResumptionReady;
		bool Resumed;
	#endif
} xtlsserverstate;



/* 设置服务端角色错误并返回 false，供构造和驱动文件共享。 */
bool __xrtTlsServerError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
);



/* 包装服务端角色底层失败并保留原因链。 */
bool __xrtTlsServerCause(cstr sOperation, cstr sMessage);



/* 根据服务端当前队列和输入需求发布等待方向。 */
bool __xrtTlsServerWait(xtlssession* pSession, bool bInput);



/* 设置服务端协议错误并进入失败终态。 */
xtlsresult __xrtTlsServerProtocol(
	xtlssession* pSession,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
);



/* 处理第一条 ClientHello 并原子提交完整服务端航次。 */
xtlsresult __xrtTlsServerFirstFlight(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
);



/* 验证客户端 Finished 并切换到双向应用 epoch。 */
xtlsresult __xrtTlsServerFinished(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
);



/* 从共享选择结果构造并提交完整 TLS 1.2 服务端首航。 */
xtlsresult __xrtTlsServer12FirstFlight(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage,
	xtlsserverselection* pSelection
);



/* 处理 TLS 1.2 ClientKeyExchange 或客户端 Finished。 */
xtlsresult __xrtTlsServer12Handshake(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlshandshake* pMessage
);



/* 验证 TLS 1.2 CCS 并启用客户端写入方向对应的接收 epoch。 */
xtlsresult __xrtTlsServer12ChangeCipherSpec(
	xtlssession* pSession,
	xtlsserverstate* pState,
	const xtlssessionrecord* pRecord
);



/* 在消费客户端 Finished 前检查服务端 CCS 与 Finished 能否原子排队。 */
xtlsresult __xrtTlsServer12FinishedWritable(
	xtlssession* pSession,
	const xtlsserverstate* pState
);



#if defined(XRT_FEATURE_TLS_SERVER_RESUME)

/* 解析全部外部 PSK，执行票据查找、路由绑定、年龄和 binder 校验。 */
bool __xrtTlsServerResumeSelect(
	const xtlshandshake* pMessage,
	const xtlsclienthello* pHello,
	const xtlsserverstate* pState,
	xbytesview ServerName,
	xbytesview Protocols,
	xtlscipher Cipher,
	size_t iProtocol,
	xtlsserverresumeoffer* pOffer
);

#endif

#endif

#endif
