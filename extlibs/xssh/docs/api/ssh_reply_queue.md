# SSH Reply Queue API

SSH global request 和同一 channel 的 request 回复没有 request id，只能按发送顺序关联。
`ssh_reply_queue` 提供调用方存储支持的 token FIFO：每条连接配置一个 global queue，每个 channel
配置一个独立 queue。

Want-reply 请求可靠进入发送队列后调用 `xrtSshReplyQueuePush`；收到 SUCCESS 或 FAILURE 后调用
`xrtSshReplyQueuePop`。空队列收到回复返回协议错误。等待任务被取消时不能删除中间 token，否则
后续回复会错配；调用方应保留 token，并在出队时丢弃已取消任务的结果。

容量完全由调用方决定，没有库内固定请求数。`xrtSshReplyQueueRebind` 可以把环形队列按逻辑顺序
迁移到更大的不重叠存储，因此长流水 channel 可以动态扩容；迁移失败不改变原队列。

队列不拥有 future、协程、锁或内存。若多个线程同时访问，同步责任属于拥有该 SSH connection 的
调度层。示例见 `examples/reply_queue/main.c`。
