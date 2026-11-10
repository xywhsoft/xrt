#ifndef XMQTT_CLIENT_LIFECYCLE_INTERNAL_H
#define XMQTT_CLIENT_LIFECYCLE_INTERNAL_H
#include <xmqtt/client_session.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT)
typedef struct xmqttclientlifecycle {
    void (*Changed)(ptr Data,xmqttclient* Client,bool Closed,xmqttclientstatus Status,uint8 Reason);
    ptr Data;
} xmqttclientlifecycle;
XRT_EXTERN_C_BEGIN
XMQTT_INTERNAL bool xmqttInternalClientLifecycle(xmqttclient* Client,xnetworker* Worker,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttclientlifecycle* Observer);
XRT_EXTERN_C_END
#endif
#endif
