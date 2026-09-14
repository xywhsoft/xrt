---
num: 71
slug: net-interface
title: 网卡接口与本机信息
volume: 卷七 网络
type: practice
lead: 接口快照、名称与索引互转、本机三件套——服务"看清自己跑在什么网络上"的跨平台收口层。
api: net_interface, net
---

## 导读

第 63～70 章解决了"数据怎么流出去"：地址、端口、缓冲、DNS、TCP、UDP、代理。本章补上另一半认知——**看清自己**：这台机器有哪些网卡、每张网卡挂着哪些地址、回环接口叫什么名字、本机首选地址是多少。这些信息在四类场景里必不可少：服务启动日志打印网络环境、管理诊断页展示节点信息、多网卡机器上选择绑定地址、IPv6 链路本地地址必须携带的 scope 标识。它们在原生 API 上的形态是出了名的碎片化——Windows 的适配器 GUID、POSIX 的 `if_nametoindex`、各平台不同的名称体系——XRT 把它们收口在一个零引擎依赖的小模块里：不引入 Socket、线程或 DNS，可以与任何传输层自由组合。

## 引入

假设你在写一个分布式服务的启动流程。运维约定每条启动日志必须带上"主机名 + 本机地址 + 硬件标识"，方便从几千行日志里立刻定位节点；管理端点 `/health` 要展示同一组信息；多网卡机器上，配置文件给了个接口名 `eth0`，你需要查出它的索引交给 IPv6 多播；还有那个经典报错——`fe80::1` 连不通，因为链路本地地址必须写成 `fe80::1%3`（3 是接口索引）才算完整。

这些需求单看都不难，合起来却是跨平台泥潭：Windows 的接口"名字"是一串 GUID，显示名是"以太网"；Linux 的规范名是 `eth0`；回环接口两边命名完全不同。硬件地址在 Windows 用 `GetAdaptersAddresses` 拿，POSIX 用 `ioctl` 拿，长度还未必是 6 字节。自己收口的代价是每个平台一套分支，而 XRT 的接口模块已经把这条收口做完了：一次调用拿到一致的快照结构，名称与索引双向转换，本机三件套（地址/硬件/主机名）各有三层便捷形态。

## 概念

### 接口快照：一次分配，全部视图借用

`xrtNetInterfaces` 生成当前系统接口与地址的**一致快照**：一次 XRT 分配装下接口数组、地址数组、名称和硬件地址，之后所有字段都是借用视图。

```diagram flow
- 枚举：xrtNetInterfaces(&List) 一次系统查询生成快照
- 读取：Items[i].Name / Addresses[j] 全部借用快照存储
- 释放：xrtNetInterfacesFree(&List) 归还全部存储并清零结构
```

快照的价值是**一致性**：枚举过程中接口增删不会让条目互相错位；代价是它只是一个瞬时切面，不订阅变更事件。每接口携带的元数据：`IPv4Index`/`IPv6Index`（Windows 分开维护，POSIX 通常相同）、`Flags` 标志位、`Mtu`、`Name`（规范名）与 `DisplayName`（显示名）两个视图、`HardwareAddress` 原始字节、`Addresses` 地址数组——每条地址带 `PrefixLength` 前缀位数（IPv4 为 0..32，IPv6 为 0..128；平台给不出时是 `XNET_INTERFACE_PREFIX_UNKNOWN`）。

两条使用纪律：接口与地址的顺序由操作系统决定，**不得把顺序当身份**；释放后视图立即失效，读取必须在 `Free` 之前完成。

### 标志位：跨平台稳定交集

`Flags` 只暴露六个跨平台能稳定观察的标志：`UP`（在线）、`RUNNING`（运行中）、`LOOPBACK`（回环）、`BROADCAST`（广播）、`POINT_TO_POINT`（点对点链路）、`MULTICAST`（支持多播）。某个平台没有可靠来源的标志保持未设置——**未设置不等于能力不存在**，只是这个平台观察不到。判断"能不能加入多播组"看 `MULTICAST`，判断"是不是回环"看 `LOOPBACK`，都只是一次位与。

### 名称与索引：双向转换

接口有两套标识。**索引**是内核世界的键（IPv6 scope、多播成员管理都用它），**名称**是人类世界的键（配置文件里写的是它）：

- `xrtNetInterfaceIndex(名称, 族)` 返回索引；索引零不是有效结果，找不到时报告 `XERR_NOT_FOUND`。
- `xrtNetInterfaceName(索引, 族, 缓冲, 容量)` 反向输出规范名称，返回所需长度；空缓冲先测量，缓冲不足安全截断补零并返回完整所需长度——与第 63 章的文本 API 同一套两段式约定。

命名差异被模块吸收：Windows 规范名来自系统适配器名称（形如 GUID），显示名是接口别名；POSIX 两者都是 `if_name`。`Index` 同时接受规范名与显示名，`UNSPEC` 族查询优先 IPv6 索引、再退 IPv4 索引。

### 本机信息三件套：三层形态

"主机名、本机地址、硬件地址"这组诊断信息有三个使用层级，API 也按层级分成三档：

| 层级 | 地址 | 硬件 | 主机名 | 适用场景 |
| --- | --- | --- | --- | --- |
| 结构/字节出参 | `xrtNetLocalAddress` | `xrtNetLocalHardware` | `xrtNetHostName` | 需要二进制形态继续使用（交给连接、比较） |
| 文本两段式 | `xrtNetLocalAddressText` | `xrtNetLocalHardwareText` | —（HostName 本身即文本） | 固定缓冲、无分配环境 |
| String 便捷 | `xrtNetLocalAddressString` | `xrtNetLocalHardwareString` | `xrtNetHostNameString` | 启动日志、诊断页一次打完 |

`xrtNetLocalAddress` 的选择是**确定性偏好查询**：依次偏好 `UP`、`RUNNING`、非回环、非链路本地的接口，条件相同时 `UNSPEC` 优先 IPv6。它适合启动日志与节点报告，但**不是公网出口地址、不代表默认路由**——需要精确选择时遍历 `xrtNetInterfaces` 自己决策。硬件地址文本是大写无分隔符的紧凑 HEX（6 字节 MAC 是 12 个字符），要冒号分隔等展示格式就读原始字节自行格式化。String 层返回 `xrtFree` 释放的字符串，判空即失败（查询语义，不设线程错误）。

### 意外的收获：IPv6 Scope 解析

启用本模块后，第 63 章的 `xrtNetAddrParse` 与 `xrtNetAddrParseEndpoint` 同时接受**数字 Scope 与接口名称 Scope**：`fe80::1%3` 和 `fe80::1%eth0` 都能解析，`[fe80::1%eth0]:8080` 也可以。解析结果只保存稳定的数字接口索引，文本化（`xrtNetAddrText`）保持无系统查询的规范输出——名称会随系统配置变化，索引才是可持久比较的身份。

## 示例

### 第一个完整程序：枚举接口与地址前缀

下面的程序来自 `examples/network/interface/main.c`，遍历本机全部接口，打印名称、双栈索引、MTU 与每条地址的前缀——多网卡巡检的一次性全景：

```embed path="examples/network/interface/main.c" title="examples/network/interface/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/interface/main.c -lws2_32 -liphlpapi
（输出随机器变化：每个接口一行"名称 (显示名) index4=.. index6=.. mtu=.."，
其下每条地址一行"地址/前缀位数"，如 127.0.0.1/8 与 ::1/128）
```

**刚才发生了什么。** 三个要点。① `xrtNetInterfaces` 一次调用完成枚举与快照，`List.Count` 与 `List.Items` 就是全部接口——没有句柄需要逐个关闭。② 接口名称与显示名都是 `xstrview`，用 `%.*s` 配 `Size` 打印；地址复用第 63 章的 `xrtNetAddrText` 转文本，`XRT_NPOS` 表示缓冲不足。③ `PrefixLength` 直接拼在地址后面——`127.0.0.1/8`、`192.168.1.10/24` 这种 CIDR 风格正是从快照得出的真实前缀。输出随机器变化，所以示例头注释的预期输出就是一句说明而非固定文本。

### 第二个完整程序：名称索引往返与本机三件套

第二个程序来自 `examples/network/interface_tour/main.c`，把名称→索引→名称的往返和本机地址、硬件、主机名的两段式查询串成一次体检：

```embed path="examples/network/interface_tour/main.c" title="examples/network/interface_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/interface_tour/main.c -lws2_32 -liphlpapi
iface: loopback index!=0 name=[[GUID]] ok
iface: local addr=[192.168.0.66] hw=00E04C7D7E66 host=[DESKTOP-DQCMPS1] ok
```

**刚才发生了什么。** ① 回环接口先试 POSIX 命名 `loopback4`，失败再试 Windows 显示名 `Loopback Pseudo-Interface 1`——这就是"跨平台命名差异由调用方一次回退吸收"的标准写法。② `InterfaceName` 的两段式：先 `NULL`/0 测量所需长度，再写入并核对两次返回一致；拿到规范名后反查索引做 roundtrip 校验（显示名与规范名不同的平台允许跳过反查）。③ 本机三件套依次验证：`LocalAddress` 结构体出参并核对族与端口为零、`LocalAddressText` 输出必含点号（IPv4 文本特征）、`LocalHardware` 原始字节至少 6 字节、`LocalHardwareText` 恰为字节数两倍的 HEX 字符、`HostName` 非空可写。任何一步失败都走统一的 `Cleanup` 返回非零——把"体检"写成一条失败即中止的链，比散落的 if 可读得多。

### 第三个完整程序：诊断页一览（String 便捷层）

第三个程序来自 `examples/network/local_info/main.c`，用 String 层一次取齐启动日志三件套，硬件地址缺失时回退占位文本：

```embed path="examples/network/local_info/main.c" title="examples/network/local_info/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/local_info/main.c -lws2_32 -liphlpapi
（输出随机器变化：host = 主机名、address = 本机地址、
hardware = 大写紧凑 HEX 或 unavailable）
```

**刚才发生了什么。** String 层的契约在这里体现：返回 `str`（`xrtFree` 释放），`NULL` 表示不可用且**不设线程错误**——所以判空就是全部错误处理。地址与主机名缺失视为环境异常返回 1，而硬件地址在纯回环环境（容器、无网卡虚拟机）可能合法缺失，打印 `unavailable` 继续运行。三个字符串的释放集中在末尾——即使中途失败也全部 `xrtFree`，`xrtFree(NULL)` 是空操作，无需逐个判空。

## 契约

- **快照所有权**：`Interfaces` 一次分配拥有全部存储；字段视图只借用；`InterfacesFree` 归还并清零结构，可对空列表重复调用；失败不修改输出。
- **顺序不担保**：接口与地址顺序由操作系统决定，不得当作稳定身份；身份是索引（数字）与规范名。
- **名称/索引**：索引零非有效结果；`Index` 接受规范名与显示名，`UNSPEC` 优先 IPv6 索引；`Name` 遵循文本 API 两段式（测量→写入，不足安全截断并返回完整所需长度）。
- **便捷层边界**：`LocalAddress` 是确定性偏好查询（UP/RUNNING/非回环/非链路本地，UNSPEC 优先 IPv6），不是公网出口、不代表默认路由、不能替代监听与源地址策略；精确选择遍历 `Interfaces`。
- **文本层**：硬件地址文本为大写紧凑 HEX；String 层 `xrtFree` 释放、`NULL` 即不可用、不设线程错误（查询语义）。
- **错误**：失败经 `xrtGetError()` 报告——`XERR_NOT_FOUND`（接口/地址/硬件不存在）、`XERR_IO`（系统枚举失败）、`XERR_RANGE` + `XNET_ERROR_BUFFER`（缓冲不足），域为 `xrt.net`。
- **依赖与裁剪**：`XRT_MODULE_NET_INTERFACE` 只依赖网络地址基础层，不引入 Socket、Engine、线程、DNS 或容器；`XRT_MODULE_NET_INTERFACE_TEXT` 是叠加 HEX 编解码的独立小层。

## 避坑

### 坑 1：把 LocalAddress 当公网出口或绑定地址

症状：服务在多网卡/NAT 环境下连不上——日志里的"本机地址"是内网某个接口的地址，不是对端可达的地址；或者绑定到它之后容器外无法访问。

原因：`xrtNetLocalAddress` 是**偏好选择**（优先在线、运行中、非回环、非链路本地的单播地址），它不知道路由表，更不知道 NAT 后面的公网出口。拿它当"外部可达地址"或唯一绑定地址，是把诊断信息当成了路由决策。

```c bad
xnetaddr Addr;
xrtNetLocalAddress(&Addr, XNET_FAMILY_UNSPEC);
/* 直接把 Addr 当作"对端可连我的地址"写进服务注册中心 */
register_service(xrtNetAddrText(&Addr, ...));
```

```c good
/* 诊断展示用 LocalAddress；对外注册用配置或连接实测；
   多网卡绑定遍历快照自己决策 */
xnetinterfacelist List;
if ( xrtNetInterfaces(&List) ) {
	for ( size_t i = 0; i < List.Count; i++ ) {
		const xnetinterface* pIf = &List.Items[i];
		if ( (pIf->Flags & XNET_INTERFACE_UP) == 0 ) {
			continue;
		}
		/* 按业务规则挑选：族、前缀、多播能力…… */
	}
	xrtNetInterfacesFree(&List);
}
```

### 坑 2：快照释放后继续使用借用视图

症状：偶发崩溃或读到垃圾字节，常发生在"把接口信息缓存起来"的代码里。

原因：`Items[i].Name.Data`、`Addresses[j]`、`HardwareAddress.Data` 全部指向快照内部存储；`xrtNetInterfacesFree` 之后这些指针全部悬空。快照是值语义的瞬时切面，不是长期持有对象。

```c bad
xnetinterfacelist List;
xrtNetInterfaces(&List);
const xstrview Name = List.Items[0].Name;   /* 借用视图 */
const xnetinterfaceaddress* Addrs = List.Items[0].Addresses;
xrtNetInterfacesFree(&List);                /* 存储已归还 */
printf("%.*s\n", (int)Name.Size, Name.Data); /* 悬空读取 */
```

```c good
xnetinterfacelist List;
if ( xrtNetInterfaces(&List) ) {
	for ( size_t i = 0; i < List.Count; i++ ) {
		const xnetinterface* pIf = &List.Items[i];
		printf("%.*s\n", (int)pIf->Name.Size, pIf->Name.Data);
		/* 需要长期保留的信息在此刻复制成自有存储 */
	}
	xrtNetInterfacesFree(&List);
}
```

### 坑 3：把硬件地址长度写死为 6 字节

症状：在带 8 字节 EUI-64 地址或某些虚拟接口的平台上缓冲截断或长度误判。

原因：MAC-48 的 6 字节只是"典型值"，模块契约明确 `LocalHardware` 返回的是**所需字节数**，不把长度写死；`HardwareAddress` 视图的 `Size` 也由系统决定。

```c bad
uint8 Mac[6];
if ( xrtNetLocalHardware(Mac, sizeof(Mac)) == 0 ) {
	/* 假定 6 字节：更长的地址被静默截断 */
}
```

```c good
size_t iNeed = xrtNetLocalHardware(NULL, 0u);   /* 先测所需 */
if ( (iNeed == 0u) || (iNeed > 16u) ) {
	return 1;
}
uint8 Hardware[16];
size_t iSize = xrtNetLocalHardware(Hardware, iNeed);
/* iSize == iNeed 才是完整地址；HEX 文本长度是 2*iSize */
```

## 练习

### 基础：回环接口报告

枚举接口快照，找出 `Flags` 含 `XNET_INTERFACE_LOOPBACK` 的接口，打印它的名称、双栈索引与全部地址。验收标准：在 Windows 与 Linux（WSL 亦可）上各跑一次，两边都能正确输出回环信息；释放快照后程序正常退出。

### 进阶：多播候选网卡选择器

写一个函数 `uint32 pick_multicast_iface(void)`：遍历快照，按"在线且支持多播、非回环、MTU 最大"挑选接口，返回其 IPv6 索引，找不到返回 0。提示：`Flags` 同时测 `XNET_INTERFACE_UP` 与 `XNET_INTERFACE_MULTICAST`，排除回环用 `XNET_INTERFACE_LOOPBACK`；返回前别忘了 `InterfacesFree`。

### 挑战：节点诊断页

实现 `print_diagnostics(void)`：一次性打印主机名（`HostName` 两段式）、本机地址（`LocalAddressText` 两段式，族用 `UNSPEC`）、硬件地址（`LocalHardware` 原始字节自格式化为冒号分隔的大写 HEX，如 `00:E0:4C:7D:7E:66`）、接口总数与地址总数（来自快照）。验收标准：所有输出零泄漏（用第 6 章的分配统计复核）；硬件地址缺失时输出 `hardware: unavailable` 而不是失败；两段式调用的两次返回值逐个核对。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 快照 | `Interfaces` 一次分配全拥有；视图全借用；`InterfacesFree` 清零可重复；顺序非身份 |
| 接口元数据 | 双栈索引 / Flags / Mtu / Name+DisplayName / HardwareAddress / Addresses+PrefixLength |
| 标志位 | `UP` `RUNNING` `LOOPBACK` `BROADCAST` `POINT_TO_POINT` `MULTICAST`；未设置≠能力不存在 |
| 名称↔索引 | `InterfaceIndex`（名→索引，0=失败）/ `InterfaceName`（索引→名，两段式）；UNSPEC 优先 IPv6 |
| 本机三件套 | 地址（偏好查询，非公网出口）/ 硬件（字节数不定）/ 主机名（本机标签，非 DNS 名） |
| 三层形态 | 出参（二进制继续用）→ Text 两段式（无分配）→ String（日志一次打完，xrtFree+判空） |
| 硬件文本 | 大写紧凑 HEX（6 字节=12 字符）；展示格式自格式化 |
| IPv6 Scope | 模块启用后 `fe80::1%3` 与 `fe80::1%eth0` 均可解析；保存稳定数字索引 |
| 错误 | NOT_FOUND（接口/地址/硬件）/ IO（枚举）/ RANGE+BUFFER（缓冲不足），域 `xrt.net` |
| 裁剪 | `XRT_MODULE_NET_INTERFACE` 零引擎依赖；TEXT 层独立叠加 HEX |
