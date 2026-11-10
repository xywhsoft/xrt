# Explicit trusted TLS Session

Build `xmqtt_client_session_tls` with the normal repository tools. Run without
arguments for usage; actual execution requires a TLS MQTT5 broker and its CA:

```
xmqtt-session-tls localhost 8883 SERVER_CA.pem unique-client-id
```

Use a fresh nonempty unique id: a broker-present session without matching local
state fails closed. The example verifies the server/hostname, preserves one
in-memory Session/Inbox across two explicitly created Clients and Engines, sends
binary QoS1 then QoS2, observes SessionPresent on the second connection, and
finishes an authenticated graceful close. It does not inject interruption into
an unacknowledged flow or persist state across process restarts. Session expiry
is 60 seconds; the broker may retain subscriptions/state until it expires.
Trust files are read at runtime. No private keys or credentials are embedded.
