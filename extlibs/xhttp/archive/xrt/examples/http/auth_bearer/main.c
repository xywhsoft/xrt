#include <stdio.h>

#include <xrt.h>



/* 构建 Bearer Authorization 字段值。 */
int main(void)
{
	char Value[128];
	size_t iSize;

	if ( !xrtHttpBearerWrite(
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM"),
		Value,
		sizeof(Value),
		&iSize
	) ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, Value);
	return 0;
}
