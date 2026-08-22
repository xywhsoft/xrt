# WebSocket 核心设计

## 目标

WebSocket 核心在不依赖 HTTP 客户端/服务器框架的前提下，优先冻结握手、帧、消息、
压缩和关闭协议。连接、背压、异步和分组实现当前保留，但仍须在下一阶段证明它们属于
不可替代的通用快速路径；高级客户端、服务器与路由属于 `xws`。

## 层次

```text
frame / mask / close
        |
message state machine
        |
optional permessage-deflate
        |
established connection + writer + Future + group
        |
caller-provided TCP or TLS stream
```

握手工具位于旁路：它读取 HTTP/1 parser 的字段并产生校验结果，成功后把传输所有权
交给 established connection。

## 核心不变量

- 客户端发送必须 mask，服务端发送禁止 mask；
- 控制帧必须 FIN 且 payload 不超过 125；
- continuation 必须与活动消息匹配；
- 文本消息跨帧保持增量 UTF-8 状态；
- RSV 位只能由成功协商的扩展占用；
- Close 后不再开始数据消息；
- 协议错误不能继续把后续字节交给应用；
- 已受理引用在所有成功、失败和取消路径恰好释放一次。

## 接收

网络流发布字节片段，帧 parser 消费 Header，消息层消费 payload。任何一层都不要求
完整消息驻留内存。

应用回调变慢时暂停网络读取。恢复必须回到所属 worker，不能从任意线程并发推进同一
parser。累计消息长度、压缩后输入和解压后输出分别受限。

## 发送

短帧复制发送；大消息使用 ref/take/buffer；writer 负责分片和压缩状态。所有路径最终
进入网络流的有界队列，因此共享同一 high/low watermark、最大排队字节和 drain。

不能为了广播或 Future 再建立无界中间队列。组广播逐连接执行受理，失败结果必须能
区分 AGAIN、关闭、取消和真实 I/O 错误。

## 压缩

协商器严格验证 RFC 7692 参数。运行时按方向保存独立窗口和 context takeover 状态，
在消息边界处理同步刷新尾和字典复位。

压缩输出可直接进入 writer，解压输出可直接进入消息回调。两者都不聚合完整消息。

## TLS

WebSocket 连接只接管已完成握手的 `xtlsstream`。证书、SNI、ALPN、恢复和代理属于
通用 TLS/网络层。TLS 和普通 TCP 必须具有一致的发送所有权、背压和关闭语义。

## HTTP Upgrade

XRT 核心公开握手计算和字段验证，不公开绑定某个 HTTP server/client 对象的适配器。

上层流程固定为：

1. HTTP/1 parser 完成 Header；
2. 验证方法、版本、Upgrade、Connection、Key/Accept、版本、协议和扩展；
3. 写出 101 或验证 101；
4. 停止 HTTP body reader；
5. 把网络流及未消费字节交给 WebSocket connection。

`xws` 可以把这五步包装成客户端/服务器便利入口，但不得隐藏未消费字节、错误原因或
传输所有权。

## Future 与取消

Future 包装 established connection 操作，不创建隐藏引擎。取消等待和取消底层操作
必须区分；取消发送前若数据已受理，连接仍负责最终释放。

协程只使用通用 Future/Wait 桥，不在 WebSocket 中实现独立调度器。

## 关闭

正常关闭遵循 Close 握手、截止时间、半关闭和传输终态。协议错误可发送合适 Close code
后 abort；底层已经不可写时直接 abort。任何关闭路径都必须停止新发送、解除读取暂停、
完成等待者并释放队列。

## 扩展边界

`extlibs/xws/archive` 保存迁出的客户端、服务器、HTTP Future、路由和代理适配资产。
后续 `xws` 重建时必须只依赖公开 HTTP/1、网络、TLS 和 WebSocket 核心，不能复制帧、
握手、压缩或错误处理实现。

## 发布门槛

- 帧、消息、Close、UTF-8、mask 和 64 位长度边界；
- 任意分片与控制帧穿插；
- permessage-deflate 参数矩阵、上下文复用和膨胀限制；
- copy/ref/take/writer 的所有权和背压；
- TCP/TLS、Future、协程、连接组和关闭竞态；
- 协议 fuzz、固定互操作向量、单头文件和裁剪构建。
