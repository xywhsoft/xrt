#include <xredis/tracking.h>
#include <xrt/memory.h>
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
XREDIS_API bool xredisTrackingConfigInit(xredistrackingconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid tracking config output");return false;
    }
    *K=(xredistrackingconfig){sizeof(*K),true,0,0,NULL,0};return true;
}
#endif
