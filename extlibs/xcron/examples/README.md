# xcron APP examples

Run all examples from the XRT repository root.

```sh
python tools/build.py --manifest extlibs/xcron/config/modules.json --suite xcron_examples --no-single
```

| Example | APP behavior |
| --- | --- |
| [expression](expression/main.c) | Compile and validate a Cron string |
| [calendar](calendar/main.c) | Iterate UTC trigger times or budget a search across host turns |
| [scheduler](scheduler/main.c) | Register a callback and choose polling or blocking wait |
| [executor](executor/main.c) | Hand a callback to an APP-owned XRT executor and join its work |
| [service_checkpoint](service_checkpoint/main.c) | Reload a persisted task name and rule, then rebind the APP callback |
| [restart_catchup](restart_catchup/main.c) | Replay a bounded number of UTC occurrences after restarting from the last successful action |
| [timezone](timezone/main.c) | Resolve one civil result using an APP-selected host time zone and fold policy |

The configuration example also accepts `save FILE` followed by `run FILE`
in a second process. The example saves a rule and reloads it with a new
process-local callback and context. Use the APP's storage conventions for
atomic replacement and crash durability.

For a service, keep business execution, overlap and retry policies beside the
callback. Persist task identifiers and rules in the APP's configuration. Persist
last-success timestamps in the APP if it needs restart catch-up. The scheduler
chooses the next time from the current UTC clock after registration. Historical
service, checkpoint and timezone implementations are available at the previous
product revision `befcce75`.


The catch-up APP fixes its rule at one occurrence per second. Timestamps are
UTC milliseconds. Use a separate shell command for each simulated process.

```sh
examples_restart_catchup_main seed checkpoint.txt 1000
examples_restart_catchup_main replay checkpoint.txt 5000 2
examples_restart_catchup_main replay checkpoint.txt 5000 2
```

Each process samples one upper bound and advances through strictly later
occurrences within its supplied count budget. It atomically replaces its
single-writer checkpoint after each successful action. On Windows the
checkpoint directory supplies the inherited ACL and its filesystem must
support POSIX-style replacement. An optional final timestamp simulates a
failed action and leaves that occurrence available to retry.

If a checkpoint write fails after a successful action, the example
reports that occurrence and exits. Once storage is available, the next
run starts at the previous checkpoint and retries the occurrence. The
same retry can follow a crash between the action and checkpoint. Use
the scheduled UTC timestamp as the APP's idempotency key.

The independent peer driver force-stops a replay after its first actions and
starts the APP again with the recovered checkpoint. It checks that committed
occurrences stay complete and that an emitted occurrence without a committed
checkpoint is eligible to run again.

```sh
python extlibs/xcron/tests/interop/run_restart_catchup.py out/gcc/native/xcron_examples/examples_restart_catchup_main out/xcron-restart-catchup.json
```

Use the built executable's path and include the `.exe` suffix on Windows.
