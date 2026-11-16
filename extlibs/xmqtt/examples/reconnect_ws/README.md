# Automatic MQTT WS/WSS reconnect

Build the native public-API example on Windows:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_reconnect_ws_tls_tests
out\gcc\native\xmqtt_reconnect_ws_tls_tests\examples_reconnect_ws_main.exe ws://localhost:9001/mqtt 5
out\gcc\native\xmqtt_reconnect_ws_tls_tests\examples_reconnect_ws_main.exe wss://localhost:9443/mqtt 311 server-ca.pem
```

On Linux (recorded local checks use Ubuntu/WSL x64):

```sh
python3 tools/build.py --compiler /usr/bin/gcc --arch x64 --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_reconnect_ws_tls_tests
./out/gcc/x64/xmqtt_reconnect_ws_tls_tests/examples_reconnect_ws_main 'wss://localhost:9443/mqtt' 5 server-ca.pem
```

A running MQTT WS/WSS listener is required. No-argument execution only prints
usage. WSS requires the version argument and a real matching CA file, at most
64KiB. The plain suite `xmqtt_reconnect_ws_tests` excludes TLS. The example uses
one host Engine, Resolver and cleanup Pool for both generations. It publishes
QoS1, closes the first leased Client to demonstrate an application-triggered
outage, releases its lease and waits for automatic generation2 with SessionPresent.
It then verifies another publication, strict graceful Disconnect and physical joins.
The CLOSED retry bit is explicitly enabled for this demonstration.

The [endpoint adapter](endpoint.h) binds each new ordinary Client to the exact
Session/Inbox supplied by the controller. Its URI, policy, Resolver and verifier
remain application-owned through controller Destroy, including future attempts.
Retaining the verifier only until Create returns would violate that contract.
The controller snapshots CONNECT; each attempt independently snapshots HTTP/URI/
TLS/config data. No wrapper payload/Future, private runtime or offline queue is added.

Persistent 3.1.1 recovery requires Clean Session=false from the first connection;
a clean session cannot recover its old obligations. This example chooses that
policy for both versions, with MQTT5 Session Expiry60. Results keep their original
Client/generation semantics. DATA and JOIN observations are separate. Incomplete
clean Disconnect remains an exit failure; the previously recorded native
Mosquitto WS-close gap is not converted to success by this example.

See [controller contracts](../../docs/reconnect.md) and the source-bound
[WS recovery report](../../../../docs/development/xmqtt-websocket-reconnect-20261116.json)
for actual local, independent gateway/broker, sanitizer, package and performance
scope. Durable recovery and full production/controlled performance remain open.
