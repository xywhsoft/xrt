# HTTP TE

`<xrt/http_te.h>` 实现 RFC 9110 `TE` 请求字段的传输无关协议层。直接路径不分配内存，
也不绑定客户端、服务器或网络对象。

## 成员

`xhttptecoding` 借用完整 `Element`、大小写不敏感的 `Coding`，以及不包含最终 `q`
权重的原始 `Parameters`。`Quality` 使用 0 到 1000 的定点值，缺省为 1000；
`ParameterCount` 只统计传输编码参数。

`trailers` 是独立能力标记，表示客户端不会丢弃响应 Trailer。它不能携带参数或权重。
其他成员遵循 `transfer-coding [ weight ]`：参数必须有值，最终 `q` 只能出现一次、不能加
引号，并严格使用 `q=` 形式。参数可用 `xrtHttpParamNext` 继续迭代。

## 列表与重复字段

`xrtHttpTeNext` 迭代一个字段值，`xrtHttpTeFieldNext` 按线路顺序跨越全部重复 `TE`
字段。两种游标必须由初始化函数建立，并在发布第一项前完成输入全集校验；畸形后缀不会
留下半份结果。quoted-string 内的逗号不会被当作列表分隔符，HTTP `#list` 空成员被
忽略。

`xrtHttpTeValid` 与 `xrtHttpTeCount` 处理单字段值。`xrtHttpTeParse` 汇总字段数量、成员
数量、传输编码数量、字段是否存在和 `trailers` 能力。描述符、游标和输出均支持未对齐
存储，输入在迭代期间必须保持不变。

`xrtHttpTeQuality` 返回同一传输编码所有声明中的最高权重；字段缺失或没有匹配时返回零，
语法错误也返回零但会设置错误。`xrtHttpTeAcceptsTrailers` 使用 `ERROR`、`END`、`ITEM`
区分错误、不接受和接受。

## HTTP/1.1 组合约束

RFC 9110 要求发送 `TE` 的发起端同时在 `Connection` 中声明 `TE`，使中间节点知道该
字段逐跳生效。核心层不构建拥有型请求，因此调用方应使用 `xrtHttpTeParse` 验证 `TE`，
使用 `xrtHttpConnectionParse` 验证连接选项，再把两条字段一并交给 HTTP/1 写出函数。
代理或自定义协议代码仍可按自身策略组合这些公开原语。

## 范例

参见 `examples/http/te/main.c`。
