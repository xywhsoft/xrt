#ifndef XMQTT_INTERNAL_CLIENT_STORE_H
#define XMQTT_INTERNAL_CLIENT_STORE_H
#include <xmqtt/client_session.h>
#include <xmqtt/store_write.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
/* Private integration under development. No public factory until all online
 * transitions and durable delivery capabilities are implemented. Callers stop
 * all Writer API entries before attaching. Engine/pool stay running through
 * Client physical retirement; file/backend outlive Writer destruction. */
typedef bool (*xmqttstorewall)(ptr User,uint64* Milliseconds);
typedef struct xmqttclientstoreplan {xmqttstorewrite* Writer;xmqttstorewall Wall;ptr User;} xmqttclientstoreplan;
XMQTT_INTERNAL bool xmqttInternalClientConnectStore(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,const xmqttclientstoreplan* Store,xmqttclient** Client);
XMQTT_INTERNAL bool xmqttInternalStoreWriteDriverClaim(xmqttstorewrite* W,const void* Owner,
    xmqttstoredeltaconfig* Delta,xmqttstorestateinfo* Info,bool* Opened,xtaskpool** Pool);
XMQTT_INTERNAL void xmqttInternalStoreWriteDriverRelease(xmqttstorewrite* W,const void* Owner,bool Reusable);
XMQTT_INTERNAL xmqttjournaladmission xmqttInternalStoreWriteDriverAppend(xmqttstorewrite* W,const void* Owner,xbytesview Data,double Deadline,xfuture** Future);
XMQTT_INTERNAL bool xmqttInternalStoreWriteEverOpened(xmqttstorestate* State,const void* Owner);
#endif
#endif
