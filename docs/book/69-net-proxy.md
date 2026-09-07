---
num: 69
slug: net-proxy
title: 代理：SOCKS5 与 HTTP CONNECT
volume: 卷七 网络
type: practice
lead: 代理拨号（托管全链回收）、SOCKS5/HTTP CONNECT 增量握手协议、地址类型与认证形态。
api: proxy, net, tcp
---

## 导读

proxy 模块把"经代理建立连接"工程化：**代理拨号**（`xnetproxydial`——DNS+握手+连接的托管链：失败时已建资源全部自动回收）；**SOCKS5**（greeting/auth/CONNECT 的增量握手状态机——纯协议层）；**HTTP CONNECT**（方法行+头+2XX 确认）；**地址类型**（IPv4/IPv6/域名三种目标表示——代理协议的特色：目标可以是域名——代理端代为解析）；认证形态（无认证/用户密码）。proxy_dial_http_connect 范例是完整实战位。

## 引入

企业网络或合规需求下，出站连接要走代理：客户端 → 代理 → 目标。手工实现是三段式苦工：连代理（地址解析+TCP 建立）→ 握手协议（SOCKS5 两往返或 CONNECT 一往返——二进制/文本两种编码各自的状态机）→ 成功后才拿到"到目标的通道"。任何一段失败——已建的 TCP、已发的握手——全部要正确回收（部分状态清理是 bug 温床）。

代理模块的答案：`xnetproxydial` **托管全链**——拨号（代理地址）→ 握手（协议状态机）→ 交付（到目标的 Stream）；**失败自动回收**（"托管"的语义——金标准章 tcp_dial 范例的同款承诺在代理场景的复现）。协议细节（报文编码/状态机/地址类型）封装在模块内——使用者声明"经什么代理、到什么目标"，其余交给模块。

## 概念

### 代理拨号形态

| 形态 | 入口 | 说明 |
| --- | --- | --- |
| 配置式 | proxy 配置+拨号 | 类型/地址/认证全配置 |
| 增量握手 | 分步生成/消费报文 | 纯协议层（socks5 范例） |
| HTTP CONNECT | 方法行+确认 | 文本协议位 |

**配置式**是最常用形态：声明代理（类型+地址+可选认证）与目标（xnetaddr 或域名）——一次拨号交付 Stream。**增量握手**面向特殊场景（自定义握手插入、测试/教学）：greeting/CONNECT 报文分步生成、应答分步消费——状态机裸露但可控。

### SOCKS5 增量握手

```diagram flow
- greeting：05 01 00（版本5/1方法/无认证）→ 应答选中方法
- auth（可选）：用户密码子协商（RFC 1929）
- CONNECT：05 01 00 + 地址类型（01 v4/03 域名/04 v6）+ 目标 + 端口
- 应答：05 00（成功）+ 绑定地址——此后通道直通目标
```

**域名目标**是代理协议的精髓（socks5 范例的 `03 0E 6F 72 69 ...`——03=域名类型、0E=14 字节、后随域名字节）：目标以域名交给代理——**代理端解析**（客户端不经手目标 DNS——内网域名/防泄漏的场景价值）。地址三类型 + 端口构成统一的 `xnetproxyendpoint`。

### HTTP CONNECT

文本协议位：`CONNECT host:port HTTP/1.1` + Host 头（+认证头）→ 代理回 `2xx` 确认 → 通道直通。与 SOCKS5 的选择常由环境定（防火墙策略/现网代理类型）——XRT 两者都备（http_connect 与 socks5 范例对称）。

### 认证与安全

**认证**：SOCKS5 无认证/用户密码（RFC 1929）；HTTP Basic/自定义头。**通道安全**：代理本身不加密——TLS over 代理（CONNECT 后跑 TLS 握手——第 80 章组合）；**代理链**：理论上可串联（拨号经代理 A 到代理 B 到目标——配置层组合）。

### 托管语义

`xnetproxydial` 的托管 = 金标准 tcp_dial 的代理版：DNS（代理域名）→ TCP（到代理）→ 握手（协议）→ 交付；任何阶段失败——`xnetproxydialstate` 状态对象记录走到哪、已建资源全回收；成功——Stream 生命周期与直连一致（后续 TLS/HTTP 无感差异——**代理对上层透明**）。

## 示例

### 完整程序：SOCKS5 增量握手

来自仓库范例 `examples/network/proxy_socks5/main.c`——纯协议层的报文生成：

```embed path="examples/network/proxy_socks5/main.c" title="examples/network/proxy_socks5/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/proxy_socks5/main.c -lws2_32 -liphlpapi
greeting: 05 01 00
connect: 05 01 00 03 0E 6F 72 69 ...
```

**刚才发生了什么。** ① greeting 报文 `05 01 00`——版本 5、一种方法（00=无认证）——三字节上线路的最小问候。② CONNECT 报文 `05 01 00 03 0E 6F 72 69 ...`——05 版本/01 CONNECT/00 保留/03 域名类型/0E 十四字节/域名首字节（ori...——范例的域名目标）——**地址类型的二进制编码**直接可见。③ 打印的是"可直接上线路的二进制"——增量握手把协议层的字节流显式化（教学/调试位）；生产代码用配置式拨号（协议层封装）。proxy_tour 范例是全接口巡礼。

### 完整程序：HTTP CONNECT 实战

来自 `examples/network/proxy_dial_http_connect/main.c`——完整代理拨号链：

```embed path="examples/network/proxy_dial_http_connect/main.c" title="examples/network/proxy_dial_http_connect/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/proxy_dial_http_connect/main.c -lws2_32 -liphlpapi
usage: proxy_dial <proxy-host> ...
```

**刚才发生了什么。** ① 无参数时打印用法——本范例设计为对真实代理的实战验证（与纯协议层的 socks5 范例对照——一个裸协议、一个全链）。② 有参数时的完整链：代理地址解析 → TCP 连接 → `CONNECT host:port` 发送 → 2xx 确认等待 → 成功即通道 → 目标通信（回显验证）。③ 失败路径的托管回收——任何一步失败资源全清（"managed" 语义的实证位）。proxy_dial 范例是 SOCKS5 版的同构实战——两协议对称。

## 契约

- **托管全链**：DNS+TCP+握手+交付一次完成；失败已建资源全回收——tcp_dial 托管语义的代理版。
- **透明通道**：成功后 Stream 与直连无差——TLS/HTTP 上层无感。
- **域名目标**：代理端解析（客户端不经手目标 DNS）——地址三类型统一编码。
- **双协议**：SOCKS5（二进制增量握手）/ HTTP CONNECT（文本）——环境决定、XRT 全备。
- **认证可选**：无认证/用户密码/自定义头——配置声明。
- **通道安全自理**：代理不加密——TLS over 代理（卷八组合位）。

### 从示例到工程：代理的三个宿主

**企业出口**（合规主场）：内网服务的出站经指定代理——配置声明代理池（第 42 章环境变量或配置文件）、代理可用性探针维护可用表（挑战练习）、拨号走托管链。**多区域访问**：按目标选代理（内网域名走 A、公网走 B）——目标路由表+代理映射；域名目标类型在此有额外价值（代理端解析——内网域名由正确的代理解析）。**隐私/测试基建**：本地 SOCKS5 转发器（进阶练习的完全体）做流量观察/注入/回放——测试基建的代理位。三宿主共同配置面：代理池管理（可用性/排序）、目标路由（哪类目标走哪个代理）、失败策略（重试/降级直连/快速失败）——这一层 xhttp 扩展库有内置（卷十）。

### 代理链上的 TLS：卷八组合预告

代理通道不加密——敏感数据必须 TLS over 代理。组合形态预告（卷八展开）：CONNECT/SOCKS5 交付的 Stream 与直连 Stream 无差 → TLS 层直接套上（`xtlssession` 包装）→ HTTP 再套（卷九）——**三层嵌套每层无感下层是代理还是直连**。这个透明性不是偶然——第 66 章 Stream 契约（与直连一致的生命周期）是它的结构保证。反向组合也存在：代理本身可以是 TLS（代理服务器要求 TLS 连接——代理地址的 TLS 拨号——配置层的扩展位）。带着这个组合图读卷八——代理是"传输中间层"的原型，透明性是中间层的设计范式。

### 一个历史注脚：SOCKS 的命名与演化

SOCKS 的名字来自 "SOCKetS"（套接字的复数戏称）——1992 年的防火墙穿透需求起源。演化脉络：SOCKS4（仅 TCP+v4）→ SOCKS4a（域名目标）→ SOCKS5（RFC 1928——v6/UDP/认证框架）——代理协议的每一步都对着前版的真实缺口。HTTP CONNECT 从反向代理的需求侧生长（早期 HTTPS 穿透）——两协议双雄并存的格局是历史叠加。理解脉络的价值：**代理协议的每一特性（域名目标/认证/UDP 转发）都对应一类真实部署需求**——读协议时按需求读而不是按字节读。XRT 的 proxy 模块覆盖两协议的客户端面（服务端代理属部署基建不在内核——进阶练习的本地代理是教学形态）。

## 避坑

### 坑 1：裸 TCP 到代理当通道用

症状：直连目标失败或被拒——只连了代理 TCP 没做协议握手（或没等 CONNECT 确认就发数据——代理把应用数据当协议报文丢弃/报错）。

原因：TCP 连上代理只是**运输建立**——通道要协议握手（SOCKS5 两往返/CONNECT 一确认）确认后才存在。

```c bad
xnetstream* S = xrtNetStreamConnect(Engine, ProxyAddr, 1, NULL, NULL, NULL);
xrtNetStreamSend(S, TargetData, Size);   /* 没握手——代理不认识这数据 */
```

```c good
/* 配置式拨号——握手状态机封装在模块内 */
xnetproxydialconfig Dial;
Dial.Type = XNET_PROXY_SOCKS5;
Dial.Proxy = ProxyAddr;
Dial.Target = Target;   /* xnetproxyendpoint（含域名形态） */
xnetstream* S = ManagedDial(Engine, &Dial);   /* 握手完成才交付 */
```

### 坑 2：代理通道上的明文凭证

症状：安全审计——代理链路上的用户名密码/业务数据明文可捕获（抓包可见）。

原因：代理协议不加密——SOSCK5 认证/Business 数据在代理链路明文。

```c bad
/* SOCKS5 用户密码认证+明文业务——代理管理员与链路嗅探者全见 */
Dial.Auth = &PlainAuth;
SendSecret(S, Data);
```

```c good
/* 敏感数据走 TLS over 代理——CONNECT 后先 TLS 握手（卷八），认证也在 TLS 内 */
xnetstream* S = ManagedDial(Engine, &Dial);
/* S 上跑 TLS（第 80 章组合形态）——通道内容加密 */
```

## 练习

### 基础：报文生成复现

复现 socks5 范例的 greeting/CONNECT 生成；对三种地址类型（v4/域名/v6）各生成一个 CONNECT——十六进制对照验证。

### 进阶：本地回环代理

起一个最小的 SOCKS5 回环代理（greeting 应答+CONNECT 总是成功+数据直通）——用 XRT 客户端经它连回环服务。握手双方都自己写——协议在中间显形。

### 挑战：代理可用性探针

实现代理池探针：N 个代理并发拨号测试目标 → 记录延迟/成功率/失败分类（协议错/超时/拒认证）→ 可用表排序。验收标准：探针超时与重试策略可配；失败原因链（第 4 章）完整进日志；可用表定期刷新（第 41 章周期）；对不可用代理的资源回收零泄漏。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 托管拨号 | DNS+TCP+握手+交付一次完成；失败全回收 |
| 透明通道 | 成功后 Stream 与直连无差——上层无感 |
| SOCKS5 | greeting→[auth]→CONNECT 二进制增量握手 |
| CONNECT | 文本方法行+2xx 确认——防火墙环境常备 |
| 地址类型 | 01 v4 / 03 域名（代理端解析）/ 04 v6 |
| 认证 | 无认证/用户密码/自定义头——配置声明 |
| 通道安全 | 代理不加密——TLS over 代理（卷八） |
