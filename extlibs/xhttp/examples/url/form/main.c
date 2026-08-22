#include <stdio.h>

#include <xhttp.h>



/* 演示构建 POST 表单正文并在接收端原地解析。 */
int main(void)
{
	static const xformfield Input[] = {
		{ XRT_BYTES_INIT("name"), XRT_BYTES_INIT("alice bob") },
		{ XRT_BYTES_INIT("language"), XRT_BYTES_INIT("xlang") }
	};
	xformfield Output[2];
	uint8 Language[16];
	char Body[128];
	size_t iOffset = 0;
	size_t iSize;
	size_t i;

	if ( !xrtFormWrite(Input, 2, Body, sizeof(Body), &iSize) ) {
		return 1;
	}
	printf("body: %.*s\n", (int)iSize, Body);
	if ( xrtFormFind(
		(xstrview){ Body, iSize }, XRT_BYTES_LITERAL("language"),
		&iOffset, Language, sizeof(Language), &i
	) != XFORM_FIND_FOUND ) {
		return 2;
	}
	printf("language: %.*s\n", (int)i, (const char*)Language);
	if ( !xrtFormParse(Body, iSize, Output, 2, &iSize, NULL) ) {
		return 3;
	}
	for ( i = 0; i < iSize; i++ ) {
		printf("%.*s = %.*s\n",
			(int)Output[i].Name.Size, (const char*)Output[i].Name.Data,
			(int)Output[i].Value.Size, (const char*)Output[i].Value.Data);
	}
	return 0;
}
