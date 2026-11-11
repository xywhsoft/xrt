# Inspect owned ROLE replies

Run `python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_role --exclude-test "*" --no-single`. Add `--compiler /usr/bin/gcc --arch x64` on Linux.

The offline example exercises all four public APIs. It decodes master replica tuples and Sentinel monitored names from existing owned replies. Keep a Reply reference alive while using views/cursors. Network discovery, role-gated readiness and failover are subsequent stages.
