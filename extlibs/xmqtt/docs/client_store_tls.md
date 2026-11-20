# Durable MQTT Client over verified TLS

Select the developing `xmqtt_client_store_tls` module. Its public factories
accept a numeric address, a hostname with a resolver, or a prepared address
list. TLS verification, protocol selection and native limits follow the
[Client TLS contract](client_tls.md). The Session, semantic Writer and
trusted wall clock follow the [durable TCP contract](client_store.md).

The verified handshake precedes MQTT CONNECT. The original setup deadline
covers address resolution, transport setup, verification, CONNACK and the
durable OPEN. A failed certificate verification leaves the journal untouched.
With an accepted CONNACK, Ready waits for OPEN even if the TLS session is
already established.

A successful graceful Disconnect observes authenticated TLS closure and the
committed END. A truncated TLS connection records an abnormal END and keeps
the original session expiry. If a complete MQTT DISCONNECT changed the expiry
and the TLS transport was truncated, recovery sees the live-at-cut prefix
and decides with authoritative broker evidence. The result exposes native
plaintext acceptance and durable outcomes separately through
`xmqttClientStoreOutcome`.

The MQTT journal holds protocol state. The TLS context, verifier, credentials,
session tickets and endpoints stay with the TLS configuration and application.
Use one Writer with each logical broker, principal and Client slot.

Build and run the modular fixture, single-header fixture and example.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_store_tls_tests
```

[Run against a TLS broker](../examples/client_store/README.md#verified-tls).

With the MQTT enhanced-auth module selected, the Auth variants require the
application to verify final broker proof before durable OPEN. See
[durable enhanced authentication](client_store_auth.md).
