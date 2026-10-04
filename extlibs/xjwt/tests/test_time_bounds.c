/* Standalone reproduction and regression entry for JWT time claims. */
#define XRT_IMPLEMENTATION
#include "../xjwt-xrt.h"
#include <xrt.h>
#include "../xjwt.c"
#include "time_bounds_cases.h"

int main(int argc, char** argv)
{
    return jwt_time_bounds_run(argc == 2 && strcmp(argv[1], "overflow") == 0);
}
