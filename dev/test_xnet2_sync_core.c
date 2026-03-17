#include "test_xnet_impl_env.h"
#include "../test/test_xnet2_sync.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
#endif

int main(void)
{
	xrtGlobalData* pCore;
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA tWSA;
		if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) {
			return 1;
		}
	#endif

	pCore = xrtInit();
	if ( !pCore ) {
		#if defined(_WIN32) || defined(_WIN64)
			WSACleanup();
		#endif
		return 1;
	}

	Test_XNet2_Sync();

	xrtUnit();
	#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
	#endif
	return 0;
}
