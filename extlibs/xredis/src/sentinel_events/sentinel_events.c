#include "../internal/sentinel_events_internal.h"
#include <xrt/memory.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
static bool xredisSentinelEventArgument(cstr Message)
{
    xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,Message);
    return false;
}
XREDIS_API bool xredisSentinelEventConfigInit(xredissentineleventconfig* Config)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))return xredisSentinelEventArgument("invalid Sentinel event config");
    *Config=(xredissentineleventconfig){sizeof(*Config),16,5000,100,10000,50};
    return true;
}
XREDIS_API xredisadmission xredisSentinelPoolListen(xredispool* Pool,const xredissentineleventconfig* Config)
{
    if(!Pool || !Config || !xrtMemRangeValid(Config,sizeof(*Config)) || Config->Size!=sizeof(*Config) ||
       !Config->WorkMessages || Config->WorkMessages>64 || Config->AttemptTimeout<=0 || Config->AttemptTimeout>60000 ||
       Config->RetryMin<=0 || Config->RetryMax<Config->RetryMin || Config->RetryMax>60000 ||
       Config->HintInterval<=0 || Config->HintInterval>60000){
        xredisSentinelEventArgument("invalid Sentinel event listener configuration");return XREDIS_ADMISSION_ERROR;
    }
    return __xredisSentinelPoolListen(Pool,Config);
}
XREDIS_API bool xredisSentinelPoolEventInfo(xredispool* Pool,xredissentineleventinfo* Info)
{return __xredisSentinelPoolEventInfo(Pool,Info);}

bool __xredisSentinelSwitchEvent(const xredisreply* Reply,xbytesview Service,size_t MaxHostBytes)
{
    static const char Channel[]="+switch-master";
    xredispubsubevent Event;
    if(xredisPubSubEventRead(Reply,&Event)!=XREDIS_PUBSUB_EVENT || Event.Kind!=XREDIS_PUBSUB_MESSAGE ||
       Event.Channel.Size!=sizeof(Channel)-1 || memcmp(Event.Channel.Data,Channel,sizeof(Channel)-1))return false;
    xbytesview Payload=Event.Payload;
    if(Payload.Size<=Service.Size || Payload.Size>Service.Size+2*MaxHostBytes+14 ||
       Payload.Data[Service.Size]!=' ' || memcmp(Payload.Data,Service.Data,Service.Size))return false;
    size_t At=Service.Size+1;
    /* Validate the documented host/port pairs. Their values never escape. */
    for(unsigned Field=0;Field<4;++Field){
        size_t Begin=At;
        size_t Limit=Field%2?5:MaxHostBytes;
        unsigned Port=0;
        while(At<Payload.Size && Payload.Data[At]!=' '){
            unsigned C=Payload.Data[At];
            if(At-Begin==Limit || C<=32 || C==127)return false;
            if(Field%2){
                if(C<'0' || C>'9')return false;
                Port=10u*Port+C-'0';
            }
            ++At;
        }
        if(At==Begin || (Field%2 && (!Port || Port>65535)))return false;
        if(Field!=3 && At++==Payload.Size)return false;
    }
    return At==Payload.Size;
}
#endif
