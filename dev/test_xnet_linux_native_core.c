#include "test_xnet_impl_env.h"
#include "../test/test_xnet_native_core.h"

#include <stdio.h>

int main(int argc, char** argv)
{
	#if !defined(__linux__)
		(void)argc;
		(void)argv;
		printf("XNet Linux native backend core test: skipped (non-Linux)\n");
		return 0;
	#else
		xrtGlobalData* pCore;
		uint32 iPlainRounds = 16u;
		uint32 iTlsRounds = 4u;
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
