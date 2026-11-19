#ifndef XMQTT_EXAMPLE_RECONNECT_OPTIONS_H
#define XMQTT_EXAMPLE_RECONNECT_OPTIONS_H
#include <xmqtt/reconnect_subscriptions.h>
#include <xmqtt/reconnect_store.h>

/* APP convenience wrappers share one library constructor. A real APP can
 * expose one or both policies through its own setup interface. */
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
static inline bool mqttAppReconnectSubscribed(xnetengine* Engine, xtaskpool* Pool,
    uint64 Affinity, const xmqttreconnectconfig* Config, const xmqttsubscriptionsconfig* Subscriptions,
    const xmqttpacket* Connect, const xmqttpacket* Initial, size_t Count, xmqttreconnect** Output)
{
    if (!Config) return false;
    xmqttreconnectconfig Options = *Config;
    xmqttreconnectsubscriptionplan Plan = {Subscriptions, Initial, Count};
    Options.Subscriptions = &Plan;
    return xmqttReconnectCreate(Engine, Pool, Affinity, &Options, Connect, Output);
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_STORE)
static inline bool mqttAppReconnectStored(xnetengine* Engine, xtaskpool* Pool,
    uint64 Affinity, const xmqttreconnectconfig* Config, xmqttsession* Session,
    const xmqttclientstoreconfig* Store, xmqttreconnectstorefactory Factory,
    const xmqttpacket* Connect, xmqttreconnect** Output)
{
    if (!Config) return false;
    xmqttreconnectconfig Options = *Config;
    xmqttreconnectstoreplan Plan = {Session, Store, Factory};
    Options.Store = &Plan;
    return xmqttReconnectCreate(Engine, Pool, Affinity, &Options, Connect, Output);
}
#endif
#endif
