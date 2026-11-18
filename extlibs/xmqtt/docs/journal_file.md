# Locked native MQTT journal

`XMQTT_MODULE_XMQTT_JOURNAL_FILE` opens a native Journal through an anchored
XRT directory. Use an existing regular file in a trusted directory. Provision
the directory entry durably before opening it. The last path component must be
a file, with symbolic links rejected. The application coordinates any
namespace changes across all processes that use the store.

`xmqttJournalCreateLocked` initializes a pre-existing empty file.
`xmqttJournalOpenLocked` verifies an existing log. Both obtain an exclusive
open-file lock before examining journal bytes. Another process, another open
in the same process, and an independent hard-link alias contend for that lock.
On `XMQTT_JOURNAL_BUSY`, the caller's output and committed history are
preserved. Process-associated Linux record locks can be lost on an unrelated
close. This API uses an open-file-description lock on Linux and a native
handle lock on Windows. Use the locked constructors consistently for all
writers. Replacing or removing a data file while it is owned violates the
namespace contract.

The root is borrowed during construction. The locked native handle moves
with the Journal to JournalWorker or Store Writer. Close and Wait retire the
worker's I/O while the owner continues to hold the lock. Destroy closes the
file. Retained commit results can outlive that ownership. If native close
reports an error, the poisoned Journal or its enclosing owner can be
destroyed again to finish disposal.

Build the module, tests and example.

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
