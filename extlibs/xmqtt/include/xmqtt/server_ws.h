#ifndef XMQTT_SERVER_WS_H
#define XMQTT_SERVER_WS_H
#include <xmqtt/server.h>
#include <xrt/websocket_stream.h>

#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
#if !defined(XMQTT_FEATURE_XMQTT_SERVER) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_REF) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_RETIRE)
#error "MQTT WebSocket broker requires the MQTT broker and XRT WebSocket reference send and retirement"
#endif
XRT_EXTERN_C_BEGIN

/* Call on the owning Worker after validating the HTTP Upgrade and queuing
 * its response. Prefix is the HTTP header length still in the native input
 * buffer, or zero if already consumed. The broker owns the received transport
 * credit on success. Preserve no extra transport aliases after a successful
 * handoff. Config selects server role and the mqtt subprotocol, with frame
 * and message limits at least Codec.MaxPacketBytes. Extensions are disabled.
 * The MQTT CONNECT deadline starts at handoff. */
XMQTT_API bool xmqttServerAttachWs(xmqttserver* Server, xnetstream* Stream,
	size_t Prefix, const xwsstreamconfig* Config);

#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS_TLS)
#if !defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS) || !defined(XRT_FEATURE_TLS_STREAM_RETIRE)
#error "MQTT WSS broker requires XRT WebSocket TLS and TLS physical retirement"
#endif
/* Same handoff after TLS Upgrade. The OPEN TLS event binding must be
 * replaceable by xrtWsStreamAttachTls. */
XMQTT_API bool xmqttServerAttachWss(xmqttserver* Server, xtlsstream* Stream,
	size_t Prefix, const xwsstreamconfig* Config);
#endif

XRT_EXTERN_C_END
#endif
#endif
