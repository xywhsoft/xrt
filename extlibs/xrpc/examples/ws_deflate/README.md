# Compressed WS RPC loopback

From the repository root:

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_ws_http_deflate_tests --exclude-test '*' --no-single --cflag=-O2
```

The example negotiates actual permessage-deflate and sets `CompressOutput=true`
with the default256-byte threshold. The first small call remains ordinary Text;
the repeated4096-byte body is compressed. Peer, Server and Engine physically join
before the retained typed response is released. The example prints negotiated
facts, which remain queryable after Close; codec internals stay private.

Select `xrpc_ws_http_tls_deflate_tests` and pass `1` to the resulting example for
TLS. Its legacy test identity and explicit pin are loopback fixtures. Passing a
second argument `0` disables negotiation/output compression for comparison;
this is a functional example, not a performance benchmark.

See [the compression contract](../../docs/ws_deflate.md), including allocation,
dictionary, output/decoded bounds and checked config Size/rebuild requirements.
