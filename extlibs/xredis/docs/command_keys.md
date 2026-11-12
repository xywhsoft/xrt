# Command key metadata

The developing `xredis_command_keys` leaf independently implements the public
[Redis key specification](https://redis.io/docs/latest/develop/reference/key-specs/)
and [COMMAND](https://redis.io/docs/latest/commands/command/) reply contracts.
It consumes actual COMMAND/COMMAND INFO arrays, including null unknown entries,
legacy six-field rows, modern specifications and nested full-name subcommands.
RESP3 flags/specification/empty-child Sets are accepted as non-positional
collections; command row positions remain arrays. Wire iteration order is
preserved within each immutable snapshot, including Set collections.
It does not embed an implementation's command table. Metadata is a snapshot from
the application-selected server, not a permanent assertion about all servers.

Create validates bounded shape in two passes, retains one immutable Reply and
allocates one contiguous command/specification/hash index. Output/ownership change
only on OK. Names and keywords borrow that owned Reply. Shared readers need a
stable retained catalogue reference; lookups take no lock/refcount and allocate
nothing. Retained snapshots remain valid across application-driven replacement.
Create publishes no network work and invokes no application callback.

Evaluation includes command/subcommand arguments, checks arity and extracts every
routing key. It supports index and forward/reverse case-insensitive keyword
search, relative/negative ranges, factor-limited tails and key-count arguments.
Legacy non-movable first/last/step metadata remains supported. Key bytes are
binary; CRC16/hash-tag handling is independent of command/keyword case folding.
All extracted routing keys, including `not_key`, must agree on one Cluster slot.
Specification order and overlapping argument occurrences are retained in output.
No duplicate-name normalization or key payload copying is done.

Only OK/NONE publish outputs. NULL Keys/zero Capacity computes count and slot
without an output-vector pass. An optional caller vector causes a second bounded
extraction pass after complete validation; no partial keys escape on capacity,
unsupported, syntax/index/count, cross-slot, alias or quota failure. NONE has no
slot: an application must choose its own policy for keyless commands. The leaf
does not assert that a command's complete business grammar is valid.

Unknown/missing commands and unfamiliar/incomplete specifications are distinct
typed results. Such specifications are not treated as keyless, even if another
known specification yields a usable key. Full cross-slot validation requires all
keys; obtain [COMMAND GETKEYS/GETKEYSANDFLAGS](https://redis.io/docs/latest/commands/command-getkeysandflags/)
under the application's authorization policy when needed. `variable_flags`
reports possible access, not exact flags for an invocation. Neither those flags
nor a successful slot calculation authorize replica routing or replay.

Limits bound commands, specifications, name/depth, retained metadata, arguments,
selected key occurrences/bytes and search work. The search budget counts hash
input/probes and keyword inspections/matched bytes, both at index construction
and per evaluation pass. Key-byte limits also count overlapping specifications,
preventing repeated large-key hashing from bypassing the argument-byte limit.
These are requested library bounds, not RSS or allocator/kernel ceilings.

The warm path validates descriptor/output spans and total argument bytes, hashes
the command name, checks arity/rules, searches optional keywords, and hashes all
selected key bytes. A caller vector repeats extraction but not CRC calculation.
These CPU/scanning/call costs are real. The raw benchmark mode knows four key
positions in advance and omits metadata/argument validation; it is an algorithm
cost reference, not a functionally equivalent client or a zero-overhead proof.
Protocol/store/controller/native Linux/mixed-load/72h acceptance remains pending.
