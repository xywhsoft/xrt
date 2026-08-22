# XRT API Index

> The current official API reference entry. This index prioritizes documents aligned with the current source mainline, and explicitly marks historical pages and Chinese-first pages when English mirrors are not ready yet.

[中文](README.md) | [Back to Docs Hub](../README.en.md)

---

## 1. Core Types and Runtime

| Module | Document | Description |
|------|------|------|
| Types | [types.en.md](types.en.md) | Basic types, public structs, and core runtime types |
| Base | [api-base.en.md](api-base.en.md) | Runtime init, memory allocation, thread-local error, and temp memory |
| Charset | [api-charset.en.md](api-charset.en.md) | UTF-8/16/32 conversion |
| OS | [api-os.en.md](api-os.en.md) | System and platform wrappers |
| Math | [api-math.en.md](api-math.en.md) | Math utilities and default random helpers |
| String | [api-string.en.md](api-string.en.md) | Strings and formatting |
| Path | [api-path.en.md](api-path.en.md) | Path handling |
| Time | [api-time.en.md](api-time.en.md) | Time and date |
| File | [api-file.en.md](api-file.en.md) | File and directory operations |
| Async File | [api-file-async.md](api-file-async.md) | Chinese for now. Async file handles, whole-file helpers, and directory jobs |
| Subprocess | [api-subprocess.md](api-subprocess.md) | Chinese for now. Process launch, pipes, waiting, and captured output |
| Network (local info) | [api-network.en.md](api-network.en.md) | Local network information |
| Hash | [api-hash.en.md](api-hash.en.md) | Hash functions |
| XID | [api-xid.en.md](api-xid.en.md) | Distributed unique IDs |


## 2. Concurrency and Async

| Module | Document | Description |
|------|------|------|
| Thread | [api-thread.en.md](api-thread.en.md) | Threads, runtime attach, cleanup, and sync primitives |
| Queue | [api-queue.en.md](api-queue.en.md) | Bounded concurrent queues and the MPSC wait wrapper |
| Coroutine | [api-coroutine.en.md](api-coroutine.en.md) | Coroutine runtime, scheduler, and event waiting |
| Future / Task / Promise | [api-future-task-promise.en.md](api-future-task-promise.en.md) | Unified async model, continuation, and task group |


## 3. Memory and Data Structures

| Module | Document | Description |
|------|------|------|
| Buffer | [api-buffer.en.md](api-buffer.en.md) | Dynamic buffer |
| BSMM | [api-bsmm.en.md](api-bsmm.en.md) | Block-structured memory management |
| MemUnit | [api-memunit.en.md](api-memunit.en.md) | Page-based memory units |
| FSMemPool | [api-mempool-fs.en.md](api-mempool-fs.en.md) | Fixed-size memory pool |
| MemPool | [api-mempool.en.md](api-mempool.en.md) | General memory pool |
| Array | [api-array.en.md](api-array.en.md) | Struct arrays |
| PtrArray | [api-ptrarray.en.md](api-ptrarray.en.md) | Pointer arrays |
| Stack | [api-stack.en.md](api-stack.en.md) | Static stack |
| DynStack | [api-dynstack.en.md](api-dynstack.en.md) | Dynamic stack |
| LList | [api-llist.en.md](api-llist.en.md) | Historical page only. The current source tree has no independent public header or public `xrtLList*` declarations |
| LList-Base | [api-llist-base.en.md](api-llist-base.en.md) | Historical page only. The current source tree has no independent public header or public `xrtLLB*` declarations |
| AVLTree | [api-avltree.en.md](api-avltree.en.md) | AVL tree |
| AVLTree-Base | [api-avltree-base.en.md](api-avltree-base.en.md) | Low-level AVL operations |
| Dict | [api-dict.en.md](api-dict.en.md) | Dictionary |
| List | [api-list.en.md](api-list.en.md) | Integer-key list |


## 4. High-Level Data and Text

| Module | Document | Description |
|------|------|------|
| Value | [api-value.en.md](api-value.en.md) | 16 dynamic data types, shared roots, and reference management |
| JNUM | [api-jnum.en.md](api-jnum.en.md) | Number parsing and conversion |
| JSON | [api-json.en.md](api-json.en.md) | JSON parsing and generation |
| XSON | [api-xson.en.md](api-xson.en.md) | Full `xvalue` serialization, JSON-compatible with `time / list / set / class` extensions |
| Template | [api-template.en.md](api-template.en.md) | Template engine |
| Crypto | [api-crypto.en.md](api-crypto.en.md) | Cryptographic building blocks |
| Regex | [api-regex.en.md](api-regex.en.md) | Regular expressions and multi-pattern matching |


## 5. Current Network Mainline

> The current official network mainline is `xnet-v2 + xtlssession + xhttp/xhttpd/xws`.

| Module | Document | Status |
|------|------|------|
| XNet V2 | [api-xnet-v2.en.md](api-xnet-v2.en.md) | Mainline overview, including shared proxy references |
| XNet Proxy | [api-xnet-proxy.md](api-xnet-proxy.md) | Chinese for now. Shared proxy objects, refcounting, and config |
| Network TLS | [api-network-tls.en.md](api-network-tls.en.md) | Current TLS session mainline |
| XURL | [api-xurl.en.md](api-xurl.en.md) | URL / authority / target parsing and building |
| HTTP Util | [api-http-util.en.md](api-http-util.en.md) | header/query/cookie/media-type/multipart helpers |
| XHTTP | [api-xhttp.en.md](api-xhttp.en.md) | Current HTTP/1.1 client mainline with proxy support |
| XHTTPD | [api-xhttpd.en.md](api-xhttpd.en.md) | Current HTTP/1.1 server mainline |
| XWS | [api-xws.en.md](api-xws.en.md) | Current WebSocket client/server mainline, with client-side proxy support |


## 6. Current Notes

- Chinese API docs are still synchronized first.
- This English index may link to a Chinese-first page when the reviewed English mirror is not ready yet.
- Obsolete old network/TLS API docs have been archived under `dev/`.
- The current mainline is organized around runtime, concurrency, queues, async, `xnet-v2`, TLS sessions, HTTP, and WebSocket.
- `llist / llist-base` remain historical references only and should not be treated as live contracts until the source tree restores a public entry.


## 7. Suggested Reading Order

If you want to understand the current mainline quickly, read in this order:

1. [types.en.md](types.en.md)
2. [api-base.en.md](api-base.en.md)
3. [api-thread.en.md](api-thread.en.md)
4. [api-queue.en.md](api-queue.en.md)
5. [api-coroutine.en.md](api-coroutine.en.md)
6. [api-future-task-promise.en.md](api-future-task-promise.en.md)
7. [api-value.en.md](api-value.en.md)
8. [api-xnet-v2.en.md](api-xnet-v2.en.md)
9. [api-network-tls.en.md](api-network-tls.en.md)
10. [api-xhttp.en.md](api-xhttp.en.md)
11. [api-xhttpd.en.md](api-xhttpd.en.md)
12. [api-xws.en.md](api-xws.en.md)


## 8. Boundary with Guides and Case Studies

This directory only covers:

- API contracts
- module capabilities
- current mainline behavior
- formal runtime, coroutine, async, and network semantics

If you want beginner-friendly explanations or combined usage patterns, continue with:

- [Guides](../guide/README.en.md)
- [Case Studies](../case/README.en.md)
