#include "test_xnet_impl_env.h"
#include "../test/test_xnet_native_core.h"

#include <stdio.h>

int main(int argc, char** argv)
{
	#if !defined(_WIN32) && !defined(_WIN64)
		(void)argc;
		(void)argv;
		printf("XNet Windows native backend core test: skipped (non-Windows)\n");
		return 0;
	#else
		xrtGlobalData* pCore;
		uint32 iPlainRounds = 32u;
		uint32 iTlsRounds = 8u;
		int iRet;

		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
		if ( argc > 1 ) iPlainRounds = (uint32)strtoul(argv[1], NULL, 10);
		if ( argc > 2 ) iTlsRounds = (uint32)strtoul(argv[2], NULL, 10);

		pCore = xrtInit();
		if ( !pCore ) {
			return 1;
		}

		iRet = Test_XNetNativeCoreWithRounds(iPlainRounds, iTlsRounds);
		xrtUnit();
		return iRet;
	#endif
}
