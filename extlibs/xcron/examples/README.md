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
