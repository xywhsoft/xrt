#ifndef XMQTT_CLIENT_AUTH_H
#define XMQTT_CLIENT_AUTH_H
#include <xmqtt/client.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH) && !defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "MQTT enhanced authentication requires the client"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
typedef struct xmqttauthticket xmqttauthticket;
typedef enum xmqttauthphase {
    XMQTT_AUTH_CHALLENGE=0,XMQTT_AUTH_VERIFY_CONNECT,XMQTT_AUTH_VERIFY_REAUTH
} xmqttauthphase;
typedef struct xmqttauthconfig {
    size_t Size,MaxRounds,MaxExchangeBytes;
    int64 ResponseTimeout;
} xmqttauthconfig;
typedef struct xmqttauthresult {
    xmqttclientstatus Status;size_t AcceptedBytes;
    const xmqttmessage* Final;const xerror* Cause;
} xmqttauthresult;
XRT_EXTERN_C_BEGIN
/* Defaults: 32 server challenges, 1 MiB aggregate AUTH/final-CONNACK wire bytes,
 * 5000 ms for each application decision. SetupTimeout/reauth Timeout is the
 * original whole-exchange deadline and never restarts. CONNECT uses MaxSetupBytes.
 * All limits are positive, MaxRounds <= 65536; ResponseTimeout is finite. */
XMQTT_API bool xmqttAuthConfigInit(xmqttauthconfig* Config);
/* MQTT5 only, explicit CONNECT Authentication Method required (including empty).
 * Copies config/method/CONNECT. Algorithms and credential generation belong to
 * the application. Ordinary xmqttClientConnect continues to reject enhanced auth.
 * A successful CONNACK is presented for local verification before Ready resolves.
 * Application sends and TakeMessage stay gated until accepted. Legitimate early
 * publications remain in the bounded queue; control ACKs/keepalive still progress.
 * No reconnect, implicit replay, TLS or authentication algorithm is implied. */
XMQTT_API bool xmqttClientConnectAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
/* Nonblocking mailbox using the existing Notify callback. Transfer one unique
 * ticket reference; false/unchanged when none. Move it to an application thread
 * without blocking the IO worker. One decision/response per ticket; duplicates
 * are rejected. Ticket pins Client: release every taken ticket before WaitFor/
 * Destroy. Message may be separately retained and then outlive Client/Engine.
 * Releasing without an answer leaves the finite decision timeout in effect. */
XMQTT_API bool xmqttClientTakeAuth(xmqttclient* Client,xmqttauthticket** Ticket);
XMQTT_API xmqttauthphase xmqttAuthPhase(const xmqttauthticket* Ticket);
XMQTT_API const xmqttmessage* xmqttAuthMessage(const xmqttauthticket* Ticket);
XMQTT_API void xmqttAuthRelease(xmqttauthticket* Ticket);
/* CHALLENGE only: AUTH reason 0x18 with exactly the original method. Immutable
 * encoded snapshot; ACCEPTED means queued, not transmitted. Control wire uses
 * MaxPrivateBytes independently from saturated ordinary request quota. Tickets,
 * messages/results use MaxObjects/MaxObjectBytes. OVERLOADED/CLOSED preserve
 * caller error. Resource/encoding refusals leave ticket state unchanged. An
 * expired decision returns ERROR and closes with typed TIMEOUT. */
XMQTT_API xmqttclientadmission xmqttAuthRespond(xmqttauthticket* Ticket,const xmqttpacket* Response);
/* Accept only VERIFY_* final proof; reject any phase. Schedules a local decision,
 * sends no client AUTH-success packet. Rejection closes with AUTHENTICATION.
 * A completed/stale ticket changes no state. May be called from Notify. */
XMQTT_API xmqttclientadmission xmqttAuthComplete(xmqttauthticket* Ticket,bool Accept);
/* One exchange on an already verified connection. AUTH reason 0x19, same method;
 * finite Timeout >= 0 ms covers queueing, all rounds and final application proof.
 * Other traffic continues with prior authentication; graceful Disconnect is
 * OVERLOADED while authentication is active. Cancellation/timeout/rejection closes
 * the connection; no replay. Success-only output is typed xmqttauthresult and
 * independent from Client/Engine after resolution. AcceptedBytes is the sum of
 * this exchange's AUTH prefixes accepted by TCP, not authentication success.
 * Retaining old tickets/results remains charged through actual retirement. */
XMQTT_API xmqttclientadmission xmqttClientReauthenticateAsync(xmqttclient* Client,
    const xmqttpacket* Start,int64 Timeout,xfuture** Future);
/* Same operation, rejects every worker of the host Engine before waiting. */
XMQTT_API xmqttclientadmission xmqttClientReauthenticate(xmqttclient* Client,
    const xmqttpacket* Start,int64 Timeout,xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
