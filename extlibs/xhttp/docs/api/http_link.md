# Link

`http_link` 实现 RFC 8288 `Link` 字段。它提供零分配线路解析、关系查询、重复字段
组合、标题和 anchor 解码、相对目标解析，以及独立可裁剪的规范 writer。协议层不
依赖 HTTP Client、Server 或网络运行时，两端都可以直接使用。

## 裁剪与依赖

- `XHTTP_FEATURE_HTTP_LINK`：解析、关系和属性语义、字段迭代及常用 Helper；依赖
  `http_param`、`http_ext_value`、`http_language` 与 `url_param`。
- `XHTTP_FEATURE_HTTP_LINK_WRITE`：调用方缓冲写出和分配型 Build；依赖
  `http_link`。

解析、关系查找、参数查找、anchor/title 直接读取、目标解析和 writer 直接写出均不分配。
`xrtHttpLinkTargetResolve` 复用通用 URL 零分配相对解析器；只有名称含 `Build` 的 Link
函数把结果所有权交给调用方并要求使用 `xrtFree`。

## 解析

```c
xhttplinkcursor Cursor;
xhttplink Link;

xrtHttpLinkCursorInit(&Cursor);
while ( xrtHttpLinkNext(
	Value, &Cursor, &Link
) == XHTTP_NEXT_ITEM ) {
	if ( xrtHttpLinkRelationFind(
		&Link, XRT_STR_LITERAL("next")
	) == XHTTP_NEXT_ITEM ) {
		/* Link.Target 借用字段，可交给 xrtHttpLinkTargetResolve。 */
	}
}
```

`xrtHttpLinkElementParse` 解析单个 `link-value`，`xrtHttpLinkNext` 迭代单字段，
`xrtHttpLinkFieldNext` 按线路顺序跨越全部重复 `Link` 字段。字段游标在第一次读取
前完整预校验所有后续成员，并绑定首次传入的字段地址与长度或字段数组地址与数量。
迭代结束前输入必须保持地址、长度和内容不变；切换输入必须重新初始化游标。任何失败
都保持游标和输出不变。RFC 的 `#link-value`
允许空列表和空逗号成员，因此空字段会直接返回 `XHTTP_NEXT_END`。

每个实际元素必须包含合法 URI-reference 目标和 `rel`。注册关系名称在线路上使用
小写语法；`xrtHttpLinkRelationFind` 按 RFC 要求以 ASCII 大小写不敏感方式查询。
扩展关系必须是无 fragment 的绝对 URI。目标由完整 URL parser 校验，字段中的
逗号因为位于 `<...>` 内不会被误认为列表分隔符。quoted-string 中的 quoted-pair
先按 HTTP 语义读取，再由同一套无分配 RFC 3986 校验器检查 authority、IP literal、
端口、路径、查询和 fragment，不会因转义写法绕过 URI 结构检查。

`xhttplink` 保存首个 `rel`、`anchor`、`rev`、`media`、`title`、`title*` 与
`type`，并记录全部参数数量和可重复 `hreflang` 数量。RFC 要求这些单值参数的
后续重复项被忽略，因此解析器不会因第二项而丢弃整个链接；原始 `Parameters`
仍可用 `xrtHttpParamNext` 遍历，`xrtHttpLinkParam` 返回首个同名项。扩展属性不被
封闭枚举限制。

## 常用 Helper

- `xrtHttpLinkRelationFind`：判断一个元素是否声明指定关系。
- `xrtHttpLinkAnchorWrite/Build`：解码 anchor，但不替应用自动改变上下文。
- `xrtHttpLinkTitleWrite/Build`：优先读取 UTF-8 `title*`，不支持的字符集回退
  普通 `title`。
- `xrtHttpLinkTargetResolve/Build`：以调用方提供的绝对 Base 按 RFC 3986 解析
  相对目标。

`anchor` 能把链接上下文改成其他资源。应用如果不允许这种上下文重定向，必须忽略
整个带 anchor 的链接，不能去掉 anchor 后继续使用。xrt 保留这一策略入口，不把
特定同源或信任模型固化进协议 parser。

## 写出

```c
static const xhttplinkparamvalue Params[] = {
	{
		XRT_STR_INIT("title"),
		XRT_STR_INIT("next page"),
		XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
	}
};
xhttplinkvalue Link = {
	XRT_STR_LITERAL("/items?page=2"),
	XRT_STR_LITERAL("next alternate"),
	Params,
	1u
};
char Value[128];
size_t Size;

xrtHttpLinkElementWrite(
	&Link, Value, sizeof(Value), &Size
);
```

`Relations` 是已经解码的一个或多个空格分隔关系；writer 对单 token 直接写出，
其余情况自动使用 quoted-string。其他参数通过 `xhttplinkparamvalue` 传入：
`XHTTP_PARAM_NONE` 省略值，`XHTTP_PARAM_HAS_VALUE` 写 token，额外设置
`XHTTP_PARAM_QUOTED` 强制引号并转义语义值。参数数组不得再次声明 `rel`；
`anchor`、`rev`、`media`、`title`、`title*`、`type` 重复会被生产侧拒绝，
`hreflang` 和扩展属性保持调用方顺序。

`xrtHttpLinkElementWrite` 写一个元素，`xrtHttpLinkWrite` 写数组，
`xrtHttpLinkBuild` 返回零结尾拥有型字符串。空输出查询精确长度，容量不足发布所需
长度但不写部分结果，输入和输出重叠会失败。writer 在读取参数数组前验证数量乘法、
范围及全部描述符；描述符或语义失败时不修改输出和长度。空数组规范写为空 Link 列表。

## 示例与测试

- `examples/http/link/main.c`
- `tests/http/test_http_link.c`
- `tests/http/test_http_link_noalloc.c`
- `tests/http/test_http_link_write.c`
- `tests/http/test_http_link_write_noalloc.c`
- `tests/http/test_http_link_write_oom.c`
- `tests/http/test_http_link_mutation.c`
- `tests/single/test_single_http_link.c`
- `tests/single/test_single_http_link_write.c`

实现遵循 [RFC 8288](https://www.rfc-editor.org/rfc/rfc8288.html)、
[RFC 8187](https://www.rfc-editor.org/rfc/rfc8187.html) 与
[RFC 3986](https://www.rfc-editor.org/rfc/rfc3986.html)。
