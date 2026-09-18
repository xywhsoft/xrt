# 网络服务的可重试退休合同

`xrtNetEngineTryDestroy` 与 `xrtNetResolverTryDestroy` 将物理清理进展和语义错误分开。
它们不等待工作提交者、查询、回调、线程执行体或线程清理尾部；短内部互斥仍可发生，
不是无锁或实时调度 API。所有者必须串行执行生命周期操作，并自行安排后续重试。

```c
typedef enum xnetretireresult {
    XNET_RETIRE_ERROR = -1,
    XNET_RETIRE_BUSY = 0,
    XNET_RETIRE_READY = 1
} xnetretireresult;

xnetretireresult xrtNetEngineTryDestroy(xnetengine* engine);
xnetretireresult xrtNetResolverTryDestroy(xnetresolver* resolver);
```

| 结果 | 创建者拥有权 | 后续动作 |
| --- | --- | --- |
| READY | 本次已消费；空指针也是 READY | 清空自己的拥有槽，不能再访问旧服务指针 |
| BUSY | 原指针保留，不设置错误 | 让既有工作继续推进，稍后再次调用 |
| ERROR | 原指针保留，结构化诊断报告失败 | 保存语义失败，再安排物理清理重试 |

BUSY/READY 保留调用前诊断的身份。后续物理清理成功不撤销已经发生的语义失败。
正常退休无须分配。调用方不能因为 ERROR 就遗忘指针，也不能将 ERROR 当作 BUSY
静默吞掉；只有 READY 是拥有权提交点。

## Engine

仍有高层对象或实际 Pin 时返回 BUSY，保持 RUNNING，避免阻止释放这些引用所需的工作。
没有此类占用时，在生命周期锁内转入 DESTROYING；关闭所有 Worker 的提交门，待实际
提交者退出后请求停机。TryDestroy 只检查提交者，不自旋等待他们。

全部 Worker 必须真实 join，包含 XRT 线程上下文与动态线程键析构，才允许读取
非原子的缓冲池状态、释放线程句柄和运行资源。自身 Worker 调用只能请求退休并返回
BUSY。已接纳队列和 Timer 仍按原有停机规则完成；不能用 Running=0 代替 join。

外借缓冲块使已退出的 Engine 继续 BUSY。缓冲池仍是原有的非线程安全对象；调用方
须遵守其单线程/外部串行化规则，不能在本步骤读取统计时并发归还块。

不收敛任务链仍产生真实 ERROR，Engine 的创建者拥有权保留，下一次可完成剩余物理
清理。开始退休后不通过 TryDestroy 的 BUSY/ERROR 恢复接纳。原同步 Destroy 仍保留
既有失败后 STOPPED/可重启行为，并可用于等待完成先前 TryDestroy 开始的退休。

## Resolver

Closing 幂等关闭接纳，已受理查询和回调继续排空。自身回调可以调用 TryDestroy，
但得到 BUSY；已开始退休的服务可以继续由外部所有者调用 TryDestroy 或原同步
Destroy 完成。生命周期调用须与 Resolve/Clear/Stats 以及其他生命周期调用串行。

在所有线程成功 join 前，不释放任何线程句柄、查询表或共享存储。同步 Destroy 的
等待失败也保留拥有权，不再先销毁句柄/释放共享状态然后返回 false。成功消费的是
创建者拥有权；调用方独立保留的 ResolveOp 和结果可以继续存活，由最后引用释放
Resolver 的同步外壳。已经消费的创建者指针不能因外壳尚存而再次销毁。

## 验证与边界

`net_service_retirement_tests` 同时包含模块化与单头文件入口。实际场景包括 Pin、
阻塞回调、外借池块、阻塞 Lookup、已受理队列/Timer、线程键析构尾部、真实停机
失败及重试、独立操作结果、调用前错误身份和无分配清理。最终检查整个进程的
分配/释放、活动字节和非法访问计数；仅在全部服务完成后执行终态线程存储退休。

本合同尚不是完整对象拥有图，也未认证原生 Future producer、取消观察器、网络
句柄、TLS 或任意回调数据的传递拥有关系。语言模块需要明确的 IR 服务 ABI、独立的
语义 finalizer 状态和可重试物理退休状态；不能通过清空拥有槽、伪造空图或将 BUSY
当成成功来接入。未经这些准入检查不能据此卸载含活动服务的生成代码。
