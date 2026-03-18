# 用 XRT 写一个最小 HTTP 服务

> 这个案例的目标不是展示所有 HTTP 特性，而是说明：在当前主线里，如何站在 `xhttpd + xvalue + json + runtime` 上，快速搭起一个最小但结构正确的服务。

[返回案例索引](README.md)

---

## 1. 场景

假设你要写一个最小的本地 HTTP 服务，用来：

- 输出健康检查
- 返回一段 JSON
- 作为以后接更多业务逻辑的起点

在 XRT 当前主线里，推荐从：

- runtime
- `xhttpd`
- `xvalue + json`

这条组合开始，而不是手工从 socket 和 HTTP 解析拼起。


## 2. 为什么推荐这样做

因为 XRT 的定位不是“只给你一堆零件”，而是：

> 给你一条从运行时到底层网络，再到应用层服务的完整主线。

如果你已经决定写 HTTP 服务，那么：

- socket 细节不应再成为入口
- HTTP 协议细节不应再成为入口
- 结构化响应也不应靠手工字符串拼接

更推荐的是：

- 服务入口走 `xhttpd`
- 数据组织走 `xvalue`
- 输出 JSON 时再统一序列化


## 3. 最小服务的结构

一个结构正确的最小服务，至少包含 4 件事：

1. 初始化 XRT runtime
2. 初始化 HTTP 服务
3. 注册一个简单路由
4. 在回调里构造 `xvalue` 响应并输出

最小代码骨架可以理解成下面这样：

```c
int main()
{
	xrtInit();

	/* 创建并启动 HTTP 服务 */
	/* 注册一个返回 JSON 的处理函数 */
	/* 进入运行状态 */

	xrtUnit();
	return 0;
}
```

这里最重要的不是“代码行数最少”，而是：

- 入口是清晰的
- 层次是正确的
- 后面容易自然扩展


## 4. 为什么响应建议先构造成 xvalue

很多最小 HTTP 示例喜欢直接：

- `printf`
- `snprintf`
- 手工拼 JSON 字符串

这样示例虽然短，但不利于后续扩展。

如果从一开始就把响应数据组织成 `xvalue`，会更自然地获得：

- 结构清晰
- 字段扩展容易
- 以后可复用到模板、日志、AI 响应、配置系统

示意写法：

```c
xvalue vRes = xvoTable();

xvoTableSetBool( vRes, "ok", TRUE );
xvoTableSetText( vRes, "service", "xrt-demo" );
xvoTableSetText( vRes, "status", "running" );
```

然后再统一序列化为 JSON 输出。


## 5. 为什么这比“字符串直接返回”更适合长期主线

因为最小服务往往会自然长成更复杂的服务：

- 返回更多字段
- 增加错误对象
- 增加分页和列表
- 增加配置和状态信息

如果你起点就是 `xvalue + json`，这些演进几乎不会改变主线结构。  
如果起点是手工拼字符串，后面通常要重写。


## 6. 建议的增长路径

这个最小 HTTP 服务后面最自然的扩展顺序通常是：

1. 增加多个路由
2. 增加统一错误输出
3. 增加配置加载
4. 接入模板渲染
5. 接入数据库或外部 HTTP client
6. 接入 future / coroutine 编排
7. 接入 LLM API

这也是 XRT “成体系而不是拼凑”的价值所在：

- 不是每走一步都换一套模型
- 而是在同一条主线上逐步叠加能力


## 7. 和更底层写法的关系

这并不是说底层网络能力不重要，而是：

- 当你的目标已经是 HTTP 服务时
- 更适合直接站在 `xhttpd` 这一层

底层 `xnet-v2`、TLS session、wait-source、future 等能力，应该在你需要更细控制时再往下看，而不是把每个服务都从最底层手工搭起。


## 8. 常见错误

### 错误一：最小服务就直接拼一堆 JSON 字符串

短期省事，长期会越来越乱。

### 错误二：已经写 HTTP 服务了，还坚持自己管底层解析和路由

这会让应用层和底层协议层混在一起。

### 错误三：最小服务没有 runtime 主线

当前 XRT 主线很多能力都依赖 runtime，因此服务程序仍然应从：

- `xrtInit()`
- `xrtUnit()`

开始和结束。


## 9. 建议继续阅读

- [XHTTPD API](../api/api-xhttpd.md)
- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [XRT 运行时与线程附加入门](../guide/runtime-thread-attach.md)
- [用 xvalue + json 构造配置系统](json-config-system.md)

---

**一句话总结：** 写最小 HTTP 服务时，当前 XRT 主线推荐你站在 `runtime + xhttpd + xvalue + json` 上开始，这样代码虽然不是最“短”的，但会是后续最容易自然扩展的一条主线。
