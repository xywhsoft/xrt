# 异步 DNS Resolver

`XRT_FEATURE_NET_RESOLVER` 在同步 DNS 原语之上提供独立的受限工作池。它不依赖网络 Engine，也不会创建隐藏 Engine；TCP、HTTP、WebSocket 和应用代码可以共享同一个 Resolver，纯 Socket 或 Engine 程序不需要携带该模块。

## 分层

```c
xnetaddrlist* xrtNetLookup(cstr host, xnetfamily family);
xnetaddrlist* xrtNetResolve(cstr host, uint16 port,
	xnetfamily family);

xnetresolver* xrtNetResolverCreate(
	const xnetresolverconfig* config);
xnetresolveop* xrtNetResolverResolve(xnetresolver* resolver,
	cstr host, xnetfamily family, xnetresolveproc done, ptr data);
```

`xrtNetLookup` 是端口无关的同步 DNS 原语，结果端口全部为零。`xrtNetResolve` 是常见端点场景的同步便捷函数。Resolver 只执行 `Lookup`，因此缓存键是规范化主机名与地址族，端口不会造成重复 DNS 查询。TCP 建连时复制候选地址并写入目标端口即可。

## 配置

`xrtNetResolverConfigInit` 写入默认值：2 个 Worker、8192 个在途请求、4096 个唯一查询、256 个缓存项、60 秒成功 TTL、5 秒失败 TTL，以及 1024 字节主机名上限。

`RequestLimit` 限制已经受理但尚未完成回调的调用方操作。`QueryLimit` 限制排队和运行中的唯一底层查询；同一主机名和地址族的请求会共享查询，但仍分别占用请求限额。所有限额都是硬边界，达到边界时提交返回空指针并设置 `XERR_AGAIN`。

`SuccessTTL` 或 `FailureTTL` 为零时关闭对应缓存。`CacheEntries` 为零时完全关闭缓存。TTL 使用 `xrtClock` 的单调微秒刻度，不受系统时间回拨影响。

`Lookup` 可替换默认的 `xrtNetLookup`。自定义过程可能被多个 Worker 并发调用，必须线程安全；返回值必须是非空、端口为零、符合请求地址族的 `xnetaddrlist`，失败时必须设置结构化错误。`LookupData` 在 Resolver 销毁返回前必须保持有效。

## 查询合并与缓存

主机名按 DNS 的 ASCII 大小写规则规范化，尾随根点保留，因此 `Example.COM` 与 `example.com` 共享查询，`example.com.` 仍是独立的绝对名称。活动查询和缓存都使用哈希索引；缓存保存完整不可变地址列表或完整失败错误，不会退化为只缓存第一个地址。

规范化临时区按主机名长度选择栈内快速路径或受 `HostLimit` 约束的精确分配。唯一查询组和缓存项都把规范化主机名保存在对象尾部，不为键再建立独立堆节点；常见短主机名的缓存命中只创建调用方操作，不产生临时主机名分配。缓存内存仍由 `CacheEntries`、`HostLimit` 和两类 TTL 共同约束，不存在每个 Resolver 预留固定主机缓冲的成本。

缓存命中也不会在提交函数内直接调用用户回调。所有已受理操作的回调都在 Resolver Worker 上执行一次，避免缓存命中、取消和真实查询使用不同线程语义。回调应保持短小；需要切换到网络 Worker、任务池或 UI 线程时，应在回调中投递后续工作。

## 操作生命周期

```c
xnetresolveopstate xrtNetResolveOpState(const xnetresolveop* op);
xnetaddrlist* xrtNetResolveOpResult(const xnetresolveop* op);
const xerror* xrtNetResolveOpError(const xnetresolveop* op);
bool xrtNetResolveOpCancel(xnetresolveop* op);
xnetresolveop* xrtNetResolveOpRef(xnetresolveop* op);
void xrtNetResolveOpDestroy(xnetresolveop* op);
```

操作状态依次为 `PENDING`、可选的 `RUNNING`，以及 `RESOLVED`、`FAILED`、`CANCELLED` 之一。终态不可改变。`Result` 成功时返回增加引用的完整列表；调用方使用 `xrtNetAddrListDestroy` 释放。`Error` 返回由操作引用保护的借用错误。

取消是协作式的。排队查询的最后一个订阅者取消后，底层查询不会执行；已经进入系统 `getaddrinfo` 的调用无法跨平台强制中断，但取消的调用方会独立进入 `CANCELLED`，底层查询可继续为其他订阅者或缓存生成结果。取消回调仍由 Resolver Worker 执行。

调用方可以在提交后立即释放自己的操作引用，Resolver 会持有到回调结束。回调参数是借用引用；需要在回调后继续保存时调用 `xrtNetResolveOpRef`。

## 销毁与统计

`xrtNetResolverDestroy` 停止接收新请求，排空所有已经受理的查询和回调，然后等待 Worker 退出。它不能从 Resolver 自己的回调中调用；调用方必须把销毁与 `Resolve`、`Clear`、`Stats` 以及第二次销毁等 Resolver 所有者操作串行化。销毁返回后 Resolver 指针立即失效，不得再次传入任何 API。操作对象仍可继续保存和读取终态；内部 Resolver 外壳会由最后一个操作引用释放。

`xrtNetResolverClear` 清空成功与失败缓存，不影响活动查询和已经交付的共享结果。`xrtNetResolverStats` 返回提交、拒绝、命中、未命中、合并、底层查询、各终态以及当前队列深度的一致快照。

## Future 便捷层

`XRT_FEATURE_NET_RESOLVER_FUTURE` 单独依赖 Resolver 与通用 Future：

```c
xfuture* xrtNetResolveAsync(xnetresolver* resolver,
	cstr host, xnetfamily family);
```

成功 Future 的值是由 Future 持有的 `xnetaddrlist`，通过 `xrtFutureValue` 借用；Future 销毁时自动释放。查询失败直接保留 Resolver 操作中的结构化错误。`xrtFutureCancel` 会请求底层解析操作取消，最终状态由查询完成与取消之间的真实竞态决定，不会把取消请求本身伪装成已取消终态。

Future 可以使用通用同步等待、组合、延续和协程 `Await` API，不需要 DNS 模块重复建设另一套等待模型。

完整示例位于 `examples/network/resolver/main.c` 和 `examples/network/resolver_future/main.c`。
