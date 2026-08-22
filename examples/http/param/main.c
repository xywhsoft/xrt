#include <stdio.h>

#include <xrt.h>



/* 逐项读取参数，并在需要时解码 quoted-string。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"charset=UTF-8; boundary=\"part;42\""
	);
	xhttpparam Param;
	xhttpnext Next;
	char Value[64];
	size_t iOffset = 0;
	size_t iSize;

	while ( (Next = xrtHttpParamNext(
		Text, &iOffset, &Param
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
			printf("%.*s\n", (int)Param.Name.Size, Param.Name.Data);
			continue;
		}
		if ( !xrtHttpParamValueWrite(
			&Param, Value, sizeof(Value), &iSize
		) ) {
			return 1;
		}
		printf("%.*s = %.*s\n",
			(int)Param.Name.Size, Param.Name.Data,
			(int)iSize, Value);
	}
	return (Next == XHTTP_NEXT_END) ? 0 : 2;
}
