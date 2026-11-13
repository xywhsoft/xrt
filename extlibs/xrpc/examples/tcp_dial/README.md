# Asynchronous DNS TCP peer

From the repository root:

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_tcp_dial --exclude-test '*' --no-single --cflag=-O2
```

Use `--compiler <gcc> --arch native` on Windows or `--arch x64` on Linux.
The local example creates a host Engine/shared business Server and bounded
Resolver, queues a call before OS localhost resolution completes, calls back in
the reverse direction and joins both peers, Resolver, Server and Engine.
Each prepared endpoint keeps its own port; `xrpcTcpDialAddresses` requires no
Resolver. Neither factory replays RPC or chooses TLS implicitly.

The common repository fixture terminates on early failure. Embedded programs
must unwind each successful owner and keep native callback code/data resident.
