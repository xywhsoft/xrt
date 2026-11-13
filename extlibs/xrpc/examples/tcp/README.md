# Bidirectional TCP example

One host-owned two-worker Engine, one shared business executor, one native
listener and two RPC peers. Both endpoints concurrently call echo through real
loopback TCP with Content-Length framing. Each result is checked before both
peers close and physically retire. No external service or credential is needed.

From the repository root:

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_tcp --exclude-test '*' --no-single --cflag=-O2
```

Use `--compiler <gcc>` and `--arch native`/`x64` for the host toolchain. See
[the transport contract](../../docs/tcp.md) for ownership, limits, copied bytes,
EOF and deadline behavior. There is no hidden listener in the library: attach
accepted streams in their owning native worker callback. The example keeps the
Engine running until both peers are retired/destroyed, then retires the listener
and shared server before joining the Engine. Early demo failures terminate the
process; embedded applications should unwind every successfully created owner.
