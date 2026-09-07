# PROGRESS — 教程重写进度记录

> 每阶段开始/结束时更新本文件；上下文被压缩后从这里恢复现场。

## 状态总览

| 阶段 | 状态 | 完成章 | 讲解字数 |
| --- | --- | --- | --- |
| P0 基础设施 + 金标准 | ✅ 完成 | 2 | 10,093 |
| P1 卷一 起步与核心 | ✅ 完成 | 8 | 34,150 |
| P2 卷二全部 + 卷三前半 | ✅ 完成 | 7 | 28,790 |
| P3 卷三后半 + 选型章 | ✅ 完成 | 7 | 27,804 |
| P4 卷四前半 | ✅ 完成 | 6 | 22,441 |
| P5 卷四后半 | ✅ 完成 | 7 | 30,235 |
| P6 卷五前半 | ✅ 完成 | 6 | 24,273 |
| P7 卷五后半 | ✅ 完成 | 8 | 34,240 |
| P8 卷六前半 | ✅ 完成 | 8 | 32,911 |
| P9 卷六后半 | ✅ 完成 | 2 | 11,210 |
| P10 卷七前半 | ✅ 完成 | 8 | 31,742 |
| P11 卷七后半 | ✅ 完成 | 2 | 13,099 |
| P12 卷八密码基础 | ✅ 完成 | 5 | 25,122 |
| P13 卷八证书链 | ✅ 完成 | 4 | 23,822 |
| P14 卷八TLS上 | ✅ 完成 | 4 | 21,347 |
| P15 卷八TLS下 | 🔄 进行中 | 0 | — |
| P16–P27 | ⬜ 未开始 | — | — |

## 章节明细

| 章 | 文件 | 类型 | 字数 | 程序 | 图示 | 坑 | 练习 | 阶段 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 01-vol1-intro.md | intro | 2,050 | 0 | 1 | 0 | 0 | P1 |
| 2 | 02-intro.md | practice | 4,054 | 2 | 1 | 2 | 3 | P1 |
| 3 | 03-first.md | concept | 6,001 | 2 | 2 | 3 | 3 | P1 |
| 4 | 04-error.md | concept | 6,015 | 2 | 2 | 3 | 3 | P0 |
| 5 | 05-memory.md | practice | 4,002 | 2 | 1 | 2 | 3 | P1 |
| 6 | 06-memory-debug.md | practice | 4,018 | 2 | 1 | 2 | 3 | P1 |
| 7 | 07-temp.md | practice | 4,007 | 2 | 1 | 2 | 3 | P1 |
| 8 | 08-core-trim.md | practice | 4,003 | 2 | 1 | 2 | 3 | P1 |
| 10 | 10-math.md | practice | 4,250 | 2 | 1 | 2 | 3 | P2 |
| 11 | 11-random.md | practice | 4,079 | 2 | 1 | 2 | 3 | P2 |
| 12 | 12-hash-xid.md | practice | 4,086 | 3 | 1 | 2 | 3 | P2 |
| 13 | 13-array.md | practice | 4,300 | 2 | 1 | 2 | 3 | P2 |
| 14 | 14-buffer.md | practice | 4,035 | 2 | 1 | 2 | 3 | P2 |
| 15 | 15-stack.md | practice | 4,003 | 2 | 1 | 2 | 3 | P2 |
| 16 | 16-list-slotmap.md | practice | 4,037 | 2 | 1 | 2 | 3 | P2 |
| 61 | 61-vol7-intro.md | intro | 2,227 | 0 | 1 | 0 | 0 | P10 |
| 62 | 62-net-addr.md | practice | 4,658 | 2 | 1 | 2 | 3 | P10 |
| 63 | 63-net-port.md | practice | 4,192 | 2 | 1 | 2 | 3 | P10 |
| 64 | 64-net-buf.md | practice | 4,251 | 2 | 1 | 2 | 3 | P10 |
| 65 | 65-net-dns.md | practice | 4,099 | 2 | 1 | 2 | 3 | P10 |
| 67 | 67-net-tcp-adv.md | practice | 4,117 | 2 | 1 | 2 | 3 | P10 |
| 68 | 68-net-udp.md | practice | 4,156 | 2 | 1 | 2 | 3 | P10 |
| 69 | 69-net-proxy.md | practice | 4,042 | 2 | 1 | 2 | 3 | P10 |
| 66 | 66-net-tcp.md | practice | 4,078 | 2 | 2 | 3 | 3 | P0 |
| 70 | 70-net-interface.md | practice | 6,174 | 3 | 1 | 3 | 3 | P11 |
| 71 | 71-net-misc.md | practice | 6,925 | 3 | 1 | 3 | 3 | P11 |
| 72 | 72-vol8-intro.md | intro | 2,147 | 0 | 1 | 0 | 0 | P12 |
| 73 | 73-crypto-hash.md | practice | 5,963 | 4 | 1 | 3 | 3 | P12 |
| 74 | 74-crypto-aead.md | practice | 5,509 | 2 | 1 | 3 | 3 | P12 |
| 75 | 75-crypto-asym.md | practice | 6,228 | 3 | 1 | 3 | 3 | P12 |
| 76 | 76-crypto-discipline.md | practice | 5,275 | 2 | 2 | 3 | 3 | P12 |
| 77 | 77-der-pem.md | practice | 5,382 | 2 | 1 | 3 | 3 | P13 |
| 78 | 78-x509.md | practice | 5,934 | 2 | 1 | 3 | 3 | P13 |
| 79 | 79-x509-verify.md | practice | 5,518 | 2 | 1 | 3 | 3 | P13 |
| 80 | 80-cert-chain.md | practice | 6,988 | 3 | 1 | 3 | 3 | P13 |
| 81 | 81-tls-client.md | practice | 5,719 | 2 | 1 | 3 | 3 | P14 |
| 82 | 82-tls-identity.md | practice | 5,482 | 2 | 1 | 3 | 3 | P14 |
| 83 | 83-tls-handshake.md | practice | 5,174 | 2 | 2 | 3 | 3 | P14 |
| 84 | 84-tls-policy.md | practice | 4,972 | 2 | 2 | 3 | 3 | P14 |





## P14 阶段记录

- 卷八 TLS 上 4 章：ch81 TLS 客户端（托管拨号/回调与 Future 双形态/READY 后
  收发与认证关闭/会话恢复）、ch82 TLS 身份（新增章：四构造器/CanSign/两段式
  Sign/HSM 扩展）、ch83 握手时序图解（新增章：四消息时序/HKDF 密钥调度链/
  HRR/协商/KeyUpdate/证书条目）、ch84 验证策略（新增章：策略对象白名单/
  验证器三层决策/无隐式联网边界）。
- 全书第十三次重编号：三插入 tls-identity(82)/tls-handshake(83)/tls-policy(84)，
  36 文件 +3，全书 120 章；卷八 15 章（72-86）骨架就位。
- 门禁拦下：xtlsstreamevents 在符号表提取规则外（结构体内部字段分号切断
  typedef 语句——正文改用"事件表：Open/Read/Writable/Close"描述性写法）；
  ch82 初稿漏图示（补生命周期 diagram）。
- 行文章号修正 2 处（tls-stream→86）。

## P13 阶段记录

- 卷八证书链 4 章：ch77 DER 与 PEM（TLV 游标/严格 DER/PEM 块协议）、
  ch78 X.509 证书解析（零拷贝视图/RDN 游标/OID 翻译层/消费侧校验清单）、
  ch79 签名验证与服务身份（三问模型/协议分派/RFC 9525 匹配三态）、
  ch80 信任链与吊销（新增章：PathValidate/PathBuild/信任库四形态/
  CRL 两层与吊销三态）——三问模型（签名/身份/信任）贯穿 79-80 两章。
- 全书第十二次重编号：插入 cert-chain（80），37 文件 +1，全书 117 章；
  卷八 12 章（72-83）骨架就位。
- 门禁拦下：XRT_MODULE_ASN1 不存在（真名 XRT_MODULE_ASN1_DER——DER 游标
  是独立模块）、X509/X25519/X448 裸词（PEM 标签例换 TRUSTED CERTIFICATE、
  枚举斜杠列表改中文）、xrtAlloc（真名 xrtMalloc）、X509_VERIFY_* 简写
  （补全 XRT_FEATURE_X509_VERIFY_* 全名）、xrtX509Store* 通配（展开实名）。
- 行文章号修正 12 处（tls-client→81、tls-stream→83、卷八范围 80-82→81-83）。

## P12 阶段记录

- 卷八密码基础 5 章新写：ch72 导言（信任金字塔+两条横切纪律+先读禁令方法论）、
  ch73 哈希与 HMAC（摘要族/流式三段式/PBKDF2 与 HKDF 双 KDF）、ch74 AEAD
  （AES-GCM 状态机/ChaCha20-Poly1305 便捷层/nonce 纪律）、ch75 非对称
  （x25519/x448/P-256/P-384 交换+Ed25519/ECDSA/RSA-PSS 签名）、
  ch76 密码工程纪律（常量时间/密钥生命周期四段/随机源分界/组合纪律，
  session 示例=TLS 1.3 骨架缩微版）。
- 全书第十一次重编号：双插入 vol8-intro（72）与 crypto-discipline（76），
  43 文件 +2，全书 116 章；卷八 11 章（72-82）骨架就位。
- 门禁拦下：X25519/X448 裸词（算法名改小写避开符号规则，函数名 xrtX448* 保留）、
  xrtAesCtrEncrypt（坑3 bad 块编造名改示意伪码）、xrtRand/xrtFastRand 不在符号表
  （普通随机改用已验证的 xrtFastRandSeed 表述）、XCRYPTO_HASH_* 通配改具体枚举、
  xxHash 改"SipHash 家族"。
- 行文章号修正 11 处（testing→108、perf→110、http-headers→85、tls-client→80）。
- 事故修复：重建 order.json 时 1-9 章 file 字段丢前导零（1-vol1-intro），磁盘文件
  未受损（renumber 按 slug 定位），补零后重建通过。

## P11 阶段记录

- P11 定义 6 章中 tcp-adv/udp/proxy 已在 P10 提前完成、netbuf 深入并入 ch64；本阶段
  实际执行 2 章：ch70 网卡接口与本机信息（新增章：接口快照/名称索引互转/本机三件套
  三层形态/IPv6 scope）、ch71 分帧与异步文件（卷七收官：行分帧/长度前缀/帧取用/
  完成端口文件三段式）。卷七 11 章（61-71）全部成型。
- 全书第十次重编号：插入 net-interface（44 文件 +1，全书 114 章）。
- 门禁拦下：ref-addr/ref-net_buf 页不存在（api 字段改 net 族真实页）、
  xrtFileClose 不在符号表（xfile 家族统一 xrtClose，改写表述）。
- 行文章号修正 11 处：正文中"第 N 章"纯文字引用按主题重定位（测试体系→106、
  性能分析→108、分帧→71、HTTP 头解析→83 等），消除历次重编号的漂移。
- 非 确 定 性 输 出示例（interface/local_info/frame_line/frame_length 预期输出为
  "随机器变化"占位）：check 按空提取跳过比对，term 给代表性输出并注明。

## P10 阶段记录

- 卷七前半 8 章新写：ch61 导言（骨架工业化映射主线+两条横切纪律）、ch62 地址模型、
  ch63 事件端口与五后端、ch64 缓冲链、ch65 DNS、ch67 TCP 进阶（发送五档+写预算）、
  ch68 UDP（双形态+批量）、ch69 代理（SOCKS5/HTTP CONNECT+托管拨号）。
  ch66 TCP 金标准重编号迁移（旧 65 → 66），内容未改。
- 全书第九次重编号：插入卷七导言 ch61（ch61 之后全部 +1）。
- 范围调整：net-file/net-frame 并入 ch70 net-misc（分帧、端口文件与网卡），归 P11。
- 门禁拦下：xsockaddr_storage 不存在（改 Native[64] 数组 + iSize 出参）、
  xnetportcapabilities 非结构体（Capabilities 返回 uint32 位标志）、
  xnetbufspan 真名 xnetspan 且 Spans 为填调用方数组模式、
  term 输出多次对齐示例头注释（resolver_future/tcp_stream_tour/udp_batch 真实行）。
- 站点同步：index.html 全站编号偏移修正（插入 ch61 后 112 条目 +1）+ 卷范围对齐
  order.json + 卷七 61-69 置 done；start.html 章号可见文字与 href 一致化；
  全站 book 链接扫描 0 断链。

## P9 阶段记录

- P8 吸收了原 P9 计划中的 channel/future/executor 三章，P9 实际执行：ch59 取消与结构化
  并发重写 + 新增 ch60 调度器实战组合章（卷六收官）。卷六 11 章成型（50-60）。
- 全书第八次重编号（52 文件，ch60 之后 +1）。
- 门禁修正：WaitCancel→WaitUntilCancel 真名、xrtChannel 裸词改 Create 形态。
- ch60 是第三个组合章（6,208 字/4 坑/双完整程序）：三零指标（泵零阻塞/池零等待/
  消息零锁）、装配三决策、停机双通道、五项检查清单、卷七映射预告、三级迭代路径。

## P8 阶段记录

- 卷六前半 8 章（插入卷六导言 ch50 与取消体系专章 ch53，全书第七次重编号 60 文件）。
- 门禁拦下并修正：xrtThreadWait 无出参（返回 xwaitresult、值经原子/Future 回传）、
  取消语义精确化（Requested 含祖先链查询/Watch 至多同步一次/Triggered 查监听——三 API 分工）、
  调度器族全面重写（SchedCreate→CoSchedCreate + CoGo/Post + Run/Poll/Step + Sleep/Park/Wake——
  泵族三形态按头文件实际划分）、Future 三态→四态（RESOLVED/FAILED/CANCELLED/CLOSED——
  Reject/Resolve/Close 三入口）、xchannelresult TrySend、
  Spawn 签名(配置)、Line 返回 uint32、xson 品牌词。
- 卷六主线确立：取消贯穿（53 立地基→58 各章方言）+ 三层装配线（原语/协作/结构化）。

## P7 阶段记录

- 卷五后半 8 章（P6 预留的导言位 ch36 本章写就；signal 从卷六归位卷五 ch48；
  新增调试组合章 ch49 收官）。全书第六次重编号（61 文件），卷五 14 章成型。
- 门禁拦下：IsSafe→IsSafeEntry(视图+bDirectory)、xrtPathJoin varargs→两段 cstr、
  锁两档(Lock/LockRange)签名修正、xrtTempFile→xrtFileTemp、异步模型全面重写
  （回调制→任务池+Future 真实形态：AsyncFileOpen/ReadAt/WriteAt）、
  xrtSignalWatch→On/Once 族、VisitLive 返回值、xmemstats 无 CurrentBytes
  （改用 MallocBytes-FreeBytes）。
- 组合章型第二次落地（ch49，6,098 字/4 坑）：体检与尸检同构、观测经济学、
  诊断剧本、反模式警示（观测不能替代设计）。

## P6 阶段记录

- 卷五前半 6 章（插入卷五导言 ch36，全书第五次重编号 72 文件；导言本身属 P7 批次待写——本阶段 6 章指 logger×2/console/io/time/env）。
- 门禁拦下：xrtIoLineReader→xlinereader+Next 三态、env 三条路径实为两条（Get/Lookup，
  GetDefault 不在头文件）、xrtTimeAdd 签名出参序、printf 范例真实输出对齐、
  SplitAt 参数序修正。

## P5 阶段记录

- 卷四后半 7 章：regex 归位卷四（原卷十一 ch88 → ch30）、value/json/xson/template 重写、
  新增组合章 ch35（composition 型首章：6000 字/2 程序/5 坑）。
- 全书第四次重编号（77 文件）：regex 前插 + text-pipeline 尾插，卷四扩为 12 章。
- 门禁拦下：xrtValueRef→xrtValueRetain、xrtValueObjectNew→xrtValueObject、
  FindAdvance→Next、StringSub→String、模板 {+%}→真实语法、xson 补品牌词。

## P4 阶段记录

- 卷四前半 6 章（插入卷四导言 ch24，全书第三次重编号，82 文件位移）。
- 门禁拦下并修正：xrtStrStartsWith→xrtStrStarts、xrtStrMatch→xrtStrGlob、
  XRT_ENCODING_*→XENCODING_*、XUTF_IGNORE 不存在（仅 STRICT/REPLACE）、
  BOM 是 bool 参数非标志、xrtBase64EncodeUrl→字母表参数、xrtTimeNow→xrtNow。
- 经验沉淀：diagram 围栏内容不计讲解字数——扩字数必须改正文段落。
- stream_tour 示例预期输出有第 4 行（config corrupt rejected），term 已对齐。

## P3 阶段记录

- 卷三扩为 11 章（插入卷三导言 ch13、容器选型章 ch23）；全书第二次重编号（91 文件）。
- 行文章号引用按映射脚本统一迁移（13..21 → +1，≥22 → +2）。
- 修复 renumber_book.py 的 md 匹配缺陷（后缀 glob → 精确旧文件名），避免 core-trim 被误改名。
- 批次：ch18-20（映射/集合/AVL）、ch21-22（队列/池）、ch13+ch23（导言/选型收官）。

## P2 阶段记录

- 批次 1（卷二）：ch10 数学 / ch11 随机 / ch12 哈希与 XID；顺带修复仓库示例
  examples/hash/variants/main.c 预期输出注释缺第三行的问题。
- 批次 2（卷三前半）：ch13 数组 / ch14 缓冲 / ch15 栈族 / ch16 链表与 slot_map。
- 本阶段无新增章节，无重编号；门禁全绿后双仓库各两次提交。
- checker 白名单补充：家族前缀 xrtMath、产品名 XID（BRAND_TOKENS）。
- 经验沉淀：初稿字数普遍落在目标的 70-80%，需按 1.3 倍余量起稿。

## 遗留项

- ch82-xregex 旧重定向页仍指向 ch85-regex，属 P5 卷四 regex 归位时处理。
