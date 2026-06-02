# XRT 范例索引

[English](README.en.md) | 简体中文 | [项目首页](../README.md) | [文档入口](../docs/README.md)

本目录当前包含 `166` 个可独立构建的范例程序，分布在 `39` 个模块目录下。本文档按当前 `examples/*/*/main.c` 的实际实现整理，面向使用者阅读和检索。

`examples/bin` 是批量构建后的输出目录，不计入范例数量。`example_common.h`、`example_wait.h`、`example_tls_fixture.h` 是部分范例共用的辅助头文件，也不是独立范例。

## 1. 新增或补充范例时的统一约束

- 目录结构统一为 `examples/<module>/<example>/main.c`，一个目录对应一个可独立构建、可独立运行的范例。
- 每个范例文件顶部都应保留双语头注释，至少包含 `Title`、`Description / 说明`、`Build / 编译`，必要时再补 `Usage / 用法`、`Note / 注意`。
- 头注释、关键步骤说明、用户可见输出、命令行用法优先保持中英双语；函数内部不必机械地逐行双语，但要保证使用者能直接看懂范例目的和结果。
- C 代码风格保持与现有范例一致：使用 `1` 个 tab 缩进，变量沿用匈牙利命名法，函数定义的大括号单独起行，`if/for/while` 等控制语句的大括号与语句同行。
- 组合条件判断应显式加括号；示例代码应尽量写成“读代码即可复用”的最小完整片段，不依赖隐式上下文。
- 范例应优先使用 XRT 的公开接口，不额外扩展依赖；能直接基于 `singlehead/xrt.h` 实现的范例，优先保持单头文件模式，需要完整网络/HTTP/TLS/WebSocket 实现时再使用 `xrt.c`。
- 范例必须考虑 Windows 和 Linux 的跨平台兼容性，避免写死仅单平台有效的路径、命令或行为差异。
- 网络类范例优先使用本地回环、自建临时服务、自验证请求，不把外部网络环境作为正确性的前提条件。
- 文件类范例应在运行结束后清理临时文件；服务类范例应能自动收尾，不要求用户手工杀进程。
- 每个范例顶部的 `Build / 编译` 说明应给出可直接复制的 `tcc` 和 `gcc` 命令，输出文件统一放到 `examples/bin`。

## 2. 使用方式

- Windows `cmd` 批量构建：`examples\\build_all.bat`
- Windows `PowerShell` 批量构建：`./examples/build_all.ps1`
- Linux / macOS shell 批量构建：`./examples/build_all.sh`
- 单个范例的推荐编译命令，直接查看对应 `main.c` 顶部的 `Build / 编译` 段落即可。
- 网络、HTTP、TLS、WebSocket 一类范例在运行前，建议先阅读源码头注释中的说明和参数约束。

## 3. 范例索引

### array（3）
- `array/array_sort`：演示结构体数组和指针数组的自定义比较排序。
- `array/ptr_array`：演示指针数组的追加、插入、设置、交换、删除和 AddAlt。
- `array/struct_array`：演示 `xrtArrayCreate`、`xrtArrayInsert`、`xrtArrayAppend`、`xrtArrayGet`、`xrtArrayRemove` 的结构体数组操作。

### avltree（3）
- `avltree/avltree_custom`：演示字符串键、自定义比较函数和降序遍历。
- `avltree/avltree_iterator`：演示 AVL 树的有序遍历和空树迭代。
- `avltree/basic_avltree`：演示 AVL 树的创建、插入、查找、删除和平衡行为。

### base（6）
- `base/error_handling`：演示 `xrtSetError`、`xrtClearError`、`xrtGetError` 和 `OnError` 回调。
- `base/init_and_config`：演示 `xrtInit` / `xrtUnit` 生命周期以及公开的全局配置项。
- `base/mem_debug`：演示启用内存调试、导出文本和 JSON 报告、读取生成文件并清理它们。
- `base/mem_telemetry`：演示启用内存遥测、执行几次分配，并读取聚合后的快照计数。
- `base/memory_basic`：演示 `xrtMalloc`、`xrtCalloc`、`xrtRealloc`、`xrtFree` 的安全内存分配和错误处理。
- `base/temp_memory`：演示 `xrtTempMemory` 与 `xrtFreeTempMemory` 的使用方式。

### bsmm（1）
- `bsmm/block_memory`：演示固定大小块内存的分配、释放、重用和按索引访问。

### buffer（2）
- `buffer/basic_buffer`：演示 `xrtBufferCreate`、`xrtBufferAppend`、`xrtBufferInsert`、`xrtBufferDestroy` 的动态缓冲区操作。
- `buffer/buffer_builder`：演示使用 `xbuffer` 作为 StringBuilder 进行高效字符串拼接和操作。

### charset（3）
- `charset/charset_detect`：演示 `xrtIsUTF8` 验证 UTF-8 编码字符串。
- `charset/endian_convert`：演示 `xrtUTF16LEtoBE` 和 `xrtUTF32LEtoBE` 在小端序和大端序之间转换。
- `charset/utf_convert`：演示 UTF-8 / UTF-16 / UTF-32 编码转换函数。

### coroutine（7）
- `coroutine/basic_coroutine`：演示单个协程的创建、恢复、挂起与销毁。
- `coroutine/cancel_and_join`：演示在同一调度器中进行协作式协程取消，以及由另一个协程执行 `join`。
- `coroutine/cleanup_and_result`：演示协程 cleanup 栈、显式弹出 cleanup、结果存储，以及协程结束后的 user-data 读取。
- `coroutine/coroutine_sleep`：演示在调度器管理的协程中使用 `xrtCoSleep`。
- `coroutine/event_wait`：演示在调度器协程流程中进行事件超时等待、deadline 等待和事件唤醒。
- `coroutine/scheduler`：演示调度器管理多个协程并逐步推进执行。
- `coroutine/userdata`：演示 `xrtCoSetUserData` 与 `xrtCoGetUserData`。

### crypto（10）
- `crypto/aes_gcm`：演示 AES-128-GCM 与 AES-256-GCM 的往返加解密。
- `crypto/chacha20`：演示 ChaCha20 流加密与 AEAD 往返验证。
- `crypto/hash_functions`：演示当前 XRT 真实公开的加密原语：SHA-256、增量 SHA-384、HMAC-SHA256 和 HKDF-SHA256。
- `crypto/hkdf`：演示基于 SHA-256 与 SHA-384 的 HKDF 提取与扩展。
- `crypto/hmac`：演示 HMAC-SHA256、HMAC-SHA384 与 HMAC-SHA512。
- `crypto/key_exchange`：演示 X25519 与 ECDH secp256r1 的共享密钥推导。
- `crypto/random_bytes`：演示加密安全随机字节生成。
- `crypto/sha256`：演示一次性与分块式 SHA-256 哈希计算。
- `crypto/sha512`：演示 SHA-512 与 SHA-384 哈希接口。
- `crypto/x25519_chacha20poly1305`：演示共享秘密协商、HKDF 派生会话密钥、AEAD 加解密往返，以及 Ed25519 签名与验证。

### dict（3）
- `dict/basic_dict`：演示字典的设置、读取、存在性检查和删除。
- `dict/dict_iterator`：演示字典条目的迭代遍历。
- `dict/dict_ptr`：演示在字典中存储、读取、更新和删除指针值。

### file（6）
- `file/binary_io`：演示 `xrtPut`、`xrtGet`、`xrtFilePutAll` 与 `xrtFileGetAll`。
- `file/dir_operations`：演示 `xrtDirCreate`、`xrtDirCreateAll`、`xrtDirScan`、`xrtDirScanEx`、`xrtDirCopy`、`xrtDirDelete` 的目录管理。
- `file/file_info`：演示 `xrtFileGetSize`、`xrtFileGetAttr`、`xrtPathExists` 获取文件元数据。
- `file/file_manage`：演示 `xrtFileCopy`、`xrtFileMove`、`xrtFileDelete` 的文件管理操作。
- `file/file_readall`：演示 `xrtFileReadAll`、`xrtFileWriteAll`、`xrtFileAppend` 的整文件读写。
- `file/read_write`：演示 `xrtOpen`、`xrtClose`、`xrtRead`、`xrtWrite` 的基本文件 I/O 操作。

### file_async（3）
- `file_async/async_read_write`：演示显式偏移异步写入、`flush`、文件大小查询和读取。
- `file_async/dir_tasks`：演示异步目录创建、复制、移动和删除任务。
- `file_async/file_helpers`：演示异步文本 / 二进制整文件 helper，以及文件复制、移动、删除。

### future（6）
- `future/continuation_chain`：演示当前线程上下文中的 `Then` / `Catch` / `Finally` 延续链。
- `future/promise_basic`：演示手动创建 future / promise，以及 `resolve`、`wait` 和结果读取。
- `future/task_coroutine`：演示在协程调度器中运行任务，并等待其 future 结果。
- `future/task_group_scope`：演示任务组的 `close` / `join` 流程，以及父作用域关闭时对子作用域的取消传播。
- `future/task_thread`：演示在线程中运行任务，并读取返回的 future 结果。
- `future/when_all_any_race`：演示 Future 聚合完成、胜出源索引和全部值读取。

### hash（2）
- `hash/hash32`：演示 `xrtHash32` 和 `xrtHash32_WithSeed` 的 32 位哈希计算。
- `hash/hash64`：演示 `xrtHash64`、`xrtHash64_Micro`、`xrtHash64_Nano` 的不同质量 64 位哈希计算。

### http（6）
- `http/http_advanced`：演示当前低层 HTTP 请求配置 API。
- `http/http_cookies`：演示发送 Cookie 头以及解析 `Set-Cookie` 响应。
- `http/http_download`：演示抓取远程响应正文并写入本地文件。
- `http/http_get`：演示使用当前 XNet HTTP 客户端执行阻塞式 GET 请求。
- `http/http_post`：演示基于当前请求对象 API 的表单式 POST 请求。
- `http/http_upload`：演示手工构造 multipart 请求体实现上传。

### http_util（3）
- `http_util/cookie_and_media_type`：演示解析 `Set-Cookie`、media type、multipart boundary，以及带 UTF-8 文件名扩展的 `Content-Disposition`。
- `http_util/multipart_stream`：演示构建 multipart body、从 `Content-Type` 提取 boundary，并通过 `xrtMultipartStream` 做增量解析。
- `http_util/query_and_header`：演示遍历 query 键值、解码参数值、拆分单行 header、扫描 header block，以及构建规范化 header。

### jnum（2）
- `jnum/num_to_str`：演示 JNUM 里的有符号十进制、无符号十六进制和 double 格式化。
- `jnum/str_to_num`：演示 `xrtParseNum` / `xrtParseNumSkipSpace` 的 token 解析，以及 `xrtStrTo*` 的便捷转换边界。

### json（5）
- `json/generate_json`：演示创建值对象并生成紧凑或格式化 JSON。
- `json/json_file`：演示 JSON 文件写入、读取和 roundtrip 校验。
- `json/parse_json`：演示将 JSON 字符串解析为值对象并读取对象、数组、嵌套字段。
- `json/sax_parser`：演示基于回调的流式 JSON SAX 解析。
- `json/sax_printer`：演示按事件增量构建对象、数组、嵌套和紧凑 JSON 输出。

### list（2）
- `list/basic_list`：演示列表的按索引设置、读取、存在性检查、删除和稀疏索引。
- `list/list_iterator`：演示列表的迭代遍历和稀疏索引 walk。

### math（2）
- `math/approx`：演示整数和浮点数的约等于比较，支持差值模式和百分比模式，可配置容差。
- `math/random`：演示 PCG 随机数生成，包括全局随机数、独立 `xrand` 状态、范围生成和线程安全版本。

### mempool（3）
- `mempool/fs_mempool`：演示定长内存池的分配、释放、跨页扩容和 GC helper。
- `mempool/general_mempool`：演示通用内存池的可变大小分配、释放和 cutoff 配置。
- `mempool/mempool_gc`：演示定长与通用内存池的 mark / sweep 垃圾回收流程。

### memunit（1）
- `memunit/mem_unit`：演示 `256` 槽内存单元的分配、按索引释放和标记回收。

### network（12）
- `network/dgram_future`：演示通过 `xrtNetDgramRecvFuture` 等待一个 UDP 数据报，并读取返回的数据包内容。
- `network/dns_resolve`：演示将主机名解析为网络地址。
- `network/engine_post`：演示在 XNet 引擎 worker 上进行即时投递、延迟投递，以及带 future 返回值的任务桥接。
- `network/listener_future`：演示通过 `xrtNetListenerAcceptFuture` 等待本地 TCP `accept`，然后继续交换数据。
- `network/local_info`：演示本机 IP、MAC 和主机名获取。
- `network/ringbuf`：演示当前公开的缓冲原语 `xrtNetChain`。
- `network/socket_basic`：演示监听、连接、发送与接收的基础流程。
- `network/stream_wait`：演示在本地 TCP 对端之间使用 stream readable wait-source 和 drain 等待。
- `network/tcp_client`：演示连接本地 TCP 服务器并发送一条消息。
- `network/tcp_echo_server`：演示使用 `xrtNet` 监听器与流回调实现 TCP Echo 服务器。
- `network/udp_client`：演示向本地 UDP 服务器发送一个数据报。
- `network/udp_echo`：演示本地回环上的 UDP 发送、接收与回显。

### network_tls（2）
- `network_tls/tls_session_handshake`：演示在内存中驱动 client / server TLS session 握手、通过加密通道交换明文，以及处理 `close-notify`。
- `network_tls/tls_session_resume`：演示从首次内存握手中导出 TLS resume 状态，并在后续 TLS 1.2 会话中复用完成恢复握手。

### os（2）
- `os/run_program`：演示 `xrtRun`、`xrtStart`、`xrtChain` 执行外部程序。
- `os/subprocess_basic`：演示 direct exec、shell 命令执行、stdin 管道写入、event 时间线轮询，以及 terminal 输出捕获。

### path（3）
- `path/path_check`：演示 `xrtPathIsAbs`、`xrtPathRandom`、`xrtPathExists` 的路径验证和生成。
- `path/path_join`：演示 `xrtPathJoin` 正确处理分隔符并拼接多个路径段。
- `path/path_parse`：演示 `xrtPathGetNameExt`、`xrtPathGetName`、`xrtPathGetExt`、`xrtPathGetDir` 提取路径组件。

### queue（4）
- `queue/mpmc_basic`：演示使用工作线程驱动的多生产者多消费者队列。
- `queue/mpsc_batch`：演示 MPSC 队列的批量 `push` / `pop`、关闭、`drain` 和 `reset`。
- `queue/mpsc_wait`：演示 MPSC wait wrapper 的超时等待和阻塞式弹出。
- `queue/spsc_basic`：演示 SPSC 队列的创建、压入、弹出、关闭、`drain` 和 `reset`。

### regex（2）
- `regex/capture_config_line`：演示命名捕获、捕获范围和 Builder flags。
- `regex/regex_set_scan`：演示用 `xregexset` 做多模式分类。

### stack（3）
- `stack/dynamic_stack`：演示动态栈的自动扩容、压栈、出栈和任意位置访问。
- `stack/ptr_stack`：演示静态栈和动态栈的指针压栈、出栈、取顶和位置访问。
- `stack/static_stack`：演示定长静态栈的压栈、出栈、取顶和溢出保护。

### string（10）
- `string/case_convert`：演示字符串大小写转换函数 `xrtLCase` 和 `xrtUCase`，支持 UTF-8 安全的字符跳过功能。
- `string/copy_and_compare`：演示字符串复制函数（`xrtCopyStr` / `xrtCopyMem`）和比较函数（`xrtStrComp`），支持区分大小写和不区分大小写模式。
- `string/encoding`：演示十六进制、Base64 与百分号编码辅助函数。
- `string/format_and_replace`：演示 `xrtFormat` 的 printf 风格格式化和 `xrtReplace` 子字符串替换操作。
- `string/number_format`：演示 `xrtIntFormat` 和 `xrtNumFormat` 实现带千位分隔符的本地化数字格式化。
- `string/random_string`：演示 `xrtRandStr` 基于字符模板生成随机字符串。
- `string/search_and_match`：演示字符串搜索函数（`xrtFindStr` / `xrtInStr`）和模式匹配函数（`xrtCheckStr` / `xrtStrLike`），支持通配符匹配。
- `string/similarity`：演示 `xrtStrSim` 计算字符串相似度和 `xrtStrApprox` 进行近似字符串比较。
- `string/split`：演示 `xrtSplit` 的复制模式与原地修改模式。
- `string/trim_and_filter`：演示字符串裁剪函数（`xrtLTrim` / `xrtRTrim` / `xrtTrim`）和字符过滤函数（`xrtFilterStr`），支持自定义字符集。

### template（5）
- `template/basic_template`：使用当前 XTE API 演示基础模板解析与渲染。
- `template/template_control`：演示 `if` / `else`、`for`、`foreach`、`break`、`continue` 的渲染效果。
- `template/template_expr`：通过 `if` / `elseif` 和内联布尔输出演示当前模板表达式能力。
- `template/template_sub`：演示局部 `define` / `include` 与外部 include map 渲染。
- `template/template_vars`：演示嵌套路径、数组索引和混合值渲染。

### thread（8）
- `thread/attach_current`：演示将宿主自行创建的原生线程附加到 XRT 运行时、使用线程绑定 API，然后再分离。
- `thread/condvar`：演示条件变量的 `Signal` 与 `Broadcast` 唤醒。
- `thread/mutex`：演示堆分配互斥体与嵌入式互斥体两种用法。
- `thread/producer_consumer`：演示使用互斥体和条件变量实现经典生产者消费者队列。
- `thread/rwlock`：演示读锁、升级、降级与写锁操作。
- `thread/semaphore`：演示等待、释放与批量释放信号量。
- `thread/thread_basic`：演示线程创建、协作停止与等待结束。
- `thread/thread_cleanup`：演示压入线程 cleanup 回调、在正常路径弹出顶部 cleanup，并让剩余 cleanup 在线程退出时自动执行。

### time（7）
- `time/basic_datetime`：演示 `xrtNow`、`xrtDate`、`xrtTime`、`xrtDateSerial`、`xrtTimeSerial`、`xrtDateTimeSerial` 的基本日期时间操作。
- `time/time_calc`：演示 `xrtDateAdd` 和 `xrtDateDiff` 进行日期时间算术运算。
- `time/time_compare`：演示 `xrtIsSameDay`、`xrtIsSameMonth`、`xrtIsSameYear`、`xrtTimeInRange`、`xrtTimeRangeOverlap` 进行时间比较。
- `time/time_extract`：演示 `xrtYear`、`xrtMonth`、`xrtDay`、`xrtHour`、`xrtMinute`、`xrtSecond`、`xrtWeekday`、`xrtDayOfYear` 提取时间组件。
- `time/time_format`：演示 `xrtTimeFormat` 和 `xrtTimeParse` 使用自定义模式格式化和解析日期时间字符串。
- `time/timer`：演示 `xrtTimer` 高精度计时和 `xrtSleep` 暂停执行。
- `time/timezone`：演示 `xrtNowUTC`、`xrtTimezoneOffset`、`xrtTimeToUTC`、`xrtTimeToLocal` 进行时区转换。

### tools（10）
- `tools/file_encrypt`：演示基于密码派生 AES-256-GCM 的文件加解密。
- `tools/file_hasher`：演示文件的流式 SHA-256 计算。
- `tools/http_server`：演示使用 `xrtHttpd` 提供本地 HTML 文件，并通过本地客户端请求验证。
- `tools/json_config`：演示 JSON 配置文件的创建、加载、更新与保存。
- `tools/kv_store`：演示基于 `xdict` 的内存 KV 存储与 JSON 持久化。
- `tools/log_analyzer`：实现全面的日志文件分析器，能够解析各种日志格式、提取有意义的信息、生成统计数据、检测模式并创建可视化报告，覆盖 Apache、Nginx、Syslog 和自定义格式。
- `tools/password_gen`：演示密码生成以及字符类别覆盖检查。
- `tools/tcp_chatroom`：演示一个简单 TCP 聊天室，收到的消息会广播给其他客户端。
- `tools/template_render`：演示从 JSON 数据文件和模板文件渲染 HTML。
- `tools/web_crawler`：实现多线程网络爬虫，能够抓取网页、提取链接、跟随重定向、遵守 `robots.txt` 规则并保存内容，支持并发爬取、深度限制和域名过滤。

### value（7）
- `value/basic_types`：演示 `xvoCreate*` 创建 `null`、`bool`、`int`、`float`、`text`、`time`、`point` 等基础值。
- `value/deep_copy`：演示值对象的浅拷贝、深拷贝以及嵌套结构复制。
- `value/refcount`：演示值对象的引用计数、共享引用和释放时机。
- `value/value_array`：演示值数组的追加、插入、读取、更新和删除。
- `value/value_coll`：演示值集合的元素写入、存在性检查和集合运算接口。
- `value/value_list`：演示值列表的按索引设置、读取和更新。
- `value/value_table`：演示值表的按键设置、读取、存在性检查和删除。

### xhttpd（2）
- `xhttpd/server_async`：演示使用 xhttpd 的 `OnRequestAsync`，通过后台线程 future 延迟返回 HTTP 响应。
- `xhttpd/server_basic`：演示启动本地 xhttpd 服务、处理请求，并通过本地 HTTP 客户端请求验证响应。

### xid（1）
- `xid/generate_xid`：演示 XID 的生成、编码、解码与比较。

### xson（3）
- `xson/generate_xson`：演示创建 `list` / `set` / `time` / `class` 等扩展值并生成 XSON。
- `xson/parse_xson`：演示解析 JSON 兼容文本以及 `list` / `set` / `time` / `class` 扩展值。
- `xson/xson_file`：演示 XSON 文件写入、读取和 roundtrip 校验。

### xurl（3）
- `xurl/url_build`：演示从已解析的 URL 视图构建 `target`、`authority`、完整 URL，以及 `Host` 头文本。
- `xurl/url_parse`：演示解析绝对 URL 视图，并读取 `scheme`、`host`、`port`、`path`、`query`、`fragment` 以及 `Host` 头文本。
- `xurl/url_resolve`：演示把相对路径、仅 query、协议相对、绝对路径与 fragment 引用解析到基准 URL 上。

### xws（3）
- `xws/client_basic`：演示启动本地 WebSocket client，发送 `text` / `binary` / `ping` 帧，并从本地 server 接收回显回调。
- `xws/client_proxy`：演示把共享的 HTTP CONNECT 代理对象接入 xws client 配置，并建立经过代理的本地 WebSocket 连接。
- `xws/server_basic`：演示本地 WebSocket server 在连接打开时主动发送 welcome 消息，并对 client 文本消息作出回复。
