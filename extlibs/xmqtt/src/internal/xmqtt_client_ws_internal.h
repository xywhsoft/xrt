#ifndef XMQTT_CLIENT_WS_INTERNAL_H
#define XMQTT_CLIENT_WS_INTERNAL_H
#include <xmqtt/client_ws.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS)
XRT_EXTERN_C_BEGIN
XMQTT_INTERNAL bool xmqttInternalClientWsAttach(ptr,bool,xnetbuf*,const xmqttclientconfig*,const xmqttclientwsconfig*,const xmqttpacket*,xmqttclient**,double);
XRT_EXTERN_C_END
#endif
#endif
