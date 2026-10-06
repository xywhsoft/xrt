#ifndef XRT_DETAIL_XSMTP_WAIT_H
#define XRT_DETAIL_XSMTP_WAIT_H
#include <xsmtp.h>
#include <xrt/detail/wait.h>

XRT_EXTERN_C_BEGIN
#if (defined(XSMTP_FEATURE_SMTP_AUTH))
XRT_API bool __xrtSmtpClientAuth(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API xsmtpclient* __xrtSmtpClientOpen(
	const xsmtpclientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientSend(
	xsmtpclient* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientAuthLine(
	xsmtpclient* pClient,
	xstrview Line,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientReceive(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientCommand(
	xsmtpclient* pClient,
	xstrview Verb,
	xstrview Arguments,
	xsmtpreply* pReply,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientMail(
	xsmtpclient* pClient,
	xstrview ReversePath,
	xstrview Parameters,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientRcpt(
	xsmtpclient* pClient,
	xstrview ForwardPath,
	xstrview Parameters,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientDataBegin(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientDataWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientDataEnd(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientData(
	xsmtpclient* pClient,
	xstrview Message,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdatBegin(
	xsmtpclient* pClient,
	size_t iChunkSize,
	bool Last,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdatWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdatEnd(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientBdat(
	xsmtpclient* pClient,
	xbytesview Data,
	bool Last,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientReset(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientNoop(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientQuit(
	xsmtpclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_CLIENT))
XRT_API bool __xrtSmtpClientClose(
	xsmtpclient* pClient,
	double iDeadline
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_SUBMIT))
XRT_API bool __xrtSmtpSubmitEnvelope(
	xsmtpclient* pClient,
	const xsmtpenvelope* pEnvelope,
	const xmailmessage* pMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XSMTP_FEATURE_SMTP_SUBMIT))
XRT_API bool __xrtSmtpSubmit(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
