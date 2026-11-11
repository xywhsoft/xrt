# Native shared connection pools

`xredis_pool_shared` is developing. It multiplexes ordinary commands over a
fixed collection of ready managed clients. Each accepted call is the native
request and Future; there is no lease, proxy Future or extra payload clone.
The ordinary exclusive pool remains available for session-scoped work.

Create validates the same bounds/profile as PoolCreate, claims all members with
rollback on partial failure, and publishes an immediately usable ready list.
The native opaque pool handle supplies Info/Close/Closed/Retain/Release. Acquire
rejects shared mode, and shared call APIs reject exclusive pools. Config
MaxPending/LeaseTimeout/ReturnTimeout remain validated but do not control shared
calls. No handle layout grows; an otherwise invalid private zero lease timeout
marks this mode after public config validation.

Selection rotates the ready list under the pool gate, retaining the selected
client before unlocking. At most min(ready members, WorkItems) candidates are
probed. A rejected constructor can try another candidate; an accepted request
never migrates or replays. Native admission rechecks ownership, state and quotas
under the client gate. No selection lock is held during allocation/encoding or
Future wait. Timeout includes selection/construction from original API entry.
No eligible capacity returns OVERLOADED immediately with output untouched;
PoolClose returns CLOSED. Sync refuses all workers of the engine.

PoolInfo Idle now means dispatch-eligible members in this mode, even when busy.
Leased/Pending/Acquired/Returned remain zero. Requests and CommandBytes include
native constructors and physical tails across every member. Cancellation or
timeout of sent requests resolves logically while preserving FIFO obligations
and quota until replies drain or the connection retires. Close fences native
admission, closes members and awaits physical retirement. Retained completed
results hold neither pool nor engine. WorkItems also bounds maintenance turns.

Known connection/session controls are rejected by native owned-call admission.
Blocking commands can delay later replies on that member; use a separate pool.
There is no cross-connection order, key affinity, reconnect, topology refresh,
redirect execution or production performance acceptance in this fixed leaf.

See the runnable [example](../examples/pool_shared/README.md). Functional and
measurement evidence is recorded with the stage report; benchmark local peer
rates are not production or fixed-arrival tail guarantees.
