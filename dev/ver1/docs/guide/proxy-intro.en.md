# XRT Guide: Proxy Intro

> Goal: turn `xrtNetProxyConfigInit()`, `xrtNetProxyCreate()`, `xrtNetProxyAddRef()`, and `xrtNetProxyRelease()` into the formal Stage 6 mainline for shared proxy objects and connection-path shaping. After this page, you should clearly understand that in the current XRT mainline, a proxy is not a pile of module-specific special cases. It is one shared, refcounted network object that really inserts itself into the sequence `TCP -> proxy handshake -> TLS -> application protocol`.

[中文](proxy-intro.md) | [Back to Guides](README.en.md)

---

## 1. Why Does `proxy` Need Its Own Page?

In many projects, proxy support gets written like this:

- one HTTP client grows a few proxy fields
- WebSocket copies them again
- the lower TCP layer adds yet another branch

The result is:

- client and WebSocket behavior drift apart
- proxy-object lifetime becomes messy
- direct-connect and proxied-connect sequencing get mixed together

The current XRT mainline is not organized that way. It closes the concept into:

- `xnetproxy`

That means the proxy is first one shared object, and only later gets attached to:

- `xnetconnectconfig`
- `xhttprequest`
- `xwsclientconfig`


## 2. Separate 7 Boundaries First

### 2.1 The Current Proxy Mainline Supports 2 CONNECT-Style Proxy Families

The current formal mainline supports:

- `XNET_PROXY_SOCKS5`
- `XNET_PROXY_HTTP_CONNECT`

So the current focus is:

- letting connection-oriented streams establish a path to the final target behind a proxy

not:

- arbitrary HTTP reverse-proxy syntax
- browser PAC / auto-discovery
- a higher-level proxy policy system


### 2.2 `xnetproxy` Is a Shared Object, Not a Config That Gets Re-Copied per Request

In the current API design:

- `xrtNetProxyCreate()`
	- creates one shared proxy object
- `xrtNetProxyAddRef()`
	- increases the reference count
- `xrtNetProxyRelease()`
	- releases one reference

The steadier mental model is:

- close proxy settings into one object first
- let requests or connection configs retain their own reference later

That is much steadier than copying `host/port/user/pass` separately in each layer.


### 2.3 Direct Connect and Proxied Connect Are Not Two Different Client APIs

In the current mainline:

- `pProxy == NULL`
	- direct connect
- `pProxy != NULL`
	- proxied connect path

This matters because it means:

- you do not learn two unrelated client interfaces
- you only attach one extra shared object to the connection config


### 2.4 The Proxy Really Inserts Itself into the Connection Sequence

If `pProxy` is configured, the right mental model is:

1. connect to the proxy server first
2. complete the proxy handshake
3. if the target is a secure scheme, perform TLS next
4. only then enter HTTP / WebSocket / other application protocol logic

In short:

- `TCP -> proxy handshake -> TLS -> HTTP/WS`

That is why proxy support cannot be reduced to "append one extra header before sending the request".


### 2.5 Proxy Lifetime Must Be Separated from Request/Connection References

This is one of the easiest mistakes in the current mainline.

For example:

```c
pProxy = xrtNetProxyCreate(&tCfg);
tReq.pProxy = xrtNetProxyAddRef(pProxy);
```

That already means there are two distinct references:

- one held by the outer owner
- one held by the request object

So cleanup must close both references explicitly instead of assuming one `Release()` is enough.


### 2.6 The Proxy Does Not Parse the Target URL for You

The proxy solves:

- how the connection path is shaped

But the target still has to be known first:

- host
- port
- scheme

So the natural order is:

1. parse with `xurl`
2. decide whether a proxy object should be attached
3. then enter `xhttp / xws / xnet-v2`


### 2.7 User and Password Belong to Proxy Config, Not to Ad-Hoc Request Header Assembly

The current `xnetproxyconfig` already contains:

- `sUser`
- `sPass`

That means proxy authentication should be configured at the proxy-object layer first, not scattered through application request logic.


## 3. What the Current Config Object Looks Like

The most important public pieces are:

```c
#define XNET_PROXY_SOCKS5       1
#define XNET_PROXY_HTTP_CONNECT 2

typedef struct {
	int iType;
	char sHost[XNET_PROXY_HOST_CAP];
	uint16 iPort;
	char sUser[XNET_PROXY_USER_CAP];
	char sPass[XNET_PROXY_PASS_CAP];
} xnetproxyconfig;
```

That means one proxy configuration is fundamentally:

1. the proxy type
2. the proxy host
3. the proxy port
4. optional authentication data


## 4. Minimal Demo: Create the Shared Proxy Object Correctly

Do not send any requests yet.  
First learn the lifecycle:

```c
xnetproxyconfig tProxyCfg;
xnetproxy* pProxy;

xrtNetProxyConfigInit(&tProxyCfg);
tProxyCfg.iType = XNET_PROXY_HTTP_CONNECT;
strcpy(tProxyCfg.sHost, "127.0.0.1");
tProxyCfg.iPort = 7897u;

pProxy = xrtNetProxyCreate(&tProxyCfg);
if ( pProxy == NULL ) {
	return 1;
}

/* later attach pProxy to requests or connection configs */

xrtNetProxyRelease(pProxy);
```

The point here is not whether the proxy handshake succeeds yet.  
The point is to build the correct lifetime model first.


## 5. Upgraded Demo: Attach the Proxy to `xhttp` and `xws`

Only in the second step should the proxy be connected to real client code.

### 5.1 `xhttp`

```c
tReq.pProxy = xrtNetProxyAddRef(pProxy);
```

### 5.2 `xws`

```c
tCfg.pProxy = xrtNetProxyAddRef(pProxy);
```

The steadier habit is:

1. create the proxy object once at the outer layer
2. let each request or client config retain its own reference
3. let the request/connection object release its own reference during cleanup
4. let the outer owner release its final reference at the end


## 6. When Should `proxy` Be Considered First?

| Scenario | Should proxy be considered first? |
|----------|-----------------------------------|
| enterprise outbound access to external APIs | yes |
| route HTTPS client traffic through HTTP CONNECT | yes |
| WebSocket client egress through SOCKS5 | yes |
| server-side listening on a local port | no |
| only URL parsing or header processing | no |


## 7. Common Mistakes

### 7.1 Building Proxy Support Only as Extra Headers at the Application Layer

That hides the real connection-sequencing problem instead of solving it.


### 7.2 Calling `Release()` Only Once

If extra `AddRef()` calls happened, the outer owner and the request/connection owner are not the same reference.


### 7.3 Deciding the Proxy Target Before the URL Was Parsed Correctly

Proxy decisions still require:

- scheme
- host
- port

to be known first.


### 7.4 Treating Direct Connect and Proxy Connect as Two Completely Separate Client Code Paths

One of the mainline advantages is exactly this:

- the connection object stays the same
- only `pProxy` changes between `NULL` and non-`NULL`


## 8. What to Read Next

Follow this line next:

- [XURL Intro: Why Correct URL Splitting Matters Before Sending Requests](xurl-intro.en.md)
- [HTTP Util Intro: Headers, Query, Cookies, and Multipart Are Not Just Strings](http-util-intro.en.md)
- [Case: XHTTP Client Chain with URL, Proxy, and TLS](../case/xhttp-client-proxy-tls.md) (Chinese for now)
- [XNet V2 API](../api/api-xnet-v2.en.md)
