# TLS physical retirement

From the repository root:

```sh
python tools/build.py --suite tls_stream_retire_tests --exclude-test '*' --no-single --cflag=-O2
```

The local two-worker example performs a real TLS handshake against an explicit
DER pin to the existing public test identity. It then aborts, waits for TLS and
TCP callback/reference tails, destroys both stream owners, and joins the host
Engine. It creates no background service or external connection. Use
`--compiler <gcc> --arch native` on Windows or `--arch x64` on Linux.

The fixture pin is solely for this lifecycle demonstration. Real applications
must supply their own trust/identity policy. Retained session/event contexts and
pooled buffers can survive a successful retirement query until stream Destroy;
they are not active protocol work. Keep callback code/data resident through
Destroy, and independently join the Engine and all external API callers before
unloading code. Early demo failures terminate the process; embedded programs
should unwind every successfully created owner.
