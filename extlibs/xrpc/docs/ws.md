# Bidirectional WebSocket RPC

The developing WS leaf maps one native UTF-8 Text message to one complete
JSON-RPC document using the existing Duplex and borrowed shared Server/Engine.
TCP/TLS attach assumes an already validated HTTP Upgrade, role/subprotocol and
compression negotiation; Initial transfers exclusive owned read-ahead only on
success. HTTP client/server handshake factories and negotiated credentials remain
separate required work. No new runtime, executor, engine or reader thread.

Inputs grow geometrically under message and old+new peak bounds. Native callbacks
pause at each End or the read-yield threshold and post owner work; one completed document
is held. Accepted Duplex input/context is copied by its existing owned parser.
Receive capacity above KeepReceiveBytes is released after each completed document.
Core handles UTF-8, masks/control/interleaved fragments and negotiated compression.
The base leaf does not select compression. Binary input is explicit opt-in;
output remains Text. Message timeout starts at native MessageBegin; native host
TCP/TLS read/idle policies must bound earlier partial-header stalls.

Sends hold actual immutable output credits through native SendRef. Unmasked TCP
servers avoid an adapter body copy; clients must mask and TLS copies in Core.
Native opaque send graphs can refuse collection until IO drains. Permanent frame
capacity is checked before Started, and current budget before each whole-message
offer. AGAIN never replays an accepted message. Notification success means native
acceptance, not business acknowledgement. Encoding and gate wait share the
original call deadline. Unknown execution after disconnection is never replayed.

Close aborts and composes Duplex jobs, post/timer references, native WS/TCP barriers,
actual send-release and event/Data tail. Buffered complete messages precede remote
EOF failure. Shared handlers are cooperatively cancelled, not forcefully stopped.
Native errors terminate promptly after any buffered complete document, preserving
their first cause rather than waiting for a protocol-close timeout. Native
cleanup uses bounded backoff; on timer exhaustion Poll/Wait nudges progress.
The host Engine stays pinned through successful Destroy. Shared Server/Engine/
external callers and callback code have separate joins. Result references survive.

This stage remains developing: HTTP Upgrade/client/server composition, full
negotiation/auth/deadline extensions, framed stdio, independent interop, broader
fault/OOM/fuzz/TSan/performance/scale/native72h acceptance remain in the full goal.

Normative references: [JSON-RPC 2.0](https://www.jsonrpc.org/specification) and
[RFC 6455](https://www.rfc-editor.org/rfc/rfc6455.html). JSON-RPC is transport
agnostic; this adapter explicitly defines its Text-message mapping.
