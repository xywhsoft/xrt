# Copied server request context

`xrpcServerSubmitWithContext` adds an explicit immutable byte snapshot to the
existing bounded execution job. The handler reads `Context->Data` through its
callback only. Every item in one validated batch shares that snapshot; independent
documents own independent storage. Plain `xrpcServerSubmit` presents a canonical
NULL/zero view. This is application data, separate from Core ambient context;
HTTP authorization and typed transport metadata are not implemented by this API.

Input and context are copied synchronously before return and may overlap each
other; neither may overlap the Future output. The caller may overwrite/free its
source afterward. Embedded pointers are copied as bytes, not deep-copied. A
nonempty view is aligned for `max_align_t`; over-aligned types still require
explicit copying. The handler must not modify or retain the view. Returned
business bytes are independently owned under the existing result contract.

`MaxContextBytes` defaults to 16 KiB and permits up to 1 MiB. Zero disables
nonempty context. Exceeding a configured context/global budget returns OVERLOADED
without changing output/ambient error. Invalid spans/aliases return an admission
error. Existing deadlines, cancellation and whole-document validation apply.
Context construction time belongs to the request timeout. Recompile consumers
for server config/context/info layouts and nested HTTP server layouts.

The allocation remains one job block. A nonempty context adds alignment padding
and its copied bytes after the JSON input. The existing private job header and
padding/input/context all consume `MaxRequestBytesTotal`. `ContextBytes` counts
context bytes within RequestBytes, including construction reservations, until
the real final job reference retires; it excludes padding. Retained result
Futures do not hold request context or server/worker owners.

At final job retirement the library securely zeros its context byte range before
freeing the job. This adds a size-proportional wipe even on stop/constructor
failure paths. It does not erase caller copies, pointed-to objects, business
results, native buffers or dumps. There is no extra context allocation, reference
counter, user cleanup callback or shared runtime. Context bounds are inferred from the existing input/allocation
lengths, so the private job header keeps its prior size. Default paths still pay larger public/
owner layouts and additional checks/accounting; no zero-overhead claim is made.

The offline `examples/server/main.c` submits a mixed call/notification batch with
a four-byte context, overwrites its input, verifies both handler views and reads
the result after server destruction. No external service is required:

```sh
python tools/build.py --compiler gcc --arch native --manifest extlibs/xrpc/config/modules.json --suite xrpc_server --no-single
out/gcc/native/xrpc_server/examples_server_main
```

On Linux select `--arch x64` and the corresponding output directory. Tests cover
queued copied binary data, batch sharing, overlap/aliases, canonical empty views,
limits and all 1 MiB bytes, retained results, four concurrent producers, queued
Future/server cancellation, deadlines and alternating empty/nonempty context
through 2048 OOM prefixes. `tests/interop/server_context_wipe.py` additionally
checks actual production job bytes before real free, including partial
construction, without changing shipped allocators or reading freed storage.

The matched context profiles compare 0/32/4096 bytes at identical worker/window/
payload settings. Windowed throughput and serial latency are separate phases.
Process CPU begins after factory return and ends after physical server join; it
includes both phases, context erasure and shutdown, and can include native
startup-return tails. Source allocation, sort/report and later source freeing
are excluded. Wall latency/throughput exclude shutdown and can precede mechanical
return tails. Context size/edge validation and submit selection are harness
costs. These observations are not transport, saturation or stable comparator
acceptance. Original noise/native/long-run gates remain required.
