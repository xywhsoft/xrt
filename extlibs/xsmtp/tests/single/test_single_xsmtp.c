#ifdef XSMTP_MODULE_XSMTP
	#undef XSMTP_MODULE_XSMTP
#endif
#define XSMTP_MODULE_XSMTP
#define XSMTP_IMPLEMENTATION
#include "../../single/xsmtp.h"



/* xsmtp 产品伞单头应点亮自身全部协议与基座闭包。 */
int main(void)
{
	xsmtpreplyline Reply;

	#if (!defined(XSMTP_FEATURE_SMTP) || \
	!defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	!defined(XSMTP_FEATURE_SMTP_CLIENT_TLS) || \
	!defined(XSMTP_FEATURE_SMTP_SUBMIT) || \
	!defined(XSMTP_FEATURE_SMTP_AUTH) || \
	!defined(XMAIL_FEATURE_MAIL_WIRE) || \
	!defined(XMAIL_FEATURE_MAIL_NET) || \
	!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	!defined(XMAIL_FEATURE_MAIL_COMPOSE))
		#error "XSMTP_MODULE_XSMTP dependency closure is incomplete"
	#endif

	return xrtSmtpReplyLineParse(
		XRT_STR_LITERAL("250-SIZE 10485760"),
		&Reply
	) ? 0 : 1;
}
