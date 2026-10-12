#define XWS_MODULE_ALL
#include "../../../xws/include/xws/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XWS_IMPLEMENTATION
#include "../../../../single/extlibs/xws.h"

int main(void) { xwsconnconfig Config; xrtWsConnConfigInit(&Config); return 0; }
