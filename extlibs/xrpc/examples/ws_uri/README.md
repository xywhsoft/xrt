# URI client example

`python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_ws_uri_tests`
builds/runs the loopback example. The numeric ws URI needs no Resolver. One Engine
and Server are shared, a call queues during native setup/checked Upgrade, and
peer/listener/Server/Engine duties are explicitly joined. It is not a benchmark.

For WSS, select xrpc_ws_uri_tls and call xrpcWsConnectUriTls with a native runtime
trust policy. Empty verification identity derives from URI host; SNI is omitted
for an IP literal. See [the full URI guide](../../docs/ws_uri.md) for encoding,
resolver/identity policies, limits and lifetime. No production key is included.
