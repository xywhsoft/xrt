# XRT Feature Trimming Macros

> Source-aligned trimming macros for the current multi-file XRT mainline. This page only documents trimming surfaces that are still valid in the current `xrt.h / xrt.c` code path.

[中文](FEATURE_TRIMMING_MACROS.md) | [Back to Docs Hub](README.en.md)

---

## 1. Scope

This page describes the current official source-tree trimming rules:

- the multi-file mainline `xrt.h + xrt.c + lib/*` is the contract source
- the single-header distribution should follow the same macro set after regeneration
- a trimming surface is considered complete only when:
	- the public declarations disappear
	- the implementation is no longer compiled in

This round specifically verified and completed the trimming surface for:

- `subprocess`
- `file_async`
- `xson`
- the documentation gap around `queue_wait`


## 2. Active Trimming Macros

### 2.1 Runtime and base capabilities

```c
#define XRT_NO_TIME
#define XRT_NO_FILE
#define XRT_NO_FILE_ASYNC
#define XRT_NO_THREAD
#define XRT_NO_QUEUE
#define XRT_NO_QUEUE_WAIT
#define XRT_NO_COROUTINE
#define XRT_NO_CRYPTO
#define XRT_NO_XID
#define XRT_NO_BUFFER
#define XRT_NO_REGEX
#define XRT_NO_SUBPROCESS
```

Notes:

- `XRT_NO_FILE_ASYNC` is now a real standalone trimming surface
- `XRT_NO_QUEUE_WAIT` already existed in code and is now documented here
- `XRT_NO_SUBPROCESS` now hides the public API and prevents `lib/subprocess.h` from entering compilation


### 2.2 Network mainline

```c
#define XRT_NO_NETWORK
#define XRT_NO_NETTLS
#define XRT_NO_XURL
#define XRT_NO_HTTP_UTIL
#define XRT_NO_XCODEC
#define XRT_NO_XHTTP
#define XRT_NO_XHTTPD
#define XRT_NO_XWS
```

Notes:

- `XRT_NO_NETWORK` is the root network trimming surface
- `xnet_proxy` is still trimmed together with the network root instead of being exposed as a separate macro


### 2.3 Memory and containers

```c
#define XRT_NO_ARRAY
#define XRT_NO_BSMN
#define XRT_NO_MEMUNIT
#define XRT_NO_MEMPOOL_FS
#define XRT_NO_STACK
#define XRT_NO_AVLTREE
#define XRT_NO_MEMPOOL
#define XRT_NO_DICT
#define XRT_NO_LIST
```


### 2.4 Values, text, and structured data

```c
#define XRT_NO_VALUE
#define XRT_NO_JNUM
#define XRT_NO_JSON
#define XRT_NO_XSON
#define XRT_NO_TEMPLATE
```

Notes:

- `XRT_NO_XSON` is now a standalone trimming surface
- `XSON` still depends on `JSON`, but it no longer relies only on an implicit “disappear together with JSON” behavior


## 3. Automatic Cascading Rules

### 3.1 `XRT_MINIMAL`

```c
#define XRT_MINIMAL
```

The current code expands it into:

```c
XRT_NO_TIME
XRT_NO_FILE
XRT_NO_FILE_ASYNC
XRT_NO_THREAD
XRT_NO_QUEUE
XRT_NO_COROUTINE
XRT_NO_NETWORK
XRT_NO_CRYPTO
XRT_NO_NETTLS
XRT_NO_XID
XRT_NO_BUFFER
XRT_NO_ARRAY
XRT_NO_BSMN
XRT_NO_MEMUNIT
XRT_NO_MEMPOOL_FS
XRT_NO_STACK
XRT_NO_AVLTREE
XRT_NO_MEMPOOL
XRT_NO_DICT
XRT_NO_LIST
XRT_NO_VALUE
XRT_NO_JNUM
XRT_NO_JSON
XRT_NO_XSON
XRT_NO_TEMPLATE
XRT_NO_REGEX
XRT_NO_SUBPROCESS
```


### 3.2 Derived relationships

```c
XRT_NO_NETWORK    -> XRT_NO_FILE_ASYNC, XRT_NO_XURL, XRT_NO_HTTP_UTIL, XRT_NO_XCODEC, XRT_NO_XHTTP, XRT_NO_XHTTPD, XRT_NO_XWS
XRT_NO_QUEUE      -> XRT_NO_QUEUE_WAIT
XRT_NO_FILE       -> XRT_NO_FILE_ASYNC
XRT_NO_JSON       -> XRT_NO_XSON
```

The most important practical consequences are:

- `file_async` depends on both `FILE` and `NETWORK`
- `xson` depends on `JSON`


## 4. Capabilities Still Trimmed Through Parent Modules

The following capabilities are still grouped under parent modules instead of exposing new standalone macros:

| Capability | Current trim path | Note |
|------------|-------------------|------|
| `ptrarray` | `XRT_NO_ARRAY` | still part of the array family |
| `dynstack` | `XRT_NO_STACK` | still part of the stack family |
| `xnet_proxy` | `XRT_NO_NETWORK` | still tightly coupled to the network mainline |

This does not mean they can never become standalone trimming surfaces later. It only means the current mainline has not been normalized far enough to expose them safely yet.


## 5. What Was Verified in This Round

The source audit for this round reduced to these concrete results:

- `subprocess` previously had no standalone trim surface; it is now complete
- `file_async` previously had no explicit standalone trim surface; it is now complete
- `xson` previously disappeared only implicitly with `JSON`; it now has a standalone trim surface
- `queue_wait` was already present in code, but missing from the documentation

The following trim profiles were compiled directly against `xrt.c`:

```c
XRT_NO_SUBPROCESS
XRT_NO_FILE_ASYNC
XRT_NO_XSON
XRT_NO_SUBPROCESS + XRT_NO_FILE_ASYNC + XRT_NO_XSON + XRT_NO_QUEUE_WAIT
```

Verification result:

- the multi-file mainline compiles
- the corresponding symbols do not remain in the generated object files
- no newly confirmed case was found where a defined trim macro still leaves the implementation compiled in


## 6. Recommended Configurations

### 6.1 Disable the three newly completed standalone modules

```c
#define XRT_NO_SUBPROCESS
#define XRT_NO_FILE_ASYNC
#define XRT_NO_XSON
#include "xrt.h"
```

Use this when:

- you do not need process spawning
- you do not need async file I/O
- you want standard JSON only, without XSON extensions


### 6.2 Build a local-only runtime without network or async file

```c
#define XRT_NO_NETWORK
#define XRT_NO_NETTLS
#include "xrt.h"
```

This also trims:

- `file_async`
- `xurl`
- `http_util`
- `xcodec`
- `xhttp`
- `xhttpd`
- `xws`


### 6.3 Keep JSON but disable XSON

```c
#define XRT_NO_XSON
#include "xrt.h"
```

Use this when:

- you want plain JSON only
- you do not need `time/list/coll/class` XSON extensions


## 7. Historical Macros That Should No Longer Drive Current Configurations

Older documents used to mention legacy network-family macros such as:

```c
XRT_NO_NETSOCK
XRT_NO_NETPOLL
XRT_NO_NETLOOP
XRT_NO_NETTCP
XRT_NO_NETUDP
XRT_NO_NETHTTP
```

They should no longer be treated as the current official trimming strategy. The active source-aligned trimming surface is the macro set documented on this page.
