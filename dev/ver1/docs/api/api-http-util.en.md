# HTTP Util Text and Multipart Utility Layer

> HTTP text parsing, validation, media-type, cookie, and multipart streaming utilities for HTTP / WebSocket / upload scenarios.

[Back to Index](README.en.md)

---

## 1. Positioning

`xhttp_util` is the text-and-structure helper layer for:

- HTTP token / header / query / cookie parsing
- format validation with limits
- `Set-Cookie` parsing
- `Content-Type` / media type parsing
- `Content-Disposition` parsing
- multipart validation
- incremental multipart stream parsing

This layer mainly serves:

- `xhttp`
- `xhttpd`
- `xws`
- upload handling
- header/query/cookie work in API and AI client code

---

## 2. Core Types

Important view/result types include:

- `xrtheaderpair`
- `xrtquerypair`
- `xrtcookiepair`
- `xrthttpparam`
- `xrtsetcookieview`
- `xrtmediatypeview`
- `xrtmultipartpartview`
- `xrtmultipartstream`

The common rule is:

- parsing usually returns borrowed views
- copy only when long-term ownership is needed

---

## 3. Main Capability Areas

### Limits and Validation

```c
XXAPI void xrtHttpUtilLimitsInit(xrthttputillimits* pLimits);
XXAPI bool xrtQueryValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtHttpHeaderBlockValidateN(const char* sBlock, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtMultipartValidateN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, const xrthttputillimits* pLimits);
```

### Iterative Parsing

```c
XXAPI bool xrtQueryNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut);
XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut);
```

### Media Type and Cookie Parsing

```c
XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut);
XXAPI bool xrtHttpMediaTypeParseN(const char* sText, size_t iLen, xrtmediatypeview* pOut);
XXAPI bool xrtHttpMediaTypeFindParamN(const xrtmediatypeview* pType, const char* sName, size_t iNameLen, xrthttpparam* pOut);
```

### Multipart Streaming

```c
XXAPI void xrtMultipartStreamConfigInit(xrtmultipartstreamconfig* pConfig);
XXAPI bool xrtMultipartStreamInit(xrtmultipartstream* pStream, const char* sBoundary, size_t iBoundaryLen, const xrtmultipartstreamconfig* pConfig);
XXAPI bool xrtMultipartStreamFeed(xrtmultipartstream* pStream, const void* pData, size_t iLen);
XXAPI xrtmultipartstreamresult xrtMultipartStreamNext(xrtmultipartstream* pStream, xrtmultipartstreamevent* pEvent);
XXAPI void xrtMultipartStreamFinish(xrtmultipartstream* pStream);
XXAPI void xrtMultipartStreamUnit(xrtmultipartstream* pStream);
```

---

## 4. Recommended Use

Use `xhttp_util` when:

- you need strict validation for untrusted HTTP input
- you want to iterate query/header/cookie blocks without copying everything
- you need to inspect `Content-Type` / `Set-Cookie`
- you need multipart parsing without requiring the whole body up front

For small bodies, one-shot validation and parsing are enough.  
For uploads or incremental receive paths, prefer `xrtMultipartStream*`.

---

## 5. Suggested Reading

- [XHTTP](api-xhttp.en.md)
- [XHTTPD](api-xhttpd.en.md)
- [XNet V2](api-xnet-v2.en.md)
