#define XRT_MODULE_STRING_SPLIT
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <assert.h>



#if !defined(XRT_FEATURE_STRING)
	#error "XRT_MODULE_STRING_SPLIT must select the string dependency"
#endif

#if !defined(XRT_FEATURE_STRING_SPLIT)
	#error "XRT_MODULE_STRING_SPLIT must select its implementation feature"
#endif



/* 验证单个公开模块宏可以得到完整、可运行的实现闭包。 */
int main(void)
{
	xstrsplit Split;
	xstrview Item;

	assert(xrtStrSplitInit(
		&Split,
		xrtStrView("alpha,beta"),
		xrtStrView(",")
	));
	assert(xrtStrSplitNext(&Split, &Item));
	assert(xrtStrEqual(Item, xrtStrView("alpha")));
	assert(xrtStrSplitNext(&Split, &Item));
	assert(xrtStrEqual(Item, xrtStrView("beta")));
	assert(!xrtStrSplitNext(&Split, &Item));
	return 0;
}
