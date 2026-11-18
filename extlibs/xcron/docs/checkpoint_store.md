# File-backed checkpoint store

Select XCRON_MODULE_XCRON_CHECKPOINT_STORE. Each file contains one persistent
job and revision. Attach the store to an XRT directory root in a trusted
application-owned directory. A single caller serializes operations. Opening
takes an exclusive native lock for the lifetime of the handle. Windows uses a
native lock and denies delete sharing. Linux uses an advisory lock tied to the
open file description. Keep the pathname stable and make every writer use the
same ownership protocol.

Exclusive creation initializes an empty generation zero. A failed creation
may leave a file requiring operator inspection. Opening an existing store
requires an exact 8192-byte file and at least one validated slot. Generation
zero returns EMPTY and does not publish a checkpoint. The save and restore
contract is described in [scheduler checkpoints](checkpoint.md).

The store retains one validated checkpoint and its generation in memory.
Reads use that cached value. Commit compares the expected generation, checks
the record format and key, then writes the alternate slot and synchronizes
the file before publishing success. Caller storage must remain stable for
the duration of each call. Restore also validates scheduler configuration
and the actual occurrence.

## Recovery

Windows and Linux use the respective native file synchronization behind
xrtFlush. The store keeps the established file name and size when committing.
The host is responsible for durable installation of the directory entry
when provisioning a new store. POSIX file synchronization alone does not
ensure the parent-directory entry has reached storage.
[Linux fsync semantics](https://man7.org/linux/man-pages/man2/fsync.2.html)
[Windows file flushing](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)

Slots begin at byte 0 and byte 4096. Each slot starts with 216 bytes. All
integers are little endian. The marker is XCST, followed by a 32-bit version,
a 64-bit generation, the two 64-bit identity keys, the 180-byte checkpoint
and a CRC-32/ISO-HDLC over the preceding 212 bytes. Generation zero uses an
all-zero payload. A generation's low bit identifies its slot.

Recovery checks both CRCs. It uses the newer of two adjacent valid
generations, or the one intact slot after a torn write. ValidSlots exposes
that fallback. A caller that tracks acknowledged generations externally
can use it to detect rollback after storage corruption. Two unreadable
images, a valid but unsupported layout or inconsistent generations stop
recovery. An identity mismatch succeeds only by opening the intended file
with its matching key.

A write or flush failure returns UNCERTAIN. Close that handle and reopen;
the new slot may have reached storage. Reopen synchronizes the selected
image before making it available. The CRC detects accidental corruption.
Protect the file from unauthorized writers. The verified fault model
covers incomplete writes, raised I/O failures and process termination.
Storage-device power-loss behavior, application effects and automatic
live-service commit scheduling remain separate acceptance work.
[Runnable example](../examples/checkpoint_store/README.md).
