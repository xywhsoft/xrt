# Processing a durable publication

Run from the repository root:

    python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_pending_tests

The example uses a native temporary journal. It records an accepted PUBLISH and
copies the oldest committed pending record into the APP buffer. The example
prints the application effect, submits retirement by the original epoch and
key, then checks the result after reopening the journal. A production APP can
use the journal namespace and delivery key to reconcile an external effect
after a crash. See [pending publication processing](../../docs/store_inbox.md).
