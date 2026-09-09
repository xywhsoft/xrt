---
num: 85
slug: tls-server
title: TLS 服务端：会话层协议机
volume: 卷八 安全
type: practice
lead: 传输无关的裸协议机：Feed 密文、Drive 推进、Send 队列取航班——SNI 动态选身份与票据签发的完整服务端。
api: tls, tls_verify, tls_server, crypto
---

## 导读

TLS 的对象栈有两层：**会话层**（`xtlssession`——裸协议机，输入密文块、输出密文块，传输完全由调用方决定）与 **Stream 层**（第 86 章——把会话机挂上 TCP 流）。本章讲会话层的服务端侧：`xtlsserverconfig` 配置身份与 ALPN，`xrtTlsServerCreate` 建会话，`Feed*` 喂收到的密文、`xrtTlsServerDrive` 推进状态机、公共 Send 队列取待发航班——传输是什么（内存桥、TCP、串口、自定义 IoT 链路）协议机毫不关心。服务端特有的三件事也在本章：**SNI 动态选择**（`Select` 回调按域名换身份与协议）、**票据签发**（`xrtTlsServerTicket`）与**恢复接受**（`Resume` 回调，PSK+DHE）。本章示例用"内存搬运"当传输，服务端与真实客户端在进程内对打——这也是协议测试的标准姿势。

## 引入

什么时候需要裸协议机而不是 Stream 层？三种真实场景。其一：协议测试——你想在内存里同时驱动客户端与服务端、逐字节检查航班，不碰网络。其二：自定义传输——IoT 串口、蓝牙 BLE、甚至消息队列上跑 TLS，传输字节从哪来由你决定。其三：极致控制——网关要把密文按自定义调度转发，Stream 层的事件模型不合身。

会话层的服务端比客户端多一个"等待"维度：客户端创建即发首航，服务端创建后**等待 ClientHello**——收到什么 SNI、什么 ALPN，可能决定用哪个身份应答（一台服务器服务多个域名的标准场景）。这个"看菜下单"的能力由 `Select` 回调承载；不需要路由就给静态 `Identity`，二者必须有一个——保证任何客户端（包括恢复回退的）都有明确认证路径。

## 概念

### 会话驱动的三步循环

```diagram flow
- Feed：把传输收到的密文交给会话（Feed 借用 / FeedTake 拥有 / FeedRef 引用 / FeedBuffer 零拷贝链）
- Drive：xrtTlsServerDrive 在记录与握手预算内推进状态机
- Send：公共 Send 队列取出待发密文（SendSize/SendFront/SendConsume / SendSpans）
```

三步循环对客户端与服务端**完全同构**（客户端用 `xrtTlsClientDrive`）——掌握一侧另一侧免费。喂入四种形态对应四种所有权（借用/接管/引用/缓冲链——第 64 章的四类追加在这里重演）；Send 队列的 Size/Front/Consume 消费三件与第 64 章缓冲链同一套词汇。TLS 1.3 完整证书航班在 Drive 中依次生成 ServerHello、EncryptedExtensions、Certificate、CertificateVerify、Finished（第 83 章时序），验证客户端 Finished 后**原子切换**到应用 epoch 与 READY。

### 服务端配置与首航 Arena

`xtlsserverconfig` 创建期间借用上下文与静态身份，成功后会话持引用并**深复制 ALPN 列表**。`RequireProtocol = true` 时没有共同 ALPN 明确失败——协议歧义在握手期消灭。首航的扩展表、证书条目、签名输入与临时编码流来自会话内**惰性临时 Arena**：空闲连接与未握手会话不分配 Arena 块，首航完成立即安全清零释放——不为每个连接保留固定握手缓冲，1.2/1.3 复用同一 Arena 与清理路径。内存形态对高连接数服务端是关键契约。

### SNI 动态选择：Select 回调

`Select` 在 ClientHello 严格解析、SNI/ALPN 提取之后、**任何服务端输出生成之前**执行。回调收到请求中的 SNI 与完整 ALPN 负载（仅回调期间借用），`xtlsserverchoice` 初始含静态身份、按服务端偏好算好的协议下标与零值 `Cookie`；回调可替换身份或协议，并写入一个 XRT 不解释的 **64 位宿主路由标识**。`xrtTlsServerCookie` 在成功选择后返回该标识——传输适配层用它把已握手连接关联回"选择身份时的配置代"（比如选择时查的租户配置版本）。约束：上下文只借用到首航完成；不能递归驱动同一会话。`xrtTlsServerName` 返回深复制的 SNI（视图稳定到会话销毁）、`xrtTlsSessionProtocol` 返回最终 ALPN。

### 票据签发与恢复接受

启用 `XRT_FEATURE_TLS_SERVER_RESUME` 后服务端两侧接入恢复：

- **签发**：READY 后 `xrtTlsServerTicket(会话, 不透明ticket, 寿命, &恢复对象)` 把调用方给的 ticket 编码成 NewSessionTicket 发送，并把**对应服务端恢复对象的所有权交给调用方**；`TicketNew` 便捷入口用 32 字节安全随机 ticket 与 86400 秒默认寿命。XRT **不维护进程全局票据缓存**——按租户/容量/过期/持久化自选 Map（第 18 章）、分片缓存或外部存储。发送硬上限在随机数与派生前预检；`XTLS_AGAIN` 时输出对象为 `NULL`、写序号与恢复状态不变——排空后原样重试。
- **接受**：`Resume` 回调收到 SNI、ALPN、不透明 ticket 与混淆年龄，返回借用的不可变 `xtlsresume`（第 87 章的对象契约）；会话加引用后校验版本/套件/SNI/ALPN/有效期/年龄容差。**未找到票据或路由不匹配安全回退完整握手**；但"票据元数据匹配而 binder 错误"是**认证失败**——必须 fatal Alert，不能降级绕过（攻击者拿不到 PSK 就该被拒）。恢复始终保留 ECDHE（PSK+DHE，前向保密不降级），不支持纯 PSK 与 0-RTT。

### TLS 1.2 路径与 KeyUpdate

服务端 1.2 路径要求 EMS，执行 ECDHE 证书完整握手、在 ChangeCipherSpec 边界原子切换 epoch；无恢复、无重新协商、无 KeyUpdate（1.2 的历史决定）。1.3 的主动/被动 KeyUpdate 都遵循"旧 epoch 完整排队、新 epoch 一次提交"——发送背压与分配失败不产生半更新态。

## 示例

### 第一个完整程序：传输无关的服务端对打

下面的程序来自 `examples/tls/server/main.c`——服务端与真实客户端在内存桥上完成握手与双向数据：

```embed path="examples/tls/server/main.c" title="examples/tls/server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/server/main.c -lws2_32 -liphlpapi
usage: server <rsa|p256|p384|ed25519> ...
```

**刚才发生了什么。** ① **身份与验证器装配**：DER 文件读入后 `exampleTlsIdentity` 构造身份（第 82 章的启动期验证）；验证器用最小 `Verify` 回调（对打场景信任自己）。② **双会话创建**：`ClientCreate` 与 `ServerCreate` 各拿一份配置——服务端配 `Identity + Protocols + RequireProtocol`；两边会话都**不碰任何 socket**。③ **内存桥握手**：`exampleTlsHandshake` 是会话层驱动循环的范本——`ClientDrive + ServerDrive` 交替推进，`exampleTlsMove` 把一边 Send 队列的密文逐 Span `Feed` 给对端——"传输"就是这两个循环之间的搬运。接真实网络时把 Move 换成 socket 读写，其余不变。④ **双向数据**：`exampleTlsTransfer` 演示应用明文的 `SessionWrite/Read`（同样经 Drive 与 Move 的循环）；最后 `SessionProtocol` 查询协商出的 ALPN——`RequireProtocol` 保证它非空。⑤ 清理段两会话 Destroy → 验证器与身份 Release → 文件缓冲 free——会话层对象全是"Destroy/Release 一次"的简单生命周期。

### 第二个完整程序：会话层全特性巡检

第二个程序来自 `examples/tls/session_tour/main.c`——公共会话 API 的六行自检：

```embed path="examples/tls/session_tour/main.c" title="examples/tls/session_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I examples/tls -include xrt.h impl.c examples/tls/session_tour/main.c -lws2_32 -liphlpapi
session: handshake via feed-borrow -> both ready ok
session: role/version/cipher/wait/context ok
session: feed-take + feed-ref (release once) ok
session: feed-buffer zero-copy chain ok
session: send spans gathered + plain spans consumed ok
session: close_notify -> peer eof ok
```

**刚才发生了什么。** ① 第 1 行是 server 示例同款的内存桥握手（借用形态 Feed）；第 3 行把另外三种喂入形态逐一验证——Take（接管所有权）、Ref（加引用喂、恰好释放一次）、Buffer（第 64 章缓冲链零拷贝直挂）——四种所有权全覆盖。② 第 2 行查询会话公开事实：角色（客户端/服务端同一对象类型）、协商版本与套件、等待原因（输入还是输出——`XTLS_AGAIN` 的两态分解）、共享上下文。③ 第 5 行验证双向 Span：发送侧 Span 族聚合发送、接收侧明文按 Span 消费——应用层的零拷贝词汇延伸到会话边界。④ 第 6 行走认证关闭：close_notify 发出、对端收到 EOF——第 81 章的关闭协议在会话层的样子。六行合起来就是"会话层这一章的全部 API 在跑"——把它当本章的**可执行速查表**。

## 契约

- **身份强制**：静态 `Identity` 或同步 `Select` 至少一个——未知 SNI 与恢复回退仍有明确认证路径。
- **驱动同构**：Feed 四形态（借用/Take/Ref/Buffer 零拷贝）+ Drive + Send 队列，客户端服务端共享同一套公共会话 API。
- **原子切换**：1.3 在客户端 Finished 验证后原子切应用 epoch 与 READY；1.2 在 CCS 边界原子切换；失败不发布半状态。
- **Arena 惰性**：首航临时内存按需分配、用后安全清零释放；空闲连接零握手缓冲。
- **Select 时机**：ClientHello 严格解析后、任何输出前；SNI/ALPN 仅回调期间借用；Cookie 为 64 位宿主路由标识（XRT 不解释）；上下文借到首航完成；禁止递归驱动。
- **票据签发**：对象所有权转移给调用方；无全局缓存；AGAIN 时状态不变可重试；硬上限预检。
- **恢复接受**：未找到/路由不匹配回退完整握手；binder 错误是认证失败（fatal，不降级）；始终 PSK+DHE；无纯 PSK/0-RTT。
- **1.2 边界**：EMS 必须；无恢复/重协商/KeyUpdate。
- **KeyUpdate**：旧 epoch 完整排队、新 epoch 一次提交；背压与 OOM 无半更新。
- **未实现**：客户端证书认证、0-RTT、异步身份选择；TCP 组合入口属第 86 章 Stream 层。

## 避坑

### 坑 1：Select 回调里驱动会话（递归 Drive）

症状：Select 回调里顺手调 `xrtTlsServerDrive` 想"提前推进"——状态错乱或断言失败。

原因：Select 运行在 Drive 内部（首航生成前）；此时递归 Drive 同一会话等于重入状态机。回调的职责只是"选身份、选协议、写 Cookie"，推进留给外层循环。

```c bad
static void onSelect(xtlsserverchoice* pChoice, ...) {
	pick_identity(pChoice);
	xrtTlsServerDrive(pSession);   /* 重入状态机：未定义行为 */
}
```

```c good
static void onSelect(xtlsserverchoice* pChoice, ...) {
	pick_identity(pChoice);        /* 只做选择 */
	pChoice->Cookie = tenant_generation();  /* 路由标识 */
	/* 推进由外层 feed/drive/send 循环完成 */
}
```

### 坑 2：把"票据未找到"与"binder 错误"都当回退处理

症状：为了"高可用"，Resume 回调查不到票据返回回退、binder 校验失败也回退完整握手——攻击者伪造票据元数据即可绕过 PSK 认证探测。

原因：两类失败的安全语义完全不同。未找到/路由不匹配：客户端可能持旧票据，正常业务路径，回退合理。binder 错误：客户端声称持有某票据的 PSK 却证明不了——要么票据被盗要么就是攻击，必须 fatal Alert。

```c bad
static const xtlsresume* onResume(...) {
	const xtlsresume* pFound = cache_lookup(Ticket);
	if ( (pFound == NULL) || !quick_check(Ticket) ) {
		return NULL;   /* 一律回退：binder 错误也被吞掉 */
	}
	return pFound;
}
```

```c good
/* 回调只负责"找到就给"：
   未找到 → 返回 NULL，会话自动回退完整握手；
   元数据匹配但 binder 错误 → 由状态机发 fatal Alert（协议内置，
   不需要也不允许回调干预）——XRT 已把这条边界实现正确，
   你要做的是不要在回调里伪造"找到" */
```

### 坑 3：高连接数下给每连接配固定握手缓冲

症状：万级空闲连接的服务端内存暴涨——每连接几百 KB 的"预分配握手区"乘以连接数。

原因：误解了会话层的内存模型。首航临时内存是**惰性 Arena**：未握手不分配、握手完立即清零释放；需要长期驻留的只有会话状态本身。

```c bad
/* 自行给每个连接 malloc 一块"握手工作区"挂着 */
struct conn { uint8 HandshakeArena[262144]; ... };  /* 全部白占 */
```

```c good
/* 直接创建会话，握手临时内存由惰性 Arena 管理；
   闲置连接近零握手内存，容量规划只看活跃握手数 */
pSession = xrtTlsServerCreate(&Config, pPool);
```

## 练习

### 基础：对打全流程

用 OpenSSL 生成 Ed25519 自签证书导出 DER，跑通 `server ed25519 cert.der key.der`——确认输出 `ready, protocol=h2, client=ping, server=pong`（ALPN 首选项命中）。把 `RequireProtocol` 换 false 再改 Protocols 只留 `h2`，观察协议下标的选择变化。验收标准：两种配置的协商协议与手推一致。

### 进阶：双域名 SNI 路由

实现 `Select` 回调：准备两套身份（自签两个域名的证书），按 ClientHello 的 SNI 选择对应身份；Cookie 写入"租户代次"并在 READY 后用 `xrtTlsServerCookie` 读回对比。验收标准：两个 SNI 各自拿到正确证书；Cookie 读写一致；未知 SNI 走你定义的默认策略（静态身份或明确失败）。

### 挑战：内存桥上的分片压力测试

把 `exampleTlsMove` 的"整 Span 搬运"改成"每次最多搬 7 字节"的随机分片——模拟恶劣传输。统计握手完成的 Drive 轮数上限。验收标准：任意分片大小（1 到整条）握手都成功（协议机支持跨记录重组——第 83 章）；最坏轮数有界并可解释（航班大小 ÷ 分片大小量级）；无内存增长（第 6 章统计）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 会话层定位 | 裸协议机：Feed 密文 / Drive 推进 / Send 队列取航班，传输无关 |
| 驱动循环 | Feed×4（借用/Take/Ref/Buffer）→ Drive → Send（Size/Front/Consume/Spans） |
| 服务端配置 | Identity 或 Select 必须一个；ALPN 深复制；RequireProtocol 歧义即失败 |
| 首航 Arena | 惰性分配、用后清零释放；空闲连接零握手缓冲；1.2/1.3 共用路径 |
| Select | ClientHello 解析后、输出前；SNI/ALPN 回调期借用；Cookie=64 位路由标识 |
| 查询 | ServerName（深复制 SNI）/ SessionProtocol（最终 ALPN）/ ServerCookie |
| 票据签发 | Ticket/TicketNew；对象所有权转移；无全局缓存；AGAIN 状态不变 |
| 恢复接受 | 未找到回退完整握手；binder 错误 fatal 不降级；PSK+DHE；无 0-RTT |
| 1.2 边界 | EMS 必须；无恢复/重协商/KeyUpdate |
| 同构性 | 客户端与服务端共享公共会话 API——学一侧得两侧 |
