# Bidirectional message example

From the repository root, using one shared bounded executor and two serialized
message endpoints:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_duplex --exclude-test '*' --no-single --cflag=-O2
```

The runnable example submits calls in both directions before pumping either
endpoint, transports complete immutable messages in memory, checks both typed
results, drains endpoint duties, joins the shared executor and then drops the
independent results. Windows can pass a full GCC executable path and
`--arch native`. A network/stdio adapter must implement the send and close
obligations described in [duplex.md](../../docs/duplex.md); this example is not
an automatic production transport.
