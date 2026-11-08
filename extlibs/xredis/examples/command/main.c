#define XREDIS_MODULE_XREDIS_COMMAND
#include <xredis.h>
#include <stdio.h>
#include <string.h>

/* Offline example: no Redis server or credentials required. Run from the root:
 * python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_command
 * It verifies the exact wire bytes and emits a readable success line. */
int main(void)
{
	xbytesview Arguments[] = {XRT_BYTES_LITERAL("SET"), XRT_BYTES_LITERAL("key"),
		XRT_BYTES_LITERAL("hello")};
	unsigned char Output[64]; size_t Size, Written;
	if (!xredisCommandSize(Arguments, 3, &Size) || Size > sizeof(Output) ||
		!xredisCommandWrite(Output, sizeof(Output), Arguments, 3, &Written)) return 1;
	if (Written != Size || Written != 33u ||
		memcmp(Output, "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nhello\r\n", Written)) return 2;
	printf("xredis command: SET encoded into %zu bytes\n", Written);
	return 0;
}
