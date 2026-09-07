---
num: 88
slug: http
title: HTTP 基础：字段、方法与目标
volume: 卷九 Web 协议核心
type: practice
lead: 传输无关的地基层：字段解析、token 迭代、Host 拆解与参数编解码——全部 http* 模块共用的词汇表。
api: http, http_connection
---

## 导读

卷九从 HTTP 的**地基层**开始——不碰网络、不碰版本，只定义"字段长什么样、方法怎么认、目标怎么拆"这些全部上层共用的词汇。`xrtHttpFieldParse` 严格解析单个头字段为借用视图；`xrtHttpTokenNext` 按 HTTP 规则切分 token-list；Host 拆解与参数编解码处理 URL 层的两类结构化文本；`FieldBlockWrite` 把字段视图原样回写。第 89 章的 HTTP/1 解析器、第 97 章的 xhttp 运行时、你自研的网关与中间件——全部建立在这套零分配原语上。读完本章你会认识到：HTTP 头处理的安全与性能，本质是**严格解析 + 借用视图 + 容量原子性**三件事。

## 引入

写一个反向代理，最内核的操作是什么？"改一个头再转发"——解析请求头、按名字查字段、改值或加字段、再把整个头块写回字节流。听起来简单，但每一步都有历史坑：字段名大小写怎么处理？token-list 里带空格与引号的项怎么切？值里的注释（RFC 7230 已废弃但线上还有）会不会炸解析器？改写时缓冲不够了怎么办——写一半吗？

XRT 地基层的答案可以总结成三条纪律。**严格解析**：非法输入当场拒绝（不静默修复）——CVE 数据库里一大半 HTTP 解析漏洞源于"宽容解析"在攻击者手里变成歧义。**借用视图**：解析结果全部是原输入的视图（零拷贝零分配）——高并发网关的每请求内存成本趋近于零。**容量原子性**：回写要么完整成功、要么一字节不动——"写半个报文"这种最恶心的中间状态从 API 层杜绝。

## 概念

### 字段模型：名、值与借用视图

`xhttpfield` 就两个视图：`Name` 与 `Value`。`xrtHttpFieldParse(文本, &字段)` 严格解析"名: 值"——冒号前是字段名（token 字符集校验）、冒号后是修剪过首尾 OWS（可选空白）的值。解析结果是借用：`Name.Data`/`Value.Data` 指向原文本内部，没有分配、没有复制。配套三件：`xrtHttpFieldCount`/`FieldFind`/`FieldNext`（字段数组的检索与遍历族）、`xrtHttpFieldBlockWrite(字段数组, 数量, 输出, 容量, &长度)`——把字段数组回写为"名: 值\r\n...\r\n"的完整头块；容量不足返回失败且**不写任何字节**（原子性）。

### token-list：逗号文化的迭代器

HTTP 是逗号分隔的世界：`Connection: keep-alive, Upgrade`、`Accept-Encoding: gzip, deflate`、`Cache-Control: no-cache, no-store`。`xrtHttpTokenNext(值视图, &偏移, &token)` 是标准迭代器——按 `,` 与 `;` 切分、跳过空白，返回 `XHTTP_NEXT_ITEM`（取到一项）/ `XHTTP_NEXT_END`（正常结束）。**参数怎么带**：`gzip;q=0.5` 里的 `q=0.5` 是分号后的参数——迭代器在分号处停，参数解析由各专字段层（如第 90 章 TE 的 q 值）处理。这个"token 先行、参数后置"的模型贯穿全部列表字段。

### 方法、目标与 Host 拆解

- **方法**：`xrtHttpMethodParse` 识别标准方法（GET/POST/...）与扩展方法（自定义 token）——严格 token 字符集，`GET;` 这种带分隔符的直接拒绝。
- **目标**：request-target 的四种形态（origin-form `/path?query`、absolute-form 完整 URL、authority-form、asterisk-form）由解析层区分；本章层提供 `xrtHttpTargetParse` 的形态识别与路径/查询切分。
- **Host 拆解**：`xrtHttpHostParse(值, &authority)` 产 `xhttpauthority` 视图（主机/端口）——处理域名、IPv4、以及**方括号 IPv6**（`[2001:db8::1]:8443` 的方括号必须成对、IPv6 字面量逐组校验——域名冒号端口这类歧义输入不会误判）。

### 参数编解码：查询串的两条路

`xrtHttpParamBuild`/`xrtHttpParamWrite`（编码侧）与 `xrtHttpParamCount`/`xrtHttpParamNext`/`xrtHttpParamFind`（解析侧）处理 `a=1&b=2` 形态的键值对：编码把任意字节转义为百分号形式，解码还原。两条纪律：**`+` 与 `%20` 的语义**——解码按 `application/x-www-form-urlencoded` 规则（`+` → 空格）；**空格与保留字符**——编码端只转义必要的（保留 `&`/`=` 作为结构分隔）。表单、查询串、Cookie 值共用这套。

### 与第 25 章字符串、第 77 章 DER 的同构

你会发现本章 API 与第 25 章（Trim/Cut 的视图管线）、第 77 章（DER 游标）形态同构：**借用输入 → 严格校验 → 视图输出 → 按需转换**。这是全库文本处理的一贯哲学——零拷贝贯穿到协议层，"解析一次、到处引用"。

### 解析管线全景

```diagram flow
- 输入：线上头字段文本（借用缓冲，零拷贝起点）
- 严格解析：FieldParse 校验 token 字符集并修剪 OWS → Name/Value 视图
- 结构化查询：TokenNext 切列表 / HostParse 拆 authority / ParamCount 数参数
- 回写：FieldBlockWrite 规范头块（容量原子性）——读改写的完整闭环
```

## 示例

### 第一个完整程序：字段解析、token 迭代与回写

下面的程序来自 `examples/http/base/main.c`——地基层三件套的最小闭环：

```embed path="examples/http/base/main.c" title="examples/http/base/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/base/main.c -lws2_32 -liphlpapi
keep-alive
Upgrade
Connection: keep-alive, Upgrade
```

**刚才发生了什么。** ① `FieldParse` 解析一条字段为借用视图（Name=`Connection`，Value=`keep-alive, Upgrade`——冒号后首空白被修剪）。② `TokenNext` 循环切出两个 token：`keep-alive` 与 `Upgrade`——偏移量由调用方持有，迭代器无状态；`NEXT_END` 是正常结束不是错误。③ `FieldBlockWrite` 把字段原样回写为完整头块——注意输出里冒号后的规范格式（一个空格）：**解析规范化了 OWS，回写得到的是规范形态**——代理"读改写"后线路字节是确定的。容量不够时它一个字节都不写——原子性让你可以放心地"先试小缓冲、失败换大的"。

### 第二个完整程序：Host 拆解的严格性

第二个程序来自 `examples/http/host/main.c`——方括号 IPv6 与端口的标准拆解：

```embed path="examples/http/host/main.c" title="examples/http/host/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/host/main.c -lws2_32 -liphlpapi
host=2001:db8::1 port=8443
```

**刚才发生了什么。** ① 输入 `[2001:db8::1]:8443` 拆出主机 `2001:db8::1` 与端口 `8443`——方括号成对校验、IPv6 字面量逐组核对。② 严格性边界：`example.com:8443` 带端口域名合法；`2001:db8::1`（无括号的裸 IPv6+意图端口）是歧义输入——解析器拒绝而不是猜。**为什么这么较真**：Host/Authority 是路由与身份判断的输入（第 79 章 SAN 匹配的 IP 形态、代理的目标选择），歧义解析等于把路由决策建立在与对端不同的理解上。配套的 `examples/http/field_tour`（3 行输出）巡检字段块计数、查找、token 迭代与回写的完整 API 面——本章的"可执行速查表"。

## 契约

- **借用视图**：字段/方法/目标/Host 的解析结果全部借用原输入；输入存活期内有效（第 15 章视图规则）。
- **严格解析**：非法 token 字符、歧义 Host、不成对方括号、字段名含非法字符——全部当场拒绝，不修复不猜测。
- **容量原子性**：`FieldBlockWrite` 等回写接口容量不足时零字节写入；成功才发布长度——不存在半报文。
- **token 迭代**：按 `,`/`;` 切分、跳空白；`NEXT_ITEM`/`NEXT_END` 两态；迭代器无内部状态（偏移归调用方）。
- **OWS 规范化**：解析修剪值首尾空白；回写产出规范形态（"名: 值"）。
- **Host 语义**：域名/IPv4/方括号 IPv6；端口可选；歧义输入拒绝。
- **参数编解码**：`+` 按表单语义解码；编码只转义必要字符（`&`/`=` 结构保留）。
- **零分配**：本章全部 API 不分配堆内存——每请求成本可预测。
- **裁剪**：`XRT_MODULE_HTTP` 独立；`HTTP_HOST`/`HTTP_PARAM`/`HTTP_TARGET` 等子域独立裁剪。

## 避坑

### 坑 1：值里自己找逗号切分

症状：对 `Accept-Encoding: gzip, deflate, br` 手写 `strchr(',')` 循环——遇到带引号的 token（`filename="a,b.txt"`）或空项（`a,,b`）切错。

原因：HTTP 列表的逗号语义有引号与注释的转义规则，裸 `strchr` 不认识。`TokenNext` 内建规则——列表处理永远走它。

```c bad
char* p = strtok(Value, ",");   /* 引号内的逗号被切断 */
while ( p ) { handle(p); p = strtok(NULL, ","); }
```

```c good
size_t iOffset = 0;
xstrview Token;
while ( (Next = xrtHttpTokenNext(Value, &iOffset, &Token)) ==
		XHTTP_NEXT_ITEM ) {
	handle(Token);   /* 引号/空白/参数边界由迭代器处理 */
}
```

### 坑 2：改写头块时缓冲不够写了半个报文

症状：自研代理在改写头后用"追加式"写法——缓冲中途满了，前半已发出、后半丢弃：对端收到截断的头块，连接报废。

原因：没有容量原子性意识的写法。HTTP 头块是一个完整单元，半单元是协议错误。

```c bad
size_t iUsed = 0;
for ( each field ) {
	iUsed += snprintf(Out + iUsed, Cap - iUsed,
		"%.*s: %.*s\r\n", ...);   /* 中途容量耗尽：半个头块 */
}
```

```c good
if ( !xrtHttpFieldBlockWrite(Fields, iCount, Out,
		sizeof(Out), &iSize) ) {
	/* 换大缓冲或分块策略——Out 保持原样（零字节写入） */
}
send(Out, iSize);
```

### 坑 3：Host 值直接当连接目标

症状：把 `Host: [2001:db8::1]:8443` 的原值传给解析函数失败或连错端口；大小写域名当不同主机（缓存键分裂）。

原因：Host 值是**结构化文本**不是裸主机名——方括号、端口、大小写归一化都要先拆解。`HostParse` 拆出主机与端口后才是可用的连接/路由输入。

```c bad
connect(g_Target /* 直接用 Host 原值 */, ...);
/* 带方括号与端口的原值不是主机名 */
```

```c good
xhttpauthority Authority;
if ( !xrtHttpHostParse(Field.Value, &Authority) ) {
	return reject("malformed host");
}
/* authority 拆解归一后的输入；
   大小写归一（域名不区分大小写）后再做路由/缓存键 */
```

## 练习

### 基础：字段巡检跑通

跑通 base 与 field_tour 两个示例，对照输出逐行标注每行验证了哪个 API。然后把 base 的输入换成 `Connection: keep-alive,, Upgrade`（空项）观察迭代器行为。验收标准：能解释空项是"跳过"还是"产出空 token"；field_tour 三行全绿。

### 进阶：请求头改写器

实现 `rewrite_host(字段数组, 数量, 新主机, 输出, 容量, &长度)`：按名字找到 Host 字段（大小写不敏感）、替换其值、其余字段原样，最后 `FieldBlockWrite` 回写。验收标准：无 Host 时明确失败；改写后头块逐字节符合规范形态；容量不足零写入（用 8 字节缓冲验证）。

### 挑战：mini 代理的头处理核

读入一段完整请求头（多行），拆成字段数组（自写循环 + `FieldParse`），统计：字段总数、重复字段名、Connection 的 token 集、Host 拆解结果、查询串参数个数（ParamDecode）。对同一输入改写"X-Forwarded-For"后回写。验收标准：全程零堆分配（第 6 章统计）；对畸形输入（字段名带空格、Host 歧义）拒绝并报告首个错误位置。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 地基三纪律 | 严格解析（不修复）+ 借用视图（零拷贝）+ 容量原子性（无半报文） |
| 字段模型 | `xhttpfield` = Name/Value 两视图；Parse 修剪 OWS、校验 token 字符集 |
| 回写 | `FieldBlockWrite` 字段数组 → 规范头块；容量不足零写入 |
| token 迭代 | `TokenNext` 按 `,`/`;` 切分；NEXT_ITEM/NEXT_END；偏移归调用方 |
| 列表参数 | 分号后参数（q 值等）由专字段层处理，token 迭代器止于分号 |
| Host 拆解 | `HostParse` 产 `xhttpauthority`；域名/IPv4/方括号 IPv6；歧义拒绝 |
| 方法/目标 | 标准+扩展方法 token 校验；target 四形态识别与路径/查询切分 |
| 参数编解码 | `+`→空格（表单语义）；编码只转义必要字符；`&`/`=` 结构保留 |
| 零分配 | 全章 API 无堆分配——每请求成本可预测 |
| 裁剪 | XRT_MODULE_HTTP 独立；fields 子域独立 |
