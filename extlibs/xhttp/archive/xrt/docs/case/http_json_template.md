# HTTP、JSON 与 Template 组合

这个案例展示一种可选的应用层组织方式：同一份 `xvalue` 可以同时生成 JSON API
响应和 HTML 页面响应。它不是 HTTP Server 的强制模型；固定 JSON、文本或完整
HTTP/1 报文仍可直接通过 `xrtHttpConnReply`、`xrtHttpConnRespondRaw` 发送。

## 分层

- HTTP Server 只负责请求、唯一响应门、背压和连接生命周期。
- Value 是这个案例选择的共享业务模型，不由 HTTP 层创建或解释。
- JSON 和 Template 是两个彼此独立的输出器。
- Reply 是可选构建器，只在需要状态、字段和通用正文组合时使用。

Template 不自动执行 HTML 上下文转义。该示例只渲染受信任的固定服务字段；来自 URL、
Header、表单、数据库或其他用户输入的值必须先按它所在的 HTML 文本或属性上下文
转义，再进入模板。JSON 输出继续使用 JSON Writer，不能复用 HTML 转义结果。

因此启用 HTTP Server 不会引入 Value、JSON 或 Template；只选择这个组合范例套件
时，构建清单才会同时启用这些模块。

## 使用方式

完整可运行代码见 `examples/http/reply_json_template/main.c`。它先创建一份包含标题、
服务名和请求数的 Value，然后分别调用：

```c
sJson = xrtJsonStringify(model, false, &jsonSize);
sHtml = xrtTemplateRender(pageTemplate, model, NULL);
```

两个结果分别写入 `application/json` 和 `text/html` Reply。Reply 会复制短正文，
因此临时 JSON 与 HTML 字符串可以在构建后立即释放。真实服务器的 Request 回调
可把生成结果直接交给 `xrtHttpConnReply`，不需要为了这个模式先创建 Reply。

## 服务运行时组合

完整服务按依赖顺序启动：先读取有硬上限的配置并建立不可变运行配置，再编译模板、
创建 Logger 与 TaskPool，最后启动 Network Engine 和 HTTP Server。配置缺失、类型
错误、数值越界和模板编译错误都应在监听端口前失败；不能把超大 `queue_capacity`
直接窄化为整数，也不能把名称、地址或路径静默截进固定数组。

请求提交后台工作时必须先确定响应语义：

- 返回 `202 Accepted` 表示有界执行器已经接管工作，不表示工作成功；任务标识必须在
  发布任务指针前复制到局部变量，发布成功后不能再读取可能已被 worker 释放的对象。
- 如果客户端需要最终结果，直接让 TaskPool、Promise 或协程产生普通 Future，并用
  `xrtHttpConnRespondFuture()` 绑定唯一响应门。
- 一旦工作已经受理，后续构建复杂 `202` 响应发生 OOM 不能改口为“未受理”。常见
  路径应在发布前准备响应，或者在发布后使用无需对象树的固定 JSON/Raw 兜底。
- 快照、外部进程或日志失败必须进入任务 Future 或结构化诊断；不能忽略失败后仍把
  任务记录为完成。

运行状态先在短锁内复制或增加不可变引用，JSON、Template、日志和文件 I/O 全部在
锁外执行。模板在启动时编译并跨请求共享；任何来自 URL、Header、表单、日志或外部
数据的文本都要先按实际 HTML 文本或属性上下文转义。快照使用原子文件入口，日志使用
有记录上限、滚动、错误和可选异步背压的 Logger Sink，不能用未经截断检查的栈缓冲
长度调用普通文件追加。

关闭顺序与启动相反但不直接销毁依赖：先停止接纳请求并 Drain HTTP Server，再关闭
后台输入，等待 TaskGroup/TaskPool 与已受理 Future 收口，Flush Logger，最后释放
Server、Engine、模板、Logger 和配置。等待使用真实 Future、Channel 关闭或 Join，
不能用 Sleep 猜测工作已经结束。启动中途失败也执行同一部分初始化清理，并返回失败
退出码。

相关可运行组合见 `examples/http/server_future`、`examples/concurrency/worker`、
`examples/file/report`、`examples/logging/async` 和本案例示例。它们共享同一套 Future、
错误、所有权和裁剪契约，不建立专用“控制台服务运行时”。

## 选择标准

当 API 与页面确实共享同一业务投影时，共享 Value 可以避免字段和计算逻辑漂移。
当响应已经是固定字节、手写高效 JSON、代理上游数据或自定义流时，直接发送更简单，
不应为了形式统一额外构建对象树。

相关契约见 [HTTP Server](../api/http_server.md)、[Value](../api/value.md)、
[JSON](../api/json.md) 和 [Template](../api/template.md)。
