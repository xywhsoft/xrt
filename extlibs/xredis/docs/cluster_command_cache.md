# Cached Cluster command metadata

Select `XREDIS_MODULE_XREDIS_CLUSTER_COMMAND_CACHE`. From a control thread,
call `xredisClusterClientRefreshCommandCatalog` with the slot of an
authorised primary that represents the cluster's command definitions. The
same managed Cluster client and ACL identity issue a full `COMMAND` through
the existing route. Both progress Engines must be available while it waits.
[Redis COMMAND response format](https://redis.io/docs/latest/commands/command/).

Only a completely parsed catalogue within the supplied command key limits
is published. The original deadline covers fetch, parse and publication.
One refresh is active per client. A concurrent refresh returns BUSY. The
accepted native result is always returned in `xredisclusterwait`, including
when command execution, waiting or parsing fails. Destroy its Future after
inspecting its existing typed route and query outcome. Earlier refusals
keep that output unchanged.

The previous catalogue stays usable after a failed refresh unless explicitly
invalidated. `xredisClusterClientInvalidateCommandCatalog` makes it
unavailable until a new refresh publishes. If called during a refresh, it
also fences the in-flight reply. Client Close prevents publication. A
retained snapshot can outlive client teardown.

`xredisClusterClientCallCachedCommand` uses a retained snapshot for the
duration of metadata validation and admission. It performs no metadata I/O.
A missing or invalidated snapshot yields CATALOG_UNAVAILABLE. The usual
keyless, unknown, incomplete, cross-slot, overload and timeout rules apply.
The snapshot is chosen when the client gate is acquired. Calls that have
already selected a snapshot may proceed across invalidation. The application
chooses when to refresh after cluster command definitions or ACLs change and
uses consistent command definitions across all eligible business primaries.

The warm path adds one existing client gate acquisition and one catalogue
retain/release pair. It reuses the existing native admission and copies no
additional business bytes. Parsing and replacement run on the control
caller. The cache holds one catalogue; a refresh may temporarily hold
both old and new snapshots. Each follows the catalogue's independent memory
bounds. Application-retained snapshots and native result Futures keep their
own references. There is no new timer or background thread.

[Runnable example](../examples/cluster_command_cache/README.md).
