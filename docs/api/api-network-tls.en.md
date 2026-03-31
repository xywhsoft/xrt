# Network TLS Session Layer

> The current formal TLS mainline. The public surface is unified around `xtlssession / xrtNetTlsSession*`. The old `xtlsctx / xrtTls*` surface is no longer part of the formal public mainline.

[Back to Index](README.en.md)

---

## 1. Positioning

The current TLS mainline in XRT is split like this:

- internal core implementation: still carried by [nettls.h](../../lib/nettls.h)
- formal public surface: `xtlssession` and `xrtNetTlsSession*`

That means the new network mainline, HTTP client, HTTP server, and WebSocket all work around the session layer instead of exposing the TLS core context directly.

---

## 2. Core Types

### `xtlssession`

```c
typedef struct xrt_tls_session xtlssession;
```

This is the formal TLS session object. It is responsible for:

- handshake driving
- cipher input
- pending cipher output
- plaintext write
- plaintext read
- close queueing
- session resume export
- SNI access

### `xtlsconfig`

TLS configuration is still provided through `xtlsconfig`, but public use is now centered on the session layer.

In particular:

- `OnSNI` now receives `xtlssession*`

instead of the old TLS core context type.

---

## 3. Official API

### Create and Destroy

```c
XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer);
XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession);
```

### Handshake and State

```c
XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession);
```

### Cipher Input and Output

```c
XXAPI xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen);
XXAPI size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
XXAPI void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen);
```

### Plaintext Input and Output

```c
XXAPI size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten);
XXAPI xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead);
```

### Close, Resume, and SNI

```c
XXAPI xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession);
XXAPI xtlsresume* xrtNetTlsSessionExportResume(const xtlssession* pSession);
XXAPI void xrtNetTlsResumeDestroy(xtlsresume* pResume);
XXAPI bool xrtNetTlsSessionWasResumed(const xtlssession* pSession);
XXAPI const char* xrtNetTlsSessionGetSNI(const xtlssession* pSession);
XXAPI xnet_result xrtNetTlsSessionSetCert(xtlssession* pSession, const char* sCertFile, const char* sKeyFile);
XXAPI void xrtNetTlsSessionSetAllowTLS12Ed25519(xtlssession* pSession, bool bAllow);
```

---

## 4. Recommended Usage

Use direct session driving when you need explicit TLS boundary control.  
For most real service or client code, the preferred path is to stand on:

- `xnet_stream`
- `xhttp`
- `xhttpd`
- `xws`

and let those layers carry the session mainline for you.

---

## 5. Why `xtlsctx / xrtTls*` Is No Longer the Public Mainline

The implementation still exists internally, but:

- it is better treated as an internal TLS core layer
- the new network mainline already converged on the session abstraction
- keeping two public TLS surfaces would create historical baggage

So the current formal docs only keep the session mainline.

---

## 6. Suggested Reading

- [XNet V2](api-xnet-v2.en.md)
- [XHTTP](api-xhttp.en.md)
- [XHTTPD](api-xhttpd.en.md)
- [XWS](api-xws.en.md)

