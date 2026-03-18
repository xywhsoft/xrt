# xnet-v2 Stream Wait-Source 入门

> 目标：理解为什么当前网络主线不再推荐“回调到处散落”的写法，而是把 stream 的可等待状态统一收进 wait-source / future / coroutine 体系。

[返回教学文档](README.md)

---

## 1. 什么是 stream wait-source

在当前 `xnet-v2` 主线里，stream 不只是“一个收发字节的连接对象”。

它还会暴露一组可等待状态，例如：

- readable
- writable
- drain
- close

这些状态之所以重要，是因为它们把“网络上发生了什么”统一变成了“程序可以等待什么”。


## 2. 为什么这很重要

如果没有统一等待模型，网络代码通常会变成：

- 某些地方靠回调
- 某些地方靠 flag
- 某些地方靠条件变量
- 某些地方靠轮询

这样一旦程序变复杂，后面会很难收口。

而 wait-source 的意义就是：

> 把不同来源的异步事件，都变成统一的等待对象。

在 XRT 当前主线里，这意味着：

- stream 状态可以进入 future
- stream 状态可以进入 coroutine wait
- stream 状态可以和 timeout / deadline / cancel 放在同一条语义链上


## 3. 当前 stream 主线有哪些等待面

当前主线最重要的等待面包括：

- readable
- writable
- drain
- close

这几类状态不是互相孤立的。

它们共同构成了“这个连接当前处于什么阶段、调用方下一步适合做什么”的统一描述。


## 4. 为什么不推荐继续只写回调

回调本身不是错，但当程序需要：

- 超时
- deadline
- cancel
- 多等待源组合
- 与协程编排配合

时，单纯回调会越来越难维护。

stream wait-source 的价值就是：

- 不否定回调
- 但给主线提供一套更统一的等待模型


## 5. 可以怎样理解这条链

初学时，推荐把它理解成下面这样：

- stream：网络连接对象
- wait-source：把“某种可等待状态”抽象成统一等待入口
- future：承接等待完成后的结果
- coroutine：把等待链写成顺序逻辑

也就是说：

1. 网络层产出等待事件
2. wait-source 统一这些事件
3. future / coroutine 负责把它们组织进更大流程


## 6. 为什么这更适合现代网络程序

一个现代网络程序通常不只是“能收发数据”。

它还需要：

- 超时控制
- 错误传播
- 取消
- 优雅关闭
- 和 HTTP / WebSocket / TLS / AI 调用链协同

这些能力如果没有统一等待模型，很容易散落到多个模块中。

而当前主线把它们往 wait-source 上收，正是为了避免这种混乱。


## 7. 对上层意味着什么

如果你站在 HTTP / WebSocket / LLM API 这一层来看，stream wait-source 带来的好处是：

- 更容易写“等待网络状态再继续”的逻辑
- 更容易接入协程
- 更容易和 future / task 主线统一
- 更容易把 timeout / deadline 做成真正一致的合同

换句话说，wait-source 不是底层小技巧，而是整个网络主线能否变成现代异步基础设施的关键。


## 8. 推荐学习顺序

建议按下面顺序理解：

1. 先知道 stream 有哪些等待状态
2. 再理解 wait-source 如何统一这些状态
3. 再去理解 future / coroutine 如何消费这些等待
4. 最后再回到 HTTP / WebSocket / TLS 场景中看完整链路


## 9. 常见误区

### 误区一：wait-source 只是 future 的别名

不对。

future 是结果对象，wait-source 是等待对象。  
wait-source 可以桥接成 future，但两者职责不同。

### 误区二：只要有 callback，就不需要 wait-source

不对。

当程序需要 timeout、deadline、cancel、组合等待、协程编排时，统一等待模型会比单纯回调稳定得多。

### 误区三：stream wait-source 只对底层网络有意义

不对。

它的真正价值在于给上层 HTTP / WebSocket / AI 场景提供统一异步基础。


## 10. 建议继续阅读

- [XNet V2 API](../api/api-xnet-v2.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
- [线程、协程与 Future 协同模型](../case/thread-coroutine-future.md)

---

**一句话总结：** stream wait-source 把“网络连接发生了什么”统一成“程序可以等待什么”，这是当前 XRT 网络主线能够与 future、协程、timeout、cancel 自然协同的关键。
