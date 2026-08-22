#include <stdio.h>
#include <xruntime.h>



/* 展示拥有型字符串槽的复制和统一销毁。 */
int main(void)
{
	const xrttype* pType = xrtTypeString();
	str sSource = xrtStrDup("xlang");
	str sCopy = NULL;

	if (
		(sSource == NULL) ||
		!xrtTypeInitValue(pType, &sCopy) ||
		!xrtTypeCopyValue(pType, &sCopy, &sSource)
	) {
		xrtTypeDropValue(pType, &sSource);
		xrtTypeDropValue(pType, &sCopy);
		return 1;
	}
	printf("type=%.*s value=%s\n",
		(int)pType->Name.Size, pType->Name.Data, sCopy);
	xrtTypeDropValue(pType, &sSource);
	xrtTypeDropValue(pType, &sCopy);
	return 0;
}
