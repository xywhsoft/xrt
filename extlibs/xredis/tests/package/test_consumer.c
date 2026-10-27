#define XREDIS_MODULE_XREDIS
#include <xredis.h>
#include <string.h>

int main(void)
{
	xbytesview Argument = XRT_BYTES_LITERAL("PING");
	unsigned char Output[14]; size_t Written;
	if (!xredisCommandWrite(Output, sizeof(Output), &Argument, 1, &Written)) return 1;
	if (Written != sizeof(Output) || memcmp(Output, "*1\r\n$4\r\nPING\r\n", Written)) return 2;
	if (xredisCommandSize(NULL, 1, &Written)) return 3;
	if (xrtErrorKind(xrtGetError()) != XERR_ARGUMENT ||
		strcmp(xrtErrorDomain(xrtGetError()), "xrt.redis") != 0) return 4;
	xrtClearError();
	return 0;
}
