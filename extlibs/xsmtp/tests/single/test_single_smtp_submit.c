#ifdef XSMTP_MODULE_XSMTP
	#undef XSMTP_MODULE_XSMTP
#endif
#define XSMTP_MODULE_SMTP_SUBMIT
#define XSMTP_IMPLEMENTATION
#include "../../single/xsmtp.h"



/* SMTP 提交单头必须只携带会话、Compose 及其公共依赖。 */
int main(void)
{
	xmailmessage Message;

	#if !defined(XSMTP_FEATURE_SMTP_SUBMIT) || \
		!defined(XSMTP_FEATURE_SMTP_CLIENT) || \
		!defined(XMAIL_FEATURE_MAIL_COMPOSE)
		#error "XSMTP_MODULE_SMTP_SUBMIT dependency closure is incomplete"
	#endif
	#if defined(XSMTP_FEATURE_SMTP_CLIENT_TLS) || \
		defined(XSMTP_FEATURE_SMTP_AUTH) || \
		defined(XMAIL_FEATURE_POP3_CLIENT) || \
		defined(XMAIL_FEATURE_IMAP_CLIENT)
		#error "XSMTP_MODULE_SMTP_SUBMIT retained unrelated transport features"
	#endif

	xrtMailMessageInit(&Message);
	return xrtSmtpSubmit(NULL, &Message, 0, NULL) ? 1 : 0;
}
