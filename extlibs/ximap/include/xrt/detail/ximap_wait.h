#ifndef XRT_DETAIL_XIMAP_WAIT_H
#define XRT_DETAIL_XIMAP_WAIT_H
#include <ximap.h>
#include <xrt/detail/wait.h>

XRT_EXTERN_C_BEGIN
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_AUTH))
XRT_API bool __xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API ximapclient* __xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API xmailnext __xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientRefresh(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientLogout(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientClose(
	ximapclient* pClient,
	double iDeadline
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientNoop(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCheck(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientUnselect(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCloseMailbox(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientEndIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMPRESS))
XRT_API bool __xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool __xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bytes __xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool __xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
