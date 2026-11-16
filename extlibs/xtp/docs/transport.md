# 现有承载接入

核心仅依赖 Codec、Session 和通用有界排空器。选择 `xtp_tcp`、`xtp_tls`、`xtp_ws` 时才引入相应依赖；`xtp_native` 是三者的便利选择，不创建网络引擎或接管事件。

宿主读循环也须守住执行顺序：原生 readiness 后端的 Read/Consume 可能同步驱动下一次 Read 回调。若采用“复制读取，再交给 XTP”，用实例级 Reading 标志阻止嵌套读循环抢先处理后续字节；也可先解析借用的前缀，再消费准确 Used。写排空同样使用 Flushing 标志合并原生 Writable/Drain 的递归提示，避免在尚未 Advance 的输出上再次发送。回环例程展示这些宿主边界。

网络数据仍由宿主读取并交给 Input，按 Used 消费。需要发包时调用 `xtpDrain(session, lane, writer, transport, budget, now, &progress)`。Bytes/Frames/ChunkBytes 限制每轮工作，STREAM 可短写，RECORD 不拆分。AGAIN 后等待可写或安排下一有界轮次。错误时已真正受理的前缀仍记入 Advance，未受理数据保留；宿主调用 Unbind 报告断线。写回调内对同一 Session 的重入会被拒绝，其他连接与 Session 的调度仍由宿主管理。

`xtpTcpWrite` 借用已有 OPEN TCP stream，复制受理并自动限制到原生 WriteLimit。输入用 STREAM。该接口明确是可信本地或测试明文选择，不能冒充安全连接。

`xtpTlsPolicyInit` 提供 TLS 1.3、X25519、Ed25519/ECDSA P-256 签名、ChaCha20-Poly1305 优先及可选 AES-128-GCM 的策略。创建 XRT client/server context 时采用它，直连 ALPN 设置为 `xtp/3`；配置验证器和服务器名/证书身份，不启用早期业务数据。`xtpTlsWrite` 再检查实际 OPEN、版本、套件和 ALPN，按原生 Written 推进，不提供降级。TLS 打开不代替服务器侧的应用主体认证，Bind.Authenticated 必须由宿主在其认证边界之后给出。

`xtpWsWrite` 借用已有 xws connection，要求 `xtp.v3` 子协议，复制一条 Binary RECORD。xws 负责 Mask、控制帧、分片和关闭；压缩沿用原生默认关闭。WSS 的 TLS ALPN 服务于 HTTP Upgrade，不错误地设置成直连 `xtp/3`。接收侧初始化 `xtpwsinput`，从 MessageBegin/Data/End 分别调用 Begin/Data/End；Begin 要求可靠 RECORD lane，拒绝 Text。Data 的未消费后缀必须保留，暂停时同时暂停 xws，恢复后重交后缀并完成 End，不能直接开始下一条 WS 消息。

三个回环例程使用真实本地 TCP；TLS 例程使用固定测试 Ed25519 身份和精确证书 pin，展示 TLS 1.3 + ChaCha20。该 pin 只用于测试，不是生产 CA 配置。WS 例程接入已经完成 Upgrade 的连接，不包含 HTTP 握手工厂。

```sh
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp_native_tests --cflag=-O2
```

预期：原生 TCP、TLS 1.3 ChaCha/AES、WS 双向三类消息通过；证书拒绝和 TLS 1.2 拒绝均不交付业务；原生 stream/engine 实际退休后才释放宿主状态。模块和单头运行同一断言，例程分别输出 PASS。程序自动关闭连接与引擎，不监听固定端口、不联网、不留下服务进程。
