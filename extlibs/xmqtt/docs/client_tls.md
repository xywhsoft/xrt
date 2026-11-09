# TLS client (developing)

Select `XMQTT_MODULE_XMQTT_CLIENT_TLS` for numeric, hostname and prepared-address
factories. The existing client, QoS, subscription, alias and enhanced MQTT5 auth
operations run over the same bounded protocol owner. TLS uses the public native
Stream/session implementation and its verified ciphertext-prefix progress leaf.

Initialize `xmqttclienttlsconfig`, supply a native Verifier and explicit peer
identity (or infer the counted hostname), then call a `xmqttClientTls*` factory.
All views and native shared references are captured before DNS. Optional
`ClientAuth` uses native mutual authentication; its native no-resumption limitation
applies. Native callback/signing contexts follow their shared-object lifetimes.

The original setup deadline covers all setup phases. TLS is required and there
is no cleartext fallback or automatic replay. Ready includes MQTT CONNACK and
the configured enhanced-auth proof gates. Graceful Disconnect includes native
authenticated TLS close within its original deadline.

Unload library/provider code only after actual Engine join and release of every
retained Future, result, message and callback context that can still invoke that
code. A retained protocol result may outlive Client/Engine; Client retirement
does not retire an application's independent result reference.

AcceptedBytes counts MQTT plaintext accepted by TLS. Its delivery classification
is conservative. QoS0 observes the saved native ciphertext prefix, without waiting
for later output. ACK completion and physical request/id retirement are separate.
One aligned request fence tail uses MaxPrivateBytes; each chunk adds no watcher.
TLS session/ciphertext limits are native bounded policies outside this local cap.

Measured Windows/WSL x64 layouts: existing public ClientConfig272/Info160 and
private request header104 bytes stay fixed; TLS config200, TLS-only state88 and
request fence24 plus alignment padding. Selecting TLS adds one private Client
pointer (8 bytes); selecting its Dial dependency adds another8 if absent before.
The adapter builds up to three stack spans per gathered send, snapshots Produced
after accepted writes and samples Completed on pending owner turns. Encryption,
native buffers, refcounts and gated notifications remain real work.

Run the [TLS example](../examples/tls/README.md) with a CA file and optional
client identity. It performs real verified QoS1 traffic and graceful shutdown.
The independent [SSL runner](../tests/interop/run_mqtt_tls.py) creates temporary
certificates and tests TLS1.2/1.3, MQTT5/311, all three factories, strict name/trust
refusals, required client certificates and authenticated shutdown. It uses literal
MQTT bytes rather than the product codec. The native fixture additionally covers
partial sends, earlier-prefix completion with later output pending, aliases,
subscriptions, enhanced authentication, cancellation, blocked DNS, deadlines and
constructor/Worker allocation failures. Modular, single-header and NET_STATS_LEVEL0
variants exercise the same ownership contract.

Performance profiles select `xmqtt_client_tls`. Its closed-loop local QoS1
benchmark includes native client/server record crypto and a callback trust fixture;
handshake is outside the message timer. Report throughput and P50/P99 together.
It does not establish broker parity, fixed-arrival performance or production
acceptance. TLS resumption can consume the supplied native Session.Resume;
optional [credential handoff](client_tls_resume.md) captures one latest TLS1.3
ticket for a subsequent connection. Cross-connection cache management and
reconnect integration remain pending. Native signing backend
selection, certificate policy and trust-store lifetime remain explicit choices.
The product stays developing; pinned brokers, native platform stress and the
original production/performance release gates remain required.

Stage receipts and measured limitations: [TLS validation](../tests/results/client-tls-20261109.json).
