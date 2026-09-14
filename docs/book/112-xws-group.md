---
num: 112
slug: xws-group
title: xws（下）：连接组与广播
volume: 卷十一 其他扩展库
type: practice
lead: 唯一成员与引用生命周期、硬容量与封闭、锁外分配的稳定快照、可等待的批量广播——从“发一条”到“发全体”。
api: xws-websocket_group, xws-websocket_runtime
---

## 导读

推送系统的基本操作是“给全体在线连接发一条消息”——`xwsgroup` 把“连接的集合”做成正确的并发容器：**唯一成员**（同一连接加入两次还是一个成员）、**引用生命周期**（Add 持引用、Remove/Clear/Destroy 归还——不关闭不中止连接）、**硬容量**（Limit 满了新成员得到可重试的容量错误）、**封闭**（Seal 永久停止接入——服务器停止收新会话的标准动作）、**稳定快照**（Snapshot 按加入序复制成员并逐个增持——锁外分配，大快照不阻塞 Add/Remove）与**批量广播**（同步/异步两形态；异步返回**可等待的操作对象**——取消等待不破坏各连接受理的数据所有权）。广播不绕过单连接上限：慢连接按你的策略跳过、等待或关闭，不拖出无界共享队列。

## 引入

“维护在线连接列表”看起来是 `数组+锁` 的入门题——直到并发细节排队而来：同一连接的 Open 回调与 Close 回调在不同 Worker 上并发执行，Add 与 Remove 竞争；遍历广播期间成员增删（遍历器失效——第 18 章老问题）；慢连接在广播时占住整批（一拖 N）；服务器停机时“先别收新的、旧的优雅关”（半关闭状态）。每一条都是真实推送服务的日常。

xwsgroup 的答案逐条对应：唯一成员语义消化重复 Add；Snapshot 的“锁内线性化、锁外分配”让遍历拿到稳定集（快照自己持引用，原组销毁也不影响——遍历器失效不可能）；广播是**每连接独立受理**（慢连接不拖别人）+ 聚合等待（异步操作对象）；Seal 提供半关闭。这章的价值不在 API 数量（十来个入口）而在**每条并发语义都想清楚了**——把它当“并发容器设计课”的 WebSocket 实例读。

## 概念

### 生命周期与唯一成员

```diagram flow
- 创建：GroupCreate(Limit)——0=无显式上限；Ref/Destroy 共享
- 加入：Add 成功持有一个连接引用；同一指针重复 Add 保持成功但不形成第二个成员
- 移除：Remove/Clear/Destroy 归还引用——不关闭不中止连接（组的职责边界）
- 查询：Has/Count/Limit 加锁瞬时快照；Sealed 查询永久封闭状态
```

**唯一成员**的意义：Open 回调重放（网络层偶发）不会造成双份引用泄漏；Remove 恰好归还 Add 的那一份。**容量语义**：Limit 满时 Add 返回失败、错误为 `XERR_AGAIN / XWS_GROUP_ERROR_CAPACITY`——**可重试**（移除成员后能再加）不是协议错误；这个错误类别设计把“过载”与“非法”分开（第 4 章类别模型的容器层应用）。

### 封闭：半关闭状态

`xrtWsGroupSeal` 永久且幂等：封闭后不能加新成员，但**重复加入已有成员仍成功**（幂等语义连续）、移除/清空/快照照常。停机流程的标准顺序：停止接受新连接（监听层）→ Seal 组（在途 Open 不再进组）→ 对现有会话逐个优雅关闭或中止——“先把入口关上再清场”。

### 快照：稳定遍历的正确姿势

`xrtWsGroupSnapshotCreate`：按成员加入顺序复制当前成员、为每个成员**增加引用**；关键工程细节——**连续快照存储在组锁外分配**：先无锁试探分配，重新取锁后若成员增长超容量就释放重试，容量足够才在**线性化点**保序增持。效果：大快照的分配工作（可能触发分配器锁）不阻塞 Add/Remove/Clear 的热路径。快照**不持有组**——原组销毁后快照成员仍有效（成员的引用在快照上）；用完 `SnapshotDestroy` 归还。这就是第 18 章“容器失效规则”的解法形态：**要稳定遍历时复制一份加引用的快照，而不是在原容器上边走边赌**。

### 批量广播：独立受理与聚合等待

同步形态：遍历快照逐连接发送（每连接的 AGAIN/失败独立处理——跳过、记慢、按策略关闭）。异步形态：`xrtWsGroupTextAsync` 等入口返回 `xwsgroupop` **操作对象**——`GroupOpWait` 等待全批完成、销毁释放；**取消聚合等待不破坏已经被各连接受理的数据所有权**（第 110 章“取消≠销毁”的批量版）。广播不绕过单连接发送上限——慢连接的队列照常受限，你的策略决定它的命运。

### 与发送三态的配合

广播的消息载荷天然适合 **ref 形态**（第 111 章）：一块静态载荷、引用交给 N 个连接的队列——释放回调计数（最后写完才真释放）。组广播的异步入口内部正是这个形态的批量应用；自写遍历时用 `xrtWsConnBinaryRef` 手工组装同款（第 111 章练习做过）。

## 示例

### 第一个完整程序：组的生命周期骨架

下面的程序来自 `examples/websocket/group`——Add/Remove 的标准接入形态：

```embed path="extlibs/xws/examples/websocket/group/main.c" title="extlibs/xws/examples/websocket/group/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/group/main.c -lws2_32 -liphlpapi
WebSocket connection group is ready
```

**刚才发生了什么。** ① `xrtWsGroupCreate(0)` 建无上限组（生产给 Limit——容量是过载防线）。② 两个函数是**接入模板**：`connectionOpen`（升级成功回调里 `GroupAdd`——连接进组）、`connectionClose`（唯一 Close 回调里 `GroupRemove`——归还引用）。真实服务器把组放进路由上下文（第 113 章 `Data` 字段的用武之地）——Open/Close 一行接入、组的并发语义全部继承。③ 本示例 main 只验证就绪（模板形态）；`connectionOpen/Close` 的 `(void)` 引用是编译期“保留回调形状”的自证。

### 第二个完整程序：可等待的空组广播

第二个程序来自 `examples/websocket/group_future`——异步广播操作对象的完整生命周期：

```embed path="extlibs/xws/examples/websocket/group_future/main.c" title="extlibs/xws/examples/websocket/group_future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/group_future/main.c -lws2_32 -liphlpapi
（空组广播操作完成自检通过后正常退出）
```

**刚才发生了什么。** ① `xrtWsGroupTextAsync(组, 消息)` 对**空组**也返回完整操作对象——头注释点明设计意图：“空组广播也会返回可等待、可检查的完整操作对象”——空集不是错误，聚合语义自洽（零连接全成功）。② `GroupOpWait` 等待全批完成（`XWAIT_OK`）、`GroupOpDestroy` 释放操作对象——异步广播的三步生命周期（发起/等待/释放）。③ 把两个示例连起来读：Open 进组后 `TextAsync` 广播——每个成员独立受理；等待返回时全批已有结论。慢连接策略（跳过/关闭）在配置与你的遍历代码里——组不替你决定。

## 契约

- **唯一成员**：重复 Add 同一指针成功但不加倍；Remove/Clear/Destroy 恰好归还 Add 的引用；组操作**不关闭不中止**连接。
- **容量**：Limit 零=无上限；满员 Add 返回 `XERR_AGAIN/CAPACITY`（可重试非协议错）。
- **封闭**：Seal 永久幂等；封后不加新、重复加旧仍成功；移除/清空/快照照常——停机先 Seal 再清场。
- **快照**：锁外分配+锁内线性化增持；按加入序保序；不持组（组销毁后成员有效）；用完 Destroy。
- **查询**：Has/Count/Limit/Sealed 皆加锁瞬时快照——不能替代稳定遍历（要 Snapshot）。
- **广播独立受理**：不绕过单连接上限；慢连接策略（跳过/等待/关闭）归调用方。
- **异步操作对象**：TextAsync 族返回可等待可检查对象；取消等待不破坏已受理所有权。
- **锁纪律**：Clear 在锁内换空集、锁外归还旧引用——析构与分配器工作不占组锁。
- **opaque 校验**：组/快照/操作皆 opaque；解引用前验固定头与连续存储；回绕按参数错拒绝；`Destroy(NULL)` 无操作。
- **裁剪**：`WEBSOCKET_GROUP`/`_GROUP_FUTURE` 独立；组依赖连接与 set/mutex——帧层不带入。

## 避坑

### 坑 1：广播时在原组上边遍历边发

症状：偶发遍历错乱或漏发——遍历期间成员增删（新连接进组、旧连接关闭移除），迭代状态失效。

原因：组的 Count/Has 是瞬时快照——遍历过程中集合在变。“边走边赌”就是第 18 章容器失效的老坑。

```c bad
size_t n = xrtWsGroupCount(pGroup);
for ( size_t i = 0; i < n; i++ ) {
	/* 取第 i 个成员——期间 Remove 可能已改变集合 */
}
```

```c good
xwsgroupsnapshot* pSnap = xrtWsGroupSnapshotCreate(pGroup);
for ( size_t i = 0; i < xrtWsGroupSnapshotCount(pSnap); i++ ) {
	xwsconn* pConn = xrtWsGroupSnapshotGet(pSnap, i);
	send_or_skip(pConn);   /* 稳定集：遍历期间增删不影响本快照 */
}
xrtWsGroupSnapshotDestroy(pSnap);
```

### 坑 2：Remove 之后以为连接被关了

症状：客户端“还在收消息”——你以为移出组就断开，实际连接活得好好的。

原因：组的职责是**成员关系**不是连接生命周期——Remove 只归还引用；关闭是 `xrtWsConnClose/Abort` 的事（第 110 章）。职责分离让“移出广播组但保持管理连接”这类组合成为可能。

```c bad
xrtWsGroupRemove(pGroup, pConn);
/* 以为连接关了——对端继续在线 */
```

```c good
xrtWsGroupRemove(pGroup, pConn);       /* 先出组 */
if ( !xrtWsConnClose(pConn, XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("kicked")) ) {
	xrtWsConnAbort(pConn);              /* 再按策略关闭 */
}
```

### 坑 3：停机时不 Seal 直接销毁组

症状：停机瞬间新 Open 还在进组——组销毁后这些连接的引用悬空；或关停顺序竞态导致崩溃。

原因：销毁组只归还组持的引用；在途的 Add（回调正在跑）需要封闭来挡。“先 Seal 再清场”是唯一安全顺序。

```c bad
stop_listener();
xrtWsGroupDestroy(pGroup);   /* 在途 Open 的 Add 还没跑完 */
```

```c good
stop_listener();             /* 1. 不再接受新连接 */
xrtWsGroupSeal(pGroup);      /* 2. 封闭：在途 Open 的 Add 拒绝 */
drain_workers();             /* 3. 等回调跑完（Open/Close 静默） */
close_all_members(pGroup);   /* 4. 优雅关闭现有会话 */
xrtWsGroupDestroy(pGroup);   /* 5. 最后销毁组 */
```

## 练习

### 基础：生命周期与容量实验

Limit=3 的组：Add 四个连接——第四个失败（容量错误）；Remove 一个后再 Add 成功；Seal 后 Add 已有成员成功、新成员失败。验收标准：六步行为与契约一致；错误码可区分容量与封闭。

### 进阶：快照广播器

快照遍历广播 ref 载荷：一块静态消息、`ConnBinaryRef` 到快照全体、释放回调计数验证恰好一次。并发开关连接（随机 Add/Remove）的同时循环广播 1000 轮。验收标准：零崩溃零泄漏（第 6 章统计）；每轮送达数与当时快照数一致。

### 挑战：频道系统

多频道（组的数组/映射）：订阅/退订（Add/Remove）、按频道广播、全局通告（全部频道）。实现 kick（出组+关闭）与停机流程（Seal→排空→逐频道关闭→销毁）。验收标准：三频道并发压测零错乱；kick 立即停止接收；停机全链干净退出（终态计数对账）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 定位 | 连接的并发容器：成员关系不管生命周期——不关闭不中止 |
| 唯一成员 | 重复 Add 成功不加倍；Remove 恰好归还一份 |
| 容量 | Limit 零=无上限；满员 AGAIN/CAPACITY——可重试非错误 |
| 封闭 | Seal 永久幂等；封新不封旧；停机第一步 |
| 快照 | 锁外分配+锁内增持；按加入序；不持组；稳定遍历唯一正解 |
| 查询 | Has/Count/Limit/Sealed 瞬时——不能当遍历用 |
| 广播 | 每连接独立受理；不绕过单连接上限；慢连接策略归你 |
| 异步操作 | TextAsync 族返回可等待对象；取消等待不破坏所有权 |
| 锁纪律 | Clear 锁内换集锁外归还——热路径不被分配阻塞 |
| 接入模板 | Open 里 Add、Close 里 Remove；组放路由 Data（下章） |
