# 原生端点的跨线程 Future

`rudp_endpoint_future` 是可选模块，依赖原生端点、XRT Future 和互斥锁。它把
异步请求送到端点绑定的 Worker；传输状态、收发缓冲和协议时钟仍只有该 Worker
能够修改。已有的单所有者接口保持原来的使用方式。

在 Bind 前同时配置 `AsyncOperations` 和 `AsyncBytes`。默认均为 0，关闭适配器；
最多保留 64 个请求/结果、16 MiB 请求节点及负载。该额度也计入端点 MemoryLimit
的准入预留。每条受理请求创建有界的 XRT Future、取消订阅和错误元数据；它们另行
分配，调用者保留的取消 Future 和自行附加的后续任务也不计入节点配额。此预算
不表示整个进程、Engine、系统套接字或分配器池的 RSS。

调用线程持有自己的 peer 引用，使用 `xrtRudpOperationInit` 初始化请求，然后调用
`xrtRudpPeerSubmitAsync`。请求和写入负载在返回前复制，调用者可立即复用输入。
返回 NULL 表示没有受理操作，通过当前线程的 XRT 错误区分参数、关闭、内存失败
和 `XERR_AGAIN` 配额占用。单个请求连空端点都放不下时返回 `XERR_RANGE`，需要缩小
请求或在 Bind 前调整额度；重试同一过大请求不会成功。临时额度占用时先释放不再
需要的已完成 Future，再重试。

返回的 Future 恰好终结一次。解析后的借用值是 `xrudpoperationresult`，其中
Data、Datagram 和保留的 Error 均随 Future 存活。写入完成会清除输入副本。
销毁观察方 Future 不等于取消操作；需要取消时先显式调用 `xrtFutureCancel`。
不能在绑定 Worker 上阻塞等待，否则会阻止它完成请求。其他线程可等待，或使用
XRT Future 的执行器/后续任务接口。

## 数据语义

| 操作 | 成功的含义 |
| --- | --- |
| READ / WRITE | 一次字节前缀；WRITE 只表示进入有界发送缓冲，不表示对端交付 |
| MESSAGE_READ / MESSAGE_WRITE | 一整条消息，允许空消息；写入空间不足时等待，不交付半条消息 |
| DATAGRAM_READ / DATAGRAM_WRITE | 一条不可靠游戏记录；WRITE 不保证送达，也不会因丢包重发 |
| STREAM_OPEN / FINISH / STOP / RESET / RETIRE | 对应原生操作已在所有者上执行，保留相同返回码 |
| DATAGRAM_CONFIGURE | 本地通道配置操作的实际结果 |
| WAIT_WRITE_DONE | 对应流的数据和 FIN 已确认；不能把单次 WRITE 成功当作此结果 |
| SHUTDOWN | 在更早排队的可靠写入/开流/FIN 受理后启动有界优雅排空，最终成功仍须查询关闭快照 |
| ABORT | 立即中止，包括握手尚未确认的 peer，不等待数据和握手 |

字节 WRITE 可能只受理前缀；调用者必须按结果 Size 处理剩余输入，不能把整块重复提交。
消息/数据报短读返回 TRUNCATED 和所需 Size，保留原记录；重新提交足够容量的读取即可。
读取返回 DONE 表示原生正常 EOF；取消、RESET 和协议错误保留各自返回码。
消息的 Size 为 0 仍可能是成功消息，须同时检查 Result。

同一个 peer、流、方向的请求按受理顺序处理；所有通道共用一个数据报写入方向。
读取和其他流可以继续推进，单条阻塞写入不会阻止它们。优雅关闭构成 peer 的写入屏障；
STOP、RESET、ABORT 可以越过阻塞数据，避免取消本身被流控卡住。
不要在同一方向同时使用异步操作和原始 owner 读写接口。

## 期限、取消与关闭

TimeoutMs 为 0 或 1..600000，从提交入口的单调时钟计时，包含复制、分配和排队耗时。
请求超时只终结该请求，保持 peer 可用。握手、空闲、业务进度及优雅排空各有独立期限。
握手中的普通请求等待确认，中止请求无需等待。

DatagramLifetimeUs 同样从提交时算起，0 关闭此期限，非零最多 10 分钟；它包含等待
Worker 和本地原生队列的时间，不能在入队或重试时重新计时。原生受理前过期返回
TIMEOUT、Size/Sequence 为 0，不花费数据报序号，也不发送旧快照。已经受理的记录
保留原来的绝对期限，后续在传输队列过期只丢弃，不重发负载。

取消采用 XRT 的生产者协作语义：所有者认领排队请求或作出终结决定前观察到取消，
Future 进入 CANCELLED，不会消费读取数据或写入字节。与认领并发的取消以真实结果
为准；不能仅凭 Cancel 的返回值判断是否写入。原生受理成功不能被取消抹掉，避免
调用者误判而重放已经提交的数据。BACKPRESSURE 表示未受理，此时取消仍能取胜。

先请求 EndpointClose 并等待 Close，然后停止/销毁 Engine；XRT 拒绝停止仍有活动传输
的 Engine，拒绝不会改变端点状态，也不会把其 Future 假装结算。EndpointClose 会结算
排队请求；PeerClose 回调前该 peer 的所有请求必须终结，端点 Close 前所有请求必须
终结。完成结果可以晚于 Engine 销毁，
端点/peer 引用及 Future 必须各自释放。关闭后的提交返回 NULL/CLOSED，不能访问已销毁 Worker。
内部调度器等资源错误触发关闭时，排队请求保留实际错误及其 Error 引用，不能在 UDP
关闭回调尚未到达时先把它覆盖为普通取消。错误对象同样可以保留到 Engine 销毁之后。

完成的结果持续占用请求和字节额度，直到 Future 释放；因此长期保留结果也不会让
新请求突破内存限制。取消结果不保留负载。AsyncState 是可跨线程查询的有界统计，
即使 Engine 已销毁，持有端点引用仍可读取。
