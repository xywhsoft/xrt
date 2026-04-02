# XRT Examples Index

This directory currently contains `166` standalone, buildable examples across `39` module directories. This document is generated from the actual `examples/*/*/main.c` implementations and is intended for end users to browse and locate examples quickly.

`examples/bin` is the output directory for batch builds and is not counted as part of the examples. `example_common.h`, `example_wait.h`, and `example_tls_fixture.h` are shared helper headers used by some examples and are not standalone examples either.

## 1. Shared Rules For New Or Supplemental Examples

- The directory layout must stay `examples/<module>/<example>/main.c`, where each directory maps to one independently buildable and runnable example.
- Every example file should keep a bilingual file header, at minimum including `Title`, `Description / 说明`, and `Build / 编译`. Add `Usage / 用法` and `Note / 注意` when needed.
- File headers, key step explanations, user-visible output, and command-line usage should remain bilingual whenever practical. Internal function bodies do not need line-by-line bilingual comments, but the example purpose and result should be obvious to readers.
- C code style should stay consistent with the existing examples: use `1` tab for indentation, keep Hungarian-style variable naming, place function-definition braces on their own line, and keep `if/for/while` braces on the same line as the statement.
- Combined conditions should use explicit parentheses. Example code should be written as minimal complete snippets that users can reuse directly without hidden context.
- Examples should prefer XRT public APIs and should not introduce extra dependencies. When an example can be implemented with `singlehead/xrt.h`, prefer the single-header mode; only use `xrt.c` when the full network / HTTP / TLS / WebSocket implementation is required.
- Examples must consider Windows and Linux cross-platform compatibility and should avoid hard-coding behavior that only works on one platform.
- Network examples should prefer loopback traffic, self-hosted temporary services, and self-validation. External network availability should not be a prerequisite for correctness.
- File examples should clean up temporary files before exit. Service examples should shut down automatically instead of requiring the user to kill the process manually.
- The `Build / 编译` section at the top of each example should provide directly copyable `tcc` and `gcc` commands, and outputs should be placed under `examples/bin`.

## 2. Usage

- Windows `cmd` batch build: `examples\\build_all.bat`
- Windows `PowerShell` batch build: `./examples/build_all.ps1`
- Linux / macOS shell batch build: `./examples/build_all.sh`
- For the recommended command to build a single example, check the `Build / 编译` section at the top of its `main.c`.
- For network, HTTP, TLS, and WebSocket examples, read the header comment first for behavior notes and parameter expectations.

## 3. Example Index

### array (3)
- `array/array_sort`: Demonstrates custom comparison sorting for struct arrays and pointer arrays.
- `array/ptr_array`: Demonstrates pointer-array append, insert, set, swap, remove, and `AddAlt`.
- `array/struct_array`: Demonstrates struct-array operations with `xrtArrayCreate`, `xrtArrayInsert`, `xrtArrayAppend`, `xrtArrayGet`, and `xrtArrayRemove`.

### avltree (3)
- `avltree/avltree_custom`: Demonstrates string keys, custom comparison functions, and descending-order traversal.
- `avltree/avltree_iterator`: Demonstrates ordered AVL tree traversal and empty-tree iteration.
- `avltree/basic_avltree`: Demonstrates AVL tree creation, insertion, lookup, removal, and balancing behavior.

### base (6)
- `base/error_handling`: Demonstrates `xrtSetError`, `xrtClearError`, `xrtGetError`, and the `OnError` callback.
- `base/init_and_config`: Demonstrates the `xrtInit` / `xrtUnit` lifecycle and exposed global configuration items.
- `base/mem_debug`: Demonstrates enabling memory debugging, exporting text and JSON reports, reading the generated files, and cleaning them up.
- `base/mem_telemetry`: Demonstrates enabling memory telemetry, performing a few allocations, and reading aggregated snapshot counters.
- `base/memory_basic`: Demonstrates safe memory allocation and error handling with `xrtMalloc`, `xrtCalloc`, `xrtRealloc`, and `xrtFree`.
- `base/temp_memory`: Demonstrates `xrtTempMemory` and `xrtFreeTempMemory`.

### bsmm (1)
- `bsmm/block_memory`: Demonstrates fixed-size block allocation, free, reuse, and index-based access.

### buffer (2)
- `buffer/basic_buffer`: Demonstrates dynamic buffer operations with `xrtBufferCreate`, `xrtBufferAppend`, `xrtBufferInsert`, and `xrtBufferDestroy`.
- `buffer/buffer_builder`: Demonstrates using `xbuffer` as a StringBuilder for efficient string concatenation and edits.

### charset (3)
- `charset/charset_detect`: Demonstrates UTF-8 validation with `xrtIsUTF8`.
- `charset/endian_convert`: Demonstrates endian conversion between little-endian and big-endian with `xrtUTF16LEtoBE` and `xrtUTF32LEtoBE`.
- `charset/utf_convert`: Demonstrates UTF-8 / UTF-16 / UTF-32 conversion functions.

### coroutine (7)
- `coroutine/basic_coroutine`: Demonstrates creating, resuming, yielding, and destroying a single coroutine.
- `coroutine/cancel_and_join`: Demonstrates cooperative coroutine cancellation inside one scheduler and a `join` performed by another coroutine.
- `coroutine/cleanup_and_result`: Demonstrates the coroutine cleanup stack, explicit cleanup pop, result storage, and user-data retrieval after completion.
- `coroutine/coroutine_sleep`: Demonstrates using `xrtCoSleep` inside scheduler-managed coroutines.
- `coroutine/event_wait`: Demonstrates timeout waits, deadline waits, and event wakeups inside a scheduler coroutine flow.
- `coroutine/scheduler`: Demonstrates running multiple coroutines under a scheduler and advancing them step by step.
- `coroutine/userdata`: Demonstrates `xrtCoSetUserData` and `xrtCoGetUserData`.

### crypto (10)
- `crypto/aes_gcm`: Demonstrates AES-128-GCM and AES-256-GCM round-trip encryption and decryption.
- `crypto/chacha20`: Demonstrates ChaCha20 stream encryption and AEAD round-trip verification.
- `crypto/hash_functions`: Demonstrates the actual public cryptographic primitives currently exposed by XRT: SHA-256, incremental SHA-384, HMAC-SHA256, and HKDF-SHA256.
- `crypto/hkdf`: Demonstrates HKDF extract and expand with SHA-256 and SHA-384.
- `crypto/hmac`: Demonstrates HMAC-SHA256, HMAC-SHA384, and HMAC-SHA512.
- `crypto/key_exchange`: Demonstrates shared-secret derivation with X25519 and ECDH secp256r1.
- `crypto/random_bytes`: Demonstrates cryptographically secure random byte generation.
- `crypto/sha256`: Demonstrates one-shot and incremental SHA-256 hashing.
- `crypto/sha512`: Demonstrates SHA-512 and SHA-384 hash APIs.
- `crypto/x25519_chacha20poly1305`: Demonstrates shared-secret agreement, HKDF-derived session keys, AEAD encrypt/decrypt round trips, and Ed25519 signing and verification.

### dict (3)
- `dict/basic_dict`: Demonstrates dictionary set, get, existence checks, and removal.
- `dict/dict_iterator`: Demonstrates dictionary entry iteration.
- `dict/dict_ptr`: Demonstrates storing, reading, updating, and removing pointer values in a dictionary.

### file (6)
- `file/binary_io`: Demonstrates `xrtPut`, `xrtGet`, `xrtFilePutAll`, and `xrtFileGetAll`.
- `file/dir_operations`: Demonstrates directory management with `xrtDirCreate`, `xrtDirCreateAll`, `xrtDirScan`, `xrtDirCopy`, and `xrtDirDelete`.
- `file/file_info`: Demonstrates retrieving file metadata with `xrtFileGetSize`, `xrtFileGetAttr`, and `xrtPathExists`.
- `file/file_manage`: Demonstrates file management with `xrtFileCopy`, `xrtFileMove`, and `xrtFileDelete`.
- `file/file_readall`: Demonstrates whole-file operations with `xrtFileReadAll`, `xrtFileWriteAll`, and `xrtFileAppend`.
- `file/read_write`: Demonstrates basic file I/O with `xrtOpen`, `xrtClose`, `xrtRead`, and `xrtWrite`.

### file_async (3)
- `file_async/async_read_write`: Demonstrates explicit-offset async write, `flush`, file-size queries, and readback.
- `file_async/dir_tasks`: Demonstrates async directory create, copy, move, and delete tasks.
- `file_async/file_helpers`: Demonstrates async whole-file text / binary helpers plus file copy, move, and delete.

### future (6)
- `future/continuation_chain`: Demonstrates `Then` / `Catch` / `Finally` continuation chains in the current thread context.
- `future/promise_basic`: Demonstrates manual future / promise creation plus `resolve`, `wait`, and result inspection.
- `future/task_coroutine`: Demonstrates running tasks inside a coroutine scheduler and waiting for their future results.
- `future/task_group_scope`: Demonstrates task-group `close` / `join` flow and cancellation propagation from a parent scope to child scopes.
- `future/task_thread`: Demonstrates running a task on a thread and reading the returned future result.
- `future/when_all_any_race`: Demonstrates future aggregation, winner-source index reporting, and reading all values.

### hash (2)
- `hash/hash32`: Demonstrates 32-bit hashing with `xrtHash32` and `xrtHash32_WithSeed`.
- `hash/hash64`: Demonstrates 64-bit hashing with `xrtHash64`, `xrtHash64_Micro`, and `xrtHash64_Nano` at different quality levels.

### http (6)
- `http/http_advanced`: Demonstrates the current low-level HTTP request configuration APIs.
- `http/http_cookies`: Demonstrates sending a `Cookie` header and parsing `Set-Cookie` responses.
- `http/http_download`: Demonstrates downloading a remote response body and writing it to a local file.
- `http/http_get`: Demonstrates blocking GET requests with the current XNet HTTP client.
- `http/http_post`: Demonstrates form-style POST requests with the current request-object API.
- `http/http_upload`: Demonstrates manual multipart request-body construction for uploads.

### http_util (3)
- `http_util/cookie_and_media_type`: Demonstrates parsing `Set-Cookie`, media types, multipart boundaries, and `Content-Disposition` with UTF-8 filename extensions.
- `http_util/multipart_stream`: Demonstrates building a multipart body, extracting the boundary from `Content-Type`, and incrementally parsing it with `xrtMultipartStream`.
- `http_util/query_and_header`: Demonstrates query key/value iteration, parameter decoding, single-line header splitting, header-block scanning, and normalized header construction.

### jnum (2)
- `jnum/num_to_str`: Demonstrates signed decimal, unsigned hexadecimal, and double formatting in JNUM.
- `jnum/str_to_num`: Demonstrates token parsing with `xrtParseNum` / `xrtParseNumSkipSpace` and boundary behavior of the `xrtStrTo*` convenience converters.

### json (5)
- `json/generate_json`: Demonstrates creating value objects and producing compact or formatted JSON.
- `json/json_file`: Demonstrates JSON file write, read, and roundtrip verification.
- `json/parse_json`: Demonstrates parsing JSON strings into value objects and reading object, array, and nested fields.
- `json/sax_parser`: Demonstrates callback-based streaming JSON SAX parsing.
- `json/sax_printer`: Demonstrates incrementally building object, array, nested, and compact JSON output through events.

### list (2)
- `list/basic_list`: Demonstrates index-based list set, get, existence checks, removal, and sparse indices.
- `list/list_iterator`: Demonstrates list iteration and sparse-index walk behavior.

### math (2)
- `math/approx`: Demonstrates approximate integer and floating-point comparisons with delta mode, percentage mode, and configurable tolerance.
- `math/random`: Demonstrates PCG random generation, including global RNG, independent `xrand` state, ranged generation, and thread-safe variants.

### mempool (3)
- `mempool/fs_mempool`: Demonstrates fixed-size memory-pool allocation, free, multi-page growth, and GC helpers.
- `mempool/general_mempool`: Demonstrates variable-size allocation, free, and cutoff configuration in the general memory pool.
- `mempool/mempool_gc`: Demonstrates mark / sweep garbage collection flows for fixed-size and general memory pools.

### memunit (1)
- `memunit/mem_unit`: Demonstrates allocation, index-based free, and mark-based reclamation in a `256`-slot memory unit.

### network (12)
- `network/dgram_future`: Demonstrates waiting for a UDP datagram with `xrtNetDgramRecvFuture` and reading the returned packet contents.
- `network/dns_resolve`: Demonstrates resolving a host name into network addresses.
- `network/engine_post`: Demonstrates immediate posting, delayed posting, and future-returning task bridging on an XNet engine worker.
- `network/listener_future`: Demonstrates waiting for local TCP `accept` with `xrtNetListenerAcceptFuture` and then exchanging data.
- `network/local_info`: Demonstrates retrieving local IP addresses, MAC addresses, and the host name.
- `network/ringbuf`: Demonstrates the currently exposed buffer primitive `xrtNetChain`.
- `network/socket_basic`: Demonstrates the basic flow for listen, connect, send, and receive.
- `network/stream_wait`: Demonstrates stream readable wait-source usage and drain waiting between local TCP peers.
- `network/tcp_client`: Demonstrates connecting to a local TCP server and sending one message.
- `network/tcp_echo_server`: Demonstrates a TCP echo server implemented with `xrtNet` listeners and stream callbacks.
- `network/udp_client`: Demonstrates sending one datagram to a local UDP server.
- `network/udp_echo`: Demonstrates UDP send, receive, and echo on loopback.

### network_tls (2)
- `network_tls/tls_session_handshake`: Demonstrates driving a client / server TLS session handshake in memory, exchanging plaintext over the encrypted channel, and handling `close-notify`.
- `network_tls/tls_session_resume`: Demonstrates exporting TLS resume state from the first in-memory handshake and reusing it in a later TLS 1.2 resumed session.

### os (2)
- `os/run_program`: Demonstrates executing external programs with `xrtRun`, `xrtStart`, and `xrtChain`.
- `os/subprocess_basic`: Demonstrates direct exec, shell-command execution, stdin pipe writes, event timeline polling, and terminal output capture.

### path (3)
- `path/path_check`: Demonstrates path validation and generation with `xrtPathIsAbs`, `xrtPathRandom`, and `xrtPathExists`.
- `path/path_join`: Demonstrates correctly joining multiple path segments with `xrtPathJoin`.
- `path/path_parse`: Demonstrates extracting path components with `xrtPathGetNameExt`, `xrtPathGetName`, `xrtPathGetExt`, and `xrtPathGetDir`.

### queue (4)
- `queue/mpmc_basic`: Demonstrates a multi-producer multi-consumer queue driven by worker threads.
- `queue/mpsc_batch`: Demonstrates batch `push` / `pop`, close, `drain`, and `reset` on an MPSC queue.
- `queue/mpsc_wait`: Demonstrates timeout waiting and blocking pop with the MPSC wait wrapper.
- `queue/spsc_basic`: Demonstrates create, push, pop, close, `drain`, and `reset` on an SPSC queue.

### regex (2)
- `regex/capture_config_line`: Demonstrates named captures, capture ranges, and builder flags.
- `regex/regex_set_scan`: Demonstrates multi-pattern classification with `xregexset`.

### stack (3)
- `stack/dynamic_stack`: Demonstrates automatic-growth stacks, push, pop, and arbitrary-position access.
- `stack/ptr_stack`: Demonstrates pointer push, pop, top, and positional access on static and dynamic stacks.
- `stack/static_stack`: Demonstrates push, pop, top, and overflow protection on a fixed-capacity stack.

### string (10)
- `string/case_convert`: Demonstrates string case conversion with `xrtLCase` and `xrtUCase`, including UTF-8-safe character skipping.
- `string/copy_and_compare`: Demonstrates string copy helpers (`xrtCopyStr` / `xrtCopyMem`) and comparison (`xrtStrComp`) in case-sensitive and case-insensitive modes.
- `string/encoding`: Demonstrates helper functions for hex, Base64, and percent encoding.
- `string/format_and_replace`: Demonstrates printf-style formatting with `xrtFormat` and substring replacement with `xrtReplace`.
- `string/number_format`: Demonstrates localized number formatting with thousand separators using `xrtIntFormat` and `xrtNumFormat`.
- `string/random_string`: Demonstrates random string generation from a character template with `xrtRandStr`.
- `string/search_and_match`: Demonstrates search helpers (`xrtFindStr` / `xrtInStr`) and pattern matching (`xrtCheckStr` / `xrtStrLike`), including wildcard matching.
- `string/similarity`: Demonstrates string similarity scoring with `xrtStrSim` and approximate comparison with `xrtStrApprox`.
- `string/split`: Demonstrates copy mode and in-place mode with `xrtSplit`.
- `string/trim_and_filter`: Demonstrates trim helpers (`xrtLTrim` / `xrtRTrim` / `xrtTrim`) and character filtering with `xrtFilterStr`, including custom character sets.

### template (5)
- `template/basic_template`: Demonstrates basic template parsing and rendering with the current XTE APIs.
- `template/template_control`: Demonstrates rendering behavior for `if` / `else`, `for`, `foreach`, `break`, and `continue`.
- `template/template_expr`: Demonstrates current template-expression capabilities through `if` / `elseif` and inline boolean output.
- `template/template_sub`: Demonstrates local `define` / `include` and external include-map rendering.
- `template/template_vars`: Demonstrates nested paths, array indexing, and mixed-value rendering.

### thread (8)
- `thread/attach_current`: Demonstrates attaching a host-created native thread to the XRT runtime, using thread-bound APIs, and then detaching it.
- `thread/condvar`: Demonstrates `Signal` and `Broadcast` wakeups on condition variables.
- `thread/mutex`: Demonstrates both heap-allocated mutexes and embedded mutex usage.
- `thread/producer_consumer`: Demonstrates a classic producer-consumer queue using mutexes and condition variables.
- `thread/rwlock`: Demonstrates read locks, upgrade, downgrade, and write-lock operations.
- `thread/semaphore`: Demonstrates wait, release, and batched semaphore release.
- `thread/thread_basic`: Demonstrates thread creation, cooperative stop, and join.
- `thread/thread_cleanup`: Demonstrates pushing thread cleanup callbacks, popping the top cleanup on the normal path, and letting remaining cleanup handlers run automatically on thread exit.

### time (7)
- `time/basic_datetime`: Demonstrates basic date/time operations with `xrtNow`, `xrtDate`, `xrtTime`, `xrtDateSerial`, `xrtTimeSerial`, and `xrtDateTimeSerial`.
- `time/time_calc`: Demonstrates date/time arithmetic with `xrtDateAdd` and `xrtDateDiff`.
- `time/time_compare`: Demonstrates time comparison with `xrtIsSameDay`, `xrtIsSameMonth`, `xrtIsSameYear`, `xrtTimeInRange`, and `xrtTimeRangeOverlap`.
- `time/time_extract`: Demonstrates extracting time components with `xrtYear`, `xrtMonth`, `xrtDay`, `xrtHour`, `xrtMinute`, `xrtSecond`, `xrtWeekday`, and `xrtDayOfYear`.
- `time/time_format`: Demonstrates formatting and parsing datetime strings with custom patterns via `xrtTimeFormat` and `xrtTimeParse`.
- `time/timer`: Demonstrates high-precision timing with `xrtTimer` and sleep pauses with `xrtSleep`.
- `time/timezone`: Demonstrates time-zone conversion with `xrtNowUTC`, `xrtTimezoneOffset`, `xrtTimeToUTC`, and `xrtTimeToLocal`.

### tools (10)
- `tools/file_encrypt`: Demonstrates password-derived AES-256-GCM file encryption and decryption.
- `tools/file_hasher`: Demonstrates streaming SHA-256 hashing for files.
- `tools/http_server`: Demonstrates serving local HTML files with `xrtHttpd` and validating the response through a local client request.
- `tools/json_config`: Demonstrates creating, loading, updating, and saving JSON configuration files.
- `tools/kv_store`: Demonstrates in-memory KV storage backed by `xdict` plus JSON persistence.
- `tools/log_analyzer`: Implements a full log-file analyzer that can parse multiple log formats, extract useful information, generate statistics, detect patterns, and create visual reports across Apache, Nginx, Syslog, and custom formats.
- `tools/password_gen`: Demonstrates password generation and character-category coverage checks.
- `tools/tcp_chatroom`: Demonstrates a simple TCP chat room that broadcasts received messages to other clients.
- `tools/template_render`: Demonstrates rendering HTML from JSON data files and template files.
- `tools/web_crawler`: Implements a multi-threaded web crawler that can fetch pages, extract links, follow redirects, respect `robots.txt`, and save content, with concurrent crawling, depth limits, and domain filtering.

### value (7)
- `value/basic_types`: Demonstrates creating basic values such as `null`, `bool`, `int`, `float`, `text`, `time`, and `point` with `xvoCreate*`.
- `value/deep_copy`: Demonstrates shallow copy, deep copy, and nested-structure copying for value objects.
- `value/refcount`: Demonstrates value-object reference counting, shared references, and release timing.
- `value/value_array`: Demonstrates value-array append, insert, read, update, and removal.
- `value/value_coll`: Demonstrates writing collection elements, existence checks, and set-operation APIs on value collections.
- `value/value_list`: Demonstrates index-based set, read, and update on value lists.
- `value/value_table`: Demonstrates key-based set, read, existence checks, and removal on value tables.

### xhttpd (2)
- `xhttpd/server_async`: Demonstrates delayed HTTP responses through a background-thread future with xhttpd `OnRequestAsync`.
- `xhttpd/server_basic`: Demonstrates starting a local xhttpd server, handling a request, and validating the reply with a local HTTP client request.

### xid (1)
- `xid/generate_xid`: Demonstrates XID generation, encoding, decoding, and comparison.

### xson (3)
- `xson/generate_xson`: Demonstrates creating extended values such as `list`, `set`, `time`, and `class`, then generating XSON.
- `xson/parse_xson`: Demonstrates parsing JSON-compatible text plus `list`, `set`, `time`, and `class` extended values.
- `xson/xson_file`: Demonstrates XSON file write, read, and roundtrip verification.

### xurl (3)
- `xurl/url_build`: Demonstrates building `target`, `authority`, full URLs, and `Host` header text from parsed URL views.
- `xurl/url_parse`: Demonstrates parsing absolute URL views and reading `scheme`, `host`, `port`, `path`, `query`, `fragment`, and `Host` header text.
- `xurl/url_resolve`: Demonstrates resolving relative paths, query-only references, scheme-relative references, absolute paths, and fragment references against a base URL.

### xws (3)
- `xws/client_basic`: Demonstrates starting a local WebSocket client, sending `text` / `binary` / `ping` frames, and receiving echo callbacks from a local server.
- `xws/client_proxy`: Demonstrates attaching a shared HTTP CONNECT proxy object to xws client configuration and establishing a proxied local WebSocket connection.
- `xws/server_basic`: Demonstrates a local WebSocket server that sends a welcome message on open and replies to client text messages.
