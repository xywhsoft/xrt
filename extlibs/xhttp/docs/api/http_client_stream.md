# HTTP/1 Stream 调用

`http_client_stream` 把已经打开的 TCP 或 TLS Stream 与一个
`xhttp1exchange` 组合为单次 HTTP/1 调用。它不负责 DNS、拨号、重定向、
连接池或 Cookie 策略，因此可以直接用于框架自己的高性能连接管理器。

## 能力边界

- `xrtHttp1CallTcp` 驱动明文 HTTP/1 调用。
- `xrtHttp1CallTls` 在启用 `http_client_tls` 时驱动 HTTPS 调用。
- 请求 Header 和正文直接从 Exchange 借出并发送，不创建整包副本。
- `WriteSize` 只限制单次借出长度，不预分配固定缓冲。
- 响应输入直接交给 Exchange，支持固定长度、chunked、close-delimited、
  `1xx` 和 Upgrade。
- 响应正文可在回调中暂停，并从任意线程提交恢复。
- 请求正文可通过 `http_client_stream_async` 等待 Future 背压。

## 创建调用

```c
xhttp1callconfig config;
xhttp1callevents events;

xrtHttp1CallConfigInit(&config);
xrtHttp1CallEventsInit(&events);
config.WriteSize = 32 * 1024;
events.Done = on_done;
events.Progress = on_progress;
events.Data = context;

xhttp1call* call = xrtHttp1CallTcp(
	stream,
	exchange,
	&config,
	&events
);
```

创建函数必须在 Stream 所属 Worker 上调用。成功时接管 Stream 的调用方引用
和 Exchange；失败时两个输入仍归调用方。成功返回前不会执行完成回调。

`xrtHttp1CallTls` 要求 TLS 握手已经成功完成，其余所有权和回调规则与 TCP
入口一致。

## 完成结果

完成回调至多执行一次，并且始终位于传输所属 Worker：

- `Result == XRT_NET_OK` 时 `Response` 非空。
- `Reusable` 表示连接可进入 HTTP/1 连接池，传输引用转移给回调。
- `Upgraded` 表示协议已经升级，传输引用和 `Buffered` 余量转移给回调。
- `Reusable` 与 `Upgraded` 互斥。
- 失败或取消时传输由调用层异常关闭，不转移给回调。
- `Error` 只在回调期间借用；需要长期保存时调用 `xrtErrorRef`。

回调外需要保存调用对象时，使用 `xrtHttp1CallRef` 与
`xrtHttp1CallDestroy` 管理独立引用。

## 取消

`xrtHttp1CallCancel` 可从任意线程调用。返回 `true` 表示取消请求已经线性化，
最终状态必为 `XHTTP1_CALL_CANCELLED`；返回 `false` 表示已有取消请求或终态
已经提交。

取消不会在调用线程同步执行用户完成回调。最终完成仍回到传输 Worker，并且
尚未移交的传输会被异常关闭。

## 暂停与恢复

响应正文回调可在传输 Worker 上调用 `xrtHttp1CallPause`。暂停会同时关闭
Exchange 输入门和底层读取门，避免继续积累未消费数据。

`xrtHttp1CallResume` 可从任意线程调用。多个恢复请求会合并为一个无分配
Worker Post；恢复过程先消费已有缓冲，再重新开放网络读取。

`xrtHttp1CallPaused` 返回并发快照。调用进入终态后始终返回 `false`。

## 异步请求正文

启用 `http_client_stream_async` 后，正文 Reader 返回 `XHTTP_BODY_AGAIN` 时，
调用层通过 `xrtHttp1ExchangeOutputWait` 获取 Future。Future 完成后使用调用
对象内嵌的 `xfuturewatch` 和 `xnetpost` 无分配地回到传输 Worker。

Future 失败或取消会成为 `XHTTP1_CALL_ERROR_EXCHANGE` 的 Cause。调用取消或
提前最终响应会摘除正文等待并停止正文来源，不允许迟到通知再次推进调用。

## 错误

创建阶段错误写入当前线程错误槽。运行阶段错误保存在调用对象中，并通过完成
结果和 `xrtHttp1CallError` 暴露。

错误域为 `xrt.http.call`，代码为 `xhttp1callerror`：

- `XHTTP1_CALL_ERROR_ARGUMENT`
- `XHTTP1_CALL_ERROR_STATE`
- `XHTTP1_CALL_ERROR_TRANSPORT`
- `XHTTP1_CALL_ERROR_EXCHANGE`
- `XHTTP1_CALL_ERROR_CANCELLED`

传输、Exchange 和 Future 的原始错误保留为 Cause。
