# HTTP / WebSocket 发布门禁

本门禁用于冻结 XRT 的 HTTP/1.1、HTTP 语义对象、HTTPD/XWeb 与 RFC 6455 WebSocket 契约。HTTP/2、HTTP/3 与 QUIC 不在本阶段范围内。

## Windows

```bat
build_HTTP_WS_RELEASE_GATE_x64.bat
```

可通过 `FUZZ_ROUNDS` 和 `STRESS_ROUNDS` 覆盖确定性模糊测试轮数与重连轮数。默认分别为 1000 和 500。

## POSIX / MinGW shell

```sh
./build_http_ws_release_gate.sh
```

门禁执行以下检查：

1. 构建并运行 URL、HTTP semantics、HTTP client、HTTPD、XWeb 和 XWS 主测试。
2. 以 `-Wall -Wextra -Werror` 独立编译 `xrt.c`。
3. 运行 HTTP OOM/failpoint 测试。
4. 对 HTTP/1.1 与 WebSocket parser 运行确定性分块输入驱动。
5. 运行 Python 标准库实现的独立 RFC 6455 peer，双向验证握手、mask、压缩分片、Ping/Pong 与 Close。
6. 运行正常重连与 1009 协议限制关闭混合压力测试，并验证下一连接可继续建立。
7. 构建并运行 single-header 测试。

## 原生 libFuzzer

确定性驱动保证没有 Clang/libFuzzer 的环境也能执行 parser 回归，但不能替代覆盖率引导模糊测试。安装 Clang 后运行：

```sh
RUNS=100000 ./build_fuzz_http.sh
RUNS=100000 ./build_fuzz_ws.sh
```

两个脚本默认启用 `fuzzer,address,undefined`。崩溃样本必须进入持久 corpus 或最小化成单元测试后才能发布。

## xlang 集成门禁

在 `D:\GIT\x-lang\demo6` 执行：

```bat
build.bat
release\xl.exe run test\cases\stdlib_http_semantics.xl
release\xl.exe run test\cases\stdlib_websocket.xl
release\xl.exe run test\cases\stdlib_websocket_http_upgrade.xl
release\xl.exe run test\cases\stdlib_websocket_tls.xl
```

共享 HTTP Upgrade 用例同时覆盖解释运行与静态构建。HTTP server 与 WebSocket manager 必须使用同一个 `NetEngine`，并按 WebSocket、HTTP server、engine 的顺序销毁。

## 发布判定

- 所有命令退出码为 0，输出中不得出现 `FAIL`。
- ASan/UBSan/libFuzzer 不得报告越界、UAF、泄漏或未定义行为。
- 协议限制关闭后必须能够继续 accept/connect。
- single-header 与模块化头文件行为一致。
- 公开结构的 size/version、所有权、线程、背压、取消和关闭语义不得只依赖实现细节。
