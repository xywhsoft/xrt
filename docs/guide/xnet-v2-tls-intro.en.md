# XRT Guide: xnet-v2 and TLS Mainline

> The recommended mental model for the current network stack: `xnet-v2` as the transport mainline and `xtlssession` as the single formal public TLS surface.

[中文](xnet-v2-tls-intro.md) | [Back to Guides](README.en.md)

---

## Mainline Summary

The current formal network mainline is:

- `xnet-v2`
- `xtlssession`
- `xhttp`
- `xhttpd`
- `xws`

Old public network surfaces are no longer the recommended long-term path.

---

## Why TLS Was Collapsed

The TLS mainline was intentionally collapsed so that the public surface now centers on:

- `xtlssession`
- `xrtNetTlsSession*`

This keeps the application-facing model aligned with:

- streams
- HTTP client/server
- WebSocket
- future / wait-source / coroutine integration

instead of carrying multiple public TLS layers forever.

---

## Recommended Understanding Order

1. read `xnet-v2`
2. understand TLS sessions
3. move to `xhttp`
4. move to `xhttpd`
5. move to `xws`

This is the cleanest way to enter the current networking mainline.

---

## Related Reading

- [XNet V2](../api/api-xnet-v2.en.md)
- [Network TLS](../api/api-network-tls.en.md)
- [XHTTP](../api/api-xhttp.en.md)
- [XHTTPD](../api/api-xhttpd.en.md)
- [XWS](../api/api-xws.en.md)
