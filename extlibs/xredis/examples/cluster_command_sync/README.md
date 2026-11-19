# Synchronous metadata routing

Run from the repository root:

```
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_app_cluster_command_sync --exclude-test "*" --no-single
```

Three owned literal peers implement an observable binary GET. An independently
authored catalogue identifies the actual routing key; no fallback slot is needed.
The example releases that catalogue, tears down all network owners, verifies the
retained binary result and finally destroys the Future. The catalogue fixture is
test data; production applications obtain authorized COMMAND/COMMAND INFO metadata
and use the APP's `xredisCommandCatalogCreate`. The
[cache example](../cluster_command_cache/README.md) shows fetch and replacement.

UNKNOWN, UNSUPPORTED, CROSS_SLOT, ARGUMENTS, LIMIT and KEYLESS leave output and
ownership unchanged. A true keyless command needs explicit destination policy.
Check ACCEPTED, then Wait, then the native result; ACCEPTED alone is not Redis
success. See the explicit-slot example for wait failure and worker restrictions.
