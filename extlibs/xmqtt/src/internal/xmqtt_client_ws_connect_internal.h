#ifndef XMQTT_CLIENT_WS_CONNECT_INTERNAL_H
#define XMQTT_CLIENT_WS_CONNECT_INTERNAL_H
#include <xmqtt/client_ws_connect.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT)
struct xmqttclientstoreconfig;
typedef struct xmqttwshttp {
    xmqttclientwsconnectinfo Info;xhttp1limits Limits;
    size_t MaxInformational,WireBytes,Offset,ColdSize;
    unsigned char *Cold,*Wire,*Cache;xhttpfield* Fields;
    size_t Used,Scan,LineStart,Line;char Key[XWS_KEY_CAPACITY];
} xmqttwshttp;
XRT_EXTERN_C_BEGIN
XMQTT_INTERNAL xmqttwshttp* xmqttInternalWsHttpCreate(const xmqttclientwsconnectconfig*,xmqttclient**);
XMQTT_INTERNAL void xmqttInternalWsHttpDrop(xmqttwshttp*);
XMQTT_INTERNAL bool xmqttInternalClientWsConnect(xnetengine*,xnetresolver*,xnetaddrlist*,const xnetaddr*,xstrview,uint16,uint64,const xmqttclientconfig*,const xmqttclientwsconnectconfig*,const void*,const xmqttpacket*,xmqttclient**,double,struct xmqttsession*,const struct xmqttclientstoreconfig*);
XRT_EXTERN_C_END
#endif
#endif
