# Bidirectional message example

From the repository root, using one shared bounded executor and two serialized
message endpoints:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_duplex --exclude-test '*' --no-single --cflag=-O2
```

The example opts both peers into negotiated cancellation. One peer sends a
call, cancels its Future when the handler enters, and observes the other
peer's cancellation token. It then negotiates a remote deadline and checks
the error produced by the receiver's shorter incoming budget. The same
connection carries calls in both directions. The owner transports complete
immutable messages in memory,
drains both endpoints, joins the shared executor and releases the independent
results. Windows can pass a full GCC executable path and
`--arch native`. A network/stdio adapter must implement the send and close
obligations described in [duplex.md](../../docs/duplex.md); this example is not
an automatic production transport.
