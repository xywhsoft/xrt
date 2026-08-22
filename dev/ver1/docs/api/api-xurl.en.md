# XURL URL and Query Utility Layer

> URL / authority / target parsing and building utilities for the current network mainline.

[Back to Index](README.en.md)

---

## 1. Positioning

`xurl` solves:

- parsing complete URLs
- parsing authority (`userinfo@host:port`)
- parsing request targets (`/path?query#fragment`)
- checking secure schemes and default ports
- building `Host` headers
- path normalization
- resolving relative references against a base URL

It mainly serves:

- `xhttp`
- `xhttpd`
- `xws`
- `xnet-v2`

---

## 2. Core Types

### `xrtstrview`

```c
typedef struct {
	const char* sPtr;
	size_t iLen;
} xrtstrview;
```

This is a borrowed string view.

### `xrturlview`

`xrturlview` carries borrowed views for:

- scheme
- authority
- host
- path
- query
- fragment
- port
- flags

If you need long-term ownership, copy fields out explicitly.

---

## 3. Main API Groups

### Parse APIs

```c
XXAPI bool xrtUrlParseAuthorityN(const char* sText, size_t iLen, xrturlview* pOut);
XXAPI bool xrtUrlParseTargetN(const char* sText, size_t iLen, xrturlview* pOut);
XXAPI bool xrtUrlParseViewN(const char* sText, size_t iLen, xrturlview* pOut);
XXAPI bool xrtUrlParse(const char* sURL, xurl pOut);
```

### Query / Check APIs

```c
XXAPI uint16 xrtUrlDefaultPort(xrtstrview tScheme);
XXAPI bool xrtUrlIsSecureScheme(xrtstrview tScheme);
XXAPI bool xrtUrlIsDefaultPort(const xrturlview* pURL);
XXAPI bool xrtUrlViewIsScheme(const xrturlview* pURL, const char* sScheme);
```

### Copy / Build APIs

```c
XXAPI bool xrtUrlViewCopyHostTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlViewCopyTargetTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlBuild(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
XXAPI bool xrtUrlMakeHostHeader(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlResolveTo(const xrturlview* pBase, const char* sRef, size_t iRefLen, char* sOut, size_t iOutCap, size_t* pOutLen);
```

---

## 4. Suggested Reading

- [XHTTP](api-xhttp.en.md)
- [XWS](api-xws.en.md)
- [XNet V2](api-xnet-v2.en.md)
