# Role-verified readiness

Run `python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_role_client --exclude-test "*" --no-single`. The offline loopback fixture scripts HELLO, ROLE and PING. Ready waits for a validated master response; a queued request runs afterwards and retained readiness survives close. The fixture uses no external Redis.
