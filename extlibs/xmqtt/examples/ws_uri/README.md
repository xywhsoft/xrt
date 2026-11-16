# Native MQTT URI example

Build and run the standalone URI CLI on Windows:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_uri_tls_tests
out\gcc\native\xmqtt_client_ws_uri_tls_tests\examples_ws_uri_main.exe ws://localhost:9001/mqtt 5
out\gcc\native\xmqtt_client_ws_uri_tls_tests\examples_ws_uri_main.exe wss://localhost:9443/mqtt 311 server-ca.pem
```

On Linux (the recorded local runs use Ubuntu/WSL x64):

```sh
python3 tools/build.py --compiler /usr/bin/gcc --arch x64 --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_uri_tls_tests
./out/gcc/x64/xmqtt_client_ws_uri_tls_tests/examples_ws_uri_main 'wss://localhost:9443/mqtt' 5 server-ca.pem
```

Point these commands at a running MQTT WebSocket listener. WSS requires a real
trusted CA (PEM/DER, at most64KiB here), a certificate matching the URI identity,
and the version argument before the CA filename. With the plain
`xmqtt_client_ws_uri_tests` selection, only ws:// is available. No-argument
execution prints usage only. The example checks strict HTTP101/mqtt, CONNACK,
QoS1 PUBACK, application Disconnect and physical Client/Resolver/Engine retirement.

The URI supplies HTTP Host/resource and the default TLS peer name. Empty path
maps to / rather than the separate native factory's /mqtt default. Use the
listener's actual path, preserve percent escaping and bracket IPv6 literals.
The library does not follow redirects or derive credentials from URI userinfo.

DATA proves the exchange; JOIN separately proves physical cleanup. Incomplete
clean Disconnect stays a nonzero exit, including the known pinned Mosquitto
close behavior recorded in the previous stage. No compatibility predicate is
relaxed. See [URI contracts](../../docs/client_ws_uri.md) and the source-bound
[stage report](../../../../docs/development/xmqtt-websocket-uri-20261116.json)
for actual independent peer and wrong-name certificate evidence.
