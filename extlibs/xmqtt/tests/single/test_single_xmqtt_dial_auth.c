#define XMQTT_MODULE_XMQTT
#define XRT_MODULE_MEMORY_STATS
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include <xmqtt/client.h>
static xatomic32 dialBindingFail;
static bool dialBindFault(xnetstream* S,ptr D,const xnetstreameventsownershipv1* P)
{uint32 Mode=xrtAtomic32Load(&dialBindingFail,XMEMORY_ACQUIRE);if(Mode==3)__xrtNetStreamEndRead(S);if(Mode==2){if(!xmqttClientClose((xmqttclient*)D))abort();}if(Mode==1){xrtSetErrorInfo(XERR_MEMORY,"fixture.mqtt.bind",1,"event binding allocation failed");return false;}return xrtNetStreamSetEventsOwnedV1(S,D,P);}
#define xrtNetStreamSetEventsOwnedV1 dialBindFault
#define XMQTT_DIAL_BIND_FAULT
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtNetStreamSetEventsOwnedV1
#include "../test_client_dial.c"
