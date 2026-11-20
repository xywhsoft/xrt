# Durable MQTT over WebSocket

Select `xmqtt_client_store_ws` for ws://. Add `xmqtt_client_ws_uri_tls` for wss://.
URI parsing, strict HTTP Upgrade, message framing and verification follow the
[ordinary URI factories](client_ws_uri.md). The matching durable Session and
semantic Writer follow the [Client Store contract](client_store.md).

HTTP failure leaves the journal untouched. Ready waits for committed OPEN after
accepted MQTT CONNACK. If `WebSocket.Auth` is configured, the application's
final proof decision also precedes OPEN. Coalesced publications retain their
wire order while the gates are pending.

Graceful Disconnect waits for the WebSocket closing handshake and native
transport closure. WSS also awaits authenticated TLS closure. END then records
the effective MQTT session expiry. If a complete expiry-changing DISCONNECT
may have reached the broker before truncation, recovery receives a live-at-cut
prefix and resolves it with broker evidence. The write result separates local
transport observations from journal effects.

Typical integration builds select the plain WebSocket adapter separately and
add the TLS URI feature when needed. The native TLS policy controls identity,
client credentials and ticket limits. Set `Session.ResumeLimit` to zero when
the existing WSS ticket capture module is absent.

The [example](../examples/client_store/README.md#websocket) publishes QoS 1
over a durable URI connection with a temporary native journal. The fixture
exercises strict Upgrade, OPEN, a coalesced Inbox delivery, PUBACK sequencing,
END and abrupt closure on both MQTT versions. See
[RFC 6455 Section 7.1](https://www.rfc-editor.org/rfc/rfc6455.html#section-7.1).
