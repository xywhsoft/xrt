# HTTP 重试协议

`http_retry` 是不依赖网络对象、任务调度器或客户端运行时的纯协议与策略基础层。它负责解释 `Retry-After`、识别默认临时失败状态，并提供不会整数回绕的封顶指数退避。实际是否重放请求、何时等待以及是否加入随机抖动，由更高层客户端策略决定。

## Retry-After

```c
xhttpretryafter retry;
uint64 delay;

if ( xrtHttpRetryAfterParse(value, &retry) &&
	xrtHttpRetryAfterDelay(&retry, xrtNow(), &delay) ) {
	/* delay 使用微秒，可交给单调计时器等待。 */
}
```

- `xrtHttpRetryAfterParse` 严格接受完整非负十进制 `delay-seconds` 或 HTTP-date，两端 OWS 可以省略。
- 接收端兼容 IMF-fixdate、RFC 850 和 ANSI C asctime 三种 HTTP 日期；写出端始终生成 IMF-fixdate。
- 十进制值保留为 `uint64` 秒，不会提前截断；转换为微秒时若溢出则明确失败。
- HTTP-date 使用 Unix Epoch 微秒。过去的日期表示立即可重试，得到零延迟。
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

## 默认策略

`xrtHttpRetryStatusDefault` 默认识别 `408`、`421`、`425`、`429`、`500`、`502`、`503` 和 `504`。状态码只是服务端失败事实，不足以单独证明一次请求可以安全重放；客户端还必须检查方法幂等性、正文可重放性、发送阶段和调用者策略。

`xrtHttpRetryBackoff(base, maximum, retry, &delay)` 计算 `min(base * 2^retry, maximum)`。`retry` 从零开始表示第一次重试，单位由调用者统一决定。函数不内置随机数，避免纯协议层强制依赖 RNG；客户端可在结果区间上应用 full jitter。

## 客户端组合层

启用 `http_client_retry` 后，高层 Client 才会建立 Timer、重放请求和隐藏中间响应。
该层默认关闭，完整配置、幂等与正文重放边界、取消和超时语义见
[`http_client.md`](http_client.md#自动重试)。纯 `http_retry` 不依赖网络、随机数
或任务体系，也不会根据一个状态码自行执行请求。

## 裁剪与验证

- 启用 `XRT_MODULE_HTTP_RETRY` 只引入 HTTP 字段基础和时间文本解析，不引入网络、任务、随机数或动态内存。
- 模块化、单头和失败分配器测试覆盖数字及日期解析、字段三态、直接写出、OOM、未对齐描述符、回绕范围、溢出、过去日期、状态集合和极大重试序号。
- 当前模块只定义重试事实和基础策略；高层 Client 也默认不自动重试任何请求。


