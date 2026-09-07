---
num: 91
slug: http-headers
title: 头字段族：参数、连接与协商
volume: 卷九 Web 协议核心
type: practice
lead: 分号参数与 quoted-string、Connection/Upgrade 能力、Expect 与 trailer——专字段层的完整处理面。
api: http, http_te, http_connection
---

## 导读

第 88 章讲了"一个字段怎么解析"；本章讲"一族字段怎么用"。HTTP 头里有一批字段自带**结构化语法**：`Content-Type` 的分号参数（`charset=UTF-8; boundary="part;42"`——值里可以含引号与分号）、`Connection`/`Upgrade` 的 token 列表（连接能力协商——第 94 章 WebSocket 握手的判定依据）、`Expect`（客户端预期——100 Continue 流程）、`TE`/`Trailer`（传输编码与尾字段能力，第 90 章）。每个族都有自己的解析陷阱（quoted-string 的转义、token 的大小写、可重复字段），XRT 为每族提供独立小模块——`ParamNext` 参数迭代、`FieldTokenFind` token 查找、`xrtHttpTeParse` 协商汇总。代理、中间件、协议实现者的日常工具箱。

## 引入

实现一个 multipart 表单解析器，第一步是从 `Content-Type: multipart/form-data; boundary="part;42"` 里取出 boundary——值带着引号，而且**引号里有分号**（boundary 可以含分号！）。用 `strchr(';')` 切分的人在这里翻车：把 `part;42` 切成两半。再比如你写 WebSocket 升级判定：`Connection` 里是否含 `upgrade` token？大小写、空格、引号——手写判断又是一堆边界。

这两个场景的共同点：**字段值的语法比"看起来是文本"复杂**——RFC 9110 为不同字段族定义了不同语法（token-list、分号参数、quoted-string），每一族都有规范的解析规则与历史实现漏洞。XRT 的专字段层把每族的规则实现成独立小 API：参数层的引号解码、token 层的大小写不敏感查找、TE 层的 q 值协商（第 90 章已见）。本章逐族讲清"语法是什么、API 怎么用、坑在哪"。

## 概念

### 参数层：分号世界与 quoted-string

字段值的"分号参数"语法（Content-Type、Content-Disposition 等）：`主值; key=value; key2="quoted string"`。API 三件：

- `xrtHttpParamNext(值, &偏移, &参数)`：迭代产出 `xhttpparam`——`Name` 视图、`Flags`（`XHTTP_PARAM_HAS_VALUE`——允许无值参数）、原始 Value 视图。
- `xrtHttpParamValueWrite(&参数, 输出, 容量, &长度)`：**解码 quoted-string 后写值**——引号剥除、`\"` 反转义、容量原子性。boundary=`"part;42"` 解出 `part;42`——分号安全存活。
- `xrtHttpParamCount`/`ParamFind`/`ParamBuild`：计数、按名查找、构造新参数串。

为什么"迭代给原始视图、解码单独一步"：多数消费场景只需要比较参数名或长度——不解引号零成本；确需值内容时才解码。

### token 层：连接能力

`Connection`/`Upgrade`/`Transfer-Encoding` 都是 token-list（第 88 章 TokenNext 的领域），专字段层补上**语义查找**：`xrtHttpFieldTokenFind`——在字段值里查找指定 token（大小写不敏感）是否存在。"Connection 含 upgrade 吗"一行回答；第 94 章 WebSocket 握手判定、代理的逐跳字段剥离（`Connection` 列出的字段名要删掉再转发——`xrtHttpConnectionNext` 迭代给出这个列表）都建立在它上面。

### Expect 与 100 Continue

客户端发大正文前问一句"服务端收不收"：`Expect: 100-continue`。`xrtHttpExpectFields`/`ExpectValid` 判定请求是否携带合法的 100-continue 预期——服务端据此先答 `100 Continue` 再收正文，或直接 417 拒绝。**为什么值得专字段**：100 的时序与正文流交织（服务端可先答 100、收正文、再答最终状态），实现者容易漏"不该等 100 就发正文"的宽容路径——专字段层把判定标准化，时序归连接层。

### 字段数组的检索全家

第 89 章解析出的字段数组，配套检索族（地基层实现、头层复用）：`xrtHttpFieldFind`（按名首个）、`xrtHttpFieldNext`（同名多值迭代——Set-Cookie 场景）、`xrtHttpFieldGet`/`FieldGetUnique`（取值/唯一值语义——`GetUnique` 用于"语义上只该出现一次"的字段，重复即错误）、`xrtHttpFieldNameEqual`（大小写不敏感比较）、`xrtHttpFieldTokenCursorInit`+`FieldTokenCount`（token 计数与游标）。这套族是"查头"的全部词汇——中间件按 Route/Cache-Control 决策的底座。

### 族模块的裁剪面

每族一个特性宏（`HTTP_HOST`/`HTTP_PARAM`/`HTTP_TARGET`/`HTTP_TE`/`HTTP_CONNECTION`/`HTTP_EXPECT`/`HTTP_TRAILER`/`HTTP_UPGRADE`...）：嵌入式客户端只带 Host+Param 不携带升级与 Expect；网关全带。专字段层不引入彼此依赖——裁剪是真正独立的。

### 专字段族的分层

```diagram flow
- 地基（第 88 章）：FieldParse 字段视图 + TokenNext 列表迭代
- 参数族：ParamNext/ValueWrite —— 分号参数与 quoted-string 解码
- 语义查找族：FieldFind/FieldGetUnique/FieldTokenFind —— 大小写不敏感检索
- 专字段族：TE/Encoding/Upgrade/Expect/Trailer/Connection —— 各自独立裁剪的协商小模块
```

## 示例

### 第一个完整程序：参数迭代与引号解码

下面的程序来自 `examples/http/param/main.c`——quoted-string 陷阱的标准解法：

```embed path="examples/http/param/main.c" title="examples/http/param/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/param/main.c -lws2_32 -liphlpapi
charset = UTF-8
boundary = part;42
```

**刚才发生了什么。** ① 输入 `charset=UTF-8; boundary="part;42"`——两条参数。② `ParamNext` 逐条产出：第一条有值（`HAS_VALUE` 标志）、Name=`charset` 原始 Value=`UTF-8`（裸 token 无需解码，但统一走 ValueWrite 也正确）；第二条 Name=`boundary`、原始值是**带引号的 `"part;42"`**。③ `ParamValueWrite` 解码：引号剥除、内部 `;` 完整保留——输出 `part;42`。**这就是 strchr 切分法的翻车现场反面**：分号在引号内不切、引号成对校验、转义序列还原，全部由解码器处理。容量不足零写入——与全库原子性一致。

### 第二个完整程序：字段检索与回写巡检

第二个程序来自 `examples/http/field_tour/main.c`——字段块、查找、token 与回写的四行自检：

```embed path="examples/http/field_tour/main.c" title="examples/http/field_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/http/field_tour/main.c -lws2_32 -liphlpapi
block-count=3 next=3
get=keep-alive, Upgrade value-valid=1 count=1 unique=hit find=1 find2=2
tokens: gzip deflate count=2 find=1
write=C blockwrite=ok
```

**刚才发生了什么。** ① 第 1 行字段块计数与迭代——多字段数组的批量处理面。② 第 2 行一次跑完检索族：按名取值、视图合法性、计数、`GetUnique` 唯一值语义（本章坑 3 的正面用法）、两次 `Find` 验证同名多值按序返回（find2=2——第二个同名字段）。③ 第 3 行 token 族：Accept-Encoding 的列表迭代、计数与指定 token 查找——本章"token 语义查找"的可运行注脚。④ 第 4 行回写往返（`write=C` 是单字段、`blockwrite` 是整块）——第 88 章原子性约定的复验。配套的 `examples/http/small_fields` 覆盖 TE/Encoding/Upgrade/Expect/Trailer 五个专字段族的联合巡检、`examples/http/host` 覆盖 authority 拆解——本章全部 API 都有可运行样本。

## 契约

- **参数迭代**：原始视图先行（Name/带引号 Value）、`HAS_VALUE` 标志区分无值参数；解码（ValueWrite）按需触发，引号剥除与转义还原、容量原子。
- **token 语义查找**：`FieldTokenFind` 大小写不敏感；`Connection` 列出的字段名经 `ConnectionNext` 迭代——逐跳字段剥离的依据。
- **唯一值语义**：`FieldGetUnique` 用于单值字段——重复出现报错而非"取第一个"（静默选择=歧义接受）。
- **Expect**：`ExpectFields`/`ExpectValid` 判定期望存在与合法性；100 时序属连接层。
- **Trailer 声明**：`Trailer` 字段预告的 trailer 名单可查——与第 90 章读取侧数组对账。
- **q 协商**：千分位整数（TE/Encoding 同模型）；未提及 q=0。
- **族独立裁剪**：各专字段宏互不依赖；`ParamValueCursorInit/ValueNext` 提供不解码的值迭代形态。
- **零分配**：族 API 全部借用视图 + 调用方缓冲。

## 避坑

### 坑 1：strchr 切参数，引号里的分号翻车

症状：multipart 边界取错——`boundary="part;42"` 被切成 `boundary="part` 与 `42"`，表单解析全错。

原因：分号参数语法里，quoted-string 内的分号不是分隔符。手写切分不认识引号上下文。

```c bad
char* p = strtok(Value, ";");   /* part;42 的分号被当分隔符 */
```

```c good
xhttpparam Param; size_t iOff = 0;
while ( (N = xrtHttpParamNext(Value, &iOff, &Param)) ==
		XHTTP_NEXT_ITEM ) {
	char Buf[64]; size_t iSize;
	if ( (Param.Flags & XHTTP_PARAM_HAS_VALUE) &&
			xrtHttpParamValueWrite(&Param, Buf,
				sizeof(Buf), &iSize) ) {
		use(Param.Name, Buf, iSize);   /* 解码值：part;42 完整 */
	}
}
```

### 坑 2：token 查找大小写敏感

症状：对 `Connection: keep-alive, Upgrade` 用 `strstr(value, "upgrade")` 判升级——返回 NULL，WebSocket 握手被拒。

原因：HTTP token 语义大小写不敏感，线上大小写五花八门（`Upgrade`/`upgrade`/`UPGRADE` 都合法）。

```c bad
if ( strstr(Field.Value, "upgrade") != NULL ) {
	/* "Upgrade" 的大小写进不来：合法握手被拒 */
}
```

```c good
if ( xrtHttpFieldTokenFind(Fields, iCount,
		XRT_STR_LITERAL("upgrade")) ) {
	/* 大小写不敏感 + 引号上下文由族 API 处理 */
}
```

### 坑 3：单值字段用"取第一个"处理重复

症状：`Content-Length: 11` 与 `Content-Length: 12` 同时出现，代码按第一个处理、无视第二个——前后端理解不同：走私变体。

原因：语义上唯一的字段重复出现就是错误输入。"取第一个"把歧义静默消解，等于替攻击者选了一个理解。

```c bad
xstrview V = first_field("Content-Length", ...);
parse_length(V);   /* 第二个 CL 无人过问 */
```

```c good
xstrview V;
if ( !xrtHttpFieldGetUnique(Fields, iCount,
		XRT_STR_LITERAL("Content-Length"), &V) ) {
	reject("conflicting content-length");  /* 重复即拒绝 */
}
```

## 练习

### 基础：五族巡检跑通

跑通 small_fields 与 param 两个示例，给每行输出标注对应族与 API。把 param 输入换成 `boundary="a\"b"`（转义引号）验证解码。验收标准：转义还原正确（`a"b`）；空参数（只有名无值）路径走 HAS_VALUE 为假的分支。

### 进阶：Content-Type 消费器

实现 `content_type(字段值, &主类型, &子类型, 参数数组, 容量)`：切出 `multipart/form-data` 两段与全部参数（引号解码）。对 `text/plain`（无参数）与 `multipart/mixed; boundary="x;y"; charset=UTF-8`（多参数含引号分号）验证。验收标准：两组输出与手推一致；主/子类型大小写归一后再比较。

### 挑战：逐跳字段剥离器

实现代理的 hop-by-hop 处理：`Connection` 迭代出逐跳字段名列表 + 标准逐跳集合（Connection/Keep-Alive/TE/Trailer/Upgrade），从字段数组中删除命中的（压缩数组），再 `FieldBlockWrite` 回写。验收标准：`Connection: keep-alive, X-Push` 的请求剥离后 `X-Push` 消失、`Connection` 自身保留但去掉已删项；对端到端字段（Accept 等）原样；全程零堆分配。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 参数层 | ParamNext 迭代（原始视图+HAS_VALUE）；ValueWrite 解码（引号剥除/转义/原子性） |
| quoted-string | 引号内分号不切、`\"` 还原——strchr 手切是事故源 |
| token 查找 | FieldTokenFind 大小写不敏感；ConnectionNext 迭代逐跳字段名 |
| 检索族 | FieldFind/FieldNext（多值）/FieldGet（取值）/FieldGetUnique（重复即错） |
| Expect | ExpectParse 判 100-continue 期望；时序归连接层 |
| Trailer 声明 | Trailer 字段预告名单可查；与读取侧数组对账 |
| q 协商 | 千分位整数；TE/Encoding 同模型；未提及 q=0 |
| 升级族 | Upgrade 迭代/计数/构造——第 94 章握手零件 |
| 裁剪 | 每族独立宏（HOST/PARAM/TE/CONNECTION/EXPECT/TRAILER/UPGRADE...） |
| 零分配 | 全族借用视图+调用方缓冲 |
