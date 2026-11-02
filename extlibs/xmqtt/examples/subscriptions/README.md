# Local subscription example

Prerequisites: Python 3, GCC and the XRT checkout; no broker, credentials or
external service. From the repository root on Windows:

```powershell
python tools/build.py --compiler gcc --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single --cflag=-O2
.\out\gcc\native\xmqtt_client\examples_subscriptions_main.exe
```

On Linux use the same build command and run
`./out/gcc/native/xmqtt_client/examples_subscriptions_main`.
The build also runs all examples automatically. Expected: `[PASS] three
subscriptions granted and removed, retained ACK readable, all owners retired`.
The process owns a numeric loopback listener and one client, grants QoS 0/1/2,
unsubscribes, gracefully disconnects and verifies owned ACK bytes after Client
and Engine destruction. It releases both Futures, native stream and listener;
no files or persistent broker/session state need cleanup. This is a functional
example, not a durable-session or benchmark demonstration.
