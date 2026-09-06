# 范例工程 API 易用性（DX）摩擦清单

> 来源：补齐核心库全部 2958 个 API 范例的过程。本清单与
> [EXAMPLES_LIBRARY_ISSUES.md](EXAMPLES_LIBRARY_ISSUES.md)（缺陷级问题）互补：
> 那份回答"行为是否错了"，本份回答"API 用起来哪里不顺、这些不顺可能
> 暗示什么设计问题"。
>
> 收录标准：每一条都曾让一个按头文件直觉编写的范例**编译通过但断言失败
> 或行为异常**，并经独立探针确认真实行为后才记录。纯粹的笔误不计入。
>
> 条目格式：**摩擦**（调用方视角的事实）→ **潜在问题假设**（为什么这可能
> 不只是"学习成本"）。日期：2026-09-06 整理。

---

## 一、生命周期与所有权（崩溃/双重释放高发区）

这一类摩擦全部以**内存错误**而非结构化错误收场，是 DX 风险最高的一类。

### 1.1 `FutureValue` 是借用而非移交 —— 释放它即双重释放

- 摩擦：Future 完成后 `xrtFutureValue` 返回的指针是 Future 自己持有的引用，
  调用方"帮 Future 释放"造成双重释放。实测在无 ASan 环境下表现为 FLS 堆
  缓存链表损坏，极难定位（当时经 gdb + 二分 + 0/20 复现才归零）。
- 潜在问题假设：`Value` 与 `ValueTake`（若存在）语义边界全靠文档；借用式
  取值配对释放式 API 的命名若不显式区分，这是每个调用方都会踩一次的雷。
  建议库侧审视 Future 族是否存在 *Take 变体或文档强化。
- 来源：udp 批次（`examples/network/udp_batch`）。

### 1.2 `Attach` 成功即接管引用，调用方再 Destroy 即双重释放

- 摩擦：`xrtTlsStreamAttach`、`xrtWsConnAttach` 一族"成功即所有权转移"，
  失败则不转移——同一行代码的失败路径和成功路径所有权不同，清理代码
  很难写对。
- 潜在问题假设：这类"条件性转移"契约建议在 API 名字上显式化（如
  AttachTake）或提供出参报告所有权；否则每个新调用方的 Cleanup 分支
  都是一次赌博。
- 来源：tls_stream 批次（`examples/tls/stream_tour`）。

### 1.3 `FeedBorrow` 借用数据的存活期契约是隐式顺序约束

- 摩擦：喂入→驱动对端→消费源 Span 的三步顺序不能乱；提前 `SendConsume`
  即悬垂指针，但报错是协议层的 "unknown content type"，与真实根因无关。
- 潜在问题假设：错误信息与根因错位（内存错误被协议层捕获）说明内部
  缺少借用期校验。可考虑 debug 模式下记录借用范围。
- 来源：tls_session 批次（`examples/tls/session_tour`）。

### 1.4 未析构 Writer 隐式发送 1011 Close

- 摩擦：`xwsWriterDestroy` 放弃未完成消息时会**替调用方发送协议层
  Close 帧**——析构函数带网络副作用。
- 潜在问题假设：析构带协议副作用违背最小惊奇原则；调用方很难从
  "连接莫名关闭"反推到是某处 Writer 没收尾。
- 来源：xws 批次。

---

## 二、线程与上下文契约（"只能在某处调用"是最大暗坑）

### 2.1 Worker 专用 API 只在错误信息里说明

- 摩擦：TLS/UDP/TCP Stream、Ws 的大多数操作都是 Worker 专用，但头文件
  多数不标注；跨线程调用得到 STATE 错误（运气好）或静默无效（运气差，
  见问题清单 A-4 一族）。
- 潜在问题假设：契约靠错误码事后传达。建议头文件统一加"线程：所属
  Worker"标注，并让跨线程调用一律返回结构化错误而非静默无效。
- 来源：udp/tls_stream/websocket_stream 多个批次。

### 2.2 `FutureBridgeWait` 调用顺序错误 = 永久阻塞，无超时无诊断

- 摩擦：它是给异步操作线程的装配窗口等待，必须在 Ready/Fail 之后调用；
  顺序颠倒则永久死锁。
- 潜在问题假设：一个没有超时参数、顺序错误即挂死的等待 API，几乎每个
  首次使用者都会挂死一次。建议加超时或在错误状态下返回错误。
- 来源：future_bridge 批次（`examples/concurrency/bridge_tour`）。

### 2.3 在 Worker 任务里等待本 Worker 的 Future = 死锁

- 摩擦：异步发送族"任意线程可调"，但它的 Future 若在所属 Worker 上
  等待就自我死锁——接口级线程安全与等待级线程安全是两回事。
- 潜在问题假设：建议在 Worker 上下文检测下让 WaitFor 返回错误而不是
  挂死。
- 来源：xws 批次（异步族从 Worker 任务改到主线程直调才通）。

### 2.4 `xrtNetPost` 的 Post 节点是一次性的，且错误码不指向原因

- 摩擦：同一个 `xnetpost` 第二次投递报 "operation is not valid in the
  current state"——不读实现不知道是节点复用问题。
- 潜在问题假设：一次性节点是零分配设计的选择，但错误信息应直说
  "post node already queued"。
- 来源：net engine 批次。

---

## 三、三态/极性返回值（反直觉判定）

### 3.1 AGAIN 语义下的 `Required` 是双重含义

- 摩擦：`xrtTlsHandshakeParse`/`ExtensionParse` 的 Required，头未凑齐时
  给 4（头大小）、头已齐给整条大小——同一字段两种口径，两种都猜错过。
- 潜在问题假设：字段语义应文档化为"下次调用前至少要凑够的字节数"并
  举例；实现本身自洽。
- 来源：tls message_tour 批次（探针扫 0..7 字节全表才确认）。

### 3.2 `CrlStatus` 未吊销返回 `VALUE+GOOD` 而非 `DONE`

- 摩擦：直觉把 DONE 当"未吊销"，实际 DONE 是"CRL 不适用"；GOOD 也是
  确定答案。
- 潜在问题假设：三态枚举承担了两维信息（适用性 × 结果），建议文档配
  状态矩阵表。
- 来源：x509 批次（`examples/x509/crl_tour`）。

### 3.3 `FutureBridgeFail` 返回真表示"发布失败"这件事**成功**了

- 摩擦：`Fail` 成功返回 true，且发布失败后 Promise 仍归调用方可写终态
  ——两处极性都与直觉相反。
- 潜在问题假设：命名建议（FailPublished 之类）或文档强化；行为本身
  自洽。
- 来源：future_bridge 批次。

### 3.4 队列 `PopBatch` 恰好清空返回 OK 而非 EMPTY

- 摩擦：部分弹空与恰好弹空的返回值不同，循环退出条件容易写错。
- 潜在问题假设：设计自洽（OK=有产出），但属于必须读实现才能确定的
  边界口径。
- 来源：queue 批次。

---

## 四、初始化契约不一致（相邻结构体零值语义不同）

### 4.1 零值合法性无规律

- 摩擦：同为 xws 握手配置，`xwsclientconfig` 零值是最小合法形态、
  `xwsserverconfig` 零值非法（须 ConfigInit）；`xhttp1limits` 零值非法；
  `xwsconnconfig` 零值非法。调用方对每个结构体都要试一次。
- 潜在问题假设：零值语义应全库统一（要么全部最小合法，要么全部
  必 Init），逐结构体不同是最典型的隐式契约税。
- 来源：xws/http1 批次。

### 4.2 无 Init 函数且含指针槽的结构体 = 调用方 memset 义务

- 摩擦：`xhttp1message`、`xx509crlset` 未清零直接用是段错误（已列入
  问题清单 C-1/C-2）。
- 潜在问题假设：从 DX 角度重申——含指针槽的结构体应配 Init 或入口
  校验。
- 来源：http1/x509 批次。

---

## 五、参数形态与命名不一致（同类 API 不同面孔）

### 5.1 析构函数命名三轨制

- 摩擦：`xrtLogFree` / `xrtTlsIdentityRelease` / `xrtFutureDestroy` /
  `xrtX509StoreFree` / `xrtWsGroupDestroy`——Free、Release、Destroy 并存，
  无规律可循。实测把 `xrtLogRelease` 当存在用过一次。
- 潜在问题假设：命名约定统一是低成本高收益的库级改进；现在的混用
  让"猜函数名"必然猜错一次。
- 来源：logger 批次等多个批次。

### 5.2 时间单位混用：`xrtSleep` 毫秒 vs `DeadlineAfter`/`WaitFor` 微秒

- 摩擦：`xrtSleep(50000)` 是 50 秒不是 50 毫秒——单位不一致直接导致
  范例挂死。同库内 Sleep 用毫秒、Deadline/Future 超时用微秒。
- 潜在问题假设：全库统一微秒（或毫秒）并保留旧名兼容；现状是每个
  新用户至少踩一次的定时炸弹。
- 来源：thread 批次（实测 50000 成 50 秒）。

### 5.3 相邻 API 参数形态漂移

- 摩擦（各批次实录）：
  - `WriterHostName` 收 `xbytesview`、`MatchHost` 收 `xstrview`（同为
    tls 名称类接口）；
  - `WriterWrite` 收散参、同族其余收视图/结构；
  - `PercentDecode` 四参无 config，同族其余有 config；
  - `ReverseTo` 三参、同族四参；
  - `Set` 族谓词三参（bProper 在第三参）易漏。
- 潜在问题假设：家族内形态应一致；漂移通常是不同时期加入未经家族
  review 的信号。
- 来源：tls/io/codec/set 批次。

### 5.4 加密层密钥形态裸/DER 混用

- 摩擦：`xrtEcdsaP256` 族收裸 32 字节标量，而库内 TLS 身份、证书里的
  密钥都是 SEC1 DER——从"拿库里的密钥签名"到"能用的参数"需要手工
  偏移解析。
- 潜在问题假设：建议提供 DER 解析变体或文档示例标明裸标量口径。
- 来源：tls 最后批次（签名链路调试半日才定位）。

---

## 六、计数与长度语义

- **supported_versions 列表长 = 字节数而非条目数**（tls）：8 位长度字段
  承载两种口径的常见混淆点，建议文档给出两例对照。
- **`xwalkstats.Items` 含根目录自身**（file_async）：清空判定须看
  `Files==0` 而非 `Items==0`。
- **`FieldBlockCount` 不含终止空行**（http）：对"块"的边界定义与
  线格式直觉差一行。
- **`CopyN/CopyLimit` 计数出参只在失败时写入**（io）：成功路径拿不到
  实际复制量——对"上限复制"场景调用方无法得知复制了多少。
  潜在问题假设：这条更接近缺陷而非摩擦，建议成功也写入。
- 来源：tls/file_async/http/io 各批次。

---

## 七、平台与后端能力分裂

### 7.1 Windows 端口能力分属两个后端，AUTO 不互补

- 摩擦：IOCP 只有 completion（Watch 报 no readiness）、SELECT 只有
  readiness（提交报 no completion）；AUTO 只选其一。范例被迫双端口。
- 潜在问题假设：AUTO 的语义预期是"能力完备"，实际是"任选其一"；
  至少应在 AUTO 文档中说明，或提供组合后端。
- 来源：net port 批次（`examples/network/port_tour`）。

### 7.2 Windows 环回收包时序：SendBatch 后第二包可能迟到

- 摩擦：`RecvBatch` 返回的是"已到达前缀"，与发送成功无对价关系，
  批量断言必须写成补收循环。
- 潜在问题假设：这是平台行为，但 API 命名（Batch）暗示原子性；
  建议文档明示"前缀语义"。
- 来源：net socket 批次。

### 7.3 非阻塞 Connect 的 AGAIN 分支不能二次调用

- 摩擦：AGAIN 后重调 Connect 直接失败（在途 socket 重连），必须等
  FinishConnect——但这个约束只有踩到才知道。
- 潜在问题假设：建议 AGAIN 二次调用返回 AGAIN 而非失败，或错误信息
  指明"connect already in flight"。
- 来源：net socket 批次。

---

## 八、事件与状态机边界

- **一次 `PortWait` 同批可返回多个操作终态**（net port）：逐事件等取
  时若不暂存同批其余事件会**静默丢失**它们。潜在问题假设：丢失已到达
  事件的 API 形态值得复核（也许 Wait 应提供"取出全部"配套）。
- **`BodyTrailers` 重绑只在终止块后的 trailer 态合法**（http1）：且首块
  DATA 返回只消费到数据边界，剩余需补喂——状态门 + 部分消费两个细节
  叠加，正确用法需两层试错。
- **`PeerVerify`/peer 视图仅在验证器回调期间有效**（tls）：回调外使用
  是悬垂；无文档标注时必然踩。
- **executor Wait 族只对已关闭执行器有意义**：未关闭调用返回 ERROR 而
  非等待——与"Wait"名字的直觉相反。
- 来源：net port / http1 / tls / executor 批次。

---

## 汇总与优先级建议

按"崩溃风险 × 意外程度"排序，最值得库侧优先处理的十项：

1. `FutureValue` 借用/移交不可区分（双重释放，1.1）
2. `Fail`/`Attach` 一族的条件性所有权（1.2 / 3.3）
3. Worker 专用契约缺标注 + 静默无效（2.1，联动问题清单 A-4）
4. `BridgeWait` 无超时死锁（2.2）
5. Worker 上等待自身 Future 死锁（2.3）
6. 零值合法性无规律（4.1）
7. 析构命名三轨制（5.1）
8. 时间单位混用（5.2）
9. PortWait 同批事件静默丢失（八-1）
10. `CopyN/CopyLimit` 成功不写计数（六，接近缺陷）

其余条目属于"可文档化的口径"，建议批量补进各头文件注释——范例注释里
已按实测写就，可直接摘录。
