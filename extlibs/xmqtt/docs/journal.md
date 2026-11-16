# MQTT transactional journal

Status: **developing**. This is the synchronous, bounded transaction envelope
for the future MQTT store. Payload bytes are opaque and tagged with a required
application schema. It does not persist a live Client, decode MQTT deltas, commit
Inbox deliveries, order broker ACKs, compact, group commits or publish filenames.
Those remain in the [full store plan](../../../docs/development/xmqtt-store-design.md).

## API and ownership

`xmqttJournalConfigInit` defaults to MQTT5, schema1, a1MiB maximum transaction,
2MiB maximum resident allocation and1GiB maximum physical file. Supply exact
1..1024 binary binding bytes for broker/security principal/logical slot and a
nonzero storage generation. MQTT3.1.1 is supported. Binding/version/generation/
schema must match on reopen; this component never guesses a new identity.

`xmqttJournalCreate` initializes a zero-length borrowed `xfile` and synchronizes
before success. A failed initialization may leave partial bytes. Caller owns
cleanup and closing. `xmqttJournalOpen` checks the immutable descriptor and
redundant commit witnesses, then returns RECOVERING. It does not claim all
committed data has been validated. Both constructors leave their output untouched
on failure and make exactly one bounded allocation on success:
`sizeof(private journal) +4096 + MaxTransactionBytes + Binding.Size`.
There is no file-sized allocation or hot growing index.

File capabilities must include absolute read/write, stat, resize and flush;
APPEND and ASYNC modes are refused. Keep file contents exclusively owned and
serialized through `xmqttJournalDestroy`, then close the file. No lock acquisition,
process alias detection, native platform escape or general shared runtime is
hidden here. A virtual provider must implement the same I/O/sync semantics as
the host requires; an in-memory VFS does not become durable through its name.

`xmqttJournalNext` validates at most one transaction per call and returns an
immutable borrow until **any** next journal operation or Destroy. The buffer is
reused; copy/apply to separately bounded staged recovery state when necessary.
Append advances past its own newly committed record; Next is the reopen recovery
cursor, not a tailing reader. Earlier validated records can have been returned
before a later corrupt record.
Publish restored application state only after EMPTY. EMPTY verifies final
sequence/chain, retires a proven uncommitted suffix with resize+flush, and admits
Append. LIMIT preserves cursor and output; destroy/reopen with adequate bounds.

`xmqttJournalAppend` borrows stable immutable disjoint payload during a blocking
storage-worker call. Never call it inside a network callback. This operation
does not acknowledge MQTT packets. DONE changes the caller sequence only after:

1. Full absolute writes of header, payload and footer.
2. Data flush to establish the transaction body before publication.
3. Full writes of **both** latest commit-witness pages.
4. Publication flush, then cached state/output advancement.

It performs no heap allocation or extra payload copy on success. Cost for a
nonempty payload is two payload CRC scans, three frame writes, two4096-byte
witness writes and two native flushes. Physical syscalls can exceed these logical
operations after short I/O. Kernel/CRT copies and metadata/device effects are
additional. This baseline deliberately exposes sync cost; group-commit policy
and end-to-end throughput/tail acceptance remain future work.

Before the first witness write an I/O failure returns IO/NOT_COMMITTED. From the
first witness write onward it returns UNCERTAIN/MAY_BE_COMMITTED. Both poison the
owner; never retry through that object. Reopen/replay to resolve actual local
history. An unacknowledged transaction can be recovered if publication happened
before caller success. `xmqttJournalInfo` reports cached committed sequence/end,
attempted sequence/end, effect, state, memory and chain without I/O. It is not a
durability check. Destroy frees the allocation and never closes/syncs the file.

One serialized caller owns the object. Synchronous backend callback reentry is
rejected for Next/Append/Info/Destroy; concurrent calls remain invalid. Keep
backend code/file alive through the call. All output/input/library spans must be
disjoint. DONE/EMPTY/LIMIT preserve ambient errors. Failure diagnostics may
allocate; false-with-no-error backend failures cannot attach stale caller errors.

## Private version1 format

All integers are unsigned big-endian, with CRC-32/ISO-HDLC (zlib/ZIP), finalized
state. Checksums detect accidental corruption; they do not authenticate storage
or prevent rollback, malicious recomputation or wrong credentials. Protect the
directory and bind trusted generation/schema externally.

| Region | Layout |
| --- | --- |
|Descriptor4096 at0|magic `XMQWAL\0\0` at0; format u16=1 at8; MQTT level at10; zero at11; binding length u32 at12; generation u64 at16; data start u64=12288 at24; schema u32 at32; zero36..63; binding at64 then zeros; CRC u32 at4092 over0..4091.|
|Witness4096 at4096 and8192|magic `XMQWCOM\0`; generation u64 at8; sequence u64 at16; committed end u64 at24; last frame CRC u32 at32; zero36..4091; page CRC at4092. Empty is sequence0/end12288/chain0.|
|Frame header48|magic `XMQWTX1\0`; total bytes u64 at8; sequence u64 at16; payload length u64 at24; previous frame CRC u32 at32; payload CRC u32 at36; zero u32 at40; header CRC u32 at44 over first44.|
|Frame body|Opaque payload immediately after header; empty payload is valid.|
|Frame footer16|magic `XMQWEND\0`; frame CRC u32 at8 over complete48-byte header then payload; zero u32 at12. Total frame bytes=64+payload.|

Witnesses are **both** updated for every acknowledged commit. One damaged page
then leaves another copy of the same latest acknowledged history. This differs
from alternating a single latest witness, which could silently roll back an
acknowledged transaction after damage. Two valid unequal witnesses must be
adjacent sequences with strictly increasing ends; this represents publication
in flight. Equal-sequence witnesses must have identical ends/chains. Both invalid
is fatal. Select the highest valid sequence **before** checking its committed
prefix. Missing/corrupt committed data never falls back to an older witness.

Replay verifies bounds, frame sequence/previous chain, CRCs, reserved fields and
the chosen witness's final end/sequence/chain. Never scan for a new magic after
damage. File/transaction/memory limits can refuse work before large reads or
allocations. Incomplete data beyond the chosen end is uncommitted; only after
the committed prefix is validated may it be truncated and synchronized.

## Durability boundary and evidence

Native XRT uses Windows `FlushFileBuffers` and POSIX `fsync` for this path.
[Microsoft documents the file-buffer flush](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers).
[Linux documents separate directory synchronization](https://man7.org/linux/man-pages/man2/fsync.2.html):
file synchronization alone does not necessarily persist the containing entry.
This journal requires an **already durably published** file. XRT anchored
creation/rename currently does not expose that whole publication contract;
the full store still must solve it, including compaction publication. No Core
change is justified for ordinary append/read/resize/flush: public file/VFS
operations passed the prototype and fault tests.

The model assumes a successful sync establishes the intended file bytes, writes
do not damage unrelated prior bytes, file contents have a unique owner and the
device/filesystem honor their reported barriers. Checksum collisions, correlated
damage of both witnesses, hostile rollback, media failure, block tearing outside
the written range and namespace loss remain explicit limits. Process kill tests
check loss of application state, not physical power loss or arbitrary device
cache behavior. WSL is not native Linux production qualification.

Run `python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite
xmqtt_journal_tests`. Tests use a public VFS backend for deterministic short/zero
write, partial failure, independently selected cache/sync persistence, reentry,
strict committed corruption/truncation and OOM; native child processes stop at
each real append/barrier before forced termination and fresh-process recovery.
An independent Python struct/zlib oracle checks bytes and emits its own fixture.
The [example](../examples/journal/README.md) and native-sync microbenchmark remain
separate from the original mixed-load, fixed-arrival, comparator and native
Windows/Linux >=72h acceptance requirements.
