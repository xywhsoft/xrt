# Verified TLS1.3 connection reuse

Build with the xmqtt manifest and `--suite xmqtt_client_tls_resume --no-single`.
The example's no-argument invocation prints usage. Run the resulting executable
with `HOST PORT SERVER_CA.pem` against a TLS1.3 ticket-capable MQTT peer.

It requires strict CA/name verification and ALPN `mqtt`, captures a credential
from the first connection, physically closes that client and requires actual PSK
resumption on a second connection. Both use Clean Start, fresh MQTT readiness,
QoS1 `example/resume` / `hello`, and authenticated TLS shutdown. It asserts no
MQTT persistence/recovery. It prints no credential/key bytes. Clients, Futures,
credentials, Resolver and Engine are retired on success/failure.

The independent local SSL runner supplies ephemeral public CA/server certificates
and checks the server's `session_reused` flag as well as literal MQTT bytes.
Native mutual authentication resumption is not currently supported by Core.
