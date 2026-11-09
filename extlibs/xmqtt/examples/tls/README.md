# TLS MQTT client

This opt-in network example requires an existing TLS MQTT service and its CA
bundle. It does not trust all peers, start a broker or embed credentials. It
prints usage without arguments. All input files are bounded and key scratch is
wiped. A native verifier validates the server chain/name. Optional mutual TLS
retains the identity before DNS; the RSA signing backend is selected separately.

From the repository root, build with:

```text
python tools/build.py --compiler gcc --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_tls,tls_identity_rsa
```

Run the resulting example path printed by the build:

```text
example_tls_main HOST 8883 SERVER_CA.pem
example_tls_main HOST 8883 SERVER_CA.pem CLIENT_CHAIN.pem RSA_KEY.pem
```

Success asserts trusted TLS, MQTT5 CONNACK, a QoS1 PUBACK for `example/xrt` and
authenticated TLS shutdown after DISCONNECT. It then physically retires Client,
Resolver and Engine before releasing the retained results. The example does not
establish broker persistence, subscriber delivery or Will suppression.

The independent SSL runner in `tests/interop/run_mqtt_tls.py` can execute the
actual example with `--example PATH`, using temporary keys and local sockets.
No credential contents enter its report. A healthy SSL byte peer proves transport
interoperability; full broker/performance/native long-run acceptance is separate.
