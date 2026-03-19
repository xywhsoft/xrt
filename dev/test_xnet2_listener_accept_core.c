#include "test_xnet_impl_env.h"
#include "../test/test_xnet2_listener_accept_core.h"

int main(void)
{
	xrtGlobalData* pCore;
	int iRet;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	pCore = xrtInit();
	if ( !pCore ) {
		return 1;
	}

	iRet = Test_XNet2_ListenerAcceptCore();
	xrtUnit();
	return iRet;
}
