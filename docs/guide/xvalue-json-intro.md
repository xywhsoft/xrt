# xvalue 与 JSON 入门

> 目标：理解 `xvalue` 为什么是 XRT 的核心动态数据模型，以及它和 JSON 之间应该如何配合使用。

[返回教学文档](README.md)

---

## 1. 为什么要先学 xvalue

在很多语言里，复杂数据处理都有一套默认的动态对象模型，例如：

- JavaScript 的 object / array
- Python 的 dict / list
- Lua 的 table

XRT 里的这层角色，就是 `xvalue`。

它的意义不只是“再做一个动态类型”，而是把下面这些东西统一起来：

- 配置数据
- JSON 数据
- 模板上下文
- HTTP 请求/响应里的结构化字段
- future / task / 网络主线周边需要承载的动态结果

因此，学会 `xvalue`，基本就抓住了 XRT 里“像脚本语言一样组织数据”的主线。


## 2. xvalue 在做什么

`xvalue` 不是一个单一类型，而是一组统一的数据对象入口。

当前主线里，你可以把它理解成：

- 标量值
	- null
	- bool
	- int
	- float
	- string
- 容器值
	- array
	- list
	- coll
	- table

它的重点不是“类型数量有多少”，而是：

- 同一套 API 组织不同层级数据
- 同一套模型和 JSON、模板、配置、网络交换数据对接
- 后续还能进入 shared root / 多线程安全模型


## 3. 最常见的用法

### 3.1 创建一个 table

```c
xvalue vUser = xvoTable();

xvoTableSetText( vUser, "name", "Alice" );
xvoTableSetInt( vUser, "age", 28 );
xvoTableSetBool( vUser, "active", TRUE );
```

这就很像在脚本语言里构造一个对象。

### 3.2 构造一个数组

```c
xvalue vTags = xvoArray();

xvoArrayAppendText( vTags, "c" );
xvoArrayAppendText( vTags, "network" );
xvoArrayAppendText( vTags, "runtime" );
```

### 3.3 组合成更复杂的对象

```c
xvalue vRoot = xvoTable();
xvalue vTags = xvoArray();

xvoArrayAppendText( vTags, "c" );
xvoArrayAppendText( vTags, "json" );

xvoTableSetText( vRoot, "project", "xrt" );
xvoTableSetValue( vRoot, "tags", vTags );
```

这时，`vRoot` 就已经是一棵完整的动态数据树。


## 4. xvalue 和 JSON 的关系

推荐理解方式是：

- `xvalue`：程序内部的数据模型
- `JSON`：数据交换格式
- `XSON`：当 `xvalue` 需要完整保留 `time / list / coll / class` 时使用的私有扩展交换格式

也就是说，程序内部应优先操作 `xvalue`，在需要对外交换、落盘、传输时，再做 JSON 编解码。

如果数据里包含标准 JSON 无法完整表达的类型，例如：

- `time`
- `list`
- `coll(set)`
- `class`

则应切换到 `XSON`，而不是继续强行使用 JSON。

这比“到处直接拼 JSON 字符串”更稳，也更适合后续演进。

### 4.1 从 JSON 解析到 xvalue

```c
str sJson = "{ \"name\": \"Alice\", \"age\": 28 }";
xvalue vData = xjsonParse( sJson );
```

### 4.2 把 xvalue 再输出成 JSON

```c
str sOut = xjsonStringify( vData );
printf( "%s\n", (char*)sOut );
```

这就是 XRT 里最推荐的“结构化数据主线”。


## 5. 为什么不建议直接把 JSON 当主数据模型

很多程序刚开始会直接：

- 读 JSON
- 改 JSON 字符串
- 手工拼 JSON

这样做的问题是：

- 数据结构不清晰
- 修改成本高
- 组合复杂对象很痛苦
- 模板、网络、配置之间无法共享同一个中间模型

XRT 更推荐：

1. 把 JSON 解析成 `xvalue`
2. 程序内部统一操作 `xvalue`
3. 最后再序列化成 JSON


## 6. 与模板、配置、网络的协同

### 模板

模板引擎最适合直接吃 `xvalue` 上下文，这样一个数据对象可以直接拿去渲染页面、配置文件或文本输出。

### 配置

配置系统如果建立在 `xvalue + JSON` 上，可以天然获得：

- 默认值注入
- 动态字段扩展
- 嵌套结构支持

### 网络

HTTP / WebSocket / LLM API 等场景，本质上都需要处理结构化数据。  
如果内部主模型统一成 `xvalue`，请求和响应的构造会清晰很多。


## 7. 当前主线下的线程安全边界

这里有一个很重要的点：

- 普通 `xvalue` 默认不是跨线程任意共享的自由对象
- 如果要跨线程共享，应该建立在当前主线的 shared root 语义上

也就是说：

- 单线程内部自由使用没问题
- 跨线程共享时，要按当前 shared-mode 合同来

这正是当前 XRT 在“像脚本一样好用”和“作为基础设施库仍然可控”之间做的平衡。


## 8. 推荐学习顺序

如果你刚接触这条线，建议按这个顺序学：

1. 学会创建 `table / array`
2. 学会 `SetText / SetInt / SetBool / SetValue`
3. 学会 `xjsonParse / xjsonStringify`
4. 再去看模板、配置、HTTP 这些更高层的用法

不要一开始就把 `xvalue` 当成“只是 JSON 附属品”。


## 9. 建议继续阅读

- [Value API](../api/api-value.md)
- [JSON API](../api/api-json.md)
- [Template API](../api/api-template.md)
- [用 xvalue + json 构造配置系统](../case/json-config-system.md)

---

**一句话总结：** 在 XRT 里，`xvalue` 是程序内部的统一动态数据模型，JSON 只是它最重要的交换格式之一；越早按这个思路组织数据，后面的模板、配置、网络与 AI 场景就会越自然。
