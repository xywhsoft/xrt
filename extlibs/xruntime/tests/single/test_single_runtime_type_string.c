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
	return iResult;
}
