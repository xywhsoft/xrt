# DNS

`XRT_FEATURE_NET_DNS` 只依赖网络地址基础层。它提供数字主机快路、系统名称解析、反向解析和不可变地址列表，不创建线程、Engine 或隐藏缓存。

## API

```c
xnetaddrlist* xrtNetLookup(cstr sHost, xnetfamily Family);
xnetaddrlist* xrtNetResolve(cstr sHost, uint16 iPort,
	xnetfamily Family);
bool xrtNetResolveOne(xnetaddr* pAddr, cstr sHost,
	uint16 iPort, xnetfamily Family);
str xrtNetReverse(const xnetaddr* pAddr);

xnetaddrlist* xrtNetAddrListCreate(const xnetaddr* pAddresses,
	size_t iCount);
xnetaddrlist* xrtNetAddrListWithPort(xnetaddrlist* pList,
	uint16 iPort);
xnetaddrlist* xrtNetAddrListRef(xnetaddrlist* pList);
void xrtNetAddrListDestroy(xnetaddrlist* pList);
size_t xrtNetAddrListCount(const xnetaddrlist* pList);
const xnetaddr* xrtNetAddrListGet(const xnetaddrlist* pList,
	size_t iIndex);
```

`xrtNetLookup` 只解析主机，返回的所有端口均为零，适合作为缓存、连接竞速和自定义 Resolver 的底层结果。`xrtNetResolve` 在同一完整查询基础上写入调用方端口，适合直接建立端点；两者都返回系统顺序中的全部 IPv4/IPv6 地址并去重。

两类查询都不使用固定 64 项临时数组，也不把主机名复制到固定 256 字节字段。数字 IPv4、IPv6 和方括号 IPv6 不进入系统 DNS。`Family` 可以是 `UNSPEC`、`IPV4` 或 `IPV6`；单地址端点场景使用 `xrtNetResolveOne`，失败时输出保持不变。

地址列表不可变且引用计数安全。`xrtNetAddrListCreate` 校验、复制并按完整端点去重外部地址；`xrtNetAddrListWithPort` 保持地址顺序并统一端口，端口未变化时直接增加原列表引用，否则返回独立列表。`Get` 返回的地址只在对应列表引用存活期间有效；缓存或异步解析器可以通过 `Ref` 与调用方共享同一完整结果。

`xrtNetReverse` 使用 `NI_NAMEREQD`。没有 PTR 名称时明确失败，不把数字地址回退伪装成主机名。名称服务失败使用 `XNET_ERROR_DNS_RESOLVE`、`XNET_ERROR_DNS_REVERSE` 或 `XNET_ERROR_DNS_RESULT`，`SystemCode` 保存平台 `getaddrinfo/getnameinfo` 返回码。

完整示例位于 `examples/network/dns/main.c`。
