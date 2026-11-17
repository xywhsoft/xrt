# Client storage integration under development

This module currently has no public factory. It adds a private path through the
actual numeric TCP Client to validate accepted CONNACK -> durable BIND/OPEN
-> Ready ordering,with native-pool Future notification and physical Client duties.
It records END at the actual native close edge. The borrowed Engine and task pool
remain running through Client retirement; Writer,file/provider and callback code
outlive their physical duties. All ordinary Client selections remain independent.

The rest of the online transaction driver is still required: outgoing snapshots/
attempts/PUBREC/completion,batched incoming acceptance/PUBREL/PUBCOMP retirement,
durable delivery leases,old-transport fencing and adapters for all transports/auth.
The private bootstrap fails closed on application flow instead of exposing a
partially durable public Client. It is not production or performance acceptance.
No new Core API,thread,pool or shared runtime is added.


Run the private integration fixture from the repository root:

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_store_tests
```

This selects modular and single-header fixtures; it adds no public API/example.
The root module includes this developing feature. Narrow Client/Session/Writer
selections remain independent. The private context and encoded transaction buffer
are charged to Client MaxPrivateBytes; the staged CONNACK uses the existing object
quota and transfers into Ready without another message copy. The Writer/worker
still copy encoded transaction bytes under their own bounded admission contract.
No zero-copy or measured throughput/tail-latency claim is made.
