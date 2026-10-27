# Service extensions progress

Target and rules: [development contract](service-extensions-plan.md).

## Current state

- All four final products are in development, not production accepted.
- P0 source/worktree isolation and execution contract are established.
- First implementation package: xredis binary-safe command encoding.
- Primary checkout changes remain outside this branch.

## Baseline checks

Source: `1d4109aff9d52d0452742669e81caa8bef626922`.

| Check | Result |
| --- | --- |
| `python tools/check_release_maturity.py` | Existing failure: missing `net_file_future` single-test closure coverage |
| `python tools/generate_features.py --check` | Passed |

## Feature ledger

| Product | Feature | State | Evidence |
| --- | --- | --- | --- |
| common | development/acceptance contract and isolated branch | complete | this document, development contract and baseline checks |
| xredis | binary command size/write | in progress | not yet verified |
| xredis | incremental RESP2/3 reader | pending | no claim |
| xredis | runtime, pools, HA, tracking | pending | no claim |
| xrpc | protocol, dispatch, transports | pending | no claim |
| xmqtt | MQTT 5.0/3.1.1, sessions, persistence, transports | pending | no claim |
| xcron | expression, calendar, time zones, scheduler, recovery | pending | no claim |

Do not replace pending rows with success based only on metadata or compilation.
