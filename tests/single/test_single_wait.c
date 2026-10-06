#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
int main(void)
{
    double Limit = __xrtWaitAfter(1);
    return XRT_WAIT_FOREVER == -1 && isfinite(Limit) &&
        __xrtWaitRemaining(Limit) <= 2 && isnan(__xrtWaitAfter(-2)) ? 0 : 1;
}
