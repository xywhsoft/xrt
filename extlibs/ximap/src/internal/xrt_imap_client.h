#include <xrt/detail/ximap_wait.h>
#include <xrt/detail/wait.h>
#ifndef XRT_INTERNAL_IMAP_CLIENT_H
#define XRT_INTERNAL_IMAP_CLIENT_H

#include "xrt_mail.h"
#include "xrt_mail_net.h"

#if defined(XIMAP_FEATURE_IMAP_COMPRESS)
	#include <xrt/compress.h>
#endif



#if defined(XIMAP_FEATURE_IMAP_CLIENT)

/* 客户端只保存协议状态、能力快照和当前顺序命令，不缓存完整响应。 */
struct ximapclient {
	__xmailtransport Transport;
	__xmailtext Last;
	ximapclientstate State;
	uint64 Capabilities;
	uint64 AppendLimit;
	size_t CommandLineLimit;
	size_t LiteralRemaining;
	size_t AppendRemaining;
	uint32 TagCounter;
	char ActiveTag[XIMAP_CLIENT_TAG_MAX + 1u];
	size_t ActiveTagSize;
	bool Active;
	bool ExpectFragment;
	bool Idle;
	bool IdleDone;
	bool Append;
	bool Closing;
	bool LogoutSent;
};

bool __xrtImapClientStateCommit(
	ximapclient* pClient,
	ximapclientstate State
);



bool __xrtImapClientProtocolFail(
	ximapclient* pClient,
	cstr sMessage
);



bool __xrtImapClientAppendStart(ximapclient* pClient, size_t iSize);



size_t __xrtImapClientAppendRemaining(const ximapclient* pClient);



bool __xrtImapClientAppendFinish(ximapclient* pClient);



bool __xrtImapClientIdleStart(ximapclient* pClient);



bool __xrtImapClientIdleEnd(ximapclient* pClient);



#if defined(XIMAP_FEATURE_IMAP_COMPRESS)
bool __xrtImapClientCompressStart(
	ximapclient* pClient,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
);



bool __xrtImapClientCompressed(const ximapclient* pClient);
#endif



#endif

#endif
