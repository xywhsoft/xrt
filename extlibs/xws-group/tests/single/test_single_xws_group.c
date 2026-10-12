#define XWS_GROUP_MODULE_ALL
#include "../../../xws-group/include/xws-group/features.h"
#include "../../../xws/include/xws/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XWS_IMPLEMENTATION
#include "../../../../single/extlibs/xws.h"
#define XWS_GROUP_IMPLEMENTATION
#include "../../../../single/extlibs/xws-group.h"

int main(void) { xwsgroup* pGroup = xrtWsGroupCreate(8); if (pGroup == NULL) return 1; xrtWsGroupDestroy(pGroup); return 0; }
