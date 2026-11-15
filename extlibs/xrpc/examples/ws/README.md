# WebSocket RPC loopback

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_ws --exclude-test '*' --no-single --cflag=-O2
```

This local fixture directly attaches two already connected TCP streams with an
explicit matching protocol. It does not perform HTTP Upgrade and is not a public
endpoint deployment recipe. Validate/negotiate HTTP and credentials first. The
shared Server and Engine join separately; retained results survive.

For the encrypted fixture, build the same source with suite
`xrpc_ws_tls_tests` and its TLS identity/verification leaves, then pass `1` to
the example. Its explicit public test-certificate pin is only a local fixture.
