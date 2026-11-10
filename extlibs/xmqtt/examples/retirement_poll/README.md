# Nonwaiting physical retirement query

Run with `python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single`.

The local TCP fixture requires no broker. An open Client returns TIMEOUT. After
external shutdown, an Engine worker queries OK; the unique external owner then
destroys the Client and joins the fixture Engine. The observation object stays
alive through that join. This demonstrates the worker API and ownership boundary;
it is not an automatic reconnect controller.

A real controller may query before retirement: TIMEOUT also covers a busy
predicate mutex. Schedule another observation through its bounded driver rather
than waiting on the IO worker. ERROR is a failure, never readiness. Keep the
unique owner and exclude competing API entries before final cleanup. Existing
Wait/Destroy remain forbidden on same-Engine workers.
