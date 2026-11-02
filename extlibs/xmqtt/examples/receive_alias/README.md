# Local incoming Topic Alias example

Prerequisites: Python 3 and GCC; no credentials, broker or external network.
Run from the repository root:

```powershell
python tools/build.py --compiler gcc --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --no-single --cflag=-O2
.\out\gcc\native\xmqtt_client\examples_receive_alias_main.exe
```

On Linux use the same build command, then run
`./out/gcc/native/xmqtt_client/examples_receive_alias_main`. The build already
runs all selected examples. Expected:
`[PASS] full and compact alias publications resolve to demo/topic, retained messages survive all owners`.

CONNECT advertises a receive alias maximum of 16. The local peer binds alias 1
with a full topic, then sends a publication with an empty wire topic and alias 1.
Both owned message descriptors expose `demo/topic`, while Wire preserves the
original frames. The client gracefully disconnects and destroys its alias cache;
assertions read the messages after Client and Engine destruction. All message,
Future, stream and listener owners are released. There are no persistent files
or external services to clean up. Outgoing aliases remain pending.
