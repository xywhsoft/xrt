# XRT 代码库深度审计复核

复核日期：2026-08-25  
复核基线：`master aff1ac5a`  
输入报告：2026-08-24《XRT 代码库深度审计报告》

## 1. 结论

原审计报告整体质量较高，三个高危生命周期/契约问题都能从当前源码中得到证明，网络和 TLS 方面也找到了多项应在发布前修复的问题。但报告对若干底层流式 API、C 资源句柄和条件变量采用了高层拥有型 API 的判断口径，因而放大了风险；另有少数问题的实际竞态窗口与报告描述不同。

复核后的结论如下：

- 高危：3 项成立，全部应作为 P0 发布阻断项。
- 中危：6 项按原描述成立，2 项部分成立或实际触发路径不同，2 项不成立，1 项应降为低危健壮性加固。
- 低危：多数是有效的防御性或互操作改进；原始 socket 重复关闭、条件变量虚假唤醒、RSA `e = 3`、SAN 下划线、WebSocket Deflate 固定缓冲等表述不构成当前实现缺陷。
- 功能缺口：HRR、TLS 兼容 CCS 状态约束、服务端合并握手消息、TLS 解析/X.509/DNS fuzz 确实没有完全压实；`xmail` “源码不在树内”不成立，但根目录仍残留引用旧外部目录的适配文件，需要清理或迁移。
- 性能项：io_uring send-file、内存池页索引和若干有界 O(n^2) 路径值得优化；“release 一律关闭完整校验”不能直接执行，必须先用基准证明成本并保留标准库的基本安全检查。

## 2. 高危项复核

### H1. `xrtThreadKeyDestroy` 跨线程 UAF

**结论：成立，P0。**

证据：

- `src/concurrency/thread_key.c:312-336` 的线程退出清理槽仍通过 `pSlot->Key->Destroy` 读取键对象，并使用键对象清理平台 TLS 槽。
- `src/concurrency/thread_key.c:394-440` 的销毁只移除当前线程的槽，随后删除平台 key 并释放 `pKey`。
- 其他线程的清理链仍保存 `pSlot->Key`，在线程退出时会解引用已释放对象。
- `include/xrt/thread.h:73-74` 要求调用方保证其他线程停止访问，但 XRT 自己在线程退出时自动访问清理槽，因此当前文字契约不能消除该问题。

修复方案：

1. 键对象采用“逻辑关闭 + 引用计数”生命周期，每个线程槽持有一个键引用。
2. `Destroy` 只关闭新访问并释放调用方引用；最后一个槽退出后再删除原生 TLS key 并释放对象。
3. 键关闭后 `Get/Set` 返回状态错误，槽清理仍可安全调用析构器。
4. 不建议只把析构函数复制到槽中：Windows TLS 索引可能在过早 `TlsFree` 后复用，仍会产生错误清理。

Release gate：Windows、WSL 各执行“多线程 Set -> 提前 Destroy -> 并发退出”压力测试，验证无 UAF、析构次数准确、键索引复用不串值。

### H2. Worker 停止阶段受理的 Post 被永久遗失

**结论：成立，P0。**

证据：

- `src/network/engine.c:938-942` 先排空命令，再关闭 timer，最后才把 `Running` 清零。
- timer CLOSED 回调期间 `src/network/engine.c:2117-2129` 仍会接受嵌入式 post。
- 此后没有再次 drain；重启路径 `src/network/engine.c:1054-1064` 直接重置内部命令头和 pending 状态。
- `include/xrt/net.h:1606-1619` 对已受理 post 给出“执行一次”的合同，当前实现会令嵌入式 `Pending` 永久停留在 1。

修复方案：

1. 把 `Running` 布尔值替换为 `STARTING/RUNNING/DRAINING/CLOSED` 生命周期。
2. 进入 timer close 前切换到 `DRAINING`；拒绝新的公共 post，但允许关闭流程所需的内部回调。
3. timer close 后持续 drain 内部命令，直到 timer 和命令队列同时达到固定点。
4. 重启不得静默覆盖非空命令头；Debug 断言，Release 返回明确状态错误。

Release gate：timer CLOSED 回调 post、并发 stop/post、stop/restart 循环、嵌入式 pending 归零和 accepted/executed 计数一致性测试。

### H3. Port 公共 API 缺少 owner 线程约束

**结论：成立，P0；影响范围比报告列出的 `Cancel/Submit` 更广。**

证据：

- `include/xrt/net.h:1476` 未声明 `Cancel` 的线程亲和；只有 `Post/Wake` 明确允许跨线程。
- `src/internal/xrt_net_port.h:252-267` 没有保存 owner 线程 ID。
- `src/network/port.c` 的 watch、submit、cancel、wait、destroy 直接进入后端。
- IOCP operation 索引和 io_uring SQ/CQ 装配均按单 owner 线程设计，跨线程调用会破坏该假设。

修复方案：

1. Port 创建时记录 owner thread ID。
2. `Watch/Unwatch/Submit*/Cancel/Wait/Destroy` 统一调用 `__xrtNetPortRequireOwner`，错误返回 `XERR_THREAD` 或专用 port affinity 错误。
3. `Post/Wake` 保持跨线程安全；只读的稳定能力查询可以跨线程。
4. 头文件逐个函数写明线程约束，Engine 继续作为普通用户的跨线程高层入口。

Release gate：IOCP、select、epoll、kqueue、io_uring 后端分别验证跨线程调用被拒绝且未修改后端状态。

## 3. 中危项复核

| 项目 | 复核结论 | 处理建议 |
| --- | --- | --- |
| TCP connect READY 把 `AGAIN` 当失败 | 成立，`tcp.c:2229-2237` 与 `socket.c:3052-3053` 语义冲突 | `AGAIN` 重新 watch write/error，只有 `OK` 打开连接，真实错误才失败；增加虚假 READY 注入测试 |
| accept 遇 EMFILE 持续重注册 | 成立，`tcp.c:4656-4706` 可在 level-triggered 后端忙转 | 对 EMFILE/ENFILE/ENOBUFS/ENOMEM 及 Windows 等价错误暂停 listener，使用有上限退避 timer；POSIX 可增加 spare-fd 策略 |
| UDP 跨线程 post 失败泄漏 Take 缓冲 | 成立，`udp_send.c:493-510` 错误地传 `false` | 改为释放外部所有权，覆盖 Take/RefsTake/Post 拒绝的 release-once 测试 |
| `xrtNetBufFront` 把文件块作为字节 | 成立且范围更广 | `Front/Peek/Find/Pullup/Read` 都要明确文件 extent 边界；字节 API 遇文件块应停止或失败，内部 send-file 使用专用访问器 |
| task_net 装配窗口 UAF | 部分成立，报告给出的成功路径不成立 | `FutureBridgeWait` 会阻塞成功装配回调；真实竞态位于 `BridgeWatch` 失败后先 `BridgeFail`、再访问 `pTimer` 取消。使用双持有引用或显式 setup-owner/callback-owner 生命周期 |
| TLS 1.3 降级哨兵缺失 | 成立 | 客户端协商 TLS 1.2/更低时检查 `DOWNGRD` 标记；支持更高版本的服务端在降级 ServerHello 中写入标记；补双端向量测试 |
| X.509 路径默认接受 SHA-1 | 成立 | 路径/CRL验证默认拒绝 SHA-1，增加显式 legacy opt-in 或签名策略回调；信任锚自签名不作为普通链签名处理 |
| HTTP decode 默认无限制 | 不成立，不应按报告修改默认值 | 这是 callback 流式解码器，不持有完整输出；`http_decode.h:75` 已公开 unlimited 语义。拥有正文的 xhttp client 默认限制为 64 MiB。应补安全用法文档，并保证所有拥有型 helper 设置有限上限 |
| WebSocket message 默认 `SIZE_MAX` | 不成立，不应按报告修改底层默认值 | 消息层是无聚合状态机；stream 层默认 1 MiB。补充“流式层不限长、拥有型层必须限长”的分层文档即可 |
| release 堆池双重释放不可检测 | 现象成立，严重度降为低危 | C 中释放后再次传入指针属于调用方错误。可在池化块归还前清除/翻转 header magic，低成本提高顺序双释放检测；并发双释放不承诺可恢复 |
| xws 不校验 Origin | 部分成立，是高层 router 的策略缺口 | 底层 staged upgrade 已允许调用方在 accept 前检查请求；固定 router 没有授权/Origin hook。增加 `ANY/SAME_ORIGIN/CALLBACK` 策略，不能强制所有非浏览器客户端必须带 Origin |

其中 `NetBuf` 必须按整个 API 族修复，而不是只在 `Front` 增加一个分支。当前 `Peek`、`Find` 和 `Pullup` 也会把文件描述块的内部对象当作字节读取。

## 4. 低危与健壮性复核

### 4.1 确认需要修复

- `tcp.c:4599-4607`：accept identity 防御分支关闭 listener 但未关闭 `pEvent->Accepted`，应先关闭 accepted socket。
- `tcp_dial.c:525-543`：获胜候选 Adopt 失败后未拒绝其余候选，应统一进入“拒绝其他候选 + 终结”路径。
- `socket.c:4589-4595`：`recvmmsg` 使用最高约 27 KiB 的 VLA，接近 32 KiB 最小协程栈；改为 socket/worker 可复用 scratch，或按小批次固定数组处理。
- 各 readiness 后端 `timeout == 0` 返回语义不一致；统一定义为“poll 成功、可以没有事件”，deadline 到期由 port 核心判断。
- `engine.c:1905-1909`：命令已经入队后 wake 失败被吞，通常不会丢命令，但会产生最长 IdleWait 延迟；增加 wake error 统计和有限重试。
- `xrt_internal.h:362-390`：TinyCC POSIX 的原子引用辅助退化为 volatile 读/CAS，不能满足 ARM64 弱内存序；复用 `xrt_atomic.h` 的架构实现。
- `memory/debug.c:1036-1063`：诊断 visit 在栈上放置约 24 KiB 快照；改用不经过被诊断堆的 backing allocator 或受控分块。
- 无 verifier 的 TLS client 失败过晚；无 VERIFY 裁剪构建会暂停在证书阶段。创建时要求 verifier，只有显式 `resume-only/PSK-only` 模式可以无 verifier，并禁止静默回退完整握手。
- TLS 1.2 `ServerKeyExchange` 签名方案应与协商套件的 RSA/ECDSA authentication family 一致。
- TLS 1.3 兼容 CCS 需要按状态和次数限制；服务端应像客户端一样消费同一 record 中的多条握手消息。
- resolver 成功结果做 cache ref 时若引用失败，当前会写入“无地址、无错误”的缓存项；引用失败应直接跳过缓存并保留原查询结果。
- Windows interface measure 应与 write 使用相同的硬件地址截断函数，消除隐性过量计量。
- HTTP chunk-size 后的 BWS 当前只在后接扩展时接受、在后接 CRLF 时拒绝；建议严格拒绝 chunk-size 后全部 BWS，避免代理解析分歧。

### 4.2 不构成当前缺陷或仅需文档说明

- 原始 `xnetsocket*` 重复 close：close 消耗句柄，释放后再次使用属于无效句柄使用；为此给底层 socket 增加引用计数会增加所有网络热路径成本。只需明确文档。
- IOCP `>4 GiB SendFile`：该入口不是公共 Port API，TCP 上层已按 `INT_MAX` 分块；可加内部范围断言，但不存在报告描述的公共静默截断合同。
- condition variable 虚假唤醒：条件变量 API 只通知，不保证谓词成立；实现正确，文档和示例应明确必须在谓词循环中等待。
- detached coroutine wake：公开合同已经要求 handle 有效，内置等待适配器在释放前会同步注销 watcher；目前没有证据证明存在 UAF。可增加内部生命周期注释和 Debug 检查。
- Windows 目录通配符：普通 Windows 文件名不能含 `*`/`?`，报告描述的“枚举到非请求目录”没有得到可执行路径证明。可为跨平台一致性提前拒绝通配字符。
- RSA `e = 3`：密码学原语支持合法奇指数不是缺陷；若要强制现代部署基线，应放在 X.509/TLS 密钥策略层，而不是删除底层能力。
- SAN 下划线：GeneralName 解析只要求 IA5，不会因下划线拒绝整张证书；身份匹配会拒绝把下划线当 DNS host reference，这是 fail-closed 行为。报告描述不成立。
- WebSocket deflater create：`xrtMalloc` 返回 NULL 时不存在泄漏；128 字节常量当前有“精确最大长度”测试，现有参数组合不会溢出。未来扩展参数时应由长度查询/静态断言保护，而不是认定当前越界。

## 5. 功能完整性复核

### 5.1 TLS

- HRR：字段解析、写出、transcript 重建和协商选择已经存在，但 client/server 状态机明确返回 unsupported，属于真实半成品。
- 0-RTT：存在 `early_data` 字段解析，但没有发送、接收、反重放和应用确认契约。发布前应明确拒绝 early data；不建议在没有 replay policy 前匆忙实现。
- `TLS_FALLBACK_SCSV`：未实现。若 TLS policy 允许主动版本回退，应补；若库从不做 fallback，则至少明确拒绝/忽略策略和文档。
- 服务端合并握手消息：`server.c:794-798` 拒绝 record 尾随第二条消息，属于真实互操作缺口。

### 5.2 HTTP/WebSocket 限额

当前四种默认值并非天然矛盾，而是分层差异：

- HTTP decode：流式 callback，默认不限长。
- xhttp client body：拥有型结果，默认解压上限 64 MiB。
- WebSocket message：流式消息状态，默认不限长。
- WebSocket stream：连接拥有型控制，默认消息上限 1 MiB。

需要统一的是“谁拥有内存，谁必须设置有限默认值”的契约和命名，而不是把所有底层流处理器硬编码成同一个限额。

### 5.3 xmail

现代 SMTP/POP3/IMAP 实现实际位于 `extlibs/xmail/src/{smtp,pop3,imap,transport}`，TLS 使用显式 `xtlsverifier*`，对应本地测试也存在。因此“依赖源码不在树内、verify_peer 无法核实”不成立。

但 `extlibs/xmail` 根目录的旧 `xmail_xlang.c`、`xmail.h`、`build_test.bat` 仍引用已不存在的 `../xsmtp`、`../xpop3`、`../ximap`，这是应在发布收口时删除或迁移的遗留边界问题。

### 5.4 Fuzz

核心 fuzz 目前只有 `fuzz/http1_protocol.c` 和 `fuzz/websocket_protocol.c`。xhttp 另有 auth/route/router/SSE harness，但 TLS record/handshake、X.509/ASN.1、DNS/地址解析没有独立 libFuzzer gate，原报告判断成立。

## 6. 性能项复核

| 项目 | 判断 | 建议 |
| --- | --- | --- |
| io_uring send-file 每次只推进 pipe capacity | 成立 | 使用更大的 pipe、链接 splice SQE 或在公平性预算内连续推进；以 1 GiB send-file 的 completions/GiB 和吞吐作为门禁 |
| 热路径完整 valid 检查 | 有优化可能，不应直接按 NDEBUG 删除 | 先 profile；将不变量拆为低成本 public guard 与 Debug deep-check，Magic/状态/范围检查在 Release 保留 |
| memory_pool 页插入和 rebuild | 成立，插入 O(n)、重建插入排序 O(n^2) | 小规模保留数组；超过阈值改一次性 `qsort`/稳定排序或分层索引，避免为常见小池增加树结构成本 |
| heap 大块走全局锁 | 需要基准证明 | 增加按线程缓存会扩大驻留和复杂度，先测试大块 churn，再决定小型 per-worker cache 或直接系统 allocator |
| HTTP/WS/X.509/ASN.1 有界 O(n^2) | 现象存在但都有上限 | 对达到上限的对抗输入做 CPU 基准；只有超过预算的路径才引入 hash/索引，避免正常小报文变重 |
| executor 无 TLS 时扫描 worker | 成立但属于裁剪后端 | 可用 thread key 保存 worker；必须确认不会反向耦合或增加最小裁剪闭包 |

## 7. 修复顺序

### P0：发布阻断

1. Thread key 延迟销毁。
2. Engine 生命周期和 shutdown drain。
3. Port owner 线程防护。
4. TCP connect `AGAIN`、accept 资源错误退避。
5. UDP Take 所有权释放、NetBuf 文件 extent 语义。
6. task_net 装配失败竞态。
7. TLS downgrade sentinel、SHA-1 路径默认策略、TLS client verifier 创建期契约。

### P1：协议压实

1. TLS HRR 双端状态机。
2. TLS 1.2 suite/signature 绑定、TLS 1.3 CCS 窗口、服务端合并握手消息。
3. 明确拒绝尚未实现的 0-RTT；补 fallback SCSV 策略。
4. xws router Origin/authorization hook。
5. HTTP chunk-size 严格 BWS 规则。
6. TLS、X.509/ASN.1、DNS fuzz harness 与语料库。

### P2：健壮性和跨平台

1. 堆池顺序双释放检测。
2. recvmmsg 和 memory debug 大栈对象清理。
3. TinyCC ARM64 原子内存序。
4. accept/dial 防御分支、resolver cache、interface measure/write 一致性。
5. 后端 zero-poll 统一和 wake error 可观测性。

### P3：按基准驱动的性能工作

1. io_uring send-file completion 合并。
2. memory_pool 大页数索引。
3. 解析器上限输入 CPU 放大测试。
4. executor 无 TLS 路径和 Release 校验分级。

## 8. 验证情况

本次在 Windows x64/GCC 上执行了受影响模块的现有基线：

```text
python tools/build.py --compiler gcc --arch x64 \
  --suite thread_key_context,net_buffer,task_net_oom_tests,tls_client_tests,\
x509_path_rsa_tests,http1_message,websocket_deflater --no-examples --jobs 4
```

结果：模块化测试和单头文件测试全部通过。该结果说明当前正常路径基线健康，但现有测试没有覆盖本报告中的跨线程销毁、shutdown post、跨线程 Port、BridgeWatch 失败竞态、文件 extent 字节访问和协议策略缺口，因此不能推翻静态证明出的缺陷。

最终 release gate 应至少包含：Windows IOCP/select、WSL epoll/io_uring、ASan/UBSan、OOM 注入、裁剪闭包、单头文件，以及上述每个缺陷对应的定向回归测试。
