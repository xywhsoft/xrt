#define XRUDP_MODULE_ALL
#include <xrudp.h>

int main(void)
{
	xrudpreplay replay;
	xrtRudpReplayInit(&replay);
	return xrtRudpReplayAccept(&replay, 1) != XRUDP_OK;
}
