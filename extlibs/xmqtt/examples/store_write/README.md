# Typed asynchronous Inbox acknowledgement

Build/run the `xmqtt_store_write_tests` suite with the normal extension manifest.
The example creates a temporary native file, recovers its typed Inbox, submits a
semantically validated application ACK on a borrowed host pool, and checks fresh
recovery after physical shutdown. An optional existing file path runs the same
format/binding/identity checks. Temporary files are removed; user files remain.

This network-free example prints application work before its ACK. It provides no
business transaction/lease/exactly-once guarantee and sends no MQTT network ACK.
The recorded historical wall clock is an offline deterministic fixture, not a
production trusted-time source. File namespace publication and power loss remain
separate acceptance work.

Build and run from the repository root (replace gcc with the selected compiler):

```text
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_write_tests --no-single
out/gcc/native/xmqtt_store_write_tests/examples_store_write_main.exe
```

On Linux use `--arch x64` and run
`out/gcc/x64/xmqtt_store_write_tests/examples_store_write_main`.
The suite also runs the example automatically. An optional existing journal path
uses the documented demo binding/generation/client identity and remains host-owned.

Build and run from the repository root (replace gcc with the selected compiler):

```text
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_store_write_tests --no-single
out/gcc/native/xmqtt_store_write_tests/examples_store_write_main.exe
```

On Linux use `--arch x64` and run
`out/gcc/x64/xmqtt_store_write_tests/examples_store_write_main`.
The suite also runs the example automatically. An optional existing journal path
uses the documented demo binding/generation/client identity and remains host-owned.
