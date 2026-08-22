# HTTP/1 Exchange

`http_exchange` 是 HTTP/1 客户端与具体传输之间的公开协议状态机。它直接依赖
请求准备、响应结果和 HTTP/1 Body 层，不创建 Socket，也不通过函数表伪装网络
依赖。TCP、TLS、内存测试传输和语言运行时都可以驱动同一份协议合同。

## 生命周期

`xrtHttp1ExchangeCreate()` 成功时接管不可变 `xhttp1requestplan`，失败时不接管。
Exchange 销毁时释放计划、正文 Reader、未取走响应和错误。最终响应可以通过
`xrtHttp1ExchangeTakeResponse()` 转移给调用方。

默认配置继承公网 Header 限额和旧版已经验证的 64 MiB 正文上限，并允许调用方
分别调整 Header、chunk、trailer、正文和信息响应数量。对象不预留固定 8 KiB
缓冲；只有跨输入边界的 Header 或分帧元数据才按实际大小增长。
配置初始化、创建时的配置和事件表都支持合法的未对齐存储。创建会在接管请求计划
之前取得对齐快照；结构范围无效或末地址回绕时同步失败，计划所有权仍属于调用方。
`AllowRawTransferCodings` 默认为 `false`：除最终 `chunked` 外的传输编码会以
`XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING` 失败。代理或自定义解码器可以显式开启
该项，Exchange 仍移除最终 chunked 分帧，但把其余编码正文原样交付。

## 出站

```c
xbytesview data;

while ( xrtHttp1ExchangeOutput(exchange, 16 * 1024, &data) ==
	XHTTP1_OUTPUT_DATA ) {
	/* transport_send 可以短写。 */
	xrtHttp1ExchangeOutputConsume(exchange, written);
}
```

`Output` 借出完整请求 Header、定长正文或 chunked 元数据与正文片段。
`OutputConsume` 明确推进短写，不复制正文租约。chunked last-chunk 包括可选
Trailer，直接借用请求计划冻结的紧凑字节，不受源请求后续修改影响，也不受
Exchange 临时 chunk 行缓冲大小限制。`XHTTP1_OUTPUT_AGAIN` 表示异步正文源暂
不可读；`XHTTP1_OUTPUT_CONTINUE` 表示正在等待 `100 Continue` 或上层超时策略
调用 `xrtHttp1ExchangeContinue()`。

已知长度正文必须精确结束，未知长度正文使用 chunked。最终响应可以在
`100 Continue` 前停止正文，Reader 不会被无意义地打开。

已经借出的出站数据不会因并行到达的最终响应而失效。传输层仍须对该次发送
调用 `xrtHttp1ExchangeOutputConsume()`；确认后 Exchange 才回收正文租约，
并且不会继续产生新的请求输出。

启用 `http_exchange_async` 后，`Output` 返回 `XHTTP1_OUTPUT_AGAIN` 时可以调用
`xrtHttp1ExchangeOutputWait()` 取得正文源的可读 Future。调用方拥有返回的
Future 引用；等待完成并释放 Future 后，再次调用 `Output`。没有待处理的
`AGAIN`、Exchange 已失败或正文源不支持异步等待时，函数返回 `NULL` 并设置
错误。正文源未能创建 Future 时，其错误会成为 Exchange 终态错误的原因，
供上层错误映射和 C 调用方完整追踪。已经返回的 Future 若以失败或取消终态
完成，则由持有它的传输事务终止 Exchange，不会被 Exchange 暗中保留或回调。

## 入站

`xrtHttp1ExchangeFeed()` 接受任意分片，严格处理：

- 多条有界 `1xx` 与 `100 Continue`；
- `HEAD`、`CONNECT 2xx`、`101`、无正文、定长、chunked 和关闭定界；
- Header、chunk 行、正文和 trailer 的独立硬限额；
- `Transfer-Encoding` 与 `Content-Length` 冲突；
- 默认拒绝未实现的非 `chunked` Transfer Coding；
- 1xx、204 和 CONNECT 2xx 中禁止的分帧字段；
- 禁止出现在 trailer 中的字段和可靠 EOF 截断。

没有 Body 回调时，响应按需缓冲正文；提供 Body 回调时直接流式交付，不建立
响应正文副本。Header、reason、trailer 和最终 URL 始终由响应对象拥有。

`Accepted` 只覆盖已经处理或为跨边界解析而内部保留的输入前缀。升级或普通响应
结束后，未接受的外部后缀仍属于调用方；因跨边界而已经接受的后缀可通过
`xrtHttp1ExchangeRemainder()` 读取。

## 连接复用

`xrtHttp1ExchangeReusable()` 同时检查请求是否完整发送、响应是否完整结束、
双方关闭语义、关闭定界、升级、EOF 和内部后缀。传输适配层还必须确认本次
`Feed` 没有留下未提交的外部输入，才能把连接放回池中。

