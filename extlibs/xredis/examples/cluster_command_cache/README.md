# Fetch a Cluster command catalogue and route a cached GET

The example starts a loopback fixture. The APP sends `COMMAND` through an
explicitly selected managed primary, validates the reply and replaces its
owned catalogue. It looks up the slot for a binary-key GET and calls the
Cluster client with that slot.

Run refresh from a control thread. Keep the last good snapshot when the fetch,
parse, budget or deadline check fails. The APP chooses when to refresh and
which primary represents its command definitions. To invalidate a snapshot,
drop the APP-owned pointer. A user of a shared immutable snapshot retains its
own reference. The example serializes refresh and replacement on one owner.

```sh
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_app_command_cache,xredis_app_command_cache_tests
```
