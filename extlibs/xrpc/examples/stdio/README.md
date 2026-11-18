# JSON-RPC over redirected pipes

Build this example from the repository root.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_stdio
```

Launch the resulting examples_stdio_main program with the argument serve.
It reads framed binary stdin and writes framed binary stdout. Request parameters
are echoed as the result. A single-process diagnostic invocation without
arguments prints usage. Use serve be to select the four-byte big-endian format.
Use the same explicit format on both ends.

The executable uses nonblocking handles and a 1 ms idle poll as a portable
example driver. It expects Windows anonymous pipes that allow changing wait
mode, or POSIX redirected descriptors. Readiness-driven hosts can replace
the example polling with their own event loop and the stdio callback API.
A clean EOF drains responses before the program exits. Application logging
belongs on stderr.

The Windows example uses byte-mode nonblocking pipe operations. Event-driven
Windows hosts can supply a separately owned overlapped I/O implementation.
[Windows named pipe wait modes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes).
