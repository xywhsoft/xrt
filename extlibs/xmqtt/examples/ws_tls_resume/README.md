# Trusted WSS TLS1.3 credential handoff

Run `xmqtt-wss-resume WSS_URI SERVER_CA.pem [5|311] [strict|fallback]`.
The TLS1.3 peer must issue tickets and the MQTT broker must retain this stable
ClientId/session. The example trusts only the supplied CA, snapshots each URI/
policy/CONNECT, takes the first connection's immutable native credential, and
uses it for the second connection before DNS. The caller alias is released as
soon as admission retains it. Each connection still performs fresh CONNACK and
QoS1 PUBACK. Both perform strict MQTT/WS/TLS shutdown and actual native joins.
The MQTT5 Session Expiry is60s;311 uses persistent Clean Session from the first
connection. One bounded protocol Session lives across both Client owners;
accepting broker SessionPresent without corresponding local state is an error.
TLS resumption is separate from MQTT session continuity.

`strict` (default) requires real TLS PSK acceptance on connection2. `fallback`
permits native full-handshake fallback after a peer loses its ticket key.
Identity, expiry, ALPN and saved-chain verification still apply. Ticket/PSK
contents are never printed. The URI/Resolver/trust/callback code remains alive
through Client destruction; all retained outcomes survive the actual owner joins.

```text
python tools/build.py --compiler E:/software/w64devkit/bin/gcc.exe --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_tls_resume_tests --cflag=-O2
out/gcc/native/xmqtt_client_ws_tls_resume_tests/examples_ws_tls_resume_main.exe wss://127.0.0.1:9001/mqtt SERVER_CA.pem 5 strict
```

On Linux/WSL use `--compiler /usr/bin/gcc --arch x64`; the executable is
`out/gcc/x64/xmqtt_client_ws_tls_resume_tests/examples_ws_tls_resume_main`.
No-argument invocation prints usage only. Protocol/fault/auth/recovery and
performance qualification remain separate registered evidence.
