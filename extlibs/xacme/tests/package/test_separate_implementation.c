/* Compile independently of the core implementation: no core-private helpers. */
#define XRT_MODULE_ALL
#include "../../../../single/xrt_decl.h"
#define XACME_MODULE_ALL
#define XACME_IMPLEMENTATION
#include "../../../../single/extlibs/xacme.h"
