# Optional WS RPC compression

Select `xrpc_ws_deflate` for a validated WS attach, or
`xrpc_ws_http_deflate` for HTTP negotiation. Combine the latter with
`xrpc_ws_http_tls` for WSS. The leaves reuse Core RFC7692 negotiation and
inflater/deflater objects; they remain developing pending full validation.

Single headers use `XRPC_MODULE_XRPC_WS_DEFLATE` or
`XRPC_MODULE_XRPC_WS_HTTP_DEFLATE`. The base leaves do not select compression.

`xrpcwsconfig.CompressOutput` defaults false and `CompressMinBytes` defaults256.
When explicitly enabled, negotiated peers compress Text documents at or above
that threshold. Smaller documents and declined negotiation retain the ordinary
reference-send path. These two unconditional config fields change checked Size;
rebuild consumers. Layout is independent of feature selection.

The new boolean occupies former config padding and the size_t is appended;
existing fields keep their offsets on the measured x64 targets. This remains a
checked-Size/ABI change, including ConfigInit storage: rebuild all linked users
and use headers and libraries from the same revision.

`xrpcWsDeflateInfo` copies `xrpcwsdeflateinfo` containing actual `Enabled`,
`Negotiated` direction/window facts and the output policy. Before WS handoff it
returns STATE and leaves output unchanged. It never exposes codec pointers.

Use existing HTTP `Client` offer/require and `Server` enable/require/AcceptDeflate
policies. The latter is a bounded protocol selection callback on the IO Worker;
it must be short and nonblocking. Its borrowed code/data lives through physical
peer retirement, including refusal/timeout. Business authorization remains on
the separate shared Server executor. Defaults do not offer/accept compression.

Decoded bytes are capped by the smaller native Inflater.OutputLimit and RPC
MaxMessageBytes. Frame and aggregate receive limits remain effective separately.
Output compression borrows the owned Duplex document only during the synchronous
Core transformation. Core stores encoded output under its native send budget;
uncompressed/TLS/client masking retain their existing copy/ownership costs.
Started is still the first send offer, even for a refused offer; AGAIN never
replays an accepted document. Context discard after a refused compression offer
may reduce compression ratio but does not introduce a dependency on unsent data.

Codec dictionaries, transformations, temporary encoded output and their CPU/
allocation costs are not zero overhead. Compression is optional; throughput,
tail latency, memory and wire-byte tradeoffs still require measured acceptance.

Measured Windows/WSL Linux x64 config Size changes520 to528, with existing fields'
offsets preserved and the boolean in former padding. The readiness marker fits
private RPC padding; selecting compression adds16 bytes to native WS objects
before any separately allocated codec/dictionary/encoded-output storage.

Validation includes actual TCP/TLS client/server peers, text fragments and
interleaved controls, optional decline/requirement and invalid negotiation,
thresholded output, coalesced first frames, bounded expansion failure without
business dispatch, context/no-context and every8..15-bit window. Independent
Python zlib checks actual RSV1 and decoded bytes and observes dictionary reuse.
For8-bit input its9-bit encoder uses Huffman-only (no distance references), a
valid8-bit stream; native8-bit output is decoded independently. Real original
budget1 at the native compression offer verifies MEMORY, conservative Started
delivery and physical rollback; no IO result is mocked. Broader async allocation
failure ladders and fuzz/scale/endurance qualification remain required.

The strong coalesced101/valid compressed-document fixture also found an HTTP
adapter bug: mutable Pullup on a borrowed TCP buffer fails when IOCP has an
active receive reservation. TCP head parsing now uses a read-only contiguous
span or the existing bounded cold cache. No Core API or receive queue is added.

Normative definition: [RFC7692](https://www.rfc-editor.org/rfc/rfc7692.html).
