# TLS DNS peer

From the repository root:

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_tls_dial --exclude-test '*' --no-single --cflag=-O2
```

Use `--compiler <gcc> --arch native` on Windows or `--arch x64` on Linux.
The local example owns one host Engine/shared business Server and bounded
Resolver. It queues RPC before OS localhost resolution and TLS verification,
calls both directions and joins peers, Resolver, Server and Engine separately.

The routing name `localhost` deliberately differs from `rpc.fixture`, the TLS
identity verified by an explicit historical public DER pin. The pin is local
test policy, not production PKIX. Applications supply actual trust/credentials
and keep borrowed verifier/resume callback code/data through peer destruction.
`xrpcTlsDialAddresses` accepts immutable prepared endpoints without a Resolver.
The example helper terminates on early failure; embedded code unwinds owners.
