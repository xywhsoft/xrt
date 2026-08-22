# XRT Guide: HTTP Util Intro

> Goal: turn `xrtQueryNext()`, `xrtHttpHeaderNextLine()`, `xrtCookieNext()`, `xrtSetCookieParse()`, `xrtHttpMediaTypeParse()`, `xrtMultipartValidate()`, and `xrtMultipartStream*` into the formal Stage 6 mainline for HTTP text-block handling. After this page, you should clearly understand when iterative scanning is enough, when strict size validation must come first, and when multipart work has already outgrown the "treat the whole thing as one big string" model.

[中文](http-util-intro.md) | [Back to Guides](README.en.md)

---

## 1. Why Does `http util` Need Its Own Page?

Many HTTP codebases start like this:

- `strstr("Content-Type:")`
- `strtok(query, "&")`
- `strncmp(cookie, "sid=", 4)`

That may run for small demos, but it gets brittle over time.

Because what HTTP actually gives you is not "one big string". It gives you several different classes of text blocks:

- token
- header line / header block
- query pair
- cookie pair
- `Set-Cookie`
- media type
- multipart

And each class has very different boundaries and risks:

- some are fine with simple iteration
- some should be validated against size limits first
- some should become structured views
- some already need stream-style parsing


## 2. Separate 8 Boundaries First

### 2.1 `xhttp_util` Is Not the HTTP Client/Server Layer

A good first split is:

- `xhttp` / `xhttpd` / `xws`
	- the protocol runtime
- `xhttp_util`
	- the text utility layer for protocol fragments

It does not handle:

- opening connections
- sending requests
- running protocol state machines

It does handle:

- parsing, validating, iterating, and structuring HTTP-related text fragments


### 2.2 Cut the Large Block First, Then Parse the Inside

This path connects directly to `xurl`.

Use:

- `xurl`

to cut out larger blocks like:

- query
- target

Then use:

- `xhttp_util`

to parse the inside of those blocks item by item:

- query pairs
- header lines
- cookie pairs


### 2.3 Validate Limits First, Then Enter Business Parsing

One of the easiest things to overlook in the current API is:

- `xrthttputillimits`

and the full `Validate*()` family.

So the current formal mainline is closer to:

1. define limits
2. validate first
3. parse item by item later

This matters especially for:

- very large header blocks
- very long tokens
- too many pairs
- oversized multipart bodies


### 2.4 Query, Header, and Cookie Scanning Share the Same Offset-Iteration Model

The feel of these APIs is intentionally close:

- `xrtQueryNext*()`
- `xrtHttpHeaderNextLine*()`
- `xrtCookieNext*()`
- `xrtHttpParamNext*()`

The shared usage pattern is:

```c
size_t iOffset = 0u;
while ( xrtQueryNextN(sText, iLen, &iOffset, &tPair) ) {
	/* handle one item */
}
```

That matters because you do not need to memorize three completely different parsing styles for three text block families.


### 2.5 `Set-Cookie` Is Not an Ordinary Cookie Pair

This is a very important boundary.

A request header like:

- `Cookie: a=1; b=2`

fits:

- `xrtCookieNext*()`

But a response header like:

- `Set-Cookie: sid=...; HttpOnly; Secure; SameSite=Lax`

should go through:

- `xrtSetCookieParse*()`

because that is no longer just a name/value pair. It is a cookie definition with attributes.


### 2.6 `Content-Type` Should Become `xrtmediatypeview`, Not Just a Substring Match

Many codebases do this:

- `if ( strstr(sType, "application/json") )`

That is not steady enough.

The more formal path is:

- `xrtHttpMediaTypeParse*()`
- `xrtHttpMediaTypeFindParam*()`

This way you can explicitly inspect:

- `type/subtype`
- `suffix`
- `charset`
- `boundary`

instead of guessing from a raw string.


### 2.7 Small Multipart Bodies Can Be Validated Whole; Large Ones Should Move to Streaming Early

The current capability line has two layers:

| Capability | Best fit |
|------------|----------|
| `xrtMultipartValidate*()` | smaller whole-body strings where one-shot legality checks are enough |
| `xrtmultipartstream*` | large bodies, uploads, or receive-as-you-go flows |

That means:

- if the whole body is already in memory
	- one-shot validation may be enough
- if an HTTP server is receiving while processing
	- do not keep assuming the whole multipart body can always be assembled first


### 2.8 View Types Also Borrow the Original Buffer by Default

Types such as:

- `xrtheaderpair`
- `xrtquerypair`
- `xrtcookiepair`
- `xrthttpparam`
- `xrtsetcookieview`
- `xrtmediatypeview`
- `xrtmultipartpartview`

are also fundamentally:

- borrowed views of the original text
- not automatic copies

So the same rule still applies:

- if you need long-term ownership, copy the fields explicitly


## 3. Minimal Demo: Cleanly Scan Query and Header Blocks

Do not start with multipart yet.  
First make the two most common flows work:

1. scan query pairs
2. scan header lines

```c
size_t iOffset = 0u;
xrtquerypair tQuery;
xrtheaderpair tHeader;

while ( xrtQueryNext("model=gpt-5&stream=true", &iOffset, &tQuery) ) {
	/* handle each query pair */
}

iOffset = 0u;
while ( xrtHttpHeaderNextLine("Host: api.example.com\r\nAccept: application/json\r\n", &iOffset, &tHeader) ) {
	/* handle each header line */
}
```

The point of this step is to build the offset-iteration mental model instead of immediately hand-splitting delimiters.


## 4. Upgraded Demo: Limits, Media Type, `Set-Cookie`, and Multipart

Only in the second step should you connect the more realistic boundaries:

1. initialize limits
2. validate the header block
3. parse `Content-Type`
4. parse `Set-Cookie`
5. switch to multipart validate / stream handling when uploads enter the picture

```c
xrthttputillimits tLimits;
xrtmediatypeview tType;
xrtsetcookieview tCookie;

xrtHttpUtilLimitsInit(&tLimits);
tLimits.iMaxHeaderBytes = 32u * 1024u;
tLimits.iMaxMultipartBytes = 16u * 1024u * 1024u;

if ( xrtHttpHeaderBlockValidate("Content-Type: multipart/form-data; boundary=abc\r\n", &tLimits) ) {
	xrtHttpMediaTypeParse("multipart/form-data; boundary=abc", &tType);
}

xrtSetCookieParse("sid=abc; Path=/; HttpOnly; SameSite=Lax", &tCookie);
```

If the next step is a large upload body, do not stay in the whole-string model. Move to:

- `xrtmultipartstream`


## 5. Which Layer Fits Which Scenario?

| Scenario | Preferred entry |
|----------|-----------------|
| iterate query pairs | `xrtQueryNext*()` |
| iterate header lines | `xrtHttpHeaderNextLine*()` |
| iterate `Cookie` pairs | `xrtCookieNext*()` |
| parse `Set-Cookie` attributes | `xrtSetCookieParse*()` |
| parse `Content-Type` / boundary / charset | `xrtHttpMediaTypeParse*()` |
| reject oversized headers or multipart early | `Validate*()` + `xrthttputillimits` |
| validate a small multipart body as one block | `xrtMultipartValidate*()` |
| handle large uploads or incremental receive | `xrtmultipartstream*` |


## 6. Common Mistakes

### 6.1 Treating `Set-Cookie` as an Ordinary Cookie List

That loses:

- `HttpOnly`
- `Secure`
- `SameSite`
- `Path`
- `Domain`


### 6.2 Skipping Limits Entirely

The biggest risk in a text utility layer is always:

- large inputs
- too many pairs
- specially constructed header bombs


### 6.3 Using `strstr()` to Judge `Content-Type`

What you really need is a structured media type, not a guess that the word `json` appears somewhere.


### 6.4 Treating Every Multipart Body as One Giant String

That may work in a small demo, but it is not a steady upload strategy.


## 7. What to Read Next

Follow this line next:

- [XURL Intro: Why Correct URL Splitting Matters Before Sending Requests](xurl-intro.en.md)
- [Proxy Intro: When the Proxy Is Just a Shared Object, and When It Becomes Part of the Connection Sequence](proxy-intro.en.md)
- [HTTP Util API](../api/api-http-util.en.md)
- [Case: Full HTTP + JSON + Template Service Chain](../case/http-json-template-chain.en.md)
