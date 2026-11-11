# Automatic desired subscription session

```
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_subscription_session --exclude-test "*" --no-single
```

This offline example uses an independent local protocol fixture. The controller
runs on an existing Engine: it retries a failed factory, restores intent, applies
a live incremental change without replacing the connection, and physically closes.
The example only observes progress; it never drives replay or retry steps. Real
applications use Notify for state changes and native subscriber readability and
batch receive for messages. Callbacks must not block an Engine worker. The example
executes every session API and retains metadata after Engine destruction.
