#include <stdio.h>

#include <xrt/http_trailer.h>
#include <xrt/memory.h>



/* 从实际 trailer 集合构建规范且去重的声明值。 */
int main(void)
{
	static const xhttpfield Trailers[] = {
		{
			XRT_STR_INIT("Content-Digest"),
			XRT_STR_INIT("sha-256=:...:")
		},
		{
			XRT_STR_INIT("X-Result"),
			XRT_STR_INIT("complete")
		}
	};
	str sNames;

	sNames = xrtHttpTrailerNamesBuild(Trailers, 2u, NULL);
	if ( sNames == NULL ) {
		return 1;
	}
	printf("Trailer: %s\n", sNames);
	xrtFree(sNames);
	return 0;
}
