# xrudp 游戏承载

选择 `XTP_MODULE_XTP_XRUDP`。`xtpXrudpWrite` 借用已经完成 Noise/地址证明的 xrudp session，不创建 endpoint、socket 或线程。STREAM 映射到原生 BYTES 流，RECORD 映射到已配置 DATAGRAM channel；首次接入不叠加完整消息组装器，也不叠加 TLS。每个实际接收流方向使用独立 XTP lane/Reader。

`xtpxrudpwriter` 指定 StreamID（0 取默认 1）、ChannelID 和本轮新鲜 NowUs；把它作为 xtpDrain 的 User。每次数据报写入都查询当前 DatagramSendLimit，完整 XTP 头、命令、参数及 Body 一起受限制。超限的未发送 NOTIFY 以 LOCAL_ERROR/E_LIMIT/NOT_SENT 完成，Drain.Dropped 增加；可靠 lane 保持可用，没有自动切流或 IP 分片。已经被 xrudp 接管的 NOTIFY 只完成 SENT，后续过期/丢失属于不可靠投递，不冒充远端收到。

宿主将原生 StreamRead 的字节交给相应 STREAM lane，将 DatagramRead 的完整报文交给对应 RECORD lane并设置 RecordEnd。PAUSE/AGAIN 时仍须保存未消费后缀。原生 Poll 由宿主在工作后及 next_due 到期时驱动；它独立负责分片、重传、拥塞、pacing、MTU 和加密，XTP 不重复执行。

可运行的 [游戏例程](../examples/game/main.c) 使用真实本机 UDP、真实 Noise IK/可信静态公钥/地址 cookie，包含三条 lane：默认可靠命令流、LATEST DATAGRAM 快照、独立低优先级可靠资源流。两个方向同时发 RPC/PUSH、按 60 Hz 时间表提交快照，并以应用 4 KiB 块传输各 32 KiB 资源；网络故障模拟器施加可控丢失、乱序、重复和 RTT。Tick/快照过期属于示例应用 schema，不添加线路时钟字段。资源例程显式采用 30 秒操作期限和 15 秒绝对接收期限，快照采用 50 ms 发送排队期限和应用 Tick 去旧；这些不是核心默认值。

```sh
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp_xrudp_tests --cflag=-O2
```

预期：0/1/5/10% 丢包与 20/80/200 ms RTT 的 12 种组合完成可靠消息和资源，快照在资源传输期间仍到达，重复快照不复活；超限通知只在本地拒绝。独立 PMTU 用例先确认 1200，再把模拟路径降至 512，验证当前发送上限降为 424、较大 NOTIFY 被本地拒绝、可靠资源和小快照继续到达。该 PMTU 用例采用注入时钟，不作为真实延迟测量。所有报文仍通过实际 UDP socket，结束时释放 session 与 socket。

这里验证协议组合和有限故障场景，不赋予移动设备、互联网长稳或媒体播放资格；FEC、抖动缓冲、应用时钟同步和编解码仍由媒体/游戏层负责。原生 xrudp 的生产验收缺口继续以其验收矩阵为准。
