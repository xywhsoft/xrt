# HTTP Headers

`xhttpheaders` 是拥有名称和值副本的可复用 Header 容器。底层协议热路径仍直接使用
XRT 的 `xhttpfield` 数组；只有需要增删改查、跨调用保存或构建字段块时才创建容器。

```c
xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);

xrtHttpHeadersAdd(
	pHeaders,
	XRT_STR_LITERAL("Content-Type"),
	XRT_STR_LITERAL("application/json")
);
xrtHttpHeadersDestroy(pHeaders);
```

默认配置允许长字段，并以 `MaxFields`、`MaxName`、`MaxValue` 和 `MaxBytes`
分别限制不可信输入。`Clear` 保留容量，适合连接或请求对象池复用。
