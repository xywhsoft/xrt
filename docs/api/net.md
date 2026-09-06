# 网络地址基础 API

## 设计契约

`XRT_FEATURE_NET` 是网络体系最底层的独立裁剪单元，只依赖 `core`。这一层不创建 Socket，不执行 DNS，也不依赖线程、任务或协程；它负责稳定的 IP 地址表示、数字地址与端点解析、规范输出、地址分类和平台 `sockaddr` 转换。

公开头文件不包含 Winsock 或 POSIX Socket 头文件。`xnetaddr` 因此可以直接进入 FFI 值类型、容器键、配置结构和协议对象，不会把平台 ABI 扩散到上层。Windows 网络运行时由后续 Socket 操作按需初始化，使用者不需要配对调用启动与清理函数。

```c
typedef enum xnetfamily {
	XNET_FAMILY_UNSPEC = 0,
	XNET_FAMILY_IPV4 = 4,
	XNET_FAMILY_IPV6 = 6
} xnetfamily;

typedef struct xnetaddr {
	uint16 Family;
	uint16 Port;
	uint32 Scope;
	uint8 Address[16];
} xnetaddr;
```

`Port` 使用主机字节序，`Address` 始终使用网络字节序。IPv4 只使用前 4 字节；`Scope` 只对 IPv6 有意义。地址族常量是 XRT 自己的稳定值，不能与 `AF_INET`、`AF_INET6` 混用。

## 构造与解析

### `xrtNetAddrAny`

构造指定族的未指定地址（IPv4 `0.0.0.0` 或 IPv6 `::`）。

```c
bool xrtNetAddrAny(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 接收构造结果 |
| `Family` | 输入 | `IPV4` 或 `IPV6` | 其他值失败 |
| `iPort` | 输入 | — | 填入端口；`0` 合法（服务器动态端口绑定） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已构造 | — |
| `false` | 族不合法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pAddr == NULL` 或 `Family` 非法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 通配地址构造与判定

```c
if ( !xrtNetAddrAny(&Any, XNET_FAMILY_IPV4, 0u) ||
```

### `xrtNetAddrLoopback`

构造指定族的回环地址（IPv4 `127.0.0.1` 或 IPv6 `::1`）。

```c
bool xrtNetAddrLoopback(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 接收构造结果 |
| `Family` | 输入 | `IPV4` 或 `IPV6` | — |
| `iPort` | 输入 | — | 填入端口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已构造 | — |
| `false` | 族不合法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pAddr == NULL` 或 `Family` 非法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 回环以 `Parse("127.0.0.1")` 等价构造（见 `xrtNetAddrParse` 范例）

```c
if ( !xrtNetAddrParse(&Loopback, "127.0.0.1", 8080u) ||
```

### `xrtNetAddrParse`

严格解析数字 IPv4 或 IPv6 文本；不执行 DNS。

```c
bool xrtNetAddrParse(xnetaddr* pAddr, cstr sIP, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sIP` | 输入 | 非空 | IPv4 四段十进制（拒绝越界/缺段/前导零歧义）；IPv6 支持 `::`、嵌入式 IPv4、`%42` 数字 Scope；启用 `XRT_FEATURE_NET_INTERFACE` 后还接受 `%eth0` 接口名 Scope |
| `iPort` | 输入 | — | 填入端口，与文本无关 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解析构造 | — |
| `false` | 文本非法 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FORMAT` — 地址文本不符合严格语法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 双地址构造供比较族使用

```c
if ( !xrtNetAddrParse(&Loopback, "127.0.0.1", 8080u) ||
	!xrtNetAddrParse(&Private, "10.0.0.5", 8080u) ||
```

### `xrtNetAddrParseEndpoint`

解析 `IPv4:port`、`[IPv6]:port` 或使用默认端口的裸地址。

```c
bool xrtNetAddrParseEndpoint(xnetaddr* pAddr, cstr sEndpoint, uint16 iDefaultPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sEndpoint` | 输入 | 非空 | 裸 IPv6 的最后一段不会被猜测为端口——IPv6 显式端口必须方括号；地址按切片解析、不复制到定长临时数组 |
| `iDefaultPort` | 输入 | — | 文本未带端口时使用；`0` 合法 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解析构造 | — |
| `false` | 文本非法 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FORMAT` — 端点语法非法

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · 带 Scope 的 IPv6 端点

```c
if ( !xrtNetAddrParseEndpoint(&Addr, "[fe80::1%3]:8080", 0) ) {
	return 1;
}
```

## 文本输出

### `xrtNetAddrText`

输出规范 IP 文本，返回不含结尾零字节的所需长度。

```c
size_t xrtNetAddrText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv6 按 RFC 5952：小写、去前导零、压缩第一个最长零段；IPv4 映射输出 `::ffff:192.0.2.1` |
| `sText` | 输出 | 允许空指针 | `NULL, 0` 为零分配查询；容量不足仍尽量写零结尾文本 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 所需长度 | 不含结尾零；写入成功时即实际字节数 | — |
| `XRT_NPOS` | 地址非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_BUFFER` — 容量不足（仍返回所需长度并尽量写出）
- `XERR_ARGUMENT` — 参数非法

#### 范例

[network/interface · 基础范例](../../examples/network/interface/main.c) · 枚举接口地址文本

```c
if ( xrtNetAddrText(
	&pAddress->Address, sAddress, sizeof(sAddress)
) == XRT_NPOS ) {
```

### `xrtNetAddrEndpointText`

输出带端口的规范端点文本，IPv6 始终使用方括号。

```c
size_t xrtNetAddrEndpointText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 端口始终出现 |
| `sText` | 输出 | 允许空指针 | 同 `xrtNetAddrText` 的两段式口径 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 所需长度 | 不含结尾零 | — |
| `XRT_NPOS` | 地址非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_BUFFER` — 容量不足
- `XERR_ARGUMENT` — 参数非法

#### 范例

[network/interface · 基础范例](../../examples/network/interface/main.c) · 端点输出与 `AddrText` 同口径（见其范例）

```c
if ( xrtNetAddrText(
	&pAddress->Address, sAddress, sizeof(sAddress)
) == XRT_NPOS ) {
```

### `xrtNetAddrString`

分配并返回规范 IP 文本。

```c
str xrtNetAddrString(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式零结尾文本，`xrtFree` 释放；可长期保存、跨函数传递、同表达式多次调用（替换旧版线程局部环形缓冲） | — |
| `NULL` | 地址非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 参数非法
- 分配失败 — 文本内存申请失败

#### 范例

[network/dns · 基础范例](../../examples/network/dns/main.c) · 同族拥有式端点文本用法

```c
str sEndpoint = xrtNetAddrEndpointString(
```

### `xrtNetAddrEndpointString`

分配并返回带端口的规范端点文本。

```c
str xrtNetAddrEndpointString(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv6 自动加方括号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式零结尾文本，`xrtFree` 释放 | — |
| `NULL` | 地址非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 参数非法
- 分配失败 — 文本内存申请失败

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · 端点往返

```c
sEndpoint = xrtNetAddrEndpointString(&Addr);
if ( sEndpoint == NULL ) {
	return 1;
}
```

## 比较与分类

### `xrtNetAddrEqual`

比较完整端点：族、地址、IPv6 Scope 与端口全等。

```c
bool xrtNetAddrEqual(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 完整端点相同（端口不同即不等） |
| `false` | 任一分量不同 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · SameIP 分界：同 IP 换端口

```c
if ( xrtNetAddrEqual(&Loopback, &Private) ||
	!xrtNetAddrEqual(&Private, &Private) ||
	xrtNetAddrEqual(&Private, &Other) ||
```

### `xrtNetAddrSameIP`

只比较地址族、地址与 IPv6 Scope，不比较端口。

```c
bool xrtNetAddrSameIP(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 同族同地址同 Scope（端口可不同） |
| `false` | 地址分量不同 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 与 Equal 对照

```c
xrtNetAddrSameIP(&Loopback, &Private) ||
	!xrtNetAddrSameIP(&Private, &Other) ) {
```

### `xrtNetAddrCompare`

为 Map、排序和稳定去重提供完整端点全序。

```c
int xrtNetAddrCompare(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 负数 | 左端点按序在前（比较序：族 → 地址 → Scope → 端口） |
| `0` | 完整端点相同 |
| 正数 | 左端点在后 |

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 自反为零、10 < 127

```c
if ( (xrtNetAddrCompare(&Private, &Private) != 0) ||
	(xrtNetAddrCompare(&Private, &Loopback) >= 0) ||
```

### `xrtNetAddrIsUnspecified`

判断地址是否为 IPv4 `0.0.0.0` 或 IPv6 `::`。

```c
bool xrtNetAddrIsUnspecified(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只看地址，端口无关 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 未指定地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · Any 构造后判定

```c
xrtNetAddrIsUnspecified(&Loopback) ||
	!xrtNetAddrIsUnspecified(&Any) ||
```

### `xrtNetAddrIsLoopback`

判断地址是否属于 IPv4 `127/8` 或 IPv6 `::1`。

```c
bool xrtNetAddrIsLoopback(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv4 整个 `127/8` 段都算回环 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 回环地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 正反判定

```c
!xrtNetAddrIsLoopback(&Loopback) ||
	xrtNetAddrIsLoopback(&Private) ||
```

### `xrtNetAddrIsMulticast`

判断地址是否属于 IPv4 `224/4` 或 IPv6 `ff00::/8`。

```c
bool xrtNetAddrIsMulticast(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 多播组地址 |
| `false` | 单播地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `224.0.0.1` 命中

```c
!xrtNetAddrParse(&Other, "224.0.0.1", 0u) ||
	!xrtNetAddrIsMulticast(&Other) ||
```

### `xrtNetAddrIsLinkLocal`

判断地址是否属于 IPv4 `169.254/16` 或 IPv6 `fe80::/10`。

```c
bool xrtNetAddrIsLinkLocal(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 链路本地地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · Scope 端点解析后判定

```c
xrtNetAddrIsLinkLocal(&Addr) ? "yes" : "no");
```

### `xrtNetAddrIsPrivate`

判断地址是否属于 RFC 1918 IPv4 或 RFC 4193 IPv6 私有范围。

```c
bool xrtNetAddrIsPrivate(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只含私有段——不把回环、链路本地、文档地址混入；按安全策略组合多个明确谓词 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 私有范围地址 |
| `false` | 公网或其他范围 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `192.168/10` 命中、`8.8.8.8` 不命中

```c
!xrtNetAddrParse(&Other, "192.168.1.1", 0u) ||
	!xrtNetAddrIsPrivate(&Other) ||
	!xrtNetAddrIsPrivate(&Private) ||
```

### `xrtNetAddrIsMapped`

判断 IPv6 地址是否为 `::ffff:0:0/96` IPv4 映射地址。

```c
bool xrtNetAddrIsMapped(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv4 地址恒为假 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | IPv4 映射 IPv6 |
| `false` | 普通地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `::ffff:192.168.0.1` 命中

```c
if ( !xrtNetAddrParse(&Mapped, "::ffff:192.168.0.1", 443u) ||
	!xrtNetAddrIsMapped(&Mapped) ||
```

### `xrtNetAddrUnmap`

把 IPv4 映射 IPv6 地址转换为 IPv4；其他地址原样复制（保留端口）。

```c
bool xrtNetAddrUnmap(const xnetaddr* pAddr, xnetaddr* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |
| `pResult` | 输出 | 非空 | 非映射地址原样复制，允许统一规范化不加分支 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（转换或原样） | — |
| `false` | 参数非法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 映射还原 + 非映射原样

```c
!xrtNetAddrUnmap(&Mapped, &Unmapped) ||
	!xrtNetAddrParse(&Other, "192.168.0.1", 443u) ||
	!xrtNetAddrEqual(&Unmapped, &Other) ||
```

## Native 逃生口

### `xrtNetAddrToNative`

转换为平台 `sockaddr`；空输出可查询所需大小。

```c
bool xrtNetAddrToNative(const xnetaddr* pAddr, void* pNative, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |
| `pNative` | 输出 | 允许空指针 | `NULL` 时只经 `*pSize` 返回所需 `sockaddr_in`/`sockaddr_in6` 大小 |
| `pSize` | 输入输出 | 非空 | 入参为容量、出参为实际大小；容量不足也会更新大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（或已报告大小） | — |
| `false` | 参数非法或容量不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_BUFFER` — 缓冲不足（`*pSize` 已更新为所需大小）

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 先查询后写出的两段式

```c
if ( !xrtNetAddrToNative(&Loopback, NULL, &iSize) ||
```

### `xrtNetAddrFromNative`

从平台 `sockaddr` 转换为稳定地址结构。

```c
bool xrtNetAddrFromNative(xnetaddr* pAddr, const void* pNative, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `pNative` | 输入 | 非空 | 平台 `sockaddr` |
| `iSize` | 输入 | — | 检查地址族与结构长度合法性 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已转换（端口与 IPv6 Scope 保留） | — |
| `false` | 族或长度非法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FAMILY` — 不支持的地址族
- `XNET_ERROR_FORMAT` — 结构长度与族不符

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · sockaddr 往返等价

```c
!xrtNetAddrToNative(&Loopback, arrSockaddr, &iSize) ||
	!xrtNetAddrFromNative(&Native, arrSockaddr, iSize) ||
	!xrtNetAddrEqual(&Native, &Loopback) ) {
```

主机名与服务名解析属于独立 DNS 模块，不塞进地址语法函数。这组 Native 接口是有意保留的底层扩展路径：自定义 Socket 选项、第三方事件循环和上层协议可以直接连接平台 API，不需要复制 XRT 内部实现，也不会迫使公开地址结构绑定平台头文件。

## 网络缓冲

`XRT_FEATURE_NET_BUFFER` 依赖 `XRT_FEATURE_NET`，提供 TCP、UDP、TLS、HTTP 和 WebSocket 共用的可变尺寸缓冲底座。缓冲只在实际收到或排队数据时持有块；默认池尺寸类为 512、2048、8192、32768 字节，超过最大类的请求按实际大小单独分配；默认总缓存硬上限 2 MiB 且缓存属于 Worker 而非连接——一万个空闲连接不会因此各自占用 8K。

**线程**：池与缓冲是线程归属对象，不在热路径加锁；同一时刻只能由所属 Worker 操作。跨线程长期保存数据时使用不绑定池的缓冲（`Init` 传空池）、复制到调用方内存，或让所属 Worker 执行最终释放。

**所有权**：四种追加形态按复制/借用/接管/自定义释放区分；`Take`/`Ref` 只有成功才转移所有权，失败时数据仍归调用方。`xnetspan` 只借用块内数据——任何追加、提交、消费、Pullup、Move 或 Clear 后必须重新获取。

### `xrtNetBufPoolConfigInit`

初始化默认尺寸类与有界缓存策略的池配置。

```c
void xrtNetBufPoolConfigInit(xnetbufpoolconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 填入 512/2048/8192/32768 尺寸类与 2 MiB 总缓存上限 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化；`pConfig == NULL` 是参数错误（经 `xrtGetError()` 可查） |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 池生命周期起点

```c
xrtNetBufPoolConfigInit(&Config);
pPool = xrtNetBufPoolCreate(&Config);
```

### `xrtNetBufPoolCreate`

创建一个缓冲池；空配置使用默认值。

```c
xnetbufpool* xrtNetBufPoolCreate(const xnetbufpoolconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空指针 | `NULL` 使用默认配置；配置内容在创建期间复制 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新池，配对 `xrtNetBufPoolDestroy()` 释放 | — |
| `NULL` | 分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 分配失败 — 池结构内存申请失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 默认配置建池并挂两个缓冲

```c
if ( (pPool == NULL) ||
	!xrtNetBufInit(&BufA, pPool) ||
	!xrtNetBufInit(&BufB, pPool) ) {
```

### `xrtNetBufPoolDestroy`

销毁已无实时块的池；仍有外借块时失败并保留池。

```c
bool xrtNetBufPoolDestroy(xnetbufpool* pPool);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 允许空指针 | 空指针是成功空操作；块即使被 `Move` 到别的缓冲也仍计入原池实时统计 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 池已释放（先 `Trim` 清空缓存） | — |
| `false` | 仍有实时块 | 池保持完整、块中无悬空指针；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_POOL_BUSY` — `LiveBlocks != 0`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 失败路径的销毁收尾

```c
if ( pPool != NULL ) {
	xrtNetBufPoolDestroy(pPool);
}
```

### `xrtNetBufPoolTrim`

把缓存裁剪到不超过指定字节数，返回真正释放的块数。

```c
size_t xrtNetBufPoolTrim(xnetbufpool* pPool, size_t iRetainBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | 只释放缓存块，不影响实时数据 |
| `iRetainBytes` | 输入 | — | 保留的缓存预算；`0` 清空全部缓存 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 释放块数 | 可能大于等于 0 | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pPool == NULL`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 清空全部缓存

```c
iGot = xrtNetBufPoolTrim(pPool, 0u);
if ( iGot < 1u ) {
```

### `xrtNetBufPoolGet`

复制缓冲池当前统计，不分配内存。

```c
void xrtNetBufPoolGet(const xnetbufpool* pPool, xnetbufpoolinfo* pInfo);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空 | — |
| `pInfo` | 输出 | 非空 | 实时/峰值块数与容量、缓存块数与容量、分配/复用/动态大块/外部引用计数；`LiveBytes` 对拥有块统计容量、对引用块统计逻辑长度 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 参数非法时不修改输出（经 `xrtGetError()` 可查） |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 空池统计全零

```c
xrtNetBufPoolGet(pPool, &Info);
if ( (Info.LiveBlocks != 0u) ||
	(Info.AllocCount != 0u) ) {
```

### `xrtNetBufInit`

初始化空缓冲链；池为空时使用全局分配器且不缓存。

```c
bool xrtNetBufInit(xnetbuf* pBuffer, xnetbufpool* pPool);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输出 | 非空 | 可栈上或嵌入连接对象；结构可继续 `Clear` 复用 |
| `pPool` | 输入 | 允许空指针 | 空池适合跨线程所有权和低频独立使用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化为空链 | — |
| `false` | 参数非法 | 结构不被触碰；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer == NULL`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 挂到所属池

```c
!xrtNetBufInit(&BufA, pPool) ||
```

### `xrtNetBufClear`

释放全部块并放弃尚未提交的写入预留。

```c
void xrtNetBufClear(xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 允许空指针 | 幂等；引用块的释放过程在此时执行 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · Clear 触发 AppendRef 释放回调

```c
xrtNetBufClear(&BufA);
xrtNetBufClear(&BufB);
if ( iReleased != 1 ) {
```

### `xrtNetBufSize`

返回缓冲链总字节数。

```c
size_t xrtNetBufSize(const xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 字节数 | 跨全部块的活动数据总量 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 四类追加后恰 11 字节

```c
iSize = xrtNetBufSize(&BufA);
```

### `xrtNetBufEmpty`

返回缓冲链是否为空。

```c
bool xrtNetBufEmpty(const xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 无活动数据 |
| `false` | 至少一个字节 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · Move 后源为空

```c
!xrtNetBufMove(&BufA, &BufB) ||
	!xrtNetBufEmpty(&BufB) ||
```

### `xrtNetBufSpanCount`

返回当前缓冲链的只读 Span 总数。

```c
size_t xrtNetBufSpanCount(const xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| Span 数 | 完整消费所需的数组容量 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 四类追加后至少 4 段

```c
iSpans = xrtNetBufSpanCount(&BufA);
```

### `xrtNetBufSpans`

借用最多指定数量的只读 Span。

```c
size_t xrtNetBufSpans(const xnetbuf* pBuffer, xnetspan* pSpans, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |
| `pSpans` | 输出 | `iCapacity > 0` 时非空 | 接收 Span 数组；Span 借用块内数据 |
| `iCapacity` | 输入 | — | 数组容量；完整数量由 `SpanCount` 查询 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际写入数 | `<= iCapacity` | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — `pBuffer == NULL` 或 `iCapacity > 0 && pSpans == NULL`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 首段恰 "hello" 5 字节

```c
(xrtNetBufSpans(&BufA, Spans, 8u) != iSpans) ||
	(Spans[0].Size != 5u) ||
```

### `xrtNetBufFront`

借用明文队列的第一个连续 Span；空缓冲返回假。

```c
bool xrtNetBufFront(const xnetbuf* pBuffer, xnetspan* pSpan);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |
| `pSpan` | 输出 | 非空 | 空缓冲时被清为 `{ NULL, 0 }` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 首段非空 | — |
| `false` | 缓冲为空或参数非法 | 空缓冲属正常路径不设错；参数非法设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[network/buffer · 基础范例](../../examples/network/buffer/main.c) · Reserve→Commit 后取首段

```c
if ( !xrtNetBufCommit(&Buffer, 6) ||
	!xrtNetBufFront(&Buffer, &Read) ) {
```

### `xrtNetBufAppend`

复制一段数据到链尾；整个追加失败原子。

```c
bool xrtNetBufAppend(xnetbuf* pBuffer, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 成功后 `Size += iSize` |
| `pData` | 输入 | `iSize > 0` 时非空 | 来源借用调用期间 |
| `iSize` | 输入 | — | 字节数；`0` 为成功空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制追加 | — |
| `false` | 参数非法或分配失败 | 原数据不变（OOM 不留部分数据）；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出/分配失败 — 容量或总长字节数溢出、块分配失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 四类追加的第一段

```c
if ( !xrtNetBufAppend(&BufA, "hello", 5u) ||
	!xrtNetBufAppendBorrow(&BufA, arrBorrow, 2u) ) {
```

### `xrtNetBufAppendBorrow`

追加借用数据；调用方保证数据存活到该段被消费或清除。

```c
bool xrtNetBufAppendBorrow(xnetbuf* pBuffer, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | **借用**——不复制，存活期由调用方保证 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已借用追加（零复制） | — |
| `false` | 参数非法或登记失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出 — 总长溢出

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 与 Append 连用

```c
!xrtNetBufAppendBorrow(&BufA, arrBorrow, 2u) ) {
```

### `xrtNetBufAppendTake`

接管由 `xrtMalloc` 家族分配的数据；成功后由缓冲最终 `xrtFree`。

```c
bool xrtNetBufAppendTake(xnetbuf* pBuffer, ptr pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | **接管**——必须来自 `xrtMalloc` 家族；失败时所有权仍归调用方 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已接管追加 | — |
| `false` | 参数非法或登记失败 | 数据仍归调用方释放；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出 — 总长溢出

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 失败路径自行释放

```c
if ( !xrtNetBufAppendTake(&BufA, pTaken, 2u) ) {
	xrtFree(pTaken);
```

### `xrtNetBufAppendRef`

接管带自定义释放过程的外部数据。

```c
bool xrtNetBufAppendRef(xnetbuf* pBuffer, const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | **接管**；失败时不会调用释放过程 |
| `iSize` | 输入 | — | 字节数 |
| `pRelease` | 输入 | 非空 | 释放过程；最后一部分离开缓冲（消费完或 `Clear`）时恰好执行一次 |
| `pContext` | 输入 | — | 释放过程的用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已接管追加 | — |
| `false` | 参数非法 | 数据与释放责任仍归调用方；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 释放回调恰好一次（Clear 时触发）

```c
if ( !xrtNetBufAppendRef(&BufA, arrRef, 2u,
		exampleRelease, (ptr)&iReleased) ) {
```

### `xrtNetBufPrepend`

把一段数据复制到新首块；不移动已有负载块。

```c
bool xrtNetBufPrepend(xnetbuf* pBuffer, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 原有块和外部引用保持不动——适合加协议头 |
| `pData` | 输入 | `iSize > 0` 时非空 | 头内容 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已加首块 | — |
| `false` | 参数非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- 溢出/分配失败 — 溢出或首块分配失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 加 ">> " 前缀

```c
if ( !xrtNetBufPrepend(&BufA, ">> ", 3u) ||
	(xrtNetBufSize(&BufA) != 14u) ||
```

### `xrtNetBufReserve`

预留至少指定大小的连续尾部可写区。

```c
bool xrtNetBufReserve(xnetbuf* pBuffer, size_t iMinimum, xnetwspan* pSpan);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 预留期间不能执行其他改变链结构的操作 |
| `iMinimum` | 输入 | — | 最小连续字节数；IOCP/io_uring/`recv`/TLS 解密器可直接写入 |
| `pSpan` | 输出 | 非空 | 可写 Span；完成后 `Commit` 实际字节数、`Cancel` 放弃 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已预留并借出可写区 | — |
| `false` | 参数/状态非法或分配失败 | 原数据不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 已有在途预留（重复预留）
- 溢出/分配失败 — 尾块扩容失败

#### 范例

[network/buffer · 基础范例](../../examples/network/buffer/main.c) · 直接写入式接收

```c
if ( (pPool == NULL) || !xrtNetBufInit(&Buffer, pPool) ||
	!xrtNetBufReserve(&Buffer, 6, &Write) ) {
```

### `xrtNetBufCommit`

提交预留空间中已经写入的字节数。

```c
bool xrtNetBufCommit(xnetbuf* pBuffer, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 提交后预留结束，可再次 `Reserve` |
| `iSize` | 输入 | `<=` 预留容量 | 实际写入字节数；`0` 合法（丢弃预留区） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已计入缓冲 | — |
| `false` | 无预留、超量或溢出 | 预留仍可正确提交或取消；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 没有在途预留
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — `iSize` 超过预留容量
- 溢出 — 总长溢出

#### 范例

[network/buffer · 基础范例](../../examples/network/buffer/main.c) · 写入 6 字节后提交

```c
memcpy(Write.Data, "packet", 6);
if ( !xrtNetBufCommit(&Buffer, 6) ||
```

### `xrtNetBufCancel`

放弃当前写入预留，缓冲内容保持不变。

```c
bool xrtNetBufCancel(xnetbuf* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | EAGAIN、取消或零字节结果用本接口收尾 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 预留已放弃（新块释放、登记复原） | — |
| `false` | 没有在途预留 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 无预留可放弃

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · Cancel 后总长不变

```c
if ( !xrtNetBufReserve(&BufA, 8u, &Reserve) ||
	!xrtNetBufCancel(&BufA) ||
```

### `xrtNetBufMove`

把源缓冲的全部块移动到目标尾部，源恢复为空但保留池配置。

```c
bool xrtNetBufMove(xnetbuf* pTarget, xnetbuf* pSource);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入输出 | 非空 | 接收全部块；只重连块链不复制负载 |
| `pSource` | 输出 | 非空 | 恢复为空；AGAIN 或失败时源缓冲保持不变 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已转移 | — |
| `false` | 参数非法（如自移） | 双方不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针或 `pTarget == pSource`

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 块链零复制转移

```c
if ( !xrtNetBufAppend(&BufB, "!", 1u) ||
	!xrtNetBufMove(&BufA, &BufB) ||
```

### `xrtNetBufPullup`

确保指定长度的内存前缀连续。

```c
bool xrtNetBufPullup(xnetbuf* pBuffer, size_t iSize, xnetspan* pSpan);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 首块足够时零复制返回，否则只复制指定前缀到首块——适合解析固定协议头 |
| `iSize` | 输入 | `<= Size` | 需要连续的前缀字节数 |
| `pSpan` | 输出 | 非空 | 连续前缀 Span；`iSize == 0` 输出空 Span 且成功 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 前缀已连续并借出 | — |
| `false` | 参数/状态非法或前缀超长 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XERR_RANGE` + `XNET_ERROR_BUFFER` — `iSize` 超过缓冲现有数据
- `XERR_STATE` + `XNET_ERROR_BUFFER_STATE` — 预留期间操作
- 溢出/分配失败 — 首块扩容失败

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 拼合 5 字节协议头

```c
!xrtNetBufPullup(&BufA, 5u, &Span) ||
	(Span.Size < 5u) ||
```

### `xrtNetBufPeek`

从指定偏移复制最多给定字节；不消费。

```c
size_t xrtNetBufPeek(const xnetbuf* pBuffer, size_t iOffset, void* pOutput, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | — |
| `iOffset` | 输入 | — | 起始偏移；遇到块边界外的引用段语义见实现（跨块复制、引用块按逻辑长度） |
| `pOutput` | 输出 | `iSize > 0` 时非空 | 接收副本 |
| `iSize` | 输入 | — | 最多复制字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际复制数 | 从 `iOffset` 起的可用前缀 | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 偏移 8 读 3 字节

```c
(xrtNetBufPeek(&BufA, 8u, arrText, 3u) != 3u) ||
```

### `xrtNetBufRead`

复制并消费：从链首取走最多给定字节。

```c
size_t xrtNetBufRead(xnetbuf* pBuffer, void* pOutput, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 已消费部分脱离活动区 |
| `pOutput` | 输出 | `iSize > 0` 时非空 | 接收副本 |
| `iSize` | 输入 | — | 请求字节数；可用不足时短读 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际读取数 | 短读合法 | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[network/tcp · 基础范例](../../examples/network/tcp/main.c) · 接收队列整段取出

```c
iSize = xrtNetBufRead(pBuffer, Data, sizeof(Data));
```

### `xrtNetBufFind`

从指定偏移查找一个字节，未找到返回 `XRT_NPOS`。

```c
size_t xrtNetBufFind(const xnetbuf* pBuffer, uint8 iByte, size_t iOffset);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空 | 跨块查找 |
| `iByte` | 输入 | — | 目标字节 |
| `iOffset` | 输入 | — | 起始偏移 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 偏移 | 第一个命中位置 |
| `XRT_NPOS` | 未命中 |

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 命中 'w' 与未命中 'z'

```c
(xrtNetBufFind(&BufA, 'w', 0u) != 9u) ||
	(xrtNetBufFind(&BufA, 'z', 0u) != XRT_NPOS) ||
```

### `xrtNetBufConsume`

消费最多给定字节的前缀。

```c
size_t xrtNetBufConsume(xnetbuf* pBuffer, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入输出 | 非空 | 不会释放仍被活动写预留借用的尾块；引用块消费到最后一段时执行释放 |
| `iSize` | 输入 | — | 请求量；允许超过剩余数据（按剩余消费） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 实际消费量 | `<= iSize` 且 `<= Size` | 参数非法时返回 0 并设置错误 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[buf_tour](../../examples/network/buf_tour/main.c) · 消费 ">> " 三字节

```c
(xrtNetBufConsume(&BufA, 3u) != 3u) ||
```

## 拥有型字节结果

### `xrtNetBytesRef`

增加拥有型网络字节结果的引用并返回原指针。

```c
xnetbytes* xrtNetBytesRef(xnetbytes* pBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBytes` | 输入 | 非空 | 接收结果（如 `xrtNetStreamRecv`）的拥有式对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 原指针 | 引用已增加；每次成功须一次 `xrtNetBytesDestroy` 配对 | — |
| `NULL` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pBytes == NULL`

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 共享持有接收结果

```c
xnetbytes* pShared = xrtNetBytesRef(pBytes);
```

### `xrtNetBytesDestroy`

释放拥有型网络字节结果；空指针视为空操作。

```c
void xrtNetBytesDestroy(xnetbytes* pBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBytes` | 输入 | 允许空指针 | 最后一个引用释放时对象销毁 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 视图取出后释放

```c
xrtNetBytesDestroy(pBytes);
```

### `xrtNetBytesView`

返回拥有型网络字节结果的借用视图。

```c
xbytesview xrtNetBytesView(const xnetbytes* pBytes);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBytes` | 输入 | 允许空指针 | 空指针返回空视图 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 视图 | 数据与长度的只读借用；对象销毁后失效 |

#### 范例

[tcp_stream_tour](../../examples/network/tcp_stream_tour/main.c) · 与期望输出逐位核对

```c
xbytesview View = xrtNetBytesView(pBytes);
```


## 地址列表与 DNS

数字地址语法不包含主机名解析；DNS 与不可变地址列表属于 `XRT_FEATURE_NET_DNS`。列表是解析结果的不可变共享快照：复制、校验、去重调用方地址后建立，`Ref` 增引用、`Destroy` 配对释放，端口统一替换用 `WithPort`。

### `xrtNetAddrListCreate`

复制、校验并去重调用方地址，建立不可变列表。

```c
xnetaddrlist* xrtNetAddrListCreate(const xnetaddr* pAddresses, size_t iCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddresses` | 输入 | `iCount > 0` 时非空 | 来源数组在创建期间复制；重复项去除 |
| `iCount` | 输入 | — | 地址数；`0` 建空列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新列表，配对 `xrtNetAddrListDestroy()` 释放 | — |
| `NULL` | 参数非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `iCount > 0 && pAddresses == NULL`
- 分配失败 — 列表内存申请失败

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 双地址建表

```c
pList = xrtNetAddrListCreate(arrTwo, 2u);
if ( (pList == NULL) ||
	(xrtNetAddrListCount(pList) != 2u) ) {
```

### `xrtNetAddrListWithPort`

复制列表并统一替换端口；端口已一致时只增加引用。

```c
xnetaddrlist* xrtNetAddrListWithPort(xnetaddrlist* pList, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 非空 | 原列表不受影响 |
| `iPort` | 输入 | — | 新端口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 新列表（或原列表的引用）；独立 `Destroy` 配对 | — |
| `NULL` | 参数非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pList == NULL`
- 分配失败 — 新列表申请失败

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 两地址统一换 443

```c
pPortList = xrtNetAddrListWithPort(pList, 443u);
if ( (pPortList == NULL) ||
	(xrtNetAddrListCount(pPortList) != 2u) ||
	(xrtNetAddrListGet(pPortList, 0u)->Port != 443u) ||
```

### `xrtNetAddrListRef`

增加不可变列表引用并返回原指针。

```c
xnetaddrlist* xrtNetAddrListRef(xnetaddrlist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 非空 | 对象内容在全部引用之间只读 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 原指针 | 引用已增加；每次成功须一次 `Destroy` 配对 | — |
| `NULL` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pList == NULL`

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 共享引用

```c
pRef = xrtNetAddrListRef(pPortList);
if ( pRef != pPortList ) {
```

### `xrtNetAddrListDestroy`

释放列表引用；空指针是空操作。

```c
void xrtNetAddrListDestroy(xnetaddrlist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 允许空指针 | 最后一个引用释放时对象销毁 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 引用与原件各自配对释放

```c
xrtNetAddrListDestroy(pRef);
xrtNetAddrListDestroy(pPortList);
```

### `xrtNetAddrListCount`

返回地址数量；空列表返回零。

```c
size_t xrtNetAddrListCount(const xnetaddrlist* pList);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 允许空指针 | 空指针返回 0 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 数量 | 列表内地址数 |

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 见 `xrtNetAddrListCreate` 范例

```c
(xrtNetAddrListCount(pList) != 2u) ) {
```

### `xrtNetAddrListGet`

返回借用地址；索引越界返回空指针并设置范围错误。

```c
const xnetaddr* xrtNetAddrListGet(const xnetaddrlist* pList, size_t iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pList` | 输入 | 非空 | — |
| `iIndex` | 输入 | `< Count` | 0 基索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 借用地址；视图随列表最后一个引用失效 | — |
| `NULL` | 越界或参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_RANGE` — `iIndex >= Count`
- `XERR_ARGUMENT` — `pList == NULL`

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 读换端口后的首地址

```c
(xrtNetAddrListGet(pPortList, 0u)->Port != 443u) ||
```

### `xrtNetLookup`

解析主机的全部地址；保留系统顺序、去重、端口为零。

```c
xnetaddrlist* xrtNetLookup(cstr sHost, xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sHost` | 输入 | 非空 | 主机名或数字地址 |
| `Family` | 输入 | — | 限定族；`UNSPEC` 接受全部 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 只查询主机地址的列表（端口全零），配对 `Destroy` 释放 | — |
| `NULL` | 解析失败 | 错误经 `xrtGetError()` 报告（`XNET_ERROR_DNS_*` 族） |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_DNS_RESOLVE` 等域名解析错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · localhost 本机解析不依赖外网

```c
xnetaddrlist* pLocal = xrtNetLookup("localhost",
	XNET_FAMILY_IPV4);
```

### `xrtNetResolve`

解析主机并统一端口。

```c
xnetaddrlist* xrtNetResolve(cstr sHost, uint16 iPort, xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sHost` | 输入 | 非空 | — |
| `iPort` | 输入 | — | 写入全部结果的端口 |
| `Family` | 输入 | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 带端口的列表，配对 `Destroy` 释放 | — |
| `NULL` | 解析失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetLookup`

#### 范例

[network/dns · 基础范例](../../examples/network/dns/main.c) · 完整列表遍历

```c
xnetaddrlist* pList = xrtNetResolve(
	"localhost",
	443,
	XNET_FAMILY_UNSPEC
);
```

### `xrtNetResolveOne`

解析并复制系统顺序中的第一个地址；单地址场景无需管理列表。

```c
bool xrtNetResolveOne(xnetaddr* pAddr, cstr sHost, uint16 iPort, xnetfamily Family);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sHost` | 输入 | 非空 | — |
| `iPort` | 输入 | — | 写入端口 |
| `Family` | 输入 | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制首地址 | — |
| `false` | 解析失败 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetLookup`

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 端口 80 的单地址

```c
if ( !xrtNetResolveOne(&Resolved, "localhost", 80u,
		XNET_FAMILY_IPV4) ||
```

### `xrtNetReverse`

反向解析一个数字地址；成功返回调用方拥有的主机名。

```c
str xrtNetReverse(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只接受数字地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式主机名，`xrtFree` 释放 | — |
| `NULL` | 反查失败或参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_DNS_REVERSE` — 反查失败

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 回环地址反查

```c
sHost = xrtNetReverse(&Loopback);
if ( sHost == NULL ) {
```

## 错误

- `XERR_ARGUMENT` — `pAddr == NULL` 或 `Family` 非法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 通配地址构造与判定

```c
if ( !xrtNetAddrAny(&Any, XNET_FAMILY_IPV4, 0u) ||
```

### `xrtNetAddrLoopback`

构造指定族的回环地址（IPv4 `127.0.0.1` 或 IPv6 `::1`）。

```c
bool xrtNetAddrLoopback(xnetaddr* pAddr, xnetfamily Family, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 接收构造结果 |
| `Family` | 输入 | `IPV4` 或 `IPV6` | — |
| `iPort` | 输入 | — | 填入端口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已构造 | — |
| `false` | 族不合法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pAddr == NULL` 或 `Family` 非法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 回环以 `Parse("127.0.0.1")` 等价构造（见 `xrtNetAddrParse` 范例）

```c
if ( !xrtNetAddrParse(&Loopback, "127.0.0.1", 8080u) ||
```

### `xrtNetAddrParse`

严格解析数字 IPv4 或 IPv6 文本；不执行 DNS。

```c
bool xrtNetAddrParse(xnetaddr* pAddr, cstr sIP, uint16 iPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sIP` | 输入 | 非空 | IPv4 四段十进制（拒绝越界/缺段/前导零歧义）；IPv6 支持 `::`、嵌入式 IPv4、`%42` 数字 Scope；启用 `XRT_FEATURE_NET_INTERFACE` 后还接受 `%eth0` 接口名 Scope |
| `iPort` | 输入 | — | 填入端口，与文本无关 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解析构造 | — |
| `false` | 文本非法 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FORMAT` — 地址文本不符合严格语法

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 双地址构造供比较族使用

```c
if ( !xrtNetAddrParse(&Loopback, "127.0.0.1", 8080u) ||
	!xrtNetAddrParse(&Private, "10.0.0.5", 8080u) ||
```

### `xrtNetAddrParseEndpoint`

解析 `IPv4:port`、`[IPv6]:port` 或使用默认端口的裸地址。

```c
bool xrtNetAddrParseEndpoint(xnetaddr* pAddr, cstr sEndpoint, uint16 iDefaultPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `sEndpoint` | 输入 | 非空 | 裸 IPv6 的最后一段不会被猜测为端口——IPv6 显式端口必须方括号；地址按切片解析、不复制到定长临时数组 |
| `iDefaultPort` | 输入 | — | 文本未带端口时使用；`0` 合法 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已解析构造 | — |
| `false` | 文本非法 | `*pAddr` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FORMAT` — 端点语法非法

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · 带 Scope 的 IPv6 端点

```c
if ( !xrtNetAddrParseEndpoint(&Addr, "[fe80::1%3]:8080", 0) ) {
	return 1;
}
```

## 文本输出

### `xrtNetAddrText`

输出规范 IP 文本，返回不含结尾零字节的所需长度。

```c
size_t xrtNetAddrText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv6 按 RFC 5952：小写、去前导零、压缩第一个最长零段；IPv4 映射输出 `::ffff:192.0.2.1` |
| `sText` | 输出 | 允许空指针 | `NULL, 0` 为零分配查询；容量不足仍尽量写零结尾文本 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 所需长度 | 不含结尾零；写入成功时即实际字节数 | — |
| `XRT_NPOS` | 地址非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_BUFFER` — 容量不足（仍返回所需长度并尽量写出）
- `XERR_ARGUMENT` — 参数非法

#### 范例

[network/interface · 基础范例](../../examples/network/interface/main.c) · 枚举接口地址文本

```c
if ( xrtNetAddrText(
	&pAddress->Address, sAddress, sizeof(sAddress)
) == XRT_NPOS ) {
```

### `xrtNetAddrEndpointText`

输出带端口的规范端点文本，IPv6 始终使用方括号。

```c
size_t xrtNetAddrEndpointText(const xnetaddr* pAddr, char* sText, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 端口始终出现 |
| `sText` | 输出 | 允许空指针 | 同 `xrtNetAddrText` 的两段式口径 |
| `iCapacity` | 输入 | — | 缓冲容量（含结尾零） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 所需长度 | 不含结尾零 | — |
| `XRT_NPOS` | 地址非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_BUFFER` — 容量不足
- `XERR_ARGUMENT` — 参数非法

#### 范例

[network/interface · 基础范例](../../examples/network/interface/main.c) · 端点输出与 `AddrText` 同口径（见其范例）

```c
if ( xrtNetAddrText(
	&pAddress->Address, sAddress, sizeof(sAddress)
) == XRT_NPOS ) {
```

### `xrtNetAddrString`

分配并返回规范 IP 文本。

```c
str xrtNetAddrString(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式零结尾文本，`xrtFree` 释放；可长期保存、跨函数传递、同表达式多次调用（替换旧版线程局部环形缓冲） | — |
| `NULL` | 地址非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 参数非法
- 分配失败 — 文本内存申请失败

#### 范例

[network/dns · 基础范例](../../examples/network/dns/main.c) · 同族拥有式端点文本用法

```c
str sEndpoint = xrtNetAddrEndpointString(
```

### `xrtNetAddrEndpointString`

分配并返回带端口的规范端点文本。

```c
str xrtNetAddrEndpointString(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv6 自动加方括号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有式零结尾文本，`xrtFree` 释放 | — |
| `NULL` | 地址非法或分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 参数非法
- 分配失败 — 文本内存申请失败

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · 端点往返

```c
sEndpoint = xrtNetAddrEndpointString(&Addr);
if ( sEndpoint == NULL ) {
	return 1;
}
```

## 比较与分类

### `xrtNetAddrEqual`

比较完整端点：族、地址、IPv6 Scope 与端口全等。

```c
bool xrtNetAddrEqual(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 完整端点相同（端口不同即不等） |
| `false` | 任一分量不同 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · SameIP 分界：同 IP 换端口

```c
if ( xrtNetAddrEqual(&Loopback, &Private) ||
	!xrtNetAddrEqual(&Private, &Private) ||
	xrtNetAddrEqual(&Private, &Other) ||
```

### `xrtNetAddrSameIP`

只比较地址族、地址与 IPv6 Scope，不比较端口。

```c
bool xrtNetAddrSameIP(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 同族同地址同 Scope（端口可不同） |
| `false` | 地址分量不同 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 与 Equal 对照

```c
xrtNetAddrSameIP(&Loopback, &Private) ||
	!xrtNetAddrSameIP(&Private, &Other) ) {
```

### `xrtNetAddrCompare`

为 Map、排序和稳定去重提供完整端点全序。

```c
int xrtNetAddrCompare(const xnetaddr* pLeft, const xnetaddr* pRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLeft` | 输入 | 非空 | — |
| `pRight` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 负数 | 左端点按序在前（比较序：族 → 地址 → Scope → 端口） |
| `0` | 完整端点相同 |
| 正数 | 左端点在后 |

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 自反为零、10 < 127

```c
if ( (xrtNetAddrCompare(&Private, &Private) != 0) ||
	(xrtNetAddrCompare(&Private, &Loopback) >= 0) ||
```

### `xrtNetAddrIsUnspecified`

判断地址是否为 IPv4 `0.0.0.0` 或 IPv6 `::`。

```c
bool xrtNetAddrIsUnspecified(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只看地址，端口无关 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 未指定地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · Any 构造后判定

```c
xrtNetAddrIsUnspecified(&Loopback) ||
	!xrtNetAddrIsUnspecified(&Any) ||
```

### `xrtNetAddrIsLoopback`

判断地址是否属于 IPv4 `127/8` 或 IPv6 `::1`。

```c
bool xrtNetAddrIsLoopback(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv4 整个 `127/8` 段都算回环 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 回环地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 正反判定

```c
!xrtNetAddrIsLoopback(&Loopback) ||
	xrtNetAddrIsLoopback(&Private) ||
```

### `xrtNetAddrIsMulticast`

判断地址是否属于 IPv4 `224/4` 或 IPv6 `ff00::/8`。

```c
bool xrtNetAddrIsMulticast(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 多播组地址 |
| `false` | 单播地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `224.0.0.1` 命中

```c
!xrtNetAddrParse(&Other, "224.0.0.1", 0u) ||
	!xrtNetAddrIsMulticast(&Other) ||
```

### `xrtNetAddrIsLinkLocal`

判断地址是否属于 IPv4 `169.254/16` 或 IPv6 `fe80::/10`。

```c
bool xrtNetAddrIsLinkLocal(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 链路本地地址 |
| `false` | 其他地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[network/address · 基础范例](../../examples/network/address/main.c) · Scope 端点解析后判定

```c
xrtNetAddrIsLinkLocal(&Addr) ? "yes" : "no");
```

### `xrtNetAddrIsPrivate`

判断地址是否属于 RFC 1918 IPv4 或 RFC 4193 IPv6 私有范围。

```c
bool xrtNetAddrIsPrivate(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | 只含私有段——不把回环、链路本地、文档地址混入；按安全策略组合多个明确谓词 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 私有范围地址 |
| `false` | 公网或其他范围 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `192.168/10` 命中、`8.8.8.8` 不命中

```c
!xrtNetAddrParse(&Other, "192.168.1.1", 0u) ||
	!xrtNetAddrIsPrivate(&Other) ||
	!xrtNetAddrIsPrivate(&Private) ||
```

### `xrtNetAddrIsMapped`

判断 IPv6 地址是否为 `::ffff:0:0/96` IPv4 映射地址。

```c
bool xrtNetAddrIsMapped(const xnetaddr* pAddr);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | IPv4 地址恒为假 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | IPv4 映射 IPv6 |
| `false` | 普通地址 |

#### 错误

- 无——纯谓词不设置错误

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · `::ffff:192.168.0.1` 命中

```c
if ( !xrtNetAddrParse(&Mapped, "::ffff:192.168.0.1", 443u) ||
	!xrtNetAddrIsMapped(&Mapped) ||
```

### `xrtNetAddrUnmap`

把 IPv4 映射 IPv6 地址转换为 IPv4；其他地址原样复制（保留端口）。

```c
bool xrtNetAddrUnmap(const xnetaddr* pAddr, xnetaddr* pResult);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |
| `pResult` | 输出 | 非空 | 非映射地址原样复制，允许统一规范化不加分支 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（转换或原样） | — |
| `false` | 参数非法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 映射还原 + 非映射原样

```c
!xrtNetAddrUnmap(&Mapped, &Unmapped) ||
	!xrtNetAddrParse(&Other, "192.168.0.1", 443u) ||
	!xrtNetAddrEqual(&Unmapped, &Other) ||
```

## Native 逃生口

### `xrtNetAddrToNative`

转换为平台 `sockaddr`；空输出可查询所需大小。

```c
bool xrtNetAddrToNative(const xnetaddr* pAddr, void* pNative, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输入 | 非空 | — |
| `pNative` | 输出 | 允许空指针 | `NULL` 时只经 `*pSize` 返回所需 `sockaddr_in`/`sockaddr_in6` 大小 |
| `pSize` | 输入输出 | 非空 | 入参为容量、出参为实际大小；容量不足也会更新大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出（或已报告大小） | — |
| `false` | 参数非法或容量不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_BUFFER` — 缓冲不足（`*pSize` 已更新为所需大小）

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · 先查询后写出的两段式

```c
if ( !xrtNetAddrToNative(&Loopback, NULL, &iSize) ||
```

### `xrtNetAddrFromNative`

从平台 `sockaddr` 转换为稳定地址结构。

```c
bool xrtNetAddrFromNative(xnetaddr* pAddr, const void* pNative, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAddr` | 输出 | 非空 | 失败不修改 |
| `pNative` | 输入 | 非空 | 平台 `sockaddr` |
| `iSize` | 输入 | — | 检查地址族与结构长度合法性 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已转换（端口与 IPv6 Scope 保留） | — |
| `false` | 族或长度非法 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_FAMILY` — 不支持的地址族
- `XNET_ERROR_FORMAT` — 结构长度与族不符

#### 范例

[addr_tour](../../examples/network/addr_tour/main.c) · sockaddr 往返等价

```c
!xrtNetAddrToNative(&Loopback, arrSockaddr, &iSize) ||
	!xrtNetAddrFromNative(&Native, arrSockaddr, iSize) ||
	!xrtNetAddrEqual(&Native, &Loopback) ) {
```

主机名与服务名解析属于独立 DNS 模块，不塞进地址语法函数。这组 Native 接口是有意保留的底层扩展路径：自定义 Socket 选项、第三方事件循环和上层协议可以直接连接平台 API，不需要复制 XRT 内部实现，也不会迫使公开地址结构绑定平台头文件。

## 网络缓冲

`XRT_FEATURE_NET_BUFFER` 依赖 `XRT_FEATURE_NET`，提供 TCP、UDP、TLS、HTTP 和 WebSocket 共用的可变尺寸缓冲底座。它保留旧版 `xnetchain` 的分块、Span 和引用数据优势，但删除“每个连接常驻固定 8K 缓冲”的模型。

缓冲只在实际收到或排队数据时持有块。默认池尺寸类为 512、2048、8192、32768 字节；超过最大类的请求按实际大小单独分配。默认总缓存硬上限为 2 MiB，并且缓存属于 worker，而不是连接：一万个空闲连接不会因此各自占用 8K。

### 池

```c
typedef struct xnetbufpoolconfig {
	size_t BlockSize[4];
	size_t CacheLimit[4];
	size_t MaxCacheBytes;
} xnetbufpoolconfig;

void xrtNetBufPoolConfigInit(xnetbufpoolconfig* pConfig);
xnetbufpool* xrtNetBufPoolCreate(const xnetbufpoolconfig* pConfig);
bool xrtNetBufPoolDestroy(xnetbufpool* pPool);
size_t xrtNetBufPoolTrim(xnetbufpool* pPool, size_t iRetainBytes);
void xrtNetBufPoolGet(const xnetbufpool* pPool, xnetbufpoolinfo* pInfo);
```

池是线程归属对象，不在热路径加锁。一个网络 worker 持有一个池；同一时刻只能由该 worker 操作池及其块。不同池可以并行。跨线程长期保存数据时，使用不绑定池的缓冲、复制到调用方内存，或让所属 worker 执行最终释放。

`Destroy` 在仍有实时块时返回 `false` 和 `XNET_ERROR_POOL_BUSY`，不会留下块中的悬空池指针。块即使被 `Move` 到另一个缓冲，也仍计入原池实时统计。`Trim` 只释放缓存块，不影响实时数据。

`xnetbufpoolinfo` 提供实时/峰值块数和容量、缓存块数和容量、底层分配、复用、动态大块和外部引用计数。`LiveBytes` 对拥有块统计容量，对引用块统计逻辑引用长度，用于诊断实际压力和在途数据。

### 生命周期与查询

```c
bool xrtNetBufInit(xnetbuf* pBuffer, xnetbufpool* pPool);
void xrtNetBufClear(xnetbuf* pBuffer);
size_t xrtNetBufSize(const xnetbuf* pBuffer);
bool xrtNetBufEmpty(const xnetbuf* pBuffer);
size_t xrtNetBufSpanCount(const xnetbuf* pBuffer);
size_t xrtNetBufSpans(const xnetbuf* pBuffer, xnetspan* pSpans, size_t iCapacity);
bool xrtNetBufFront(const xnetbuf* pBuffer, xnetspan* pSpan);
```

`xnetbuf` 可以栈上或嵌入连接对象。传入空池时使用全局分配器且不缓存，适合跨线程所有权和低频独立使用。`Clear` 释放全部块并放弃未提交预留，结构保持可继续使用。

`xnetspan` 只借用块内数据。任何追加、预留提交、消费、Pullup、Move 或 Clear 后都必须重新获取 Span。`Spans` 返回实际写入数组的数量；完整所需数量由 `SpanCount` 查询。`Front` 只在存在非空首段时返回 `true`；空缓冲会把输出清为 `{ NULL, 0 }` 并返回 `false`。合法的 UDP 零长度数据报由 UDP 消息自身的 `Size == 0` 表达，不依赖空缓冲视图。

### 四种写入所有权

```c
bool xrtNetBufAppend(xnetbuf* pBuffer, const void* pData, size_t iSize);
bool xrtNetBufPrepend(xnetbuf* pBuffer, const void* pData, size_t iSize);
bool xrtNetBufAppendBorrow(xnetbuf* pBuffer, const void* pData, size_t iSize);
bool xrtNetBufAppendTake(xnetbuf* pBuffer, ptr pData, size_t iSize);
bool xrtNetBufAppendRef(xnetbuf* pBuffer, const void* pData,
	size_t iSize, xnetreleaseproc pRelease, ptr pContext);
```

- `Append`：复制数据。整个追加具有失败原子性，OOM 不会留下部分数据。
- `Prepend`：把协议头复制到新首块，不移动或复制已有负载块。
- `AppendBorrow`：零复制借用，调用方保证数据存活到该段被消费或清除。
- `AppendTake`：接管由 `xrtMalloc` 家族分配的数据，最终调用 `xrtFree`。
- `AppendRef`：接管带自定义释放过程的数据，最后一部分离开缓冲时执行一次释放过程。

`Take` 和 `Ref` 只有成功后才转移所有权；失败时调用方仍负责数据。部分消费不会提前释放引用块。

### 直接接收与编码

```c
bool xrtNetBufReserve(xnetbuf* pBuffer, size_t iMinimum, xnetwspan* pSpan);
bool xrtNetBufCommit(xnetbuf* pBuffer, size_t iSize);
bool xrtNetBufCancel(xnetbuf* pBuffer);
```

`Reserve` 返回至少 `iMinimum` 字节的连续可写区。IOCP、io_uring、`recv`、TLS 解密器和协议编码器可以直接把结果写进该区，完成后 `Commit` 实际字节数；EAGAIN、取消或零字节结果使用 `Cancel`。预留期间不能执行其他改变链结构的操作，重复预留或错误提交会返回 `XNET_ERROR_BUFFER_STATE`，原预留仍可正确提交或取消。

后端的推荐路径是：为一次接收创建空缓冲，Reserve 自适应块，直接把 Span 提交给操作系统，完成后 Commit，再把整条缓冲 Move 到连接接收队列。这个路径没有“固定 8K 接收区 -> 再复制到 Chain”的第二份内存和复制开销。

### 协议操作

```c
bool xrtNetBufMove(xnetbuf* pTarget, xnetbuf* pSource);
bool xrtNetBufPullup(xnetbuf* pBuffer, size_t iSize, xnetspan* pSpan);
size_t xrtNetBufPeek(const xnetbuf* pBuffer,
	size_t iOffset, void* pOutput, size_t iSize);
size_t xrtNetBufRead(xnetbuf* pBuffer, void* pOutput, size_t iSize);
size_t xrtNetBufFind(const xnetbuf* pBuffer, uint8 iByte, size_t iOffset);
size_t xrtNetBufConsume(xnetbuf* pBuffer, size_t iSize);
```

`Move` 只重连块链，不复制负载；源缓冲恢复为空但保留自己的池配置。`Pullup` 在首块足够时零复制返回，否则只复制指定前缀，适合解析固定协议头。`Peek` 不消费，`Read` 复制并消费，`Find` 跨块查找，`Consume` 返回实际消费量并允许请求超过剩余数据。

## 错误

网络错误域是 `xrt.net`，稳定代码为：

| 代码 | 含义 |
| --- | --- |
| `XNET_ERROR_NONE` | 没有网络错误；用于统计快照等非失败状态 |
| `XNET_ERROR_FORMAT` | 数字地址或端点语法错误 |
| `XNET_ERROR_FAMILY` | 地址族不受支持 |
| `XNET_ERROR_PORT` | 端口为空、非十进制或越界 |
| `XNET_ERROR_SCOPE` | IPv6 Scope 非法或越界 |
| `XNET_ERROR_BUFFER` | 文本或 native 输出缓冲不足 |
| `XNET_ERROR_NATIVE` | 平台地址结构截断或不支持 |
| `XNET_ERROR_SYSTEM` | 平台网络运行时失败 |
| `XNET_ERROR_DNS_RESOLVE` | 正向 DNS 解析失败 |
| `XNET_ERROR_DNS_REVERSE` | 反向 DNS 解析失败 |
| `XNET_ERROR_DNS_RESULT` | DNS 结果索引、地址族或输出非法 |
| `XNET_ERROR_RESOLVER_CREATE` | 异步 Resolver 配置或创建失败 |
| `XNET_ERROR_RESOLVER_SUBMIT` | Resolver 请求非法、超限或提交失败 |
| `XNET_ERROR_RESOLVER_CLOSED` | Resolver 已关闭或关闭过程失败 |
| `XNET_ERROR_RESOLVER_QUERY` | Resolver 后台查询失败 |
| `XNET_ERROR_BUFFER_STATE` | 缓冲预留、池配置或修改状态非法 |
| `XNET_ERROR_POOL_BUSY` | 缓冲池仍有实时块，不能销毁 |
| `XNET_ERROR_SOCKET_OPEN` | 创建或初始化 Socket 失败 |
| `XNET_ERROR_SOCKET_CLOSE` | 关闭 Socket 失败；对象仍立即失效 |
| `XNET_ERROR_SOCKET_OPTION` | Socket 选项非法、不支持或系统操作失败 |
| `XNET_ERROR_SOCKET_BIND` | Socket 绑定失败 |
| `XNET_ERROR_SOCKET_LISTEN` | Socket 监听失败 |
| `XNET_ERROR_SOCKET_ACCEPT` | 接受连接失败 |
| `XNET_ERROR_SOCKET_CONNECT` | 发起或完成连接失败 |
| `XNET_ERROR_SOCKET_SHUTDOWN` | 半关闭失败 |
| `XNET_ERROR_SOCKET_READ` | Socket 接收失败 |
| `XNET_ERROR_SOCKET_WRITE` | Socket 发送失败 |
| `XNET_ERROR_PORT_CREATE` | 事件端口配置、后端选择或初始化失败 |
| `XNET_ERROR_PORT_CLOSE` | 事件端口资源关闭失败 |
| `XNET_ERROR_PORT_WATCH` | readiness 观察非法、超限或后端不支持 |
| `XNET_ERROR_PORT_WAIT` | 事件等待参数或平台调用失败 |
| `XNET_ERROR_PORT_POST` | 用户事件队列满、投递或唤醒失败 |
| `XNET_ERROR_PORT_SUBMIT` | 完成式 IO 提交失败 |
| `XNET_ERROR_PORT_CANCEL` | 完成式 IO 取消失败 |
| `XNET_ERROR_ENGINE_CREATE` | Engine 配置、Worker 或资源创建失败 |
| `XNET_ERROR_ENGINE_START` | Engine 启动失败 |
| `XNET_ERROR_ENGINE_STOP` | Engine 停止或对象排空失败 |
| `XNET_ERROR_ENGINE_POST` | Worker 命令投递失败 |
| `XNET_ERROR_ENGINE_TIMER` | 定时器参数、容量或操作失败 |
| `XNET_ERROR_STREAM_CONFIG` | TCP Stream 配置或 Worker 限定操作非法 |
| `XNET_ERROR_STREAM_CREATE` | TCP Stream 创建失败 |
| `XNET_ERROR_STREAM_CONNECT` | TCP Stream 异步连接失败 |
| `XNET_ERROR_STREAM_READ` | TCP Stream 读取、缓冲或协议消费失败 |
| `XNET_ERROR_STREAM_WRITE` | TCP Stream 发送或写半关闭失败 |
| `XNET_ERROR_STREAM_CLOSE` | TCP Stream 关闭失败 |
| `XNET_ERROR_LISTENER_CREATE` | TCP Listener 配置或创建失败 |
| `XNET_ERROR_LISTENER_ACCEPT` | TCP Listener 接受或分发失败 |
| `XNET_ERROR_LISTENER_CLOSE` | TCP Listener 关闭失败 |

完整示例位于 `examples/network/address/main.c` 和 `examples/network/buffer/main.c`。

## Socket 原语

`XRT_FEATURE_NET_SOCKET` 只依赖 `XRT_FEATURE_NET`。这一层拥有平台 Socket 句柄，但不包含事件循环、线程、协程、隐藏收发缓冲、TCP 重连或 UDP 队列；TCP/UDP 客户端与服务器模型在它和事件端口之上组合。

**错误与线程**：收发族返回 `xnetresult` 三态（`OK`/`AGAIN`/`CLOSED`/`ERROR`），系统调用失败经 `xrtGetError()` 报告且保留平台错误码（`XNET_ERROR_SOCKET_*` 域码 + `SystemCode`）。Socket 无内部锁——同一 Socket 的收发应由单执行流执行；`xnetsocket` 是不透明指针，句柄值可为 0（视同空）。`NATIVE` 后缀在 Windows 上即 IOCP 兼容句柄。

### `xrtNetSocketOpen`

打开一个流式或数据报 Socket。

```c
xnetsocket xrtNetSocketOpen(xnetfamily Family, xnetsockettype Type, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Family` | 输入 | `IPV4` 或 `IPV6` | — |
| `Type` | 输入 | `STREAM` 或 `DGRAM` | — |
| `iFlags` | 输入 | `XNET_SOCKET_NONBLOCK` 或 `0` | 非阻塞从创建即生效 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 成功返回的对象拥有原生句柄；配对 `xrtNetSocketClose()` 释放 | — |
| `NULL` | 参数非法或系统创建失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 族/类型非法
- `XNET_ERROR_SOCKET_OPEN`（系统错误）— 平台 `socket()` 失败，保留 `SystemCode`

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 双 UDP 套接字

```c
A = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0u);
B = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_DGRAM, 0u);
```

### `xrtNetSocketClose`

关闭原生句柄并销毁对象；即使系统关闭失败，对象也立即失效。

```c
bool xrtNetSocketClose(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 允许空指针（0） | 空指针是空操作；关闭后句柄不可再用 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 系统关闭成功或对象为空 | — |
| `false` | 系统关闭失败 | **对象仍已销毁**；错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_CLOSE`（系统错误）— `closesocket()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 收尾统一关闭

```c
xrtNetSocketClose(C);
xrtNetSocketClose(L);
```

### `xrtNetSocketNative`

返回借用的原生句柄，调用方不得自行关闭。

```c
intptr_t xrtNetSocketNative(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 句柄值 | 平台 SOCKET/fd；所有权仍在 XRT 对象 |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 创建后即验证非零

```c
(xrtNetSocketNative(A) == 0) ||
```

### `xrtNetSocketFamily`

返回 Socket 创建时确定的地址族。

```c
xnetfamily xrtNetSocketFamily(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 族枚举 | 与 `Open` 传入值一致 |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 创建后核对

```c
(xrtNetSocketFamily(A) != XNET_FAMILY_IPV4) ||
```

### `xrtNetSocketType`

返回 Socket 创建时确定的类型。

```c
xnetsockettype xrtNetSocketType(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 类型枚举 | 与 `Open` 传入值一致 |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 Family 成对核对

```c
(xrtNetSocketType(A) != XNET_SOCKET_DGRAM) ||
```

### `xrtNetSocketSet`

设置一个通用 Socket 选项。

```c
bool xrtNetSocketSet(xnetsocket Socket, xnetoption Option, int64 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `Option` | 输入 | 见 `xnetoption` 枚举 | 部分选项仅特定平台支持 |
| `iValue` | 输入 | — | 选项值；`NONBLOCK` 用 0/1 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已生效 | — |
| `false` | 参数非法、不支持或系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空对象或非法选项
- `XERR_UNSUPPORTED` — 平台不支持该选项
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `setsockopt()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 接收缓冲与逐字节读回

```c
!xrtNetSocketSet(A, XNET_OPTION_RECEIVE_BUFFER, 65536) ||
	!xrtNetSocketGet(A, XNET_OPTION_RECEIVE_BUFFER, &iValue) ||
```

### `xrtNetSocketGet`

查询一个通用 Socket 选项。

```c
bool xrtNetSocketGet(xnetsocket Socket, xnetoption Option, int64* pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `Option` | 输入 | — | — |
| `pValue` | 输出 | 非空 | 接收选项当前值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 参数非法、不支持或系统失败 | `*pValue` 不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针或非法选项
- `XERR_UNSUPPORTED` — 平台不支持
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `getsockopt()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 见 `xrtNetSocketSet` 范例

```c
!xrtNetSocketGet(A, XNET_OPTION_RECEIVE_BUFFER, &iValue) ||
	(iValue <= 0) ||
```

### `xrtNetSocketAvailable`

查询当前可立即读取的字节数；成功才修改输出。

```c
bool xrtNetSocketAvailable(xnetsocket Socket, size_t* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSize` | 输出 | 非空 | 空接收队列上成功返回 0 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出可读量 | — |
| `false` | 参数非法或系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_READ`（系统错误）— `ioctlsocket`/`ioctl` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 空队列上恰为 0

```c
!xrtNetSocketAvailable(A, &iGot) ||
	(iGot != 0u) ) {
```

### `xrtNetSocketBind`

把 Socket 绑定到本地地址；端口为零时由系统分配。

```c
bool xrtNetSocketBind(xnetsocket Socket, const xnetaddr* pAddress);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pAddress` | 输入 | 非空 | 通配地址绑定全部接口；实际端口由 `Local` 读回 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已绑定 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_BIND`（系统错误）— `bind()` 失败（端口占用等）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 通配绑定后 Local 读回实际端口

```c
!xrtNetSocketBind(A, &AddrA) ||
	!xrtNetSocketLocal(A, &AddrA) ||
```

### `xrtNetSocketListen`

把已绑定的流式 Socket 转为监听状态。

```c
bool xrtNetSocketListen(xnetsocket Socket, int iBacklog);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 必须已 `Bind` 且为 `STREAM` |
| `iBacklog` | 输入 | `> 0` | 等待连接队列长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已监听 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_LISTEN`（系统错误）— `listen()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 绑定→监听→接受

```c
!xrtNetSocketListen(Listener, 16) ||
```

### `xrtNetSocketAccept`

接受一个连接；非阻塞 Socket 暂无连接时返回 `AGAIN`。

```c
xnetresult xrtNetSocketAccept(xnetsocket Socket, xnetsocket* pClient, xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 监听中的流式 Socket |
| `pClient` | 输出 | 非空 | 接受成功时获得新建 Socket（调用方拥有） |
| `pRemote` | 输出 | 非空 | 接受成功时获得对端地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `OK` | 已接受，`*pClient`/`*pRemote` 已写出 | — |
| `AGAIN` | 非阻塞暂无连接 | 输出不被修改 |
| `ERROR` | 参数或系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_ACCEPT`（系统错误）— `accept()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 连接后立即接受

```c
(xrtNetSocketAccept(Listener,
	&Accepted, &Remote) != XNET_RESULT_OK) ||
```

### `xrtNetSocketConnect`

发起连接；非阻塞连接尚未完成时返回 `AGAIN`。

```c
xnetresult xrtNetSocketConnect(xnetsocket Socket, const xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pRemote` | 输入 | 非空 | 目标地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `OK` | 连接建立（含环回立即完成） | — |
| `AGAIN` | 非阻塞连接在途 | **不得二次调用**，交给 `FinishConnect` 轮询收口 |
| `ERROR` | 系统拒绝或失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_CONNECT`（系统错误）— `connect()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · OK/AGAIN 双路收口

```c
xnetresult ConnResult = xrtNetSocketConnect(C, &AddrA);

if ( (ConnResult != XNET_RESULT_OK) &&
	(ConnResult != XNET_RESULT_AGAIN) ) {
```

### `xrtNetSocketFinishConnect`

在可写事件到达后读取 `SO_ERROR`，完成非阻塞连接判定。

```c
xnetresult xrtNetSocketFinishConnect(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 处于在途连接的 Socket |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | 连接已建立 |
| `AGAIN` | 仍在途，继续轮询 |
| `ERROR` | 连接被拒或失败（`SO_ERROR`） |

#### 错误

- `XERR_ARGUMENT` — 空对象
- `XNET_ERROR_SOCKET_CONNECT`（系统错误）— 远端拒绝等

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 2000ms 轮询收口

```c
Result = xrtNetSocketFinishConnect(Socket);
```

### `xrtNetSocketShutdown`

半关闭指定方向，不销毁 Socket 对象。

```c
bool xrtNetSocketShutdown(xnetsocket Socket, xnetshutdown Direction);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `Direction` | 输入 | `READ`/`WRITE`/`BOTH` | `WRITE` 后对端读到 EOF |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已关闭指定方向 | — |
| `false` | 系统失败 | 对象仍可用；错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_SHUTDOWN`（系统错误）— `shutdown()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 客户端写半关

```c
if ( !xrtNetSocketShutdown(Client, XNET_SHUTDOWN_WRITE) ) {
```

### `xrtNetSocketLocal`

查询实际本地地址。

```c
bool xrtNetSocketLocal(xnetsocket Socket, xnetaddr* pAddress);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pAddress` | 输出 | 非空 | 接收本地地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 系统失败 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_NATIVE`（系统错误）— `getsockname()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 绑定后读回实际端口

```c
!xrtNetSocketLocal(A, &AddrA) ||
	(AddrA.Port == 0u) ||
```

### `xrtNetSocketRemote`

查询已连接的对端地址。

```c
bool xrtNetSocketRemote(xnetsocket Socket, xnetaddr* pAddress);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 须为已连接 Socket |
| `pAddress` | 输出 | 非空 | 接收对端地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写出 | — |
| `false` | 未连接或系统失败 | 输出不被修改；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_NATIVE`（系统错误）— `getpeername()` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 连接后与目标地址全等

```c
!xrtNetSocketRemote(A, &Remote) ||
	!xrtNetAddrEqual(&Remote, &DestB) ) {
```

### `xrtNetSocketSend`

单次发送；允许成功短写，非阻塞无法推进时返回 `AGAIN`。

```c
xnetresult xrtNetSocketSend(xnetsocket Socket, const void* pData, size_t iSize, size_t* pSent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输入 | `iSize > 0` 时非空 | 发送内容 |
| `iSize` | 输入 | — | 字节数 |
| `pSent` | 输出 | 非空 | 成功时接收实际发送量（可短写） |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | `*pSent` 字节已受理 |
| `AGAIN` | 非阻塞暂无法推进 |
| `ERROR` | 系统失败 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_WRITE`（系统错误）— `send()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 连接后即发送

```c
(xrtNetSocketSend(Client, "hello", 5,
	&iSize) != XNET_RESULT_OK) ||
```

### `xrtNetSocketRecv`

单次接收；流式 EOF 返回 `CLOSED`，非阻塞无数据返回 `AGAIN`。

```c
xnetresult xrtNetSocketRecv(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | `iSize > 0` 时非空 | 接收缓冲 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 成功时接收实际读取量 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | `*pReceived` 字节可用 |
| `AGAIN` | 非阻塞暂无数据 |
| `CLOSED` | 流式对端已 EOF（`*pReceived` 为 0） |
| `ERROR` | 系统失败 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_READ`（系统错误）— `recv()` 失败

#### 范例

[network/socket_tcp · 基础范例](../../examples/network/socket_tcp/main.c) · 服务端读取五字节

```c
(xrtNetSocketRecv(Accepted, sData, sizeof(sData) - 1,
	&iSize) != XNET_RESULT_OK) ) {
```

### `xrtNetSocketSendVec`

单次聚集发送；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketSendVec(xnetsocket Socket, const xnetspan* pSpans, size_t iCount, size_t* pSent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 连接式 |
| `pSpans` | 输入 | `iCount > 0` 时非空 | 只读 Span 数组 |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pSent` | 输出 | 非空 | 实际发送量 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `Send` |

#### 错误

- `XERR_ARGUMENT` — 空指针或 Span 数超限
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 两段 "xy"+"z" 三字节

```c
(xrtNetSocketSendVec(A, Out, 2u, &iSent) != XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvVec`

单次分散接收；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketRecvVec(xnetsocket Socket, xnetwspan* pSpans, size_t iCount, size_t* pReceived);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 连接式 |
| `pSpans` | 输出 | `iCount > 0` 时非空 | 可写 Span 数组 |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pReceived` | 输出 | 非空 | 实际读取量 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `CLOSED` / `ERROR` | 同 `Recv` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendVec 配对

```c
(xrtNetSocketRecvVec(B, In, 2u, &iGot) != XNET_RESULT_OK) ||
```

### `xrtNetSocketSendTo`

单次发送数据报；允许发送零长度数据报。

```c
xnetresult xrtNetSocketSendTo(xnetsocket Socket, const void* pData, size_t iSize, size_t* pSent, const xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输入 | 允许空指针（零长度报文） | 发送内容 |
| `iSize` | 输入 | — | 字节数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `Send` |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 消息形态三字节

```c
(xrtNetSocketSendTo(A, "msg", 3u, &iSent, &DestB) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvFrom`

单次接收数据报；零长度返回 `OK`，缓冲不足返回 `TRUNCATED`。

```c
xnetresult xrtNetSocketRecvFrom(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived, xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | `iSize > 0` 时非空 | 接收缓冲 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | `*pReceived` 字节可用（可为 0） |
| `AGAIN` | 非阻塞暂无数据 |
| `TRUNCATED` | 报文超过缓冲容量（余量被截去） |
| `ERROR` | 系统失败 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 多播自收一包

```c
McResult = xrtNetSocketRecvFrom(B, arrBuf, 8u, &iGot,
	&From);
```

### `xrtNetSocketSendToVec`

单次聚集发送数据报；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketSendToVec(xnetsocket Socket, const xnetspan* pSpans, size_t iCount, size_t* pSent, const xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输入 | `iCount > 0` 时非空 | 聚集 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `SendTo` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 两段 "ab"+"cd" 四字节

```c
if ( (xrtNetSocketSendToVec(A, Out, 2u, &iSent, &DestB) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvFromVec`

单次分散接收数据报；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketRecvFromVec(xnetsocket Socket, xnetwspan* pSpans, size_t iCount, size_t* pReceived, xnetaddr* pRemote);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输出 | `iCount > 0` 时非空 | 分散 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `TRUNCATED` / `ERROR` | 同 `RecvFrom` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendToVec 四字节配对

```c
(xrtNetSocketRecvFromVec(B, In, 2u, &iGot, &From) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketSendMsg`

发送数据报并覆盖本包源地址、接口、Hop Limit 或 Traffic Class。

```c
xnetresult xrtNetSocketSendMsg(xnetsocket Socket, const void* pData, size_t iSize, size_t* pSent, const xnetaddr* pRemote, const xnetdgramcontrol* pControl);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输入 | 允许空指针 | 发送内容 |
| `iSize` | 输入 | — | 字节数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |
| `pControl` | 输入 | 允许空指针 | 逐包控制；空表示无覆盖 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `SendTo` |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 多播自收一包（SendTo 同型路径）

```c
McResult = xrtNetSocketSendTo(A, "m", 1u, &iSent, &Group);
```

### `xrtNetSocketSendMsgVec`

聚集发送带逐包控制的数据报；Span 数量不能超过 64。

```c
xnetresult xrtNetSocketSendMsgVec(xnetsocket Socket, const xnetspan* pSpans, size_t iCount, size_t* pSent, const xnetaddr* pRemote, const xnetdgramcontrol* pControl);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输入 | `iCount > 0` 时非空 | 聚集 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pSent` | 输出 | 非空 | 实际发送量 |
| `pRemote` | 输入 | 非空 | 目标地址 |
| `pControl` | 输入 | 允许空指针 | 逐包控制 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 同 `SendTo` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 零控制双段四字节

```c
if ( (xrtNetSocketSendMsgVec(A, Out, 2u, &iSent, &DestB,
		&Control) != XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvMsg`

接收数据报及已启用的目标、接口、Hop Limit 和 Traffic Class 元数据。

```c
xnetresult xrtNetSocketRecvMsg(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived, xnetaddr* pRemote, xnetdgrammeta* pMeta);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | `iSize > 0` 时非空 | 接收缓冲 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |
| `pMeta` | 输出 | **非空** | 元数据结构；传 `NULL` 是参数错误——与 `RecvFrom` 的差异点 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `TRUNCATED` / `ERROR` | 同 `RecvFrom`；`Meta.Flags` 标记有效字段 |

#### 错误

- `XERR_ARGUMENT` — 空指针（含 `pMeta == NULL`）
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendTo 三字节配对

```c
(xrtNetSocketRecvMsg(B, arrBuf, 64u, &iGot, &From, &Meta) !=
		XNET_RESULT_OK) ||
	(iGot != 3u) ||
```

### `xrtNetSocketRecvMsgVec`

分散接收数据报及元数据，Span 数量不能超过 64。

```c
xnetresult xrtNetSocketRecvMsgVec(xnetsocket Socket, xnetwspan* pSpans, size_t iCount, size_t* pReceived, xnetaddr* pRemote, xnetdgrammeta* pMeta);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pSpans` | 输出 | `iCount > 0` 时非空 | 分散 Span |
| `iCount` | 输入 | `<= 64` | Span 数 |
| `pReceived` | 输出 | 非空 | 实际读取量 |
| `pRemote` | 输出 | 非空 | 发送方地址 |
| `pMeta` | 输出 | 非空 | 元数据结构 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `TRUNCATED` / `ERROR` | 同 `RecvFrom` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 SendMsgVec 四字节配对

```c
(xrtNetSocketRecvMsgVec(B, In, 2u, &iGot, &From, &Meta) !=
		XNET_RESULT_OK) ||
```

### `xrtNetSocketRecvBatch`

接收最多 64 个数据报；返回已消费前缀，每项独立记录 `OK` 或 `TRUNCATED`。

```c
xnetresult xrtNetSocketRecvBatch(xnetsocket Socket, xnetdgramrecv* pItems, size_t iCapacity, size_t* pReceived);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pItems` | 输出 | `iCapacity > 0` 时非空 | 调用方提供缓冲的接收项数组 |
| `iCapacity` | 输入 | `<= 64` | 最多接收数 |
| `pReceived` | 输出 | 非空 | 已到达前缀数（可能小于发送量） |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | 每项自带 `Result`/`Size`/`Remote` |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XNET_ERROR_SOCKET_READ`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 两包批量含部分到达补收

```c
if ( (xrtNetSocketSendBatch(A, Send, 2u, &iSent) != XNET_RESULT_OK) ||
	(iSent != 2u) ||
	(xrtNetSocketRecvBatch(B, Recv, 2u, &iGot) != XNET_RESULT_OK) ) {
```

### `xrtNetSocketSendBatch`

发送最多 64 个数据报；返回已经完整发送的输入前缀。

```c
xnetresult xrtNetSocketSendBatch(xnetsocket Socket, const xnetdgramsend* pItems, size_t iCount, size_t* pSent);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pItems` | 输入 | `iCount > 0` 时非空 | 调用期间借用数据 |
| `iCount` | 输入 | `<= 64` | 发送项数 |
| `pSent` | 输出 | 非空 | 已完整发送的项数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` / `AGAIN` / `ERROR` | `AGAIN` 表示队列暂满，稍后重试剩余 |

#### 错误

- `XERR_ARGUMENT` — 空指针或超限
- `XERR_IO` + `XNET_ERROR_SOCKET_WRITE` — 某项系统失败
- `XNET_ERROR_SOCKET_WRITE`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 双项批量（连接式空远端）

```c
if ( (xrtNetSocketSendBatch(A, Send, 2u, &iSent) != XNET_RESULT_OK) ||
```

### `xrtNetSocketDgramMetaAvailable`

返回当前平台和地址族可能提供的数据报接收元数据位。

```c
uint32 xrtNetSocketDgramMetaAvailable(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 平台可能支持的 `XNET_DGRAM_META_*` 位组合 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— 探测失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 Enabled 成对使用（见 DgramMetaSet 范例）

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramMetaEnabled`

返回 Socket 当前已经启用的数据报接收元数据位。

```c
uint32 xrtNetSocketDgramMetaEnabled(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 已启用位（默认 0） |

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 默认全零

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramMetaSet`

成功后精确设置接收元数据位。

```c
bool xrtNetSocketDgramMetaSet(xnetsocket Socket, uint32 iFlags);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 数据报 Socket |
| `iFlags` | 输入 | 合法元数据位组合 | 期望启用的位集 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已生效；`Enabled` 反映实际位 | — |
| `false` | 参数非法、平台不支持或系统失败 | 可查询 `Enabled` 看实际生效状态；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空对象
- `XERR_RANGE` — 含非法元数据位
- `XERR_UNSUPPORTED` — 平台不支持全部请求位
- `XNET_ERROR_SOCKET_OPTION`（系统错误）

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 启用 Hop Limit 后收包带 HopLimit

```c
if ( !xrtNetSocketDgramMetaSet(B, XNET_DGRAM_META_HOP_LIMIT) ||
```

### `xrtNetSocketDgramControlAvailable`

返回当前平台、地址族和 Socket Provider 可用的逐数据报发送控制位。

```c
uint32 xrtNetSocketDgramControlAvailable(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 可用的 `XNET_DGRAM_CONTROL_*` 位组合 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— 探测失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 与 DgramMetaAvailable 同族用法

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramCapabilities`

返回 PMTU、错误队列及后续高级数据报能力。

```c
uint32 xrtNetSocketDgramCapabilities(xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 非数据报 Socket 返回零 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 位集 | 数据报高级能力位；含 `XNET_OPTION_DGRAM_ERRORS` 可设性等 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— 探测失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 同族用法见 DgramMetaSet 范例

```c
(xrtNetSocketDgramMetaEnabled(A) != 0u) ||
```

### `xrtNetSocketDgramRecvError`

非阻塞读取一个已启用的异步数据报错误。

```c
xnetresult xrtNetSocketDgramRecvError(xnetsocket Socket, void* pData, size_t iSize, size_t* pReceived, xnetdgramerror* pError);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pData` | 输出 | 允许空指针 | 错误附带的原始数据 |
| `iSize` | 输入 | — | 缓冲容量 |
| `pReceived` | 输出 | 非空 | 附带数据量 |
| `pError` | 输出 | 非空 | 错误描述结构 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `OK` | 已读取一个排队错误 |
| `AGAIN` | 队列为空（Linux 上空队列返回本值） |
| `ERROR` | 平台不支持或失败（Windows 恒 `ERROR`："not supported on this platform"） |

#### 错误

- `XERR_UNSUPPORTED` — 平台无 `IP_RECVERR` 等价物

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 平台能力门控（Windows 不支持）

```c
xnetresult ErrResult = xrtNetSocketDgramRecvError(A,
	arrBuf, sizeof(arrBuf), &iGot, &DgramError);

if ( ErrResult == XNET_RESULT_OK ) {
```

### `xrtNetSocketMulticastJoin`

将数据报 Socket 加入一个同地址族多播组。

```c
bool xrtNetSocketMulticastJoin(xnetsocket Socket, const xnetaddr* pGroup, const xnetaddr* pInterface);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | 数据报 Socket |
| `pGroup` | 输入 | 非空、同族多播地址 | 目标组 |
| `pInterface` | 输入 | 非空 | 指定出接口；IPv6 使用 Scope 作为接口索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已加入 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `setsockopt(IP_ADD_MEMBERSHIP)` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · Loop+Hop+Iface+Join 全指环回

```c
!xrtNetSocketMulticastJoin(B, &Group, &Iface) ) {
```

### `xrtNetSocketMulticastLeave`

将数据报 Socket 移出一个多播组。

```c
bool xrtNetSocketMulticastLeave(xnetsocket Socket, const xnetaddr* pGroup, const xnetaddr* pInterface);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pGroup` | 输入 | 非空 | 已加入的组 |
| `pInterface` | 输入 | 非空 | 与加入时相同的接口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已移出 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空指针
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_DROP_MEMBERSHIP` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 自收验证后移出

```c
if ( !xrtNetSocketMulticastLeave(B, &Group, &Iface) ||
	!xrtNetSocketMulticastInterface(A, NULL) ) {
```

### `xrtNetSocketMulticastLoop`

设置数据报 Socket 是否接收自己发出的多播报文。

```c
bool xrtNetSocketMulticastLoop(xnetsocket Socket, bool bEnabled);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `bEnabled` | 输入 | — | 自收开关 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_MULTICAST_LOOP` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 自收前提

```c
!xrtNetSocketMulticastLoop(A, true) ||
```

### `xrtNetSocketMulticastHopLimit`

设置数据报 Socket 的多播跳数，合法范围为 0 到 255。

```c
bool xrtNetSocketMulticastHopLimit(xnetsocket Socket, int iHopLimit);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `iHopLimit` | 输入 | `[0, 255]` | 多播 TTL |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_MULTICAST_TTL` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 单跳自收

```c
!xrtNetSocketMulticastHopLimit(A, 1) ||
```

### `xrtNetSocketMulticastInterface`

选择多播发送接口；空接口恢复系统默认。

```c
bool xrtNetSocketMulticastInterface(xnetsocket Socket, const xnetaddr* pInterface);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Socket` | 输入 | 非空 | — |
| `pInterface` | 输入 | 允许空指针 | 空恢复默认；IPv6 使用 Scope 接口索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已设置 | — |
| `false` | 系统失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 空对象
- `XNET_ERROR_SOCKET_OPTION`（系统错误）— `IP_MULTICAST_IF` 失败

#### 范例

[socket_tour](../../examples/network/socket_tour/main.c) · 设环回接口并在收尾恢复默认

```c
!xrtNetSocketMulticastInterface(A, &Iface) ||
```

## 网络事件端口

`XRT_FEATURE_NET_PORT` 依赖 Socket、单调截止时间和 Mutex，提供 backend-neutral 事件端口核心；具体后端使用独立裁剪宏。`XRT_FEATURE_NET_PORT_SELECT` 提供全平台 select fallback，`XRT_FEATURE_NET_PORT_EPOLL` 提供 Linux 原生 readiness，`XRT_FEATURE_NET_PORT_KQUEUE` 提供 Darwin/BSD 原生 readiness，`XRT_FEATURE_NET_PORT_IOCP` 提供 Windows 原生完成式 IO，`XRT_FEATURE_NET_PORT_URING` 提供 Linux 原生完成式 IO。readiness 与 completion 使用同一组事件、截止时间、错误和所有权口径，但不会被强迫伪装成相同执行模型。

端口由创建线程拥有：`Watch`、提交、取消、等待和销毁只能由拥有线程执行；查询函数以及 `Post`、`Wake` 可跨线程调用，调用方仍须保证端口对象存活。

### Readiness 与 Completion

旧版 select/epoll/kqueue 为了模仿 IOCP，在每个后端中重复执行 recv/send、分配事件 Chain，并持有固定 8K 临时接收区。这既重复网络与缓冲逻辑，也让 TLS 等需要 `WANT_READ`/`WANT_WRITE` 的协议无法自然使用端口。

新端口通过能力位诚实区分两条路径：

```c
typedef enum xnetportcap {
	XNET_PORT_CAP_READINESS = 0x0001,
	XNET_PORT_CAP_COMPLETION = 0x0002,
	XNET_PORT_CAP_ONESHOT = 0x0004,
	XNET_PORT_CAP_EDGE = 0x0008,
	XNET_PORT_CAP_BATCH = 0x0010,
	XNET_PORT_CAP_WAKE = 0x0020,
	XNET_PORT_CAP_POST = 0x0040,
	XNET_PORT_CAP_CANCEL = 0x0080,
	XNET_PORT_CAP_READ_PROBE = 0x0100
} xnetportcap;
```

- select/epoll/kqueue 只报告 readiness；transport 收到事件后使用 `xrtNetSocketRecv` 和 `xrtNetBufReserve/Commit` 排空到 `AGAIN`。
- IOCP/io_uring 使用 completion；transport 将调用方拥有的 Span 直接提交给内核，完成后提交实际字节数。
- `READ_PROBE` 表示 completion 后端可以不持有载荷缓冲等待流变为可读；readiness 后端直接使用 `Watch`，不重复公开该能力。
- 上层按 `xrtNetPortCapabilities` 只在后端边界分流一次，不需要每个协议复制平台分支。
- 端口不拥有每连接缓冲，也不解析 TCP、UDP、TLS 或应用协议。

Windows 后端能力是分裂的：IOCP 只有 `COMPLETION`（`Watch` 提交返回 `XERR_UNSUPPORTED`），SELECT 只有 `READINESS`（完成式提交返回 `XERR_UNSUPPORTED`），`AUTO` 在 Windows 选择 IOCP；Linux 的 EPOLL/URING 二者兼备。

### `xrtNetPortConfigInit`

初始化端口配置为默认值：`Backend=AUTO`、`PostLimit=4096`、`WatchLimit=0`、`OperationLimit=0`、`OperationCache=64`。

```c
void xrtNetPortConfigInit(xnetportconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · 每次创建端口前重新初始化

```c
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_IOCP;
pIocp = xrtNetPortCreate(&Config);
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_SELECT;
pSelect = xrtNetPortCreate(&Config);
```

### `xrtNetPortCreate`

创建事件端口。`AUTO` 在当前已编译后端中选择最高能力实现：Windows 优先 IOCP，Linux 优先 epoll，Darwin/BSD 优先 kqueue，其他平台使用 select；显式指定的后端不可用时返回 `XERR_UNSUPPORTED`，不会静默换后端。

```c
xnetport* xrtNetPortCreate(const xnetportconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 通常为 `ConfigInit` 产物，可按需覆盖字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 端口已创建，创建线程即拥有线程 | — |
| `NULL` | 配置非法、后端不可用或资源不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_CREATE` — 配置指针为空或字段非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_CREATE` — 显式后端未编译进当前构建
- 内存分配失败 — 内部资源（唤醒通道、索引表）分配失败

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · 显式指定 IOCP 与 SELECT 各建一个

```c
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_IOCP;
pIocp = xrtNetPortCreate(&Config);
xrtNetPortConfigInit(&Config);
Config.Backend = XNET_PORT_SELECT;
pSelect = xrtNetPortCreate(&Config);
```

### `xrtNetPortDestroy`

取消并排空全部在途 IO，再销毁端口、观察、用户事件及唤醒资源；返回后系统不再引用任何调用方缓冲。只能由拥有线程调用。

```c
bool xrtNetPortDestroy(xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 允许空指针 | 空指针是空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 端口已销毁 | — |
| `false` | 参数非法、非拥有线程或底层关闭失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pPort` 非法
- `XERR_STATE` — 从非拥有线程调用
- `XERR_IO` + `XNET_ERROR_PORT_CLOSE` — 关闭后端句柄失败

#### 范例

[network/port_tour · 收尾](../../examples/network/port_tour/main.c) · 先关 Socket 再销毁端口

```c
if ( pSelect != NULL ) {
	xrtNetPortDestroy(pSelect);
}
if ( pIocp != NULL ) {
	xrtNetPortDestroy(pIocp);
}
```

### `xrtNetPortBackend`

返回端口实际启用的后端。

```c
xnetportbackend xrtNetPortBackend(const xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XNET_PORT_AUTO` | `pPort` 为空（`AUTO` 是零值，仅作失败哨兵） |
| `XNET_PORT_IOCP`/`XNET_PORT_URING`/`XNET_PORT_EPOLL`/`XNET_PORT_KQUEUE`/`XNET_PORT_SELECT` | 实际后端；`AUTO` 创建后此处是解析结果 |

#### 错误

- `XERR_ARGUMENT` — `pPort` 为空（返回值仍是 `XNET_PORT_AUTO`）

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · 校验显式后端未被替换

```c
if ( (pIocp == NULL) || (pSelect == NULL) ||
	(xrtNetPortBackend(pIocp) != XNET_PORT_IOCP) ||
	(xrtNetPortBackend(pSelect) != XNET_PORT_SELECT) ) {
	goto Cleanup;
}
```

### `xrtNetPortGetConfig`

返回已经解析 `AUTO` 容量和实际后端的有效配置。返回值是副本，修改它不影响端口。

```c
bool xrtNetPortGetConfig(
	const xnetport* pPort,
	xnetportconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |
| `pConfig` | 输出 | 非空 | 接收解析后的配置副本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | `*pConfig` 已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/port_tour · 自省](../../examples/network/port_tour/main.c) · 零值上限被解析为后端硬上限

```c
{
	xnetportconfig Resolved;

	if ( !xrtNetPortGetConfig(pIocp, &Resolved) ||
		(Resolved.Backend != XNET_PORT_IOCP) ) {
		goto Cleanup;
	}
}
```

### `xrtNetPortName`

返回静态后端名称（如 `"iocp"`、`"select"`），字符串存活期与进程相同。

```c
cstr xrtNetPortName(const xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 `cstr` | 静态后端名称 |
| `NULL` | `pPort` 为空 |

#### 错误

- `XERR_ARGUMENT` — `pPort` 为空

#### 范例

[network/port_iocp · 完成式](../../examples/network/port_iocp/main.c) · 诊断输出中打印后端名

```c
printf("backend=%s bytes=%zu data=%s\n",
	xrtNetPortName(pPort), Events[i].Bytes, sData);
```

### `xrtNetPortCapabilities`

返回实际后端能力位（`XNET_PORT_CAP_*` 的按位或）。

```c
uint32 xrtNetPortCapabilities(const xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 能力位组合 | `READINESS`/`COMPLETION`/`ONESHOT`/`EDGE`/`BATCH`/`WAKE`/`POST`/`CANCEL`/`READ_PROBE` 的按位或 |
| `0` | `pPort` 为空（无后端具备零能力，可作失败哨兵） |

#### 错误

- `XERR_ARGUMENT` — `pPort` 为空

#### 范例

[network/port_tour · 双后端](../../examples/network/port_tour/main.c) · IOCP 只有 COMPLETION，没有 READINESS

```c
iCaps = xrtNetPortCapabilities(pIocp);
if ( ((iCaps & XNET_PORT_CAP_COMPLETION) == 0u) ||
	((iCaps & XNET_PORT_CAP_READINESS) != 0u) ) {
	goto Cleanup;
}
```

### 配置与容量解析

`PostLimit`、`WatchLimit` 和 `OperationLimit` 分别约束跨线程用户事件、readiness 观察和原生在途 IO。两个零值表示按实际后端自动选择硬上限，不表示禁用：select 的观察上限为 1024，epoll/kqueue 为 65536，其他后端为 4096；IOCP 的在途操作上限为 65536，其他后端为 4096。显式非零值始终作为调用方选择的硬边界。

`OperationCache` 表示 completion 后端每个 256/512/1024/2048 字节尺寸类最多缓存的终态操作描述符数，零值完全关闭缓存。缓存只复用描述符和 Span 副本空间，不持有 Socket 或载荷缓冲；超过 2048 字节的罕见描述符直接使用堆。completion 操作 ID 索引从 16 个桶开始，活动操作超过两倍桶数时才逐级扩展到由 `OperationLimit` 确定的上限；因此为高并发配置较大的硬上限不会让空端口提前分配整张索引。扩展内存不足会让触发扩展的当前提交明确失败，已有在途操作保持有效。用户事件队列或操作队列满时返回 `XERR_AGAIN`；失败提交不会留下节点、幽灵事件或被系统继续引用的缓冲。`Wait` 每轮最多先提取输出容量的一半 Post；队列仍有积压时，下一轮先给原生后端一次非阻塞提取机会，因此即使调用方使用容量为一的事件数组，持续跨线程 Post 也不会永久饿死 Socket 完成或 readiness。已经从 Post 队列取出的事件不会因为同轮后端等待失败而丢失。

### 完成式提交契约

完成式 API 以 bytes 为基础，不创建隐藏 `chain`，也没有每对象 8K 或每数据报 64K 固定缓冲。后端在提交时复制 Span 描述符与地址，但不复制载荷；成功提交后，Socket、缓冲和只读发送数据必须保持有效且不变，直到同一 `Id` 的终态事件到达。`Id` 必须非零并在当前端口全部在途操作中唯一。每次成功提交恰好产生一个对应类型的终态；短读和短写由事件的 `Bytes` 表达，调用方决定是否继续提交。

终态失败位于事件的 `Result` 与 `SystemCode`，不把一次操作失败误报为端口等待失败。流接收零字节返回 `CLOSED|EOF`；零长度 UDP 报文返回 `OK`；UDP 缓冲不足返回 `TRUNCATED` 并保留实际写入长度、远端地址和已取得的元数据。事件的 `Address` 与 `Meta` 都是值对象，不借用平台控制缓冲。`Cancel` 只请求取消，成功后仍等待原操作唯一的 `CANCELLED` 终态；完成已经先于取消发生时，仍提取原完成。`Destroy` 会取消并排空全部在途操作，返回后系统不再引用调用方缓冲。

正常关闭顺序是取消并提取终态、关闭 Socket、销毁端口；也可以直接销毁端口来同步取消并排空在途 IO，再关闭相关 Socket，但这些 Socket 不能继续提交或改绑到另一个端口。IOCP 的 Socket 首次提交时永久关联当前完成端口，同一端口后续提交不再执行关联系统调用；Windows 不支持把该 Socket 改绑到另一个 IOCP。关联身份使用进程内单调 owner 标识，不使用可被分配器复用的上下文地址，因此旧端口销毁后仍不会把原 Socket 误认成新端口成员。

### `xrtNetPortAccept`

异步接受一个连接。成功提交后 `Accepted` Socket 由终态事件（`ACCEPT`）转移给调用方，调用方成为其唯一拥有者。

```c
bool xrtNetPortAccept(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已监听 | 必须是 `Listen` 状态的流 Socket |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已受理；`ACCEPT` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或 `Socket` 不是监听流
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力（如 SELECT）
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · `Event.Accepted` 是新 Socket

```c
!xrtNetPortConnect(pIocp, Client, &AddrListen, 201u, NULL) ||
!xrtNetPortAccept(pIocp, Listener, 202u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_CONNECT, 201u,
	&Event, 2000000ull) ||
(Event.Result != XNET_RESULT_OK) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_ACCEPT, 202u,
	&Event, 2000000ull) ||
(Event.Accepted == 0) ) {
	goto Cleanup;
}
```

### `xrtNetPortConnect`

异步连接远端地址。终态事件（`CONNECT`）到达前 Socket 必须保持有效。

```c
bool xrtNetPortConnect(xnetport* pPort, xnetsocket Socket,
	const xnetaddr* pRemote, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已打开 | 未连接的流 Socket |
| `pRemote` | 输入 | 非空 | 目标地址；族必须与 Socket 一致 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`CONNECT` 终态事件待提取，结果在 `Event.Result` | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或地址族与 Socket 不匹配
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 连接结果在终态事件的 `Result` 字段

```c
!xrtNetPortConnect(pIocp, Client, &AddrListen, 201u, NULL) ||
!xrtNetPortAccept(pIocp, Listener, 202u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_CONNECT, 201u,
	&Event, 2000000ull) ||
(Event.Result != XNET_RESULT_OK) ||
```

### `xrtNetPortReadProbe`

异步等待流 Socket 可读；不借用数据缓冲，终态（`READ_PROBE`，`Bytes == 0`）到达后再提交 `Recv` 才读取数据或确认 EOF。适合 TLS 等由上层状态机驱动读取的场景。

```c
bool xrtNetPortReadProbe(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 具备 `READ_PROBE` 能力的后端 |
| `Socket` | 输入 | 已连接流 | 只接受流 Socket |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；可读或 EOF 时产生 `READ_PROBE` 终态 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或 `Socket` 不是流
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力，或后端无法探询流可读性（无 `READ_PROBE` 能力）
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 探针终态后再提交 `Recv`

```c
if ( !xrtNetPortReadProbe(pIocp, Server, 203u, NULL) ||
	(xrtNetSocketSend(Client, "hi", 2u, &iSent) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_READ_PROBE, 203u,
		&Event, 2000000ull) ) {
	goto Cleanup;
}
```

### `xrtNetPortRecv`

异步接收到调用方缓冲；支持流和已连接数据报，单次最多 `INT_MAX` 字节。等价于单跨度 `RecvVec`，错误集与之相同。

```c
bool xrtNetPortRecv(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pData` | 输入/输出 | 借用至终态 | 接收缓冲，流接收必须非空 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`RECV` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、流接收缓冲为空或大小超限
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 请求字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 实际读取量在 `Event.Bytes`

```c
if ( !xrtNetPortRecv(pIocp, Server, arrBuf, 8u, 204u, NULL) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 204u,
		&Event, 2000000ull) ||
	(Event.Bytes != 2u) ||
	(memcmp(arrBuf, "hi", 2u) != 0) ||
```

### `xrtNetPortRecvVec`

异步分散接收；支持流和已连接数据报，Span 总长度最多 `INT_MAX` 字节。

```c
bool xrtNetPortRecvVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pSpans` | 输入/输出 | 借用至终态 | 可写跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`RECV` 终态事件待提取，`Bytes` 为全部跨度写入总量 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空、单跨度非法或流接收总长为零
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 流向量](../../examples/network/port_tour/main.c) · 两段分散接收

```c
if ( !xrtNetPortRecvVec(pIocp, Server, In, 2u, 210u, NULL) ||
	(xrtNetSocketSendVec(Client, Out, 2u, &iSent) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 210u,
		&Event, 2000000ull) ||
	(Event.Bytes != 4u) ||
```

### `xrtNetPortSend`

异步发送调用方缓冲；支持流和已连接数据报，单次最多 `INT_MAX` 字节。等价于单跨度 `SendVec`，错误集与之相同。

```c
bool xrtNetPortSend(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pData` | 输入 | 借用至终态 | 只读发送数据，期间不得修改 |
| `iSize` | 输入 | `<= INT_MAX` | 发送字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`SEND` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或发送缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 发送字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · TCP](../../examples/network/port_tour/main.c) · 发送完成即缓冲可复用

```c
!xrtNetPortSend(pIocp, Server, "ok", 2u, 205u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND, 205u,
	&Event, 2000000ull) ) {
	goto Cleanup;
}
```

### `xrtNetPortSendVec`

异步聚集发送；支持流和已连接数据报，Span 总长度最多 `INT_MAX` 字节。

```c
bool xrtNetPortSendVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已连接 | 流或已连接数据报 |
| `pSpans` | 输入 | 借用至终态 | 只读跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`SEND` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 流向量](../../examples/network/port_tour/main.c) · 两段聚集发送

```c
!xrtNetPortSendVec(pIocp, Server, Out, 2u, 211u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND, 211u,
	&Event, 2000000ull) ) {
	goto Cleanup;
}
```

### `xrtNetPortRecvFrom`

异步接收数据报；远端地址由终态事件（`RECV_FROM`）的 `Address` 字段返回。等价于单跨度 `RecvFromVec`，错误集与之相同。

```c
bool xrtNetPortRecvFrom(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接或已连接数据报 |
| `pData` | 输入/输出 | 借用至终态 | 接收缓冲 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数；不足时终态为 `TRUNCATED` |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`RECV_FROM` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 缓冲超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_iocp · 完成式](../../examples/network/port_iocp/main.c) · 终态事件带来源地址

```c
!xrtNetPortRecvFrom(pPort, Server,
	sData, sizeof(sData) - 1, 1, NULL) ||
```

### `xrtNetPortRecvFromVec`

异步分散接收数据报；缓冲不足由终态事件返回 `TRUNCATED`，`Bytes` 保留实际写入长度。

```c
bool xrtNetPortRecvFromVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接或已连接数据报 |
| `pSpans` | 输入/输出 | 借用至终态 | 可写跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`RECV_FROM` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报](../../examples/network/port_tour/main.c) · 来源端口在 `Event.Address.Port`

```c
if ( !xrtNetPortRecvFromVec(pIocp, UdpA, In, 2u, 220u, NULL) ||
	(xrtNetSocketSendTo(UdpB, "d1", 2u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_FROM, 220u,
		&Event, 2000000ull) ||
```

### `xrtNetPortRecvMsg`

异步接收数据报及 Socket 已启用的元数据；终态事件（`RECV_MSG`）同时返回 `Address` 与 `Meta`。等价于单跨度 `RecvMsgVec`，错误集与之相同。

```c
bool xrtNetPortRecvMsg(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已启用元数据 | 须先 `xrtNetSocketDgramMetaSet` 启用至少一位 |
| `pData` | 输入/输出 | 借用至终态 | 接收缓冲 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`RECV_MSG` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_PORT_SUBMIT` — Socket 未启用任何接收元数据
- `XERR_ARGUMENT` / `XERR_RANGE` / `XERR_UNSUPPORTED` / `XERR_STATE`（拥有线程）— 同 `RecvVec`
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`

#### 范例

[network/port_tour · 数据报](../../examples/network/port_tour/main.c) · 终态事件携带已启用的元数据位

```c
if ( !xrtNetPortRecvMsg(pIocp, UdpA, arrBuf, 16u, 221u, NULL) ||
	(xrtNetSocketSendTo(UdpB, "d2", 2u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_MSG, 221u,
		&Event, 2000000ull) ||
	(Event.Bytes != 2u) ||
```

### `xrtNetPortRecvMsgVec`

异步分散接收数据报及元数据；终态事件同时返回 `Address` 和 `Meta`。

```c
bool xrtNetPortRecvMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 已启用元数据 | 须先 `xrtNetSocketDgramMetaSet` 启用至少一位 |
| `pSpans` | 输入/输出 | 借用至终态 | 可写跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`RECV_MSG` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_PORT_SUBMIT` — Socket 未启用任何接收元数据
- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报](../../examples/network/port_tour/main.c) · 分散接收第三形态

```c
if ( !xrtNetPortRecvMsgVec(pIocp, UdpA, In, 2u, 222u, NULL) ||
	(xrtNetSocketSendTo(UdpB, "d3", 2u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV_MSG, 222u,
		&Event, 2000000ull) ||
```

### `xrtNetPortRecvError`

异步等待并读取一个数据报错误；终态（`RECV_ERROR`）同时返回原负载前缀和 `DgramError`。要求 Socket 已启用数据报错误队列，且后端支持等待该队列。

```c
bool xrtNetPortRecvError(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 支持数据报错误等待的后端 |
| `Socket` | 输入 | 已启用错误队列 | 须先 `xrtNetSocketDgramErrorSet` 启用 |
| `pData` | 输入/输出 | 借用至终态 | 接收原负载前缀的缓冲 |
| `iSize` | 输入 | `<= INT_MAX` | 缓冲字节数 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`RECV_ERROR` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_PORT_SUBMIT` — Socket 未启用数据报错误队列
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — Socket 不支持错误队列（平台限制），或后端无法等待数据报错误（如 IOCP）
- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 缓冲超过 `INT_MAX`
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 平台门控](../../examples/network/port_tour/main.c) · Windows IOCP 上提交被拒是预期行为

```c
if ( xrtNetPortRecvError(pIocp, UdpA, arrBuf, 16u, 300u, NULL) ) {
	goto Cleanup;
}
```

### `xrtNetPortSendTo`

异步发送数据报；远端地址在提交时复制，提交返回后可立即复用地址对象。等价于单跨度 `SendToVec`，错误集与之相同。

```c
bool xrtNetPortSendTo(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接数据报 Socket |
| `pData` | 输入 | 借用至终态 | 只读发送数据 |
| `iSize` | 输入 | `<= INT_MAX` | 发送字节数，允许零长度报文 |
| `pRemote` | 输入 | 非空 | 目标地址；族与 Socket 一致，提交时复制 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`SEND_TO` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 发送字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_uring · 完成式](../../examples/network/port_uring/main.c) · 提交后地址对象即可复用

```c
!xrtNetPortSendTo(
	pPort,
	Client,
	"completion",
	10,
	&Address,
	2,
	NULL
 ) ||
```

### `xrtNetPortSendToVec`

异步聚集发送数据报；远端地址和 Span 描述符在提交时复制。

```c
bool xrtNetPortSendToVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 未连接数据报 Socket |
| `pSpans` | 输入 | 借用至终态 | 只读跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `pRemote` | 输入 | 非空 | 目标地址；提交时复制 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；`SEND_TO` 终态事件待提取 | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报发送](../../examples/network/port_tour/main.c) · 两段聚集发送

```c
if ( !xrtNetPortSendToVec(pIocp, UdpB, Out, 2u, &DestUdpA,
		230u, NULL) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND_TO, 230u,
		&Event, 2000000ull) ||
	(Event.Result != XNET_RESULT_OK) ||
```

### `xrtNetPortSendMsg`

异步发送带逐包控制的数据报；地址和控制值在提交时复制。非零控制 `Flags` 的终态为 `SEND_MSG`；空控制或零 `Flags` 走普通发送路径，有 `pRemote` 时终态为 `SEND_TO`，否则为 `SEND`（Socket 须已连接）。等价于单跨度 `SendMsgVec`，错误集与之相同。

```c
bool xrtNetPortSendMsg(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 数据报 Socket |
| `pData` | 输入 | 借用至终态 | 只读发送数据 |
| `iSize` | 输入 | `<= INT_MAX` | 发送字节数 |
| `pRemote` | 输入 | 可空 | 目标地址；空表示 Socket 已连接 |
| `pControl` | 输入 | 可空 | 逐包控制；空或零 `Flags` 走普通发送 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；终态类型按控制与地址组合（见上文） | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配或缓冲/大小非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 发送字节数超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_iocp · 完成式](../../examples/network/port_iocp/main.c) · 逐包覆盖源地址发送

```c
!xrtNetPortSendMsg(pPort, Client,
	"completion", 10, &Address, &Control, 2, NULL) ||
```

### `xrtNetPortSendMsgVec`

异步聚集发送带逐包控制的数据报；Span 描述符、地址和控制值在提交时复制。终态类型与 `SendMsg` 相同。

```c
bool xrtNetPortSendMsgVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | completion 后端端口 |
| `Socket` | 输入 | 数据报 | 数据报 Socket |
| `pSpans` | 输入 | 借用至终态 | 只读跨度数组；提交时复制描述符 |
| `iCount` | 输入 | `> 0` | 跨度数量 |
| `pRemote` | 输入 | 可空 | 目标地址；空表示 Socket 已连接 |
| `pControl` | 输入 | 可空 | 逐包控制；空或零 `Flags` 走普通发送 |
| `Id` | 输入 | 非零且唯一 | 终态事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入终态事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；终态类型按控制与地址组合（同 `SendMsg`） | — |
| `false` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_SUBMIT` — 参数非法、地址族不匹配、跨度数组为空或单跨度非法
- `XERR_RANGE` + `XNET_ERROR_PORT_SUBMIT` — 跨度总长超过 `INT_MAX`
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT` — 后端没有 completion 能力
- `XERR_AGAIN` + `XNET_ERROR_PORT_SUBMIT` — 在途操作数达到 `OperationLimit`
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 数据报发送](../../examples/network/port_tour/main.c) · 零 `Flags` 控制走 `SEND_TO` 路径

```c
!xrtNetPortSendMsgVec(pIocp, UdpB, Out, 2u, &DestUdpA,
	&Control, 231u, NULL) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_SEND_TO, 231u,
	&Event, 2000000ull) ||
(Event.Result != XNET_RESULT_OK) ) {
	goto Cleanup;
}
```

### `xrtNetPortCancel`

请求取消指定 `Id` 的在途操作。操作仍以一个终态事件结束：取消成功时为 `CANCELLED`，完成先于取消发生时仍是原完成。

```c
bool xrtNetPortCancel(xnetport* pPort, uint64 Id);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 支持取消的后端 |
| `Id` | 输入 | 非零 | 要取消的在途操作 ID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 取消请求已受理；仍须等待该操作唯一终态 | — |
| `false` | 参数非法、后端不支持取消或非拥有线程 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_CANCEL` — `pPort`/`Id` 非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_CANCEL` — 后端不支持取消
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · 取消](../../examples/network/port_tour/main.c) · 在途 `Recv` 以 `CANCELLED` 终结

```c
if ( !xrtNetPortRecv(pIocp, UdpA, arrBuf, 8u, 600u, NULL) ||
	!xrtNetPortCancel(pIocp, 600u) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_RECV, 600u,
		&Event, 2000000ull) ||
	(Event.Result != XNET_RESULT_CANCELLED) ) {
```

### 观察与等待（readiness）

一个端口对一个 Socket 保留一份 readiness 观察。`Watch` 替换关注位和事件身份，零关注位等价于 `Unwatch`；`Unwatch` 幂等。`Id` 与 `User` 原样进入事件，端口不拥有用户上下文。错误和挂断由后端隐式观察，不需要加入关注掩码。

readiness 采用 one-shot 契约：已报告的读写方向自动清除，transport 排空或推进状态机后显式重新观察。这样 level、edge 和 completion 后端都不会因未消费状态持续空转。`Wait` 使用 `xrtClock` 单调微秒截止时间；定时器不再由每个后端各自维护链表，后续 engine 使用统一最小堆，并把最近截止时间直接传给 `Wait`。

`Watch`、`Unwatch` 和 `Wait` 属于端口拥有线程。关闭 Socket 前必须先移除仍在生效的观察，销毁端口前必须停止所有生产者。

### `xrtNetPortWatch`

替换一个 Socket 的 readiness 关注位；事件为零等价于 `Unwatch`。每 Socket 单份观察，重复 `Watch` 覆盖旧关注位与事件身份。

```c
bool xrtNetPortWatch(xnetport* pPort, xnetsocket Socket,
	uint64 Id, uint32 iEvents, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | readiness 后端端口 |
| `Socket` | 输入 | 已打开 | 要观察的 Socket |
| `Id` | 输入 | 非零 | 事件身份；替换观察时同时替换 |
| `iEvents` | 输入 | `XNET_PORT_EVENT_*` 位 | 关注方向；`READ`/`WRITE` 等，零等价 `Unwatch` |
| `pUser` | 输入 | 任意值 | 原样进入事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 内核观察与用户身份均已登记 | — |
| `false` | 未登记 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_WATCH` — 参数非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_WATCH` — 后端没有 readiness 能力（如 IOCP）
- `XERR_RANGE` + `XNET_ERROR_PORT_WATCH` — 观察数达到 `WatchLimit`（select 还受 `FD_SETSIZE` 约束）
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · readiness](../../examples/network/port_tour/main.c) · SELECT 端口上观察读方向

```c
if ( !xrtNetPortWatch(pSelect, UdpA, 100u, XNET_PORT_EVENT_READ,
		NULL) ||
	(xrtNetSocketSendTo(UdpB, "r", 1u, &iSent, &DestUdpA) !=
		XNET_RESULT_OK) ||
	!exampleWaitFor(pSelect, XNET_PORT_EVENT_READY, 100u,
		&Event, 2000000ull) ||
	!xrtNetPortUnwatch(pSelect, UdpA) ) {
	goto Cleanup;
}
```

### `xrtNetPortUnwatch`

幂等移除观察。失败也会退休用户身份，调用方随后必须关闭该 Socket，不能继续观察或执行 IO。

```c
bool xrtNetPortUnwatch(xnetport* pPort, xnetsocket Socket);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | readiness 后端端口 |
| `Socket` | 输入 | 已打开 | 要移除观察的 Socket |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 内核观察和用户身份均已移除 | — |
| `false` | 移除失败，但用户身份仍已退休；须立即关闭该 Socket | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_WATCH` — 参数非法
- `XERR_UNSUPPORTED` + `XNET_ERROR_PORT_WATCH` — 后端没有 readiness 能力
- `XERR_STATE` — 从非拥有线程调用

#### 范例

[network/port_tour · readiness](../../examples/network/port_tour/main.c) · 消费事件后移除观察

```c
!exampleWaitFor(pSelect, XNET_PORT_EVENT_READY, 100u,
	&Event, 2000000ull) ||
!xrtNetPortUnwatch(pSelect, UdpA) ) {
	goto Cleanup;
}
```

### `xrtNetPortWait`

等待到事件、截止时间或错误；成功和超时都会先清零 `*pCount`。只能由拥有线程调用。

```c
xnetresult xrtNetPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	xdeadline iDeadline, size_t* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |
| `pEvents` | 输出 | 非空数组 | 事件输出缓冲 |
| `iCapacity` | 输入 | `> 0` | 输出容量；后端一次最多写入这么多事件 |
| `iDeadline` | 输入 | 单调微秒 | `xrtClock` 截止时间；`xrtDeadlineAfter`/`xrtDeadlineNever` |
| `pCount` | 输出 | 非空 | 实际写入事件数；进入时即清零 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XNET_RESULT_OK` | 提取到 `*pCount > 0` 个事件 | — |
| `XNET_RESULT_TIMEOUT` | 到达截止时间，`*pCount == 0` | — |
| `XNET_RESULT_ERROR` | 参数非法、非拥有线程或后端等待失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_WAIT` — 缓冲为空、容量为零或 `pCount` 为空
- `XERR_STATE` — 从非拥有线程调用
- `XERR_IO` + `XNET_ERROR_PORT_WAIT` — 后端等待系统调用失败

#### 范例

[network/port_tour · 等待辅助](../../examples/network/port_tour/main.c) · 截止时间分片轮询

```c
if ( xrtNetPortWait(pPort, Events, 8u,
		xrtDeadlineAfter(iTimeoutUs / 100u),
		&iCount) != XNET_RESULT_OK ) {
	continue;
}
```

### 用户事件与唤醒

每次成功 `Post` 产生一个 FIFO `USER` 事件，不会合并。`Wake` 只请求一个可合并的 `WAKE` 事件，适合“命令队列已有工作”通知；连续 Wake 不会让 worker 重复空转。端口会合并底层通知而不合并用户事件，因此突发的数千次 `Post` 不需要执行同等数量的唤醒系统调用。事件入队与首次底层通知在同一临界区原子成立，`Post` 返回 `false` 时不会留下随后仍可提取的幽灵事件。select 后端使用两个非阻塞 UDP Socket 形成全平台唤醒通道；epoll 使用 `EFD_NONBLOCK|EFD_CLOEXEC` eventfd，并为不支持原子标志的旧内核补设 `O_NONBLOCK` 与 `FD_CLOEXEC`；kqueue 使用可合并的 `EVFILT_USER/NOTE_TRIGGER`，不再创建旧版 pipe。三者都只承载通知，不承载用户事件本体。

### `xrtNetPortPost`

跨线程投递一个不会合并的用户事件（`USER`）。`Id` 与 `pUser` 原样进入事件。

```c
bool xrtNetPortPost(xnetport* pPort, uint64 Id, ptr pUser);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针；跨线程调用仍须保证对象存活 |
| `Id` | 输入 | 非零 | 事件身份 |
| `pUser` | 输入 | 任意值 | 原样进入事件 `User` 字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 事件已入队，等待中必然可提取 | — |
| `false` | 未入队，不留幽灵事件 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_POST` — `pPort`/`Id` 非法
- `XERR_STATE` + `XNET_ERROR_PORT_POST` — 端口正在关闭
- `XERR_AGAIN` + `XNET_ERROR_PORT_POST` — 用户事件队列达到 `PostLimit`

#### 范例

[network/port_tour · 用户事件](../../examples/network/port_tour/main.c) · `USER` 事件携带 `Id`

```c
if ( !xrtNetPortPost(pIocp, 777u, NULL) ||
	!exampleWaitFor(pIocp, XNET_PORT_EVENT_USER, 777u,
		&Event, 2000000ull) ||
```

### `xrtNetPortWake`

跨线程请求一个可合并的 `WAKE` 事件；连续请求只产生一个事件。适合“命令队列已有工作”式通知。

```c
bool xrtNetPortWake(xnetport* pPort);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPort` | 输入 | 非空 | 端口指针 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已有挂起 `WAKE` 或新请求了一个 | — |
| `false` | 未请求 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_PORT_POST` — `pPort` 非法
- `XERR_STATE` + `XNET_ERROR_PORT_POST` — 端口正在关闭

#### 范例

[network/port_tour · 唤醒](../../examples/network/port_tour/main.c) · `WAKE` 事件 `Id` 为零

```c
!xrtNetPortWake(pIocp) ||
!exampleWaitFor(pIocp, XNET_PORT_EVENT_WAKE, 0u,
	&Event, 2000000ull) ) {
	goto Cleanup;
}
```

### Io_uring 边界

io_uring 是 Linux 原生 completion 后端，能力为 completion、原生取消、read probe、batch completion、wake 和 post。它直接映射稳定 Linux UAPI，不依赖 liburing；在提交时复制 `iovec`、地址描述符和显式发送控制值，载荷始终借用调用方内存。接收和发送统一使用 `RECVMSG`/`SENDMSG`，读探针使用 `POLL_ADD`，因此流、未连接 UDP、已连接 UDP、零长度报文、逐包发送控制与截断共用一套完成映射，不引入固定 2K 流缓冲或固定 64K 数据报缓冲。

普通操作只在用户态 SQ 中依次保留并发布 SQE，不为每条操作单独调用 `io_uring_enter`。
达到 SQ 容量、进入 `Wait` 或提交取消前，端口一次发布整批待提交项；内核只接受部分
SQE 或系统调用被信号中断时会继续提交剩余项。提交函数成功表示操作已经进入端口的
有序提交队列，后续批量系统调用失败由下一次 `Wait` 作为端口错误报告。取消会先发布
此前普通操作，再立即发布取消 SQE，保持“先提交、后取消”的顺序，不会让取消越过仍在
用户态 SQ 中的原操作。该批处理只合并系统调用，不改变每个操作唯一终态、缓冲借用期或
完成顺序契约。

创建端口时必须同时具有 `IORING_FEAT_NODROP`、`IORING_FEAT_FAST_POLL`，并通过 probe 确认 `POLL_ADD`、`SENDMSG`、`RECVMSG`、`ACCEPT`、`ASYNC_CANCEL` 与 `CONNECT`；能力不足会明确返回 `XERR_UNSUPPORTED`。环使用单生产者 SQ 与单消费者 CQ，提交、取消和等待属于 owner 线程；`Post` 与 `Wake` 通过非阻塞 `eventfd` 可跨线程且唤醒可合并。取消 SQE 的 `user_data` 使用带标记的操作令牌；原操作 CQE 和取消控制 CQE 都到达前，描述符退出公开活动索引但不会释放或进入缓存，避免地址复用后迟到取消误命中新操作。控制 CQE 不占公共事件容量，也不产生第二个用户终态。映射前会按内核返回的全部 SQ/CQ 偏移检查加法和数组乘法，32 位构建不会因尺寸回绕映射过小区域；映射后还会校验 entries 与 mask 的幂次契约。

`OperationLimit` 对 io_uring 的最大值为 32768，CQ 至少按活动操作与取消控制完成的总量配置。SQ dropped 或 CQ overflow 被视为端口一致性故障，不会继续交付可能缺失的终态。销毁会先请求取消并排空全部活动操作；只有端口进入不可恢复故障时才关闭 ring，利用内核关闭语义同步撤销剩余引用。

当前 `AUTO` 仍在 Linux 选择 epoll。io_uring 必须由 `XNET_PORT_URING` 显式选择，待 Linux 真实运行期的 TCP、UDP、取消、慢端、OOM 和长稳压力门禁在发布 runner 上持续稳定后再提升默认优先级；交叉编译和链接证据不冒充运行期证据。基础、OOM、预提交批量压力、示例和单头文件入口分别位于 `tests/network/test_net_port_uring*.c`、`examples/network/port_uring/main.c` 与 `tests/single/test_single_net_port_uring.c`。TCP、UDP、Future、Dial、慢对端和并发关闭沿用原有 transport 契约测试，Linux io_uring 入口为 `tests/network/test_net_*_uring.c`；单头 transport 入口为 `tests/single/test_single_net_tcp_uring.c` 与 `tests/single/test_single_net_udp_uring.c`，不复制另一套测试逻辑。

### Epoll 边界

epoll 是 Linux Tier A readiness 后端，能力为 readiness、内核 one-shot、batch wait、wake 和 post，不伪装 completion，也不在后端中执行 `recv`、`send` 或分配载荷缓冲。观察索引从 16 个桶开始，活动观察超过两倍桶数时才逐级扩展到由 `WatchLimit` 确定的上限，新增、替换和移除的平均复杂度为 O(1)；配置较大硬上限不会提高空端口成本。单次 `epoll_wait` 最多提取 256 个内核事件，调用方容量更小时严格服从调用方容量。

内核注册使用 level-triggered `EPOLLONESHOT`。一次事件只清除真正报告的读写方向，未报告方向会立即重新武装；`EPOLLHUP`、`EPOLLRDHUP` 会同时推进受关注的方向，使 transport 能执行最终读写并观察 EOF。每次 `Watch` 都生成包含 fd 与代际的内部令牌，迟到事件无法错配给被替换的身份或复用同一 fd 的新 Socket。`EPOLLERR` 只报告错误标志，不提前读取会清除待处理错误的 `SO_ERROR`；非阻塞连接由 `xrtNetSocketFinishConnect` 唯一读取并判定真实结果。观察节点只保存 Socket、身份、令牌和掩码，不包含旧版固定 8K 接收区，也不产生“临时接收区再复制到链”的第二次复制。`epoll_create1` 不可用时回退到 `epoll_create` 并显式设置 `FD_CLOEXEC`。

epoll 基础、OOM、数百 Socket 批量压力和单头文件回归分别位于 `tests/network/test_net_port_epoll*.c` 与 `tests/single/test_single_net_port_epoll.c`。

### Kqueue 边界

kqueue 是 Darwin 与 FreeBSD、OpenBSD、NetBSD、DragonFly BSD 的 Tier A readiness 后端，能力为 readiness、内核 one-shot、batch wait、wake 和 post。读写方向使用独立 `EV_ONESHOT` 过滤器，因此一个方向触发不会误删另一个方向；同一次 `kevent` 返回的同观察读写事件会合并为一个公共事件。`EV_EOF` 映射为挂断并推进对应方向，非零 `fflags` 与 `EV_ERROR` 保存到 `SystemCode`。Apple、FreeBSD 和 NetBSD 在头文件提供能力时使用 `EVFILT_USER` 唤醒；OpenBSD、DragonFly BSD 以及缺少该过滤器的目标使用非阻塞、禁止继承的 pipe，写满按已有唤醒合并，读事件会完整排空到 `EAGAIN`。

观察表分别按原生描述符和整数代际令牌建立有界哈希索引，两张索引均从 16 个桶开始并同步动态扩展，新增、替换、移除和迟到事件校验平均为 O(1)。内核 `udata` 不保存可释放节点指针；替换或 fd 复用后，旧令牌事件无法访问新对象。替换过滤器失败会优先恢复旧集合；若内核连恢复也失败，则尽力删除全部过滤器并移除用户态观察，绝不保留两边分叉的状态。OOM 或首次注册失败不会留下可见观察。后端不执行 Socket IO，不保存发送向量，不分配载荷 Chain，也没有旧版固定 8K 接收区。

kqueue 基础、OOM、数百 Socket 批量压力和单头文件回归分别位于 `tests/network/test_net_port_kqueue*.c` 与 `tests/single/test_single_net_port_kqueue.c`。当前 Windows 开发机只执行其不可用契约；Darwin/BSD 运行期结果属于对应平台发布门禁。

### Select 边界

select 是 Tier C fallback，能力为 readiness、one-shot、batch wait、wake 和 post，不宣称 completion 或 edge。Windows 的 `fd_set` 受 `FD_SETSIZE` 数量限制，内部唤醒 Socket 占一个槽位；POSIX 同时要求原生 fd 小于 `FD_SETSIZE`。用户 Socket 超限在 `Watch` 时确定性失败；进程已有大量文件导致内部唤醒 fd 无法表示时，端口直接在 `Create` 阶段返回 `XNET_ERROR_PORT_CREATE`，两条路径都不会进入越界的 `FD_SET`。高连接数部署应选择 IOCP、io_uring、epoll 或 kqueue。

select、epoll、kqueue、IOCP 和 io_uring 示例分别位于 `examples/network/port_select/main.c`、`examples/network/port_epoll/main.c`、`examples/network/port_kqueue/main.c`、`examples/network/port_iocp/main.c` 与 `examples/network/port_uring/main.c`。

## 嵌入式任务投递

`xnetpost` 是嵌入调用方结构的无分配任务节点：`xrtNetPostInit` 初始化，`xrtNetPost` 投递到指定 Worker 队列，出队前 `xrtNetPostPending` 为真。与 `xrtNetEnginePost` 相比它不分配节点，适合高频、固定生命周期的任务；同一 `xnetpost` 在出队并执行完成前不能再次投递。调用方必须通过网络对象引用保证 Worker 和 `xnetpost` 存活到回调返回。

### `xrtNetPostInit`

初始化一个尚未投递的嵌入式 Post；清零节点并写入内部魔数。

```c
bool xrtNetPostInit(xnetpost* pPost);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPost` | 输出 | 非空 | 嵌入调用方结构的 `xnetpost` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 节点已就绪，可投递 | — |
| `false` | `pPost` 非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pPost` 为空或范围非法

#### 范例

[network/engine_tour · 嵌入式 Post](../../examples/network/engine_tour/main.c) · 初始化后立即投递

```c
if ( !xrtNetPostInit(&Post) ||
	!xrtNetPost(pWorker0, &Post,
		exampleSimpleTask, (ptr)&bTaskDone) ) {
	goto Cleanup;
}
```

### `xrtNetPostPending`

判断嵌入式 Post 是否仍在 Worker 队列中等待执行；执行完成后清除。

```c
bool xrtNetPostPending(const xnetpost* pPost);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPost` | 输入 | 非空 | 已 `PostInit` 的节点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理投递、尚未执行 | — |
| `false` | 已执行完成，或从未投递成功 | 参数非法/状态非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pPost` 为空或范围非法
- `XERR_STATE` — 节点未经 `xrtNetPostInit` 初始化（魔数不匹配）

#### 范例

[network/engine_tour · 嵌入式 Post](../../examples/network/engine_tour/main.c) · 受理为真，执行后为假

```c
if ( !xrtNetPostPending(&Post) ) {
	goto Cleanup;
}
if ( !exampleSpinUntil(&bTaskDone, 2000u) ||
	xrtNetPostPending(&Post) ) {
	goto Cleanup;
}
```

### `xrtNetPost`

无分配地投递到指定 Worker；同一 Post 在出队前不能再次投递。从 Worker 自身线程调用时直接入队不阻塞，跨线程投递经有界提交段进入。受理失败时节点状态回滚，不留半提交状态。

```c
bool xrtNetPost(
	xnetworker* pWorker,
	xnetpost* pPost,
	xnettaskproc pProc,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 目标 Worker；须保持存活到回调返回 |
| `pPost` | 输入/输出 | 已 `PostInit` | 嵌入式任务节点 |
| `pProc` | 输入 | 非空 | 任务回调，在亲和 Worker 上执行一次 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；回调必在目标 Worker 上执行一次 | — |
| `false` | 未受理，节点无残留状态 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker`/`pPost`/`pProc` 为空或非法
- `XERR_STATE` — `pPost` 未经 `xrtNetPostInit` 初始化，或仍在队列中等待（不能重复投递）
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_POST` — Worker 未运行、不再受理投递或关停已封口

#### 范例

[network/engine_tour · 嵌入式 Post](../../examples/network/engine_tour/main.c) · 投递固定任务并自旋等待

```c
if ( !xrtNetPostInit(&Post) ||
	!xrtNetPost(pWorker0, &Post,
		exampleSimpleTask, (ptr)&bTaskDone) ) {
	goto Cleanup;
}
```

## 网络 Engine

启用 `XRT_FEATURE_NET_ENGINE` 后，Engine 在事件端口之上提供固定 Worker、
有界跨线程命令、可取消 Timer 和统一 Completion 分发。它不实现 TCP、UDP、DNS
或应用协议，因此自定义传输与 XRT 高层传输可以建立在同一套运行时契约上。

标准 Engine 的平台闭包为 Windows `IOCP + select`、Linux `epoll + select`、Darwin/BSD `kqueue + select`、其他 POSIX 平台当前 `select`。Linux 的精细裁剪构建也允许 `io_uring + select`，不要求同时编入 epoll；此时调用方必须显式选择 `XNET_PORT_URING`，`AUTO` 仍只在已经编译的稳定默认后端中选择。显式 `XNET_PORT_SELECT` 可用于受限环境、诊断和后端差异回归；具体端口后端仍可脱离 Engine 独立裁剪。

### 配置

`xrtNetEngineConfigInit` 初始化 `xnetengineconfig`。配置没有版本字段，也没有旧版
兼容分支。

| 字段 | 默认值 | 契约 |
| --- | ---: | --- |
| `Backend` | `XNET_PORT_AUTO` | 每个 Worker 使用的端口后端 |
| `Workers` | `0` | 自动取在线处理器数，自动值最多 64，显式值最多 256 |
| `BufferPool` | `NULL` | 每 Worker 自适应缓冲池配置；空指针使用网络缓冲默认值 |
| `CommandCapacity` | 4096 | 每 Worker MPSC 命令容量，向上取整为 2 次幂 |
| `NodeCacheBytes` | 64 KiB | 每 Worker 命令、Timer 与协议小节点的共享缓存预算；零关闭缓存 |
| `TimerLimit` | 65536 | 已受理但尚未终结的 Timer 硬上限；同时受目标平台指针数组可表示长度约束 |
| `EventBatch` | 128 | 每 Worker 的端口事件批容量，最大 4096 |
| `PortPostLimit` | 4096 | 端口用户事件硬上限 |
| `PortWatchLimit` | 0 | readiness 观察硬上限；零由每个 Worker 的实际后端自动选择 |
| `PortOperationLimit` | 0 | completion 在途操作硬上限；零由每个 Worker 的实际后端自动选择 |
| `PortOperationCache` | 64 | 每 Worker、每 completion 操作尺寸类的缓存上限；零关闭 |
| `IdleWait` | 1000000 | 无命令、无 Timer 时的最大等待微秒数；端口等待失败时还作为退避上限输入 |
| `ThreadStack` | 0 | Worker 栈大小，零使用平台默认值 |

`NodeCacheBytes` 使用 64、128、256、512、1024 字节五个尺寸类，按实际流量惰性增长，
全部尺寸类共享同一个每 Worker 字节硬上限。缓存承载 Engine 命令、Timer、TCP 接受
分发、Dial 候选与发送元数据、TCP/UDP Future 等待节点、TLS Stream 小型 Future 节点、UDP
发送元数据，不给每个连接、Future 或数据报预留固定空间。超过 1 KiB 的节点直接使用
全局堆；零预算只关闭这一层
Worker 缓存，不改变 xrt 全局堆自己的尺寸类策略。Worker 停止时释放全部缓存节点，
重新启动后按需建立。

活动网络对象只有在自己的等待锁内确认 Worker 仍然有效后，才为缓存节点取得一个临时
Engine 生命周期租约；节点先归还 Worker 缓存，再释放该租约。进入终态后新建的 Future
节点不再访问 Worker，而是使用独立堆。因此调用方可以让已经完整终止的 Stream、
Listener、UDP 或 TLS Stream 引用晚于 Engine 销毁，并继续查询它们的固定终态；活动
对象和活动 Future 则仍会阻止 Engine 完成销毁。

组合式网络库可以使用 `xrtNetEnginePin` 为借用的 Engine 指针取得一个显式生命周期
占用，并在自身最后一个引用释放时调用 `xrtNetEngineUnpin`。成功的 Pin 必须严格一一
配对；Engine 未运行时 Pin 失败，未匹配的 Unpin 返回 `XERR_STATE`。这组 API 只管理
生命周期，不赋予跨线程访问 Worker 私有状态的权限，也不替代上层对象自己的关闭与
排空状态机。

`xrtNetEngineCreate` 在返回前复制 `BufferPool` 指向的完整配置，不保留该指针；
调用方随后可以修改或释放原配置。它只创建停止状态对象。`xrtNetEngineStart`
为每个 Worker 建立一个独占缓冲池、事件端口和线程；
部分启动失败会完整回滚到 `XNET_ENGINE_STOPPED`，之后允许重试。`Start` 和 `Stop`
在已经处于目标状态时幂等成功。

`xrtNetEngineStop` 先阻止新提交，再等待正在提交的调用离开，关闭各命令队列，
唤醒 Worker，并排空已经受理的普通任务。尚未到期的 Timer 以
`XNET_RESULT_CLOSED` 终结。Timer 关闭回调及其投递的有限后续任务仍会在所属
Worker 上执行。停机排空按任务投递代推进；任务链在固定安全代数内没有收敛时，
Worker 会原子封闭后续投递，继续执行封口前已经受理的任务，然后完整释放运行资源。
此时 Engine 进入 `XNET_ENGINE_STOPPED`，`Stop` 返回 `false`、`XERR_STATE` 和
`XNET_ERROR_ENGINE_STOP`，`ShutdownStalls` 增加一次；Engine 可以再次启动。

调用方和高层网络对象必须在停止前归还 Worker
缓冲池分配的全部块；若仍有外借块，Worker 线程已经停止且 Engine 进入
`XNET_ENGINE_STOPPED`，但 `Stop` 返回 `XNET_ERROR_POOL_BUSY`，并保留池和 Engine。
调用方可以重新 `Start`，在所属 Worker 上归还旧块，再次 `Stop` 完成清理。
`Destroy` 遇到相同情况也返回失败并保留 Engine，不会制造悬空缓冲引用。
不能从 Engine 自己的 Worker 回调中调用 `Stop` 或 `Destroy`，这类调用返回
`XERR_STATE`，避免自等待死锁。

### `xrtNetEngineConfigInit`

初始化兼顾吞吐与内存占用的 Engine 默认配置（见上表）。

```c
void xrtNetEngineConfigInit(xnetengineconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 非空 | 接收默认配置 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 覆盖 Worker 数后创建

```c
xrtNetEngineConfigInit(&Config);
Config.Workers = 2;
pEngine = xrtNetEngineCreate(&Config);
```

### `xrtNetEngineCreate`

创建停止状态的 Engine；Worker 线程和端口在 `Start` 时建立。`BufferPool` 指向的配置在返回前完整复制。

```c
xnetengine* xrtNetEngineCreate(const xnetengineconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 非空 | 通常为 `ConfigInit` 产物；字段越界（如 `Workers > 256`、`EventBatch > 4096`）立即失败 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 停止状态的 Engine | — |
| `NULL` | 配置非法或内存不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` + `XNET_ERROR_ENGINE_CREATE` — 配置指针为空或字段非法
- 内存分配失败 — Engine 结构或初始表分配失败

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 创建即 `STOPPED`

```c
pEngine = xrtNetEngineCreate(&Config);
if ( (pEngine == NULL) ||
	(xrtNetEngineState(pEngine) != XNET_ENGINE_STOPPED) ||
	!xrtNetEngineStart(pEngine) ) {
	goto Cleanup;
}
```

### `xrtNetEngineStart`

建立全部 Worker、端口和线程；已经运行时幂等成功。部分启动失败会完整回滚到 `STOPPED`，允许重试。

```c
bool xrtNetEngineStart(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 停止或运行状态的 Engine |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已运行（或原本已运行） | — |
| `false` | 参数非法、状态切换冲突或资源创建失败；失败后回到 `STOPPED` | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空
- `XERR_STATE` + `XNET_ERROR_ENGINE_START` — 状态正在切换（并发 Start/Stop）
- `XERR_INTERNAL` + `XNET_ERROR_ENGINE_START` — Worker 端口、线程或缓冲池创建失败（含底层端口错误传播）

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · Start 后自旋到 `RUNNING`

```c
!xrtNetEngineStart(pEngine) ) {
	goto Cleanup;
}
while ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
	xrtSleep(1u);
}
```

### `xrtNetEngineStop`

排空任务并释放运行资源。任务链不收敛或仍有外借池块时返回失败，但 Engine 仍进入可重启的停止状态。

```c
bool xrtNetEngineStop(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行或停止状态的 Engine |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已排空并停止；可再次 `Start` | — |
| `false` | 已进入 `STOPPED` 但排空不完整（见错误） | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空
- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 从 Worker 回调内调用（自等待死锁），或状态正在切换
- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 任务链不收敛触发封口（`ShutdownStalls` 计数）
- `XNET_ERROR_POOL_BUSY` — Worker 缓冲池仍有外借块；池和 Engine 保留，可 `Start` 后归还再 `Stop`

#### 范例

[network/engine_tour · 收尾](../../examples/network/engine_tour/main.c) · 先停再销毁

```c
(xrtNetEngineState(pEngine) != XNET_ENGINE_STOPPED) ) {
	xrtNetEngineStop(pEngine);
}
```

### `xrtNetEngineDestroy`

停止并销毁 Engine；仍有高层对象或外借池块时失败并保留对象。

```c
bool xrtNetEngineDestroy(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针是空操作；运行中会先执行停止流程 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | Engine 已销毁 | — |
| `false` | 仍有活动对象或外借池块，Engine 保留 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 从 Worker 回调内调用
- `XERR_STATE` + `XNET_ERROR_ENGINE_STOP` — 状态正在切换或仍有活动网络对象
- `XNET_ERROR_POOL_BUSY` — 缓冲池仍有外借块

#### 范例

[network/engine_tour · 收尾](../../examples/network/engine_tour/main.c) · Stop 之后再 Destroy

```c
if ( pEngine != NULL ) {
	xrtNetEngineDestroy(pEngine);
}
```

### `xrtNetEngineState`

返回当前生命周期状态，可安全跨线程查询。

```c
xnetenginestate xrtNetEngineState(const xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针返回 `XNET_ENGINE_STOPPED`（零值） |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XNET_ENGINE_STOPPED` | 停止（或空指针）；可 `Start` |
| `XNET_ENGINE_STARTING` | 正在建立 Worker |
| `XNET_ENGINE_RUNNING` | 运行中 |
| `XNET_ENGINE_STOPPING` | 正在排空停机 |
| `XNET_ENGINE_DESTROYING` | 正在销毁 |

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 自旋等待进入运行态

```c
while ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
	xrtSleep(1u);
}
```

### `xrtNetEnginePin`

占用一个正在运行的 Engine 生命周期，供组合网络对象保存借用指针。每次成功占用必须由一次 `xrtNetEngineUnpin` 配对释放。

```c
bool xrtNetEnginePin(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 必须处于 `RUNNING` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 生命周期占用 +1，Destroy 被阻止 | — |
| `false` | 未占用 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_CLOSED` + `XNET_ERROR_ENGINE_POST` — Engine 未运行（Pin 只对运行中的 Engine 有意义）

#### 范例

[network/engine_tour · 占用](../../examples/network/engine_tour/main.c) · Pin/Unpin 严格配对

```c
if ( !xrtNetEnginePin(pEngine) ||
	!xrtNetEngineUnpin(pEngine) ) {
	goto Cleanup;
}
```

### `xrtNetEngineUnpin`

释放一次 Engine 生命周期占用；没有匹配占用时返回状态错误。

```c
bool xrtNetEngineUnpin(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 必须有未释放的 Pin |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 占用 -1；归零后 Destroy 可继续 | — |
| `false` | 参数非法或没有匹配占用 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空
- `XERR_STATE` — 没有匹配的 Pin 占用

#### 范例

[network/engine_tour · 占用](../../examples/network/engine_tour/main.c) · Pin/Unpin 严格配对

```c
if ( !xrtNetEnginePin(pEngine) ||
	!xrtNetEngineUnpin(pEngine) ) {
	goto Cleanup;
}
```

### Worker 与任务

- `xrtNetEngineWorkerCount` 返回固定 Worker 数。
- `xrtNetEngineWorker` 返回指定索引的借用 Worker。
- `xrtNetEngineCurrent` 返回当前线程在指定 Engine 中的 Worker。
- `xrtNetWorkerEngine`、`xrtNetWorkerIndex` 和 `xrtNetWorkerIsCurrent` 提供稳定查询。
- `xrtNetWorkerPort` 只在运行期返回借用端口。
- `xrtNetWorkerBufPool` 只在所属 Worker 回调内返回借用的共享自适应缓冲池。
- `xrtNetWorkerAlloc/Free` 为协议对象提供线程安全的分级小对象缓存；内存由 `Alloc` 清零，必须把原始大小和同一 Worker 传回 `Free`，并由调用方保证 Worker 生命周期覆盖分配与归还。
- `xrtNetWorkerOperationId` 从任意线程分配 Engine 内唯一的非零端口操作 ID。
- `xrtNetEnginePost` 按 `affinity % workers` 选择 Worker。

`Post` 返回成功后，任务必在目标 Worker 上执行一次，包括与 `Stop` 并发时已经
受理的任务。队列达到硬容量返回 `false` 和 `XERR_AGAIN`；停止后返回
`XERR_CLOSED`。停机期间，当前 Worker 只允许投递完成清理所需的有限后续任务；
一旦不收敛保护触发，公开与内部投递均以 `XERR_CLOSED` 拒绝。普通命令和 transport
内部生命周期命令分别按每轮 256 个的预算消费，
随后必须回到 Timer 与端口事件，避免大批连接同时关闭、取消或完成解析时饿死 IO；
未消费的内部命令由 Worker 自己保留，不重新入队，也不增加分配。Worker 回调应保持
短小，阻塞一个回调会同时阻塞该 Worker 的 IO、Timer 和后续命令。

端口等待失败不会让 Worker 无界热循环，也不会立即丢弃仍可处理的关闭与控制命令。
Worker 记录结构化网络错误和系统错误码，清理线程局部错误后退避 1 到 10 毫秒；
实际退避由 `IdleWait` 限制在该范围内。终止性后端故障会在后续循环继续计数，调用方
可通过统计发现并决定停止、重建或降级 Engine。

`Stop` 先原子关闭全部 Worker 的提交门，再等待已经进入提交区的调用离开，因此
不会释放仍可能被生产者访问的命令队列。`xrtNetWorkerPort` 返回的端口只是借用：
除端口 API 明确允许跨线程的操作外，应在所属 Worker 使用；外部线程直接使用
`Post` 或 `Wake` 时，调用方必须先停止自己的生产者，再停止或销毁 Engine。

Worker 缓冲池供 TCP、UDP、TLS 和自定义协议共享，缓存预算按 Worker 而不是按连接
计算。通过该池建立的 `xnetbuf` 必须在所属 Worker 上操作，并在回调返回前清空或把
后续处理继续投递到同一 Worker；不能把带池块跨线程释放。需要跨线程长期保存时，
应复制数据或使用 `Pool == NULL` 的独立缓冲。

### `xrtNetEngineWorkerCount`

返回 Engine 固定的 Worker 数量。

```c
uint32 xrtNetEngineWorkerCount(const xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针返回 0 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `> 0` | Worker 数；创建后固定，跨 `Stop`/`Start` 不变 |
| `0` | `pEngine` 为空 |

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 显式 2 Worker

```c
if ( (xrtNetEngineWorkerCount(pEngine) != 2u) ||
	((pWorker0 = xrtNetEngineWorker(pEngine, 0u)) == NULL) ||
```

### `xrtNetEngineWorker`

返回借用的指定 Worker；索引越界时返回空指针。

```c
xnetworker* xrtNetEngineWorker(xnetengine* pEngine, uint32 iIndex);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Engine 指针 |
| `iIndex` | 输入 | `< WorkerCount` | Worker 索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 借用 Worker，存活期由 Engine 决定 | — |
| `NULL` | 索引越界 | `XERR_RANGE`（越界时设置） |

#### 错误

- `XERR_RANGE` — `iIndex >= WorkerCount`

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 越界索引返回空

```c
((pWorker0 = xrtNetEngineWorker(pEngine, 0u)) == NULL) ||
((pWorker1 = xrtNetEngineWorker(pEngine, 1u)) == NULL) ||
(xrtNetEngineWorker(pEngine, 99u) != NULL) ||
```

### `xrtNetEngineCurrent`

返回当前线程所属的借用 Worker，不属于该 Engine 时返回空指针。

```c
xnetworker* xrtNetEngineCurrent(xnetengine* pEngine);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 允许空指针 | 空指针返回 `NULL` |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | 当前线程正是该 Worker（在其回调内调用） |
| `NULL` | 当前线程不属于该 Engine（如外部主线程） |

#### 错误

- 无 — 不属于任何 Worker 是查询结果而非错误，不设置线程错误

#### 范例

[network/engine_tour · 生命周期](../../examples/network/engine_tour/main.c) · 主线程不属于任何 Worker

```c
(xrtNetEngineWorker(pEngine, 99u) != NULL) ||
/* 主线程不属于任何 Worker → Current 为空。 */
(xrtNetEngineCurrent(pEngine) != NULL) ) {
	goto Cleanup;
}
```

### `xrtNetWorkerEngine`

返回 Worker 所属的借用 Engine。

```c
xnetengine* xrtNetWorkerEngine(const xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 允许空指针 | 空指针返回 `NULL` |

#### 返回值

| 返回 | 含义 |
|---|---|
| 非空 | 所属 Engine（借用） |
| `NULL` | `pWorker` 为空 |

#### 错误

- 无 — 空指针返回 `NULL`，不设置线程错误

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内反查 Engine

```c
pTask->bEngineOk =
	(xrtNetWorkerEngine(pWorker) == pTask->pEngine);
```

### `xrtNetWorkerIndex`

返回 Worker 在所属 Engine 内的稳定索引。

```c
uint32 xrtNetWorkerIndex(const xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | Worker 指针 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `[0, WorkerCount)` | 稳定索引，跨 `Stop`/`Start` 不变 |
| `UINT32_MAX` | `pWorker` 为空 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空（返回 `UINT32_MAX`）

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内核对亲和索引

```c
pTask->bIndexOk = (xrtNetWorkerIndex(pWorker) == 0u);
```

### `xrtNetWorkerIsCurrent`

判断调用线程是否正是指定 Worker。

```c
bool xrtNetWorkerIsCurrent(const xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 允许空指针 | 空指针返回 `false` |

#### 返回值

| 返回 | 含义 |
|---|---|
| `true` | 当前线程正是该 Worker |
| `false` | 不是，或 `pWorker` 为空 |

#### 错误

- 无 — 否定回答与空指针都是查询结果，不设置线程错误

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内确认自身

```c
pTask->bIsCurrent = xrtNetWorkerIsCurrent(pWorker);
```

### `xrtNetWorkerPort`

返回运行期间借用的端口；调用方必须保证借用操作先于 Stop 结束。

```c
xnetport* xrtNetWorkerPort(xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 运行中的 Worker |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 借用端口；除跨线程明确允许的操作外应在所属 Worker 使用 | — |
| `NULL` | Worker 未运行 | `XERR_STATE`（未运行时设置） |

#### 错误

- `XERR_STATE` + `XNET_ERROR_ENGINE_POST` — Worker 未运行（Stop 后端口已释放）

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 回调内取端口

```c
pTask->bPortOk = (xrtNetWorkerPort(pWorker) != NULL);
```

### `xrtNetWorkerBufPool`

返回 Worker 独占的自适应缓冲池；只能从该 Worker 的回调中调用。

```c
xnetbufpool* xrtNetWorkerBufPool(xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 目标 Worker |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 借用缓冲池；池块必须在所属 Worker 上归还 | — |
| `NULL` | 参数非法或不在该 Worker 回调内 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空
- `XERR_STATE` + `XNET_ERROR_ENGINE_POST` — 不在所属 Worker 上调用（缓冲池仅 Worker 内可用）

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 只在本 Worker 回调内可用

```c
/* BufPool 只能从本 Worker 回调内调用。 */
pPool = xrtNetWorkerBufPool(pWorker);
pTask->bBufPoolOk = (pPool != NULL);
```

### `xrtNetWorkerAlloc`

从 Worker 的线程安全分级缓存分配并清零一块内存；调用方必须保持 Worker 生命周期有效。

```c
ptr xrtNetWorkerAlloc(xnetworker* pWorker, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 分配归属的 Worker；归还也须同一 Worker |
| `iSize` | 输入 | `> 0` | 请求字节数；按 64–1024 尺寸类圆整，更大直接堆分配 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 已清零的内存块 | — |
| `NULL` | 参数非法或内存不足 | 参数非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空或 `iSize` 为零
- 内存分配失败 — 缓存未命中且堆分配失败

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 分配即清零，配对归还

```c
pBlock = xrtNetWorkerAlloc(pWorker, 32u);
pTask->bAllocOk = (pBlock != NULL) &&
	(((const uint8*)pBlock)[0] == 0u) &&
	(((const uint8*)pBlock)[31] == 0u);
xrtNetWorkerFree(pWorker, pBlock, 32u);
```

### `xrtNetWorkerFree`

把 Worker 分配的内存归还同一 Worker；空指针可以直接释放。

```c
void xrtNetWorkerFree(
	xnetworker* pWorker,
	ptr pMemory,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 必须与分配时的 Worker 相同 |
| `pMemory` | 输入 | 允许空指针 | `Alloc` 返回的块；空指针为空操作 |
| `iSize` | 输入 | 同分配时 | 必须传回原始请求大小 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 小节点回缓存，大节点直接释放 |

#### 范例

[network/engine_tour · Worker 自省](../../examples/network/engine_tour/main.c) · 原始大小配对归还

```c
xrtNetWorkerFree(pWorker, pBlock, 32u);
pTask->bFreeOk = true;
```

### `xrtNetWorkerOperationId`

分配 Engine 内唯一的非零端口操作 ID，可从任意线程调用。

```c
uint64 xrtNetWorkerOperationId(xnetworker* pWorker);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 任意 Worker；ID 空间按 Engine 全局划分 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非零 | Engine 内唯一 ID，可直接作端口操作 `Id` | — |
| `0` | 参数非法或 ID 空间耗尽 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pWorker` 为空
- `XERR_INTERNAL` + `XNET_ERROR_ENGINE_POST` — 64 位 ID 空间耗尽（实际不可达）

#### 范例

[network/engine_tour · 占用](../../examples/network/engine_tour/main.c) · 不同 Worker 的 ID 互不相同

```c
IdOpA = xrtNetWorkerOperationId(pWorker0);
IdOpB = xrtNetWorkerOperationId(pWorker1);
if ( (IdOpA == 0u) || (IdOpB == 0u) || (IdOpA == IdOpB) ) {
	goto Cleanup;
}
```

### `xrtNetEnginePost`

有界投递任务；成功受理后必在亲和 Worker（`iAffinity % Workers`）上执行一次。

```c
bool xrtNetEnginePost(xnetengine* pEngine,
	uint64 iAffinity, xnettaskproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行中的 Engine |
| `iAffinity` | 输入 | 任意值 | 亲和键；按模 Worker 数选目标 |
| `pProc` | 输入 | 非空 | 任务回调，短小非阻塞 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已受理；含与 `Stop` 并发时也必执行一次 | — |
| `false` | 未受理，不留任务 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine`/`pProc` 为空
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_POST` — Engine 未运行或停机封口已触发
- `XERR_AGAIN` — 目标 Worker 命令队列达到 `CommandCapacity`
- `XERR_INTERNAL` — Worker 唤醒失败

#### 范例

[network/engine_tour · 任务](../../examples/network/engine_tour/main.c) · 投递到 0 号亲和并自旋等待

```c
if ( !xrtNetEnginePost(pEngine, 0u, exampleWorkerTask,
		(ptr)&Task) ||
	!exampleSpinUntil(&Task.bDone, 2000u) ||
```

### Timer

`xrtNetEngineSchedule` 使用 `xrtClock` 的绝对微秒截止时间；
`xrtNetEngineAfter` 是相对微秒 Helper。成功返回的非零 ID 在所属 Engine 内唯一，
回调只在所属 Worker 执行且绝不在非 Worker 调用线程内联；跨线程调度时，Worker
可能在调度函数返回前并发完成回调。每个成功返回的 ID 恰好发生一次终态回调：

| 结果 | 含义 |
| --- | --- |
| `XNET_RESULT_OK` | Timer 到期 |
| `XNET_RESULT_CANCELLED` | 取消命令在到期前生效 |
| `XNET_RESULT_CLOSED` | Engine 停止或销毁 |
| `XNET_RESULT_ERROR` | 受理后的 Worker 内部资源扩展失败 |

`xrtNetEngineTimerCancel` 是异步请求。返回成功表示取消命令已进入目标 Worker，
不表示 Timer 一定尚未到期；Timer 自己的唯一终态回调给出最终结果。Timer 表使用
自适应最小堆和 ID 哈希：插入、到期和有效取消不扫描全部 Timer，空闲 Worker 也
只保留很小的初始表。从 Timer 所属 Worker 调度时直接完成容量检查和入堆，不经过
命令队列，也不为后续同 Worker 生命周期终结强制分配取消命令；跨线程调度和公开
取消仍通过有界命令队列保持有序。

已经位于 Timer 所属 Worker 的协议状态机可以调用
`xrtNetEngineTimerCancelCurrent` 立即取消。该路径不分配命令节点，也不经过有界队列；
不在所属 Worker、Timer 尚未安装或已经终结时返回 `false`，且不会覆盖当前线程错误。
调用方因此可以先尝试 Current 路径，再用 `xrtNetEngineTimerCancel` 处理跨线程情况。

Timer 终态会先从堆和活动索引移除、归还节点并更新统计，再调用用户回调。因此周期
任务可以在回调中重新调度，并在 Worker 缓存已有节点时不依赖新的底层内存分配。
回调收到的 ID、结果和用户数据已经保存为局部值，节点复用不会改变本次终态参数。

### `xrtNetEngineSchedule`

按单调时钟截止时间调度 Timer；成功返回非零 ID。

```c
uint64 xrtNetEngineSchedule(xnetengine* pEngine,
	uint64 iAffinity, xdeadline iDeadline,
	xnettimerproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行中的 Engine |
| `iAffinity` | 输入 | 任意值 | 亲和键；Timer 归属该 Worker |
| `iDeadline` | 输入 | 单调微秒 | `xrtClock` 绝对截止时间 |
| `pProc` | 输入 | 非空 | 终态回调（四种结果见上表） |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非零 | Timer ID，Engine 内唯一；恰好一次终态回调 | — |
| `0` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine`/`pProc` 为空
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_TIMER` — Engine 未运行或停机封口
- `XERR_AGAIN` + `XNET_ERROR_ENGINE_TIMER` — 在途 Timer 数达到 `TimerLimit`
- `XERR_RANGE` + `XNET_ERROR_ENGINE_TIMER` — Timer 表无法表示更多条目
- `XERR_INTERNAL` — 跨线程命令入队后唤醒失败

#### 范例

[network/engine_tour · 定时器](../../examples/network/engine_tour/main.c) · 即时到期 + 长时定时器

```c
IdFire = xrtNetEngineSchedule(pEngine, 0u,
	xrtDeadlineAfter(0u), exampleFireTimer, (ptr)&Timers);
Timers.iLongId = xrtNetEngineSchedule(pEngine, 0u,
	xrtDeadlineAfter(3600000000ull), exampleLongTimer,
	(ptr)&Timers);
```

### `xrtNetEngineAfter`

按相对微秒数调度 Timer；零表示在下一次 Worker 循环到期。等价于 `Schedule` + `xrtDeadlineAfter`，错误集与之相同。

```c
uint64 xrtNetEngineAfter(xnetengine* pEngine,
	uint64 iAffinity, uint64 iTimeout,
	xnettimerproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 运行中的 Engine |
| `iAffinity` | 输入 | 任意值 | 亲和键 |
| `iTimeout` | 输入 | 微秒 | 相对延迟；零表示尽快到期 |
| `pProc` | 输入 | 非空 | 终态回调 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非零 | Timer ID | — |
| `0` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetEngineSchedule`（`ARGUMENT`/`CLOSED`/`AGAIN`/`RANGE`/`INTERNAL`）

#### 范例

[network/engine · 延迟任务](../../examples/network/engine/main.c) · 100 毫秒后触发

```c
(xrtNetEngineAfter(
	pEngine,
	1,
	100000u,
	exampleTimer,
	&State
) == 0) ) {
```

### `xrtNetEngineTimerCancel`

异步请求取消 Timer；成功只表示取消命令已进入目标 Worker。

```c
bool xrtNetEngineTimerCancel(xnetengine* pEngine, uint64 Id);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Timer 所属 Engine |
| `Id` | 输入 | 非零 | `Schedule`/`After` 返回的 ID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 取消请求已入队；最终结果仍由唯一终态回调给出 | — |
| `false` | 请求未入队 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pEngine` 为空或 `Id` 为零
- `XERR_CLOSED` + `XNET_ERROR_ENGINE_TIMER` — Engine 未运行或停机封口
- `XERR_AGAIN` — 目标 Worker 命令队列达到容量
- `XERR_INTERNAL` — 唤醒失败

#### 范例

[network/engine_tour · 定时器](../../examples/network/engine_tour/main.c) · 异步取消长定时器

```c
if ( (IdFire == 0u) || (Timers.iLongId == 0u) ||
	!xrtNetEngineTimerCancel(pEngine, Timers.iLongId) ) {
	goto Cleanup;
}
```

### `xrtNetEngineTimerCancelCurrent`

只在 Timer 所属 Worker 上立即取消且不分配内存。不在所属 Worker、Timer 尚未入堆或已经终结时返回 `false`，且不修改线程错误。

```c
bool xrtNetEngineTimerCancelCurrent(
	xnetengine* pEngine,
	uint64 Id
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Timer 所属 Engine |
| `Id` | 输入 | 非零 | 要取消的 Timer ID |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已取消；终态回调以 `CANCELLED` 触发 | — |
| `false` | 不在所属 Worker、尚未入堆或已终结 | 不修改线程错误 |

#### 错误

- 无 — 失败路径刻意不设置错误（调用方可先试本函数，失败再走 `TimerCancel`）

#### 范例

[network/engine_tour · CancelCurrent](../../examples/network/engine_tour/main.c) · 同亲和回调内取消另一个 Timer

```c
pTimers->bCancelCurrentOk = xrtNetEngineTimerCancelCurrent(
	xrtNetWorkerEngine(pWorker), pTimers->iLongId);
```

### Completion

`xrtNetCompletionInit` 初始化调用方拥有的 `xnetcompletion`。通过
`xrtNetWorkerPort` 直接提交端口操作时，`User` 必须指向一个有效 Completion，且
Completion、Socket 和 IO 缓冲都必须存活到终态事件回调结束。Engine 在所属
Worker 上调用 `Proc(worker, event, data)`。

该约束取代旧版 Engine 只有两个全局端口回调槽的设计。每个在途操作都能携带自己
的 Completion，因此 TCP、UDP、监听器和自定义协议可以任意组合。无人接收的
Accept 结果由 Engine 自动关闭，避免泄漏已接受 Socket。

### `xrtNetCompletionInit`

初始化一个借用过程和数据的端口 Completion；空指针是空操作，不设置错误。

```c
void xrtNetCompletionInit(xnetcompletion* pCompletion,
	xnetcompletionproc pProc, ptr pData);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCompletion` | 输出 | 建议非空 | 调用方拥有的 Completion；空指针是空操作 |
| `pProc` | 输入 | 可空 | 终态回调 `Proc(worker, event, data)`，在所属 Worker 上执行 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化；存活期约束见上文 Completion 契约 |

#### 错误

- 无 — 初始化不失败，空指针静默忽略

#### 范例

[network/engine_tour · Completion](../../examples/network/engine_tour/main.c) · 借用过程与数据的初始化形态

```c
xrtNetCompletionInit(&Completion, exampleCompletionProc,
	(ptr)&bTaskDone);
```

### 统计

`xrtNetWorkerStats` 返回单 Worker 并发快照，`xrtNetEngineStats` 聚合全部 Worker。
统计覆盖任务受理、拒绝、执行，Timer 受理、拒绝及四种终态，端口事件、等待错误、
唤醒错误、停机任务链不收敛次数、小节点缓存命中/未命中、当前缓存字节、当前命令
深度和活动 Timer。`ShutdownStalls` 从 `XRT_STATS_BASIC` 开始记录，是跨
`Stop/Start` 累计的诊断计数；一次失败停机中每个不收敛 Worker 最多增加一次。
单 Worker 的 `LastWaitError` 和 `LastWaitSystemCode` 保留最近一次端口等待失败详情；
从未失败时分别为 `XNET_ERROR_NONE` 和零。Engine 聚合统计只累计 `WaitErrors`，需要
定位具体后端错误时应读取各 Worker 快照。
`NodeCacheHits`/`NodeCacheMisses` 是累计分配路径计数；`NodeCachedBytes` 是并发快照，
始终不超过各 Worker 的 `NodeCacheBytes`，停止后归零。累计计数跨 `Stop/Start` 保留，
当前深度在停止后归零。

### `xrtNetWorkerStats`

读取一个 Worker 的统计快照。

```c
bool xrtNetWorkerStats(const xnetworker* pWorker,
	xnetworkerstats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWorker` | 输入 | 非空 | 目标 Worker |
| `pStats` | 输出 | 非空 | 接收并发快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 快照已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/engine_tour · 统计](../../examples/network/engine_tour/main.c) · 任务执行后核对计数

```c
if ( !xrtNetWorkerStats(pWorker0, &WorkerStats) ||
	(WorkerStats.PostsExecuted < 1u) ) {
	goto Cleanup;
}
```

### `xrtNetEngineStats`

聚合全部 Worker 的统计快照。

```c
bool xrtNetEngineStats(const xnetengine* pEngine,
	xnetenginestats* pStats);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | Engine 指针 |
| `pStats` | 输出 | 非空 | 接收聚合快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 聚合快照已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/engine_tour · 统计](../../examples/network/engine_tour/main.c) · 全 Worker 聚合计数

```c
if ( !xrtNetEngineStats(pEngine, &EngineStats) ||
	(EngineStats.PostsExecuted < 2u) ||
	(EngineStats.TimersFired < 2u) ) {
	goto Cleanup;
}
```

## 名称解析（Resolver）

Resolver 提供独立于 Engine 的异步名称解析：自带工作线程池、成功/失败结果缓存与并发上限。同一规范化主机与地址族只执行一次底层查询，并发请求共享同一查询组。`xrtNetResolveAsync` 把查询包装为 Future。Op（`xnetresolveop`）是引用计数对象：回调在 Resolver Worker 上恰好执行一次，回调后仍可查询状态与结果，跨线程保留须先 `xrtNetResolveOpRef`。

### `xrtNetResolverConfigInit`

写入兼顾桌面与高并发服务的默认配置：`Workers=2`、`RequestLimit=8192`、`QueryLimit=4096`、`CacheEntries=256`、`SuccessTTL=60s`、`FailureTTL=5s`、`HostLimit=1024`。

```c
void xrtNetResolverConfigInit(xnetresolverconfig* pConfig);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输出 | 建议非空 | 接收默认配置；空指针是空操作 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯初始化，不失败 |

#### 范例

[network/resolve_tour · 创建](../../examples/network/resolve_tour/main.c) · 初始化后直接创建

```c
xrtNetResolverConfigInit(&Config);
pResolver = xrtNetResolverCreate(&Config);
```

### `xrtNetResolverCreate`

创建并立即启动独立解析工作池；空配置使用默认值（内部先取默认再整体复制调用方配置）。

```c
xnetresolver* xrtNetResolverCreate(
	const xnetresolverconfig* pConfig
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | 允许空指针 | 空指针用默认值；非空时字段任一上限为零或不支持即失败 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 运行中的 Resolver | — |
| `NULL` | 配置非法或资源不足 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_VALUE` + `XNET_ERROR_RESOLVER_CREATE` — 配置含零值或不支持的限额（如 `Workers > 32`、`RequestLimit == 0`）
- 内存分配失败 — 结构或线程池创建失败

#### 范例

[network/resolve_tour · 创建](../../examples/network/resolve_tour/main.c) · 默认配置 + 初始统计核对

```c
pResolver = xrtNetResolverCreate(&Config);
if ( (pResolver == NULL) ||
	!xrtNetResolverStats(pResolver, &Stats) ||
	(Stats.Submitted != 0u) ) {
	goto Cleanup;
}
```

### `xrtNetResolverDestroy`

排空已受理请求并等待全部回调；必须与其他 Resolver 所有者操作串行，返回后指针失效。

```c
bool xrtNetResolverDestroy(xnetresolver* pResolver);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 允许空指针 | 空指针是空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已排空并销毁 | — |
| `false` | 参数非法、状态非法或回调内部失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_STATE` + `XNET_ERROR_RESOLVER_CLOSED` — 从 Resolver 自己的 Worker 回调内调用，或已在关闭/已销毁

#### 范例

[network/resolve_tour · 收尾](../../examples/network/resolve_tour/main.c) · 销毁失败以退出码暴露

```c
if ( (pResolver != NULL) &&
	!xrtNetResolverDestroy(pResolver) ) {
	iResult = 2;
}
```

### `xrtNetResolverResolve`

提交主机查询；同一规范化主机与地址族只执行一次底层查询，命中缓存立即返回已关联的 Op。

```c
xnetresolveop* xrtNetResolverResolve(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family,
	xnetresolveproc pDone,
	ptr pData
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | 运行中的 Resolver |
| `sHost` | 输入 | 非空、以零结尾 | 主机名；长度不得超过 `HostLimit` |
| `Family` | 输入 | `UNSPEC`/`IPV4`/`IPV6` | 目标地址族 |
| `pDone` | 输入 | 非空 | 完成回调，在 Resolver Worker 上恰好执行一次 |
| `pData` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 解析操作（引用归调用方，用后 `ResolveOpDestroy`） | — |
| `NULL` | 未受理 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pResolver`/`sHost`/`pDone` 为空
- `XERR_VALUE` + `XNET_ERROR_FAMILY` — `Family` 不是三种支持值之一
- `XERR_CLOSED` + `XNET_ERROR_RESOLVER_CLOSED` — Resolver 已关闭或正在关闭
- `XERR_RANGE` + `XNET_ERROR_RESOLVER_SUBMIT` — 主机名超过 `HostLimit`
- `XERR_AGAIN` + `XNET_ERROR_RESOLVER_SUBMIT` — 在途请求达到 `RequestLimit`，或唯一查询数达到 `QueryLimit`

#### 范例

[network/resolve_tour · 查询](../../examples/network/resolve_tour/main.c) · 提交 localhost 并核对初始状态

```c
pOperation = xrtNetResolverResolve(pResolver, "localhost",
	XNET_FAMILY_IPV4, exampleResolveDone, (ptr)&State);
```

### `xrtNetResolverClear`

清空成功和失败缓存；已经运行或排队的查询不受影响。

```c
bool xrtNetResolverClear(xnetresolver* pResolver);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | 运行中的 Resolver |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 缓存已清空 | — |
| `false` | 参数非法或 Resolver 已关闭 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pResolver` 为空
- `XERR_STATE` + `XNET_ERROR_RESOLVER_CLOSED` — 正在关闭或已销毁

#### 范例

[network/resolve_tour · 清缓存](../../examples/network/resolve_tour/main.c) · 清空后缓存计数归零

```c
if ( !xrtNetResolverClear(pResolver) ) {
	goto Cleanup;
}
```

### `xrtNetResolverStats`

取得 Resolver 的并发一致统计快照。

```c
bool xrtNetResolverStats(
	const xnetresolver* pResolver,
	xnetresolverstats* pStats
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | Resolver 指针 |
| `pStats` | 输出 | 非空 | 接收快照（Submitted/Resolved/CachedResults 等） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 快照已写入 | — |
| `false` | 参数非法 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 任一指针为空

#### 范例

[network/resolve_tour · 统计](../../examples/network/resolve_tour/main.c) · 查询后核对计数推进

```c
if ( !xrtNetResolverStats(pResolver, &Stats) ||
	(Stats.Submitted < 1u) ||
	(Stats.Resolved < 1u) ) {
	goto Cleanup;
}
```

### `xrtNetResolveOpRef`

增加解析操作引用并返回原指针。跨线程保留 Op（回调后仍要查询）必须先取引用。

```c
xnetresolveop* xrtNetResolveOpRef(xnetresolveop* pOperation);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 原指针，引用 +1；之后必须多一次 `ResolveOpDestroy` | — |
| `NULL` | 参数非法或引用耗尽 | 参数非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOperation` 为空

#### 范例

[network/resolve_tour · OpRef](../../examples/network/resolve_tour/main.c) · 共享引用后双份销毁

```c
pRef = xrtNetResolveOpRef(pOperation);
if ( (pRef == NULL) || (pRef != pOperation) ) {
	goto Cleanup;
}
xrtNetResolveOpDestroy(pRef);
xrtNetResolveOpDestroy(pOperation);
```

### `xrtNetResolveOpDestroy`

释放解析操作引用；空指针视为空操作。归零时释放操作与关联结果。

```c
void xrtNetResolveOpDestroy(xnetresolveop* pOperation);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 允许空指针 | 要释放引用的操作 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 空指针是空操作 |

#### 范例

[network/resolve_tour · OpRef](../../examples/network/resolve_tour/main.c) · 每份引用各自配对销毁

```c
xrtNetResolveOpDestroy(pRef);
xrtNetResolveOpDestroy(pOperation);
```

### `xrtNetResolveOpCancel`

协作取消尚未进入终态的操作；回调仍在 Resolver Worker 上执行一次（以 `CANCELLED` 终态）。

```c
bool xrtNetResolveOpCancel(xnetresolveop* pOperation);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 要取消的操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 取消请求已受理（或查询组已在取消） | — |
| `false` | 参数非法或操作已进入终态 | 参数非法时错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — `pOperation` 为空
- 无额外错误 — 已终态的取消失败是查询结果，不设置错误

#### 范例

[network/resolver · 超时取消](../../examples/network/resolver/main.c) · 等待截止后协作取消

```c
if ( xrtDeadlineExpired(iDeadline) ) {
	(void)xrtNetResolveOpCancel(pOperation);
	break;
}
```

### `xrtNetResolveOpState`

返回解析操作当前状态的原子快照。

```c
xnetresolveopstate xrtNetResolveOpState(
	const xnetresolveop* pOperation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 |
|---|---|
| `XNET_RESOLVE_PENDING` | 已受理、尚未开始底层查询 |
| `XNET_RESOLVE_RUNNING` | 底层查询进行中 |
| `XNET_RESOLVE_RESOLVED` | 成功终态（结果可取） |
| `XNET_RESOLVE_FAILED` | 失败终态（错误可借） |
| `XNET_RESOLVE_CANCELLED` | 取消终态 |

#### 范例

[network/resolve_tour · 状态机](../../examples/network/resolve_tour/main.c) · 提交时未完成、回调后已解析

```c
if ( (pOperation == NULL) ||
	(xrtNetResolveOpState(pOperation) ==
		XNET_RESOLVE_RESOLVED) ) {
	goto Cleanup;
}
```

### `xrtNetResolveOpResult`

成功时返回增加引用的完整地址列表，其他状态返回空指针并设置对应错误。

```c
xnetaddrlist* xrtNetResolveOpResult(
	const xnetresolveop* pOperation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 地址列表（引用 +1，调用方 `xrtNetAddrListDestroy`） | — |
| `NULL` | 未终态或非成功终态 | 错误经 `xrtGetError()` 报告；失败详情另见 `ResolveOpError` |

#### 错误

- `XERR_ARGUMENT` — `pOperation` 为空
- `XERR_VALUE` + `XNET_ERROR_RESOLVER_QUERY` — 操作尚未完成（终态前查询结果）
- 失败/取消状态 — `NULL`，结构化错误经 `ResolveOpError` 借用返回

#### 范例

[network/resolve_tour · 结果](../../examples/network/resolve_tour/main.c) · 返回的列表由调用方销毁

```c
pList = xrtNetResolveOpResult(pOperation);
if ( (pList == NULL) ||
	(xrtNetAddrListCount(pList) < 1u) ) {
	goto Cleanup;
}
xrtNetAddrListDestroy(pList);
```

### `xrtNetResolveOpError`

失败或取消时返回借用的结构化错误，其他状态返回空指针。

```c
const xerror* xrtNetResolveOpError(
	const xnetresolveop* pOperation
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOperation` | 输入 | 非空 | 解析操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 借用的 `xerror`，存活到操作销毁；不转移所有权 | — |
| `NULL` | 非失败/取消状态 | 非错误状态，不设置线程错误 |

#### 错误

- 无 — 空返回是状态查询结果而非错误

#### 范例

[network/resolver · 失败路径](../../examples/network/resolver/main.c) · 借用错误打印消息

```c
const xerror* pError = xrtNetResolveOpError(pOperation);

fprintf(stderr, "%s\n", pError != NULL ?
	xrtErrorMessage(pError) : "resolve failed");
```

### `xrtNetResolveAsync`

把 Resolver 查询包装为 Future；成功值是由 Future 持有的地址列表。

```c
xfuture* xrtNetResolveAsync(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResolver` | 输入 | 非空 | 运行中的 Resolver |
| `sHost` | 输入 | 非空 | 主机名 |
| `Family` | 输入 | 三种支持值 | 目标地址族 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | Future：成功值 `xnetaddrlist*`（Future 持有），失败值 `xerror` | — |
| `NULL` | 提交失败（同 `ResolverResolve`）或 Future 分配失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- 同 `xrtNetResolverResolve`（`ARGUMENT`/`VALUE`/`CLOSED`/`RANGE`/`AGAIN`）
- 内存分配失败 — Future 包装失败

#### 范例

[network/resolver_future · Future 形态](../../examples/network/resolver_future/main.c) · 提交后 `xrtFutureWaitFor` 等待

```c
pFuture = xrtNetResolveAsync(
	pResolver,
	"localhost",
	XNET_FAMILY_UNSPEC
);
```

## 网络接口与本机信息

接口族回答三类问题：名字与接口索引互查（`InterfaceIndex`/`InterfaceName`）、整机一致快照（`Interfaces`）、本机诊断偏好（`LocalAddress`/`LocalHardware`/`HostName` 及其文本形态）。本机诊断函数是确定性偏好查询，不代表公网出口、默认路由或服务监听策略。缓冲输出统一采用两段式：空缓冲查询所需大小，缓冲不足不写入并报告完整所需大小。

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
