#define XMAIL_MODULE_XMAIL
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 当前邮件内容聚合入口同时提供编码与字段原语。 */
int main(void)
{
	char arrOutput[32];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_MAIL) || \
		!defined(XMAIL_FEATURE_MAIL_CORE) || \
		!defined(XMAIL_FEATURE_MAIL_CODEC) || \
		!defined(XMAIL_FEATURE_MAIL_HEADER) || \
		!defined(XMAIL_FEATURE_MAIL_WORD) || \
		!defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
		!defined(XMAIL_FEATURE_MAIL_DATE) || \
		!defined(XMAIL_FEATURE_MAIL_ID) || \
		!defined(XMAIL_FEATURE_MAIL_PARAM) || \
		!defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
		!defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
		!defined(XMAIL_FEATURE_MAIL_WIRE) || \
		!defined(XMAIL_FEATURE_SMTP) || \
		!defined(XMAIL_FEATURE_POP3) || \
		!defined(XMAIL_FEATURE_IMAP)
		#error "XMAIL_MODULE_XMAIL dependency closure is incomplete"
	#endif

	return xrtMailQpWrite(
		"a b",
		3u,
		0,
		XMAIL_QP_TEXT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) ? 0 : 1;
}
