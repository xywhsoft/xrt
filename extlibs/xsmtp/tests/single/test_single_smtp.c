#ifdef XSMTP_MODULE_XSMTP
	#undef XSMTP_MODULE_XSMTP
#endif
#define XSMTP_MODULE_SMTP
#define XSMTP_IMPLEMENTATION
#include "../../single/xsmtp.h"



/* SMTP 协议单头不应拉入网络、TLS 或高级 MIME 模块。 */
int main(void)
{
	xsmtpreplyline Reply;
	char arrCommand[16];
	size_t iSize;

	#if !defined(XSMTP_FEATURE_SMTP) || !defined(XMAIL_FEATURE_MAIL_WIRE)
		#error "XSMTP_MODULE_SMTP dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NET) || defined(XRT_FEATURE_TLS_STREAM) || \
		defined(XMAIL_FEATURE_MAIL_MESSAGE)
		#error "SMTP protocol unexpectedly enabled client dependencies"
	#endif

	return xrtSmtpReplyLineParse(XRT_STR_LITERAL("220 ready"), &Reply) &&
		xrtSmtpCommandWrite(
			XRT_STR_LITERAL("NOOP"),
			XRT_STR_LITERAL(""),
			arrCommand,
			sizeof(arrCommand),
			&iSize
		) && (Reply.Code == 220) && (iSize == 6u) ? 0 : 1;
}
