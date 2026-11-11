# Automatic shared pool example

This offline program creates two managed connections to a local protocol peer,
issues a binary ECHO, loses one member after an uncertain DROP request, and
waits for its replacement. The original request is never replayed. It awaits
physical close/context Drop and inspects a retained reply after engine shutdown.

```powershell
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_pool_shared_connect --no-single
```

Replace the fixture factory with numeric/DNS/prepared/TLS/auth managed factories.
Use one service/session/RESP profile on every attempt. Context and callback code
must outlive Drop. The example's bounded polling observes maintenance; a real
caller chooses its own bounded admission policy. Shared calls return OVERLOADED
immediately during outages, even with a forever timeout. Accepted requests keep
their own outcome and never move or replay. Close/await Closed/Release normally.
