#ifndef XMQTT_RECONNECT_H
#define XMQTT_RECONNECT_H
#include <xmqtt/client_session.h>
#include <xrt/task.h>
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION) || !defined(XRT_FEATURE_TASK_POOL))
#error "MQTT reconnect requires Client Session and Task Pool"
#endif
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT)
XRT_EXTERN_C_BEGIN
typedef struct xmqttreconnect xmqttreconnect;
typedef enum xmqttreconnectstate {
    XMQTT_RECONNECT_CONNECTING=0,XMQTT_RECONNECT_ONLINE,XMQTT_RECONNECT_RETIRING,
    XMQTT_RECONNECT_WAITING,XMQTT_RECONNECT_STOPPED,XMQTT_RECONNECT_FAILED
} xmqttreconnectstate;
/* Called only on the supplied Engine/Affinity worker, outside controller locks.
 * Use a Session factory with these exact Engine, affinity, config, Session and
 * Inbox. Return OK with one unique Client; another typed status transfers none.
 * Failure classification never depends on an older ambient error. Borrowed
 * inputs last only through this call. Do not wait or retain Session/Inbox.
 * Numeric TCP, Dial, TLS and Auth factories may be selected by the application.
 * FactoryData and callback code remain alive through controller Destroy. */
typedef xmqttclientstatus (*xmqttreconnectfactory)(xnetengine* Engine,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,ptr FactoryData,xmqttclient** Client);
typedef void (*xmqttreconnectnotify)(xmqttreconnect* Controller,ptr User);
typedef struct xmqttreconnectconfig {
    size_t Size;xmqttclientconfig Client;
    size_t MaxControlBytes,MaxLeases,MaxSessionOutgoing,MaxSessionIncoming,MaxSessionBytes,MaxAssignedIdBytes;
    int64 InitialDelay,MaxDelay,StableReset,RecoveryTimeout,RetirementPoll;
    uint32 JitterPermille,MaxAttempts,RetryStatuses;uint64 RandomSeed;bool CleanStartFirstOnly;
    xmqttreconnectfactory Factory;ptr FactoryData;xmqttreconnectnotify Notify;ptr User;
} xmqttreconnectconfig;
typedef struct xmqttreconnectinfo {
    size_t Size;xmqttreconnectstate State;xmqttclientstatus LastStatus;uint8 ServerReason;
    bool StopRequested;uint64 Generation,Attempts,Connections;uint32 ConsecutiveAttempts;
    size_t Leases;double NextAttemptAt,RecoveryDeadline;
} xmqttreconnectinfo;
/* Zero-initialize before Acquire. All fields are library-owned while active;
 * Client/Generation may be read. Storage must not move/copy through Release.
 * A lease prevents physical Client destruction, not closure or Future failure. */
typedef struct xmqttclientlease {
    xmqttclient* Client;uint64 Generation;xmqttreconnect* Owner;
    struct xmqttclientlease *Previous,*Next;
} xmqttclientlease;
/* Defaults: 256KiB controller storage,256 leases,128/64 Session flows/16MiB,
 * 1024 assigned-id bytes,250..30000ms backoff,20% jitter,8 consecutive attempts,
 * 30s stable reset,60s per-outage recovery budget,1ms retirement observation,
 * CleanStartFirstOnly=true. False preserves CONNECT's clean flag on every try.
 * Only TRANSPORT/TIMEOUT/SERVER_DISCONNECT retry by default. No retry of auth,
 * protocol, local capacity or rejection failures unless explicitly selected. */
XMQTT_API bool xmqttReconnectConfigInit(xmqttreconnectconfig* Config);
/* Owns a private Session/Inbox and copied encoded CONNECT, wiped at Destroy.
 * Starts asynchronously, pins Engine and borrows the running cleanup Pool until
 * physical Destroy. No private thread or runtime is created. Config callbacks
 * may run before return. Output changes only on success. Client.Notify remains
 * the original direct data-path callback. Controller Notify is lifecycle-only.
 * All durations are milliseconds; MaxAttempts=0/RecoveryTimeout=-1 allow an
 * unbounded attempt count/time with bounded live storage. By default CONNECT's
 * clean flag is used only for attempt1, then false (including 311 Clean Session).
 * Disable CleanStartFirstOnly for always-clean/stateless retries. Assigned
 * identity is read before each try.
 * Futures belong to their original Client; only Session obligations recover.
 * No offline request queue, subscription registry or durable storage is implied. */
XMQTT_API bool xmqttReconnectCreate(xnetengine* Engine,xtaskpool* CleanupPool,uint64 Affinity,
    const xmqttreconnectconfig* Config,const xmqttpacket* Connect,xmqttreconnect** Controller);
/* Acquire only while ONLINE; normal offline/lease-cap false preserves output
 * and ambient error. Release exactly once, including after Close. No allocation
 * or wrapper Future/payload copy is added. A batch may share one lease. */
XMQTT_API bool xmqttReconnectAcquire(xmqttreconnect* Controller,xmqttclientlease* Lease);
XMQTT_API bool xmqttReconnectRelease(xmqttclientlease* Lease);
XMQTT_API bool xmqttReconnectInfo(xmqttreconnect* Controller,xmqttreconnectinfo* Info);
XMQTT_API bool xmqttReconnectTakeMessage(xmqttreconnect* Controller,xmqttmessage** Message);
/* Nonblocking stop; waits additionally include all leases, old Clients, timers,
 * posts and cleanup data finalizers. A closed/full Pool or timer admission
 * failure retains ownership for external Wait/Destroy assistance. Wait uses one
 * timeout budget and may perform already-required cleanup outside IO workers.
 * Wait/Destroy reject both Engine and cleanup-Pool workers. Stable unique caller
 * owner required; stop other API entries/borrowers before Destroy. */
XMQTT_API bool xmqttReconnectClose(xmqttreconnect* Controller);
XMQTT_API xwaitresult xmqttReconnectWaitFor(xmqttreconnect* Controller,int64 Timeout);
XMQTT_API bool xmqttReconnectDestroy(xmqttreconnect* Controller);
XRT_EXTERN_C_END
#endif
#endif
