#include <xmqtt/client_ws.h>
#include "../internal/xmqtt_client_ws_internal.h"
#include <xrt/memory.h>
#include <xrt/time.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS)
XMQTT_API bool xmqttClientWsConfigInit(xmqttclientwsconfig* K)
{
    if(!K||!xrtMemRangeValid(K,sizeof(*K))){xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt.client",1,"invalid WS config output");return false;}
    *K=(xmqttclientwsconfig){.Size=sizeof(*K)};xrtWsStreamConfigInit(&K->Stream);K->Stream.Role=XWS_ROLE_CLIENT;K->Stream.Protocol=XRT_STR_LITERAL("mqtt");K->MaxStagedBytes=1048576;K->KeepStagedBytes=65536;return true;
}
XMQTT_API bool xmqttClientWsAttachTcp(xnetstream* N,xnetbuf* B,const xmqttclientconfig* C,const xmqttclientwsconfig* W,const xmqttpacket* Q,xmqttclient** O)
{return xmqttInternalClientWsAttach(N,false,B,C,W,Q,O,xrtTimer());}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
XMQTT_API bool xmqttClientWsAttachTls(xtlsstream* N,xnetbuf* B,const xmqttclientconfig* C,const xmqttclientwsconfig* W,const xmqttpacket* Q,xmqttclient** O)
{return xmqttInternalClientWsAttach(N,true,B,C,W,Q,O,xrtTimer());}
#endif
#endif
