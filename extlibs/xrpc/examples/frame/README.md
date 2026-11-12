# Incremental stream framing

Run from the repository root:

```
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_frame --exclude-test "*" --no-single
```

The example feeds two coalesced Content-Length frames in five-byte pieces,
consumes only the reported prefix, verifies each complete body and checks clean
EOF before destroying the reader. Bodies are borrowed until their documented
boundary; asynchronous consumers must take their own snapshot. No external
service, callback, transport or credentials are required.

Select UINT32_BE explicitly in Config for the separate four-byte binary length
profile. No format sniffing is performed. The separate interop test runs real
binary stdin/stdout peers with independent Python framing and JSON checks. A
production stdio/process adapter and bidirectional Future runtime remain work.
