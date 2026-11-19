# xrpc coverage

The codec, dispatch, methods, IDs, pending table, response snapshots and
native call results retain their modular and single-header regressions.
The endpoint suite exercises deferred APP replies, shape and ID validation,
notification suppression, batch backpressure, typed deadlines and stops,
allocation prefixes, concurrent producers and lifetime on close.

Run `xrpc_examples` for APP examples. An independent Python process drives
the stdio sample through complete JSON-RPC records. The duplex size profile
checks the absence of network, executor and process dependencies. The
host-driven duplex workload is an in-memory benchmark.

Historical transport, executor and private extension coverage remains
recorded at its original Git revisions and in the historical result JSON.
