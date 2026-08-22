#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_FORM_URLENCODED
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 form-urlencoded 写出与原地解析。 */
int main(void)
{
	static const xformfield Input[] = {
		{ XRT_BYTES_INIT("name"), XRT_BYTES_INIT("alice bob") }
	};
	xformfield Output[1];
	uint8 Value[16];
	char Text[64];
	size_t iOffset = 0;
	size_t iSize;

	if ( !xrtFormWrite(Input, 1, Text, sizeof(Text), &iSize) ||
		(iSize != 14) || (memcmp(Text, "name=alice+bob", 14) != 0) ||
		!xrtFormParse(Text, iSize, Output, 1, &iSize, NULL) ||
		(iSize != 1) || (Output[0].Value.Size != 9) ||
		(memcmp(Output[0].Value.Data, "alice bob", 9) != 0) ||
		(xrtFormFind(
			XRT_STR_LITERAL("name=alice+bob"), XRT_BYTES_LITERAL("name"),
			&iOffset, Value, sizeof(Value), &iSize
		) != XFORM_FIND_FOUND) || (iSize != 9) ||
		(memcmp(Value, "alice bob", 9) != 0) ) {
		return 1;
	}
	printf("[PASS] single-form-urlencoded\n");
	return 0;
}
