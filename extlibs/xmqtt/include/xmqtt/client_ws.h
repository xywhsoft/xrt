#ifndef XMQTT_CLIENT_WS_H
#define XMQTT_CLIENT_WS_H
#include <xmqtt/client.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS)
#include <xrt/websocket_stream.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_WRITE_PROGRESS) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_RETIRE))
#error "MQTT WebSocket requires Client, native WS write prefix and retirement"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_WS) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS))
#error "MQTT WSS requires WS and native WSS write progress"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS_RESUME) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_TICKET))
#error "MQTT WSS resume requires native WSS ticket observation and existing TLS cache"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS)
struct xmqttclientinbox;struct xmqttsession;struct xmqttauthconfig;
typedef struct xmqttclientwsconfig {
    size_t Size;xwsstreamconfig Stream;
    size_t MaxStagedBytes,KeepStagedBytes;
    struct xmqttclientinbox* Inbox;struct xmqttsession* Session;
    const struct xmqttauthconfig* Auth;
} xmqttclientwsconfig;
#define XMQTT_WS_STAGED_MAX 16777216u
XRT_EXTERN_C_BEGIN
/* Native client role and exact mqtt subprotocol;1 MiB frame/message/staging caps,
 *64 KiB staging retention. No compression. Optional Inbox/Session/Auth pointers
 * require their existing feature selections; Session also requires Inbox.
 * Public layout is selection-independent. No additional MQTT runtime. */
XMQTT_API bool xmqttClientWsConfigInit(xmqttclientwsconfig* Config);
/* Owning native Worker only, OPEN transport, exclusive handoff. The host must
 * already have sent/validated HTTP Upgrade offering/selecting mqtt, then supply
 * its exact negotiated Stream policy. Config Protocol must be mqtt and Role client;
 * this attachment API does not perform/attest a network handshake itself.
 * Initial may be an exclusive owned HTTP read-ahead buffer. Success consumes
 * Native and Initial exactly as native WS attachment; false consumes neither
 * and leaves Client unchanged. Snapshots CONNECT/config/auth before return.
 * SetupTimeout starts at this API entry; earlier host setup has its own budget.
 * Existing Client calls/subscriptions/QoS/aliases/auth/Session use the same handle.
 * Byte parsing spans WS frames/messages; only Binary is accepted. One direct
 * copy into normal packet input, bounded charged staging only for unread suffix.
 * Native masking/TLS/result copies and per-frame native allocations remain real.
 * QoS0 uses saved native prefixes; early ACKs keep real request credits through
 * physical completion. Close/Wait/Destroy join WS/TLS/TCP duties and borrowed
 * native aliases can delay them. Stop external API entries before final Destroy.
 * Optional trust/auth callbacks and shared objects follow their native lifetimes.
 * URI/DNS/HTTP setup factories and durable state are separate product work. */
XMQTT_API bool xmqttClientWsAttachTcp(xnetstream* Native,xnetbuf* Initial,
    const xmqttclientconfig*,const xmqttclientwsconfig*,const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
/* Ownership-certified (OwnedV1 factory), verified OPEN client TLS is required;
 * no prior additional event binding, active callback/drive or pending read.
 * Original TLS configuration owner stays retained; no cleartext fallback. */
XMQTT_API bool xmqttClientWsAttachTls(xtlsstream* Native,xnetbuf* Initial,
    const xmqttclientconfig*,const xmqttclientwsconfig*,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
