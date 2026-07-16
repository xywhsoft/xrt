# XHTTP 动态 Header 存储修复

## 1. 修复范围

基线提交：`0273ea4da49b5d135818da4ca0cef6c2389f6d5e`

该补丁修复 HTTP/1 codec、HTTP 客户端和 HTTP 服务端对单个 header 名称和值设置固定长度上限的问题。修复后，header 名称和值按实际长度保存，不再通过增大固定数组规避问题。

本次问题最初由 DeepSeek 流式接口触发。服务端返回合法响应头：

```http
Access-Control-Allow-Credentials: true
```

字段名 `Access-Control-Allow-Credentials` 长度为 32。旧 codec 使用 `XCODEC_HTTP1_TOKEN_CAP == 32`，并通过 `iNameLen >= XCODEC_HTTP1_TOKEN_CAP` 拒绝字段，因此实际只能接受最多 31 个字符。TLS 握手和 HTTP 请求发送均已成功，但响应头解析返回 `XCODEC_STATUS_ERROR`，上层最终只能看到误导性的网络请求失败。

## 2. 根因

旧实现存在三层独立固定数组：

1. `xcodechttp1header`：名称 32 字节，值 256 字节。
2. `xhttpheader`：名称 64 字节，值 256 字节。
3. `xhttpdheader`：名称 64 字节，值 256 字节。

只扩大第一层或只修改 DeepSeek 对应长度并不能彻底解决问题。即使 codec 解析成功，客户端响应物化、服务端请求物化或响应序列化仍可能截断更长字段。

HTTP 规范没有定义 31 或 63 字符的通用字段名上限。资源限制应施加在完整 header 区域的总字节数和字段数量上，而不是通过很小的单字段结构体数组静默截断合法数据。服务端原有 `iHeaderLimit` 和 `iHeaderBytesLimit` 继续承担聚合资源限制职责。

## 3. 修复设计

### 3.1 HTTP/1 codec

`xcodechttp1header.sName` 和 `sValue` 改为字符串指针。解析器仍只复制一次完整 header 区域，随后在该连续缓冲区内写入 NUL 并让各 header 项指向对应名称和值。

`xcodechttp1msg.pHeaderStorage` 持有这块连续缓冲区，`xrtCodecHttp1MessageUnit()` 统一释放它。该方案具有以下性质：

- 不按 header 数量进行额外字符串分配。
- 名称和值保持完整，不发生截断。
- header 表扩容时只移动指针，不复制或释放字符串。
- 所有解析失败路径仍释放临时缓冲区。

成功调用 `xrtCodecHttp1Parse()` 或 `xrtCodecHttp1ParseHead()` 后，调用方必须调用 `xrtCodecHttp1MessageUnit()`。

### 3.2 HTTP 客户端

`xhttpheader` 改为动态字符串指针，并通过 `XHTTP_HEADER_F_OWN_STRINGS` 标记字符串所有权。

请求设置、追加、删除、异步请求克隆、响应构造和响应销毁均改为显式深拷贝或释放。分配新名称和值时采用事务式更新：两次分配全部成功后才替换原 header，因此内存不足不会破坏旧值。

HEAD 响应原先维护了一套重复的响应头解析器。补丁删除这套分支，改为统一调用 `xrtCodecHttp1ParseHead()`，避免普通、流式和 HEAD 路径以后再次产生长度策略差异。

### 3.3 HTTP 服务端

`xhttpdheader` 同样改为动态字符串和明确的所有权标志。以下路径均进行完整深拷贝：

- codec 请求到 `xhttpdrequest` 的物化。
- `xrtHttpdResponseSetHeader()` 和 `xrtHttpdResponseAddHeader()`。
- 异步响应及内部响应对象复制。
- `xrtHttpdResponseReply()` 对 header block 的解析。

响应序列化不再使用 512 字节临时行缓冲拼接用户 header，而是将名称、分隔符、值和 CRLF 分段追加到动态输出缓冲，防止序列化阶段再次截断。

## 4. API 和 ABI 影响

这是公开结构体布局变更：

- `xcodechttp1header`
- `xcodechttp1msg`
- `xhttpheader`
- `xhttpdheader`
- 间接包含上述类型的 request / response 结构体

因此补丁合入后必须重新编译 XRT DLL、静态库和所有直接包含 `xrt.h` 的依赖。旧 DLL 不能与新头文件混用，新 DLL 也不能与按旧结构体布局编译的调用方混用。

`XCODEC_HTTP1_TOKEN_CAP` 仍用于方法和 HTTP 版本等协议 token。`XCODEC_HTTP1_VALUE_CAP`、`XHTTP_HEADER_NAME_CAP`、`XHTTP_HEADER_VALUE_CAP`、`XHTTPD_HEADER_NAME_CAP` 和 `XHTTPD_HEADER_VALUE_CAP` 为源码兼容继续保留，但不再是 header 名称和值的运行时上限。

不要按值复制已经初始化的请求、响应或 codec 消息，也不要使用 `memcpy` 复制它们。应使用公开初始化、设置和释放 API。

## 5. 修改文件

- `lib/xcodec_http1.h`：动态解析存储和消息生命周期。
- `lib/xhttp.h`：客户端 header 所有权、深拷贝、删除、销毁及 HEAD 解析统一。
- `lib/xhttpd.h`：服务端 header 所有权、深拷贝、header block 和动态序列化。
- `xrt.h`：同步公开结构体声明。
- `test/test_xnet2_codec.h`：codec 与 DeepSeek 响应头回归。
- `test/test_xnet_http.h`：客户端请求、普通响应和流式响应回归。
- `test/test_xnet_httpd.h`：服务端请求物化、响应存储、header block 和序列化回归。
- `docs/api/api-xhttp*.md`、`docs/api/api-xhttpd*.md`：公开所有权和长度语义。
- `singlehead/xrt.h`：由修改后的主源码重新生成的发布单头文件。

`singlehead/xrt.h` 是生成文件，应在主源码和 `xrt.h` 更新后通过 `build_single_head.bat` 重新生成，不应单独手工修复。

## 6. 回归测试

新增测试覆盖：

- DeepSeek 的 `Access-Control-Allow-Credentials` 响应头。
- 320 字符字段名和 1024 字符字段值。
- HTTP 请求保存、序列化和异步深拷贝。
- 普通客户端响应的完整字段保留。
- 流式响应 `OnHeaders` 路径。
- HTTPD 请求物化和索引访问。
- HTTPD 响应设置、header block、动态序列化和生命周期释放。
- 原有 HEAD、chunked、keep-alive、TLS、超时和异步响应路径。

Windows GCC 验证命令：

```bat
gcc -m64 test.c -I . -lWs2_32 -lIPHLPAPI -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections -Wall -Wextra -o xrt_header_tests.exe
xrt_header_tests.exe xnet2_codec
xrt_header_tests.exe xnet_http
xrt_header_tests.exe xnet_httpd
xrt_header_tests.exe xnet_ws
```

四个测试组均应返回 `[PASS]` 和退出码 0。补丁开发环境中的四组测试均已通过；重新生成的 `singlehead/xrt.h` 也分别通过 GCC 和 TCC 编译及长 header 运行探针。

此外，使用 `xllm`、修改后的模块化 XRT 源码和 IDE 中现有 DeepSeek profile 发起真实 HTTPS/SSE 请求，`deepseek-v4-flash` 返回 HTTP 200 和预期内容。验证程序仅通过进程环境接收密钥，未在源码、日志或补丁中保存凭据。

## 7. 同步步骤

1. 确认目标线程基于相同或更新的 XRT 主线。
2. 优先同步 `lib/xcodec_http1.h`、`lib/xhttp.h`、`lib/xhttpd.h` 和对应测试。
3. 解决新主线冲突时保留动态字符串所有权、深拷贝和统一销毁语义，不要恢复任何单字段长度判断。
4. 同步 `xrt.h` 的公开声明。
5. 运行 `build_single_head.bat` 重新生成 `singlehead/xrt.h`。
6. 运行三个网络测试组和目标平台完整测试。
7. 重新编译所有依赖 XRT 公开结构体的 DLL 和程序。

## 8. 验收标准

- DeepSeek 流式响应能够进入 header 回调并继续接收 SSE body。
- 任意合法、且完整 header 区域未超过聚合资源限制的名称和值都不会因单字段固定数组而失败或截断。
- 分配失败时返回错误且不泄漏、不保留半初始化 header。
- 请求、响应和 codec 消息释放后不发生重复释放。
- 普通、流式、HEAD、HTTPD 和 WebSocket 共用 codec 的路径保持通过。
