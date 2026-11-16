# Journal example

Build with `python tools/build.py --manifest extlibs/xmqtt/config/modules.json
--suite xmqtt_journal_tests`. The build's no-argument smoke prints usage.
Run the resulting `example` for `examples/journal/main.c` with an **existing**,
exclusively owned file: empty initializes, nonempty replays before appending.
Use a storage worker; the example blocks for native sync.

Prepare/publish the file durably using the host's filesystem policy before
relying on crash persistence. Opening/closing/creating a pathname alone does
not establish parent-directory power-loss durability. The journal borrows the
file, never acquires its lock or owns its namespace, and leaves closing to this
example. Preserve generation/binding and treat uncertain append as a recovery
boundary. The opaque demonstration text is not MQTT protocol/Inbox state.

See [journal contract](../../docs/journal.md) for framing, bounded recovery,
durability assumptions and remaining full-store work.
