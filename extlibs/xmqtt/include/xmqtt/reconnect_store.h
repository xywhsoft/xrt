#ifndef XMQTT_RECONNECT_STORE_H
#define XMQTT_RECONNECT_STORE_H
#include <xmqtt/reconnect.h>
#include <xmqtt/client_store.h>
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE) && (!defined(XMQTT_FEATURE_XMQTT_RECONNECT) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE))
#error "Durable MQTT reconnect requires Reconnect and Client Store"
#endif
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
XRT_EXTERN_C_BEGIN
/* Runs on the supplied Engine worker. Use a durable Client factory with the
 * exact supplied Session and Store. Borrow the arguments for this call.
 * Transport policy and FactoryData belong to the application. Return one
 * Client with OK, or a typed failure without transferring a Client. */
typedef xmqttclientstatus (*xmqttreconnectstorefactory)(xnetengine* Engine,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttsession* Session,const xmqttclientstoreconfig* Store,
    const xmqttpacket* Connect,ptr FactoryData,xmqttclient** Client);
/* Success consumes the matching fresh or recovered disconnected Session, and
 * reserves the caller-owned Inbox Writer through Destroy. Serialize creation
 * with Writer lifecycle and mutations. CleanupPool is the existing Reconnect
 * cleanup pool and must differ from the Writer's IO pool. Both must keep
 * running. The stored factory argument selects transport; Config.FactoryData,
 * retry and notification policy follow Reconnect. Session caps come from
 * the transferred Session. ReconnectTakeMessage is empty; use StoreInboxTake
 * and StoreInboxAckAsync throughout the controller lifetime.
 * Each new connection waits for actual retirement of the previous Client,
 * leases and storage effects. An unusable Writer prevents the next attempt
 * with SESSION. Recover the authoritative journal before constructing another
 * controller. Original Futures never transfer to the new connection. */
XMQTT_API bool xmqttReconnectCreateStore(xnetengine* Engine,xtaskpool* CleanupPool,uint64 Affinity,
    const xmqttreconnectconfig* Config,xmqttsession* Session,const xmqttclientstoreconfig* Store,
    xmqttreconnectstorefactory Factory,const xmqttpacket* Connect,xmqttreconnect** Controller);
XRT_EXTERN_C_END
#endif
#endif
