# HTTP 核心设计

## 目标

XRT HTTP 核心必须保证任何 HTTP/1.0、HTTP/1.1 报文都能由公开 API 正确解析、
验证、写出或扩展，同时不强迫调用方建立拥有型请求/响应对象。

性能路径优先：协议层只处理线路事实，网络层负责缓冲、所有权、背压、截止时间和
取消，TLS 负责加密与身份验证。高级策略属于 `xhttp`。

## 层次

```text
HTTP 字段/token/参数
        |
        +-- Host / target / Expect / TE / Connection / Trailer / Upgrade
        |
HTTP/1 Header parser + writer
        |
HTTP/1 body plan + streaming reader + chunk writer
        |
Content-Encoding plan + optional streaming decode
        |
caller-owned TCP/TLS/event loop/application
```

每一层都可单独使用。HTTP 不持有 socket，网络不理解 HTTP，压缩只在明确启用
`http_decode` 时进入依赖闭包。

## 输入所有权

parser 借用累计输入和调用方字段数组。Header 完成后，调用方可以：

1. 在输入缓冲保持稳定时直接处理借用字段；
2. 只复制业务需要长期保存的字段；
3. 把未消费区交给 body reader；
4. Upgrade 成功时把未消费区交给新协议。

核心不提供动态 Header 容器，因为拥有、索引和修改策略会把分配器、容量增长和
框架生命周期引入所有调用方。

## 增量模型

Header parser 接收从消息首字节开始的累计视图。这样可以在不保存内部指针的情况下
安全处理接收缓冲搬移，并能先返回所需字段数量。

body reader 接收未消费的新片段，只保存分帧状态和计数。它一次最多发布一个借用
正文片段，调用方在返回后决定立即消费、复制或转发。

完整消息 parser 只服务连续内存场景，不替代流式网络路径。

## 分帧不变量

- Transfer-Encoding 优先于 Content-Length；
- 请求中的 Transfer-Encoding 必须以 chunked 结束；
- 冲突 Content-Length 必须拒绝；
- HEAD、成功 CONNECT、1xx、204、304 使用响应专用规则；
- close-delimited 只由可靠 EOF 完成；
- 101 和成功 CONNECT 后的字节不再属于 HTTP；
- trailer 不能包含分帧、路由、认证或其他被标准禁止的字段。

这些检查集中在 HTTP/1 层，客户端与服务器扩展不能各自复制一份不同实现。

## 发送模型

Header writer 先测量后写入调用方缓冲，容量不足失败原子。典型 Header 适合栈或连接
小缓冲，一次生成比多次细粒度属性调用和字符串拼接更可控。

正文保持独立所有权：小正文与 Header 聚集复制，大正文引用发送，chunked 只生成
短前缀和结尾。网络队列的硬上限和 drain 是唯一背压来源，HTTP 不维护第二个无界
发送队列。

## 解码模型

Content-Encoding parser 只产生线路计划。`http_decode` 是可选组合器：

- identity 直接发布输入视图；
- gzip/deflate 逆序建立 Inflate 链；
- 未知编码默认拒绝，可显式选择完整原样回退；
- 每层和最终明文共享硬限额；
- reset 复用窗口，避免每响应重复分配。

解码器不修改 Header。调用方必须根据 `xhttpdecodemode` 决定是否移除或保存
Content-Encoding、Content-Length 和原始长度元数据。

## 错误

语法错误通过 HTTP/1 稳定错误码、偏移和行号报告，同时设置 XRT 结构化错误。压缩、
网络、TLS 和应用回调保留自己的错误域。

输入不足不是错误，字段容量不足也不是错误；二者必须与非法协议明确区分。

## 扩展点

- 新字段语义基于公开 token、参数和字段迭代器实现；
- 新内容编码先使用 Content-Encoding 游标，再接入外部解码器；
- 新传输只需提供输入片段和原始写出，不需要修改 HTTP parser；
- HTTP/2 和 HTTP/3 将使用独立 framing，不复用 HTTP/1 body reader；
- `xhttp` 可以建立拥有型对象、客户端池、服务器路由和策略，但必须保留直接访问
  当前核心的路径。

## 发布门槛

- parser/writer/body/message 的正常、边界、未对齐和 no-allocation 测试；
- CL/TE、chunk、trailer、Upgrade 和 request smuggling 变体；
- gzip/deflate 分片、嵌套、CRC、截断、未知编码和膨胀限额；
- WebSocket 握手与 permessage-deflate 组合回归；
- 单头文件和逐模块裁剪编译；
- fuzz 语料、属性测试和 sanitizers；
- 单头文件与依赖闭包体积门禁。
