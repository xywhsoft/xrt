#ifndef XRT_IMAP_AUTH_H
#define XRT_IMAP_AUTH_H

#include <xrt/codec.h>
#include <xrt/imap_client.h>



#if defined(XIMAP_FEATURE_IMAP_AUTH) && \
	(!defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XIMAP_FEATURE_IMAP_AUTH requires IMAP client and Base64"
#endif



#if defined(XIMAP_FEATURE_IMAP_AUTH)

/* LOGIN 保留传统命令，其余机制使用标准 SASL continuation 交换。 */
typedef enum ximapauthmethod {
	XIMAP_AUTH_LOGIN = 0,
	XIMAP_AUTH_PLAIN,
	XIMAP_AUTH_XOAUTH2,
	XIMAP_AUTH_OAUTHBEARER
} ximapauthmethod;



/* 凭据只在 Auth 调用期间借用，结束前所有临时副本都会被清零。 */
typedef struct ximapauthconfig {
	ximapauthmethod Method;
	xstrview Username;
	xstrview Secret;
	xstrview AuthorizationId;
	bool InitialResponse;
	bool AllowPlaintext;
} ximapauthconfig;



XRT_EXTERN_C_BEGIN



/* 初始化 PLAIN、SASL-IR 开启和明文凭据关闭的安全默认值。 */
XRT_API void xrtImapAuthConfigInit(ximapauthconfig* pConfig);



/* 验证认证机制、凭据视图和机制专属分隔符。 */
XRT_API bool xrtImapAuthConfigValid(const ximapauthconfig* pConfig);



/* 在 NOT_AUTHENTICATED 会话上认证。非 IR 凭据超限在命令前拒绝；意外 literal
 * 或未完成的 LOGIN 取消终止会话。已完成的拒绝/取消保留可复用状态和主错误。 */
XRT_API bool xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
