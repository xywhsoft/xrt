#include "../lib/xnet_port.h"
#include "../test/test_xnet2_port.h"
#include "../test/test_xnet2_engine.h"
#include "../test/test_xnet2_stream.h"
#include "../test/test_xnet2_dgram.h"
#include "../test/test_xnet2_tls.h"
#include "../test/test_xnet2_sync.h"
#include "../test/test_xnet2_codec.h"
#include "../test/test_xnet_http.h"
#include "../test/test_xnet_httpd.h"
#include "../test/test_xnet_ws.h"
#include "../test/test_xnet2_base.h"
#include "../test/test_xnet2_mem.h"

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

	Test_XNet2_Base();
	Test_XNet2_Port();
	Test_XNet2_Engine();
	Test_XNet2_Stream();
	Test_XNet2_Dgram();
	Test_XNet2_TLS();
	Test_XNet2_Sync();
	Test_XNet2_Codec();
	Test_XNet_Http();
	Test_XNet_Httpd();
	Test_XNet_Ws();
	Test_XNet2_Mem();

	#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
	#endif

	return 0;
}
