---
num: 94
slug: http-upgrade
title: HTTP 升级：从请求到 101
volume: 卷九 Web 协议核心
type: practice
lead: Upgrade 提议的解析与应答、WebSocket 握手请求构造、Accept 键计算与子协议协商——协议切换的完整入口。
api: http_upgrade, websocket_upgrade, websocket, http
---

## 导读

HTTP/1.1 有一个"变身"机制：`Upgrade` 头提议切换协议，服务端回 `101 Switching Protocols` 后**同一条 TCP 连接上说新语言**。WebSocket 是这个机制最著名的用户——第 95～97 章的帧、消息、流全部从一次成功的升级开始。本章把升级的两侧讲全：**提议侧**（`xrtHttpUpgradeFieldCursorInit/FieldNext` 迭代客户端提议、`xrtHttpUpgradeWrite` 把"我支持的子集"写成规范应答——不限于 WebSocket，h2c 等升级同用）；**WebSocket 握手侧**（`websocket/upgrade` 的请求/应答构造与校验、`xrtWsAccept` 的 RFC 规范计算、`xrtWsProtocolSelect` 子协议协商）。Accept 值有 RFC 6455 的标准测试向量——对上即实现正确的互操作证明。

## 引入

为什么 WebSocket 要从 HTTP 开始？工程答案：80/443 端口已开放、代理与防火墙认识 HTTP、鉴权与路由可以复用 HTTP 基础设施。协议答案：升级让"谈判"用双方都会的语言完成——客户端提议（我要说 websocket，子协议给这几个）、服务端选择（101 + 我接受的关键参数），谈判完成才换语言；任何一方不满意就继续说 HTTP（正常响应，不切换）。

升级语义的三个细节值得先立起来：**提议可以重复**（多条 Upgrade 字段、逗号列表——第 92 章的字段族规则）；**协议名大小写不敏感**（`WebSocket`/`websocket` 同义）；**版本可以带**（`HTTP/2.0` 这种 `protocol/version` 形态）。这些全部由升级层的迭代器处理——又是"别手切"的故事。

## 概念

### 提议侧：迭代与应答

```diagram flow
- 客户端：Upgrade: h2c, websocket（可重复字段、逗号列表、大小写不敏感）
- 服务端迭代：FieldCursorInit + FieldNext → 逐个产出 protocol/version 提议
- 服务端决策：与本地支持列表求交集（顺序按客户端偏好）
- 应答：UpgradeWrite 把"我支持的子集"写成规范 Upgrade 值 + 101 响应
- 切换：101 发出后同连接改说新协议；未匹配则正常 HTTP 响应（不切换）
```

`xrtHttpUpgradeFieldNext` 的产出 `xhttpupgradeitem` 是 `Protocol` 视图 + 可选 `Version` 视图——迭代器处理了重复字段合并、逗号切分、大小写归一（比较时）。`xrtHttpUpgradeWrite(支持列表, 数量, 输出, 容量, &长度)` 反向生成规范值——容量原子性同全库。**应答语义**：升级应答只写"选中的协议"（不是全部支持列表）；`101 Switching Protocols` 响应还必须回显 `Connection: Upgrade`——响应构造由升级层的 WebSocket 变体（下一节）或手工组合完成。

### WebSocket 握手：请求、键与应答校验

WebSocket 升级在通用机制上叠加专用字段：客户端发 `Upgrade: websocket` + `Connection: Upgrade` + `Sec-WebSocket-Key`（16 字节随机数的 Base64）+ 可选 `Sec-WebSocket-Protocol`（子协议列表）；服务端必须回 `Sec-WebSocket-Accept`——**按 RFC 6455 规范计算**：`Base64(SHA1(Key + 魔法串))`。这个计算不是"验证密钥"（Key 本身无秘密），而是**证明服务端真的懂协议**——能算出正确 Accept 的实现必然实现了规范。`xrtWsAccept(Key, 输出, 容量)` 一步完成，输出 28 字符 Base64。

请求构造（`examples/websocket/upgrade` 演示）：`GET 路径 HTTP/1.1` + Host + Upgrade/Connection + Key/Version(+Protocol) 的完整请求头；应答校验（`upgrade_tour` 的 `response check binds key+protocol`）：**校验必须绑定本次的 Key**——对端回显的 Accept 与自己发的 Key 算出的值比对（防中间人替换会话），选中的子协议必须在提议列表内。

### 子协议协商

`Sec-WebSocket-Protocol` 是应用层协议选择（`chat, superchat, binary` 这种）：`xrtWsProtocolSelect(客户端提议, 服务端支持, &选中)` 取**客户端偏好顺序中服务端也支持的第一个**——顺序敏感（客户端的优先级表达）。没有交集返回失败（握手继续但不带子协议，或应用选择拒绝——策略归你）。

### 升级在管线中的位置

```diagram flow
- HTTP 请求到达：RequestParse（第 90 章）
- 判定升级：FieldTokenFind 查 Connection 含 upgrade（第 92 章）+ Upgrade 字段存在
- 提议匹配：UpgradeFieldNext 迭代 ∩ 本地支持 → websocket 命中
- 握手处理：Key 校验/Accept 计算/子协议选择（本节）
- 101 应答：UpgradeWrite + Accept + 选中协议
- 接管：WsStreamAttach 在 101 后接管传输（第 96 章）
```

五层串联——每一层都是前几章的 API。这就是卷九的积木结构：升级是"HTTP 的最后一件事，WebSocket 的第一件事"。

## 示例

### 第一个完整程序：提议迭代与应答生成

下面的程序来自 `examples/http/upgrade/main.c`——重复字段、逗号列表与版本形态的完整处理：

```embed path="examples/http/upgrade/main.c" title="examples/http/upgrade/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/http/upgrade/main.c -lws2_32 -liphlpapi
protocol: h2c
protocol: websocket
protocol: HTTP/2.0
field: websocket, HTTP/2.0
```

**刚才发生了什么。** ① 输入两条 Upgrade 字段（大小写不同的名字、一条含逗号列表）——迭代器合并产出三个提议：`h2c`、`websocket`、`HTTP/2.0`（protocol/version 形态拆开显示）。**没有手写切分**——重复字段、逗号、大小写全部由 `FieldCursorInit/FieldNext` 处理。② `UpgradeWrite` 把"我支持的子集"（websocket、HTTP/2.0）写成规范值 `websocket, HTTP/2.0`——应答只写选中集合，顺序来自提供列表。③ 本例是纯字段层（无网络）——升级语义的单元测试形态；把它夹在 RequestParse 之后、101 响应构造之前，就是完整的升级服务端路径。

### 第二个完整程序：Accept 向量与子协议协商

第二个程序来自 `examples/websocket/handshake/main.c`——RFC 标准向量与偏好协商：

```embed path="examples/websocket/handshake/main.c" title="examples/websocket/handshake/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/handshake/main.c -lws2_32 -liphlpapi
accept=s3pPLMBiTxaQ9kYGzzhZRbK+xOo= protocol=superchat
```

**刚才发生了什么。** ① 输入 nonce `dGhlIHNhbXBsZSBub25jZQ==` 是 RFC 6455 §1.3 的规范样例——`xrtWsAccept` 算出的 `s3pPLMBiTxaQ9kYGzzhZRbK+xOo=` 与标准向量逐字符一致，**对上即实现正确的互操作证明**（第 76 章 RFC 向量的又一应用）。② `ProtocolSelect("chat, superchat", "superchat, binary")` 选中 `superchat`——客户端偏好序（chat 在前）里服务端不认识 chat，轮到 superchat 双方都支持。③ 零网络对象：握手计算层完全独立——测试、代理、调试器都能直接用。配套的 `examples/websocket/upgrade`（构造完整握手请求）与 `upgrade_tour`（四行自检：配置/请求校验/应答绑定/流配置衔接）覆盖握手两侧的全 API 面。

## 契约

- **提议语义**：Upgrade 字段可重复、值可逗号列表、名字大小写不敏感；迭代器产 `protocol/version` 结构化提议。
- **应答语义**：`UpgradeWrite` 只写选中子集；101 响应须回显 `Connection: Upgrade`；容量原子性。
- **Accept 计算**：`Base64(SHA1(Key + RFC 魔法串))`；Key 是 16 字节随机数的 Base64（24 字符）；计算本身无秘密性——它是规范符合性证明。
- **应答校验绑定**：客户端必须验证 Accept 与**自己发出的 Key** 对应、选中协议在提议内——防替换。
- **子协议协商**：客户端偏好顺序 ∩ 服务端支持，取客户端序第一个；无交集的处置归应用策略。
- **升级失败路径**：不匹配则正常 HTTP 响应（不切换）——升级是提议不是要求。
- **裁剪**：`HTTP_UPGRADE` 独立；WebSocket 握手依赖 `CRYPTO_SHA1`（Accept 计算）与 Base64。
- **组合边界**：上游 RequestParse/FieldTokenFind（第 90/92 章）；下游 WsStreamAttach（第 96 章）。

## 避坑

### 坑 1：应答校验不绑定自己的 Key

症状：客户端校验 Accept "格式像"就通过（28 字符 Base64）——中间设备重新握手后连接的是另一个会话。

原因：Accept 的验证是**等值断言**：`服务端回的 Accept == 我用我的 Key 算的值`。格式检查不是校验。

```c bad
if ( looks_like_base64(AcceptField, 28) ) {
	proceed();   /* "格式对"就过：中间人换 Key+Accept 对也能过 */
}
```

```c good
char Mine[XWS_ACCEPT_CAPACITY];
if ( !xrtWsAccept(MyKey, Mine, sizeof(Mine)) ||
		!equal(AcceptField, Mine) ) {
	abort_handshake();   /* 等值断言失败：不是我的会话 */
}
```

### 坑 2：子协议协商用了服务端偏好序

症状：服务端按自己的支持列表顺序回选了 `binary`，但客户端明明更想要 `chat`——协商结果违反客户端优先级，行为分歧。

原因：RFC 6455 规定选择应尊重**客户端提议的顺序**——`ProtocolSelect` 已内建；自己写"遍历服务端列表找第一个共同项"就反了。

```c bad
for ( each server_proto ) {        /* 服务端序：违反协议 */
	for ( each client_proto ) {
		if ( equal ) return server_proto;
	}
}
```

```c good
xstrview Selected;
if ( xrtWsProtocolSelect(ClientOffered, ServerSupported,
		&Selected) ) {
	/* 客户端偏好序 ∩ 服务端支持：协议语义内建 */
}
```

### 坑 3：升级判定只看 Upgrade 字段不看 Connection

症状：收到只有 `Upgrade: websocket` 但 `Connection: keep-alive` 的请求就把连接切换了——客户端根本没要求升级，连接状态错乱。

原因：升级是**两个字段的联合语义**——`Upgrade` 说切什么，`Connection: Upgrade` 说"我要求切换"。只看一边等于替客户端做了决定。

```c bad
if ( has_field(Fields, "Upgrade") ) {
	switch_protocol();   /* Connection 没说 upgrade：误切换 */
}
```

```c good
if ( xrtHttpFieldTokenFind(Fields, iCount,
		XRT_STR_LITERAL("upgrade")) &&       /* Connection 含 upgrade */
		has_upgrade_websocket(Fields) ) {    /* Upgrade 提议 websocket */
	switch_protocol();
}
```

## 练习

### 基础：握手请求构造跑通

跑通 `websocket/upgrade` 与 `handshake` 两个示例；用 OpenSSL 命令行把同 Key 算出的 Accept 与示例对照。验收标准：Accept 与 RFC 向量一致；构造的请求包含全部必备字段。

### 进阶：升级判定中间件

在字段数组上实现 `is_upgrade_request(Fields, 数量, &提议协议)`：Connection 含 upgrade（token 查找）+ Upgrade 提议迭代，返回是否升级与首个提议。对四种输入验证：标准升级、缺 Connection、空 Upgrade 列表、非 websocket 提议。验收标准：四种判定与手推一致；全程用第 92 章族 API 零手切。

### 挑战：握手应答器

实现 `accept_handshake(请求字段, 支持的子协议, 输出, 容量, &长度)`：提取 Key、计算 Accept、协商子协议、生成完整 101 响应（状态行 + Upgrade/Connection/Accept/Protocol 字段）。用 `websocket/upgrade` 构造的请求做端到端验证。验收标准：响应可被 `upgrade_tour` 的应答校验通过；无匹配子协议时按策略降级（不带 Protocol 字段）；容量不足零写入。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 通用升级 | 提议可重复/逗号/大小写不敏感；迭代 FieldCursorInit+FieldNext；应答 UpgradeWrite 只写选中集 |
| 101 语义 | 应答回显 Connection: Upgrade；不匹配则正常 HTTP 响应（提议非要求） |
| WS 专用字段 | Upgrade: websocket + Connection + Sec-WebSocket-Key + Protocol(可选) |
| Accept 计算 | `Base64(SHA1(Key+魔法))`；RFC 向量 s3pPLMBiTxaQ9kYGzzhZRbK+xOo= |
| 应答校验 | 等值断言绑定自己的 Key + 选中协议在提议内——格式检查不是校验 |
| 子协议协商 | 客户端偏好序 ∩ 服务端支持，取客户端序首个；无交集策略归应用 |
| 判定联合 | Connection 含 upgrade 且 Upgrade 有提议——两个字段缺一不可 |
| 管线位置 | HTTP 的最后一件事、WebSocket 的第一件事；五层串联第 89→95 章 |
| 裁剪 | HTTP_UPGRADE 独立；WS 握手依赖 SHA1+Base64 |
