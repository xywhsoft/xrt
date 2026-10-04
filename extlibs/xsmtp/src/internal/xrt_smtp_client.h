#ifndef XRT_INTERNAL_SMTP_CLIENT_H
#define XRT_INTERNAL_SMTP_CLIENT_H

#include <xrt/smtp_client.h>
#include "xrt_mail_net.h"



#if defined(XSMTP_FEATURE_SMTP_CLIENT)

/* 同步客户端只保存会话状态、能力和最后响应，不复制配置。 */
struct xsmtpclient {
	__xmailtransport Transport;
	xsmtpclientstate State;
	uint64 Capabilities;
	uint64 SizeLimit;
	__xmailtext Reply;
	size_t ReplyLines;
	size_t ReplyLineLimit;
	int ReplyCode;
	xmaildotwriter DataWriter;
	size_t ChunkRemaining;
	bool ChunkLast;
	bool ChunkActive;
	bool ChunkRejected;
	bool Authenticated;
};

/* 认证层成功完成 AUTH 后只通过该内部边界更新会话状态。 */
void __xrtSmtpClientAuthComplete(xsmtpclient* pClient);

#endif

#endif
