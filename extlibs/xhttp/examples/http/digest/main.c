#include <stdio.h>

#include <xrt/http_digest.h>



/* 读取 Content-Digest 中全部可扩展算法和值。 */
int main(void)
{
	xhttpdigestcursor Cursor;
	xhttpdigest Digest;
	uint8 arrValue[64];
	size_t iSize;

	xrtHttpDigestCursorInit(&Cursor);
	while ( xrtHttpDigestNext(
		XRT_STR_LITERAL(
			"sha-256=:AA==:, future=:AQI=:"
		), &Cursor, &Digest
	) == XHTTP_NEXT_ITEM ) {
		if ( !xrtHttpDigestRead(
			&Digest, arrValue, sizeof(arrValue), &iSize
		) ) {
			return 1;
		}
		printf(
			"algorithm = %.*s, bytes = %zu\n",
			(int)Digest.Algorithm.Size,
			Digest.Algorithm.Data, iSize
		);
	}
	return xrtGetError() == NULL ? 0 : 1;
}
