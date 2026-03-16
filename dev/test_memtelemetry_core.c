#include "../xrt.h"
#include "../test/test_memtelemetry.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#endif

int main(void)
{
	xrtGlobalData* pCore;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

#if defined(_WIN32) || defined(_WIN64)
	{
		WSADATA tWSA;
		if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) {
			return 1;
		}
	}
#endif

	pCore = xrtInit();
	if ( !pCore ) {
		return 2;
	}

	Test_MemTelemetry();

	xrtUnit();

#if defined(_WIN32) || defined(_WIN64)
	WSACleanup();
#endif

	return 0;
}
