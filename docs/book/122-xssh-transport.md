---
num: 122
slug: xssh-transport
title: SSH（一）：传输与包层
volume: 卷十一 其他扩展库
type: practice
lead: wire 编解码、RFC 4253 二进制包框架、AES-GCM 包加密与无缓冲的传输核心——SSH 栈最底层的四块积木。
api: xssh-ssh_wire, xssh-ssh_packet, xssh-ssh_transport_core
---

## 导读

SSH 系列开篇。与 TLS（卷八）的"高层一体化"不同，xssh 的设计哲学是**彻底分层**：最底层四块积木各自独立、零分配、可单独测试——**wire**（`ssh_wire`：SSH 的 boolean/uint32/string/mpint 基本类型编解码）；**packet**（`ssh_packet`：RFC 4253 二进制包框架——长度/padding/载荷，不绑定密码与网络）；**packet AES-GCM**（`ssh_packet_aes_gcm`：OpenSSH 的 aes128/256-gcm 包封装——明文长度头作 AAD、原位加密、十六字节标签）；**transport core**（`ssh_transport_core`：包 codec+协议顺序+rekey 预算的无缓冲组合——同步、回调、Future、协程客户端驱动**同一个状态契约**）。后续六章全部建立在这层之上。

## 引入

SSH 的线路形态是一个"洋葱"：应用数据（通道里的）包在加密包里、加密包包在 TCP 里。包框架（RFC 4253 第 6 节）规定了洋葱的形状——`packet_length（4B）| padding_length（1B）| payload | padding`，padding 保证整包按块对齐（密码块或防流量分析）。**为什么强制 padding 回调**：`xrtSshPacketWrite` 要求调用方提供 `xsshpaddingproc`、不静默用零——核心不携带随机数依赖（裁剪自由），transport 用会话 PRNG、测试用确定性填充。

transport core 的关键设计是**发送/接收事务三段式**（Prepare/Commit/Abort）：发送先 `WritePrepare` 把最终线路包写入调用方缓冲（此时 sequence/nonce/预算**均未推进**）——网络队列接受后 `WriteCommit` 才推进；`XNET_RESULT_AGAIN` 时保留同一线路包等 writable 重试（**不得重新准备**——sequence 不可复用）；放弃则 `WriteAbort` 丢弃新增字节。每方向最多一包待提交——背压期间不占内存也不复用计数器。

## 概念

### wire：SSH 的类型系统

SSH 协议的"序列化格式"就五种基本类型：`boolean`（1B）、`uint32`（4B 大端）、`uint64`（8B）、`string`（4B 长度前缀 + 字节）、`mpint`（string 的整数形态——规范非负编码、去掉多余符号位）。`xsshwriter`/`xsshreader` 是零分配游标对（与第 77 章 DER 游标同构）：`WriterInit` 绑定调用方缓冲、写各类型的 `WriterWrite*`、`Size` 报产出；reader 侧 `ReaderInit/Read*` 借用输入逐类型读。**严格性**：截断拒绝、尾随数据（在报文层）拒绝、mpint 非规范编码拒绝——SSH 报文解析器的全部词汇。

### packet：框架与原子性

`xrtSshPacketMeasure(载荷长, 块长, &padding长, &packet长)` 计算框架：零块长用 RFC 最小 8；显式块长 8..255；padding 至少 4 字节且整包按块对齐；完整线路长 `4 + packet_length`。`xrtSshPacketWrite(writer, 载荷, ..., padding回调, 上下文)` 产完整线路包——回调先写栈临时区、成功才提交目标（**writer 与序列号在失败时不变**）。`xrtSshPacketRead(reader, 预算, ..., &packet视图)` 返回借用输入的 `xsshpacketview`：短包 `XSSH_NEED_MORE`（增量等数据——与全库三态同构）；畸形长度/对齐/padding 协议错误；超预算 `XSSH_ERROR_OVERFLOW`。**序列号**：uint32 自然回绕；NULL 可忽略（独立 framing 工具）。**原子性**：reader 在副本中完成全部校验，完整成功才推进输入、发布视图、递增序列——失败不半推进。

### packet AES-GCM：OpenSSH 的 AEAD 包

`aes128-gcm@openssh.com`/`aes256-gcm@openssh.com` 的共用封装：4 字节 `packet_length` **保持明文并作为 AAD**；`padding_length|payload|padding` 原位加密；尾部 16 字节标签。`xsshaesgcm` 是**单向状态**（读写各自初始化，不可并发推进同一状态）：密钥 16/32 字节、initial IV 12 字节（前 4 fixed IV + 后 8 大端 invocation counter）；**counter 每包递增、UINT64_MAX 前停止绝不回绕**（nonce 复用零容忍——第 74 章纪律的协议化）。四入口：`AesGcmMeasure`（16 字节对齐的框架计算）/`AesGcmWrite`（writer 未提交区域构建并原位加密、零堆分配）/`AesGcmRead`（**先认证再解密**到调用方缓冲——认证失败不改明文输出；成功但结构非法则清零已解密 body）/`AesGcmInvocation`（查下包 counter）。失败（容量/截断/回调/认证/状态耗尽）不推进 reader、writer、sequence、counter 任何一个。

### transport core：无缓冲组合与 NEWKEYS

```diagram flow
- 发送：WritePrepare(最终包入调用方缓冲，不推进) → 队列成功 → WriteCommit(推进 sequence/nonce/预算)
  → AGAIN（XNET_RESULT_AGAIN）则保留原包重试 → 放弃则 WriteAbort
- 接收：Inspect(读 4B 长度头) → 按 WireSize 聚合输入 → ReadPrepare(认证+分类+状态检查)
  → 上层解析 payload → ReadCommit / ReadAbort(已认证包被拒→core 关闭，不伪装回滚)
- NEWKEYS：本端包入队后写方向关，SetWriteAesGcm 激活；对端包认证后 SetReadAesGcm
  → strict-kex 序列重置+方向 rekey 计数清零+cipher 切换同一调用内完成
  → 两方向都生效后 KexComplete 才为真
```

**ReadAbort 的语义值得咀嚼**：codec 已消费该包——拒绝已认证包意味着协议层不信任了，transport 直接关闭，“不会伪装回滚序列号继续通信”（回滚序列=制造重放窗口）。**自动识别**：Prepare 自动识别 KEXINIT/NEWKEYS/USERAUTH_SUCCESS 三类特殊包——rekey 预算在这些边界推进。**网络适配边界**：发送缓冲可直接给 `xrtNetStreamSend/SendRef`；接收可借用 `xrtNetStreamBuffer` 连续区间——适配层只管背压/生命周期/deadline/cancel，**不重复实现 SSH 规则**。

## 示例

### 第一个完整程序：包框架的写读往返

下面的程序来自 `examples/packet`——最小 IGNORE 风格包的构建与读取：

```embed path="extlibs/xssh/examples/packet/main.c" title="extlibs/xssh/examples/packet/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/packet/main.c -lws2_32 -liphlpapi
（输出 packet=16 payload=8 padding=7 形态的包框架自检结果）
```

**刚才发生了什么。** ① `xsshwriter` 绑定 32 字节栈缓冲；`xrtSshPacketWrite` 写入 8 字节 `\2ignore` 载荷（消息号 2=SSH_MSG_IGNORE）——`exampleSshPadding` 确定性回调产 padding（示例专用；生产传 `xrtSshSecurePadding` 或会话 PRNG——契约卡明说“示例的确定性 padding 不能直接用于真实 SSH transport”）。② `xrtSshPacketRead` 两次调用：第一次不带出参（只要校验通过）、第二次取 `xsshpacketview`——`PacketSize/Payload/Padding` 三视图借用输入。③ 预算参数 `8u` 是显式上限——真实 transport 用协商上限。往返一致即框架自洽：这是第 77 章 DER 往返模式的 SSH 版。

### 第二个完整程序：传输核心的零负担声明

第二个程序来自 `examples/transport_core`——core 的资源形态：

```embed path="extlibs/xssh/examples/transport_core/main.c" title="extlibs/xssh/examples/transport_core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/transport_core/main.c -lws2_32 -liphlpapi
（输出 transport-core 结构尺寸与默认最大包长的自检结果）
```

**刚才发生了什么。** ① `xrtSshTransportCoreInit(&Core, XSSH_ROLE_CLIENT, 0, NULL, 0)` 在**栈上**初始化客户端角色 core——零参数版本取默认上限；输出 `sizeof(Core)` 与 `Codec.MaxPacketSize`——头注释点明设计声明：“不持有网络和缓冲”。**这个 sizeof 输出本身就是文档**：core 是可预测的栈对象，不是堆怪兽。② `TransportCoreClear` 收尾——core 的清理不涉及任何句柄（它从未打开过任何东西）。③ 同步、事件回调、Future、协程四种客户端驱动**同一个** `Core`——驱动形态是外挂的（第 126 章看到全部四种），状态契约只有一份。配套示例族：`packet_codec`（明文/GCM 双模式）、`packet_aes_gcm`（GCM 状态与四入口）、`packet_random`/`transport_tcp_random`（随机源集成）、`transport_rekey`（rekey 预算路径）、`transport_state`（状态机）。

## 契约

- **wire 严格性**：截断/尾随/非规范 mpint 拒绝；零分配游标对；writer 失败字节不变。
- **packet 框架**：块长 8..255（零=8 默认）；padding≥4 且整包块对齐；`4+packet_length` 为线路长。
- **padding 强制回调**：不静默零填充；回调先栈后提交；`XSSH_ERROR_CALLBACK` 与协议错误区分。
- **reader 原子**：副本校验全过才推进输入/发布视图/递增序列；短包 NEED_MORE；超预算 OVERFLOW。
- **GCM 状态**：单向独立初始化；IV=fixed(4)+counter(8 大端)；counter 到 UINT64_MAX 停不回绕。
- **GCM 原子**：先认证后解密；失败不动 reader/writer/sequence/counter；认证失败不改明文；成功但结构非法清零 body。
- **GCM 别名**：pPlain 不与输入/状态重叠；写入 Payload 不与本次输出区间重叠（维持原位加密路径）。
- **发送事务**：Prepare 不推进；Commit 推进；AGAIN 保留原包重试（不重新准备）；Abort 丢弃；每方向至多一包待提交。
- **接收事务**：Inspect→聚合→ReadPrepare（认证+分类）→ReadCommit/Abort；ReadAbort 关闭 core（已认证包不可回滚）。
- **NEWKEYS**：写方向关到 SetWriteAesGcm 成功；strict-kex 重置+计数清零+cipher 切换同调用；双方向生效 KexComplete 才真。
- **网络边界**：适配层管背压/生命周期/deadline/cancel——SSH 规则归 core，不重复实现。

## 避坑

### 坑 1：AGAIN 之后重新 Prepare

症状：对端报 MAC/认证失败或直接断开——重新准备产生了新包但 sequence/nonce 与已排队包冲突。

原因：Prepare 不推进计数器正是为了 AGAIN 的重试：**保留同一线路包原样重试入队**。重新 Prepare = 用新 padding/nonce 重造包，与队列里可能已部分受理的旧包形成计数器分叉。

```c bad
if ( prepare(&Core, Buf) == OK ) {
	if ( enqueue(Buf) == AGAIN ) {
		prepare(&Core, Buf);   /* 重造：nonce/序列分叉风险 */
		enqueue(Buf);
	}
}
```

```c good
xrtSshTransportCoreWritePrepareWithPadding(&Core, Buf, ...);
if ( enqueue(Buf) == XNET_RESULT_AGAIN ) {
	wait_writable();          /* 保留原包 */
	enqueue(Buf);             /* 原样重试 */
}
/* 决定不发才 Abort */
```

### 坑 2：packet 层忘了传最大包预算

症状：恶意服务端宣布超大包——读入越堆越大直到 OOM。

原因：`iMaxPacketSize` 零用默认上限，但**预算是防线不是可选项**；独立工具场景也要显式传（哪怕就传默认常量——自文档化）。

```c bad
xrtSshPacketRead(&Reader, 0u, 0u, NULL, &Packet);  /* 依赖默认——预算意图不明 */
```

```c good
xrtSshPacketRead(&Reader, XSSH_PACKET_MAX_DEFAULT, 0u,
	NULL, &Packet);   /* 显式预算：代码即文档 */
```

### 坑 3：GCM 读写共用一个状态

症状：加解密互相错乱——第 N 包的解密用了第 N+1 的 counter。

原因：`xsshaesgcm` 是**单向**状态——SSH 每方向独立密钥独立 IV 独立 counter；一个状态两用等于两个方向共享计数器。

```c bad
xsshaesgcm Gcm;
xrtSshAesGcmInit(&Gcm, Key, IV);
write_packet(&Gcm, ...);   /* 写用 */
read_packet(&Gcm, ...);    /* 读也用：counter 冲突 */
```

```c good
xsshaesgcm WriteGcm, ReadGcm;
xrtSshAesGcmInit(&WriteGcm, C2SKey, C2SIv);  /* 客户端→服务端 */
xrtSshAesGcmInit(&ReadGcm, S2CKey, S2CIv);   /* 服务端→客户端 */
```

## 练习

### 基础：五种 wire 类型往返

用 writer/reader 写读 boolean/uint32/uint64/string/mpint 各一，验证往返与字节形态（对照 RFC 4251 的编码示例）。验收标准：mpint 的规范编码（去符号位）能手推；截断输入被拒。

### 进阶：包框架边界实验

对 `PacketMeasure` 做 0-100 字节载荷扫描，绘制 padding/packet_length 变化；验证块长 8 与 16 两档的对齐差异。再构造超预算读取验证 OVERFLOW。验收标准：测量值与 RFC 4253 §6 公式一致。

### 挑战：双向 GCM 通道

两个进程（或同进程双 core）用 KEX 派生的两方向密钥建 GCM 通道：互发 1000 包、验证 counter 单调、断言第 UINT64_MAX-1 包后拒绝继续写（状态耗尽——用小 counter 模拟）。验收标准：全双工无错包；counter 耗尽路径显式拒绝而非回绕。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 四积木 | wire（类型）→ packet（框架）→ aes_gcm（加密包）→ transport core（组合+顺序+rekey） |
| wire 类型 | boolean/uint32/uint64/string/mpint；零分配游标；严格拒绝 |
| 包框架 | 4B 长度+1B padding 长+载荷+padding；块对齐；线路长=4+packet_length |
| padding 回调 | 强制提供；栈先后提交；CALLBACK 错误与协议错误分开 |
| reader 原子 | 副本校验全过才推进；NEED_MORE/OVERFLOW/协议错三态 |
| GCM 形态 | 长度头明文作 AAD；载荷原位加密；16B 标签；counter 不回绕 |
| GCM 原子 | 先认证后解密；任何失败不动任何计数器 |
| 发送事务 | Prepare(不推进)→Commit/AGAIN 保留/Abort；每方向一包 |
| 接收事务 | Inspect→聚合→Prepare→Commit/Abort(关 core) |
| NEWKEYS | 方向独立激活；strict-kex 重置同调用；双活才 KexComplete |
| 网络边界 | 缓冲直给 Send/SendRef/Buffer——适配层不重复 SSH 规则 |
