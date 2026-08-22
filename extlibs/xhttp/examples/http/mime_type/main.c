#include <stdio.h>

#include <xhttp.h>



/* 解析媒体类型并读取参数。 */
int main(void)
{
	xmediatype Type;
	xhttpparam Charset;

	if ( !xrtHttpMediaTypeParse(
		XRT_STR_LITERAL(
			"application/problem+json; charset=UTF-8"
		), &Type
	) || (xrtHttpMediaTypeParam(
		&Type, XRT_STR_LITERAL("charset"), &Charset
	) != XHTTP_NEXT_ITEM) ) {
		return 1;
	}
	printf(
		"charset = %.*s\n",
		(int)Charset.Value.Size,
		Charset.Value.Data
	);
	return 0;
}
