/* Standalone reproduction and regression entry for JWT time claims. */
#if defined(XJWT_FEATURE_XJWT) || defined(XJWT_SINGLE_HEADER)
#include "../src/internal/xjwt_internal.h"
#else
#define XRT_IMPLEMENTATION
#include "support/runtime.h"
#include "support/implementation.c"
#endif
#include "time_bounds_cases.h"

int main(int argc, char** argv)
{
    return jwt_time_bounds_run(argc == 2 && strcmp(argv[1], "overflow") == 0);
}
