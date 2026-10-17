#define XRUDP_MODULE_ALL
#include "../../include/xrudp/features.h"
#if defined(XRT_MODULE_MEMORY_DEBUG) || defined(XRT_FEATURE_MEMORY_DEBUG)
#error "Selecting production xrudp ALL must not import test-only memory debugging"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#if defined(XRT_FEATURE_MEMORY_DEBUG)
#error "Production xrudp ALL unexpectedly enabled memory fault instrumentation"
#endif
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"

int main(void)
{
	xrudpparameters parameters;
	unsigned char wire[XRUDP_PARAMETERS_SIZE];
	xrtRudpParametersDefault(&parameters);
	return xrtRudpParametersEncode(&parameters, wire, sizeof(wire)) != XRUDP_OK;
}
