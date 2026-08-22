# HTTP 路由模板

`<xrt/http_route.h>` 提供不依赖网络、Server、Handler 或路由表的原始路径模板层。它用于服务器路由器、代理、鉴权规则和测试工具，也允许应用直接匹配少量路径而不创建任何对象。

## 模板

模板必须是以 `/` 开始的绝对 RFC 3986 路径，不包含 query 或 fragment：

- `/users`：静态路径，按原始字节区分大小写。
- `/users/{id}`：`id` 捕获一个非空路径段。
- `/files/{path...}`：末尾参数捕获零个或多个剩余段，值中保留 `/`。

参数名使用 ASCII 标识符规则：首字节为字母或下划线，其余字节还可以是数字。同一模板不能重复参数名，尾参数只能是最后一段。需要把 `{`、`}` 或非 ASCII 字节作为静态路径内容时，应先使用 percent 编码。

匹配不会折叠重复斜杠，也不会忽略尾斜杠。因此 `/a`、`/a/` 和 `/a//` 是三个不同路径。该规则避免路由器在应用之前悄悄改变签名、代理或安全策略所看到的路径。应用需要规范化时，应在路由前显式调用 URL 规范化能力并决定重定向或拒绝策略。

## 原始捕获

`xrtHttpRouteMatch` 不执行 percent 解码。`%2F` 仍属于当前路径段，捕获值保留原始拼写；这样编码斜杠不会改变路由边界，也不会让代理和应用看到不同路径。需要文本值的应用可以在命中后显式解码单个参数，并自行决定是否允许解码后的 `/`、零字节或非 UTF-8 数据。

`xhttprouteparam.Name` 借用 `Pattern`，`Value` 借用 `Path`。两个输入在捕获使用期间都必须保持有效。模块不分配内存、没有全局状态，可以并发调用。

`xrtHttpRouteValidate` 的计数输出、`xrtHttpRouteMatch` 的计数与捕获数组都允许使用完整但未对齐的存储。固定描述符通过字节复制发布，不要求调用方为了协议数据额外调整布局。`xrtHttpRouteParam` 返回输入数组中的借用地址；输入数组未对齐时，返回地址也可能未对齐，此时必须用 `memcpy` 复制到自然对齐的局部变量后再访问字段。

## 容量

`xrtHttpRouteMatch` 的结果具有四种稳定含义：

| 结果 | 含义 |
| --- | --- |
| `XHTTP_ROUTE_ERROR` | 参数、模板或路径非法，线程错误已设置 |
| `XHTTP_ROUTE_MISS` | 合法模板与路径正常未命中，不设置错误 |
| `XHTTP_ROUTE_MATCH` | 命中且全部捕获已经写入 |
| `XHTTP_ROUTE_MORE` | 路径命中但捕获数组不足，未写入任何捕获 |

传入 `Params = NULL`、`Capacity = 0` 可以先查询所需捕获数量。容量不足时 `Count` 返回完整需求，已有数组保持不变。模板可在注册阶段通过 `xrtHttpRouteValidate` 提前验证并取得参数数量。

捕获数量没有内嵌数组上限，只受调用方容量和 `size_t` 地址空间约束；实现不会因为参数超过 32 个而分配临时内存。

## 示例

```c
xhttprouteparam Params[2];
size_t Count;

if ( xrtHttpRouteMatch(
	XRT_STR_LITERAL("/users/{id}/files/{path...}"),
	XRT_STR_LITERAL("/users/42/files/docs/readme.txt"),
	Params, 2, &Count
) == XHTTP_ROUTE_MATCH ) {
	/* Params[0] 为 id，Params[1] 为 path。 */
}
```

完整示例见 `examples/http/route/main.c`。

边界门禁位于 `tests/http/test_http_route.c`、`tests/http/test_http_route_noalloc.c` 和 `tests/http/test_http_route_fuzz.c`。`fuzz/http_route.c` 同时提供 Clang/libFuzzer 入口，随机输入只在本地内存中运行，不访问网络。
