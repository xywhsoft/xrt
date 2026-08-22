# HTTP Router

`<xrt/http_router.h>` 在 `http_route` 原始模板层上提供拥有型预编译索引。Router 不依赖网络、HTTP Server 或 Handler 类型，注册的 `Value` 可以是函数、上下文、整数标识或应用对象。这样协议路由、服务端分派和代理规则可以复用同一实现，而不会互相绑死。

## 生命周期

1. `xrtHttpRouterCreate` 创建只含根节点的空 Router。
2. `xrtHttpRouterAdd` 复制方法和模板，并借用 `Value`。
3. `xrtHttpRouterFreeze` 把静态边编译为节点连续的排序区间。
4. 冻结后可以从任意线程并发调用 `xrtHttpRouterMatch`。
5. 所有匹配结束后调用 `xrtHttpRouterDestroy`。

Router 配置是只读固定描述符，可以位于完整但未对齐的存储中；创建函数在任何分配前
完成范围校验和快照，返回后调用方可以修改或释放原配置。地址回绕与零预算配置直接失败。

冻结成功后不能继续注册。需要热更新的服务应创建和冻结一个新 Router，再由应用使用引用计数或读写代际原子替换整个不可变对象。该模型让请求热路径没有锁、分配和半更新状态。

Router 拥有方法、模板、节点和索引，不拥有 `Value`。`Value` 必须至少存活到 Router 销毁且所有并发匹配结束。

## 方法

方法必须是合法 HTTP token，按 RFC 语义区分大小写。`"*"` 是 Router 保留的任意方法注册值。匹配优先级为：

1. 当前结构路径的精确方法；
2. HEAD 请求回退到同一路径的 GET；
3. 当前结构路径的 `*` 方法；
4. 更低结构优先级的路径候选。

HEAD 回退设置 `XHTTP_ROUTER_HEAD_FALLBACK`，任意方法设置 `XHTTP_ROUTER_ANY_METHOD`。存在结构路径但所有候选都没有可用方法时返回 `XHTTP_ROUTER_METHOD_NOT_ALLOWED`；没有任何结构路径返回 `XHTTP_ROUTER_NOT_FOUND`。

`xrtHttpRouterMethods` 可用于构建 405 的 `Allow` 字段或自动 OPTIONS 响应。它列出所有匹配结构模板中的唯一注册方法，为 GET 合成 HEAD，并保留 `*` 任意方法事实。常见的不超过 16 种方法集合使用固定栈内视图一次收集；更多扩展方法进入无隐藏上限、无分配的精确扫描。容量不足时返回精确数量，调用方数组保持不变。

## 路径优先级

每一层按静态段、单段参数、尾参数选择。静态分支在更深位置未命中时，会迭代回溯到参数和尾分支。实现不使用递归，也不在请求期建立段数组；冻结阶段以无分配堆排序建立静态边有序区间，请求期通过二分查找选择静态边。常规前进只消费一次路径游标，只有真实回溯才依据节点父索引重放必要路径段，因此无分支的深路径保持线性复杂度。

同一结构路径可以为不同方法使用不同参数名，例如 GET `/{id}` 与 POST `/{name}`。同一结构路径重复注册相同方法会被拒绝，因为它没有确定分派语义。

## 容量与限额

`xhttprouterconfig` 显式限制路由、节点和拥有文本字节。默认分别为 4096、16384 和 4 MiB。零值非法；需要不同规模时应在创建前修改初始化后的配置。

`xrtHttpRouterMatch` 的 `Params` 契约与 `xrtHttpRouteMatch` 一致。容量不足返回 `XHTTP_ROUTER_MORE`，`Count` 和 `Match` 已描述选中路由，但捕获数组完全不变。匹配结果中的方法和模板借用冻结 Router，参数名借用模板，参数值借用请求路径。

`Params`、`Count`、`Match` 以及 `xrtHttpRouterMethods` 的结果数组都支持未对齐存储。包装地址、输出互相覆盖或输出覆盖输入属于参数错误，并且不会修改任何输出。

## 示例

```c
xhttprouter* Router = xrtHttpRouterCreate(NULL);
xhttproutermatch Match;
xhttprouteparam Params[1];
size_t Count;

xrtHttpRouterAdd(
	Router,
	XRT_STR_LITERAL("GET"),
	XRT_STR_LITERAL("/users/{id}"),
	Handler
);
xrtHttpRouterFreeze(Router);

if ( xrtHttpRouterMatch(
	Router,
	XRT_STR_LITERAL("GET"),
	XRT_STR_LITERAL("/users/42"),
	Params, 1, &Count, &Match
) == XHTTP_ROUTER_MATCH ) {
	/* Match.Value 是 Handler，Params[0].Value 是 "42"。 */
}
```

## 差分模糊测试

`http_router_fuzz_tests` 使用公开 `xrtHttpRouteMatch` 组成独立线性参考模型，差分检查结构回溯、方法回退、参数捕获、方法枚举及短容量原子性。该入口既能执行固定种子回归，也能直接接入 Clang/libFuzzer。

完整示例见 `examples/http/router/main.c`。确定性边界、无分配、故障注入和并发门禁位于 `tests/http/test_http_router.c`、`tests/http/test_http_router_noalloc.c`、`tests/http/test_http_router_oom.c` 与 `tests/http/test_http_router_threads.c`。
