# SSH TCP Transport Random

`ssh_transport_tcp_random` 是 `ssh_transport_tcp` 的生产便利层。它只提供
`xrtSshTransportTcpWritePrepare`，把 XRT 系统安全随机源作为 packet padding 回调传给核心的
`xrtSshTransportTcpWritePrepareWithPadding`。

核心 TCP 适配不依赖 `random_secure`。已经持有安全播种会话 PRNG 的高吞吐服务可以只选择
`ssh_transport_tcp` 并注入自己的 padding 回调；普通程序选择本模块即可获得安全默认值。
