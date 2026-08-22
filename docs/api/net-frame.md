# 网络 Framing API

## 分层与裁剪

- `XRT_FEATURE_NET_FRAME`：统一帧描述、payload 复制和精确消费，只依赖 `NET_BUFFER`。
- `XRT_FEATURE_NET_FRAME_LINE`：任意长度分隔符的增量行 framing，只依赖 `NET_FRAME`。
- `XRT_FEATURE_NET_FRAME_LENGTH`：1 到 8 字节长度字段 framing，只依赖 `NET_FRAME`。

HTTP/1 和 WebSocket 有各自的协议状态机，不经过通用 framing vtable。这里用于 TCP 上的文本协议、长度前缀 RPC、自定义消息协议和协议探测层。

## 通用帧

```c
typedef enum xnetframestatus {
	XNET_FRAME_ERROR = -1,
	XNET_FRAME_MORE = 0,
	XNET_FRAME_READY = 1
} xnetframestatus;

typedef struct xnetframe {
	size_t PayloadOffset;
	size_t PayloadSize;
	size_t FrameSize;
	uint64 Declared;
} xnetframe;
```

帧只借用当前 `xnetbuf`。`PayloadOffset` 和 `FrameSize` 都相对缓冲头部；输入前缀被消费、替换或重排后，调用方不得继续使用旧帧。`xrtNetFrameCopy` 最多复制输出容量个 payload 字节，适合小消息便利路径；零拷贝用户可直接使用 `xrtNetBufSpans`、`xrtNetBufPeek` 和偏移。`xrtNetFrameConsume` 会验证当前输入仍能完整容纳该范围，再精确消费 `FrameSize` 字节。

## 行分隔帧

```c
typedef struct xnetlineconfig {
	xbytesview Delimiter;
	size_t MaxPayload;
	bool IncludeDelimiter;
} xnetlineconfig;
```

`xrtNetLineConfigInit` 默认使用 LF、8192 字节 payload 上限并从 payload 去除分隔符。Delimiter 是借用视图，没有固定长度上限，必须在 Framer 使用期间保持存活且内容不变。`MaxPayload` 只限制分隔符之前的字节；需要无界模式时显式设为 `SIZE_MAX`。

`xrtNetLineNext` 返回 `MORE` 后，调用方只能保留原输入前缀并在同一个 `xnetbuf` 尾部追加；消费、替换、换用另一缓冲或重排输入前应调用 `xrtNetLineReset`。Framer 保存块指针、块内偏移和未决候选位置，因此即使逐次追加大量单字节引用块，也不会为定位旧偏移反复遍历链头。分隔符跨缓冲块、自重叠和位于 payload 上限的情况都受支持；分隔符开始位置超过上限立即返回 `XNET_FRAME_ERROR` 和 `XNET_ERROR_FRAME_LIMIT`。

完整示例位于 `examples/network/frame_line/main.c`。

## 长度前缀帧

```c
typedef struct xnetlengthconfig {
	size_t LengthOffset;
	size_t LengthSize;
	int64 Adjustment;
	size_t Strip;
	size_t MaxFrame;
	xnetframeorder Order;
} xnetlengthconfig;
```

帧总长按以下公式计算：

```text
FrameSize = LengthOffset + LengthSize + Declared + Adjustment
```

`LengthSize` 支持 1 到 8 字节，`Order` 明确选择大小端。`Strip` 指定 payload 起点，可以保留完整头部、只去除长度字段，或去除应用头。若协议长度字段已经包含头部，可使用负 `Adjustment`。所有字段末端、无符号声明值、有符号调整、`size_t` 转换、strip 和 `MaxFrame` 都在访问 payload 前验证。

默认配置是四字节大端 payload 长度、去除四字节字段和 1 MiB 总帧上限。完整示例位于 `examples/network/frame_length/main.c`。

## 错误

- `XNET_ERROR_FRAME_CONFIG`：配置本身不可能形成合法帧。
- `XNET_ERROR_FRAME_STATE`：Framer 或帧范围已失效。
- `XNET_ERROR_FRAME_LIMIT`：完整帧或行 payload 超过硬上限。
- `XNET_ERROR_FRAME_LENGTH`：声明长度、调整或本机长度转换溢出。

`MORE` 是正常增量控制结果，不设置错误。`ERROR` 保留结构化错误；调用方通常应关闭当前协议流或按协议定义丢弃输入后重置 Framer。
