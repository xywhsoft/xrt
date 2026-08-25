#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_CHARSET
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 字符集裁剪入口只带入 Unicode，不带入 MIME 编码器。 */
int main(void)
{
	char arrOutput[16];
	size_t iSize;
	static const unsigned char arrText[] = { 'c', 'a', 'f', 0xE9u };

	#if !defined(XMAIL_FEATURE_MAIL_CHARSET) || \
		!defined(XRT_FEATURE_UNICODE)
		#error "XMAIL_MODULE_MAIL_CHARSET dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_CODEC)
		#error "mail charset unexpectedly enabled mail codec"
	#endif

	return xrtMailCharsetToUtf8Write(
		XRT_STR_LITERAL("latin1"),
		(xbytesview) { arrText, sizeof(arrText) },
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) && (iSize == 5u) ? 0 : 1;
}
