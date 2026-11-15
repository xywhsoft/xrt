# HTTP Upgrade RPC loopback

Build and run the local TCP example from the repository root:

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_ws_http_tests --exclude-test '*' --no-single --cflag=-O2
```

For TLS, select `xrpc_ws_http_tls_tests` and run the resulting `main` executable
with argument `1`. Both endpoints use actual HTTP GET/101, a random client nonce
and the required `jsonrpc` subprotocol before a queued bidirectional RPC call.
The TLS path posts attachment outside the native Open driver. Its checked-in
legacy test identity and explicit certificate pin are local fixtures; a deployed
host supplies its own identity, verification and HTTP authorization policy.

The example borrows the existing Engine/shared Server. It joins peers, listener,
Server and Engine before releasing the retained typed result. See
[the Upgrade guide](../../docs/ws_http.md) for auth, deadlines, bounds and scope.
