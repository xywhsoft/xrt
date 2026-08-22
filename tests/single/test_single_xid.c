#define XRT_MODULE_XID
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头 XID 拉起精确依赖并完成生成、文本往返和时间提取。 */
int main(void)
{
	xid Value;
	xid Parsed;
	char arrText[XID_TEXT_CAPACITY];
	xtime iTime;

	#if !defined(XRT_FEATURE_XID) || \
		!defined(XRT_FEATURE_TIME) || \
		!defined(XRT_FEATURE_RANDOM_SECURE) || \
		!defined(XRT_FEATURE_CODEC_BASE64)
		#error "XRT_MODULE_XID did not enable its dependency closure"
	#endif

	if ( !xrtXidMake(&Value) ) {
		return 1;
	}
	if ( !xrtXidWrite(&Value, arrText, sizeof(arrText)) ) {
		return 2;
	}
	if ( !xrtXidParse(
		(xstrview){ arrText, XID_TEXT_SIZE },
		&Parsed
	) || !xrtXidEqual(&Value, &Parsed) ) {
		return 3;
	}
	if ( !xrtXidTime(&Parsed, &iTime) || (iTime <= 0) ) {
		return 4;
	}
	return 0;
}
