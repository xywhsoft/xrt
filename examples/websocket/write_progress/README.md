# Local write-prefix observation

Run `python tools/build.py --suite websocket_stream_write_progress --no-single`.
The public-API loopback example records an accepted masked frame's native byte
cut and observes its physical write completion on the owning worker. It joins
both endpoints/listener/Engine. This is not a peer/application acknowledgement.
The raw local pair exercises the post-Upgrade attachment stage only; real hosts
must first complete and validate HTTP Upgrade and negotiate their subprotocol.
Select `websocket_stream_tls_write_progress` for a WSS native ciphertext cursor;
an unsent WSS frame suffix marks Staged while existing cuts stay observable.
Pause ordinary writes until it clears before taking a cut for a new message. Full MQTT WS/WSS integration remains separate.
