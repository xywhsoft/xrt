#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_CODEC
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 邮件编码裁剪入口必须带入 Base64 而不带入字段解析。 */
int main(void)
{
	char arrOutput[32];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_MAIL_CODEC) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XMAIL_MODULE_MAIL_CODEC dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_HEADER)
		#error "mail codec unexpectedly enabled mail header"
	#endif

	return xrtMailBase64Write(
		"hello",
		5u,
		0,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 10u) ? 0 : 1;
}
