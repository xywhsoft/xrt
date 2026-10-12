# HTTP 重试协议

`http_retry` 是不依赖网络对象、任务调度器或客户端运行时的纯协议与策略基础层。它负责解释 `Retry-After`、识别默认临时失败状态，并提供不会整数回绕的封顶指数退避。实际是否重放请求、何时等待以及是否加入随机抖动，由更高层客户端策略决定。

## Retry-After

```c
xhttpretryafter retry;
uint64 delay;

if ( xrtHttpRetryAfterParse(value, &retry) &&
	xrtHttpRetryAfterDelay(&retry, xrtNow(), &delay) ) {
	/* delay 使用毫秒，可交给单调计时器等待。 */
}
```

- `xrtHttpRetryAfterParse` 严格接受完整非负十进制 `delay-seconds` 或 HTTP-date，两端 OWS 可以省略。
- 接收端兼容 IMF-fixdate、RFC 850 和 ANSI C asctime 三种 HTTP 日期；写出端始终生成 IMF-fixdate。
- 十进制值保留为 `uint64` 秒，不会提前截断；转换为毫秒时若溢出则明确失败。
- HTTP-date 使用 公元 UTC 毫秒。过去的日期表示立即可重试，得到零延迟。
- `xrtHttpRetryAfterFields` 要求字段唯一。缺失、有效、重复或非法分别通过 `XHTTP_NEXT_END`、`XHTTP_NEXT_ITEM`、`XHTTP_NEXT_ERROR` 表达，重复值不会被静默挑选。

## 直接写出

```c
size_t size;
char value[30];

if ( xrtHttpRetryAfterWrite(&retry, value, sizeof(value), &size) ) {
	/* value[0..size) 是不带零结尾的规范 Retry-After 值。 */
}

str owned = xrtHttpRetryAfterBuild(&retry, &size);
if ( owned != NULL ) {
	xrtFree(owned);
}
```

`xrtHttpRetryAfterWrite` 支持 `NULL, 0` 精确测长。短缓冲会返回所需长度并保持输出字节不变；相对秒数使用最短十进制形式，绝对日期统一写为 29 字节 IMF-fixdate。`xrtHttpRetryAfterBuild` 只提供常用的零结尾拥有型结果，OOM 或非法描述符不会发布长度。

所有固定描述符和输出参数都允许未对齐存储，但必须构成完整、不回绕且不与仍需读取的数据重叠的内存范围。

## 策略与网络执行

默认状态集合和封顶退避已迁入 xhttp-policy 的 `http_retry_policy`，见 [策略说明](../../../xhttp-policy/docs/api/http_transfer_policy.md)。
实际 Timer、请求重放、抖动与响应排空由 xhttp-client 的 `http_client_retry` 执行。纯 `http_retry` 只编解码 Retry-After，不依赖网络、随机数或任务体系。
