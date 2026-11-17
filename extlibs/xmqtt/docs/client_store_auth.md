# Durable MQTT 5 enhanced authentication

Select `xmqtt_client_auth` together with `xmqtt_client_store` for numeric TCP
or `xmqtt_client_store_tls` for verified TLS. Use `xmqttClientConnectStoreAuth`
or the numeric, hostname and prepared-address TLS StoreAuth factories.
The caller supplies the mutually agreed authentication algorithm and verifies
the server's final proof through the [existing ticket API](client_auth.md).
TLS peer verification is a separate prerequisite for the TLS factories.

A successful MQTT CONNACK produces an application proof ticket. The Session
and Writer keep their previous protocol state until the application accepts
that proof. Acceptance schedules BIND/OPEN under the original setup deadline.
Ready and protocol replay wait for committed OPEN. Rejection, malformed
authentication, cancellation or a decision timeout before acceptance leave the
journal unchanged. Cancellation after admission follows the ordinary storage
effect contract. Application frames stay in order behind proof and OPEN.
A PINGRESP or DISCONNECT at the head of input can progress during those gates.
A preceding application frame can delay the control response within the
existing input and timeout bounds. Before CONNACK the enhanced-auth path allows
AUTH and DISCONNECT only. [MQTT 5.0 sections 3.1.2.10 and 4.12](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

Reauthentication runs within the current durable epoch. It creates no store
transaction on its own. Application traffic uses the previous authentication
until the reauth verdict. Failure closes the connection under the existing
transport/END rules. The journal stores protocol identity and state. CONNECT
credentials and AUTH proofs stay in the bounded in-memory handshake and result
owners.

The [local example](../examples/client_store_auth/README.md) uses a temporary
journal and fixture tokens. Cryptographic mechanisms and external broker
enhanced-auth interoperability are separate acceptance work.
