# Bounded native journal generation switch

Select `XMQTT_MODULE_XMQTT_JOURNAL_SLOT` for a two-generation store in one
anchored directory. Provision three distinct empty regular files named
`journal.slot`, `journal.0` and `journal.1`. Keep the names and directory
stable. The application ensures initial durable publication of those names.
Later switches update file contents in place with XRT sync operations.
[Linux file and directory sync](https://man7.org/linux/man-pages/man2/fsync.2.html) ·
[Windows file sync](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)

The control file has a fixed 12 KiB layout. Its descriptor identifies this
broker, principal, payload format, MQTT version and initial generation. The
two checksummed witnesses identify the generation and its published journal
prefix. The open file lock stays held from slot open through destruction.
Every participant uses the slot owner, including while the data file is idle.

Open the selected Journal, fully replay it and retire the old owner before
preparing a replacement. Only the inactive file is reset. The application
writes a complete logical replacement through the Journal API. On successful
Publish, the same Journal accepts appends and may transfer into JournalWorker.
An operation that abandons a prepared Journal simply destroys it.

A publication writes both witnesses after the replacement's journal writes
have synced. After an interrupted publication the newest valid witness
selects the one exact generation to recover. Reopen validates that Journal's
published sequence, end offset and chain and checks newer frames during replay.
If a witness has to be repaired, the surviving choice stays untouched while
the stale page is repaired and synced, before the inactive file can be reused.
Corruption in the selected history fails recovery. A failed initial
installation can leave incomplete files. Inspect those files before retrying.

This layer bounds each data generation by the Journal configuration and
uses a fixed-size selector. Application snapshot equivalence, automated MQTT
state compaction and storage-device power-loss qualification are separate work.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_journal_slot_tests
```

With three empty files in a private fixture directory, run the example using
`create DIRECTORY`, `rotate DIRECTORY`, then `read DIRECTORY`. The demo's
records each contain the entire application snapshot. Rotation keeps the
latest snapshot in the next generation. The Journal payload schema in a
real application determines what state has to be transferred.
