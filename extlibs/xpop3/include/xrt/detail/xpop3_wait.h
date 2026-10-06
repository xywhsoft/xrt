#ifndef XRT_DETAIL_XPOP3_WAIT_H
#define XRT_DETAIL_XPOP3_WAIT_H
#include <xpop3.h>
#include <xrt/detail/wait.h>

XRT_EXTERN_C_BEGIN
#if (defined(XPOP3_FEATURE_POP3_AUTH))
XRT_API bool __xrtPop3ClientAuth(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_AUTH))
XRT_API bool __xrtPop3ClientLogin(
	xpop3client* pClient,
	xstrview Username,
	xstrview Password,
	bool AllowPlaintext,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API xpop3client* __xrtPop3ClientOpen(
	const xpop3clientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientSend(
	xpop3client* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientAuthLine(
	xpop3client* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientLine(
	xpop3client* pClient,
	xstrview* pLine,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientReceive(
	xpop3client* pClient,
	xpop3reply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientCommand(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xpop3reply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientBegin(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API xmailnext __xrtPop3ClientNext(
	xpop3client* pClient,
	xstrview* pLine,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientStat(
	xpop3client* pClient,
	xpop3stat* pStat,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientList(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3listview* pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientListAll(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientUidl(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3uidlview* pItem,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientUidlAll(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientRetr(
	xpop3client* pClient,
	uint64 iMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientTop(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientDelete(
	xpop3client* pClient,
	uint64 iMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientReset(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientNoop(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientQuit(
	xpop3client* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_CLIENT))
XRT_API bool __xrtPop3ClientClose(
	xpop3client* pClient,
	double iDeadline
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bool __xrtPop3ClientRetrWrite(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bool __xrtPop3ClientTopWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bytes __xrtPop3ClientRetrBytes(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bytes __xrtPop3ClientTopBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XPOP3_FEATURE_POP3_MESSAGE))
XRT_API bool __xrtPop3ClientRetrTree(
	xpop3client* pClient,
	uint64 iMessage,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
