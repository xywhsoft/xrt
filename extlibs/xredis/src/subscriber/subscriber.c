#include <xredis/subscriber.h>
#include <xrt/memory.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
XREDIS_API bool xredisSubscriberConfigInit(xredissubscriberconfig* Config)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config))){xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid subscriber config output");return false;}
    *Config=(xredissubscriberconfig){sizeof(*Config),4096,1024u*1024u};return true;
}
#endif
