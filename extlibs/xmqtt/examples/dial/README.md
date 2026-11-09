# MQTT hostname/address Dial example

From the repository root:

```text
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_dial --no-single
```

The self-contained local peer and custom asynchronous Resolver return a refused
IPv6 candidate followed by a healthy IPv4 endpoint. Core performs candidate
selection; MQTT CONNECT/CONNACK and QoS1 then use the existing client. All clients,
resolver work, candidates, sockets and Engine resources are physically retired.
No broker, DNS server, credentials or external process is required. The fixture
is not a broker/algorithm interoperability or production-load comparison.

Expected output includes `MQTT5 Dial: bounded dual-stack DNS, QoS1 and physical
retirement verified`. See [contracts](../../docs/client_dial.md).
