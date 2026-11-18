# Atomic checkpoint groups

Select XCRON_MODULE_XCRON_CHECKPOINT_GROUP. One file contains the latest
complete snapshot of a set of jobs. Group is a nonzero persistent
application identity. Capacity fixes the maximum records and file size
at creation. Use the same identity and capacity on every reopen.
Records are existing 180-byte checkpoints in ascending stable Job ID
order, with exactly one current revision per job. A committed empty
group is a new generation with no jobs.

The caller opens the file beneath a trusted XRT directory root and
serializes access to the handle. The native exclusive lock lasts
through close. Read exposes one borrowed immutable view until the next
commit or close. CheckpointGetKey returns the stable identity from a
record. Use the matching configuration and zone when restoring. Pause
jobs and collect their physical completions before capturing a set of
quiescent checkpoints. Paused restore lets the host stage recovered jobs
before releasing dispatch. Application effects and collection order
remain host responsibilities.

Commit checks the expected generation and validates the entire caller
array before writing. It copies the records once into the alternate
slot buffer, writes one contiguous image, flushes once, then swaps the
cached buffers. Open requests one allocation for metadata and two
capacity-sized frames. Subsequent successful reads and commits allocate
no memory. Call on a caller or storage thread. An I/O failure returns
UNCERTAIN. Close the handle and reopen it before deciding whether to
retry. The single-job store format remains available independently.

## File format and recovery

Two slots use a stride of the maximum frame size rounded up to 4096
bytes. The fixed little-endian 40-byte header contains, in order,
the XCGS marker, version 1, a 64-bit generation, a 64-bit group ID,
32-bit capacity, 32-bit count, group CRC and header CRC. All fields
besides the marker, generation and group ID are 32-bit. The group CRC
uses CRC-32/ISO-HDLC over the ordered sequence of four-byte CRC
fields from the checkpoint records. Each record also validates its
own CRC. The header CRC covers the preceding 36 bytes. Using whole
CRC-protected records as group CRC input would give the same residue
for same-sized valid records, so it would miss mixed generations.

The records follow the header. Unused slot bytes are outside the
committed image. The generation's low bit selects its slot. Generation
zero represents the initial empty group. Reopen checks the two headers
and reads each declared payload up to Capacity. It chooses the newer
of adjacent intact generations or the single intact slot, then
synchronizes before publication. Wrong identity, unsupported
checksum-valid format, bad parity, a non-adjacent pair and two invalid
slots fail. The CRCs detect accidental corruption in a trusted store.
The host owns durable initial directory installation. Process-kill
validation covers torn writes and a complete write before flush.
Device power-loss validation remains open.
[Runnable example](../examples/checkpoint_group/README.md).
