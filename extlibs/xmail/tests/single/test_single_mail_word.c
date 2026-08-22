#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_WORD
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 编码词裁剪入口必须闭包 Unicode、Base64 与邮件编码层。 */
int main(void)
{
	char arrOutput[64];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_MAIL_WORD) || \
		!defined(XMAIL_FEATURE_MAIL_CODEC) || \
		!defined(XRT_FEATURE_UNICODE) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XMAIL_MODULE_MAIL_WORD dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_HEADER)
		#error "mail encoded-word unexpectedly enabled mail header"
	#endif

	return xrtMailWordDecodeWrite(
		XRT_STR_LITERAL("=?UTF-8?Q?hello?="),
		XMAIL_WORD_STRICT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (strcmp(arrOutput, "hello") == 0) ? 0 : 1;
}
