# HTTP Accept 内容协商

`XHTTP_FEATURE_HTTP_ACCEPT` 提供 RFC 9110 `Accept` 媒体类型协商，依赖
`XRT_FEATURE_MIME`。解析、匹配和选择路径均不分配堆内存，也不把协议层与
客户端或服务器运行时绑定。

规范依据：[RFC 9110 Section 12.5.1](https://www.rfc-editor.org/rfc/rfc9110.html#name-accept)。

## API 层次

- `xrtHttpMediaRangeNext` 迭代一个字段值，适合协议工具和自定义策略。
- `xrtHttpMediaRangeParamNext` 按线路顺序迭代非 `q` 媒体参数。
- `xrtHttpAcceptNext` 跨越重复 `Accept` 字段，保留线路顺序。
- `xrtHttpMediaRangeMatch` 匹配两个已解析对象。
- `xrtHttpAcceptMatch` 返回决定质量值的具体范围。
- `xrtHttpAcceptQuality` 是只需要质量值时的便捷函数。
- `xrtHttpAcceptSelect` 从服务端偏好顺序中选择表示。

## 解析契约

`xhttpmediarange` 将一项拆为 `Type`、`Subtype`、完整 `Parameters`、
`ParameterCount`、`Quality` 和 `Specificity`。`Parameters` 借用并保留原始参数
序列，包含线路中的 `q`；`ParameterCount` 只统计实际参与表示匹配的非 `q`
参数。需要查看原始参数时可使用 `xrtHttpParamNext`，普通调用方应使用
`xrtHttpMediaRangeParamNext` 自动跳过权重参数。

解析器接受 `*/*`、`type/*` 和完整媒体类型，忽略列表空成员，拒绝重复参数、
quoted `q`、超过三位小数的质量值、无值媒体参数和不完整 quoted-string。
根据 RFC 9110，接收端把任意位置且名称大小写不敏感的 `q` 解释为权重；其他
参数无论位于 `q` 之前还是之后都属于媒体范围并参与匹配。发送端仍应把 `q`
放在最后，避免与旧实现互操作时产生歧义。错误不推进游标，并清空结果输出。

## 匹配与选择

一个表示同时匹配多个范围时，完整媒体类型优先于 `type/*`，后者优先于
`*/*`；具体度相同时，媒体参数更多者优先；仍相同时保留最早的字段成员。
媒体参数名称忽略 ASCII 大小写，普通参数值比较解码后的字节，`charset` 值
额外忽略 ASCII 大小写。

没有 `Accept` 字段表示任意媒体类型均可接受，质量值为 1000；字段存在但为空
表示没有可接受表示。`xrtHttpAcceptSelect` 先比较客户端有效质量，质量相同则
保留 `pMediaTypes` 中更靠前的服务端偏好项。质量为零的表示不会被选择。

```c
static const xhttpfield Fields[] = {
	{
		XRT_STR_INIT("Accept"),
		XRT_STR_INIT(
			"text/html;q=0.7, "
			"application/json;q=1;profile=full"
		)
	}
};
static const xstrview Available[] = {
	XRT_STR_INIT("text/html; charset=UTF-8"),
	XRT_STR_INIT("application/json; profile=full")
};
size_t iIndex;

if ( xrtHttpAcceptSelect(
	Fields, 1, Available, 2, &iIndex
) == XHTTP_NEXT_ITEM ) {
	/* Available[iIndex] 是最终表示。 */
}
```

需要自定义媒体参数语义、组合服务端权重或实现其他排序策略时，应使用
`xrtHttpAcceptNext` 和公开的 `xhttpmediarange` 自行决策，不需要复制解析器。

## 裁剪与测试

单独启用 `XRT_MODULE_HTTP_ACCEPT` 只拉入 HTTP 参数和 MIME 媒体类型层，不依赖
网络、客户端、服务器、`Content-Disposition` 或 RFC 8187。模块测试覆盖重复
字段、通配符、具体度、`q` 前后参数、选择稳定性、非法语法、未对齐描述符、
游标和结果、地址回绕、输入重叠、失败原子性、零分配和单头发布。
