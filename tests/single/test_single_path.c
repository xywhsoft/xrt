#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供两种路径风格的词法能力。 */
int main(void)
{
	str sPath = xrtPathClean(XRT_STR_LITERAL("C:/a/../b"), XPATH_WINDOWS);
	xpathiter Iterator;
	xpathcomponent Component;
	bool bIter = xrtPathIterInit(&Iterator,
		XRT_STR_LITERAL("C:\\a"), XPATH_WINDOWS) &&
		xrtPathNext(&Iterator, &Component) &&
		(Component.Kind == XPATH_COMPONENT_ROOT);
	int iResult = (sPath != NULL) && (strcmp(sPath, "C:\\b") == 0) &&
		bIter && xrtPathIsLocal(XRT_STR_LITERAL("a\\b"), XPATH_WINDOWS) ?
		0 : 1;

	xrtFree(sPath);
	return iResult;
}
