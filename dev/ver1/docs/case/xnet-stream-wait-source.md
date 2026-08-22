# XRT 范例解析：xnet-v2 Stream Wait-Source 实战

> 用统一 wait-source 串起 stream readable / writable / drain / close，以及线程等待、协程等待与 future 主线。

[返回案例索引](README.md)

---

## 目录

- [场景目标](#场景目标)
- [为什么要用统一 wait-source](#为什么要用统一-wait-source)
- [推荐理解顺序](#推荐理解顺序)
- [最小线程等待示例](#最小线程等待示例)
- [最小协程等待示例](#最小协程等待示例)
- [典型使用场景](#典型使用场景)
- [常见错误](#常见错误)
- [相关文档](#相关文档)

---

## 场景目标

当前 `xnet-v2` 主线里，stream 不只是“能收发数据”的对象，它还可以被统一看作一组等待源：

- readable
- writable
- drain
- close

这条案例要说明的是：

- 为什么 XRT 不推荐你为每一种等待面单独记忆一套零散 helper
- 为什么应该优先理解成 `stream -> wait-source -> future -> thread/co wait`
- 在什么场景下应该等 `readable`
- 在什么场景下应该等 `drain`
- 如何把它自然地接进协程和正式异步主线

---

## 为什么要用统一 wait-source

如果只把 stream 当成“socket 包装”，代码很容易分裂成三套模型：

- 线程里自己等条件
- 协程里自己轮询
- 网络层再维护一套回调状态

当前主线更推荐统一成：

- `xrtNetWaitSourceStream(...)`
- `xrtNetWaitSourceWait*()`
- `xrtNetWaitSourceWaitCo*()`

这样做的好处是：

- 线程等待和协程等待合同一致
- `future / task / promise` 主线可以直接接进来
- `stream`、`dgram`、`future` 的等待方式会越来越统一
- 后续扩展 `socket/port` 级等待源时，不需要再发明第四套接口

---

## 推荐理解顺序

建议先把 stream 的等待面理解成 4 类：

1. `READABLE`
2. `WRITABLE`
3. `DRAIN`
4. `CLOSE`

再记住 3 组正式入口：

1. future 形式  
	`xrtNetStreamFutureEx(...)`
2. 线程等待形式  
	`xrtNetStreamWaitEx(...)`
3. 协程等待形式  
	`xrtNetStreamWaitCoEx(...)`

如果只是想写最少代码，可以直接使用已经封装好的 helper：

- `xrtNetStreamWaitReadableCo*`
- `xrtNetStreamWaitWritableCo*`
- `xrtNetStreamWaitDrainCo*`
- `xrtNetStreamWaitCloseCo*`

但长期理解时，还是应当优先把它们视为统一 `wait-source` 的薄封装。

---

## 最小线程等待示例

下面这个例子演示“等待一个 stream 进入 readable 状态”：

```c
xnetwaitsrc tSrc;

tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_READABLE);

if ( xrtNetWaitSourceWaitTimeout(&tSrc, 5000) ) {
	/* 现在可以继续从 stream 取数据或等待后续 recv 链路处理 */
} else {
	/* timeout / close / cancel 路径按当前状态继续处理 */
}
```

如果你已经明确知道自己就是要等 `readable`，也可以直接写成：

```c
if ( xrtNetStreamWaitTimeoutEx(pStream, XNET_STREAM_WAIT_READABLE, 5000) ) {
	/* readable */
}
```

推荐理解：

- generic `wait-source` 入口更适合组合
- 专名 helper 更适合写快速业务代码

---

## 最小协程等待示例

协程里最自然的写法是：

```c
xnetwaitsrc tSrc;

tSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_DRAIN);

if ( xrtNetWaitSourceWaitCoUntil(&tSrc, iDeadlineMs) ) {
	/* 发送队列已经 drain */
} else {
	/* deadline 到期或等待失败 */
}
```

如果你只想表达“等这个 stream 发完并排空”，也可以直接写：

```c
if ( xrtNetStreamWaitDrainCoUntil(pStream, iDeadlineMs) ) {
	/* drain 完成 */
}
```

这里和线程等待最大的区别不是接口形状，而是：

- 协程会挂起当前执行流
- 恢复后仍然沿同一条 future / wait-source 主线继续

---

## 典型使用场景

### 1. `READABLE`

适合：

- 你已经暂停自动读取
- 你想自己控制何时恢复消费
- 你想把网络对象挂进统一等待逻辑

### 2. `WRITABLE`

适合：

- 发送端需要等到低水位以下再继续写
- 你想避免持续忙等“现在能不能继续发”

### 3. `DRAIN`

适合：

- 你要等发送队列真正清空
- 你准备做优雅关闭
- 你需要把“写完成”作为上层逻辑的一个明确节点

### 4. `CLOSE`

适合：

- 服务端等待连接彻底关闭
- 客户端等待对端完成关闭
- 你要把 close 事件纳入统一异步编排

---

## 常见错误

### 1. 把 wait-source 当成长期所有权对象

`xnetwaitsrc` 更适合被看成一个轻量等待描述，而不是长期保存的资源句柄。

如果你需要长期异步结果，应该继续往上收口到：

- future
- task
- task group

### 2. 一边使用自动回调消费，一边再注册同类同步 waiter

尤其是 `readable / recv` 这类场景，应先明确：

- 你是要走回调驱动
- 还是要走统一等待源

不要两套消费模型混用。

### 3. 把 `shared root` 的锁语义和 `wait-source` 混在一起

`wait-source` 解决的是“何时可继续推进”，不是“如何为共享数据结构提供稳定视图”。

如果你接下来要操作 shared root：

- 仍然要按 root contract 使用 `Lock / Unlock`

### 4. 在协程里继续手写轮询

如果你已经使用当前主线，就不应再写：

- `while ( !ready ) { xrtCoYield(); }`

而应优先迁移到：

- `xrtNetWaitSourceWaitCo*`
- `xrtNetStreamWait*Co*`

---

## 相关文档

- [XNet V2](../api/api-xnet-v2.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [Coroutine](../api/api-coroutine.md)
- [xnet-v2 Stream Wait-Source 入门](../guide/xnet-stream-wait-source-intro.md)
- [线程、协程与 Future 协同模型](thread-coroutine-future.md)

---

**当前定位：这是网络等待主线的正式案例页，后续可继续扩写具体的 readable / writable / drain / close 组合链路。**
