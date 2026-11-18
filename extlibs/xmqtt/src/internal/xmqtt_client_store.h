#ifndef XMQTT_INTERNAL_CLIENT_STORE_H
#define XMQTT_INTERNAL_CLIENT_STORE_H
#include <xmqtt/client_store.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
/* Test/protocol integration entry permitting a legacy Writer with a caller
 * Inbox. The public factory requires explicit durable Inbox mode. */
typedef xmqttclientstoreclock xmqttstorewall;
typedef struct xmqttclientstoreplan {xmqttstorewrite* Writer;xmqttstorewall Wall;ptr User;} xmqttclientstoreplan;
XMQTT_INTERNAL bool xmqttInternalClientConnectStore(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,const xmqttclientstoreplan* Store,xmqttclient** Client);
XMQTT_INTERNAL bool xmqttInternalStoreWriteDriverClaim(xmqttstorewrite* W,const void* Owner,xmqttsession* Session,bool RequireInbox,
    xmqttstoredeltaconfig* Delta,xmqttstorestateinfo* Info,bool* Opened,xtaskpool** Pool);
XMQTT_INTERNAL void xmqttInternalStoreWriteDriverRelease(xmqttstorewrite* W,const void* Owner,bool Reusable);
XMQTT_INTERNAL xmqttjournaladmission xmqttInternalStoreWriteDriverAppend(xmqttstorewrite* W,const void* Owner,xbytesview Data,double Deadline,xfuture** Future);
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteDriverIncoming(xmqttstorewrite* W,const void* Owner,uint16 Identifier);
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteIncomingKey(xmqttstorestate* State,const void* Owner,uint16 Identifier);
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteDriverOutgoing(xmqttstorewrite* W,const void* Owner,
    uint16 Identifier,xbytesview Wire,bool Pubrel,bool Sent,uint8 PubrecReason,bool* Attempted);
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteOutgoingKey(xmqttstorestate* State,const void* Owner,
    uint16 Identifier,xbytesview Wire,bool Pubrel,bool Sent,uint8 PubrecReason,bool* Attempted);
XMQTT_INTERNAL bool xmqttInternalStoreWriteEverOpened(xmqttstorestate* State,const void* Owner);
XMQTT_INTERNAL void xmqttInternalSessionStoreAttempt(xmqttsession* Session,const void* Owner,
    const xmqttsessionpacket* Packet,bool Attempted);
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
XMQTT_INTERNAL bool xmqttInternalStoreWriteReserveReconnect(xmqttstorewrite* W,const void* Owner,xmqttsession* Session,
    xtaskpool* Cleanup,xtaskpool** StoragePool);
XMQTT_INTERNAL bool xmqttInternalStoreWriteReconnectUsable(xmqttstorewrite* W,const void* Owner);
XMQTT_INTERNAL void xmqttInternalStoreWriteReleaseReconnect(xmqttstorewrite* W,const void* Owner);
XMQTT_INTERNAL void xmqttInternalSessionStoreClosed(xmqttsession* Session,const void* Owner,bool Present,uint32 Expiry,double At);
#endif
#endif
#endif
