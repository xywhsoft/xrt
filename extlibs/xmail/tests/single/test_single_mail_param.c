#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_PARAM
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 参数裁剪入口依赖邮件核心和 UTF-8 校验，不拉入通用 string 或内容树。 */
int main(void)
{
	xmailmediatypeview MediaType;
	char arrValue[XMAIL_PARAM_SECTION_SIZE + 1u];
	char arrOutput[192];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_MAIL_PARAM) || \
		!defined(XMAIL_FEATURE_MAIL_CORE) || !defined(XRT_FEATURE_UNICODE)
		#error "XMAIL_MODULE_MAIL_PARAM dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_STRING) || defined(XMAIL_FEATURE_MAIL_MULTIPART)
		#error "mail param unexpectedly enabled unrelated modules"
	#endif

	if ( !xrtMailMediaTypeParse(
		XRT_STR_LITERAL("text/plain; charset=UTF-8"),
		&MediaType
	) ) {
		return 1;
	}
	if ( !xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		XRT_STR_LITERAL("报告.txt"),
		XMAIL_PARAM_ENCODING_AUTO,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) ) {
		return 2;
	}
	if ( strcmp(
		arrOutput,
		"; filename*=UTF-8''%E6%8A%A5%E5%91%8A.txt"
	) != 0 ) {
		return 3;
	}
	memset(arrValue, 'n', sizeof(arrValue));
	if ( !xrtMailParamWrite(
		XRT_STR_LITERAL("filename"),
		(xstrview){ arrValue, sizeof(arrValue) },
		XMAIL_PARAM_ENCODING_TOKEN,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) ) {
		return 4;
	}
	return strstr(arrOutput, "; filename*1=") != NULL ? 0 : 5;
}
