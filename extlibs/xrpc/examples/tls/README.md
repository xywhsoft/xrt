# Bidirectional framed TLS example

From the repository root, run with the target compiler:

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_tls --exclude-test '*' --no-single --cflag=-O2
```

Supply `--compiler <gcc> --arch native` on Windows or `--arch x64` on Linux.
The example creates one host Engine, one shared business executor and a local
native TLS pair, then calls both directions over Content-Length framing.
Calls may queue before handshake completion. Explicit Close/WaitFor/Destroy
retire both encrypted streams and callbacks before the host owners are joined.

The historical public fixture is trusted only by an explicit DER pin. This is
not a general PKIX trust policy. Applications supply their own native verifier,
identity and callback context lifetime. Early demo failures terminate the process;
embedded programs must unwind every successfully created owner.
