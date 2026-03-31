# 用 XRT 写最小 HTTP 服务入门

> 目标：理解为什么在当前主线里，写 HTTP 服务应当从 `runtime + xhttpd + xvalue + json` 出发，而不是从 socket 和协议解析手工拼起。

[返回教学文档](README.md)

---

## 1. 从哪里开始

如果你的目标已经是：

- 写一个本地 HTTP 服务
- 暴露健康检查接口
- 返回 JSON
- 后续继续扩成更完整的服务

那么在当前 XRT 主线里，推荐起点不是：

- 手写 socket
- 自己解析 HTTP
- 手工拼 JSON 字符串

而是：

- `xrtInit()`
- `xhttpd`
- `xvalue`
- `json`


## 2. 为什么这条主线更合适

因为 XRT 的定位不是只给你底层零件，而是提供一条可以持续长大的基础设施主线。

如果一开始就站在：

- runtime
- HTTP 服务层
- 结构化数据模型

之上，那么后面增加：

- 多路由
- 配置
- 模板
- future / coroutine
- 外部 API 调用

都还能沿着同一条结构继续走。


## 3. 一个最小服务应该有哪些部分

一个结构正确的最小服务，通常至少有：

1. 初始化 XRT runtime
2. 创建并启动 HTTP 服务
3. 注册一个最简单的处理函数
4. 先分清哪些路径走同步 handler，哪些路径需要异步 handler
5. 在处理函数里构造结构化响应
6. 结束时清理运行时

可以先把它理解成下面这种骨架：

```c
int main()
{
	xrtInit();

	/* 创建服务 */
	/* 注册路由 */
	/* 启动运行 */

	xrtUnit();
	return 0;
}
```

重点不是“代码要最短”，而是：

- 入口明确
- 层次正确
- 以后容易扩展

当前正式案例页会把这条骨架具体展开成 4 条路径：

- `/health`：同步快路径
- `/inspect`：把 request 元信息组织成 JSON
- `/slow/future`：`OnRequestAsync` 返回 future，由 `xhttpd` 统一回包
- `/slow/manual`：worker 线程手工 `xrtHttpdConnRespond()`


## 4. 为什么响应要优先构造成 xvalue

很多最小示例会直接：

- `snprintf`
- 拼一段 JSON
- 返回字符串

短期可以工作，但后面一旦字段增长，就会越来越乱。

更推荐的方式是：

1. 先把响应数据组织成 `xvalue`
2. 再统一输出成 JSON

例如：

```c
xvalue vRes = xvoCreateTable();

xvoTableSetBool( vRes, "ok", 0, TRUE );
xvoTableSetText( vRes, "service", 0, "xrt-demo", 0, FALSE );
xvoTableSetText( vRes, "status", 0, "running", 0, FALSE );
```

这样后面加字段、加嵌套对象、加列表都很自然。


## 5. 为什么 xhttpd 比直接操作底层网络更适合作为入口

如果你已经确定要写的是 HTTP 服务，那么：

- 路由
- 请求/响应
- HTTP 头
- 状态码

这些都属于应用层问题。

这时继续从底层 stream/socket 直接开始，往往只会把：

- 协议层
- 服务层
- 业务层

混在一起。

当前主线更推荐：

- 底层网络能力作为基础
- HTTP 服务直接站在 `xhttpd` 上


## 6. 这条线如何往后长

从最小 HTTP 服务继续增长时，最自然的顺序通常是：

1. 增加更多路由
2. 增加统一错误输出
3. 加配置系统
4. 加模板渲染
5. 接未来的协程 / future 编排
6. 接外部 HTTP / LLM API

这正是当前主线“成体系”的价值：

- 不是每增长一步就换一套模型
- 而是一直沿着同一条技术路径扩展


## 7. 常见误区

### 误区一：最小服务就不需要 runtime

不对。

当前 XRT 主线很多能力都依赖 runtime，因此服务程序仍应明确使用：

- `xrtInit()`
- `xrtUnit()`

### 误区二：示例越短越好

不完全对。

如果“短”是通过绕开主线模型换来的，那么后面只会更难维护。

### 误区三：HTTP 服务和 JSON 输出是两件独立事情

不对。

在当前主线里，它们应该通过 `xvalue` 很自然地连在一起。


## 8. 建议继续阅读

- [XHTTPD API](../api/api-xhttpd.md)
- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [用 XRT 写一个最小 HTTP 服务](../case/minimal-http-service.md)

---

**一句话总结：** 在当前 XRT 主线里，最小 HTTP 服务的正确起点是 `runtime + xhttpd + xvalue + json`，这样虽然不是最“炫技”的底层写法，但会是后续最容易长成完整服务的一条主线。
