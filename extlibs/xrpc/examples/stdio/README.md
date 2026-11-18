# JSON-RPC over redirected pipes

Build this example from the repository root.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_stdio
```

The child reads framed binary stdin, echoes request parameters in its replies,
and writes framed binary stdout. Use Content-Length by default. Add the
argument be on the child command line to select UINT32_BE. The peer and child
must use the same format. Application logging belongs on stderr.

## Pipe setup and waiting

- Linux. Launch with redirected stdin and stdout and pass serve-event. The
  child uses nonblocking descriptors and poll for readiness, executor
  wakeups and deadlines. It ignores SIGPIPE within the example process.
  [Linux poll](https://man7.org/linux/man-pages/man2/poll.2.html).
- Windows. Pass serve-event after supplying two byte-mode named pipe handles
  opened with FILE_FLAG_OVERLAPPED and PIPE_WAIT as the child's standard streams. The
  child keeps one outstanding operation per direction. Completion signals
  its owner and reports the accepted bytes to xrpc. Shutdown cancels and
  joins pending operations before releasing the staging buffers and events.
  [Windows overlapped pipe I/O](https://learn.microsoft.com/en-us/windows/win32/ipc/synchronous-and-overlapped-input-and-output)
  and [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex).
- Use serve for the 1 ms polling driver, including Windows anonymous-pipe
  redirection. On Windows this mode requires permission to set PIPE_NOWAIT.
  [Windows named pipe wait modes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes).

The Windows event driver copies each read and write through fixed staging
storage. Linux reads and writes directly through the callback spans. Clean EOF
drains accepted responses for up to five seconds. Transport and framing
failures exit with status 7.

## Independent peer

The integration harness launches the child with the correct pipe handles.
Run it with the path to the built executable. Use --driver event for the
event-driven mode alone or --perf-count 2048 for a three-run comparison.

```sh
python extlibs/xrpc/tests/interop/run_stdio.py <executable> out/stdio-interop.json
```
