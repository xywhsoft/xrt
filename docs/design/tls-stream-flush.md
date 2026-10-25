# Explicit TLS control-record flush

`xrtTlsStreamFlush(xtlsstream*)` publishes ciphertext already queued by an
operation on the Stream's actual TLS session. It does not manufacture an empty
application write. Typical producers are `xrtTlsServerTicket*()` and the role's
`KeyUpdate()` operation; those operations must also run on the owning Worker.

The public call requires an OPEN Stream on that Worker and respects the same
ownership-claim admission barrier as other public protocol submissions. It does
not expose a new session pointer, callback, graph edge or resource owner. No
public structure layout, existing signature or time unit changes.

Results describe queue transfer, not delivery:

- `XTLS_OK`: all currently queued ciphertext entered TCP, or the TLS queue was
  already empty. It is not an acknowledgement and does not promise TCP drain.
- `XTLS_AGAIN`: ciphertext remains owned by TLS. The ordinary TCP low-water/
  drain driver will retry; do not repeat the ticket or KeyUpdate producer.
- `XTLS_CLOSED`: the transport reached a terminal close during the call.
- `XTLS_ERROR`: invalid admission/state or a transfer failure. A transfer error
  follows the existing Stream failure/close lifecycle, rather than discarding
  the queued output and pretending success.

The empty case is idempotent and allocation-free. Nonempty output uses the
existing atomic TLS-block-chain-to-TCP transfer without copying payload bytes.
Drain/Writable callbacks are deferred around the transfer using the existing
Sending/DriveAgain protocol; ordinary internal drive commands continue pending
work. This is not a synchronous network drain or a cross-thread scheduler.

`tools/test_tls_stream_flush.py` checks ordinary literal-loopback communication,
four successive full/resumed connections, ticket queue rotation, bidirectional
KeyUpdate followed by application data, allocation-free repeated empty flush,
wrong-Worker/null admission, ordinary bounded queue backpressure and cleanup.
It compiles modular and three standalone product variants with GCC O0/O2/TCC.
No external DNS/public endpoint, malformed crypto or security campaign is run.
The fixture uses public monotonic timer seconds so it can verify both native
millisecond/For and xlang's pinned microsecond/Until product without upgrading
either time ABI. Those tests do not certify xlang's public control API or its
shipping DLL/JIT/AOT integration.
