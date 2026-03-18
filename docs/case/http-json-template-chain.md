# 一个完整的 HTTP + JSON + Template 服务链路

> 这个案例的重点不是堆更多功能点，而是说明：在当前 XRT 主线里，HTTP 服务、结构化数据和页面渲染怎样自然接在同一条链上。

[返回案例索引](README.md)

---

## 1. 这个案例在解决什么问题

很多程序最后都会同时面对三类需求：

- 对外提供 HTTP 接口
- 处理 JSON 请求和响应
- 渲染 HTML 页面

如果这三件事分别走三套模型，代码很快就会变成：

- HTTP 一套
- JSON 一套
- 页面渲染再一套

而 XRT 当前主线更希望你沿着同一条链来组织它们：

- `xhttpd`
- `xvalue`
- `json`
- `template`


## 2. 推荐的主线结构

可以把这条链理解成下面 4 层：

- 服务入口：`xhttpd`
- 数据模型：`xvalue`
- 交换格式：`json`
- 展示输出：`template`

这样一个服务程序就不会在不同层反复切换数据模型。


## 3. 为什么 xvalue 是中轴

这条链里最关键的一点是：

> 程序内部统一使用 `xvalue` 作为结构化数据模型。

这样做有 3 个直接好处：

- JSON 响应可以直接从 `xvalue` 序列化
- HTML 页面可以直接拿同一个 `xvalue` 做模板上下文
- 后续配置、日志、AI 响应、模板输出都能复用同一套对象树

这正是 XRT “成体系” 的核心价值之一。


## 4. 最小数据流可以怎样组织

一个典型的请求处理流程，可以按下面方式理解：

1. HTTP 请求进入服务
2. 服务层解析参数并构造 `xvalue`
3. 业务层继续在 `xvalue` 上加工
4. 如果返回 API，就输出 JSON
5. 如果返回页面，就把同一个 `xvalue` 交给 template

也就是说，JSON 和 HTML 不必从一开始就分裂成两套实现。


## 5. JSON 输出链路

如果一个路由返回 JSON，那么最自然的方式是：

```c
xvalue vRes = xvoTable();

xvoTableSetBool( vRes, "ok", TRUE );
xvoTableSetText( vRes, "service", "xrt-demo" );
xvoTableSetText( vRes, "message", "hello" );
```

然后统一序列化成 JSON 输出。

关键点是：

- 程序内部先组织结构
- 最后一步再做文本交换格式


## 6. HTML 输出链路

如果另一个路由要返回 HTML 页面，那么完全可以继续复用同一个 `xvalue` 思路：

```c
xvalue vPage = xvoTable();

xvoTableSetText( vPage, "title", "XRT Demo" );
xvoTableSetText( vPage, "name", "Alice" );
xvoTableSetText( vPage, "status", "running" );
```

然后把它交给模板引擎渲染。

这样 JSON API 和 HTML 页面共享的是：

- 同一套结构化数据主线

而不是两个完全不同的对象体系。


## 7. 为什么这比“接口一套、页面一套”更稳

如果接口层和页面层各自维护一套模型，会出现这些问题：

- 字段命名不统一
- 业务逻辑重复
- 页面和接口经常不同步
- 后续改数据结构时要改两遍

而统一到 `xvalue` 之后：

- JSON 输出和页面输出只是“同一份数据的不同出口”
- 这会让程序结构明显更稳定


## 8. 这条链如何继续扩展

一旦这条主线建立起来，后面很自然就能继续加：

- 配置系统
- 模板布局
- 用户会话
- 外部 HTTP / LLM API 调用
- future / coroutine 编排

尤其在 AI 场景下，这种统一数据主线会更有价值，因为：

- 模型响应
- 页面展示
- API 输出

往往最终都要围绕同一份结构化数据做转换。


## 9. 一个推荐的工程习惯

如果你准备长期维护一个 HTTP 服务，推荐遵守下面的思路：

- 路由层尽量轻
- 数据整理尽量集中在 `xvalue`
- JSON 和 template 都只做各自最擅长的事
- 不要在多层反复手工拼字符串

这样后面接新功能时，主线会清楚很多。


## 10. 常见错误

### 错误一：接口返回 JSON，页面返回字符串，两边完全分裂

这样短期能快一点，长期维护成本会越来越高。

### 错误二：模板只是字符串替换，数据模型还是散的

如果数据没有统一组织到 `xvalue`，模板层的收益会明显下降。

### 错误三：HTTP 服务只看作网络层问题

实际上服务层最关键的往往是：

- 数据模型
- 输出结构
- 主线一致性

不只是网络收发。


## 11. 建议继续阅读

- [XHTTPD API](../api/api-xhttpd.md)
- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [Template API](../api/api-template.md)
- [用 XRT 写一个最小 HTTP 服务](minimal-http-service.md)
- [用 Template 渲染一个 HTML 页面](template-render-html.md)

---

**一句话总结：** 在当前 XRT 主线里，HTTP、JSON 和 Template 最稳定的组合方式，不是三套模型并行，而是都围绕 `xvalue` 这一套统一数据主线展开。
