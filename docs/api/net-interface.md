# 网络接口

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
