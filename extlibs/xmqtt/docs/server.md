# MQTT server

Select `XMQTT_MODULE_XMQTT_SERVER` for the server-only build. Select
`XMQTT_MODULE_XMQTT_CODEC311` and set `Allow311` for legacy peers. The APP owns
its XRT listener. Return `xmqttServerAttach` from its Accept callback to
transfer the accepted stream. Keep the listener's receive limit at least as
large as the MQTT packet cap. Choose appropriate XRT accept, read and write
limits for the aggregate connection budget.

The current TCP stage implements version selection, bounded CONNECT admission,
CONNACK, one active binding per Client ID, keep alive, PING and DISCONNECT.
The APP can inspect borrowed CONNECT credentials in a nonblocking callback.
By default the server allows anonymous connections. Run the example on
loopback and provide an authorizer before enabling external access. A peer
supplies a nonempty Client ID and a connection-scoped session. MQTT 5 CONNACK
advertises the currently available QoS and topic capabilities. QoS publication,
subscriptions, persistent sessions, Wills, enhanced authentication, TLS and
WebSocket are subsequent broker stages.

Each peer stays on its original XRT worker. The handshake and connection
retirement update the bounded identity index under a short gate. The steady
PING path uses the worker-local state, a validated fixed header and XRT's
bounded send queue. A contiguous native receive span can be parsed directly;
XRT coalesces a fragmented complete control packet when needed. One queued
timer per connection observes updated deadlines. Close seals admission and
requests stream aborts. The connection quota is released after the native
event owner and its pending timer have retired. Join listener callbacks and
then join the broker outside its workers before destruction.

Run the loopback example as `examples_server_main wait`. It writes a numeric
endpoint, then accepts test peers until a line arrives on stdin. Both the
MQTT 5-only and dual-version module configurations have standalone tests.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server_dual_tests
python tools/measure_performance.py --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_server
```

The PING benchmark verifies each literal response and reports connection rate,
round-trip throughput and latency with 1, 64 and 1024 TCP clients. Each round
queues a PING on each socket before collecting replies. Numbers include local
peer-side collection and memory instrumentation.
The protocol baselines are [MQTT 5.0](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).
