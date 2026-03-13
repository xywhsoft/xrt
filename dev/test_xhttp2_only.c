#include "../test/test_xnet2_http2.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
#endif

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA tWSA;
		if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) {
			return 1;
		}
	#endif

	Test_XNet2_Http2();

	#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
	#endif

	return 0;
}
