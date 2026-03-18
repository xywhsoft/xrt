# XRT Feature Trimming Macros

> Feature-trimming guide for the current source-tree mainline. This page only documents trimming macros that are still valid in the current mainline. Historical trimming surfaces for retired network modules are kept only as migration reference.

[中文](裁剪宏定义说明.md) | [Back to Docs Hub](README.en.md)

---

## 1. Goals

XRT supports feature trimming through preprocessor macros so you can:

- reduce build size
- reduce dependency surface
- keep only the subsystems you actually need
- stay lightweight while preserving the current formal mainline

Trimming rules:

- trimming should be based on the **current formal mainline**
- historical trimming macros for retired modules are no longer treated as formal configuration entry points


## 2. Currently Valid Trimming Macros

### 2.1 Runtime and Foundation

```c
#define XRT_NO_TIME
#define XRT_NO_FILE
#define XRT_NO_THREAD
#define XRT_NO_COROUTINE
#define XRT_NO_CRYPTO
#define XRT_NO_XID
#define XRT_NO_BUFFER
#define XRT_NO_REGEX
```

### 2.2 Network Mainline

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

- `XRT_NO_NETWORK` disables the current network mainline as a whole
- if you do not disable `XRT_NO_NETWORK`, you can still trim individual submodules

### 2.3 Memory and Data Structures

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

### 2.4 Structured Data and Text

```c
#define XRT_NO_VALUE
#define XRT_NO_JNUM
#define XRT_NO_JSON
#define XRT_NO_TEMPLATE
```


## 3. Retired Historical Trimming Surface

The following historical trimming macros are no longer part of the formal current mainline:

```c
XRT_NO_NETSOCK
XRT_NO_NETPOLL
XRT_NO_NETLOOP
XRT_NO_NETTCP
XRT_NO_NETUDP
XRT_NO_NETHTTP
```

Reason:

- the legacy network module family is no longer the formal network mainline
- the current network mainline is organized around:
	- `xnet-v2`
	- `xtlssession`
	- `xhttp / xhttpd / xws`


## 4. Automatic Dependency Recovery

The source tree still keeps automatic dependency recovery logic.

### 4.1 Template

When template support is enabled, XRT automatically pulls in:

- `VALUE`
- `JNUM`
- `DICT`
- `LIST`
- `AVLTREE`
- `BSMN`
- `MEMUNIT`
- `MEMPOOL_FS`

### 4.2 JSON

When JSON support is enabled, XRT automatically pulls in:

- `VALUE`
- `DICT`
- `LIST`
- `AVLTREE`
- `BSMN`
- `MEMUNIT`
- `MEMPOOL_FS`

### 4.3 VALUE

When `VALUE` is enabled, XRT automatically pulls in:

- `DICT`
- `LIST`
- `AVLTREE`
- `BSMN`
- `MEMUNIT`
- `MEMPOOL_FS`

### 4.4 MemPool / FSMemPool

When:

- `MEMPOOL`
- `MEMPOOL_FS`

are enabled, XRT automatically pulls in:

- `BSMN`
- `MEMUNIT`

### 4.5 Dict / List

When:

- `DICT`
- `LIST`

are enabled, XRT automatically pulls in:

- `AVLTREE`


## 5. Minimal Configuration

The source tree still supports:

```c
#define XRT_MINIMAL
```

This provides a small runtime starting point.

Notes:

- `XRT_MINIMAL` is a baseline trimming entry, not a full final profile
- if higher-level modules are enabled later, required dependencies are pulled back in automatically


## 6. Recommended Configuration Style

### 6.1 Minimal Runtime in the Source Tree

In the current source-tree mainline, you usually place trimming macros directly in your main translation unit:

```c
#define XRT_MINIMAL
#include "xrt.h"
```

Suitable for:

- small tools
- text processing
- script-style utility programs

Notes:

- current docs explain the mainline primarily through the source-tree build model
- single-header distribution still supports the same trimming macros, but it is no longer the default documentation model

### 6.2 Runtime Without the Network Mainline

```c
#define XRT_NO_NETWORK
#include "xrt.h"
```

Suitable for:

- runtime, containers, JSON, template, regex only
- no network mainline required

### 6.3 Keep the Network Mainline but Trim the Application Layer

```c
#define XRT_NO_XHTTP
#define XRT_NO_XHTTPD
#define XRT_NO_XWS
#include "xrt.h"
```

Suitable for:

- low-level networking, TLS, wait-source, future bridge
- no HTTP client/server or WebSocket surface required

### 6.4 Disable TLS

```c
#define XRT_NO_NETTLS
#include "xrt.h"
```

Suitable for:

- explicitly plaintext networking only
- no `xtlssession` required


## 7. Recommendations

### 7.1 Trim from Mainline Modules First

Recommended order:

- decide whether you need the network mainline
- decide whether you need higher-level structured data modules
- then trim memory and container layers

Do not keep reasoning from the retired network module family when trimming a new project.

### 7.2 Re-verify After Every Trim Change

After each trimming profile change, at minimum verify:

- `xrt.c` still compiles
- the main test program still compiles
- the modules your current project depends on still work

### 7.3 Trimming Does Not Bypass Dependencies

If you explicitly enable higher-level modules, XRT restores the required lower-level modules based on the current dependency graph.

That is intentional, to keep:

- single-header distribution stable
- module dependency chains intact


## 8. Historical Patterns That Should Not Be Used in New Docs or Templates

The following should not continue to appear in new docs, new examples, or new project templates:

```c
#define XRT_NO_NETTCP
#define XRT_NO_NETUDP
#define XRT_NO_NETHTTP
```

They no longer represent the formal current network mainline.


## 9. Acceptance Criteria

This document should satisfy:

- only currently valid trimming macros are presented as formal configuration points
- retired network trimming surfaces are not presented as current entry points
- the page reflects the actual dependency behavior in `xrt.h / xrt.c`
- the document stays consistent with the current network, TLS, and application-layer mainline

---

<div align="center">

[⬆️ Back to Top](#xrt-feature-trimming-macros)

</div>
