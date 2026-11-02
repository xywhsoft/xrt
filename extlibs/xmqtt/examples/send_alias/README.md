# Local outgoing Topic Alias example

Prerequisites: Python 3, GCC and this checkout; no external broker or secrets.
Run from the repository root on Windows:

```powershell
python tools/build.py --compiler gcc --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single --cflag=-O2
.\out\gcc\native\xmqtt_client\examples_send_alias_main.exe
```

Linux uses the same build command, then
`./out/gcc/native/xmqtt_client/examples_send_alias_main`. The build already runs
the selected examples. Expected:
`[PASS] alias bound, compact QoS1 sent, 23/14 wire bytes, all owners retired`.

The local peer advertises alias capacity 16. The client submits a QoS1 PUBLISH
with full topic `demo/send` and alias 1, then an empty-topic reference. Both await
PUBACK, then graceful DISCONNECT. Actual publication bytes are 23 and 14. Inputs
are snapshotted during admission; results outlive Client/Engine. All Futures,
client, peer stream, listener and Engine are released; no files or persistent
broker state require cleanup. See the client contract for cancellation/flow
cases where a logical compact request is sent with its captured full topic.
