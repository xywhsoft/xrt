#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_CORE
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 最小邮件核心单头必须覆盖换行规范化和边界语法。 */
int main(void)
{
	char arrOutput[8];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_MAIL_CORE)
		#error "XMAIL_MODULE_MAIL_CORE did not enable mail core"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_CODEC) || defined(XMAIL_FEATURE_MAIL_HEADER)
		#error "mail core unexpectedly enabled higher mail features"
	#endif

	if ( !xrtMailCrlfWrite(
		XRT_STR_LITERAL("a\nb"),
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) || (iSize != 4u) ) {
		return 1;
	}
	if ( !xrtMailBoundaryValid(XRT_STR_LITERAL("xrt-boundary_01")) ) {
		return 2;
	}
	return xrtMailBoundaryValid(XRT_STR_LITERAL("bad boundary ")) ? 3 : 0;
}
