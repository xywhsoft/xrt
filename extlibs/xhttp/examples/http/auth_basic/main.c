#include <stdio.h>

#include <xhttp.h>



/* 构建 Basic Authorization 字段值。 */
int main(void)
{
	char Value[128];
	size_t iSize;

	if ( !xrtHttpBasicWrite(
		XRT_STR_LITERAL("Aladdin"),
		XRT_STR_LITERAL("open sesame"),
		Value,
		sizeof(Value),
		&iSize
	) ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, Value);
	return 0;
}
