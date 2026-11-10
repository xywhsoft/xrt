# Explicit Dial Session example

Select `xmqtt_client_session_dial` and `time`. Run `client_session_dial` for a
copied hostname and bounded local Resolver, or `client_session_dial --prepared`
for a refused endpoint followed by the live endpoint. The prepared list is
released immediately; hostname scratch is overwritten after factory return.

The in-process literal peer withholds PUBCOMP. Two Clients on two separately
created Engines share one passive Session/Inbox and finish with PUBREL only.
The original Future remains CLOSED. Physical Clients retire before Resolver/
Engine and passive owners. No Internet broker or credentials are needed.

This is a deterministic protocol/lifetime example, not broker interoperability,
automatic reconnect, durable storage or a production authentication algorithm.
For MQTT5 enhanced authentication, also select `xmqtt_client_auth`, supply the
CONNECT Authentication Method and use `xmqttClientDialSessionAuth` (or prepared/
numeric variants). Respond to application tickets and accept final proof using
the [existing Auth contract](../../docs/client_auth.md). Select the TLS Session
factories when transport encryption/verification is required.
