# Cluster route execution example

This offline example creates three shared protocol peers, starts from a topology
snapshot, automatically follows MOVED then ASK, and retains the final native
Future after controller/pools/engine destruction.

```powershell
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_route --no-single
```

The driver demonstrates the low-level nonblocking Step contract: schedule MORE
immediately, observe Pending or NextWake on WAIT, and drive to DONE. Integrate
that contract into an existing event loop; never block an engine worker. The
provider authorizes the exact endpoint/profile and returns a retained fixed or
maintained shared pool. Replace the fixture with the application's bounded node
cache/refresh policy. This controller does not own a new worker/timer/proxy
Future or the node pools. Slot is explicit; all command keys must share it.
Both parent status and native outcomes remain meaningful. Keep ResultFuture
for escaping native reply views; copy endpoint views before destroying Route.
