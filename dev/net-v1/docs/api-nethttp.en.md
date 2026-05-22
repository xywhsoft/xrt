# NetHTTP HTTP Client

> HTTP/1.1 client based on Socket + TLS, supports HTTPS, redirects, and chunked transfer

[English](api-nethttp.en.md) | [中文](api-nethttp.md) | [Back to Index](README.md)

---

## 📑 Table of Contents

- [Feature Overview](#feature-overview)
- [Data Types](#data-types)
- [Constants](#constants)
- [URL Parsing](#url-parsing)
- [Simple API](#simple-api)
- [Full API - Request Object](#full-api---request-object)
- [Full API - Response Object](#full-api---response-object)
- [Cookie Management](#cookie-management)
- [Multipart Forms](#multipart-forms)
- [Usage Examples](#usage-examples)

---

## Feature Overview

**NetHTTP** is a fully-featured HTTP/1.1 client with the following features:

- 🌐 **HTTP/HTTPS Support**: Based on nettcp.h + nettls.h
- 🔄 **Auto Redirect**: Supports 301/302/303/307/308 redirects
- 📦 **Chunked Transfer**: Supports Transfer-Encoding: chunked
- 🍪 **Cookie Management**: Automatic Set-Cookie handling and sending
- 📝 **Multipart Forms**: Supports form fields and file uploads
- 🎯 **Progress Callback**: Supports data receive progress callback
- ⚡ **Synchronous Blocking Model**: Simple and easy-to-use blocking API
- 🔒 **Optional SSL Verification**: Supports certificate verification toggle

**Dependencies**:
- `netsock.h` - Socket foundation
- `nettls.h` - TLS protocol
- `buffer.h` - Auto-growing buffer

---

## Data Types

### xhttpreq

HTTP request object, used for building and sending HTTP requests.

**Definition**:
```c
typedef struct xhttpreq_struct* xhttpreq;
```

**Memory Management**:
- Create: `xrtHttpReqCreate()`
- Free: `xrtHttpReqFree()`

### xhttpresp

HTTP response object containing status, headers, and body.

**Definition**:
```c
typedef struct xhttpresp_struct* xhttpresp;
```

**Memory Management**:
- Free: `xrtHttpRespFree()` - Must be called

### xurl

URL parsing result structure.

**Definition**:
```c
typedef struct xurl_struct {
    bool bHttps;          // Is HTTPS
    char sHost[256];      // Hostname
    uint16 iPort;         // Port number
    char sPath[2048];     // Path (including query parameters)
} xurl_struct, *xurl;
```

### xhttp_method

HTTP request method enumeration.

**Definition**:
```c
typedef enum {
    XHTTP_GET,     // GET request
    XHTTP_POST,    // POST request
    XHTTP_PUT,     // PUT request
    XHTTP_DELETE,  // DELETE request
    XHTTP_PATCH,   // PATCH request
    XHTTP_HEAD     // HEAD request
} xhttp_method;
```

### xhttp_proc

HTTP data receive progress callback function type.

**Definition**:
```c
typedef bool (*xhttp_proc)(xbuffer pData, size_t iTotal, size_t iReceived);
```

**Parameters**:
- `pData` - Current received data buffer
- `iTotal` - Total data size (Content-Length, 0 if unknown)
- `iReceived` - Received data size

**Return Value**:
- `true` - Continue receiving
- `false` - Abort receiving

---

## Constants

### Default Configuration

| Constant | Value | Description |
|----------|-------|-------------|
| Default timeout | 10 seconds | Connection and read timeout |
| Default redirect count | 5 times | Maximum redirect count |
| Default SSL verification | true | Whether to verify SSL certificate |

---

## URL Parsing

### Function: xrtUrlParse

**Function Prototype**:
```c
XXAPI bool xrtUrlParse(str sURL, xurl pOut);
```

**Description**:
Parse a URL string into an xurl structure.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `sURL` | `str` | URL string | No |
| `pOut` | `xurl` | Output structure pointer | No |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Parse successful |
| `false` | Parse failed |

**Example**:
```c
xurl_struct tURL;
if (xrtUrlParse("https://example.com:443/api/v1/users?id=123", &tURL)) {
    printf("Host: %s\n", tURL.sHost);
    printf("Port: %d\n", tURL.iPort);
    printf("Path: %s\n", tURL.sPath);
    printf("HTTPS: %s\n", tURL.bHttps ? "yes" : "no");
}
```

---

## Simple API

### Function: xrtHttpGet

**Function Prototype**:
```c
XXAPI xhttpresp xrtHttpGet(str sURL, str sHeaders, xhttp_proc pProc);
```

**Description**:
Send an HTTP GET request.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `sURL` | `str` | Request URL | No |
| `sHeaders` | `str` | Custom request headers | Yes |
| `pProc` | `xhttp_proc` | Progress callback function | Yes |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `Non-NULL` | Response object (must be freed with `xrtHttpRespFree`) |
| `NULL` | Request failed |

**Example**:
```c
xhttpresp resp = xrtHttpGet("https://api.example.com/users", NULL, NULL);
if (resp) {
    printf("Status: %d\n", xrtHttpRespCode(resp));
    printf("Body: %s\n", xrtHttpRespBody(resp));
    xrtHttpRespFree(resp);
}
```

### Function: xrtHttpPost

**Function Prototype**:
```c
XXAPI xhttpresp xrtHttpPost(str sURL, str sBody, str sHeaders, xhttp_proc pProc);
```

**Description**:
Send an HTTP POST request.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `sURL` | `str` | Request URL | No |
| `sBody` | `str` | Request body | No |
| `sHeaders` | `str` | Custom request headers | Yes |
| `pProc` | `xhttp_proc` | Progress callback function | Yes |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `Non-NULL` | Response object (must be freed with `xrtHttpRespFree`) |
| `NULL` | Request failed |

**Example**:
```c
str json = "{\"name\":\"Alice\",\"age\":25}";
xhttpresp resp = xrtHttpPost(
    "https://api.example.com/users",
    json,
    "Content-Type: application/json\r\n",
    NULL
);
if (resp) {
    printf("Status: %d\n", xrtHttpRespCode(resp));
    printf("Body: %s\n", xrtHttpRespBody(resp));
    xrtHttpRespFree(resp);
}
xrtFree(json);
```

### Function: xrtHttpGetFile

**Function Prototype**:
```c
XXAPI bool xrtHttpGetFile(str sURL, str sFilePath, str sHeaders, xhttp_proc pProc);
```

**Description**:
Download a file to a local path.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `sURL` | `str` | Download URL | No |
| `sFilePath` | `str` | Save path | No |
| `sHeaders` | `str` | Custom request headers | Yes |
| `pProc` | `xhttp_proc` | Progress callback function | Yes |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Download successful (HTTP status code 200-299) |
| `false` | Download failed |

**Example**:
```c
bool ok = xrtHttpGetFile(
    "https://example.com/file.zip",
    "file.zip",
    NULL,
    NULL
);
if (ok) {
    printf("Download successful!\n");
} else {
    printf("Download failed!\n");
}
```

### Function: xrtHttpPostFile

**Function Prototype**:
```c
XXAPI bool xrtHttpPostFile(str sURL, str sBody,
    str sFilePath, str sHeaders, xhttp_proc pProc);
```

**Description**:
Send a POST request and save the response to a file.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `sURL` | `str` | Request URL | No |
| `sBody` | `str` | Request body | Yes |
| `sFilePath` | `str` | Save path | No |
| `sHeaders` | `str` | Custom request headers | Yes |
| `pProc` | `xhttp_proc` | Progress callback function | Yes |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Request successful (HTTP status code 200-299) |
| `false` | Request failed |

### Function: xrtHttpRespFree

**Function Prototype**:
```c
XXAPI void xrtHttpRespFree(xhttpresp pResp);
```

**Description**:
Free the response object and its associated memory.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pResp` | `xhttpresp` | Response object | Yes |

---

## Full API - Request Object

### Function: xrtHttpReqCreate

**Function Prototype**:
```c
XXAPI xhttpreq xrtHttpReqCreate(xhttp_method iMethod, str sURL);
```

**Description**:
Create an HTTP request object.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `iMethod` | `xhttp_method` | HTTP request method | - |
| `sURL` | `str` | Request URL | No |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `Non-NULL` | Request object (must be freed with `xrtHttpReqFree`) |
| `NULL` | Create failed |

### Function: xrtHttpReqFree

**Function Prototype**:
```c
XXAPI void xrtHttpReqFree(xhttpreq pReq);
```

**Description**:
Free the request object and its associated memory.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | Yes |

### Function: xrtHttpReqSetHeader

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetHeader(xhttpreq pReq, str sName, str sValue);
```

**Description**:
Set a request header.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `sName` | `str` | Header name | No |
| `sValue` | `str` | Header value | No |

**Example**:
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/data");
xrtHttpReqSetHeader(req, "Authorization", "Bearer token123");
xrtHttpReqSetHeader(req, "Accept", "application/json");
// ... execute request
xrtHttpReqFree(req);
```

### Function: xrtHttpReqSetBody

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetBody(xhttpreq pReq, str pData, size_t iLen, str sContentType);
```

**Description**:
Set request body and Content-Type.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `pData` | `str` | Request body data | No |
| `iLen` | `size_t` | Data length | - |
| `sContentType` | `str` | Content-Type value | Yes |

**Example**:
```c
str json = "{\"name\":\"Alice\"}";
xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/users");
xrtHttpReqSetBody(req, json, strlen(json), "application/json");
// ... execute request
xrtHttpReqFree(req);
xrtFree(json);
```

### Function: xrtHttpReqAddField

**Function Prototype**:
```c
XXAPI void xrtHttpReqAddField(xhttpreq pReq, str sName, str sValue);
```

**Description**:
Add a URL-encoded form field (for x-www-form-urlencoded).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `sName` | `str` | Field name | No |
| `sValue` | `str` | Field value | No |

**Description**:
- Field names and values are automatically URL-encoded
- Multiple fields are automatically connected with `&`
- Content-Type will be automatically set to `application/x-www-form-urlencoded` after using this function

**Example**:
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/form");
xrtHttpReqAddField(req, "username", "alice@example.com");
xrtHttpReqAddField(req, "password", "secret123");
// ... execute request
xrtHttpReqFree(req);
```

### Function: xrtHttpReqSetTimeout

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetTimeout(xhttpreq pReq, int iTimeoutSec);
```

**Description**:
Set connection and read timeout.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `iTimeoutSec` | `int` | Timeout in seconds | - |

### Function: xrtHttpReqSetRedirect

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetRedirect(xhttpreq pReq, int iMaxRedirects);
```

**Description**:
Set maximum number of redirects.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `iMaxRedirects` | `int` | Maximum number of redirects (0 = don't follow redirects) | - |

### Function: xrtHttpReqSetVerifySSL

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetVerifySSL(xhttpreq pReq, bool bVerify);
```

**Description**:
Set whether to verify SSL certificates.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `bVerify` | `bool` | Whether to verify certificates | - |

**Description**:
- Production environment recommends setting to `true`
- Test environment can be set to `false` to avoid certificate issues

### Function: xrtHttpReqSetCallback

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetCallback(xhttpreq pReq, xhttp_proc pProc);
```

**Description**:
Set data receive progress callback function.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `pProc` | `xhttp_proc` | Callback function | Yes |

**Example**:
```c
bool progress_cb(xbuffer pData, size_t iTotal, size_t iReceived) {
    printf("Progress: %zu/%zu (%.1f%%)\n", iReceived, iTotal, iTotal > 0 ? iReceived * 100.0 / iTotal : 0);
    return true;  // Continue receiving
}

xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://example.com/file.zip");
xrtHttpReqSetCallback(req, progress_cb);
// ... execute request
xrtHttpReqFree(req);
```

### Function: xrtHttpReqSetUserData

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetUserData(xhttpreq pReq, ptr pData);
```

**Description**:
Set user data pointer (for use in callbacks).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `pData` | `ptr` | User data pointer | Yes |

### Function: xrtHttpReqExecute

**Function Prototype**:
```c
XXAPI xhttpresp xrtHttpReqExecute(xhttpreq pReq);
```

**Description**:
Execute HTTP request.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `Non-NULL` | Response object (must be freed with `xrtHttpRespFree`) |
| `NULL` | Request failed |

**Example**:
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/users");
xrtHttpReqSetHeader(req, "Authorization", "Bearer token123");
xhttpresp resp = xrtHttpReqExecute(req);
if (resp) {
    printf("Status: %d\n", xrtHttpRespCode(resp));
    printf("Body: %s\n", xrtHttpRespBody(resp));
    xrtHttpRespFree(resp);
}
xrtHttpReqFree(req);
```

---

## Full API - Response Object

### Function: xrtHttpRespCode

**Function Prototype**:
```c
XXAPI int xrtHttpRespCode(xhttpresp pResp);
```

**Description**:
Get HTTP status code.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pResp` | `xhttpresp` | Response object | No |

**Return Value**:
HTTP status code (e.g., 200, 404, 500, etc.)

### Function: xrtHttpRespBody

**Function Prototype**:
```c
XXAPI str xrtHttpRespBody(xhttpresp pResp);
```

**Description**:
Get response body.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pResp` | `xhttpresp` | Response object | No |

**Return Value**:
Response body string (no need to free)

### Function: xrtHttpRespBodyLen

**Function Prototype**:
```c
XXAPI size_t xrtHttpRespBodyLen(xhttpresp pResp);
```

**Description**:
Get response body length.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pResp` | `xhttpresp` | Response object | No |

**Return Value**:
Response body length (bytes)

### Function: xrtHttpRespHeader

**Function Prototype**:
```c
XXAPI str xrtHttpRespHeader(xhttpresp pResp, str sName);
```

**Description**:
Get response header value (case-insensitive).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pResp` | `xhttpresp` | Response object | No |
| `sName` | `str` | Header name | No |

**Return Value**:
Response header value (temporary buffer, no need to free)

**Example**:
```c
str ct = xrtHttpRespHeader(resp, "Content-Type");
str cl = xrtHttpRespHeader(resp, "Content-Length");
printf("Content-Type: %s\n", ct);
printf("Content-Length: %s\n", cl);
```

### Function: xrtHttpRespCookie

**Function Prototype**:
```c
XXAPI str xrtHttpRespCookie(xhttpresp pResp, str sName);
```

**Description**:
Get Cookie value from response.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pResp` | `xhttpresp` | Response object | No |
| `sName` | `str` | Cookie name | No |

**Return Value**:
Cookie value (temporary buffer, no need to free)

### Function: xrtHttpRespContentType

**Function Prototype**:
```c
XXAPI str xrtHttpRespContentType(xhttpresp pResp);
```

**Description**:
Get response Content-Type.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pResp` | `xhttpresp` | Response object | No |

**Return Value**:
Content-Type value (no need to free)

---

## Cookie Management

### Function: xrtHttpReqEnableCookies

**Function Prototype**:
```c
XXAPI void xrtHttpReqEnableCookies(xhttpreq pReq, bool bEnable);
```

**Description**:
Enable or disable cookie management.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `bEnable` | `bool` | Whether to enable | - |

**Description**:
- When enabled, Set-Cookie in responses will be automatically saved
- Subsequent requests will automatically send saved cookies

### Function: xrtHttpReqSetCookie

**Function Prototype**:
```c
XXAPI void xrtHttpReqSetCookie(xhttpreq pReq, str sName, str sValue);
```

**Description**:
Set request cookie.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `sName` | `str` | Cookie name | No |
| `sValue` | `str` | Cookie value | No |

### Function: xrtHttpReqRemoveCookie

**Function Prototype**:
```c
XXAPI void xrtHttpReqRemoveCookie(xhttpreq pReq, str sName);
```

**Description**:
Remove request cookie.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `sName` | `str` | Cookie name | No |

---

## Multipart Forms

### Function: xrtHttpReqAddFormField

**Function Prototype**:
```c
XXAPI void xrtHttpReqAddFormField(xhttpreq pReq, str sName, str sValue);
```

**Description**:
Add multipart form field.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `sName` | `str` | Field name | No |
| `sValue` | `str` | Field value | No |

**Description**:
- Using this function automatically enables Multipart mode
- Content-Type will be automatically set to `multipart/form-data; boundary=...`

### Function: xrtHttpReqAddFormFile

**Function Prototype**:
```c
XXAPI void xrtHttpReqAddFormFile(xhttpreq pReq, str sFieldName,
    str sFilePath, str sMimeType);
```

**Description**:
Add file upload field.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `sFieldName` | `str` | Form field name | No |
| `sFilePath` | `str` | File path | No |
| `sMimeType` | `str` | MIME type (optional) | Yes |

**Description**:
- File is automatically read into memory
- If MIME type is not specified, defaults to `application/octet-stream`

**Example**:
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/upload");
xrtHttpReqAddFormField(req, "description", "A file");
xrtHttpReqAddFormFile(req, "file", "data.txt", "text/plain");
// ... execute request
xrtHttpReqFree(req);
```

### Function: xrtHttpReqAddFormData

**Function Prototype**:
```c
XXAPI void xrtHttpReqAddFormData(xhttpreq pReq, str sFieldName,
    str sFileName, str pData, size_t iLen, str sMimeType);
```

**Description**:
Add in-memory data as file upload.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pReq` | `xhttpreq` | Request object | No |
| `sFieldName` | `str` | Form field name | No |
| `sFileName` | `str` | Filename | Yes |
| `pData` | `str` | Data pointer | No |
| `iLen` | `size_t` | Data length | - |
| `sMimeType` | `str` | MIME type (optional) | Yes |

---

## Usage Examples

### Example 1: RESTful API Call

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

int main() {
    xrtInit();

    // GET request
    xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/users/1");
    xrtHttpReqSetHeader(req, "Authorization", "Bearer token123");
    xrtHttpReqSetHeader(req, "Accept", "application/json");

    xhttpresp resp = xrtHttpReqExecute(req);
    if (resp && xrtHttpRespCode(resp) == 200) {
        printf("User Data: %s\n", xrtHttpRespBody(resp));
    }

    xrtHttpRespFree(resp);
    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

### Example 2: File Download

```c
bool progress_cb(xbuffer pData, size_t iTotal, size_t iReceived) {
    printf("\rDownloaded: %zu/%zu bytes", iReceived, iTotal);
    fflush(stdout);
    return true;
}

int main() {
    xrtInit();

    xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://example.com/file.zip");
    xrtHttpReqSetCallback(req, progress_cb);
    xrtHttpReqSetTimeout(req, 30);

    xhttpresp resp = xrtHttpReqExecute(req);
    if (resp && xrtHttpRespCode(resp) == 200) {
        // Save file
        FILE* fp = fopen("file.zip", "wb");
        if (fp) {
            fwrite(xrtHttpRespBody(resp), 1, xrtHttpRespBodyLen(resp), fp);
            fclose(fp);
            printf("\nDownload complete!\n");
        }
    }

    xrtHttpRespFree(resp);
    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

### Example 3: Multipart Form Upload

```c
int main() {
    xrtInit();

    xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/upload");
    xrtHttpReqAddFormField(req, "username", "alice");
    xrtHttpReqAddFormField(req, "description", "Profile picture");

    // Upload file
    xrtHttpReqAddFormFile(req, "avatar", "avatar.jpg", "image/jpeg");

    xhttpresp resp = xrtHttpReqExecute(req);
    if (resp) {
        printf("Status: %d\n", xrtHttpRespCode(resp));
        printf("Response: %s\n", xrtHttpRespBody(resp));
    }

    xrtHttpRespFree(resp);
    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

### Example 4: Using Cookies

```c
int main() {
    xrtInit();

    xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/profile");
    xrtHttpReqEnableCookies(req, true);

    // First request, will automatically save Set-Cookie from response
    xhttpresp resp1 = xrtHttpReqExecute(req);
    xrtHttpRespFree(resp1);

    // Second request, will automatically send saved cookies
    xhttpresp resp2 = xrtHttpReqExecute(req);
    if (resp2) {
        printf("Status: %d\n", xrtHttpRespCode(resp2));
        printf("Response: %s\n", xrtHttpRespBody(resp2));
        xrtHttpRespFree(resp2);
    }

    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

---

## Notes

1. **Memory Management**:
   - Request and response objects must be freed with corresponding Free functions
   - Cookie strings are automatically freed when freeing the request object
   - Response header and cookie return values use temporary buffers, no need to free

2. **Timeout Settings**:
   - Connection timeout and read timeout use the same timeout value
   - Too short timeout may cause large file downloads to fail

3. **Redirects**:
   - 301/302/303 are automatically converted to GET requests
   - 307/308 preserve the original request method and request body

4. **HTTPS**:
   - SSL certificate verification is enabled by default
   - Test environment can use `xrtHttpReqSetVerifySSL(req, false)` to disable verification

5. **Performance**:
   - Synchronous blocking model, not suitable for high-concurrency scenarios
   - Large file downloads recommend using progress callbacks

---

## Related Functions

- [xrtSockCreate](api-nettcp.md) - Create Socket
- [xrtTlsCreate](api-nettls.md) - Create TLS Context
- [xrtBufferInit](api-buffer.md) - Initialize Buffer
- [xrtDictCreate](api-dict.md) - Create Dictionary (for Cookie storage)

---

<div align="center">

**XRT HTTP Client** | Version 1.0 | Last Updated: 2025-01

</div>
