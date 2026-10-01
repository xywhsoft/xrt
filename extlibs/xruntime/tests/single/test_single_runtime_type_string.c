#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 验证单头文件中的拥有型字符串描述。 */
int main(void)
{
	const xrttype* pType = xrtTypeString();
	str sSource = xrtStrDup("single");
	str sCopy = NULL;
	int iResult = 0;

	if (
		(sSource == NULL) ||
		!xrtTypeValidate(pType) ||
		!xrtTypeInitValue(pType, &sCopy) ||
		!xrtTypeCopyValue(pType, &sCopy, &sSource) ||
		(sCopy == sSource) ||
		(strcmp(sCopy, sSource) != 0)
	) {
		iResult = 1;
	}
	xrtTypeDropValue(pType, &sSource);
	xrtTypeDropValue(pType, &sCopy);
	{
		xstrview Source = XRT_STR_LITERAL("a\0b"), Copy = {0};
		if (!xrtTypeValidate(xrtTypeStringView()) ||
			!xrtTypeCopyValue(xrtTypeStringView(), &Copy, &Source) ||
			Copy.Size != 3u || memcmp(Copy.Data, Source.Data, 3u) != 0) iResult = 1;
		xrtTypeDropValue(xrtTypeStringView(), &Copy);
	}
	return iResult;
}
