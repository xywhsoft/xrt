# Bidirectional WebSocket RPC

The developing WS leaf maps one native UTF-8 Text message to one complete
JSON-RPC document using the existing Duplex and borrowed shared Server/Engine.
TCP/TLS attach assumes an already validated HTTP Upgrade, role/subprotocol and
compression negotiation; Initial transfers exclusive owned read-ahead only on
success. The optional [HTTP Upgrade leaves](ws_http.md) add same-handle native
client/server handshake and shared-executor authorization. No new runtime,
executor, engine or reader thread.

Inputs grow geometrically under message and old+new peak bounds. Native callbacks
pause at each End or the read-yield threshold and post owner work; one completed document
is held. Accepted Duplex input/context is copied by its existing owned parser.
Receive capacity above KeepReceiveBytes is released after each completed document.
Core handles UTF-8, masks/control/interleaved fragments and negotiated compression.
The base leaf does not select compression. Binary input is explicit opt-in;
output remains Text. Message timeout starts at native MessageBegin; native host
TCP/TLS read/idle policies must bound earlier partial-header stalls.

Uncompressed sends hold actual immutable output credits through native SendRef. Unmasked TCP
servers avoid an adapter body copy; clients must mask and TLS copies in Core.
Native opaque send graphs can refuse collection until IO drains. Permanent frame
capacity is checked before Started, and current budget before each whole-message
offer. AGAIN never replays an accepted message. Notification success means native
acceptance, not business acknowledgement. Encoding and gate wait share the
original call deadline. Unknown execution after disconnection is never replayed.
Peers can opt in to [remote deadlines and cancellation](network_extensions.md).

Close aborts and composes Duplex jobs, post/timer references, native WS/TCP barriers,
actual send-release and event/Data tail. Buffered complete messages precede remote
EOF failure. Shared handlers are cooperatively cancelled, not forcefully stopped.
Native errors terminate promptly after any buffered complete document, preserving
their first cause rather than waiting for a protocol-close timeout. Native
cleanup uses bounded backoff; on timer exhaustion Poll/Wait nudges progress.
The host Engine stays pinned through successful Destroy. Shared Server/Engine/
external callers and callback code have separate joins. Result references survive.

This stage remains developing: URI/dial composition for the
HTTP factories, framed stdio, broader
fault/OOM/fuzz/TSan/performance/scale/native72h acceptance remain in the full goal.

The optional [compression leaves](ws_deflate.md) negotiate permessage-deflate,
apply explicit output thresholds and cap decoded input before RPC accumulation.
Their dictionaries, encoded output and CPU costs remain counted separately.

Normative references: [JSON-RPC 2.0](https://www.jsonrpc.org/specification) and
[RFC 6455](https://www.rfc-editor.org/rfc/rfc6455.html). JSON-RPC is transport
agnostic; this adapter explicitly defines its Text-message mapping.

The RPC wake uses the existing embedded `xrtNetPost`, not an allocated ordinary
`xrtNetEnginePost` task. Its intrusive lifecycle channel is independent of the
ordinary command queue capacity and node allocator budget. Every peer embeds
one post; the private gate coalesces pending wakes, with real per-post owner
credits through each callback. Constructor/first-call/incoming-only/Close,
original deadline, cancellation and notification progress is tested with actual
command capacity2 and port post capacity1. Embedded-post admission also succeeds
under allocation budget1; constructor, encoding, native IO and jobs retain their
existing allocation contracts. Completion needs no peer Poll/Wait nudge. Existing
Duplex/admission/native limits still apply.
Keep the host Engine and Worker running through peer retirement; this does not
promise progress after host shutdown or allow concurrent reuse of a queued node.
`xrtNetPostPending == false` is a queue observation, not a physical callback join.
