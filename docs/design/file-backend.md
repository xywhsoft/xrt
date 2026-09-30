# xfile 内部 backend 合同

状态：`XRT-101` 至 `XRT-104` 的实现合同。该接口属于 xrt 内部 ABI，VFS provider
不能直接依赖它；公开 provider ABI 由 `vfs.h` 单独定义，并由 VFS 实现把 provider
handle 适配成 backend state。

## 目标

`xfile` 保存 backend ops、backend state、打开标志、capability 快照和可选 owner
及其 release 回调。read、write、read-at、write-at、seek、stat、resize、flush 与 close 都通过
backend 分派。原生文件是其中一个 backend，不再是 `xfile` 的结构定义。

这个分层允许后续 memory、pack 和外部 provider 返回普通 `xfile`，同时保留已有
文件 API。公共 `xfile` 仍是不透明指针，现有调用方和公共 ABI 不发生变化。

## ops 表

内部 `xrt_file_backend_ops` 以 `Size`、`Version` 和 `Capabilities` 开头。当前版本是
1。构造时必须满足以下条件：

- `Size` 必须等于当前结构大小，`Version` 必须为当前版本；
- capability 之外的未知位被拒绝；
- 每个 operation capability 必须与对应回调同时存在或同时缺失；
- `Close` 没有可选 capability，任何有效 backend 都必须提供；
- ops 表不被复制，必须具有静态生命周期，或至少活到最后一个相关 `xfile` 关闭。

capability 分为普通文件操作和原生逃生能力。普通操作包括 read、write、read-at、
write-at、seek、stat、resize、flush。`NATIVE`、`CONTROL_NATIVE` 和 `ASYNC_BIND`
只由能提供真实平台句柄与完成端口语义的 backend 声明。

## 所有权与关闭

`__xrtFileTakeBackend` 接管 state，无论成功还是失败，调用方都不能再访问或释放它：

- 成功时，state 由返回的 `xfile` 持有；
- ops 无效时，在能够安全识别 `Close` 的前提下调用一次 `Close`；
- wrapper 分配失败时调用一次 `Close`，同时保留原始内存错误；
- `xrtClose` 调用一次 backend `Close`，随后释放 wrapper；
- `Close` 消费 state，其他回调只借用 state。

回调执行期间，调用方必须保证同一个 `xfile` 没有被并发关闭。backend 自己负责其
合同允许的并发操作；core 不在每次分派外再增加全局锁。

## native backend

native state 保存平台数据句柄、可选控制句柄、Windows 共享游标锁，以及启用网络
文件模块时的异步绑定状态。wrapper 和 native state 使用一次连续分配，保持原有
打开和关闭路径的分配次数。

native backend 声明全部普通能力。启用 `XRT_FEATURE_NET_FILE` 时还声明
`ASYNC_BIND`。Windows 追加文件的数据句柄与控制句柄仍然分离；锁、resize 和 stat
通过 control handle 保留原语义。

## 原生打开的分配事务

原生对象及 inline backend state 由 `__xrtFileAlloc()` 在操作系统创建/截断前一次
预分配；`__xrtFileInitNativePair()` 不分配内存。普通路径、root-relative 和 disk
provider 使用同一规则，不得在成功创建/截断后因包装分配失败而报告 OOM。
已有句柄的接管仍可使用 `__xrtFileTakeNativePair()`；失败消费并关闭该句柄。
2026-09-30 主线整合的 `vfs_write_open_oom_tests` 穷举三条打开路径的分配失败，
检查原始含 NUL 字节未被截断以及 live allocation 账本平衡。

## 非 native backend

缺少普通 operation capability 时，对应公共 API 返回 `XERR_UNSUPPORTED`。缺少
原生能力时：

- `xrtFileNative` 返回 `-1` 和 `XERR_UNSUPPORTED`；
- file lock 通过缺少 `CONTROL_NATIVE` 返回 `XERR_UNSUPPORTED`；
- file map 通过缺少 `NATIVE` 返回 `XERR_UNSUPPORTED`；
- 完成式文件 I/O 通过缺少 `ASYNC_BIND` 返回 `XERR_UNSUPPORTED`。

不得把虚拟句柄伪装成整数原生句柄，也不得让 map、lock 或 IOCP 直接读取 backend
state 的私有布局。

## 验证要求

每次修改 backend 合同必须验证：

1. modular 与 single-header 的 read/write/position/stat/resize/flush 分派；
2. capability 与回调不一致时拒绝构造并且 state 只关闭一次；
3. wrapper OOM 时保留内存错误并且 state 只关闭一次；
4. 非 native backend 的 native、map、lock 和 async 路径返回 unsupported；
5. native file、file map、file lock、file async 在 Windows 和 POSIX 上回归通过；
6. ASan/LSan 和适用平台的并发检查不报告泄漏、越界或生命周期错误。
