---
num: 102
slug: xhttp-query
title: 查询串与表单：urlencoded 与 multipart
volume: 卷十 扩展库：xhttp
type: practice
lead: 零分配的 query 遍历（区分缺失值与空值）、拥有型 QueryParams 的改写重建、编码层与表单双形态——a 与 a= 不是一回事。
api: xhttp-query, xhttp-query_params, xhttp-form, xhttp-form_data
---

## 导读

URL 的 query 串与 HTTP 表单是两套相近但不相同的键值语法，xhttp 给了两层工具。**零分配层**（`query` 模块）：`xrtQueryNext` 直接在原始串上遍历——`xquerypair` 的 `XQUERY_HAS_VALUE` 精确区分 `a`（无值）、`a=`（空值）、`=v`（空 key）；`xrtQueryFind/Count/Validate` 同族；启用 `query_codec` 后加 percent 编解码。**拥有型层**（`query_params` 模块）：`xqueryparams` 容器解析、改写（`Set` 保位去重/`Append` 追加/`Sort` 稳定排序/`Compact` 紧凑）、重建——"改一个参数再拼回去"的标准路径。表单侧两形态：`application/x-www-form-urlencoded`（同一语法 + `+` 空格语义）与 `multipart/form-data`（第 91 章 quoted-string 的实战场）。raw 与拥有型两层与第 88/91 章的分层完全同构——先看语法层再谈容器。

## 引入

API 客户端的高频操作：翻页（`page=2` 改成 `page=3`）、加过滤（追加 `tag=network`）、复制链接改参数。手工字符串拼接的问题是**重复键**：`?tag=c&tag=xlang&debug` 里 tag 有两个值——追加第三个？替换全部？保留哪个的位置？`xqueryparams` 的语义是明确的：`Set` 保留首个同名位置删其余、`Append` 尾部追加、`Find` 顺序迭代全部同名——三种意图三个动词。而零分配层的价值在**读侧热路径**：代理按 `debug` 标志分流，每个请求都要看一眼 query——视图遍历零成本。

`a` 与 `a=` 的区分贯穿两层：`XQUERY_HAS_VALUE` 未设=无值（URL 里没写 `=`），设了且空=显式空值——写出往返时"要不要补 `=`"由此决定。与第 101 章存在位同一哲学：**语法结构不靠视图长度猜**。

## 概念

### 零分配层：迭代、查找与校验

`xrtQueryNext(串, &偏移, &对)` 三态迭代（`XQUERY_NEXT_ITEM/END/ERROR`）；重复键保持原顺序；前导 `?` 可省略；连续/前导/尾随 `&` 的空段跳过。`xrtQueryFind` 从偏移起按名迭代同名项（重复键的标准读法）；`xrtQueryCount` 计数；`xrtQueryValidate` 合法性判定。**本层不解码**——`%2F` 就是无interpretation的三个字符；确需解码值走 `query_codec`（`XQUERY_PARAMS_LENIENT_PERCENT` 之类的宽容开关在那层）。

### 拥有型层：容器四动词

```diagram flow
- 解析：ParamsParse（表单规则：+ → 空格、percent 解码、ErrorOffset 报百分号位置）
- 改写：Set（保首同名位删其余）/ Append（尾部）/ AppendPair·SetPair（保留无等号形态）
- 整理：Sort（按名字节稳定排序——同名保序）/ Compact（丢弃删除残留的废弃字节）
- 重建：Build → xrtFree 释放的零结尾串（写出即规范形态）
```

- **解析语义**：`application/x-www-form-urlencoded` 规则——默认严格拒绝非法 percent（`ErrorOffset` 给出百分号位置）；宽容模式（`XQUERY_PARAMS_LENIENT_PERCENT`）把无效百分号按普通字节保留——浏览器 `URLSearchParams` 兼容场景。
- **修改语义**：追加/设置复制名称与值（输入可借用容器自身——就地改安全）；`Get` 读首个同名、`Find` 迭代全部；容器修改使既有借用视图失效（第 18 章通则）。
- **限额**：`xqueryparamsconfig` 限对数/名单项/单值/总解码字节——物理字符串区触到 `MaxBytes` 前先紧凑化（限额逻辑不含增长余量）。
- **原子性**：`ParseAppend` 用工作副本解析，限额/语法/内存失败不暴露部分追加。

### 编码层与写出

`query_codec` 依赖 query 与 percent codec：从"未编码字段"直接构建 RFC 3986 query——`xrtQueryBuild`/`QueryAppendWrite` 一族把"编码哪些字符"的规则（`&`/`=` 结构保留、空格形态选择）内建。raw 层（解析转发不改写）与 codec 层（构造新串）**可独立裁剪**——纯代理不需要编码器。

### 表单双形态

- **urlencoded**：正文就是 query 语法（`a=1&b=2`）——第 102 章全部词汇直接适用；`Content-Type: application/x-www-form-urlencoded`。
- **multipart/form-data**：边界分隔的部件结构（每部件自己的头与内容）——文件上传的载体；boundary 来自第 91 章 Content-Type 参数（quoted-string 解码的那位常客）；`xrtFormDataConfigInit`/`FormDataAppendBody` 族提供构建与解析。`examples/http/form_data`/`multipart`/`multipart_stream`/`multipart_write` 覆盖构建、流式、写出三面。

### 与第 91 章参数层的分工

第 91 章讲的是**字段值内的分号参数**（`Content-Type; key=value`——HTTP 头语法）；本章是 **URL query 与表单**（`&` 分隔——URI/表单语法）。两套迭代器形态相同（偏移+三态）但语法域不同——选错域就是解析错误。`+` 的语义也只在表单域成立（urlencoded 的 `+`=空格；纯 URI query 里 `+` 是普通字符——第 88 章讲过的分界）。

## 示例

### 第一个完整程序：零分配遍历与语义区分

下面的程序来自 `examples/url/query/main.c`——四种键值形态一次遍历：

```embed path="extlibs/xhttp/examples/url/query/main.c" title="extlibs/xhttp/examples/url/query/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url/query/main.c -lws2_32 -liphlpapi
tag = c
tag = xlang
debug = <missing>
empty = 
```

**刚才发生了什么。** ① 输入 `?tag=c&tag=xlang&debug&empty=` 四项：重复键 tag 两值按原序产出、`debug` 无值（标志未设——打印 `<missing>`）、`empty` 显式空值（标志已设、长度 0——打印空串）。② **四种形态一个循环**：`XQUERY_HAS_VALUE` 一个位区分两对语义（无值 vs 空值）。③ 全程零分配——Key/Value 借用原串；这就是代理/网关读 query 的成本形态。④ 这个输出形态同时是**校验清单**：任何 query 处理代码对这四种输入的行为都该像这样显式分路。

### 第二个完整程序：容器改写与重建

第二个程序来自 `examples/url/query_params/main.c`——解析、Set/Append、Build 闭环：

```embed path="extlibs/xhttp/examples/url/query_params/main.c" title="extlibs/xhttp/examples/url/query_params/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url/query_params/main.c -lws2_32 -liphlpapi
page=2&tag=c&tag=network&tag=xlang
```

**刚才发生了什么。** ① `ParamsParse("page=1&tag=c&tag=network")` 表单规则解析三项（+ 与 percent 解码在此发生；ErrorOffset 在失败时报百分号位置）。② `Set("page","2")` ——page 原位改值；`Append("tag","xlang")` ——尾部追加第三个 tag：**Set 与 Append 对重复键的意图差**在此显形（Set 会清掉其余同名，这里 tag 用的是 Append 所以三值共存）。③ `Build` 重建 `page=2&tag=c&tag=network&tag=xlang`——规范形态零结尾串，`xrtFree` 释放。翻页、加过滤、复制改参的三行模板就是这两个 Set/Append 加一次 Build。

## 契约

- **零分配层**：迭代/查找/计数/校验全视图；三态返回；重复键保序；空段跳过；前导 ? 可省；不解码。
- **语义区分**：`XQUERY_HAS_VALUE` 区分无值/空值/空 key 四形态；写出往返由标志保真。
- **容器解析**：表单规则（+→空格、percent 解码）；默认严格（ErrorOffset 报位置）；宽容模式按字节保留无效百分号。
- **改写四动词**：Set（保位去重）/Append（追加）/Pair 形（保留无等号项）/Sort（名字节稳定序）/Compact（清废弃存储）。
- **所有权**：容器复制追加/设置的输入（可借自身）；修改使借用视图失效；Build 返回 xrtFree 串。
- **限额**：对数/名单/单值/总解码字节四线；触限先紧凑再判；ParseAppend 原子（失败不暴露部分结果）。
- **编码层**：`query_codec` 独立（raw 与构造可分开裁剪）；结构字符保留规则内建。
- **表单**：urlencoded=query 语法直用；multipart=部件结构（FormData 族）；boundary 出自 Content-Type 参数。
- **域分界**：本章 `&` 域；第 91 章 `;` 域——迭代器同形不同语法。

## 避坑

### 坑 1：把无值当空值（或反之）改写

症状：`?debug` 被改写成 `?debug=`——语义从"flag 存在"变成"值为空"；或反向丢 `=`。

原因：无值与空值是两种语法结构；用 `Pair.Value.Size == 0` 判断会把两者混为一谈——判断要看 `XQUERY_HAS_VALUE`，写出要用 `AppendPair/SetPair` 保形。

```c bad
if ( Pair.Value.Size == 0 ) {
	treat_as_flag(Pair.Key);   /* debug 与 empty= 被同等对待 */
}
```

```c good
if ( (Pair.Flags & XQUERY_HAS_VALUE) == 0 ) {
	treat_as_flag(Pair.Key);         /* 真·无值 */
} else {
	treat_as_value(Pair.Key, Pair.Value);  /* 有值（可能为空串） */
}
```

### 坑 2：容器解析当 URL 解析用（+ 号歧义）

症状：用 `ParamsParse` 处理**非表单**的 URI query——`a+b` 被解码成 `a b`，但纯 URI 语义里 `+` 就是加号字符。

原因：`+`→空格是 urlencoded **表单**规则；RFC 3986 query 里 `+` 是普通字符。两层规则不同：读 URI query 用零分配层（不解码）+ 按需 percent 解码；表单正文才用 ParamsParse。

```c bad
/* 处理搜索 URL 的 query：+ 被误当空格 */
pParams = xrtQueryParamsParse(Url.Query, NULL, &iErr);
```

```c good
/* URI query：零分配遍历 + 明确的 percent 解码 */
while ( xrtQueryNext(Url.Query, &iOff, &Pair) == XQUERY_NEXT_ITEM ) {
	percent_decode_value(Pair.Value, Buf, sizeof(Buf));   /* 只解码 %xx */
}
```

### 坑 3：边遍历边 Build 生成翻页链接漏了其余参数

症状：翻页链接只带了 page——tag、排序等参数全丢了。

原因：从零拼新 query（只写 page）而不是在既有容器上改。正确路径：Parse 既有 → Set("page", N) → Build——**改一个参数不动其余**。

```c bad
snprintf(Next, "%s?page=%u", Path, Page + 1);  /* 其余参数蒸发 */
```

```c good
pParams = xrtQueryParamsParse(Url.Query, NULL, &iErr);
xrtQueryParamsSet(pParams, XRT_STR_LITERAL("page"), PageText);
sNext = xrtQueryParamsBuild(pParams, &iSize);
/* page 原位改，tag/sort 等全部保留 */
```

## 练习

### 基础：四形态往返

构造含四种形态的 query（无值/空值/空 key/正常），零分配层遍历打印；再 `ParamsParse` → `Build` 对比往返差异（哪些形态 ParamsParse 会规范化）。验收标准：能指出往返中语义保真与规范化的边界。

### 进阶：翻页链接生成器

实现 `next_page(当前URL文本, &下一页文本)`：`UrlParse` → `ParamsParse` → `Set("page")` → `Build` → 拼回 URL。对带 tag 重复键与 `debug` 无值 flag 的 URL 验证。验收标准：翻页只动 page；重复键与 flag 原样保留；产出零结尾串正确释放。

### 挑战：表单双形态转换器

实现 `urlencoded_to_multipart(字段容器, 输出, 容量)`：把 Params 容器的字段渲染成 multipart 部件（boundary 自生成、每部件带 `Content-Disposition: form-data; name="..."`——名字过 quoted-string 规则）。反向实现 multipart 解析回容器。用 `form_data` 示例对拍。验收标准：双向转换字段保真（含含引号/分号的值）；boundary 满足分隔符唯一性要求（正文不出现）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 两层结构 | 零分配（Next/Find/Count/Validate，不解码）+ 拥有型（Params 容器） |
| 四形态 | a=无值 / a==空值 / =v=空 key / 正常——XQUERY_HAS_VALUE 一个位分全 |
| 容器解析 | 表单规则（+→空格/percent 解码）；严格默认+ErrorOffset；宽容模式可开 |
| 四动词 | Set 保位去重 / Append 追加 / Sort 稳定 / Compact 清废弃 |
| 原子性 | ParseAppend 工作副本——失败不暴露部分追加 |
| 限额 | 对/名单/单值/总字节四线；触限先紧凑 |
| 编码层 | query_codec 独立（构造新串）；raw 与构造分开裁剪 |
| + 号域 | 表单=空格；URI query=普通字符——两层别混用 |
| 表单双形态 | urlencoded=query 语法；multipart=部件+boundary（FormData 族） |
| 与 91 章分界 | 本章 & 域（URI/表单）；91 章 ; 域（头字段值） |
