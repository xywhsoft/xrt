#define XWS_MODULE_ALL
#include <xws.h>
#include <string.h>

int main(void)
{
	union {
		xwsconnconfig Alignment;
		uint8 Bytes[sizeof(xwsconnconfig) + 2u];
	} Storage;
	xwsconnconfig Config;

	memset(Storage.Bytes, 0xA5, sizeof(Storage.Bytes));
	xrtWsConnConfigInit(
		(xwsconnconfig*)(void*)(Storage.Bytes + 1u)
	);
	memcpy(&Config, Storage.Bytes + 1u, sizeof(Config));
	if ( !xrtWsConnConfigValid(
		(const xwsconnconfig*)(const void*)(Storage.Bytes + 1u)
	) || (Config.Role != XWS_ROLE_SERVER) ||
		(Storage.Bytes[0] != 0xA5) ||
		(Storage.Bytes[sizeof(Storage.Bytes) - 1u] != 0xA5) ) {
		return 1;
	}
	return 0;
}
