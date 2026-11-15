# Native MQTT WebSocket connection example

Build from the repository root (Windows: add --compiler PATH/gcc.exe as needed):

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_connect_tests --no-single
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_connect_tls_tests --no-single
```

Then run the produced `examples_ws_connect_main` executable (Windows adds `.exe`):

```text
out/gcc/native/xmqtt_client_ws_connect_tests/examples_ws_connect_main localhost 9001 5
out/gcc/native/xmqtt_client_ws_connect_tls_tests/examples_ws_connect_main localhost 9443 311 server-ca.pem
```

Linux `--arch x64` builds use `out/gcc/x64`. The executable uses native DNS,
TCP/verified TLS, a fresh checked HTTP Upgrade with mqtt, the ordinary MQTT Client,
CONNACK/QoS1 PUBACK, MQTT DISCONNECT and physical WS/TLS/TCP/Engine retirement.
Supply your broker WebSocket port and a CA trust file for WSS; the WSS example
uses real X509 verification and does not install an accept-all callback. Native
TLS captures no new resumption credentials after WS handoff in this stage, so
ResumeLimit is explicitly zero. An already supplied Resume can still be used.

HTTP Host includes the port and brackets IPv6. The counted network host controls
DNS/default TLS identity. Target defaults to /mqtt. Change the copied config for
other broker routes or Origin/Authorization fields. No redirect, proxy, implicit
retry or transport downgrade is performed. Do not add credentials to source.

The no-argument build invocation only prints usage. Integration tests run this
same executable against an actual independent broker; see the stage evidence.
