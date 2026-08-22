# QueryParams

`query_params` 是 `query` 和 `form_urlencoded` 之上的拥有型参数容器。它保留插入顺序、重复名称和 `a` 与 `a=` 的差异，名称比较区分大小写。

## 裁剪

定义 `XHTTP_MODULE_QUERY_PARAMS` 启用该模块。它依赖 `query` 与 `form_urlencoded`，不会引入 HTTP 客户端、服务器或网络运行时。

## 所有权

- `xrtQueryParamsCreate`、`xrtQueryParamsParse` 和 `xrtQueryParamsClone` 返回的对象由 `xrtQueryParamsDestroy` 释放。
- `xrtQueryParamsAt` 与 `xrtQueryParamsFind` 返回借用视图；任意容器修改都会使这些视图失效。
- `xrtQueryParamsBuild` 返回由 `xrtFree` 释放的零结尾文本。
- 容器会复制追加和设置的名称和值，输入可以借用容器自身。

## 解析

`xrtQueryParamsParse` 和 `xrtQueryParamsParseAppend` 使用 `application/x-www-form-urlencoded` 规则：`+` 解码为空格，percent 转义解码为原始字节，连续与首尾空段跳过。

默认严格拒绝不完整或非法 percent 转义，并通过 `ErrorOffset` 返回百分号位置。配置 `XQUERY_PARAMS_LENIENT_PERCENT` 后，无效百分号按普通字节保留；该模式适合需要浏览器 `URLSearchParams` 兼容性的输入。

`ParseAppend` 使用工作副本完成全部解析，限额、语法或内存失败不会暴露部分追加结果。字段在测量和预留后直接解码到工作容器的连续存储，解析不会为每个字段建立临时堆缓冲。

## 修改

`xrtQueryParamsAppend` 与 `xrtQueryParamsSet` 是常用带值路径。`AppendPair` 与 `SetPair` 允许通过 `XQUERY_HAS_VALUE` 保留无等号项。`Get` 一步读取首个同名项，`Find` 负责顺序迭代全部同名项。

`Set` 保留首个同名项的位置并删除其余同名项。`Sort` 按名称字节稳定排序，同名项维持原顺序。`Compact` 丢弃删除和替换留下的废弃字节。

## 限额

`xqueryparamsconfig` 分别限制 pair 数、单个名称、单个值和全部有效已解码字节。容量增长余量与废弃存储不计入逻辑限额；实现会在物理字符串区触及 `MaxBytes` 前先紧凑化。

## 范例

```c
xqueryparams* pParams;
str sQuery;
size_t iError;
size_t iSize;

pParams = xrtQueryParamsParse(
	XRT_STR_LITERAL("page=1&tag=c&tag=network"),
	NULL,
	&iError
);
if ( pParams == NULL ) {
	return false;
}
xrtQueryParamsSet(
	pParams, XRT_STR_LITERAL("page"), XRT_STR_LITERAL("2")
);
xrtQueryParamsAppend(
	pParams, XRT_STR_LITERAL("tag"), XRT_STR_LITERAL("xrt")
);
sQuery = xrtQueryParamsBuild(pParams, &iSize);
xrtQueryParamsDestroy(pParams);
```
