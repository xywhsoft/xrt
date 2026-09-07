# 网络接口

## 类型与常量

### `xnetinterfaceflag`

接口标志只表达跨平台能够稳定观察的状态和能力。

```c
typedef enum xnetinterfaceflag {
	XNET_INTERFACE_UP = 0x0001,
	XNET_INTERFACE_RUNNING = 0x0002,
	XNET_INTERFACE_LOOPBACK = 0x0004,
	XNET_INTERFACE_BROADCAST = 0x0008,
	XNET_INTERFACE_POINT_TO_POINT = 0x0010,
	XNET_INTERFACE_MULTICAST = 0x0020
} xnetinterfaceflag;
```

| 值 | 语义 |
|---|---|
| `XNET_INTERFACE_UP` | 接口在线 |
| `XNET_INTERFACE_RUNNING` | 运行中 |
| `XNET_INTERFACE_LOOPBACK` | 回环接口 |
| `XNET_INTERFACE_BROADCAST` | 广播 |
| `XNET_INTERFACE_POINT_TO_POINT` | 点对点链路 |

### `xnetinterfaceaddress`

接口地址不带传输端口，PrefixLength 为网络前缀位数。

```c
typedef struct xnetinterfaceaddress {
	xnetaddr Address;
	uint8 PrefixLength;
} xnetinterfaceaddress;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Address` | `xnetaddr` | 地址 |
| `PrefixLength` | `uint8` | PrefixLength |

### `xnetinterface`

名称、硬件地址和地址数组均借用所属接口快照。

```c
typedef struct xnetinterface {
	uint32 IPv4Index;
	uint32 IPv6Index;
	uint32 Flags;
	uint32 Mtu;
	xstrview Name;
	xstrview DisplayName;
	xbytesview HardwareAddress;
	const xnetinterfaceaddress* Addresses;
	size_t AddressCount;
} xnetinterface;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `IPv4Index` | `uint32` | IPv4Index |
| `IPv6Index` | `uint32` | IPv6Index |
| `Flags` | `uint32` | 标志位 |
| `Mtu` | `uint32` | 最大传输单元 |
| `Name` | `xstrview` | 名称 |
| `DisplayName` | `xstrview` | DisplayName |
| `HardwareAddress` | `xbytesview` | HardwareAddress |
| `Addresses` | `const xnetinterfaceaddress*` | Addresses |
| `AddressCount` | `size_t` | AddressCount |

### `xnetinterfacelist`

接口列表拥有 Items 以及所有条目借用的存储。

```c
typedef struct xnetinterfacelist {
	const xnetinterface* Items;
	size_t Count;
} xnetinterfacelist;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Items` | `const xnetinterface*` | 元素数组 |
| `Count` | `size_t` | 数量 |

## 分层

`XRT_FEATURE_NET_INTERFACE` 只依赖网络地址基础层，公开头文件为
`<xrt/net_interface.h>`。它负责接口名称与索引互转、接口快照、MTU、硬件地址、
接口标志、单播地址和前缀长度，不引入 Socket、Engine、线程、DNS 或容器。

启用该模块后，`xrtNetAddrParse` 和 `xrtNetAddrParseEndpoint` 同时接受 IPv6
数字 Scope 与接口名称 Scope，例如 `fe80::1%eth0` 和
`[fe80::1%eth0]:8080`。解析结果仍只保存稳定的数字接口索引，
`xrtNetAddrText` 因此保持无系统查询的规范数字输出。

## 名称与索引

```c
uint32 xrtNetInterfaceIndex(cstr sName, xnetfamily Family);
size_t xrtNetInterfaceName(
	uint32 iIndex,
	xnetfamily Family,
	char* sName,
	size_t iCapacity
);
```

Windows 的规范名称来自系统适配器名称，显示名称是 UTF-8 接口别名；POSIX 两者
都是 `if_name`。`xrtNetInterfaceIndex` 同时接受规范名称和显示名称。
Windows 分别保存 IPv4 与 IPv6 索引；POSIX 两者通常相同。`UNSPEC` 查询优先
IPv6 索引，再使用 IPv4 索引。

索引零不是有效结果。找不到名称或索引时分别报告
`XNET_ERROR_INTERFACE_INDEX` 与 `XNET_ERROR_INTERFACE_NAME`。
`xrtNetInterfaceName` 与其他文本 API 一样支持空输出测量；缓冲不足时安全截断、
补零、返回完整所需长度并报告 `XNET_ERROR_BUFFER`。

## 接口快照

```c
typedef struct xnetinterfaceaddress {
	xnetaddr Address;
	uint8 PrefixLength;
} xnetinterfaceaddress;

typedef struct xnetinterface {
	uint32 IPv4Index;
	uint32 IPv6Index;
	uint32 Flags;
	uint32 Mtu;
	xstrview Name;
	xstrview DisplayName;
	xbytesview HardwareAddress;
	const xnetinterfaceaddress* Addresses;
	size_t AddressCount;
} xnetinterface;

typedef struct xnetinterfacelist {
	const xnetinterface* Items;
	size_t Count;
} xnetinterfacelist;

bool xrtNetInterfaces(xnetinterfacelist* pList);
void xrtNetInterfacesFree(xnetinterfacelist* pList);
```

快照使用一次 XRT 分配保存接口数组、地址数组、名称和硬件地址。所有视图都借用
快照，释放后立即失效；`Free` 可对空列表重复调用。失败不会修改输出。
接口和地址顺序由操作系统决定，调用方不得把顺序当作身份。

`PrefixLength` 为 IPv4 的 `0..32` 或 IPv6 的 `0..128`；平台不能可靠提供连续
掩码时使用 `XNET_INTERFACE_PREFIX_UNKNOWN`。地址端口恒为零。链路本地 IPv6
地址缺少原生 Scope 时，快照使用该接口的 IPv6 索引补齐。

标志只暴露跨平台稳定交集：`UP`、`RUNNING`、`LOOPBACK`、`BROADCAST`、
`POINT_TO_POINT` 和 `MULTICAST`。某个平台没有可靠来源的标志保持未设置，
不能把未设置解释为能力一定不存在。

## 本机信息便捷层

```c
bool xrtNetLocalAddress(xnetaddr* pAddress, xnetfamily Family);
size_t xrtNetLocalHardware(void* pAddress, size_t iCapacity);
size_t xrtNetHostName(char* sName, size_t iCapacity);
```

`xrtNetLocalAddress` 从接口快照中选择适合启动日志、节点报告和诊断页展示的
单播地址。它依次偏好 `UP`、`RUNNING`、非回环、非链路本地接口；条件相同时
`UNSPEC` 优先 IPv6。该结果不是公网出口地址，也不代表系统默认路由，不能直接
替代服务监听、客户端源地址或多网卡路由策略。需要精确选择时应遍历
`xrtNetInterfaces`。

`xrtNetLocalHardware` 返回原始硬件地址，不把地址长度写死为 6 字节。空输出和零
容量只查询所需大小；缓冲不足时不写入，返回完整所需大小并报告
`XNET_ERROR_BUFFER`。没有带有效硬件地址的接口时报告
`XNET_ERROR_INTERFACE_HARDWARE`。

`xrtNetHostName` 使用动态平台缓冲读取完整主机名。它支持空输出测量；缓冲不足时
安全截断、补零并返回完整所需长度。主机名只是本机标签，不保证能够通过 DNS
解析，也不等于 HTTP Host、TLS SNI 或服务绑定名。

## 文本便捷函数

启用 `XRT_FEATURE_NET_INTERFACE_TEXT` 后可以直接使用：

```c
size_t xrtNetLocalAddressText(
	xnetfamily Family,
	char* sAddress,
	size_t iCapacity
);
str xrtNetLocalAddressString(xnetfamily Family);
size_t xrtNetLocalHardwareText(char* sAddress, size_t iCapacity);
str xrtNetLocalHardwareString(void);
str xrtNetHostNameString(void);
```

`String` 函数返回由 `xrtFree` 释放的字符串。硬件地址文本使用大写、无分隔符的
紧凑 HEX；需要冒号、短横线或其他展示格式时应读取原始字节后自行格式化。
文本层依赖 `NET_INTERFACE` 与 `CODEC_HEX`，基础接口快照和二进制便捷查询不会
因此引入 HEX 编解码。

## 裁剪与验证

只选择 `XRT_MODULE_NET_INTERFACE` 时不会带入 TCP、UDP、异步运行时或 HTTP。
`XRT_MODULE_NET_INTERFACE_TEXT` 是独立裁剪的小型易用层。
模块测试只枚举本机系统接口并解析本机接口名称，不发送报文，也不访问外部地址。
底层示例位于 `examples/network/interface/main.c`，便捷层示例位于
`examples/network/local_info/main.c`。

## 模块契约：错误

本文件与 [net.md](net.md) 共享 `include/xrt/net_interface.h` 的错误体系；失败经 `xrtGetError()` 报告：

| 域/种类 | 触发场景 |
|---|---|
| `XERR_ARGUMENT` / `XERR_RANGE` | 参数与索引越界 |
| `XERR_NOT_FOUND` | 指定接口或地址不存在 |
| `XERR_IO` | 系统接口枚举失败 |
| `xrt.net` / `XNET_ERROR_INTERFACE_*` | 接口地址、硬件等平台查询失败 |

## API

### `xrtNetInterfaceIndex`
把规范名称或显示名称转换为接口索引。`UNSPEC` 优先返回 IPv6 索引，再返回 IPv4 索引；失败返回零。

```c
uint32 xrtNetInterfaceIndex(cstr sName, xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sName` | 输入 | 非空 | 规范名或系统显示名（Windows 为显示名） |
| `Family` | 输入 | 地址族 | `UNSPEC` 两族都匹配，优先 IPv6 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非 `0` | 接口索引 | — |
| `0` | 未找到或系统查询失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_NOT_FOUND` + `XNET_ERROR_INTERFACE_INDEX` — 没有该名称的接口
- `XERR_IO` + `XNET_ERROR_INTERFACE_INDEX` — 系统接口查询失败

#### 范例

[network/interface_tour · 往返](../../examples/network/interface_tour/main.c) · 跨平台回环命名差异

```c
iIndex = xrtNetInterfaceIndex("loopback4", XNET_FAMILY_IPV4);
if ( iIndex == 0u ) {
	iIndex = xrtNetInterfaceIndex(
		"Loopback Pseudo-Interface 1", XNET_FAMILY_IPV4);
}
```

### `xrtNetInterfaceName`
输出指定接口索引的规范名称并返回所需长度。`UNSPEC` 同时匹配 IPv4 与 IPv6 索引；空输出可查询所需大小。

```c
size_t xrtNetInterfaceName(
	uint32 iIndex,
	xnetfamily Family,
	char* sName,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iIndex` | 输入 | 非零 | 接口索引 |
| `Family` | 输入 | 地址族 | 指定匹配哪族索引；`UNSPEC` 两族都试 |
| `sName` | 输出 | 可空 | 输出缓冲；空指针表示只查所需大小 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零字节） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 不含结尾零的所需长度；缓冲足够时已写入 | — |
| `0` | 索引未找到 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_NOT_FOUND` + `XNET_ERROR_INTERFACE_NAME` — 没有该索引的接口
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — 缓冲不足：不写入，返回完整所需大小

#### 范例

[network/interface_tour · 往返](../../examples/network/interface_tour/main.c) · 两段式：先查大小再写入

```c
iNeed = xrtNetInterfaceName(iIndex, XNET_FAMILY_IPV4, NULL,
	0u);
if ( (iNeed == 0u) || (iNeed >= sizeof(sName)) ) {
	goto Cleanup;
}
iSize = xrtNetInterfaceName(iIndex, XNET_FAMILY_IPV4, sName,
	sizeof(sName));
```

### `xrtNetInterfaces`
创建当前系统接口、地址和元数据的一致快照。

```c
bool xrtNetInterfaces(xnetinterfacelist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输出 | 非空 | 接收拥有型快照；用后 `InterfacesFree` 释放 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 快照已写入（`Items`/`Count` 与每接口地址数组） | — |
| `false` | 参数非法或系统查询/分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pList` 为空
- `XERR_IO` — 系统接口枚举失败
- 内存分配失败 — 快照存储分配失败

#### 范例

[network/interface · 枚举](../../examples/network/interface/main.c) · 遍历接口与地址前缀

```c
if ( !xrtNetInterfaces(&List) ) {
	return 1;
}
for ( i = 0; i < List.Count; i++ ) {
	const xnetinterface* pInterface = &List.Items[i];
```

### `xrtNetInterfacesFree`
释放接口快照拥有的全部存储并清零。

```c
void xrtNetInterfacesFree(xnetinterfacelist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 允许空指针 | `Interfaces` 产物；释放后清零可复用 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[network/interface · 枚举](../../examples/network/interface/main.c) · 用后释放

```c
xrtNetInterfacesFree(&List);
return 0;
}
```

### `xrtNetLocalAddress`
选择一个适合本机诊断的单播地址。这是确定性偏好查询，不代表公网出口、默认路由或服务监听策略。

```c
bool xrtNetLocalAddress(
	xnetaddr* pAddress,
	xnetfamily Family
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddress` | 输出 | 非空 | 接收地址；端口为零 |
| `Family` | 输入 | `IPV4`/`IPV6`/`UNSPEC` | 期望族；`UNSPEC` 按平台偏好选择 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写入偏好地址 | — |
| `false` | 参数非法或无可用地址 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pAddress` 为空或 `Family` 非法
- `XERR_NOT_FOUND` + `XNET_ERROR_INTERFACE_ADDRESS` — 没有可用的本机单播地址

#### 范例

[network/interface_tour · 本机信息](../../examples/network/interface_tour/main.c) · 结构体出参形态

```c
if ( !xrtNetLocalAddress(&Address, XNET_FAMILY_IPV4) ||
	(Address.Family != XNET_FAMILY_IPV4) ||
	(Address.Port != 0u) ) {
	goto Cleanup;
}
```

### `xrtNetLocalAddressText`
输出首选本机地址文本并返回不含结尾零字节的所需长度。

```c
size_t xrtNetLocalAddressText(
	xnetfamily Family,
	char* sAddress,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Family` | 输入 | 地址族 | 期望族 |
| `sAddress` | 输出 | 可空 | 输出缓冲；空指针表示只查所需大小 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零字节） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 不含结尾零的所需长度；缓冲足够时已写入 | — |
| `XRT_NPOS` | 无可用本机地址 | 错误经 `xrtGetError()` 报告（`NOT_FOUND` 族） |

#### 错误

- 同 `xrtNetLocalAddress` — 地址选择失败时透传（`NOT_FOUND` + `INTERFACE_ADDRESS`）

#### 范例

[network/interface_tour · 本机信息](../../examples/network/interface_tour/main.c) · 两段式文本输出

```c
iNeed = xrtNetLocalAddressText(XNET_FAMILY_IPV4, NULL, 0u);
if ( (iNeed == 0u) || (iNeed >= sizeof(sText)) ) {
	goto Cleanup;
}
iSize = xrtNetLocalAddressText(XNET_FAMILY_IPV4, sText,
	sizeof(sText));
```

### `xrtNetLocalAddressString`
分配并返回首选本机地址文本。

```c
str xrtNetLocalAddressString(xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Family` | 输入 | 地址族 | 期望族 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | `xrtAlloc` 分配的文本，调用方 `xrtFree` | — |
| `NULL` | 无可用地址或分配失败 | 不设置线程错误（查询语义） |

#### 错误

- 无 — `NULL` 表示本机无可用地址或分配失败，调用方判空即可

#### 范例

[network/local_info · 一览](../../examples/network/local_info/main.c) · 启动日志三件套之一

```c
str sAddress = xrtNetLocalAddressString(XNET_FAMILY_UNSPEC);
str sHost = xrtNetHostNameString();
str sHardware = xrtNetLocalHardwareString();
```

### `xrtNetLocalHardware`
输出首选活动接口的原始硬件地址并返回所需字节数。空输出可查询大小；缓冲不足时不写入并报告完整所需大小。

```c
size_t xrtNetLocalHardware(
	void* pAddress,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddress` | 输出 | 可空 | 输出缓冲；空指针表示只查所需大小 |
| `iCapacity` | 输入 | — | 缓冲容量（字节） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 所需字节数（典型 MAC 为 6）；缓冲足够时已写入 | — |
| `0` | 无可用硬件地址 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_NOT_FOUND` + `XNET_ERROR_INTERFACE_HARDWARE` — 没有可用的本机硬件地址
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — 缓冲不足：不写入，返回完整所需大小

#### 范例

[network/interface_tour · 本机信息](../../examples/network/interface_tour/main.c) · 原始字节两段式

```c
iNeed = xrtNetLocalHardware(NULL, 0u);
if ( (iNeed < 6u) || (iNeed > sizeof(Hardware)) ) {
	goto Cleanup;
}
iSize = xrtNetLocalHardware(Hardware, sizeof(Hardware));
```

### `xrtNetLocalHardwareText`
输出首选接口硬件地址的大写紧凑 HEX 文本。

```c
size_t xrtNetLocalHardwareText(
	char* sAddress,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sAddress` | 输出 | 可空 | 输出缓冲；空指针表示只查所需大小 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零字节） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 不含结尾零的所需长度（6 字节 MAC 为 12 字符） | — |
| `0` | 无可用硬件地址 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_NOT_FOUND` — 无可用硬件地址（透传 `LocalHardware`）
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — 缓冲不足：不写入，返回完整所需大小

#### 范例

[network/interface_tour · 本机信息](../../examples/network/interface_tour/main.c) · 6 字节 MAC → 12 个 HEX 字符

```c
iNeed = xrtNetLocalHardwareText(NULL, 0u);
if ( (iNeed < 12u) || (iNeed >= sizeof(sText)) ) {
	goto Cleanup;
}
iSize = xrtNetLocalHardwareText(sText, sizeof(sText));
```

### `xrtNetLocalHardwareString`
分配并返回首选接口硬件地址的大写紧凑 HEX 文本。

```c
str xrtNetLocalHardwareString(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 取首选活动接口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | `xrtAlloc` 分配的文本，调用方 `xrtFree` | — |
| `NULL` | 无可用硬件地址或分配失败（如纯回环环境） | 不设置线程错误（查询语义） |

#### 错误

- 无 — `NULL` 表示不可用，调用方判空即可

#### 范例

[network/local_info · 一览](../../examples/network/local_info/main.c) · 判空后回退占位文本

```c
str sAddress = xrtNetLocalAddressString(XNET_FAMILY_UNSPEC);
str sHost = xrtNetHostNameString();
str sHardware = xrtNetLocalHardwareString();
```

### `xrtNetHostName`
输出本机主机名并返回不含结尾零字节的所需长度。

```c
size_t xrtNetHostName(
	char* sName,
	size_t iCapacity
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sName` | 输出 | 可空 | 输出缓冲；空指针表示只查所需大小 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零字节） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 不含结尾零的所需长度；缓冲足够时已写入 | — |
| `0` | 系统查询失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_IO` — 系统主机名查询失败
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — 缓冲不足：不写入，返回完整所需大小

#### 范例

[network/interface_tour · 本机信息](../../examples/network/interface_tour/main.c) · 两段式输出

```c
iNeed = xrtNetHostName(NULL, 0u);
if ( (iNeed == 0u) || (iNeed >= sizeof(sName)) ) {
	goto Cleanup;
}
iSize = xrtNetHostName(sName, sizeof(sName));
```

### `xrtNetHostNameString`
分配并返回本机主机名。

```c
str xrtNetHostNameString(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 查询系统主机名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | `xrtAlloc` 分配的文本，调用方 `xrtFree` | — |
| `NULL` | 系统查询或分配失败 | 不设置线程错误（查询语义） |

#### 错误

- 无 — `NULL` 表示不可用，调用方判空即可

#### 范例

[network/local_info · 一览](../../examples/network/local_info/main.c) · 三件套打印后统一释放

```c
str sAddress = xrtNetLocalAddressString(XNET_FAMILY_UNSPEC);
str sHost = xrtNetHostNameString();
str sHardware = xrtNetLocalHardwareString();
```
