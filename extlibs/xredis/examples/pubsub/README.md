# Typed Pub/Sub wire example

From the repository root:

```text
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_pubsub --exclude-test "*" --no-single
```

This offline example encodes a binary/empty-name SUBSCRIBE command and decodes
an attributed, fragmented pattern-message frame. The owned reply remains valid
after destroying its reader. It does not open a connection or install a
subscription; the dedicated native connection runtime is still pending.

See [wire contracts and remaining scope](../../docs/pubsub.md).
