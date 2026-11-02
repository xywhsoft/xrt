# Bounded pending calls

`xrpc_pending` is a transport-independent, serialized-owner admission and removal
table. It copies exact ID comparison keys, borrows one application `User` pointer
per call, and arbitrates response, local cancellation, expiry and shutdown through
removal. Only an `OK` removal publishes a record. Callers complete their future or
invoke their continuation **after** removal, on the executor they select.

There are no callbacks, futures, sockets, threads, locks or response-envelope
validation in this leaf. The connection/worker owner must serialize all operations,
inspection and destruction. Do not access a table concurrently, including reads.
An external lock is possible; its cost belongs in runtime measurements.

`Insert` validates the entire raw ID and copies its opaque key. Equivalent decimal
values and decoded strings follow [the ID policy](id.md). Null is a distinct live
key. Keep the original raw ID with application request state for encoding; the
table does not retain it. Duplicate live IDs and full capacity are ordinary
outcomes with unchanged output and ambient diagnostic. Invalid input fails even
when full, and duplicate takes precedence over full.

Each admission assigns a new nonzero 64-bit serial plus a slot. Reusing a slot
cannot make an old local token cancel a new call. Serial exhaustion fails locally
without wrapping. Tokens belong to one specific table lifetime; they do not carry
a global table identity. Never apply a token from another table or a destroyed
table, even if an address was reused.

This local-token protection does not solve remote late-response ambiguity. After
removing an ID, inserting the same wire ID allows an old response to match the new
entry. A runtime must choose fresh wire IDs, track connection epochs and admit
only eligible, completely validated response envelopes. `TakeId` accepts an ID
only: it cannot distinguish an unsolicited parse-error/null ID from an eligible
response, or validate a malformed batch tail. A future response pump must validate
the whole document before any removal.

Deadlines are absolute ticks in one caller-selected monotonic domain. The table
does no clock reading/conversion and assumes the owner supplies comparable values.
`UINT64_MAX` means never; `Expire` does not remove it, even if Now is that sentinel.
`Info.NextDeadline` is the earliest deadline, or the sentinel when no finite
deadline exists. Count distinguishes an empty table from never-only calls.
`Expire` removes at most one due entry; equal deadlines use admission order.
`TakeFirst` also drains never entries. Destruction does not free User objects or
complete outstanding calls; drain before destruction when those effects are needed.

One checked allocation reserves the header, entries, a deadline heap, one shared
key scratch area and `MaxCalls` fixed-size key slots. Defaults are 1024 calls,
2 MiB requested owned-byte ceiling, ID input 4096, decoded string 256, key 512.
`MaxOwnedBytes` counts requested storage, excluding allocator headers/caches. Fixed
slots trade a predictable bound and allocation-free mutations for unused space:
size the key limit to the actual ID policy. No table resize occurs. All bounds and
size arithmetic are checked before allocation. Cold diagnostics can allocate;
successful JSON/error-reference bookkeeping can still synchronize in XRT.

An AVL comparison tree provides worst-case logarithmic search depth independently
of attacker-chosen hash collisions. Each comparison can scan a common key prefix.
The private tree uses 32-bit slot links, keeping keys and tokens in the fixed
slot layout; this is a memory-layout choice, not a measured speed advantage over
the public XRT pointer-node AVL. No shared runtime is added. Removal transplants
nodes structurally so live token slots never change. A binary
min-heap provides logarithmic deadline insertion/removal; tokens check a slot in
constant time before normal tree/heap removal. Tree mutations recurse only to AVL
height. Success does no per-operation ordinary allocation. `Insert`/`TakeId`
independently validate/canonicalize the ID once per call, copy on insert and compare
keys along the tree path. Insert first searches for a duplicate, then makes a
second tree traversal for insertion. TakeId searches, then traverses again for
structural removal. Token/expiry removal skips the initial ID search but still
traverses the comparison tree to remove its known slot. Cancellation/expiry do
not parse a raw ID. These passes are included in the benchmark.

Run the documented [build commands](../README.md) with suite `xrpc_pending` to
execute the example. It demonstrates one response winning over stale cancellation
and expiry at an exact tick. Tests are separate: 100,000 operations against an
independent linear model, ordered/adversarial tree mutations, deadline ties,
semantic ID cases, slot reuse, limits/aliases/publication, allocation counts and
constructor/diagnostic OOM. The benchmark measures remove+reinsert at fixed
occupancy, with precomputed locally reused wire IDs. It excludes setup, formatting,
future completion, queueing and transport. Per-cycle latency uses a separate phase
and includes two clock reads. This is not an end-to-end performance claim.
