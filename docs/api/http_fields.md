# HTTP 字段与参数

`<xrt/http.h>` 提供 HTTP 线协议共享的零分配语法工具。所有结果借用输入，适合
HTTP/1、WebSocket 握手、代理协议和扩展库复用。

## 字段

`xhttpfield` 只有 `Name` 和 `Value` 两个借用视图。字段名称不带冒号，字段值不带
两端 OWS 或 CRLF。

`xrtHttpFieldParse` 解析一条不含 CRLF 的字段行；`xrtHttpFieldNext` 逐行扫描字段
块；`xrtHttpFieldBlockCount` 在不保存描述符时完成严格验证和计数。

`xrtHttpFieldWrite` 写一条字段，`xrtHttpFieldBlockWrite` 写字段数组和可选最终空行。
两者都支持空输出测量，容量不足时不会部分写出。

查询函数包括：

- `xrtHttpFieldNameEqual`：ASCII 不区分大小写字段名比较；
- `xrtHttpFieldFind`、`Get`、`Count`：查找重复字段；
- `xrtHttpFieldGetUnique`：区分缺失、唯一和重复；
- `xrtHttpFieldValueValid`：验证字段值字节边界。

字段描述符可以未对齐，但描述符数组、借用文本和写出区不能发生不明确重叠。

## Content-Length

`xrtHttpContentLengthParse` 接受十进制值和 RFC 允许的重复相同列表值，拒绝空值、
符号、非数字、溢出和冲突值。

该函数只解析单个字段值。HTTP/1 parser 会合并全部重复 Content-Length 字段，
同时执行 Transfer-Encoding 冲突和请求分帧检查。

## token 与列表

- `xrtHttpTokenValid` 验证 RFC tchar；
- `xrtHttpTokenEqual` 和 `xrtHttpMethodEqual` 执行 ASCII 不区分大小写比较；
- `xrtHttpTokenNext` 迭代逗号 token-list；
- `xrtHttpFieldTokenNext` 跨重复同名字段迭代；
- `xrtHttpFieldTokenCount`、`Find` 处理重复字段集合；
- `xrtHttpQualityParse` 解析 0 到 1 的 qvalue 为 0 到 1000 定点数；
- `xrtHttpWeightedTokenNext` 迭代带权 token。

`xrtHttpOwsTrim` 只移除 SP 和 HTAB，不把其他空白字符当作 HTTP OWS。

## 方法与状态

`xrtHttpStatusText` 返回内置状态的标准 reason phrase。未知状态返回空视图，调用方
仍可用 `uint16` 写出扩展状态。

`xrtHttpMethodSafe`、`xrtHttpMethodIdempotent` 和
`xrtHttpResponseContentAllowed` 提供基础协议语义。它们不替代应用权限、重试和
缓存策略。

## 参数

启用 `XRT_MODULE_HTTP_PARAM` 后，`xhttpparam` 表达 `name[=value]`：

- `XHTTP_PARAM_HAS_VALUE`：存在等号和值；
- `XHTTP_PARAM_QUOTED`：值来自 quoted-string，视图不含外层双引号；
- `XHTTP_PARAM_NONE`：只有名称。

`xrtHttpParamNext` 迭代分号参数，`xrtHttpDirectiveNext` 迭代逗号指令。对应的
`Count` 和 `Find` 函数使用同一套严格语法。

`xrtHttpQuotedValid`、`Read`、`Write` 处理 quoted-string 和 quoted-pair；
`xrtHttpParamValueCursorInit` 与 `xrtHttpParamValueNext` 零分配逐字节读取语义值，
并通过 `Offset` 暴露已经消耗的原始值长度；`xrtHttpParamValueWrite` 一次性输出
参数语义值；`xrtHttpParamWrite` 写单个参数。

`Build` 便利函数返回由 `xrtFree` 释放的零结尾字符串。热路径应优先使用测量加
调用方缓冲的 `Write` 版本。

## Host 与 request-target

启用 `http_host` 后，`xrtHttpHostParse` 返回借用的 `xhttpauthority`；
`xrtHttpHostValid` 执行同样的严格验证。解析过程不分配内存，IPv6 与 IPvFuture
字面地址的 `Host` 视图不包含方括号，`PortText` 保留显式端口的原始文本。

`xrtHttpAuthorityValid` 验证解析或手工构造的 authority。手工构造数值端口时设置
`XHTTP_AUTHORITY_HAS_PORT | XHTTP_AUTHORITY_PORT_VALUE` 并填写 `Port`，不需要伪造
`PortText`。`xrtHttpAuthorityPort` 将可用的显式端口写入调用方变量；省略端口或显式
空端口时返回调用方提供的默认值，超出 `uint16` 的合法协议端口文本不能直接交给
网络层，因此返回失败。

`xrtHttpTargetParse` 区分 origin-form、absolute-form、authority-form 和
asterisk-form，并结合方法检查 CONNECT 与 OPTIONS 的专用约束。结果同样只借用输入，
核心层不构建 URL 对象，也不执行查询参数解码。

## 已迁出能力

动态 Header 容器、RFC 8187 扩展值、MIME、Structured Fields、Digest Fields、
Forwarded、Link、Priority、Cache-Status、Proxy-Status 和 Content-Disposition
不属于 XRT HTTP 核心；需要这些高级协议能力时，请使用独立发布的 `xhttp` 扩展。
