#define XRUDP_MODULE_RUDP_WIRE
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"

int main(void)
{
	xrudpreplay replay;
	xrtRudpReplayInit(&replay);
	return xrtRudpReplayAccept(&replay, 1) != XRUDP_OK ||
		xrtRudpReplayCheck(&replay, 1) != XRUDP_DUPLICATE;
}
