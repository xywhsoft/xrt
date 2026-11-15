# Native WS/WSS dial example

`python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_ws_dial_tls_tests`
builds/runs this loopback example. Run its executable with0 for plain TCP,1 for
TLS. The host shares one Engine and Server, starts a listener, creates the client
directly from its numeric endpoint and queues an echo call before setup finishes.
The same handle proceeds through TCP/TLS and checked HTTP Upgrade. A retained
result survives explicit peer/native/listener/Server/Engine joins.

TLS uses the repository's local certificate fixture and explicit DER pin solely
for this loopback demonstration. A deployed host supplies its own runtime trust
policy, identity and callback lifetime. DNS and prepared-address variants are
documented in [the guide](../../docs/ws_dial.md). No production keys or benchmark
acceptance are supplied by this example.
