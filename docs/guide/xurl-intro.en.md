# XRT Guide: XURL Intro

> Goal: turn `xrtUrlParseView()`, `xrtUrlParseFixedTo()`, `xrtUrlMakeHostHeader()`, `xrtUrlNormalizePathTo()`, and `xrtUrlResolveTo()` into the formal Stage 6 starting point for address parsing and URL construction. After this page, you should clearly understand that URL, authority, and target are not the same thing, and that many connection failures, wrong-host requests, and broken `Host` headers are not caused by `xhttp` itself. They are caused by a URL that was already split incorrectly before networking even started.

[中文](xurl-intro.md) | [Back to Guides](README.en.md)

---

## 1. Why Learn `xurl` Before Jumping Straight to `xhttp`?

Many network bugs look like this on the surface:

- cannot connect
- TLS handshake fails
- proxy forwarding fails
- the server returns 400

But the real root cause often happens earlier:

- the `scheme` was identified incorrectly
- `host` and `target` were mixed together
- the default port was handled incorrectly
- the `Host` header no longer matches the real connection target
- a relative reference was resolved incorrectly against the base URL

In other words, before a real connection exists, a more basic question already exists:

- what does this URL string actually mean?

That is where `xurl` sits.


## 2. Separate 8 Boundaries First

### 2.1 Full URL, Authority, and Target Are Not the Same Thing

Split these three apart first:

| Name | Typical content |
|------|-----------------|
| Full URL | `https://user@host:8443/path?q=1#frag` |
| Authority | `user@host:8443` |
| Target | `/path?q=1#frag` |

That is why the current API separates them into:

- `xrtUrlParseAuthority*()`
- `xrtUrlParseTarget*()`
- `xrtUrlParseView*()`

If you only need the HTTP request target, do not parse it as if it were a full URL first.  
If you are building proxy targets, `Host` headers, or connection destinations, authority is the real focus.


### 2.2 `xrturlview` Borrows the Original Input by Default

The core return type of the current `xurl` layer is:

- `xrturlview`

Internally it is built from several:

- `xrtstrview`

So this family behaves more like:

- parsing borrowed views

not:

- allocating a new owned string object for you

If you need to keep fields for longer, copy them out explicitly instead of treating the view as an independent heap object.


### 2.3 `scheme` Is Not Only a Prefix; It Affects Default Port and TLS Choice

Several current helpers tied to `scheme` are:

- `xrtUrlDefaultPort()`
- `xrtUrlIsSecureScheme()`
- `xrtUrlIsDefaultPort()`
- `xrtUrlViewIsScheme()`
- `xrtUrlViewMatchesScheme2()`

They directly affect:

- whether the default port becomes `80 / 443`
- whether the connection should be treated as secure
- whether the port should be omitted in the final `Host` header

So `scheme` is not just a string fragment for logs. It changes how the connection path is built.


### 2.4 `Host` Headers Should Not Be Hand-Built, Especially with Default Ports and IPv6

This style is very common and very easy to get wrong:

```c
sprintf(sHostHeader, "%s:%u", sHost, iPort);
```

The problems are:

- default ports should not always be emitted
- IPv6 hosts need brackets
- `http` / `https` / `ws` / `wss` do not share the same default-port rule

The steadier entry points are:

- `xrtUrlMakeHostHeader()`
- `xrtUrlMakeHostHeaderFixed()`


### 2.5 Path Normalization Is Not Cosmetic String Cleanup

`xrtUrlNormalizePathTo()` solves:

- duplicate separators
- `.` / `..`
- closing relative path segments correctly

This matters because it affects:

- route matching
- cache keys
- base-URL resolution

It is not only about making a path string look prettier.


### 2.6 Relative References Should Be Resolved Against a Base URL

If you already have:

- a base URL
- a relative reference

the steadier entries are:

- `xrtUrlResolveTo()`
- `xrtUrlResolve()`

Do not hand-roll it by:

- trimming the last `/`
- then concatenating strings

because:

- query / fragment handling
- directory-level semantics
- root-path behavior

are more subtle than they look.


### 2.7 `ParseFixedTo()` Fits Fast Paths; `ParseView()` Fits Full URL Handling

A useful way to read the current entry points is:

| Entry | Best fit |
|-------|----------|
| `xrtUrlParseView*()` | you need complete fields and later build / normalize / resolve operations |
| `xrtUrlParseFixedTo()` | you only want fast access to `host / port / target` |
| `xrtUrlParse()` | you want to fill the fixed `xurl` struct directly |

If your goal is only:

- verify that it is `http/https`
- extract `host / port / target` for connection setup

then `xrtUrlParseFixedTo()` is usually the shortest route.


### 2.8 `xurl` Owns the Address Layer, Not the Fine-Grained HTTP Text Blocks

The easiest confusion here is between `xurl` and `xhttp_util`.

A useful split is:

| Module | Responsibility |
|--------|----------------|
| `xurl` | split, normalize, resolve, and rebuild URL / authority / target |
| `xhttp_util` | handle header / query / cookie / multipart text blocks |

So:

- `?a=1&b=2`
	- `xurl` extracts the query segment as a whole
- each pair like `a=1`
	- `xhttp_util` parses the inside of that segment


## 3. Minimal Demo: Split a Full URL into the 4 Pieces a Client Actually Needs

Do not send a request yet.  
First learn how to get:

1. `scheme`
2. `host`
3. `port`
4. `target`

```c
xrturlview tURL;
char sHost[256];
char sTarget[1024];

if ( xrtUrlParseView("https://api.example.com:8443/v1/chat?q=1", &tURL) ) {
	xrtUrlViewCopyHostTo(&tURL, sHost, sizeof(sHost));
	xrtUrlViewCopyTargetTo(&tURL, sTarget, sizeof(sTarget));

	printf("secure = %s\n", xrtUrlIsSecureScheme(tURL.tScheme) ? "true" : "false");
	printf("host = %s\n", sHost);
	printf("port = %u\n", tURL.iPort);
	printf("target = %s\n", sTarget);
}
```

The point here is to separate:

- the connection destination
- the HTTP request target


## 4. Upgraded Demo: `Host` Header, Path Normalization, and Relative URL Resolution

Only in the second step should you move into a more realistic client preprocessing flow:

1. parse the base URL
2. normalize the path
3. resolve a relative reference
4. build the final `Host` header

```c
xrturlview tBase;
char sResolved[1024];
char sNormalized[1024];
char sHostHeader[256];
size_t iLen = 0u;

if ( xrtUrlParseView("https://api.example.com/v1/models/", &tBase) ) {
	xrtUrlNormalizePathTo("/v1/../v1/chat/./completions", 0, sNormalized, sizeof(sNormalized), &iLen);
	xrtUrlResolve(&tBase, "../files?id=7", sResolved, sizeof(sResolved), &iLen);
	xrtUrlMakeHostHeader(&tBase, sHostHeader, sizeof(sHostHeader));
}
```

This chain is a strong fit in front of:

- `xhttp`
- proxy CONNECT targets
- WebSocket client URL preprocessing


## 5. When Should `xurl` Be Used?

| Scenario | Should it go through `xurl` first? |
|----------|------------------------------------|
| an HTTP client request URL | yes |
| a WebSocket client connection URL | yes |
| proxy target and `Host` header construction | yes |
| an HTTP server handling only the request target | usually only the target / authority parts matter |
| parsing individual query pairs | no, that belongs to `xhttp_util` |


## 6. Common Mistakes

### 6.1 Mixing `host` and `target` into One String

That leads directly to:

- wrong connection targets
- broken request lines
- wrong proxy destinations


### 6.2 Hand-Building the `Host` Header

Default ports and IPv6 make this especially easy to break.


### 6.3 Treating a View as If It Owned Its Own Data

`xrturlview` borrows the source text by default.  
Once the original buffer goes away, the view is no longer reliable.


### 6.4 Replacing `Resolve` with String Concatenation

Relative URL resolution is more subtle than it looks and is rarely worth rewriting by hand.


## 7. What to Read Next

Follow this line next:

- [HTTP Util Intro: Headers, Query, Cookies, and Multipart Are Not Just Strings](http-util-intro.en.md)
- [Proxy Intro: When the Proxy Is Just a Shared Object, and When It Becomes Part of the Connection Sequence](proxy-intro.en.md)
- [XURL API](../api/api-xurl.en.md)
- [Case: XHTTP Client Chain with URL, Proxy, and TLS](../case/xhttp-client-proxy-tls.md) (Chinese for now)
