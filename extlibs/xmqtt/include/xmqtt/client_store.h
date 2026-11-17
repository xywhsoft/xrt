#ifndef XMQTT_CLIENT_STORE_H
#define XMQTT_CLIENT_STORE_H
#include <xmqtt/client_session.h>
#include <xmqtt/store_inbox.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION) || !defined(XMQTT_FEATURE_XMQTT_STORE_INBOX))
#error "MQTT durable Client requires Session and the durable Inbox writer"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
typedef bool (*xmqttclientstoreclock)(ptr User,uint64* Milliseconds);
typedef struct xmqttclientstoreconfig {
    size_t Size;
    xmqttstorewrite* Writer;
    xmqttclientstoreclock Wall;
    ptr User;
} xmqttclientstoreconfig;
typedef struct xmqttclientstorecheckpoint {
    uint64 Sequence;
    xmqttjournaleffect Effect;
} xmqttclientstorecheckpoint;
typedef struct xmqttclientstoreoutcome {
    uint64 Epoch,Key;
    xmqttclientstorecheckpoint Attempt,Pubrel,Completion,End;
} xmqttclientstoreoutcome;
XRT_EXTERN_C_BEGIN
/* Developing numeric TCP durable Client. Use a Writer from StoreInboxCreate
 * and a corresponding fresh or exactly recovered disconnected Session.
 * Serialize construction with Writer API calls. Successful construction claims
 * both through ClientDestroy and borrows the running Engine and storage pool.
 * Keep the file, provider and clock context alive through actual retirement.
 * Clock returns trusted nondecreasing Unix epoch milliseconds (<=INT64_MAX);
 * it runs on the Engine worker and must be nonblocking. Use a trusted TCP endpoint
 * for the same logical broker/principal/slot as the Writer.
 * Ready follows durable OPEN. Incoming deliveries use StoreInboxTake.
 * SUBSCRIBE/UNSUBSCRIBE are ordinary broker operations. The caller reconciles
 * subscription intent after broker reset. Output changes only on success. */
XMQTT_API bool xmqttClientConnectStore(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttsession* Session,const xmqttclientstoreconfig* Store,
    const xmqttpacket* Connect,xmqttclient** Client);
/* Result must be the live resolved value from a Client Publish/Disconnect
 * Future from this build. This copy remains usable after Client destruction.
 * False leaves Outcome unchanged for a volatile result or invalid output.
 * Each checkpoint reports physical Effect independently of the local network
 * result. Sequence is the committed sequence or the possible sequence for
 * MAY_BE_COMMITTED. A zero slot has no committed or uncertain effect. Epoch
 * and Key identify the protocol obligation once its attempt may be durable.
 * End has Epoch and no Key. QoS0 uses the ordinary network-only result. */
XMQTT_API bool xmqttClientStoreOutcome(const xmqttwriteresult* Result,xmqttclientstoreoutcome* Outcome);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* MQTT 5 enhanced authentication before durable BIND/OPEN. The application
 * must accept final server proof before file or Session effects. Ready also
 * waits for committed OPEN. Post-CONNACK application frames stay ordered
 * behind these gates. Proof and credentials stay outside the store log.
 * Same Session/Writer ownership and original setup deadline as ConnectStore. */
XMQTT_API bool xmqttClientConnectStoreAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttauthconfig* Auth,xmqttsession* Session,
    const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
