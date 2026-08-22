#include <stdio.h>

#include <xhttp.h>



/* 逐项读取服务端返回的认证 challenge。 */
int main(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"Digest realm=\"api\", Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
	);
	xhttpauth Challenge;
	xhttpnext Next;
	size_t iOffset = 0;

	while ( (Next = xrtHttpChallengeNext(
		Value, &iOffset, &Challenge
	)) == XHTTP_NEXT_ITEM ) {
		printf("%.*s\n",
			(int)Challenge.Scheme.Size,
			Challenge.Scheme.Data);
	}
	return (Next == XHTTP_NEXT_END) ? 0 : 1;
}
