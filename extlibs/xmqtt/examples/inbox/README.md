# Client-independent Inbox

Run from the repository root:

```
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_inbox
```

The local fixture echoes QoS1 messages on two different Clients and Engines.
Both Engines are destroyed before the application drains the one shared Inbox.
This demonstrates owned receive data and bounded quotas, not a persistent broker
session or automatic reconnect. Use a verified TLS connection for remote traffic;
this numeric loopback Inbox factory currently composes only with raw TCP.
