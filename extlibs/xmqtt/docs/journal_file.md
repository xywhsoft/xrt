# APP example for a locked native MQTT journal

The [example helper](../examples/common/journal_files.h) opens an existing
regular file through an anchored XRT directory and acquires an exclusive
open-file lock before calling `xmqttJournalCreate` or `xmqttJournalOpen`.
Provision the directory entry durably in a trusted directory. The helper
rejects symbolic links in the final component. Coordinate replacement or
removal of the entry across all processes that use the store.

Other processes, independent opens within the same process, and hard-link
aliases contend for the same lock. The helper returns `XMQTT_JOURNAL_BUSY`
on contention and preserves the caller's output and current error. XRT uses
an open-file-description lock on Linux and a native handle lock on Windows.

The APP owns the open file and can close the directory after construction.
If a JournalWorker or Store Writer takes the Journal, the APP keeps the file
open until that worker's Destroy succeeds. Retained commit results can outlive
the worker and the file. Handle close errors in the APP after retirement.

Build the Journal, APP example and tests.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_journal_file_tests
```

For the example, supply a private directory and an existing empty file.
Run the resulting `examples_journal_file_main create DIRECTORY FILE` to
initialize and append one record, then `examples_journal_file_main open
DIRECTORY FILE` to replay it and append another. Use the same directory and
file name on both invocations.

The process test starts several contenders on a native file, checks that only
one can initialize it, and opens the winning history from a fresh process
after killing the owner. This tests process death and native lock retirement.
Directory sync and power-loss durability of name publication are separate.

[XRT file locks](../../../docs/api/file.md) · [MQTT API reference](api/reference.md)
