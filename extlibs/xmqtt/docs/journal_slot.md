# APP two-generation journal selector

The [selector example](../examples/common/journal_slot.h) uses three
distinct, exclusively accessible XRT file handles. The handles may come
from native files or a VFS provider. The control file needs synchronous
absolute read/write, stat and flush. Both data files additionally require
resize. Keep these handles open until the Journal or Writer has retired
and the APP selector is destroyed.

The [native APP example](../examples/common/journal_files.h) opens
`journal.slot`, `journal.0` and `journal.1` in a stable private directory.
The caller provides the three files and ensures their initial durable
publication. The helper verifies the three file identities,
locks all three handles and keeps the locks through logical retirement.
Later switches update file contents in place with XRT sync operations.
[Linux file and directory sync](https://man7.org/linux/man-pages/man2/fsync.2.html) ·
[Windows file sync](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)

The control file has a fixed 12 KiB layout. Its descriptor identifies this
broker, principal, payload format, MQTT version and initial generation. The
two checksummed witnesses identify the generation and its published journal
prefix. Every participant uses the same exclusive-access policy, including
while either data file is idle.

The example passes the published prefix to `xmqttJournalOpenAtPrefix`.
Fully replay the selected Journal and close it through the APP helper
before preparing an offline replacement. Only the inactive file is reset. The application
writes a complete logical replacement through the Journal API. On successful
publication, the same Journal accepts appends. After transfer to a Worker,
clear the APP-held child with the detach helper. Serialize APP access with
that Worker and retire it before closing the slot. Close an abandoned
candidate through the same APP close helper.

A publication writes both witnesses after the replacement's journal writes
have synced. After an interrupted publication the newest valid witness
selects the one exact generation to recover. Reopen validates that Journal's
published sequence, end offset and chain and checks newer frames during replay.
If a witness has to be repaired, the surviving choice stays untouched while
the stale page is repaired and synced, before the inactive file can be reused.
Corruption in the selected history fails recovery. A failed initial
installation can leave incomplete files. Inspect those files before retrying.

The APP bounds each generation by the Journal configuration and uses a
fixed-size selector. Its snapshot policy determines which records transfer.
A process-crash test validates these restart decisions. Device power-loss
qualification requires a separate test.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_journal_slot_tests
```

With three empty files in a private fixture directory, run the example using
`create DIRECTORY`, `rotate DIRECTORY`, then `read DIRECTORY`. The demo's
records each contain the entire application snapshot. Rotation keeps the
latest snapshot in the next generation. The Journal payload schema in a
real application determines what state has to be transferred.
