# Subscriber readiness and batch example

Run from the repository root:

```text
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_subscriber_notify --exclude-test "*" --no-single
```

The owned local protocol fixture needs no Redis installation or external service.
The example dedicates a ready connection, subscribes to a binary name, drains an
owned message using TakeBatch/Readable, unsubscribes and closes every owner. The
message remains valid after Client and Engine destruction. An application can
pass a larger reply array to transfer a FIFO prefix with one queue lock.
Readability is a shared wake hint: always recheck the queue after waking, and
use a caller-owned cancellation token for an independently cancelable wait.
